#include "wauvio.hpp"
#include "wauvio_ext.hpp"

using namespace wauvio::audio;

int main() {
    auto piano = wauvio::instruments::AcousticGrandPiano();
    piano.reverb(0.55f, 0.22f);

    Arrangement song;
    song.tempo = Tempo(100.0);

    Melody arpeggio = {
        Note(60, 0.35, Dynamics::p),
        Note(64, 0.35, Dynamics::mp),
        Note(67, 0.35, Dynamics::mf),
        Note(72, 0.35, Dynamics::f),
        Note(76, 0.7,  Dynamics::ff),
    };
    song.add(piano, arpeggio, 0.0);

    Track chords(piano);
    chords.add_chord(Chord{ {60, 64, 67}, 1.5, Dynamics::mp });
    song.add_track(chords, 2.2);

    Melody staccato = {
        Note(72, 0.2, Dynamics::mf, Articulation::Hard),
        Note(74, 0.2, Dynamics::mp, Articulation::Soft),
        Note(76, 0.2, Dynamics::mf, Articulation::Hard),
    };
    song.add(piano, staccato, 4.0);

    wauvio::StereoBuffer mix = song.render();
    wauvio::save_wav_stereo(mix, "piano_demo.wav");
    return 0;
}
