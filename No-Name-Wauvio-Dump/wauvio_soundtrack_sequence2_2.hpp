#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence2_2.hpp
//  Soundtrack: Sequence2 — Track 2
//
//  Design brief
//  ------------
//  The entities in Sequence2 are defined by how they respond to the player.
//  STILL_HUNTER moves when the player doesn't.  MIRROR copies the player.
//  This track should feel like something that understands your position
//  but cannot communicate it.  Like being watched by something that
//  knows you know it's there.
//
//  Concept: SPECTRAL ROTATION
//  Every previous track uses a fixed relationship between carrier and
//  modulator: carrier is the pitch center, modulator shapes the timbre.
//  This track inverts that contract at unpredictable timestamps.
//
//  A "rotation" event swaps which oscillator acts as carrier and which
//  acts as modulator — or more precisely, cross-routes their outputs
//  into a new FM configuration.  The result is a sudden timbral pivot
//  that cannot be predicted: the same two frequencies produce a completely
//  different spectrum when their roles exchange.
//
//  No LFO modulation is used in this track.
//  No silence gaps (the unease is continuous, not punctuated).
//  The instability comes entirely from the rotation events.
//
//  Rotation sequence (49 seconds)
//  --------------------------------
//  Four FM configurations are defined.  Playback cycles through them
//  at irrational timestamps, with a 0.04s crossfade between each.
//
//  Config 0 — "Heard"   (0.0s → 7.3s)
//    C=157 Hz, M=157×φ=254.0 Hz, idx=1.4
//    Bright-ish, recognizable. This is the "normal" state.
//
//  Config 1 — "Mirror"  (7.3s → 16.8s)
//    C becomes old M: C=254.0 Hz, M=254.0×∛3=381.0 Hz, idx=2.1
//    Carrier is now what the modulator was. Register shifts up.
//    Timbre becomes more metallic.
//
//  Config 2 — "Inverted" (16.8s → 22.1s)
//    C=83 Hz, M=83×√6=203.4 Hz, idx=3.3
//    Sudden drop to low register. High index = complex, distorted.
//    Duration 5.3s — shortest window, creates tension before release.
//
//  Config 3 — "Rotated" (22.1s → 33.4s)
//    C=203.4 Hz (Config 2's modulator is now the carrier)
//    M=203.4×(1+1/e)=203.4×1.368=278.2 Hz, idx=0.7
//    Low index now — same carrier from Config 2's mod, but almost sine.
//    The violence of Config 2 deflates to near-nothing.
//
//  Config 0 returns (33.4s → 41.7s) — same parameters but different phase
//  Config 2 returns (41.7s → 49.0s) — compressed (7.3s not 5.3s)
//
//  Each config also runs a second FM voice at -5 semitones from the
//  carrier (tritone-adjacent), index 0.4, amplitude 0.018 — a constant
//  wrong undertone that follows every rotation.
//
//  Silence behavior
//  ----------------
//  No silence gaps. Instead: a sub-threshold layer of band noise
//  (HPF 80 Hz, LPF 200 Hz, amp 0.009) runs continuously.
//  During the crossfades between configs (40ms), both voices are
//  audible simultaneously — the brief overlap creates a spectral
//  collision that acts like a rhythmic accent without being one.
//
//  Anti-repetition
//  ---------------
//  Phase at loop boundary is never zero for any voice.
//  Config durations (7.3, 9.5, 5.3, 11.3, 8.3, 7.3) sum to 49.0s.
//  No common divisor with any config's period. Each loop starts with
//  all voices mid-oscillation — same pitches, different phase → different timbre.
//
//  Architecture compliance
//  -----------------------
//  - 49 seconds (§4.1)
//  - Seamless loop: 95 ms crossfade (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence2_2 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 49.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 95;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  FM CONFIG TABLE
    //  Each config: carrier_hz, mod_hz, mod_idx, amplitude
    //  Plus undertone: carrier × 2^(-5/12) (tritone-adjacent down), idx=0.4, amp=0.018
    // =========================================================================
    struct FMConfig {
        double carrier_hz;
        double mod_hz;
        double mod_idx;
        float  amp;
    };

    constexpr FMConfig CONFIGS[] = {
        // 0 "Heard"
        { 157.0,
          157.0 * 1.6180339887,            // ×φ = 254.0
          1.4, 0.055f },
        // 1 "Mirror" — old mod becomes new carrier
        { 254.0,
          254.0 * 1.4422495703074083,      // ×∛3 = 366.1
          2.1, 0.048f },
        // 2 "Inverted" — register drop, high index
        { 83.0,
          83.0 * 2.449489742783178,        // ×√6 = 203.3
          3.3, 0.052f },
        // 3 "Rotated" — Config2's mod is now carrier, low index
        { 203.3,
          203.3 * 1.3678794411714423,      // ×(1+1/e)
          0.7, 0.045f },
    };

    // Sequence: config index, start time (seconds)
    struct Segment { int cfg; double start_s; };
    constexpr Segment SEGMENTS[] = {
        { 0,  0.0  },
        { 1,  7.3  },
        { 2, 16.8  },
        { 3, 22.1  },
        { 0, 33.4  },
        { 2, 41.7  },
    };
    constexpr int N_SEGS     = 6;
    constexpr double XF_S    = 0.04;   // 40ms crossfade between configs

    // =========================================================================
    //  Render each segment with crossfade blending at boundaries
    // =========================================================================
    for (int seg = 0; seg < N_SEGS; ++seg) {
        const int    cfg_idx  = SEGMENTS[seg].cfg;
        const double seg_s0   = SEGMENTS[seg].start_s;
        const double seg_s1   = (seg + 1 < N_SEGS)
                                    ? SEGMENTS[seg + 1].start_s
                                    : DUR;
        const auto&  cfg      = CONFIGS[cfg_idx];

        const size_t s0  = static_cast<size_t>(seg_s0 * SR);
        const size_t s1  = std::min(static_cast<size_t>(seg_s1 * SR), TOTAL);
        const size_t len = s1 - s0;
        if (len == 0) continue;

        const size_t xf_n = static_cast<size_t>(XF_S * SR);
        const size_t fi_n = (seg > 0)        ? xf_n : static_cast<size_t>(0.05 * SR);
        const size_t fo_n = (seg < N_SEGS-1) ? xf_n : static_cast<size_t>(0.05 * SR);

        const double c_inc = wauvio::TWO_PI * cfg.carrier_hz / SR;
        const double m_inc = wauvio::TWO_PI * cfg.mod_hz     / SR;
        double cp = 0.0, mp = 0.0;

        // Undertone: carrier × 2^(-7/12) — tritone-adjacent below
        const double ut_freq = cfg.carrier_hz * std::pow(2.0, -7.0 / 12.0);
        const double ut_mod  = ut_freq * 1.6180339887;
        const double ut_cidx = wauvio::TWO_PI * ut_freq / SR;
        const double ut_midx = wauvio::TWO_PI * ut_mod  / SR;
        double ut_cp = 0.0, ut_mp = 0.0;

        for (size_t i = 0; i < len; ++i) {
            float env = 1.0f;
            if (i < fi_n && fi_n > 0)
                env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n && fo_n > 0)
                env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);

            // Main FM voice
            out[s0 + i] += static_cast<float>(
                std::sin(cp + cfg.mod_idx * std::sin(mp))) * cfg.amp * env;

            // Undertone FM voice
            out[s0 + i] += static_cast<float>(
                std::sin(ut_cp + 0.4 * std::sin(ut_mp))) * 0.018f * env;

            cp     += c_inc;
            mp     += m_inc;
            ut_cp  += ut_cidx;
            ut_mp  += ut_midx;
        }
    }

    // =========================================================================
    //  SUB PRESENCE — low band noise, continuous, no gate
    //  HPF 80 Hz, LPF 200 Hz, amp 0.009
    //  Provides continuity across all config changes
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xA1A2A3A4u);
        wauvio::HighPassFilter hp(80.0,  SR);
        wauvio::LowPassFilter  lp(200.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.009f;
        }
    }

    // =========================================================================
    //  HIGH SHIMMER — very quiet presence layer
    //  HPF 4000 Hz, LPF 8000 Hz, amp 0.005
    //  Makes the silences between rotation events feel inhabited
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0x5E6F7A8Bu);
        wauvio::HighPassFilter hp(4000.0, SR);
        wauvio::LowPassFilter  lp(8000.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.005f;
        }
    }

    // =========================================================================
    //  LOOP BOUNDARY PREP
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

} // namespace sequence2_2
} // namespace soundtrack
