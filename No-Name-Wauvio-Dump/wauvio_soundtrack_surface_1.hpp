#pragma once

// =============================================================================
//  wauvio_soundtrack_surface_1.hpp
//  Soundtrack: Surface — Track 1
//
//  Concept: TIDAL SHIMMER
//  ----------------------
//  A harmonic field constructed from partials of an infrasonic root.
//  The root frequency is 14.7 Hz — below human hearing threshold.
//  The audible voices are its 3rd, 5th, 7th, 11th, and 13th partials:
//    P3  = 14.7 × 3  =  44.1  Hz
//    P5  = 14.7 × 5  =  73.5  Hz
//    P7  = 14.7 × 7  = 102.9  Hz
//    P11 = 14.7 × 11 = 161.7  Hz
//    P13 = 14.7 × 13 = 191.1  Hz
//
//  Because 14.7 Hz is irrational relative to any integer sample-rate grid,
//  the partials never simultaneously reach phase zero.  The harmonic
//  relationships are exact (integer multiples), but phase alignment is
//  perpetually deferred — the sound is "almost resolved" at all times.
//
//  Each partial is an FM voice:
//    Carrier  = partial frequency
//    Modulator = carrier × φ (irrational modulator ensures non-harmonic sidebands)
//    Index varies per partial (higher partials = lower index, more tonal)
//
//  Amplitude architecture:
//  ----------------------
//  Five partials are paired into two amplitude-cross-fade groups:
//    Group A: P3, P7, P13  (low-mid-high spread)
//    Group B: P5, P11
//  A slow cross-fade LFO (period 27.3s) continuously moves amplitude
//  weight between groups — when Group A is loud, Group B is quiet.
//  The LFO uses a custom raised-cosine shape (smoother than sine) so
//  transitions feel like tides rather than wobble.
//  Each partial within a group has a secondary micro-LFO (period 8–19s)
//  that prevents any two partials from peaking simultaneously.
//
//  No silence gaps.  No events.  No percussive elements.
//  The surface is open air and the sound should feel like wind-driven
//  pressure changes in a large open space — not a threat, but not
//  safe either.  Something about the proportions is wrong.
//
//  Duration: 44 seconds
//  Crossfade: 90ms
//  Normalize: 0.70
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace surface_1 {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 44.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 90;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  PARTIAL DEFINITIONS
    //  root = 14.7 Hz, partials at primes 3, 5, 7, 11, 13
    // =========================================================================
    constexpr double ROOT = 14.7;
    constexpr double PHI  = 1.6180339887498948482;

    struct Partial {
        int    harmonic;   // integer multiple of root
        double mod_idx;    // FM index (lower for higher partials)
        float  base_amp;   // peak amplitude
        double micro_per;  // micro-LFO period (seconds)
        double micro_ph;   // starting phase of micro-LFO
        int    group;      // 0 = Group A, 1 = Group B
    };

    const Partial PARTIALS[] = {
        //  harm   idx    amp     micro_per  micro_ph  group
        {   3,    1.80,  0.058f,  17.3,      0.00,     0 },   // P3  = 44.1 Hz
        {   5,    1.20,  0.052f,  11.8,      2.41,     1 },   // P5  = 73.5 Hz
        {   7,    0.85,  0.046f,  19.1,      1.07,     0 },   // P7  = 102.9 Hz
        {  11,    0.55,  0.040f,   8.7,      3.88,     1 },   // P11 = 161.7 Hz
        {  13,    0.38,  0.034f,  14.6,      5.23,     0 },   // P13 = 191.1 Hz
    };
    constexpr int N_PARTIALS = 5;

    // =========================================================================
    //  GROUP CROSS-FADE LFO
    //  Period: 27.3s.  Raised-cosine shape: 0.5 + 0.5 × cos(phase)
    //  When group_lfo = 1.0 → Group A full, Group B silent
    //  When group_lfo = 0.0 → Group B full, Group A silent
    //  Both groups always sum to 1.0 amplitude.
    // =========================================================================
    const double group_lfo_inc = wauvio::TWO_PI / (27.3 * SR);

    // =========================================================================
    //  RENDER EACH PARTIAL
    // =========================================================================
    for (int p = 0; p < N_PARTIALS; ++p) {
        const Partial& px = PARTIALS[p];

        const double carrier_hz  = ROOT * px.harmonic;
        const double mod_hz      = carrier_hz * PHI;
        const double c_inc       = wauvio::TWO_PI * carrier_hz / SR;
        const double m_inc       = wauvio::TWO_PI * mod_hz     / SR;
        const double micro_inc   = wauvio::TWO_PI / (px.micro_per * SR);

        double cp    = 0.0;
        double mp    = 0.0;
        double micp  = px.micro_ph;
        double glfo  = 0.0;   // group lfo phase — starts at 0

        for (size_t i = 0; i < TOTAL; ++i) {
            // Group cross-fade (raised cosine)
            const double gval   = 0.5 + 0.5 * std::cos(glfo);
            const float  g_gain = (px.group == 0)
                ? static_cast<float>(gval)
                : static_cast<float>(1.0 - gval);

            // Micro-variation within group
            const float micro = 1.0f
                + 0.12f * static_cast<float>(std::sin(micp));

            out[i] += static_cast<float>(
                std::sin(cp + px.mod_idx * std::sin(mp)))
                * px.base_amp * g_gain * micro;

            cp   += c_inc;
            mp   += m_inc;
            micp += micro_inc;
            glfo += group_lfo_inc;
        }
    }

    // =========================================================================
    //  SUB TEXTURE — infrasonic-region noise: HPF 10 Hz, LPF 45 Hz
    //  Adds weight below the lowest partial without creating pitch
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0xA9B8C7D6u);
        wauvio::HighPassFilter hp(10.0, SR);
        wauvio::LowPassFilter  lp(45.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.018f;
        }
    }

    // =========================================================================
    //  AIR PRESENCE — high shimmer, very quiet
    //  HPF 3500 Hz, LPF 7000 Hz, amp 0.006
    // =========================================================================
    {
        wauvio::NoiseGenerator ng(0x1F2E3D4Cu);
        wauvio::HighPassFilter hp(3500.0, SR);
        wauvio::LowPassFilter  lp(7000.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = hp.tick(s);
            s = lp.tick(s);
            out[i] += s * 0.006f;
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

} // namespace surface_1
} // namespace soundtrack
