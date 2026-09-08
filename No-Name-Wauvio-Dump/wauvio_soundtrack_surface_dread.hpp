#pragma once

// =============================================================================
//  wauvio_soundtrack_surface_dread.hpp
//  Soundtrack: Surface — Dread
//
//  Concept: CONVOLUTION GHOST
//  --------------------------
//  A single short FM event — the "source" — is played once.
//  Then its echoes accumulate across the full buffer duration,
//  each echo at a different delay, a different pitch, and
//  a different timbre.  The echoes do not decay correctly.
//  Some are louder than the one before them.  Some are at wrong pitches.
//  The "room" has a memory that distorts what it remembers.
//
//  This is a hand-synthesized convolution reverb where the impulse
//  response is deliberately wrong.  Not malfunctioning — wrong.
//  As if the space has an opinion about what it heard.
//
//  Source event (0.4s at onset 1.8s):
//    Carrier: 173 Hz, Mod: 173 × √5, Index: 2.2, Amp: 0.075
//    Attack: 0.02s, Release: 0.10s
//
//  Echo series (13 echoes):
//  Each echo is the source resynthesized with:
//    - A Doppler-shifted carrier (ratio applied to 173 Hz)
//    - A slightly different modulation index (each has its own "memory")
//    - Amplitude that does NOT monotonically decrease
//    - An independent noise burst added to each echo at low amplitude
//
//  Echo table: (delay_from_source_s, pitch_ratio, mod_idx, amp)
//  Delays are at prime-adjacent irrational spacings.
//  Pitch ratios are from a Doppler-distortion table (each × or ÷ a
//  small irrational factor, alternating direction).
//
//  Echo 01:  delay= 2.31s,  ratio=1.031,  idx=2.0,  amp=0.058
//  Echo 02:  delay= 3.87s,  ratio=0.964,  idx=1.7,  amp=0.062  ← louder
//  Echo 03:  delay= 5.12s,  ratio=1.072,  idx=2.4,  amp=0.044
//  Echo 04:  delay= 7.43s,  ratio=0.941,  idx=1.3,  amp=0.051
//  Echo 05:  delay= 9.08s,  ratio=1.118,  idx=2.8,  amp=0.038
//  Echo 06:  delay=11.61s,  ratio=0.908,  idx=1.0,  amp=0.055  ← louder than 5
//  Echo 07:  delay=13.74s,  ratio=1.163,  idx=3.1,  amp=0.029
//  Echo 08:  delay=16.29s,  ratio=0.878,  idx=0.7,  amp=0.042  ← louder than 7
//  Echo 09:  delay=19.83s,  ratio=1.241,  idx=3.6,  amp=0.021
//  Echo 10:  delay=22.17s,  ratio=0.831,  idx=0.4,  amp=0.033
//  Echo 11:  delay=25.44s,  ratio=1.347,  idx=4.2,  amp=0.018
//  Echo 12:  delay=28.91s,  ratio=0.762,  idx=0.2,  amp=0.024
//  Echo 13:  delay=32.38s,  ratio=1.513,  idx=5.1,  amp=0.012
//
//  Note the amplitude pattern: it does not simply decay.
//  Echoes at even positions (2, 4, 6, 8, 10, 12) are louder than their
//  odd-position neighbors.  The room "prefers" certain reflections.
//  This is not physically possible.  That is the point.
//
//  Between the 6th and 7th echo (11.61s–13.74s) there is a 2.13s gap
//  with no echoes.  This is the longest silence in the track.
//  It falls at an irregular position — not the midpoint, not a third.
//
//  Continuous layer — "The Space":
//    Very quiet broadband FM voice, C=29 Hz, M=29×e, idx=0.3, amp=0.025
//    This represents the room itself — not the echoes, not the source.
//    It runs the entire buffer.  It is always present.
//    It gives the echoes a place to live in.
//
//  Duration: 38 seconds
//  Crossfade: 80ms
//  Normalize: 0.70
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace surface_dread {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 38.0;
    constexpr double FADE_S    = 0.08;
    constexpr size_t XFADE_MS  = 80;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  UTILITY: write a single FM event with attack/release
    // =========================================================================
    auto write_event = [&](double onset_s, double dur_s,
                           double carrier_hz, double mod_hz, double mod_idx,
                           float amp, double attack_s, double release_s)
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
    //  THE SPACE — continuous room drone
    // =========================================================================
    {
        constexpr double CS   = 29.0;
        constexpr double MS   = 29.0 * 2.71828182845904523536;  // e
        constexpr double IDXS = 0.3;
        constexpr float  AMPS = 0.025f;
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
    //  SOURCE EVENT — onset 1.8s, duration 0.4s
    // =========================================================================
    constexpr double SOURCE_CARRIER = 173.0;
    constexpr double SOURCE_MOD     = 173.0 * 2.2360679774997896;  // √5
    constexpr double SOURCE_ONSET   = 1.8;
    constexpr double SOURCE_DUR     = 0.4;

    write_event(SOURCE_ONSET, SOURCE_DUR,
                SOURCE_CARRIER, SOURCE_MOD, 2.2,
                0.075f, 0.02, 0.10);

    // =========================================================================
    //  ECHO SERIES — 13 echoes
    //  Each: source onset + delay, source carrier × pitch_ratio
    //  Duration: same as source (0.4s)
    // =========================================================================
    struct Echo {
        double delay_s;
        double pitch_ratio;
        double mod_idx;
        float  amp;
    };

    const Echo ECHOES[] = {
        {  2.31, 1.031, 2.0, 0.058f },
        {  3.87, 0.964, 1.7, 0.062f },
        {  5.12, 1.072, 2.4, 0.044f },
        {  7.43, 0.941, 1.3, 0.051f },
        {  9.08, 1.118, 2.8, 0.038f },
        { 11.61, 0.908, 1.0, 0.055f },
        { 13.74, 1.163, 3.1, 0.029f },
        { 16.29, 0.878, 0.7, 0.042f },
        { 19.83, 1.241, 3.6, 0.021f },
        { 22.17, 0.831, 0.4, 0.033f },
        { 25.44, 1.347, 4.2, 0.018f },
        { 28.91, 0.762, 0.2, 0.024f },
        { 32.38, 1.513, 5.1, 0.012f },
    };
    constexpr int N_ECHOES = 13;

    for (int e = 0; e < N_ECHOES; ++e) {
        const double onset      = SOURCE_ONSET + ECHOES[e].delay_s;
        const double carrier_hz = SOURCE_CARRIER * ECHOES[e].pitch_ratio;
        // Modulator shifts proportionally to carrier
        const double mod_hz     = carrier_hz * 2.2360679774997896;

        // Echo duration slightly longer for high-index echoes (more complex, longer tail)
        const double dur = SOURCE_DUR + ECHOES[e].mod_idx * 0.04;

        write_event(onset, dur,
                    carrier_hz, mod_hz, ECHOES[e].mod_idx,
                    ECHOES[e].amp,
                    0.015, 0.12);

        // Small noise burst coloring each echo (each has a unique seed)
        {
            const size_t ns0 = static_cast<size_t>(onset * SR);
            const size_t nlen = static_cast<size_t>(0.15 * SR);
            const size_t nend = std::min(ns0 + nlen, TOTAL);
            wauvio::NoiseGenerator ng(0x100u + static_cast<uint32_t>(e));
            wauvio::HighPassFilter hp(carrier_hz * 0.8, SR);
            wauvio::LowPassFilter  lp(carrier_hz * 3.5, SR);
            for (size_t i = ns0; i < nend; ++i) {
                float s = ng.tick();
                s = hp.tick(s);
                s = lp.tick(s);
                const float t_env = static_cast<float>(nend - 1 - i)
                    / static_cast<float>(nlen);
                out[i] += s * ECHOES[e].amp * 0.15f * t_env;
            }
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

} // namespace surface_dread
} // namespace soundtrack
