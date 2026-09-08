#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence2_intense_final.hpp
//  Soundtrack: Sequence2 — Intense Final
//
//  Design brief
//  ------------
//  This is the last music the player hears in Sequence2 before death
//  or transition.  It plays when the spawn interval is 5 seconds and
//  the player has been in the dark too long.
//
//  Every other track in this project was built on the premise that
//  the listener should never find a pulse — because a pulse implies
//  predictability, and predictability implies safety.
//
//  This track reverses that contract.  It gives the listener a pulse.
//  Then it destroys it.
//
//  The destruction is not violent — it is gradual and inevitable,
//  like watching something structural fail in slow motion.
//  By the time the loop reaches its end, what began as a near-rhythm
//  has split into two incommensurable streams, one of which accelerates
//  past the point of coherence while the other collapses into silence.
//
//  Concept: RHYTHMIC COLLAPSE
//  Synthesis approach: the only track in the project to use Supersaw.
//  The supersaw provides a "wall of sound" quality during the structured
//  phase — something recognizable, almost musical, almost grounded.
//  Its subsequent erosion into detuned FM fragments is more disturbing
//  precisely because the listener had something to hold.
//
//  Structure (52 seconds)
//  ----------------------
//
//  PHASE 1 — "The Pulse"  (0.0s → 13.8s)
//    Supersaw: 7 voices, root 98 Hz (G2), detune 18 cents.
//    Amplitude: 0.055.
//    Amplitude is gated by a near-regular percussive envelope:
//      Hits at: 0.0, 1.35, 2.6, 4.05, 5.2, 6.7, 7.9, 9.4, 10.5, 12.1, 13.1
//      Inter-onset intervals: 1.35, 1.25, 1.45, 1.15, 1.50, 1.20, 1.50, 1.10, 1.60, 1.00
//      Average IOI: ~1.31s (≈ 46 BPM).  Close to a pulse but not one.
//      The IOIs never repeat consecutively.  The listener hears "rhythm"
//      even though no two consecutive hits are equally spaced.
//    Each hit: 15ms attack, hold duration = IOI × 0.6, 80ms release.
//    A sub FM voice at 49 Hz (G1) sustains continuously beneath the hits.
//
//  PHASE 2 — "The Split"  (13.8s → 29.4s)
//    The supersaw CONTINUES but its gate events begin to diverge.
//    Two separate streams emerge from the single pulse:
//      Stream A: continues the near-pulse at increasing IOI (slowing down)
//        IOIs: 1.6, 1.9, 2.3, 2.8, 3.5 (each × ~1.23 of previous)
//        Hits at: 15.4, 17.0, 18.9, 21.2, 24.0, 27.5
//      Stream B: a new FM voice (carrier 211 Hz, mod 211×φ=341.5 Hz, idx=2.4)
//        Appearing at the gaps between Stream A hits.
//        IOIs: 0.95, 0.87, 0.79, 0.72, 0.65 (accelerating)
//        Hits at: 14.7, 15.9, 17.1, 18.3, 19.3, 20.1, 20.9, 21.6
//    By 24s: Stream A hits are ~3.5s apart (barely perceptible as rhythm).
//    By 24s: Stream B hits are ~0.6s apart (too fast to feel like rhythm).
//    Between 24.0s and 29.4s: only Stream B remains, accelerating.
//    At 29.4s: Stream B reaches 0.5s IOI.  It cuts out completely.
//
//  SILENCE  (29.4s → 34.1s)  — 4.7 seconds
//    The first true silence in this track.  After 29 seconds of motion,
//    stopping is more frightening than any new sound.
//    Only the sub FM voice at 49 Hz persists, very quietly (amp 0.018).
//    And background noise.
//
//  PHASE 3 — "The Fragment"  (34.1s → 52.0s)
//    The supersaw does not return.  Instead, 3 detuned FM voices enter
//    in rapid succession, each drifting further from the others.
//    No gating — continuous from entry.  No rhythm.  Accumulation without
//    the sense of accumulation, because there is no anchor to build from.
//
//    V1 enters 34.1s: C=119 Hz, M=119×∛5=203.5 Hz, idx=1.8, amp=0.040
//    V2 enters 36.9s: C=121.7 Hz (2.7 Hz above V1 — beating at 2.7 Hz)
//               M=121.7×√7=322.2 Hz, idx=1.3, amp=0.035
//    V3 enters 40.3s: C=77 Hz, M=77×(π+1)=319.9 Hz, idx=2.6, amp=0.030
//
//    All three run to 52s.  No silence.  No gate.
//    The loop boundary cuts them off — they do not fade out.
//    The crossfade handles the boundary.
//
//  Sub voice at 49 Hz runs the entire 52 seconds — same as Phase 1.
//  It is the only element that survives Phase 1 into the end.
//  But by Phase 3 it is buried under the FM cluster.  Lost, not gone.
//
//  Anti-repetition
//  ---------------
//  The supersaw's voice phases at loop start differ from loop start because
//  Supersaw::render() uses a fresh phase=0.0 each call — BUT the gate
//  envelope starts identically, so Phase 1 is always identical on every loop.
//  This is intentional: the near-pulse must feel recognizable on each pass
//  to make its collapse register.  The Fragment phase (34.1–52s), however,
//  accumulates at incommensurable FM frequencies — each loop iteration
//  places the fragment cluster at different phase states.
//
//  Architecture compliance
//  -----------------------
//  - 52 seconds (§4.1)
//  - Seamless loop: 90 ms crossfade (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence2_intense_final {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 52.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 90;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  UTILITY: write a windowed FM event
    // =========================================================================
    auto write_fm = [&](double onset_s, double dur_s,
                        double carrier_hz, double mod_hz, double mod_idx,
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
        const size_t fi_n = static_cast<size_t>(attack_s  * SR);
        const size_t fo_n = static_cast<size_t>(release_s * SR);

        for (size_t i = 0; i < len; ++i) {
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
    //  UTILITY: write a windowed Supersaw hit
    //  Uses Supersaw::render() on a short buffer, then mixes at offset.
    // =========================================================================
    auto write_saw_hit = [&](double onset_s, double dur_s, float amp) {
        const size_t s0  = static_cast<size_t>(onset_s * SR);
        const size_t end = std::min(s0 + static_cast<size_t>(dur_s * SR), TOTAL);
        const size_t len = end - s0;
        if (len == 0) return;

        wauvio::Supersaw saw;
        saw.voices       = 7;
        saw.frequency    = 98.0;
        saw.detune_cents = 18.0;
        saw.amplitude    = static_cast<double>(amp);
        saw.mix_center   = 0.65;

        wauvio::Buffer hit = saw.render(dur_s, SR);

        // Attack: 15ms, Release: 80ms
        constexpr double ATK_S = 0.015, REL_S = 0.080;
        const size_t fi_n = static_cast<size_t>(ATK_S * SR);
        const size_t fo_n = static_cast<size_t>(REL_S * SR);

        for (size_t i = 0; i < len && i < hit.size(); ++i) {
            float env = 1.0f;
            if (i < fi_n) env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);
            out[s0 + i] += hit[i] * env;
        }
    };

    // =========================================================================
    //  SUB FM VOICE — 49 Hz, runs entire buffer
    //  Phase 1: amp 0.030, Phase 3: partially buried but still there
    //  Implemented as continuous — normalize handles the level
    // =========================================================================
    {
        constexpr double CS   = 49.0;
        constexpr double MS   = 49.0 * 1.6180339887;  // φ
        constexpr double IDXS = 0.45;
        constexpr float  AMPS = 0.030f;
        const double c_inc = wauvio::TWO_PI * CS / SR;
        const double m_inc = wauvio::TWO_PI * MS / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            out[i] += static_cast<float>(
                std::sin(cp + IDXS * std::sin(mp))) * AMPS;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // =========================================================================
    //  PHASE 1 — "The Pulse"  0.0s → 13.8s
    //  Supersaw hits at asymmetric IOIs
    //  IOIs: 1.35, 1.25, 1.45, 1.15, 1.50, 1.20, 1.50, 1.10, 1.60, 1.00
    //  Hits:  0.00, 1.35, 2.60, 4.05, 5.20, 6.70, 7.90, 9.40, 10.50, 12.10, 13.10
    //  Duration per hit: IOI × 0.65, capped at 1.5s, min 0.5s
    // =========================================================================
    {
        const double hit_onsets[] = {
             0.00,  1.35,  2.60,  4.05,  5.20,
             6.70,  7.90,  9.40, 10.50, 12.10, 13.10
        };
        const double hit_ioi[] = {
            1.35, 1.25, 1.45, 1.15, 1.50,
            1.20, 1.50, 1.10, 1.60, 1.00, 0.70   // last IOI: distance to 13.80
        };
        constexpr int N_HITS1 = 11;

        for (int h = 0; h < N_HITS1; ++h) {
            const double dur = std::max(0.5, std::min(1.5, hit_ioi[h] * 0.65));
            write_saw_hit(hit_onsets[h], dur, 0.055f);
        }
    }

    // =========================================================================
    //  PHASE 2 — "The Split"  13.8s → 29.4s
    //
    //  Stream A: supersaw slowing down
    //    Onsets: 15.4, 17.0, 18.9, 21.2, 24.0, 27.5
    //    IOIs:    1.6,  1.9,  2.3,  2.8,  3.5
    //    Duration: IOI × 0.55
    //
    //  Stream B: FM (C=211 Hz, M=341.5 Hz, idx=2.4) accelerating
    //    Onsets: 14.7, 15.9, 17.1, 18.3, 19.3, 20.1, 20.9, 21.6, 22.2, 22.7,
    //            23.2, 23.7, 24.1, 24.5, 24.9, 25.3, 25.7, 26.1, 26.5, 26.9,
    //            27.3, 27.7, 28.1, 28.5, 28.9, 29.3
    //    IOIs beginning at 1.2, decreasing to 0.4 geometrically
    //    Hit duration: 0.25s constant
    // =========================================================================
    {
        // Stream A — supersaw, slowing
        const double sa_onsets[] = { 15.4, 17.0, 18.9, 21.2, 24.0, 27.5 };
        const double sa_iois[]   = {  1.6,  1.9,  2.3,  2.8,  3.5,  2.5 };
        constexpr int N_SA = 6;
        for (int h = 0; h < N_SA; ++h) {
            const double dur = std::max(0.4, std::min(1.8, sa_iois[h] * 0.55));
            write_saw_hit(sa_onsets[h], dur, 0.042f);  // slightly softer than Phase 1
        }

        // Stream B — FM, accelerating
        // Generate onset times procedurally: each IOI = prev × 0.88
        constexpr double PHI = 1.6180339887;
        double onset  = 14.7;
        double ioi    = 1.2;
        constexpr double IOI_DECAY = 0.88;
        constexpr double MIN_IOI   = 0.40;
        constexpr double END_T     = 29.4;
        constexpr float  SB_AMP    = 0.044f;
        constexpr double SB_DUR    = 0.25;

        while (onset < END_T && ioi >= MIN_IOI) {
            write_fm(onset, SB_DUR,
                     211.0, 211.0 * PHI, 2.4,
                     SB_AMP,
                     0.010, 0.080);
            onset += ioi;
            ioi   *= IOI_DECAY;
        }
    }

    // =========================================================================
    //  SILENCE ZONE — 29.4s → 34.1s  (4.7 seconds)
    //  Sub FM voice continues at reduced visibility (normalized away).
    //  Background noise only.  Intentionally empty.
    // =========================================================================

    // =========================================================================
    //  PHASE 3 — "The Fragment"  34.1s → 52.0s
    //  Three FM voices, entered sequentially, no gating, no exit
    // =========================================================================
    {
        constexpr double PI_PLUS1 = 3.14159265358979323846 + 1.0;

        // V1: 34.1s — continuous FM
        {
            constexpr double ONSET = 34.1;
            constexpr double ENDV  = 52.0;
            constexpr double C1 = 119.0;
            constexpr double M1 = 119.0 * 1.7099759466766967976799;  // ∛5
            constexpr double I1 = 1.8;
            constexpr float  A1 = 0.040f;
            const size_t s0  = static_cast<size_t>(ONSET * SR);
            const size_t s1  = std::min(static_cast<size_t>(ENDV  * SR), TOTAL);
            const size_t len = s1 - s0;
            const double c_inc = wauvio::TWO_PI * C1 / SR;
            const double m_inc = wauvio::TWO_PI * M1 / SR;
            const size_t fi_n  = static_cast<size_t>(1.2 * SR);
            double cp = 0.0, mp = 0.0;
            for (size_t i = 0; i < len; ++i) {
                const float env = (i < fi_n)
                    ? static_cast<float>(i) / static_cast<float>(fi_n)
                    : 1.0f;
                out[s0 + i] += static_cast<float>(
                    std::sin(cp + I1 * std::sin(mp))) * A1 * env;
                cp += c_inc;
                mp += m_inc;
            }
        }

        // V2: 36.9s — beats at 2.7 Hz against V1
        {
            constexpr double ONSET = 36.9;
            constexpr double ENDV  = 52.0;
            constexpr double C2 = 121.7;  // V1 carrier + 2.7 Hz → beats at 2.7 Hz
            constexpr double M2 = 121.7 * 2.6457513110645905905016;  // √7
            constexpr double I2 = 1.3;
            constexpr float  A2 = 0.035f;
            const size_t s0  = static_cast<size_t>(ONSET * SR);
            const size_t s1  = std::min(static_cast<size_t>(ENDV  * SR), TOTAL);
            const size_t len = s1 - s0;
            const double c_inc = wauvio::TWO_PI * C2 / SR;
            const double m_inc = wauvio::TWO_PI * M2 / SR;
            const size_t fi_n  = static_cast<size_t>(0.9 * SR);
            double cp = 0.0, mp = 0.0;
            for (size_t i = 0; i < len; ++i) {
                const float env = (i < fi_n)
                    ? static_cast<float>(i) / static_cast<float>(fi_n)
                    : 1.0f;
                out[s0 + i] += static_cast<float>(
                    std::sin(cp + I2 * std::sin(mp))) * A2 * env;
                cp += c_inc;
                mp += m_inc;
            }
        }

        // V3: 40.3s — low register, high index
        {
            constexpr double ONSET = 40.3;
            constexpr double ENDV  = 52.0;
            constexpr double C3 = 77.0;
            constexpr double M3 = 77.0 * PI_PLUS1;   // π+1
            constexpr double I3 = 2.6;
            constexpr float  A3 = 0.030f;
            const size_t s0  = static_cast<size_t>(ONSET * SR);
            const size_t s1  = std::min(static_cast<size_t>(ENDV  * SR), TOTAL);
            const size_t len = s1 - s0;
            const double c_inc = wauvio::TWO_PI * C3 / SR;
            const double m_inc = wauvio::TWO_PI * M3 / SR;
            const size_t fi_n  = static_cast<size_t>(1.5 * SR);
            double cp = 0.0, mp = 0.0;
            for (size_t i = 0; i < len; ++i) {
                const float env = (i < fi_n)
                    ? static_cast<float>(i) / static_cast<float>(fi_n)
                    : 1.0f;
                out[s0 + i] += static_cast<float>(
                    std::sin(cp + I3 * std::sin(mp))) * A3 * env;
                cp += c_inc;
                mp += m_inc;
            }
        }
    }

    // =========================================================================
    //  BACKGROUND NOISE — continuous, upper-mid
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xC1D2E3F4u);
        wauvio::HighPassFilter hp(700.0,  SR);
        wauvio::LowPassFilter  lp(2400.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.007f;
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

} // namespace sequence2_intense_final
} // namespace soundtrack
