#include "wauvio.hpp"
#include "wauvio_ext.hpp"
#include "wauvio_midi.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::string midi_path = argc > 1 ? argv[1] : "music.mid";

    auto music = wauvio::track::load_midi(midi_path);

    std::cout << "Loaded '" << midi_path << "': " << music.part_count() << " parts, "
              << music.duration_seconds << "s at " << music.initial_bpm << " BPM\n";
    for (size_t i = 0; i < music.part_count(); ++i) {
        const auto& p = music.part(i);
        std::cout << "  [" << i << "] " << p.name << " (channel " << p.channel << ", "
                  << p.notes.size() << " notes)"
                  << (p.is_percussion ? " [percussion]" : "") << "\n";
    }

    wauvio::StereoBuffer audio = music.render();
    wauvio::save_wav_stereo(audio, "midi_output.wav");
    std::cout << "Wrote midi_output.wav\n";

    return 0;
}
