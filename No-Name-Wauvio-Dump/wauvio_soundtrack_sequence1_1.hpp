#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence1_1.hpp
//  Soundtrack: Sequence1 — Track 1
//
//  Design brief
//  ------------
//  The player has just entered the cave.  Nothing has happened yet.
//  The music should feel like something behind the walls is aware of
//  being listened to.  It does not move.  It waits.
//
//  Structure (53 seconds)
//  ----------------------
//  0s  – 8s    Silence gradient: sub pressure creeps in from nothing.
//               No attack. No event. Just mass accumulating.
//  8s  – 22s   Layer A enters: a badly-detuned two-oscillator FM cluster
//               that never resolves — the modulator is irrational relative
//               to the carrier, producing aperiodic timbre.
//  22s – 31s   Silence gap. Layer A fades before the gap, not on it.
//               The silence is 9 seconds and contains only sub.
//  31s – 44s   Layer B: a different FM voice at a lower register, with a
//               slow tremolo that accelerates then stops mid-cycle.
//  44s – 53s   Both layers re-enter briefly at reduced amplitude, then
//               dissolve back into the sub before loop boundary.
//
//  Asymmetry sources
//  -----------------
//  - Layer A duration (14s) ≠ Layer B duration (13s)
//  - Silence gap is offset from the mathematical midpoint
//  - Tremolo acceleration is non-uniform (quadratic speedup)
//  - FM modulator indices drift at irrational rates
//  - Loop boundary deliberately misaligned with any internal phrase
//
//  Architecture compliance
//  -----------------------
//  - 53 seconds (within 30–60 s) (§4.1)
//  - Seamless loop: 100 ms crossfade + silence at boundary (§4.1)
//  - Normalized to 0.70 (soundtrack peak, §5.1)
//  - Fixed noise seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence1_1 {

inline wauvio::Buffer gen() {

    const int    SR  = wauvio::global_config().sample_rate;
    constexpr double DUR        = 53.0;
    constexpr double FADE_S     = 0.10;
    constexpr size_t XFADE_MS   = 100;
    const size_t     TOTAL      = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  UTILITY: write a windowed FM voice into `out` between onset and end.
    //  carrier_hz, mod_hz    — base frequencies
    //  mod_idx_start/end     — modulation index linearly interpolates over window
    //  amp                   — peak amplitude
    //  fade_in_s/fade_out_s  — amplitude envelope on this voice only
    // =========================================================================
    auto write_fm = [&](double onset_s, double end_s,
                        double carrier_hz, double mod_hz,
                        double mod_idx_start, double mod_idx_end,
                        float amp,
                        double fade_in_s, double fade_out_s)
    {
        const size_t s0  = static_cast<size_t>(onset_s * SR);
        const size_t s1  = std::min(static_cast<size_t>(end_s * SR), TOTAL);
        if (s0 >= s1) return;

        const double dur_s  = (s1 - s0) / static_cast<double>(SR);
        const double c_inc  = wauvio::TWO_PI * carrier_hz / SR;
        const double m_inc  = wauvio::TWO_PI * mod_hz      / SR;
        double cp = 0.0, mp = 0.0;

        const size_t fi_n = static_cast<size_t>(fade_in_s  * SR);
        const size_t fo_n = static_cast<size_t>(fade_out_s * SR);
        const size_t len  = s1 - s0;

        for (size_t i = 0; i < len; ++i) {
            // Interpolated modulation index
            const double t_norm  = static_cast<double>(i) / static_cast<double>(len);
            const double mod_idx = mod_idx_start + t_norm * (mod_idx_end - mod_idx_start);

            // Per-voice fade envelope
            float env = 1.0f;
            if (i < fi_n && fi_n > 0)
                env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n && fo_n > 0)
                env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);

            out[s0 + i] += static_cast<float>(
                std::sin(cp + mod_idx * std::sin(mp))) * amp * env;
            cp += c_inc;
            mp += m_inc;
        }
    };

    // =========================================================================
    //  SUB PRESSURE LAYER  — runs the entire buffer
    //  Very low FM: carrier 38 Hz, modulator 38 × φ (golden ratio) = irrational
    //  Modulation index 0.3 — barely FM, almost sine, produces slow beating
    // =========================================================================
    {
        constexpr double CARRIER = 38.0;
        constexpr double MOD     = 38.0 * 1.6180339887; // φ — never resolves
        constexpr double IDX     = 0.28;
        const double c_inc = wauvio::TWO_PI * CARRIER / SR;
        const double m_inc = wauvio::TWO_PI * MOD     / SR;
        double cp = 0.0, mp = 0.0;

        // Slow amplitude swell: silence → 0.055 over first 8s, holds, fades last 4s
        const size_t ramp_in  = static_cast<size_t>(8.0  * SR);
        const size_t ramp_out = static_cast<size_t>(4.0  * SR);

        for (size_t i = 0; i < TOTAL; ++i) {
            float env;
            if (i < ramp_in) {
                env = static_cast<float>(i) / static_cast<float>(ramp_in);
                env = env * env;   // quadratic ease-in — slower start
            } else if (i >= TOTAL - ramp_out) {
                env = static_cast<float>(TOTAL - 1 - i) / static_cast<float>(ramp_out);
            } else {
                env = 1.0f;
            }
            out[i] += static_cast<float>(std::sin(cp + IDX * std::sin(mp)))
                      * 0.055f * env;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // =========================================================================
    //  LAYER A  — 8s → 22s  (14 seconds)
    //  Two FM voices, irrational frequency relationship, diverging indices
    //
    //  Voice A1: carrier 131 Hz (C3-ish), mod 131 × √2 = 185.3 Hz
    //            Index rises 0.6 → 2.4 (timbre opens over time)
    //  Voice A2: carrier 127 Hz (slightly flat), mod 127 × e = 345.2 Hz
    //            Index rises 0.4 → 1.8, amplitude slightly lower
    //            Creates interference beating with A1
    // =========================================================================
    write_fm(8.0, 22.0,
             131.0, 131.0 * 1.41421356,   // mod = carrier × √2
             0.6, 2.4,
             0.058f,
             2.5, 3.0);

    write_fm(8.0, 22.0,
             127.0, 127.0 * 2.71828182,   // mod = carrier × e
             0.4, 1.8,
             0.042f,
             2.5, 3.0);

    // =========================================================================
    //  LAYER B  — 31s → 44s  (13 seconds)
    //  Different register: lower, heavier.
    //  Single FM voice with a tremolo that accelerates then stops.
    //
    //  Carrier: 73 Hz (D2)
    //  Mod: 73 × π = 229.3 Hz — deepens the irrationality
    //  Index: 1.2 → 3.6 (broad, metallic timbre)
    //  Tremolo: LFO rate begins at 0.8 Hz, accelerates quadratically to 4.2 Hz
    //           then cuts off at 11.5s into the 13s window (not at the end)
    // =========================================================================
    {
        constexpr double ONSET    = 31.0;
        constexpr double END_T    = 44.0;
        constexpr double CARRIER  = 73.0;
        constexpr double MOD_FREQ = 73.0 * wauvio::PI;      // × π
        constexpr double IDX_S    = 1.2,  IDX_E = 3.6;
        constexpr float  AMP      = 0.065f;
        constexpr double TREM_OFF = 11.5; // tremolo stops at this offset in window

        const size_t s0  = static_cast<size_t>(ONSET * SR);
        const size_t s1  = std::min(static_cast<size_t>(END_T * SR), TOTAL);
        const size_t len = s1 - s0;
        const size_t fi_n = static_cast<size_t>(2.0 * SR);
        const size_t fo_n = static_cast<size_t>(2.5 * SR);

        const double c_inc = wauvio::TWO_PI * CARRIER  / SR;
        const double m_inc = wauvio::TWO_PI * MOD_FREQ / SR;
        double cp = 0.0, mp = 0.0;

        const size_t trem_off_samples = static_cast<size_t>(TREM_OFF * SR);

        for (size_t i = 0; i < len; ++i) {
            const double t_norm = static_cast<double>(i) / static_cast<double>(len);
            const double mod_idx = IDX_S + t_norm * (IDX_E - IDX_S);

            // Tremolo: accelerates quadratically from 0.8 Hz to 4.2 Hz
            float trem = 1.0f;
            if (i < trem_off_samples) {
                const double t_trem = static_cast<double>(i) / static_cast<double>(trem_off_samples);
                const double trem_rate = 0.8 + t_trem * t_trem * (4.2 - 0.8);
                const double trem_phase = wauvio::TWO_PI * trem_rate
                    * static_cast<double>(i) / SR;
                trem = static_cast<float>(0.60 + 0.40 * std::sin(trem_phase));
            }
            // After trem_off_samples: tremolo is gone — flat amplitude

            float env = 1.0f;
            if (i < fi_n && fi_n > 0)
                env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n && fo_n > 0)
                env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);

            out[s0 + i] += static_cast<float>(
                std::sin(cp + mod_idx * std::sin(mp))) * AMP * env * trem;

            cp += c_inc;
            mp += m_inc;
        }
    }

    // =========================================================================
    //  CODA  — 44s → 53s  (9 seconds)
    //  Both voices return at 60% amplitude, then dissolve.
    //  Uses same oscillator parameters as A and B to create false familiarity —
    //  but the indices are now reversed (A2 rides higher, A1 is quieter).
    //  No resolution. No arrival.
    // =========================================================================
    write_fm(44.0, 52.5,
             131.0, 131.0 * 1.41421356,
             2.4, 1.0,                    // indices reversed from Layer A
             0.033f,
             1.5, 2.5);

    write_fm(44.0, 53.0,
             73.0, 73.0 * wauvio::PI,
             3.6, 0.8,                    // decays from peak index
             0.040f,
             1.5, 2.0);

    // =========================================================================
    //  NOISE TEXTURE — very quiet broadband noise throughout
    //  Filtered to upper-mid range (800–3500 Hz), amplitude < 0.012
    //  Adds breath and prevents the silence feeling like dead air
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xB00BF00Du);
        wauvio::HighPassFilter hp(800.0, SR);
        wauvio::LowPassFilter  lp(3500.0, SR);

        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.011f;
        }
    }

    // =========================================================================
    //  LOOP BOUNDARY PREP
    // =========================================================================
    const size_t xfade = static_cast<size_t>(XFADE_MS * SR / 1000);
    wauvio::fade_in(out,  FADE_S, SR);
    wauvio::fade_out(out, FADE_S, SR);

    // Crossfade tail into head
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

} // namespace sequence1_1
} // namespace soundtrack
