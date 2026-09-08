// =============================================================================
//  W A U V I O _ S O U N D T R A C K _ L O F I . H P P
//
//  Standalone lo-fi hip-hop / chill-hop procedural soundtrack module.
//  Depends on wauvio.hpp ONLY.
//
//  Theme  : Rainy afternoon, dusty record spinning, coffee going cold.
//           Nostalgic, unhurried, slightly worn at the edges.
//
//  Namespace : wauvio::soundtrack::lofi
//  Entry point:
//      wauvio::Buffer wauvio::soundtrack::lofi::generate(int sample_rate = 0);
//
//  Audio characteristics:
//    • ~70 BPM with swing 8th-note feel (delayed off-beats)
//    • Warm jazz-influenced chord voicings (maj7, min7, dom7 extensions)
//    • Slightly detuned piano-like leads (sine + triangle layer)
//    • Soft Rhodes-style chord stabs (triangle + lpf, long release)
//    • Walking bass (sawtooth, heavily filtered)
//    • Lo-fi drum groove: soft kick, rim snare, subtle closed hats
//    • Vinyl hiss layer (shaped white noise)
//    • Occasional high-pass "crackle" transient (vinyl pop)
//    • Subtle tape-wobble pitch drift on the melody
//
//  Structure  (~75 seconds):
//    [00:00–00:06]  Intro      — vinyl hiss only, bass note fades in
//    [00:06–00:22]  Verse A    — chords + bass groove + hats
//    [00:22–00:38]  Verse B    — melody enters over the groove
//    [00:38–00:54]  Bridge     — chord variation, more open feel
//    [00:54–01:09]  Verse A'   — melody + groove, slight variation
//    [01:09–01:15]  Outro      — strip back, vinyl hiss, fade
// =============================================================================

#pragma once
#include "wauvio.hpp"

namespace wauvio::soundtrack::lofi {

// =============================================================================
//  INTERNAL DETAIL NAMESPACE
// =============================================================================

namespace detail {

// ---------------------------------------------------------------------------
//  PRNG — same lightweight xorshift32 used in cave module
// ---------------------------------------------------------------------------
struct PRNG {
    uint32_t state;
    explicit PRNG(uint32_t seed = 0xBEEFCAFEu) : state(seed) {}
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    double randf(double lo, double hi) {
        return lo + (static_cast<double>(next()) / 4294967295.0) * (hi - lo);
    }
};

// ---------------------------------------------------------------------------
//  SWING HELPER
//  Lo-fi hip-hop is defined by its swing timing.
//  In a 16-step grid at 70 BPM, odd 8th-note subdivisions are pushed back
//  by ~30% of the sub-grid interval, creating a laid-back feel.
//
//  Returns the onset time (seconds) of step `s` in bar `bar`.
// ---------------------------------------------------------------------------
inline double swung_onset(int bar, int step, double bpm,
                           int steps_per_bar, double swing_ratio = 0.62)
{
    // Base duration of one grid step (straight)
    const double step_dur  = 60.0 / (bpm * static_cast<double>(steps_per_bar) / 4.0);
    const double bar_dur   = step_dur * steps_per_bar;

    // Pair steps into 8th-note groups (steps 0-1, 2-3, 4-5…)
    // The "and" of each beat (odd steps of each pair) is pushed later.
    const int    pair       = step / 2;
    const int    within     = step % 2;
    const double pair_start = pair * step_dur * 2.0;
    const double offset     = (within == 0)
        ? 0.0
        : step_dur * 2.0 * swing_ratio;  // delayed "and"

    return bar * bar_dur + pair_start + offset;
}

// ---------------------------------------------------------------------------
//  VINYL HISS  — shaped white noise simulating vinyl surface noise
// ---------------------------------------------------------------------------
inline Buffer vinyl_hiss(double duration, int sample_rate, uint32_t seed) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);

    NoiseGenerator ng(seed);
    LowPassFilter  lpf(7000.0, sample_rate);
    HighPassFilter hpf(400.0,  sample_rate);

    // Very slow amplitude sway (0.12 Hz)
    double ph = 0.0;
    const double inc = TWO_PI * 0.12 / sample_rate;

    for (size_t i = 0; i < n; ++i) {
        const float mod = 0.72f + 0.28f * static_cast<float>(std::sin(ph));
        buf[i] = hpf.tick(lpf.tick(ng.tick())) * mod * 0.055f;
        ph += inc;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  VINYL CRACKLE — sparse high-frequency transient pops
// ---------------------------------------------------------------------------
inline Buffer vinyl_crackle(double duration, int sample_rate, uint32_t seed) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    PRNG rng(seed);
    NoiseGenerator ng(seed ^ 0x1234u);
    HighPassFilter hpf(5000.0, sample_rate);

    // Sprinkle ~12 crackle events over the duration
    for (int k = 0; k < 12; ++k) {
        const size_t pos    = static_cast<size_t>(rng.randf(0.0, 1.0) * n);
        const size_t crack_n = static_cast<size_t>(0.006 * sample_rate);  // 6 ms
        for (size_t i = 0; i < crack_n && (pos + i) < n; ++i) {
            const double t   = static_cast<double>(i) / sample_rate;
            const float  env = static_cast<float>(std::exp(-t * 500.0));
            buf[pos + i] += hpf.tick(ng.tick()) * env * 0.35f;
        }
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  TAPE WOBBLE
//  Tiny sinusoidal pitch modulation applied to a pre-rendered buffer via
//  nearest-sample interpolation — produces the subtle waver of old tape.
//  wobble_hz: how fast the pitch wanders (~0.4 Hz for gentle tape flutter)
//  depth:     maximum pitch deviation fraction (0.003 ≈ ±0.3%)
// ---------------------------------------------------------------------------
inline Buffer tape_wobble(const Buffer& src, double wobble_hz,
                           double depth, int sample_rate)
{
    const size_t n = src.size();
    Buffer out(n);

    double phase = 0.0;
    const double inc = TWO_PI * wobble_hz / sample_rate;

    for (size_t i = 0; i < n; ++i) {
        const double offset = depth * std::sin(phase) * sample_rate;
        const double pos    = static_cast<double>(i) + offset;
        const size_t idx    = static_cast<size_t>(
            std::max(0.0, std::min(static_cast<double>(n - 1), pos)));
        out[i] = src[idx];
        phase += inc;
    }
    return out;
}

// ---------------------------------------------------------------------------
//  LO-FI KICK
//  Soft sine-body pitch sweep + tiny click transient.
//  Much quieter and rounder than a standard kick.
// ---------------------------------------------------------------------------
inline Buffer lofi_kick(int sample_rate) {
    const double dur = 0.35;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);

    NoiseGenerator ng(0xAABBu);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t    = static_cast<double>(i) / sample_rate;
        const double freq = 55.0 + 120.0 * std::exp(-t * 20.0);
        phase += TWO_PI * freq / sample_rate;
        const float body  = static_cast<float>(std::sin(phase));
        const float click = ng.tick() * 0.12f;
        const float env   = static_cast<float>(std::exp(-t * 14.0));
        buf[i] = (body + click) * env * 0.65f;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  RIM SHOT / SNARE  (lighter than a pop snare — more like a rim tap)
// ---------------------------------------------------------------------------
inline Buffer lofi_rim(int sample_rate) {
    const double dur = 0.10;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);

    NoiseGenerator ng(0xCCDDu);
    HighPassFilter hpf(2500.0, sample_rate);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        phase += TWO_PI * 400.0 / sample_rate;
        const float body = static_cast<float>(std::sin(phase)) * 0.3f;
        const float snap = hpf.tick(ng.tick()) * 0.7f;
        const float env  = static_cast<float>(std::exp(-t * 45.0));
        buf[i] = (body + snap) * env * 0.45f;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  CLOSED HI-HAT  (very soft, brushy)
// ---------------------------------------------------------------------------
inline Buffer lofi_hat(int sample_rate) {
    const double dur = 0.04;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);

    NoiseGenerator ng(0xEEFFu);
    HighPassFilter hpf(7000.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        const float  env = static_cast<float>(std::exp(-t * 90.0));
        buf[i] = hpf.tick(ng.tick()) * env * 0.28f;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  RHODES-STYLE CHORD STAB
//  Triangle wave + detuned copy → LPF → long ADSR
//  Jazz chord voicings built from MIDI intervals.
// ---------------------------------------------------------------------------
inline Buffer rhodes_chord(const std::vector<double>& freqs,
                            double duration, float gain, int sample_rate)
{
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    for (double f : freqs) {
        // Layer 1: triangle
        {
            Oscillator osc(WaveShape::Triangle, f, 0.55);
            Buffer b(n);
            for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
            ADSR(0.012, 0.18, 0.55, 0.35).apply(b, duration, -1.0, sample_rate);
            mix_into(buf, b);
        }
        // Layer 2: slightly detuned sine (adds warmth)
        {
            Oscillator osc(WaveShape::Sine, f * 1.0015, 0.35);
            Buffer b(n);
            for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
            ADSR(0.015, 0.20, 0.45, 0.40).apply(b, duration, -1.0, sample_rate);
            mix_into(buf, b);
        }
    }

    LowPassFilter lpf(2800.0, sample_rate);
    lpf.process(buf);
    apply_gain(buf, gain);
    return buf;
}

// ---------------------------------------------------------------------------
//  WALKING BASS NOTE
//  Sawtooth → heavy LPF → ADSR.  The characteristic lo-fi bass timbre.
// ---------------------------------------------------------------------------
inline Buffer bass_note(double freq, double duration, float gain, int sample_rate) {
    Instrument bi;
    bi.shape      = WaveShape::Sawtooth;
    bi.envelope   = ADSR(0.008, 0.08, 0.72, 0.12);
    bi.gain       = gain;
    bi.use_lpf    = true;
    bi.lpf_cutoff = 320.0;
    return bi.render_note(Note(freq, duration, 1.0), sample_rate);
}

// ---------------------------------------------------------------------------
//  MELODY NOTE  (piano-like sine + harmonics)
//  Tape wobble applied to the complete melody track after rendering.
// ---------------------------------------------------------------------------
inline Buffer melody_note(double freq, double duration, float gain, int sample_rate) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    // Fundamental
    {
        Oscillator osc(WaveShape::Sine, freq, 0.6);
        Buffer b(n);
        for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
        ADSR(0.008, 0.12, 0.55, 0.20).apply(b, duration, -1.0, sample_rate);
        mix_into(buf, b);
    }
    // 2nd harmonic (octave, softer)
    {
        Oscillator osc(WaveShape::Sine, freq * 2.0, 0.22);
        Buffer b(n);
        for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
        ADSR(0.006, 0.14, 0.40, 0.15).apply(b, duration, -1.0, sample_rate);
        mix_into(buf, b);
    }
    // 3rd harmonic (adds presence)
    {
        Oscillator osc(WaveShape::Sine, freq * 3.0, 0.10);
        Buffer b(n);
        for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
        ADSR(0.005, 0.10, 0.30, 0.10).apply(b, duration, -1.0, sample_rate);
        mix_into(buf, b);
    }

    LowPassFilter lpf(4500.0, sample_rate);
    lpf.process(buf);
    apply_gain(buf, gain);
    return buf;
}

} // namespace detail

// =============================================================================
//  PUBLIC ENTRY POINT
// =============================================================================

inline Buffer generate(int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    // ─────────────────────────────────────────────────────────────────────
    //  Timing — 70 BPM, 16 steps/bar, swing ~0.62
    // ─────────────────────────────────────────────────────────────────────
    constexpr double BPM          = 70.0;
    constexpr int    STEPS        = 16;
    constexpr double SWING        = 0.63;
    const double     STEP         = 60.0 / (BPM * STEPS / 4.0);
    const double     BAR          = STEP * STEPS;

    // Section durations (in bars)
    constexpr int INTRO_BARS  = 2;   //  ~6.8 s
    constexpr int VERSE_A_BARS = 6;  // ~20.6 s
    constexpr int VERSE_B_BARS = 6;  // ~20.6 s
    constexpr int BRIDGE_BARS  = 6;  // ~20.6 s
    constexpr int VERSEA2_BARS = 5;  // ~17.2 s
    constexpr int OUTRO_BARS   = 2;  //  ~6.8 s
    constexpr int TOTAL_BARS   = INTRO_BARS + VERSE_A_BARS + VERSE_B_BARS
                               + BRIDGE_BARS + VERSEA2_BARS + OUTRO_BARS;
    const double TOTAL_SECS    = TOTAL_BARS * BAR;

    MasterBus bus(sample_rate);

    // ─────────────────────────────────────────────────────────────────────
    //  CHORD PALETTE  (all voicings use root + extension stacks)
    //  Key: F major / D minor — warm and relaxed
    //
    //  Chord tones (Hz):
    //    Fmaj7  : F3  A3  C4  E4        (174.6  220.0  261.6  329.6)
    //    Dm7    : D3  F3  A3  C4        (146.8  174.6  220.0  261.6)
    //    Bbmaj7 : Bb2 D3  F3  A3        (116.5  146.8  174.6  220.0)
    //    Gm7    : G2  Bb2 D3  F3        ( 98.0  116.5  146.8  174.6)
    //    C9(no root): E3 G3  Bb3 D4     (164.8  196.0  233.1  293.7)
    //    Am7    : A2  C3  E3  G3        (110.0  130.8  164.8  196.0)
    // ─────────────────────────────────────────────────────────────────────
    using CV = std::vector<double>;

    const CV fmaj7  = { 174.61, 220.00, 261.63, 329.63 };
    const CV dm7    = { 146.83, 174.61, 220.00, 261.63 };
    const CV bbmaj7 = { 116.54, 146.83, 174.61, 220.00 };
    const CV gm7    = {  98.00, 116.54, 146.83, 174.61 };
    const CV c9     = { 164.81, 196.00, 233.08, 293.66 };
    const CV am7    = { 110.00, 130.81, 164.81, 196.00 };

    // Main progression: Fmaj7 → Dm7 → Bbmaj7 → C9   (4 bars)
    // Bridge progression: Am7 → Dm7 → Gm7 → C9       (4 bars)
    const std::vector<CV> main_prog   = { fmaj7, dm7, bbmaj7, c9 };
    const std::vector<CV> bridge_prog = { am7, dm7, gm7, c9 };

    // Bass root frequencies for each chord (one octave below chord root)
    const double bass_roots_main[]   = { 87.31, 73.42, 58.27, 65.41 };  // F2 D2 Bb1 C2
    const double bass_roots_bridge[] = { 55.00, 73.42, 49.00, 65.41 };  // A1 D2 G1  C2

    // ─────────────────────────────────────────────────────────────────────
    //  LAYER 1: VINYL HISS  (runs entire piece)
    // ─────────────────────────────────────────────────────────────────────
    {
        Buffer hiss = detail::vinyl_hiss(TOTAL_SECS, sample_rate, 0xDECAFBADu);
        fade_in (hiss, 2.0, sample_rate);
        fade_out(hiss, 3.0, sample_rate);
        bus.schedule(hiss, 0.0, 1.0f);
    }
    // Crackle events across the body
    {
        Buffer crack = detail::vinyl_crackle(TOTAL_SECS, sample_rate, 0x0BADCAFE);
        bus.schedule(crack, 0.0, 1.0f);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  HELPER: schedule a chord stab at a given bar + step offset
    // ─────────────────────────────────────────────────────────────────────
    auto sched_chord = [&](double bar_off, int step, const CV& freqs,
                            double dur, float gain) {
        const double t = bar_off * BAR
                       + detail::swung_onset(0, step, BPM, STEPS, SWING);
        Buffer b = detail::rhodes_chord(freqs, dur, gain, sample_rate);
        bus.schedule(b, t, 1.0f);
    };

    // ─────────────────────────────────────────────────────────────────────
    //  INTRO  [bars 0–1] — bass drone only, hiss present
    // ─────────────────────────────────────────────────────────────────────
    {
        Buffer b = detail::bass_note(87.31, BAR * INTRO_BARS, 0.45f, sample_rate);
        fade_in(b, 1.5, sample_rate);
        bus.schedule(b, 0.0);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  VERSE A  [bars 2–7] — chords + bass walking + drum groove
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = INTRO_BARS * BAR;

        for (int rep = 0; rep < VERSE_A_BARS; ++rep) {
            const double bt = t0 + rep * BAR;
            const int chord_idx = rep % 4;
            const CV& chord = main_prog[chord_idx];
            const double bass_root = bass_roots_main[chord_idx];

            // Chord stab on beat 1 (step 0) and beat 3 (step 8)
            sched_chord(t0 / BAR + rep, 0, chord, STEP * 7.0, 0.35f);
            sched_chord(t0 / BAR + rep, 8, chord, STEP * 7.0, 0.28f);

            // Walking bass: 4 notes per bar (on the beat with swing feel)
            const double bass_dur = STEP * 3.5;
            for (int beat = 0; beat < 4; ++beat) {
                const double t_bass = bt + beat * STEP * 4.0;
                // slight chromatic walk on beats 3-4
                double bf = bass_root;
                if (beat == 2) bf *= 1.0595;  // semitone up
                if (beat == 3) bf = bass_roots_main[(chord_idx + 1) % 4]; // approach note
                Buffer bn = detail::bass_note(bf, bass_dur, 0.52f, sample_rate);
                bus.schedule(bn, t_bass);
            }

            // Drums: kick on 1 and 3 (steps 0, 8); rim on 2 and 4 (steps 4, 12)
            bus.schedule(detail::lofi_kick(sample_rate),
                         bt + detail::swung_onset(0, 0,  BPM, STEPS, SWING), 0.80f);
            bus.schedule(detail::lofi_kick(sample_rate),
                         bt + detail::swung_onset(0, 8,  BPM, STEPS, SWING), 0.60f);
            bus.schedule(detail::lofi_rim(sample_rate),
                         bt + detail::swung_onset(0, 4,  BPM, STEPS, SWING), 0.65f);
            bus.schedule(detail::lofi_rim(sample_rate),
                         bt + detail::swung_onset(0, 12, BPM, STEPS, SWING), 0.65f);

            // Hats on all 8th notes (steps 0,2,4,6,8,10,12,14) — swung
            for (int h : {0, 2, 4, 6, 8, 10, 12, 14}) {
                bus.schedule(detail::lofi_hat(sample_rate),
                             bt + detail::swung_onset(0, h, BPM, STEPS, SWING),
                             0.30f);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  VERSE B  [bars 8–13] — melody enters
    //  Melody phrase: Fmaj pentatonic with gentle motion
    //  F4  A4  C5  D5  E5  and back — gentle, unhurried
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = (INTRO_BARS + VERSE_A_BARS) * BAR;

        // Chord + bass groove (same as verse A, continues cycle)
        for (int rep = 0; rep < VERSE_B_BARS; ++rep) {
            const double bt = t0 + rep * BAR;
            const int chord_idx = rep % 4;
            const CV& chord = main_prog[chord_idx];
            const double bass_root = bass_roots_main[chord_idx];

            sched_chord(t0 / BAR + rep, 0, chord, STEP * 7.0, 0.30f);
            sched_chord(t0 / BAR + rep, 8, chord, STEP * 7.0, 0.24f);

            for (int beat = 0; beat < 4; ++beat) {
                const double t_bass = bt + beat * STEP * 4.0;
                double bf = bass_root;
                if (beat == 3) bf = bass_roots_main[(chord_idx + 1) % 4];
                bus.schedule(detail::bass_note(bf, STEP * 3.5, 0.50f, sample_rate), t_bass);
            }

            bus.schedule(detail::lofi_kick(sample_rate),
                         bt + detail::swung_onset(0, 0,  BPM, STEPS, SWING), 0.80f);
            bus.schedule(detail::lofi_kick(sample_rate),
                         bt + detail::swung_onset(0, 8,  BPM, STEPS, SWING), 0.60f);
            bus.schedule(detail::lofi_rim(sample_rate),
                         bt + detail::swung_onset(0, 4,  BPM, STEPS, SWING), 0.65f);
            bus.schedule(detail::lofi_rim(sample_rate),
                         bt + detail::swung_onset(0, 12, BPM, STEPS, SWING), 0.65f);
            for (int h : {0, 2, 4, 6, 8, 10, 12, 14}) {
                bus.schedule(detail::lofi_hat(sample_rate),
                             bt + detail::swung_onset(0, h, BPM, STEPS, SWING),
                             0.28f);
            }
        }

        // Melody: 2-bar phrase × 3 (with tape wobble applied after)
        // Notes: F4–A4–C5–A4  / D5–C5–A4–F4  (lazy, relaxed pacing)
        using NP = std::pair<double, double>;  // {freq, duration_in_steps}
        const std::vector<NP> phrase1 = {
            {349.23, STEP*3}, {440.00, STEP*2}, {523.25, STEP*4},
            {440.00, STEP*3}, {0.0,    STEP*4}, // rest
        };
        const std::vector<NP> phrase2 = {
            {587.33, STEP*4}, {523.25, STEP*2}, {440.00, STEP*3},
            {349.23, STEP*5}, {0.0,    STEP*2},
        };

        auto render_phrase = [&](const std::vector<NP>& ph) -> Buffer {
            Buffer mel_buf;
            for (auto& [f, d] : ph) {
                if (f < 1.0) {
                    Buffer s(static_cast<size_t>(d * sample_rate), 0.0f);
                    for (float x : s) mel_buf.push_back(x);
                } else {
                    Buffer nb = detail::melody_note(f, d, 0.55f, sample_rate);
                    for (float x : nb) mel_buf.push_back(x);
                }
            }
            return mel_buf;
        };

        // Stagger phrase starts with swing offsets
        for (int rep = 0; rep < 3; ++rep) {
            const double onset = t0 + rep * 2.0 * BAR
                               + detail::swung_onset(0, 2, BPM, STEPS, SWING);
            Buffer mel = render_phrase(rep % 2 == 0 ? phrase1 : phrase2);
            mel = detail::tape_wobble(mel, 0.38, 0.0025, sample_rate);
            bus.schedule(mel, onset, 0.70f);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  BRIDGE  [bars 14–19] — bridge chord progression, more open
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = (INTRO_BARS + VERSE_A_BARS + VERSE_B_BARS) * BAR;

        for (int rep = 0; rep < BRIDGE_BARS; ++rep) {
            const double bt = t0 + rep * BAR;
            const int chord_idx = rep % 4;
            const CV& chord = bridge_prog[chord_idx];
            const double bass_root = bass_roots_bridge[chord_idx];

            // Sparser chord placement in bridge
            sched_chord(t0 / BAR + rep, 1, chord, STEP * 6.5, 0.28f);
            if (rep % 2 == 1)
                sched_chord(t0 / BAR + rep, 9, chord, STEP * 5.0, 0.22f);

            // Simplified bass (just root + fifth)
            bus.schedule(detail::bass_note(bass_root, STEP * 7.5, 0.48f, sample_rate), bt);
            bus.schedule(detail::bass_note(bass_root * 1.5, STEP * 4.5, 0.38f, sample_rate),
                         bt + STEP * 8.0);

            // Kick on 1 only; snare on 3 — gives space
            bus.schedule(detail::lofi_kick(sample_rate),
                         bt + detail::swung_onset(0, 0, BPM, STEPS, SWING), 0.72f);
            bus.schedule(detail::lofi_rim(sample_rate),
                         bt + detail::swung_onset(0, 8, BPM, STEPS, SWING), 0.58f);
            // Hats every 4 steps only
            for (int h : {0, 4, 8, 12}) {
                bus.schedule(detail::lofi_hat(sample_rate),
                             bt + detail::swung_onset(0, h, BPM, STEPS, SWING),
                             0.22f);
            }
        }

        // Bridge melody: higher register, more spacious phrasing
        const std::vector<std::pair<double,double>> bridge_mel = {
            {659.25, STEP*4}, {0.0, STEP*2}, {587.33, STEP*3},
            {523.25, STEP*5}, {0.0, STEP*4},
            {659.25, STEP*2}, {698.46, STEP*4}, {0.0, STEP*2},
            {587.33, STEP*6}, {0.0, STEP*4},
        };
        Buffer bm;
        for (auto& [f, d] : bridge_mel) {
            if (f < 1.0) {
                Buffer s(static_cast<size_t>(d * sample_rate), 0.0f);
                for (float x : s) bm.push_back(x);
            } else {
                Buffer nb = detail::melody_note(f, d, 0.50f, sample_rate);
                for (float x : nb) bm.push_back(x);
            }
        }
        bm = detail::tape_wobble(bm, 0.35, 0.003, sample_rate);
        bus.schedule(bm, t0 + detail::swung_onset(0, 1, BPM, STEPS, SWING), 0.68f);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  VERSE A'  [bars 20–24] — return, melody + groove, slight variation
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = (INTRO_BARS + VERSE_A_BARS + VERSE_B_BARS
                           + BRIDGE_BARS) * BAR;

        for (int rep = 0; rep < VERSEA2_BARS; ++rep) {
            const double bt = t0 + rep * BAR;
            const int chord_idx = rep % 4;
            const CV& chord = main_prog[chord_idx];
            const double bass_root = bass_roots_main[chord_idx];

            sched_chord(t0 / BAR + rep, 0, chord, STEP * 7.0, 0.28f);
            sched_chord(t0 / BAR + rep, 8, chord, STEP * 7.0, 0.22f);

            for (int beat = 0; beat < 4; ++beat) {
                const double t_bass = bt + beat * STEP * 4.0;
                double bf = bass_root;
                if (beat == 3) bf = bass_roots_main[(chord_idx + 1) % 4];
                bus.schedule(detail::bass_note(bf, STEP * 3.5, 0.48f, sample_rate), t_bass);
            }

            bus.schedule(detail::lofi_kick(sample_rate),
                         bt + detail::swung_onset(0, 0,  BPM, STEPS, SWING), 0.78f);
            bus.schedule(detail::lofi_kick(sample_rate),
                         bt + detail::swung_onset(0, 8,  BPM, STEPS, SWING), 0.55f);
            bus.schedule(detail::lofi_rim(sample_rate),
                         bt + detail::swung_onset(0, 4,  BPM, STEPS, SWING), 0.60f);
            bus.schedule(detail::lofi_rim(sample_rate),
                         bt + detail::swung_onset(0, 12, BPM, STEPS, SWING), 0.60f);
            for (int h : {0, 2, 4, 6, 8, 10, 12, 14}) {
                bus.schedule(detail::lofi_hat(sample_rate),
                             bt + detail::swung_onset(0, h, BPM, STEPS, SWING),
                             0.25f);
            }
        }

        // Return melody: phrase1 twice
        for (int rep = 0; rep < 2; ++rep) {
            const double onset = t0 + rep * 2.0 * BAR
                               + detail::swung_onset(0, 2, BPM, STEPS, SWING);
            const std::vector<std::pair<double,double>> mel = {
                {349.23, STEP*3}, {440.00, STEP*2}, {523.25, STEP*4},
                {440.00, STEP*3}, {0.0,    STEP*4},
            };
            Buffer mb;
            for (auto& [f, d] : mel) {
                if (f < 1.0) {
                    Buffer s(static_cast<size_t>(d * sample_rate), 0.0f);
                    for (float x : s) mb.push_back(x);
                } else {
                    Buffer nb = detail::melody_note(f, d, 0.50f, sample_rate);
                    for (float x : nb) mb.push_back(x);
                }
            }
            mb = detail::tape_wobble(mb, 0.40, 0.0028, sample_rate);
            bus.schedule(mb, onset, 0.65f);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  OUTRO  [bars 25–26] — strip back, hiss remains
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = (INTRO_BARS + VERSE_A_BARS + VERSE_B_BARS
                           + BRIDGE_BARS + VERSEA2_BARS) * BAR;
        // Final bass note fading out
        Buffer ob = detail::bass_note(87.31, OUTRO_BARS * BAR, 0.35f, sample_rate);
        fade_out(ob, OUTRO_BARS * BAR * 0.7, sample_rate);
        bus.schedule(ob, t0);

        // One final chord stab
        Buffer fc = detail::rhodes_chord(fmaj7, STEP * 10.0, 0.22f, sample_rate);
        fade_out(fc, STEP * 9.0, sample_rate);
        bus.schedule(fc, t0 + detail::swung_onset(0, 0, BPM, STEPS, SWING));
    }

    // ─────────────────────────────────────────────────────────────────────
    //  FINAL MIX
    // ─────────────────────────────────────────────────────────────────────
    Buffer master = bus.mix();
    clamp_buffer(master);
    normalize(master, 0.82f);
    fade_in (master, 1.0, sample_rate);
    fade_out(master, 4.0, sample_rate);
    return master;
}

} // namespace wauvio::soundtrack::lofi
