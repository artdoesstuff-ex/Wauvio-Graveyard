#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence1_tutorial.hpp
//  Soundtrack: Sequence1 — Tutorial Track
//
//  Design brief
//  ------------
//  This plays during the very first moments of the game — the player learning
//  to walk.  It must not overwhelm.  It must not be ignored.
//  It should feel like a room that knows you are new to it.
//
//  Unlike Track 1 (waiting) and Track 2 (fragmented movement),
//  the tutorial track is architecturally sparse.  Events are far apart.
//  The silences are longer than the sounds.
//  When sound does appear, it is close — intimate, not distant.
//  No reverb tails.  Dry.  Present.  Like a breath just behind you.
//
//  Structure (38 seconds)
//  ----------------------
//  0s  – 0s    Starts in silence. No ramp-in event. Just background.
//  3.7s        First event: a single FM tone, 0.6s duration.
//              High index, detuned. Sounds like a question without words.
//  7.1s – 9.8s Slow bass movement: FM glide from 62 Hz down to 41 Hz over 2.7s.
//              No rhythm. No resolution.
//  11.2s       Repeat of first tone type — but different carrier, different index.
//              Listener will not consciously recognize the relationship.
//  14.0s       Silence begins. 7 full seconds of nothing but background noise.
//  21.0s – 25.5s  Texture event: two FM voices detuned by exactly 3 Hz.
//              Beating at 3 Hz is almost subliminal.  Feels like nausea.
//  27.3s       Short percussive FM thud — same character as Track 2's cold open
//              but softer, shorter.  A reminder.
//  29.4s – 37s  Long drone swell: carrier 88 Hz, FM index rises 0.2 → 1.8.
//              Fades before loop boundary.  No tonic return.
//
//  Asymmetry sources
//  -----------------
//  - All event onsets are irrational (not multiples of any common grid)
//  - FM glide: pitch is never constant — always moving or stopping unexpectedly
//  - 3 Hz beating is sub-rhythmic — not a tempo, just instability
//  - Drone enters at 29.4s (not 30s) — off the 10s boundary by design
//  - Total duration 38s — prime-adjacent, avoids common loop lengths
//
//  Architecture compliance
//  -----------------------
//  - 38 seconds (within 30–60 s) (§4.1)
//  - Seamless loop: 80 ms crossfade + silence boundary (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence1_tutorial {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 38.0;
    constexpr double FADE_S    = 0.08;
    constexpr size_t XFADE_MS  = 80;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  HELPER: write single FM event with linear attack/release
    // =========================================================================
    auto write_fm = [&](double onset_s, double dur_s,
                        double carrier_hz, double mod_hz,
                        double mod_idx_start, double mod_idx_end,
                        float amp,
                        double attack_s, double release_s)
    {
        const size_t s0  = static_cast<size_t>(onset_s * SR);
        const size_t n   = static_cast<size_t>(dur_s   * SR);
        const size_t end = std::min(s0 + n, TOTAL);
        const size_t len = end - s0;
        if (len == 0) return;

        const double c_inc = wauvio::TWO_PI * carrier_hz / SR;
        const double m_inc = wauvio::TWO_PI * mod_hz     / SR;
        double cp = 0.0, mp = 0.0;

        const size_t att_n = static_cast<size_t>(attack_s  * SR);
        const size_t rel_n = static_cast<size_t>(release_s * SR);

        for (size_t i = 0; i < len; ++i) {
            const double t_norm  = static_cast<double>(i) / static_cast<double>(len);
            const double mod_idx = mod_idx_start + t_norm * (mod_idx_end - mod_idx_start);

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
    //  BACKGROUND NOISE — very low, runs entire buffer
    //  Narrow band: HPF @ 1200 Hz, LPF @ 3000 Hz → present but subliminal
    //  Lower amplitude than other tracks — tutorial is quieter
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0x0F1E2D3Cu);
        wauvio::HighPassFilter hp(1200.0, SR);
        wauvio::LowPassFilter  lp(3000.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.006f;
        }
    }

    // =========================================================================
    //  EVENT 1 — 3.7s
    //  A single tone: the sound of something noticing you.
    //  Carrier: 220 Hz (A3), Mod: 220 × 1.5 = 330 Hz (tritone-adjacent)
    //  Wait — 330/220 = 1.5 which is a perfect fifth. Too consonant.
    //  Use 220 × 1.618 (φ) = 355.96 Hz instead — irrational, unstable.
    //  Index 2.8 (metallic, complex spectrum). Duration 0.6s. Fast attack.
    // =========================================================================
    write_fm(3.7, 0.6,
             220.0, 220.0 * 1.6180339887,
             2.8, 2.8,           // constant index — no evolution, just presence
             0.055f,
             0.015, 0.25);

    // =========================================================================
    //  EVENT 2 — BASS GLIDE  7.1s → 9.8s
    //  FM carrier glides from 62 Hz → 41 Hz over 2.7s.
    //  Not a portamento — implemented as decreasing carrier frequency
    //  sample-by-sample with a rate derived from exponential pitch space.
    //  Modulator tracks the carrier proportionally (ratio × carrier).
    //  Mod ratio: 2.2 (non-harmonic overtone relationship)
    //  Index: 1.4 (moderate, tonal but unsettled)
    //  No attack (0s) — enters immediately from silence. Harsh presence.
    //  Release: 0.4s.
    // =========================================================================
    {
        constexpr double ONSET    = 7.1;
        constexpr double DUR_L    = 2.7;
        constexpr double FREQ_S   = 62.0;
        constexpr double FREQ_E   = 41.0;
        constexpr double MOD_RATIO = 2.2;
        constexpr double MOD_IDX  = 1.4;
        constexpr float  AMP      = 0.058f;
        constexpr double REL_S    = 0.4;

        const size_t s0  = static_cast<size_t>(ONSET * SR);
        const size_t len = static_cast<size_t>(DUR_L * SR);
        const size_t end = std::min(s0 + len, TOTAL);
        const size_t rel_n = static_cast<size_t>(REL_S * SR);
        // Carrier phase accumulates with varying increment
        double cp = 0.0, mp = 0.0;

        for (size_t i = 0; i < end - s0; ++i) {
            const double t_norm  = static_cast<double>(i) / static_cast<double>(len);
            // Exponential glide: carrier_freq = FREQ_S × (FREQ_E/FREQ_S)^t
            const double freq_c  = FREQ_S * std::pow(FREQ_E / FREQ_S, t_norm);
            const double freq_m  = freq_c * MOD_RATIO;
            cp += wauvio::TWO_PI * freq_c / SR;
            mp += wauvio::TWO_PI * freq_m / SR;

            const size_t pos = end - s0 - 1 - i;  // reverse index for release test
            float env = 1.0f;
            if (i >= (end - s0) - rel_n && rel_n > 0)
                env = static_cast<float>(pos) / static_cast<float>(rel_n);

            out[s0 + i] += static_cast<float>(
                std::sin(cp + MOD_IDX * std::sin(mp))) * AMP * env;
        }
    }

    // =========================================================================
    //  EVENT 3 — 11.2s
    //  Similar to Event 1 but different carrier and index.
    //  Carrier: 185 Hz (F#3-ish), Mod: 185 × φ = 299.3 Hz
    //  Index: 1.6 (softer spectrum than Event 1's 2.8)
    //  Duration: 0.9s (50% longer than Event 1 — just enough to be different)
    //  The listener may notice "something repeated" without knowing what.
    // =========================================================================
    write_fm(11.2, 0.9,
             185.0, 185.0 * 1.6180339887,
             1.6, 1.6,
             0.050f,
             0.02, 0.35);

    // =========================================================================
    //  SILENCE ZONE — 14.0s → 21.0s
    //  Intentionally empty except for background noise.
    //  7.0 seconds. The longest silence in the loop.
    //  No events, no sub-bass, nothing.
    //  [Nothing written here — the silence is the feature]
    // =========================================================================

    // =========================================================================
    //  EVENT 4 — BEATING TEXTURE  21.0s → 25.5s
    //  Two FM voices separated by exactly 3 Hz.
    //  Voice A: 156.0 Hz, Voice B: 159.0 Hz
    //  Beat frequency: 3 Hz — below conscious rhythm perception (~4 Hz)
    //  Feels like an unstable, nauseous oscillation.
    //  Both voices use same mod ratio (φ) but different indices (1.0, 1.3)
    //  to give them slightly different timbres so they are audibly distinct.
    // =========================================================================
    write_fm(21.0, 4.5,
             156.0, 156.0 * 1.6180339887,
             1.0, 1.0,
             0.042f,
             0.6, 1.2);

    write_fm(21.0, 4.5,
             159.0, 159.0 * 1.6180339887,
             1.3, 1.3,
             0.038f,
             0.6, 1.2);

    // =========================================================================
    //  EVENT 5 — PERCUSSIVE THUD  27.3s
    //  Same character as Track 2's cold-open hits but quieter, shorter.
    //  Carrier: 48 Hz, Mod: 192 Hz (× 4), Index: 5.5
    //  Duration: 0.2s. Attack: 0.001s (nearly instantaneous). Release: 0.18s.
    //  Amplitude: 0.070 — present but not dominant.
    // =========================================================================
    write_fm(27.3, 0.20,
             48.0, 192.0,
             5.5, 5.5,
             0.070f,
             0.001, 0.18);

    // =========================================================================
    //  EVENT 6 — LONG DRONE SWELL  29.4s → 37.2s
    //  Carrier: 88 Hz (F2, low and full), Mod: 88 × e = 239.2 Hz
    //  FM index rises slowly 0.2 → 1.8 — drone evolves from near-sine to complex
    //  Amplitude: 0.060
    //  Attack: 2.0s (slow swell from silence)
    //  Release: 2.5s (dissolves before loop boundary at 38s)
    //  No reverb — this track is intentionally dry. Presence, not distance.
    // =========================================================================
    write_fm(29.4, 7.8,               // 29.4 + 7.8 = 37.2s
             88.0, 88.0 * 2.71828182,  // mod = carrier × e
             0.2, 1.8,                 // index evolves
             0.060f,
             2.0, 2.5);

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

} // namespace sequence1_tutorial
} // namespace soundtrack
