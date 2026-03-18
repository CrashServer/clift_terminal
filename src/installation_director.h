#ifndef INSTALLATION_DIRECTOR_H
#define INSTALLATION_DIRECTOR_H

#include <stdbool.h>

typedef enum {
    DIR_BOOT = 0,
    DIR_AMBIENT,
    DIR_NEWS_FEED,
    DIR_MASTODON_INTERCEPT,
    DIR_STOCK_CRASH,
    DIR_RECOVERY,
    DIR_ERROR_CASCADE,
    DIR_DRIFT,
    DIR_WIFI_SURVEY,
    DIR_SERVER_MOCKERY,
    DIR_NETWORK_MAP,
    DIR_WIREFRAME,
    DIR_ROGUELIKE,
    DIR_BIG_TEXT,
    DIR_SCIFI,
    DIR_SURVEILLANCE,
    DIR_CPU_SCHEMATIC,
    DIR_AUDIO_DASH,
    DIR_DATA_TRANSFER,
    DIR_WAFER_MAP,
    DIR_SERVER_ROOM,
    DIR_PANOPTICON,
    DIR_SPLIT_DASH,
    DIR_LORE_NARRATIVE,
    DIR_HEX_DUMP,
    DIR_MOTION_ANALYZER,
    DIR_CONSCIOUSNESS,
    DIR_VERTICAL_SCROLLER,
    DIR_EXPLOSION_MONTAGE,
    DIR_CRIC_META,
    DIR_SOCIAL_FEED,
    DIR_PROPAGANDA,
    DIR_POETRY,
    DIR_FACTION_WAR,
    DIR_CODE_RAIN,
    DIR_TIMELINE_SCROLL,
    DIR_NEWS_WALL,
    DIR_SYSTEM_OVERLOAD,
    DIR_SPACE_BATTLE,
    DIR_TAZ,
    DIR_RETRO_ARCADE,
    DIR_SIMULACRA,
    DIR_RHIZOME,
    DIR_TRUTH_MACHINE,
    DIR_BIOMETRIC,
    DIR_BELIEF_ENGINE,
    DIR_TETRIS_RAIN,
    DIR_FLASH_MANIFESTO,
    DIR_DISCIPLINE,
    DIR_PIRATE_RADIO,
    DIR_FINAL_WARNING,
    DIR_CRYPTO_TICKER,
    DIR_NEURAL_FUSION,
    DIR_EUROPA_DESCENT,
    DIR_ECOSYSTEM,
    DIR_MARKET_MELTDOWN,
    DIR_BARCELONA,
    DIR_DNA_SEQUENCER,
    DIR_BLOCKCHAIN,
    DIR_LOVE_VIRUS,
    DIR_PARTICLE_ACCEL,
    DIR_EDSA_CONTROL,
    DIR_QUANTUM_FIELD,
    DIR_SERVER_DIAG,
    DIR_CLIMATE,
    DIR_CRASH_VOICE,
    DIR_SPECTRAL,
    DIR_NETWORK_TOPO,
    DIR_MEMORY_PALACE,
    DIR_COSMIC_BG,
    DIR_REISUB,
    DIR_WORLD_MAP,
    DIR_COUNTRY_INTEL,
    DIR_GEO_DRIFT,
    DIR_SERVER_SPEAKS,
    DIR_SERVER_FACE,
    DIR_ORGANIC_EYE,
    DIR_FACE_GALLERY,
    DIR_HUMAN_FIGURES,
    DIR_FACE_MORPH,
    DIR_LIVE_SPECTRUM,
    DIR_LIVE_GRID,
    DIR_LIVE_PULSE,
    DIR_SURFACE_WASH,
    DIR_SURFACE_AURORA,
    DIR_SURFACE_PULSE,
    DIR_MODE_COUNT
} DirectorMode;

typedef struct {
    DirectorMode current_mode;
    DirectorMode previous_mode;
    float mode_elapsed;          // seconds in current mode
    float mode_duration;         // target duration for current mode
    float global_intensity;      // 0.0-1.0, slow sine ~25min cycle
    float intensity_phase;       // phase of intensity sine
    float total_runtime;         // seconds since start
    bool installation_active;
    bool boot_complete;

    // Transition state
    bool transitioning;
    float transition_elapsed;
    float transition_duration;
    DirectorMode next_mode;

    // Per-mode scene assignments
    int deck_a_scene;
    int deck_b_scene;
    int deck_a_effect;       // PostEffect as int to avoid circular dep
    int deck_b_effect;
    int charset;
    int crossfade_mode;

    // Color assignments (1-7: Red,Green,Blue,Yellow,Magenta,Cyan,White)
    int deck_a_primary_color;
    int deck_a_secondary_color;
    int deck_b_primary_color;
    int deck_b_secondary_color;
    int deck_a_gradient;     // GradientType 0-9
    int deck_b_gradient;

    // Data-reactive intensity modifiers
    float intensity_spike;       // temporary spike (decays)
    float intensity_spike_decay; // decay rate

    // Random post-effect system
    float effect_swap_timer;     // countdown to next random effect swap
    float effect_swap_interval;  // seconds between swaps (varies with intensity)
    bool fx_chaos_mode;          // when true, effects change rapidly
    float fx_chaos_duration;     // remaining chaos mode time

    // Speed/duration control (--speed parameter)
    float duration_scale;        // multiplier for mode durations (1.0 = default)

    // Visual pulsation system
    float pulse_timer;           // countdown to next pulse event
    float pulse_cooldown;        // min seconds between pulses
    bool pulse_active;           // currently pulsing
    float pulse_remaining;       // remaining pulse duration
    int pulse_type;              // 0=invert, 1=flash, 2=color_swap, 3=all

    // Mid-scene dynamics (periodic deck/crossfade/color swapping)
    float deck_b_swap_timer;       // countdown to next deck B scene swap
    float crossfade_swap_timer;    // countdown to next crossfade mode change
    float color_drift_timer;       // countdown to next color variation

    // Pause / scene lock (--hold, --scene, --mode)
    bool paused;                   // hold current mode, no auto-advance
    int forced_scene;              // if > 0, override deck_a_scene

    // Narrative act system (5-act cycle, ~30 min)
    int current_act;               // 0-4: assertion, power, doubt, contact, love_virus
    int deferred_mode;             // stores real next mode during server interludes (-1 = none)

    // Cinematic director mode (--director)
    bool cinematic_mode;           // true = sequential beat script, false = weighted random
    int cinematic_beat;            // current beat index in cinematic_script[]

    // Live coding performance mode (--live)
    bool live_mode;                // true = beat-synced live scene rotation
    int live_beat_count;           // beats since last scene change
    int live_beats_per_scene;      // beats before switching (default 8)
} Director;

void director_init(Director* dir);
void director_update(Director* dir, float dt);
void director_select_mode(Director* dir);
void director_configure_mode(Director* dir);
void director_start_transition(Director* dir, DirectorMode next);
void director_start_cinematic(Director* dir);
void director_start_live(Director* dir);
const char* director_mode_name(DirectorMode mode);

// Global cinematic act override (-1 = use time-based, 0-4 = forced act)
extern int g_cinematic_act;

// Global beat flag (set by clift_engine each frame, read by director for live mode)
extern bool g_beat_detected;

#endif // INSTALLATION_DIRECTOR_H
