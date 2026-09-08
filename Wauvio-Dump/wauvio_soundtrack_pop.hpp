// =============================================================================
//  W A U V I O _ S O U N D T R A C K _ P O P . H P P
//
//  Standalone upbeat pop/game-music soundtrack module.
//  Depends on wauvio.hpp ONLY.
//
//  Theme  : Bright, energetic, game-ready pop track.
//           Confident melody, driving groove, clean mix — the kind of
//           music that makes a menu screen feel exciting.
//
//  Namespace : wauvio::soundtrack::pop
//  Entry point:
//      wauvio::Buffer wauvio::soundtrack::pop::generate(int sample_rate = 0);
//
//  Audio characteristics:
//    • 120 BPM, 4/4 straight feel
//    • I–V–vi–IV chord progression (C major: C–G–Am–F)
//    • Bright lead (square wave, LPF shaped)
//    • Bright counter-melody / harmony (sawtooth)
//    • Punchy synth bass (sawtooth, sub-filtered)
//    • Full drum groove: four-on-floor kick, snare on 2&4, 16th hats
//    • Pad layer (sine chord stabs, sustained)
//    • Arpeggio fills during verses
//
//  Structure  (~75 seconds):
//    [00:00–00:08]  Intro      — drum groove + bass only (2 bars)
//    [00:08–00:24]  Verse 1    — chords + bass + arpeggio (4 bars)
//    [00:24–00:40]  Chorus 1   — full melody + all layers (4 bars)
//    [00:40–00:56]  Verse 2    — variation arpeggio, harmony added (4 bars)
//    [00:56–01:12]  Chorus 2   — chorus reprise, counter-melody added (4 bars)
//    [01:12–01:16]  Drop       — half-time feel, bass only (1 bar)
//    [01:16–01:28]  Breakdown  — stripped groove + melody (3 bars)
//    [01:28–01:15]  Final chorus — full intensity climax (last 2 bars)
// =============================================================================

#pragma once
#include "wauvio.hpp"

namespace wauvio::soundtrack::pop {

namespace detail {

// ---------------------------------------------------------------------------
//  POP KICK  — punchy, short decay, clean click
// ---------------------------------------------------------------------------
inline Buffer pop_kick(int sample_rate) {
    const double dur = 0.28;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);
    NoiseGenerator ng(0x1A2B3Cu);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t    = static_cast<double>(i) / sample_rate;
        const double freq = 50.0 + 100.0 * std::exp(-t * 25.0);
        phase += TWO_PI * freq / sample_rate;
        const float body  = static_cast<float>(std::sin(phase));
        const float click = ng.tick() * 0.18f;
        const float env   = static_cast<float>(std::exp(-t * 18.0));
        buf[i] = (body * 0.85f + click) * env;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  POP SNARE  — bright crack, wide noise band
// ---------------------------------------------------------------------------
inline Buffer pop_snare(int sample_rate) {
    const double dur = 0.16;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);
    NoiseGenerator ng(0x4D5E6Fu);
    HighPassFilter hpf(1200.0, sample_rate);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        phase += TWO_PI * 180.0 / sample_rate;
        const float body = static_cast<float>(std::sin(phase)) * 0.35f;
        const float snap = hpf.tick(ng.tick()) * 0.65f;
        const float env  = static_cast<float>(std::exp(-t * 28.0));
        buf[i] = (body + snap) * env * 0.70f;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  CLOSED HI-HAT  — tight, defined
// ---------------------------------------------------------------------------
inline Buffer pop_hihat(int sample_rate) {
    const double dur = 0.045;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);
    NoiseGenerator ng(0x7A8B9Cu);
    HighPassFilter hpf(7500.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        const float  env = static_cast<float>(std::exp(-t * 75.0));
        buf[i] = hpf.tick(ng.tick()) * env * 0.40f;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  OPEN HI-HAT  — used on the "and" of beat 4 for groove
// ---------------------------------------------------------------------------
inline Buffer pop_open_hat(int sample_rate) {
    const double dur = 0.22;
    const size_t n   = static_cast<size_t>(dur * sample_rate);
    Buffer buf(n);
    NoiseGenerator ng(0xABCDEFu);
    HighPassFilter hpf(6000.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        const float  env = static_cast<float>(std::exp(-t * 12.0));
        buf[i] = hpf.tick(ng.tick()) * env * 0.38f;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  SYNTH BASS NOTE  — saw wave, heavy LPF, tight ADSR
// ---------------------------------------------------------------------------
inline Buffer synth_bass(double freq, double duration, float gain, int sample_rate) {
    Instrument bi;
    bi.shape      = WaveShape::Sawtooth;
    bi.envelope   = ADSR(0.006, 0.04, 0.82, 0.08);
    bi.gain       = gain;
    bi.use_lpf    = true;
    bi.lpf_cutoff = 550.0;
    return bi.render_note(Note(freq, duration, 1.0), sample_rate);
}

// ---------------------------------------------------------------------------
//  PAD CHORD  — soft sine stab, medium ADSR, full duration
// ---------------------------------------------------------------------------
inline Buffer pad_chord(const std::vector<double>& freqs,
                         double duration, float gain, int sample_rate)
{
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    for (double f : freqs) {
        Oscillator osc(WaveShape::Sine, f, 0.45);
        Buffer b(n);
        for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
        ADSR(0.05, 0.12, 0.70, 0.25).apply(b, duration, -1.0, sample_rate);
        mix_into(buf, b);
    }
    LowPassFilter lpf(3500.0, sample_rate);
    lpf.process(buf);
    apply_gain(buf, gain);
    return buf;
}

// ---------------------------------------------------------------------------
//  LEAD NOTE  — bright square wave, shaped with LPF
// ---------------------------------------------------------------------------
inline Buffer lead_note(double freq, double duration, float gain, int sample_rate) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    // Square wave (fundamental character)
    {
        Oscillator osc(WaveShape::Square, freq, 0.7);
        Buffer b(n);
        for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
        ADSR(0.008, 0.06, 0.68, 0.08).apply(b, duration, -1.0, sample_rate);
        mix_into(buf, b);
    }
    // Soft sine layer underneath (body)
    {
        Oscillator osc(WaveShape::Sine, freq, 0.30);
        Buffer b(n);
        for (size_t i = 0; i < n; ++i) b[i] = osc.tick(sample_rate);
        ADSR(0.010, 0.08, 0.60, 0.10).apply(b, duration, -1.0, sample_rate);
        mix_into(buf, b);
    }

    LowPassFilter lpf(5500.0, sample_rate);
    lpf.process(buf);
    apply_gain(buf, gain);
    return buf;
}

// ---------------------------------------------------------------------------
//  COUNTER-MELODY NOTE  — bright sawtooth, slightly softer
// ---------------------------------------------------------------------------
inline Buffer counter_note(double freq, double duration, float gain, int sample_rate) {
    Instrument ci;
    ci.shape      = WaveShape::Sawtooth;
    ci.envelope   = ADSR(0.010, 0.07, 0.58, 0.10);
    ci.gain       = gain;
    ci.use_lpf    = true;
    ci.lpf_cutoff = 4000.0;
    return ci.render_note(Note(freq, duration, 1.0), sample_rate);
}

} // namespace detail

// =============================================================================
//  PUBLIC ENTRY POINT
// =============================================================================

inline Buffer generate(int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    // ─────────────────────────────────────────────────────────────────────
    //  Timing — 120 BPM, straight 16th grid
    // ─────────────────────────────────────────────────────────────────────
    constexpr double BPM   = 120.0;
    constexpr int    STEPS = 16;
    const double     STEP  = 60.0 / (BPM * STEPS / 4.0);   // ~0.125 s
    const double     BEAT  = STEP * 4.0;                    //  0.5 s
    const double     BAR   = BEAT * 4.0;                    //  2.0 s

    MasterBus bus(sample_rate);

    // ─────────────────────────────────────────────────────────────────────
    //  CHORD PALETTE — C major:  C–G–Am–F  (I–V–vi–IV)
    //
    //  Note frequencies (Hz):
    //    C3=130.81  E3=164.81  G3=196.00  B3=246.94
    //    C4=261.63  D4=293.66  E4=329.63  F4=349.23
    //    G4=392.00  A4=440.00  B4=493.88  C5=523.25
    //    D5=587.33  E5=659.25
    // ─────────────────────────────────────────────────────────────────────
    using CV = std::vector<double>;

    const CV C_chord  = { 261.63, 329.63, 392.00, 523.25 }; // C4 E4 G4 C5
    const CV G_chord  = { 196.00, 246.94, 293.66, 392.00 }; // G3 B3 D4 G4
    const CV Am_chord = { 220.00, 261.63, 329.63, 440.00 }; // A3 C4 E4 A4
    const CV F_chord  = { 174.61, 220.00, 261.63, 349.23 }; // F3 A3 C4 F4

    const std::vector<CV>     prog        = { C_chord, G_chord, Am_chord, F_chord };
    const std::vector<double> bass_roots  = { 65.41, 49.00, 55.00, 43.65 }; // C2 G1 A1 F1
    const std::vector<double> bass_roots2 = { 130.81, 98.00, 110.00, 87.31 }; // C3 G2 A2 F2

    // ─────────────────────────────────────────────────────────────────────
    //  DRUM PATTERN HELPER
    //  Standard pop pattern: kick on 1&3, snare on 2&4, 16th hats, open on "4+"
    // ─────────────────────────────────────────────────────────────────────
    auto add_drum_bar = [&](double bar_start, float kick_gain = 1.0f,
                             bool add_open_hat = true)
    {
        const double kick_steps[]  = { 0, 8 };
        const double snare_steps[] = { 4, 12 };

        for (double s : kick_steps)
            bus.schedule(detail::pop_kick(sample_rate),
                         bar_start + s * STEP, kick_gain);
        for (double s : snare_steps)
            bus.schedule(detail::pop_snare(sample_rate),
                         bar_start + s * STEP, 0.70f);
        // 16th note hats
        for (int h = 0; h < 16; ++h)
            bus.schedule(detail::pop_hihat(sample_rate),
                         bar_start + h * STEP, 0.35f);
        // Open hat on step 14 (the "and" of beat 4)
        if (add_open_hat)
            bus.schedule(detail::pop_open_hat(sample_rate),
                         bar_start + 14 * STEP, 0.40f);
    };

    // ─────────────────────────────────────────────────────────────────────
    //  MELODY PHRASES
    //  Main hook — memorable, singable, outlines the C major chord tones.
    //  Uses scale: C D E F G A B C
    // ─────────────────────────────────────────────────────────────────────
    using NP = std::pair<double, double>;  // {freq_hz, duration_secs}

    // Phrase A (2 bars): C5  B4  A4  G4 | E4  G4  A4  C5
    const std::vector<NP> phrase_A = {
        {523.25, STEP*2}, {493.88, STEP*2}, {440.00, STEP*3}, {392.00, STEP*3},
        {329.63, STEP*2}, {392.00, STEP*2}, {440.00, STEP*2}, {523.25, STEP*4},
        {0.0,    STEP*2},  // breath
    };

    // Phrase B (2 bars): E5  D5  C5  B4 | A4  G4  E4  C4
    const std::vector<NP> phrase_B = {
        {659.25, STEP*2}, {587.33, STEP*2}, {523.25, STEP*3}, {493.88, STEP*3},
        {440.00, STEP*2}, {392.00, STEP*2}, {329.63, STEP*2}, {261.63, STEP*6},
        {0.0,    STEP*2},
    };

    // Bridge melody (2 bars): A4  A4  G4  F4 | E4  F4  G4  E4
    const std::vector<NP> phrase_bridge = {
        {440.00, STEP*2}, {440.00, STEP*1}, {392.00, STEP*2}, {349.23, STEP*3},
        {329.63, STEP*2}, {349.23, STEP*2}, {392.00, STEP*3}, {329.63, STEP*7},
        {0.0,    STEP*2},
    };

    // Counter-melody (answer phrase, played a 3rd below phrase A)
    const std::vector<NP> counter_A = {
        {392.00, STEP*2}, {349.23, STEP*2}, {329.63, STEP*3}, {261.63, STEP*3},
        {246.94, STEP*2}, {261.63, STEP*2}, {329.63, STEP*2}, {392.00, STEP*4},
        {0.0,    STEP*2},
    };

    auto render_melody = [&](const std::vector<NP>& ph, float gain) -> Buffer {
        Buffer out;
        for (auto& [f, d] : ph) {
            Buffer nb = (f < 1.0)
                ? Buffer(static_cast<size_t>(d * sample_rate), 0.0f)
                : detail::lead_note(f, d, gain, sample_rate);
            for (float s : nb) out.push_back(s);
        }
        return out;
    };

    auto render_counter = [&](const std::vector<NP>& ph, float gain) -> Buffer {
        Buffer out;
        for (auto& [f, d] : ph) {
            Buffer nb = (f < 1.0)
                ? Buffer(static_cast<size_t>(d * sample_rate), 0.0f)
                : detail::counter_note(f, d, gain, sample_rate);
            for (float s : nb) out.push_back(s);
        }
        return out;
    };

    // Arpeggio helpers: fast upward run through chord, 16th notes
    auto render_arp = [&](const CV& freqs, int bars) -> Buffer {
        Instrument ai;
        ai.shape    = WaveShape::Triangle;
        ai.envelope = ADSR(0.005, 0.04, 0.65, 0.05);
        ai.gain     = 0.30f;
        ai.use_lpf    = true;
        ai.lpf_cutoff = 5000.0;

        Track tr(ai);
        const int total_steps = STEPS * bars;
        for (int s = 0; s < total_steps; ++s)
            tr.add(freqs[s % freqs.size()], STEP, 0.75);
        return tr.render(sample_rate);
    };

    // ─────────────────────────────────────────────────────────────────────
    //  INTRO  [0:00–0:08]  2 bars — drums + bass only
    // ─────────────────────────────────────────────────────────────────────
    constexpr int INTRO_B     = 2;
    constexpr int VERSE1_B    = 4;
    constexpr int CHORUS1_B   = 4;
    constexpr int VERSE2_B    = 4;
    constexpr int CHORUS2_B   = 4;
    constexpr int DROP_B      = 1;
    constexpr int BREAKDOWN_B = 3;
    constexpr int FINAL_CH_B  = 4;
    constexpr int OUTRO_B     = 2;

    double t = 0.0;

    // Intro
    for (int b = 0; b < INTRO_B; ++b) {
        add_drum_bar(t + b * BAR, 0.90f, b > 0);
        // Simple bass root notes (C2 whole note per bar)
        bus.schedule(detail::synth_bass(65.41, BAR, 0.55f, sample_rate), t + b * BAR);
    }
    t += INTRO_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  VERSE 1  [0:08–0:24]  4 bars — chords + arpeggio + bass
    // ─────────────────────────────────────────────────────────────────────
    for (int b = 0; b < VERSE1_B; ++b) {
        add_drum_bar(t + b * BAR);
        const int ci = b % 4;

        // Pad chord
        Buffer pc = detail::pad_chord(prog[ci], BAR * 0.9, 0.32f, sample_rate);
        bus.schedule(pc, t + b * BAR);

        // Bass — root + 5th walk
        bus.schedule(detail::synth_bass(bass_roots[ci],  BEAT * 1.5, 0.58f, sample_rate),
                     t + b * BAR);
        bus.schedule(detail::synth_bass(bass_roots[ci] * 1.5, BEAT * 0.9, 0.42f, sample_rate),
                     t + b * BAR + BEAT * 2.0);
        bus.schedule(detail::synth_bass(bass_roots[(ci+1)%4], BEAT * 0.5, 0.48f, sample_rate),
                     t + b * BAR + BEAT * 3.5);

        // Arpeggio (16th notes through chord tones)
        Buffer arp = render_arp(prog[ci], 1);
        bus.schedule(arp, t + b * BAR, 0.75f);
    }
    t += VERSE1_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  CHORUS 1  [0:24–0:40]  4 bars — full melody
    // ─────────────────────────────────────────────────────────────────────
    // Schedule melody: phrase A (2 bars) + phrase B (2 bars)
    {
        Buffer melA = render_melody(phrase_A, 0.62f);
        bus.schedule(melA, t);
        Buffer melB = render_melody(phrase_B, 0.62f);
        bus.schedule(melB, t + 2.0 * BAR);
    }
    for (int b = 0; b < CHORUS1_B; ++b) {
        add_drum_bar(t + b * BAR, 1.0f, true);
        const int ci = b % 4;

        Buffer pc = detail::pad_chord(prog[ci], BAR, 0.28f, sample_rate);
        bus.schedule(pc, t + b * BAR);

        // Bass (root + octave fills)
        bus.schedule(detail::synth_bass(bass_roots2[ci], BEAT * 2.0, 0.65f, sample_rate),
                     t + b * BAR);
        bus.schedule(detail::synth_bass(bass_roots2[ci] * 1.5, BEAT, 0.50f, sample_rate),
                     t + b * BAR + BEAT * 2.5);
        bus.schedule(detail::synth_bass(bass_roots2[(ci+1)%4], BEAT * 0.5, 0.55f, sample_rate),
                     t + b * BAR + BEAT * 3.5);
    }
    t += CHORUS1_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  VERSE 2  [0:40–0:56]  4 bars — variation: harmony added
    // ─────────────────────────────────────────────────────────────────────
    for (int b = 0; b < VERSE2_B; ++b) {
        add_drum_bar(t + b * BAR);
        const int ci = b % 4;

        Buffer pc = detail::pad_chord(prog[ci], BAR * 0.9, 0.30f, sample_rate);
        bus.schedule(pc, t + b * BAR);

        bus.schedule(detail::synth_bass(bass_roots[ci],  BEAT * 1.5, 0.56f, sample_rate),
                     t + b * BAR);
        bus.schedule(detail::synth_bass(bass_roots[(ci+1)%4], BEAT * 0.5, 0.46f, sample_rate),
                     t + b * BAR + BEAT * 3.5);

        // Variation arpeggio: reverse pattern
        const CV& ch = prog[ci];
        CV rev_ch(ch.rbegin(), ch.rend());
        Buffer arp = render_arp(rev_ch, 1);
        bus.schedule(arp, t + b * BAR, 0.65f);
    }
    // Harmony melody: phrase A a 3rd above
    {
        const std::vector<NP> phrase_A_high = {
            {659.25, STEP*2}, {622.25, STEP*2}, {554.37, STEP*3}, {493.88, STEP*3},
            {415.30, STEP*2}, {493.88, STEP*2}, {554.37, STEP*2}, {659.25, STEP*4},
            {0.0,    STEP*2},
        };
        Buffer harm = render_counter(phrase_A_high, 0.38f);
        bus.schedule(harm, t);
    }
    t += VERSE2_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  CHORUS 2  [0:56–1:12]  4 bars — counter-melody added
    // ─────────────────────────────────────────────────────────────────────
    {
        Buffer melA = render_melody(phrase_A, 0.60f);
        bus.schedule(melA, t);
        Buffer melB = render_melody(phrase_B, 0.60f);
        bus.schedule(melB, t + 2.0 * BAR);
        // Counter-melody
        Buffer cntA = render_counter(counter_A, 0.38f);
        bus.schedule(cntA, t);
    }
    for (int b = 0; b < CHORUS2_B; ++b) {
        add_drum_bar(t + b * BAR, 1.0f, true);
        const int ci = b % 4;

        Buffer pc = detail::pad_chord(prog[ci], BAR, 0.28f, sample_rate);
        bus.schedule(pc, t + b * BAR);

        bus.schedule(detail::synth_bass(bass_roots2[ci], BEAT * 2.0, 0.65f, sample_rate),
                     t + b * BAR);
        bus.schedule(detail::synth_bass(bass_roots2[ci] * 1.5, BEAT, 0.50f, sample_rate),
                     t + b * BAR + BEAT * 2.5);
        bus.schedule(detail::synth_bass(bass_roots2[(ci+1)%4], BEAT * 0.5, 0.55f, sample_rate),
                     t + b * BAR + BEAT * 3.5);
    }
    t += CHORUS2_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  DROP  [1:12–1:14]  1 bar — half-time, bass + kick only
    // ─────────────────────────────────────────────────────────────────────
    {
        // Only kick on 1, no other drums — sudden space
        bus.schedule(detail::pop_kick(sample_rate), t, 0.95f);
        bus.schedule(detail::synth_bass(65.41, BAR, 0.70f, sample_rate), t);
        // Single snare hit at end of bar for tension-break
        bus.schedule(detail::pop_snare(sample_rate), t + BEAT * 3.5, 0.80f);
    }
    t += DROP_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  BREAKDOWN  [1:14–1:20]  3 bars — stripped groove + melody
    // ─────────────────────────────────────────────────────────────────────
    {
        // Simpler drum: just kick + snare, no hats
        for (int b = 0; b < BREAKDOWN_B; ++b) {
            bus.schedule(detail::pop_kick(sample_rate),  t + b * BAR,            0.90f);
            bus.schedule(detail::pop_kick(sample_rate),  t + b * BAR + BEAT * 2, 0.70f);
            bus.schedule(detail::pop_snare(sample_rate), t + b * BAR + BEAT,     0.75f);
            bus.schedule(detail::pop_snare(sample_rate), t + b * BAR + BEAT * 3, 0.75f);
        }
        // Bridge melody
        Buffer bm = render_melody(phrase_bridge, 0.58f);
        bus.schedule(bm, t);
        // Minimal bass
        for (int b = 0; b < BREAKDOWN_B; ++b) {
            const int ci = b % 4;
            bus.schedule(detail::synth_bass(bass_roots2[ci], BAR * 0.9, 0.55f, sample_rate),
                         t + b * BAR);
        }
    }
    t += BREAKDOWN_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  FINAL CHORUS  [1:20–1:28]  4 bars — max intensity
    // ─────────────────────────────────────────────────────────────────────
    {
        // Melody + counter in unison layer
        Buffer melA = render_melody(phrase_A, 0.65f);
        bus.schedule(melA, t);
        Buffer melB = render_melody(phrase_B, 0.65f);
        bus.schedule(melB, t + 2.0 * BAR);
        Buffer cntA = render_counter(counter_A, 0.40f);
        bus.schedule(cntA, t);

        for (int b = 0; b < FINAL_CH_B; ++b) {
            add_drum_bar(t + b * BAR, 1.05f, true);
            const int ci = b % 4;

            Buffer pc = detail::pad_chord(prog[ci], BAR, 0.30f, sample_rate);
            bus.schedule(pc, t + b * BAR);

            bus.schedule(detail::synth_bass(bass_roots2[ci], BEAT * 2.0, 0.70f, sample_rate),
                         t + b * BAR);
            bus.schedule(detail::synth_bass(bass_roots2[ci] * 1.5, BEAT, 0.55f, sample_rate),
                         t + b * BAR + BEAT * 2.5);
            bus.schedule(detail::synth_bass(bass_roots2[(ci+1)%4], BEAT * 0.5, 0.60f, sample_rate),
                         t + b * BAR + BEAT * 3.5);

            // Arpeggio back for final energy
            Buffer arp = render_arp(prog[ci], 1);
            bus.schedule(arp, t + b * BAR, 0.55f);
        }
    }
    t += FINAL_CH_B * BAR;

    // ─────────────────────────────────────────────────────────────────────
    //  OUTRO  [last 2 bars] — kit out, bass fades
    // ─────────────────────────────────────────────────────────────────────
    {
        for (int b = 0; b < OUTRO_B; ++b) {
            bus.schedule(detail::pop_kick(sample_rate),  t + b * BAR, 0.70f);
            bus.schedule(detail::pop_snare(sample_rate), t + b * BAR + BEAT * 2, 0.50f);
        }
        Buffer ob = detail::pad_chord(C_chord, OUTRO_B * BAR, 0.30f, sample_rate);
        fade_out(ob, OUTRO_B * BAR * 0.8, sample_rate);
        bus.schedule(ob, t);
        Buffer bass_out = detail::synth_bass(65.41, OUTRO_B * BAR, 0.45f, sample_rate);
        fade_out(bass_out, OUTRO_B * BAR, sample_rate);
        bus.schedule(bass_out, t);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  FINAL MIX
    // ─────────────────────────────────────────────────────────────────────
    Buffer master = bus.mix();
    clamp_buffer(master);
    normalize(master, 0.88f);
    fade_in (master, 0.05, sample_rate);
    fade_out(master, 3.0,  sample_rate);
    return master;
}

} // namespace wauvio::soundtrack::pop
