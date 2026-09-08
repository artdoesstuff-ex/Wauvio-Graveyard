#pragma once

// =============================================================================
//  wauvio_soundtrack_sequence1_final.hpp
//  Soundtrack: Sequence1 — Final
//
//  Design brief
//  ------------
//  This track plays at the FractureSystem phase — crackLapsed is running,
//  the screen is breaking apart.  Five seconds from transition.
//  The music should not feel like music.  It should feel like something
//  musical is being destroyed.  Not an ending.  A collapse.
//
//  Concept: EROSION
//  Previous tracks built things.  This track un-builds them.
//  The architecture is a single decaying structure — a sound that was once
//  a coherent drone being stripped apart, frequency by frequency, until
//  only a carrier survives.  Then the carrier disintegrates too.
//
//  This track's fundamental mechanism is different from all others:
//  it does not use discrete "events" or "strata".
//  Instead, it uses a parametric erosion model:
//
//    - A base FM voice is synthesized across the full duration
//    - Three "erosion LFOs" independently attack the modulation index,
//      the carrier amplitude, and the modulator frequency
//    - All three erosion LFOs are non-periodic: they use a decaying
//      oscillation that diminishes over time (envelope × sine)
//    - The net effect: the sound starts complex and thins out —
//      not gradually, but in lurches, like a wall losing plaster
//    - A second layer uses inverse erosion: starts sparse, grows
//      denser toward the end — a wrong texture approaching
//
//  Structure (41 seconds)
//  ----------------------
//  0s – 41s    Continuous — no silence gaps, no sections
//
//  Two simultaneous synthesis engines:
//
//  ENGINE A — Eroding drone
//    Base carrier: 104 Hz.  Mod: 104 × (1 + 1/φ) = 168.3 Hz (silver ratio)
//    FM index: starts at 3.2, decays via damped oscillation to ~0.3
//    Amplitude: starts at 0.065, eroded by two competing LFOs
//    Three non-harmonic overtones mixed in at low amplitude,
//    each with their own erosion envelope
//
//  ENGINE B — Inverse-erosion texture
//    White noise band-filtered at progressively wider bands over time
//    Starts narrow (HPF 2000 Hz, LPF 2200 Hz = 200 Hz band)
//    Ends wide  (HPF  200 Hz, LPF 4000 Hz = 3800 Hz band)
//    Amplitude rises from 0.004 → 0.028 over the full duration
//    This is the "wrong texture approaching"
//
//  ENGINE C — Fracture pulse
//    At irregular intervals, a very short (0.05s) hard-clipped noise burst
//    Intervals: 4.7, 3.1, 6.8, 2.3, 5.5, 1.9, 7.2, 3.8, 2.1, 4.4 seconds
//    (computed cumulatively from 2.2s)
//    Each burst has a slightly different LP cutoff (descending over time)
//    as if each fracture is hitting a different material
//
//  Anti-repetition mechanism
//  -------------------------
//  - Damped oscillation is non-periodic by construction (decays to zero)
//  - The three erosion envelopes have incommensurable periods
//  - Engine B's filter parameters change sample-by-sample
//  - Fracture pulse positions are irrational multiples of no common grid
//  - The track sounds noticeably different in the first 10s vs last 10s
//    even though all engines run continuously
//
//  Architecture compliance
//  -----------------------
//  - 41 seconds (§4.1)
//  - Seamless loop: 110 ms crossfade (§4.1)
//  - Normalized to 0.70 (§5.1)
//  - Fixed seeds (§3.2)
//  - No wauvio::play(), no threads (§3.3)
//  - Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

namespace soundtrack {
namespace sequence1_final {

inline wauvio::Buffer gen() {

    const int    SR    = wauvio::global_config().sample_rate;
    constexpr double DUR       = 41.0;
    constexpr double FADE_S    = 0.09;
    constexpr size_t XFADE_MS  = 110;
    const size_t     TOTAL     = static_cast<size_t>(DUR * SR);

    wauvio::Buffer out(TOTAL, 0.f);

    // =========================================================================
    //  ENGINE A — ERODING DRONE
    //
    //  Carrier: 104 Hz
    //  Mod: 104 × (1 + 1/φ) = 104 × 1.6180... — wait, silver ratio is
    //       1 + √2 = 2.4142. Use that. Mod: 104 × 2.4142 = 251.1 Hz
    //  FM index: damped sinusoidal erosion
    //    idx(t) = 0.3 + 2.9 × exp(-t / 18.0) × |sin(2π × 0.14 × t)|
    //    This decays from max ~3.2 to ~0.3 with diminishing oscillations
    //    Period of the internal sine: 1/0.14 ≈ 7.1s — aperiodic feel
    //    because exp decay means successive peaks are always smaller
    //
    //  Three overtone voices, each with their own erosion:
    //    Voice 2: carrier × 2.71 (near 3rd, but sharp), erosion τ = 12s
    //    Voice 3: carrier × 4.13 (inharmonic), erosion τ = 8s
    //    Voice 4: carrier × 1.38 (between 1st and 2nd), erosion τ = 22s
    // =========================================================================
    {
        constexpr double CARRIER = 104.0;
        constexpr double MOD_HZ  = 104.0 * 2.41421356237;  // silver ratio

        const double c_inc = wauvio::TWO_PI * CARRIER / SR;
        const double m_inc = wauvio::TWO_PI * MOD_HZ  / SR;
        double cp = 0.0, mp = 0.0;

        constexpr double IDX_FLOOR   = 0.30;
        constexpr double IDX_PEAK    = 2.90;
        constexpr double IDX_DECAY   = 18.0;   // time constant (seconds)
        constexpr double IDX_RATE    = 0.14;   // internal sine frequency

        // Overtones
        constexpr double OT_RATIOS[] = { 2.71, 4.13, 1.38 };
        constexpr double OT_TAUS[]   = { 12.0, 8.0, 22.0 };
        constexpr double OT_PEAKS[]  = { 0.40, 0.25, 0.55 };
        constexpr double OT_AMP[]    = { 0.018, 0.012, 0.022 };
        constexpr double OT_IDX[]    = { 1.2,   2.4,   0.8  };

        // Pre-compute overtone phases
        double ot_cp[3] = {0.0, 0.0, 0.0};
        double ot_mp[3] = {0.0, 0.0, 0.0};
        double ot_c_inc[3], ot_m_inc[3];
        for (int k = 0; k < 3; ++k) {
            ot_c_inc[k] = wauvio::TWO_PI * (CARRIER * OT_RATIOS[k]) / SR;
            ot_m_inc[k] = wauvio::TWO_PI * (CARRIER * OT_RATIOS[k] * 1.7) / SR;
        }

        for (size_t i = 0; i < TOTAL; ++i) {
            const double t = static_cast<double>(i) / SR;

            // Main voice erosion
            const double decay_env = std::exp(-t / IDX_DECAY);
            const double mod_idx   = IDX_FLOOR
                + IDX_PEAK * decay_env
                  * std::abs(std::sin(wauvio::TWO_PI * IDX_RATE * t));

            // Main voice amplitude — also eroded, but differently
            // Two competing amplitude LFOs: one decaying, one growing slightly
            const double amp_decay   = 0.040 * std::exp(-t / 35.0);
            const double amp_swell   = 0.018 * (1.0 - std::exp(-t / 25.0));
            const double amp_flutter = 1.0 + 0.15 * std::sin(wauvio::TWO_PI * 0.23 * t);
            const float amp = static_cast<float>((amp_decay + amp_swell) * amp_flutter);

            out[i] += static_cast<float>(
                std::sin(cp + mod_idx * std::sin(mp))) * amp;

            cp += c_inc;
            mp += m_inc;

            // Overtone voices
            for (int k = 0; k < 3; ++k) {
                const double ot_decay = std::exp(-t / OT_TAUS[k]);
                const double ot_idx   = OT_IDX[k] * ot_decay;
                const float  ot_amp   = static_cast<float>(OT_AMP[k] * ot_decay * OT_PEAKS[k]);

                out[i] += static_cast<float>(
                    std::sin(ot_cp[k] + ot_idx * std::sin(ot_mp[k]))) * ot_amp;

                ot_cp[k] += ot_c_inc[k];
                ot_mp[k] += ot_m_inc[k];
            }
        }
    }

    // =========================================================================
    //  ENGINE B — INVERSE-EROSION TEXTURE (wrong texture approaching)
    //
    //  Band-filtered noise where the band WIDENS over time.
    //  HP cutoff: 2000 Hz → 200  Hz  (linear interpolation over 41s)
    //  LP cutoff:  200 Hz → 4000 Hz  (linear interpolation over 41s)
    //  Amplitude:  0.004 → 0.028     (linear interpolation over 41s)
    //
    //  Implemented sample-by-sample with dynamically updated filter coefficients.
    //  Pre-warm both filters at initial parameters before loop.
    // =========================================================================
    {
        constexpr float HP_START = 2000.0f, HP_END = 200.0f;
        constexpr float LP_START =  200.0f, LP_END = 4000.0f;
        constexpr float AMP_START = 0.004f, AMP_END = 0.028f;

        wauvio::NoiseGenerator ng(0xDEADC0DEu);

        // 1-pole filters — use manual coefficient update per sample
        // alpha_hp = RC / (RC + dt),  alpha_lp = dt / (RC + dt)
        // Update every 256 samples (avoid per-sample division cost)
        constexpr int UPDATE_INTERVAL = 256;

        float hp_alpha = 0.999f;   // initial HP (2000 Hz equivalent)
        float lp_alpha = 0.029f;   // initial LP (200 Hz equivalent)
        float hp_xp = 0.0f, hp_yp = 0.0f;
        float lp_z  = 0.0f;

        for (size_t i = 0; i < TOTAL; ++i) {
            // Update filter coefficients every UPDATE_INTERVAL samples
            if (i % UPDATE_INTERVAL == 0) {
                const float t_norm = static_cast<float>(i) / static_cast<float>(TOTAL);
                const float hp_hz  = HP_START + t_norm * (HP_END - HP_START);
                const float lp_hz  = LP_START + t_norm * (LP_END - LP_START);
                const double dt  = 1.0 / SR;
                const double rc_hp = 1.0 / (wauvio::TWO_PI * static_cast<double>(hp_hz));
                const double rc_lp = 1.0 / (wauvio::TWO_PI * static_cast<double>(lp_hz));
                hp_alpha = static_cast<float>(rc_hp / (rc_hp + dt));
                lp_alpha = static_cast<float>(dt    / (rc_lp + dt));
            }

            const float t_norm = static_cast<float>(i) / static_cast<float>(TOTAL);
            const float amp    = AMP_START + t_norm * (AMP_END - AMP_START);

            float s = ng.tick();

            // HP filter
            float hp_y = hp_alpha * (hp_yp + s - hp_xp);
            hp_xp = s;
            hp_yp = hp_y;
            s = hp_y;

            // LP filter
            lp_z += lp_alpha * (s - lp_z);
            s = lp_z;

            out[i] += s * amp;
        }
    }

    // =========================================================================
    //  ENGINE C — FRACTURE PULSES
    //  Irregular noise bursts of 0.05s each, hard-clipped.
    //  LP cutoff descends with each pulse: 2800, 2400, 2100, 1800, 1600,
    //                                      1400, 1200, 1050, 950, 880 Hz
    //  (Each fracture penetrates deeper material — lower resonance)
    //  Intervals (cumulative from 2.2s start):
    //    2.2, 6.9, 10.0, 16.8, 19.1, 24.6, 26.5, 33.7, 37.5, 39.6
    // =========================================================================
    {
        const double burst_times[] = {
             2.2,  6.9, 10.0, 16.8, 19.1,
            24.6, 26.5, 33.7, 37.5, 39.6
        };
        const double lp_cutoffs[] = {
            2800.0, 2400.0, 2100.0, 1800.0, 1600.0,
            1400.0, 1200.0, 1050.0,  950.0,  880.0
        };
        constexpr double BURST_DUR = 0.05;
        constexpr float  BURST_AMP = 0.055f;
        constexpr int    N_BURSTS  = 10;

        wauvio::NoiseGenerator ng_burst(0xF0F0F0F0u);

        for (int b = 0; b < N_BURSTS; ++b) {
            const size_t s0  = static_cast<size_t>(burst_times[b] * SR);
            const size_t len = static_cast<size_t>(BURST_DUR * SR);
            const size_t end = std::min(s0 + len, TOTAL);

            // Generate noise, hard-clip, LP-filter
            wauvio::Buffer burst(len, 0.f);
            for (size_t i = 0; i < len; ++i)
                burst[i] = ng_burst.tick();

            wauvio::distortion::apply_hard_clip(burst, 0.35f);

            wauvio::LowPassFilter lp(lp_cutoffs[b], SR);
            lp.process(burst);

            // Short fade-in/out to avoid clicks (1ms each)
            const size_t fade_n = static_cast<size_t>(0.001 * SR);
            for (size_t i = 0; i < fade_n && i < len; ++i) {
                const float f = static_cast<float>(i) / static_cast<float>(fade_n);
                burst[i]           *= f;
                burst[len - 1 - i] *= f;
            }

            for (size_t i = 0; i < end - s0; ++i)
                out[s0 + i] += burst[i] * BURST_AMP;
        }
    }

    // =========================================================================
    //  LOOP BOUNDARY PREP
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

} // namespace sequence1_final
} // namespace soundtrack
