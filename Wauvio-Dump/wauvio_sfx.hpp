// =============================================================================
//  W A U V I O _ S F X . H P P  —  Sound Effect Presets
//
//  Optional module; depends on wauvio.hpp.
//  Provides ready-made sound effects synthesised entirely from the core engine.
//
//  Included effects:
//    - laser      : descending FM chirp
//    - explosion  : layered noise with low rumble
//    - beep       : short sinusoidal UI tone
//    - powerup    : rising pulse-wave sweep
//    - coin       : two-tone chime
//    - hit        : sharp noise transient
//
//  Usage:
//    #include "wauvio_sfx.hpp"
//    wauvio::Buffer snd = wauvio::sfx::laser();
//    wauvio::play(snd);
// =============================================================================

#pragma once
#include "wauvio.hpp"

namespace wauvio::sfx {

// =============================================================================
//  LASER  —  Descending FM frequency sweep
//  Simulates a sci-fi blaster / energy-weapon discharge.
//
//  Technique:
//    Sine carrier whose frequency drops exponentially from ~1200 Hz → ~200 Hz.
//    Amplitude also decays, giving a crisp front attack and fast tail.
// =============================================================================

/// @param duration  Length in seconds (default 0.35).
/// @param sample_rate  Override; 0 = use global.
inline Buffer laser(double duration = 0.35, int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t    = static_cast<double>(i) / sample_rate;
        // Exponential frequency drop: 1200 Hz → floor of ~200 Hz
        const double freq = 200.0 + 1000.0 * std::exp(-t * 10.0);
        phase += TWO_PI * freq / sample_rate;
        const float  env  = static_cast<float>(std::exp(-t * 7.0));
        buf[i] = static_cast<float>(std::sin(phase)) * env;
    }
    return buf;
}

// =============================================================================
//  EXPLOSION  —  Multi-band noise burst
//  Layered white noise through two IIR low-pass filters at different cutoffs:
//    - Deep band (< 400 Hz)  → body / punch
//    - Mid band  (< 1800 Hz) → presence / crackle
//  Both bands share a shared exponential amplitude decay.
// =============================================================================

/// @param duration  Length in seconds (default 1.8).
inline Buffer explosion(double duration = 1.8, int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);

    NoiseGenerator noise;
    LowPassFilter  lpf_deep(380.0,  sample_rate);
    LowPassFilter  lpf_mid (1700.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        const float  raw = noise.tick();
        const float  deep = lpf_deep.tick(raw);
        const float  mid  = lpf_mid .tick(raw);
        // Slower decay for deep, faster for mid
        const float env_d = static_cast<float>(std::exp(-t * 2.8));
        const float env_m = static_cast<float>(std::exp(-t * 5.0));
        buf[i] = deep * env_d * 1.3f + mid * env_m * 0.4f;
    }
    return buf;
}

// =============================================================================
//  BEEP  —  Short sinusoidal UI tone
//  A clean sine wave shaped with a fast ADSR — suitable for UI confirmations,
//  notifications, or button feedback.
// =============================================================================

/// @param freq      Frequency in Hz (default 880).
/// @param duration  Length in seconds (default 0.12).
inline Buffer beep(double freq = 880.0, double duration = 0.12,
                   int sample_rate = 0)
{
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    Instrument instr;
    instr.shape    = WaveShape::Sine;
    instr.envelope = ADSR(0.005, 0.01, 0.85, 0.04);
    instr.gain     = 0.8f;

    return instr.render_note(Note(freq, duration, 1.0), sample_rate);
}

// =============================================================================
//  POWERUP  —  Rising pulse-wave sweep
//  Frequency rises linearly from `start_freq` to `end_freq` over `duration`.
//  A pulse wave gives the classic chiptune / retro-game power-up character.
// =============================================================================

/// @param duration    Length in seconds (default 0.65).
/// @param start_freq  Starting frequency Hz (default 280).
/// @param end_freq    Ending frequency Hz   (default 1800).
inline Buffer powerup(double duration   = 0.65,
                      double start_freq = 280.0,
                      double end_freq   = 1800.0,
                      int sample_rate   = 0)
{
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t    = static_cast<double>(i) / sample_rate;
        // Linear frequency ramp
        const double freq = start_freq + (end_freq - start_freq) * (t / duration);
        phase += TWO_PI * freq / sample_rate;
        // Amplitude slightly decreases toward tail to prevent abrupt cut
        const float env = 0.7f * (1.0f - static_cast<float>(t / duration) * 0.25f);
        buf[i] = wave::pulse(phase, 0.3) * env;
    }
    return buf;
}

// =============================================================================
//  COIN  —  Two-tone chime  (retro collect sound)
//  Plays a quick ascending two-note sine sequence with very short ADSR.
// =============================================================================

/// @param sample_rate  Override; 0 = use global.
inline Buffer coin(int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    Instrument instr;
    instr.shape    = WaveShape::Sine;
    instr.envelope = ADSR(0.003, 0.01, 0.8, 0.06);
    instr.gain     = 0.75f;

    Buffer a = instr.render_note(Note(988.0,  0.07, 1.0), sample_rate);
    Buffer b = instr.render_note(Note(1480.0, 0.10, 1.0), sample_rate);

    Buffer out;
    out.reserve(a.size() + b.size());
    for (float s : a) out.push_back(s);
    for (float s : b) out.push_back(s);
    return out;
}

// =============================================================================
//  HIT  —  Sharp noise transient
//  Very short high-pass-filtered noise burst — suitable for impact sounds,
//  punch effects, or percussive UI events.
// =============================================================================

/// @param duration  Length in seconds (default 0.08).
inline Buffer hit(double duration = 0.08, int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);

    NoiseGenerator noise;
    HighPassFilter hpf(1800.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        const float  env = static_cast<float>(std::exp(-t * 40.0));
        buf[i] = hpf.tick(noise.tick()) * env;
    }
    return buf;
}

} // namespace wauvio::sfx
