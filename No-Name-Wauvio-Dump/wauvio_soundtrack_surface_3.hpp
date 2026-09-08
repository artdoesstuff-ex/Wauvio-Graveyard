#pragma once

// =============================================================================
//  wauvio_soundtrack_surface_3.hpp
//  Soundtrack: Surface — Track 3
//
//  Concept: PHASE LATTICE
//  ----------------------
//  Three oscillator pairs.  Each pair consists of two FM voices detuned
//  by a specific amount, producing a beating rate at their difference:
//
//    Pair 1: F=148.0 Hz vs F=148.3 Hz → beat = 0.3 Hz  (very slow)
//    Pair 2: F=213.0 Hz vs F=213.7 Hz → beat = 0.7 Hz
//    Pair 3: F= 97.0 Hz vs F= 98.9 Hz → beat = 1.9 Hz  (fastest)
//
//  Beat rates 0.3, 0.7, 1.9 are pairwise incommensurable — no two will
//  simultaneously reach a peak except at their LCM, which exceeds any
//  reasonable loop length.
//
//  Amplitude rotation:
//  -------------------
//  A 3×3 amplitude matrix governs the relative loudness of the three pairs.
//  The matrix is updated at seven irregular timestamps:
//    0.0s, 7.3s, 16.1s, 22.8s, 31.4s, 38.7s, 46.3s (→ 53s loop end)
//  At each update, the matrix rotates: pair amplitudes cycle through
//  three configurations, so that whichever pair was loudest becomes
//  quietest on the next step.  The rotation is clockwise in amplitude
//  space.  Transitions use a 1.5s linear fade between configurations.
//
//  Configuration table (amp for pairs [1, 2, 3]):
//    Config 0:  [0.055, 0.022, 0.012]  — Pair 1 dominant
//    Config 1:  [0.012, 0.055, 0.022]  — Pair 2 dominant
//    Config 2:  [0.022, 0.012, 0.055]  — Pair 3 dominant
//
//  Transitions: at each timestamp, interpolate linearly over 1.5s from
//  current config to next config.
//
//  FM parameters per pair:
//    Each voice: mod = carrier × √3, index per table below.
//    Lower-frequency pairs use higher index (rounder sound at low pitch).
//
//    Pair 1 (148 Hz region): index 0.9 / 1.1 (slightly different per voice)
//    Pair 2 (213 Hz region): index 0.6 / 0.8
//    Pair 3 ( 97 Hz region): index 1.5 / 1.7
//
//  Two noise layers:
//    Texture noise: HPF 300 Hz, LPF 900 Hz, amp 0.008
//    Sub presence:  HPF  20 Hz, LPF  60 Hz, amp 0.014
//
//  Duration: 53 seconds
//  Crossfade: 95ms
//  Normalize: 0.70
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace surface_3 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 53.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 95;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  AMPLITUDE CONFIG TABLE
    // =========================================================================
    constexpr float CONFIGS[3][3] = {
        { 0.055f, 0.022f, 0.012f },
        { 0.012f, 0.055f, 0.022f },
        { 0.022f, 0.012f, 0.055f },
    };

    // Rotation schedule: (onset_s, config_index)
    struct RotStep { double onset_s; int cfg; };
    const RotStep STEPS[] = {
        {  0.0, 0 }, {  7.3, 1 }, { 16.1, 2 }, { 22.8, 0 },
        { 31.4, 1 }, { 38.7, 2 }, { 46.3, 0 },
    };
    constexpr int    N_STEPS     = 7;
    constexpr double TRANS_S     = 1.5;   // transition duration

    // Precompute per-sample amplitude for each of the three pairs
    // by interpolating between rotation configs
    float amp_sched[3][1] = {};  // placeholder — will allocate dynamically below

    // Use three amplitude buffers
    std::vector<float> amp0(TOTAL), amp1(TOTAL), amp2(TOTAL);

    for (size_t i = 0; i < TOTAL; ++i) {
        const double t = static_cast<double>(i) / SR;

        // Find current and next step
        int cur_step = 0;
        for (int s = N_STEPS - 1; s >= 0; --s) {
            if (t >= STEPS[s].onset_s) { cur_step = s; break; }
        }
        int nxt_step = (cur_step + 1 < N_STEPS) ? cur_step + 1 : cur_step;

        const int    cfg_cur = STEPS[cur_step].cfg;
        const int    cfg_nxt = STEPS[nxt_step].cfg;
        const double t0      = STEPS[cur_step].onset_s;
        const double t1      = (nxt_step != cur_step)
            ? STEPS[nxt_step].onset_s : DUR;

        // Interpolation factor within transition window
        float frac = 0.0f;
        if (nxt_step != cur_step && t >= t0 && t < t0 + TRANS_S) {
            frac = static_cast<float>((t - t0) / TRANS_S);
        } else if (t >= t0 + TRANS_S) {
            frac = 1.0f;
        }

        amp0[i] = CONFIGS[cfg_cur][0] * (1.f - frac) + CONFIGS[cfg_nxt][0] * frac;
        amp1[i] = CONFIGS[cfg_cur][1] * (1.f - frac) + CONFIGS[cfg_nxt][1] * frac;
        amp2[i] = CONFIGS[cfg_cur][2] * (1.f - frac) + CONFIGS[cfg_nxt][2] * frac;
    }

    // =========================================================================
    //  PAIR 1: 148.0 Hz / 148.3 Hz  (beat 0.3 Hz)
    // =========================================================================
    {
        constexpr double MOD_RATIO = 1.7320508075688772935;  // √3
        const double c1 = 148.0, c2 = 148.3;
        const double m1 = c1 * MOD_RATIO, m2 = c2 * MOD_RATIO;
        const double c1_inc = wauvio::TWO_PI * c1 / SR;
        const double c2_inc = wauvio::TWO_PI * c2 / SR;
        const double m1_inc = wauvio::TWO_PI * m1 / SR;
        const double m2_inc = wauvio::TWO_PI * m2 / SR;
        double cp1 = 0.0, cp2 = 0.0, mp1 = 0.0, mp2 = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            out[i] += static_cast<float>(std::sin(cp1 + 0.9 * std::sin(mp1))) * amp0[i];
            out[i] += static_cast<float>(std::sin(cp2 + 1.1 * std::sin(mp2))) * amp0[i];
            cp1 += c1_inc; cp2 += c2_inc;
            mp1 += m1_inc; mp2 += m2_inc;
        }
    }

    // =========================================================================
    //  PAIR 2: 213.0 Hz / 213.7 Hz  (beat 0.7 Hz)
    // =========================================================================
    {
        constexpr double MOD_RATIO = 1.7320508075688772935;
        const double c1 = 213.0, c2 = 213.7;
        const double m1 = c1 * MOD_RATIO, m2 = c2 * MOD_RATIO;
        const double c1_inc = wauvio::TWO_PI * c1 / SR;
        const double c2_inc = wauvio::TWO_PI * c2 / SR;
        const double m1_inc = wauvio::TWO_PI * m1 / SR;
        const double m2_inc = wauvio::TWO_PI * m2 / SR;
        double cp1 = 0.0, cp2 = 0.0, mp1 = 0.0, mp2 = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            out[i] += static_cast<float>(std::sin(cp1 + 0.6 * std::sin(mp1))) * amp1[i];
            out[i] += static_cast<float>(std::sin(cp2 + 0.8 * std::sin(mp2))) * amp1[i];
            cp1 += c1_inc; cp2 += c2_inc;
            mp1 += m1_inc; mp2 += m2_inc;
        }
    }

    // =========================================================================
    //  PAIR 3: 97.0 Hz / 98.9 Hz  (beat 1.9 Hz)
    // =========================================================================
    {
        constexpr double MOD_RATIO = 1.7320508075688772935;
        const double c1 = 97.0, c2 = 98.9;
        const double m1 = c1 * MOD_RATIO, m2 = c2 * MOD_RATIO;
        const double c1_inc = wauvio::TWO_PI * c1 / SR;
        const double c2_inc = wauvio::TWO_PI * c2 / SR;
        const double m1_inc = wauvio::TWO_PI * m1 / SR;
        const double m2_inc = wauvio::TWO_PI * m2 / SR;
        double cp1 = 0.0, cp2 = 0.0, mp1 = 0.0, mp2 = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            out[i] += static_cast<float>(std::sin(cp1 + 1.5 * std::sin(mp1))) * amp2[i];
            out[i] += static_cast<float>(std::sin(cp2 + 1.7 * std::sin(mp2))) * amp2[i];
            cp1 += c1_inc; cp2 += c2_inc;
            mp1 += m1_inc; mp2 += m2_inc;
        }
    }

    // =========================================================================
    //  NOISE LAYERS
    // =========================================================================
    {
        wauvio::NoiseGenerator ng_tex(0x12AB34CDu);
        wauvio::HighPassFilter hp_tex(300.0, SR);
        wauvio::LowPassFilter  lp_tex(900.0, SR);

        wauvio::NoiseGenerator ng_sub(0xEF56AB78u);
        wauvio::HighPassFilter hp_sub(20.0,  SR);
        wauvio::LowPassFilter  lp_sub(60.0,  SR);

        for (size_t i = 0; i < TOTAL; ++i) {
            float t = ng_tex.tick();
            t = hp_tex.tick(t); t = lp_tex.tick(t);
            out[i] += t * 0.008f;

            float s = ng_sub.tick();
            s = hp_sub.tick(s); s = lp_sub.tick(s);
            out[i] += s * 0.014f;
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

} // namespace surface_3
} // namespace soundtrack
