#pragma once

// =============================================================================
//  wauvio_sfx_system.hpp
//  System sound effects for ExplorationState and Sequence2.
//
//  All sounds are pure functions: no global state, no playback,
//  no external dependencies beyond wauvio.hpp.
//
//  Normalization targets (AUDIO_ARCHITECTURE §5.1):
//    Flashlight on/off:  0.55
//    Flashlight fault:   0.65
//    Sanity:             0.75 (sting), 0.55 (subtle)
//    UI:                 0.50
//    Environment:        0.55
// =============================================================================

#include "../wauvio.hpp"

namespace sfx {
namespace system {

// =============================================================================
//  INTERNAL HELPERS  (file-local, not exposed)
// =============================================================================

namespace {

// Write a short FM event into buf at sample offset s0.
void sys_fm(wauvio::Buffer& buf,
            size_t s0, double dur_s,
            double carrier_hz, double mod_hz, double mod_idx,
            float amp, double attack_s, double release_s, int SR)
{
    const size_t n   = static_cast<size_t>(dur_s * SR);
    const size_t end = std::min(s0 + n, buf.size());
    const size_t len = end - s0;
    if (len == 0) return;
    const double c_inc = wauvio::TWO_PI * carrier_hz / SR;
    const double m_inc = wauvio::TWO_PI * mod_hz     / SR;
    double cp = 0.0, mp = 0.0;
    const size_t fi = static_cast<size_t>(attack_s  * SR);
    const size_t fo = static_cast<size_t>(release_s * SR);
    for (size_t i = 0; i < len; ++i) {
        float env = 1.0f;
        if (i < fi && fi > 0)             env = static_cast<float>(i)           / static_cast<float>(fi);
        else if (i >= len - fo && fo > 0) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo);
        buf[s0 + i] += static_cast<float>(
            std::sin(cp + mod_idx * std::sin(mp))) * amp * env;
        cp += c_inc; mp += m_inc;
    }
}

// Write a band-limited noise burst into buf at sample offset s0.
void sys_noise(wauvio::Buffer& buf,
               size_t s0, double dur_s,
               float amp, double hp_hz, double lp_hz,
               uint32_t seed, int SR)
{
    const size_t n   = static_cast<size_t>(dur_s * SR);
    const size_t end = std::min(s0 + n, buf.size());
    const size_t len = end - s0;
    if (len == 0) return;
    wauvio::NoiseGenerator ng(seed);
    wauvio::HighPassFilter hp(hp_hz, SR);
    wauvio::LowPassFilter  lp(lp_hz, SR);
    const size_t ramp = std::min(len / 4, static_cast<size_t>(0.010 * SR));
    for (size_t i = 0; i < len; ++i) {
        float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
        float env = 1.0f;
        if (i < ramp)        env = static_cast<float>(i)           / static_cast<float>(ramp);
        if (i >= len - ramp) env = static_cast<float>(len - 1 - i) / static_cast<float>(ramp);
        buf[s0 + i] += s * amp * env;
    }
}

// Write an FM glide (carrier interpolates start→end) into buf at offset s0.
void sys_fm_glide(wauvio::Buffer& buf,
                  size_t s0, double dur_s,
                  double c_start, double c_end,
                  double mod_ratio, double mod_idx,
                  float amp_start, float amp_end,
                  double attack_s, double release_s, int SR)
{
    const size_t n   = static_cast<size_t>(dur_s * SR);
    const size_t end = std::min(s0 + n, buf.size());
    const size_t len = end - s0;
    if (len == 0) return;
    double cp = 0.0, mp = 0.0;
    const size_t fi = static_cast<size_t>(attack_s  * SR);
    const size_t fo = static_cast<size_t>(release_s * SR);
    for (size_t i = 0; i < len; ++i) {
        const double t      = static_cast<double>(i) / static_cast<double>(len);
        const double freq_c = c_start + t * (c_end - c_start);
        const float  amp    = amp_start + static_cast<float>(t) * (amp_end - amp_start);
        float env = 1.0f;
        if (i < fi && fi > 0)             env = static_cast<float>(i)           / static_cast<float>(fi);
        else if (i >= len - fo && fo > 0) env = static_cast<float>(len - 1 - i) / static_cast<float>(fo);
        buf[s0 + i] += static_cast<float>(
            std::sin(cp + mod_idx * std::sin(mp))) * amp * env;
        cp += wauvio::TWO_PI * freq_c              / SR;
        mp += wauvio::TWO_PI * (freq_c * mod_ratio)/ SR;
    }
}

} // anonymous namespace


// =============================================================================
//  §1  FLASHLIGHT SYSTEM
// =============================================================================

// Purpose:     Flashlight activation — responsive, tactile, physical.
// Design:      A short electrical tick followed by a narrow-band HF transient.
//              The "click" is a hard FM onset; the "on" is a brief high-frequency
//              settling tone that decays quickly. Fast but not harsh.
// Key choices: High-index FM (4.5) at 480 Hz for the click character.
//              A short HPF-filtered noise tail simulates the capacitor discharge.
//              Total duration: 0.18s — tight and responsive.
inline wauvio::Buffer gen_flashlight_on() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.18;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Click transient: high-index FM, very fast attack (1ms), moderate release
    sys_fm(out, 0, 0.12,
           480.0, 480.0 * 1.6180339887, 4.5,
           0.075f, 0.001, 0.10, SR);

    // Settling hiss: HPF noise above 3kHz — electrical discharge tail
    sys_noise(out, static_cast<size_t>(0.01 * SR), 0.10,
              0.035f, 3000.0, 8000.0, 0xF1A5H000u, SR);

    // Sub-click: brief very low pulse for tactile body (felt, not heard)
    sys_fm(out, 0, 0.04,
           55.0, 55.0 * 2.41421356, 1.2,
           0.040f, 0.001, 0.035, SR);

    wauvio::fade_in(out, 0.001, SR);
    wauvio::fade_out(out, 0.025, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

// Purpose:     Flashlight deactivation — softer, with a brief decay tail.
// Design:      Inverse of 'on': no onset click, just a gentle HF damping and a
//              very short FM tone that falls in pitch as the light collapses.
//              Should feel like a physical switch with spring tension releasing.
// Key choices: Descending FM glide (820→180 Hz, 0.20s). Low mod index (0.6)
//              keeps it clean and non-aggressive. Noise below 1kHz for the
//              mechanical release. Total duration: 0.28s.
inline wauvio::Buffer gen_flashlight_off() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.28;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Pitch fall: the light "collapses" downward in frequency
    sys_fm_glide(out, 0, 0.20,
                 820.0, 180.0, 1.4142135623, 0.6,
                 0.055f, 0.005f, 0.005, 0.12, SR);

    // Soft mechanical noise: low-mid band, brief — the switch mechanism
    sys_noise(out, 0, 0.12,
              0.022f, 200.0, 1200.0, 0xF1A5H0FFu, SR);

    wauvio::fade_in(out, 0.002, SR);
    wauvio::fade_out(out, 0.06, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

// Purpose:     Flashlight flickering during malfunction (FLICKER_DURATION phase).
// Design:      Chaotic and broken — multiple micro-bursts at irregular intervals
//              with a fast-jittering amplitude. No rhythmic pattern.
//              Covers the 1.8s FLICKER_DURATION defined in FlashlightSystem.
// Key choices: Seven noise bursts at irrational offsets (not metered).
//              Each burst slightly different: different seed → different texture.
//              FM crackle between bursts adds instability. No clean tones.
//              Distortion (soft clip) applied to the whole buffer for grit.
inline wauvio::Buffer gen_flashlight_flicker() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 1.8;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Seven chaotic bursts at irrational positions
    const double burst_times[] = { 0.04, 0.19, 0.31, 0.56, 0.83, 1.07, 1.51 };
    const double burst_durs[]  = { 0.07, 0.04, 0.09, 0.05, 0.06, 0.08, 0.05 };
    const double burst_hp[]    = { 1200, 2400, 800,  1800, 1000, 2000, 1500  };
    const double burst_lp[]    = { 4000, 7000, 2800, 5500, 3200, 6000, 4800  };
    const uint32_t seeds[]     = {
        0xF1CK001u, 0xF1CK002u, 0xF1CK003u, 0xF1CK004u,
        0xF1CK005u, 0xF1CK006u, 0xF1CK007u
    };

    for (int b = 0; b < 7; ++b) {
        sys_noise(out,
                  static_cast<size_t>(burst_times[b] * SR),
                  burst_durs[b],
                  0.050f, burst_hp[b], burst_lp[b], seeds[b], SR);
    }

    // Continuous low-level instability between bursts: HPF @ 400 Hz
    // Represents the circuit struggling — always present, never clean
    {
        wauvio::NoiseGenerator ng(0xF1CK0FFu);
        wauvio::HighPassFilter hp(400.0, SR);
        wauvio::LowPassFilter  lp(1600.0, SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            out[i] += s * 0.018f;
        }
    }

    // FM crackle at onset and at t=0.83s (the worst flicker moment)
    sys_fm(out, 0, 0.05,
           320.0, 320.0 * 1.6180339887, 5.2,
           0.040f, 0.001, 0.04, SR);
    sys_fm(out, static_cast<size_t>(0.83 * SR), 0.06,
           280.0, 280.0 * 2.41421356, 4.8,
           0.038f, 0.001, 0.05, SR);

    // Apply soft-clip for grit — simulates electrical overload
    wauvio::distortion::apply_soft_clip(out, 2.2f);

    wauvio::fade_in(out, 0.005, SR);
    wauvio::fade_out(out, 0.08, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.65f);
    return out;
}

// Purpose:     Flashlight total failure — the OUT_DURATION phase where light dies.
// Design:      Hard cutoff character. The pitch falls quickly then noise drops
//              abruptly. Followed by 0.4s of near-silence (the dead state).
//              The abruptness is the design — contrast with the flicker's chaos.
// Key choices: Fast descending FM glide (600→30 Hz, 0.3s) — pitch crashes.
//              Mid-band noise burst at 0.08s — the final electrical hiccup.
//              Hard fade_out at 0.30s, leaving 0.4s of silence at the end.
//              Total duration: 0.70s (fits within OUT_DURATION of 1.4s).
inline wauvio::Buffer gen_flashlight_fail() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.70;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Pitch crash: FM glide from 600 Hz down to near-DC
    sys_fm_glide(out, 0, 0.28,
                 600.0, 30.0, 1.7320508075688773, 2.4,
                 0.070f, 0.0f, 0.002, 0.04, SR);

    // Final hiccup: short noise burst mid-fall
    sys_noise(out, static_cast<size_t>(0.08 * SR), 0.08,
              0.045f, 300.0, 2400.0, 0xFA11F001u, SR);

    // Hard amplitude cut at 0.30s — the light dies
    const size_t cut = static_cast<size_t>(0.30 * SR);
    for (size_t i = cut; i < TOTAL; ++i) out[i] = 0.f;

    wauvio::fade_in(out, 0.002, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.65f);
    return out;
}


// =============================================================================
//  §2  SANITY SYSTEM
// =============================================================================

// Purpose:     Trigger sound when hallucination text begins appearing.
// Design:      Barely there — a breath-like low-amplitude noise onset.
//              Something has arrived at the threshold of perception.
//              Should not startle; should unsettle.
// Key choices: Very narrow band noise (HPF 2800 Hz, LPF 3800 Hz) — like breath
//              passing through teeth. Amplitude rises slowly (0.3s attack).
//              A barely-audible sub-FM voice at 42 Hz gives it weight.
//              Total duration: 0.9s.
inline wauvio::Buffer gen_sanity_whisper_start() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.9;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Breath onset: narrow HF noise, slow rise
    {
        wauvio::NoiseGenerator ng(0xBEA37001u);
        wauvio::HighPassFilter hp(2800.0, SR);
        wauvio::LowPassFilter  lp(3800.0, SR);
        const size_t ramp = static_cast<size_t>(0.3 * SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            const float env = (i < ramp)
                ? static_cast<float>(i) / static_cast<float>(ramp)
                : 1.0f;
            out[i] += s * 0.040f * env;
        }
    }

    // Sub-weight: barely audible presence below speech register
    sys_fm(out, static_cast<size_t>(0.2 * SR), 0.65,
           42.0, 42.0 * 1.6180339887, 0.4,
           0.022f, 0.30, 0.25, SR);

    wauvio::fade_in(out, 0.01, SR);
    wauvio::fade_out(out, 0.15, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

// Purpose:     Short repeatable whisper fragment — played intermittently during
//              hallucination text display. NOT a loop; retriggered by game logic.
// Design:      A 0.4s fragment that uses a different synthesis path each time
//              it is *generated* (deterministic but varied via fixed seeds that
//              produce spectrally distinct noise). The voice-like character
//              comes from a formant-approximation: two narrow noise bands at
//              vowel-like frequencies (700 Hz, 1200 Hz), amplitude-enveloped
//              to suggest a spoken syllable shape. No actual words.
// Key choices: Two formant bands summed: F1 ≈ 700 Hz (bandpass 500–900 Hz),
//              F2 ≈ 1200 Hz (bandpass 1000–1500 Hz). Both shaped with a fast
//              attack (0.04s) and mid release (0.25s) — syllable-like.
//              A slow sub-sine at 78 Hz adds tonal weight without pitch identity.
//              Total duration: 0.4s.
inline wauvio::Buffer gen_sanity_whisper_loop_fragment() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.4;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Formant 1 — low vowel resonance (~700 Hz)
    {
        wauvio::NoiseGenerator ng(0xF0RM4NT1u);
        wauvio::HighPassFilter hp(500.0,  SR);
        wauvio::LowPassFilter  lp(900.0,  SR);
        const size_t fi = static_cast<size_t>(0.04 * SR);
        const size_t fo = static_cast<size_t>(0.25 * SR);
        const size_t n  = TOTAL;
        for (size_t i = 0; i < n; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            float env = 1.0f;
            if (i < fi)        env = static_cast<float>(i)           / static_cast<float>(fi);
            if (i >= n - fo)   env = static_cast<float>(n - 1 - i)   / static_cast<float>(fo);
            out[i] += s * 0.045f * env;
        }
    }

    // Formant 2 — upper consonant resonance (~1200 Hz)
    {
        wauvio::NoiseGenerator ng(0xF0RM4NT2u);
        wauvio::HighPassFilter hp(1000.0, SR);
        wauvio::LowPassFilter  lp(1500.0, SR);
        const size_t fi = static_cast<size_t>(0.02 * SR);
        const size_t fo = static_cast<size_t>(0.28 * SR);
        const size_t n  = TOTAL;
        for (size_t i = 0; i < n; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            float env = 1.0f;
            if (i < fi)        env = static_cast<float>(i)           / static_cast<float>(fi);
            if (i >= n - fo)   env = static_cast<float>(n - 1 - i)   / static_cast<float>(fo);
            out[i] += s * 0.030f * env;
        }
    }

    // Sub-tonal weight — not a pitch, just presence
    sys_fm(out, 0, DUR,
           78.0, 78.0 * 1.41421356, 0.3,
           0.015f, 0.06, 0.18, SR);

    wauvio::fade_in(out, 0.008, SR);
    wauvio::fade_out(out, 0.05, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

// Purpose:     The whisper presence dissolves — something leaving.
// Design:      The formant character from the fragment, but in reverse:
//              starts present and thins to nothing. Uses a high-shelf
//              filter that progressively rolls off — the voice walks away.
//              Longer than the fragment; closure needs time.
// Key choices: Broadband voice-like noise (HPF 400 Hz, LPF 3500 Hz) starts
//              full and fades via time-varying LP cutoff (3500→400 Hz, per-block).
//              A final low FM event at 0.9s represents the last physical trace.
//              Total duration: 1.4s.
inline wauvio::Buffer gen_sanity_whisper_end() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 1.4;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Dissolving voice: LP cutoff sweeps downward over time
    {
        wauvio::NoiseGenerator ng(0xAA150ED0u);
        wauvio::HighPassFilter hp(400.0, SR);
        wauvio::LowPassFilter  lp(3500.0, SR); // initial cutoff
        constexpr int UPDATE_N = 128;

        for (size_t i = 0; i < TOTAL; ++i) {
            // Update LP cutoff every UPDATE_N samples
            if (i % UPDATE_N == 0) {
                const float t_norm = static_cast<float>(i) / static_cast<float>(TOTAL);
                const double new_lp = 3500.0 - t_norm * (3500.0 - 400.0);
                lp.set_cutoff(std::max(400.0, new_lp), SR);
            }
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            const float env = 1.0f - static_cast<float>(i) / static_cast<float>(TOTAL);
            out[i] += s * 0.040f * env * env;  // quadratic fade
        }
    }

    // Trace: a final very quiet low FM tone — the ghost of it
    sys_fm(out, static_cast<size_t>(0.9 * SR), 0.5,
           55.0, 55.0 * 1.6180339887, 0.3,
           0.012f, 0.15, 0.35, SR);

    wauvio::fade_in(out, 0.04, SR);
    wauvio::fade_out(out, 0.20, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

// Purpose:     Sudden psychological hit — brief, sharp, uncomfortable.
//              Triggered by high hallucination events or sanity threshold.
// Design:      Two simultaneous dissonant FM voices at an interval that
//              creates strong beating (major 7th: ratio ≈ 1.888).
//              Onset is near-instantaneous (2ms). Duration is short (0.6s).
//              The discomfort comes from the interval, not the volume.
// Key choices: Voice 1: carrier 211 Hz, Voice 2: 211 × 1.888 = 398 Hz.
//              Both FM with moderate index (1.6, 1.4).
//              A hard-clipped noise burst at onset creates the "spike" character.
//              Soft-clip applied to entire output — the edge stays controlled.
inline wauvio::Buffer gen_sanity_spike() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.6;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Dissonant pair: major 7th interval (ratio 15:8 ≈ 1.875, close enough for unease)
    sys_fm(out, 0, DUR,
           211.0, 211.0 * 1.6180339887, 1.6,
           0.058f, 0.002, 0.35, SR);

    sys_fm(out, 0, DUR,
           398.0, 398.0 * 1.41421356, 1.4,
           0.052f, 0.002, 0.40, SR);

    // Spike onset: hard-clipped noise — the "hit" character
    {
        wauvio::NoiseGenerator ng(0x5P1KE000u);
        const size_t spike_n = static_cast<size_t>(0.04 * SR);
        const size_t spike_end = std::min(spike_n, TOTAL);
        for (size_t i = 0; i < spike_end; ++i) {
            float s = ng.tick();
            s = wauvio::distortion::hard_clip(s, 0.4f);
            out[i] += s * 0.060f;
        }
    }

    wauvio::distortion::apply_soft_clip(out, 1.8f);
    wauvio::fade_in(out, 0.002, SR);
    wauvio::fade_out(out, 0.20, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.75f);
    return out;
}


// =============================================================================
//  §3  UI / FEEDBACK
// =============================================================================

// Purpose:     Positive confirmation — inventory use, item pick-up, action accepted.
// Design:      Minimal and clean. A single short tone at a register that sits
//              above the ambient mix without clashing. No sharp onset.
//              Should feel like a quiet system acknowledgment.
// Key choices: Low-index FM (0.4) at 680 Hz — almost sine, just slightly complex.
//              Very fast but smooth attack (8ms), moderate release (0.18s).
//              No noise. No distortion. Pure signal. Total: 0.25s.
inline wauvio::Buffer gen_ui_confirm() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.25;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    sys_fm(out, 0, DUR,
           680.0, 680.0 * 1.6180339887, 0.4,
           0.060f, 0.008, 0.18, SR);

    // Harmonic ghost: a second voice at 1.5× (a fifth up) at very low amplitude
    // Adds slight warmth without becoming musical.
    sys_fm(out, 0, DUR,
           1020.0, 1020.0 * 1.6180339887, 0.2,
           0.018f, 0.012, 0.22, SR);

    wauvio::fade_in(out, 0.003, SR);
    wauvio::fade_out(out, 0.04, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

// Purpose:     Negative feedback — invalid action, blocked input.
// Design:      Harder than confirm but not alarming. Uses a detuned pair
//              to signal "wrong" without being harsh. Slightly lower register
//              than confirm so it reads as different in character.
// Key choices: Two FM voices separated by an irrational ratio (× √2 = 1.414),
//              creating a slightly dissonant timbre. Faster decay than confirm.
//              Noise burst of 12ms at onset gives it a "bump" quality.
//              Total: 0.22s.
inline wauvio::Buffer gen_ui_error() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.22;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Detuned pair: √2 interval (tritone-ish) — the "wrong" interval
    sys_fm(out, 0, DUR,
           440.0, 440.0 * 1.6180339887, 0.8,
           0.050f, 0.004, 0.15, SR);

    sys_fm(out, 0, DUR,
           440.0 * 1.41421356, 440.0 * 1.41421356 * 1.6180339887, 0.6,
           0.040f, 0.006, 0.16, SR);

    // Onset bump: very short mid-band noise
    sys_noise(out, 0, 0.012,
              0.030f, 400.0, 1800.0, 0xE22020u, SR);

    wauvio::fade_in(out, 0.002, SR);
    wauvio::fade_out(out, 0.04, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

// Purpose:     Subtle feedback tick — footstep accent, menu cursor, brief pulse.
// Design:      The shortest sound in this file (0.06s). A single transient.
//              Must not draw attention; must be felt rather than heard.
//              Design target: sub-perceptual but confirmatory.
// Key choices: Single low-mid FM tap (180 Hz, idx 1.2) with 1ms attack,
//              50ms exponential release. No noise. Total: 0.06s.
inline wauvio::Buffer gen_ui_tick() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.06;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    sys_fm(out, 0, DUR,
           180.0, 180.0 * 1.6180339887, 1.2,
           0.060f, 0.001, 0.05, SR);

    wauvio::fade_in(out, 0.001, SR);
    wauvio::fade_out(out, 0.015, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}


// =============================================================================
//  §4  GENERIC INTERACTION / ENVIRONMENT
// =============================================================================

// Purpose:     Low-intensity physical bump — soft collision, interaction with
//              environment, item landing, door brush.
// Design:      A low FM thud with fast onset and moderate decay. Should feel
//              physical without weight. No sharp transient; rounded attack.
// Key choices: Carrier 88 Hz, mod 88×√3 (inharmonic), idx 2.8 (tonal thud
//              character). Amplitude envelope: 5ms attack, 200ms decay.
//              Light broadband noise at onset (60–300 Hz) for the impact texture.
//              Total: 0.28s.
inline wauvio::Buffer gen_soft_impact() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.28;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    sys_fm(out, 0, DUR,
           88.0, 88.0 * 1.7320508075688773, 2.8,
           0.065f, 0.005, 0.22, SR);

    // Impact texture: sub-band noise at onset
    sys_noise(out, 0, 0.06,
              0.030f, 60.0, 300.0, 0x50F71000u, SR);

    wauvio::fade_in(out, 0.003, SR);
    wauvio::fade_out(out, 0.05, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

// Purpose:     Digital/unstable artifact — used for distortion events, shader
//              malfunction, reality-glitch moments.
// Design:      Three overlapping synthesis elements that feel electronically broken:
//              a hard-clipped noise burst, a rapidly-modulated FM voice, and a
//              very brief square-wave burst. Irregular envelope — no smooth fade.
// Key choices: Hard-clipped noise (full band) — raw, damaged character.
//              FM with rapid index LFO (17 Hz tremolo on the FM voice itself)
//              to create the stuttering digital quality.
//              Hard waveshaper on final output (knee=0.9f). Total: 0.35s.
inline wauvio::Buffer gen_glitch() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.35;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Hard-clipped broadband noise — the raw digital break
    {
        wauvio::NoiseGenerator ng(0x611A1C00u);
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick();
            s = wauvio::distortion::hard_clip(s, 0.3f);
            out[i] += s * 0.035f;
        }
    }

    // Rapidly tremolo'd FM — the unstable carrier
    {
        constexpr double C   = 340.0, M = 340.0 * 1.41421356;
        constexpr double IDX = 3.2;
        const double c_inc   = wauvio::TWO_PI * C / SR;
        const double m_inc   = wauvio::TWO_PI * M / SR;
        const double t_inc   = wauvio::TWO_PI * 17.0 / SR; // 17 Hz tremolo
        double cp = 0.0, mp = 0.0, tp = 0.0;
        const size_t fi = static_cast<size_t>(0.002 * SR);
        const size_t fo = static_cast<size_t>(0.08  * SR);
        for (size_t i = 0; i < TOTAL; ++i) {
            const float trem = 0.4f + 0.6f * static_cast<float>(std::sin(tp));
            float env = 1.0f;
            if (i < fi)             env = static_cast<float>(i)           / static_cast<float>(fi);
            if (i >= TOTAL - fo)    env = static_cast<float>(TOTAL-1-i)   / static_cast<float>(fo);
            out[i] += static_cast<float>(std::sin(cp + IDX * std::sin(mp)))
                      * 0.042f * trem * env;
            cp += c_inc; mp += m_inc; tp += t_inc;
        }
    }

    // Brief square-wave burst at onset: 480 Hz, 30ms
    {
        wauvio::Oscillator sq(wauvio::WaveShape::Square, 480.0, 0.025);
        const size_t sq_n = static_cast<size_t>(0.030 * SR);
        const size_t fi = static_cast<size_t>(0.001 * SR);
        for (size_t i = 0; i < sq_n && i < TOTAL; ++i) {
            float env = 1.0f;
            if (i < fi) env = static_cast<float>(i) / static_cast<float>(fi);
            out[i] += sq.tick(SR) * env;
        }
    }

    // Waveshaper for final asymmetric distortion
    wauvio::distortion::apply_waveshaper(out, 0.9f);

    wauvio::fade_in(out, 0.002, SR);
    wauvio::fade_out(out, 0.06, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

// Purpose:     Rising tension cue — "something is happening", suspicion rising,
//              entity approaching threshold, environmental shift.
// Design:      An ascending FM glide from low to mid register over 1.2s.
//              Not musical; not resolved. It rises and stops — no arrival.
//              The stopping is the tension.
// Key choices: Carrier glides from 62 Hz → 196 Hz (covers a full tension arc).
//              Mod ratio: √5 (inharmonic throughout the glide). Index: 1.0→2.4
//              (timbre opens as pitch rises — complexity grows with urgency).
//              Noise texture widens from sub-only to mid as tension builds.
//              Total: 1.2s.
inline wauvio::Buffer gen_presence_rise() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 1.2;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Primary rising FM
    sys_fm_glide(out, 0, DUR,
                 62.0, 196.0,
                 2.2360679774997896,  // √5
                 1.0,                 // index (static — amplitude carries the rise)
                 0.015f, 0.058f,
                 0.06, 0.25, SR);

    // Modulation index also rises — rendered as a secondary pass
    // (Achieved via manual per-sample render for full control)
    {
        constexpr double C_START = 62.0, C_END = 196.0;
        constexpr double IDX_START = 1.0, IDX_END = 2.4;
        const double c_inc_base = wauvio::TWO_PI * C_START / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            const double t      = static_cast<double>(i) / static_cast<double>(TOTAL);
            const double freq_c = C_START + t * (C_END - C_START);
            const double idx    = IDX_START + t * (IDX_END - IDX_START);
            // Secondary layer at 50% of primary — adds the opening timbre
            out[i] += static_cast<float>(
                0.5 * std::sin(cp + idx * std::sin(mp))) * 0.020f;
            cp += wauvio::TWO_PI * freq_c              / SR;
            mp += wauvio::TWO_PI * (freq_c * 2.23606)  / SR;
        }
    }

    // Noise texture: HP cutoff descends as tension builds (more bass enters)
    {
        wauvio::NoiseGenerator ng(0x215E0000u);
        wauvio::HighPassFilter hp(800.0, SR);
        wauvio::LowPassFilter  lp(1200.0, SR);
        constexpr int UPDATE_N = 256;
        for (size_t i = 0; i < TOTAL; ++i) {
            if (i % UPDATE_N == 0) {
                const float t = static_cast<float>(i) / static_cast<float>(TOTAL);
                const double new_hp = 800.0 - t * (800.0 - 80.0);
                hp.set_cutoff(std::max(80.0, new_hp), SR);
            }
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            const float amp = 0.008f + 0.012f * (static_cast<float>(i) / static_cast<float>(TOTAL));
            out[i] += s * amp;
        }
    }

    wauvio::fade_in(out, 0.02, SR);
    wauvio::fade_out(out, 0.12, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.55f);
    return out;
}

} // namespace system
} // namespace sfx
