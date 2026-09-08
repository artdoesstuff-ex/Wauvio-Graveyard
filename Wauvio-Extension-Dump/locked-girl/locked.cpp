#include "wauvio.hpp"
#include "wauvio_ext.hpp"
#include "notes.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <thread>

using namespace wauvio;
using namespace wauvio::audio;
using locked_data::NoteEvent;

constexpr int SR = 44100;

constexpr float STACCATO_THRESHOLD_SEC = 0.25f;

static Articulation articulation_for(const NoteEvent& n) {
    return (n.dur < STACCATO_THRESHOLD_SEC) ? Articulation::Staccato : Articulation::Sustain;
}

static void play_note_at(StereoBuffer& dst, const audio::Instrument& instr, const NoteEvent& n,
                          float gain, int sr)
{
    if (n.dur <= 0.0f) return;
    PlayedNote pn = instr.play(n.midi_note, n.dur, n.dyn, articulation_for(n), sr);
    const size_t off = static_cast<size_t>(n.t * sr);
    for (size_t i = 0; i < pn.audio.size() && off + i < dst.size(); ++i) {
        dst.L[off + i] += pn.audio.L[i] * gain;
        dst.R[off + i] += pn.audio.R[i] * gain;
    }
}

static void render_part(StereoBuffer& track, const audio::Instrument& instr,
                         const NoteEvent* notes, int count, float gain, int sr,
                         const char* label)
{
    std::cout << "  " << label << " (" << count << " notes)...\n" << std::flush;
    for (int i = 0; i < count; ++i) {
        play_note_at(track, instr, notes[i], gain, sr);
        if (i % 200 == 0)
            std::cout << "    " << i << "/" << count << "\r" << std::flush;
    }
    std::cout << "\n";
}

void print_help(const char* prog) {
    std::cout <<
        "Locked -- orchestral variation of le.mid\n"
        "Rendered with the wauvio_ext instrument abstraction layer.\n"
        "Usage:\n"
        "  " << prog << " [options]\n\n"
        "Options:\n"
        "  -o, --output <file>   Output WAV (default: locked.wav)\n"
        "      --play            Play after rendering\n"
        "  -h, --help            Show this help\n";
}

int main(int argc, char** argv) {
    std::string output  = "locked.wav";
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

    const double TOTAL_DUR = locked_data::SOURCE_DURATION_SEC + 4.0;
    const size_t total = static_cast<size_t>(TOTAL_DUR * SR);

    std::cout << "Locked -- orchestral variation of le.mid\n";
    std::cout << "  Tempo : " << std::fixed << std::setprecision(1)
               << locked_data::SOURCE_BPM << " BPM\n";
    std::cout << "  Length: " << std::setprecision(1) << locked_data::SOURCE_DURATION_SEC << " s\n\n";

    auto violin1 = instruments::SoloViolin();
    violin1.recipe.stereo_width = 0.20;
    violin1.reverb(0.55f, 0.24f);

    auto violin2 = instruments::ChamberStrings();
    violin2.recipe.filter_cutoff = 4400.0;
    violin2.reverb(0.55f, 0.22f);

    auto viola = instruments::Viola();
    viola.reverb(0.55f, 0.22f);

    auto cello = instruments::Cello();
    cello.reverb(0.6f, 0.24f);

    auto bass = instruments::DoubleBass();
    bass.reverb(0.6f, 0.2f);

    StereoBuffer violin1_track(total, 0.0f);
    StereoBuffer violin2_track(total, 0.0f);
    StereoBuffer viola_track(total, 0.0f);
    StereoBuffer cello_track(total, 0.0f);
    StereoBuffer bass_track(total, 0.0f);

    using namespace locked_data;

    render_part(violin1_track, violin1, violin1_notes, violin1_notes_count, 0.62f, SR, "[1/5] Violin I");
    render_part(violin2_track, violin2, violin2_notes, violin2_notes_count, 0.52f, SR, "[2/5] Violin II");
    render_part(viola_track,   viola,   viola_notes,   viola_notes_count,   0.56f, SR, "[3/5] Viola");
    render_part(cello_track,   cello,   cello_notes,   cello_notes_count,   0.62f, SR, "[4/5] Violoncello");
    render_part(bass_track,    bass,    bass_notes,    bass_notes_count,    0.68f, SR, "[5/5] Contrabass");

    std::cout << "Mix & master...\n" << std::flush;

    StereoBuffer mix(total, 0.0f);
    mix_into(mix, violin1_track, 0.95f);
    mix_into(mix, violin2_track, 0.85f);
    mix_into(mix, viola_track,   0.80f);
    mix_into(mix, cello_track,   0.85f);
    mix_into(mix, bass_track,    0.90f);

    Reverb hallL; hallL.room_size = 0.72f; hallL.wet = 0.16f; hallL.dry = 0.9f; hallL.init(SR);
    Reverb hallR; hallR.room_size = 0.76f; hallR.wet = 0.16f; hallR.dry = 0.9f; hallR.init(SR);
    hallL.process(mix.L);
    hallR.process(mix.R);

    distortion::apply_soft_clip(mix.L, 1.05f);
    distortion::apply_soft_clip(mix.R, 1.05f);
    fade_in(mix,  0.25, SR);
    fade_out(mix, 3.0,  SR);
    normalize(mix, 0.92f);
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
