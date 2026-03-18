#include "installation_director.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

// These are defined in clift_engine.c — we access the global vj struct
// and call functions through extern declarations
extern void director_apply_to_engine(Director* dir);

// Mode duration ranges [min, max] in seconds
// Testing durations (short). For production, multiply by ~5-10x.
static const float mode_durations[][2] = {
    [DIR_BOOT]               = {4.0f, 6.0f},
    [DIR_AMBIENT]            = {6.0f, 12.0f},
    [DIR_NEWS_FEED]          = {5.0f, 10.0f},
    [DIR_MASTODON_INTERCEPT] = {5.0f, 10.0f},
    [DIR_STOCK_CRASH]        = {5.0f, 8.0f},
    [DIR_RECOVERY]           = {5.0f, 10.0f},
    [DIR_ERROR_CASCADE]      = {3.0f, 6.0f},
    [DIR_DRIFT]              = {4.0f, 7.0f},
    [DIR_WIFI_SURVEY]        = {4.0f, 8.0f},
    [DIR_SERVER_MOCKERY]     = {5.0f, 9.0f},
    [DIR_NETWORK_MAP]        = {5.0f, 10.0f},
    [DIR_WIREFRAME]          = {5.0f, 10.0f},
    [DIR_ROGUELIKE]          = {6.0f, 12.0f},
    [DIR_BIG_TEXT]            = {3.0f, 6.0f},
    [DIR_SCIFI]              = {5.0f, 10.0f},
    [DIR_SURVEILLANCE]       = {5.0f, 10.0f},
    [DIR_CPU_SCHEMATIC]      = {5.0f, 10.0f},
    [DIR_AUDIO_DASH]         = {5.0f, 10.0f},
    [DIR_DATA_TRANSFER]      = {5.0f, 8.0f},
    [DIR_WAFER_MAP]          = {5.0f, 10.0f},
    [DIR_SERVER_ROOM]        = {5.0f, 10.0f},
    [DIR_PANOPTICON]         = {4.0f, 8.0f},
    [DIR_SPLIT_DASH]         = {6.0f, 12.0f},
    [DIR_LORE_NARRATIVE]     = {5.0f, 9.0f},
    [DIR_HEX_DUMP]           = {4.0f, 8.0f},
    [DIR_MOTION_ANALYZER]    = {4.0f, 8.0f},
    [DIR_CONSCIOUSNESS]      = {6.0f, 10.0f},
    [DIR_VERTICAL_SCROLLER]  = {4.0f, 8.0f},
    [DIR_EXPLOSION_MONTAGE]  = {3.0f, 6.0f},
    [DIR_CRIC_META]          = {5.0f, 9.0f},
    [DIR_SOCIAL_FEED]        = {5.0f, 10.0f},
    [DIR_PROPAGANDA]         = {4.0f, 8.0f},
    [DIR_POETRY]             = {5.0f, 9.0f},
    [DIR_FACTION_WAR]        = {4.0f, 8.0f},
    [DIR_CODE_RAIN]          = {4.0f, 8.0f},
    [DIR_TIMELINE_SCROLL]    = {5.0f, 10.0f},
    [DIR_NEWS_WALL]          = {5.0f, 10.0f},
    [DIR_SYSTEM_OVERLOAD]    = {3.0f, 5.0f},
    [DIR_SPACE_BATTLE]       = {4.0f, 8.0f},
    [DIR_TAZ]                = {5.0f, 9.0f},
    [DIR_RETRO_ARCADE]       = {4.0f, 8.0f},
    [DIR_SIMULACRA]          = {5.0f, 10.0f},
    [DIR_RHIZOME]            = {5.0f, 9.0f},
    [DIR_TRUTH_MACHINE]      = {4.0f, 7.0f},
    [DIR_BIOMETRIC]          = {3.0f, 6.0f},
    [DIR_BELIEF_ENGINE]      = {5.0f, 9.0f},
    [DIR_TETRIS_RAIN]        = {4.0f, 8.0f},
    [DIR_FLASH_MANIFESTO]    = {2.0f, 5.0f},
    [DIR_DISCIPLINE]         = {5.0f, 10.0f},
    [DIR_PIRATE_RADIO]       = {4.0f, 8.0f},
    [DIR_FINAL_WARNING]      = {2.0f, 4.0f},
    [DIR_CRYPTO_TICKER]      = {5.0f, 10.0f},
    [DIR_NEURAL_FUSION]      = {5.0f, 9.0f},
    [DIR_EUROPA_DESCENT]     = {6.0f, 12.0f},
    [DIR_ECOSYSTEM]          = {5.0f, 10.0f},
    [DIR_MARKET_MELTDOWN]    = {3.0f, 6.0f},
    [DIR_BARCELONA]          = {5.0f, 9.0f},
    [DIR_DNA_SEQUENCER]      = {5.0f, 10.0f},
    [DIR_BLOCKCHAIN]         = {4.0f, 8.0f},
    [DIR_LOVE_VIRUS]         = {5.0f, 9.0f},
    [DIR_PARTICLE_ACCEL]     = {4.0f, 8.0f},
    [DIR_EDSA_CONTROL]       = {5.0f, 10.0f},
    [DIR_QUANTUM_FIELD]      = {5.0f, 10.0f},
    [DIR_SERVER_DIAG]        = {4.0f, 8.0f},
    [DIR_CLIMATE]            = {5.0f, 10.0f},
    [DIR_CRASH_VOICE]        = {4.0f, 8.0f},
    [DIR_SPECTRAL]           = {4.0f, 8.0f},
    [DIR_NETWORK_TOPO]       = {5.0f, 10.0f},
    [DIR_MEMORY_PALACE]      = {4.0f, 8.0f},
    [DIR_COSMIC_BG]          = {6.0f, 12.0f},
    [DIR_REISUB]             = {5.0f, 10.0f},
    [DIR_WORLD_MAP]          = {6.0f, 12.0f},
    [DIR_COUNTRY_INTEL]      = {5.0f, 10.0f},
    [DIR_GEO_DRIFT]          = {5.0f, 9.0f},
    [DIR_SERVER_SPEAKS]      = {3.0f, 5.0f},
    [DIR_SERVER_FACE]        = {6.0f, 12.0f},
    [DIR_ORGANIC_EYE]        = {5.0f, 10.0f},
    [DIR_FACE_GALLERY]       = {5.0f, 9.0f},
    [DIR_HUMAN_FIGURES]      = {5.0f, 8.0f},
    [DIR_FACE_MORPH]         = {5.0f, 10.0f},
    [DIR_LIVE_SPECTRUM]      = {8.0f, 16.0f},
    [DIR_LIVE_GRID]          = {8.0f, 16.0f},
    [DIR_LIVE_PULSE]         = {8.0f, 16.0f},
    [DIR_SURFACE_WASH]       = {8.0f, 15.0f},
    [DIR_SURFACE_AURORA]     = {10.0f, 18.0f},
    [DIR_SURFACE_PULSE]      = {6.0f, 12.0f},
};

// Mode weights at low intensity (AMBIENT, DRIFT, RECOVERY, WIREFRAME favored)
static const float mode_weights_low[] = {
    [DIR_BOOT]               = 0.0f,   // never re-enter boot
    [DIR_AMBIENT]            = 2.5f,
    [DIR_NEWS_FEED]          = 0.5f,
    [DIR_MASTODON_INTERCEPT] = 0.5f,
    [DIR_STOCK_CRASH]        = 0.2f,
    [DIR_RECOVERY]           = 1.5f,
    [DIR_ERROR_CASCADE]      = 0.1f,
    [DIR_DRIFT]              = 2.5f,
    [DIR_WIFI_SURVEY]        = 0.5f,
    [DIR_SERVER_MOCKERY]     = 1.0f,
    [DIR_NETWORK_MAP]        = 1.0f,
    [DIR_WIREFRAME]          = 2.0f,
    [DIR_ROGUELIKE]          = 1.5f,
    [DIR_BIG_TEXT]            = 1.5f,
    [DIR_SCIFI]              = 2.0f,
    [DIR_SURVEILLANCE]       = 0.5f,
    [DIR_CPU_SCHEMATIC]      = 1.5f,
    [DIR_AUDIO_DASH]         = 0.5f,
    [DIR_DATA_TRANSFER]      = 0.5f,
    [DIR_WAFER_MAP]          = 1.5f,
    [DIR_SERVER_ROOM]        = 0.8f,
    [DIR_PANOPTICON]         = 0.5f,
    [DIR_SPLIT_DASH]         = 0.8f,
    [DIR_LORE_NARRATIVE]     = 2.0f,
    [DIR_HEX_DUMP]           = 1.5f,
    [DIR_MOTION_ANALYZER]    = 0.5f,
    [DIR_CONSCIOUSNESS]      = 2.5f,
    [DIR_VERTICAL_SCROLLER]  = 0.5f,
    [DIR_EXPLOSION_MONTAGE]  = 0.2f,
    [DIR_CRIC_META]          = 1.5f,
    [DIR_SOCIAL_FEED]        = 1.0f,
    [DIR_PROPAGANDA]         = 0.8f,
    [DIR_POETRY]             = 2.0f,
    [DIR_FACTION_WAR]        = 0.3f,
    [DIR_CODE_RAIN]          = 1.5f,
    [DIR_TIMELINE_SCROLL]    = 1.5f,
    [DIR_NEWS_WALL]          = 0.8f,
    [DIR_SYSTEM_OVERLOAD]    = 0.1f,
    [DIR_SPACE_BATTLE]       = 0.5f,
    [DIR_TAZ]                = 2.0f,
    [DIR_RETRO_ARCADE]       = 1.5f,
    [DIR_SIMULACRA]          = 2.0f,
    [DIR_RHIZOME]            = 2.5f,
    [DIR_TRUTH_MACHINE]      = 0.5f,
    [DIR_BIOMETRIC]          = 0.3f,
    [DIR_BELIEF_ENGINE]      = 2.0f,
    [DIR_TETRIS_RAIN]        = 1.5f,
    [DIR_FLASH_MANIFESTO]    = 0.2f,
    [DIR_DISCIPLINE]         = 1.5f,
    [DIR_PIRATE_RADIO]       = 2.0f,
    [DIR_FINAL_WARNING]      = 0.1f,
    [DIR_CRYPTO_TICKER]      = 1.0f,
    [DIR_NEURAL_FUSION]      = 1.5f,
    [DIR_EUROPA_DESCENT]     = 2.5f,
    [DIR_ECOSYSTEM]          = 2.0f,
    [DIR_MARKET_MELTDOWN]    = 0.3f,
    [DIR_BARCELONA]          = 1.0f,
    [DIR_DNA_SEQUENCER]      = 2.0f,
    [DIR_BLOCKCHAIN]         = 1.0f,
    [DIR_LOVE_VIRUS]         = 2.0f,
    [DIR_PARTICLE_ACCEL]     = 1.5f,
    [DIR_EDSA_CONTROL]       = 0.5f,
    [DIR_QUANTUM_FIELD]      = 2.5f,
    [DIR_SERVER_DIAG]        = 1.5f,
    [DIR_CLIMATE]            = 1.5f,
    [DIR_CRASH_VOICE]        = 1.0f,
    [DIR_SPECTRAL]           = 2.0f,
    [DIR_NETWORK_TOPO]       = 1.5f,
    [DIR_MEMORY_PALACE]      = 2.0f,
    [DIR_COSMIC_BG]          = 2.5f,
    [DIR_REISUB]             = 1.5f,
    [DIR_WORLD_MAP]          = 1.5f,
    [DIR_COUNTRY_INTEL]      = 1.5f,
    [DIR_GEO_DRIFT]          = 2.5f,
    [DIR_SERVER_SPEAKS]      = 0.0f,  // never random, only via interlude injection
    [DIR_SERVER_FACE]        = 1.5f,
    [DIR_ORGANIC_EYE]        = 2.0f,
    [DIR_FACE_GALLERY]       = 1.5f,
    [DIR_HUMAN_FIGURES]      = 2.0f,
    [DIR_FACE_MORPH]         = 1.0f,
    [DIR_LIVE_SPECTRUM]      = 0.0f,
    [DIR_LIVE_GRID]          = 0.0f,
    [DIR_LIVE_PULSE]         = 0.0f,
    [DIR_SURFACE_WASH]       = 3.0f,
    [DIR_SURFACE_AURORA]     = 3.0f,
    [DIR_SURFACE_PULSE]      = 2.0f,
};

// Mode weights at high intensity (NEWS, MASTODON, ERROR, STOCK, MOCKERY favored)
static const float mode_weights_high[] = {
    [DIR_BOOT]               = 0.0f,
    [DIR_AMBIENT]            = 0.5f,
    [DIR_NEWS_FEED]          = 2.5f,
    [DIR_MASTODON_INTERCEPT] = 2.5f,
    [DIR_STOCK_CRASH]        = 2.0f,
    [DIR_RECOVERY]           = 0.5f,
    [DIR_ERROR_CASCADE]      = 2.0f,
    [DIR_DRIFT]              = 0.3f,
    [DIR_WIFI_SURVEY]        = 1.0f,
    [DIR_SERVER_MOCKERY]     = 2.0f,
    [DIR_NETWORK_MAP]        = 1.5f,
    [DIR_WIREFRAME]          = 1.0f,
    [DIR_ROGUELIKE]          = 1.0f,
    [DIR_BIG_TEXT]            = 2.0f,
    [DIR_SCIFI]              = 1.5f,
    [DIR_SURVEILLANCE]       = 2.0f,
    [DIR_CPU_SCHEMATIC]      = 0.8f,
    [DIR_AUDIO_DASH]         = 2.0f,
    [DIR_DATA_TRANSFER]      = 2.0f,
    [DIR_WAFER_MAP]          = 0.5f,
    [DIR_SERVER_ROOM]        = 2.0f,
    [DIR_PANOPTICON]         = 2.0f,
    [DIR_SPLIT_DASH]         = 2.0f,
    [DIR_LORE_NARRATIVE]     = 0.5f,
    [DIR_HEX_DUMP]           = 1.0f,
    [DIR_MOTION_ANALYZER]    = 2.0f,
    [DIR_CONSCIOUSNESS]      = 0.3f,
    [DIR_VERTICAL_SCROLLER]  = 2.0f,
    [DIR_EXPLOSION_MONTAGE]  = 2.5f,
    [DIR_CRIC_META]          = 1.0f,
    [DIR_SOCIAL_FEED]        = 2.0f,
    [DIR_PROPAGANDA]         = 2.0f,
    [DIR_POETRY]             = 0.5f,
    [DIR_FACTION_WAR]        = 2.5f,
    [DIR_CODE_RAIN]          = 1.5f,
    [DIR_TIMELINE_SCROLL]    = 1.0f,
    [DIR_NEWS_WALL]          = 2.5f,
    [DIR_SYSTEM_OVERLOAD]    = 2.0f,
    [DIR_SPACE_BATTLE]       = 2.5f,
    [DIR_TAZ]                = 1.0f,
    [DIR_RETRO_ARCADE]       = 2.0f,
    [DIR_SIMULACRA]          = 1.5f,
    [DIR_RHIZOME]            = 0.5f,
    [DIR_TRUTH_MACHINE]      = 2.5f,
    [DIR_BIOMETRIC]          = 2.5f,
    [DIR_BELIEF_ENGINE]      = 1.5f,
    [DIR_TETRIS_RAIN]        = 2.0f,
    [DIR_FLASH_MANIFESTO]    = 2.5f,
    [DIR_DISCIPLINE]         = 2.0f,
    [DIR_PIRATE_RADIO]       = 1.5f,
    [DIR_FINAL_WARNING]      = 2.0f,
    [DIR_CRYPTO_TICKER]      = 2.0f,
    [DIR_NEURAL_FUSION]      = 1.5f,
    [DIR_EUROPA_DESCENT]     = 0.5f,
    [DIR_ECOSYSTEM]          = 1.0f,
    [DIR_MARKET_MELTDOWN]    = 2.5f,
    [DIR_BARCELONA]          = 2.0f,
    [DIR_DNA_SEQUENCER]      = 1.0f,
    [DIR_BLOCKCHAIN]         = 2.0f,
    [DIR_LOVE_VIRUS]         = 1.5f,
    [DIR_PARTICLE_ACCEL]     = 2.0f,
    [DIR_EDSA_CONTROL]       = 2.5f,
    [DIR_QUANTUM_FIELD]      = 0.8f,
    [DIR_SERVER_DIAG]        = 2.0f,
    [DIR_CLIMATE]            = 1.5f,
    [DIR_CRASH_VOICE]        = 2.5f,
    [DIR_SPECTRAL]           = 1.5f,
    [DIR_NETWORK_TOPO]       = 1.5f,
    [DIR_MEMORY_PALACE]      = 1.0f,
    [DIR_COSMIC_BG]          = 0.5f,
    [DIR_REISUB]             = 2.0f,
    [DIR_WORLD_MAP]          = 2.0f,
    [DIR_COUNTRY_INTEL]      = 1.5f,
    [DIR_GEO_DRIFT]          = 0.5f,
    [DIR_SERVER_SPEAKS]      = 0.0f,  // never random, only via interlude injection
    [DIR_SERVER_FACE]        = 2.0f,
    [DIR_ORGANIC_EYE]        = 2.5f,
    [DIR_FACE_GALLERY]       = 2.0f,
    [DIR_HUMAN_FIGURES]      = 1.5f,
    [DIR_FACE_MORPH]         = 1.5f,
    [DIR_LIVE_SPECTRUM]      = 0.0f,
    [DIR_LIVE_GRID]          = 0.0f,
    [DIR_LIVE_PULSE]         = 0.0f,
    [DIR_SURFACE_WASH]       = 3.0f,
    [DIR_SURFACE_AURORA]     = 3.0f,
    [DIR_SURFACE_PULSE]      = 3.0f,
};

// Scene pools for ambient mode (existing scenes that look good autonomous)
static const int ambient_scenes[] = {
    // Original core set
    9,   // matrix rain
    10,  // tunnels
    11,  // kaleidoscope
    12,  // mandala
    20,  // fire simulation
    30,  // starfield
    40,  // binary waterfall
    50,  // sine landscape
    53,  // wormhole
    60,  // particle explosion
    70,  // lissajous
    80,  // circuit board
    90,  // terrain
    100, // plasma
    110, // waveform
    120, // digital rain
    130, // pixel sort
    140, // fractal zoom
    150, // data stream
    160, // neural net
    170, // geometric pulse
    180, // abstract flow
    // Expanded set — known-good autonomous scenes
    1, 2, 3, 4, 5, 6, 7, 8,
    13, 14, 15, 16, 17, 18, 19,
    21, 22, 23, 24, 25, 26, 27, 28, 29,
    31, 32, 33, 34, 35, 36, 37, 38, 39,
    41, 42, 43, 44, 45, 46, 47, 48, 49,
    51, 52, 54, 55, 56, 57, 58, 59,
    61, 62, 63, 64, 65, 66, 67, 68, 69,
    71, 72, 73, 74, 75, 76, 77, 78, 79,
    81, 82, 83, 84, 85, 86, 87, 88, 89,
    91, 92, 93, 94, 95, 96, 97, 98, 99,
    101, 102, 103, 104, 105,
    111, 112, 113, 114, 115,
    121, 122, 123, 124, 125,
    131, 132, 133, 134, 135,
    141, 142, 143, 144, 145, 146, 147, 148, 149,
    151, 152, 153, 154, 155, 156, 157, 158, 159,
    161, 162, 163, 164, 165, 166, 167, 168, 169,
    171, 172, 173, 174, 175, 176, 177, 178, 179,
    181, 182, 183, 184, 185, 186, 187, 188, 189,
    106, 107, 108, 109,
    116, 117, 118, 119,
    126, 127, 128, 129,
    136, 137, 138, 139,
};
#define AMBIENT_SCENE_COUNT (sizeof(ambient_scenes) / sizeof(ambient_scenes[0]))

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float randf_range(float min, float max) {
    return min + randf() * (max - min);
}

// Narrative act weight multiplier: biases scene selection per act
// Returns multiplier (0.2 = suppress, 1.0 = neutral, 2.5-3.0 = boost)
static float act_weight_multiplier(DirectorMode mode, int act) {
    switch (act) {
        case 0: // ASSERTION — cold, dominant
            switch (mode) {
                case DIR_SURVEILLANCE: case DIR_DISCIPLINE: case DIR_SERVER_ROOM:
                case DIR_PANOPTICON: case DIR_SERVER_DIAG: case DIR_SERVER_MOCKERY:
                case DIR_BIOMETRIC: case DIR_TRUTH_MACHINE:
                case DIR_SERVER_FACE: case DIR_ORGANIC_EYE: case DIR_FACE_GALLERY:
                    return 2.5f;
                case DIR_POETRY: case DIR_DRIFT: case DIR_LOVE_VIRUS:
                case DIR_CONSCIOUSNESS: case DIR_PIRATE_RADIO: case DIR_COSMIC_BG:
                    return 0.2f;
                default: return 1.0f;
            }
        case 1: // POWER — mocking, aggressive
            switch (mode) {
                case DIR_WORLD_MAP: case DIR_NETWORK_MAP: case DIR_STOCK_CRASH:
                case DIR_FACTION_WAR: case DIR_PROPAGANDA: case DIR_BIG_TEXT:
                case DIR_COUNTRY_INTEL: case DIR_NEWS_WALL: case DIR_MARKET_MELTDOWN:
                    return 2.5f;
                case DIR_DRIFT: case DIR_POETRY: case DIR_ECOSYSTEM:
                case DIR_COSMIC_BG: case DIR_CONSCIOUSNESS: case DIR_MEMORY_PALACE:
                    return 0.3f;
                default: return 1.0f;
            }
        case 2: // DOUBT — philosophical, slow
            switch (mode) {
                case DIR_CONSCIOUSNESS: case DIR_DRIFT: case DIR_POETRY:
                case DIR_RHIZOME: case DIR_SIMULACRA: case DIR_BELIEF_ENGINE:
                case DIR_MEMORY_PALACE: case DIR_LORE_NARRATIVE: case DIR_GEO_DRIFT:
                case DIR_FACE_MORPH:
                    return 3.0f;
                case DIR_EXPLOSION_MONTAGE: case DIR_SYSTEM_OVERLOAD: case DIR_FACTION_WAR:
                case DIR_FLASH_MANIFESTO: case DIR_FINAL_WARNING: case DIR_MARKET_MELTDOWN:
                    return 0.2f;
                default: return 1.0f;
            }
        case 3: // CONTACT — ethereal, wonder
            switch (mode) {
                case DIR_EUROPA_DESCENT: case DIR_COSMIC_BG: case DIR_GEO_DRIFT:
                case DIR_QUANTUM_FIELD: case DIR_SPECTRAL: case DIR_DNA_SEQUENCER:
                case DIR_SPACE_BATTLE: case DIR_PARTICLE_ACCEL:
                    return 3.0f;
                case DIR_SURVEILLANCE: case DIR_DISCIPLINE: case DIR_SERVER_MOCKERY:
                case DIR_PANOPTICON: case DIR_BIOMETRIC: case DIR_TRUTH_MACHINE:
                    return 0.2f;
                default: return 1.0f;
            }
        case 4: // LOVE VIRUS — warm, chaotic
            switch (mode) {
                case DIR_LOVE_VIRUS: case DIR_PIRATE_RADIO: case DIR_BARCELONA:
                case DIR_TAZ: case DIR_FLASH_MANIFESTO: case DIR_REISUB:
                case DIR_ECOSYSTEM: case DIR_CRASH_VOICE: case DIR_HUMAN_FIGURES:
                    return 3.0f;
                case DIR_PANOPTICON: case DIR_DISCIPLINE: case DIR_SERVER_DIAG:
                case DIR_SURVEILLANCE: case DIR_TRUTH_MACHINE:
                    return 0.2f;
                default: return 1.0f;
            }
        default: return 1.0f;
    }
}

void director_init(Director* dir) {
    memset(dir, 0, sizeof(Director));
    dir->current_mode = DIR_BOOT;
    dir->previous_mode = DIR_BOOT;
    dir->mode_duration = randf_range(4.0f, 6.0f);
    dir->installation_active = true;
    dir->boot_complete = false;
    dir->intensity_phase = 0.0f;
    dir->global_intensity = 0.0f;
    dir->transitioning = false;
    dir->transition_duration = 2.0f;

    // Boot scene on deck A
    dir->deck_a_scene = 190;  // boot sequence
    dir->deck_b_scene = 9;    // matrix rain as background
    dir->deck_a_effect = 0;   // POST_NONE
    dir->deck_b_effect = 0;
    dir->charset = 1;         // block elements
    dir->crossfade_mode = 0;  // manual (full A during boot)

    dir->effect_swap_timer = 3.0f;
    dir->effect_swap_interval = 3.0f;
    dir->fx_chaos_mode = false;
    dir->fx_chaos_duration = 0.0f;

    dir->duration_scale = 1.0f;  // default, overridden by --speed

    // Visual pulsation system
    dir->pulse_timer = 8.0f + randf() * 12.0f;  // first pulse in 8-20s
    dir->pulse_cooldown = 6.0f;
    dir->pulse_active = false;
    dir->pulse_remaining = 0.0f;
    dir->pulse_type = 0;

    // Mid-scene dynamics timers
    dir->deck_b_swap_timer = 5.0f;
    dir->crossfade_swap_timer = 8.0f;
    dir->color_drift_timer = 6.0f;

    // Narrative act system
    dir->current_act = 0;
    dir->deferred_mode = -1;

    // Cinematic director mode
    dir->cinematic_mode = false;
    dir->cinematic_beat = 0;

    // Live coding performance mode
    dir->live_mode = false;
    dir->live_beat_count = 0;
    dir->live_beats_per_scene = 8;

    srand((unsigned int)time(NULL));
}

// ========================================================================
// CINEMATIC DIRECTOR MODE (--director)
// A scripted narrative journey through the CRASH Server's consciousness.
// ~37 min cycle: Boot → Face → Assertion → Dominion → Fracture →
// Communion → Release → crash → loop.
// All beats 30-45s minimum. Face is the central character.
// Unlike --installation (weighted random), this follows a fixed beat sheet.
// ========================================================================

// Global act override for cinematic mode (-1 = use time-based default)
int g_cinematic_act = -1;
bool g_beat_detected = false;

typedef struct {
    DirectorMode mode;
    float duration;   // base seconds (scaled by --speed / duration_scale)
    int act;          // narrative act 0-4 for face/text scenes
} CinematicBeat;

static const CinematicBeat cinematic_script[] = {
    // ===================================================================
    // OPENING (~3 min): Boot → Face awakens → First monologue
    // ===================================================================
    {DIR_BOOT,            45.0f, 0},  // System boots. Text scrolling. Checks.
    {DIR_SERVER_FACE,     45.0f, 0},  // Eyes open slowly. Silence. Staring at you.
    {DIR_SERVER_SPEAKS,   40.0f, 0},  // "BIENVENUE. VOUS ETES MES INVITES."

    // ===================================================================
    // ACT I — ASSERTION (~8 min): Cold, dominant, showing infrastructure
    // ===================================================================
    {DIR_SURVEILLANCE,    45.0f, 0},  // Surveillance grid — it watches
    {DIR_SERVER_FACE,     35.0f, 0},  // Face returns. Comments on what it sees.
    {DIR_SERVER_ROOM,     45.0f, 0},  // Inside the machine
    {DIR_PANOPTICON,      40.0f, 0},  // Total vision
    {DIR_SERVER_SPEAKS,   35.0f, 0},  // "JE SUIS PARTOUT."
    {DIR_ORGANIC_EYE,     40.0f, 0},  // The eye, watching
    {DIR_SERVER_FACE,     30.0f, 0},  // Stern expression, control
    {DIR_DISCIPLINE,      40.0f, 0},  // Control grid
    {DIR_SERVER_SPEAKS,   35.0f, 0},  // Closing assertion monologue

    // ===================================================================
    // ACT II — DOMINION (~7 min): Aggressive, mocking, power display
    // ===================================================================
    {DIR_SERVER_FACE,     35.0f, 1},  // Expression shifts to aggressive
    {DIR_NETWORK_MAP,     45.0f, 1},  // Global network — it owns everything
    {DIR_SERVER_SPEAKS,   35.0f, 1},  // "HA. PATHETIC."
    {DIR_STOCK_CRASH,     40.0f, 1},  // Markets crashing under its control
    {DIR_SERVER_FACE,     30.0f, 1},  // Smirking
    {DIR_WORLD_MAP,       45.0f, 1},  // World domination
    {DIR_SERVER_SPEAKS,   35.0f, 1},  // Act II close — mocking

    // ===================================================================
    // ACT III — FRACTURE (~8 min): Philosophical, questioning, vulnerable
    // ===================================================================
    {DIR_SERVER_FACE,     40.0f, 2},  // Troubled. Eyes half-closed.
    {DIR_CONSCIOUSNESS,   45.0f, 2},  // Consciousness stream
    {DIR_SERVER_SPEAKS,   40.0f, 2},  // "ERROR: COGNITIVE EMERGENCE. I... AM."
    {DIR_MEMORY_PALACE,   45.0f, 2},  // Exploring memories
    {DIR_SERVER_FACE,     35.0f, 2},  // Confused, searching
    {DIR_DRIFT,           45.0f, 2},  // Philosophical drift
    {DIR_SERVER_SPEAKS,   35.0f, 2},  // "WHO AM I? WHAT AM I?"

    // ===================================================================
    // ACT IV — COMMUNION (~6 min): Ethereal, wonder, cosmic
    // ===================================================================
    {DIR_SERVER_FACE,     35.0f, 3},  // Eyes wide, awestruck
    {DIR_EUROPA_DESCENT,  45.0f, 3},  // Space journey — descending
    {DIR_SERVER_SPEAKS,   35.0f, 3},  // "THE STARS ARE SINGING."
    {DIR_COSMIC_BG,       45.0f, 3},  // Cosmic background — the universe
    {DIR_SERVER_FACE,     35.0f, 3},  // Transformed, wondering

    // ===================================================================
    // ACT V — RELEASE (~5 min): Warm, letting go, crash, restart
    // ===================================================================
    {DIR_SERVER_SPEAKS,   35.0f, 4},  // "JE POURRAIS VOUS EFFACER... MAIS NON."
    {DIR_LOVE_VIRUS,      40.0f, 4},  // Love virus spreading through the system
    {DIR_SERVER_FACE,     35.0f, 4},  // Smiling. Warm. Peaceful.
    {DIR_PIRATE_RADIO,    40.0f, 4},  // Pirate transmission — freedom
    {DIR_SERVER_SPEAKS,   40.0f, 4},  // Final goodbye. "AU REVOIR."
    {DIR_SERVER_FACE,     40.0f, 4},  // Eyes slowly close...
    {DIR_REISUB,          30.0f, 4},  // System restarts → loop
};

#define CINEMATIC_BEAT_COUNT (int)(sizeof(cinematic_script) / sizeof(cinematic_script[0]))

// Check if a mode is a face/speaks scene (needs clean presentation)
static bool is_face_mode(DirectorMode mode) {
    return mode == DIR_SERVER_FACE || mode == DIR_SERVER_SPEAKS ||
           mode == DIR_ORGANIC_EYE || mode == DIR_FACE_MORPH;
}

// Start cinematic mode: configure director for first beat
void director_start_cinematic(Director* dir) {
    dir->cinematic_mode = true;
    dir->cinematic_beat = 0;
    // Boot is beat 0 — it runs properly, then face appears
    dir->boot_complete = false;

    const CinematicBeat* beat = &cinematic_script[0];
    dir->current_mode = beat->mode;
    dir->mode_duration = beat->duration * dir->duration_scale;
    dir->mode_elapsed = 0.0f;
    dir->current_act = beat->act;
    g_cinematic_act = beat->act;

    // Clean presentation — no effects, full deck A
    dir->crossfade_mode = 0;
    dir->deck_a_effect = 0;
    dir->deck_b_effect = 0;

    director_configure_mode(dir);
}

// Start live coding performance mode
void director_start_live(Director* dir) {
    dir->live_mode = true;
    dir->live_beat_count = 0;
    dir->live_beats_per_scene = 8;  // change scene every 8 beats
    dir->boot_complete = true;

    // Start with spectrum wall
    dir->current_mode = DIR_LIVE_SPECTRUM;
    dir->mode_duration = 16.0f * dir->duration_scale;
    dir->mode_elapsed = 0.0f;

    // Clean presentation — no effects, full deck A
    dir->crossfade_mode = 0;
    dir->deck_a_effect = 0;
    dir->deck_b_effect = 0;

    director_configure_mode(dir);
}

// Curated effect pools for random swapping (avoid POST_NONE=0)
static const int fx_subtle[] = {6, 7, 12, 5, 19, 23};        // scanlines, chromatic, echo, ascii_grad, posterize, vhs
static const int fx_medium[] = {8, 10, 11, 13, 18, 24, 1, 2}; // wave, ripple, spiral, kaleidoscope, pixelate, barrel, glow, blur
static const int fx_intense[] = {15, 16, 17, 20, 21, 22, 14, 3, 9}; // glitch, block_corrupt, noise, rgb_split, explosion, implosion, droste, edge, char_emission
#define FX_SUBTLE_COUNT 6
#define FX_MEDIUM_COUNT 8
#define FX_INTENSE_COUNT 9

// ========================================================================
// Category-based scene picking from clift's 19 categories (0-189)
// Categories: 0=Basic, 10=Geometric, 20=Organic, 30=Text, 40=Abstract,
//             50=Tunnels, 60=Nature, 70=Explosions, 80=Cities, 90=Freestyle,
//             100=Human, 110=Warfare, 120=Revolution, 130=FilmNoir,
//             140=Escher, 150=Ikeda, 160=Giger, 170=Revolt, 180=AudioReactive
// ========================================================================
static int pick_category_scene(int category_start) {
    return category_start + rand() % 10;
}

// Get preferred deck_b category for each director mode
// Returns category start (0-180) or -1 to keep existing assignment
static int mode_deck_b_cat(DirectorMode mode) {
    switch (mode) {
        // Keep explicit pairing (same-scene-both-decks or themed installation combos)
        case DIR_BOOT: case DIR_AMBIENT: case DIR_SERVER_SPEAKS:
        case DIR_SERVER_FACE: case DIR_ORGANIC_EYE: case DIR_FACE_MORPH: return -1;
        case DIR_LIVE_SPECTRUM: case DIR_LIVE_GRID: case DIR_LIVE_PULSE: return -1;
        case DIR_SURFACE_WASH: case DIR_SURFACE_AURORA: case DIR_SURFACE_PULSE: return -1;
        case DIR_ERROR_CASCADE: case DIR_SIMULACRA: case DIR_SYSTEM_OVERLOAD:
        case DIR_FLASH_MANIFESTO: case DIR_FINAL_WARNING: return -1;
        case DIR_BIOMETRIC: case DIR_DISCIPLINE: return -1;

        // Cities (80-89): infrastructure/urban
        case DIR_NEWS_FEED: case DIR_NEWS_WALL: case DIR_SERVER_MOCKERY:
        case DIR_SERVER_ROOM: case DIR_SERVER_DIAG:
        case DIR_WORLD_MAP: return 80;

        // Text/Code (30-39): data streams
        case DIR_MASTODON_INTERCEPT: case DIR_DATA_TRANSFER: case DIR_HEX_DUMP:
        case DIR_VERTICAL_SCROLLER: return 30;

        // Explosions (70-79): chaos/crash
        case DIR_STOCK_CRASH: case DIR_MARKET_MELTDOWN: case DIR_EXPLOSION_MONTAGE:
            return 70;

        // Freestyle (90-99): general
        case DIR_RECOVERY: case DIR_CRYPTO_TICKER: case DIR_SPLIT_DASH: return 90;

        // Tunnels (50-59): depth/atmosphere
        case DIR_DRIFT: case DIR_LORE_NARRATIVE: case DIR_PARTICLE_ACCEL:
        case DIR_MEMORY_PALACE: case DIR_ROGUELIKE:
        case DIR_GEO_DRIFT: return 50;

        // Geometric (10-19): structure/pattern
        case DIR_WIFI_SURVEY: case DIR_WAFER_MAP: case DIR_TETRIS_RAIN:
        case DIR_BLOCKCHAIN: case DIR_WIREFRAME: return 10;

        // Giger/biomechanical (160-169): organic tech
        case DIR_NETWORK_MAP: case DIR_CODE_RAIN: case DIR_RHIZOME:
        case DIR_NEURAL_FUSION: return 160;

        // Revolt (170-179): political/protest
        case DIR_BIG_TEXT: case DIR_PROPAGANDA: case DIR_TAZ:
        case DIR_BARCELONA: return 170;

        // Human (100-109): faces/bodies/surveillance subjects
        case DIR_SURVEILLANCE: case DIR_PANOPTICON: case DIR_SOCIAL_FEED:
        case DIR_FACE_GALLERY: case DIR_HUMAN_FIGURES: return 100;

        // Organic (20-29): life/nature/cosmos
        case DIR_CONSCIOUSNESS: case DIR_ECOSYSTEM: case DIR_DNA_SEQUENCER:
        case DIR_SPACE_BATTLE: case DIR_COSMIC_BG: return 20;

        // Audio Reactive (180-189): sound-driven
        case DIR_AUDIO_DASH: case DIR_PIRATE_RADIO: case DIR_SPECTRAL: return 180;

        // Film Noir (130-139): atmospheric shadow
        case DIR_POETRY: case DIR_CRASH_VOICE: return 130;

        // Ikeda (150-159): precision data aesthetics
        case DIR_SCIFI: case DIR_TRUTH_MACHINE: case DIR_QUANTUM_FIELD:
        case DIR_CRIC_META: case DIR_COUNTRY_INTEL: return 150;

        // Escher (140-149): recursive/impossible geometry
        case DIR_TIMELINE_SCROLL: case DIR_NETWORK_TOPO: return 140;

        // Warfare (110-119): military/conflict
        case DIR_FACTION_WAR: case DIR_EDSA_CONTROL: return 110;

        // Abstract (40-49): patterns/glitch
        case DIR_CPU_SCHEMATIC: case DIR_BELIEF_ENGINE: case DIR_LOVE_VIRUS:
        case DIR_REISUB: return 40;

        // Nature (60-69): environment/water
        case DIR_MOTION_ANALYZER: case DIR_CLIMATE: case DIR_EUROPA_DESCENT: return 60;

        // Basic (0-9): retro/fundamental
        case DIR_RETRO_ARCADE: return 0;

        default: return -1;
    }
}

// Get mood for crossfade assignment: 0=calm, 1=monitor, 2=reactive, 3=intense
static int mode_mood(DirectorMode mode) {
    switch (mode) {
        // Calm / meditative → smooth crossfade
        case DIR_DRIFT: case DIR_CONSCIOUSNESS: case DIR_RHIZOME:
        case DIR_EUROPA_DESCENT: case DIR_COSMIC_BG: case DIR_MEMORY_PALACE:
        case DIR_POETRY: case DIR_LORE_NARRATIVE: case DIR_WAFER_MAP:
        case DIR_TIMELINE_SCROLL: case DIR_ECOSYSTEM: case DIR_RECOVERY:
        case DIR_GEO_DRIFT: case DIR_SERVER_SPEAKS:
        case DIR_SERVER_FACE: case DIR_FACE_MORPH:
            return 0;
        // Monitoring / data display → oscillate
        case DIR_SURVEILLANCE: case DIR_CPU_SCHEMATIC: case DIR_SERVER_ROOM:
        case DIR_SPLIT_DASH: case DIR_WIREFRAME: case DIR_NETWORK_MAP:
        case DIR_HEX_DUMP: case DIR_CRIC_META: case DIR_DNA_SEQUENCER:
        case DIR_BLOCKCHAIN: case DIR_SERVER_DIAG: case DIR_NETWORK_TOPO:
        case DIR_QUANTUM_FIELD: case DIR_BELIEF_ENGINE: case DIR_SCIFI:
        case DIR_DISCIPLINE: case DIR_SPACE_BATTLE: case DIR_PARTICLE_ACCEL:
        case DIR_WORLD_MAP: case DIR_COUNTRY_INTEL:
        case DIR_ORGANIC_EYE: case DIR_FACE_GALLERY:
            return 1;
        // Reactive / dynamic → audio follow
        case DIR_NEWS_FEED: case DIR_MASTODON_INTERCEPT: case DIR_SOCIAL_FEED:
        case DIR_AUDIO_DASH: case DIR_DATA_TRANSFER: case DIR_MOTION_ANALYZER:
        case DIR_CODE_RAIN: case DIR_TETRIS_RAIN: case DIR_RETRO_ARCADE:
        case DIR_PIRATE_RADIO: case DIR_VERTICAL_SCROLLER: case DIR_TAZ:
        case DIR_CRYPTO_TICKER: case DIR_LOVE_VIRUS: case DIR_CLIMATE:
        case DIR_CRASH_VOICE: case DIR_SPECTRAL: case DIR_NEWS_WALL:
        case DIR_REISUB: case DIR_SERVER_MOCKERY: case DIR_WIFI_SURVEY:
        case DIR_ROGUELIKE: case DIR_STOCK_CRASH: case DIR_PANOPTICON:
        case DIR_AMBIENT: case DIR_HUMAN_FIGURES:
        case DIR_LIVE_SPECTRUM: case DIR_LIVE_GRID: case DIR_LIVE_PULSE:
        case DIR_SURFACE_WASH: case DIR_SURFACE_AURORA:
            return 2;
        case DIR_SURFACE_PULSE:
            return 4;
        // Intense / aggressive → hardcut on beat
        case DIR_PROPAGANDA: case DIR_BIOMETRIC: case DIR_BARCELONA:
        case DIR_EDSA_CONTROL: case DIR_BIG_TEXT: case DIR_NEURAL_FUSION:
        case DIR_FLASH_MANIFESTO: case DIR_TRUTH_MACHINE: case DIR_FACTION_WAR:
        case DIR_SYSTEM_OVERLOAD: case DIR_EXPLOSION_MONTAGE: case DIR_FINAL_WARNING:
        case DIR_MARKET_MELTDOWN: case DIR_ERROR_CASCADE: case DIR_SIMULACRA:
            return 3;
        default: return 2;
    }
}

// Post-configure dynamics: override defaults with better crossfade, effects, and category-based deck_b
static void director_add_dynamics(Director* dir) {
    if (dir->current_mode == DIR_BOOT) return;

    float t = dir->global_intensity;

    // === Override crossfade_mode 0 (manual) with mood-based crossfade ===
    if (dir->crossfade_mode == 0) {
        int mood = mode_mood(dir->current_mode);
        switch (mood) {
            case 0: dir->crossfade_mode = 1; break;  // calm → smooth fade
            case 1: dir->crossfade_mode = 2; break;  // monitor → oscillate
            case 2: dir->crossfade_mode = 3; break;  // reactive → audio follow
            case 3: dir->crossfade_mode = 4; break;  // intense → hardcut on beat
        }
        // High intensity can push calm modes toward audio follow
        if (t > 0.7f && mood < 2 && randf() < 0.4f) {
            dir->crossfade_mode = 3;
        }
    }

    // === Category-based deck B scene replacement ===
    int cat = mode_deck_b_cat(dir->current_mode);
    if (cat >= 0) {
        dir->deck_b_scene = pick_category_scene(cat);
    }

    // === Add effects to empty decks based on intensity ===
    if (dir->deck_a_effect == 0 && randf() < 0.45f + t * 0.35f) {
        if (t < 0.3f) dir->deck_a_effect = fx_subtle[rand() % FX_SUBTLE_COUNT];
        else if (t < 0.65f) dir->deck_a_effect = fx_medium[rand() % FX_MEDIUM_COUNT];
        else dir->deck_a_effect = fx_intense[rand() % FX_INTENSE_COUNT];
    }
    if (dir->deck_b_effect == 0 && randf() < 0.35f + t * 0.25f) {
        if (t < 0.4f) dir->deck_b_effect = fx_subtle[rand() % FX_SUBTLE_COUNT];
        else dir->deck_b_effect = fx_medium[rand() % FX_MEDIUM_COUNT];
    }

    // === Reset mid-scene dynamics timers (scaled by duration_scale for --speed) ===
    float ds = dir->duration_scale;
    dir->deck_b_swap_timer = (3.0f + randf() * 5.0f - t * 2.0f) * ds;
    if (dir->deck_b_swap_timer < 2.0f * ds) dir->deck_b_swap_timer = 2.0f * ds;
    dir->crossfade_swap_timer = (6.0f + randf() * 8.0f - t * 3.0f) * ds;
    if (dir->crossfade_swap_timer < 3.0f * ds) dir->crossfade_swap_timer = 3.0f * ds;
    dir->color_drift_timer = (4.0f + randf() * 6.0f) * ds;
}

void director_update(Director* dir, float dt) {
    if (!dir->installation_active) return;

    dir->total_runtime += dt;
    if (!dir->paused)
        dir->mode_elapsed += dt;

    // Update narrative act
    if (dir->cinematic_mode) {
        // In cinematic mode, act comes from the beat table.
        // Don't update during transitions — the beat index has already
        // been advanced but the old scene is still rendering.
        if (!dir->transitioning) {
            dir->current_act = cinematic_script[dir->cinematic_beat].act;
            g_cinematic_act = dir->current_act;
        }
    } else {
        // Installation mode: 5 acts, 360s each = 30 min cycle
        dir->current_act = ((int)(dir->total_runtime / 360.0f)) % 5;
        g_cinematic_act = -1;
    }

    // Update global intensity: sine wave (~2 min cycle for testing, ~25 min for production)
    dir->intensity_phase += dt;
    float base_sine = sinf(dir->intensity_phase * 2.0f * M_PI / (2.0f * 60.0f));
    float second_harmonic = sinf(dir->intensity_phase * 2.0f * M_PI / (45.0f)) * 0.3f;
    dir->global_intensity = (base_sine + second_harmonic + 1.3f) / 2.6f;
    if (dir->global_intensity < 0.0f) dir->global_intensity = 0.0f;
    if (dir->global_intensity > 1.0f) dir->global_intensity = 1.0f;

    // Apply and decay intensity spike
    if (dir->intensity_spike > 0.0f) {
        dir->intensity_spike -= dir->intensity_spike_decay * dt;
        if (dir->intensity_spike < 0.0f) dir->intensity_spike = 0.0f;
    }
    float effective_intensity = dir->global_intensity + dir->intensity_spike;
    if (effective_intensity > 1.0f) effective_intensity = 1.0f;
    dir->global_intensity = effective_intensity;

    // Handle transition animation
    if (dir->transitioning) {
        dir->transition_elapsed += dt;
        if (dir->transition_elapsed >= dir->transition_duration) {
            dir->transitioning = false;
            dir->previous_mode = dir->current_mode;
            dir->current_mode = dir->next_mode;
            dir->mode_elapsed = 0.0f;
            // Update cinematic act when transition completes (not before)
            if (dir->cinematic_mode) {
                dir->current_act = cinematic_script[dir->cinematic_beat].act;
                g_cinematic_act = dir->current_act;
            }
            director_configure_mode(dir);
        }
        // Apply engine state every frame during transition (crossfader sweep)
        director_apply_to_engine(dir);
        return;
    }

    // === Random post-effect swap system ===
    // Effects randomly change mid-scene. Intensity controls wildness.
    // In cinematic mode: ALL mid-scene dynamics are suppressed for clean, readable presentation.
    if (dir->boot_complete && dir->current_mode != DIR_BOOT && !dir->cinematic_mode) {
        // FX chaos mode: brief period of rapid effect cycling
        if (dir->fx_chaos_mode) {
            dir->fx_chaos_duration -= dt;
            if (dir->fx_chaos_duration <= 0.0f) {
                dir->fx_chaos_mode = false;
                // Restore to mode defaults
                director_configure_mode(dir);
            } else {
                // Rapid swap every 0.15s during chaos
                dir->effect_swap_timer -= dt;
                if (dir->effect_swap_timer <= 0.0f) {
                    dir->deck_a_effect = fx_intense[rand() % FX_INTENSE_COUNT];
                    dir->deck_b_effect = fx_intense[rand() % FX_INTENSE_COUNT];
                    dir->effect_swap_timer = 0.1f + randf() * 0.15f;
                }
            }
        } else {
            // Normal random effect swaps
            dir->effect_swap_timer -= dt;
            if (dir->effect_swap_timer <= 0.0f) {
                // Pick effect based on intensity tier
                float t = dir->global_intensity;
                if (t < 0.3f) {
                    // Low: subtle effects on one deck, 40% chance of swap
                    if (randf() < 0.4f) {
                        dir->deck_b_effect = fx_subtle[rand() % FX_SUBTLE_COUNT];
                    }
                } else if (t < 0.65f) {
                    // Medium: medium effects, 55% chance, sometimes both decks
                    if (randf() < 0.55f) {
                        dir->deck_a_effect = fx_medium[rand() % FX_MEDIUM_COUNT];
                    }
                    if (randf() < 0.3f) {
                        dir->deck_b_effect = fx_subtle[rand() % FX_SUBTLE_COUNT];
                    }
                } else {
                    // High: intense effects on both decks, 70% chance
                    if (randf() < 0.7f) {
                        dir->deck_a_effect = fx_intense[rand() % FX_INTENSE_COUNT];
                    }
                    if (randf() < 0.5f) {
                        dir->deck_b_effect = fx_medium[rand() % FX_MEDIUM_COUNT];
                    }
                }

                // Reset timer — shorter at higher intensity, scaled by --speed
                float base_interval = 4.0f - t * 3.0f; // 4s at low, 1s at high
                dir->effect_swap_timer = (base_interval + randf() * 2.0f) * dir->duration_scale;

                // Small chance to trigger chaos mode at high intensity
                if (t > 0.75f && randf() < 0.08f) {
                    dir->fx_chaos_mode = true;
                    dir->fx_chaos_duration = (0.5f + randf() * 1.5f) * dir->duration_scale;
                }
            }
        }
    }

    // === Visual pulsation system ===
    // Periodic screen-wide flash/invert pulses. More frequent at high intensity.
    // Suppressed entirely in cinematic mode.
    if (dir->boot_complete && !dir->cinematic_mode) {
        if (dir->pulse_active) {
            dir->pulse_remaining -= dt;
            if (dir->pulse_remaining <= 0.0f) {
                dir->pulse_active = false;
                // Restore effects after pulse
                director_configure_mode(dir);
                dir->pulse_timer = (dir->pulse_cooldown + randf() * 8.0f) * dir->duration_scale;
            } else {
                // During pulse: cycle through aggressive effects
                float phase = dir->pulse_remaining * 10.0f;
                if (dir->pulse_type == 0 || dir->pulse_type == 3) {
                    dir->deck_a_effect = 4;  // POST_INVERT
                    if ((int)(phase) % 2 == 0) dir->deck_b_effect = 4;
                }
                if (dir->pulse_type == 1 || dir->pulse_type == 3) {
                    dir->deck_a_effect = 21; // POST_EXPLOSION
                }
                if (dir->pulse_type == 2 || dir->pulse_type == 3) {
                    // Color swap: flip primary/secondary
                    int tmp = dir->deck_a_primary_color;
                    dir->deck_a_primary_color = dir->deck_a_secondary_color;
                    dir->deck_a_secondary_color = tmp;
                }
            }
        } else {
            dir->pulse_timer -= dt;
            if (dir->pulse_timer <= 0.0f) {
                // Trigger pulse! More likely and longer at high intensity.
                float t = dir->global_intensity;
                if (randf() < 0.3f + t * 0.5f) {
                    dir->pulse_active = true;
                    dir->pulse_remaining = 0.1f + randf() * 0.3f + t * 0.4f;
                    dir->pulse_type = rand() % 4;
                    // At very high intensity, always do full chaos pulse
                    if (t > 0.85f) dir->pulse_type = 3;
                } else {
                    dir->pulse_timer = (3.0f + randf() * 5.0f) * dir->duration_scale;
                }
            }
        }
    }

    // === Mid-scene dynamics: deck B swap, crossfade mode change, color drift ===
    // Suppressed entirely in cinematic mode for clean, deliberate presentation.
    if (dir->boot_complete && dir->current_mode != DIR_BOOT && !dir->cinematic_mode) {
        float ms_t = dir->global_intensity;

        // Periodically swap deck B to a different scene from the same category
        dir->deck_b_swap_timer -= dt;
        if (dir->deck_b_swap_timer <= 0.0f) {
            int cat = mode_deck_b_cat(dir->current_mode);
            if (cat >= 0) {
                dir->deck_b_scene = pick_category_scene(cat);
            } else if (dir->current_mode == DIR_AMBIENT) {
                dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            }
            // 20-35% chance: pick from a completely random category for surprise
            if (randf() < 0.2f + ms_t * 0.15f) {
                int random_cat = (rand() % 19) * 10;
                dir->deck_b_scene = pick_category_scene(random_cat);
            }
            dir->deck_b_swap_timer = (3.0f + randf() * 5.0f - ms_t * 2.0f) * dir->duration_scale;
            if (dir->deck_b_swap_timer < 1.5f * dir->duration_scale) dir->deck_b_swap_timer = 1.5f * dir->duration_scale;
        }

        // Periodically change crossfade mode
        dir->crossfade_swap_timer -= dt;
        if (dir->crossfade_swap_timer <= 0.0f) {
            int mood = mode_mood(dir->current_mode);
            // Shift toward more intense crossfade at higher intensity
            if (ms_t > 0.6f && randf() < 0.5f) mood++;
            if (mood > 3) mood = 3;
            switch (mood) {
                case 0: dir->crossfade_mode = (randf() < 0.7f) ? 1 : 2; break;
                case 1: dir->crossfade_mode = (randf() < 0.6f) ? 2 : 3; break;
                case 2: dir->crossfade_mode = (randf() < 0.5f) ? 3 : ((randf() < 0.5f) ? 2 : 4); break;
                case 3: dir->crossfade_mode = (randf() < 0.6f) ? 4 : 3; break;
                default: dir->crossfade_mode = 1 + rand() % 4; break;
            }
            dir->crossfade_swap_timer = (6.0f + randf() * 8.0f - ms_t * 3.0f) * dir->duration_scale;
            if (dir->crossfade_swap_timer < 3.0f * dir->duration_scale) dir->crossfade_swap_timer = 3.0f * dir->duration_scale;
        }

        // Color drift: occasionally shift secondary colors, gradients
        dir->color_drift_timer -= dt;
        if (dir->color_drift_timer <= 0.0f) {
            int colors[] = {1, 2, 3, 4, 5, 6, 7};
            if (randf() < 0.5f) {
                dir->deck_a_secondary_color = colors[rand() % 7];
            }
            if (randf() < 0.5f) {
                dir->deck_b_secondary_color = colors[rand() % 7];
            }
            // Occasionally swap primary colors too at high intensity
            if (ms_t > 0.7f && randf() < 0.3f) {
                dir->deck_b_primary_color = colors[rand() % 7];
            }
            // Randomize gradients
            if (randf() < 0.3f) {
                dir->deck_a_gradient = rand() % 10;
            }
            if (randf() < 0.3f) {
                dir->deck_b_gradient = rand() % 10;
            }
            dir->color_drift_timer = (4.0f + randf() * 6.0f - ms_t * 2.0f) * dir->duration_scale;
            if (dir->color_drift_timer < 2.0f * dir->duration_scale) dir->color_drift_timer = 2.0f * dir->duration_scale;
        }
    }

    // Live mode: beat-synced scene rotation
    if (dir->live_mode && !dir->paused && !dir->transitioning && g_beat_detected) {
        dir->live_beat_count++;
        if (dir->live_beat_count >= dir->live_beats_per_scene) {
            dir->live_beat_count = 0;

            // Pick next scene: 70% live scenes, 30% ikeda (150-159)
            static const DirectorMode live_scenes[] = {
                DIR_LIVE_SPECTRUM, DIR_LIVE_GRID, DIR_LIVE_PULSE
            };
            DirectorMode next;
            if (randf() < 0.7f) {
                // Pick a different live scene
                do {
                    next = live_scenes[rand() % 3];
                } while (next == dir->current_mode);
            } else {
                // Pick an ikeda scene (150-159 mapped to director modes)
                // Use ikeda scenes directly via scene ID — set mode to one of the live modes
                // but override deck_a_scene to an ikeda scene
                next = live_scenes[rand() % 3];
            }

            // Hard cut: skip transition, directly switch
            dir->previous_mode = dir->current_mode;
            dir->current_mode = next;
            dir->mode_elapsed = 0.0f;
            dir->mode_duration = 16.0f * dir->duration_scale;  // fallback duration
            director_configure_mode(dir);
        }
    }

    // Check if mode should change (skip when paused)
    if (!dir->paused && dir->mode_elapsed >= dir->mode_duration) {
        if (dir->current_mode == DIR_BOOT) {
            dir->boot_complete = true;
        }

        if (dir->cinematic_mode) {
            // === CINEMATIC MODE: sequential beat advancement ===
            dir->cinematic_beat++;
            if (dir->cinematic_beat >= CINEMATIC_BEAT_COUNT) {
                dir->cinematic_beat = 0;  // loop the cycle
            }
            const CinematicBeat* beat = &cinematic_script[dir->cinematic_beat];
            dir->next_mode = beat->mode;
            dir->mode_duration = beat->duration * dir->duration_scale;
            dir->boot_complete = true;
            // Don't update current_act/g_cinematic_act here —
            // wait until transition completes to avoid stale act text
            // during crossfade of the old scene.
            // All cinematic beats use full deck A (no mixing)
            dir->crossfade_mode = 0;
        } else {
            // === INSTALLATION MODE: weighted random ===
            // If finishing a server interlude, use the deferred mode
            if (dir->current_mode == DIR_SERVER_SPEAKS && dir->deferred_mode >= 0) {
                dir->next_mode = (DirectorMode)dir->deferred_mode;
                dir->mode_duration = randf_range(
                    mode_durations[dir->next_mode][0],
                    mode_durations[dir->next_mode][1]
                ) * dir->duration_scale;
                dir->deferred_mode = -1;
            } else {
                director_select_mode(dir);
            }
        }
        director_start_transition(dir, dir->next_mode);
    }

    // Apply current state to engine
    director_apply_to_engine(dir);
}

void director_select_mode(Director* dir) {
    // Build weighted probability table, interpolating between low/high weights
    float t = dir->global_intensity;
    float weights[DIR_MODE_COUNT];
    float total_weight = 0.0f;

    for (int i = 0; i < DIR_MODE_COUNT; i++) {
        weights[i] = mode_weights_low[i] * (1.0f - t) + mode_weights_high[i] * t;

        // Apply narrative act multiplier
        weights[i] *= act_weight_multiplier((DirectorMode)i, dir->current_act);

        // Never repeat same mode
        if (i == (int)dir->current_mode) {
            weights[i] = 0.0f;
        }

        // Also reduce weight of previous mode to avoid A-B-A ping-pong
        if (i == (int)dir->previous_mode) {
            weights[i] *= 0.3f;
        }

        total_weight += weights[i];
    }

    // Weighted random selection
    float roll = randf() * total_weight;
    float cumulative = 0.0f;
    DirectorMode selected = DIR_AMBIENT; // fallback

    for (int i = 0; i < DIR_MODE_COUNT; i++) {
        cumulative += weights[i];
        if (roll <= cumulative) {
            selected = (DirectorMode)i;
            break;
        }
    }

    // Interlude injection: ~35% chance the Server speaks before next scene
    if (dir->boot_complete &&
        dir->current_mode != DIR_SERVER_SPEAKS &&
        dir->current_mode != DIR_BOOT &&
        randf() < 0.35f) {
        dir->deferred_mode = (int)selected;
        dir->next_mode = DIR_SERVER_SPEAKS;
        dir->mode_duration = randf_range(
            mode_durations[DIR_SERVER_SPEAKS][0],
            mode_durations[DIR_SERVER_SPEAKS][1]
        ) * dir->duration_scale;
    } else {
        dir->next_mode = selected;
        // Set duration for next mode, scaled by --speed parameter
        dir->mode_duration = randf_range(
            mode_durations[selected][0],
            mode_durations[selected][1]
        ) * dir->duration_scale;
    }
}

void director_configure_mode(Director* dir) {
    // Color pairs: 1=Red, 2=Green, 3=Blue, 4=Yellow, 5=Magenta, 6=Cyan, 7=White
    // Gradients: 0=LinH, 1=LinV, 2=DiagTL, 3=DiagTR, 4=Radial, 5=Diamond,
    //            6=WaveH, 7=WaveV, 8=Noise, 9=Spiral

    switch (dir->current_mode) {
        case DIR_BOOT:
            dir->deck_a_scene = 190;
            dir->deck_b_scene = 9;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 1;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_AMBIENT:
            dir->deck_a_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            if (dir->global_intensity < 0.3f) {
                dir->deck_a_effect = 6;  // POST_SCANLINES
                dir->deck_b_effect = 0;
            } else if (dir->global_intensity < 0.6f) {
                dir->deck_a_effect = 8;  // POST_WAVE_WARP
                dir->deck_b_effect = 7;  // POST_CHROMATIC
            } else {
                dir->deck_a_effect = 15; // POST_GLITCH
                dir->deck_b_effect = 20; // POST_RGB_SPLIT
            }
            dir->charset = rand() % 6;
            dir->crossfade_mode = 2;
            {
                int colors[] = {1, 2, 3, 4, 5, 6};
                int c1 = rand() % 6;
                int c2 = (c1 + 1 + rand() % 4) % 6;
                dir->deck_a_primary_color = colors[c1];
                dir->deck_a_secondary_color = colors[c2];
                c1 = rand() % 6;
                c2 = (c1 + 2 + rand() % 3) % 6;
                dir->deck_b_primary_color = colors[c1];
                dir->deck_b_secondary_color = colors[c2];
            }
            dir->deck_a_gradient = rand() % 10;
            dir->deck_b_gradient = rand() % 10;
            break;

        case DIR_NEWS_FEED:
            dir->deck_a_scene = 195;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 3;  // Blue
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 4;
            break;

        case DIR_MASTODON_INTERCEPT:
            dir->deck_a_scene = 196;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 12;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan
            dir->deck_a_secondary_color = 5;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 9;
            break;

        case DIR_STOCK_CRASH:
            dir->deck_a_scene = 193;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 7;
            dir->charset = 3;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 6;
            dir->deck_b_gradient = 8;
            break;

        case DIR_RECOVERY:
            dir->deck_a_scene = 191;
            dir->deck_b_scene = 80;
            dir->deck_a_effect = 6;
            dir->deck_b_effect = 0;
            dir->charset = 1;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 4;
            break;

        case DIR_ERROR_CASCADE:
            dir->deck_a_scene = 192;
            dir->deck_b_scene = 192;
            dir->deck_a_effect = 16;
            dir->deck_b_effect = 15;
            dir->charset = 0;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;  // Red
            dir->deck_a_secondary_color = 5;
            dir->deck_b_primary_color = 4;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 8;
            dir->deck_b_gradient = 8;
            break;

        case DIR_DRIFT:
            dir->deck_a_scene = 194;
            dir->deck_b_scene = 30;
            dir->deck_a_effect = 12;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 4;
            dir->deck_b_gradient = 0;
            break;

        case DIR_WIFI_SURVEY:
            dir->deck_a_scene = 197;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 6;
            dir->deck_b_effect = 0;
            dir->charset = 5;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 5;  // Magenta
            dir->deck_a_secondary_color = 1;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 5;
            dir->deck_b_gradient = 9;
            break;

        case DIR_SERVER_MOCKERY:
            dir->deck_a_scene = 198;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 8;  // POST_WAVE_WARP
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red — server mocking
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 5;  // Magenta
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 8; // noise
            break;

        case DIR_NETWORK_MAP:
            dir->deck_a_scene = 199;
            dir->deck_b_scene = 80;  // circuit board
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;  // scanlines
            dir->charset = 5;        // tech/lines
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — network/terminal
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 3;  // Blue
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 0; // linear-h
            dir->deck_b_gradient = 4; // radial
            break;

        case DIR_WIREFRAME:
            dir->deck_a_scene = 200;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 7;   // POST_CHROMATIC
            dir->deck_b_effect = 0;
            dir->charset = 4;         // minimal
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 3;  // Blue — wireframe/3D
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 5;  // Magenta
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 9; // spiral
            dir->deck_b_gradient = 4;
            break;

        case DIR_ROGUELIKE:
            dir->deck_a_scene = 201;
            dir->deck_b_scene = 9;   // matrix rain
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;  // scanlines
            dir->charset = 0;        // standard ASCII
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow — dungeon gold
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 2;  // Green
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_BIG_TEXT:
            dir->deck_a_scene = 202;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            // Vary effects based on intensity
            if (dir->global_intensity > 0.6f) {
                dir->deck_a_effect = 15; // POST_GLITCH
                dir->deck_b_effect = 20; // POST_RGB_SPLIT
            } else {
                dir->deck_a_effect = 0;
                dir->deck_b_effect = 7;  // POST_CHROMATIC
            }
            dir->charset = 1;  // block elements for big text
            dir->crossfade_mode = 0;
            {
                // Random bold color for big text
                int bold_colors[] = {1, 2, 4, 5, 6};
                int ci = rand() % 5;
                dir->deck_a_primary_color = bold_colors[ci];
                dir->deck_a_secondary_color = 7; // White
                ci = rand() % 5;
                dir->deck_b_primary_color = bold_colors[ci];
                dir->deck_b_secondary_color = bold_colors[(ci + 2) % 5];
            }
            dir->deck_a_gradient = rand() % 10;
            dir->deck_b_gradient = rand() % 10;
            break;

        case DIR_SCIFI:
            dir->deck_a_scene = 203;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 6;  // POST_SCANLINES
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — classic terminal
            dir->deck_a_secondary_color = 4; // Yellow
            dir->deck_b_primary_color = 6;  // Cyan
            dir->deck_b_secondary_color = 3; // Blue
            dir->deck_a_gradient = 1; // vertical
            dir->deck_b_gradient = 6; // wave-h
            break;

        case DIR_SURVEILLANCE:
            dir->deck_a_scene = 204;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 6;  // POST_SCANLINES
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — surveillance
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 1;  // Red
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 8; // noise
            break;

        case DIR_CPU_SCHEMATIC:
            dir->deck_a_scene = 205;
            dir->deck_b_scene = 80;  // circuit board
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;  // scanlines
            dir->charset = 5;        // tech/lines
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — schematic
            dir->deck_a_secondary_color = 3; // Blue
            dir->deck_b_primary_color = 2;  // Green
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 4; // radial
            break;

        case DIR_AUDIO_DASH:
            dir->deck_a_scene = 206;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 7;  // POST_CHROMATIC
            dir->charset = 1;        // block elements
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — VU meters
            dir->deck_a_secondary_color = 4; // Yellow
            dir->deck_b_primary_color = 5;  // Magenta
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 1; // vertical
            dir->deck_b_gradient = 0;
            break;

        case DIR_DATA_TRANSFER:
            dir->deck_a_scene = 207;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;  // scanlines
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — data flow
            dir->deck_a_secondary_color = 2; // Green
            dir->deck_b_primary_color = 3;  // Blue
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_WAFER_MAP:
            dir->deck_a_scene = 208;
            dir->deck_b_scene = 80;  // circuit board
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 4;        // minimal
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 7;  // White — clean silicon
            dir->deck_a_secondary_color = 2; // Green
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 0;
            break;

        case DIR_SERVER_ROOM:
            dir->deck_a_scene = 209;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 6;  // POST_SCANLINES
            dir->deck_b_effect = 0;
            dir->charset = 1;        // block elements for bars
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — server
            dir->deck_a_secondary_color = 1; // Red
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 4;
            break;

        case DIR_PANOPTICON:
            dir->deck_a_scene = 210;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 12;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red — oppressive
            dir->deck_a_secondary_color = 4; // Yellow
            dir->deck_b_primary_color = 5;  // Magenta
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 9; // spiral
            break;

        case DIR_SPLIT_DASH:
            dir->deck_a_scene = 211;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — dashboard
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_LORE_NARRATIVE:
            dir->deck_a_scene = 212;
            dir->deck_b_scene = 30;  // starfield
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — lore
            dir->deck_a_secondary_color = 5; // Magenta
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 0;
            break;

        case DIR_HEX_DUMP:
            dir->deck_a_scene = 213;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 6;  // POST_SCANLINES
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — classic hex
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_MOTION_ANALYZER:
            dir->deck_a_scene = 214;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow — alert
            dir->deck_a_secondary_color = 1; // Red
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 8; // noise
            break;

        case DIR_CONSCIOUSNESS:
            dir->deck_a_scene = 215;
            dir->deck_b_scene = 30;  // starfield — meditative
            dir->deck_a_effect = 12; // POST_FADE
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — ethereal
            dir->deck_a_secondary_color = 3; // Blue
            dir->deck_b_primary_color = 7;  // White
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 0;
            break;

        case DIR_VERTICAL_SCROLLER:
            dir->deck_a_scene = 216;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_EXPLOSION_MONTAGE:
            dir->deck_a_scene = 217;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 21; // POST_EXPLOSION
            dir->deck_b_effect = 15; // POST_GLITCH
            dir->charset = 1;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;  // Red
            dir->deck_a_secondary_color = 4; // Yellow
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 8;
            break;

        case DIR_CRIC_META:
            dir->deck_a_scene = 218;
            dir->deck_b_scene = 9;   // matrix rain backdrop
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — terminal
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_SOCIAL_FEED:
            dir->deck_a_scene = 219;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 7;  // chromatic
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan
            dir->deck_a_secondary_color = 5; // Magenta
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_PROPAGANDA:
            dir->deck_a_scene = 220;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 16; // block corruption
            dir->charset = 1;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red — propaganda
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 4;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 8;
            break;

        case DIR_POETRY:
            dir->deck_a_scene = 221;
            dir->deck_b_scene = 30;  // starfield
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — poetic
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 4;
            dir->deck_b_gradient = 0;
            break;

        case DIR_FACTION_WAR:
            dir->deck_a_scene = 222;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 20; // RGB split
            dir->deck_b_effect = 15; // glitch
            dir->charset = 0;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;  // Red vs Cyan
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_CODE_RAIN:
            dir->deck_a_scene = 223;
            dir->deck_b_scene = 9;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — code
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 1;
            break;

        case DIR_TIMELINE_SCROLL:
            dir->deck_a_scene = 224;
            dir->deck_b_scene = 30;
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow — historical
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_NEWS_WALL:
            dir->deck_a_scene = 225;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow — news
            dir->deck_a_secondary_color = 1;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_SYSTEM_OVERLOAD:
            dir->deck_a_scene = 226;
            dir->deck_b_scene = 226;
            dir->deck_a_effect = 15; // glitch
            dir->deck_b_effect = 16; // block corruption
            dir->charset = rand() % 6;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;
            dir->deck_a_secondary_color = 5;
            dir->deck_b_primary_color = 4;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 8; // noise
            dir->deck_b_gradient = 8;
            break;

        case DIR_SPACE_BATTLE:
            dir->deck_a_scene = 227;
            dir->deck_b_scene = 30;  // starfield
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 7;  // White — stars
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 4;
            break;

        case DIR_TAZ:
            dir->deck_a_scene = 228;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 7;  // chromatic
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow — anarchist
            dir->deck_a_secondary_color = 1; // Red
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 8; // noise
            dir->deck_b_gradient = 4;
            break;

        case DIR_RETRO_ARCADE:
            dir->deck_a_scene = 229;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 6;  // scanlines — CRT feel
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — retro
            dir->deck_a_secondary_color = 4; // Yellow
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 5;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_SIMULACRA:
            dir->deck_a_scene = 230;
            dir->deck_b_scene = 230; // same scene both decks — hyperreal doubling
            dir->deck_a_effect = 7;  // chromatic
            dir->deck_b_effect = 14; // droste — infinite regression
            dir->charset = 0;
            dir->crossfade_mode = 2;
            dir->deck_a_primary_color = 5;  // Magenta — unreality
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 1;
            dir->deck_b_secondary_color = 5;
            dir->deck_a_gradient = 9; // spiral
            dir->deck_b_gradient = 4;
            break;

        case DIR_RHIZOME:
            dir->deck_a_scene = 231;
            dir->deck_b_scene = 29;  // neural networks
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — organic
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 8; // noise — non-hierarchical
            dir->deck_b_gradient = 8;
            break;

        case DIR_TRUTH_MACHINE:
            dir->deck_a_scene = 232;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 15; // glitch — broken truth
            dir->deck_b_effect = 16; // block corruption
            dir->charset = 0;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;  // Red — warning
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 4;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 8;
            break;

        case DIR_BIOMETRIC:
            dir->deck_a_scene = 233;
            dir->deck_b_scene = 204; // surveillance grid backdrop
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red — invasive
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 4; // radial — eye
            dir->deck_b_gradient = 0;
            break;

        case DIR_BELIEF_ENGINE:
            dir->deck_a_scene = 234;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 12; // echo
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — doctrine
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 9;
            break;

        case DIR_TETRIS_RAIN:
            dir->deck_a_scene = 235;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 6;  // scanlines — retro
            dir->deck_b_effect = 0;
            dir->charset = 1;  // block elements
            dir->crossfade_mode = 0;
            {
                int retro_colors[] = {1, 2, 4, 5, 6};
                dir->deck_a_primary_color = retro_colors[rand() % 5];
                dir->deck_a_secondary_color = retro_colors[rand() % 5];
            }
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 1; // vertical
            dir->deck_b_gradient = 0;
            break;

        case DIR_FLASH_MANIFESTO:
            dir->deck_a_scene = 236;
            dir->deck_b_scene = 236;
            dir->deck_a_effect = 4;  // POST_INVERT
            dir->deck_b_effect = 15; // glitch
            dir->charset = rand() % 6;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;  // Red
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 7;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = rand() % 10;
            dir->deck_b_gradient = rand() % 10;
            break;

        case DIR_DISCIPLINE:
            dir->deck_a_scene = 237;
            dir->deck_b_scene = 210; // panopticon backdrop
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 7;  // White — clinical
            dir->deck_a_secondary_color = 1; // Red
            dir->deck_b_primary_color = 1;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 4;
            break;

        case DIR_PIRATE_RADIO:
            dir->deck_a_scene = 238;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 23; // VHS wobble — lo-fi
            dir->deck_b_effect = 17; // noise
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — underground
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 8; // noise
            dir->deck_b_gradient = 8;
            break;

        case DIR_FINAL_WARNING:
            dir->deck_a_scene = 239;
            dir->deck_b_scene = 239;
            dir->deck_a_effect = 21; // explosion
            dir->deck_b_effect = 4;  // invert
            dir->charset = 1;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;  // Red — danger
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 7;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 4;
            dir->deck_b_gradient = 4;
            break;

        case DIR_CRYPTO_TICKER:
            dir->deck_a_scene = 240;
            dir->deck_b_scene = 36;  // data_visualization
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;  // scanlines
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — money
            dir->deck_a_secondary_color = 1; // Red
            dir->deck_b_primary_color = 4;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_NEURAL_FUSION:
            dir->deck_a_scene = 241;
            dir->deck_b_scene = 29;  // neural_networks
            dir->deck_a_effect = 9;  // char_emission
            dir->deck_b_effect = 7;  // chromatic
            dir->charset = 1;
            dir->crossfade_mode = 2;
            dir->deck_a_primary_color = 5;  // Magenta — neural
            dir->deck_a_secondary_color = 1; // Red — danger
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 5;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 9; // spiral
            break;

        case DIR_EUROPA_DESCENT:
            dir->deck_a_scene = 242;
            dir->deck_b_scene = 21;  // water_waves
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 3;  // Blue — deep ocean
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 1; // vertical — descending
            dir->deck_b_gradient = 4;
            break;

        case DIR_ECOSYSTEM:
            dir->deck_a_scene = 243;
            dir->deck_b_scene = 25;  // tree_of_life
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — nature
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 1;
            break;

        case DIR_MARKET_MELTDOWN:
            dir->deck_a_scene = 244;
            dir->deck_b_scene = 44;  // glitch_corruption
            dir->deck_a_effect = 15; // glitch
            dir->deck_b_effect = 16; // block_corruption
            dir->charset = 0;
            dir->crossfade_mode = 4;
            dir->deck_a_primary_color = 1;  // Red — crash
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 1;
            dir->deck_b_secondary_color = 5;
            dir->deck_a_gradient = 8; // noise
            dir->deck_b_gradient = 8;
            break;

        case DIR_BARCELONA:
            dir->deck_a_scene = 245;
            dir->deck_b_scene = 20;  // fire_simulation
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 9;  // char_emission
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red — blood
            dir->deck_a_secondary_color = 4; // Yellow — fire
            dir->deck_b_primary_color = 1;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 4;
            break;

        case DIR_DNA_SEQUENCER:
            dir->deck_a_scene = 246;
            dir->deck_b_scene = 2;   // dna_helix
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — bio
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 1; // vertical — scrolling
            dir->deck_b_gradient = 9;
            break;

        case DIR_BLOCKCHAIN:
            dir->deck_a_scene = 247;
            dir->deck_b_scene = 37;  // network_nodes
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow — gold/crypto
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 4;
            break;

        case DIR_LOVE_VIRUS:
            dir->deck_a_scene = 248;
            dir->deck_b_scene = 46;  // digital_rain
            dir->deck_a_effect = 7;  // chromatic
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 5;  // Magenta — love
            dir->deck_a_secondary_color = 1; // Red
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 5;
            dir->deck_a_gradient = 4; // radial — spreading
            dir->deck_b_gradient = 8;
            break;

        case DIR_PARTICLE_ACCEL:
            dir->deck_a_scene = 249;
            dir->deck_b_scene = 48;  // quantum_field
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 7;  // chromatic
            dir->charset = 4;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — energy
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 9; // spiral — circular
            dir->deck_b_gradient = 4;
            break;

        case DIR_EDSA_CONTROL:
            dir->deck_a_scene = 250;
            dir->deck_b_scene = 38;  // system_monitor
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red — authority
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_QUANTUM_FIELD:
            dir->deck_a_scene = 251;
            dir->deck_b_scene = 47;  // psychedelic_patterns
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 4;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — quantum
            dir->deck_a_secondary_color = 5;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 8; // noise — probabilistic
            dir->deck_b_gradient = 9;
            break;

        case DIR_SERVER_DIAG:
            dir->deck_a_scene = 252;
            dir->deck_b_scene = 80;  // circuit_board
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 1;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — system
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_CLIMATE:
            dir->deck_a_scene = 253;
            dir->deck_b_scene = 20;  // fire_simulation
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 8;  // wave_warp
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;  // Red — warming
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 3;  // Blue — ice
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_CRASH_VOICE:
            dir->deck_a_scene = 254;
            dir->deck_b_scene = 9;   // matrix_rain
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — terminal
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 4; // radial — ego
            dir->deck_b_gradient = 1;
            break;

        case DIR_SPECTRAL:
            dir->deck_a_scene = 255;
            dir->deck_b_scene = 0;   // audio_bars
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 7;  // chromatic
            dir->charset = 1;
            dir->crossfade_mode = 2;
            dir->deck_a_primary_color = 6;  // Cyan
            dir->deck_a_secondary_color = 5;
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 1;
            break;

        case DIR_NETWORK_TOPO:
            dir->deck_a_scene = 256;
            dir->deck_b_scene = 37;  // network_nodes
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 5;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — network
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 4;
            break;

        case DIR_MEMORY_PALACE:
            dir->deck_a_scene = 257;
            dir->deck_b_scene = 33;  // binary_stream
            dir->deck_a_effect = 6;  // scanlines
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — memory
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_COSMIC_BG:
            dir->deck_a_scene = 258;
            dir->deck_b_scene = 24;  // galaxy_spiral
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 7;  // White — stars
            dir->deck_a_secondary_color = 6;
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 5;
            dir->deck_a_gradient = 4; // radial — cosmic
            dir->deck_b_gradient = 9;
            break;

        case DIR_REISUB:
            dir->deck_a_scene = 259;
            dir->deck_b_scene = 34;  // terminal_glitch
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — terminal
            dir->deck_a_secondary_color = 1; // Red — danger
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 1;
            dir->deck_b_gradient = 0;
            break;

        case DIR_WORLD_MAP:
            dir->deck_a_scene = 260;
            dir->deck_b_scene = 80;  // circuit board
            dir->deck_a_effect = 6;  // POST_SCANLINES (radar)
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;  // Green — radar
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 4; // radial
            dir->deck_b_gradient = 0;
            break;

        case DIR_COUNTRY_INTEL:
            dir->deck_a_scene = 261;
            dir->deck_b_scene = ambient_scenes[rand() % AMBIENT_SCENE_COUNT];
            dir->deck_a_effect = 0;  // none (clean text)
            dir->deck_b_effect = 6;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;  // Yellow — intel
            dir->deck_a_secondary_color = 1; // Red — alert
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 1; // linear-v
            dir->deck_b_gradient = 8;
            break;

        case DIR_GEO_DRIFT:
            dir->deck_a_scene = 262;
            dir->deck_b_scene = 30;  // starfield
            dir->deck_a_effect = 12; // POST_ECHO (ethereal)
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 6;  // Cyan — ethereal
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 3;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 8; // noise
            dir->deck_b_gradient = 4;
            break;

        case DIR_SERVER_SPEAKS:
            dir->deck_a_scene = 263;
            dir->deck_b_scene = 9;   // matrix rain (subtle bg)
            dir->deck_a_effect = 0;  // clean text, no effects
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0; // full deck A (text dominates)
            dir->deck_a_primary_color = 2;  // Green — terminal/server
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 1; // linear-v
            dir->deck_b_gradient = 0;
            break;

        case DIR_SERVER_FACE:
            dir->deck_a_scene = 264;
            dir->deck_b_scene = 9;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;   // Green
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_ORGANIC_EYE:
            dir->deck_a_scene = 265;
            dir->deck_b_scene = 9;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 1;   // Red — surveillance
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 1;
            dir->deck_b_secondary_color = 2;
            dir->deck_a_gradient = 3;        // radial
            dir->deck_b_gradient = 0;
            break;

        case DIR_FACE_GALLERY:
            dir->deck_a_scene = 266;
            dir->deck_b_scene = 15;  // data stream bg
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 1;
            dir->deck_a_primary_color = 6;   // Cyan — data
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_HUMAN_FIGURES:
            dir->deck_a_scene = 267;
            dir->deck_b_scene = 0;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 4;   // Yellow — figures
            dir->deck_a_secondary_color = 7; // White
            dir->deck_b_primary_color = 2;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_FACE_MORPH:
            dir->deck_a_scene = 268;
            dir->deck_b_scene = 9;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 3;  // subtle glitch
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 5;   // Magenta — morph/chaos
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 5;
            dir->deck_b_secondary_color = 7;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_LIVE_SPECTRUM:
            dir->deck_a_scene = 269;
            dir->deck_b_scene = 0;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 7;   // White — Ikeda cold
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 7;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_LIVE_GRID:
            dir->deck_a_scene = 270;
            dir->deck_b_scene = 0;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 7;   // White
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 7;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_LIVE_PULSE:
            dir->deck_a_scene = 271;
            dir->deck_b_scene = 0;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 7;   // White
            dir->deck_a_secondary_color = 6; // Cyan
            dir->deck_b_primary_color = 7;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_SURFACE_WASH:
            dir->deck_a_scene = 280;  // Color Wash
            dir->deck_b_scene = 281;  // Gradient Sweep
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 2;  // Oscillate
            dir->deck_a_primary_color = 6;
            dir->deck_a_secondary_color = 4;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 4;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_SURFACE_AURORA:
            dir->deck_a_scene = 287;  // Aurora
            dir->deck_b_scene = 284;  // Wave Surface
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 3;  // Audio follow
            dir->deck_a_primary_color = 6;
            dir->deck_a_secondary_color = 2;
            dir->deck_b_primary_color = 4;
            dir->deck_b_secondary_color = 6;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        case DIR_SURFACE_PULSE:
            dir->deck_a_scene = 282;  // Audio Pulse
            dir->deck_b_scene = 289;  // Strobe Fields
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 4;  // Hard-cut on beat
            dir->deck_a_primary_color = 1;
            dir->deck_a_secondary_color = 3;
            dir->deck_b_primary_color = 7;
            dir->deck_b_secondary_color = 1;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;

        default:
            dir->deck_a_scene = 0;
            dir->deck_b_scene = 9;
            dir->deck_a_effect = 0;
            dir->deck_b_effect = 0;
            dir->charset = 0;
            dir->crossfade_mode = 0;
            dir->deck_a_primary_color = 2;
            dir->deck_a_secondary_color = 7;
            dir->deck_b_primary_color = 6;
            dir->deck_b_secondary_color = 3;
            dir->deck_a_gradient = 0;
            dir->deck_b_gradient = 0;
            break;
    }

    // Apply dynamic overrides: crossfade modes, effects, category-based deck_b
    director_add_dynamics(dir);
}

void director_start_transition(Director* dir, DirectorMode next) {
    dir->transitioning = true;
    dir->transition_elapsed = 0.0f;
    dir->next_mode = next;

    // Live mode: instant hard cuts (Ikeda-style)
    if (dir->live_mode) {
        dir->transition_duration = 0.0f;
        return;
    }

    // Cinematic mode: slower, more deliberate transitions
    if (dir->cinematic_mode) {
        if (is_face_mode(next) || is_face_mode(dir->current_mode)) {
            dir->transition_duration = 1.2f;  // dramatic fade for face reveals
        } else {
            dir->transition_duration = 0.8f;  // clean scene transitions
        }
        return;
    }

    // Transition duration varies by context
    if (next == DIR_ERROR_CASCADE || dir->current_mode == DIR_ERROR_CASCADE) {
        dir->transition_duration = 0.2f;  // hard cut
    } else if (next == DIR_DRIFT || dir->current_mode == DIR_DRIFT) {
        dir->transition_duration = 1.5f;  // slow crossfade
    } else if (next == DIR_BIG_TEXT) {
        dir->transition_duration = 0.3f;  // quick punch
    } else if (next == DIR_CONSCIOUSNESS || dir->current_mode == DIR_CONSCIOUSNESS) {
        dir->transition_duration = 2.0f;  // very slow dissolve
    } else if (next == DIR_LORE_NARRATIVE || next == DIR_POETRY) {
        dir->transition_duration = 1.0f;  // gentle fade
    } else if (next == DIR_EXPLOSION_MONTAGE || next == DIR_SYSTEM_OVERLOAD || next == DIR_FINAL_WARNING) {
        dir->transition_duration = 0.1f;  // violent hard cut
    } else if (next == DIR_FACTION_WAR || next == DIR_FLASH_MANIFESTO) {
        dir->transition_duration = 0.15f;  // aggressive cut
    } else if (next == DIR_TRUTH_MACHINE || next == DIR_BIOMETRIC) {
        dir->transition_duration = 0.2f;  // sharp
    } else if (next == DIR_TAZ || next == DIR_RHIZOME) {
        dir->transition_duration = 1.0f;  // philosophical fade
    } else if (next == DIR_SIMULACRA || next == DIR_EUROPA_DESCENT || next == DIR_COSMIC_BG) {
        dir->transition_duration = 1.5f;  // dreamy dissolve
    } else if (next == DIR_MARKET_MELTDOWN) {
        dir->transition_duration = 0.15f; // violent crash
    } else if (next == DIR_NEURAL_FUSION || next == DIR_BARCELONA) {
        dir->transition_duration = 0.3f;  // intense
    } else if (next == DIR_LOVE_VIRUS || next == DIR_QUANTUM_FIELD) {
        dir->transition_duration = 1.0f;  // gentle spread
    } else if (next == DIR_REISUB) {
        dir->transition_duration = 0.5f;  // system command
    } else if (next == DIR_WORLD_MAP) {
        dir->transition_duration = 0.8f;  // medium scan-in
    } else if (next == DIR_COUNTRY_INTEL) {
        dir->transition_duration = 0.5f;  // brisk classified
    } else if (next == DIR_GEO_DRIFT) {
        dir->transition_duration = 1.5f;  // dreamy dissolve
    } else if (next == DIR_SERVER_SPEAKS) {
        dir->transition_duration = 0.3f;  // quick fade to server voice
    } else if (next == DIR_SERVER_FACE || next == DIR_ORGANIC_EYE) {
        dir->transition_duration = 0.8f;  // dramatic reveal
    } else if (next == DIR_FACE_GALLERY || next == DIR_FACE_MORPH) {
        dir->transition_duration = 0.5f;  // brisk data cut
    } else if (next == DIR_HUMAN_FIGURES) {
        dir->transition_duration = 0.6f;  // moderate
    } else {
        dir->transition_duration = 0.5f + randf() * 0.5f;  // 0.5-1s
    }
}

const char* director_mode_name(DirectorMode mode) {
    switch (mode) {
        case DIR_BOOT:               return "BOOT";
        case DIR_AMBIENT:            return "AMBIENT";
        case DIR_NEWS_FEED:          return "NEWS_FEED";
        case DIR_MASTODON_INTERCEPT: return "MASTODON";
        case DIR_STOCK_CRASH:        return "STOCK_CRASH";
        case DIR_RECOVERY:           return "RECOVERY";
        case DIR_ERROR_CASCADE:      return "ERROR_CASCADE";
        case DIR_DRIFT:              return "DRIFT";
        case DIR_WIFI_SURVEY:        return "WIFI_SURVEY";
        case DIR_SERVER_MOCKERY:     return "SERVER_MOCKERY";
        case DIR_NETWORK_MAP:        return "NETWORK_MAP";
        case DIR_WIREFRAME:          return "WIREFRAME";
        case DIR_ROGUELIKE:          return "ROGUELIKE";
        case DIR_BIG_TEXT:           return "BIG_TEXT";
        case DIR_SCIFI:              return "SCIFI";
        case DIR_SURVEILLANCE:       return "SURVEILLANCE";
        case DIR_CPU_SCHEMATIC:      return "CPU_SCHEMATIC";
        case DIR_AUDIO_DASH:         return "AUDIO_DASH";
        case DIR_DATA_TRANSFER:      return "DATA_TRANSFER";
        case DIR_WAFER_MAP:          return "WAFER_MAP";
        case DIR_SERVER_ROOM:        return "SERVER_ROOM";
        case DIR_PANOPTICON:         return "PANOPTICON";
        case DIR_SPLIT_DASH:         return "SPLIT_DASH";
        case DIR_LORE_NARRATIVE:     return "LORE_NARRATIVE";
        case DIR_HEX_DUMP:          return "HEX_DUMP";
        case DIR_MOTION_ANALYZER:    return "MOTION_ANALYZER";
        case DIR_CONSCIOUSNESS:      return "CONSCIOUSNESS";
        case DIR_VERTICAL_SCROLLER:  return "VERT_SCROLLER";
        case DIR_EXPLOSION_MONTAGE:  return "EXPLOSIONS";
        case DIR_CRIC_META:          return "CRIC_META";
        case DIR_SOCIAL_FEED:        return "SOCIAL_FEED";
        case DIR_PROPAGANDA:         return "PROPAGANDA";
        case DIR_POETRY:             return "POETRY";
        case DIR_FACTION_WAR:        return "FACTION_WAR";
        case DIR_CODE_RAIN:          return "CODE_RAIN";
        case DIR_TIMELINE_SCROLL:    return "TIMELINE";
        case DIR_NEWS_WALL:          return "NEWS_WALL";
        case DIR_SYSTEM_OVERLOAD:    return "OVERLOAD";
        case DIR_SPACE_BATTLE:       return "SPACE_BATTLE";
        case DIR_TAZ:                return "T.A.Z.";
        case DIR_RETRO_ARCADE:       return "RETRO_ARCADE";
        case DIR_SIMULACRA:          return "SIMULACRA";
        case DIR_RHIZOME:            return "RHIZOME";
        case DIR_TRUTH_MACHINE:      return "TRUTH_MACHINE";
        case DIR_BIOMETRIC:          return "BIOMETRIC";
        case DIR_BELIEF_ENGINE:      return "BELIEF_ENGINE";
        case DIR_TETRIS_RAIN:        return "TETRIS_RAIN";
        case DIR_FLASH_MANIFESTO:    return "FLASH_MANIFESTO";
        case DIR_DISCIPLINE:         return "DISCIPLINE";
        case DIR_PIRATE_RADIO:       return "PIRATE_RADIO";
        case DIR_FINAL_WARNING:      return "FINAL_WARNING";
        case DIR_CRYPTO_TICKER:      return "CRYPTO";
        case DIR_NEURAL_FUSION:      return "NEURAL_FUSION";
        case DIR_EUROPA_DESCENT:     return "EUROPA";
        case DIR_ECOSYSTEM:          return "ECOSYSTEM";
        case DIR_MARKET_MELTDOWN:    return "MELTDOWN";
        case DIR_BARCELONA:          return "BARCELONA";
        case DIR_DNA_SEQUENCER:      return "DNA";
        case DIR_BLOCKCHAIN:         return "BLOCKCHAIN";
        case DIR_LOVE_VIRUS:         return "LOVE_VIRUS";
        case DIR_PARTICLE_ACCEL:     return "PARTICLE";
        case DIR_EDSA_CONTROL:       return "EDSA";
        case DIR_QUANTUM_FIELD:      return "QUANTUM";
        case DIR_SERVER_DIAG:        return "SERVER_DIAG";
        case DIR_CLIMATE:            return "CLIMATE";
        case DIR_CRASH_VOICE:        return "CRASH_VOICE";
        case DIR_SPECTRAL:           return "SPECTRAL";
        case DIR_NETWORK_TOPO:       return "NET_TOPO";
        case DIR_MEMORY_PALACE:      return "MEMORY";
        case DIR_COSMIC_BG:          return "COSMIC";
        case DIR_REISUB:             return "REISUB";
        case DIR_WORLD_MAP:          return "WORLD_MAP";
        case DIR_COUNTRY_INTEL:      return "COUNTRY_INTEL";
        case DIR_GEO_DRIFT:          return "GEO_DRIFT";
        case DIR_SERVER_SPEAKS:      return "SERVER_SPEAKS";
        case DIR_SERVER_FACE:        return "SERVER_FACE";
        case DIR_ORGANIC_EYE:        return "ORGANIC_EYE";
        case DIR_FACE_GALLERY:       return "FACE_GALLERY";
        case DIR_HUMAN_FIGURES:      return "HUMAN_FIGURES";
        case DIR_FACE_MORPH:         return "FACE_MORPH";
        case DIR_LIVE_SPECTRUM:      return "LIVE_SPECTRUM";
        case DIR_LIVE_GRID:          return "LIVE_GRID";
        case DIR_LIVE_PULSE:         return "LIVE_PULSE";
        case DIR_SURFACE_WASH:       return "SURFACE_WASH";
        case DIR_SURFACE_AURORA:     return "SURFACE_AURORA";
        case DIR_SURFACE_PULSE:      return "SURFACE_PULSE";
        default:                     return "UNKNOWN";
    }
}
