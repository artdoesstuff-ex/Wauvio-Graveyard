#include "wauvio.hpp"
#include "wauvio_ext.hpp"
#include "notes.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include <algorithm>
#include <thread>

using namespace wauvio;
using namespace wauvio::audio;
using flamewall_data::NoteEvent;

constexpr int SR = 44100;

static int freq_to_midi(double freq) {
    if (freq <= 0.0) return -1;
    return static_cast<int>(std::lround(69.0 + 12.0 * std::log2(freq / 440.0)));
}

static void mix_mono_at(StereoBuffer& dst, const Buffer& src, double t_sec, float gain, int sr) {
    const size_t off = static_cast<size_t>(t_sec * sr);
    for (size_t i = 0; i < src.size() && off + i < dst.size(); ++i) {
        dst.L[off + i] += src[i] * gain;
        dst.R[off + i] += src[i] * gain;
    }
}

static void mix_stereo_at(StereoBuffer& dst, const StereoBuffer& src, double t_sec, float gain, int sr) {
    const size_t off = static_cast<size_t>(t_sec * sr);
    for (size_t i = 0; i < src.size() && off + i < dst.size(); ++i) {
        dst.L[off + i] += src.L[i] * gain;
        dst.R[off + i] += src.R[i] * gain;
    }
}

static void play_note_at(StereoBuffer& dst, const audio::Instrument& instr, const NoteEvent& n,
                          float gain, int sr, Dynamics dyn = Dynamics::mf,
                          Articulation art = Articulation::Sustain)
{
    int midi = freq_to_midi(n.freq);
    if (midi < 0) return;
    PlayedNote pn = instr.play(midi, n.dur, dyn, art, sr);
    mix_stereo_at(dst, pn.audio, n.t, gain, sr);
}

void print_help(const char* prog) {
    std::cout <<
        "Flamewall - Camellia (CraftyZerg MIDI)\n"
        "Rebuilt on the wauvio_ext instrument abstraction layer.\n"
        "Usage:\n"
        "  " << prog << " [options]\n\n"
        "Options:\n"
        "  -o, --output <file>   Output WAV (default: flamewall.wav)\n"
        "      --play            Play after rendering\n"
        "  -h, --help            Show this help\n";
}

int main(int argc, char** argv) {
    std::string output  = "flamewall.wav";
    bool        do_play = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" || a == "--output") {
            if (i + 1 < argc) output = argv[++i];
            else { std::cerr << "Missing argument for " << a << "\n"; return 1; }
        } else if (a == "--play") {
            do_play = true;
        } else if (a == "-h" || a == "--help") {
            print_help(argv[0]); return 0;
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            print_help(argv[0]); return 1;
        }
    }

    global_config().sample_rate = SR;
    global_config().channels    = 2;
    global_config().bits        = 16;

    constexpr double TOTAL_DUR = 422.0;
    const size_t total = static_cast<size_t>(TOTAL_DUR * SR);

    std::cout << "Flamewall - Camellia (CraftyZerg MIDI)\n";
    std::cout << "  Tempo : 300 BPM\n";
    std::cout << "  Length: " << std::fixed << std::setprecision(1) << TOTAL_DUR << " s\n\n";

    auto kick_instr  = instruments::Kick();
    auto snare_instr = instruments::Snare();
    auto hat_c_instr = instruments::ClosedHiHat();
    auto hat_o_instr = instruments::OpenHiHat();
    auto crash_instr = instruments::CrashCymbal();

    const Buffer kick_buf  = kick_instr.hit(Dynamics::ff).audio.to_mono();
    const Buffer snare_buf = snare_instr.hit(Dynamics::ff).audio.to_mono();
    const Buffer hat_c_buf = hat_c_instr.hit(Dynamics::mf, 0.05).audio.to_mono();
    const Buffer hat_o_buf = hat_o_instr.hit(Dynamics::mf, 0.22).audio.to_mono();
    const Buffer crash_buf = crash_instr.hit(Dynamics::f, 0.9).audio.to_mono();

    auto lead_guitar  = instruments::ElectricGuitar();
    lead_guitar.recipe.filter_cutoff = 6200.0;
    lead_guitar.recipe.envelope.decay = 0.45;
    lead_guitar.distortion(2.6f);

    auto power_guitar = instruments::ElectricGuitar();
    power_guitar.recipe.filter_cutoff = 2600.0;
    power_guitar.recipe.osc_mix = 0.5;
    power_guitar.recipe.envelope.decay = 0.35;
    power_guitar.distortion(3.4f);

    auto violin = instruments::SoloViolin();
    violin.recipe.vibrato_depth_cents = 22.0;
    violin.distortion(1.15f);

    auto strings_pad = instruments::StringEnsemble();
    strings_pad.recipe.stereo_width = 0.45;
    strings_pad.reverb(0.80f, 0.28f);

    auto synth_bass = instruments::SynthBass();

    auto fm_piano = instruments::ElectricPiano();

    StereoBuffer kick_track(total, 0.0f);
    StereoBuffer snare_track(total, 0.0f);
    StereoBuffer perc_track(total, 0.0f);
    StereoBuffer guitar_track(total, 0.0f);
    StereoBuffer power_track(total, 0.0f);
    StereoBuffer violin_track(total, 0.0f);
    StereoBuffer bass_track(total, 0.0f);
    StereoBuffer pad_track(total, 0.0f);
    StereoBuffer piano_track(total, 0.0f);

    using namespace flamewall_data;

    std::cout << "  [1/9] Drums...\n" << std::flush;
    for (int i = 0; i < kick_times_count; ++i)
        mix_mono_at(kick_track, kick_buf, kick_times[i], 1.0f, SR);
    for (int i = 0; i < snare_times_count; ++i)
        mix_mono_at(snare_track, snare_buf, snare_times[i], 1.0f, SR);
    for (int i = 0; i < hat_times_count; ++i)
        mix_mono_at(perc_track, hat_c_buf, hat_times[i], 0.40f, SR);
    for (int i = 0; i < hat_o_times_count; ++i)
        mix_mono_at(perc_track, hat_o_buf, hat_o_times[i], 0.34f, SR);
    for (int i = 0; i < crash_times_count; ++i)
        mix_mono_at(perc_track, crash_buf, crash_times[i], 0.38f, SR);

    std::cout << "  [2/9] FM Guitar (" << guitar_notes_count << " notes)...\n" << std::flush;
    for (int i = 0; i < guitar_notes_count; ++i) {
        const NoteEvent& n = guitar_notes[i];
        if (n.dur < 0.005f) continue;
        if (n.freq >= 330.0f) {
            play_note_at(guitar_track, lead_guitar, n, 0.70f, SR);
        } else {
            play_note_at(power_track, power_guitar, n, 0.50f, SR);
        }
        if (i % 250 == 0)
            std::cout << "    " << i << "/" << guitar_notes_count << "\r" << std::flush;
    }
    std::cout << "\n";

    std::cout << "  [3/9] Melody doubles (" << strings2_notes_count << " notes)...\n" << std::flush;
    for (int i = 0; i < strings2_notes_count; ++i) {
        const NoteEvent& n = strings2_notes[i];
        if (n.dur < 0.005f || n.freq < 300.0f) continue;
        play_note_at(guitar_track, lead_guitar, n, 0.40f, SR);
        if (i % 250 == 0)
            std::cout << "    " << i << "/" << strings2_notes_count << "\r" << std::flush;
    }
    std::cout << "\n";

    std::cout << "  [4/9] Violin (" << violin_notes_count << " notes)...\n" << std::flush;
    for (int i = 0; i < violin_notes_count; ++i) {
        const NoteEvent& n = violin_notes[i];
        if (n.dur < 0.005f) continue;
        play_note_at(violin_track, violin, n, 0.52f, SR);
    }
    std::cout << "  [5/9] Screech lead (" << guit_d7_notes_count << " notes)...\n" << std::flush;
    for (int i = 0; i < guit_d7_notes_count; ++i) {
        const NoteEvent& n = guit_d7_notes[i];
        if (n.dur < 0.005f) continue;
        play_note_at(violin_track, violin, n, 0.40f, SR);
    }

    std::cout << "  [6/9] Bass (" << bass_notes_count << "+" << bass2_notes_count << " notes)...\n" << std::flush;
    for (int i = 0; i < bass_notes_count; ++i) {
        const NoteEvent& n = bass_notes[i];
        if (n.dur < 0.005f) continue;
        int midi = freq_to_midi(n.freq);
        if (midi < 0) continue;
        StereoBuffer note = synth_bass.play(midi, n.dur, Dynamics::mf, Articulation::Sustain, SR).audio;
        Buffer mono = note.to_mono();
        const size_t off = static_cast<size_t>(n.t * SR);
        for (size_t j = 0; j < mono.size() && off + j < bass_track.size(); ++j) {
            bass_track.L[off + j] += mono[j] * 0.55f;
            bass_track.R[off + j] += mono[j] * 0.55f;
        }
        if (i % 300 == 0)
            std::cout << "    " << i << "/" << bass_notes_count << "\r" << std::flush;
    }
    for (int i = 0; i < bass2_notes_count; ++i) {
        const NoteEvent& n = bass2_notes[i];
        if (n.dur < 0.005f) continue;
        int midi = freq_to_midi(n.freq);
        if (midi < 0) continue;
        StereoBuffer note = synth_bass.play(midi, n.dur, Dynamics::mf, Articulation::Sustain, SR).audio;
        Buffer mono = note.to_mono();
        const size_t off = static_cast<size_t>(n.t * SR);
        for (size_t j = 0; j < mono.size() && off + j < bass_track.size(); ++j) {
            bass_track.L[off + j] += mono[j] * 0.40f;
            bass_track.R[off + j] += mono[j] * 0.40f;
        }
    }
    std::cout << "\n";

    std::cout << "  [7/9] Strings pad (" << piano_notes_count << " notes)...\n" << std::flush;
    for (int i = 0; i < piano_notes_count; ++i) {
        const NoteEvent& n = piano_notes[i];
        if (n.dur < 0.03f) continue;
        play_note_at(pad_track, strings_pad, n, 0.22f, SR);
        if (i % 300 == 0)
            std::cout << "    " << i << "/" << piano_notes_count << "\r" << std::flush;
    }
    std::cout << "\n";

    std::cout << "  [8/9] FM Piano & Harp (" << gp4_notes_count << "+" << harp_notes_count << " notes)...\n" << std::flush;
    for (int i = 0; i < gp4_notes_count; ++i) {
        const NoteEvent& n = gp4_notes[i];
        if (n.dur < 0.005f) continue;
        play_note_at(piano_track, fm_piano, n, 0.30f, SR);
        if (i % 300 == 0)
            std::cout << "    gp4 " << i << "/" << gp4_notes_count << "\r" << std::flush;
    }
    for (int i = 0; i < harp_notes_count; ++i) {
        const NoteEvent& n = harp_notes[i];
        if (n.dur < 0.005f) continue;
        play_note_at(piano_track, fm_piano, n, 0.35f, SR);
    }
    std::cout << "\n";

    std::cout << "  [9/9] Mix & master...\n" << std::flush;
    {
        SidechainCompressor sc;
        sc.attack_ms  = 1.5f;
        sc.release_ms = 80.0f;
        sc.strength   = 0.55f;
        sc.threshold  = 0.030f;
        Buffer kick_mono = kick_track.to_mono();
        sc.apply(pad_track,   kick_mono, SR);
        sc.apply(power_track, kick_mono, SR);
    }

    StereoBuffer mix(total, 0.0f);
    mix_into(mix, kick_track,   0.78f);
    mix_into(mix, snare_track,  0.85f);
    mix_into(mix, perc_track,   0.85f);
    mix_into(mix, bass_track,   0.88f);
    mix_into(mix, power_track,  0.80f);
    mix_into(mix, guitar_track, 0.95f);
    mix_into(mix, violin_track, 0.70f);
    mix_into(mix, pad_track,    0.55f);
    mix_into(mix, piano_track,  0.60f);

    distortion::apply_soft_clip(mix.L, 1.20f);
    distortion::apply_soft_clip(mix.R, 1.20f);
    fade_in(mix,  0.08, SR);
    fade_out(mix, 3.5,  SR);
    normalize(mix, 0.93f);
    clamp_buffer(mix);

    std::cout << "Writing WAV: " << output << "\n";
    save_wav_stereo(mix, output, SR);
    std::cout << "Done.\n";

    if (do_play) {
        std::cout << "Playing (press Enter to stop early)...\n";
        PlaybackHandle handle = play_async(mix, SR);
        std::thread waiter([handle]() mutable {
            std::cin.get(); handle.stop();
        });
        waiter.detach();
        handle.wait();
        std::cout << "Playback finished.\n";
    }

    return 0;
}
