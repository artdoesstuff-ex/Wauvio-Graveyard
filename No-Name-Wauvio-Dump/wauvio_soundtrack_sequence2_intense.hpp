#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence2_intense.hpp
//  Soundtrack: Sequence2 — Intense
//
//  Design brief
//  ------------
//  ctx.player.lWalked >= 270.  Spawn interval is 5 seconds.
//  FAULT_CHASER is dominant.  The flashlight is unreliable.
//  The player can see something but cannot locate it.
//  The music should feel like pressure — not a threat, but weight.
//  The weight of too many things occupying the same space.
//
//  Concept: PRESSURE ACCUMULATION
//  Every other track in this project changes over time through
//  subtraction, gating, rotation, or decay.
//  This track changes only through addition.
//
//  A single FM voice begins the loop.  Every few seconds, a new voice
//  enters.  No voice ever exits.  No gate suppresses them.
//  No LFO reduces their amplitude.  They simply accumulate.
//  By the end of the loop, seven voices sound simultaneously — each
//  at a moderate amplitude, each with a different carrier and modulator,
//  none related to any other by a simple ratio.
//
//  The perceptual effect is not loudness — each individual voice is quiet.
//  The perceptual effect is occupancy: the frequency space fills up.
//  The listener cannot focus on any single element because every element
//  is part of a cluster that has no center.
//
//  Voice entry schedule (55 seconds)
//  ----------------------------------
//  Voice 1 enters at 0.0s   — always present, the anchor that won't hold
//  Voice 2 enters at 4.7s   — first arrival, different register
//  Voice 3 enters at 11.3s  — silence between 2 and 3 makes 3 feel wrong
//  Voice 4 enters at 19.8s  — the cluster starts sounding "thick"
//  Voice 5 enters at 28.1s  — listener begins losing track of sources
//  Voice 6 enters at 36.7s  — listener has stopped trying
//  Voice 7 enters at 44.2s  — full accumulation, loop ends at 55s
//
//  Entry timing properties:
//    Gaps: 4.7, 6.6, 8.5, 8.3, 8.6, 7.5, 10.8
//    All different. Slight upward trend with interruption at gap 4.
//    Gap 4→5 (8.3) < Gap 3→4 (8.5): defies expectation of steady growth.
//
//  Voice parameters
//  ----------------
//  All voices use FM.  Carriers chosen by the following constraint:
//    No two carriers may share a factor within 3 semitones.
//    No carrier/modulator pair across different voices may beat
//    at a frequency in the 0.5–4 Hz range (would create false rhythm).
//
//  V1: C=83.0 Hz,   M=83.0×√7=219.6 Hz,     idx=1.2, amp=0.042
//  V2: C=131.0 Hz,  M=131.0×φ²=343.0 Hz,    idx=0.8, amp=0.038
//  V3: C=197.0 Hz,  M=197.0×∛5=336.6 Hz,    idx=1.6, amp=0.034
//  V4: C=59.0 Hz,   M=59.0×(e+1)=219.7 Hz,  idx=2.1, amp=0.030
//  V5: C=167.0 Hz,  M=167.0×√3=289.3 Hz,    idx=0.6, amp=0.028
//  V6: C=241.0 Hz,  M=241.0×∛7=393.1 Hz,    idx=1.4, amp=0.025
//  V7: C=47.0 Hz,   M=47.0×π=147.7 Hz,      idx=2.8, amp=0.022
//
//  Total peak amplitude (all 7 voices): ~0.219 before normalization.
//  After normalize(0.70), full-density sections are at target.
//  Early sections (1-3 voices) will be quieter — intentional.
//  The track gets louder as it progresses.
//
//  Each voice fades in over 1.5s when it enters.
//  No voice fades out — they sustain to the loop boundary.
//
//  Each voice has a slow, independent amplitude micro-variation:
//    A very low-depth sine (depth=0.04, period between 8–25s each).
//    This is not LFO modulation of FM index — only amplitude.
//    Purpose: prevent exact amplitude cancellation between voices
//    and create the perception that voices are "alive" but not moving.
//
//  Continuous texture
//  ------------------
//  Noise band: HPF 500 Hz, LPF 1800 Hz, amplitude 0.008.
//  Runs entire buffer.  Provides upper-mid presence between FM voices.
//
//  Anti-repetition
//  ---------------
//  Seven voices at incommensurable frequencies.  At the loop boundary,
//  all seven are at different phases.  The cluster sounds different
//  each time it reaches full density — same spectral components,
//  different phase relationships → different perceived texture.
//  The attack transients on each entry (1.5s fade-in) also prevent
//  the entries from feeling "mechanical" on repeat.
//
//  Architecture compliance
//  -----------------------
//  - 55 seconds (§4.1)
//  - Seamless loop: 100 ms crossfade (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence2_intense {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 55.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 100;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  VOICE TABLE
    // =========================================================================
    struct Voice {
        double entry_s;      // when this voice enters
        double carrier_hz;
        double mod_hz;
        double mod_idx;
        float  amp;
        double micro_period; // amplitude micro-variation period (seconds)
        double micro_phase;  // starting phase of micro-variation
    };

    constexpr double E   = 2.71828182845904523536;
    constexpr double PHI = 1.6180339887498948482;
    constexpr double PHI2 = PHI * PHI;   // φ² = 2.6180

    const Voice VOICES[] = {
        //  entry    carrier    mod                            idx    amp     micro_per  micro_ph
        {   0.0,   83.0,   83.0 * 2.6457513110645905905016,  1.2,  0.042f,  17.3,      0.00 },  // √7
        {   4.7,  131.0,  131.0 * PHI2,                      0.8,  0.038f,  22.1,      1.17 },  // φ²
        {  11.3,  197.0,  197.0 * 1.7099759466766967976799,  1.6,  0.034f,  13.8,      2.43 },  // ∛5
        {  19.8,   59.0,   59.0 * (E + 1.0),                 2.1,  0.030f,   9.6,      0.88 },  // e+1
        {  28.1,  167.0,  167.0 * 1.7320508075688772935274,  0.6,  0.028f,  25.7,      3.61 },  // √3
        {  36.7,  241.0,  241.0 * 1.9129311827723891011986,  1.4,  0.025f,  11.2,      1.94 },  // ∛7
        {  44.2,   47.0,   47.0 * 3.14159265358979323846,    2.8,  0.022f,  19.5,      4.72 },  // π
    };
    constexpr int N_VOICES   = 7;
    constexpr double FADE_IN_V = 1.5;   // all voices fade in over 1.5s

    // =========================================================================
    //  Render each voice from its entry point to the end of the buffer
    // =========================================================================
    for (int v = 0; v < N_VOICES; ++v) {
        const Voice& vx  = VOICES[v];
        const size_t s0  = static_cast<size_t>(vx.entry_s * SR);
        if (s0 >= TOTAL) continue;
        const size_t len = TOTAL - s0;

        const double c_inc      = wauvio::TWO_PI * vx.carrier_hz / SR;
        const double m_inc      = wauvio::TWO_PI * vx.mod_hz     / SR;
        const double micro_inc  = wauvio::TWO_PI / (vx.micro_period * SR);
        double cp   = 0.0;
        double mp   = 0.0;
        double micp = vx.micro_phase;

        const size_t fi_n = static_cast<size_t>(FADE_IN_V * SR);

        for (size_t i = 0; i < len; ++i) {
            // Fade-in envelope on entry
            float env = 1.0f;
            if (i < fi_n)
                env = static_cast<float>(i) / static_cast<float>(fi_n);

            // Micro amplitude variation — very shallow, prevents phase cancellation
            const float micro_amp = vx.amp
                * (1.0f + 0.04f * static_cast<float>(std::sin(micp)));

            out[s0 + i] += static_cast<float>(
                std::sin(cp + vx.mod_idx * std::sin(mp)))
                * micro_amp * env;

            cp   += c_inc;
            mp   += m_inc;
            micp += micro_inc;
        }
    }

    // =========================================================================
    //  NOISE TEXTURE — upper-mid band, continuous
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0x7E8F9A0Bu);
        wauvio::HighPassFilter hp(500.0,  SR);
        wauvio::LowPassFilter  lp(1800.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.008f;
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

} // namespace sequence2_intense
} // namespace soundtrack
