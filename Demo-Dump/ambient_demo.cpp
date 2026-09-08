#include "wauvio.hpp"
#include "wauvio_ext.hpp"

using namespace wauvio::audio;

int main() {
    auto pad     = wauvio::instruments::AtmosphericSynth();
    auto drone   = wauvio::instruments::Drone();
    auto pluck   = wauvio::instruments::Pluck();
    auto bell    = wauvio::instruments::FMBell();
    auto duduk   = wauvio::instruments::Duduk();
    auto kalimba = wauvio::instruments::Kalimba();

    pad.chorus(0.15, 3.0, 0.5f);
    bell.delay(420.0, 0.4f, 0.35f);
    duduk.reverb(0.75f, 0.35f);

    Arrangement ambient;
    ambient.tempo = Tempo(60.0);
    ambient.master_reverb(0.8f, 0.3f);

    ambient.add(drone, Melody{ Note(45, 8.0, Dynamics::mp) }, 0.0);
    ambient.add(pad,   Melody{ Note(57, 8.0, Dynamics::p), Note(60, 6.0, Dynamics::p) }, 1.0);

    auto pent = scale::minor_pentatonic(69, 1);
    Melody pluck_line;
    for (int i = 0; i < 8; ++i)
        pluck_line.push_back(Note(pent[i % pent.size()], 0.4, Dynamics::mp));
    ambient.add(pluck, pluck_line, 2.0);

    Melody kalimba_line = { rest(4.0), Note(72, 0.5, Dynamics::mf), Note(76, 0.5, Dynamics::mf),
                             Note(79, 1.0, Dynamics::f) };
    ambient.add(kalimba, kalimba_line, 0.0);

    Melody duduk_line = { rest(3.0), Note(64, 2.0, Dynamics::mp, Articulation::Sustain) };
    ambient.add(duduk, duduk_line, 0.0);

    Melody bell_line = { rest(7.0), Note(84, 1.5, Dynamics::mf) };
    ambient.add(bell, bell_line, 0.0);

    wauvio::StereoBuffer mix = ambient.render();
    wauvio::save_wav_stereo(mix, "ambient_demo.wav");
    return 0;
}
