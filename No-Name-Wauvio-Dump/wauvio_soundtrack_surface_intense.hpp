#pragma once

// =============================================================================
//  wauvio_soundtrack_surface_intense.hpp
//  Soundtrack: Surface — Intense
//
//  Concept: METRIC STORM
//  ---------------------
//  The only track in this project to use the DrumSequencer.
//  The step functions are not drum sounds — they are tonal FM bursts,
//  each with a different carrier frequency, modulation index, and
//  amplitude.  The sequencer is used purely for its timing architecture.
//
//  Two independent patterns run simultaneously:
//    Pattern A: 13 steps at 120 BPM × (16/13) = 147.7 BPM effective
//    Pattern B: 17 steps at 120 BPM × (16/17) = 112.9 BPM effective
//
//  Both are sequenced at BPM=120, steps_per_bar=16.
//  Step duration = 60.0 / (120 × 16 / 4) = 0.125s per step.
//  Pattern A has 13 active steps distributed across 16 positions.
//  Pattern B has 17 active steps across 32 positions (wraps).
//
//  The composite pattern (both together) only repeats after
//  LCM(13, 17) = 221 steps = 221 × 0.125s = 27.625 seconds.
//  The loop is 45 seconds long — exactly 1.629 composite cycles.
//  The loop boundary falls in the middle of the second cycle.
//  The listener hears the pattern restart before it finishes.
//
//  Pattern A positions (13 of 16 steps, step indices 0-based):
//    0, 1, 3, 4, 6, 7, 9, 10, 11, 12, 13, 14, 15
//    (Missing: steps 2, 5, 8 — irregular gaps of 2, 2, 2 create triplet feel
//     that fights the straight 16-step grid)
//
//  Pattern B positions (17 of 32 steps):
//    0, 2, 3, 5, 7, 8, 10, 12, 13, 15, 17, 18, 20, 22, 24, 26, 29
//    (Irregular gaps: 2,1,2,2,1,2,2,1,2,2,1,2,2,2,2,3 — varies)
//
//  Sound functions per pattern:
//    Pattern A generates "sting" events:
//      FM burst: C alternates between 4 carriers [97, 143, 211, 73] Hz
//      across steps (cycling), M = C × φ, idx = 1.8, dur = 0.09s
//      Amp: 0.048, attack 0.004s, release 0.075s
//
//    Pattern B generates "shadow" events:
//      FM burst: C alternates between [167, 89, 241] Hz
//      M = C × ∛3, idx = 0.9, dur = 0.12s
//      Amp: 0.038, attack 0.006s, release 0.100s
//
//  Both patterns play simultaneously.  Their collisions (when both fire
//  on the same beat) produce momentary spectral collisions that have
//  no regular period — they appear as unpredictable accents.
//
//  Continuous drone — "The Storm Ground":
//    C=61 Hz, M=61×(1+√5)/2=98.7 Hz (φ), idx=0.7, amp=0.030
//    Amplitude LFO: sine, period=8.3s, depth=0.010 around center 0.030
//    This is not rhythmic.  It holds the texture together.
//
//  Duration: 45 seconds
//  Crossfade: 100ms
//  Normalize: 0.70
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace surface_intense {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 45.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 100;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  STORM GROUND — continuous FM drone
    // =========================================================================
    {
        constexpr double CG    = 61.0;
        constexpr double MG    = 61.0 * 1.6180339887;  // φ
        constexpr double IDXG  = 0.7;
        constexpr double A_CTR = 0.030;
        constexpr double A_DEP = 0.010;
        constexpr double A_PER = 8.3;
        const double c_inc = wauvio::TWO_PI * CG / SR;
        const double m_inc = wauvio::TWO_PI * MG / SR;
        const double a_inc = wauvio::TWO_PI / (A_PER * SR);
        double cp = 0.0, mp = 0.0, ap = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            const float amp = static_cast<float>(A_CTR + A_DEP * std::sin(ap));
            out[i] += static_cast<float>(std::sin(cp + IDXG * std::sin(mp))) * amp;
            cp += c_inc; mp += m_inc; ap += a_inc;
        }
    }

    // =========================================================================
    //  SEQUENCER SETUP
    // =========================================================================
    constexpr double BPM           = 120.0;
    constexpr int    STEPS_PER_BAR = 16;
    constexpr double STEP_DUR      = 60.0 / (BPM * STEPS_PER_BAR / 4.0);
    // STEP_DUR = 60.0 / (120 * 4) = 0.125s

    // =========================================================================
    //  PATTERN A — 13 steps across 16 positions
    //  Step bitmask: 1110110110111111 (positions 0,1,3,4,6,7,9,10,11,12,13,14,15)
    // =========================================================================
    {
        constexpr bool PA[16] = {
            true, true, false, true, true, false, true, true,
            false,true, true,  true, true, true,  true, true
        };

        // Carrier frequency cycles across 4 values
        constexpr double CARRIERS_A[] = { 97.0, 143.0, 211.0, 73.0 };
        constexpr int N_CA = 4;
        constexpr double PHI = 1.6180339887;
        constexpr double IDX_A  = 1.8;
        constexpr double DUR_A  = 0.090;
        constexpr float  AMP_A  = 0.048f;
        constexpr double ATK_A  = 0.004;
        constexpr double REL_A  = 0.075;

        int carrier_idx = 0;
        int num_bars = static_cast<int>(std::ceil(DUR / (STEPS_PER_BAR * STEP_DUR))) + 1;

        for (int bar = 0; bar < num_bars; ++bar) {
            for (int step = 0; step < STEPS_PER_BAR; ++step) {
                if (!PA[step]) continue;

                const double onset = (bar * STEPS_PER_BAR + step) * STEP_DUR;
                if (onset >= DUR) break;

                const double c = CARRIERS_A[carrier_idx % N_CA];
                const double m = c * PHI;
                const size_t s0  = static_cast<size_t>(onset * SR);
                const size_t n   = static_cast<size_t>(DUR_A * SR);
                const size_t end = std::min(s0 + n, TOTAL);
                const size_t len = end - s0;
                const size_t fi_n = static_cast<size_t>(ATK_A * SR);
                const size_t fo_n = static_cast<size_t>(REL_A * SR);
                double cp = 0.0, mp = 0.0;
                for (size_t i = 0; i < len; ++i) {
                    float env = 1.0f;
                    if (i < fi_n && fi_n > 0) env = static_cast<float>(i) / static_cast<float>(fi_n);
                    else if (i >= len - fo_n && fo_n > 0) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);
                    out[s0 + i] += static_cast<float>(
                        std::sin(cp + IDX_A * std::sin(mp))) * AMP_A * env;
                    cp += wauvio::TWO_PI * c / SR;
                    mp += wauvio::TWO_PI * m / SR;
                }
                carrier_idx++;
            }
        }
    }

    // =========================================================================
    //  PATTERN B — 17 steps across 32 positions
    //  Step indices: 0,2,3,5,7,8,10,12,13,15,17,18,20,22,24,26,29
    // =========================================================================
    {
        constexpr bool PB[32] = {
            true,  false, true,  true,  false, true,  false, true,
            true,  false, true,  false, true,  true,  false, true,
            false, true,  true,  false, true,  false, true,  false,
            true,  false, true,  false, false, true,  false, false
        };
        constexpr int STEPS_B = 32;

        constexpr double CARRIERS_B[] = { 167.0, 89.0, 241.0 };
        constexpr int N_CB = 3;
        constexpr double CBRT3 = 1.44224957030741;
        constexpr double IDX_B  = 0.9;
        constexpr double DUR_B  = 0.120;
        constexpr float  AMP_B  = 0.038f;
        constexpr double ATK_B  = 0.006;
        constexpr double REL_B  = 0.100;

        int carrier_idx = 0;
        int num_bars = static_cast<int>(std::ceil(DUR / (STEPS_B * STEP_DUR))) + 1;

        for (int bar = 0; bar < num_bars; ++bar) {
            for (int step = 0; step < STEPS_B; ++step) {
                if (!PB[step]) continue;

                const double onset = (bar * STEPS_B + step) * STEP_DUR;
                if (onset >= DUR) break;

                const double c = CARRIERS_B[carrier_idx % N_CB];
                const double m = c * CBRT3;
                const size_t s0  = static_cast<size_t>(onset * SR);
                const size_t n   = static_cast<size_t>(DUR_B * SR);
                const size_t end = std::min(s0 + n, TOTAL);
                const size_t len = end - s0;
                const size_t fi_n = static_cast<size_t>(ATK_B * SR);
                const size_t fo_n = static_cast<size_t>(REL_B * SR);
                double cp = 0.0, mp = 0.0;
                for (size_t i = 0; i < len; ++i) {
                    float env = 1.0f;
                    if (i < fi_n && fi_n > 0) env = static_cast<float>(i) / static_cast<float>(fi_n);
                    else if (i >= len - fo_n && fo_n > 0) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);
                    out[s0 + i] += static_cast<float>(
                        std::sin(cp + IDX_B * std::sin(mp))) * AMP_B * env;
                    cp += wauvio::TWO_PI * c / SR;
                    mp += wauvio::TWO_PI * m / SR;
                }
                carrier_idx++;
            }
        }
    }

    // =========================================================================
    //  BACKGROUND NOISE
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0x33221100u);
        wauvio::HighPassFilter hp(800.0,  SR);
        wauvio::LowPassFilter  lp(2500.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
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

} // namespace surface_intense
} // namespace soundtrack
