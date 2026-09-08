// =============================================================================
//  W A U V I O _ S O U N D T R A C K _ C A V E . H P P
//
//  Standalone procedural soundtrack module.
//  Depends on wauvio.hpp ONLY — no other Wauvio modules required.
//
//  Theme: A dark cave lit by a single, dying torch.
//         The flame flickers unpredictably. Drips echo in the void.
//         Something moves, far below.
//
//  Namespace: wauvio::soundtrack::cave
//  Entry point:
//      wauvio::Buffer wauvio::soundtrack::cave::generate(int sample_rate = 0);
//
//  Audio characteristics:
//    • Deep amber drones (slowly beating low-frequency oscillators)
//    • Torch flicker (amplitude-modulated high-frequency noise bursts)
//    • Distant echo tones (spaced, decaying sine pings)
//    • Cave wind (shaped, filtered noise)
//    • Subtle dissonance via slight detuning between layers
//    • No fixed rhythm — timing is irregular and organic
//    • Total duration: ~90–100 seconds
//
//  Structure:
//    [0:00–0:12]  Intro fade-in — deep drone swells from silence
//    [0:12–0:45]  Ambient body — all layers active, torch begins flickering
//    [0:45–1:10]  Development — modulation deepens, echo tones increase
//    [1:10–1:35]  Tension peak — resonant dissonance, irregular noise bursts
//    [1:35–1:45]  Fade-out — all layers thin and decay to silence
// =============================================================================

#pragma once
#include "wauvio.hpp"

namespace wauvio::soundtrack::cave {

// =============================================================================
//  INTERNAL HELPERS (private to this module via anonymous detail namespace)
// =============================================================================

namespace detail {

// ---------------------------------------------------------------------------
//  Deterministic pseudo-random float in [lo, hi] from a running seed.
//  Used for reproducible irregular timings without std::random overhead.
// ---------------------------------------------------------------------------
struct PRNG {
    uint32_t state;
    explicit PRNG(uint32_t seed = 0xDEADBEEFu) : state(seed) {}

    uint32_t next() {
        // xorshift32
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /// Float in [lo, hi]
    double randf(double lo, double hi) {
        const double t = static_cast<double>(next()) / 4294967295.0;
        return lo + t * (hi - lo);
    }

    /// Int in [lo, hi] inclusive
    int randi(int lo, int hi) {
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }
};

// ---------------------------------------------------------------------------
//  Deep ambient drone layer
//  Two slightly detuned sawtooth oscillators passed through a very
//  narrow low-pass filter, creating a warm, rumbling sub-drone.
//  A slow LFO modulates the amplitude to produce a breathing quality.
//
//  Parameters:
//    root_hz    – root frequency of the drone (e.g. 55 Hz = A1)
//    detune_hz  – frequency offset of second oscillator (default 0.7 Hz)
//    duration   – total length in seconds
// ---------------------------------------------------------------------------
inline Buffer drone(double root_hz, double detune_hz,
                    double duration, int sample_rate)
{
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    // Oscillator A: root
    double phA = 0.0;
    const double incA = TWO_PI * root_hz / sample_rate;

    // Oscillator B: slightly detuned for beating
    double phB = 0.0;
    const double incB = TWO_PI * (root_hz + detune_hz) / sample_rate;

    // LFO: very slow amplitude modulation (0.07 Hz cycle ≈ 14 s period)
    double phLFO = 0.0;
    const double incLFO = TWO_PI * 0.07 / sample_rate;

    LowPassFilter lpf(220.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const float sa  = wave::sawtooth(phA) * 0.55f;
        const float sb  = wave::sawtooth(phB) * 0.45f;
        const float lfo = 0.65f + 0.35f * static_cast<float>(std::sin(phLFO));
        buf[i] = lpf.tick(sa + sb) * lfo;

        phA   += incA;   if (phA   >= TWO_PI) phA   -= TWO_PI;
        phB   += incB;   if (phB   >= TWO_PI) phB   -= TWO_PI;
        phLFO += incLFO; if (phLFO >= TWO_PI) phLFO -= TWO_PI;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  Torch flicker layer
//  Band-limited white noise whose amplitude is controlled by a jagged,
//  irregular LFO — simulating the unpredictable nature of firelight.
//  Runs at a higher frequency band (1 kHz–4 kHz) to evoke warm crackling.
//
//  The flicker LFO is synthesised from two misaligned sine oscillators
//  at incommensurable ratios, creating aperiodic variation.
// ---------------------------------------------------------------------------
inline Buffer torch_flicker(double duration, int sample_rate, uint32_t seed) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    NoiseGenerator noise(seed);
    LowPassFilter  lpf_hi(4200.0, sample_rate);
    HighPassFilter hpf_lo(900.0,  sample_rate);

    // Two LFOs at irrational ratio to avoid periodicity
    double ph1 = 0.0, ph2 = 0.0;
    const double inc1 = TWO_PI * 1.27  / sample_rate;  // ~1.27 Hz
    const double inc2 = TWO_PI * 3.141 / sample_rate;  // ~π Hz

    for (size_t i = 0; i < n; ++i) {
        // Combine LFOs: range [0, 1], always positive
        const double raw_lfo =
            0.5 * (std::sin(ph1) + 1.0) * 0.6
          + 0.5 * (std::sin(ph2) + 1.0) * 0.4;
        // Apply a power curve to make dim moments more frequent (torch fades)
        const float flicker = static_cast<float>(std::pow(raw_lfo, 2.2));

        const float raw = noise.tick();
        const float filtered = hpf_lo.tick(lpf_hi.tick(raw));
        buf[i] = filtered * flicker * 0.18f;

        ph1 += inc1; if (ph1 >= TWO_PI) ph1 -= TWO_PI;
        ph2 += inc2; if (ph2 >= TWO_PI) ph2 -= TWO_PI;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  Cave wind / air movement layer
//  Very low-pass filtered noise with slow amplitude envelope.
//  Gives the impression of still, damp air in an enclosed space.
// ---------------------------------------------------------------------------
inline Buffer cave_wind(double duration, int sample_rate, uint32_t seed) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    NoiseGenerator noise(seed);
    LowPassFilter  lpf1(180.0, sample_rate);
    LowPassFilter  lpf2(180.0, sample_rate);  // Two-pass for steeper roll-off

    // Slow modulation envelope (quasi-breathing, ~0.04 Hz)
    double phMod = 0.0;
    const double incMod = TWO_PI * 0.04 / sample_rate;

    for (size_t i = 0; i < n; ++i) {
        const float raw  = noise.tick();
        const float filt = lpf2.tick(lpf1.tick(raw));
        const float mod  = 0.5f + 0.5f * static_cast<float>(std::sin(phMod));
        buf[i] = filt * mod * 0.22f;

        phMod += incMod;
        if (phMod >= TWO_PI) phMod -= TWO_PI;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  Single echo-tone event
//  A short sine ping with long exponential decay — placed at irregular
//  intervals to simulate reflections off cave walls.
//
//  Technique: FM with low mod_index gives a slightly metallic timbre that
//  better resembles a stone resonance than a pure sine.
// ---------------------------------------------------------------------------
inline Buffer echo_tone(double freq_hz, double duration,
                        float peak_amp, int sample_rate)
{
    FMSynth fm;
    fm.carrier_freq   = freq_hz;
    fm.modulator_freq = freq_hz * 1.41;  // tritone ratio — subtly dissonant
    fm.mod_index      = 0.35;
    fm.amplitude      = 1.0;

    Buffer buf = fm.render(duration, sample_rate);

    // Exponential amplitude decay (not ADSR — we want continuous decay)
    for (size_t i = 0; i < buf.size(); ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        const float  env = static_cast<float>(peak_amp * std::exp(-t * 3.5));
        buf[i] *= env;
    }

    // Light low-pass to reduce harshness
    LowPassFilter lpf(2200.0, sample_rate);
    lpf.process(buf);
    return buf;
}

// ---------------------------------------------------------------------------
//  Drip transient
//  A very brief sine chirp with rapid exponential decay — like a water
//  droplet hitting a still pool in total darkness.
// ---------------------------------------------------------------------------
inline Buffer drip(double freq_hz, int sample_rate) {
    const double dur = 0.22;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t    = static_cast<double>(i) / sample_rate;
        // Frequency falls quickly (like a pitched drop hitting water)
        const double freq = freq_hz * std::exp(-t * 12.0) + freq_hz * 0.3;
        phase += TWO_PI * freq / sample_rate;
        const float env = static_cast<float>(std::exp(-t * 22.0));
        buf[i] = static_cast<float>(std::sin(phase)) * env * 0.55f;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  Dissonant pad  — two FM voices separated by a narrow, beating interval
//  Creates the sense of something vast and unresolved in the dark.
// ---------------------------------------------------------------------------
inline Buffer dissonant_pad(double root_hz, double duration, int sample_rate) {
    Buffer buf(static_cast<size_t>(duration * sample_rate), 0.0f);

    // Voice 1: root + minor 2nd (semitone above) with slow FM
    FMSynth fm1;
    fm1.carrier_freq   = root_hz;
    fm1.modulator_freq = root_hz * 0.5;
    fm1.mod_index      = 1.1;
    fm1.amplitude      = 0.35;
    mix_into(buf, fm1.render(duration, sample_rate));

    // Voice 2: root × 2^(1/12) — one semitone up, detuned slightly more
    FMSynth fm2;
    fm2.carrier_freq   = root_hz * std::pow(2.0, 1.0 / 12.0) + 0.4; // tiny extra detune
    fm2.modulator_freq = fm2.carrier_freq * 0.75;
    fm2.mod_index      = 0.9;
    fm2.amplitude      = 0.28;
    mix_into(buf, fm2.render(duration, sample_rate));

    // Heavy low-pass smoothing
    LowPassFilter lpf(600.0, sample_rate);
    lpf.process(buf);
    return buf;
}

} // namespace detail

// =============================================================================
//  PUBLIC ENTRY POINT
//
//  generate()  —  Produce the full cave ambient soundtrack.
//  Returns a normalised float Buffer ready to pass to wauvio::play().
// =============================================================================

inline Buffer generate(int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    // ─────────────────────────────────────────────────────────────────────
    //  Timing constants
    //  Total target: ~95 seconds
    // ─────────────────────────────────────────────────────────────────────
    constexpr double INTRO_DUR    = 12.0;   // [00:00 – 00:12]
    constexpr double BODY_DUR     = 33.0;   // [00:12 – 00:45]
    constexpr double DEVEL_DUR    = 25.0;   // [00:45 – 01:10]
    constexpr double TENSION_DUR  = 25.0;   // [01:10 – 01:35]
    constexpr double FADEOUT_DUR  = 10.0;   // [01:35 – 01:45]
    constexpr double TOTAL        = INTRO_DUR + BODY_DUR + DEVEL_DUR
                                  + TENSION_DUR + FADEOUT_DUR;

    MasterBus bus(sample_rate);

    // Pseudo-random source for all irregular timings
    detail::PRNG rng(0xCAFEBABEu);

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 1 — Deep drone (runs the entire piece, fades in/out)
    //  Root: A1 = 55 Hz.  Two drones with different detuning amounts.
    // ─────────────────────────────────────────────────────────────────────
    {
        // Primary drone: A1, slow 0.6 Hz beat
        Buffer d1 = detail::drone(55.0, 0.6, TOTAL, sample_rate);
        fade_in (d1, INTRO_DUR * 0.8, sample_rate);
        fade_out(d1, FADEOUT_DUR,     sample_rate);
        apply_gain(d1, 0.72f);
        bus.schedule(d1, 0.0);
    }
    {
        // Second drone: E1 (a fifth below A1 ≈ 36.7 Hz), very slow beat
        Buffer d2 = detail::drone(36.7, 0.25, TOTAL, sample_rate);
        fade_in (d2, INTRO_DUR * 1.2, sample_rate);
        fade_out(d2, FADEOUT_DUR,     sample_rate);
        apply_gain(d2, 0.48f);
        bus.schedule(d2, 0.0);
    }
    {
        // A very slow FM sub-tone that enters mid-piece
        double sub_start = INTRO_DUR + BODY_DUR * 0.4;
        double sub_dur   = TOTAL - sub_start - FADEOUT_DUR;
        FMSynth fmSub;
        fmSub.carrier_freq   = 27.5;  // A0 — infrasonic rumble zone
        fmSub.modulator_freq = 27.5 * 1.003;
        fmSub.mod_index      = 0.6;
        fmSub.amplitude      = 0.55;
        Buffer subBuf = fmSub.render(sub_dur, sample_rate);
        LowPassFilter lpfSub(80.0, sample_rate);
        lpfSub.process(subBuf);
        fade_in(subBuf, 6.0, sample_rate);
        bus.schedule(subBuf, sub_start, 0.6f);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 2 — Cave wind (continuous, very quiet)
    // ─────────────────────────────────────────────────────────────────────
    {
        Buffer wind = detail::cave_wind(TOTAL, sample_rate, 0xABCD1234u);
        fade_in (wind, INTRO_DUR * 0.5, sample_rate);
        fade_out(wind, FADEOUT_DUR,     sample_rate);
        bus.schedule(wind, 0.0, 0.55f);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 3 — Torch flicker (starts at end of intro, intensifies mid-piece)
    //  Three overlapping flicker passes with different seeds and gains
    //  create the sensation of an unpredictably dancing flame.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double flicker_start = INTRO_DUR * 0.7;
        const double flicker_dur   = TOTAL - flicker_start - FADEOUT_DUR * 0.5;

        Buffer f1 = detail::torch_flicker(flicker_dur, sample_rate, 0x11223344u);
        fade_in(f1, 4.0, sample_rate);
        bus.schedule(f1, flicker_start, 0.85f);

        // A second, offset flicker with different timbre seed
        Buffer f2 = detail::torch_flicker(
            flicker_dur * 0.9, sample_rate, 0x55667788u);
        fade_in(f2, 6.0, sample_rate);
        bus.schedule(f2, flicker_start + 2.3, 0.45f);
    }
    // Extra torch sputters in the tension section
    {
        const double t0   = INTRO_DUR + BODY_DUR + DEVEL_DUR;
        Buffer sputter = detail::torch_flicker(TENSION_DUR, sample_rate, 0xFEDCBA98u);
        // Invert the gain envelope by a slow LFO to suggest near-extinction moments
        for (size_t i = 0; i < sputter.size(); ++i) {
            const double t   = static_cast<double>(i) / sample_rate;
            const double mod = 0.4 + 0.6 * std::abs(std::sin(TWO_PI * 0.11 * t));
            sputter[i] *= static_cast<float>(mod);
        }
        bus.schedule(sputter, t0, 1.1f);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 4 — Distant echo tones (placed at irregular intervals)
    //  Frequencies chosen from a phrygian-inflected set for maximum unease.
    //  These are the resonances of the cave walls — ancient and indifferent.
    // ─────────────────────────────────────────────────────────────────────
    {
        // Phrygian-ish pitch palette in low-mid range
        const double echo_pitches[] = {
            110.0,  // A2
            116.54, // Bb2  — minor 2nd above A
            130.81, // C3
            138.59, // C#3
            155.56, // Eb3
            164.81, // E3
            185.0,  // F#3
            196.0,  // G3
        };
        constexpr int n_pitches = static_cast<int>(
            sizeof(echo_pitches) / sizeof(echo_pitches[0]));

        // Populate tones across body + development + tension sections
        // Irregular spacing: 3.0 s – 9.0 s between events
        double t_cursor = INTRO_DUR + 1.5;
        const double echo_end = TOTAL - FADEOUT_DUR - 2.0;
        int echo_count = 0;

        while (t_cursor < echo_end) {
            // Pick pitch pseudo-randomly
            const int   pitch_idx = rng.randi(0, n_pitches - 1);
            const double freq     = echo_pitches[pitch_idx];

            // Duration: 3.0 – 7.0 s (longer = further away)
            const double dur      = rng.randf(3.0, 7.0);

            // Amplitude varies with "distance" — quieter = further
            const float amp       = static_cast<float>(rng.randf(0.12, 0.42));

            Buffer et = detail::echo_tone(freq, dur, amp, sample_rate);

            // Fade tension: echo tones slightly louder in tension section
            float section_gain = 0.75f;
            if (t_cursor > INTRO_DUR + BODY_DUR + DEVEL_DUR)
                section_gain = 1.0f;

            bus.schedule(et, t_cursor, section_gain);

            // Irregular gap before next echo
            t_cursor += rng.randf(3.5, 8.5);
            ++echo_count;

            // In tension section: occasional rapid double echo (reflection)
            if (t_cursor > INTRO_DUR + BODY_DUR + DEVEL_DUR) {
                if ((rng.next() & 3u) == 0) {   // 25% chance
                    const double offset = rng.randf(0.8, 2.2);
                    const double freq2  = echo_pitches[rng.randi(0, n_pitches - 1)];
                    const float amp2    = amp * 0.5f;
                    Buffer et2 = detail::echo_tone(freq2, dur * 0.7, amp2, sample_rate);
                    bus.schedule(et2, t_cursor - rng.randf(1.0, 3.0), section_gain);
                    (void)offset;
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 5 — Water drip events (body + development sections)
    //  Sparse, irregular — each drop a tiny moment of pitch in the silence.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double drip_pitches[] = {
            392.0,   // G4
            440.0,   // A4
            493.88,  // B4
            523.25,  // C5
            659.25,  // E5
        };
        constexpr int n_dp = static_cast<int>(
            sizeof(drip_pitches) / sizeof(drip_pitches[0]));

        double t_cursor = INTRO_DUR + rng.randf(1.5, 4.0);
        const double drip_end = INTRO_DUR + BODY_DUR + DEVEL_DUR - 2.0;

        while (t_cursor < drip_end) {
            const double freq = drip_pitches[rng.randi(0, n_dp - 1)];
            Buffer d = detail::drip(freq, sample_rate);
            bus.schedule(d, t_cursor, static_cast<float>(rng.randf(0.5, 0.85)));

            // Occasional double-drip (drop splits on impact)
            if ((rng.next() & 7u) < 2u) {  // 25% chance
                const double offset = rng.randf(0.15, 0.35);
                Buffer d2 = detail::drip(freq * 0.95, sample_rate);
                bus.schedule(d2, t_cursor + offset, 0.4f);
            }
            t_cursor += rng.randf(4.5, 14.0);  // very sparse
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 6 — Dissonant pad (enters in development, peaks in tension)
    //  Slowly resolves to silence in the fade-out.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double pad_start = INTRO_DUR + BODY_DUR * 0.65;
        const double pad_dur   = TOTAL - pad_start - FADEOUT_DUR * 0.6;

        Buffer pad = detail::dissonant_pad(82.41, pad_dur, sample_rate); // E2

        // Envelope: slow fade-in, hold, slow fade-out
        fade_in (pad, 8.0,  sample_rate);
        fade_out(pad, 10.0, sample_rate);

        bus.schedule(pad, pad_start, 0.65f);
    }
    // A second dissonant pad a tritone away — enters later
    {
        const double pad2_start = INTRO_DUR + BODY_DUR + DEVEL_DUR * 0.3;
        const double pad2_dur   = TENSION_DUR + FADEOUT_DUR * 0.5;

        Buffer pad2 = detail::dissonant_pad(
            82.41 * std::pow(2.0, 6.0 / 12.0),  // augmented 4th / tritone
            pad2_dur, sample_rate);
        fade_in (pad2, 9.0, sample_rate);
        fade_out(pad2, 8.0, sample_rate);
        bus.schedule(pad2, pad2_start, 0.45f);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 7 — Breath-like modulation pulses (late development + tension)
    //  Slow amplitude-modulated sine tones that suggest organic presence.
    //  They appear, hold briefly, then fade — like something exhaling.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double breath_start = INTRO_DUR + BODY_DUR + DEVEL_DUR * 0.5;

        // Three breath events, irregularly spaced
        const double breathe_freqs[] = { 65.41, 55.0, 73.42 }; // C2, A1, D2
        const double breathe_durs [] = { 9.0,   12.0, 8.5 };
        const double breathe_offs [] = { 0.0,   10.5, 22.0 };

        for (int b = 0; b < 3; ++b) {
            const double dur = breathe_durs[b];
            const size_t n   = static_cast<size_t>(dur * sample_rate);
            Buffer breath(n, 0.0f);

            Oscillator osc(WaveShape::Sine, breathe_freqs[b], 0.7);
            LowPassFilter lpf(160.0, sample_rate);

            // Slow tremolo: 0.05 Hz
            double trPh = 0.0;
            const double trInc = TWO_PI * 0.05 / sample_rate;

            for (size_t i = 0; i < n; ++i) {
                const float s   = osc.tick(sample_rate);
                const float tr  = 0.5f + 0.5f * static_cast<float>(std::sin(trPh));
                breath[i] = lpf.tick(s) * tr;
                trPh += trInc;
            }
            fade_in (breath, dur * 0.35, sample_rate);
            fade_out(breath, dur * 0.4,  sample_rate);

            const double t_place = breath_start + breathe_offs[b];
            if (t_place + dur < TOTAL)
                bus.schedule(breath, t_place, 0.5f);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  FINAL MIX
    // ─────────────────────────────────────────────────────────────────────
    Buffer master = bus.mix();

    // Hard-clamp before normalise (layering can push past ±1)
    clamp_buffer(master);

    // Normalise to 0.82 peak — leave headroom, this is meant to be quiet
    normalize(master, 0.82f);

    // Final fade-in / fade-out polish
    fade_in (master, 0.5,           sample_rate);
    fade_out(master, FADEOUT_DUR,   sample_rate);

    return master;
}

} // namespace wauvio::soundtrack::cave
