// =============================================================================
//  wauvio_soundtrack_heavenly.hpp
//  Heavenly / Ethereal Ambient Soundtrack Generator
//
//  Namespace  : wauvio::soundtrack::heavenly
//  Depends on : wauvio.hpp  (included by caller — never modified here)
//
//  Musical spec
//  ─────────────
//  Key        : C major / A minor (modal, open)
//  Tempo feel : ~72 BPM pulse (very soft, mostly implied)
//  Duration   : 90 s default  (smoothly loopable)
//  Chords     : Cmaj7 → Fmaj7 → Am9 → Gsus2  (lush, open, spacious)
//  Layers
//    1. Pad          — detuned-triangle chorus, very slow attack, Haas stereo
//    2. Lead bells   — FM bell tones, sparse, long reverb tail
//    3. Soft bass    — low sine, minimal movement, 2-bar root pedal
//    4. Atmosphere   — HP-filtered breathe noise, subtle modulation
//    5. Pulse        — barely-there sub-sine heartbeat at half-tempo
//  FX
//    Reverb on pad + bells
//    Long delay  on bells
//    Chorus  on pad
//    LFO amplitude shimmer on atmosphere
//
//  Usage
//  ─────
//  #include "wauvio.hpp"
//  #include "wauvio_soundtrack_heavenly.hpp"
//  auto stereo = wauvio::soundtrack::heavenly::generate();
//  wauvio::save_wav_stereo(stereo, "heavenly.wav");
//  wauvio::play(stereo);
// =============================================================================

#pragma once
#include "wauvio.hpp"

namespace wauvio {
namespace soundtrack {
namespace heavenly {

// ─────────────────────────────────────────────────────────────────────────────
//  Private implementation detail
// ─────────────────────────────────────────────────────────────────────────────
namespace detail {

// ── Timing (soft 72 BPM feel) ────────────────────────────────────────────────
constexpr double BPM      = 72.0;
constexpr double BEAT     = 60.0 / BPM;   // ~0.833 s
constexpr double BAR      = BEAT * 4.0;   // ~3.333 s
constexpr double HALF_BAR = BAR * 0.5;

// ── Chord definitions — Cmaj7 / Fmaj7 / Am9 / Gsus2 ─────────────────────────
//  Each chord is defined by its pad voices, bass root, and sparse bell notes.
//
//  Cmaj7  = C3 E3 G3 B3  (C4 E4 for shimmer)
//  Fmaj7  = F3 A3 C4 E4
//  Am9    = A2 C3 E3 G3 B3
//  Gsus2  = G2 A2 D3 G3

struct ChordDef {
    double bass_root;                 // lowest sine bass note
    std::vector<double> pad_voices;   // detuned pad layers
    std::vector<double> bell_melody;  // sparse melody notes (one or two per bar)
};

// Helper: midi_to_freq lives in wauvio already but we alias it for clarity
static inline double mf(int m) noexcept { return midi_to_freq(m); }

// MIDI notes (C4 = 60):
//  C2=36  D2=38  E2=40  F2=41  G2=43  A2=45  B2=47
//  C3=48  D3=50  E3=52  F3=53  G3=55  A3=57  B3=59
//  C4=60  D4=62  E4=64  F4=65  G4=67  A4=69  B4=71
//  C5=72  D5=74  E5=76  F5=77  G5=79  A5=81  B5=83

static const ChordDef CHORDS[4] = {
    // Cmaj7  root=C2
    { mf(36),
      { mf(48), mf(52), mf(55), mf(59), mf(60), mf(64) },
      { mf(72), mf(76) } },
    // Fmaj7  root=F2
    { mf(41),
      { mf(53), mf(57), mf(60), mf(64), mf(65), mf(69) },
      { mf(69), mf(72) } },
    // Am9    root=A1=45-12=33
    { mf(33),
      { mf(45), mf(48), mf(52), mf(55), mf(59), mf(62) },
      { mf(76), mf(71) } },
    // Gsus2  root=G1=43-12=31
    { mf(31),
      { mf(43), mf(45), mf(50), mf(55), mf(62), mf(67) },
      { mf(74), mf(67) } },
};

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 1 — CHORD PAD
//  Technique: for each chord, render each voice as a Triangle oscillator with
//  slow ADSR (long attack), then blend into a wide stereo field via Haas + pan.
//  Chorus applied for extra shimmer.
// ─────────────────────────────────────────────────────────────────────────────

static StereoBuffer build_pad(double total_dur, int sr) {
    const size_t total_samples = static_cast<size_t>(total_dur * sr);
    StereoBuffer out(total_samples, 0.0f);

    // Per-voice: slightly detuned triangle, very slow attack
    Instrument voice_inst;
    voice_inst.shape    = WaveShape::Triangle;
    voice_inst.envelope = ADSR(BAR * 0.6, BAR * 0.3, 0.80, BAR * 0.8);
    voice_inst.gain     = 0.22f;

    // Reverb: large room for space
    Reverb rev;
    rev.room_size = 0.88f;
    rev.damping   = 0.40f;
    rev.wet       = 0.35f;
    rev.dry       = 1.0f;
    rev.init(sr);

    // Chorus: gentle shimmer
    Chorus cho;
    cho.rate_hz  = 0.18;
    cho.depth_ms = 3.5;
    cho.delay_ms = 10.0;
    cho.wet      = 0.40f;
    cho.dry      = 1.0f;
    cho.init(sr);

    const int repeats = static_cast<int>(total_dur / (BAR * 4.0) + 0.5);
    size_t offset = 0;

    // Chord duration = 1 bar each, but we allow a short overlap/crossfade
    // by rendering slightly longer than one bar so the long release bleeds
    const double chord_dur = BAR * 1.35;  // note longer than bar for long tail

    for (int rep = 0; rep < repeats; ++rep) {
        for (int ci = 0; ci < 4; ++ci) {
            const auto& chord = CHORDS[ci];
            const int   nv    = static_cast<int>(chord.pad_voices.size());

            for (int vi = 0; vi < nv; ++vi) {
                // Slight detune per voice for thickness (±4 cents alternating)
                const double detune_cents = (vi % 2 == 0) ? 0.0 : ((vi - nv/2) * 1.8);
                const double detune_ratio = std::pow(2.0, detune_cents / 1200.0);
                const double freq = chord.pad_voices[static_cast<size_t>(vi)] * detune_ratio;

                Buffer mono = voice_inst.render_note({freq, chord_dur, 0.85}, sr);

                // Apply chorus to each voice
                cho.process(mono);
                // Apply reverb
                rev.process(mono);

                // Pan voices across the stereo field evenly
                const float pan = (nv > 1)
                    ? -0.75f + 1.5f * static_cast<float>(vi) / static_cast<float>(nv - 1)
                    : 0.0f;

                const float angle = (pan + 1.0f) * static_cast<float>(PI) * 0.25f;
                const float gL    = std::cos(angle) * 0.85f;
                const float gR    = std::sin(angle) * 0.85f;

                const size_t csamp = mono.size();
                for (size_t i = 0; i < csamp && offset + i < total_samples; ++i) {
                    out.L[offset + i] += mono[i] * gL;
                    out.R[offset + i] += mono[i] * gR;
                }
            }
            offset += static_cast<size_t>(BAR * sr);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 2 — BELL LEAD MELODY
//  Technique: FM synthesis for a bell/glasslike timbre.
//  Notes are very sparse — one or two per bar, with long silences.
//  Long reverb + a dotted-quarter delay for shimmer.
// ─────────────────────────────────────────────────────────────────────────────

static Buffer build_bells(double total_dur, int sr) {
    const size_t total_samples = static_cast<size_t>(total_dur * sr);
    Buffer       out(total_samples, 0.0f);

    // Bell timbre via FMSynth: mod_ratio = 2.756, mod_index = 2.2 → glassy bell
    // Each note rendered individually with an exponential amplitude decay.
    const double note_dur   = BAR * 0.9;    // slightly shorter than bar
    const double note_decay = 5.5;          // exp decay rate (sec^-1)

    // Long reverb for ethereal tail
    Reverb rev;
    rev.room_size = 0.92f;
    rev.damping   = 0.30f;
    rev.wet       = 0.55f;
    rev.dry       = 0.80f;
    rev.init(sr);

    // Delay: dotted-quarter at 72 BPM ≈ 625 ms
    DelayLine delay;
    delay.time_ms  = BEAT * 1000.0 * 0.75;
    delay.feedback = 0.35f;
    delay.wet      = 0.30f;
    delay.dry      = 1.0f;
    delay.init(sr);

    const int repeats = static_cast<int>(total_dur / (BAR * 4.0) + 0.5);
    size_t offset = 0;

    for (int rep = 0; rep < repeats; ++rep) {
        for (int ci = 0; ci < 4; ++ci) {
            const auto& chord      = CHORDS[ci];
            const size_t n_bells   = chord.bell_melody.size();

            // Place first bell note at bar start, second at half-bar
            for (size_t bi = 0; bi < n_bells && bi < 2; ++bi) {
                const double freq    = chord.bell_melody[bi];
                const size_t n       = static_cast<size_t>(note_dur * sr);
                Buffer note_buf(n, 0.0f);

                // FM bell: carrier = freq, modulator = freq * 2.756
                FMSynth fm;
                fm.carrier_freq   = freq;
                fm.modulator_freq = freq * 2.756;
                fm.mod_index      = 2.2;
                fm.amplitude      = 0.55;

                Buffer fm_buf = fm.render(note_dur, sr);

                // Apply exponential amplitude decay (bell ring-out)
                for (size_t i = 0; i < n; ++i) {
                    const double t   = static_cast<double>(i) / sr;
                    const float  env = static_cast<float>(std::exp(-t * note_decay));
                    note_buf[i] = fm_buf[i] * env;
                }

                // Effects
                delay.process(note_buf);
                rev.process(note_buf);

                // Place into output: first note at bar start, second at half-bar
                const size_t note_offset = offset + static_cast<size_t>(bi * HALF_BAR * sr);
                for (size_t i = 0; i < n && note_offset + i < total_samples; ++i)
                    out[note_offset + i] += note_buf[i];
            }
            offset += static_cast<size_t>(BAR * sr);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 3 — SOFT SINE BASS
//  Very low, very quiet. Root pedal note holds for 2 bars, transitions gently.
//  Sine wave only, LPF to keep it rounded.
// ─────────────────────────────────────────────────────────────────────────────

static Buffer build_bass(double total_dur, int sr) {
    const size_t total_samples = static_cast<size_t>(total_dur * sr);
    Buffer out(total_samples, 0.0f);

    // Slow attack, slow release — the bass just breathes under the pad
    ADSR env(BAR * 0.5, BAR * 0.2, 0.65, BAR * 0.9);

    LowPassFilter lpf(180.0, sr);

    const int repeats = static_cast<int>(total_dur / (BAR * 4.0) + 0.5);
    size_t offset = 0;

    for (int rep = 0; rep < repeats; ++rep) {
        for (int ci = 0; ci < 4; ++ci) {
            const double freq = CHORDS[ci].bass_root;
            const double dur  = BAR * 1.20;  // allow gentle bleed/overlap
            const size_t n    = static_cast<size_t>(dur * sr);
            Buffer note(n, 0.0f);

            // Simple sine oscillator for bass
            double phase = 0.0;
            const double inc = TWO_PI * freq / sr;
            for (size_t i = 0; i < n; ++i) {
                note[i] = static_cast<float>(std::sin(phase) * 0.50);
                phase += inc;
                if (phase >= TWO_PI) phase -= TWO_PI;
            }

            env.apply(note, dur, dur - BAR * 0.9, sr);
            lpf.process(note);

            for (size_t i = 0; i < n && offset + i < total_samples; ++i)
                out[offset + i] += note[i];

            offset += static_cast<size_t>(BAR * sr);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 4 — ATMOSPHERE / AIR TEXTURE
//  HP-filtered pink-ish noise, modulated very slowly by a sine LFO for the
//  "breathing" quality. Kept very low in the mix.
// ─────────────────────────────────────────────────────────────────────────────

static Buffer build_atmosphere(double total_dur, int sr) {
    const size_t n = static_cast<size_t>(total_dur * sr);
    Buffer out(n, 0.0f);

    NoiseGenerator ng(0xBEEFu);
    HighPassFilter hp(1800.0, sr);
    LowPassFilter  lp(8000.0, sr);

    // Render noise
    for (size_t i = 0; i < n; ++i)
        out[i] = ng.tick() * 0.15f;

    hp.process(out);
    lp.process(out);

    // Amplitude modulation via very slow LFO (0.07 Hz — one breath every ~14 s)
    {
        double phase = 0.0;
        const double inc = TWO_PI * 0.07 / sr;
        for (size_t i = 0; i < n; ++i) {
            // LFO maps to [0.15 … 0.50] — never fully silent, never loud
            const float lfo = 0.325f + 0.175f * static_cast<float>(std::sin(phase));
            out[i] *= lfo;
            phase += inc;
            if (phase >= TWO_PI) phase -= TWO_PI;
        }
    }

    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 5 — SOFT PULSE / HEARTBEAT
//  A barely-audible sub-sine pulse at half the BPM (one hit every 2 beats).
//  Very short, very quiet, low-pitched — felt more than heard.
//  Adds a sense of gentle cosmic rhythm without being a "drum".
// ─────────────────────────────────────────────────────────────────────────────

static Buffer build_pulse(double total_dur, int sr) {
    const size_t total_samples = static_cast<size_t>(total_dur * sr);
    Buffer out(total_samples, 0.0f);

    // Sub-sine at C1 (32.7 Hz) with very fast decay
    const double freq    = midi_to_freq(24);  // C1 ≈ 32.7 Hz
    const double dur     = 0.40;              // 400 ms each pulse
    const size_t pn      = static_cast<size_t>(dur * sr);
    const double interval = BEAT * 2.0;       // every 2 beats

    Buffer pulse_template(pn, 0.0f);
    {
        double ph = 0.0;
        const double inc = TWO_PI * freq / sr;
        for (size_t i = 0; i < pn; ++i) {
            const double t   = static_cast<double>(i) / sr;
            const float  env = static_cast<float>(std::exp(-t * 12.0));
            pulse_template[i] = static_cast<float>(std::sin(ph)) * env * 0.18f;
            ph += inc;
            if (ph >= TWO_PI) ph -= TWO_PI;
        }
    }

    // Stamp at every interval
    double t = 0.0;
    while (t < total_dur - dur) {
        const size_t off = static_cast<size_t>(t * sr);
        for (size_t i = 0; i < pn && off + i < total_samples; ++i)
            out[off + i] += pulse_template[i];
        t += interval;
    }

    return out;
}

} // namespace detail

// =============================================================================
//  PUBLIC API
// =============================================================================

/// Generate a heavenly / ethereal ambient stereo track.
///
/// @param duration     Total duration in seconds (snapped to 4-bar multiples).
///                     Default 90 s.
/// @param sample_rate  0 = use wauvio::global_config().sample_rate.
/// @return             Normalised stereo buffer ready for save_wav_stereo / play.
inline StereoBuffer generate(double duration = 90.0, int sample_rate = 0) {
    using namespace detail;

    if (sample_rate <= 0)
        sample_rate = global_config().sample_rate;

    // Snap to exact 4-bar multiples for seamless looping
    const double bar4  = BAR * 4.0;
    const int    reps  = std::max(2, static_cast<int>(duration / bar4 + 0.5));
    const double total = bar4 * static_cast<double>(reps);

    // ── Render each layer ────────────────────────────────────────────────────
    StereoBuffer pad  = build_pad        (total, sample_rate);
    Buffer       bells= build_bells      (total, sample_rate);
    Buffer       bass = build_bass       (total, sample_rate);
    Buffer       atmo = build_atmosphere (total, sample_rate);
    Buffer       pulse= build_pulse      (total, sample_rate);

    // ── Mix everything onto the master bus ───────────────────────────────────
    MasterBus bus(sample_rate);

    bus.schedule(std::move(pad),   0.0, 0.70f);   // pad stereo — primary carrier
    bus.schedule(std::move(bells), 0.0, 0.20f);   // bells centred  (mono → both ch)
    bus.schedule(std::move(bass),  0.0, 0.60f);   // bass centred
    bus.schedule(std::move(atmo),  0.0, 0.25f);   // atmosphere centred
    bus.schedule(std::move(pulse), 0.0, 0.55f);   // sub-pulse centred

    StereoBuffer mix = bus.mix_stereo();

    // ── Pan the bell lead very slightly right for spatial interest ───────────
    // (Already placed centred, but we can tilt the mix slightly in post.)
    // We achieve this by applying gentle L/R asymmetry on the mix itself:
    // subtle Haas-like treatment of the full mix is too heavy — instead
    // we just gently raise R by 0.5 dB to breathe space. Not audible as
    // a discrete effect, just a subliminal openness.
    for (size_t i = 0; i < mix.size(); ++i)
        mix.R[i] *= 1.06f;

    // ── Very long fade-in / fade-out for the drifting feel ───────────────────
    fade_in (mix, 3.0,  sample_rate);   // 3 s slow sunrise
    fade_out(mix, 6.0,  sample_rate);   // 6 s long sunset

    // ── Normalise to 0.80 — keep headroom and preserve soft dynamics ─────────
    normalize(mix, 0.80f);

    return mix;
}

} // namespace heavenly
} // namespace soundtrack
} // namespace wauvio
