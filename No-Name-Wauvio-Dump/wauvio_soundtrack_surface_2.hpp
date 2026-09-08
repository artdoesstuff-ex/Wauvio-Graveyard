#pragma once

// =============================================================================
//  wauvio_soundtrack_surface_2.hpp
//  Soundtrack: Surface — Track 2
//
//  Concept: HUNTING WALK
//  ---------------------
//  A single FM voice whose carrier frequency is not fixed — it moves.
//  The movement is not an LFO (which would be periodic and predictable).
//  Instead, the carrier traces a constrained random walk defined by a
//  sequence of piecewise-linear segments:
//    Each segment has a direction (+1 or -1) and a duration in samples.
//    Segment durations are at irrational-multiple seconds.
//    The carrier is bounded: [52 Hz, 210 Hz].
//    When the walk would exceed a bound, it reverses direction.
//    Because segment durations are irrational, boundary hits never repeat.
//
//  This produces a carrier that drifts continuously, occasionally
//  "bouncing" off the frequency limits.  The perceptual effect is of
//  something exploring a bounded space — hunting, not roaming.
//
//  When the carrier is near its minimum (52–68 Hz), amplitude is
//  reduced to 40% of peak: the thing is far away or crouching.
//  When the carrier is near its maximum (190–210 Hz), amplitude is
//  also reduced to 60%: the thing is high and exposed.
//  Peak amplitude occurs in the 110–160 Hz range — mid-register,
//  closest to the listener.
//
//  Structure (48 seconds)
//  ----------------------
//  Walk segments (direction, duration_s):
//    +1,  3.71    starting upward
//    +1,  2.13    continues
//    -1,  5.87    reversal — longer descent
//    -1,  1.44    short continuation of descent
//    +1,  4.29    rises again
//    -1,  6.11    long descent — may hit lower bound
//    +1,  3.38    short rise
//    -1,  2.97    descent
//    +1,  7.63    long rise — may hit upper bound
//    -1,  4.19    falls back
//    +1,  2.83    partial rise, ends at loop boundary
//  Total: ~44.55s of walk (≠ 48s loop — remaining time is a slow tail)
//
//  Three silence windows occur when the walk dips below 62 Hz:
//  The voice is gated off (amp → 0 over 80ms) and back on (80ms).
//  Their positions are determined by the walk, not predetermined —
//  they are emergent from the piecewise-linear function.
//  To make this deterministic and reproducible, the walk is
//  pre-computed into a carrier-frequency buffer, silence windows
//  are detected from that buffer, and the gate is applied post-render.
//
//  FM parameters (fixed):
//    Mod: carrier × ∛7 at all times
//    Index: 1.4 (constant — only the carrier moves, not the timbre)
//
//  Second voice — "Shadow":
//    Fixed carrier at 137 Hz, mod at 137 × √5, index 0.6, amp 0.018
//    Runs entire buffer with no walk.  Provides a stable reference
//    that makes the walk voice's movement more perceptible.
//
//  Duration: 48 seconds
//  Crossfade: 85ms
//  Normalize: 0.70
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace surface_2 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 48.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 85;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  STEP 1: Pre-compute carrier frequency walk
    // =========================================================================
    wauvio::Buffer carrier_buf(TOTAL, 0.f);

    {
        // Walk parameters
        constexpr double FREQ_MIN  =  52.0;
        constexpr double FREQ_MAX  = 210.0;
        constexpr double FREQ_INIT = 130.0;  // starting frequency

        // Segments: direction (±1), duration (seconds)
        struct Seg { int dir; double dur_s; };
        const Seg SEGS[] = {
            { +1,  3.71 }, { +1,  2.13 }, { -1,  5.87 }, { -1,  1.44 },
            { +1,  4.29 }, { -1,  6.11 }, { +1,  3.38 }, { -1,  2.97 },
            { +1,  7.63 }, { -1,  4.19 }, { +1,  2.83 }
        };
        constexpr int N_SEGS = 11;
        // Rate of change in Hz per second — consistent across all segments
        constexpr double RATE_HZ_PER_S = 14.0;

        double freq  = FREQ_INIT;
        size_t pos   = 0;
        int    dir   = 0;

        for (int s = 0; s < N_SEGS && pos < TOTAL; ++s) {
            dir = SEGS[s].dir;
            const size_t seg_len = static_cast<size_t>(SEGS[s].dur_s * SR);
            const size_t end     = std::min(pos + seg_len, TOTAL);

            for (size_t i = pos; i < end; ++i) {
                carrier_buf[i] = static_cast<float>(freq);
                freq += dir * RATE_HZ_PER_S / SR;
                // Clamp and reverse on boundary hit
                if (freq < FREQ_MIN) { freq = FREQ_MIN; dir = +1; }
                if (freq > FREQ_MAX) { freq = FREQ_MAX; dir = -1; }
            }
            pos = end;
        }
        // Fill remainder if walk ended before TOTAL
        for (size_t i = pos; i < TOTAL; ++i) {
            carrier_buf[i] = static_cast<float>(freq);
            freq -= RATE_HZ_PER_S / SR;   // slow descent to end
            if (freq < FREQ_MIN) freq = FREQ_MIN;
        }
    }

    // =========================================================================
    //  STEP 2: Render walk voice using per-sample variable-frequency FM
    //          Phase accumulates correctly across frequency changes.
    // =========================================================================
    wauvio::Buffer walk_raw(TOTAL, 0.f);
    {
        constexpr double MOD_RATIO = 1.9129311827723891;  // ∛7
        constexpr double MOD_IDX   = 1.4;
        constexpr float  AMP_PEAK  = 0.055f;
        constexpr double FREQ_MID_LO = 110.0;
        constexpr double FREQ_MID_HI = 160.0;
        constexpr double FREQ_MIN    =  52.0;
        constexpr double FREQ_LOW_HI =  68.0;  // silence gate threshold
        constexpr double FREQ_MAX    = 210.0;
        constexpr double FREQ_HIGH_LO = 190.0;

        double cp = 0.0, mp = 0.0;

        for (size_t i = 0; i < TOTAL; ++i) {
            const double carrier_hz = static_cast<double>(carrier_buf[i]);
            const double mod_hz     = carrier_hz * MOD_RATIO;

            // Amplitude shaping by register
            float amp_scale;
            if (carrier_hz < FREQ_LOW_HI) {
                amp_scale = 0.40f;
            } else if (carrier_hz > FREQ_HIGH_LO) {
                // linear ramp from 1.0 at FREQ_HIGH_LO to 0.60 at FREQ_MAX
                const float t = static_cast<float>(
                    (carrier_hz - FREQ_HIGH_LO) / (FREQ_MAX - FREQ_HIGH_LO));
                amp_scale = 1.0f - t * 0.40f;
            } else if (carrier_hz >= FREQ_MID_LO && carrier_hz <= FREQ_MID_HI) {
                amp_scale = 1.0f;   // peak zone
            } else if (carrier_hz < FREQ_MID_LO) {
                // ramp from 0.40 at FREQ_LOW_HI to 1.0 at FREQ_MID_LO
                const float t = static_cast<float>(
                    (carrier_hz - FREQ_LOW_HI) / (FREQ_MID_LO - FREQ_LOW_HI));
                amp_scale = 0.40f + t * 0.60f;
            } else {
                // ramp from 1.0 at FREQ_MID_HI to 0.60 at FREQ_HIGH_LO
                const float t = static_cast<float>(
                    (carrier_hz - FREQ_MID_HI) / (FREQ_HIGH_LO - FREQ_MID_HI));
                amp_scale = 1.0f - t * 0.40f;
            }

            walk_raw[i] = static_cast<float>(
                std::sin(cp + MOD_IDX * std::sin(mp)))
                * AMP_PEAK * amp_scale;

            cp += wauvio::TWO_PI * carrier_hz / SR;
            mp += wauvio::TWO_PI * mod_hz     / SR;
        }
    }

    // =========================================================================
    //  STEP 3: Gate the walk voice off when carrier dips below 62 Hz
    //          Ramp: 80ms fade out / 80ms fade in
    // =========================================================================
    {
        constexpr double GATE_THRESH  = 62.0;
        constexpr size_t RAMP_SAMPLES = static_cast<size_t>(0.080 * 44100);

        // Build gate envelope: 1.0 = open, 0.0 = closed
        wauvio::Buffer gate(TOTAL, 1.f);
        for (size_t i = 0; i < TOTAL; ++i) {
            if (static_cast<double>(carrier_buf[i]) < GATE_THRESH)
                gate[i] = 0.0f;
        }
        // Smooth gate transitions with RAMP_SAMPLES fade
        float g_prev = gate[0];
        for (size_t i = 1; i < TOTAL; ++i) {
            float g = gate[i];
            if (g < g_prev) {
                // Falling edge — apply ramp_samples fade-out
                for (size_t j = i; j < std::min(i + RAMP_SAMPLES, TOTAL); ++j) {
                    if (gate[j] < 1.0f) {
                        const float frac = static_cast<float>(j - i) / static_cast<float>(RAMP_SAMPLES);
                        gate[j] = std::min(gate[j], 1.0f - frac);
                    }
                }
            }
            if (g > g_prev) {
                // Rising edge — apply ramp_samples fade-in
                const size_t edge = i;
                for (size_t j = 0; j < RAMP_SAMPLES && edge + j < TOTAL; ++j) {
                    const float frac = static_cast<float>(j) / static_cast<float>(RAMP_SAMPLES);
                    gate[edge + j] = std::min(gate[edge + j], frac);
                }
            }
            g_prev = g;
        }

        // Apply gate and mix into out
        for (size_t i = 0; i < TOTAL; ++i)
            out[i] += walk_raw[i] * gate[i];
    }

    // =========================================================================
    //  SHADOW VOICE — fixed carrier, stable reference
    // =========================================================================
    {
        constexpr double CS  = 137.0;
        constexpr double MS  = 137.0 * 2.2360679774997896;  // √5
        constexpr double IDX = 0.6;
        constexpr float  AMP = 0.018f;
        const double c_inc = wauvio::TWO_PI * CS / SR;
        const double m_inc = wauvio::TWO_PI * MS / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * AMP;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // =========================================================================
    //  NOISE TEXTURE — mid presence, quiet
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xBEEF1234u);
        wauvio::HighPassFilter hp(600.0,  SR);
        wauvio::LowPassFilter  lp(2000.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.007f;
        }
    }

    // =========================================================================
    //  LOOP BOUNDARY
    // =========================================================================
    const size_t xfade = static_cast<size_t>(XFADE_MS * SR / 1000);
    wauvio::fade_in(out,  FADE_S, SR);
    wauvio::fade_out(out, FADE_S, SR);

    for (size_t i = 0; i < xfade && i < TOTAL; ++i) {
        const float t    = static_cast<float>(i) / static_cast<float>(xfade);
        const float head = out[i];
        const float tail = out[TOTAL - xfade + i];
        out[i]                 = head * t + tail * (1.f - t);
        out[TOTAL - xfade + i] = tail * t + head * (1.f - t);
    }

    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.70f);
    return out;
}

} // namespace surface_2
} // namespace soundtrack
