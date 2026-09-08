// =============================================================================
//  W A U V I O _ S O U N D T R A C K _ U N S E T T L I N G . H P P
//
//  Standalone psychologically unsettling soundtrack module.
//  Depends on wauvio.hpp ONLY.
//
//  Theme  : Something is wrong. Not loud, not obviously frightening.
//           Just: wrong. The proportions are off. The intervals don't
//           resolve. The timing is almost right but never quite is.
//           You keep expecting something to happen. It doesn't.
//           Or it does, but not when or how you expected.
//
//  Namespace : wauvio::soundtrack::unsettling
//  Entry point:
//      wauvio::Buffer wauvio::soundtrack::unsettling::generate(int sample_rate = 0);
//
//  Techniques deployed:
//    1. MICROTONAL DETUNING
//       Frequencies are offset from standard 12-TET by fractions of a
//       semitone using a slow random walk — creating harmonic relationships
//       that no key-signature resolves.
//
//    2. IRREGULAR TIMING
//       All events are scheduled via a jittered timeline where the period
//       is never constant.  No two "bars" have the same length.
//
//    3. FM WITH UNSTABLE MODULATION INDEX
//       The FM mod_index itself is modulated by a sub-audio oscillator,
//       causing the timbre to mutate unpredictably.
//
//    4. AMPLITUDE FLICKER
//       Multiple layers have their gain controlled by chaotic functions
//       (product of incommensurable sinusoids) so beats and interference
//       emerge quasi-randomly.
//
//    5. LOW-FREQUENCY RUMBLE
//       Sub-bass sinusoids (18–45 Hz) create physical discomfort that
//       bypasses conscious processing.
//
//    6. HIGH-FREQUENCY INTRUSIONS
//       Occasional very short high-pitched tones appear above 3 kHz —
//       too brief to track consciously, but registering as wrong.
//
//    7. PHASE DRIFT
//       Two near-identical oscillators at irrational frequency ratios
//       slowly drift in phase, causing comb filtering that never repeats.
//
//    8. DISSONANT INTERVALS
//       Intervals used: minor 2nd (semitone), tritone (augmented 4th),
//       minor 9th — all maximally dissonant in the harmonic series.
//
//    9. NON-REPEATING STRUCTURE
//       The PRNG seed changes each layer independently; no two events
//       align to form a recognisable pattern.
//
//  Structure  (~90 seconds, non-sectional — continuous evolution):
//    The piece has no obvious "intro/verse/chorus".
//    Instead it passes through five loosely-defined psychological states:
//      [00:00–00:18]  Stillness  — near-silence, barely perceptible hum
//      [00:18–00:38]  Intrusion  — something enters, the space changes
//      [00:38–01:00]  Dread      — layering intensifies, dissonance builds
//      [01:00–01:18]  Peak       — everything wrong at once
//      [01:18–01:30]  Recession  — withdrawal, but the hum stays
// =============================================================================

#pragma once
#include "wauvio.hpp"

namespace wauvio::soundtrack::unsettling {

namespace detail {

// ---------------------------------------------------------------------------
//  PRNG  — xorshift32, same as other soundtrack modules for consistency
// ---------------------------------------------------------------------------
struct PRNG {
    uint32_t state;
    explicit PRNG(uint32_t seed = 0xDEADFACEu) : state(seed) {}
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    double randf(double lo, double hi) {
        return lo + (static_cast<double>(next()) / 4294967295.0) * (hi - lo);
    }
    int randi(int lo, int hi) {
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }
};

// ---------------------------------------------------------------------------
//  MICROTONAL PITCH TABLE
//  Standard frequencies shifted by fractional semitones using a
//  deterministic drift function.  The drift is slow enough that the
//  listener cannot adjust to a new tonic — the ground keeps moving.
//
//  drift(t) = base_freq × 2^(semitone_offset(t) / 12)
//  semitone_offset(t) = A·sin(ω₁t) + B·sin(ω₂t) where ω₁/ω₂ is irrational
// ---------------------------------------------------------------------------
inline double microtonal_freq(double base_hz, double t,
                               double depth_semitones = 0.22)
{
    // Two incommensurable drift oscillators
    const double drift = depth_semitones
        * (0.6 * std::sin(TWO_PI * 0.031 * t)       // 0.031 Hz ≈ 32 s period
         + 0.4 * std::sin(TWO_PI * 0.031 * PI * t)); // irrational ratio
    return base_hz * std::pow(2.0, drift / 12.0);
}

// ---------------------------------------------------------------------------
//  SUB-BASS RUMBLE
//  Sine oscillator below 40 Hz — felt more than heard.
//  Its amplitude is modulated by a chaotic three-component oscillator.
// ---------------------------------------------------------------------------
inline Buffer sub_rumble(double base_hz, double duration, int sample_rate) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);

    double phase  = 0.0;
    double ph_m1  = 0.0, ph_m2  = 0.0, ph_m3  = 0.0;
    const double m1 = TWO_PI * 0.077 / sample_rate;  // chaos modulator 1
    const double m2 = TWO_PI * 0.131 / sample_rate;  // chaos modulator 2
    const double m3 = TWO_PI * 0.197 / sample_rate;  // chaos modulator 3

    LowPassFilter lpf(60.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t    = static_cast<double>(i) / sample_rate;
        const double freq = microtonal_freq(base_hz, t, 0.15);
        phase += TWO_PI * freq / sample_rate;

        // Chaotic amplitude: product of three slow oscillators
        const double amp =
              (0.5 + 0.5 * std::sin(ph_m1))
            * (0.5 + 0.5 * std::sin(ph_m2))
            * (0.4 + 0.6 * std::abs(std::sin(ph_m3)));

        buf[i] = lpf.tick(static_cast<float>(std::sin(phase) * amp * 0.70));

        ph_m1 += m1;  ph_m2 += m2;  ph_m3 += m3;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  UNSTABLE FM TONE
//  Carrier and modulator frequencies both drift microtonally.
//  The mod_index is itself modulated by a slow sub-audio oscillator —
//  the timbre morphs continuously between sine-like and complex/metallic.
// ---------------------------------------------------------------------------
inline Buffer unstable_fm(double carrier_base, double mod_ratio,
                           double duration, float gain, int sample_rate)
{
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);

    double c_phase  = 0.0;
    double m_phase  = 0.0;
    double mi_phase = 0.0;  // modulation-index oscillator phase

    // mod_index oscillates between 0.3 and 4.8 at ~0.07 Hz
    const double mi_inc = TWO_PI * 0.07 / sample_rate;

    // Amplitude flicker: two incommensurable oscillators
    double fl1 = 0.0, fl2 = 0.0;
    const double fl1_inc = TWO_PI * 0.19 / sample_rate;  // ~0.19 Hz
    const double fl2_inc = TWO_PI * 0.31 / sample_rate;  // ~0.31 Hz

    LowPassFilter lpf(3500.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sample_rate;

        // Microtonal drift on both carrier and modulator
        const double c_freq = microtonal_freq(carrier_base, t, 0.18);
        const double m_freq = microtonal_freq(carrier_base * mod_ratio, t * 1.3, 0.22);

        const double mod_index = 0.3 + 4.5 * (0.5 + 0.5 * std::sin(mi_phase));
        const double mod       = mod_index * std::sin(m_phase);

        // Chaotic amplitude envelope
        const double amp_fl = (0.5 + 0.5 * std::sin(fl1))
                            * (0.5 + 0.5 * std::sin(fl2));

        buf[i] = lpf.tick(static_cast<float>(
            gain * std::sin(c_phase + mod) * amp_fl));

        c_phase  += TWO_PI * c_freq / sample_rate;
        m_phase  += TWO_PI * m_freq / sample_rate;
        mi_phase += mi_inc;
        fl1      += fl1_inc;
        fl2      += fl2_inc;

        if (c_phase  >= TWO_PI) c_phase  -= TWO_PI;
        if (m_phase  >= TWO_PI) m_phase  -= TWO_PI;
        if (mi_phase >= TWO_PI) mi_phase -= TWO_PI;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  PHASE-DRIFT PAIR
//  Two near-identical oscillators at a frequency ratio of √2 (irrational).
//  As they drift in phase, they create a slowly-evolving interference
//  pattern — constructive and destructive interference with no period.
// ---------------------------------------------------------------------------
inline Buffer phase_drift_pair(double base_hz, double duration,
                                float gain, int sample_rate)
{
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);

    double phA = 0.0, phB = 0.0;
    const double incA = TWO_PI * base_hz / sample_rate;
    const double incB = TWO_PI * (base_hz * 1.4142135623) / sample_rate; // × √2

    LowPassFilter lpf(1800.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const float s = static_cast<float>(std::sin(phA) + std::sin(phB));
        buf[i] = lpf.tick(s * gain * 0.5f);
        phA += incA;  phB += incB;
        if (phA >= TWO_PI) phA -= TWO_PI;
        if (phB >= TWO_PI) phB -= TWO_PI;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  DISSONANT CLUSTER
//  A tight cluster of three pitches: root, root+semitone, root+tritone.
//  All three drift microtonally at different rates — the dissonance
//  never quite settles and never quite resolves.
// ---------------------------------------------------------------------------
inline Buffer dissonant_cluster(double root_hz, double duration,
                                 float gain, int sample_rate)
{
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n, 0.0f);

    // Three oscillators with independent microtonal drift seeds
    struct Voice { double base; double drift_mult; double phase; };
    Voice voices[3] = {
        { root_hz,                          1.00, 0.0 },
        { root_hz * std::pow(2.0, 1.0/12.0), 1.13, 0.0 }, // minor 2nd
        { root_hz * std::pow(2.0, 6.0/12.0), 0.91, 0.0 }, // tritone
    };

    LowPassFilter lpf(2200.0, sample_rate);

    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sample_rate;
        float sum = 0.0f;

        for (auto& v : voices) {
            const double freq = microtonal_freq(v.base, t * v.drift_mult, 0.30);
            v.phase += TWO_PI * freq / sample_rate;
            if (v.phase >= TWO_PI) v.phase -= TWO_PI;
            sum += static_cast<float>(std::sin(v.phase)) * (gain / 3.0f);
        }
        buf[i] = lpf.tick(sum);
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  HIGH-FREQUENCY INTRUSION
//  A short, piercing sine tone at 3–6 kHz — lasts 50–200 ms.
//  Placed irregularly.  Not loud enough to feel like a scare event —
//  just present enough to unsettle without being identifiable.
// ---------------------------------------------------------------------------
inline Buffer intrusion(double freq_hz, double duration,
                         float gain, int sample_rate)
{
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t   = static_cast<double>(i) / sample_rate;
        phase += TWO_PI * freq_hz / sample_rate;
        // Very fast attack, quick exponential decay
        const float env = static_cast<float>(std::exp(-t * (1.0 / duration) * 6.0));
        buf[i] = static_cast<float>(std::sin(phase)) * env * gain;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  FILTERED BREATH  — unpredictably gated noise
//  White noise filtered to a 200–800 Hz band, gated by an aperiodic
//  amplitude function.  Sounds like shallow, irregular breathing.
// ---------------------------------------------------------------------------
inline Buffer filtered_breath(double duration, int sample_rate, uint32_t seed) {
    const size_t n = static_cast<size_t>(duration * sample_rate);
    Buffer buf(n);

    NoiseGenerator ng(seed);
    LowPassFilter  lpf(800.0,  sample_rate);
    HighPassFilter hpf(200.0,  sample_rate);

    // Gate: product of three incommensurable oscillators, mostly OFF
    double ph1 = 0.0, ph2 = 0.0, ph3 = 0.0;
    const double g1 = TWO_PI * 0.041 / sample_rate;
    const double g2 = TWO_PI * 0.079 / sample_rate;
    const double g3 = TWO_PI * 0.127 / sample_rate;

    for (size_t i = 0; i < n; ++i) {
        // Gate output: biased so it's mostly closed (near 0)
        const double g = std::max(0.0,
            std::sin(ph1) * std::sin(ph2) * std::cos(ph3));
        const float gate = static_cast<float>(std::pow(g, 3.0));

        buf[i] = hpf.tick(lpf.tick(ng.tick())) * gate * 0.35f;
        ph1 += g1;  ph2 += g2;  ph3 += g3;
    }
    return buf;
}

// ---------------------------------------------------------------------------
//  SUDDEN DYNAMIC SHIFT  — a short silence-then-loud event
//  Places a quiet period (amplitude near zero) followed immediately by a
//  transient burst.  The human auditory system finds this deeply jarring
//  because it sets up an expectation of "quiet = safe" and violates it.
// ---------------------------------------------------------------------------
inline Buffer sudden_shift(double quiet_dur, double burst_dur,
                            int sample_rate, uint32_t seed)
{
    const size_t qn = static_cast<size_t>(quiet_dur * sample_rate);
    const size_t bn = static_cast<size_t>(burst_dur * sample_rate);
    Buffer buf(qn + bn, 0.0f);

    // Burst: dense FM cluster
    NoiseGenerator ng(seed);
    LowPassFilter  lpf(1200.0, sample_rate);
    double phase = 0.0;

    for (size_t i = 0; i < bn; ++i) {
        const double t    = static_cast<double>(i) / sample_rate;
        const double freq = 220.0 * std::exp(-t * 8.0) + 60.0;
        phase += TWO_PI * freq / sample_rate;
        const float noise_comp = ng.tick() * 0.4f;
        const float tone_comp  = static_cast<float>(std::sin(phase)) * 0.6f;
        const float env        = static_cast<float>(std::exp(-t * 12.0));
        buf[qn + i] = lpf.tick(noise_comp + tone_comp) * env * 0.85f;
    }
    return buf;
}

} // namespace detail

// =============================================================================
//  PUBLIC ENTRY POINT
// =============================================================================

inline Buffer generate(int sample_rate = 0) {
    if (sample_rate <= 0) sample_rate = global_config().sample_rate;

    // Total: ~90 seconds (non-sectional — continuous evolution)
    constexpr double TOTAL = 90.0;

    MasterBus bus(sample_rate);
    detail::PRNG rng(0xF00DCAFE);

    // ─────────────────────────────────────────────────────────────────────
    //  STATE 0  [0:00–0:18]  STILLNESS
    //  Near silence.  A sub-sonic rumble just below perception.
    //  A faint phase-drift pair at ~60 Hz.
    // ─────────────────────────────────────────────────────────────────────
    {
        // Sub-bass rumble (barely audible — 28 Hz)
        Buffer sub = detail::sub_rumble(28.0, 18.0, sample_rate);
        fade_in(sub, 6.0, sample_rate);
        bus.schedule(sub, 0.0, 0.35f);

        // Phase-drift pair: 55 Hz, soft
        Buffer pdp = detail::phase_drift_pair(55.0, 18.0, 0.18f, sample_rate);
        fade_in(pdp, 8.0, sample_rate);
        bus.schedule(pdp, 0.0);

        // A single intrusion at second 12 — very brief
        Buffer intr = detail::intrusion(
            3200.0 + rng.randf(-200, 200), 0.07, 0.28f, sample_rate);
        bus.schedule(intr, 12.0 + rng.randf(-0.5, 0.5));
    }

    // ─────────────────────────────────────────────────────────────────────
    //  STATE 1  [0:18–0:38]  INTRUSION
    //  Something enters.  An unstable FM tone at an ambiguous pitch.
    //  Irregular breathing textures.
    //  The sub-bass continues; a dissonant cluster begins forming.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = 18.0;

        // Sub-bass continues: different pitch (36 Hz — minor 9th above 28 Hz)
        Buffer sub2 = detail::sub_rumble(36.0, 20.0, sample_rate);
        fade_in(sub2, 5.0, sample_rate);
        bus.schedule(sub2, t0, 0.40f);

        // Unstable FM: carrier at ~110 Hz (A2), mod ratio 1.41 (√2 — maximally irrational)
        Buffer fm1 = detail::unstable_fm(110.0, 1.41, 20.0, 0.45f, sample_rate);
        fade_in(fm1, 4.0, sample_rate);
        bus.schedule(fm1, t0);

        // Filtered breath — irregular gated noise
        Buffer br1 = detail::filtered_breath(20.0, sample_rate, 0x1A2B3C4Du);
        bus.schedule(br1, t0, 0.60f);

        // Phase drift at 82 Hz (E2)
        Buffer pd2 = detail::phase_drift_pair(82.41, 20.0, 0.20f, sample_rate);
        fade_in(pd2, 6.0, sample_rate);
        bus.schedule(pd2, t0);

        // Dissonant cluster begins at t=25 (7 seconds in)
        Buffer dc1 = detail::dissonant_cluster(82.41, 13.0, 0.22f, sample_rate);
        fade_in(dc1, 5.0, sample_rate);
        bus.schedule(dc1, t0 + 7.0);

        // Sporadic intrusions — 3–4 events at jittered timings
        for (int k = 0; k < 4; ++k) {
            const double onset  = t0 + rng.randf(2.0, 19.0);
            const double freq   = rng.randf(2800.0, 5200.0);
            const double dur    = rng.randf(0.05, 0.18);
            const float  gain   = static_cast<float>(rng.randf(0.15, 0.32));
            bus.schedule(detail::intrusion(freq, dur, gain, sample_rate), onset);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  STATE 2  [0:38–1:00]  DREAD
    //  All layers active.  FM complexity increases.
    //  A second FM voice enters at a tritone interval.
    //  Sudden shift event planted at ~0:52.
    //  More intrusions, closer together.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = 38.0;
        const double dur = 22.0;

        // Primary sub rumble (now 32 Hz)
        Buffer sub3 = detail::sub_rumble(32.0, dur, sample_rate);
        fade_in(sub3, 3.0, sample_rate);
        bus.schedule(sub3, t0, 0.55f);

        // FM voice 1: 110 Hz (same as before — familiarity used against listener)
        Buffer fm2 = detail::unstable_fm(110.0, 1.41, dur, 0.50f, sample_rate);
        bus.schedule(fm2, t0);

        // FM voice 2: 155.56 Hz (Eb3) — tritone above A2. Pure dissonance.
        Buffer fm3 = detail::unstable_fm(155.56, 1.73, dur, 0.38f, sample_rate);
        fade_in(fm3, 5.0, sample_rate);
        bus.schedule(fm3, t0 + 3.0);

        // Dissonant cluster: root 110 Hz now (tritone from Eb)
        Buffer dc2 = detail::dissonant_cluster(110.0, dur, 0.28f, sample_rate);
        bus.schedule(dc2, t0);

        // Phase drift at 73.42 Hz (D2) — adds beating against the 82 Hz from before
        Buffer pd3 = detail::phase_drift_pair(73.42, dur, 0.22f, sample_rate);
        bus.schedule(pd3, t0);

        // Filtered breath continues, slightly louder
        Buffer br2 = detail::filtered_breath(dur, sample_rate, 0x5E6F7A8Bu);
        bus.schedule(br2, t0, 0.65f);

        // SUDDEN SHIFT event at ~0:52 (14 seconds into this section)
        // Brief silence of 0.4s then burst
        Buffer ss = detail::sudden_shift(0.4, 0.6, sample_rate, 0xBADC0FFEu);
        bus.schedule(ss, t0 + 14.0, 0.80f);

        // Intrusions — now tighter clustering (5–7 events)
        for (int k = 0; k < 6; ++k) {
            const double onset = t0 + rng.randf(0.5, dur - 1.0);
            const double freq  = rng.randf(2200.0, 6500.0);
            const double d     = rng.randf(0.04, 0.25);
            const float  gain  = static_cast<float>(rng.randf(0.18, 0.40));
            bus.schedule(detail::intrusion(freq, d, gain, sample_rate), onset);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  STATE 3  [1:00–1:18]  PEAK
    //  Maximum layering.  Everything wrong at once.
    //  A low-frequency square-wave pulse stutters at an irregular tempo.
    //  Multiple FM voices. Dissonant clusters on two pitch centers.
    //  The rumble is felt, not heard.
    //  High-frequency intrusions become almost too frequent.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = 60.0;
        const double dur = 18.0;

        // Sub-bass: two layers at minor 9th interval
        bus.schedule(detail::sub_rumble(24.5, dur, sample_rate), t0, 0.60f);
        bus.schedule(detail::sub_rumble(38.0, dur, sample_rate), t0, 0.45f);

        // FM triad — three voices, all drifting
        bus.schedule(detail::unstable_fm(110.0, 1.41, dur, 0.45f, sample_rate), t0);
        bus.schedule(detail::unstable_fm(155.56, 1.73, dur, 0.42f, sample_rate), t0);
        bus.schedule(detail::unstable_fm( 77.78, 2.00, dur, 0.35f, sample_rate), t0 + 1.5);

        // Phase drift on two pitch centers
        bus.schedule(detail::phase_drift_pair( 55.0, dur, 0.25f, sample_rate), t0);
        bus.schedule(detail::phase_drift_pair(116.54, dur, 0.20f, sample_rate), t0 + 0.8);

        // Two dissonant clusters
        bus.schedule(detail::dissonant_cluster(110.0, dur, 0.30f, sample_rate), t0);
        bus.schedule(detail::dissonant_cluster( 73.42, dur, 0.25f, sample_rate), t0 + 2.0);

        // Irregular square-wave pulse at low frequency (30 Hz stuttering)
        // — rendered manually with irregular timing
        {
            detail::PRNG prng2(0xC0FFEE11u);
            double cursor = t0;
            while (cursor < t0 + dur - 1.0) {
                const double pulse_dur = prng2.randf(0.04, 0.22);
                Buffer pb(static_cast<size_t>(pulse_dur * sample_rate));
                double ph = 0.0;
                for (size_t i = 0; i < pb.size(); ++i) {
                    ph += TWO_PI * 30.0 / sample_rate;
                    pb[i] = wave::square(ph) * 0.30f;
                }
                LowPassFilter lpf(80.0, sample_rate);
                lpf.process(pb);
                bus.schedule(pb, cursor, 0.70f);
                cursor += pulse_dur + prng2.randf(0.08, 0.65);  // irregular gap
            }
        }

        // Dense intrusions — clusters of 2–3
        for (int k = 0; k < 12; ++k) {
            const double base_onset = t0 + rng.randf(0.0, dur - 0.5);
            const int cluster_size  = rng.randi(1, 3);
            for (int c = 0; c < cluster_size; ++c) {
                const double onset = base_onset + c * rng.randf(0.08, 0.30);
                const double freq  = rng.randf(1800.0, 7000.0);
                const double d     = rng.randf(0.04, 0.20);
                const float  gain  = static_cast<float>(rng.randf(0.20, 0.45));
                if (onset < t0 + dur)
                    bus.schedule(detail::intrusion(freq, d, gain, sample_rate), onset);
            }
        }

        // Two sudden shifts
        bus.schedule(detail::sudden_shift(0.3, 0.5, sample_rate, 0xDEAD1234u),
                     t0 + 6.0, 0.85f);
        bus.schedule(detail::sudden_shift(0.6, 0.4, sample_rate, 0xBEEF5678u),
                     t0 + 13.5, 0.80f);

        // Heavy breath texture
        Buffer br3 = detail::filtered_breath(dur, sample_rate, 0x9A0B1C2Du);
        bus.schedule(br3, t0, 0.70f);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  STATE 4  [1:18–1:30]  RECESSION
    //  Layers drop away one by one.
    //  But the sub-bass hum and phase-drift pair remain until the end.
    //  One final intrusion at ~1:27 — then silence.
    // ─────────────────────────────────────────────────────────────────────
    {
        const double t0 = 78.0;
        const double dur = 12.0;

        // Sub-bass: slow fade out
        Buffer sub_end = detail::sub_rumble(28.0, dur, sample_rate);
        fade_out(sub_end, dur * 0.7, sample_rate);
        bus.schedule(sub_end, t0, 0.45f);

        // One last FM voice, fading
        Buffer fm_end = detail::unstable_fm(110.0, 1.41, dur, 0.35f, sample_rate);
        fade_out(fm_end, dur * 0.6, sample_rate);
        bus.schedule(fm_end, t0);

        // Phase drift: persists, fades last
        Buffer pd_end = detail::phase_drift_pair(55.0, dur, 0.22f, sample_rate);
        fade_out(pd_end, dur * 0.85, sample_rate);
        bus.schedule(pd_end, t0);

        // Final dissonant cluster, almost inaudible
        Buffer dc_end = detail::dissonant_cluster(82.41, dur * 0.7, 0.14f, sample_rate);
        fade_out(dc_end, dur * 0.5, sample_rate);
        bus.schedule(dc_end, t0);

        // Sparse intrusions
        for (int k = 0; k < 3; ++k) {
            const double onset = t0 + rng.randf(1.0, dur - 2.0);
            const float  gain  = static_cast<float>(rng.randf(0.12, 0.22));
            const double freq  = rng.randf(2000.0, 4500.0);
            bus.schedule(detail::intrusion(freq, 0.08, gain, sample_rate), onset);
        }

        // FINAL intrusion: precisely at t=87 — one last wrong note before silence
        bus.schedule(detail::intrusion(
            3732.0,   // B♭5 — maximally inconclusive
            0.15, 0.35f, sample_rate),
            87.0);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  CONTINUITY LAYER  (runs the entire piece)
    //  A phase-drift pair at 41 Hz — just above infrasound.
    //  Never louder than -20 dB.  Never absent.
    //  This is the foundation of the discomfort.
    // ─────────────────────────────────────────────────────────────────────
    {
        Buffer cont = detail::phase_drift_pair(41.0, TOTAL, 0.15f, sample_rate);
        fade_in (cont, 10.0, sample_rate);
        fade_out(cont, 8.0,  sample_rate);
        bus.schedule(cont, 0.0);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  FINAL MIX
    // ─────────────────────────────────────────────────────────────────────
    Buffer master = bus.mix();
    clamp_buffer(master);
    // Normalise quieter than other soundtracks — the quiet makes it worse
    normalize(master, 0.72f);
    fade_in (master, 0.5,  sample_rate);
    fade_out(master, 5.0,  sample_rate);
    return master;
}

} // namespace wauvio::soundtrack::unsettling
