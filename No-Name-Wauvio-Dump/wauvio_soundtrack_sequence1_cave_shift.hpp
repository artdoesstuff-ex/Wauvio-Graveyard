#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence1_cave_shift.hpp
//  Soundtrack: Sequence1 — Cave Shift
//
//  Design brief
//  ------------
//  This track plays during the CaveSystem's distortion phase — when
//  ctx.world.caveDistort first becomes true and the walls start wrong.
//  Something structural is moving.  Not fast.  Not threatening yet.
//  Just wrong.  Like a building settling, but it is not a building.
//
//  Concept: TECTONIC STRATA
//  The track is built from five independent "strata" — FM voices each
//  running at their own unrelated tempo.  None of them are aware of
//  the others.  They do not align.  They do not resolve.
//  The listener cannot find a pulse because there is no pulse.
//  There are only five slow pressures, independently grinding.
//
//  Architecture (43 seconds)
//  -------------------------
//  No discrete "sections".  This track has no silence gaps.
//  Instead: five continuous FM voices at very different rates.
//
//  Stratum 1 — "Stone" (runs 0s → 43s)
//    Carrier: 29 Hz. Mod: 29 × φ². Index oscillates via a 17.3s LFO.
//    This is below musical pitch — felt, not heard.
//    Amplitude swell LFO period: 23.7s.
//
//  Stratum 2 — "Water" (runs 0s → 43s, offset 5.1s phase)
//    Carrier: 67.4 Hz. Mod: 67.4 × √5. Index LFO period: 11.8s.
//    Amplitude LFO period: 31.1s.
//    The phase offset means it enters mid-swell at loop start.
//
//  Stratum 3 — "Air" (runs 0s → 43s, offset 12.7s phase)
//    Carrier: 142 Hz. Mod: 142 × (1+√5)/2 (also φ, different octave).
//    Index LFO period: 7.2s — fastest stratum, most nervous.
//    Amplitude LFO period: 19.4s.
//
//  Stratum 4 — "Void" (enters at 8.3s, runs to 43s)
//    Carrier: 211 Hz. Mod: 211 × ∛2 = 265.8 Hz (cube root of 2).
//    Index modulation: slow random-walk approximated via stacked LFOs.
//    Amplitude: constant 0.030, no LFO — the only stable element,
//    which makes it the most unsettling.
//
//  Stratum 5 — "Fault" (enters at 21.9s, runs to 38.4s, then fades)
//    Carrier: 89 Hz. Mod: 89 × π.
//    Hard frequency interrupt: at t=30.1s within its window (8.2s in),
//    the modulator abruptly jumps by +17 Hz for 1.4s then returns.
//    Simulates a crack propagating through stone.
//    Amplitude: 0.038.
//
//  Anti-repetition mechanism
//  -------------------------
//  LFO periods for all five strata are mutually incommensurable:
//    17.3, 23.7, 11.8, 31.1, 7.2, 19.4 — no common divisor with 43.
//  The loop will sound different each time it repeats because no two
//  strata are in the same phase relationship at the loop boundary.
//
//  Architecture compliance
//  -----------------------
//  - 43 seconds (§4.1)
//  - Seamless loop: 90 ms crossfade (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence1_cave_shift {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 43.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 90;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  UTILITY: write a stratum — a continuous FM voice with two independent
    //  LFOs: one modulating the FM index, one modulating amplitude.
    //  onset_s:     when to start (phase of amplitude LFO is pre-wound
    //               by onset_s to simulate a mid-swell entry)
    //  end_s:       when to end (with fade_out_s release before end)
    //  carrier_hz:  FM carrier frequency
    //  mod_hz:      FM modulator frequency
    //  idx_center:  FM index center value
    //  idx_depth:   FM index LFO depth (±idx_depth around idx_center)
    //  idx_period:  FM index LFO period in seconds
    //  idx_phase0:  starting phase of index LFO (radians)
    //  amp_center:  amplitude center
    //  amp_depth:   amplitude LFO depth (±amp_depth around amp_center)
    //  amp_period:  amplitude LFO period in seconds
    //  amp_phase0:  starting phase of amplitude LFO
    //  fade_in_s:   voice fade-in duration
    //  fade_out_s:  voice fade-out duration
    // =========================================================================
    auto write_stratum = [&](
        double onset_s, double end_s,
        double carrier_hz, double mod_hz,
        double idx_center, double idx_depth, double idx_period, double idx_phase0,
        double amp_center, double amp_depth, double amp_period, double amp_phase0,
        double fade_in_s, double fade_out_s)
    {
        const size_t s0  = static_cast<size_t>(onset_s * SR);
        const size_t s1  = std::min(static_cast<size_t>(end_s * SR), TOTAL);
        if (s0 >= s1) return;
        const size_t len = s1 - s0;

        const double c_inc       = wauvio::TWO_PI * carrier_hz / SR;
        const double m_inc       = wauvio::TWO_PI * mod_hz     / SR;
        const double idx_lfo_inc = wauvio::TWO_PI / (idx_period * SR);
        const double amp_lfo_inc = wauvio::TWO_PI / (amp_period * SR);

        double cp       = 0.0;
        double mp       = 0.0;
        double idx_phi  = idx_phase0;
        double amp_phi  = amp_phase0;

        const size_t fi_n = static_cast<size_t>(fade_in_s  * SR);
        const size_t fo_n = static_cast<size_t>(fade_out_s * SR);

        for (size_t i = 0; i < len; ++i) {
            // FM index from LFO
            const double mod_idx = idx_center + idx_depth * std::sin(idx_phi);
            // Amplitude from LFO — clamp to [0, 1] for safety
            const double amp_raw = amp_center + amp_depth * std::sin(amp_phi);
            const float  amp     = static_cast<float>(std::max(0.0, amp_raw));

            // Per-voice fade envelope
            float env = 1.0f;
            if (i < fi_n && fi_n > 0)
                env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n && fo_n > 0)
                env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);

            out[s0 + i] += static_cast<float>(
                std::sin(cp + mod_idx * std::sin(mp))) * amp * env;

            cp      += c_inc;
            mp      += m_inc;
            idx_phi += idx_lfo_inc;
            amp_phi += amp_lfo_inc;
        }
    };

    // =========================================================================
    //  STRATUM 1 — "Stone"
    //  Carrier: 29 Hz (subsonic presence — felt)
    //  Mod: 29 × φ² = 29 × 2.6180 = 75.92 Hz
    //  Index LFO: center 1.4, depth 1.2, period 17.3s
    //  Amplitude LFO: center 0.048, depth 0.032, period 23.7s
    //  Phase 0 for both LFOs — this is the anchor stratum
    // =========================================================================
    write_stratum(
        0.0, 43.0,
        29.0, 29.0 * 2.6180339887,          // φ²
        1.4, 1.2, 17.3, 0.0,                // idx LFO
        0.048, 0.032, 23.7, 0.0,            // amp LFO
        1.5, 2.0);

    // =========================================================================
    //  STRATUM 2 — "Water"
    //  Carrier: 67.4 Hz, Mod: 67.4 × √5 = 150.7 Hz
    //  Index LFO: center 0.8, depth 0.7, period 11.8s
    //  Amplitude LFO: center 0.038, depth 0.028, period 31.1s
    //  amp_phase0 = 5.1 / 31.1 × TWO_PI = 1.030 rad — pre-wound mid-swell
    // =========================================================================
    write_stratum(
        0.0, 43.0,
        67.4, 67.4 * 2.2360679774,          // √5
        0.8, 0.7, 11.8, 3.71,               // idx_phase0 = arbitrary irrational
        0.038, 0.028, 31.1, 1.030,          // amp pre-wound by 5.1s equivalent
        2.5, 2.5);

    // =========================================================================
    //  STRATUM 3 — "Air"
    //  Carrier: 142 Hz, Mod: 142 × φ = 229.8 Hz
    //  Index LFO: center 1.1, depth 0.9, period 7.2s (fastest — most nervous)
    //  Amplitude LFO: center 0.028, depth 0.022, period 19.4s
    //  amp_phase0 = 12.7 / 19.4 × TWO_PI = 4.106 rad
    // =========================================================================
    write_stratum(
        0.0, 43.0,
        142.0, 142.0 * 1.6180339887,        // φ
        1.1, 0.9, 7.2, 1.88,
        0.028, 0.022, 19.4, 4.106,
        3.0, 2.0);

    // =========================================================================
    //  STRATUM 4 — "Void"
    //  Carrier: 211 Hz, Mod: 211 × ∛2 = 265.8 Hz (cube root of 2)
    //  Index: stacked LFOs approximate a slow random walk
    //    base: 1.6, LFO1: depth 0.5 period 13.7s, LFO2: depth 0.3 period 8.9s
    //  Amplitude: CONSTANT 0.030 — no LFO (makes it the most unsettling element)
    //  Enters at 8.3s with a 1.8s fade-in
    // =========================================================================
    {
        constexpr double ONSET    = 8.3;
        constexpr double CARRIER  = 211.0;
        constexpr double MOD_HZ   = 211.0 * 1.2599210498948732; // ∛2
        constexpr float  AMP      = 0.030f;
        constexpr double IDX_BASE = 1.6;
        constexpr double IDX_D1   = 0.5,  IDX_P1 = 13.7, IDX_PH1 = 0.774;
        constexpr double IDX_D2   = 0.3,  IDX_P2 = 8.9,  IDX_PH2 = 2.315;
        constexpr double FADE_IN  = 1.8,  FADE_OUT = 2.0;

        const size_t s0  = static_cast<size_t>(ONSET * SR);
        const size_t s1  = TOTAL;
        const size_t len = s1 - s0;

        const double c_inc  = wauvio::TWO_PI * CARRIER / SR;
        const double m_inc  = wauvio::TWO_PI * MOD_HZ  / SR;
        const double l1_inc = wauvio::TWO_PI / (IDX_P1 * SR);
        const double l2_inc = wauvio::TWO_PI / (IDX_P2 * SR);
        double cp = 0.0, mp = 0.0, lp1 = IDX_PH1, lp2 = IDX_PH2;

        const size_t fi_n = static_cast<size_t>(FADE_IN  * SR);
        const size_t fo_n = static_cast<size_t>(FADE_OUT * SR);

        for (size_t i = 0; i < len; ++i) {
            const double mod_idx = IDX_BASE
                + IDX_D1 * std::sin(lp1)
                + IDX_D2 * std::sin(lp2);

            float env = 1.0f;
            if (i < fi_n) env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);

            out[s0 + i] += static_cast<float>(
                std::sin(cp + mod_idx * std::sin(mp))) * AMP * env;

            cp  += c_inc;
            mp  += m_inc;
            lp1 += l1_inc;
            lp2 += l2_inc;
        }
    }

    // =========================================================================
    //  STRATUM 5 — "Fault"
    //  Carrier: 89 Hz, Mod: 89 × π = 279.6 Hz
    //  Index: 1.7 constant
    //  Runs: 21.9s → 38.4s (16.5s window)
    //  HARD INTERRUPT: at absolute t=30.1s (8.2s into window), the modulator
    //  frequency jumps by +17 Hz for 1.4s then returns.
    //  Simulates a fracture event. No fade on the interrupt — instantaneous.
    // =========================================================================
    {
        constexpr double ONSET     = 21.9;
        constexpr double END_T     = 38.4;
        constexpr double CARRIER   = 89.0;
        constexpr double MOD_BASE  = 89.0 * wauvio::PI;     // 279.6 Hz
        constexpr double MOD_JUMP  = MOD_BASE + 17.0;       // interrupt freq
        constexpr double JUMP_S    = 30.1 - ONSET;          // 8.2s into window
        constexpr double JUMP_DUR  = 1.4;
        constexpr double MOD_IDX   = 1.7;
        constexpr float  AMP       = 0.038f;
        constexpr double FI_S      = 2.2, FO_S = 2.5;

        const size_t s0      = static_cast<size_t>(ONSET  * SR);
        const size_t s1      = std::min(static_cast<size_t>(END_T * SR), TOTAL);
        const size_t len     = s1 - s0;
        const size_t jump_s0 = static_cast<size_t>(JUMP_S       * SR);
        const size_t jump_s1 = static_cast<size_t>((JUMP_S + JUMP_DUR) * SR);
        const size_t fi_n    = static_cast<size_t>(FI_S * SR);
        const size_t fo_n    = static_cast<size_t>(FO_S * SR);

        const double c_inc_base  = wauvio::TWO_PI * CARRIER  / SR;
        const double m_inc_base  = wauvio::TWO_PI * MOD_BASE / SR;
        const double m_inc_jump  = wauvio::TWO_PI * MOD_JUMP / SR;
        double cp = 0.0, mp = 0.0;

        for (size_t i = 0; i < len; ++i) {
            const bool in_jump = (i >= jump_s0 && i < jump_s1);
            const double m_inc = in_jump ? m_inc_jump : m_inc_base;

            float env = 1.0f;
            if (i < fi_n) env = static_cast<float>(i) / static_cast<float>(fi_n);
            else if (i >= len - fo_n) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo_n);

            out[s0 + i] += static_cast<float>(
                std::sin(cp + MOD_IDX * std::sin(mp))) * AMP * env;

            cp += c_inc_base;
            mp += m_inc;
        }
    }

    // =========================================================================
    //  TEXTURE LAYER — band-noise throughout, very quiet
    //  HPF @ 400 Hz, LPF @ 1600 Hz, amplitude 0.007
    //  Fills the spectral gaps between strata without drawing attention
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xC0DE1337u);
        wauvio::HighPassFilter hp(400.0, SR);
        wauvio::LowPassFilter  lp(1600.0, SR);
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

} // namespace sequence1_cave_shift
} // namespace soundtrack
