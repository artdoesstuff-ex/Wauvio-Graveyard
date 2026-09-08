#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence2_1.hpp
//  Soundtrack: Sequence2 — Track 1
//
//  Design brief
//  ------------
//  The player is back under the cave.  The flashlight exists now.
//  Something is responding to them — not moving, but attending.
//  The music should feel like irregular breathing.
//  Not a heartbeat. Not a rhythm. Breathing that almost has a pattern,
//  then doesn't.  Gaps that are almost the same length.  But aren't.
//
//  Concept: BREATH-GATE
//  Unlike all previous tracks, this one's macro-structure is controlled
//  entirely by a custom amplitude gate sequence — not LFOs, not envelopes.
//  A hand-designed sequence of gate windows (open/closed intervals) drives
//  when FM voices are audible.  The gate windows are asymmetric in both
//  duration and spacing, tuned to frustrate any attempt at pulse-finding.
//
//  The underlying FM material is always present.  The gate determines
//  when you hear it.  This creates a fundamentally different perceptual
//  experience from tracks that turn voices on/off with ADSR — the
//  material feels continuous but intermittently suppressed.
//
//  Gate architecture (57 seconds)
//  --------------------------------
//  Gate sequence (onset_s, duration_s) — 13 gate windows:
//    (0.0, 1.7), (4.3, 0.9), (6.8, 2.3), (11.4, 0.4),
//    (14.1, 1.8), (17.9, 0.6), (20.3, 3.1), (26.2, 0.8),
//    (29.7, 2.0), (33.5, 0.5), (36.8, 1.4), (41.2, 2.6),
//    (47.1, 1.1)
//  Silence fractions: 1.7/4.3=0.40, 0.9/2.5=0.36, 2.3/4.6=0.50...
//  All different. No two consecutive gates produce the same rhythm feeling.
//  Remaining buffer (~50.4s → 57s) is silence — the longest gap closes
//  the loop, so re-entry feels like a breath being held too long.
//
//  Gate shape: not a rectangle, not a sine.
//  Each gate uses a custom trapezoidal shape: 30ms attack, 30ms release,
//  flat sustain between.  The flatness within a gate makes the
//  on/off more abrupt and less "musical" than a smooth envelope.
//
//  FM material underneath
//  ----------------------
//  Two layered FM voices run continuously beneath the gate:
//
//  Voice 1 — "Lung A"
//    Carrier: 117 Hz.  Mod: 117 × (√7) = 309.6 Hz.
//    Index: 1.9 (constant — no per-voice LFO, gate is the only modulation)
//    Amplitude (pre-gate): 0.058
//
//  Voice 2 — "Lung B"
//    Carrier: 119.4 Hz (3 cents sharp of Voice 1 — slow beating at 2.4 Hz)
//    Mod: 119.4 × (∛5) = 203.9 Hz
//    Index: 1.6
//    Amplitude (pre-gate): 0.042
//
//  The 2.4 Hz beating between Voice 1 and Voice 2 is below the tempo
//  threshold of rhythm perception.  It reads as instability, not pulse.
//
//  Sub layer — "Diaphragm"
//    Carrier: 41 Hz, Mod: 41 × φ = 66.3 Hz
//    Index: 0.8, runs continuously at 0.035 amplitude with NO gate applied
//    Always present.  Something always breathing under everything.
//
//  Anti-repetition
//  ---------------
//  The gate sequence does not divide 57 evenly.  The FM voice phases at
//  loop boundary are at non-zero positions (they ran for 57s of non-integer
//  multiples of their periods).  Each loop the gate opens on slightly
//  different phase positions of Voice 1 and Voice 2.
//
//  Architecture compliance
//  -----------------------
//  - 57 seconds (§4.1)
//  - Seamless loop: 80 ms crossfade (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence2_1 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 57.0;
    constexpr double FADE_S    = 0.08;
    constexpr size_t XFADE_MS  = 80;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  STEP 1: Render FM material continuously into a raw buffer
    //          Voice 1, Voice 2, Sub — all unmodulated by gate yet
    // =========================================================================
    wauvio::Buffer raw(TOTAL, 0.f);

    // Voice 1 — Lung A
    {
        constexpr double C1   = 117.0;
        constexpr double M1   = 117.0 * 2.6457513110645907;  // √7
        constexpr double IDX1 = 1.9;
        constexpr float  AMP1 = 0.058f;
        const double c_inc = wauvio::TWO_PI * C1 / SR;
        const double m_inc = wauvio::TWO_PI * M1 / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            raw[i] += static_cast<float>(
                std::sin(cp + IDX1 * std::sin(mp))) * AMP1;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // Voice 2 — Lung B  (beats at 2.4 Hz against Voice 1)
    {
        constexpr double C2   = 119.4;
        constexpr double M2   = 119.4 * 1.7099759466766968;  // ∛5
        constexpr double IDX2 = 1.6;
        constexpr float  AMP2 = 0.042f;
        const double c_inc = wauvio::TWO_PI * C2 / SR;
        const double m_inc = wauvio::TWO_PI * M2 / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            raw[i] += static_cast<float>(
                std::sin(cp + IDX2 * std::sin(mp))) * AMP2;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // =========================================================================
    //  STEP 2: Build gate envelope
    //          Trapezoidal: 30ms attack, flat sustain, 30ms release
    // =========================================================================
    wauvio::Buffer gate_env(TOTAL, 0.f);

    struct GateWindow { double onset_s; double dur_s; };
    constexpr GateWindow GATES[] = {
        {  0.0, 1.7 }, {  4.3, 0.9 }, {  6.8, 2.3 }, { 11.4, 0.4 },
        { 14.1, 1.8 }, { 17.9, 0.6 }, { 20.3, 3.1 }, { 26.2, 0.8 },
        { 29.7, 2.0 }, { 33.5, 0.5 }, { 36.8, 1.4 }, { 41.2, 2.6 },
        { 47.1, 1.1 }
    };
    constexpr int    N_GATES  = 13;
    constexpr double RAMP_S   = 0.030;   // 30ms attack and release

    for (int g = 0; g < N_GATES; ++g) {
        const size_t s0    = static_cast<size_t>(GATES[g].onset_s * SR);
        const size_t len   = static_cast<size_t>(GATES[g].dur_s   * SR);
        const size_t end   = std::min(s0 + len, TOTAL);
        const size_t ramp  = static_cast<size_t>(RAMP_S * SR);

        for (size_t i = s0; i < end; ++i) {
            const size_t local = i - s0;
            const size_t from_end = end - 1 - i;
            float v = 1.0f;
            if (local   < ramp) v = static_cast<float>(local)    / static_cast<float>(ramp);
            if (from_end < ramp) v = static_cast<float>(from_end) / static_cast<float>(ramp);
            // Trapezoidal: take minimum so both ramps apply correctly at short windows
            gate_env[i] = std::max(gate_env[i], v);
        }
    }

    // =========================================================================
    //  STEP 3: Apply gate to raw FM material → out
    // =========================================================================
    for (size_t i = 0; i < TOTAL; ++i)
        out[i] = raw[i] * gate_env[i];

    // =========================================================================
    //  STEP 4: Sub layer — "Diaphragm" — NO gate, runs continuously
    //          Always breathing under everything, even in silence
    // =========================================================================
    {
        constexpr double CS   = 41.0;
        constexpr double MS   = 41.0 * 1.6180339887;  // φ
        constexpr double IDXS = 0.8;
        constexpr float  AMPS = 0.035f;
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
    //  STEP 5: Quiet background noise — seeded, narrow band, ungated
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xB4D5E6F7u);
        wauvio::HighPassFilter hp(900.0,  SR);
        wauvio::LowPassFilter  lp(2200.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.006f;
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

} // namespace sequence2_1
} // namespace soundtrack
