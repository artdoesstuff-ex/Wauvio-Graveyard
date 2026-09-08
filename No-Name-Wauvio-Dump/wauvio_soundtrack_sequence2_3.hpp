#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence2_3.hpp
//  Soundtrack: Sequence2 — Track 3
//
//  Design brief
//  ------------
//  Late Sequence2.  ctx.player.lWalked >= 200.  The spawn interval is 10s.
//  FAULT_CHASER is now in the rotation.  The player has survived long enough
//  to know the rules.  This track should make the rules feel insufficient.
//
//  Concept: INTERFERENCE FIELD
//  All previous tracks derive their tension from deliberate
//  asymmetry — irregular timing, irrational ratios, decaying structures.
//  This track uses a different mechanism: psychoacoustic interference.
//
//  When two sine waves play simultaneously, the human auditory system
//  generates additional "phantom" tones at their sum and difference
//  frequencies.  These are called combination tones (or Tartini tones).
//  They are not in the audio signal — they exist only in the listener's
//  ear.  They cannot be turned off.  They cannot be predicted by the
//  listener without training.
//
//  This track is built entirely from pairs of sine tones chosen so that
//  their combination tones land in psychoacoustically disturbing zones:
//    - below 20 Hz (infrasonic flutter — felt as unease, not heard)
//    - between 20–40 Hz (barely audible sub-bass — ambiguous, threatening)
//    - near 1/f noise frequencies that suggest biological processes
//
//  Additionally, beating between close-frequency pairs creates "virtual
//  rhythms" — amplitude fluctuations at the difference frequency.
//  All difference frequencies in this track are chosen to be:
//    - non-integer (cannot be felt as a pulse)
//    - between 0.5 and 3.5 Hz (below rhythm perception threshold of ~4 Hz)
//    - incommensurable with each other
//
//  Structure (36 seconds)
//  ----------------------
//  Four interference pairs, each active for different windows with
//  specific silence gaps between them.  Pairs are not simultaneous
//  except during brief (0.3s) handoff overlaps.
//
//  Pair A — "Difference 2.3 Hz" (0.0s → 9.8s)
//    F1: 220.0 Hz, F2: 222.3 Hz
//    Beat: 2.3 Hz (felt as slow, uneasy oscillation)
//    Combination tones: F2-F1=2.3 Hz (infrasonic), F1+F2=442.3 Hz (near A4)
//    F1 amplitude: 0.052, F2 amplitude: 0.044
//    F1 uses no FM — pure sine. F2 has idx=0.3 (slight FM color).
//    The near-purity of F1 makes the beating more audible.
//
//  Silence gap: 9.8s → 14.1s  (4.3 seconds)
//
//  Pair B — "Difference 1.7 Hz" (14.1s → 22.9s)
//    F1: 174.6 Hz, F2: 176.3 Hz
//    Beat: 1.7 Hz (slower than Pair A — creates temporal disorientation)
//    Both use FM: idx=0.6, idx=1.1 — more complex spectra, less pure beating
//    F1 amplitude: 0.048, F2 amplitude: 0.038
//    A sub oscillator at 87.3 Hz (F1/2) runs beneath, amp 0.022, no FM
//
//  Silence gap: 22.9s → 25.6s  (2.7 seconds — shorter than first gap)
//
//  Pair C — "Difference 3.1 Hz" (25.6s → 31.4s)
//    F1: 311.0 Hz (D#4/Eb4), F2: 314.1 Hz (near Eb but wrong)
//    Beat: 3.1 Hz (fastest pair — more agitated)
//    F1: pure sine, amp=0.040. F2: FM idx=2.2, amp=0.035
//    The high-index FM on F2 with a low-index F1 means the beating
//    is irregular in character: sometimes clean (when F2's sidebands
//    constructively interfere with F1), sometimes muddy.
//
//  Silence gap: 31.4s → 33.7s  (2.3 seconds — shortest gap)
//
//  Pair D — "Difference 0.8 Hz" (33.7s → 36.0s)
//    F1: 261.6 Hz (C4 — only "named" pitch in the track, briefly)
//    F2: 262.4 Hz (C4 + 5 cents — almost unison)
//    Beat: 0.8 Hz (extremely slow — the period of one beat = 1.25s,
//          barely completes one cycle in the 2.3s window)
//    Both pure sine. This is deliberately almost-consonant —
//    the threat of resolution that never arrives.
//    The loop boundary cuts it off before the beat completes.
//    F1 amp: 0.045, F2 amp: 0.045 (equal — maximum beating depth).
//
//  Continuous elements
//  -------------------
//  Background: very quiet sub-bass noise (HPF 18 Hz, LPF 55 Hz, amp 0.011)
//  This provides infrasonic texture that supports the combination tones
//  even when no pairs are active.
//
//  Single long FM voice — "Undertow"
//    C=51 Hz, M=51×2.41421=123.1 Hz (silver ratio), idx=0.5
//    Runs 0s → 36s at amp 0.020, no silence, no gate.
//    Slow amplitude sine-wave: center 0.020, depth 0.010, period 14.7s.
//    Provides harmonic context that all pairs feel like disturbances of.
//
//  Anti-repetition
//  ---------------
//  At the loop boundary (36s), Pair D is mid-beat (0.8 Hz, ~0.64 beats).
//  The crossfade blends the mid-beat state into the start of Pair A.
//  The combination tone at the ear shifts discontinuously — jarring,
//  but not a click.  This creates a non-zero "seam artifact" that is
//  musically appropriate: something wrong at the boundary every time.
//
//  Architecture compliance
//  -----------------------
//  - 36 seconds (§4.1)
//  - Seamless loop: 70 ms crossfade (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence2_3 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 36.0;
    constexpr double FADE_S    = 0.08;
    constexpr size_t XFADE_MS  = 70;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  UTILITY: write a windowed tone — either pure sine or FM
    //  If mod_hz == 0, renders pure sine (idx ignored).
    //  Returns silently if window is out of bounds.
    // =========================================================================
    auto write_tone = [&](double onset_s, double end_s,
                          double carrier_hz, double mod_hz, double mod_idx,
                          float amp,
                          double fade_in_s, double fade_out_s)
    {
        const size_t s0  = static_cast<size_t>(onset_s * SR);
        const size_t s1  = std::min(static_cast<size_t>(end_s * SR), TOTAL);
        if (s0 >= s1) return;
        const size_t len = s1 - s0;

        const double c_inc = wauvio::TWO_PI * carrier_hz / SR;
        const double m_inc = (mod_hz > 0.0) ? wauvio::TWO_PI * mod_hz / SR : 0.0;
        double cp = 0.0, mp = 0.0;

        const size_t fi_n = static_cast<size_t>(fade_in_s  * SR);
        const size_t fo_n = static_cast<size_t>(fade_out_s * SR);

        for (size_t i = 0; i < len; ++i) {
            float env = 1.0f;
            if (i < fi_n && fi_n > 0)
                env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n && fo_n > 0)
                env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);

            const float sample = (mod_hz > 0.0)
                ? static_cast<float>(std::sin(cp + mod_idx * std::sin(mp)))
                : static_cast<float>(std::sin(cp));

            out[s0 + i] += sample * amp * env;

            cp += c_inc;
            if (mod_hz > 0.0) mp += m_inc;
        }
    };

    // =========================================================================
    //  UNDERTOW — continuous low FM drone, amplitude sine-modulated
    // =========================================================================
    {
        constexpr double CU   = 51.0;
        constexpr double MU   = 51.0 * 2.41421356237;   // silver ratio
        constexpr double IDXU = 0.5;
        constexpr double AMP_CENTER = 0.020;
        constexpr double AMP_DEPTH  = 0.010;
        constexpr double AMP_PERIOD = 14.7;

        const double c_inc   = wauvio::TWO_PI * CU / SR;
        const double m_inc   = wauvio::TWO_PI * MU / SR;
        const double amp_inc = wauvio::TWO_PI / (AMP_PERIOD * SR);
        double cp = 0.0, mp = 0.0, ap = 0.0;

        for (size_t i = 0; i < TOTAL; ++i) {
            const float amp = static_cast<float>(
                AMP_CENTER + AMP_DEPTH * std::sin(ap));
            out[i] += static_cast<float>(
                std::sin(cp + IDXU * std::sin(mp))) * amp;
            cp += c_inc;
            mp += m_inc;
            ap += amp_inc;
        }
    }

    // =========================================================================
    //  SUB-BASS NOISE — infrasonic texture, continuous
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0x3C4D5E6Fu);
        wauvio::HighPassFilter hp(18.0, SR);
        wauvio::LowPassFilter  lp(55.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.011f;
        }
    }

    // =========================================================================
    //  PAIR A — "Difference 2.3 Hz"  (0.0s → 9.8s)
    //  F1: 220.0 Hz pure sine
    //  F2: 222.3 Hz, FM idx=0.3
    // =========================================================================
    write_tone(0.0, 9.8,
               220.0, 0.0, 0.0,     // pure sine
               0.052f, 0.15, 0.40);

    write_tone(0.0, 9.8,
               222.3, 222.3 * 1.6180339887, 0.3,
               0.044f, 0.15, 0.40);

    // =========================================================================
    //  PAIR B — "Difference 1.7 Hz"  (14.1s → 22.9s)
    //  F1: 174.6 Hz, FM idx=0.6
    //  F2: 176.3 Hz, FM idx=1.1
    //  Sub: 87.3 Hz pure sine
    // =========================================================================
    write_tone(14.1, 22.9,
               174.6, 174.6 * 1.4142135623730951, 0.6,   // mod = ×√2
               0.048f, 0.20, 0.35);

    write_tone(14.1, 22.9,
               176.3, 176.3 * 2.71828182845, 1.1,         // mod = ×e
               0.038f, 0.20, 0.35);

    // Sub at F1/2
    write_tone(14.1, 22.9,
               87.3, 0.0, 0.0,      // pure sine sub
               0.022f, 0.30, 0.30);

    // =========================================================================
    //  PAIR C — "Difference 3.1 Hz"  (25.6s → 31.4s)
    //  F1: 311.0 Hz pure sine (Eb4-ish, but exact Hz)
    //  F2: 314.1 Hz, FM idx=2.2 (high index — irregular beating character)
    // =========================================================================
    write_tone(25.6, 31.4,
               311.0, 0.0, 0.0,     // pure sine
               0.040f, 0.12, 0.30);

    write_tone(25.6, 31.4,
               314.1, 314.1 * 1.7320508075688772, 2.2,   // mod = ×√3
               0.035f, 0.12, 0.30);

    // =========================================================================
    //  PAIR D — "Difference 0.8 Hz"  (33.7s → 36.0s)
    //  F1: 261.6 Hz pure sine (C4 — deliberate near-consonance)
    //  F2: 262.4 Hz pure sine (C4 + 5 cents)
    //  Equal amplitudes for maximum beating depth
    //  Cut off before beat completes — unresolved at loop boundary
    // =========================================================================
    write_tone(33.7, 36.0,
               261.6, 0.0, 0.0,
               0.045f, 0.10, 0.08);  // short release — loop cuts it

    write_tone(33.7, 36.0,
               262.4, 0.0, 0.0,
               0.045f, 0.10, 0.08);

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

} // namespace sequence2_3
} // namespace soundtrack
