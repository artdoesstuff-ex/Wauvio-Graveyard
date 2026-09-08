#pragma once

// =============================================================================
//  wauvio_sfx_entities.hpp
//  Sound effects for all seven ExplorationState / EntitySystem entities.
//
//  Entity roster and sound identities
//  -----------------------------------
//
//  LURKER
//    Behaviour: patrols quietly; chases when suspicion >= 1.5; deleted when
//    directly looked at (lookFac > 0.7, dist < chSize×3).
//    Sound identity: PERIPHERAL.  It exists at the edge of perception.
//    Sounds like something that does not want to be heard — but you can.
//    Presence: a dry sub-scrape, irregular, always slightly off-axis.
//    Contact:  a sharp retraction — it was almost there.
//    Flee:     a brief skitter, disappearing quickly.
//
//  LEECH
//    Behaviour: fast chase (150 spd), steals inventory on contact.
//    Sound identity: CLINICAL VELOCITY.  No hesitation.  No warning.
//    Sounds purposeful and fast.  No warmth.  No emotion.
//    Presence: rapid, light percussion bursts.  Think needle-point contact.
//    Contact:  a brief "got it" click — done, gone.
//    Flee:     just the tail end of its speed, already receding.
//
//  SHINING
//    Behaviour: orbits player, moves AWAY, deleted by player blinking.
//    Sound identity: SPECTRAL HALO.  A wrong brightness.
//    Sounds like something that emits rather than reflects.
//    Presence: rotating FM shimmer with ring-modulated overtones.
//    Blink-delete: the shimmer contracts to a point and disappears.
//
//  WRAITH
//    Behaviour: spirals inward tightening orbit, repulsed by blinks,
//    drains sanity in range, torch pushes it back.
//    Sound identity: ORBITAL DREAD.  Weight increasing over distance.
//    Sounds like something made of pressure.  The closer it is the
//    more the sound occupies the skull, not just the ears.
//    Presence: low FM orbit, amplitude increases as orbit tightens.
//    Repulse:  sudden release — pressure venting outward.
//
//  HOLLOW
//    Behaviour: hears footsteps (suspicion rises while player walks),
//    chases when suspicious, patrols when not.
//    Sound identity: RESONANT CAVITY.  It is defined by what it lacks.
//    Sounds like a hollow space that should be filled but isn't.
//    Presence: tubular resonance, driven by a mid-low FM voice.
//              Responds to walking — amplitude breathes with stride.
//    Contact:  a deep concussive hollow thud.
//
//  STALKER
//    Behaviour: teleports when looked at; barely moves; passively drains
//    sanity every 20 seconds.  Always at the edge of the visible range.
//    Sound identity: LIMINAL STATIC.  Something that should not be seen.
//    The sound of a presence that exists between frames of perception.
//    Presence: diffuse high noise, barely pitched, drifting.
//              No clear source.  The sound comes from everywhere.
//    Teleport: a sudden phase inversion — not a movement sound,
//              but the sound of spatial wrongness.
//
//  HUSK
//    Behaviour: spawns in packs of 3; charges in formation within radius;
//    patrols otherwise.  Spread angles keep the trio from clumping.
//    Sound identity: PACK MACHINERY.  Coordinated.  Grinding.
//    Sounds like something that knows how to work with others.
//    Presence: interlocking scrapes — three voices slightly offset
//              in phase, producing a mechanical ratchet quality.
//    Contact:  a hard tri-impact — all three hit nearly simultaneously.
//
//  Architecture compliance
//  -----------------------
//  Presence buffers (loopable ambient): 4–6 s, normalize 0.50 (§5.1)
//  One-shot buffers (contact/flee/delete): 0.3–2 s, normalize 0.60 (§5.1)
//  Fixed noise seeds (§3.2)
//  No wauvio::play(), no threads, no loops called inside generators (§3.3)
//  Sample rate via global_config() (§6.4)
// =============================================================================

#include "../wauvio.hpp"

// =============================================================================
//  INTERNAL HELPERS
// =============================================================================

namespace ent_detail {

// FM event writer — writes into buf at sample offset s0
inline void fm(wauvio::Buffer& buf,
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

// FM glide — carrier linearly interpolates start→end
inline void fm_glide(wauvio::Buffer& buf,
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
        cp += wauvio::TWO_PI * freq_c            / SR;
        mp += wauvio::TWO_PI * (freq_c*mod_ratio)/ SR;
    }
}

// Band-limited noise burst
inline void noise(wauvio::Buffer& buf,
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
    const size_t ramp = std::min(len / 4, static_cast<size_t>(0.012 * SR));
    for (size_t i = 0; i < len; ++i) {
        float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
        float env = 1.0f;
        if (i < ramp)        env = static_cast<float>(i)           / static_cast<float>(ramp);
        if (i >= len - ramp) env = static_cast<float>(len - 1 - i) / static_cast<float>(ramp);
        buf[s0 + i] += s * amp * env;
    }
}

} // namespace ent_detail


// =============================================================================
//  LURKER
//  Presence: dry sub-scrape, irregular, peripheral
//  Contact:  sharp retraction
//  Flee:     brief skitter disappearing left
// =============================================================================

namespace ent {

inline wauvio::Buffer gen_lurker_presence() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 4.8;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Primary scrape voice: low FM, irregular amplitude driven by a non-periodic
    // envelope built from three summed sine LFOs at incommensurable rates.
    // Carrier: 72 Hz (hollow scrape register), Mod: 72×√5, index 1.4
    {
        constexpr double C = 72.0, M = 72.0 * 2.2360679774997896;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        // Three LFOs: 0.73, 1.17, 2.31 Hz — their sum is quasi-random
        const double l1_inc = wauvio::TWO_PI * 0.73 / SR;
        const double l2_inc = wauvio::TWO_PI * 1.17 / SR;
        const double l3_inc = wauvio::TWO_PI * 2.31 / SR;
        double cp = 0.0, mp = 0.0, l1 = 0.0, l2 = wauvio::PI * 0.7, l3 = wauvio::PI * 1.4;
        for (size_t i = 0; i < TOTAL; ++i) {
            // Sum three LFOs, normalize to [0.05, 1.0]
            const float lfo_sum = (std::sin(l1) + std::sin(l2) + std::sin(l3)) / 3.f;
            const float env     = 0.05f + 0.95f * (0.5f + 0.5f * lfo_sum);
            out[i] += static_cast<float>(
                std::sin(cp + 1.4 * std::sin(mp))) * 0.045f * env;
            cp += c_inc; mp += m_inc;
            l1 += l1_inc; l2 += l2_inc; l3 += l3_inc;
        }
    }

    // High scrape component: very quiet, above the FM voice
    // HPF 1400 Hz, LPF 3200 Hz — the fingernail-on-stone quality
    ent_detail::noise(out, 0, DUR, 0.016f, 1400.0, 3200.0, 0x1EA3BE11u, SR);

    // Periodic sub-thud: appears at irregular intervals (2.1s, 3.7s)
    // Each thud: low FM, 0.08s, hard onset, slow decay
    for (double t : {0.7, 2.1, 3.7}) {
        ent_detail::fm(out, static_cast<size_t>(t * SR), 0.08,
                       55.0, 55.0 * 1.6180339887, 2.2,
                       0.038f, 0.003, 0.07, SR);
    }

    wauvio::fade_in(out, 0.05, SR); wauvio::fade_out(out, 0.08, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_lurker_contact() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.5;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // A retraction: high→low FM sweep (it pulls back)
    ent_detail::fm_glide(out, 0, 0.35, 420.0, 68.0, 1.6180339887, 1.8,
                         0.065f, 0.008f, 0.003, 0.12, SR);
    // Sub-crunch at onset
    ent_detail::noise(out, 0, 0.06, 0.040f, 80.0, 400.0, 0xAA3C0000u, SR);

    wauvio::fade_in(out, 0.003, SR); wauvio::fade_out(out, 0.10, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}

inline wauvio::Buffer gen_lurker_flee() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.7;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Rapid staccato scrapes trailing off — 4 hits, each quieter and faster
    const double onsets[] = { 0.0, 0.11, 0.20, 0.27 };
    const float  amps[]   = { 0.050f, 0.038f, 0.027f, 0.016f };
    for (int i = 0; i < 4; ++i) {
        ent_detail::fm(out, static_cast<size_t>(onsets[i] * SR), 0.07,
                       72.0 + i * 18.0, (72.0 + i * 18.0) * 1.4142135623, 1.4,
                       amps[i], 0.003, 0.055, SR);
    }

    wauvio::fade_in(out, 0.003, SR); wauvio::fade_out(out, 0.12, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  LEECH
//  Presence: rapid light percussion, clinical needle-point motion
//  Contact:  single click — done
//  Flee:     receding speed tail
// =============================================================================

inline wauvio::Buffer gen_leech_presence() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 4.2;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Rapid light FM ticks at irregular timing — fast motion, purposeful
    // Carrier: 280 Hz (mid-high, bright, thin), Mod: 280×φ, index 0.7
    // Tick timing: bunched, not metered — like quick coordinated steps
    const double ticks[] = {
        0.08, 0.17, 0.27, 0.56, 0.65, 0.74, 0.84,
        1.22, 1.31, 1.40, 1.78, 1.87, 2.35, 2.44,
        2.63, 2.72, 3.11, 3.20, 3.59, 3.78, 3.87, 3.97
    };
    for (double t : ticks) {
        ent_detail::fm(out, static_cast<size_t>(t * SR), 0.04,
                       280.0, 280.0 * 1.6180339887, 0.7,
                       0.032f, 0.002, 0.030, SR);
    }

    // Thin high-frequency continuous hiss — the leech's body moving
    ent_detail::noise(out, 0, DUR, 0.010f, 3500.0, 7000.0, 0xAEEC1110u, SR);

    wauvio::fade_in(out, 0.04, SR); wauvio::fade_out(out, 0.06, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_leech_contact() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.3;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Single dry click — item taken, no drama
    // High FM stab: carrier 480 Hz, mod 480×√3, idx 0.4 (almost pure tone)
    ent_detail::fm(out, 0, 0.06, 480.0, 480.0 * 1.7320508075688773, 0.4,
                   0.058f, 0.001, 0.05, SR);
    // Quiet high-frequency transient — physical contact
    ent_detail::noise(out, 0, 0.025, 0.030f, 4000.0, 9000.0, 0xC1C1C100u, SR);

    wauvio::fade_in(out, 0.001, SR); wauvio::fade_out(out, 0.05, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}

inline wauvio::Buffer gen_leech_flee() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.6;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Diminishing tick cluster — already receding
    const double ticks[] = { 0.0, 0.08, 0.15, 0.21, 0.27 };
    const float  amps[]  = { 0.040f, 0.030f, 0.021f, 0.014f, 0.008f };
    for (int i = 0; i < 5; ++i) {
        ent_detail::fm(out, static_cast<size_t>(ticks[i] * SR), 0.04,
                       280.0, 280.0 * 1.6180339887, 0.7,
                       amps[i], 0.001, 0.030, SR);
    }

    wauvio::fade_in(out, 0.002, SR); wauvio::fade_out(out, 0.10, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  SHINING
//  Presence: rotating spectral halo — ring-mod shimmer, orbiting quality
//  Blink-delete: shimmer contracts and disappears
// =============================================================================

inline wauvio::Buffer gen_shining_presence() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 5.0;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Primary halo: FM voice with amplitude modulated by a rotating "orbit" LFO
    // Carrier: 312 Hz (high-mid, clear, ethereal), Mod: 312×φ, index 0.6
    // Orbit LFO: 1.4 rad/s (approximates WRAITH_ORBIT_SPD from AI) — the spiral
    {
        constexpr double C   = 312.0, M = 312.0 * 1.6180339887;
        constexpr double IDX = 0.6;
        const double c_inc   = wauvio::TWO_PI * C / SR;
        const double m_inc   = wauvio::TWO_PI * M / SR;
        const double orb_inc = wauvio::TWO_PI * (1.4 / wauvio::TWO_PI) / SR; // orbit ≈ 0.223 Hz
        double cp = 0.0, mp = 0.0, op = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            // Orbit modulation: amplitude rises and falls as the Shining circles
            const float orbit_env = 0.50f + 0.50f * static_cast<float>(std::sin(op));
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.048f * orbit_env;
            cp += c_inc; mp += m_inc; op += orb_inc;
        }
    }

    // Ring-modulated shimmer layer: noise × carrier sine = wrong overtones
    // The Shining emits; it doesn't merely reflect.
    {
        wauvio::NoiseGenerator ng(0x51A1A900u);
        wauvio::HighPassFilter hp(800.0, SR);
        wauvio::LowPassFilter  lp(2400.0, SR);
        constexpr double RING = 312.0 * 1.41421356;  // ring mod carrier = C×√2
        const double ring_inc = wauvio::TWO_PI * RING / SR;
        double ring_phase = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            s *= static_cast<float>(std::sin(ring_phase));  // ring modulate
            out[i] += s * 0.022f;
            ring_phase += ring_inc;
        }
    }

    wauvio::fade_in(out, 0.08, SR); wauvio::fade_out(out, 0.10, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_shining_blink_delete() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.7;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Shimmer contracts: FM index drops rapidly 0.6 → 0.0, amplitude falls
    // Then a brief high-frequency inward sweep — like a light filament going out
    {
        constexpr double C = 312.0, M = 312.0 * 1.6180339887;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            const double t   = static_cast<double>(i) / static_cast<double>(TOTAL);
            const double idx = 0.6 * (1.0 - t);               // index shrinks
            const float  amp = static_cast<float>((1.0 - t) * (1.0 - t)); // quadratic fall
            out[i] += static_cast<float>(
                std::sin(cp + idx * std::sin(mp))) * 0.055f * amp;
            cp += c_inc; mp += m_inc;
        }
    }

    // Inward high-frequency sweep (pitch falls toward 0 at end)
    ent_detail::fm_glide(out, 0, 0.4,
                         1800.0, 200.0,
                         0.5, 0.3,
                         0.030f, 0.0f,
                         0.002, 0.08, SR);

    wauvio::fade_in(out, 0.003, SR); wauvio::fade_out(out, 0.05, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  WRAITH
//  Presence: slow orbital drone, amplitude tightening — orbital dread
//  Repulse:  pressure venting outward — sudden release
// =============================================================================

inline wauvio::Buffer gen_wraith_presence() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 5.5;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Orbital drone: amplitude rises as orbit tightens (simulates closing distance)
    // Carrier: 58 Hz (low, chest-resonant, droning), Mod: 58×(√3), index 1.0→1.8
    // Amplitude: rises from 0.020 → 0.065 over full duration (tightening orbit)
    {
        constexpr double C = 58.0, M = 58.0 * 1.7320508075688773;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        // Orbit panning effect: slow amplitude sine at orbit speed (0.223 Hz)
        const double orb_inc = wauvio::TWO_PI * 0.223 / SR;
        double cp = 0.0, mp = 0.0, op = wauvio::PI * 0.5;
        for (size_t i = 0; i < TOTAL; ++i) {
            const double t      = static_cast<double>(i) / static_cast<double>(TOTAL);
            const double idx    = 1.0 + 0.8 * t;                              // index opens
            const float  base   = 0.020f + 0.045f * static_cast<float>(t);    // tightening
            const float  orbit  = 0.70f + 0.30f * static_cast<float>(std::sin(op));
            out[i] += static_cast<float>(
                std::sin(cp + idx * std::sin(mp))) * base * orbit;
            cp += c_inc; mp += m_inc; op += orb_inc;
        }
    }

    // Sanity resonance layer: upper-mid hum that hints at the sanity drain
    // Very quiet, near 100 Hz — adds an additional pressure feeling
    ent_detail::fm(out, 0, DUR,
                   103.0, 103.0 * 2.41421356237, 0.4,
                   0.018f, 0.50, 0.80, SR);

    wauvio::fade_in(out, 0.10, SR); wauvio::fade_out(out, 0.12, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_wraith_repulse() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 1.2;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Pressure release: FM burst that expands outward (pitch rises as it flees)
    // Starts at orbit frequency, sweeps up as it's pushed away
    ent_detail::fm_glide(out, 0, 0.7,
                         58.0, 180.0,        // pitch rises with repulsion speed
                         1.7320508075688773,
                         1.6,
                         0.065f, 0.020f,
                         0.005, 0.25, SR);

    // Low-frequency thud at the moment of repulsion — blink impact
    ent_detail::fm(out, 0, 0.15,
                   38.0, 38.0 * 1.6180339887, 0.9,
                   0.055f, 0.003, 0.12, SR);

    wauvio::fade_in(out, 0.003, SR); wauvio::fade_out(out, 0.15, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  HOLLOW
//  Presence: tubular resonance, breathing with player's movement
//  Contact:  deep concussive hollow thud
// =============================================================================

inline wauvio::Buffer gen_hollow_presence() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 5.2;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Hollow resonance: FM voice with a tubular quality
    // Carrier: 121 Hz (hollow bodied, between bass and mid), Mod: 121×(1+1/e), index 0.5
    // The low index gives it a tuned, almost-bell quality — wrong for a cave thing
    {
        constexpr double C   = 121.0;
        constexpr double M   = 121.0 * 1.3678794411714423; // 1+1/e
        constexpr double IDX = 0.5;
        const double c_inc   = wauvio::TWO_PI * C / SR;
        const double m_inc   = wauvio::TWO_PI * M / SR;
        // Breathing modulation: slow swell at 0.9 Hz — subtly tied to walking rhythm
        const double swell_inc = wauvio::TWO_PI * 0.9 / SR;
        double cp = 0.0, mp = 0.0, sp = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            const float swell = 0.40f + 0.60f * static_cast<float>(
                0.5 + 0.5 * std::sin(sp));
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.052f * swell;
            cp += c_inc; mp += m_inc; sp += swell_inc;
        }
    }

    // Hollow cavity resonance: second harmonic of the fundamental (242 Hz)
    // at much lower amplitude — the overtone that makes it sound empty
    ent_detail::fm(out, 0, DUR,
                   242.0, 242.0 * 1.3678794411714423, 0.3,
                   0.014f, 0.30, 0.50, SR);

    // Subtle low rumble — the body of the hollow thing moving
    ent_detail::noise(out, 0, DUR, 0.008f, 30.0, 90.0, 0xA011A000u, SR);

    wauvio::fade_in(out, 0.08, SR); wauvio::fade_out(out, 0.10, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_hollow_contact() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.8;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Deep concussive hollow thud — the inside of the thing hitting you
    // Low FM: carrier 48 Hz, mod 48×∛3, index 3.2 — tonal attack into mud
    ent_detail::fm(out, 0, 0.5,
                   48.0, 48.0 * 1.44224957030741, 3.2,
                   0.072f, 0.004, 0.40, SR);

    // Mid-hollow resonance ring after impact
    ent_detail::fm(out, static_cast<size_t>(0.05 * SR), 0.55,
                   121.0, 121.0 * 1.3678794411714423, 0.5,
                   0.035f, 0.01, 0.50, SR);

    wauvio::fade_in(out, 0.003, SR); wauvio::fade_out(out, 0.12, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  STALKER
//  Presence: diffuse high noise, sourceless, liminal static
//  Teleport: spatial phase inversion — wrongness, not movement
// =============================================================================

inline wauvio::Buffer gen_stalker_presence() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 5.0;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Three noise bands at different frequencies — the sound has no single source
    // Each band drifts in amplitude with a different LFO rate
    // Band A: HPF 1800 Hz, LPF 3500 Hz — thin, high, present
    // Band B: HPF 3500 Hz, LPF 6000 Hz — very high, almost inaudible
    // Band C: HPF  600 Hz, LPF 1200 Hz — mid, ghostly

    // Band A with amplitude LFO 0.31 Hz
    {
        wauvio::NoiseGenerator ng(0x57A13300u);
        wauvio::HighPassFilter hp(1800.0, SR);
        wauvio::LowPassFilter  lp(3500.0, SR);
        const double lfo_inc = wauvio::TWO_PI * 0.31 / SR;
        double lp_phase = 0.0;
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            const float env = 0.35f + 0.65f * static_cast<float>(
                0.5 + 0.5 * std::sin(lp_phase));
            out[i] += s * 0.028f * env;
            lp_phase += lfo_inc;
        }
    }

    // Band B with amplitude LFO 0.53 Hz
    {
        wauvio::NoiseGenerator ng(0x57A13311u);
        wauvio::HighPassFilter hp(3500.0, SR);
        wauvio::LowPassFilter  lp(6000.0, SR);
        const double lfo_inc = wauvio::TWO_PI * 0.53 / SR;
        double lp_phase = wauvio::PI;
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            const float env = 0.20f + 0.80f * static_cast<float>(
                0.5 + 0.5 * std::sin(lp_phase));
            out[i] += s * 0.014f * env;
            lp_phase += lfo_inc;
        }
    }

    // Band C with amplitude LFO 0.17 Hz (very slow)
    {
        wauvio::NoiseGenerator ng(0x57413322u);
        wauvio::HighPassFilter hp(600.0,  SR);
        wauvio::LowPassFilter  lp(1200.0, SR);
        const double lfo_inc = wauvio::TWO_PI * 0.17 / SR;
        double lp_phase = wauvio::PI * 0.6;
        for (size_t i = 0; i < TOTAL; ++i) {
            float s = ng.tick(); s = hp.tick(s); s = lp.tick(s);
            const float env = 0.10f + 0.90f * static_cast<float>(
                0.5 + 0.5 * std::sin(lp_phase));
            out[i] += s * 0.018f * env;
            lp_phase += lfo_inc;
        }
    }

    wauvio::fade_in(out, 0.10, SR); wauvio::fade_out(out, 0.12, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_stalker_teleport() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.45;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Phase inversion artifact: a brief FM tone that immediately inverts polarity
    // Not a movement sound — the sensation of spatial wrongness
    // Implemented as: FM burst followed immediately by its phase-inverted copy
    {
        constexpr double C = 280.0, M = 280.0 * 1.6180339887;
        constexpr double IDX = 1.2;
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        const size_t half = TOTAL / 2;
        // First half: normal polarity, fast attack, no release
        double cp = 0.0, mp = 0.0;
        for (size_t i = 0; i < half; ++i) {
            const float env = static_cast<float>(i) / static_cast<float>(half);
            out[i] += static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.060f * env;
            cp += c_inc; mp += m_inc;
        }
        // Second half: INVERTED polarity, decaying
        cp = 0.0; mp = 0.0;
        for (size_t i = 0; i < half; ++i) {
            const float env = 1.0f - static_cast<float>(i) / static_cast<float>(half);
            // Polarity flip: negate the FM output
            out[half + i] += -static_cast<float>(
                std::sin(cp + IDX * std::sin(mp))) * 0.060f * env;
            cp += c_inc; mp += m_inc;
        }
    }

    // Broadband click at the phase-inversion boundary
    ent_detail::noise(out,
                      TOTAL / 2, 0.015,
                      0.045f, 100.0, 8000.0, 0x7E1E0000u, SR);

    wauvio::fade_in(out, 0.002, SR); wauvio::fade_out(out, 0.04, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}


// =============================================================================
//  HUSK
//  Presence: interlocking scrapes — three offset voices, mechanical ratchet
//  Contact:  tri-impact — three hits nearly simultaneous, slightly offset
// =============================================================================

inline wauvio::Buffer gen_husk_presence() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 4.5;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Three scrape voices — phase-offset versions of the same FM voice
    // Carrier: 94 Hz, Mod: 94×√6, index 1.6
    // Each voice has a slightly different step rate (the three pack members)
    // Voice offsets: 0s, 0.38s, 0.71s — irregular so they never fully align
    const double offsets[] = { 0.0, 0.38, 0.71 };
    const double freqs[]   = { 94.0, 97.3, 91.1 };  // slightly detuned — separate
    const float  amps[]    = { 0.038f, 0.033f, 0.030f };

    for (int v = 0; v < 3; ++v) {
        // Each voice: regular step pulses at the pack's movement rate
        // Step period: ~0.36s (pace of 55 spd / chSize approximation)
        constexpr double STEP_PERIOD = 0.36;
        const size_t v_offset = static_cast<size_t>(offsets[v] * SR);
        const double C = freqs[v];
        const double M = C * 2.449489742783178;  // √6
        const double c_inc = wauvio::TWO_PI * C / SR;
        const double m_inc = wauvio::TWO_PI * M / SR;
        double cp = 0.0, mp = 0.0;

        for (size_t i = v_offset; i < TOTAL; ++i) {
            const double t_in_step = std::fmod(
                static_cast<double>(i - v_offset) / SR, STEP_PERIOD);
            const float step_env = (t_in_step < 0.10)
                ? static_cast<float>(std::exp(-t_in_step / 0.03))
                : 0.0f;
            out[i] += static_cast<float>(
                std::sin(cp + 1.6 * std::sin(mp))) * amps[v] * step_env;
            cp += c_inc; mp += m_inc;
        }
    }

    // Metal-scrape texture between steps
    ent_detail::noise(out, 0, DUR, 0.009f, 900.0, 2800.0, 0xA00FACE0u, SR);

    wauvio::fade_in(out, 0.05, SR); wauvio::fade_out(out, 0.07, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.50f);
    return out;
}

inline wauvio::Buffer gen_husk_contact() {
    const int SR = wauvio::global_config().sample_rate;
    constexpr double DUR = 0.55;
    const size_t TOTAL = static_cast<size_t>(DUR * SR);
    wauvio::Buffer out(TOTAL, 0.f);

    // Three impacts in tight succession — the pack hits together
    // Each impact: low-mid FM burst, very short, sequential
    const double impact_times[] = { 0.0, 0.025, 0.055 };
    const double impact_freqs[] = { 94.0, 97.3, 91.1 };
    const float  impact_amps[]  = { 0.065f, 0.058f, 0.052f };

    for (int i = 0; i < 3; ++i) {
        ent_detail::fm(out,
            static_cast<size_t>(impact_times[i] * SR), 0.12,
            impact_freqs[i], impact_freqs[i] * 2.449489742783178, 1.6,
            impact_amps[i], 0.002, 0.10, SR);
    }

    // Broadband crunch at impact onset — physical mass
    ent_detail::noise(out, 0, 0.04, 0.040f, 60.0, 1200.0, 0xA05CA110u, SR);

    wauvio::fade_in(out, 0.002, SR); wauvio::fade_out(out, 0.10, SR);
    wauvio::clamp_buffer(out);
    wauvio::normalize(out, 0.60f);
    return out;
}

} // namespace ent
