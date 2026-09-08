#pragma once

// =============================================================================
//  wauvio_ambience.hpp
//  Procedural ambience system — single header, zero assets.
//
//  Namespaces
//  ----------
//  ambience::surface   — open-air cave mouth, wind, faint drips
//  ambience::cave      — deep cave passage, resonant hum, dripping water
//  ambience::deep      — below the cave, pressure, low rumble, tonal drones
//  ambience::eerie     — distorted cave when caveDistort = true
//  ambience::static_   — white-noise static, Sequence1 "fracture" moment
//
//  Each namespace exposes one public function:
//
//      wauvio::Buffer gen();
//
//  Architecture compliance
//  -----------------------
//  - All buffers: 20 – 60 seconds (architecture §4.1)
//  - Seamless loop: fade-in and fade-out >= 20 ms (§4.1)
//  - Normalized to peak 0.35 (ambience target, §5.1)
//  - Deterministic: all noise seeded from fixed constexpr seeds (§3.2)
//  - No wauvio::play(), no threads, no file I/O (§3.3)
//  - No game logic or GameContext references (§3.3)
//  - All effect units pre-warmed before synthesis loop (§6.2)
//  - Sample rate via global_config() — never hardcoded (§6.4)
// =============================================================================

#include "../wauvio.hpp"

// =============================================================================
//  INTERNAL SYNTHESIS HELPERS  (private to this translation unit)
// =============================================================================

namespace ambience_detail {

// ---------------------------------------------------------------------------
//  Crossfade the tail of a buffer into its head so it loops without a click.
//  xfade_samples: number of samples on each end to blend.
// ---------------------------------------------------------------------------
inline void crossfade_loop(wauvio::Buffer& buf, size_t xfade_samples) {
    if (buf.size() < xfade_samples * 2) return;
    const size_t N = buf.size();
    for (size_t i = 0; i < xfade_samples; ++i) {
        const float t    = static_cast<float>(i) / static_cast<float>(xfade_samples);
        const float head = buf[i];
        const float tail = buf[N - xfade_samples + i];
        // Blend tail into head (fade out tail, fade in head)
        buf[i]                      = head * t + tail * (1.f - t);
        buf[N - xfade_samples + i]  = tail * t + head * (1.f - t);
    }
}

// ---------------------------------------------------------------------------
//  Render a slow sine LFO into a modulation buffer [0, 1].
// ---------------------------------------------------------------------------
inline wauvio::Buffer make_lfo_mod(double duration_s, double rate_hz,
                                   double phase_offset = 0.0,
                                   int sample_rate = 0)
{
    if (sample_rate <= 0) sample_rate = wauvio::global_config().sample_rate;
    const size_t n = static_cast<size_t>(duration_s * sample_rate);
    wauvio::Buffer mod(n);
    const double inc = wauvio::TWO_PI * rate_hz / sample_rate;
    double phase = phase_offset;
    for (size_t i = 0; i < n; ++i) {
        mod[i] = static_cast<float>(0.5 + 0.5 * std::sin(phase));
        phase += inc;
    }
    return mod;
}

// ---------------------------------------------------------------------------
//  Apply a modulation buffer as a gain envelope to buf in-place.
//  depth: 0 = no modulation (flat), 1 = full range (0 to 2x)
//  centre: the unmodulated gain level
// ---------------------------------------------------------------------------
inline void apply_mod(wauvio::Buffer& buf, const wauvio::Buffer& mod,
                      float centre, float depth)
{
    const size_t n = std::min(buf.size(), mod.size());
    for (size_t i = 0; i < n; ++i)
        buf[i] *= centre + depth * (mod[i] - 0.5f) * 2.f;
}

// ---------------------------------------------------------------------------
//  Build a band-passed noise layer: noise → HP → LP.
// ---------------------------------------------------------------------------
inline wauvio::Buffer band_noise(double duration_s, uint32_t seed,
                                 double hp_hz, double lp_hz,
                                 float gain = 1.f, int sample_rate = 0)
{
    if (sample_rate <= 0) sample_rate = wauvio::global_config().sample_rate;
    wauvio::NoiseGenerator ng(seed);
    wauvio::Buffer buf = ng.render(duration_s, sample_rate);

    wauvio::HighPassFilter hp(hp_hz, sample_rate);
    wauvio::LowPassFilter  lp(lp_hz, sample_rate);
    hp.process(buf);
    lp.process(buf);
    wauvio::apply_gain(buf, gain);
    return buf;
}

// ---------------------------------------------------------------------------
//  Drip impulse: short sine burst with exponential decay.
//  Returns a short buffer (< 0.25 s) suitable for mixing at an offset.
// ---------------------------------------------------------------------------
inline wauvio::Buffer make_drip(double freq_hz, double decay_s,
                                float amplitude, int sample_rate = 0)
{
    if (sample_rate <= 0) sample_rate = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.22;
    const size_t n = static_cast<size_t>(DUR * sample_rate);
    wauvio::Buffer buf(n);
    const double inc = wauvio::TWO_PI * freq_hz / sample_rate;
    double phase = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const float t   = static_cast<float>(i) / static_cast<float>(sample_rate);
        const float env = amplitude * std::exp(-t / static_cast<float>(decay_s));
        buf[i] = static_cast<float>(std::sin(phase)) * env;
        phase += inc;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  Mix a short buffer into dst at sample offset, clamped to dst length.
// ---------------------------------------------------------------------------
inline void mix_at(wauvio::Buffer& dst, const wauvio::Buffer& src, size_t offset) {
    const size_t end = std::min(dst.size(), offset + src.size());
    for (size_t i = offset; i < end; ++i)
        dst[i] += src[i - offset];
}

} // namespace ambience_detail


// =============================================================================
//  ambience::surface
//  Scene: cave entrance / open-air mouth.
//  Character: wind gusts, faint high resonance, very sparse drips outside.
//  Duration: 30 s
// =============================================================================

namespace ambience {
namespace surface {

inline wauvio::Buffer gen() {
    const int    SR  = wauvio::global_config().sample_rate;
    constexpr double DUR        = 30.0;
    constexpr double FADE_S     = 0.08;
    constexpr size_t XFADE_MS  = 60;   // crossfade in ms

    // ---- Layer 1: low wind rumble — broadband noise, HPF @ 40 Hz, LPF @ 220 Hz
    wauvio::Buffer wind_low = ambience_detail::band_noise(DUR, 0xA1B2C3D4u,
                                                          40.0, 220.0, 0.55f, SR);

    // ---- Layer 2: mid wind breath — HPF @ 200 Hz, LPF @ 900 Hz
    wauvio::Buffer wind_mid = ambience_detail::band_noise(DUR, 0xD4C3B2A1u,
                                                          200.0, 900.0, 0.30f, SR);

    // ---- Layer 3: high air hiss — HPF @ 2000 Hz, LPF @ 6000 Hz (very quiet)
    wauvio::Buffer wind_hi  = ambience_detail::band_noise(DUR, 0xFEDCBA98u,
                                                          2000.0, 6000.0, 0.10f, SR);

    // ---- Modulate wind layers with slow LFOs to simulate gusts
    //      Gust cycle ~7 s, slight phase offset between low and mid
    auto lfo_gust_low = ambience_detail::make_lfo_mod(DUR, 1.0 / 7.0, 0.0,       SR);
    auto lfo_gust_mid = ambience_detail::make_lfo_mod(DUR, 1.0 / 9.0, wauvio::PI, SR);
    ambience_detail::apply_mod(wind_low, lfo_gust_low, 0.70f, 0.30f);
    ambience_detail::apply_mod(wind_mid, lfo_gust_mid, 0.60f, 0.40f);

    // ---- Layer 4: faint cave-mouth resonance — narrow sine at ~185 Hz
    wauvio::Oscillator resonance(wauvio::WaveShape::Sine, 185.0, 0.045);
    wauvio::Buffer res_buf = resonance.render(DUR, SR);

    // Modulate resonance amplitude very slowly (16 s period)
    auto lfo_res = ambience_detail::make_lfo_mod(DUR, 1.0 / 16.0, 1.2, SR);
    ambience_detail::apply_mod(res_buf, lfo_res, 0.5f, 0.5f);

    // ---- Layer 5: sparse exterior drips (5 drips scattered across 30 s)
    const size_t drip_offsets[] = {
        static_cast<size_t>(3.1  * SR),
        static_cast<size_t>(9.7  * SR),
        static_cast<size_t>(14.3 * SR),
        static_cast<size_t>(21.8 * SR),
        static_cast<size_t>(27.5 * SR),
    };
    const double drip_freqs[]  = { 1340.0, 1580.0, 1210.0, 1450.0, 1680.0 };
    const double drip_decays[] = {  0.06,   0.05,   0.07,   0.05,   0.06  };
    const float  drip_amps[]   = {  0.18f,  0.14f,  0.20f,  0.16f,  0.13f };

    wauvio::Buffer drips(static_cast<size_t>(DUR * SR), 0.f);
    for (int d = 0; d < 5; ++d) {
        auto drip = ambience_detail::make_drip(drip_freqs[d], drip_decays[d],
                                               drip_amps[d], SR);
        ambience_detail::mix_at(drips, drip, drip_offsets[d]);
    }

    // Lightly reverb the drips
    wauvio::Reverb drip_verb;
    drip_verb.room_size = 0.45f;
    drip_verb.damping   = 0.60f;
    drip_verb.wet       = 0.35f;
    drip_verb.dry       = 1.0f;
    drip_verb.init(SR);
    drip_verb.process(drips);

    // ---- Mix all layers into master
    const size_t total = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(total, 0.f);
    wauvio::mix_into(out, wind_low);
    wauvio::mix_into(out, wind_mid);
    wauvio::mix_into(out, wind_hi);
    wauvio::mix_into(out, res_buf);
    wauvio::mix_into(out, drips);

    // ---- Loop prep
    const size_t xfade = static_cast<size_t>(XFADE_MS * SR / 1000);
    wauvio::fade_in(out,  FADE_S, SR);
    wauvio::fade_out(out, FADE_S, SR);
    ambience_detail::crossfade_loop(out, xfade);

    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.35f);
    return out;
}

} // namespace surface


// =============================================================================
//  ambience::cave
//  Scene: deep cave passage — Sequence1 and early Exploration.
//  Character: resonant hum, dripping water, distant hollow air movement.
//  Duration: 40 s
// =============================================================================

namespace cave {

inline wauvio::Buffer gen() {
    const int    SR  = wauvio::global_config().sample_rate;
    constexpr double DUR       = 40.0;
    constexpr double FADE_S    = 0.08;
    constexpr size_t XFADE_MS  = 80;

    // ---- Layer 1: sub-bass stone rumble — noise, HPF @ 18 Hz, LPF @ 80 Hz
    wauvio::Buffer sub = ambience_detail::band_noise(DUR, 0x11223344u,
                                                     18.0, 80.0, 0.65f, SR);
    auto lfo_sub = ambience_detail::make_lfo_mod(DUR, 1.0 / 12.0, 0.5, SR);
    ambience_detail::apply_mod(sub, lfo_sub, 0.75f, 0.25f);

    // ---- Layer 2: mid cave breath — HPF @ 90 Hz, LPF @ 350 Hz
    wauvio::Buffer breath = ambience_detail::band_noise(DUR, 0x55667788u,
                                                        90.0, 350.0, 0.40f, SR);
    auto lfo_breath = ambience_detail::make_lfo_mod(DUR, 1.0 / 18.0, wauvio::PI * 0.7, SR);
    ambience_detail::apply_mod(breath, lfo_breath, 0.65f, 0.35f);

    // ---- Layer 3: tonal cave resonance — fundamental + two harmonics
    //      Cave roughly resonates near 73 Hz (A1-ish) and its odd harmonics
    struct { double freq; double amp; } resonances[] = {
        { 73.0,  0.055 },
        { 146.0, 0.030 },
        { 219.0, 0.018 },
    };
    wauvio::Buffer tones(static_cast<size_t>(DUR * SR), 0.f);
    for (auto& r : resonances) {
        wauvio::Oscillator osc(wauvio::WaveShape::Sine, r.freq, r.amp);
        wauvio::Buffer t = osc.render(DUR, SR);
        // Each harmonic breathes at a slightly different LFO rate
        auto lfo_tone = ambience_detail::make_lfo_mod(DUR,
            1.0 / (11.0 + r.freq * 0.01), r.freq * 0.003, SR);
        ambience_detail::apply_mod(t, lfo_tone, 0.5f, 0.5f);
        wauvio::mix_into(tones, t);
    }

    // Apply light reverb to the tonal layer to smear resonances
    wauvio::Reverb tone_verb;
    tone_verb.room_size = 0.80f;
    tone_verb.damping   = 0.30f;
    tone_verb.wet       = 0.55f;
    tone_verb.dry       = 0.80f;
    tone_verb.init(SR);
    tone_verb.process(tones);

    // ---- Layer 4: water drips — 8 drips, heavier than surface
    const size_t drip_offsets[] = {
        static_cast<size_t>(1.2  * SR),
        static_cast<size_t>(4.8  * SR),
        static_cast<size_t>(7.3  * SR),
        static_cast<size_t>(11.1 * SR),
        static_cast<size_t>(16.9 * SR),
        static_cast<size_t>(22.4 * SR),
        static_cast<size_t>(28.7 * SR),
        static_cast<size_t>(35.1 * SR),
    };
    const double drip_freqs[]  = { 980.0, 1120.0,  860.0, 1050.0,
                                   940.0, 1200.0, 1010.0,  890.0 };
    const double drip_decays[] = { 0.09,  0.08,   0.11,   0.08,
                                   0.10,  0.07,   0.09,   0.12  };
    const float  drip_amps[]   = { 0.28f, 0.24f,  0.32f,  0.22f,
                                   0.30f, 0.20f,  0.26f,  0.35f };

    wauvio::Buffer drips(static_cast<size_t>(DUR * SR), 0.f);
    for (int d = 0; d < 8; ++d) {
        auto drip = ambience_detail::make_drip(drip_freqs[d], drip_decays[d],
                                               drip_amps[d], SR);
        ambience_detail::mix_at(drips, drip, drip_offsets[d]);
    }

    // Cave reverb on drips — larger room
    wauvio::Reverb drip_verb;
    drip_verb.room_size = 0.72f;
    drip_verb.damping   = 0.40f;
    drip_verb.wet       = 0.60f;
    drip_verb.dry       = 1.0f;
    drip_verb.init(SR);
    drip_verb.process(drips);

    // ---- Layer 5: distant hollow whoosh — bandpass noise, very low amplitude
    //      HPF @ 300 Hz, LPF @ 700 Hz, slow swell
    wauvio::Buffer whoosh = ambience_detail::band_noise(DUR, 0xAABBCCDDu,
                                                        300.0, 700.0, 0.14f, SR);
    auto lfo_whoosh = ambience_detail::make_lfo_mod(DUR, 1.0 / 22.0, wauvio::PI * 1.3, SR);
    ambience_detail::apply_mod(whoosh, lfo_whoosh, 0.4f, 0.6f);

    // ---- Mix
    const size_t total = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(total, 0.f);
    wauvio::mix_into(out, sub);
    wauvio::mix_into(out, breath);
    wauvio::mix_into(out, tones);
    wauvio::mix_into(out, drips);
    wauvio::mix_into(out, whoosh);

    // ---- Loop prep
    const size_t xfade = static_cast<size_t>(XFADE_MS * SR / 1000);
    wauvio::fade_in(out,  FADE_S, SR);
    wauvio::fade_out(out, FADE_S, SR);
    ambience_detail::crossfade_loop(out, xfade);

    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.35f);
    return out;
}

} // namespace cave


// =============================================================================
//  ambience::deep
//  Scene: far below the cave — Sequence2, late Exploration.
//  Character: oppressive sub pressure, tonal drones, near-silence broken
//             by low structural groans. Less water, more earth.
//  Duration: 50 s
// =============================================================================

namespace deep {

inline wauvio::Buffer gen() {
    const int    SR  = wauvio::global_config().sample_rate;
    constexpr double DUR       = 50.0;
    constexpr double FADE_S    = 0.10;
    constexpr size_t XFADE_MS  = 100;

    // ---- Layer 1: infrasonic pressure — noise shelf below 60 Hz
    wauvio::Buffer pressure = ambience_detail::band_noise(DUR, 0xDEADBEEFu,
                                                          10.0, 60.0, 0.80f, SR);
    auto lfo_pressure = ambience_detail::make_lfo_mod(DUR, 1.0 / 28.0, 0.0, SR);
    ambience_detail::apply_mod(pressure, lfo_pressure, 0.80f, 0.20f);

    // ---- Layer 2: stone texture — mid-low noise, HPF @ 60 Hz, LPF @ 200 Hz
    wauvio::Buffer stone = ambience_detail::band_noise(DUR, 0xCAFEBABEu,
                                                       60.0, 200.0, 0.35f, SR);
    auto lfo_stone = ambience_detail::make_lfo_mod(DUR, 1.0 / 15.0, wauvio::PI * 0.4, SR);
    ambience_detail::apply_mod(stone, lfo_stone, 0.70f, 0.30f);

    // ---- Layer 3: tonal drone cluster — three detuned sines, very slow beating
    //      Root around 55 Hz (A1), slight detuning creates a beating drone
    struct { double freq; double amp; double lfo_rate; double lfo_phase; } drones[] = {
        { 54.8,  0.070, 1.0 / 31.0, 0.00 },
        { 55.2,  0.065, 1.0 / 27.0, 1.10 },
        { 82.3,  0.038, 1.0 / 19.0, 2.30 },  // fifth harmonic, quiet
        { 110.0, 0.025, 1.0 / 23.0, 0.70 },  // octave, very quiet
    };
    wauvio::Buffer drone_buf(static_cast<size_t>(DUR * SR), 0.f);
    for (auto& d : drones) {
        wauvio::Oscillator osc(wauvio::WaveShape::Sine, d.freq, d.amp);
        wauvio::Buffer t = osc.render(DUR, SR);
        auto lfo = ambience_detail::make_lfo_mod(DUR, d.lfo_rate, d.lfo_phase, SR);
        ambience_detail::apply_mod(t, lfo, 0.45f, 0.55f);
        wauvio::mix_into(drone_buf, t);
    }

    // Heavy reverb on the drone to fill the space
    wauvio::Reverb drone_verb;
    drone_verb.room_size = 0.92f;
    drone_verb.damping   = 0.15f;
    drone_verb.wet       = 0.65f;
    drone_verb.dry       = 0.90f;
    drone_verb.init(SR);
    drone_verb.process(drone_buf);

    // ---- Layer 4: structural groans — 3 events, low-mid FM sweeps
    //      Each groan: a short FM burst that rises then falls in pitch
    auto make_groan = [&](double onset_s, double carrier, double mod_ratio,
                          double mod_idx, double dur_s, float amp) -> wauvio::Buffer {
        const size_t n   = static_cast<size_t>(dur_s * SR);
        wauvio::Buffer g(n, 0.f);
        const double c_inc = wauvio::TWO_PI * carrier / SR;
        const double m_inc = wauvio::TWO_PI * (carrier * mod_ratio) / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < n; ++i) {
            // Envelope: triangle — rise to peak then decay
            const float t_n  = static_cast<float>(i) / static_cast<float>(n);
            const float env  = (t_n < 0.4f)
                ? t_n / 0.4f
                : 1.f - (t_n - 0.4f) / 0.6f;
            g[i] = static_cast<float>(std::sin(cp + mod_idx * std::sin(mp)))
                   * env * amp;
            cp += c_inc;
            mp += m_inc;
        }
        return g;
    };

    wauvio::Buffer groans(static_cast<size_t>(DUR * SR), 0.f);
    struct { double onset; double carrier; double mod_r; double mod_i; double dur; float amp; } groan_params[] = {
        {  5.5,  68.0, 1.5, 1.8, 3.2, 0.20f },
        { 19.2,  52.0, 2.0, 2.4, 4.0, 0.25f },
        { 36.8,  75.0, 1.3, 1.5, 2.8, 0.18f },
    };
    for (auto& gp : groan_params) {
        wauvio::Buffer g = make_groan(gp.onset, gp.carrier, gp.mod_r,
                                      gp.mod_i, gp.dur, gp.amp);
        const size_t off = static_cast<size_t>(gp.onset * SR);
        ambience_detail::mix_at(groans, g, off);
    }

    // Reverb + LP on groans to bury them in distance
    wauvio::Reverb groan_verb;
    groan_verb.room_size = 0.85f;
    groan_verb.damping   = 0.50f;
    groan_verb.wet       = 0.70f;
    groan_verb.dry       = 0.80f;
    groan_verb.init(SR);
    groan_verb.process(groans);

    wauvio::LowPassFilter groan_lp(350.0, SR);
    groan_lp.process(groans);

    // ---- Layer 5: very sparse single drips (3 total — deep and slow)
    const size_t deep_drip_offsets[] = {
        static_cast<size_t>(8.4  * SR),
        static_cast<size_t>(24.7 * SR),
        static_cast<size_t>(43.1 * SR),
    };
    const double deep_drip_freqs[]  = { 620.0, 580.0, 640.0 };
    const double deep_drip_decays[] = { 0.18,  0.22,  0.16  };
    const float  deep_drip_amps[]   = { 0.30f, 0.26f, 0.28f };

    wauvio::Buffer deep_drips(static_cast<size_t>(DUR * SR), 0.f);
    for (int d = 0; d < 3; ++d) {
        auto drip = ambience_detail::make_drip(deep_drip_freqs[d],
                                               deep_drip_decays[d],
                                               deep_drip_amps[d], SR);
        ambience_detail::mix_at(deep_drips, drip, deep_drip_offsets[d]);
    }
    wauvio::Reverb deep_drip_verb;
    deep_drip_verb.room_size = 0.88f;
    deep_drip_verb.damping   = 0.25f;
    deep_drip_verb.wet       = 0.75f;
    deep_drip_verb.dry       = 1.0f;
    deep_drip_verb.init(SR);
    deep_drip_verb.process(deep_drips);

    // ---- Mix
    const size_t total = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(total, 0.f);
    wauvio::mix_into(out, pressure);
    wauvio::mix_into(out, stone);
    wauvio::mix_into(out, drone_buf);
    wauvio::mix_into(out, groans);
    wauvio::mix_into(out, deep_drips);

    // ---- Loop prep
    const size_t xfade = static_cast<size_t>(XFADE_MS * SR / 1000);
    wauvio::fade_in(out,  FADE_S, SR);
    wauvio::fade_out(out, FADE_S, SR);
    ambience_detail::crossfade_loop(out, xfade);

    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.35f);
    return out;
}

} // namespace deep


// =============================================================================
//  ambience::eerie
//  Scene: caveDistort = true (Sequence1 deterioration, Shining approach).
//  Character: the cave sound warped — shifted resonances, ring-mod shimmer,
//             pitch-unstable drones, subliminal rhythmic pulse.
//  Duration: 35 s
// =============================================================================

namespace eerie {

inline wauvio::Buffer gen() {
    const int    SR  = wauvio::global_config().sample_rate;
    constexpr double DUR       = 35.0;
    constexpr double FADE_S    = 0.08;
    constexpr size_t XFADE_MS  = 80;

    // ---- Layer 1: cave base noise warped — HPF @ 30 Hz, LPF @ 160 Hz
    wauvio::Buffer base = ambience_detail::band_noise(DUR, 0x13572468u,
                                                      30.0, 160.0, 0.60f, SR);
    // Amplitude tremolo — fast-ish (3.7 Hz) to feel unstable
    auto lfo_trem = ambience_detail::make_lfo_mod(DUR, 3.7, 0.0, SR);
    ambience_detail::apply_mod(base, lfo_trem, 0.65f, 0.35f);

    // ---- Layer 2: ring-modulated mid noise
    //      Mid noise × carrier sine = ring mod creates sideband shimmer
    wauvio::Buffer mid_noise = ambience_detail::band_noise(DUR, 0x86420ACEu,
                                                           150.0, 800.0, 0.45f, SR);
    {
        // Ring mod carrier — slightly detuned from integer frequency for unease
        const double ring_freq = 213.7;
        const double ring_inc  = wauvio::TWO_PI * ring_freq / SR;
        double ring_phase = 0.0;
        const size_t n = mid_noise.size();
        for (size_t i = 0; i < n; ++i) {
            mid_noise[i] *= static_cast<float>(std::sin(ring_phase));
            ring_phase += ring_inc;
        }
    }
    wauvio::LowPassFilter ring_lp(1200.0, SR);
    ring_lp.process(mid_noise);

    // ---- Layer 3: pitch-unstable drone — FM with slowly drifting modulator
    //      Modulator drifts via a second LFO — never settles
    {
        // LFO controlling modulator index: 0.4 → 2.8 Hz, period ~14 s
    }
    wauvio::Buffer drone(static_cast<size_t>(DUR * SR), 0.f);
    {
        const double carrier_freq = 89.0;
        const double mod_freq_base = 133.5;
        const double c_inc = wauvio::TWO_PI * carrier_freq / SR;
        // Modulator freq drifts slowly
        const double lfo_mod_rate = 1.0 / 14.0;
        const double lfo_mod_inc  = wauvio::TWO_PI * lfo_mod_rate / SR;
        double cp = 0.0, lfo_mod_phase = 0.0;
        const size_t n = drone.size();
        for (size_t i = 0; i < n; ++i) {
            const double lfo_val    = 0.5 + 0.5 * std::sin(lfo_mod_phase);
            const double mod_idx    = 0.4 + lfo_val * 2.4;
            const double mod_freq   = mod_freq_base + lfo_val * 18.0;
            const double m_inc      = wauvio::TWO_PI * mod_freq / SR;
            // Single-sample FM (modulator phase inline — lightweight)
            const double mp = static_cast<double>(i) * m_inc;
            drone[i] = static_cast<float>(
                0.068 * std::sin(cp + mod_idx * std::sin(mp)));
            cp += c_inc;
            lfo_mod_phase += lfo_mod_inc;
        }
    }

    // Amplitude envelope on drone: swell in/out with 8 s period
    auto lfo_drone_amp = ambience_detail::make_lfo_mod(DUR, 1.0 / 8.0, wauvio::PI * 0.6, SR);
    ambience_detail::apply_mod(drone, lfo_drone_amp, 0.4f, 0.6f);

    wauvio::Reverb drone_verb;
    drone_verb.room_size = 0.75f;
    drone_verb.damping   = 0.20f;
    drone_verb.wet       = 0.50f;
    drone_verb.dry       = 0.90f;
    drone_verb.init(SR);
    drone_verb.process(drone);

    // ---- Layer 4: subliminal rhythmic pulse — bandpass noise at ~2.8 Hz
    //      Sounds like a distant heartbeat but tonally ambiguous
    wauvio::Buffer pulse_noise = ambience_detail::band_noise(DUR, 0x7B3F9E2Du,
                                                             60.0, 180.0, 0.25f, SR);
    auto lfo_pulse = ambience_detail::make_lfo_mod(DUR, 2.8, wauvio::PI * 0.2, SR);
    // Hard-shape the LFO to get a pulsed rather than smooth envelope
    const size_t pn = pulse_noise.size();
    const size_t lm = lfo_pulse.size();
    for (size_t i = 0; i < std::min(pn, lm); ++i) {
        // Raise to power 4 to make pulses narrow and punchy
        const float v = lfo_pulse[i];
        pulse_noise[i] *= v * v * v * v;
    }

    // ---- Layer 5: high shimmer — very quiet, HPF @ 3000 Hz, LPF @ 9000 Hz
    //      Sounds like distant metallic resonance
    wauvio::Buffer shimmer = ambience_detail::band_noise(DUR, 0x2F4A8B6Cu,
                                                         3000.0, 9000.0, 0.07f, SR);
    auto lfo_shim = ambience_detail::make_lfo_mod(DUR, 1.0 / 5.5, wauvio::PI * 1.7, SR);
    ambience_detail::apply_mod(shimmer, lfo_shim, 0.40f, 0.60f);

    // ---- Mix
    const size_t total = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(total, 0.f);
    wauvio::mix_into(out, base);
    wauvio::mix_into(out, mid_noise);
    wauvio::mix_into(out, drone);
    wauvio::mix_into(out, pulse_noise);
    wauvio::mix_into(out, shimmer);

    // ---- Loop prep
    const size_t xfade = static_cast<size_t>(XFADE_MS * SR / 1000);
    wauvio::fade_in(out,  FADE_S, SR);
    wauvio::fade_out(out, FADE_S, SR);
    ambience_detail::crossfade_loop(out, xfade);

    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.35f);
    return out;
}

} // namespace eerie


// =============================================================================
//  ambience::static_
//  Scene: fracture moment / white noise intrusion (Sequence1 endgame).
//  Character: raw white noise shaped to feel like dying signal — lo-fi,
//             bandwidth-limited, with aliasing artefacts baked in.
//             Occasional "carrier" tone barely visible beneath the noise.
//  Duration: 20 s  (shortest loop — high intensity, designed to loop fast)
// =============================================================================

namespace static_ {

inline wauvio::Buffer gen() {
    const int    SR  = wauvio::global_config().sample_rate;
    constexpr double DUR       = 20.0;
    constexpr double FADE_S    = 0.05;
    constexpr size_t XFADE_MS  = 50;

    // ---- Layer 1: wideband noise base — the signal breaking up
    wauvio::Buffer wide = ambience_detail::band_noise(DUR, 0xFFEEDDCCu,
                                                      80.0, 8000.0, 0.70f, SR);

    // Rapid amplitude stuttering — fast irregular LFO at ~22 Hz
    // (Below hearing threshold as pitch but audible as tremolo flutter)
    auto lfo_flutter = ambience_detail::make_lfo_mod(DUR, 22.3, 0.0, SR);
    const size_t wn = wide.size();
    const size_t fn = lfo_flutter.size();
    for (size_t i = 0; i < std::min(wn, fn); ++i) {
        // Clip the LFO to create irregular on/off stuttering
        const float v   = lfo_flutter[i];
        const float gate = (v > 0.35f) ? 1.0f : 0.2f;
        wide[i] *= gate;
    }

    // ---- Layer 2: lo-fi crackle — very high-frequency noise then crushed
    //      Simulates digital signal degradation
    wauvio::Buffer crackle = ambience_detail::band_noise(DUR, 0x01234567u,
                                                         5000.0, 12000.0, 0.30f, SR);
    // Hard clip to add crunch
    wauvio::distortion::apply_hard_clip(crackle, 0.3f);
    // Then LP to smear the harmonics back down
    wauvio::LowPassFilter crackle_lp(3500.0, SR);
    crackle_lp.process(crackle);

    // Amplitude gating — crackle appears in bursts
    auto lfo_crackle = ambience_detail::make_lfo_mod(DUR, 7.1, wauvio::PI, SR);
    const size_t cn = crackle.size();
    const size_t lgn = lfo_crackle.size();
    for (size_t i = 0; i < std::min(cn, lgn); ++i) {
        crackle[i] *= (lfo_crackle[i] > 0.55f) ? lfo_crackle[i] : 0.0f;
    }

    // ---- Layer 3: ghost carrier — barely audible sine at ~440 Hz
    //      Fades in and out suggesting a signal trying to come through
    wauvio::Oscillator carrier(wauvio::WaveShape::Sine, 440.0, 0.04);
    wauvio::Buffer carrier_buf = carrier.render(DUR, SR);
    auto lfo_carrier = ambience_detail::make_lfo_mod(DUR, 1.0 / 6.3, wauvio::PI * 0.9, SR);
    // Use squared LFO so carrier only emerges near the peak
    const size_t cblen = carrier_buf.size();
    const size_t cllen = lfo_carrier.size();
    for (size_t i = 0; i < std::min(cblen, cllen); ++i) {
        const float v = lfo_carrier[i];
        carrier_buf[i] *= v * v;
    }

    // ---- Layer 4: sub thud — low end thumping, like a signal hammer
    wauvio::Buffer sub = ambience_detail::band_noise(DUR, 0xABCDEF01u,
                                                     20.0, 55.0, 0.50f, SR);
    auto lfo_sub = ambience_detail::make_lfo_mod(DUR, 1.4, wauvio::PI * 0.3, SR);
    const size_t sbn = sub.size();
    const size_t sln = lfo_sub.size();
    for (size_t i = 0; i < std::min(sbn, sln); ++i) {
        const float v = lfo_sub[i];
        sub[i] *= v * v * v; // cubic shaping → tight thumps
    }

    // ---- Mix
    const size_t total = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(total, 0.f);
    wauvio::mix_into(out, wide,        1.00f);
    wauvio::mix_into(out, crackle,     0.55f);
    wauvio::mix_into(out, carrier_buf, 1.00f);
    wauvio::mix_into(out, sub,         0.80f);

    // ---- Loop prep
    const size_t xfade = static_cast<size_t>(XFADE_MS * SR / 1000);
    wauvio::fade_in(out,  FADE_S, SR);
    wauvio::fade_out(out, FADE_S, SR);
    ambience_detail::crossfade_loop(out, xfade);

    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.35f);
    return out;
}

} // namespace static_

} // namespace ambience
