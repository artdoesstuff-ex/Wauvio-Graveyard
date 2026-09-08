// =============================================================================
//  audio_preview.cpp
//  CLI test tool for all procedural audio in the game.
//
//  Compile (Linux):
//    g++ -std=c++17 -O2 audio_preview.cpp -o audio_preview
//
//  Compile (Windows):
//    cl /std:c++17 /O2 audio_preview.cpp /link winmm.lib
//
//  Usage:
//    ./audio_preview
//
//  No game engine, no SFML.  Only wauvio.hpp and the generator headers.
//  wauvio.hpp and all wauvio_*.hpp files must be in the same directory
//  as this file, or adjust the include paths below.
// =============================================================================

// ---- Engine -----------------------------------------------------------------
#include "../wauvio.hpp"

// ---- Ambience ----------------------------------------------------------------
#include "wauvio_ambience.hpp"

// ---- Soundtracks: Sequence 1 ------------------------------------------------
#include "wauvio_soundtrack_sequence1_tutorial.hpp"
#include "wauvio_soundtrack_sequence1_1.hpp"
#include "wauvio_soundtrack_sequence1_2.hpp"
#include "wauvio_soundtrack_sequence1_cave_shift.hpp"
#include "wauvio_soundtrack_sequence1_final.hpp"

// ---- Soundtracks: Sequence 2 ------------------------------------------------
#include "wauvio_soundtrack_sequence2_1.hpp"
#include "wauvio_soundtrack_sequence2_2.hpp"
#include "wauvio_soundtrack_sequence2_3.hpp"
#include "wauvio_soundtrack_sequence2_intense.hpp"
#include "wauvio_soundtrack_sequence2_intense_final.hpp"

// ---- Soundtracks: Surface / Exploration ------------------------------------
#include "wauvio_soundtrack_surface_1.hpp"
#include "wauvio_soundtrack_surface_2.hpp"
#include "wauvio_soundtrack_surface_3.hpp"
#include "wauvio_soundtrack_surface_4.hpp"
#include "wauvio_soundtrack_surface_dread.hpp"
#include "wauvio_soundtrack_surface_intense.hpp"

// ---- SFX: Exploration entities ----------------------------------------------
#include "wauvio_sfx_entities.hpp"

// ---- SFX: Sequence 2 entities -----------------------------------------------
#include "wauvio_sfx_seq2_entities.hpp"

// ---- SFX: System sounds -----------------------------------------------------
#include "wauvio_sfx_system.hpp"

// ---- Standard library -------------------------------------------------------
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

// =============================================================================
//  UTILITIES
// =============================================================================

namespace preview {

// ANSI colour codes (work on Linux; gracefully ignored on Windows if not VT)
const char* RESET   = "\033[0m";
const char* BOLD    = "\033[1m";
const char* DIM     = "\033[2m";
const char* CYAN    = "\033[36m";
const char* YELLOW  = "\033[33m";
const char* GREEN   = "\033[32m";
const char* RED     = "\033[31m";
const char* MAGENTA = "\033[35m";
const char* WHITE   = "\033[37m";

void print_header(const char* title) {
    std::cout << "\n" << BOLD << CYAN
              << "══════════════════════════════════════════════\n"
              << "  " << title << "\n"
              << "══════════════════════════════════════════════"
              << RESET << "\n\n";
}

void print_section(const char* s) {
    std::cout << BOLD << YELLOW << "  ── " << s << " ──" << RESET << "\n";
}

void print_item(int idx, const char* label, const char* note = "") {
    std::cout << "  " << BOLD << WHITE << "[" << std::setw(2) << idx << "]" << RESET
              << "  " << label;
    if (note && note[0])
        std::cout << "  " << DIM << note << RESET;
    std::cout << "\n";
}

void print_playing(const char* name) {
    std::cout << "\n  " << GREEN << "▶ generating + playing: " << BOLD << name << RESET << "\n";
    std::cout << "    " << DIM << "(press Enter after playback to continue)" << RESET << "\n";
}

void print_done() {
    std::cout << "  " << DIM << "done.\n" << RESET;
}

void print_error(const char* msg) {
    std::cout << "  " << RED << "✗ " << msg << RESET << "\n";
}

void wait_enter() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// ---------------------------------------------------------------------------
//  EXPORT MODE STATE
// ---------------------------------------------------------------------------

// When export_mode == true, generated sounds are saved to .wav files instead
// of (or in addition to) being played.  preview_length_s == 0 means full export.
struct ExportSettings {
    bool   export_mode      = false;
    double preview_length_s = 0.0;   // 0 = full; >0 = trim to this many seconds
};

inline ExportSettings& g_export() {
    static ExportSettings s;
    return s;
}

// Forward declaration — export_named is defined after the menu helpers
// (it depends on the export infrastructure added after this namespace block)
void export_named(const char* logical_name, std::function<wauvio::Buffer()> gen);

// Generate buffer, report duration, then play.
// In export mode: saves to .wav instead of playing (calls export_named).
void play_named(const char* name, std::function<wauvio::Buffer()> gen) {
    if (g_export().export_mode) {
        export_named(name, gen);
        return;
    }
    print_playing(name);
    const auto t0 = std::chrono::steady_clock::now();
    wauvio::Buffer buf = gen();
    const auto t1 = std::chrono::steady_clock::now();
    const double gen_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double dur_s  = static_cast<double>(buf.size())
                        / wauvio::global_config().sample_rate;
    std::cout << "    " << DIM
              << "buffer: " << buf.size() << " samples  "
              << "(" << std::fixed << std::setprecision(2) << dur_s << "s)  "
              << "gen: " << std::setprecision(1) << gen_ms << "ms"
              << RESET << "\n";
    wauvio::play(buf);
    print_done();
}

// Pause between simulation steps.
void sim_pause(int ms = 600) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int read_int(int lo, int hi) {
    int v;
    while (true) {
        std::cout << "  " << BOLD << "> " << RESET;
        if (std::cin >> v && v >= lo && v <= hi) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return v;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "  " << RED << "invalid — enter " << lo << "–" << hi << RESET << "\n";
    }
}

// ---------------------------------------------------------------------------
//  DIRECTORY HELPERS
// ---------------------------------------------------------------------------

// Recursively create a directory path (works on Linux and Windows).
inline void ensure_dir(const std::string& path) {
#ifdef _WIN32
    // CreateDirectoryA ignores trailing slashes; iterate segments
    std::string p = path;
    for (size_t i = 1; i < p.size(); ++i) {
        if (p[i] == '/' || p[i] == '\\') {
            std::string seg = p.substr(0, i);
            CreateDirectoryA(seg.c_str(), nullptr);
        }
    }
    CreateDirectoryA(p.c_str(), nullptr);
#else
    std::string cmd = "mkdir -p \"" + path + "\"";
    std::system(cmd.c_str());
#endif
}

// ---------------------------------------------------------------------------
//  FILENAME / PATH HELPERS
// ---------------------------------------------------------------------------

// Convert a logical name like "seq1::tutorial" → "seq1_tutorial"
// Rules: replace "::" with "_", lowercase.
inline std::string logical_to_stem(const std::string& logical) {
    std::string out = logical;
    // Replace every "::" with "_"
    size_t pos;
    while ((pos = out.find("::")) != std::string::npos)
        out.replace(pos, 2, "_");
    // Lowercase
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Map a logical name to its export sub-folder inside exports/.
// Returns a path relative to exports/ with a trailing '/'.
//
// Category routing table:
//   ambience::*                   → ambience/
//   seq1::* / seq2::*             → soundtrack/sequence1/ or soundtrack/sequence2/
//   surface::*                    → soundtrack/surface/
//   lurker / leech / shining /
//   wraith / hollow / stalker /
//   husk  (exploration entities)  → sfx/entities/
//   still_hunter / blink_feeder /
//   torch_hater / mirror /
//   fault_chaser                  → sfx/seq2_entities/
//   flashlight_* / whisper_* /
//   sanity_* / ui_* / soft_* /
//   glitch / presence_*           → sfx/system/
inline std::string logical_to_subfolder(const std::string& logical) {
    // Ambience
    if (logical.rfind("ambience::", 0) == 0)
        return "ambience/";
    // Sequence 1 soundtracks
    if (logical.rfind("seq1::", 0) == 0)
        return "soundtrack/sequence1/";
    // Sequence 2 soundtracks
    if (logical.rfind("seq2::", 0) == 0)
        return "soundtrack/sequence2/";
    // Surface soundtracks
    if (logical.rfind("surface::", 0) == 0)
        return "soundtrack/surface/";
    // Exploration entities
    const char* exploration_ents[] = {
        "lurker::", "leech::", "shining::", "wraith::",
        "hollow::", "stalker::", "husk::", nullptr
    };
    for (int i = 0; exploration_ents[i]; ++i)
        if (logical.rfind(exploration_ents[i], 0) == 0)
            return "sfx/entities/";
    // Seq2 entities
    const char* seq2_ents[] = {
        "still_hunter::", "blink_feeder::", "torch_hater::",
        "mirror::", "fault_chaser::", nullptr
    };
    for (int i = 0; seq2_ents[i]; ++i)
        if (logical.rfind(seq2_ents[i], 0) == 0)
            return "sfx/seq2_entities/";
    // Everything else → system SFX
    return "sfx/system/";
}

// Build the full output path: exports/<subfolder><stem>.wav
inline std::string make_export_path(const std::string& logical) {
    return "exports/" + logical_to_subfolder(logical) + logical_to_stem(logical) + ".wav";
}

// ---------------------------------------------------------------------------
//  EXPORT HELPER  (mirrors play_named; calls wauvio::save_wav directly)
// ---------------------------------------------------------------------------

// Export a mono buffer.  Uses wauvio::save_wav from §28 of wauvio.hpp.
void export_named(const char* logical_name, std::function<wauvio::Buffer()> gen) {
    const std::string path = make_export_path(logical_name);

    // Ensure target directory exists
    const std::string dir = path.substr(0, path.rfind('/'));
    ensure_dir(dir);

    std::cout << "  " << GREEN << "⬇ Exporting: " << BOLD << logical_to_stem(logical_name)
              << ".wav" << RESET << "  " << DIM << "→ " << path << RESET << "\n";

    const auto t0 = std::chrono::steady_clock::now();
    wauvio::Buffer buf = gen();
    const auto t1 = std::chrono::steady_clock::now();
    const double gen_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Optionally trim to preview length
    const double preview_s = g_export().preview_length_s;
    if (preview_s > 0.0) {
        const size_t max_samples = static_cast<size_t>(
            preview_s * wauvio::global_config().sample_rate);
        if (buf.size() > max_samples) {
            buf.resize(max_samples);
            wauvio::fade_out(buf, 0.3, wauvio::global_config().sample_rate);
        }
    }

    const double dur_s = static_cast<double>(buf.size()) / wauvio::global_config().sample_rate;
    std::cout << "    " << DIM
              << "samples: " << buf.size()
              << "  (" << std::fixed << std::setprecision(2) << dur_s << "s)"
              << "  gen: " << std::setprecision(1) << gen_ms << "ms"
              << RESET << "\n";

    // §28 wauvio::save_wav — the library's own WAV writer
    wauvio::save_wav(buf, path, wauvio::global_config().sample_rate);
}

} // namespace preview

// =============================================================================
//  MENU IMPLEMENTATIONS
// =============================================================================

// ---- 1. AMBIENCE ------------------------------------------------------------

void menu_ambience() {
    using namespace preview;
    while (true) {
        print_header("AMBIENCE");
        print_item( 1, "surface",      "cave mouth, wind, sparse drips  [30s]");
        print_item( 2, "cave",         "deep passage, resonant hum       [40s]");
        print_item( 3, "deep",         "oppressive sub, structural groans[50s]");
        print_item( 4, "eerie",        "distorted cave, ring-mod shimmer [35s]");
        print_item( 5, "static_",      "fracture / signal break          [20s]");
        print_item( 0, "back");

        int c = read_int(0, 5);
        if (c == 0) return;

        switch (c) {
        case 1: play_named("ambience::surface",  ambience::surface::gen);  break;
        case 2: play_named("ambience::cave",     ambience::cave::gen);     break;
        case 3: play_named("ambience::deep",     ambience::deep::gen);     break;
        case 4: play_named("ambience::eerie",    ambience::eerie::gen);    break;
        case 5: play_named("ambience::static_",  ambience::static_::gen);  break;
        }
    }
}

// ---- 2. SEQUENCE 1 SOUNDTRACKS ----------------------------------------------

void menu_sequence1() {
    using namespace preview;
    while (true) {
        print_header("SEQUENCE 1 SOUNDTRACKS");
        print_section("Distance-gated progression");
        print_item(1, "tutorial",   "lWalked 0-59   sparse events, dry    [38s]");
        print_item(2, "track 1",    "lWalked 60-159 waiting/presence       [53s]");
        print_item(3, "track 2",    "lWalked 160+   fragmented motion      [47s]");
        print_section("State-triggered");
        print_item(4, "cave_shift", "caveDistort=true tectonic strata      [43s]");
        print_item(5, "final",      "FractureSystem active erosion         [41s]");
        print_item(0, "back");

        int c = read_int(0, 5);
        if (c == 0) return;

        switch (c) {
        case 1: play_named("seq1::tutorial",   soundtrack::sequence1_tutorial::gen);   break;
        case 2: play_named("seq1::1",          soundtrack::sequence1_1::gen);          break;
        case 3: play_named("seq1::2",          soundtrack::sequence1_2::gen);          break;
        case 4: play_named("seq1::cave_shift", soundtrack::sequence1_cave_shift::gen); break;
        case 5: play_named("seq1::final",      soundtrack::sequence1_final::gen);      break;
        }
    }
}

// ---- 3. SEQUENCE 2 SOUNDTRACKS ----------------------------------------------

void menu_sequence2() {
    using namespace preview;
    while (true) {
        print_header("SEQUENCE 2 SOUNDTRACKS");
        print_section("Progressive distance-gated");
        print_item(1, "seq2_1",          "lWalked 0-39   breath-gate           [57s]");
        print_item(2, "seq2_2",          "lWalked 40-99  spectral rotation     [49s]");
        print_item(3, "seq2_3",          "lWalked 100-159 interference field   [36s]");
        print_item(4, "intense",         "lWalked 160-229 pressure accumulation[55s]");
        print_item(5, "intense_final",   "lWalked 230+   rhythmic collapse     [52s]");
        print_item(0, "back");

        int c = read_int(0, 5);
        if (c == 0) return;

        switch (c) {
        case 1: play_named("seq2::1",             soundtrack::sequence2_1::gen);             break;
        case 2: play_named("seq2::2",             soundtrack::sequence2_2::gen);             break;
        case 3: play_named("seq2::3",             soundtrack::sequence2_3::gen);             break;
        case 4: play_named("seq2::intense",       soundtrack::sequence2_intense::gen);       break;
        case 5: play_named("seq2::intense_final", soundtrack::sequence2_intense_final::gen); break;
        }
    }
}

// ---- 4. SURFACE SOUNDTRACKS -------------------------------------------------

void menu_surface() {
    using namespace preview;
    while (true) {
        print_header("SURFACE / EXPLORATION SOUNDTRACKS");
        print_section("Distance-gated progression");
        print_item(1, "surface_1",      "lWalked 0-49   tidal shimmer         [44s]");
        print_item(2, "surface_2",      "lWalked 50-119 hunting walk          [48s]");
        print_item(3, "surface_3",      "lWalked 120-199 phase lattice        [53s]");
        print_item(4, "surface_4",      "lWalked 200-299 spectral shedding    [51s]");
        print_item(5, "surface_dread",  "lWalked 300-399 convolution ghost    [38s]");
        print_item(6, "surface_intense","lWalked 400+   metric storm          [45s]");
        print_item(0, "back");

        int c = read_int(0, 6);
        if (c == 0) return;

        switch (c) {
        case 1: play_named("surface::1",       soundtrack::surface_1::gen);       break;
        case 2: play_named("surface::2",       soundtrack::surface_2::gen);       break;
        case 3: play_named("surface::3",       soundtrack::surface_3::gen);       break;
        case 4: play_named("surface::4",       soundtrack::surface_4::gen);       break;
        case 5: play_named("surface::dread",   soundtrack::surface_dread::gen);   break;
        case 6: play_named("surface::intense", soundtrack::surface_intense::gen); break;
        }
    }
}

// ---- 5. ENTITY SOUNDS -------------------------------------------------------

void menu_entity_sounds() {
    using namespace preview;
    while (true) {
        print_header("ENTITY SOUNDS");
        print_section("Exploration entities (EntitySystem)");
        print_item( 1, "lurker  — presence",    "peripheral scrape");
        print_item( 2, "lurker  — contact",     "sharp retraction");
        print_item( 3, "lurker  — flee",        "staccato skitter");
        print_item( 4, "leech   — presence",    "rapid light ticks");
        print_item( 5, "leech   — contact",     "single dry click");
        print_item( 6, "leech   — flee",        "diminishing ticks");
        print_item( 7, "shining — presence",    "spectral halo");
        print_item( 8, "shining — blink delete","shimmer contracts");
        print_item( 9, "wraith  — presence",    "orbital dread swell");
        print_item(10, "wraith  — repulse",     "pressure venting");
        print_item(11, "hollow  — presence",    "tubular resonance");
        print_item(12, "hollow  — contact",     "deep concussive thud");
        print_item(13, "stalker — presence",    "diffuse liminal static");
        print_item(14, "stalker — teleport",    "spatial phase inversion");
        print_item(15, "husk    — presence",    "interlocking pack scrape");
        print_item(16, "husk    — contact",     "tri-impact");
        print_section("Sequence 2 entities (Seq2EntitySystem)");
        print_item(17, "still_hunter — approach", "slow AM drone");
        print_item(18, "still_hunter — retreat",  "FM index exhale");
        print_item(19, "blink_feeder — approach", "irregular stabs");
        print_item(20, "blink_feeder — retreat",  "cubic decay burst");
        print_item(21, "torch_hater  — approach", "ascending FM glide");
        print_item(22, "torch_hater  — retreat",  "scorched downward sweep");
        print_item(23, "mirror       — approach", "drifting copy steps");
        print_item(24, "mirror       — retreat",  "copy outlasts original");
        print_item(25, "fault_chaser — approach", "dormancy → explosion");
        print_item(26, "fault_chaser — retreat",  "hard cut, no fade");
        print_item( 0, "back");

        int c = read_int(0, 26);
        if (c == 0) return;

        switch (c) {
        // Exploration
        case  1: play_named("lurker::presence",         ent::gen_lurker_presence);         break;
        case  2: play_named("lurker::contact",          ent::gen_lurker_contact);           break;
        case  3: play_named("lurker::flee",             ent::gen_lurker_flee);              break;
        case  4: play_named("leech::presence",          ent::gen_leech_presence);           break;
        case  5: play_named("leech::contact",           ent::gen_leech_contact);            break;
        case  6: play_named("leech::flee",              ent::gen_leech_flee);               break;
        case  7: play_named("shining::presence",        ent::gen_shining_presence);         break;
        case  8: play_named("shining::blink_delete",    ent::gen_shining_blink_delete);     break;
        case  9: play_named("wraith::presence",         ent::gen_wraith_presence);          break;
        case 10: play_named("wraith::repulse",          ent::gen_wraith_repulse);           break;
        case 11: play_named("hollow::presence",         ent::gen_hollow_presence);          break;
        case 12: play_named("hollow::contact",          ent::gen_hollow_contact);           break;
        case 13: play_named("stalker::presence",        ent::gen_stalker_presence);         break;
        case 14: play_named("stalker::teleport",        ent::gen_stalker_teleport);         break;
        case 15: play_named("husk::presence",           ent::gen_husk_presence);            break;
        case 16: play_named("husk::contact",            ent::gen_husk_contact);             break;
        // Seq2
        case 17: play_named("still_hunter::approach",   sfx::gen_still_hunter_approach);   break;
        case 18: play_named("still_hunter::retreat",    sfx::gen_still_hunter_retreat);    break;
        case 19: play_named("blink_feeder::approach",   sfx::gen_blink_feeder_approach);   break;
        case 20: play_named("blink_feeder::retreat",    sfx::gen_blink_feeder_retreat);    break;
        case 21: play_named("torch_hater::approach",    sfx::gen_torch_hater_approach);    break;
        case 22: play_named("torch_hater::retreat",     sfx::gen_torch_hater_retreat);     break;
        case 23: play_named("mirror::approach",         sfx::gen_mirror_approach);         break;
        case 24: play_named("mirror::retreat",          sfx::gen_mirror_retreat);          break;
        case 25: play_named("fault_chaser::approach",   sfx::gen_fault_chaser_approach);   break;
        case 26: play_named("fault_chaser::retreat",    sfx::gen_fault_chaser_retreat);    break;
        }
    }
}

// ---- 6. SYSTEM SFX ----------------------------------------------------------

void menu_system_sfx() {
    using namespace preview;
    while (true) {
        print_header("SYSTEM SFX");
        print_section("Flashlight");
        print_item(1, "flashlight on",      "0.18s — tactile click + discharge");
        print_item(2, "flashlight off",     "0.28s — pitch fall + mechanical");
        print_item(3, "flashlight flicker", "1.80s — chaotic instability");
        print_item(4, "flashlight fail",    "0.70s — crash + hard cut");
        print_section("Sanity");
        print_item(5, "whisper start",    "0.9s  — breath onset");
        print_item(6, "whisper fragment", "0.4s  — formant syllable");
        print_item(7, "whisper end",      "1.4s  — dissolving presence");
        print_item(8, "sanity spike",     "0.6s  — dissonant pair + hit");
        print_section("UI");
        print_item( 9, "ui confirm",  "0.25s — soft single tone");
        print_item(10, "ui error",    "0.22s — detuned pair");
        print_item(11, "ui tick",     "0.06s — sub-perceptual tap");
        print_section("Environment");
        print_item(12, "soft impact",    "0.28s — low-intensity bump");
        print_item(13, "glitch",         "0.35s — digital artifact");
        print_item(14, "presence rise",  "1.20s — ascending tension");
        print_item( 0, "back");

        int c = read_int(0, 14);
        if (c == 0) return;

        switch (c) {
        case  1: play_named("flashlight_on",       sfx::system::gen_flashlight_on);              break;
        case  2: play_named("flashlight_off",      sfx::system::gen_flashlight_off);             break;
        case  3: play_named("flashlight_flicker",  sfx::system::gen_flashlight_flicker);         break;
        case  4: play_named("flashlight_fail",     sfx::system::gen_flashlight_fail);            break;
        case  5: play_named("whisper_start",       sfx::system::gen_sanity_whisper_start);       break;
        case  6: play_named("whisper_fragment",    sfx::system::gen_sanity_whisper_loop_fragment);break;
        case  7: play_named("whisper_end",         sfx::system::gen_sanity_whisper_end);         break;
        case  8: play_named("sanity_spike",        sfx::system::gen_sanity_spike);               break;
        case  9: play_named("ui_confirm",          sfx::system::gen_ui_confirm);                 break;
        case 10: play_named("ui_error",            sfx::system::gen_ui_error);                   break;
        case 11: play_named("ui_tick",             sfx::system::gen_ui_tick);                    break;
        case 12: play_named("soft_impact",         sfx::system::gen_soft_impact);                break;
        case 13: play_named("glitch",              sfx::system::gen_glitch);                     break;
        case 14: play_named("presence_rise",       sfx::system::gen_presence_rise);              break;
        }
    }
}

// =============================================================================
//  SIMULATION SEQUENCES
// =============================================================================

namespace sim {

using namespace preview;

// ---- Helper: announce step, play, brief pause
void step(const char* label, const char* name, std::function<wauvio::Buffer()> gen,
          int pause_ms = 400)
{
    std::cout << "  " << MAGENTA << "[ " << label << " ]" << RESET
              << "  " << BOLD << name << RESET << "\n";
    const auto t0 = std::chrono::steady_clock::now();
    wauvio::Buffer buf = gen();
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double,std::milli>(t1-t0).count();
    std::cout << "    " << DIM << std::fixed << std::setprecision(1) << ms << "ms gen, "
              << std::setprecision(2)
              << static_cast<double>(buf.size()) / wauvio::global_config().sample_rate
              << "s playback" << RESET << "\n";
    wauvio::play(buf);
    sim_pause(pause_ms);
}

// ---- Simulation 1: Flashlight full malfunction cycle
void sim_flashlight() {
    print_header("SIMULATE — Flashlight Malfunction Cycle");
    std::cout << "  Sequence: ON → flicker (1.8s) → fail (out) → ON again\n\n";

    step("toggle ON",  "flashlight_on",      sfx::system::gen_flashlight_on,      300);
    step("FLICKER",    "flashlight_flicker", sfx::system::gen_flashlight_flicker, 200);
    step("FAIL",       "flashlight_fail",    sfx::system::gen_flashlight_fail,    800);
    step("restore ON", "flashlight_on",      sfx::system::gen_flashlight_on,      300);
    step("toggle OFF", "flashlight_off",     sfx::system::gen_flashlight_off,     200);

    std::cout << "\n  " << GREEN << "✓ flashlight cycle complete" << RESET << "\n";
}

// ---- Simulation 2: Sanity descent
void sim_sanity() {
    print_header("SIMULATE — Sanity Descent");
    std::cout << "  Sequence: whispers begin → 3 fragments → spike → end\n\n";

    step("sanity falls", "whisper_start",    sfx::system::gen_sanity_whisper_start,       500);
    step("text visible", "whisper_fragment", sfx::system::gen_sanity_whisper_loop_fragment,800);
    step("retrigger",    "whisper_fragment", sfx::system::gen_sanity_whisper_loop_fragment,800);
    step("retrigger",    "whisper_fragment", sfx::system::gen_sanity_whisper_loop_fragment,500);
    step("sanity ≤ 3",   "sanity_spike",     sfx::system::gen_sanity_spike,               600);
    step("hallu clears", "whisper_end",      sfx::system::gen_sanity_whisper_end,         200);

    std::cout << "\n  " << GREEN << "✓ sanity cycle complete" << RESET << "\n";
}

// ---- Simulation 3: Seq2 entity spawn and retreat
void sim_entity_spawn() {
    print_header("SIMULATE — Seq2 Entity Spawn / Retreat");
    std::cout << "  Tests all five entity sounds back-to-back\n";
    std::cout << "  Each plays approach, pauses 800ms, then retreat\n\n";

    struct E { const char* name; std::function<wauvio::Buffer()> appr, retr; };
    const E ENTS[] = {
        { "STILL_HUNTER", sfx::gen_still_hunter_approach, sfx::gen_still_hunter_retreat },
        { "BLINK_FEEDER",  sfx::gen_blink_feeder_approach, sfx::gen_blink_feeder_retreat },
        { "TORCH_HATER",  sfx::gen_torch_hater_approach,  sfx::gen_torch_hater_retreat  },
        { "MIRROR",       sfx::gen_mirror_approach,        sfx::gen_mirror_retreat       },
        { "FAULT_CHASER", sfx::gen_fault_chaser_approach,  sfx::gen_fault_chaser_retreat },
    };

    for (const auto& e : ENTS) {
        std::cout << "  " << BOLD << YELLOW << "── " << e.name << " ──" << RESET << "\n";
        step("approach", e.name, e.appr, 800);
        step("retreat",  e.name, e.retr, 600);
        sim_pause(400);
    }

    std::cout << "\n  " << GREEN << "✓ entity spawn/retreat cycle complete" << RESET << "\n";
}

// ---- Simulation 4: Exploration entity escalation
void sim_exploration_entities() {
    print_header("SIMULATE — Exploration Entity Events");
    std::cout << "  Presence → suspicion triggers → contact → outcome\n\n";

    step("LURKER spotted",   "lurker::presence",      ent::gen_lurker_presence,      500);
    step("player looks",     "lurker::flee",           ent::gen_lurker_flee,          400);
    step("LEECH chasing",    "leech::presence",        ent::gen_leech_presence,       500);
    step("leech contacts",   "leech::contact",         ent::gen_leech_contact,        400);
    step("WRAITH enters",    "wraith::presence",       ent::gen_wraith_presence,      600);
    step("blink repels",     "wraith::repulse",        ent::gen_wraith_repulse,       400);
    step("STALKER spawns",   "stalker::presence",      ent::gen_stalker_presence,     500);
    step("player looks →",   "stalker::teleport",      ent::gen_stalker_teleport,     400);
    step("HOLLOW moving",    "hollow::presence",       ent::gen_hollow_presence,      500);
    step("hollow hits",      "hollow::contact",        ent::gen_hollow_contact,       400);
    step("SHINING orbits",   "shining::presence",      ent::gen_shining_presence,     500);
    step("player blinks",    "shining::blink_delete",  ent::gen_shining_blink_delete, 400);
    step("HUSK pack near",   "husk::presence",         ent::gen_husk_presence,        500);
    step("husk pack hits",   "husk::contact",          ent::gen_husk_contact,         300);

    std::cout << "\n  " << GREEN << "✓ exploration entity events complete" << RESET << "\n";
}

// ---- Simulation 5: Sequence 1 soundtrack progression
void sim_seq1_progression() {
    print_header("SIMULATE — Sequence 1 Soundtrack Transitions");
    std::cout << "  Plays first 10 seconds of each track in order\n";
    std::cout << "  (full buffers generated; only first ~10s is audible before skip)\n";
    std::cout << "  Press Enter to advance each transition.\n\n";

    struct T { const char* name; const char* band; std::function<wauvio::Buffer()> gen; };
    const T TRACKS[] = {
        { "seq1::tutorial",   "lWalked 0-59",    soundtrack::sequence1_tutorial::gen   },
        { "seq1::1",          "lWalked 60-159",  soundtrack::sequence1_1::gen          },
        { "seq1::2",          "lWalked 160+",    soundtrack::sequence1_2::gen          },
        { "seq1::cave_shift", "caveDistort=true",soundtrack::sequence1_cave_shift::gen },
        { "seq1::final",      "cracking=true",   soundtrack::sequence1_final::gen      },
    };

    for (const auto& t : TRACKS) {
        std::cout << "  " << MAGENTA << "→ transition to " << BOLD << t.name << RESET
                  << "  " << DIM << "(" << t.band << ")" << RESET << "\n";
        std::cout << "    " << DIM << "generating... " << RESET;
        std::cout.flush();

        wauvio::Buffer buf = t.gen();
        // Trim to first 10 seconds for preview speed
        const size_t preview_len = static_cast<size_t>(10.0 * wauvio::global_config().sample_rate);
        if (buf.size() > preview_len) buf.resize(preview_len);
        wauvio::fade_out(buf, 0.5, wauvio::global_config().sample_rate);

        std::cout << "playing (10s preview)\n";
        wauvio::play(buf);
        std::cout << "  " << DIM << "  [Enter to advance]" << RESET << "\n";
        wait_enter();
    }

    std::cout << "\n  " << GREEN << "✓ sequence 1 progression complete" << RESET << "\n";
}

// ---- Simulation 6: Sequence 2 escalation
void sim_seq2_progression() {
    print_header("SIMULATE — Sequence 2 Progressive Escalation");
    std::cout << "  Simulates walking deeper — five soundtrack transitions\n";
    std::cout << "  Each plays 8 seconds, then transitions with presence_rise cue.\n";
    std::cout << "  Press Enter to advance.\n\n";

    struct T { const char* name; const char* dist; std::function<wauvio::Buffer()> gen; };
    const T TRACKS[] = {
        { "seq2::1",             "lWalked 0-39",    soundtrack::sequence2_1::gen             },
        { "seq2::2",             "lWalked 40-99",   soundtrack::sequence2_2::gen             },
        { "seq2::3",             "lWalked 100-159", soundtrack::sequence2_3::gen             },
        { "seq2::intense",       "lWalked 160-229", soundtrack::sequence2_intense::gen       },
        { "seq2::intense_final", "lWalked 230+",    soundtrack::sequence2_intense_final::gen },
    };

    for (int i = 0; i < 5; ++i) {
        const auto& t = TRACKS[i];
        std::cout << "  " << MAGENTA << "→ " << BOLD << t.name << RESET
                  << "  " << DIM << "(" << t.dist << ")" << RESET << "\n";
        std::cout << "    generating... "; std::cout.flush();

        wauvio::Buffer buf = t.gen();
        const size_t preview_len = static_cast<size_t>(8.0 * wauvio::global_config().sample_rate);
        if (buf.size() > preview_len) buf.resize(preview_len);
        wauvio::fade_out(buf, 0.5, wauvio::global_config().sample_rate);

        std::cout << "playing (8s)\n";
        wauvio::play(buf);

        if (i < 4) {
            std::cout << "  " << CYAN << "  ↑ escalation cue (presence_rise)" << RESET << "\n";
            wauvio::play(sfx::system::gen_presence_rise());
        }
        std::cout << "  " << DIM << "  [Enter to advance]" << RESET << "\n";
        wait_enter();
    }

    std::cout << "\n  " << GREEN << "✓ sequence 2 escalation complete" << RESET << "\n";
}

// ---- Simulation 7: Full gameplay event chain
void sim_gameplay_chain() {
    print_header("SIMULATE — Full Gameplay Event Chain");
    std::cout << "  Simulates a realistic sequence of events:\n";
    std::cout << "  spawn → player walks → flashlight fault → entity retreats\n";
    std::cout << "  → sanity drains → more entities → contact\n\n";
    std::cout << "  " << DIM << "  (~60 seconds total — each step plays fully)" << RESET << "\n\n";
    std::cout << "  " << DIM << "  [Enter to begin]" << RESET; wait_enter();

    // 1. Player enters the space
    step("SCENE",           "presence_rise",        sfx::system::gen_presence_rise,              600);
    step("entity spawns",   "still_hunter::approach",sfx::gen_still_hunter_approach,            400);

    // 2. Player toggles flashlight
    step("FLASHLIGHT ON",   "flashlight_on",         sfx::system::gen_flashlight_on,             500);

    // 3. Flashlight malfunctions
    step("FAULT begins",    "flashlight_flicker",    sfx::system::gen_flashlight_flicker,        300);
    step("FAULT_CHASER activates", "fault_chaser::approach", sfx::gen_fault_chaser_approach,    400);
    step("flashlight dead", "flashlight_fail",        sfx::system::gen_flashlight_fail,          800);

    // 4. Fault chaser retreated (light came back)
    step("light restores",  "flashlight_on",          sfx::system::gen_flashlight_on,            500);
    step("chaser retreats", "fault_chaser::retreat",  sfx::gen_fault_chaser_retreat,             600);

    // 5. Still hunter retreats (player moved)
    step("hunter retreats", "still_hunter::retreat",  sfx::gen_still_hunter_retreat,             500);

    // 6. Sanity dropping
    step("sanity drains",   "whisper_start",          sfx::system::gen_sanity_whisper_start,     400);
    step("text flashing",   "whisper_fragment",       sfx::system::gen_sanity_whisper_loop_fragment,600);
    step("sanity spike",    "sanity_spike",            sfx::system::gen_sanity_spike,             600);

    // 7. Wraith appears
    step("WRAITH orbits",   "wraith::presence",       ent::gen_wraith_presence,                  500);
    step("player blinks",   "wraith::repulse",        ent::gen_wraith_repulse,                   400);

    // 8. Leech steals item
    step("LEECH closes in", "leech::presence",        ent::gen_leech_presence,                   400);
    step("item stolen",     "leech::contact",          ent::gen_leech_contact,                    500);

    // 9. All clear — whispers fade
    step("sanity recovers", "whisper_end",             sfx::system::gen_sanity_whisper_end,       400);
    step("flashlight off",  "flashlight_off",          sfx::system::gen_flashlight_off,           200);

    std::cout << "\n  " << GREEN << BOLD << "✓ gameplay chain complete" << RESET << "\n";
}

} // namespace sim

// ---- Simulation menu --------------------------------------------------------

void menu_simulate() {
    using namespace preview;
    while (true) {
        print_header("SIMULATE GAMEPLAY EVENTS");
        print_item(1, "flashlight malfunction cycle", "ON → flicker → fail → restore");
        print_item(2, "sanity descent",               "whispers → fragments → spike → end");
        print_item(3, "seq2 entity spawn/retreat",    "all 5 behaviors");
        print_item(4, "exploration entity events",    "all 7 entity types");
        print_item(5, "sequence 1 progression",       "tutorial → 1 → 2 → cave_shift → final");
        print_item(6, "sequence 2 escalation",        "1 → 2 → 3 → intense → intense_final");
        print_item(7, "full gameplay event chain",    "~60s realistic sequence");
        print_item(0, "back");

        int c = read_int(0, 7);
        if (c == 0) return;

        switch (c) {
        case 1: sim::sim_flashlight();              break;
        case 2: sim::sim_sanity();                  break;
        case 3: sim::sim_entity_spawn();            break;
        case 4: sim::sim_exploration_entities();    break;
        case 5: sim::sim_seq1_progression();        break;
        case 6: sim::sim_seq2_progression();        break;
        case 7: sim::sim_gameplay_chain();          break;
        }

        std::cout << "\n  " << preview::DIM << "[Enter to continue]" << preview::RESET;
        preview::wait_enter();
    }
}

// =============================================================================
//  EXPORT ALL — calls every generator and saves to the exports/ tree
//  Triggered by main menu option [8].
// =============================================================================

void export_all() {
    using namespace preview;

    print_header("EXPORT ALL AUDIO");
    std::cout << "  Exporting all sounds to exports/ ...\n";
    if (g_export().preview_length_s > 0.0)
        std::cout << "  " << DIM << "(preview mode — trimmed to "
                  << g_export().preview_length_s << "s per file)" << RESET << "\n";
    std::cout << "\n";

    // ── Ambience ────────────────────────────────────────────────────────────
    print_section("Ambience");
    export_named("ambience::surface",  ambience::surface::gen);
    export_named("ambience::cave",     ambience::cave::gen);
    export_named("ambience::deep",     ambience::deep::gen);
    export_named("ambience::eerie",    ambience::eerie::gen);
    export_named("ambience::static_",  ambience::static_::gen);

    // ── Sequence 1 soundtracks ───────────────────────────────────────────────
    print_section("Sequence 1 soundtracks");
    export_named("seq1::tutorial",   soundtrack::sequence1_tutorial::gen);
    export_named("seq1::1",          soundtrack::sequence1_1::gen);
    export_named("seq1::2",          soundtrack::sequence1_2::gen);
    export_named("seq1::cave_shift", soundtrack::sequence1_cave_shift::gen);
    export_named("seq1::final",      soundtrack::sequence1_final::gen);

    // ── Sequence 2 soundtracks ───────────────────────────────────────────────
    print_section("Sequence 2 soundtracks");
    export_named("seq2::1",             soundtrack::sequence2_1::gen);
    export_named("seq2::2",             soundtrack::sequence2_2::gen);
    export_named("seq2::3",             soundtrack::sequence2_3::gen);
    export_named("seq2::intense",       soundtrack::sequence2_intense::gen);
    export_named("seq2::intense_final", soundtrack::sequence2_intense_final::gen);

    // ── Surface soundtracks ──────────────────────────────────────────────────
    print_section("Surface soundtracks");
    export_named("surface::1",       soundtrack::surface_1::gen);
    export_named("surface::2",       soundtrack::surface_2::gen);
    export_named("surface::3",       soundtrack::surface_3::gen);
    export_named("surface::4",       soundtrack::surface_4::gen);
    export_named("surface::dread",   soundtrack::surface_dread::gen);
    export_named("surface::intense", soundtrack::surface_intense::gen);

    // ── Exploration entity SFX ───────────────────────────────────────────────
    print_section("Exploration entities");
    export_named("lurker::presence",      ent::gen_lurker_presence);
    export_named("lurker::contact",       ent::gen_lurker_contact);
    export_named("lurker::flee",          ent::gen_lurker_flee);
    export_named("leech::presence",       ent::gen_leech_presence);
    export_named("leech::contact",        ent::gen_leech_contact);
    export_named("leech::flee",           ent::gen_leech_flee);
    export_named("shining::presence",     ent::gen_shining_presence);
    export_named("shining::blink_delete", ent::gen_shining_blink_delete);
    export_named("wraith::presence",      ent::gen_wraith_presence);
    export_named("wraith::repulse",       ent::gen_wraith_repulse);
    export_named("hollow::presence",      ent::gen_hollow_presence);
    export_named("hollow::contact",       ent::gen_hollow_contact);
    export_named("stalker::presence",     ent::gen_stalker_presence);
    export_named("stalker::teleport",     ent::gen_stalker_teleport);
    export_named("husk::presence",        ent::gen_husk_presence);
    export_named("husk::contact",         ent::gen_husk_contact);

    // ── Seq2 entity SFX ─────────────────────────────────────────────────────
    print_section("Sequence 2 entities");
    export_named("still_hunter::approach", sfx::gen_still_hunter_approach);
    export_named("still_hunter::retreat",  sfx::gen_still_hunter_retreat);
    export_named("blink_feeder::approach", sfx::gen_blink_feeder_approach);
    export_named("blink_feeder::retreat",  sfx::gen_blink_feeder_retreat);
    export_named("torch_hater::approach",  sfx::gen_torch_hater_approach);
    export_named("torch_hater::retreat",   sfx::gen_torch_hater_retreat);
    export_named("mirror::approach",       sfx::gen_mirror_approach);
    export_named("mirror::retreat",        sfx::gen_mirror_retreat);
    export_named("fault_chaser::approach", sfx::gen_fault_chaser_approach);
    export_named("fault_chaser::retreat",  sfx::gen_fault_chaser_retreat);

    // ── System SFX ──────────────────────────────────────────────────────────
    print_section("System SFX");
    export_named("flashlight_on",       sfx::system::gen_flashlight_on);
    export_named("flashlight_off",      sfx::system::gen_flashlight_off);
    export_named("flashlight_flicker",  sfx::system::gen_flashlight_flicker);
    export_named("flashlight_fail",     sfx::system::gen_flashlight_fail);
    export_named("whisper_start",       sfx::system::gen_sanity_whisper_start);
    export_named("whisper_fragment",    sfx::system::gen_sanity_whisper_loop_fragment);
    export_named("whisper_end",         sfx::system::gen_sanity_whisper_end);
    export_named("sanity_spike",        sfx::system::gen_sanity_spike);
    export_named("ui_confirm",          sfx::system::gen_ui_confirm);
    export_named("ui_error",            sfx::system::gen_ui_error);
    export_named("ui_tick",             sfx::system::gen_ui_tick);
    export_named("soft_impact",         sfx::system::gen_soft_impact);
    export_named("glitch",              sfx::system::gen_glitch);
    export_named("presence_rise",       sfx::system::gen_presence_rise);

    std::cout << "\n  " << GREEN << BOLD << "✓ Done. " << RESET
              << DIM << "All files written to exports/" << RESET << "\n";
}

// =============================================================================
//  MAIN MENU
// =============================================================================

int main() {
    using namespace preview;

    // Ensure default sample rate is set
    wauvio::global_config().sample_rate = 44100;
    wauvio::global_config().channels    = 1;
    wauvio::global_config().bits        = 16;

    // -------------------------------------------------------------------------
    //  STARTUP — Mode selection
    //  Choose between Preview (play sounds) and Export WAV modes.
    // -------------------------------------------------------------------------
    print_header("AUDIO PREVIEW — Procedural Audio Test CLI");
    std::cout << "  " << DIM << "wauvio.hpp — header-only procedural synthesis\n"
              << "  All sounds generated at runtime — no assets loaded.\n\n" << RESET;

    print_section("Select Mode");
    print_item(1, "Preview",     "generate + play sounds");
    print_item(2, "Export WAV",  "generate + save .wav files (no playback)");
    std::cout << "\n";

    int mode = read_int(1, 2);

    if (mode == 2) {
        // Export mode — choose full vs. preview-length
        g_export().export_mode = true;
        std::cout << "\n";
        print_section("Export Length");
        print_item(1, "Full export",    "save entire buffer as generated");
        print_item(2, "Preview export", "trim to 10 seconds (smaller files)");
        std::cout << "\n";
        int len_choice = read_int(1, 2);
        if (len_choice == 2) g_export().preview_length_s = 10.0;

        std::cout << "\n  " << CYAN << "Export mode active." << RESET
                  << "  Files will be written to " << BOLD << "exports/" << RESET << "\n";
        if (g_export().preview_length_s > 0.0)
            std::cout << "  " << DIM << "(trimmed to "
                      << g_export().preview_length_s << "s per file)" << RESET << "\n";
    } else {
        std::cout << "\n  " << CYAN << "Preview mode active." << RESET
                  << "  Sounds will be played via system audio.\n";
    }

    // -------------------------------------------------------------------------
    //  MAIN MENU LOOP
    // -------------------------------------------------------------------------

    while (true) {
        print_header("AUDIO PREVIEW — Procedural Audio Test CLI");

        if (g_export().export_mode) {
            std::cout << "  " << MAGENTA << "Mode: " << BOLD << "EXPORT WAV"
                      << RESET << MAGENTA
                      << (g_export().preview_length_s > 0.0 ? "  (preview length)" : "  (full length)")
                      << RESET << "\n\n";
        } else {
            std::cout << "  " << CYAN << "Mode: " << BOLD << "PREVIEW (play)"
                      << RESET << "\n\n";
        }

        print_item(1, "Ambience",        "5 zones: surface, cave, deep, eerie, static");
        print_item(2, "Sequence 1",      "5 soundtracks (tutorial, 1, 2, cave_shift, final)");
        print_item(3, "Sequence 2",      "5 soundtracks (1–3, intense, intense_final)");
        print_item(4, "Surface",         "6 soundtracks (1–4, dread, intense)");
        print_item(5, "Entity sounds",   "26 sounds: 7 exploration + 5 seq2 × approach/retreat");
        print_item(6, "System SFX",      "14 sounds: flashlight, sanity, UI, environment");
        print_item(7, "Simulate",        "7 gameplay event sequences");
        print_item(8, "Export ALL audio","generate + save every sound to exports/");
        print_item(0, "quit");

        int c = read_int(0, 8);
        if (c == 0) {
            std::cout << "\n  " << DIM << "exiting." << RESET << "\n\n";
            return 0;
        }

        switch (c) {
        case 1: menu_ambience();       break;
        case 2: menu_sequence1();      break;
        case 3: menu_sequence2();      break;
        case 4: menu_surface();        break;
        case 5: menu_entity_sounds();  break;
        case 6: menu_system_sfx();     break;
        case 7: menu_simulate();       break;
        case 8:
            // Export All is available in both modes; always exports (never plays)
            export_all();
            std::cout << "\n  " << DIM << "[Enter to continue]" << RESET;
            wait_enter();
            break;
        }
    }
}