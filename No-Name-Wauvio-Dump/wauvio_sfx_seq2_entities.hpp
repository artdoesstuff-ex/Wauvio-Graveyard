#pragma once

// =============================================================================
//  wauvio_sfx_seq2_entities.hpp
//  Sound effects for all five Seq2EntitySystem behaviors.
//
//  Per-behavior design rationale
//  ------------------------------
//
//  STILL_HUNTER
//    Moves only while player is NOT moving.  It knows the player stopped.
//    Approach: ultra-slow amplitude modulation over a low FM drone.
//              The threat closes the gap in silence — you feel weight, not sound.
//              Almost no attack transient.  Just pressure that was not there before.
//    Retreat:  a single long exhale — the FM index deflates from high to zero.
//              As if it breathes out when it stops.
//
//  BLINK_FEEDER
//    Lurches forward each time the player blinks.
//    Approach: rhythmically irregular sharp FM stabs with silence between.
//              Each stab is a lurch — they sound like a countdown that skips steps.
//              High FM index for a dry, toneless crack.  No sustain.
//    Retreat:  a fast reversed-envelope burst — attack is the tail of a lurch
//              that never arrived.  Drops to silence instantly.
//
//  TORCH_HATER
//    Retreats from light; charges hard in darkness.
//    Approach: (represents the dark-charge state) — aggressive ascending FM glide
//              from low to mid register with rapidly increasing amplitude.
//              High mod index throughout.  The closer it gets, the louder and higher.
//    Retreat:  (represents being hit by the torch) — a sharp high-frequency
//              downward FM sweep, like something scorched and recoiling.
//              Very fast, very bright, then silence.
//
//  MIRROR
//    Mirrors the player's own walk rhythm.
//    Approach: two FM voices separated by a small interval, one slightly delayed
//              relative to the other — one voice is "you", one is "it".
//              The delay is not constant; it drifts, so the copy is wrong.
//    Retreat:  the copy voice fades but the "original" stays a moment longer
//              before also fading — the ghost of a copy of something that was you.
//
//  FAULT_CHASER
//    Dormant until flashlight malfunctions, then charges.
//    Approach: a long period of near-silence (0.5s dead) followed by a sudden
//              burst of dense FM chaos — amplitude goes from 0 to peak instantly.
//              The silence before is the dormancy.  The explosion is the charge.
//    Retreat:  hard cut to noise burst, then silence.  No fade.
//              It did not decide to stop.  It was interrupted.
//
//  Architecture compliance
//  -----------------------
//  Approach buffers: 4–6 seconds, normalize to 0.50 (§5.1)
//  Retreat buffers:  0.5–2 seconds, normalize to 0.60 (§5.1)
//  No wauvio::play(), no loops, no threads (§3.3)
//  Fixed noise seeds (§3.2)
//  Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

// =============================================================================
//  INTERNAL HELPERS
// =============================================================================

namespace sfx_detail {

// Write a single FM event directly into `out` at sample offset s0.
// Modulator phase accumulates independently from carrier.
inline void fm_event(wauvio::Buffer& out,
                     size_t s0, double dur_s,
                     double carrier_hz, double mod_hz, double mod_idx,
                     float amp,
                     double attack_s, double release_s,
                     int SR)
{
    const size_t n   = static_cast<size_t>(dur_s * SR);
    const size_t end = std::min(s0 + n, out.size());
    const size_t len = end - s0;
    if (len == 0) return;

    const double c_inc = wauvio::TWO_PI * carrier_hz / SR;
    const double m_inc = wauvio::TWO_PI * mod_hz     / SR;
    double cp = 0.0, mp = 0.0;

    const size_t fi = static_cast<size_t>(attack_s  * SR);
    const size_t fo = static_cast<size_t>(release_s * SR);

    for (size_t i = 0; i < len; ++i) {
        float env = 1.0f;
        if (i < fi && fi > 0)        env = static_cast<float>(i)           / static_cast<float>(fi);
        else if (i >= len - fo && fo > 0) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo);
        out[s0 + i] += static_cast<float>(
            std::sin(cp + mod_idx * std::sin(mp))) * amp * env;
        cp += c_inc;
        mp += m_inc;
    }
}

// Write FM with linearly interpolated carrier (pitch glide).
inline void fm_glide(wauvio::Buffer& out,
                     size_t s0, double dur_s,
                     double carrier_start, double carrier_end,
                     double mod_ratio, double mod_idx,
                     float amp_start, float amp_end,
                     double attack_s, double release_s,
                     int SR)
{
    const size_t n   = static_cast<size_t>(dur_s * SR);
    const size_t end = std::min(s0 + n, out.size());
    const size_t len = end - s0;
    if (len == 0) return;

    double cp = 0.0, mp = 0.0;
    const size_t fi = static_cast<size_t>(attack_s  * SR);
    const size_t fo = static_cast<size_t>(release_s * SR);

    for (size_t i = 0; i < len; ++i) {
        const double t      = static_cast<double>(i) / static_cast<double>(len);
        const double freq_c = carrier_start + t * (carrier_end - carrier_start);
        const double freq_m = freq_c * mod_ratio;
        const float  amp    = amp_start + static_cast<float>(t) * (amp_end - amp_start);

        float env = 1.0f;
        if (i < fi && fi > 0)             env = static_cast<float>(i)           / static_cast<float>(fi);
        else if (i >= len - fo && fo > 0) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo);

        out[s0 + i] += static_cast<float>(
            std::sin(cp + mod_idx * std::sin(mp))) * amp * env;
        cp += wauvio::TWO_PI * freq_c / SR;
        mp += wauvio::TWO_PI * freq_m / SR;
    }
}

// Band-limited noise burst at offset.
inline void noise_burst(wauvio::Buffer& out,
                        size_t s0, double dur_s,
                        float amp, double hp_hz, double lp_hz,
                        uint32_t seed, int SR)
{
    const size_t n   = static_cast<size_t>(dur_s * SR);
    const size_t end = std::min(s0 + n, out.size());
    const size_t len = end - s0;
    if (len == 0) return;

    wauvio::NoiseGenerator ng(seed);
    wauvio::HighPassFilter hp(hp_hz, SR);
    wauvio::LowPassFilter  lp(lp_hz, SR);

    // Short fade in/out to avoid clicks
    const size_t ramp = std::min(len / 4, static_cast<size_t>(0.015 * SR));

    for (size_t i = 0; i < len; ++i) {
        float s = ng.tick();
        s = hp.tick(s);
        s = lp.tick(s);
        float env = 1.0f;
        if (i < ramp)             env = static_cast<float>(i)           / static_cast<float>(ramp);
        if (i >= len - ramp)      env = static_cast<float>(len - 1 - i) / static_cast<float>(ramp);
        out[s0 + i] += s * amp * env;
    }
}

} // namespace sfx_detail


// =============================================================================
//  STILL_HUNTER
//
//  Moves only while player is NOT moving.
//  Approach: slow amplitude modulation, low FM drone — weight closing in.
//  Retreat: long FM index exhale — it stops, and breathes out.
// =============================================================================

namespace sfx {

inline wauvio::Buffer gen_still_hunter_approach() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 5.0;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Primary voice: very low FM drone, amplitude rises extremely slowly
    // Carrier: 63 Hz (deep, felt in the chest), Mod: 63 × √3
    // Index: 0.9 — tonal but not clean.  This is presence, not pitch.
    // Amplitude envelope: silence for first 0.8s, then rises quadratically.
    // The slowness IS the threat — it has been moving since before you noticed.
    {
        constexpr double C = 63.0, M = 63.0 * 1.7320508075688773;
        constexpr double IDX = 0.9;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        double cp = 0.0, mp = 0.0;

        const size_t silence_n = static_cast<size_t>(0.8 * SR);

        for (size_t i = 0; i < TOTAL; ++i) {
            float amp_env = 0.0f;
            if (i > silence_n) {
                const float t = static_cast<float>(i - silence_n)
                              / static_cast<float>(TOTAL - silence_n);
                amp_env = t * t;  // quadratic — barely moving at first, then approaching
            }
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.065f * amp_env;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // Secondary voice: 63 × φ = 102 Hz — a partial that underlines the fundamental
    // Amplitude modulated by a very slow sine (period 3.1s) — creates an uneven breathing
    {
        constexpr double C2 = 63.0 * 1.6180339887;  // 102 Hz
        constexpr double M2 = C2   * 1.4142135623;  // × √2
        constexpr double IDX2 = 0.4;
        const double c_inc = wauvio::TWO_PI * C2 / SR;
        const double m_inc = wauvio::TWO_PI * M2 / SR;
        const double lfo_inc = wauvio::TWO_PI / (3.1 * SR);
        double cp = 0.0, mp = 0.0, lp = wauvio::PI;  // offset phase — rises as primary falls

        for (size_t i = 0; i < TOTAL; ++i) {
            const float lfo_val = 0.5f + 0.5f * static_cast<float>(std::sin(lp));
            out[i] += static_cast<float>(
                std::sin(cp + IDX2 * std::sin(mp))) * 0.030f * lfo_val;
            cp  += c_inc;
            mp  += m_inc;
            lp  += lfo_inc;
        }
    }

    // Sub-bass pressure: filtered noise, HPF 15 Hz, LPF 50 Hz — felt in body
    sfx_detail::noise_burst(out, 0, DUR, 0.025f, 15.0, 50.0, 0xA1B1C1D1u, SR);

    wauvio::fade_in(out,  0.04, SR);
    wauvio::fade_out(out, 0.08, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_still_hunter_retreat() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 1.8;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // FM "exhale": index deflates from 2.4 → 0.1 over full duration
    // Carrier holds steady at 63 Hz — same as approach, but the timbre collapses
    // Amplitude: starts at peak, falls to zero.  It stopped.
    {
        constexpr double C = 63.0, M = 63.0 * 1.7320508075688773;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        double cp = 0.0, mp = 0.0;

        for (size_t i = 0; i < TOTAL; ++i) {
            const double t   = static_cast<double>(i) / static_cast<double>(TOTAL);
            const double idx = 2.4 - t * 2.3;  // 2.4 → 0.1 — the exhale
            const float  amp = static_cast<float>((1.0 - t) * (1.0 - t)); // quadratic fall

            out[i] += static_cast<float>(
                std::sin(cp + idx * std::sin(mp))) * 0.070f * amp;
            cp += c_inc;
            mp += m_inc;
        }
    }

    wauvio::fade_in(out,  0.01, SR);
    wauvio::fade_out(out, 0.15, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  BLINK_FEEDER
//
//  Lurches forward each time the player blinks.
//  Approach: irregular FM stabs — each one is a lurch, silence between them.
//  Retreat: reversed-envelope burst, drops instantly.
// =============================================================================

inline wauvio::Buffer gen_blink_feeder_approach() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 4.5;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Five FM stabs at irregular positions.
    // Each stab: short (0.07s), hard attack (0.004s), fast release (0.055s).
    // Carrier escalates with each lurch — it is closer each time.
    // High FM index (3.5–4.8) — dry, crack-like, almost no pitch identity.
    // Silence between is intentional — the blinks that aren't happening yet.
    struct Stab { double onset; double carrier; double mod_idx; float amp; };
    const Stab STABS[] = {
        { 0.31, 144.0, 3.5, 0.055f },
        { 1.03, 162.0, 3.8, 0.062f },
        { 1.59, 181.0, 4.1, 0.070f },
        { 2.44, 203.0, 4.4, 0.078f },
        { 3.17, 228.0, 4.8, 0.088f },
    };

    for (const auto& s : STABS) {
        sfx_detail::fm_event(out,
            static_cast<size_t>(s.onset * SR), 0.07,
            s.carrier, s.carrier * 1.6180339887, s.mod_idx,
            s.amp, 0.004, 0.055, SR);
    }

    // Sub-noise pulse on each stab — tactile impact below the FM crack
    const double noise_onsets[] = { 0.31, 1.03, 1.59, 2.44, 3.17 };
    for (double no : noise_onsets) {
        sfx_detail::noise_burst(out,
            static_cast<size_t>(no * SR), 0.05,
            0.020f, 40.0, 180.0, 0xFEED1000u + static_cast<uint32_t>(no * 100), SR);
    }

    wauvio::fade_in(out,  0.01, SR);
    wauvio::fade_out(out, 0.06, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_blink_feeder_retreat() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.6;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Reversed-envelope burst: amplitude STARTS at peak, drops immediately.
    // Simulates the trailing edge of a lurch that was cancelled.
    // Single FM stab, high index, fast decay.
    // Carrier: 228 Hz (same as final approach stab — continuation of the last lurch)
    {
        constexpr double C = 228.0, M = 228.0 * 1.6180339887;
        constexpr double IDX = 4.8;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        double cp = 0.0, mp = 0.0;

        for (size_t i = 0; i < TOTAL; ++i) {
            const float t   = static_cast<float>(i) / static_cast<float>(TOTAL);
            const float env = (1.0f - t) * (1.0f - t) * (1.0f - t); // cubic decay
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.080f * env;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // Very brief noise hit at onset — the crack of the final lurch being stopped
    sfx_detail::noise_burst(out, 0, 0.04, 0.045f, 200.0, 900.0, 0xFEEDBEEFu, SR);

    wauvio::fade_in(out,  0.003, SR);
    wauvio::fade_out(out, 0.04,  SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  TORCH_HATER
//
//  Retreats from light; charges hard in darkness.
//  Approach = the charge state (darkness active).
//  Retreat = scorched by the torch, recoiling.
// =============================================================================

inline wauvio::Buffer gen_torch_hater_approach() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 4.0;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Ascending FM glide: carrier rises from 48 Hz → 196 Hz over 4 seconds.
    // This is not subtle — it's aggressive forward motion translated to pitch.
    // Mod ratio: √7 (inharmonic, raw)
    // Index: 2.8 throughout — heavy spectral content, almost distorted
    // Amplitude: rises from 0.020 → 0.075 — getting louder as it gets closer
    sfx_detail::fm_glide(out,
        0, DUR,
        48.0, 196.0,         // carrier glide
        2.6457513110645907,  // √7
        2.8,
        0.020f, 0.075f,
        0.08, 0.12,
        SR);

    // Mid-point acceleration: at 2.2s, a second shorter glide layer enters
    // This represents the charge breaking from a jog into a sprint
    sfx_detail::fm_glide(out,
        static_cast<size_t>(2.2 * SR), 1.8,
        110.0, 240.0,
        1.7320508075688773,  // √3
        3.4,
        0.028f, 0.055f,
        0.05, 0.15,
        SR);

    // Rapid low-frequency tremolo on the entire buffer (simulates stride rhythm)
    // Applied as a modulation on the existing signal, not a new voice
    // LFO: 7.3 Hz (irregular-feeling, above typical step rate)
    {
        const double lfo_inc = wauvio::TWO_PI * 7.3 / SR;
        double lp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            const float lfo = 0.70f + 0.30f * static_cast<float>(std::sin(lp));
            out[i] *= lfo;
            lp += lfo_inc;
        }
    }

    wauvio::fade_in(out,  0.06, SR);
    wauvio::fade_out(out, 0.12, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_torch_hater_retreat() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.9;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Bright, high, downward FM sweep — scorched, recoiling.
    // Carrier: 1100 Hz → 180 Hz over 0.6s (sharp descending scorch)
    // Amplitude: starts at peak, falls steeply.
    sfx_detail::fm_glide(out,
        0, 0.6,
        1100.0, 180.0,
        0.5,       // low mod ratio — keeps it from sounding too "musical"
        1.2,
        0.068f, 0.005f,
        0.003, 0.08,
        SR);

    // High-frequency noise burst — the sizzle of light hitting it
    sfx_detail::noise_burst(out,
        0, 0.15,
        0.055f, 2000.0, 8000.0, 0xB17CB17Cu, SR);

    // Low thud after the retreat — it landed somewhere behind you
    sfx_detail::fm_event(out,
        static_cast<size_t>(0.55 * SR), 0.3,
        62.0, 62.0 * 2.41421356, 1.6,
        0.040f, 0.01, 0.25, SR);

    wauvio::fade_in(out,  0.003, SR);
    wauvio::fade_out(out, 0.06,  SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  MIRROR
//
//  Mirrors the player's own walk rhythm.
//  Approach: two FM voices — one is "you", one is "it", slightly delayed and wrong.
//  Retreat: the copy lingers after the original, then both die.
// =============================================================================

inline wauvio::Buffer gen_mirror_approach() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 5.5;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // "You" voice: carrier 131 Hz, mod 131×φ, index 1.1, amplitude 0.040
    // Regular, clean-ish.  This is the sound of a footstep pattern.
    {
        constexpr double C = 131.0, M = 131.0 * 1.6180339887;
        constexpr double IDX = 1.1;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        // Step rhythm: pulses at ~0.55s intervals (roughly 109 BPM — fast walk)
        // 9 pulses across 5.5s
        const double STEP_T = 0.55;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            // Step envelope: repeat a short pulse
            const double t_in_step = std::fmod(static_cast<double>(i) / SR, STEP_T);
            const float  pulse_env = (t_in_step < 0.12)
                ? static_cast<float>(std::exp(-t_in_step / 0.04))
                : 0.0f;
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.040f * pulse_env;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // "It" voice: same carrier + 4 Hz (beating at 4 Hz — just perceptible as timing)
    // Index 1.4 (slightly more complex timbre — not quite the same)
    // Delayed by 0.09s (the copy is always late — reaction, not synchrony)
    // Drift: delay increases by 0.008s per step — the copy slowly falls behind
    {
        constexpr double C = 135.0, M = 135.0 * 1.6180339887;  // +4 Hz
        constexpr double IDX = 1.4;
        constexpr double INIT_DELAY = 0.09;
        constexpr double DRIFT      = 0.008;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        double cp = 0.0, mp = 0.0;

        for (size_t i = 0; i < TOTAL; ++i) {
            const double step_num = (static_cast<double>(i) / SR) / 0.55;
            const double delay    = INIT_DELAY + std::floor(step_num) * DRIFT;
            const double t_in_step = std::fmod(
                std::max(0.0, static_cast<double>(i) / SR - delay), 0.55);
            const float pulse_env = (t_in_step < 0.12)
                ? static_cast<float>(std::exp(-t_in_step / 0.04))
                : 0.0f;
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.038f * pulse_env;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // Continuous low noise bed — the space they share
    sfx_detail::noise_burst(out, 0, DUR, 0.010f, 80.0, 300.0, 0x11220033u, SR);

    wauvio::fade_in(out,  0.05, SR);
    wauvio::fade_out(out, 0.10, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_mirror_retreat() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 1.6;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // The "copy" voice lingers 0.15s after the "original" cuts.
    // Original: fades out over 0.5s starting at onset 0.
    // Copy: starts at 0.15s, fades out over 0.6s.
    // Both then leave. What's left: a brief moment of uncanny afterimage.

    // Original voice
    {
        constexpr double C = 131.0, M = 131.0 * 1.6180339887;
        constexpr double IDX = 1.1;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        const size_t n   = static_cast<size_t>(0.5 * SR);
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < n && i < TOTAL; ++i) {
            const float env = 1.0f - static_cast<float>(i) / static_cast<float>(n);
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.042f * env;
            cp += c_inc;
            mp += m_inc;
        }
    }

    // Copy voice — enters slightly late
    {
        const size_t offset = static_cast<size_t>(0.15 * SR);
        constexpr double C = 135.0, M = 135.0 * 1.6180339887;
        constexpr double IDX = 1.4;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        const size_t n   = static_cast<size_t>(0.65 * SR);
        const size_t end = std::min(offset + n, TOTAL);
        double cp = 0.0, mp = 0.0;
        for (size_t i = offset; i < end; ++i) {
            const float env = 1.0f - static_cast<float>(i - offset) / static_cast<float>(n);
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.038f * env;
            cp += c_inc;
            mp += m_inc;
        }
    }

    wauvio::fade_in(out,  0.01, SR);
    wauvio::fade_out(out, 0.08, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  FAULT_CHASER
//
//  Dormant until flashlight malfunctions, then charges.
//  Approach: 0.5s near-silence → explosion of FM chaos.
//  Retreat: hard noise cut, no decay, then silence.  Interrupted, not stopped.
// =============================================================================

inline wauvio::Buffer gen_fault_chaser_approach() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 5.0;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // DORMANCY PHASE: 0s – 0.55s
    // Only a barely-audible sub hum (amp 0.008) — it is there but not moving
    sfx_detail::fm_event(out,
        0, 0.55,
        38.0, 38.0 * 1.6180339887, 0.3,
        0.008f, 0.10, 0.10, SR);

    // ACTIVATION INSTANT: sample 0.55s
    // Hard onset — amplitude goes from 0.008 to full in 0.005s (1 frame at 60fps)
    // Three simultaneous FM voices, all high index, all inharmonic:

    // Voice A: carrier 187 Hz, mod 187×√7, index 4.2 — primary attack
    sfx_detail::fm_event(out,
        static_cast<size_t>(0.55 * SR), 4.45,
        187.0, 187.0 * 2.6457513110645907, 4.2,
        0.062f, 0.005, 0.35, SR);

    // Voice B: carrier 89 Hz, mod 89×π, index 3.1 — sub-weight of the charge
    sfx_detail::fm_event(out,
        static_cast<size_t>(0.55 * SR), 4.45,
        89.0, 89.0 * 3.14159265358979, 3.1,
        0.045f, 0.005, 0.40, SR);

    // Voice C: carrier 331 Hz, mod 331×∛2, index 2.8 — high presence
    sfx_detail::fm_event(out,
        static_cast<size_t>(0.55 * SR), 4.45,
        331.0, 331.0 * 1.2599210498948732, 2.8,
        0.038f, 0.005, 0.45, SR);

    // Broadband noise burst at activation — the electrical discharge of the fault
    sfx_detail::noise_burst(out,
        static_cast<size_t>(0.55 * SR), 0.20,
        0.048f, 100.0, 6000.0, 0xF417C4A0u, SR);

    // Rapid amplitude modulation on charge section (8.7 Hz — flickering like the fault)
    {
        const double lfo_inc = wauvio::TWO_PI * 8.7 / SR;
        const size_t onset_s = static_cast<size_t>(0.55 * SR);
        double lp = 0.0;
        for (size_t i = onset_s; i < TOTAL; ++i) {
            const float mod = 0.75f + 0.25f * static_cast<float>(std::sin(lp));
            out[i] *= mod;
            lp += lfo_inc;
        }
    }

    wauvio::fade_in(out,  0.02, SR);
    wauvio::fade_out(out, 0.18, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_fault_chaser_retreat() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.7;
    const size_t TOTAL   = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Hard noise cut: no wind-down, no fade.  The charge was INTERRUPTED.
    // A single burst of broadband noise, very short (0.08s), then silence.
    // Followed by a low FM residue that cuts mid-oscillation (no release).
    sfx_detail::noise_burst(out,
        0, 0.08,
        0.075f, 60.0, 4000.0, 0xC071C700u, SR);

    // Electrical snap at the cut — very high frequency, very short
    sfx_detail::noise_burst(out,
        0, 0.02,
        0.050f, 5000.0, 12000.0, 0x5A75A700u, SR);

    // Residual FM: the charge tone still ringing for 0.12s then hard-cut
    // No fade_out on this voice — it just stops.
    {
        constexpr double C = 187.0, M = 187.0 * 2.6457513110645907;
        constexpr double IDX = 4.2;
        const size_t n = static_cast<size_t>(0.12 * SR);
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < n && i < TOTAL; ++i) {
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.055f;
            cp += c_inc;
            mp += m_inc;
        }
        // Hard cut at n: no fade.  The residue just stops.
    }

    // Remaining buffer (0.12s → 0.7s) is silence.  The echo of a sudden stop.
    wauvio::fade_in(out, 0.003, SR);
    // NO fade_out — intentional hard cut feel at end
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}

} // namespace sfx
