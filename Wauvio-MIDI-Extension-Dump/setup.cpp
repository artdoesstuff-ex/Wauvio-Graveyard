#include "wauvio.hpp"
#include "wauvio_ext.hpp"
#include "wauvio_midi.hpp"

int main() {
    auto music = wauvio::track::load_midi(".mid");
    wauvio::save_wav_stereo(music.render(), "track.wav");
}