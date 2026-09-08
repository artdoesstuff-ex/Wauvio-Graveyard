#pragma once

// =============================================================================
//  wauvio_soundtrack_surface_4.hpp
//  Soundtrack: Surface — Track 4
//
//  Concept: SPECTRAL SHEDDING
//  --------------------------
//  The inverse of Pressure Accumulation (seq2_intense).
//  This track begins with a cluster of 8 FM voices all sounding together.
//  At irregular timestamps, one voice exits — fading out over 2.0 seconds.
//  No voice ever re-enters.  By the end of the loop, only one voice remains.
//
//  The perceptual paradox:
//  When the loop repeats, the dense cluster returns.  But the listener
//  has just heard it reduced to a single voice.  The return of density
//  feels wrong — like a crowd that was gone is suddenly back.
//  The fullness at loop-start is more disturbing than any silence.
//
//  Voice exit schedule (51 seconds)
//  ---------------------------------
//  All 8 voices active from 0s.
//  Exit timestamps (voice index, exit_onset_s):
//    V8 exits at  5.3s
//    V6 exits at 12.7s
//    V3 exits at 18.1s
//    V7 exits at 23.9s
//    V2 exits at 28.4s
//    V5 exits at 33.8s
//    V4 exits at 40.2s
//    V1 exits at 46.5s   ← last to exit; V0 is the survivor
//
//  V0 (the survivor) runs 0s → 51s continuously — never exits.
//  Loop boundary: V0 is the only voice at 49s–51s.
//  Crossfade blends V0's tail into the full-cluster head.
//
//  Voice parameters:
//  -----------------
//  Survivors and exiting voices are interleaved across frequency range
//  so that as voices exit, the spectral holes are noticed.
//
//  V0 (survivor): C=103 Hz, M=103×φ=166.6 Hz,       idx=1.1, amp=0.052
//  V1:            C=158 Hz, M=158×∛3=223.4 Hz,       idx=0.8, amp=0.046
//  V2:            C= 67 Hz, M= 67×√7=177.2 Hz,       idx=1.6, amp=0.044
//  V3:            C=231 Hz, M=231×(e-1)=334.5 Hz,    idx=0.5, amp=0.040
//  V4:            C= 43 Hz, M= 43×π=135.1 Hz,        idx=2.2, amp=0.038
//  V5:            C=187 Hz, M=187×√5=418.1 Hz,       idx=0.7, amp=0.036
//  V6:            C= 89 Hz, M= 89×∛5=152.2 Hz,       idx=1.4, amp=0.034
//  V7:            C=261 Hz, M=261×(1+1/φ)=422.4 Hz,  idx=0.4, amp=0.030
//
//  Each non-survivor voice fades out over 2.0s starting at its exit time.
//  Fade shape: linear (not cosine, not exponential — the abruptness of
//  linear removal makes the departure more noticeable).
//
//  Two silence windows:
//  After V3 exits (18.1s + 2s = 20.1s), a 3.2s silence window:
//    All remaining voices fade to 35% amplitude over 0.8s, hold 1.6s,
//    restore over 0.8s.  This is not a full silence — the cluster dims.
//    The listener notices what's gone.
//  After V2 exits (28.4s + 2s = 30.4s), a 2.4s full-silence window
//    (the survivor V0 fades to 20% for 2.4s).
//
//  Duration: 51 seconds
//  Crossfade: 100ms
//  Normalize: 0.70
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace surface_4 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 51.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 100;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  VOICE TABLE
    // =========================================================================
    constexpr double E   = 2.71828182845904523536;
    constexpr double PHI = 1.6180339887498948482;
    constexpr double PI  = 3.14159265358979323846;

    struct VoiceDef {
        double carrier_hz;
        double mod_hz;
        double mod_idx;
        float  amp;
        double exit_s;    // -1.0 = survivor (never exits)
    };

    const VoiceDef VOICES[] = {
        // V0 — survivor
        { 103.0, 103.0 * PHI,               1.1, 0.052f, -1.0  },
        // V1
        { 158.0, 158.0 * 1.44224957030741,  0.8, 0.046f, 46.5  },  // ∛3
        // V2
        {  67.0,  67.0 * 2.64575131106459,  1.6, 0.044f, 28.4  },  // √7
        // V3
        { 231.0, 231.0 * (E - 1.0),         0.5, 0.040f, 18.1  },  // e-1
        // V4
        {  43.0,  43.0 * PI,                2.2, 0.038f, 40.2  },
        // V5
        { 187.0, 187.0 * 2.2360679774998,   0.7, 0.036f, 33.8  },  // √5
        // V6
        {  89.0,  89.0 * 1.70997594667670,  1.4, 0.034f, 12.7  },  // ∛5
        // V7
        { 261.0, 261.0 * (1.0 + 1.0/PHI),  0.4, 0.030f,  5.3  },
    };
    constexpr int N_VOICES   = 8;
    constexpr double EXIT_FADE = 2.0;  // linear fade-out duration for each exit

    // =========================================================================
    //  GLOBAL AMPLITUDE DIM WINDOWS
    //  Window A: 20.1s → 23.3s  — all voices dim to 35%, then restore
    //  Window B: 30.4s → 32.8s  — V0 only, dim to 20%
    // =========================================================================
    // Precompute a global dim envelope (1.0 = normal, <1.0 = dimmed)
    std::vector<float> global_dim(TOTAL, 1.0f);
    {
        // Window A
        const size_t wA_s   = static_cast<size_t>(20.1 * SR);
        const size_t wA_e   = static_cast<size_t>(23.3 * SR);
        const size_t rampA  = static_cast<size_t>(0.8  * SR);
        const size_t holdA  = static_cast<size_t>(1.6  * SR);
        for (size_t i = wA_s; i < std::min(wA_e, TOTAL); ++i) {
            const size_t local = i - wA_s;
            if (local < rampA) {
                global_dim[i] = 1.0f - 0.65f * (static_cast<float>(local) / static_cast<float>(rampA));
            } else if (local < rampA + holdA) {
                global_dim[i] = 0.35f;
            } else {
                const size_t r = local - rampA - holdA;
                global_dim[i] = 0.35f + 0.65f * (static_cast<float>(r) / static_cast<float>(rampA));
                if (global_dim[i] > 1.0f) global_dim[i] = 1.0f;
            }
        }
    }

    // Precompute V0-only dim for Window B (stored separately, applied only to V0)
    std::vector<float> v0_extra_dim(TOTAL, 1.0f);
    {
        const size_t wB_s  = static_cast<size_t>(30.4 * SR);
        const size_t wB_e  = static_cast<size_t>(32.8 * SR);
        const size_t rampB = static_cast<size_t>(0.5  * SR);
        const size_t holdB = static_cast<size_t>(1.4  * SR);
        for (size_t i = wB_s; i < std::min(wB_e, TOTAL); ++i) {
            const size_t local = i - wB_s;
            if (local < rampB) {
                v0_extra_dim[i] = 1.0f - 0.80f * (static_cast<float>(local) / static_cast<float>(rampB));
            } else if (local < rampB + holdB) {
                v0_extra_dim[i] = 0.20f;
            } else {
                const size_t r = local - rampB - holdB;
                v0_extra_dim[i] = 0.20f + 0.80f * (static_cast<float>(r) / static_cast<float>(rampB));
                if (v0_extra_dim[i] > 1.0f) v0_extra_dim[i] = 1.0f;
            }
        }
    }

    // =========================================================================
    //  RENDER VOICES
    // =========================================================================
    for (int v = 0; v < N_VOICES; ++v) {
        const VoiceDef& vx = VOICES[v];
        const bool is_survivor = (vx.exit_s < 0.0);

        // Determine active window
        const size_t s0 = 0;
        const size_t s1 = is_survivor
            ? TOTAL
            : std::min(static_cast<size_t>((vx.exit_s + EXIT_FADE) * SR), TOTAL);

        const size_t exit_start = is_survivor
            ? TOTAL
            : static_cast<size_t>(vx.exit_s * SR);
        const size_t exit_ramp  = static_cast<size_t>(EXIT_FADE * SR);

        const double c_inc = wauvio::TWO_PI * vx.carrier_hz / SR;
        const double m_inc = wauvio::TWO_PI * vx.mod_hz     / SR;
        double cp = 0.0, mp = 0.0;

        for (size_t i = s0; i < s1; ++i) {
            // Exit fade
            float exit_env = 1.0f;
            if (!is_survivor && i >= exit_start) {
                const size_t d = i - exit_start;
                exit_env = (d < exit_ramp)
                    ? 1.0f - static_cast<float>(d) / static_cast<float>(exit_ramp)
                    : 0.0f;
            }

            // Global dim
            float dim = global_dim[i];
            if (v == 0) dim *= v0_extra_dim[i];

            out[i] += static_cast<float>(
                std::sin(cp + vx.mod_idx * std::sin(mp)))
                * vx.amp * exit_env * dim;

            cp += c_inc;
            mp += m_inc;
        }
    }

    // =========================================================================
    //  NOISE TEXTURE
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0x9988776655u & 0xFFFFFFFFu);
        wauvio::HighPassFilter hp(400.0,  SR);
        wauvio::LowPassFilter  lp(1400.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s); s = lp.tick(s);
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

} // namespace surface_4
} // namespace soundtrack
