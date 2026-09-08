#include "wauvio.hpp"
#include "wauvio_ext.hpp"

using namespace wauvio::audio;

int main() {
    auto strings  = wauvio::instruments::FullStringSection();
    auto oboe     = wauvio::instruments::Oboe();
    auto horn     = wauvio::instruments::FrenchHorn();
    auto timpani  = wauvio::instruments::Timpani();
    auto glock    = wauvio::instruments::Glockenspiel();
    auto cello    = wauvio::instruments::Cello();

    strings.reverb(0.7f, 0.28f);
    horn.reverb(0.6f, 0.2f);
    oboe.reverb(0.5f, 0.18f);

    Arrangement piece;
    piece.tempo = Tempo(76.0);
    piece.master_reverb(0.6f, 0.18f);
    piece.limiter(0.92f);

    auto a_minor = scale::natural_minor(57, 1);
    Melody string_line;
    for (size_t i = 0; i < a_minor.size(); ++i)
        string_line.push_back(Note(a_minor[i], 0.6, Dynamics::mp));
    piece.add(strings, string_line, 0.0);

    Melody cello_line = {
        Note(45, 1.2, Dynamics::mf),
        Note(48, 1.2, Dynamics::mf),
        Note(52, 2.0, Dynamics::f),
    };
    piece.add(cello, cello_line, 0.5);

    Melody oboe_line = {
        rest(1.0),
        Note(69, 0.5, Dynamics::mf, Articulation::Legato),
        Note(72, 0.5, Dynamics::mf, Articulation::Legato),
        Note(76, 1.0, Dynamics::f,  Articulation::Sustain),
    };
    piece.add(oboe, oboe_line, 0.0);

    Melody horn_line = { rest(2.5), Note(57, 1.5, Dynamics::f, Articulation::Marcato) };
    piece.add(horn, horn_line, 0.0);

    Melody glock_line = { rest(3.0), Note(81, 0.3, Dynamics::mf), Note(84, 0.3, Dynamics::mf),
                           Note(88, 0.6, Dynamics::f) };
    piece.add(glock, glock_line, 0.0);

    Track timp(timpani);
    timp.add(Note(45, 0.4, Dynamics::f));
    timp.rest(2.6);
    timp.add(Note(45, 0.6, Dynamics::ff));
    piece.add_track(timp, 0.0);

    wauvio::StereoBuffer mix = piece.render();
    wauvio::save_wav_stereo(mix, "orchestral_demo.wav");
    return 0;
}
