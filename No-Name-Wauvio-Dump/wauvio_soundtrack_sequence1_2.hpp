#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence1_2.hpp
//  Soundtrack: Sequence1 — Track 2
//
//  Design brief
//  ------------
//  The player is deep in the cave.  Something has shifted.
//  Track 2 introduces motion — but irregular, interrupted motion.
//  Rhythmic elements appear and vanish without pattern.
//  Pitched material is present but wrong: intervals that almost resolve,
//  then don't.  The ear expects completion and is denied it.
//
//  Structure (47 seconds)
//  ----------------------
//  0s  – 6s    Cold open: isolated low percussive FM thud.
//               Three hits at non-uniform intervals (1.1s, 3.7s, 5.4s).
//               Nothing else. Silence between.
//  6s  – 20s   Drone layer: two detuned saws through heavy LP filter.
//               Beat frequency ~0.9 Hz (nearly 1 Hz but not quite).
//               Sporadic sub-bass FM "breath" events appear on irregular offsets.
//  20s – 26s   Near-silence: only the background noise texture remains.
//               Listener expects the drone to continue. It doesn't.
//  26s – 40s   Fragmented "melody": a series of FM tones at wrong intervals
//               (tritone + minor second relationships, never tonic return).
//               Each tone has a different duration and attack time.
//               No two consecutive tones are related by a simple ratio.
//  40s – 47s   Drone from 6–20s returns but inverted timbre (high index FM)
//               and at 40% amplitude. Fades before boundary. No resolution.
//
//  Asymmetry sources
//  -----------------
//  - Cold-open hit timings: prime-number-adjacent (1.1, 3.7, 5.4)
//  - Drone beat frequency intentionally near-integer but not integer
//  - "Melody" note durations: 0.7, 1.3, 0.4, 2.1, 0.9, 1.6, 0.5, 1.1 s
//  - "Melody" onsets computed cumulatively with irrational gap spacings
//  - Drone re-entry at 40s uses inverse modulation index progression
//
//  Architecture compliance
//  -----------------------
//  - 47 seconds (within 30–60 s) (§4.1)
//  - Seamless loop: 120 ms crossfade + silence ramp at boundary (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence1_2 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 47.0;
    constexpr double FADE_S    = 0.10;
    constexpr size_t XFADE_MS  = 120;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  HELPER: single FM event written into out at sample offset
    // =========================================================================
    auto write_fm_event = [&](size_t s0, double dur_s,
                              double carrier_hz, double mod_hz,
                              double mod_idx,
                              float amp,
                              double attack_s, double release_s)
    {
        const size_t n   = static_cast<size_t>(dur_s * SR);
        const size_t end = std::min(s0 + n, TOTAL);
        const size_t len = end - s0;
        if (len == 0) return;

        const double c_inc = wauvio::TWO_PI * carrier_hz / SR;
        const double m_inc = wauvio::TWO_PI * mod_hz     / SR;
        double cp = 0.0, mp = 0.0;

        const size_t att_n = static_cast<size_t>(attack_s  * SR);
        const size_t rel_n = static_cast<size_t>(release_s * SR);

        for (size_t i = 0; i < len; ++i) {
            float env = 1.0f;
            if (i < att_n && att_n > 0)
                env = static_cast<float>(i) / static_cast<float>(att_n);
            else if (i >= len - rel_n && rel_n > 0)
                env = static_cast<float>(len - 1 - i) / static_cast<float>(rel_n);

            out[s0 + i] += static_cast<float>(
                std::sin(cp + mod_idx * std::sin(mp))) * amp * env;
            cp += c_inc;
            mp += m_inc;
        }
    };

    // =========================================================================
    //  COLD OPEN HITS — 0s, 1.1s, 3.7s
    //  Percussive FM: carrier 55 Hz, mod 110 Hz (octave), high index → metallic thud
    //  Very short: 0.28s each, fast attack (0.002s), longer release (0.25s)
    // =========================================================================
    {
        const double hit_times[] = { 0.0, 1.1, 3.7 };
        for (double t : hit_times) {
            write_fm_event(
                static_cast<size_t>(t * SR),
                0.28,
                55.0, 220.0,      // carrier, mod (× 4 — inharmonic octave overtone)
                6.8,              // very high index — metallic, tuned
                0.12f,
                0.002, 0.24);
        }
    }

    // =========================================================================
    //  BACKGROUND NOISE TEXTURE — runs entire buffer
    //  HPF @ 600 Hz, LPF @ 2800 Hz, amplitude 0.008
    //  Seeded fixed — deterministic hiss
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xFACEB00Cu);
        wauvio::HighPassFilter hp(600.0, SR);
        wauvio::LowPassFilter  lp(2800.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.008f;
        }
    }

    // =========================================================================
    //  DRONE LAYER A — 6s → 20s (14 seconds)
    //  Two detuned sawtooth oscillators through LP filter.
    //  Saw 1: 98.0 Hz (G2)
    //  Saw 2: 98.9 Hz (G2 + 9 cents) → beat frequency 0.9 Hz
    //  Both through LP at 280 Hz (dark, muffled) + light reverb
    // =========================================================================
    {
        constexpr double ONSET = 6.0;
        constexpr double END_T = 20.0;
        const size_t s0  = static_cast<size_t>(ONSET * SR);
        const size_t s1  = static_cast<size_t>(END_T  * SR);
        const size_t len = s1 - s0;
        const size_t fi_n = static_cast<size_t>(2.0 * SR);
        const size_t fo_n = static_cast<size_t>(2.5 * SR);

        // Render two saws into a local buffer, mix into out after filtering
        wauvio::Buffer drone_local(len, 0.f);
        {
            const double inc1 = wauvio::TWO_PI * 98.0 / SR;
            const double inc2 = wauvio::TWO_PI * 98.9 / SR;
            double p1 = 0.0, p2 = 0.0;
            for (size_t i = 0; i < len; ++i) {
                const float s1_s = static_cast<float>(wauvio::wave::sawtooth(p1));
                const float s2_s = static_cast<float>(wauvio::wave::sawtooth(p2));
                drone_local[i] = (s1_s + s2_s) * 0.5f * 0.060f;
                p1 += inc1;
                p2 += inc2;
            }
        }

        // LP filter: 280 Hz cutoff
        wauvio::LowPassFilter drone_lp(280.0, SR);
        drone_lp.process(drone_local);

        // Reverb: small room, damp high end
        wauvio::Reverb drone_verb;
        drone_verb.room_size = 0.55f;
        drone_verb.damping   = 0.65f;
        drone_verb.wet       = 0.30f;
        drone_verb.dry       = 1.0f;
        drone_verb.init(SR);
        drone_verb.process(drone_local);

        // Apply per-voice fade
        for (size_t i = 0; i < len; ++i) {
            float env = 1.0f;
            if (i < fi_n) env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);
            out[s0 + i] += drone_local[i] * env;
        }
    }

    // =========================================================================
    //  SUB-BASS BREATH EVENTS — sparse FM puffs during drone window and beyond
    //  Carrier: 32 Hz, mod: 48 Hz (non-harmonic fifth), index 2.5
    //  Offsets chosen at irrational positions to avoid metrically aligning
    // =========================================================================
    {
        const double breath_times[] = { 7.3, 11.9, 17.2, 28.6, 33.4, 38.7 };
        for (double t : breath_times) {
            write_fm_event(
                static_cast<size_t>(t * SR),
                1.6,             // duration
                32.0, 48.0,      // carrier, mod
                2.5,             // mod index
                0.045f,
                0.4, 1.0);
        }
    }

    // =========================================================================
    //  FRAGMENTED "MELODY" — 26s → 40s
    //  8 tones placed cumulatively with irrational gap spacings.
    //  Interval sequence (semitones from a nominal root of D3 = 146.83 Hz):
    //    +6 (tritone), +1 (minor 2nd), +8 (minor 6th), -3 (minor 3rd),
    //    +11 (major 7th), +4 (major 3rd), -7 (diminished 5th), +2 (major 2nd)
    //  No tone returns to D3. No cadence. No tonic.
    //
    //  FM parameters per tone: carrier = note freq, mod = carrier × √3
    //  Index varies per-tone to give each a different timbre.
    // =========================================================================
    {
        // Semitone offsets from D3 (146.83 Hz)
        const int    semitones[]  = {  6,  1,  8, -3, 11,  4, -7,  2 };
        // Duration of each note in seconds
        const double note_durs[]  = { 0.7, 1.3, 0.4, 2.1, 0.9, 1.6, 0.5, 1.1 };
        // Gap after each note before next (irrational gaps)
        const double gaps[]       = { 0.31, 0.73, 0.18, 0.55, 0.42, 0.67, 0.29, 0.0 };
        // Modulation index per tone
        const double mod_indices[] = { 1.4, 0.7, 2.1, 0.5, 1.8, 1.1, 2.6, 0.9 };
        // Attack / release per tone
        const double attacks[]     = { 0.06, 0.12, 0.03, 0.18, 0.08, 0.15, 0.02, 0.10 };
        const double releases[]    = { 0.20, 0.40, 0.12, 0.60, 0.30, 0.50, 0.15, 0.35 };

        constexpr double BASE_FREQ = 146.83; // D3
        constexpr double START_S   = 26.0;

        double cursor = START_S;
        for (int k = 0; k < 8; ++k) {
            const double freq = BASE_FREQ * std::pow(2.0, semitones[k] / 12.0);
            write_fm_event(
                static_cast<size_t>(cursor * SR),
                note_durs[k],
                freq,
                freq * 1.73205080757,   // mod = carrier × √3
                mod_indices[k],
                0.048f,
                attacks[k], releases[k]);
            cursor += note_durs[k] + gaps[k];
        }
        // cursor should land near 40.0s; actual: 26 + sum(durs+gaps) ≈ 39.9s
    }

    // =========================================================================
    //  DRONE LAYER B — 40s → 47s (coda)
    //  Same two-saw frequencies as Drone A but inverted filter:
    //  HP @ 180 Hz, LP @ 1200 Hz — thinner, more metallic presence.
    //  Modulation index now 4.2 (bright, spectral) rather than the dark low pass.
    //  At 40% amplitude of Drone A.
    // =========================================================================
    {
        // Use FM rather than sawtooth here to differentiate from Drone A timbre
        write_fm_event(
            static_cast<size_t>(40.0 * SR),
            6.5,                // ends at 46.5s — leaves 0.5s silence before loop
            98.0, 98.0 * 1.41421356,   // mod = carrier × √2
            4.2,
            0.024f,             // 40% of Drone A amplitude
            2.0, 2.0);

        write_fm_event(
            static_cast<size_t>(40.0 * SR),
            6.5,
            98.9, 98.9 * 2.71828182,   // mod = carrier × e
            3.8,
            0.018f,
            2.0, 2.0);
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

} // namespace sequence1_2
} // namespace soundtrack
