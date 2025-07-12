#define _DEFAULT_SOURCE
#include <ncurses.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <complex.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "link_wrapper.hpp"
#include "audio_pipewire.h"

// ============= CLIFT (CLI-Shift) ENGINE =============

// Audio analysis data
typedef struct {
    float bass, mid, treble, volume;
    float bpm;
    bool beat_detected;
    float beat_intensity;
    float spectrum[64];
    bool valid;
} AudioData;

// Post effect types (15 total)
typedef enum {
    POST_NONE = 0,
    POST_GLOW,
    POST_BLUR,
    POST_EDGE,
    POST_INVERT,
    POST_ASCII_GRADIENT,
    POST_SCANLINES,
    POST_CHROMATIC,
    POST_WAVE_WARP,
    POST_CHAR_EMISSION,
    POST_RIPPLE,
    POST_SPIRAL_WARP,
    POST_ECHO,
    POST_KALEIDOSCOPE,
    POST_DROSTE,
    POST_COUNT
} PostEffect;

// Gradient types for two-color blending
typedef enum {
    GRADIENT_LINEAR_H = 0,    // Horizontal linear
    GRADIENT_LINEAR_V = 1,    // Vertical linear
    GRADIENT_LINEAR_D1 = 2,   // Diagonal top-left to bottom-right
    GRADIENT_LINEAR_D2 = 3,   // Diagonal top-right to bottom-left
    GRADIENT_RADIAL = 4,      // Circular from center
    GRADIENT_DIAMOND = 5,     // Diamond shape
    GRADIENT_WAVE_H = 6,      // Horizontal wave
    GRADIENT_WAVE_V = 7,      // Vertical wave
    GRADIENT_NOISE = 8,       // Random noise blend
    GRADIENT_SPIRAL = 9,      // Spiral pattern
    GRADIENT_COUNT = 10
} GradientType;

// Gradient type names for UI display
const char* gradient_names[] = {
    "Lin-H", "Lin-V", "Diag1", "Diag2", "Radial", 
    "Diamond", "Wave-H", "Wave-V", "Noise", "Spiral"
};

// Parameter with automation
typedef struct {
    float value;
    float target;
    float min, max;
    float speed;
    bool auto_mode;
    float lfo_freq;
    float lfo_amount;
    const char* name;
} Parameter;

// CLIFT Deck with post effects
typedef struct {
    int scene_id;
    PostEffect post_effect;
    Parameter params[8];  // Scene parameters
    char* buffer;
    float* zbuffer;
    bool active;
    bool selected;  // Visual indicator
    int primary_color;    // Primary color pair (1-7)
    int secondary_color;  // Secondary color pair for intensity
    GradientType gradient_type;  // How to blend the two colors
} CLIFTDeck;

// Crossfade states for simple 3-state mixing
typedef enum {
    XFADE_FULL_A = 0,    // Only deck A visible
    XFADE_MIX = 1,       // Both decks mixed
    XFADE_FULL_B = 2     // Only deck B visible
} CrossfadeState;

// BPM and timing system
typedef struct {
    float bpm;
    float last_tap_time;
    float tap_times[8];  // Store last 8 taps for averaging
    int tap_count;
    bool auto_crossfade_enabled;
    float crossfade_beat_interval;  // How many beats between crossfade changes (default: 16)
    float last_crossfade_time;
} BPMSystem;

// UI Pages for better organization
typedef enum {
    UI_PAGE_PERFORMANCE = 0,  // Minimal live performance view
    UI_PAGE_PRESETS = 1,      // Preset management
    UI_PAGE_SETTINGS = 2,     // Parameter control and settings
    UI_PAGE_MONITOR = 3,      // Future websocket/audio data
    UI_PAGE_HELP = 4,         // Controls and help
    UI_PAGE_LINK = 5,         // Ableton Link sync
    UI_PAGE_AUDIO = 6,        // Audio input settings and visualization
    UI_PAGE_COUNT = 7
} UIPage;

// Ableton Link state
typedef struct {
    bool enabled;
    bool connected;
    float link_bpm;
    float link_beat;
    float link_phase;
    int num_peers;
    float quantum;  // Bar length in beats
    bool start_stop_sync;
    bool is_playing;
    void* link_handle;  // Handle to C++ Link object
} AbletonLinkState;

// Preset data structure
typedef struct {
    char name[32];                    // Preset name
    int deck_a_scene;                 // Deck A scene ID
    int deck_b_scene;                 // Deck B scene ID
    int deck_a_primary_color;         // Deck A primary color
    int deck_a_secondary_color;       // Deck A secondary color
    int deck_b_primary_color;         // Deck B primary color
    int deck_b_secondary_color;       // Deck B secondary color
    GradientType deck_a_gradient;     // Deck A gradient type
    GradientType deck_b_gradient;     // Deck B gradient type
    PostEffect deck_a_effect;         // Deck A post effect
    PostEffect deck_b_effect;         // Deck B post effect
    CrossfadeState crossfade_state;   // Crossfader position
    float deck_a_params[8];           // Deck A parameter values
    float deck_b_params[8];           // Deck B parameter values
    bool is_used;                     // Whether this preset slot is used
} Preset;

#define MAX_PRESETS 20

// Live coding display system
typedef struct {
    char player_name[32];        // "Player 1", "Player 2", etc.
    char current_code[512];      // Current code being typed/executed
    char last_executed[256];     // Last executed line
    bool is_active;              // Whether player is currently coding
    float last_update_time;      // When last updated
} LiveCodingPlayer;

typedef struct {
    bool enabled;                    // Websocket server enabled
    int port;                       // Server port (default 8080)
    LiveCodingPlayer players[2];    // Two live coding players
    float cpu_usage;                // Current CPU usage %
    float memory_usage;             // Current memory usage %
    bool display_overlay;           // Show overlay on visuals
    int overlay_opacity;            // Overlay transparency (0-100)
    
    // Websocket server internals
    int server_socket;              // Server socket file descriptor
    pthread_t server_thread;        // Server thread handle
    bool server_running;            // Server thread status
    int client_sockets[8];          // Connected client sockets (max 8)
    bool client_handshake_done[8];  // Track if handshake completed for each client
    int client_count;               // Number of connected clients
} LiveCodingMonitor;

#define MAX_PRESETS 20

// Forward declarations
void init_presets();
void save_preset(int preset_id, const char* name);
void load_preset(int preset_id);
void clear_preset(int preset_id);
void cycle_preset_name(int preset_id);
void init_live_coding_monitor();
void update_cpu_usage();
void parse_websocket_message(const char* message);
void render_live_coding_overlay();
void* websocket_server_thread(void* arg);
void start_websocket_server();
void stop_websocket_server();
void handle_websocket_handshake(int client_socket);
void process_websocket_frame(int client_socket, unsigned char* frame, size_t len);

// Main CLIFT Engine
typedef struct {
    CLIFTDeck deck_a;
    CLIFTDeck deck_b;
    CrossfadeState crossfade_state;
    Parameter master_volume;
    Parameter master_speed;
    
    char* output_buffer;
    char* temp_buffer;
    float* output_zbuffer;
    
    int width, height;
    float time;
    float effect_time;
    
    // BPM and timing
    BPMSystem bpm_system;
    
    // UI State
    bool performance_mode;
    int selected_deck;      // 0=A, 1=B
    int selected_param;
    UIPage current_ui_page; // Current UI page
    bool show_help;
    bool hide_ui;
    
    // Preset system
    Preset presets[MAX_PRESETS];
    int selected_preset;    // Currently selected preset (0-19)
    int preset_page;        // For paging through presets (0-3 for 5 presets per page)
    
    // Live coding monitor
    LiveCodingMonitor live_coding;
    
    // Full Auto Mode
    bool full_auto_mode;
    float last_auto_change;
    float auto_change_interval;
    float last_effect_change;
    float effect_change_interval;
    
    // Ableton Link integration
    AbletonLinkState link;
    
    // Audio input system
    AudioData audio_data;
    bool audio_enabled;
    float audio_gain;
    float audio_smoothing;
    int audio_device_id;
    char audio_device_name[64];
    pthread_t audio_thread;
    pthread_mutex_t audio_mutex;
    bool audio_thread_running;
    int selected_audio_source;  // For cycling through available sources
} CLIFTEngine;

// Global engine
CLIFTEngine vj;
bool running = true;

// Constants for buffer sizes
#define MAX_WIDTH 256
#define MAX_HEIGHT 128

// Function declarations
void draw_fractal_building(char* buffer, float* zbuffer, int width, int height, 
                          float x, float y, float z, float size, int depth,
                          float t, float twist_factor);

// Scene names - 190 scenes across 19 categories
const char* scene_names[] = {
    // Basic (0-9)
    "Audio Bars", "Rotating Cube", "DNA Helix", "Particle Field", 
    "Torus", "Fractal Tree", "Wave Mesh", "Sphere", "Spirograph", "Matrix Rain",
    
    // Geometric (10-19)
    "Tunnels", "Kaleidoscope", "Mandala", "Sierpinski", "Hexagon Grid",
    "Tessellations", "Voronoi Cells", "Sacred Geometry", "Polyhedra", "Maze Generator",
    
    // Organic (20-29) 
    "Fire Simulation", "Water Waves", "Lightning", "Plasma Clouds", "Galaxy Spiral",
    "Tree of Life", "Cellular Automata", "Flocking Birds", "Wind Patterns", "Neural Networks",
    
    // Text/Code (30-39)
    "Code Rain", "Terminal Hacking", "Binary Waterfall", "ASCII Art Morph", "Glitch Text",
    "Data Streams", "Circuit Patterns", "QR Code Rain", "Font Showcase", "Terminal Commands",
    
    // Abstract (40-49)
    "Noise Field", "Interference", "Hologram", "Digital Rain", "Glitch Corruption",
    "Signal Static", "Bitmap Fade", "Pixel Sort", "Datamosh", "Buffer Overflow",
    
    // Infinite Tunnels (50-59)
    "Spiral Tunnel", "Hex Tunnel", "Star Tunnel", "Wormhole", "Cyber Tunnel",
    "Ring Tunnel", "Matrix Tunnel", "Speed Tunnel", "Pulse Tunnel", "Vortex Tunnel",
    
    // Nature (60-69)
    "Ocean Waves", "Rain Storm", "Infinite Forest", "Growing Trees", "Mountain Range",
    "Aurora Borealis", "Flowing River", "Desert Dunes", "Coral Reef", "Butterfly Garden",
    
    // Explosions (70-79)
    "Nuclear Blast", "Building Collapse", "Meteor Impact", "Chain Explosions", "Volcanic Eruption",
    "Shockwave Blast", "Glass Shatter", "Demolition Blast", "Supernova Burst", "Plasma Discharge",
    
    // Cities (80-89)
    "Cyberpunk City", "City Lights", "Skyscraper Forest", "Urban Decay", "Future Metropolis",
    "City Grid", "Digital City", "City Flythrough", "Neon Districts", "Urban Canyon",
    
    // Freestyle (90-99)
    "Black Hole", "Quantum Field", "Dimensional Rift", "Alien Landscape", "Robot Factory",
    "Time Vortex", "Glitch World", "Neural Network", "Cosmic Dance", "Reality Glitch",
    
    // Human (100-109)
    "Human Walker", "Dance Party", "Martial Arts", "Human Pyramid", "Yoga Flow",
    "Sports Stadium", "Robot Dance", "Crowd Wave", "Mirror Dance", "Human Evolution",
    
    // Warfare (110-119)
    "Fighter Squadron", "Drone Swarm", "Strategic Bombing", "Dogfight", "Helicopter Assault",
    "Stealth Mission", "Carrier Strike", "Missile Defense", "Recon Drone", "Air Command",
    
    // Revolution & Eyes (120-129)
    "Street Revolution", "Barricade Building", "CCTV Camera", "Giant Eye", "Crowd March",
    "Displaced Sphere", "Morphing Cube", "Protest Rally", "Surveillance Eyes", "Fractal Displacement",
    
    // Film Noir (130-139)
    "Venetian Blinds", "Silhouette Door", "Rain Window", "Detective Coat", "Femme Fatale",
    "Smoke Room", "Stair Shadows", "Car Headlights", "Neon Rain", "Film Strip",
    
    // Escher 3D Illusions (140-149)
    "Impossible Stairs", "Möbius Strip", "Impossible Cube", "Penrose Triangle", "Infinite Corridor",
    "Tessellated Reality", "Gravity Wells", "Dimensional Shift", "Fractal Architecture", "Escher Waterfall",
    
    // Ikeda-Inspired (150-159)
    "Data Matrix", "Test Pattern", "Sine Wave", "Barcode", "Pulse",
    "Glitch", "Spectrum", "Phase", "Binary", "Circuit",
    
    // Giger-Inspired (160-169)
    "Biomech Spine", "Alien Eggs", "Mech Tentacles", "Xenomorph Hive", "Biomech Skull",
    "Face Hugger", "Biomech Heart", "Alien Architecture", "Chestburster", "Space Jockey",
    
    // Revolt (170-179)
    "Rising Fists", "Breaking Chains", "Crowd March", "Barricade Building", "Molotov Cocktails",
    "Tear Gas", "Graffiti Wall", "Police Line Breaking", "Flag Burning", "Victory Dance",
    
    // Audio Reactive (180-189)
    "Audio 3D Cubes", "Audio Strobes", "Audio Explosions", "Audio Wave Tunnel", "Audio Spectrum 3D",
    "Audio Particles", "Audio Pulse Rings", "Audio Waveform 3D", "Audio Matrix Grid", "Audio Fractals"
};

const char* post_effect_names[] = {
    "None", "Glow", "Blur", "Edge", "Invert", "ASCII", "Scanlines", 
    "Chromatic", "WaveWarp", "CharEmit", "Ripple", "Spiral", "Echo", "Kaleidoscope", "Droste"
};

// ============= MATH & UTILITIES =============

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float clamp(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

void param_init(Parameter* p, const char* name, float value, float min, float max) {
    p->value = p->target = value;
    p->min = min;
    p->max = max;
    p->speed = 5.0f;
    p->auto_mode = false;
    p->lfo_freq = 1.0f;
    p->lfo_amount = 0.0f;
    p->name = name;
}

void param_update(Parameter* p, float dt) {
    if (p->lfo_amount > 0) {
        float lfo_val = sinf(vj.time * p->lfo_freq * 2 * M_PI) * p->lfo_amount;
        p->target = clamp(p->value + lfo_val, p->min, p->max);
    }
    
    if (fabsf(p->value - p->target) > 0.001f) {
        p->value = lerp(p->value, p->target, p->speed * dt);
    }
}

// ============= AUDIO ANALYSIS =============


// ============= VISUAL UTILITIES =============

void clear_buffer(char* buffer, float* zbuffer, int width, int height) {
    memset(buffer, ' ', width * height);
    for (int i = 0; i < width * height; i++) {
        zbuffer[i] = 1000.0f;
    }
}

void set_pixel(char* buffer, float* zbuffer, int width, int height, int x, int y, char c, float z) {
    // Debug: Extra safety checks
    if (!buffer || !zbuffer) {
        fprintf(stderr, "ERROR: set_pixel called with NULL buffer\n");
        return;
    }
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "ERROR: set_pixel called with invalid dimensions: %dx%d\n", width, height);
        return;
    }
    
    if (x >= 0 && x < width && y >= 0 && y < height) {
        int idx = y * width + x;
        // Extra safety: check idx bounds
        if (idx >= 0 && idx < width * height) {
            if (z < zbuffer[idx]) {
                buffer[idx] = c;
                zbuffer[idx] = z;
            }
        } else {
            fprintf(stderr, "ERROR: set_pixel calculated invalid index %d for %dx%d at (%d,%d)\n", 
                    idx, width, height, x, y);
        }
    }
}

char get_pixel(char* buffer, int width, int height, int x, int y) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        return buffer[y * width + x];
    }
    return ' ';
}

void draw_text(char* buffer, float* zbuffer, int width, int height, int x, int y, const char* text, float z) {
    if (!text) return;
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        if (x + i >= 0 && x + i < width && y >= 0 && y < height) {
            set_pixel(buffer, zbuffer, width, height, x + i, y, text[i], z);
        }
    }
}

// ============= ALL 9 SCENES FROM ORIGINAL =============

// Scene 0: Audio Bars
void scene_audio_bars(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)time;
    clear_buffer(buffer, zbuffer, width, height);
    
    float bar_spacing = params[0].value * 3.0f + 0.5f;
    float bar_width_factor = params[1].value * 0.8f + 0.2f;
    float color_mode = params[2].value;
    
    if (audio && audio->valid) {
        // Use real audio spectrum data
        pthread_mutex_lock(&vj.audio_mutex);
        
        int num_bars = 32;
        int bar_width = (int)((width / (float)num_bars) * bar_width_factor);
        if (bar_width < 1) bar_width = 1;
        
        int spacing = (int)bar_spacing;
        int total_bar_space = bar_width + spacing;
        
        for (int i = 0; i < num_bars && i * total_bar_space < width; i++) {
            // Average two spectrum values for each bar
            float val1 = audio->spectrum[i * 2];
            float val2 = audio->spectrum[i * 2 + 1];
            float spectrum_val = (val1 + val2) * 0.5f;
            
            int bar_height = (int)(spectrum_val * height * 0.9f);
            
            // Choose character based on intensity and color mode
            char bar_char;
            if (color_mode < 0.33f) {
                bar_char = spectrum_val > 0.7f ? '#' : spectrum_val > 0.4f ? '*' : '+';
            } else if (color_mode < 0.66f) {
                const char* chars = "_.:=+*#%@";
                int char_idx = (int)(spectrum_val * 8.99f);
                bar_char = chars[char_idx];
            } else {
                bar_char = audio->beat_detected && i < 8 ? '@' : '█';
            }
            
            // Draw the bar
            for (int y = height - bar_height; y < height; y++) {
                for (int x = i * total_bar_space; x < i * total_bar_space + bar_width && x < width; x++) {
                    set_pixel(buffer, zbuffer, width, height, x, y, bar_char, 1.0f);
                }
            }
            
            // Add peak indicator
            if (bar_height > 0) {
                for (int x = i * total_bar_space; x < i * total_bar_space + bar_width && x < width; x++) {
                    set_pixel(buffer, zbuffer, width, height, x, height - bar_height - 1, '-', 0.5f);
                }
            }
        }
        
        // Display audio levels at the top
        int text_y = 1;
        char info[80];
        
        // Convert levels to visual bars
        const char* bars[] = {"[....]", "[x...]", "[xx..]", "[xxx.]", "[xxxx]"};
        int bass_idx = (int)(audio->bass * 4.0f);
        int mid_idx = (int)(audio->mid * 4.0f);
        int treble_idx = (int)(audio->treble * 4.0f);
        int vol_idx = (int)(audio->volume * 4.0f);
        
        // Clamp indices
        bass_idx = bass_idx > 4 ? 4 : bass_idx;
        mid_idx = mid_idx > 4 ? 4 : mid_idx;
        treble_idx = treble_idx > 4 ? 4 : treble_idx;
        vol_idx = vol_idx > 4 ? 4 : vol_idx;
        
        snprintf(info, sizeof(info), "B:%s M:%s T:%s V:%s %s", 
                 bars[bass_idx], bars[mid_idx], bars[treble_idx], bars[vol_idx],
                 audio->beat_detected ? "BEAT!" : "");
        for (int i = 0; info[i] && i < width - 2; i++) {
            set_pixel(buffer, zbuffer, width, height, i + 1, text_y, info[i], 0.0f);
        }
        
        pthread_mutex_unlock(&vj.audio_mutex);
    } else {
        // No audio - show placeholder
        const char* msg = "Audio Disabled - Enable in Audio Page (Tab to navigate)";
        int msg_len = strlen(msg);
        int start_x = (width - msg_len) / 2;
        int mid_y = height / 2;
        
        for (int i = 0; i < msg_len && start_x + i < width; i++) {
            set_pixel(buffer, zbuffer, width, height, start_x + i, mid_y, msg[i], 1.0f);
        }
    }
}

// Draw line between two points
void draw_line(char* buffer, int width, int height, int x0, int y0, int x1, int y1, char c) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
            buffer[y0 * width + x0] = c;
        }
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Draw circle using Bresenham's algorithm
void draw_circle(char* buffer, float* zbuffer, int width, int height, int cx, int cy, int radius, char c, float z) {
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        // Draw 8 octants
        set_pixel(buffer, zbuffer, width, height, cx + x, cy + y, c, z);
        set_pixel(buffer, zbuffer, width, height, cx + y, cy + x, c, z);
        set_pixel(buffer, zbuffer, width, height, cx - y, cy + x, c, z);
        set_pixel(buffer, zbuffer, width, height, cx - x, cy + y, c, z);
        set_pixel(buffer, zbuffer, width, height, cx - x, cy - y, c, z);
        set_pixel(buffer, zbuffer, width, height, cx - y, cy - x, c, z);
        set_pixel(buffer, zbuffer, width, height, cx + y, cy - x, c, z);
        set_pixel(buffer, zbuffer, width, height, cx + x, cy - y, c, z);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

// Scene 1: Rotating Cube (MUCH LARGER)
void scene_cube(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    // Much larger base size
    float size = 8.0f + (params[0].value - 1.0f) * 4.0f;
    float speed = params[1].value;
    
    if (false) {
        size *= (1.0f + 0.3f * 0.8f);
        speed *= (1.0f + 0.5f * 2.0f);
    }
    
    float angle_x = time * speed * 30.0f;
    float angle_y = time * speed * 45.0f;
    
    // 3D cube vertices
    float vertices[8][3] = {
        {-size, -size, -size}, { size, -size, -size},
        { size,  size, -size}, {-size,  size, -size},
        {-size, -size,  size}, { size, -size,  size},
        { size,  size,  size}, {-size,  size,  size}
    };
    
    // Rotate and project
    int projected[8][2];
    float cos_x = cosf(angle_x * M_PI / 180.0f);
    float sin_x = sinf(angle_x * M_PI / 180.0f);
    float cos_y = cosf(angle_y * M_PI / 180.0f);
    float sin_y = sinf(angle_y * M_PI / 180.0f);
    
    for (int i = 0; i < 8; i++) {
        // Y rotation
        float x = vertices[i][0] * cos_y - vertices[i][2] * sin_y;
        float z = vertices[i][0] * sin_y + vertices[i][2] * cos_y;
        float y = vertices[i][1];
        
        // X rotation
        float new_y = y * cos_x - z * sin_x;
        z = y * sin_x + z * cos_x;
        y = new_y;
        
        // Project to screen with much closer camera and larger scale
        z += 25.0f;  // Camera distance
        if (z > 0.1f) {
            // Much larger projection scale
            projected[i][0] = (int)(width/2 + x * (width/4.0f) / z);
            projected[i][1] = (int)(height/2 + y * (height/3.0f) / z);
        }
    }
    
    // Draw edges with lines
    int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    
    char edge_char = false ? '#' : '*';
    for (int e = 0; e < 12; e++) {
        int p1 = edges[e][0], p2 = edges[e][1];
        
        // Draw thick lines for better visibility
        draw_line(buffer, width, height, projected[p1][0], projected[p1][1], projected[p2][0], projected[p2][1], edge_char);
        
        // Draw vertices as points
        set_pixel(buffer, zbuffer, width, height, projected[p1][0], projected[p1][1], 'o', 1.0f);
        set_pixel(buffer, zbuffer, width, height, projected[p2][0], projected[p2][1], 'o', 1.0f);
    }
}

// Scene 2: DNA Helix (MUCH LARGER)
void scene_dna_helix(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    // Much larger helix
    float radius = 8.0f + params[0].value * 3.0f;
    float height_span = height * 0.8f;  // Use most of screen height
    int turns = 4;
    int points_per_turn = 30;
    int total_points = turns * points_per_turn;
    
    if (false) {
        radius += 0.5f * 4.0f;
    }
    
    for (int i = 0; i <= total_points; i++) {
        float t = (float)i / points_per_turn;
        float angle = t * 2.0f * M_PI + time;
        float y = (height_span * t / turns) - height_span / 2.0f;
        
        // First helix
        float x1 = radius * cosf(angle);
        float z1 = radius * sinf(angle) + 20.0f;  // Depth
        
        // Second helix (180 degrees offset)
        float x2 = radius * cosf(angle + M_PI);
        float z2 = radius * sinf(angle + M_PI) + 20.0f;
        
        // Project to screen with much larger scale
        if (z1 > 0.1f && z2 > 0.1f) {
            int px1 = width/2 + (int)(x1 * (width/6.0f) / z1);
            int py1 = height/2 + (int)(y * (height/height_span));
            int px2 = width/2 + (int)(x2 * (width/6.0f) / z2);
            int py2 = height/2 + (int)(y * (height/height_span));
            
            char dna_char1 = false && 0.3f > 0.5f ? '#' : '+';
            char dna_char2 = false && 0.4f > 0.5f ? '*' : '+';
            
            set_pixel(buffer, zbuffer, width, height, px1, py1, dna_char1, z1);
            set_pixel(buffer, zbuffer, width, height, px2, py2, dna_char2, z2);
            
            // Connecting lines (more frequent)
            if (i % (points_per_turn/6) == 0) {
                draw_line(buffer, width, height, px1, py1, px2, py2, '-');
            }
            
            // Connect to previous point for continuous helix
            if (i > 0) {
                // Previous points calculation
                float prev_t = (float)(i-1) / points_per_turn;
                float prev_angle = prev_t * 2.0f * M_PI + time;
                float prev_y = (height_span * prev_t / turns) - height_span / 2.0f;
                
                float prev_x1 = radius * cosf(prev_angle);
                float prev_z1 = radius * sinf(prev_angle) + 20.0f;
                
                if (prev_z1 > 0.1f) {
                    int prev_px1 = width/2 + (int)(prev_x1 * (width/6.0f) / prev_z1);
                    int prev_py1 = height/2 + (int)(prev_y * (height/height_span));
                    
                    if (i % 2 == 0) {  // Every other point to avoid clutter
                        draw_line(buffer, width, height, prev_px1, prev_py1, px1, py1, '.');
                    }
                }
            }
        }
    }
}

// Scene 3: Particle Field
void scene_particle_field(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    int num_particles = 150 + (int)(params[0].value * 100);
    
    if (false) {
        num_particles += (int)(0.5f * 100);
    }
    
    for (int i = 0; i < num_particles; i++) {
        float seed = (float)i * 0.12345f;
        float angle = time + seed * 10.0f;
        float radius = 3.0f + sinf(seed * 5.0f) * 5.0f;
        float height_offset = sinf(time * 0.3f + seed * 3.0f) * 5.0f;
        
        float x = radius * cosf(angle);
        float y = height_offset + sinf(time + seed) * 2.0f;
        float z = radius * sinf(angle) + 8.0f;
        
        if (z > 0.1f) {
            int px = width/2 + (int)(x * 15.0f / z);
            int py = height/2 + (int)(y * 10.0f / z);
            
            char particle_char = false && 0.4f > 0.8f ? '*' :
                                 false && 0.4f > 0.6f ? '+' :
                                 false && 0.3f > 0.4f ? 'o' : '.';
            
            set_pixel(buffer, zbuffer, width, height, px, py, particle_char, z);
        }
    }
}

// Scene 4: Torus
void scene_torus(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float major_radius = 4.0f;
    float minor_radius = 1.5f + params[0].value;
    int major_segments = 20;
    int minor_segments = 15;
    
    if (false) {
        minor_radius += 0.3f * 2.0f;
    }
    
    for (int i = 0; i < major_segments; i++) {
        float theta = (float)i / major_segments * 2 * M_PI;
        
        for (int j = 0; j < minor_segments; j++) {
            float phi = (float)j / minor_segments * 2 * M_PI;
            
            float x = (major_radius + minor_radius * cosf(phi)) * cosf(theta);
            float y = minor_radius * sinf(phi);
            float z = (major_radius + minor_radius * cosf(phi)) * sinf(theta) + 10.0f;
            
            // Rotate around Y and X
            float cos_t = cosf(time * 30.0f * M_PI / 180.0f);
            float sin_t = sinf(time * 30.0f * M_PI / 180.0f);
            float new_x = x * cos_t - z * sin_t;
            z = x * sin_t + z * cos_t;
            x = new_x;
            
            if (z > 0.1f) {
                int px = width/2 + (int)(x * 12.0f / z);
                int py = height/2 + (int)(y * 12.0f / z);
                
                char torus_char = (i + j) % 4 == 0 ? '#' : '+';
                set_pixel(buffer, zbuffer, width, height, px, py, torus_char, z);
            }
        }
    }
}

// Recursive fractal tree drawing
void draw_tree_branch(char* buffer, int width, int height, int x, int y, float angle, float length, int depth, float time) {
    if (depth <= 0 || length < 2.0f) return;
    
    // Calculate end point
    int end_x = x + (int)(cosf(angle) * length);
    int end_y = y + (int)(sinf(angle) * length);
    
    // Draw the branch
    char branch_char = depth > 4 ? '#' : (depth > 2 ? '*' : (depth > 1 ? '+' : '.'));
    draw_line(buffer, width, height, x, y, end_x, end_y, branch_char);
    
    // Recursive branches
    float branch_angle = 30.0f + sinf(time + depth) * 15.0f;  // Animated branching
    float length_factor = 0.7f + sinf(time * 0.5f + depth) * 0.1f;  // Animated length
    
    // Left branch
    draw_tree_branch(buffer, width, height, end_x, end_y, 
                    angle - branch_angle * M_PI / 180.0f, 
                    length * length_factor, depth - 1, time);
    
    // Right branch  
    draw_tree_branch(buffer, width, height, end_x, end_y, 
                    angle + branch_angle * M_PI / 180.0f, 
                    length * length_factor, depth - 1, time);
    
    // Optional middle branch for fuller tree
    if (depth > 3) {
        draw_tree_branch(buffer, width, height, end_x, end_y, 
                        angle + sinf(time + depth) * 0.2f, 
                        length * 0.5f, depth - 2, time);
    }
}

// Scene 5: Proper Fractal Tree (MUCH LARGER)
void scene_fractal_tree(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer;
    clear_buffer(buffer, zbuffer, width, height);
    
    // Multiple trees for forest effect
    int num_trees = 3;
    float base_length = height * 0.25f;  // Much larger base length
    int max_depth = 6 + (int)(params[0].value * 2);  // Configurable depth
    
    if (false) {
        base_length *= (1.0f + 0.3f * 0.5f);
        max_depth += (int)(0.5f * 2);
    }
    
    for (int tree = 0; tree < num_trees; tree++) {
        // Tree positions
        int tree_x = width * (tree + 1) / (num_trees + 1);
        int tree_y = height - 5;
        
        // Main trunk angle (slightly different per tree)
        float trunk_angle = -M_PI/2 + (tree - 1) * 0.2f + sinf(time + tree) * 0.1f;
        
        // Draw the main tree
        draw_tree_branch(buffer, width, height, tree_x, tree_y, 
                        trunk_angle, base_length, max_depth, time + tree);
        
        // Draw roots
        for (int root = 0; root < 3; root++) {
            float root_angle = M_PI/2 + (root - 1) * 0.5f;
            draw_tree_branch(buffer, width, height, tree_x, tree_y,
                           root_angle, base_length * 0.3f, 3, time);
        }
    }
}

void scene_wave_mesh(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float intensity = false ? 0.5f : 0.5f;
    
    for (int y = 5; y < height - 5; y += 3) {
        for (int x = 5; x < width - 5; x += 4) {
            float fx = (float)x / width * 8.0f;
            float fy = (float)y / height * 6.0f;
            
            float wave = sinf(fx + time) * cosf(fy + time * 0.7f) * intensity;
            char wave_char = wave > 0.3f ? '#' : wave > 0.0f ? '*' : wave > -0.3f ? '+' : '.';
            
            set_pixel(buffer, zbuffer, width, height, x, y, wave_char, 1.0f);
        }
    }
}

void scene_sphere(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    // Much larger sphere
    float radius = width * 0.3f;  // Use 30% of screen width
    if (false) {
        radius *= (1.0f + 0.3f * 0.5f);
    }
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Draw multiple concentric spheres for depth
    for (int ring = 0; ring < 4; ring++) {
        float ring_radius = radius - ring * 3;
        if (ring_radius <= 0) continue;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dx = x - center_x;
                float dy = (y - center_y) * 2;  // Aspect correction
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist <= ring_radius && dist >= ring_radius - 2) {
                    float angle = atan2f(dy, dx) + time + ring;
                    float latitude = acosf(clamp(dy / ring_radius, -1.0f, 1.0f));
                    
                    // Create sphere pattern
                    char sphere_char;
                    if (ring == 0) {
                        sphere_char = ((int)(angle * 6 + latitude * 4) % 4 == 0) ? '#' : '*';
                    } else {
                        sphere_char = ((int)(angle * 4 + latitude * 3) % 3 == 0) ? '+' : '.';
                    }
                    
                    set_pixel(buffer, zbuffer, width, height, x, y, sphere_char, ring);
                }
            }
        }
    }
    
    // Add rotating meridians
    for (int meridian = 0; meridian < 8; meridian++) {
        float meridian_angle = meridian * M_PI / 4 + time * 0.5f;
        
        for (int point = 0; point < 50; point++) {
            float t = (float)point / 49.0f * M_PI;  // 0 to PI
            float x = radius * sinf(t) * cosf(meridian_angle);
            float y = radius * cosf(t) * 0.5f;  // Aspect correction
            
            int px = center_x + (int)x;
            int py = center_y + (int)y;
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                set_pixel(buffer, zbuffer, width, height, px, py, '|', 0.5f);
            }
        }
    }
}

void scene_spirograph(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    // Much larger spirograph - scale to screen size
    float scale = fminf(width, height * 2) * 0.35f;  // Use 35% of smaller dimension
    float R = scale * 0.8f;  // Outer radius
    float r = scale * 0.3f;  // Inner radius  
    float d = scale * 0.5f;  // Distance
    
    if (false) {
        R += 0.3f * scale * 0.3f;
        r += 0.4f * scale * 0.2f;
        d += 0.4f * scale * 0.2f;
    }
    
    // Parameters for variation
    float speed1 = 1.0f + params[0].value;
    float speed2 = 1.0f + params[1].value * 0.5f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Draw multiple spirograph patterns
    for (int pattern = 0; pattern < 3; pattern++) {
        float pattern_R = R * (1.0f - pattern * 0.2f);
        float pattern_r = r * (1.0f + pattern * 0.1f);
        float pattern_d = d * (1.0f - pattern * 0.15f);
        float pattern_time = time * speed1 + pattern * 2.0f;
        
        int prev_x = 0, prev_y = 0;
        bool first_point = true;
        
        for (int i = 0; i < 500; i++) {
            float t = (float)i / 500.0f * 20 * M_PI * speed2 + pattern_time;
            
            // Spirograph equations
            float x = (pattern_R - pattern_r) * cosf(t) + pattern_d * cosf((pattern_R - pattern_r) / pattern_r * t);
            float y = (pattern_R - pattern_r) * sinf(t) - pattern_d * sinf((pattern_R - pattern_r) / pattern_r * t);
            
            int px = center_x + (int)x;
            int py = center_y + (int)(y * 0.5f);  // Aspect correction
            
            // Choose character based on pattern and audio
            char spiro_char;
            if (pattern == 0) {
                spiro_char = false ? '#' : '*';
            } else if (pattern == 1) {
                spiro_char = '+';
            } else {
                spiro_char = '.';
            }
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                set_pixel(buffer, zbuffer, width, height, px, py, spiro_char, pattern);
                
                // Connect to previous point for continuous lines
                if (!first_point && i % 2 == 0) {
                    draw_line(buffer, width, height, prev_x, prev_y, px, py, '-');
                }
            }
            
            prev_x = px;
            prev_y = py;
            first_point = false;
        }
    }
}

void scene_matrix_rain(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    memset(buffer, ' ', width * height);
    
    // Matrix rain effect
    static float columns[200];
    static bool initialized = false;
    
    if (!initialized || width > 200) {
        for (int i = 0; i < width && i < 200; i++) {
            columns[i] = rand() % height;
        }
        initialized = true;
    }
    
    char matrix_chars[] = "01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()";
    int char_count = strlen(matrix_chars);
    
    for (int x = 0; x < width && x < 200; x++) {
        float speed = 0.1f + (x % 3) * 0.05f;
        columns[x] += speed * 30.0f * (1.0f / 60.0f); // Assume 60 FPS
        
        if (columns[x] > height + 20) {
            columns[x] = -rand() % 10;
        }
        
        // Draw falling characters
        for (int trail = 0; trail < 15; trail++) {
            int y = (int)(columns[x] - trail);
            if (y >= 0 && y < height) {
                char c;
                if (trail == 0) {
                    // Bright white leader
                    c = matrix_chars[rand() % char_count];
                } else if (trail < 5) {
                    // Bright green
                    c = matrix_chars[(x + trail + (int)(time * 10)) % char_count];
                } else {
                    // Dim green trail
                    c = matrix_chars[(x + trail) % char_count];
                }
                buffer[y * width + x] = c;
            }
        }
    }
}

// ============= GEOMETRIC SCENES (10-19) =============

// Scene 10: Tunnels
void scene_tunnels(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params; (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = 2.0f + (false ? 0.5f * 3.0f : 0.0f);
    float tunnel_z = fmodf(time * speed, 10.0f);
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    for (int ring = 1; ring < 20; ring++) {
        float z = ring - tunnel_z;
        if (z <= 0) z += 20.0f;
        
        float radius = (width * 0.4f) / z;
        if (radius < 1.0f) continue;
        
        int segments = (int)(radius * 8);
        if (segments < 8) segments = 8;
        
        for (int seg = 0; seg < segments; seg++) {
            float angle = (seg * 2.0f * M_PI) / segments;
            int x = center_x + (int)(cosf(angle) * radius);
            int y = center_y + (int)(sinf(angle) * radius * 0.6f);
            
            char tunnel_char = z < 5.0f ? '#' : z < 10.0f ? '*' : '+';
            set_pixel(buffer, zbuffer, width, height, x, y, tunnel_char, z);
        }
    }
}

// Scene 11: Kaleidoscope
void scene_kaleidoscope(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    int center_x = width / 2;
    int center_y = height / 2;
    float rotation = time * 1.0f;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = (y - center_y) * 2.0f; // Aspect correction
            float dist = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx) + rotation;
            
            // Create kaleidoscope pattern
            float seg_angle = fmodf(angle + M_PI, M_PI / 3.0f);
            if (seg_angle > M_PI / 6.0f) seg_angle = M_PI / 3.0f - seg_angle;
            
            float pattern = sinf(dist * 0.1f + time * 2.0f) * cosf(seg_angle * 6.0f);
            if (pattern > 0.3f) {
                char kal_char = pattern > 0.7f ? '#' : pattern > 0.5f ? '*' : '+';
                buffer[y * width + x] = kal_char;
            }
        }
    }
}

// Scene 12: Mandala
void scene_mandala(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    int center_x = width / 2;
    int center_y = height / 2;
    float rotation = time * 0.5f;
    
    for (int ring = 1; ring < 8; ring++) {
        float radius = ring * fminf(width, height * 2) * 0.06f;
        int petals = ring * 6;
        
        for (int petal = 0; petal < petals; petal++) {
            float angle = (petal * 2.0f * M_PI / petals) + rotation;
            float petal_radius = radius * (1.0f + sinf(angle * ring + time * 3.0f) * 0.2f);
            
            if (false) {
                petal_radius *= (1.0f + 0.5f * 0.3f);
            }
            
            int x = center_x + (int)(cosf(angle) * petal_radius);
            int y = center_y + (int)(sinf(angle) * petal_radius * 0.6f);
            
            char mandala_char = ring % 3 == 0 ? '#' : ring % 3 == 1 ? '*' : '+';
            set_pixel(buffer, zbuffer, width, height, x, y, mandala_char, ring);
        }
    }
}

// Scene 13: Sierpinski Triangle
void scene_sierpinski(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params; (void)zbuffer; (void)audio;
    memset(buffer, ' ', width * height);
    
    // Sierpinski triangle fractal
    int scale = (int)(height * 0.8f);
    int offset_x = width / 2 - scale / 2;
    int offset_y = height / 10;
    
    float anim_phase = sinf(time * 0.5f) * 0.5f + 0.5f;
    int max_iterations = (int)(anim_phase * 8) + 1;
    
    for (int iter = 0; iter < max_iterations; iter++) {
        int size = scale >> iter;
        if (size < 2) break;
        
        int rows = 1 << iter;
        for (int row = 0; row < rows; row++) {
            int triangles = row + 1;
            for (int tri = 0; tri < triangles; tri++) {
                int base_x = offset_x + tri * size + (rows - row - 1) * size / 2;
                int base_y = offset_y + row * size * 3 / 4;
                
                // Draw triangle outline
                char sierp_char = iter % 3 == 0 ? '#' : iter % 3 == 1 ? '*' : '.';
                
                // Top vertex
                if (base_y >= 0 && base_y < height && base_x + size/2 >= 0 && base_x + size/2 < width) {
                    buffer[base_y * width + base_x + size/2] = sierp_char;
                }
                
                // Bottom vertices
                if (base_y + size/2 >= 0 && base_y + size/2 < height) {
                    if (base_x >= 0 && base_x < width) buffer[(base_y + size/2) * width + base_x] = sierp_char;
                    if (base_x + size >= 0 && base_x + size < width) buffer[(base_y + size/2) * width + base_x + size] = sierp_char;
                }
            }
        }
    }
}

// Scene 14: Hexagon Grid
void scene_hexagon_grid(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float hex_size = 8.0f + (false ? 0.3f * 4.0f : 0.0f);
    float anim_offset = time * 2.0f;
    
    for (int row = 0; row < height / (int)(hex_size * 1.5f) + 2; row++) {
        for (int col = 0; col < width / (int)(hex_size * 2) + 2; col++) {
            float x_offset = (row % 2) * hex_size;
            float center_x = col * hex_size * 2 + x_offset;
            float center_y = row * hex_size * 1.5f;
            
            // Hexagon vertices
            for (int vertex = 0; vertex < 6; vertex++) {
                float angle = vertex * M_PI / 3.0f + anim_offset;
                float next_angle = (vertex + 1) * M_PI / 3.0f + anim_offset;
                
                int x1 = (int)(center_x + cosf(angle) * hex_size);
                int y1 = (int)(center_y + sinf(angle) * hex_size * 0.6f);
                int x2 = (int)(center_x + cosf(next_angle) * hex_size);
                int y2 = (int)(center_y + sinf(next_angle) * hex_size * 0.6f);
                
                char hex_char = (row + col + vertex) % 3 == 0 ? '#' : '*';
                draw_line(buffer, width, height, x1, y1, x2, y2, hex_char);
            }
        }
    }
}

// Scene 15: Tessellations
void scene_tessellations(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    int tile_size = 12 + (false ? (int)(0.5f * 8) : 0);
    float rotation = time * 0.3f;
    
    for (int y = 0; y < height; y += tile_size) {
        for (int x = 0; x < width; x += tile_size) {
            int pattern = ((x / tile_size) + (y / tile_size)) % 4;
            
            switch (pattern) {
                case 0: // Square
                    for (int dy = 0; dy < tile_size && y + dy < height; dy++) {
                        for (int dx = 0; dx < tile_size && x + dx < width; dx++) {
                            if (dx == 0 || dx == tile_size - 1 || dy == 0 || dy == tile_size - 1) {
                                buffer[(y + dy) * width + (x + dx)] = '#';
                            }
                        }
                    }
                    break;
                case 1: // Diamond
                    for (int i = 0; i < tile_size; i++) {
                        int top_y = y + i;
                        int bot_y = y + tile_size - 1 - i;
                        int left_x = x + tile_size / 2 - i;
                        int right_x = x + tile_size / 2 + i;
                        
                        if (top_y >= 0 && top_y < height) {
                            if (left_x >= 0 && left_x < width) buffer[top_y * width + left_x] = '*';
                            if (right_x >= 0 && right_x < width) buffer[top_y * width + right_x] = '*';
                        }
                    }
                    break;
                case 2: // Cross
                    for (int dy = 0; dy < tile_size && y + dy < height; dy++) {
                        if (x + tile_size / 2 >= 0 && x + tile_size / 2 < width) {
                            buffer[(y + dy) * width + (x + tile_size / 2)] = '+';
                        }
                    }
                    for (int dx = 0; dx < tile_size && x + dx < width; dx++) {
                        if (y + tile_size / 2 >= 0 && y + tile_size / 2 < height) {
                            buffer[(y + tile_size / 2) * width + (x + dx)] = '+';
                        }
                    }
                    break;
                case 3: // Circle
                    for (int dy = 0; dy < tile_size && y + dy < height; dy++) {
                        for (int dx = 0; dx < tile_size && x + dx < width; dx++) {
                            float dist = sqrtf((dx - tile_size/2) * (dx - tile_size/2) + 
                                             (dy - tile_size/2) * (dy - tile_size/2));
                            if (dist >= tile_size/3 && dist <= tile_size/2) {
                                buffer[(y + dy) * width + (x + dx)] = 'o';
                            }
                        }
                    }
                    break;
            }
        }
    }
}

// Scene 16: Voronoi Cells
void scene_voronoi_cells(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params; (void)zbuffer;
    clear_buffer(buffer, zbuffer, width, height);
    
    // Generate Voronoi diagram with moving seed points
    int num_seeds = 15 + (false ? (int)(0.5f * 10) : 0);
    if (num_seeds > 25) num_seeds = 25;
    
    // Calculate seed positions that move over time
    float seeds_x[25], seeds_y[25];
    for (int i = 0; i < num_seeds; i++) {
        float angle = (i * 2.0f * M_PI) / num_seeds + time * 0.3f;
        float radius = (height / 4.0f) * (1.0f + sinf(time * 0.5f + i) * 0.5f);
        seeds_x[i] = width / 2.0f + cosf(angle) * radius;
        seeds_y[i] = height / 2.0f + sinf(angle) * radius;
    }
    
    // Draw Voronoi cells
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float min_dist = 99999.0f;
            float second_min_dist = 99999.0f;
            int closest_seed = 0;
            
            // Find closest and second closest seed points
            for (int i = 0; i < num_seeds; i++) {
                float dx = x - seeds_x[i];
                float dy = y - seeds_y[i];
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist < min_dist) {
                    second_min_dist = min_dist;
                    min_dist = dist;
                    closest_seed = i;
                } else if (dist < second_min_dist) {
                    second_min_dist = dist;
                }
            }
            
            // Draw cell boundaries and patterns
            float edge_distance = second_min_dist - min_dist;
            if (edge_distance < 2.0f) {
                // Cell boundary
                buffer[y * width + x] = '+';
            } else if (min_dist < 5.0f) {
                // Seed point
                buffer[y * width + x] = '@';
            } else {
                // Fill cells with different patterns based on seed index
                char patterns[] = ".:-=*#";
                int pattern_idx = closest_seed % 6;
                if ((x + y) % 3 == 0) {
                    buffer[y * width + x] = patterns[pattern_idx];
                }
            }
        }
    }
}

// Scene 17: Sacred Geometry
void scene_sacred_geometry(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params; (void)zbuffer;
    clear_buffer(buffer, zbuffer, width, height);
    
    float scale = 1.0f + (false ? 0.5f * 0.5f : 0.0f);
    float rotation = time * 0.2f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Draw Flower of Life pattern
    float radius = (height / 4.0f) * scale;
    int num_circles = 7;
    
    // Central circle
    for (float angle = 0; angle < 2 * M_PI; angle += 0.1f) {
        int x = center_x + (int)(cosf(angle) * radius);
        int y = center_y + (int)(sinf(angle) * radius * 0.5f); // Adjust for aspect ratio
        if (x >= 0 && x < width && y >= 0 && y < height) {
            buffer[y * width + x] = 'O';
        }
    }
    
    // Surrounding circles in hexagonal pattern
    for (int i = 0; i < 6; i++) {
        float hex_angle = (i * M_PI / 3.0f) + rotation;
        int cx = center_x + (int)(cosf(hex_angle) * radius);
        int cy = center_y + (int)(sinf(hex_angle) * radius * 0.5f);
        
        // Draw each circle
        for (float angle = 0; angle < 2 * M_PI; angle += 0.1f) {
            int x = cx + (int)(cosf(angle) * radius);
            int y = cy + (int)(sinf(angle) * radius * 0.5f);
            if (x >= 0 && x < width && y >= 0 && y < height) {
                if (buffer[y * width + x] == ' ') {
                    buffer[y * width + x] = 'o';
                } else if (buffer[y * width + x] == 'o' || buffer[y * width + x] == 'O') {
                    buffer[y * width + x] = '*'; // Intersection points
                }
            }
        }
    }
    
    // Add Metatron's Cube lines
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            float angle1 = (i * M_PI / 3.0f) + rotation;
            float angle2 = (j * M_PI / 3.0f) + rotation;
            
            int x1 = center_x + (int)(cosf(angle1) * radius);
            int y1 = center_y + (int)(sinf(angle1) * radius * 0.5f);
            int x2 = center_x + (int)(cosf(angle2) * radius);
            int y2 = center_y + (int)(sinf(angle2) * radius * 0.5f);
            
            // Draw connecting lines
            draw_line(buffer, width, height, x1, y1, x2, y2, '-');
        }
    }
    
    // Add golden ratio spirals
    float golden_ratio = 1.618033988749f;
    float spiral_angle = 0.0f;
    float spiral_radius = 2.0f;
    
    while (spiral_radius < radius * 1.5f) {
        int sx = center_x + (int)(cosf(spiral_angle + rotation) * spiral_radius);
        int sy = center_y + (int)(sinf(spiral_angle + rotation) * spiral_radius * 0.5f);
        
        if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
            if (buffer[sy * width + sx] == ' ') {
                buffer[sy * width + sx] = '.';
            }
        }
        
        spiral_angle += 0.2f;
        spiral_radius *= 1.01f;
    }
}

// Helper function for 3D line drawing with rotation and projection
void project_and_draw_line(char* buffer, float* zbuffer, int width, int height,
                          float x1, float y1, float z1, float x2, float y2, float z2,
                          float angle_x, float angle_y, float angle_z,
                          int center_x, int center_y, char c) {
    // Rotation matrices
    float cos_x = cosf(angle_x);
    float sin_x = sinf(angle_x);
    float cos_y = cosf(angle_y);
    float sin_y = sinf(angle_y);
    float cos_z = cosf(angle_z);
    float sin_z = sinf(angle_z);
    
    // Rotate and project first point
    // Z rotation
    float rx1 = x1 * cos_z - y1 * sin_z;
    float ry1 = x1 * sin_z + y1 * cos_z;
    float rz1 = z1;
    
    // Y rotation
    float tx1 = rx1 * cos_y - rz1 * sin_y;
    rz1 = rx1 * sin_y + rz1 * cos_y;
    rx1 = tx1;
    
    // X rotation
    float ty1 = ry1 * cos_x - rz1 * sin_x;
    rz1 = ry1 * sin_x + rz1 * cos_x;
    ry1 = ty1;
    
    // Rotate and project second point
    // Z rotation
    float rx2 = x2 * cos_z - y2 * sin_z;
    float ry2 = x2 * sin_z + y2 * cos_z;
    float rz2 = z2;
    
    // Y rotation
    float tx2 = rx2 * cos_y - rz2 * sin_y;
    rz2 = rx2 * sin_y + rz2 * cos_y;
    rx2 = tx2;
    
    // X rotation
    float ty2 = ry2 * cos_x - rz2 * sin_x;
    rz2 = ry2 * sin_x + rz2 * cos_x;
    ry2 = ty2;
    
    // Perspective projection
    float camera_distance = 40.0f;
    rz1 += camera_distance;
    rz2 += camera_distance;
    
    if (rz1 > 0.1f && rz2 > 0.1f) {
        int px1 = center_x + (int)(rx1 * 30.0f / rz1);
        int py1 = center_y + (int)(ry1 * 20.0f / rz1);
        int px2 = center_x + (int)(rx2 * 30.0f / rz2);
        int py2 = center_y + (int)(ry2 * 20.0f / rz2);
        
        draw_line(buffer, width, height, px1, py1, px2, py2, c);
    }
}

// Scene 18: Polyhedra
void scene_polyhedra(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float rotation_speed = 1.0f + (false ? 0.5f * 2.0f : 0.0f);
    float scale = 15.0f;
    
    // Platonic solids cycling
    int solid_type = ((int)(time * 0.3f)) % 5;
    
    // Rotation angles
    float angle_x = time * rotation_speed * 0.7f;
    float angle_y = time * rotation_speed;
    float angle_z = time * rotation_speed * 0.3f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    switch (solid_type) {
        case 0: // Tetrahedron
        {
            float vertices[][3] = {
                {1, 1, 1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}
            };
            int edges[][2] = {
                {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}
            };
            
            // Draw edges
            for (int i = 0; i < 6; i++) {
                float x1 = vertices[edges[i][0]][0] * scale;
                float y1 = vertices[edges[i][0]][1] * scale;
                float z1 = vertices[edges[i][0]][2] * scale;
                float x2 = vertices[edges[i][1]][0] * scale;
                float y2 = vertices[edges[i][1]][1] * scale;
                float z2 = vertices[edges[i][1]][2] * scale;
                
                // Rotate and project
                project_and_draw_line(buffer, zbuffer, width, height,
                    x1, y1, z1, x2, y2, z2,
                    angle_x, angle_y, angle_z,
                    center_x, center_y, '=');
            }
            break;
        }
        
        case 1: // Octahedron
        {
            float vertices[][3] = {
                {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
            };
            int edges[][2] = {
                {0, 2}, {0, 3}, {0, 4}, {0, 5},
                {1, 2}, {1, 3}, {1, 4}, {1, 5},
                {2, 4}, {2, 5}, {3, 4}, {3, 5}
            };
            
            for (int i = 0; i < 12; i++) {
                float x1 = vertices[edges[i][0]][0] * scale;
                float y1 = vertices[edges[i][0]][1] * scale;
                float z1 = vertices[edges[i][0]][2] * scale;
                float x2 = vertices[edges[i][1]][0] * scale;
                float y2 = vertices[edges[i][1]][1] * scale;
                float z2 = vertices[edges[i][1]][2] * scale;
                
                project_and_draw_line(buffer, zbuffer, width, height,
                    x1, y1, z1, x2, y2, z2,
                    angle_x, angle_y, angle_z,
                    center_x, center_y, '#');
            }
            break;
        }
        
        case 2: // Icosahedron (simplified)
        {
            // Golden ratio
            float phi = 1.618033988749f;
            float a = 1.0f;
            float b = 1.0f / phi;
            
            // Draw simplified icosahedron with main edges
            int num_vertices = 12;
            for (int i = 0; i < num_vertices; i++) {
                float angle = i * 2.0f * M_PI / num_vertices;
                float x = cosf(angle) * scale;
                float y = sinf(angle) * scale;
                float z = (i % 2 == 0) ? scale * 0.5f : -scale * 0.5f;
                
                // Connect to nearby vertices
                for (int j = 0; j < 3; j++) {
                    int next = (i + j + 1) % num_vertices;
                    float angle2 = next * 2.0f * M_PI / num_vertices;
                    float x2 = cosf(angle2) * scale;
                    float y2 = sinf(angle2) * scale;
                    float z2 = (next % 2 == 0) ? scale * 0.5f : -scale * 0.5f;
                    
                    project_and_draw_line(buffer, zbuffer, width, height,
                        x, y, z, x2, y2, z2,
                        angle_x, angle_y, angle_z,
                        center_x, center_y, '*');
                }
            }
            break;
        }
        
        case 3: // Dodecahedron (simplified using pentagon)
        {
            // Draw pentagonal faces
            for (int face = 0; face < 3; face++) {
                float face_angle = face * 2.0f * M_PI / 3.0f;
                float fx = cosf(face_angle) * scale * 0.7f;
                float fy = sinf(face_angle) * scale * 0.7f;
                float fz = (face - 1) * scale * 0.5f;
                
                // Draw pentagon
                for (int i = 0; i < 5; i++) {
                    float angle1 = i * 2.0f * M_PI / 5.0f;
                    float angle2 = ((i + 1) % 5) * 2.0f * M_PI / 5.0f;
                    
                    float x1 = fx + cosf(angle1) * scale * 0.4f;
                    float y1 = fy + sinf(angle1) * scale * 0.4f;
                    float z1 = fz;
                    float x2 = fx + cosf(angle2) * scale * 0.4f;
                    float y2 = fy + sinf(angle2) * scale * 0.4f;
                    float z2 = fz;
                    
                    project_and_draw_line(buffer, zbuffer, width, height,
                        x1, y1, z1, x2, y2, z2,
                        angle_x, angle_y, angle_z,
                        center_x, center_y, '+');
                }
            }
            break;
        }
        
        case 4: // Cube (already exists but adding here for completeness)
        {
            float size = scale;
            float vertices[][3] = {
                {-size, -size, -size}, {size, -size, -size},
                {size, size, -size}, {-size, size, -size},
                {-size, -size, size}, {size, -size, size},
                {size, size, size}, {-size, size, size}
            };
            
            int edges[][2] = {
                {0, 1}, {1, 2}, {2, 3}, {3, 0},
                {4, 5}, {5, 6}, {6, 7}, {7, 4},
                {0, 4}, {1, 5}, {2, 6}, {3, 7}
            };
            
            for (int i = 0; i < 12; i++) {
                project_and_draw_line(buffer, zbuffer, width, height,
                    vertices[edges[i][0]][0], vertices[edges[i][0]][1], vertices[edges[i][0]][2],
                    vertices[edges[i][1]][0], vertices[edges[i][1]][1], vertices[edges[i][1]][2],
                    angle_x, angle_y, angle_z,
                    center_x, center_y, '@');
            }
            break;
        }
    }
}

// Scene 19: Maze Generator
void scene_maze_generator(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params; (void)zbuffer;
    clear_buffer(buffer, zbuffer, width, height);
    
    // Generate maze using recursive backtracker algorithm
    int maze_width = (width - 4) / 4;  // Each cell is 4 chars wide
    int maze_height = (height - 2) / 2; // Each cell is 2 chars tall
    
    if (maze_width < 5 || maze_height < 5) {
        // Fallback for small screens
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                    buffer[y * width + x] = '#';
                }
            }
        }
        return;
    }
    
    // Use time to generate different mazes
    int seed = (int)(time * 0.2f) % 100;
    
    // Initialize maze grid (walls everywhere)
    static bool h_walls[50][50]; // Horizontal walls
    static bool v_walls[50][50]; // Vertical walls
    static bool visited[50][50];
    static int maze_seed = -1;
    
    if (maze_seed != seed) {
        maze_seed = seed;
        
        // Reset maze
        for (int y = 0; y < 50; y++) {
            for (int x = 0; x < 50; x++) {
                h_walls[y][x] = true;
                v_walls[y][x] = true;
                visited[y][x] = false;
            }
        }
        
        // Generate maze using simplified algorithm
        // Start from center
        int cx = maze_width / 2;
        int cy = maze_height / 2;
        visited[cy][cx] = true;
        
        // Create paths
        for (int i = 0; i < maze_width * maze_height / 2; i++) {
            int x = (seed * 73 + i * 31) % maze_width;
            int y = (seed * 97 + i * 43) % maze_height;
            
            if (x > 0 && x < maze_width - 1 && y > 0 && y < maze_height - 1) {
                visited[y][x] = true;
                
                // Remove random walls
                int dir = (seed + i) % 4;
                switch (dir) {
                    case 0: if (y > 0) h_walls[y][x] = false; break;
                    case 1: if (x < maze_width - 1) v_walls[y][x] = false; break;
                    case 2: if (y < maze_height - 1) h_walls[y + 1][x] = false; break;
                    case 3: if (x > 0) v_walls[y][x - 1] = false; break;
                }
            }
        }
    }
    
    // Draw maze
    for (int my = 0; my < maze_height && my < 50; my++) {
        for (int mx = 0; mx < maze_width && mx < 50; mx++) {
            int x_base = mx * 4 + 2;
            int y_base = my * 2 + 1;
            
            // Draw cell
            if (x_base < width - 2 && y_base < height - 1) {
                // Top wall
                if (h_walls[my][mx]) {
                    for (int i = 0; i < 3 && x_base + i < width; i++) {
                        buffer[y_base * width + x_base + i] = '=';
                    }
                }
                
                // Left wall
                if (v_walls[my][mx]) {
                    buffer[y_base * width + x_base - 1] = '|';
                    if (y_base + 1 < height) {
                        buffer[(y_base + 1) * width + x_base - 1] = '|';
                    }
                }
                
                // Corners
                buffer[y_base * width + x_base - 1] = '+';
            }
        }
    }
    
    // Add animated solution path
    float solution_progress = (sinf(time * (false ? 1.0f + 0.5f : 1.0f)) + 1.0f) * 0.5f;
    int path_length = (int)(solution_progress * maze_width);
    
    int px = 2, py = 1;
    for (int step = 0; step < path_length && px < width - 3 && py < height - 2; step++) {
        buffer[py * width + px] = '*';
        
        // Move through maze (simplified pathfinding)
        int dir = (step + seed) % 4;
        switch (dir) {
            case 0: if (py < height - 3) py += 2; break;
            case 1: if (px < width - 5) px += 4; break;
            case 2: if (py > 3) py -= 2; break;
            case 3: if (px > 5) px -= 4; break;
        }
    }
}

// ============= ORGANIC SCENES (20-29) =============

void scene_fire_simulation(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    memset(buffer, ' ', width * height);
    
    static float fire_map[200][100];
    static bool fire_initialized = false;
    
    int w = width > 200 ? 200 : width;
    int h = height > 100 ? 100 : height;
    
    if (!fire_initialized) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                fire_map[x][y] = 0.0f;
            }
        }
        fire_initialized = true;
    }
    
    // Add fire sources at bottom
    for (int x = 0; x < w; x++) {
        fire_map[x][h-1] = 1.0f + sinf(time * 3.0f + x * 0.1f) * 0.3f;
    }
    
    // Fire simulation
    for (int y = h - 2; y >= 0; y--) {
        for (int x = 1; x < w - 1; x++) {
            float sum = fire_map[x-1][y+1] + fire_map[x][y+1] + fire_map[x+1][y+1];
            fire_map[x][y] = (sum / 3.0f) * 0.96f; // Cooling
        }
    }
    
    // Render fire
    char fire_chars[] = " .'\":;*%#@";
    for (int y = 0; y < h && y < height; y++) {
        for (int x = 0; x < w && x < width; x++) {
            float intensity = fire_map[x][y];
            if (intensity > 0.1f) {
                int char_idx = (int)(intensity * 8);
                if (char_idx > 8) char_idx = 8;
                buffer[y * width + x] = fire_chars[char_idx];
            }
        }
    }
}

void scene_water_waves(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    memset(buffer, ' ', width * height);
    
    char wave_chars[] = "~-=*#@+oO";
    int char_count = 9;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float wave1 = sinf((x * 0.1f) + (time * 2.0f)) * 0.5f;
            float wave2 = sinf((x * 0.05f) + (time * 1.5f) + (y * 0.1f)) * 0.3f;
            float wave3 = sinf((x * 0.2f) + (time * 3.0f)) * 0.2f;
            
            float combined = wave1 + wave2 + wave3;
            int wave_y = y + (int)(combined * 3.0f);
            
            if (wave_y >= 0 && wave_y < height && wave_y == y) {
                float intensity = (combined + 1.0f) * 0.5f;
                int char_idx = (int)(intensity * char_count);
                if (char_idx >= char_count) char_idx = char_count - 1;
                buffer[y * width + x] = wave_chars[char_idx];
            }
        }
    }
}

void scene_lightning(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    memset(buffer, ' ', width * height);
    
    static float next_bolt = 0.0f;
    static int bolt_x, bolt_y;
    static float bolt_life = 0.0f;
    
    if (time > next_bolt) {
        bolt_x = rand() % width;
        bolt_y = 0;
        bolt_life = 0.3f;
        next_bolt = time + 1.0f + (rand() % 3);
    }
    
    if (bolt_life > 0.0f) {
        bolt_life -= 1.0f/60.0f;
        
        // Draw main bolt
        int current_x = bolt_x;
        for (int y = 0; y < height; y++) {
            if (current_x >= 0 && current_x < width) {
                buffer[y * width + current_x] = '|';
            }
            
            // Random zigzag
            if (rand() % 4 == 0) {
                current_x += (rand() % 3) - 1;
            }
        }
        
        // Add branches
        for (int branch = 0; branch < 3; branch++) {
            int branch_x = bolt_x + (rand() % 20) - 10;
            int branch_start = rand() % (height/2);
            for (int y = branch_start; y < branch_start + 10 && y < height; y++) {
                if (branch_x >= 0 && branch_x < width) {
                    buffer[y * width + branch_x] = rand() % 2 ? '/' : '\\';
                }
                branch_x += (rand() % 3) - 1;
            }
        }
    }
}

void scene_plasma_clouds(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    memset(buffer, ' ', width * height);
    
    char plasma_chars[] = " .:;+=xX#%@";
    int char_count = 11;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float plasma = 0.0f;
            
            // Multiple plasma layers
            plasma += sinf((x + time * 50.0f) * 0.02f) * 0.5f;
            plasma += sinf((y + time * 30.0f) * 0.03f) * 0.5f;
            plasma += sinf((x + y + time * 40.0f) * 0.01f) * 0.5f;
            plasma += sinf(sqrtf((x-width/2)*(x-width/2) + (y-height/2)*(y-height/2)) * 0.05f + time * 20.0f) * 0.5f;
            
            float intensity = (plasma + 2.0f) * 0.25f;
            if (intensity > 0.2f) {
                int char_idx = (int)(intensity * char_count);
                if (char_idx >= char_count) char_idx = char_count - 1;
                buffer[y * width + x] = plasma_chars[char_idx];
            }
        }
    }
}

void scene_galaxy_spiral(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    memset(buffer, ' ', width * height);
    
    int center_x = width / 2;
    int center_y = height / 2;
    char star_chars[] = ".+*oO@";
    
    for (int i = 0; i < 800; i++) {
        float angle = (i * 0.1f) + (time * 0.5f);
        float radius = i * 0.05f;
        
        // Spiral arms
        float spiral_angle = angle + (radius * 0.3f);
        int x = center_x + (int)(cosf(spiral_angle) * radius);
        int y = center_y + (int)(sinf(spiral_angle) * radius * 0.6f);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            float brightness = 1.0f - (radius / 30.0f);
            if (brightness > 0.0f) {
                int char_idx = (int)(brightness * 5);
                if (char_idx > 5) char_idx = 5;
                buffer[y * width + x] = star_chars[char_idx];
            }
        }
        
        // Second spiral arm
        spiral_angle = angle + (radius * 0.3f) + M_PI;
        x = center_x + (int)(cosf(spiral_angle) * radius);
        y = center_y + (int)(sinf(spiral_angle) * radius * 0.6f);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            float brightness = 1.0f - (radius / 30.0f);
            if (brightness > 0.0f) {
                int char_idx = (int)(brightness * 5);
                if (char_idx > 5) char_idx = 5;
                buffer[y * width + x] = star_chars[char_idx];
            }
        }
    }
}

// Scene 25: Tree of Life
void scene_tree_of_life(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    int center_x = width / 2;
    int center_y = height - 5;
    float growth = sinf(time * 0.5f) * 0.3f + 0.7f;
    
    if (false) {
        growth *= (1.0f + 0.5f * 0.5f);
    }
    
    // Draw trunk
    int trunk_height = (int)(height * 0.3f * growth);
    for (int y = 0; y < trunk_height; y++) {
        int trunk_width = 3 + y / 10;
        for (int x = -trunk_width/2; x <= trunk_width/2; x++) {
            if (center_x + x >= 0 && center_x + x < width && center_y - y >= 0) {
                buffer[(center_y - y) * width + center_x + x] = '#';
            }
        }
    }
    
    // Draw branches in organic pattern
    for (int branch = 0; branch < 8; branch++) {
        float angle = (branch * M_PI / 4.0f) + sinf(time + branch) * 0.3f;
        float branch_length = trunk_height * 0.6f * growth;
        
        int start_y = center_y - trunk_height + branch * trunk_height / 8;
        
        for (int step = 0; step < (int)branch_length; step++) {
            float curve = sinf(step * 0.1f + time + branch) * 0.2f;
            int x = center_x + (int)(cosf(angle + curve) * step);
            int y = start_y + (int)(sinf(angle + curve) * step * 0.5f);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char branch_char = step < branch_length * 0.3f ? '*' : 
                                  step < branch_length * 0.6f ? '+' : '.';
                buffer[y * width + x] = branch_char;
            }
        }
    }
}

// Scene 26: Cellular Automata
void scene_cellular_automata(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    static char grid[200][100];
    static bool initialized = false;
    static float last_update = 0.0f;
    
    int w = width > 200 ? 200 : width;
    int h = height > 100 ? 100 : height;
    
    if (!initialized) {
        // Initialize with random pattern
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                grid[x][y] = (rand() % 100) < 30 ? 1 : 0;
            }
        }
        initialized = true;
        last_update = time;
    }
    
    // Update automata every 0.2 seconds
    if (time - last_update > 0.2f) {
        char new_grid[200][100];
        
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int neighbors = 0;
                
                // Count neighbors
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            neighbors += grid[nx][ny];
                        }
                    }
                }
                
                // Conway's Game of Life rules
                if (grid[x][y] == 1) {
                    new_grid[x][y] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
                } else {
                    new_grid[x][y] = (neighbors == 3) ? 1 : 0;
                }
            }
        }
        
        memcpy(grid, new_grid, sizeof(grid));
        last_update = time;
    }
    
    // Render automata
    memset(buffer, ' ', width * height);
    for (int y = 0; y < h && y < height; y++) {
        for (int x = 0; x < w && x < width; x++) {
            if (grid[x][y]) {
                char cell_char = false ? '#' : '*';
                buffer[y * width + x] = cell_char;
            }
        }
    }
}

// Scene 27: Flocking Birds
void scene_flocking_birds(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params; (void)zbuffer;
    clear_buffer(buffer, zbuffer, width, height);
    
    static float birds[50][4]; // x, y, vx, vy
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < 50; i++) {
            birds[i][0] = rand() % width;   // x
            birds[i][1] = rand() % height;  // y
            birds[i][2] = (rand() % 200 - 100) / 100.0f; // vx
            birds[i][3] = (rand() % 200 - 100) / 100.0f; // vy
        }
        initialized = true;
    }
    
    float flock_speed = 1.0f + (false ? 0.5f : 0.0f);
    
    // Update bird positions with simple flocking behavior
    for (int i = 0; i < 50; i++) {
        float sep_x = 0, sep_y = 0;
        float align_x = 0, align_y = 0;
        float coh_x = 0, coh_y = 0;
        int neighbors = 0;
        
        // Check neighbors
        for (int j = 0; j < 50; j++) {
            if (i == j) continue;
            
            float dx = birds[j][0] - birds[i][0];
            float dy = birds[j][1] - birds[i][1];
            float dist = sqrtf(dx * dx + dy * dy);
            
            if (dist < 20.0f && dist > 0) {
                // Separation
                sep_x -= dx / dist;
                sep_y -= dy / dist;
                
                // Alignment
                align_x += birds[j][2];
                align_y += birds[j][3];
                
                // Cohesion
                coh_x += dx;
                coh_y += dy;
                
                neighbors++;
            }
        }
        
        if (neighbors > 0) {
            align_x /= neighbors;
            align_y /= neighbors;
            coh_x /= neighbors;
            coh_y /= neighbors;
            
            // Apply flocking forces
            birds[i][2] += (sep_x * 0.1f + align_x * 0.05f + coh_x * 0.01f) * flock_speed;
            birds[i][3] += (sep_y * 0.1f + align_y * 0.05f + coh_y * 0.01f) * flock_speed;
        }
        
        // Limit velocity
        float vel = sqrtf(birds[i][2] * birds[i][2] + birds[i][3] * birds[i][3]);
        if (vel > 2.0f) {
            birds[i][2] = (birds[i][2] / vel) * 2.0f;
            birds[i][3] = (birds[i][3] / vel) * 2.0f;
        }
        
        // Update position
        birds[i][0] += birds[i][2];
        birds[i][1] += birds[i][3];
        
        // Wrap around screen
        if (birds[i][0] < 0) birds[i][0] = width;
        if (birds[i][0] >= width) birds[i][0] = 0;
        if (birds[i][1] < 0) birds[i][1] = height;
        if (birds[i][1] >= height) birds[i][1] = 0;
        
        // Draw bird
        int x = (int)birds[i][0];
        int y = (int)birds[i][1];
        char bird_char = i % 3 == 0 ? '>' : i % 3 == 1 ? '^' : 'v';
        set_pixel(buffer, zbuffer, width, height, x, y, bird_char, 1.0f);
    }
}

// Scene 28: Wind Patterns
void scene_wind_patterns(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params; (void)zbuffer;
    clear_buffer(buffer, zbuffer, width, height);
    
    float wind_strength = 1.0f + (false ? 0.5f * 2.0f : 0.0f);
    
    for (int y = 5; y < height - 5; y += 8) {
        for (int x = 10; x < width - 10; x += 12) {
            // Calculate wind vector based on position and time
            float wind_x = sinf(x * 0.05f + time * wind_strength) * cosf(y * 0.03f + time * 0.7f);
            float wind_y = cosf(x * 0.03f + time * 0.5f) * sinf(y * 0.05f + time * wind_strength);
            
            // Draw wind streamlines
            for (int step = 0; step < 15; step++) {
                float t = step / 15.0f;
                int stream_x = x + (int)(wind_x * step * 2);
                int stream_y = y + (int)(wind_y * step * 2);
                
                // Add some turbulence
                stream_x += (int)(sinf(time * 3.0f + step * 0.5f) * 2);
                stream_y += (int)(cosf(time * 2.0f + step * 0.3f) * 1);
                
                if (stream_x >= 0 && stream_x < width && stream_y >= 0 && stream_y < height) {
                    char wind_char = t < 0.3f ? '.' : t < 0.6f ? '-' : t < 0.8f ? '=' : '~';
                    buffer[stream_y * width + stream_x] = wind_char;
                }
            }
        }
    }
}

// Scene 29: Neural Networks
void scene_neural_networks(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    int layers = 4;
    int nodes_per_layer = 8;
    float layer_spacing = (float)width / (layers + 1);
    float node_spacing = (float)height / (nodes_per_layer + 1);
    
    float activation_speed = 1.0f + (false ? 0.5f * 3.0f : 0.0f);
    
    // Draw network structure
    for (int layer = 0; layer < layers; layer++) {
        float x = (layer + 1) * layer_spacing;
        
        for (int node = 0; node < nodes_per_layer; node++) {
            float y = (node + 1) * node_spacing;
            
            // Draw node with activation
            float activation = sinf(time * activation_speed + layer + node) * 0.5f + 0.5f;
            char node_char = activation > 0.7f ? '@' : activation > 0.4f ? 'O' : 'o';
            
            if ((int)x >= 0 && (int)x < width && (int)y >= 0 && (int)y < height) {
                buffer[(int)y * width + (int)x] = node_char;
            }
            
            // Draw connections to next layer
            if (layer < layers - 1) {
                float next_x = (layer + 2) * layer_spacing;
                
                for (int next_node = 0; next_node < nodes_per_layer; next_node++) {
                    float next_y = (next_node + 1) * node_spacing;
                    
                    // Draw connection line with varying intensity
                    float connection_strength = sinf(time * 2.0f + layer + node + next_node) * 0.5f + 0.5f;
                    if (connection_strength > 0.3f) {
                        char conn_char = connection_strength > 0.7f ? '=' : '-';
                        draw_line(buffer, width, height, (int)x, (int)y, (int)next_x, (int)next_y, conn_char);
                    }
                }
            }
        }
    }
}

// ============= TEXT/CODE SCENES (30-39) =============

// Scene 30: Matrix Rain (already exists as placeholder - keeping as is)

// Scene 31: ASCII Art Generator
void scene_ascii_art_generator(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    char ascii_chars[] = " .:-=+*#%@";
    int char_count = 10;
    float animation_speed = 1.0f + (false ? 0.5f * 2.0f : 0.0f);
    
    // Create animated ASCII portrait/pattern
    int center_x = width / 2;
    int center_y = height / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = y - center_y;
            float distance = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx);
            
            // Create face-like pattern
            float intensity = 0.0f;
            
            // Eyes
            float eye1_dx = dx - width/6;
            float eye1_dy = dy + height/8;
            float eye1_dist = sqrtf(eye1_dx*eye1_dx + eye1_dy*eye1_dy);
            if (eye1_dist < 3) intensity += 0.8f;
            
            float eye2_dx = dx + width/6;
            float eye2_dy = dy + height/8;
            float eye2_dist = sqrtf(eye2_dx*eye2_dx + eye2_dy*eye2_dy);
            if (eye2_dist < 3) intensity += 0.8f;
            
            // Nose
            if (abs((int)dx) < 2 && dy > height/8 && dy < height/4) {
                intensity += 0.6f;
            }
            
            // Mouth
            float mouth_curve = sinf((dx / (float)width) * M_PI * 4.0f + time * animation_speed);
            if (dy > height/3 && dy < height/3 + 2 && mouth_curve > 0.5f) {
                intensity += 0.7f;
            }
            
            // Face outline
            if (distance > width/4 && distance < width/3) {
                intensity += 0.4f * sinf(angle * 8.0f + time * animation_speed);
            }
            
            if (intensity > 0.1f) {
                int char_idx = (int)(intensity * char_count);
                if (char_idx >= char_count) char_idx = char_count - 1;
                buffer[y * width + x] = ascii_chars[char_idx];
            }
        }
    }
}

// Scene 32: Code Rain
void scene_code_rain(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    static char columns[200][100];
    static float speeds[200];
    static float positions[200];
    static bool initialized = false;
    
    int w = width > 200 ? 200 : width;
    int h = height > 100 ? 100 : height;
    
    if (!initialized) {
        for (int x = 0; x < w; x++) {
            speeds[x] = 0.5f + (rand() % 100) / 100.0f;
            positions[x] = -(rand() % h);
        }
        initialized = true;
    }
    
    memset(buffer, ' ', width * height);
    
    // Programming language characters
    char code_chars[] = "{}();[]+=*&|<>?:.,01";
    int char_count = 20;
    float rain_speed = 1.0f + (false ? 0.5f * 3.0f : 0.0f);
    
    for (int x = 0; x < w; x++) {
        positions[x] += speeds[x] * rain_speed;
        
        if (positions[x] > h + 10) {
            positions[x] = -(rand() % 20);
        }
        
        // Draw column of code
        for (int trail = 0; trail < 15; trail++) {
            int y = (int)positions[x] - trail;
            if (y >= 0 && y < h && y < height && x < width) {
                float fade = 1.0f - (trail / 15.0f);
                if (fade > 0.3f) {
                    int char_idx = (rand() + x + trail) % char_count;
                    buffer[y * width + x] = code_chars[char_idx];
                }
            }
        }
    }
}

// Scene 33: Binary Stream
void scene_binary_stream(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float flow_speed = 2.0f + (false ? 0.5f * 4.0f : 0.0f);
    
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x += 8) {
            // Create flowing binary pattern
            int offset = (int)(time * flow_speed * 10) % 8;
            
            for (int bit = 0; bit < 8 && x + bit < width; bit++) {
                // Generate pseudo-random but flowing binary
                int seed = (x + bit + offset + y * 137) ^ ((int)(time * 10) % 256);
                char binary_char = (seed % 2) ? '1' : '0';
                
                // Add some visual variety
                if ((seed % 16) == 0) binary_char = (seed % 2) ? '#' : '.';
                
                buffer[y * width + (x + bit)] = binary_char;
                
                // Secondary line with different pattern
                if (y + 1 < height) {
                    int seed2 = (seed * 31) % 256;
                    char binary_char2 = (seed2 % 2) ? '1' : '0';
                    buffer[(y + 1) * width + (x + bit)] = binary_char2;
                }
            }
        }
    }
}

// Scene 34: Terminal Glitch
void scene_terminal_glitch(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    
    // Base terminal text
    const char* terminal_lines[] = {
        "$ system status",
        "OK: Network connection established",
        "OK: Memory allocation successful", 
        "ERROR: Segmentation fault at 0x",
        "WARNING: Buffer overflow detected",
        "$ ./execute_program",
        "Loading modules...",
        "Processing data stream...",
        "[==========] 100%",
        "$ logout"
    };
    int line_count = 10;
    
    memset(buffer, ' ', width * height);
    
    // Draw terminal text
    for (int line = 0; line < line_count && line * 2 < height; line++) {
        const char* text = terminal_lines[line];
        int y = line * 2 + 2;
        
        for (int i = 0; text[i] && i < width - 2; i++) {
            if (y < height) {
                buffer[y * width + i + 2] = text[i];
            }
        }
    }
    
    // Add glitch effects
    float glitch_intensity = false ? 0.5f : 0.3f;
    static float next_glitch = 0.0f;
    
    if (time > next_glitch) {
        next_glitch = time + 0.05f + (rand() % 20) * 0.01f;
        
        // Horizontal line corruption
        for (int glitch = 0; glitch < (int)(glitch_intensity * 5) + 1; glitch++) {
            int y = rand() % height;
            int start_x = rand() % (width / 2);
            int end_x = start_x + 10 + rand() % 20;
            
            char corruption_chars[] = "#@$%^&*(){}[]<>?";
            for (int x = start_x; x < end_x && x < width; x++) {
                buffer[y * width + x] = corruption_chars[rand() % 16];
            }
        }
        
        // Character replacement glitches
        for (int i = 0; i < width * height; i++) {
            if (buffer[i] != ' ' && (rand() % 200) < glitch_intensity * 100) {
                buffer[i] = "!@#$%^&*"[rand() % 8];
            }
        }
    }
}

// Scene 35: Syntax Highlighting
void scene_syntax_highlighting(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    memset(buffer, ' ', width * height);
    
    // Simulate syntax highlighted code
    const char* code_lines[] = {
        "function animate(time) {",
        "  var x = Math.sin(time);",
        "  var y = Math.cos(time);",
        "  if (x > 0.5) {",
        "    drawPixel(x, y, '#FF0000');",
        "  } else {",
        "    drawPixel(x, y, '#00FF00');",
        "  }",
        "  return {x: x, y: y};",
        "}"
    };
    int line_count = 10;
    
    float typing_progress = fmodf(time * 0.5f, 1.0f);
    if (false) {
        typing_progress = fmodf(time * (0.5f + 0.5f), 1.0f);
    }
    
    int max_chars = (int)(typing_progress * width * line_count);
    int char_count = 0;
    
    for (int line = 0; line < line_count && line < height; line++) {
        const char* text = code_lines[line];
        int y = line;
        
        for (int i = 0; text[i] && i < width && char_count < max_chars; i++, char_count++) {
            char c = text[i];
            
            // Simulate syntax highlighting with different characters
            if (c >= 'a' && c <= 'z') {
                // Keywords in caps
                if (strstr("function var if else return", &text[i-5])) {
                    c = c - 'a' + 'A';
                }
            } else if (c >= '0' && c <= '9') {
                // Numbers highlighted
                c = '#';
            } else if (c == '"' || c == '\'') {
                // Strings
                c = '*';
            }
            
            buffer[y * width + i] = c;
        }
    }
}

// Scene 36: Data Visualization
void scene_data_visualization(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    // Create animated bar chart
    int num_bars = 12;
    int bar_width = width / num_bars;
    
    for (int bar = 0; bar < num_bars; bar++) {
        // Generate data value that changes over time
        float data_value = sinf(time * 0.5f + bar * 0.5f) * 0.4f + 0.6f;
        if (false) {
            data_value += 0.5f * 0.3f;
        }
        
        int bar_height = (int)(data_value * height * 0.8f);
        int bar_x = bar * bar_width;
        
        // Draw bar
        for (int y = height - bar_height; y < height; y++) {
            for (int x = bar_x + 1; x < bar_x + bar_width - 1 && x < width; x++) {
                char bar_char = y < height - bar_height/2 ? '#' : '=';
                buffer[y * width + x] = bar_char;
            }
        }
        
        // Draw value label at top
        char value_str[8];
        snprintf(value_str, sizeof(value_str), "%.1f", data_value * 100);
        int label_y = height - bar_height - 2;
        if (label_y >= 0) {
            for (int i = 0; value_str[i] && i < bar_width - 1; i++) {
                if (bar_x + 1 + i < width) {
                    buffer[label_y * width + bar_x + 1 + i] = value_str[i];
                }
            }
        }
        
        // Draw x-axis labels
        char label = 'A' + bar;
        if (height - 1 >= 0 && bar_x + bar_width/2 < width) {
            buffer[(height - 1) * width + bar_x + bar_width/2] = label;
        }
    }
    
    // Draw axes
    for (int x = 0; x < width; x++) {
        buffer[(height - 1) * width + x] = '-';
    }
    for (int y = 0; y < height; y++) {
        buffer[y * width + 0] = '|';
    }
}

// Scene 37: Network Nodes
void scene_network_nodes(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    int num_nodes = 20;
    static float nodes[20][2]; // x, y positions
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < num_nodes; i++) {
            nodes[i][0] = rand() % width;
            nodes[i][1] = rand() % height;
        }
        initialized = true;
    }
    
    float activity = false ? 0.5f : 0.5f;
    
    // Update node positions slightly
    for (int i = 0; i < num_nodes; i++) {
        nodes[i][0] += sinf(time * 0.5f + i) * 0.1f;
        nodes[i][1] += cosf(time * 0.3f + i) * 0.1f;
        
        // Keep nodes in bounds
        if (nodes[i][0] < 0) nodes[i][0] = width - 1;
        if (nodes[i][0] >= width) nodes[i][0] = 0;
        if (nodes[i][1] < 0) nodes[i][1] = height - 1;
        if (nodes[i][1] >= height) nodes[i][1] = 0;
    }
    
    // Draw connections between nearby nodes
    for (int i = 0; i < num_nodes; i++) {
        for (int j = i + 1; j < num_nodes; j++) {
            float dx = nodes[i][0] - nodes[j][0];
            float dy = nodes[i][1] - nodes[j][1];
            float dist = sqrtf(dx * dx + dy * dy);
            
            if (dist < 15.0f + activity * 10.0f) {
                // Draw connection line
                char conn_char = dist < 10.0f ? '=' : '-';
                draw_line(buffer, width, height, 
                         (int)nodes[i][0], (int)nodes[i][1],
                         (int)nodes[j][0], (int)nodes[j][1], conn_char);
            }
        }
    }
    
    // Draw nodes
    for (int i = 0; i < num_nodes; i++) {
        int x = (int)nodes[i][0];
        int y = (int)nodes[i][1];
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            // Node activity based on connections
            int connections = 0;
            for (int j = 0; j < num_nodes; j++) {
                if (i != j) {
                    float dx = nodes[i][0] - nodes[j][0];
                    float dy = nodes[i][1] - nodes[j][1];
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < 15.0f + activity * 10.0f) connections++;
                }
            }
            
            char node_char = connections > 3 ? '@' : connections > 1 ? 'O' : 'o';
            buffer[y * width + x] = node_char;
        }
    }
}

// Scene 38: System Monitor
void scene_system_monitor(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    memset(buffer, ' ', width * height);
    
    // System metrics that change over time
    float cpu_usage = (sinf(time * 1.2f) * 0.3f + 0.7f) * 100;
    float mem_usage = (cosf(time * 0.8f) * 0.2f + 0.6f) * 100;
    float disk_usage = 45.0f + sinf(time * 0.3f) * 10.0f;
    float network = (false ? 0.5f : sinf(time * 2.0f) * 0.5f + 0.5f) * 100;
    
    // Header
    const char* header = "SYSTEM MONITOR";
    int header_x = (width - strlen(header)) / 2;
    if (header_x > 0) {
        for (int i = 0; header[i]; i++) {
            if (header_x + i < width) {
                buffer[0 * width + header_x + i] = header[i];
            }
        }
    }
    
    // Draw metrics
    char metrics[4][64];
    snprintf(metrics[0], sizeof(metrics[0]), "CPU:  [%s%s] %.1f%%", 
             (int)(cpu_usage/10) < 5 ? "=====" : "==========",
             (int)(cpu_usage/10) < 5 ? "     " : "", cpu_usage);
    snprintf(metrics[1], sizeof(metrics[1]), "MEM:  [%s%s] %.1f%%",
             (int)(mem_usage/10) < 5 ? "=====" : "==========",
             (int)(mem_usage/10) < 5 ? "     " : "", mem_usage);
    snprintf(metrics[2], sizeof(metrics[2]), "DISK: [%s%s] %.1f%%",
             (int)(disk_usage/10) < 5 ? "=====" : "==========", 
             (int)(disk_usage/10) < 5 ? "     " : "", disk_usage);
    snprintf(metrics[3], sizeof(metrics[3]), "NET:  [%s%s] %.1f%%",
             (int)(network/10) < 5 ? "=====" : "==========",
             (int)(network/10) < 5 ? "     " : "", network);
    
    for (int metric = 0; metric < 4; metric++) {
        int y = 3 + metric * 2;
        if (y < height) {
            for (int i = 0; metrics[metric][i] && i < width - 2; i++) {
                buffer[y * width + 2 + i] = metrics[metric][i];
            }
        }
    }
    
    // Process list simulation
    if (height > 15) {
        const char* proc_header = "PID  CMD        CPU%  MEM%";
        int y = 12;
        for (int i = 0; proc_header[i] && i < width - 2; i++) {
            buffer[y * width + 2 + i] = proc_header[i];
        }
        
        // Fake processes
        const char* processes[] = {"firefox", "code", "bash", "clift", "music"};
        for (int proc = 0; proc < 5 && y + proc + 2 < height; proc++) {
            char proc_line[64];
            int pid = 1000 + proc * 100 + (int)(time * 10) % 100;
            float proc_cpu = (sinf(time + proc) * 20.0f + 30.0f);
            float proc_mem = 5.0f + proc * 3.0f + sinf(time * 0.5f + proc) * 2.0f;
            
            snprintf(proc_line, sizeof(proc_line), "%d  %-10s %.1f  %.1f", 
                     pid, processes[proc], proc_cpu, proc_mem);
            
            for (int i = 0; proc_line[i] && i < width - 2; i++) {
                buffer[(y + proc + 2) * width + 2 + i] = proc_line[i];
            }
        }
    }
}

// Scene 39: Command Line Interface
void scene_command_line(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    memset(buffer, ' ', width * height);
    
    // Terminal prompt and commands
    const char* commands[] = {
        "user@system:~$ ls -la",
        "drwxr-xr-x  4 user user  4096 Jan  1 12:00 .",
        "drwxr-xr-x  3 root root  4096 Jan  1 11:00 ..",
        "-rw-r--r--  1 user user   742 Jan  1 12:00 .bashrc",
        "drwxr-xr-x  2 user user  4096 Jan  1 12:00 Documents",
        "-rwxr-xr-x  1 user user  8192 Jan  1 12:00 program",
        "",
        "user@system:~$ ./program",
        "Initializing...",
        "Processing data...",
        "[████████████████████] 100%",
        "Done.",
        "",
        "user@system:~$ "
    };
    int cmd_count = 14;
    
    // Simulate typing effect
    float typing_speed = 1.0f + (false ? 0.5f * 2.0f : 0.0f);
    int visible_lines = (int)(fmodf(time * typing_speed * 0.5f, cmd_count + 2));
    
    for (int line = 0; line < visible_lines && line < cmd_count && line < height; line++) {
        const char* text = commands[line];
        
        // Cursor effect on current line being typed
        int chars_to_show = strlen(text);
        if (line == visible_lines - 1) {
            chars_to_show = (int)(fmodf(time * typing_speed * 20.0f, strlen(text) + 5));
        }
        
        for (int i = 0; i < chars_to_show && text[i] && i < width - 2; i++) {
            buffer[line * width + 1 + i] = text[i];
        }
        
        // Blinking cursor
        if (line == visible_lines - 1 && chars_to_show < strlen(text)) {
            int cursor_x = 1 + chars_to_show;
            if (cursor_x < width && (int)(time * 4.0f) % 2 == 0) {
                buffer[line * width + cursor_x] = '_';
            }
        }
    }
}

// ============= ABSTRACT SCENES (40-49) =============

void scene_noise_field(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    memset(buffer, ' ', width * height);
    
    char noise_chars[] = " .'\":;!/\\|()[]{}";
    int char_count = 16;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Perlin-like noise approximation
            float noise = 0.0f;
            float freq = 0.05f;
            float amp = 1.0f;
            
            for (int octave = 0; octave < 4; octave++) {
                float sample_x = x * freq + time * 10.0f;
                float sample_y = y * freq + time * 5.0f;
                
                noise += sinf(sample_x) * cosf(sample_y) * amp;
                freq *= 2.0f;
                amp *= 0.5f;
            }
            
            float intensity = (noise + 1.0f) * 0.5f;
            if (intensity > 0.3f) {
                int char_idx = (int)(intensity * char_count);
                if (char_idx >= char_count) char_idx = char_count - 1;
                buffer[y * width + x] = noise_chars[char_idx];
            }
        }
    }
}

void scene_glitch_corruption(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params; (void)audio;
    
    // Start with a base pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float wave = sinf(x * 0.1f + time * 2.0f);
            buffer[y * width + x] = wave > 0 ? '#' : '.';
        }
    }
    
    // Add glitch corruption
    static float next_glitch = 0.0f;
    if (time > next_glitch) {
        next_glitch = time + 0.1f + (rand() % 100) * 0.01f;
        
        // Horizontal line corruption
        int glitch_y = rand() % height;
        int glitch_start = rand() % width;
        int glitch_len = 5 + rand() % 20;
        
        char glitch_chars[] = "@#$%^&*()_+{}|:<>?";
        for (int x = glitch_start; x < glitch_start + glitch_len && x < width; x++) {
            buffer[glitch_y * width + x] = glitch_chars[rand() % 18];
        }
        
        // Vertical line corruption
        int glitch_x = rand() % width;
        int glitch_y_start = rand() % height;
        int glitch_y_len = 3 + rand() % 10;
        
        for (int y = glitch_y_start; y < glitch_y_start + glitch_y_len && y < height; y++) {
            buffer[y * width + glitch_x] = glitch_chars[rand() % 18];
        }
    }
    
    // Random pixel corruption
    for (int i = 0; i < 50; i++) {
        int x = rand() % width;
        int y = rand() % height;
        if (rand() % 10 == 0) {
            buffer[y * width + x] = '#';
        }
    }
}

// Scene 41: Swarm Intelligence
void scene_swarm_intelligence(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    static float particles[200][4]; // x, y, vx, vy
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < 200; i++) {
            particles[i][0] = rand() % width;
            particles[i][1] = rand() % height;
            particles[i][2] = (rand() % 200 - 100) / 100.0f;
            particles[i][3] = (rand() % 200 - 100) / 100.0f;
        }
        initialized = true;
    }
    
    float energy = false ? 0.5f * 3.0f : 1.0f;
    
    // Update particles
    for (int i = 0; i < 200; i++) {
        // Apply forces
        float center_x = width / 2.0f;
        float center_y = height / 2.0f;
        float dx = center_x - particles[i][0];
        float dy = center_y - particles[i][1];
        float dist = sqrtf(dx * dx + dy * dy);
        
        if (dist > 0) {
            float force = energy / (dist + 1.0f);
            particles[i][2] += (dx / dist) * force * 0.01f;
            particles[i][3] += (dy / dist) * force * 0.01f;
        }
        
        // Update position
        particles[i][0] += particles[i][2];
        particles[i][1] += particles[i][3];
        
        // Friction
        particles[i][2] *= 0.98f;
        particles[i][3] *= 0.98f;
        
        // Wrap around
        if (particles[i][0] < 0) particles[i][0] = width;
        if (particles[i][0] >= width) particles[i][0] = 0;
        if (particles[i][1] < 0) particles[i][1] = height;
        if (particles[i][1] >= height) particles[i][1] = 0;
        
        // Draw particle
        int x = (int)particles[i][0];
        int y = (int)particles[i][1];
        float speed = sqrtf(particles[i][2] * particles[i][2] + particles[i][3] * particles[i][3]);
        char particle_char = speed > 0.5f ? '*' : speed > 0.2f ? '+' : '.';
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            buffer[y * width + x] = particle_char;
        }
    }
}

// Scene 42: Fractal Zoom
void scene_fractal_zoom(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float zoom = 1.0f + time * 0.1f;
    if (false) {
        zoom += 0.5f * 0.5f;
    }
    
    float center_x = width / 2.0f;
    float center_y = height / 2.0f;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Map screen coordinates to complex plane
            float real = (x - center_x) / (width * 0.25f * zoom) - 0.75f;
            float imag = (y - center_y) / (height * 0.25f * zoom);
            
            // Julia set calculation
            float zr = real;
            float zi = imag;
            float cr = -0.7f + sinf(time * 0.3f) * 0.2f;
            float ci = 0.27015f + cosf(time * 0.2f) * 0.1f;
            
            int iterations = 0;
            int max_iter = 50;
            
            while (iterations < max_iter && (zr * zr + zi * zi) < 4.0f) {
                float temp = zr * zr - zi * zi + cr;
                zi = 2.0f * zr * zi + ci;
                zr = temp;
                iterations++;
            }
            
            if (iterations < max_iter) {
                char fractal_chars[] = " .'\":;!/\\|()1";
                int char_idx = iterations % 12;
                buffer[y * width + x] = fractal_chars[char_idx];
            }
        }
    }
}

// Scene 43: Morphing Shapes
void scene_morphing_shapes(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float morph_factor = sinf(time * 0.5f) * 0.5f + 0.5f;
    if (false) {
        morph_factor += 0.5f * 0.3f;
        if (morph_factor > 1.0f) morph_factor = 1.0f;
    }
    
    int center_x = width / 2;
    int center_y = height / 2;
    int radius = height / 4;
    
    // Draw morphing between circle and square
    for (int angle = 0; angle < 360; angle += 5) {
        float rad = angle * M_PI / 180.0f;
        
        // Circle coordinates
        float circle_x = center_x + cosf(rad) * radius;
        float circle_y = center_y + sinf(rad) * radius;
        
        // Square coordinates (approximate)
        float square_dist = radius / fmaxf(fabsf(cosf(rad)), fabsf(sinf(rad)));
        float square_x = center_x + cosf(rad) * square_dist;
        float square_y = center_y + sinf(rad) * square_dist;
        
        // Morph between them
        int x = (int)(circle_x * (1.0f - morph_factor) + square_x * morph_factor);
        int y = (int)(circle_y * (1.0f - morph_factor) + square_y * morph_factor);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            buffer[y * width + x] = '#';
        }
        
        // Add inner shapes
        int inner_radius = radius / 2;
        float inner_x = center_x + cosf(rad + time) * inner_radius * morph_factor;
        float inner_y = center_y + sinf(rad + time) * inner_radius * morph_factor;
        
        if ((int)inner_x >= 0 && (int)inner_x < width && (int)inner_y >= 0 && (int)inner_y < height) {
            buffer[(int)inner_y * width + (int)inner_x] = '*';
        }
    }
}

// Scene 45: Energy Waves
void scene_energy_waves(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float energy = false ? 0.5f : 0.5f;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float distance_from_center = sqrtf(powf(x - width/2.0f, 2) + powf(y - height/2.0f, 2));
            
            // Multiple wave layers
            float wave1 = sinf(distance_from_center * 0.1f - time * 3.0f);
            float wave2 = sinf(distance_from_center * 0.05f - time * 2.0f + M_PI);
            float wave3 = sinf(distance_from_center * 0.2f - time * 4.0f);
            
            float combined = (wave1 + wave2 + wave3) / 3.0f;
            combined *= energy;
            
            if (combined > 0.3f) {
                char intensity_chars[] = ".:-=+*#%@";
                int char_idx = (int)((combined + 1.0f) * 4.5f);
                if (char_idx >= 9) char_idx = 8;
                buffer[y * width + x] = intensity_chars[char_idx];
            }
        }
    }
}

// Scene 46: Digital Rain
void scene_digital_rain(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    static float columns[200];
    static bool initialized = false;
    
    int w = width > 200 ? 200 : width;
    
    if (!initialized) {
        for (int x = 0; x < w; x++) {
            columns[x] = -(rand() % height);
        }
        initialized = true;
    }
    
    memset(buffer, ' ', width * height);
    
    float speed = 1.5f + (false ? 0.5f * 2.0f : 0.0f);
    
    for (int x = 0; x < w; x++) {
        columns[x] += speed;
        
        if (columns[x] > height + 20) {
            columns[x] = -(rand() % 30);
        }
        
        // Draw digital characters falling
        char digital_chars[] = "01#$%@&*+=<>?";
        for (int trail = 0; trail < 20; trail++) {
            int y = (int)columns[x] - trail;
            if (y >= 0 && y < height && x < width) {
                float fade = 1.0f - (trail / 20.0f);
                if (fade > 0.2f) {
                    int char_idx = (rand() + x + trail + (int)time) % 13;
                    buffer[y * width + x] = digital_chars[char_idx];
                }
            }
        }
    }
}

// Scene 47: Psychedelic Patterns
void scene_psychedelic_patterns(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float intensity = false ? 0.5f : 0.5f;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Multiple overlapping patterns
            float pattern1 = sinf(x * 0.1f + time * 2.0f) * cosf(y * 0.1f + time * 1.5f);
            float pattern2 = sinf((x + y) * 0.07f + time * 3.0f);
            float pattern3 = cosf(sqrtf(x*x + y*y) * 0.05f - time * 2.5f);
            
            float combined = (pattern1 + pattern2 + pattern3) * intensity;
            
            if (fabsf(combined) > 0.3f) {
                char pattern_chars[] = "~=*+#@.oO:;!?<>";
                int char_idx = abs((int)(combined * 8 + time * 10)) % 16;
                buffer[y * width + x] = pattern_chars[char_idx];
            }
        }
    }
}

// Scene 48: Quantum Field
void scene_quantum_field(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float quantum_energy = false ? 0.5f : 0.3f;
    
    // Simulate quantum field fluctuations
    for (int i = 0; i < 300; i++) {
        // Random quantum events
        int x = (rand() + (int)(time * 100)) % width;
        int y = (rand() + (int)(time * 73)) % height;
        
        float field_strength = sinf(time * 5.0f + i * 0.1f) * quantum_energy;
        
        if (field_strength > 0.2f) {
            // Quantum particle creation
            char quantum_chars[] = "+*oO.@";
            int char_idx = (int)(field_strength * 6);
            if (char_idx >= 6) char_idx = 5;
            
            buffer[y * width + x] = quantum_chars[char_idx];
            
            // Create field lines around particle
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist > 0 && dist < 3 && field_strength > 0.5f) {
                            if (buffer[ny * width + nx] == ' ') {
                                buffer[ny * width + nx] = dist < 1.5f ? '.' : ':';
                            }
                        }
                    }
                }
            }
        }
    }
}

// Scene 49: Abstract Flow
void scene_abstract_flow(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)zbuffer; (void)params;
    clear_buffer(buffer, zbuffer, width, height);
    
    float flow_speed = 1.0f + (false ? 0.5f * 2.0f : 0.0f);
    
    // Create flowing abstract patterns
    for (int y = 0; y < height; y += 3) {
        for (int x = 0; x < width; x += 4) {
            // Calculate flow direction
            float flow_x = sinf(x * 0.02f + time * flow_speed) * cosf(y * 0.03f + time * 0.7f);
            float flow_y = cosf(x * 0.03f + time * 0.5f) * sinf(y * 0.02f + time * flow_speed);
            
            // Draw flow streams
            for (int step = 0; step < 8; step++) {
                float t = step / 8.0f;
                int stream_x = x + (int)(flow_x * step * 2);
                int stream_y = y + (int)(flow_y * step * 2);
                
                if (stream_x >= 0 && stream_x < width && stream_y >= 0 && stream_y < height) {
                    char flow_chars[] = ".+#@";
                    int char_idx = (int)(t * 4);
                    if (char_idx >= 4) char_idx = 3;
                    
                    buffer[stream_y * width + stream_x] = flow_chars[char_idx];
                }
            }
        }
    }
}

// ============= INFINITE TUNNEL SCENES (50-59) =============

// Scene 50: Spiral Tunnel
void scene_spiral_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value * 2.0f;
    float twist = params[0].value * 3.0f;
    float density = params[2].value;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Create spiral tunnel effect
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = (y - center_y) * 2.0f; // Aspect correction
            
            float dist = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx);
            
            // Spiral tunnel calculation
            float tunnel_z = fmodf(dist * 0.3f - time * speed, 10.0f);
            float spiral_angle = angle + tunnel_z * twist + time * 0.5f;
            
            float spiral_dist = tunnel_z * 2.0f;
            
            // Draw tunnel rings with spiral twist
            if (fmodf(spiral_dist, 2.0f + density) < 0.5f) {
                char tunnel_chars[] = ".+*#@";
                int char_idx = (int)(fmodf(spiral_angle * 2.0f, 5.0f));
                set_pixel(buffer, zbuffer, width, height, x, y, tunnel_chars[char_idx], tunnel_z);
            }
        }
    }
}

// Scene 51: Hex Tunnel
void scene_hex_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value * 3.0f;
    float size = params[0].value * 2.0f + 1.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Create hexagonal tunnel
    for (int layer = 1; layer < 20; layer++) {
        float z = layer * 2.0f - fmodf(time * speed, 40.0f);
        if (z <= 0) continue;
        
        float perspective = 100.0f / z;
        int hex_size = (int)(size * perspective);
        
        if (hex_size < 1) continue;
        
        // Draw hexagon outline
        for (int side = 0; side < 6; side++) {
            float angle1 = side * M_PI / 3.0f;
            float angle2 = (side + 1) * M_PI / 3.0f;
            
            int x1 = center_x + (int)(hex_size * cosf(angle1));
            int y1 = center_y + (int)(hex_size * sinf(angle1) * 0.5f);
            int x2 = center_x + (int)(hex_size * cosf(angle2));
            int y2 = center_y + (int)(hex_size * sinf(angle2) * 0.5f);
            
            draw_line(buffer, width, height, x1, y1, x2, y2, 
                     layer % 3 == 0 ? '#' : layer % 3 == 1 ? '*' : '+');
        }
    }
}

// Scene 52: Star Tunnel  
void scene_star_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value * 4.0f;
    float star_count = params[2].value * 50.0f + 20.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Create star field tunnel effect
    for (int i = 0; i < (int)star_count; i++) {
        // Use deterministic "random" based on star index
        float star_angle = (i * 137.508f) * M_PI / 180.0f; // Golden angle
        float star_dist = (i % 10) * 3.0f + 5.0f;
        
        float z = fmodf(star_dist + time * speed, 50.0f);
        if (z <= 1.0f) z = 50.0f;
        
        float perspective = 100.0f / z;
        
        int x = center_x + (int)(cosf(star_angle) * star_dist * perspective);
        int y = center_y + (int)(sinf(star_angle) * star_dist * perspective * 0.5f);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            char star_char = z < 10.0f ? '*' : z < 20.0f ? '+' : '.';
            set_pixel(buffer, zbuffer, width, height, x, y, star_char, z);
        }
    }
}

// Scene 53: Wormhole
void scene_wormhole(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value * 2.0f;
    float distortion = params[3].value * 2.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Wormhole effect with space-time distortion
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = (y - center_y) * 2.0f;
            
            float dist = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx);
            
            // Wormhole distortion
            float warp_factor = 1.0f + distortion * sinf(dist * 0.1f + time * 2.0f);
            float tunnel_z = fmodf(dist * 0.2f * warp_factor - time * speed, 15.0f);
            
            // Energy ripples
            float ripple = sinf(tunnel_z * 2.0f + angle * 3.0f + time * 3.0f);
            
            if (ripple > 0.3f) {
                char wormhole_chars[] = ".:-=*#@";
                int char_idx = (int)((ripple + 1.0f) * 3.5f);
                if (char_idx >= 7) char_idx = 6;
                
                set_pixel(buffer, zbuffer, width, height, x, y, wormhole_chars[char_idx], tunnel_z);
            }
        }
    }
}

// Scene 54: Cyber Tunnel
void scene_cyber_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value * 3.0f;
    float grid_size = params[0].value * 2.0f + 2.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Cyber/digital tunnel with grid lines
    for (int z_layer = 0; z_layer < 20; z_layer++) {
        float z = z_layer * 3.0f - fmodf(time * speed, 60.0f);
        if (z <= 0) continue;
        
        float perspective = 50.0f / z;
        int tunnel_size = (int)(20.0f * perspective);
        
        if (tunnel_size < 2) continue;
        
        // Draw vertical grid lines
        for (int grid_x = -3; grid_x <= 3; grid_x++) {
            int x1 = center_x + (int)(grid_x * grid_size * perspective);
            int x2 = x1;
            int y1 = center_y - tunnel_size;
            int y2 = center_y + tunnel_size;
            
            if (x1 >= 0 && x1 < width) {
                draw_line(buffer, width, height, x1, y1, x2, y2, 
                         z_layer % 4 == 0 ? '#' : '|');
            }
        }
        
        // Draw horizontal grid lines  
        for (int grid_y = -2; grid_y <= 2; grid_y++) {
            int y1 = center_y + (int)(grid_y * grid_size * perspective * 0.5f);
            int y2 = y1;
            int x1 = center_x - tunnel_size;
            int x2 = center_x + tunnel_size;
            
            if (y1 >= 0 && y1 < height) {
                draw_line(buffer, width, height, x1, y1, x2, y2, 
                         z_layer % 4 == 0 ? '#' : '-');
            }
        }
    }
}

// Scene 55: Ring Tunnel
void scene_ring_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value * 2.5f;
    float ring_spacing = params[2].value * 3.0f + 2.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Concentric ring tunnel
    for (int ring = 0; ring < 25; ring++) {
        float z = ring * ring_spacing - fmodf(time * speed, 100.0f);
        if (z <= 0) continue;
        
        float perspective = 80.0f / z;
        int radius = (int)(15.0f * perspective);
        
        if (radius < 1) continue;
        
        // Draw ring
        for (int angle_deg = 0; angle_deg < 360; angle_deg += 5) {
            float angle = angle_deg * M_PI / 180.0f;
            int x = center_x + (int)(radius * cosf(angle));
            int y = center_y + (int)(radius * sinf(angle) * 0.5f);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char ring_char = ring % 3 == 0 ? '#' : ring % 3 == 1 ? '*' : 'o';
                set_pixel(buffer, zbuffer, width, height, x, y, ring_char, z);
            }
        }
    }
}

// Scene 56: Matrix Tunnel
void scene_matrix_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float tunnel_depth = params[0].value * 40.0f + 20.0f;
    float speed = params[1].value * 5.0f + 1.0f;
    float code_density = params[2].value * 0.8f + 0.2f;
    
    // Matrix-style characters
    static char matrix_chars[] = "01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()+-=[]{}|;:,.<>?";
    int char_count = strlen(matrix_chars);
    
    int center_x = width / 2;
    int center_y = height / 2;
    float max_radius = sqrtf(center_x * center_x + center_y * center_y);
    
    // Create tunnel rings
    for (float z = 0.1f; z < tunnel_depth; z += 0.5f) {
        float ring_time = time * speed - z * 0.3f;
        float ring_radius = (z / tunnel_depth) * max_radius;
        
        // Draw ring points
        int num_points = (int)(ring_radius * 3.14159f);
        if (num_points < 8) num_points = 8;
        
        for (int i = 0; i < num_points; i++) {
            float angle = (i / (float)num_points) * 2.0f * 3.14159f;
            angle += ring_time * 0.1f;
            
            // Calculate position with some wobble
            float wobble = sinf(angle * 3.0f + ring_time) * 0.1f;
            float x = center_x + cosf(angle) * ring_radius * (1.0f + wobble);
            float y = center_y + sinf(angle) * ring_radius * 0.5f * (1.0f + wobble);
            
            // Check bounds
            int px = (int)x;
            int py = (int)y;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                // Determine if we should draw a character at this position
                float noise = sinf(angle * 10.0f + z * 2.0f + ring_time * 3.0f);
                if (noise > (1.0f - code_density)) {
                    // Select character based on position and time
                    int char_idx = (int)(angle * 10.0f + z + ring_time * 5.0f) % char_count;
                    if (char_idx < 0) char_idx += char_count;
                    char matrix_char = matrix_chars[char_idx];
                    
                    // Depth-based brightness (closer = brighter)
                    float brightness = 1.0f - (z / tunnel_depth);
                    if (brightness > 0.3f || z < 5.0f) {
                        set_pixel(buffer, zbuffer, width, height, px, py, matrix_char, z);
                    }
                }
            }
        }
    }
    
    // Add streaming matrix rain effect in the tunnel
    for (int x = 0; x < width; x += 2) {
        for (int y = 0; y < height; y++) {
            float dx = (x - center_x) / (float)center_x;
            float dy = (y - center_y) / (float)center_y;
            float dist = sqrtf(dx * dx + dy * dy);
            
            // Only draw within tunnel radius
            if (dist < 0.9f) {
                // Create vertical streams
                float stream = sinf(x * 0.1f + time * 2.0f) * cosf(y * 0.05f - time * speed);
                if (stream > 0.7f) {
                    int char_idx = (int)(x + y * 2 + time * 10.0f) % char_count;
                    if (char_idx < 0) char_idx += char_count;
                    char matrix_char = matrix_chars[char_idx];
                    
                    // Calculate depth based on distance from center
                    float z = (1.0f - dist) * tunnel_depth * 0.5f;
                    if (zbuffer[y * width + x] > z) {
                        set_pixel(buffer, zbuffer, width, height, x, y, matrix_char, z);
                    }
                }
            }
        }
    }
}

// Scene 57: Speed Tunnel
void scene_speed_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value * 5.0f + 1.0f;
    float line_count = params[2].value * 30.0f + 10.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // High-speed motion lines tunnel
    for (int i = 0; i < (int)line_count; i++) {
        // Deterministic line placement
        float line_angle = (i * 23.0f) * M_PI / 180.0f;
        float line_offset = (i % 5) * 2.0f;
        
        float z = fmodf(line_offset + time * speed, 20.0f);
        if (z <= 0.5f) z = 20.0f;
        
        // Motion blur effect - draw line from center outward
        for (int step = 0; step < 15; step++) {
            float t = step / 15.0f;
            float dist = t * (20.0f - z) * 2.0f + 5.0f;
            
            int x = center_x + (int)(cosf(line_angle) * dist);
            int y = center_y + (int)(sinf(line_angle) * dist * 0.5f);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char speed_chars[] = ".+*#";
                int char_idx = step / 4;
                if (char_idx >= 4) char_idx = 3;
                
                set_pixel(buffer, zbuffer, width, height, x, y, speed_chars[char_idx], z - t);
            }
        }
    }
}

// Scene 58: Pulse Tunnel
void scene_pulse_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float pulse_speed = params[1].value * 2.0f + 0.5f;
    float pulse_freq = params[3].value * 3.0f + 1.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Pulsing energy tunnel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = (y - center_y) * 2.0f;
            
            float dist = sqrtf(dx * dx + dy * dy);
            
            // Pulse wave calculation
            float pulse_wave = sinf(dist * 0.2f - time * pulse_speed) * 
                              sinf(time * pulse_freq) * 0.5f + 0.5f;
            
            // Energy rings
            float ring_dist = fmodf(dist - time * pulse_speed * 5.0f, 8.0f);
            
            if (ring_dist < 1.5f && pulse_wave > 0.4f) {
                char pulse_chars[] = ".+*#@";
                int char_idx = (int)(pulse_wave * 5.0f);
                if (char_idx >= 5) char_idx = 4;
                
                set_pixel(buffer, zbuffer, width, height, x, y, pulse_chars[char_idx], dist);
            }
        }
    }
}

// Scene 59: Vortex Tunnel
void scene_vortex_tunnel(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float rotation_speed = params[1].value * 2.0f;
    float vortex_power = params[3].value * 2.0f + 1.0f;
    float depth_speed = params[0].value * 3.0f + 1.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Swirling vortex tunnel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = (y - center_y) * 2.0f;
            
            float dist = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx);
            
            // Vortex transformation
            float vortex_angle = angle + (vortex_power / (dist + 1.0f)) + time * rotation_speed;
            float tunnel_z = fmodf(dist * 0.3f - time * depth_speed, 15.0f);
            
            // Spiral arms
            float spiral_intensity = sinf(vortex_angle * 3.0f + tunnel_z * 2.0f);
            
            if (spiral_intensity > 0.2f) {
                char vortex_chars[] = ".:-=*#@";
                int char_idx = (int)((spiral_intensity + 1.0f) * 3.5f);
                if (char_idx >= 7) char_idx = 6;
                
                set_pixel(buffer, zbuffer, width, height, x, y, vortex_chars[char_idx], tunnel_z);
            }
        }
    }
}

// Function declarations
void draw_growing_tree_branch(char* buffer, float* zbuffer, int width, int height, 
                     int start_x, int start_y, float angle, int length, 
                     int depth, float branch_factor, float growth_phase);

// ============= NATURE SCENES (60-69) =============

void scene_ocean_waves(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float wave_speed = params[1].value + 0.5f;
    float wave_height = params[0].value * 0.3f + 0.1f;
    float detail = params[2].value * 2.0f + 1.0f;
    
    // Multiple wave layers for realistic ocean
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float wave1 = sinf(x * 0.1f + time * wave_speed) * wave_height;
            float wave2 = sinf(x * 0.05f + time * wave_speed * 0.7f) * wave_height * 0.8f;
            float wave3 = sinf(x * 0.15f + time * wave_speed * 1.3f) * wave_height * 0.6f;
            
            float combined_wave = wave1 + wave2 + wave3;
            float wave_y = height * 0.6f + combined_wave * height * 0.2f;
            
            if (y > wave_y) {
                // Water characters based on depth
                float depth = (y - wave_y) / (height - wave_y);
                char water_chars[] = "~≈≋∞";
                int char_idx = (int)(depth * detail) % 4;
                set_pixel(buffer, zbuffer, width, height, x, y, water_chars[char_idx], depth);
            } else if (y > wave_y - 2) {
                // Wave foam
                if ((int)(x + time * 10) % 3 == 0) {
                    set_pixel(buffer, zbuffer, width, height, x, y, '.', 0.1f);
                }
            }
        }
    }
}

void scene_rain_storm(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float rain_intensity = params[0].value * 200.0f + 50.0f;
    float wind_effect = params[2].value * 2.0f;
    float lightning_chance = params[3].value * 0.1f;
    
    // Rain drops
    for (int i = 0; i < (int)rain_intensity; i++) {
        float drop_x = fmodf((i * 17.3f + time * 20.0f), width);
        float drop_y = fmodf((i * 23.7f + time * 25.0f), height);
        
        // Wind effect
        drop_x += sinf(time + i) * wind_effect;
        
        if (drop_x >= 0 && drop_x < width) {
            char rain_chars[] = "|/\\";
            int char_idx = (int)(wind_effect) % 3;
            set_pixel(buffer, zbuffer, width, height, (int)drop_x, (int)drop_y, rain_chars[char_idx], 0.5f);
        }
    }
    
    // Lightning flashes
    if (fmodf(time * 0.7f, 1.0f) < lightning_chance) {
        // Lightning bolt
        int bolt_x = (int)(fmodf(time * 13.7f, width));
        for (int y = 0; y < height/3; y++) {
            int x = bolt_x + (int)(sinf(y * 0.5f + time * 20.0f) * 3.0f);
            if (x >= 0 && x < width) {
                set_pixel(buffer, zbuffer, width, height, x, y, '|', 0.0f);
            }
        }
    }
}

void scene_infinite_forest(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float depth_speed = params[1].value + 0.5f;
    float tree_density = params[0].value * 20.0f + 10.0f;
    float fog_amount = params[3].value * 0.5f;
    
    // Perspective forest effect
    for (int layer = 0; layer < 5; layer++) {
        float layer_depth = layer * 3.0f + fmodf(time * depth_speed, 3.0f);
        float tree_scale = 1.0f / (layer_depth * 0.3f + 1.0f);
        
        for (int t = 0; t < (int)(tree_density / (layer + 1)); t++) {
            float tree_x = fmodf(t * 47.3f + layer_depth * 0.1f, width);
            float tree_base_y = height * 0.8f - layer * height * 0.1f;
            float tree_height = 20.0f * tree_scale;
            
            // Tree trunk
            for (int h = 0; h < (int)tree_height && tree_base_y - h >= 0; h++) {
                int y = (int)(tree_base_y - h);
                if (y < height) {
                    float fog_factor = 1.0f - (layer * fog_amount);
                    if (fog_factor > 0.2f) {
                        set_pixel(buffer, zbuffer, width, height, (int)tree_x, y, '|', layer_depth);
                    }
                }
            }
            
            // Tree crown
            float crown_radius = tree_height * 0.3f;
            for (int dy = -(int)crown_radius; dy <= (int)crown_radius; dy++) {
                for (int dx = -(int)crown_radius; dx <= (int)crown_radius; dx++) {
                    float dist = sqrtf(dx*dx + dy*dy);
                    if (dist < crown_radius) {
                        int x = (int)(tree_x + dx);
                        int y = (int)(tree_base_y - tree_height + dy);
                        if (x >= 0 && x < width && y >= 0 && y < height) {
                            float fog_factor = 1.0f - (layer * fog_amount);
                            if (fog_factor > 0.2f) {
                                set_pixel(buffer, zbuffer, width, height, x, y, '*', layer_depth);
                            }
                        }
                    }
                }
            }
        }
    }
}

void scene_growing_trees(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float growth_speed = params[1].value + 0.3f;
    int tree_count = (int)(params[0].value * 5.0f + 3.0f);
    float branch_factor = params[2].value + 0.5f;
    
    for (int t = 0; t < tree_count; t++) {
        float tree_x = width * (t + 1.0f) / (tree_count + 1.0f);
        float growth_phase = fmodf(time * growth_speed + t * 0.3f, 4.0f);
        
        // Recursive tree growth
        draw_growing_tree_branch(buffer, zbuffer, width, height, 
                        (int)tree_x, height - 1, 
                        -90.0f, // Upward angle
                        (int)(growth_phase * 8.0f), // Branch length
                        4, // Max depth
                        branch_factor,
                        growth_phase);
    }
}

void draw_growing_tree_branch(char* buffer, float* zbuffer, int width, int height, 
                     int start_x, int start_y, float angle, int length, 
                     int depth, float branch_factor, float growth_phase) {
    if (depth <= 0 || length <= 0) return;
    
    float rad = angle * 3.14159f / 180.0f;
    float dx = cosf(rad);
    float dy = sinf(rad);
    
    // Draw branch
    for (int i = 0; i < length; i++) {
        int x = start_x + (int)(dx * i);
        int y = start_y + (int)(dy * i);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            char branch_char = (depth > 2) ? '|' : '*';
            set_pixel(buffer, zbuffer, width, height, x, y, branch_char, 0.5f);
        }
    }
    
    // Recursive branches
    if (growth_phase > depth * 0.5f) {
        int end_x = start_x + (int)(dx * length);
        int end_y = start_y + (int)(dy * length);
        
        // Left branch
        draw_growing_tree_branch(buffer, zbuffer, width, height, 
                        end_x, end_y, 
                        angle - 30.0f + sinf(growth_phase) * 10.0f,
                        (int)(length * 0.7f * branch_factor), 
                        depth - 1, branch_factor, growth_phase);
        
        // Right branch
        draw_growing_tree_branch(buffer, zbuffer, width, height, 
                        end_x, end_y, 
                        angle + 30.0f + cosf(growth_phase) * 10.0f,
                        (int)(length * 0.7f * branch_factor), 
                        depth - 1, branch_factor, growth_phase);
    }
}

void scene_mountain_range(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float cloud_speed = params[1].value * 0.5f;
    float mountain_height = params[0].value * 0.4f + 0.3f;
    float detail_level = params[2].value * 3.0f + 1.0f;
    
    // Multiple mountain layers for depth
    for (int layer = 0; layer < 4; layer++) {
        float layer_height = mountain_height * (1.0f - layer * 0.2f);
        float parallax = layer * 0.1f;
        
        for (int x = 0; x < width; x++) {
            // Fractal mountain generation
            float mountain_y = 0.0f;
            float freq = 0.01f;
            float amp = layer_height;
            
            for (int octave = 0; octave < (int)detail_level; octave++) {
                mountain_y += sinf((x + time * parallax) * freq) * amp;
                freq *= 2.0f;
                amp *= 0.5f;
            }
            
            int peak_y = (int)(height * (0.7f - mountain_y));
            
            // Draw mountain silhouette
            for (int y = peak_y; y < height; y++) {
                float depth = layer * 2.0f;
                char mountain_chars[] = "^∧▲";
                int char_idx = (y - peak_y) / 3;
                if (char_idx >= 3) char_idx = 2;
                
                set_pixel(buffer, zbuffer, width, height, x, y, mountain_chars[char_idx], depth);
            }
        }
    }
    
    // Moving clouds
    for (int c = 0; c < 20; c++) {
        float cloud_x = fmodf(c * 31.7f + time * cloud_speed * 10.0f, width + 20.0f) - 10.0f;
        float cloud_y = height * 0.2f + sinf(c + time * 0.1f) * height * 0.1f;
        
        // Simple cloud shape
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                float dist = sqrtf(dx*dx*0.5f + dy*dy);
                if (dist < 3.0f) {
                    int x = (int)(cloud_x + dx);
                    int y = (int)(cloud_y + dy);
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        set_pixel(buffer, zbuffer, width, height, x, y, 'o', 0.1f);
                    }
                }
            }
        }
    }
}

void scene_aurora_borealis(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float wave_speed = params[1].value * 2.0f + 1.0f;
    float intensity = params[0].value + 0.3f;
    float color_shift = params[3].value * 10.0f;
    
    // Aurora curtains
    for (int curtain = 0; curtain < 5; curtain++) {
        float curtain_x = width * (curtain + 1.0f) / 6.0f;
        float phase_offset = curtain * 1.2f;
        
        for (int y = 0; y < height * 0.7f; y++) {
            float wave_amplitude = (height - y) * 0.1f * intensity;
            float wave1 = sinf(y * 0.1f + time * wave_speed + phase_offset) * wave_amplitude;
            float wave2 = cosf(y * 0.05f + time * wave_speed * 0.7f + phase_offset) * wave_amplitude * 0.5f;
            
            int x = (int)(curtain_x + wave1 + wave2);
            
            if (x >= 0 && x < width) {
                // Aurora glow effect
                for (int spread = -2; spread <= 2; spread++) {
                    int glow_x = x + spread;
                    if (glow_x >= 0 && glow_x < width) {
                        float glow_intensity = 1.0f - fabsf(spread) * 0.3f;
                        if (glow_intensity > 0.2f) {
                            char aurora_chars[] = ".:-=*#@";
                            int char_idx = (int)((glow_intensity + sinf(time * 3.0f + color_shift)) * 3.0f);
                            if (char_idx >= 7) char_idx = 6;
                            if (char_idx < 0) char_idx = 0;
                            
                            set_pixel(buffer, zbuffer, width, height, glow_x, y, aurora_chars[char_idx], 0.5f);
                        }
                    }
                }
            }
        }
    }
    
    // Starfield background
    for (int star = 0; star < 50; star++) {
        int star_x = (int)(fmodf(star * 17.3f, width));
        int star_y = (int)(fmodf(star * 23.7f, height * 0.3f));
        
        if ((int)(time * 2.0f + star) % 4 == 0) {
            set_pixel(buffer, zbuffer, width, height, star_x, star_y, '.', 10.0f);
        }
    }
}

void scene_flowing_river(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float flow_speed = params[1].value * 3.0f + 1.0f;
    float river_width = params[0].value * 20.0f + 10.0f;
    float turbulence = params[2].value * 2.0f;
    
    int center_y = height / 2;
    
    // River banks
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float bank_distance = fabsf(y - center_y) - river_width * 0.5f;
            
            if (bank_distance > 0 && bank_distance < 5) {
                // Grass and vegetation on banks
                if ((x + y + (int)(time * 2.0f)) % 3 == 0) {
                    char vegetation[] = ".,|'`";
                    int char_idx = (int)bank_distance;
                    if (char_idx >= 5) char_idx = 4;
                    set_pixel(buffer, zbuffer, width, height, x, y, vegetation[char_idx], 2.0f);
                }
            }
        }
    }
    
    // Flowing water
    for (int x = 0; x < width; x++) {
        float river_center = center_y + sinf(x * 0.1f + time * 0.5f) * 3.0f;
        
        for (int dy = -(int)(river_width * 0.5f); dy <= (int)(river_width * 0.5f); dy++) {
            int y = (int)(river_center + dy);
            if (y >= 0 && y < height) {
                // Water flow patterns
                float flow_offset = time * flow_speed;
                float wave_pattern = sinf(x * 0.2f + flow_offset + dy * 0.3f) + 
                                   cosf(x * 0.15f + flow_offset * 0.7f + dy * 0.2f) * turbulence;
                
                char water_chars[] = "~≈≋∞";
                int char_idx = (int)(fmodf(wave_pattern + 2.0f, 4.0f));
                if (char_idx < 0) char_idx = 0;
                if (char_idx >= 4) char_idx = 3;
                
                set_pixel(buffer, zbuffer, width, height, x, y, water_chars[char_idx], 1.0f);
            }
        }
    }
    
    // Fish jumping occasionally
    if (fmodf(time, 3.0f) < 0.1f) {
        int fish_x = (int)(fmodf(time * 37.3f, width));
        int fish_y = center_y + (int)(sinf(time * 15.0f) * 3.0f);
        if (fish_x >= 0 && fish_x < width && fish_y >= 0 && fish_y < height) {
            set_pixel(buffer, zbuffer, width, height, fish_x, fish_y, '>', 0.5f);
        }
    }
}

void scene_desert_dunes(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float wind_speed = params[1].value * 0.5f + 0.1f;
    float dune_height = params[0].value * 0.3f + 0.2f;
    float sand_detail = params[2].value * 5.0f + 2.0f;
    
    // Dune layers with wind effect
    for (int layer = 0; layer < 3; layer++) {
        float layer_depth = layer * 2.0f;
        float wind_offset = time * wind_speed * (layer + 1);
        
        for (int x = 0; x < width; x++) {
            // Multiple sine waves for realistic dune shapes
            float dune_y = 0.0f;
            float freq = 0.02f * (layer + 1);
            float amp = dune_height * (1.0f - layer * 0.3f);
            
            for (int octave = 0; octave < (int)sand_detail; octave++) {
                dune_y += sinf((x + wind_offset * 20.0f) * freq) * amp;
                freq *= 2.0f;
                amp *= 0.6f;
            }
            
            int peak_y = (int)(height * (0.6f + dune_y));
            
            // Draw sand dune
            for (int y = peak_y; y < height; y++) {
                char sand_chars[] = ".·°∘";
                int char_idx = ((x + y + (int)(wind_offset * 10.0f)) % 4);
                set_pixel(buffer, zbuffer, width, height, x, y, sand_chars[char_idx], layer_depth);
            }
        }
    }
    
    // Blowing sand particles
    for (int p = 0; p < 100; p++) {
        float particle_x = fmodf(p * 13.7f + time * wind_speed * 50.0f, width + 10.0f) - 5.0f;
        float particle_y = height * 0.7f + sinf(p + time * 3.0f) * height * 0.2f;
        
        if (particle_x >= 0 && particle_x < width) {
            set_pixel(buffer, zbuffer, width, height, (int)particle_x, (int)particle_y, '.', 0.1f);
        }
    }
    
    // Heat mirage effect
    for (int x = 0; x < width; x += 4) {
        int mirage_y = (int)(height * 0.8f + sinf(x * 0.1f + time * 5.0f) * 2.0f);
        if (mirage_y >= 0 && mirage_y < height) {
            set_pixel(buffer, zbuffer, width, height, x, mirage_y, '~', 0.1f);
        }
    }
}

void scene_coral_reef(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float current_speed = params[1].value + 0.3f;
    float coral_density = params[0].value * 30.0f + 20.0f;
    float fish_activity = params[3].value * 50.0f + 20.0f;
    
    // Coral formations
    for (int c = 0; c < (int)coral_density; c++) {
        float coral_x = fmodf(c * 23.7f, width);
        float coral_base_y = height * 0.8f + sinf(c) * height * 0.1f;
        float coral_height = 5.0f + sinf(c * 1.3f) * 8.0f;
        float sway = sinf(time * current_speed + c) * 2.0f;
        
        // Coral structure
        for (int h = 0; h < (int)coral_height; h++) {
            int x = (int)(coral_x + sinf(h * 0.3f + time * 0.5f) * sway);
            int y = (int)(coral_base_y - h);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char coral_chars[] = "YΨ∩∪";
                int char_idx = h % 4;
                set_pixel(buffer, zbuffer, width, height, x, y, coral_chars[char_idx], 1.0f + h * 0.1f);
            }
        }
        
        // Coral polyps (small details)
        for (int p = 0; p < 5; p++) {
            int polyp_x = (int)(coral_x + sinf(p + time * 2.0f) * 3.0f);
            int polyp_y = (int)(coral_base_y - p * 2.0f);
            
            if (polyp_x >= 0 && polyp_x < width && polyp_y >= 0 && polyp_y < height) {
                set_pixel(buffer, zbuffer, width, height, polyp_x, polyp_y, 'o', 0.8f);
            }
        }
    }
    
    // Swimming fish
    for (int f = 0; f < (int)fish_activity; f++) {
        float fish_x = fmodf(f * 17.3f + time * current_speed * 20.0f, width + 20.0f) - 10.0f;
        float fish_y = height * 0.3f + sinf(f + time * 2.0f) * height * 0.4f;
        
        if (fish_x >= 0 && fish_x < width) {
            char fish_chars[] = "><>o";
            int char_idx = f % 4;
            set_pixel(buffer, zbuffer, width, height, (int)fish_x, (int)fish_y, fish_chars[char_idx], 0.5f);
        }
    }
    
    // Underwater bubbles
    for (int b = 0; b < 30; b++) {
        float bubble_x = fmodf(b * 31.7f, width);
        float bubble_y = fmodf(height - (time * 10.0f + b * 3.0f), height);
        
        if ((int)(time * 3.0f + b) % 4 == 0) {
            set_pixel(buffer, zbuffer, width, height, (int)bubble_x, (int)bubble_y, 'o', 0.2f);
        }
    }
}

void scene_butterfly_garden(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float flutter_speed = params[1].value * 2.0f + 1.0f;
    int butterfly_count = (int)(params[0].value * 20.0f + 10.0f);
    float flower_density = params[2].value * 50.0f + 30.0f;
    
    // Garden flowers
    for (int f = 0; f < (int)flower_density; f++) {
        float flower_x = fmodf(f * 19.7f, width);
        float flower_y = height * 0.7f + sinf(f) * height * 0.2f;
        
        // Flower stem
        for (int s = 0; s < 5; s++) {
            int stem_y = (int)(flower_y + s);
            if (stem_y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)flower_x, stem_y, '|', 2.0f);
            }
        }
        
        // Flower bloom
        char flower_chars[] = "*@#%&";
        int flower_type = f % 5;
        set_pixel(buffer, zbuffer, width, height, (int)flower_x, (int)flower_y, flower_chars[flower_type], 1.8f);
        
        // Petals around flower
        for (int p = 0; p < 4; p++) {
            float petal_angle = p * 1.57f; // 90 degrees
            int petal_x = (int)(flower_x + cosf(petal_angle) * 1.5f);
            int petal_y = (int)(flower_y + sinf(petal_angle) * 1.0f);
            
            if (petal_x >= 0 && petal_x < width && petal_y >= 0 && petal_y < height) {
                set_pixel(buffer, zbuffer, width, height, petal_x, petal_y, 'o', 1.9f);
            }
        }
    }
    
    // Flying butterflies
    for (int b = 0; b < butterfly_count; b++) {
        float flight_phase = time * flutter_speed + b * 0.7f;
        
        // Butterfly flight path (figure-8 pattern)
        float center_x = width * (0.2f + 0.6f * ((float)b / butterfly_count));
        float center_y = height * 0.4f;
        
        float path_x = center_x + sinf(flight_phase) * 15.0f;
        float path_y = center_y + sinf(flight_phase * 2.0f) * 10.0f;
        
        int butterfly_x = (int)path_x;
        int butterfly_y = (int)path_y;
        
        if (butterfly_x >= 0 && butterfly_x < width && butterfly_y >= 0 && butterfly_y < height) {
            // Wing flapping effect
            if ((int)(flight_phase * 8.0f) % 2 == 0) {
                set_pixel(buffer, zbuffer, width, height, butterfly_x, butterfly_y, 'W', 0.5f);
            } else {
                set_pixel(buffer, zbuffer, width, height, butterfly_x, butterfly_y, 'w', 0.5f);
            }
            
            // Wing shadows
            set_pixel(buffer, zbuffer, width, height, butterfly_x - 1, butterfly_y, '.', 0.6f);
            set_pixel(buffer, zbuffer, width, height, butterfly_x + 1, butterfly_y, '.', 0.6f);
        }
    }
    
    // Floating seeds/pollen
    for (int s = 0; s < 40; s++) {
        float seed_x = fmodf(s * 41.3f + time * 5.0f, width);
        float seed_y = fmodf(s * 29.7f + sinf(time + s) * 2.0f, height);
        
        if ((int)(time * 4.0f + s) % 6 == 0) {
            set_pixel(buffer, zbuffer, width, height, (int)seed_x, (int)seed_y, '.', 0.3f);
        }
    }
}

// ============= EXPLOSION SCENES (70-79) =============

void scene_nuclear_blast(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float blast_power = params[0].value * 3.0f + 1.0f;
    float shockwave_speed = params[1].value * 5.0f + 2.0f;
    float debris_density = params[2].value * 100.0f + 50.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    float explosion_phase = fmodf(time * 0.3f, 8.0f); // 8-second cycle
    
    if (explosion_phase < 2.0f) {
        // Initial flash
        float flash_intensity = 1.0f - explosion_phase * 0.5f;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (flash_intensity > 0.5f) {
                    set_pixel(buffer, zbuffer, width, height, x, y, '#', 0.1f);
                }
            }
        }
    }
    
    // Expanding fireball
    float fireball_radius = explosion_phase * blast_power * 15.0f;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dist = sqrtf((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
            
            if (dist < fireball_radius && dist > fireball_radius * 0.3f) {
                float intensity = 1.0f - (dist / fireball_radius);
                char fire_chars[] = ".:-=*#@";
                int char_idx = (int)(intensity * 6.0f + sinf(time * 10.0f + dist * 0.1f));
                if (char_idx >= 7) char_idx = 6;
                if (char_idx < 0) char_idx = 0;
                
                set_pixel(buffer, zbuffer, width, height, x, y, fire_chars[char_idx], dist * 0.1f);
            }
        }
    }
    
    // Mushroom cloud formation (after initial blast)
    if (explosion_phase > 3.0f) {
        float cloud_height = (explosion_phase - 3.0f) * 20.0f;
        float cloud_width = blast_power * 8.0f;
        
        // Stem
        for (int y = center_y; y > center_y - cloud_height * 0.6f; y--) {
            for (int x = center_x - 3; x <= center_x + 3; x++) {
                if (x >= 0 && x < width && y >= 0) {
                    char steam_chars[] = "░▒▓";
                    int char_idx = (int)((center_y - y) * 3.0f / (cloud_height * 0.6f));
                    if (char_idx >= 3) char_idx = 2;
                    set_pixel(buffer, zbuffer, width, height, x, y, steam_chars[char_idx], 2.0f);
                }
            }
        }
        
        // Mushroom cap
        int cap_center_y = (int)(center_y - cloud_height * 0.6f);
        for (int dy = -8; dy <= 4; dy++) {
            for (int dx = -(int)cloud_width; dx <= (int)cloud_width; dx++) {
                float dist = sqrtf(dx*dx*0.5f + dy*dy*2.0f);
                if (dist < cloud_width) {
                    int x = center_x + dx;
                    int y = cap_center_y + dy;
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        char cloud_chars[] = "░▒▓█";
                        int char_idx = (int)(dist / cloud_width * 4.0f);
                        if (char_idx >= 4) char_idx = 3;
                        set_pixel(buffer, zbuffer, width, height, x, y, cloud_chars[char_idx], 3.0f);
                    }
                }
            }
        }
    }
    
    // Debris field
    for (int d = 0; d < (int)debris_density; d++) {
        float debris_x = center_x + sinf(d + explosion_phase * shockwave_speed) * explosion_phase * 30.0f;
        float debris_y = center_y + cosf(d * 1.3f + explosion_phase * shockwave_speed) * explosion_phase * 20.0f;
        
        if (debris_x >= 0 && debris_x < width && debris_y >= 0 && debris_y < height) {
            char debris_chars[] = ".,·°*";
            int char_idx = d % 5;
            set_pixel(buffer, zbuffer, width, height, (int)debris_x, (int)debris_y, debris_chars[char_idx], 1.0f);
        }
    }
}

void scene_building_collapse(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float collapse_speed = params[1].value + 0.5f;
    float building_height = params[0].value * 30.0f + 20.0f;
    float dust_amount = params[3].value * 200.0f + 100.0f;
    
    float collapse_progress = fmodf(time * collapse_speed, 6.0f);
    
    // Multiple buildings
    for (int b = 0; b < 5; b++) {
        float building_x = width * (b + 1.0f) / 6.0f;
        float current_height = building_height * (1.0f - (collapse_progress + b * 0.2f) * 0.3f);
        current_height = fmaxf(current_height, 0.0f);
        
        // Building structure (before collapse)
        for (int h = 0; h < (int)current_height; h++) {
            int y = height - 1 - h;
            if (y >= 0) {
                // Building walls
                set_pixel(buffer, zbuffer, width, height, (int)building_x - 2, y, '|', 2.0f);
                set_pixel(buffer, zbuffer, width, height, (int)building_x + 2, y, '|', 2.0f);
                
                // Windows (every few floors)
                if (h % 4 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, 'o', 1.8f);
                    set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, 'o', 1.8f);
                }
            }
        }
        
        // Falling debris during collapse
        if (collapse_progress > b * 0.5f) {
            for (int d = 0; d < 20; d++) {
                float debris_fall = (collapse_progress - b * 0.5f) * 40.0f;
                float debris_x = building_x + sinf(d + b * 2.0f) * 8.0f;
                float debris_y = height - building_height + debris_fall + d * 2.0f;
                
                if (debris_x >= 0 && debris_x < width && debris_y >= 0 && debris_y < height) {
                    char debris_chars[] = "▓▒░.";
                    int char_idx = d % 4;
                    set_pixel(buffer, zbuffer, width, height, (int)debris_x, (int)debris_y, debris_chars[char_idx], 1.0f);
                }
            }
        }
    }
    
    // Dust clouds
    for (int d = 0; d < (int)dust_amount; d++) {
        float dust_x = fmodf(d * 13.7f + time * 10.0f, width);
        float dust_y = height - 10.0f + sinf(d + time * 3.0f) * 8.0f;
        
        if (collapse_progress > 1.0f) {
            set_pixel(buffer, zbuffer, width, height, (int)dust_x, (int)dust_y, '.', 0.5f);
        }
    }
}

void scene_meteor_impact(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float impact_power = params[0].value * 3.0f + 2.0f;
    float atmospheric_effects = params[1].value + 0.5f;
    float trail_length = params[2].value * 80.0f + 40.0f;
    float destruction_scale = params[3].value * 2.0f + 1.0f;
    
    float impact_cycle = fmodf(time * 0.6f, 12.0f); // Longer, more dramatic cycle
    
    // Atmospheric entry phase (0-2 seconds)
    if (impact_cycle < 2.0f) {
        float entry_progress = impact_cycle / 2.0f;
        
        // Multiple meteors in formation
        for (int m = 0; m < 3; m++) {
            float meteor_x = width * (0.1f + entry_progress * 0.7f) + sinf(m * 2.0f) * 20.0f;
            float meteor_y = height * (0.1f + entry_progress * 0.6f) + cosf(m * 1.5f) * 15.0f;
            
            // Atmospheric heating glow
            float glow_size = 3.0f + sinf(time * 10.0f + m) * 2.0f;
            for (int dy = -(int)glow_size; dy <= (int)glow_size; dy++) {
                for (int dx = -(int)glow_size; dx <= (int)glow_size; dx++) {
                    float dist = sqrtf(dx*dx + dy*dy);
                    if (dist < glow_size) {
                        int x = (int)(meteor_x + dx);
                        int y = (int)(meteor_y + dy);
                        if (x >= 0 && x < width && y >= 0 && y < height) {
                            float glow_intensity = (1.0f - dist / glow_size) * atmospheric_effects;
                            char glow_chars[] = ".:-=*#@";
                            int char_idx = (int)(glow_intensity * 6.0f);
                            if (char_idx >= 7) char_idx = 6;
                            if (char_idx >= 0) {
                                set_pixel(buffer, zbuffer, width, height, x, y, glow_chars[char_idx], 0.3f + dist * 0.1f);
                            }
                        }
                    }
                }
            }
            
            // Extended plasma trail
            for (int t = 0; t < (int)trail_length; t++) {
                float trail_x = meteor_x - t * 3.0f - sinf(t * 0.1f + m) * 2.0f;
                float trail_y = meteor_y - t * 2.0f - cosf(t * 0.1f + m) * 1.5f;
                
                if (trail_x >= 0 && trail_x < width && trail_y >= 0 && trail_y < height) {
                    float trail_intensity = (1.0f - (float)t / trail_length) * atmospheric_effects;
                    trail_intensity += sinf(time * 15.0f + t * 0.2f) * 0.3f; // Flickering
                    
                    char trail_chars[] = ".:-=*#@";
                    int char_idx = (int)(trail_intensity * 6.0f);
                    if (char_idx >= 7) char_idx = 6;
                    if (char_idx >= 0) {
                        set_pixel(buffer, zbuffer, width, height, (int)trail_x, (int)trail_y, trail_chars[char_idx], 0.4f + t * 0.01f);
                    }
                }
            }
            
            // Meteor core
            set_pixel(buffer, zbuffer, width, height, (int)meteor_x, (int)meteor_y, '@', 0.1f);
        }
        
        // Atmospheric shockwaves from sonic booms
        float shockwave_radius = entry_progress * 30.0f;
        for (int wave = 0; wave < 2; wave++) {
            float wave_radius = shockwave_radius - wave * 10.0f;
            if (wave_radius > 0) {
                int center_x = (int)(width * 0.4f);
                int center_y = (int)(height * 0.3f);
                
                for (int angle = 0; angle < 360; angle += 15) {
                    float wave_x = center_x + cosf(angle * 0.0174f) * wave_radius;
                    float wave_y = center_y + sinf(angle * 0.0174f) * wave_radius;
                    
                    if (wave_x >= 0 && wave_x < width && wave_y >= 0 && wave_y < height) {
                        set_pixel(buffer, zbuffer, width, height, (int)wave_x, (int)wave_y, '~', 1.0f);
                    }
                }
            }
        }
    }
    
    // Impact phase (2-5 seconds) - MASSIVE explosion
    else if (impact_cycle >= 2.0f && impact_cycle < 5.0f) {
        float explosion_progress = (impact_cycle - 2.0f) / 3.0f;
        int impact_x = (int)(width * 0.7f);
        int impact_y = (int)(height * 0.8f);
        
        // Multiple expanding blast waves
        for (int wave = 0; wave < 4; wave++) {
            float wave_delay = wave * 0.3f;
            float wave_progress = fmaxf(0.0f, explosion_progress - wave_delay);
            
            if (wave_progress > 0) {
                float wave_radius = wave_progress * impact_power * 40.0f;
                float wave_thickness = 4.0f + wave * 2.0f;
                
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        float dist = sqrtf((x - impact_x) * (x - impact_x) + (y - impact_y) * (y - impact_y));
                        
                        if (dist >= wave_radius - wave_thickness && dist <= wave_radius + wave_thickness) {
                            float ring_intensity = 1.0f - fabsf(dist - wave_radius) / wave_thickness;
                            ring_intensity *= (1.0f - wave * 0.2f); // Diminishing waves
                            ring_intensity += sinf(time * 20.0f + dist * 0.1f) * 0.4f; // Intense flickering
                            
                            if (ring_intensity > 0.2f) {
                                char blast_chars[] = ".:-=*#@█";
                                int char_idx = (int)(ring_intensity * 7.0f);
                                if (char_idx >= 8) char_idx = 7;
                                if (char_idx < 0) char_idx = 0;
                                
                                set_pixel(buffer, zbuffer, width, height, x, y, blast_chars[char_idx], dist * 0.02f);
                            }
                        }
                    }
                }
            }
        }
        
        // Central fireball
        float fireball_size = (1.0f - explosion_progress * 0.5f) * impact_power * 15.0f;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dist = sqrtf((x - impact_x) * (x - impact_x) + (y - impact_y) * (y - impact_y));
                
                if (dist < fireball_size) {
                    float fire_intensity = 1.0f - (dist / fireball_size);
                    fire_intensity += sinf(time * 25.0f + dist * 0.2f) * 0.5f;
                    
                    char fire_chars[] = ".:-=*#@█";
                    int char_idx = (int)(fire_intensity * 7.0f);
                    if (char_idx >= 8) char_idx = 7;
                    if (char_idx < 0) char_idx = 0;
                    
                    set_pixel(buffer, zbuffer, width, height, x, y, fire_chars[char_idx], 0.1f);
                }
            }
        }
        
        // Massive debris field
        for (int d = 0; d < 200; d++) {
            float debris_angle = d * 0.0314f;
            float debris_speed = 20.0f + sinf(d) * 15.0f;
            float debris_dist = explosion_progress * debris_speed * destruction_scale;
            
            float debris_x = impact_x + cosf(debris_angle) * debris_dist;
            float debris_y = impact_y + sinf(debris_angle) * debris_dist - explosion_progress * 5.0f; // Gravity effect
            
            if (debris_x >= 0 && debris_x < width && debris_y >= 0 && debris_y < height) {
                char debris_chars[] = ".,*+x#";
                int char_idx = d % 6;
                set_pixel(buffer, zbuffer, width, height, (int)debris_x, (int)debris_y, debris_chars[char_idx], 0.8f + explosion_progress);
            }
        }
        
        // Ground ejection (material thrown up)
        for (int e = 0; e < 100; e++) {
            float eject_angle = -1.57f + (e * 0.0314f); // Upward arc
            float eject_speed = 25.0f + sinf(e) * 10.0f;
            float eject_time = explosion_progress * 3.0f;
            
            float eject_x = impact_x + cosf(eject_angle) * eject_speed * eject_time;
            float eject_y = impact_y + sinf(eject_angle) * eject_speed * eject_time + 0.5f * 9.8f * eject_time * eject_time; // Gravity
            
            if (eject_x >= 0 && eject_x < width && eject_y >= 0 && eject_y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)eject_x, (int)eject_y, '#', 1.2f);
            }
        }
    }
    
    // Aftermath phase (5+ seconds) - Crater and environmental damage
    else if (impact_cycle >= 5.0f) {
        float aftermath_time = impact_cycle - 5.0f;
        int crater_x = (int)(width * 0.7f);
        int crater_y = (int)(height * 0.8f);
        float crater_radius = destruction_scale * 25.0f;
        
        // Large impact crater with rim
        for (int dy = -(int)crater_radius; dy <= (int)crater_radius; dy++) {
            for (int dx = -(int)crater_radius; dx <= (int)crater_radius; dx++) {
                float dist = sqrtf(dx*dx + dy*dy);
                
                if (dist < crater_radius) {
                    int x = crater_x + dx;
                    int y = crater_y + dy;
                    
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        if (dist > crater_radius * 0.8f) {
                            // Crater rim (raised material)
                            set_pixel(buffer, zbuffer, width, height, x, y, '^', 1.8f);
                        } else {
                            // Crater interior
                            float depth = 1.0f - (dist / (crater_radius * 0.8f));
                            char crater_chars[] = ".,~-";
                            int char_idx = (int)(depth * 3.0f);
                            if (char_idx >= 4) char_idx = 3;
                            
                            set_pixel(buffer, zbuffer, width, height, x, y, crater_chars[char_idx], 2.0f + depth);
                        }
                    }
                }
            }
        }
        
        // Lingering fires and smoke
        for (int f = 0; f < 50; f++) {
            float fire_x = crater_x + sinf(f + aftermath_time) * crater_radius * 0.6f;
            float fire_y = crater_y + cosf(f * 1.3f + aftermath_time) * crater_radius * 0.6f;
            
            if (fire_x >= 0 && fire_x < width && fire_y >= 0 && fire_y < height) {
                if ((int)(time * 8.0f + f) % 4 == 0) { // Flickering fires
                    char fire_chars[] = ".:-*";
                    int char_idx = (int)(sinf(time * 6.0f + f) * 2.0f + 2.0f);
                    if (char_idx >= 4) char_idx = 3;
                    if (char_idx < 0) char_idx = 0;
                    
                    set_pixel(buffer, zbuffer, width, height, (int)fire_x, (int)fire_y, fire_chars[char_idx], 1.0f);
                }
            }
        }
        
        // Rising smoke columns
        for (int s = 0; s < 20; s++) {
            float smoke_x = crater_x + sinf(s * 2.0f) * 30.0f;
            float smoke_base_y = crater_y;
            
            for (int h = 0; h < 15; h++) {
                float smoke_y = smoke_base_y - h * 2.0f - aftermath_time * 5.0f;
                smoke_x += sinf(h * 0.3f + time + s) * 2.0f; // Wind effect
                
                if (smoke_x >= 0 && smoke_x < width && smoke_y >= 0 && smoke_y < height) {
                    float smoke_density = 1.0f - (float)h / 15.0f;
                    if (smoke_density > 0.2f) {
                        char smoke_chars[] = ".,'~";
                        int char_idx = (int)(smoke_density * 3.0f);
                        if (char_idx >= 4) char_idx = 3;
                        
                        set_pixel(buffer, zbuffer, width, height, (int)smoke_x, (int)smoke_y, smoke_chars[char_idx], 2.0f + h * 0.1f);
                    }
                }
            }
        }
    }
}

void scene_chain_explosions(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float chain_speed = params[1].value * 2.0f + 1.0f;
    float explosion_size = params[0].value * 15.0f + 8.0f;
    int explosion_count = (int)(params[2].value * 5.0f + 3.0f);
    
    // Chain reaction timing
    for (int e = 0; e < explosion_count; e++) {
        float explosion_start = e * 0.8f;
        float explosion_time = fmodf(time * chain_speed - explosion_start, 8.0f);
        
        if (explosion_time >= 0 && explosion_time < 3.0f) {
            float explosion_x = width * (0.2f + 0.6f * (float)e / explosion_count);
            float explosion_y = height * (0.3f + 0.4f * sinf(e * 1.7f));
            
            float blast_radius = explosion_time * explosion_size;
            
            // Explosion sphere
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    float dist = sqrtf((x - explosion_x) * (x - explosion_x) + (y - explosion_y) * (y - explosion_y));
                    
                    if (dist < blast_radius && dist > blast_radius * 0.3f) {
                        float intensity = 1.0f - (dist / blast_radius);
                        intensity += sinf(time * 15.0f + dist * 0.2f) * 0.3f; // Flickering
                        
                        char explosion_chars[] = ".:-=*#@";
                        int char_idx = (int)(intensity * 6.0f);
                        if (char_idx >= 7) char_idx = 6;
                        if (char_idx < 0) char_idx = 0;
                        
                        set_pixel(buffer, zbuffer, width, height, x, y, explosion_chars[char_idx], dist * 0.1f);
                    }
                }
            }
            
            // Sparks flying out
            for (int s = 0; s < 20; s++) {
                float spark_angle = s * 0.314f; // 2π/20
                float spark_dist = explosion_time * 20.0f + sinf(s + time * 5.0f) * 5.0f;
                float spark_x = explosion_x + cosf(spark_angle) * spark_dist;
                float spark_y = explosion_y + sinf(spark_angle) * spark_dist;
                
                if (spark_x >= 0 && spark_x < width && spark_y >= 0 && spark_y < height) {
                    set_pixel(buffer, zbuffer, width, height, (int)spark_x, (int)spark_y, '*', 0.5f);
                }
            }
        }
    }
}

void scene_volcanic_eruption(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float eruption_power = params[0].value * 3.0f + 1.0f;
    float lava_flow_speed = params[1].value * 2.0f + 0.5f;
    float ash_density = params[3].value * 100.0f + 50.0f;
    
    int volcano_x = width / 2;
    int volcano_base = height - 5;
    int volcano_peak = (int)(height * 0.6f);
    
    // Volcano mountain
    for (int y = volcano_peak; y < volcano_base; y++) {
        int slope_width = (y - volcano_peak) / 3;
        for (int x = volcano_x - slope_width; x <= volcano_x + slope_width; x++) {
            if (x >= 0 && x < width) {
                char mountain_chars[] = "^∧▲";
                int char_idx = (y - volcano_peak) / 5;
                if (char_idx >= 3) char_idx = 2;
                
                set_pixel(buffer, zbuffer, width, height, x, y, mountain_chars[char_idx], 3.0f);
            }
        }
    }
    
    // Lava fountain
    float fountain_height = eruption_power * 20.0f;
    for (int p = 0; p < 30; p++) {
        float particle_x = volcano_x + sinf(p + time * 5.0f) * 5.0f;
        float particle_y = volcano_peak - (sinf(p * 0.5f + time * 3.0f) + 1.0f) * fountain_height * 0.5f;
        
        if (particle_x >= 0 && particle_x < width && particle_y >= 0 && particle_y < height) {
            char lava_chars[] = ".:-=*#@";
            int char_idx = (int)((volcano_peak - particle_y) / fountain_height * 6.0f);
            if (char_idx >= 7) char_idx = 6;
            if (char_idx < 0) char_idx = 0;
            
            set_pixel(buffer, zbuffer, width, height, (int)particle_x, (int)particle_y, lava_chars[char_idx], 0.5f);
        }
    }
    
    // Lava flows
    for (int flow = 0; flow < 3; flow++) {
        float flow_direction = (flow - 1) * 0.5f; // -0.5, 0, 0.5
        float flow_progress = time * lava_flow_speed;
        
        for (int step = 0; step < (int)flow_progress && step < 50; step++) {
            float lava_x = volcano_x + flow_direction * step * 2.0f + sinf(step * 0.1f + time) * 2.0f;
            float lava_y = volcano_peak + step * 0.8f;
            
            if (lava_x >= 0 && lava_x < width && lava_y >= 0 && lava_y < height) {
                char flow_chars[] = "≈∞#@";
                int char_idx = step % 4;
                set_pixel(buffer, zbuffer, width, height, (int)lava_x, (int)lava_y, flow_chars[char_idx], 1.0f);
            }
        }
    }
    
    // Volcanic ash
    for (int a = 0; a < (int)ash_density; a++) {
        float ash_x = fmodf(a * 17.3f + time * 15.0f, width);
        float ash_y = fmodf(a * 23.7f + sinf(time + a) * 3.0f, height * 0.6f);
        
        char ash_chars[] = ".,·°";
        int char_idx = a % 4;
        set_pixel(buffer, zbuffer, width, height, (int)ash_x, (int)ash_y, ash_chars[char_idx], 0.2f);
    }
    
    // Lightning in ash cloud
    if (fmodf(time, 2.0f) < 0.1f) {
        int lightning_x = volcano_x + (int)(sinf(time * 10.0f) * 20.0f);
        for (int y = 0; y < height * 0.4f; y++) {
            int x = lightning_x + (int)(sinf(y * 0.5f + time * 20.0f) * 3.0f);
            if (x >= 0 && x < width) {
                set_pixel(buffer, zbuffer, width, height, x, y, '|', 0.0f);
            }
        }
    }
}

void scene_shockwave_blast(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float wave_speed = params[1].value * 5.0f + 3.0f;
    float wave_power = params[0].value * 2.0f + 1.0f;
    float distortion = params[2].value * 3.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    float wave_cycle = fmodf(time * 0.5f, 4.0f);
    float primary_wave = wave_cycle * wave_speed * 15.0f;
    float secondary_wave = fmaxf(0.0f, (wave_cycle - 0.5f) * wave_speed * 12.0f);
    float tertiary_wave = fmaxf(0.0f, (wave_cycle - 1.0f) * wave_speed * 10.0f);
    
    // Multiple expanding shockwaves
    float waves[] = {primary_wave, secondary_wave, tertiary_wave};
    float wave_strengths[] = {1.0f, 0.7f, 0.5f};
    
    for (int w = 0; w < 3; w++) {
        if (waves[w] > 0) {
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    float dist = sqrtf((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
                    
                    // Shockwave ring
                    float ring_thickness = 3.0f * wave_power;
                    if (dist >= waves[w] - ring_thickness && dist <= waves[w] + ring_thickness) {
                        float ring_intensity = 1.0f - fabsf(dist - waves[w]) / ring_thickness;
                        ring_intensity *= wave_strengths[w];
                        
                        // Add distortion effect
                        ring_intensity += sinf(dist * 0.2f + time * 8.0f) * distortion * 0.2f;
                        
                        if (ring_intensity > 0.2f) {
                            char wave_chars[] = ".:-=*#@";
                            int char_idx = (int)(ring_intensity * 6.0f);
                            if (char_idx >= 7) char_idx = 6;
                            if (char_idx < 0) char_idx = 0;
                            
                            set_pixel(buffer, zbuffer, width, height, x, y, wave_chars[char_idx], dist * 0.05f);
                        }
                    }
                }
            }
        }
    }
    
    // Central explosion core
    if (wave_cycle < 1.0f) {
        float core_size = (1.0f - wave_cycle) * wave_power * 8.0f;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dist = sqrtf((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
                
                if (dist < core_size) {
                    float core_intensity = 1.0f - (dist / core_size);
                    core_intensity += sinf(time * 20.0f) * 0.3f; // Intense flickering
                    
                    char core_chars[] = "#@█";
                    int char_idx = (int)(core_intensity * 2.0f);
                    if (char_idx >= 3) char_idx = 2;
                    if (char_idx < 0) char_idx = 0;
                    
                    set_pixel(buffer, zbuffer, width, height, x, y, core_chars[char_idx], 0.1f);
                }
            }
        }
    }
}

void scene_glass_shatter(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float shatter_speed = params[1].value * 2.0f + 1.0f;
    float crack_density = params[2].value * 20.0f + 10.0f;
    float fragment_size = params[3].value * 5.0f + 2.0f;
    
    float shatter_progress = fmodf(time * shatter_speed, 6.0f);
    
    int impact_x = width / 2;
    int impact_y = height / 2;
    
    // Initial glass surface (before impact)
    if (shatter_progress < 1.0f) {
        for (int y = 0; y < height; y += 2) {
            for (int x = 0; x < width; x += 3) {
                if ((x + y) % 6 == 0) {
                    set_pixel(buffer, zbuffer, width, height, x, y, '#', 2.0f);
                }
            }
        }
    }
    
    // Crack propagation
    if (shatter_progress >= 1.0f && shatter_progress < 3.0f) {
        float crack_reach = (shatter_progress - 1.0f) * 40.0f;
        
        // Radial cracks
        for (int c = 0; c < (int)crack_density; c++) {
            float crack_angle = c * 0.628f; // 2π/10 roughly
            float crack_length = crack_reach + sinf(c + time * 5.0f) * 5.0f;
            
            for (int step = 0; step < (int)crack_length; step++) {
                float crack_x = impact_x + cosf(crack_angle) * step;
                float crack_y = impact_y + sinf(crack_angle) * step;
                
                // Add jagged variation to cracks
                crack_x += sinf(step * 0.3f + c) * 2.0f;
                crack_y += cosf(step * 0.3f + c) * 1.5f;
                
                if (crack_x >= 0 && crack_x < width && crack_y >= 0 && crack_y < height) {
                    set_pixel(buffer, zbuffer, width, height, (int)crack_x, (int)crack_y, '/', 1.5f);
                }
            }
        }
        
        // Concentric cracks
        for (int ring = 1; ring <= 5; ring++) {
            float ring_radius = ring * 8.0f;
            if (ring_radius < crack_reach) {
                for (int a = 0; a < 360; a += 10) {
                    float ring_x = impact_x + cosf(a * 0.0174f) * ring_radius;
                    float ring_y = impact_y + sinf(a * 0.0174f) * ring_radius;
                    
                    if (ring_x >= 0 && ring_x < width && ring_y >= 0 && ring_y < height) {
                        set_pixel(buffer, zbuffer, width, height, (int)ring_x, (int)ring_y, '\\', 1.5f);
                    }
                }
            }
        }
    }
    
    // Glass fragments falling
    if (shatter_progress >= 3.0f) {
        float fall_time = shatter_progress - 3.0f;
        
        for (int f = 0; f < 100; f++) {
            float start_x = fmodf(f * 13.7f, width);
            float start_y = fmodf(f * 17.3f, height * 0.7f);
            
            // Fragment trajectory
            float fragment_x = start_x + sinf(f) * fall_time * 5.0f;
            float fragment_y = start_y + fall_time * 20.0f + f * 2.0f;
            
            if (fragment_x >= 0 && fragment_x < width && fragment_y >= 0 && fragment_y < height) {
                // Fragment size and rotation effect
                char fragment_chars[] = "◊◇○◑◐";
                int char_idx = (int)(fall_time + f) % 5;
                
                set_pixel(buffer, zbuffer, width, height, (int)fragment_x, (int)fragment_y, fragment_chars[char_idx], 1.0f);
                
                // Fragment sparkle/reflection
                if ((int)(time * 10.0f + f) % 4 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)fragment_x + 1, (int)fragment_y, '.', 0.8f);
                }
            }
        }
    }
}

void scene_demolition_blast(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float blast_sequence_speed = params[1].value + 0.5f;
    float building_count = params[0].value * 3.0f + 2.0f;
    float debris_amount = params[2].value * 150.0f + 100.0f;
    
    float demolition_time = fmodf(time * blast_sequence_speed, 8.0f);
    
    // Sequential building demolition
    for (int b = 0; b < (int)building_count; b++) {
        float building_x = width * (b + 1.0f) / (building_count + 1.0f);
        float building_height = 25.0f + sinf(b) * 10.0f;
        float blast_delay = b * 1.5f;
        
        float building_phase = demolition_time - blast_delay;
        
        if (building_phase < 0) {
            // Building intact
            for (int h = 0; h < (int)building_height; h++) {
                int y = height - 1 - h;
                if (y >= 0) {
                    // Building structure
                    set_pixel(buffer, zbuffer, width, height, (int)building_x - 2, y, '#', 3.0f);
                    set_pixel(buffer, zbuffer, width, height, (int)building_x + 2, y, '#', 3.0f);
                    
                    // Windows
                    if (h % 3 == 0) {
                        set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, 'o', 2.8f);
                        set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, 'o', 2.8f);
                    }
                }
            }
        } else if (building_phase < 2.0f) {
            // Explosive charges detonating
            float explosion_progress = building_phase / 2.0f;
            
            // Charges going off at different floors
            for (int floor = 0; floor < 5; floor++) {
                float charge_y = height - (floor + 1) * 5.0f;
                float charge_time = explosion_progress * 5.0f - floor;
                
                if (charge_time > 0 && charge_time < 0.5f) {
                    // Explosion flash
                    for (int ex = -3; ex <= 3; ex++) {
                        float flash_x = building_x + ex;
                        if (flash_x >= 0 && flash_x < width && charge_y >= 0) {
                            char flash_chars[] = "*#@█";
                            int char_idx = (int)(charge_time * 8.0f) % 4;
                            set_pixel(buffer, zbuffer, width, height, (int)flash_x, (int)charge_y, flash_chars[char_idx], 0.5f);
                        }
                    }
                }
            }
            
            // Building starting to collapse
            float remaining_height = building_height * (1.0f - explosion_progress * 0.7f);
            for (int h = 0; h < (int)remaining_height; h++) {
                int y = height - 1 - h;
                if (y >= 0) {
                    // Tilting/crumbling effect
                    float tilt = explosion_progress * (h * 0.1f);
                    int tilt_x = (int)(building_x + tilt);
                    
                    if (tilt_x >= 0 && tilt_x < width) {
                        char crumble_chars[] = "▓▒░";
                        int char_idx = (int)(explosion_progress * 3.0f);
                        if (char_idx >= 3) char_idx = 2;
                        
                        set_pixel(buffer, zbuffer, width, height, tilt_x, y, crumble_chars[char_idx], 2.0f);
                    }
                }
            }
        } else {
            // Debris field after collapse
            for (int d = 0; d < 30; d++) {
                float debris_x = building_x + sinf(d + blast_delay) * 15.0f;
                float debris_y = height - 5.0f + cosf(d * 1.3f) * 3.0f;
                
                if (debris_x >= 0 && debris_x < width && debris_y >= 0 && debris_y < height) {
                    char debris_chars[] = "▓▒░.,";
                    int char_idx = d % 5;
                    set_pixel(buffer, zbuffer, width, height, (int)debris_x, (int)debris_y, debris_chars[char_idx], 2.5f);
                }
            }
        }
    }
    
    // Dust clouds
    if (demolition_time > 2.0f) {
        for (int d = 0; d < (int)debris_amount; d++) {
            float dust_x = fmodf(d * 23.7f + time * 20.0f, width);
            float dust_y = height - 15.0f + sinf(d + time * 2.0f) * 10.0f;
            
            set_pixel(buffer, zbuffer, width, height, (int)dust_x, (int)dust_y, '.', 0.3f);
        }
    }
}

void scene_supernova_burst(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float explosion_scale = params[0].value * 3.0f + 1.0f;
    float energy_waves = params[1].value * 5.0f + 3.0f;
    float stellar_debris = params[3].value * 200.0f + 100.0f;
    
    int star_x = width / 2;
    int star_y = height / 2;
    
    float supernova_phase = fmodf(time * 0.2f, 15.0f); // 15-second cycle
    
    // Pre-explosion: Massive star
    if (supernova_phase < 2.0f) {
        float star_size = 5.0f + sinf(time * 3.0f) * 2.0f; // Pulsating
        
        for (int dy = -(int)star_size; dy <= (int)star_size; dy++) {
            for (int dx = -(int)star_size; dx <= (int)star_size; dx++) {
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist < star_size) {
                    int x = star_x + dx;
                    int y = star_y + dy;
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        float star_intensity = 1.0f - (dist / star_size);
                        char star_chars[] = ".:-=*#@█";
                        int char_idx = (int)(star_intensity * 7.0f);
                        if (char_idx >= 8) char_idx = 7;
                        
                        set_pixel(buffer, zbuffer, width, height, x, y, star_chars[char_idx], 1.0f);
                    }
                }
            }
        }
    }
    
    // Core collapse and explosion
    if (supernova_phase >= 2.0f && supernova_phase < 8.0f) {
        float explosion_progress = (supernova_phase - 2.0f) / 6.0f;
        float blast_radius = explosion_progress * explosion_scale * 50.0f;
        
        // Multiple expanding energy shells
        for (int shell = 0; shell < 4; shell++) {
            float shell_radius = blast_radius * (1.0f - shell * 0.2f);
            float shell_thickness = 3.0f + shell * 2.0f;
            
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    float dist = sqrtf((x - star_x) * (x - star_x) + (y - star_y) * (y - star_y));
                    
                    if (dist >= shell_radius - shell_thickness && dist <= shell_radius + shell_thickness) {
                        float shell_intensity = 1.0f - fabsf(dist - shell_radius) / shell_thickness;
                        shell_intensity *= (1.0f - shell * 0.2f); // Outer shells dimmer
                        
                        // Energy wave interference patterns
                        shell_intensity += sinf(dist * 0.1f + time * energy_waves) * 0.3f;
                        
                        if (shell_intensity > 0.3f) {
                            char energy_chars[] = ".:-=*#@█";
                            int char_idx = (int)(shell_intensity * 7.0f);
                            if (char_idx >= 8) char_idx = 7;
                            if (char_idx < 0) char_idx = 0;
                            
                            set_pixel(buffer, zbuffer, width, height, x, y, energy_chars[char_idx], dist * 0.02f);
                        }
                    }
                }
            }
        }
        
        // Central white dwarf/neutron star remnant
        if (explosion_progress > 0.5f) {
            float core_size = 2.0f + sinf(time * 10.0f) * 0.5f;
            for (int dy = -(int)core_size; dy <= (int)core_size; dy++) {
                for (int dx = -(int)core_size; dx <= (int)core_size; dx++) {
                    float dist = sqrtf(dx*dx + dy*dy);
                    if (dist < core_size) {
                        int x = star_x + dx;
                        int y = star_y + dy;
                        if (x >= 0 && x < width && y >= 0 && y < height) {
                            set_pixel(buffer, zbuffer, width, height, x, y, '#', 0.1f);
                        }
                    }
                }
            }
        }
    }
    
    // Expanding stellar material and heavy elements
    if (supernova_phase >= 4.0f) {
        for (int d = 0; d < (int)stellar_debris; d++) {
            float debris_angle = d * 0.0628f; // 2π/100
            float debris_speed = 10.0f + sinf(d) * 5.0f;
            float debris_dist = (supernova_phase - 4.0f) * debris_speed;
            
            float debris_x = star_x + cosf(debris_angle) * debris_dist;
            float debris_y = star_y + sinf(debris_angle) * debris_dist;
            
            if (debris_x >= 0 && debris_x < width && debris_y >= 0 && debris_y < height) {
                // Different elements have different representations
                char element_chars[] = "·°*◦○";
                int element_type = d % 5;
                
                // Twinkling effect for distant matter
                if ((int)(time * 5.0f + d) % 3 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)debris_x, (int)debris_y, element_chars[element_type], 2.0f);
                }
            }
        }
    }
    
    // Gravitational waves (visual distortion effect)
    if (supernova_phase >= 1.0f && supernova_phase < 5.0f) {
        float wave_strength = (5.0f - supernova_phase) * 0.1f;
        for (int y = 0; y < height; y += 4) {
            for (int x = 0; x < width; x += 4) {
                float wave_dist = sqrtf((x - star_x) * (x - star_x) + (y - star_y) * (y - star_y));
                float wave_effect = sinf(wave_dist * 0.1f - time * 8.0f) * wave_strength;
                
                if (wave_effect > 0.05f) {
                    set_pixel(buffer, zbuffer, width, height, x, y, '~', 3.0f);
                }
            }
        }
    }
}

void scene_plasma_discharge(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float discharge_frequency = params[1].value * 3.0f + 2.0f;
    float arc_intensity = params[0].value * 2.0f + 1.0f;
    float field_strength = params[2].value * 5.0f + 2.0f;
    
    // Multiple plasma discharge points
    int discharge_points[][2] = {
        {width/4, height/3},
        {3*width/4, height/3},
        {width/2, 2*height/3},
        {width/6, 2*height/3},
        {5*width/6, 2*height/3}
    };
    
    // Electric arcs between discharge points
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            float arc_chance = sinf(time * discharge_frequency + i + j) * 0.5f + 0.5f;
            
            if (arc_chance > 0.7f) {
                int start_x = discharge_points[i][0];
                int start_y = discharge_points[i][1];
                int end_x = discharge_points[j][0];
                int end_y = discharge_points[j][1];
                
                // Draw jagged lightning arc
                int steps = 20;
                for (int step = 0; step <= steps; step++) {
                    float t = (float)step / steps;
                    
                    // Base interpolation
                    float arc_x = start_x + t * (end_x - start_x);
                    float arc_y = start_y + t * (end_y - start_y);
                    
                    // Add electrical zigzag
                    arc_x += sinf(step * 0.8f + time * 20.0f + i * j) * arc_intensity * 5.0f;
                    arc_y += cosf(step * 0.6f + time * 25.0f + i * j) * arc_intensity * 3.0f;
                    
                    if (arc_x >= 0 && arc_x < width && arc_y >= 0 && arc_y < height) {
                        char arc_chars[] = "|/\\-=*#";
                        int char_idx = (int)(arc_chance * 6.0f + sinf(time * 15.0f + step));
                        if (char_idx >= 7) char_idx = 6;
                        if (char_idx < 0) char_idx = 0;
                        
                        set_pixel(buffer, zbuffer, width, height, (int)arc_x, (int)arc_y, arc_chars[char_idx], 0.5f);
                        
                        // Arc glow
                        for (int glow = -1; glow <= 1; glow++) {
                            int glow_x = (int)arc_x + glow;
                            if (glow_x >= 0 && glow_x < width && glow != 0) {
                                set_pixel(buffer, zbuffer, width, height, glow_x, (int)arc_y, '.', 0.7f);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Discharge points with corona effects
    for (int i = 0; i < 5; i++) {
        int point_x = discharge_points[i][0];
        int point_y = discharge_points[i][1];
        
        float corona_size = 3.0f + sinf(time * discharge_frequency * 2.0f + i) * 2.0f;
        
        for (int dy = -(int)corona_size; dy <= (int)corona_size; dy++) {
            for (int dx = -(int)corona_size; dx <= (int)corona_size; dx++) {
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist < corona_size && dist > 1.0f) {
                    int x = point_x + dx;
                    int y = point_y + dy;
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        float corona_intensity = 1.0f - (dist / corona_size);
                        corona_intensity += sinf(time * 10.0f + dist + i) * 0.3f;
                        
                        if (corona_intensity > 0.3f) {
                            char corona_chars[] = ".:-=*@";
                            int char_idx = (int)(corona_intensity * 5.0f);
                            if (char_idx >= 6) char_idx = 5;
                            if (char_idx < 0) char_idx = 0;
                            
                            set_pixel(buffer, zbuffer, width, height, x, y, corona_chars[char_idx], 1.0f);
                        }
                    }
                }
            }
        }
        
        // Central electrode
        set_pixel(buffer, zbuffer, width, height, point_x, point_y, '@', 0.1f);
    }
    
    // Electromagnetic field visualization
    for (int y = 0; y < height; y += 3) {
        for (int x = 0; x < width; x += 3) {
            float field_x = 0.0f, field_y = 0.0f;
            
            // Calculate field influence from all discharge points
            for (int i = 0; i < 5; i++) {
                float dx = x - discharge_points[i][0];
                float dy = y - discharge_points[i][1];
                float dist_sq = dx*dx + dy*dy + 1.0f; // +1 to avoid division by zero
                
                field_x += dx / dist_sq;
                field_y += dy / dist_sq;
            }
            
            float field_magnitude = sqrtf(field_x*field_x + field_y*field_y) * field_strength;
            
            if (field_magnitude > 0.1f && field_magnitude < 0.8f) {
                // Field line indicators
                char field_chars[] = "·°∘";
                int char_idx = (int)(field_magnitude * 3.0f) % 3;
                
                if ((int)(time * 4.0f + x + y) % 8 == 0) {
                    set_pixel(buffer, zbuffer, width, height, x, y, field_chars[char_idx], 2.0f);
                }
            }
        }
    }
}

// ============= CITY SCENES (80-89) =============

void scene_city_flythrough(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float camera_speed = params[1].value * 2.0f + 1.0f;
    float building_density = params[0].value * 20.0f + 15.0f;
    float perspective_depth = params[2].value * 50.0f + 30.0f;
    
    float camera_z = fmodf(time * camera_speed * 10.0f, 100.0f);
    
    // Draw buildings in perspective
    for (int b = 0; b < (int)building_density; b++) {
        float building_x = fmodf(b * 17.3f, width) - width * 0.5f;
        float building_z = fmodf(b * 23.7f, perspective_depth) + camera_z;
        
        if (building_z > 0 && building_z < perspective_depth) {
            // Perspective projection
            float screen_x = building_x / building_z * perspective_depth + width * 0.5f;
            float building_height = (20.0f + sinf(b) * 15.0f) / building_z * perspective_depth;
            float building_width = 4.0f / building_z * perspective_depth;
            
            if (screen_x > -building_width && screen_x < width + building_width) {
                // Draw building
                for (int h = 0; h < (int)building_height && h < height; h++) {
                    int y = height - 1 - h;
                    
                    for (int w = -(int)(building_width * 0.5f); w <= (int)(building_width * 0.5f); w++) {
                        int x = (int)(screen_x + w);
                        if (x >= 0 && x < width) {
                            // Building structure
                            char building_chars[] = "|#H";
                            int char_idx = (int)(building_z / perspective_depth * 2.0f);
                            if (char_idx >= 3) char_idx = 2;
                            
                            set_pixel(buffer, zbuffer, width, height, x, y, building_chars[char_idx], building_z);
                            
                            // Windows
                            if (h % 3 == 0 && w % 2 == 0) {
                                set_pixel(buffer, zbuffer, width, height, x, y, 'o', building_z - 0.1f);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Street level
    for (int x = 0; x < width; x++) {
        float street_pattern = sinf(x * 0.1f + camera_z * 0.1f);
        if (street_pattern > 0.5f) {
            set_pixel(buffer, zbuffer, width, height, x, height - 1, '-', 100.0f);
        }
    }
}

void scene_building_growth(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float growth_speed = params[1].value + 0.5f;
    int building_count = (int)(params[0].value * 8.0f + 5.0f);
    float max_height = params[2].value * 25.0f + 15.0f;
    
    for (int b = 0; b < building_count; b++) {
        float building_x = width * (b + 1.0f) / (building_count + 1.0f);
        float growth_phase = fmodf(time * growth_speed + b * 0.5f, 8.0f);
        float target_height = max_height * (0.5f + sinf(b * 1.3f) * 0.5f);
        
        float current_height = 0.0f;
        if (growth_phase < 4.0f) {
            // Growing phase
            current_height = (growth_phase / 4.0f) * target_height;
        } else {
            // Fully grown
            current_height = target_height;
        }
        
        // Construction effect
        bool under_construction = growth_phase < 4.0f;
        
        for (int h = 0; h < (int)current_height; h++) {
            int y = height - 1 - h;
            if (y >= 0) {
                // Building core
                set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, '|', 2.0f);
                set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, '|', 2.0f);
                
                // Under construction top
                if (under_construction && h >= (int)current_height - 3) {
                    // Construction crane effect
                    set_pixel(buffer, zbuffer, width, height, (int)building_x, y, '+', 1.8f);
                    
                    // Sparks/construction activity
                    if ((int)(time * 10.0f + h + b) % 4 == 0) {
                        set_pixel(buffer, zbuffer, width, height, (int)building_x + 2, y, '*', 1.5f);
                    }
                } else {
                    // Regular floors
                    if (h % 4 == 0) {
                        // Floor indicators
                        set_pixel(buffer, zbuffer, width, height, (int)building_x, y, '=', 1.9f);
                    }
                    
                    // Windows
                    if (h % 3 == 1) {
                        set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, 'o', 1.8f);
                        set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, 'o', 1.8f);
                    }
                }
            }
        }
        
        // Foundation
        for (int f = 0; f < 2; f++) {
            int y = height - 1 + f;
            if (y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)building_x - 2, y, '#', 3.0f);
                set_pixel(buffer, zbuffer, width, height, (int)building_x + 2, y, '#', 3.0f);
            }
        }
    }
}

void scene_traffic_flow(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float traffic_speed = params[1].value * 3.0f + 1.0f;
    float vehicle_density = params[0].value * 50.0f + 30.0f;
    int street_count = (int)(params[2].value * 4.0f + 3.0f);
    
    // Draw street grid
    for (int s = 0; s < street_count; s++) {
        int street_y = height * (s + 1) / (street_count + 1);
        
        // Horizontal streets
        for (int x = 0; x < width; x++) {
            set_pixel(buffer, zbuffer, width, height, x, street_y, '-', 5.0f);
            if (street_y + 1 < height) {
                set_pixel(buffer, zbuffer, width, height, x, street_y + 1, '-', 5.0f);
            }
        }
        
        // Traffic on this street
        for (int v = 0; v < (int)(vehicle_density / street_count); v++) {
            float vehicle_x = fmodf(v * 31.7f + time * traffic_speed * 20.0f + s * 10.0f, width + 10.0f) - 5.0f;
            
            if (vehicle_x >= 0 && vehicle_x < width - 2) {
                // Vehicle types
                char vehicle_chars[] = ">=<";
                int vehicle_type = (v + s) % 3;
                
                set_pixel(buffer, zbuffer, width, height, (int)vehicle_x, street_y, vehicle_chars[vehicle_type], 4.0f);
                set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + 1, street_y, '=', 4.2f);
            }
        }
    }
    
    // Vertical streets
    for (int vs = 0; vs < 5; vs++) {
        int street_x = width * (vs + 1) / 6;
        
        for (int y = 0; y < height; y++) {
            set_pixel(buffer, zbuffer, width, height, street_x, y, '|', 5.0f);
        }
        
        // Vertical traffic
        for (int v = 0; v < 8; v++) {
            float vehicle_y = fmodf(v * 17.3f + time * traffic_speed * 15.0f + vs * 5.0f, height + 5.0f) - 2.0f;
            
            if (vehicle_y >= 0 && vehicle_y < height - 1) {
                char vert_chars[] = "^v";
                int dir = (v + vs) % 2;
                set_pixel(buffer, zbuffer, width, height, street_x, (int)vehicle_y, vert_chars[dir], 4.0f);
            }
        }
    }
    
    // Traffic lights
    for (int tl = 0; tl < 3; tl++) {
        int light_x = width * (tl * 2 + 1) / 6;
        int light_y = height / 2;
        
        // Light cycle
        int light_phase = (int)(time * 2.0f + tl) % 3;
        char light_colors[] = "ryo"; // red, yellow, green
        set_pixel(buffer, zbuffer, width, height, light_x, light_y, light_colors[light_phase], 3.0f);
    }
}

void scene_neon_cyberpunk(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float neon_intensity = params[0].value + 0.5f;
    float flicker_rate = params[1].value * 5.0f + 3.0f;
    float rain_amount = params[3].value * 100.0f + 50.0f;
    
    // Towering cyberpunk buildings
    for (int b = 0; b < 8; b++) {
        float building_x = width * (b + 1.0f) / 9.0f;
        float building_height = 25.0f + sinf(b * 1.7f) * 15.0f;
        
        for (int h = 0; h < (int)building_height; h++) {
            int y = height - 1 - h;
            if (y >= 0) {
                // Building silhouette
                set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, '#', 3.0f);
                set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, '#', 3.0f);
                
                // Neon signs (flickering)
                if (h % 5 == 0) {
                    float flicker = sinf(time * flicker_rate + b + h) * 0.5f + 0.5f;
                    if (flicker > 0.3f) {
                        char neon_chars[] = "=+*@";
                        int neon_idx = (int)(flicker * neon_intensity * 3.0f);
                        if (neon_idx >= 4) neon_idx = 3;
                        
                        set_pixel(buffer, zbuffer, width, height, (int)building_x, y, neon_chars[neon_idx], 2.5f);
                        
                        // Neon glow
                        if (flicker > 0.7f) {
                            set_pixel(buffer, zbuffer, width, height, (int)building_x - 2, y, '.', 2.8f);
                            set_pixel(buffer, zbuffer, width, height, (int)building_x + 2, y, '.', 2.8f);
                        }
                    }
                }
                
                // Windows with electric glow
                if (h % 3 == 1 && (int)(time * 2.0f + h + b) % 4 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, 'o', 2.7f);
                    set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, 'o', 2.7f);
                }
            }
        }
    }
    
    // Cyberpunk rain
    for (int r = 0; r < (int)rain_amount; r++) {
        float rain_x = fmodf(r * 13.7f + time * 2.0f, width);
        float rain_y = fmodf(r * 23.7f + time * 30.0f, height);
        
        // Digital rain effect
        char rain_chars[] = ".|/\\";
        int rain_idx = r % 4;
        set_pixel(buffer, zbuffer, width, height, (int)rain_x, (int)rain_y, rain_chars[rain_idx], 0.5f);
    }
    
    // Holographic advertisements
    for (int ad = 0; ad < 3; ad++) {
        float ad_x = width * (ad * 3 + 1) / 10.0f;
        float ad_y = height * 0.3f + sinf(time + ad) * 5.0f;
        
        // Scrolling text effect
        int text_offset = (int)(time * 5.0f + ad * 20.0f) % 20;
        char holo_text[] = "NEON CITY 2077 ";
        
        for (int i = 0; i < 15 && i + text_offset < 16; i++) {
            int x = (int)(ad_x + i * 2);
            if (x >= 0 && x < width) {
                float hologram_flicker = sinf(time * 8.0f + i + ad) * 0.3f + 0.7f;
                if (hologram_flicker > 0.5f) {
                    set_pixel(buffer, zbuffer, width, height, x, (int)ad_y, holo_text[(i + text_offset) % 15], 1.0f);
                }
            }
        }
    }
    
    // Flying vehicles
    for (int v = 0; v < 5; v++) {
        float vehicle_x = fmodf(v * 37.3f + time * 15.0f, width + 20.0f) - 10.0f;
        float vehicle_y = height * 0.2f + sinf(v + time * 2.0f) * height * 0.1f;
        
        if (vehicle_x >= 0 && vehicle_x < width - 3) {
            // Flying car
            set_pixel(buffer, zbuffer, width, height, (int)vehicle_x, (int)vehicle_y, '<', 1.0f);
            set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + 1, (int)vehicle_y, '=', 1.0f);
            set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + 2, (int)vehicle_y, '>', 1.0f);
            
            // Engine glow
            if ((int)(time * 20.0f + v) % 3 == 0) {
                set_pixel(buffer, zbuffer, width, height, (int)vehicle_x - 1, (int)vehicle_y, '*', 0.8f);
            }
        }
    }
}

void scene_city_lights(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float light_density = params[0].value * 200.0f + 150.0f;
    float twinkle_speed = params[1].value * 3.0f + 2.0f;
    float building_height_var = params[2].value + 0.5f;
    
    // City skyline silhouette
    for (int x = 0; x < width; x++) {
        float building_height = height * (0.3f + 0.4f * building_height_var * sinf(x * 0.02f + time * 0.1f));
        
        for (int y = (int)building_height; y < height; y++) {
            set_pixel(buffer, zbuffer, width, height, x, y, '#', 5.0f);
        }
    }
    
    // City lights (windows and street lights)
    for (int l = 0; l < (int)light_density; l++) {
        float light_x = fmodf(l * 17.3f, width);
        float light_y = fmodf(l * 23.7f, height);
        
        // Make sure light is on a building
        float building_height = height * (0.3f + 0.4f * building_height_var * sinf(light_x * 0.02f + time * 0.1f));
        
        if (light_y > building_height && light_y < height - 2) {
            // Twinkling effect
            float twinkle = sinf(time * twinkle_speed + l * 0.1f) * 0.5f + 0.5f;
            
            if (twinkle > 0.3f) {
                char light_chars[] = ".:-*";
                int light_idx = (int)(twinkle * 3.0f);
                if (light_idx >= 4) light_idx = 3;
                
                set_pixel(buffer, zbuffer, width, height, (int)light_x, (int)light_y, light_chars[light_idx], 4.0f);
            }
        }
    }
    
    // Street level lighting
    for (int x = 0; x < width; x += 8) {
        int street_y = height - 1;
        
        // Street lamps
        float lamp_intensity = sinf(time * 1.0f + x * 0.1f) * 0.2f + 0.8f;
        if (lamp_intensity > 0.6f) {
            set_pixel(buffer, zbuffer, width, height, x, street_y - 3, 'T', 4.5f);
            
            // Lamp glow
            for (int glow = -2; glow <= 2; glow++) {
                if (x + glow >= 0 && x + glow < width) {
                    float glow_intensity = 1.0f - fabsf(glow) * 0.3f;
                    if (glow_intensity > 0.3f) {
                        set_pixel(buffer, zbuffer, width, height, x + glow, street_y - 2, '.', 4.2f);
                        set_pixel(buffer, zbuffer, width, height, x + glow, street_y - 1, '.', 4.3f);
                    }
                }
            }
        }
    }
    
    // Illuminated billboards
    for (int b = 0; b < 4; b++) {
        float billboard_x = width * (b + 1.0f) / 5.0f;
        float billboard_y = height * 0.6f;
        
        // Billboard frame
        for (int w = -6; w <= 6; w++) {
            for (int h = -2; h <= 2; h++) {
                int x = (int)(billboard_x + w);
                int y = (int)(billboard_y + h);
                
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    if (h == -2 || h == 2 || w == -6 || w == 6) {
                        set_pixel(buffer, zbuffer, width, height, x, y, '+', 3.0f);
                    } else {
                        // Animated billboard content
                        float animation = sinf(time * 2.0f + b + w + h) * 0.5f + 0.5f;
                        if (animation > 0.4f) {
                            char billboard_chars[] = "@#%&";
                            int char_idx = (int)(animation * 3.0f);
                            if (char_idx >= 4) char_idx = 3;
                            
                            set_pixel(buffer, zbuffer, width, height, x, y, billboard_chars[char_idx], 2.8f);
                        }
                    }
                }
            }
        }
    }
    
    // Reflection on water/wet streets
    for (int x = 0; x < width; x++) {
        int reflection_y = height - 1;
        
        // Get the light from above and create reflection
        char above_char = get_pixel(buffer, width, height, x, reflection_y - 10);
        if (above_char != ' ') {
            float reflection_strength = 0.3f + sinf(time * 4.0f + x * 0.1f) * 0.2f;
            if (reflection_strength > 0.25f) {
                set_pixel(buffer, zbuffer, width, height, x, reflection_y, '.', 4.8f);
            }
        }
    }
}

void scene_skyscraper_forest(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float density = params[0].value * 30.0f + 20.0f;
    float height_variation = params[1].value + 0.5f;
    float architectural_detail = params[2].value * 3.0f + 1.0f;
    
    // Dense forest of skyscrapers
    for (int b = 0; b < (int)density; b++) {
        float building_x = fmodf(b * 23.7f, width);
        float building_width = 2.0f + sinf(b * 1.3f) * 2.0f;
        float building_height = height * (0.4f + height_variation * 0.5f * sinf(b * 0.7f));
        
        for (int h = 0; h < (int)building_height; h++) {
            int y = height - 1 - h;
            if (y >= 0) {
                for (int w = -(int)(building_width * 0.5f); w <= (int)(building_width * 0.5f); w++) {
                    int x = (int)(building_x + w);
                    if (x >= 0 && x < width) {
                        // Building structure with architectural variety
                        char structure_chars[] = "|#H[]";
                        int structure_idx = ((int)(b + h * 0.1f)) % 5;
                        
                        set_pixel(buffer, zbuffer, width, height, x, y, structure_chars[structure_idx], 3.0f + b * 0.1f);
                        
                        // Architectural details
                        if (architectural_detail > 1.0f) {
                            // Setbacks at different floors
                            if (h % (int)(10.0f / architectural_detail) == 0 && w == 0) {
                                set_pixel(buffer, zbuffer, width, height, x, y, '=', 2.8f);
                            }
                            
                            // Spires and antennas
                            if (h >= (int)building_height - 3 && w == 0) {
                                set_pixel(buffer, zbuffer, width, height, x, y, '^', 2.5f);
                            }
                        }
                        
                        // Windows with office lighting
                        if (h % 3 == 1 && w % 2 == 0) {
                            float office_light = sinf(time * 0.5f + b + h) * 0.3f + 0.7f;
                            if (office_light > 0.5f) {
                                set_pixel(buffer, zbuffer, width, height, x, y, 'o', 2.7f);
                            }
                        }
                    }
                }
            }
        }
        
        // Building top features
        int top_y = height - (int)building_height;
        if (top_y >= 0 && top_y < height) {
            // Rooftop equipment
            set_pixel(buffer, zbuffer, width, height, (int)building_x, top_y, '+', 2.0f);
            
            // Blinking aircraft warning lights
            if ((int)(time * 4.0f + b) % 8 == 0) {
                set_pixel(buffer, zbuffer, width, height, (int)building_x, top_y, '*', 1.8f);
            }
        }
    }
    
    // Interconnecting bridges and walkways
    for (int bridge = 0; bridge < 5; bridge++) {
        int bridge_y = height - (int)(15.0f + bridge * 8.0f);
        int bridge_start = (int)(width * 0.1f + bridge * width * 0.15f);
        int bridge_end = bridge_start + (int)(width * 0.1f);
        
        if (bridge_y >= 0 && bridge_y < height) {
            for (int x = bridge_start; x < bridge_end && x < width; x++) {
                set_pixel(buffer, zbuffer, width, height, x, bridge_y, '=', 2.2f);
                
                // Bridge supports
                if ((x - bridge_start) % 5 == 0) {
                    set_pixel(buffer, zbuffer, width, height, x, bridge_y + 1, '|', 2.4f);
                }
            }
        }
    }
    
    // Atmospheric depth effect (fog between buildings)
    for (int fog = 0; fog < 30; fog++) {
        float fog_x = fmodf(fog * 31.7f + time * 2.0f, width);
        float fog_y = height * 0.3f + sinf(fog + time * 0.5f) * height * 0.2f;
        
        if ((int)(time * 3.0f + fog) % 6 == 0) {
            set_pixel(buffer, zbuffer, width, height, (int)fog_x, (int)fog_y, '.', 1.0f);
        }
    }
}

void scene_urban_decay(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float decay_level = params[0].value + 0.3f;
    float nature_reclaim = params[1].value;
    float structural_damage = params[2].value;
    
    // Abandoned buildings with structural damage
    for (int b = 0; b < 6; b++) {
        float building_x = width * (b + 1.0f) / 7.0f;
        float building_height = 20.0f + sinf(b) * 10.0f;
        float damage_factor = decay_level * (0.5f + sinf(b * 1.7f) * 0.5f);
        
        for (int h = 0; h < (int)building_height; h++) {
            int y = height - 1 - h;
            if (y >= 0) {
                // Structural integrity decreases with damage
                float integrity = 1.0f - damage_factor * (h / building_height);
                
                if (integrity > 0.3f) {
                    // Building walls (damaged)
                    char decay_chars[] = "|#+.";
                    int decay_idx = (int)(damage_factor * 3.0f);
                    if (decay_idx >= 4) decay_idx = 3;
                    
                    if (integrity > 0.7f || ((int)(h + b + time * 2.0f)) % 3 != 0) {
                        set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, decay_chars[decay_idx], 3.0f);
                        set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, decay_chars[decay_idx], 3.0f);
                    }
                    
                    // Broken windows
                    if (h % 3 == 1) {
                        float window_damage = sinf(h + b + time * 0.1f) * 0.5f + 0.5f;
                        if (window_damage > damage_factor) {
                            char window_chars[] = "o.x ";
                            int window_idx = (int)(damage_factor * 3.0f);
                            if (window_idx >= 4) window_idx = 3;
                            
                            set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, window_chars[window_idx], 2.8f);
                            set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, window_chars[window_idx], 2.8f);
                        }
                    }
                }
                
                // Structural collapse
                if (structural_damage > 0.5f && integrity < 0.4f) {
                    // Debris falling
                    float debris_x = building_x + sinf(h + time * 3.0f) * 5.0f;
                    float debris_y = y + (1.0f - integrity) * 10.0f;
                    
                    if (debris_x >= 0 && debris_x < width && debris_y < height) {
                        char debris_chars[] = ".,*#";
                        int debris_idx = (int)(structural_damage * 3.0f);
                        if (debris_idx >= 4) debris_idx = 3;
                        
                        set_pixel(buffer, zbuffer, width, height, (int)debris_x, (int)debris_y, debris_chars[debris_idx], 2.5f);
                    }
                }
            }
        }
    }
    
    // Nature reclaiming the city
    if (nature_reclaim > 0.2f) {
        // Vines growing on buildings
        for (int v = 0; v < (int)(nature_reclaim * 20.0f); v++) {
            float vine_x = fmodf(v * 19.7f, width);
            float vine_base_y = height - 5.0f;
            float vine_growth = nature_reclaim * 25.0f;
            
            for (int g = 0; g < (int)vine_growth; g++) {
                float vine_y = vine_base_y - g - sinf(time * 0.5f + v + g * 0.1f) * 2.0f;
                vine_x += sinf(g * 0.2f + time * 0.3f + v) * 0.5f;
                
                if (vine_x >= 0 && vine_x < width && vine_y >= 0 && vine_y < height) {
                    char nature_chars[] = ",.'|";
                    int nature_idx = g % 4;
                    set_pixel(buffer, zbuffer, width, height, (int)vine_x, (int)vine_y, nature_chars[nature_idx], 2.0f);
                }
            }
        }
        
        // Trees growing through concrete
        for (int t = 0; t < 3; t++) {
            float tree_x = width * (t * 2 + 1) / 7.0f;
            float tree_height = nature_reclaim * 15.0f;
            
            for (int h = 0; h < (int)tree_height; h++) {
                int y = height - 1 - h;
                if (y >= 0) {
                    if (h < tree_height * 0.7f) {
                        // Trunk
                        set_pixel(buffer, zbuffer, width, height, (int)tree_x, y, '|', 1.8f);
                    } else {
                        // Canopy
                        for (int dx = -2; dx <= 2; dx++) {
                            int x = (int)(tree_x + dx);
                            if (x >= 0 && x < width && fabsf(dx) <= 2.0f - (h - tree_height * 0.7f) * 0.5f) {
                                set_pixel(buffer, zbuffer, width, height, x, y, '*', 1.5f);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Graffiti and urban art
    for (int g = 0; g < 8; g++) {
        float graffiti_x = width * (g + 1.0f) / 9.0f;
        float graffiti_y = height * 0.7f + sinf(g) * height * 0.1f;
        
        // Simple graffiti patterns
        char graffiti_chars[] = "@#%&*";
        for (int i = 0; i < 5; i++) {
            int x = (int)(graffiti_x + i);
            if (x >= 0 && x < width) {
                set_pixel(buffer, zbuffer, width, height, x, (int)graffiti_y, graffiti_chars[i], 2.3f);
            }
        }
    }
    
    // Trash and debris on ground
    for (int trash = 0; trash < (int)(decay_level * 50.0f); trash++) {
        float trash_x = fmodf(trash * 37.3f, width);
        float trash_y = height - 1 - (trash % 3);
        
        char trash_chars[] = ".,*o";
        int trash_idx = trash % 4;
        set_pixel(buffer, zbuffer, width, height, (int)trash_x, (int)trash_y, trash_chars[trash_idx], 4.0f);
    }
}

void scene_future_metropolis(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float tech_level = params[0].value + 0.5f;
    float aerial_traffic = params[1].value * 20.0f + 10.0f;
    float energy_flow = params[2].value * 3.0f + 1.0f;
    
    // Futuristic architecture with flowing lines
    for (int b = 0; b < 8; b++) {
        float building_x = width * (b + 1.0f) / 9.0f;
        float building_height = 30.0f + sinf(b * 1.3f) * 15.0f;
        float curve_factor = sinf(b) * 3.0f;
        
        for (int h = 0; h < (int)building_height; h++) {
            int y = height - 1 - h;
            if (y >= 0) {
                // Curved building profile
                float curve_offset = sinf(h * 0.1f + b) * curve_factor;
                int x_base = (int)(building_x + curve_offset);
                
                // Holographic building material
                char future_chars[] = "[]{}()<>";
                int char_idx = ((h + b) % 8);
                
                for (int w = -1; w <= 1; w++) {
                    int x = x_base + w;
                    if (x >= 0 && x < width) {
                        set_pixel(buffer, zbuffer, width, height, x, y, future_chars[char_idx], 3.0f);
                        
                        // Energy conduits
                        if (h % 4 == 0 && w == 0) {
                            float energy_pulse = sinf(time * energy_flow * 2.0f + h * 0.1f + b) * 0.5f + 0.5f;
                            if (energy_pulse > 0.6f) {
                                set_pixel(buffer, zbuffer, width, height, x, y, '=', 2.5f);
                            }
                        }
                    }
                }
                
                // Holographic windows
                if (h % 3 == 1 && tech_level > 0.7f) {
                    float holo_flicker = sinf(time * 8.0f + h + b) * 0.3f + 0.7f;
                    if (holo_flicker > 0.5f) {
                        char holo_chars[] = "o*+";
                        int holo_idx = (int)(holo_flicker * 2.0f);
                        if (holo_idx >= 3) holo_idx = 2;
                        
                        set_pixel(buffer, zbuffer, width, height, x_base - 1, y, holo_chars[holo_idx], 2.7f);
                        set_pixel(buffer, zbuffer, width, height, x_base + 1, y, holo_chars[holo_idx], 2.7f);
                    }
                }
            }
        }
        
        // Spire with energy discharge
        int spire_top = height - (int)building_height - 5;
        if (spire_top >= 0) {
            set_pixel(buffer, zbuffer, width, height, (int)building_x, spire_top, '^', 2.0f);
            
            // Energy discharge
            if ((int)(time * 6.0f + b) % 10 == 0) {
                for (int spark = 1; spark <= 3; spark++) {
                    float spark_x = building_x + sinf(time * 20.0f + spark + b) * 4.0f;
                    float spark_y = spire_top - spark;
                    
                    if (spark_x >= 0 && spark_x < width && spark_y >= 0) {
                        set_pixel(buffer, zbuffer, width, height, (int)spark_x, (int)spark_y, '*', 1.8f);
                    }
                }
            }
        }
    }
    
    // Flying vehicles at multiple levels
    for (int level = 0; level < 3; level++) {
        float flight_altitude = height * (0.2f + level * 0.2f);
        
        for (int v = 0; v < (int)(aerial_traffic / (level + 1)); v++) {
            float vehicle_x = fmodf(v * 41.3f + time * (15.0f + level * 5.0f) + level * 30.0f, width + 25.0f) - 12.0f;
            float vehicle_y = flight_altitude + sinf(v + time * 2.0f + level) * 3.0f;
            
            if (vehicle_x >= 0 && vehicle_x < width - 4) {
                // Futuristic vehicle design
                char vehicle_chars[] = "<==>";
                for (int i = 0; i < 4; i++) {
                    set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + i, (int)vehicle_y, vehicle_chars[i], 1.0f);
                }
                
                // Anti-gravity effect
                if ((int)(time * 15.0f + v + level) % 4 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + 1, (int)vehicle_y + 1, '.', 0.8f);
                    set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + 2, (int)vehicle_y + 1, '.', 0.8f);
                }
                
                // Propulsion trail
                for (int trail = 1; trail <= 3; trail++) {
                    float trail_x = vehicle_x - trail * 2.0f;
                    if (trail_x >= 0) {
                        float trail_intensity = 1.0f - trail * 0.3f;
                        if (trail_intensity > 0.2f) {
                            set_pixel(buffer, zbuffer, width, height, (int)trail_x, (int)vehicle_y, '.', 1.2f);
                        }
                    }
                }
            }
        }
    }
    
    // Holographic data streams
    for (int stream = 0; stream < 5; stream++) {
        float stream_x = width * (stream + 1.0f) / 6.0f;
        
        for (int bit = 0; bit < 20; bit++) {
            float bit_y = fmodf(bit * 5.0f + time * energy_flow * 30.0f + stream * 10.0f, height + 20.0f) - 10.0f;
            
            if (bit_y >= 0 && bit_y < height) {
                char data_chars[] = "01";
                int data_bit = (int)(time * 10.0f + bit + stream) % 2;
                
                float data_intensity = sinf(time * 4.0f + bit + stream) * 0.3f + 0.7f;
                if (data_intensity > 0.5f) {
                    set_pixel(buffer, zbuffer, width, height, (int)stream_x, (int)bit_y, data_chars[data_bit], 1.5f);
                }
            }
        }
    }
    
    // Atmospheric processors
    for (int proc = 0; proc < 2; proc++) {
        float proc_x = width * (proc * 3 + 1) / 7.0f;
        float proc_y = height * 0.1f;
        
        // Processor structure
        for (int h = 0; h < 8; h++) {
            int y = (int)(proc_y + h);
            if (y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)proc_x, y, '|', 2.0f);
                
                // Energy rings
                if (h % 2 == 0) {
                    float ring_phase = time * energy_flow + proc + h;
                    float ring_size = 2.0f + sinf(ring_phase) * 1.0f;
                    
                    for (int r = -(int)ring_size; r <= (int)ring_size; r++) {
                        int x = (int)(proc_x + r);
                        if (x >= 0 && x < width && fabsf(r) >= ring_size - 0.5f) {
                            set_pixel(buffer, zbuffer, width, height, x, y, 'o', 1.8f);
                        }
                    }
                }
            }
        }
    }
}

void scene_city_grid(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float grid_density = params[0].value * 2.0f + 1.0f;
    float data_flow = params[1].value * 5.0f + 2.0f;
    float zoom_level = params[2].value * 0.5f + 0.5f;
    
    int grid_size = (int)(8.0f / grid_density);
    if (grid_size < 2) grid_size = 2;
    
    // Top-down city grid view
    for (int gy = 0; gy < height; gy += grid_size) {
        for (int gx = 0; gx < width; gx += grid_size) {
            // Grid cell represents a city block
            int block_type = (int)(sinf(gx * 0.1f + gy * 0.1f + time * 0.1f) * 3.0f + 3.0f) % 5;
            
            // Zoom effect
            int effective_x = (int)(gx * zoom_level + width * (1.0f - zoom_level) * 0.5f);
            int effective_y = (int)(gy * zoom_level + height * (1.0f - zoom_level) * 0.5f);
            
            if (effective_x >= 0 && effective_x < width - grid_size && 
                effective_y >= 0 && effective_y < height - grid_size) {
                
                switch (block_type) {
                    case 0: // Residential block
                        for (int dy = 1; dy < grid_size - 1; dy++) {
                            for (int dx = 1; dx < grid_size - 1; dx++) {
                                set_pixel(buffer, zbuffer, width, height, effective_x + dx, effective_y + dy, '.', 3.0f);
                            }
                        }
                        break;
                        
                    case 1: // Commercial block
                        for (int dy = 0; dy < grid_size; dy++) {
                            for (int dx = 0; dx < grid_size; dx++) {
                                if (dy == 0 || dy == grid_size - 1 || dx == 0 || dx == grid_size - 1) {
                                    set_pixel(buffer, zbuffer, width, height, effective_x + dx, effective_y + dy, '#', 2.8f);
                                }
                            }
                        }
                        break;
                        
                    case 2: // Industrial block
                        for (int dy = 0; dy < grid_size; dy++) {
                            for (int dx = 0; dx < grid_size; dx++) {
                                set_pixel(buffer, zbuffer, width, height, effective_x + dx, effective_y + dy, '+', 2.5f);
                            }
                        }
                        break;
                        
                    case 3: // Park/green space
                        for (int dy = 1; dy < grid_size - 1; dy++) {
                            for (int dx = 1; dx < grid_size - 1; dx++) {
                                if ((dx + dy + (int)(time * 2.0f)) % 3 == 0) {
                                    set_pixel(buffer, zbuffer, width, height, effective_x + dx, effective_y + dy, '*', 3.2f);
                                }
                            }
                        }
                        break;
                        
                    case 4: // Empty lot
                        // Deliberately empty
                        break;
                }
                
                // Grid lines (streets)
                for (int i = 0; i < grid_size; i++) {
                    // Horizontal street
                    set_pixel(buffer, zbuffer, width, height, effective_x + i, effective_y, '-', 4.0f);
                    // Vertical street
                    set_pixel(buffer, zbuffer, width, height, effective_x, effective_y + i, '|', 4.0f);
                }
            }
        }
    }
    
    // Traffic flow visualization
    for (int t = 0; t < 30; t++) {
        // Horizontal traffic
        float traffic_x = fmodf(t * 23.7f + time * data_flow * 20.0f, width);
        int traffic_y = ((t * 7) % (height / grid_size)) * grid_size;
        
        if (traffic_y >= 0 && traffic_y < height) {
            set_pixel(buffer, zbuffer, width, height, (int)traffic_x, traffic_y, '>', 3.5f);
        }
        
        // Vertical traffic
        int traffic_x_v = ((t * 11) % (width / grid_size)) * grid_size;
        float traffic_y_v = fmodf(t * 31.3f + time * data_flow * 15.0f, height);
        
        if (traffic_x_v >= 0 && traffic_x_v < width) {
            set_pixel(buffer, zbuffer, width, height, traffic_x_v, (int)traffic_y_v, 'v', 3.5f);
        }
    }
    
    // Data overlay (signals, communications)
    for (int signal = 0; signal < 15; signal++) {
        float signal_x = fmodf(signal * 43.7f, width);
        float signal_y = fmodf(signal * 37.3f, height);
        
        // Pulsing signals
        float pulse = sinf(time * data_flow + signal) * 0.5f + 0.5f;
        if (pulse > 0.7f) {
            char signal_chars[] = "o*@";
            int signal_idx = (int)(pulse * 2.0f);
            if (signal_idx >= 3) signal_idx = 2;
            
            set_pixel(buffer, zbuffer, width, height, (int)signal_x, (int)signal_y, signal_chars[signal_idx], 2.0f);
            
            // Signal propagation
            for (int prop = 1; prop <= 3; prop++) {
                float prop_radius = pulse * prop * 3.0f;
                for (int angle = 0; angle < 360; angle += 45) {
                    float prop_x = signal_x + cosf(angle * 0.0174f) * prop_radius;
                    float prop_y = signal_y + sinf(angle * 0.0174f) * prop_radius;
                    
                    if (prop_x >= 0 && prop_x < width && prop_y >= 0 && prop_y < height) {
                        set_pixel(buffer, zbuffer, width, height, (int)prop_x, (int)prop_y, '.', 1.5f);
                    }
                }
            }
        }
    }
    
    // Information overlays (coordinates, data)
    if (zoom_level > 0.8f) {
        // Show coordinates at high zoom
        char coord_text[20];
        int text_x = width - 15;
        int text_y = 2;
        
        sprintf(coord_text, "ZOOM:%.1f", zoom_level);
        for (int i = 0; i < 9 && i < width - text_x; i++) {
            if (coord_text[i] != '\0') {
                set_pixel(buffer, zbuffer, width, height, text_x + i, text_y, coord_text[i], 1.0f);
            }
        }
    }
}

void scene_digital_city(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float wireframe_density = params[0].value + 0.5f;
    float digital_noise = params[1].value * 3.0f;
    float data_streams = params[2].value * 20.0f + 10.0f;
    
    // Wireframe buildings
    for (int b = 0; b < 7; b++) {
        float building_x = width * (b + 1.0f) / 8.0f;
        float building_height = 25.0f + sinf(b * 1.7f) * 12.0f;
        float building_width = 4.0f + sinf(b * 0.9f) * 2.0f;
        
        // Digital glitch effect
        float glitch_offset = sinf(time * 20.0f + b) * digital_noise;
        building_x += glitch_offset;
        
        for (int h = 0; h < (int)building_height; h += 2) {
            int y = height - 1 - h;
            if (y >= 0) {
                // Wireframe outline only
                for (int w = -(int)(building_width * 0.5f); w <= (int)(building_width * 0.5f); w++) {
                    int x = (int)(building_x + w);
                    if (x >= 0 && x < width) {
                        // Draw only edges for wireframe effect
                        if (w == -(int)(building_width * 0.5f) || w == (int)(building_width * 0.5f) || 
                            h == 0 || h % (int)(8.0f / wireframe_density) == 0) {
                            
                            char wire_chars[] = "+||-";
                            int wire_idx = 0;
                            if (w == -(int)(building_width * 0.5f) || w == (int)(building_width * 0.5f)) wire_idx = 1;
                            if (h % (int)(8.0f / wireframe_density) == 0) wire_idx = 2;
                            
                            set_pixel(buffer, zbuffer, width, height, x, y, wire_chars[wire_idx], 3.0f);
                        }
                    }
                }
                
                // Digital artifacts
                if (digital_noise > 1.0f && (int)(time * 15.0f + h + b) % 10 == 0) {
                    float artifact_x = building_x + sinf(time * 30.0f + h) * 3.0f;
                    if (artifact_x >= 0 && artifact_x < width) {
                        char artifact_chars[] = "@#%&*";
                        int artifact_idx = (int)(time * 10.0f + h + b) % 5;
                        set_pixel(buffer, zbuffer, width, height, (int)artifact_x, y, artifact_chars[artifact_idx], 2.5f);
                    }
                }
            }
        }
        
        // Digital spire
        int spire_top = height - (int)building_height - 3;
        if (spire_top >= 0) {
            for (int s = 0; s < 3; s++) {
                int spire_y = spire_top - s;
                if (spire_y >= 0) {
                    char spire_chars[] = "^/\\";
                    set_pixel(buffer, zbuffer, width, height, (int)building_x, spire_y, spire_chars[s], 2.0f);
                }
            }
        }
    }
    
    // Data streams flowing through the city
    for (int stream = 0; stream < (int)data_streams; stream++) {
        float stream_path = stream * 0.3f;
        float stream_speed = 2.0f + sinf(stream) * 1.0f;
        
        // Horizontal data streams
        float stream_x = fmodf(stream * 17.3f + time * stream_speed * 25.0f, width + 30.0f) - 15.0f;
        float stream_y = height * (0.2f + 0.6f * sinf(stream_path));
        
        if (stream_x >= 0 && stream_x < width - 5) {
            char data_chars[] = "01><[]{}";
            for (int i = 0; i < 5; i++) {
                int char_idx = (int)(time * 8.0f + stream + i) % 8;
                set_pixel(buffer, zbuffer, width, height, (int)stream_x + i, (int)stream_y, data_chars[char_idx], 1.5f);
            }
            
            // Data stream glow
            for (int g = -1; g <= 1; g++) {
                int glow_y = (int)stream_y + g;
                if (glow_y >= 0 && glow_y < height && g != 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)stream_x + 2, glow_y, '.', 1.8f);
                }
            }
        }
        
        // Vertical data streams
        float stream_x_v = width * (stream % 8) / 8.0f;
        float stream_y_v = fmodf(stream * 23.7f + time * stream_speed * 20.0f, height + 20.0f) - 10.0f;
        
        if (stream_y_v >= 0 && stream_y_v < height - 3) {
            for (int i = 0; i < 3; i++) {
                int y = (int)stream_y_v + i;
                if (y < height) {
                    char vert_chars[] = "!|:";
                    set_pixel(buffer, zbuffer, width, height, (int)stream_x_v, y, vert_chars[i], 1.5f);
                }
            }
        }
    }
    
    // Digital grid overlay
    for (int gx = 0; gx < width; gx += 8) {
        for (int gy = 0; gy < height; gy += 6) {
            // Grid intersection points
            float grid_intensity = sinf(time * 2.0f + gx * 0.1f + gy * 0.1f) * 0.3f + 0.7f;
            if (grid_intensity > 0.8f) {
                set_pixel(buffer, zbuffer, width, height, gx, gy, '+', 4.0f);
            }
        }
    }
    
    // Digital weather/interference
    if (digital_noise > 2.0f) {
        for (int noise = 0; noise < 50; noise++) {
            float noise_x = fmodf(noise * 41.3f + time * 100.0f, width);
            float noise_y = fmodf(noise * 37.7f + sinf(time * 50.0f + noise) * 5.0f, height);
            
            if ((int)(time * 30.0f + noise) % 8 == 0) {
                char noise_chars[] = "#@*%&";
                int noise_idx = (int)(time * 20.0f + noise) % 5;
                set_pixel(buffer, zbuffer, width, height, (int)noise_x, (int)noise_y, noise_chars[noise_idx], 0.5f);
            }
        }
    }
    
    // System status indicators
    char status_text[] = "SYS_ONLINE";
    int status_y = 1;
    for (int i = 0; i < 10; i++) {
        float flicker = sinf(time * 6.0f + i) * 0.2f + 0.8f;
        if (flicker > 0.7f) {
            set_pixel(buffer, zbuffer, width, height, i * 2, status_y, status_text[i], 1.0f);
        }
    }
}

// ============= FREESTYLE SCENES (90-99) =============

void scene_black_hole(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float gravity_strength = params[0].value * 2.0f + 1.0f;
    float accretion_speed = params[1].value * 3.0f + 2.0f;
    float hawking_radiation = params[2].value;
    float time_dilation = params[3].value * 0.5f + 1.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    float event_horizon = 8.0f * gravity_strength;
    
    // Event horizon (pure black)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dist = sqrtf((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
            
            if (dist < event_horizon) {
                set_pixel(buffer, zbuffer, width, height, x, y, ' ', 0.1f);
            }
        }
    }
    
    // Accretion disk
    for (int particle = 0; particle < 200; particle++) {
        float orbit_radius = event_horizon * 1.5f + sinf(particle) * event_horizon * 2.0f;
        float orbit_angle = particle * 0.1f + time * accretion_speed / (orbit_radius * 0.1f); // Closer orbits faster
        
        // Gravitational lensing effect
        float lensing_factor = 1.0f + gravity_strength / (orbit_radius * 0.1f);
        
        float particle_x = center_x + cosf(orbit_angle) * orbit_radius * lensing_factor;
        float particle_y = center_y + sinf(orbit_angle) * orbit_radius * 0.5f; // Flatten disk
        
        if (particle_x >= 0 && particle_x < width && particle_y >= 0 && particle_y < height) {
            // Color based on temperature (closer = hotter)
            float temperature = 1.0f / (orbit_radius / event_horizon);
            char disk_chars[] = ".:-=*#@";
            int char_idx = (int)(temperature * 6.0f);
            if (char_idx >= 7) char_idx = 6;
            
            set_pixel(buffer, zbuffer, width, height, (int)particle_x, (int)particle_y, disk_chars[char_idx], orbit_radius * 0.1f);
        }
    }
    
    // Gravitational lensing of background stars
    for (int star = 0; star < 100; star++) {
        float star_x = fmodf(star * 37.3f, width);
        float star_y = fmodf(star * 23.7f, height);
        
        float dist_to_hole = sqrtf((star_x - center_x) * (star_x - center_x) + (star_y - center_y) * (star_y - center_y));
        
        if (dist_to_hole > event_horizon * 2.0f) {
            // Lensing effect - bend light around the black hole
            float lensing_angle = gravity_strength / dist_to_hole;
            float lensed_x = star_x + sinf(lensing_angle + time * 0.1f) * 2.0f;
            float lensed_y = star_y + cosf(lensing_angle + time * 0.1f) * 2.0f;
            
            if (lensed_x >= 0 && lensed_x < width && lensed_y >= 0 && lensed_y < height) {
                // Twinkling stars
                if ((int)(time * 3.0f + star) % 5 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)lensed_x, (int)lensed_y, '.', 10.0f);
                }
            }
        }
    }
    
    // Relativistic jets
    for (int jet = 0; jet < 2; jet++) {
        float jet_direction = jet == 0 ? -1.57f : 1.57f; // Up and down
        
        for (int length = 0; length < 30; length++) {
            float jet_x = center_x + cosf(jet_direction) * length * 2.0f;
            float jet_y = center_y + sinf(jet_direction) * length * 2.0f;
            
            // Add spiral to jet
            jet_x += sinf(length * 0.2f + time * accretion_speed) * 2.0f;
            
            if (jet_x >= 0 && jet_x < width && jet_y >= 0 && jet_y < height) {
                float jet_intensity = 1.0f - (float)length / 30.0f;
                jet_intensity += sinf(time * 10.0f + length) * 0.3f;
                
                if (jet_intensity > 0.4f) {
                    char jet_chars[] = ".:-=*#";
                    int char_idx = (int)(jet_intensity * 5.0f);
                    if (char_idx >= 6) char_idx = 5;
                    if (char_idx < 0) char_idx = 0;
                    
                    set_pixel(buffer, zbuffer, width, height, (int)jet_x, (int)jet_y, jet_chars[char_idx], 5.0f + length * 0.1f);
                }
            }
        }
    }
    
    // Hawking radiation
    if (hawking_radiation > 0.3f) {
        for (int rad = 0; rad < (int)(hawking_radiation * 30.0f); rad++) {
            float rad_angle = rad * 0.628f + time * 5.0f;
            float rad_dist = event_horizon + sinf(time * 8.0f + rad) * 3.0f;
            
            float rad_x = center_x + cosf(rad_angle) * rad_dist;
            float rad_y = center_y + sinf(rad_angle) * rad_dist;
            
            if (rad_x >= 0 && rad_x < width && rad_y >= 0 && rad_y < height) {
                if ((int)(time * 12.0f + rad) % 6 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)rad_x, (int)rad_y, '*', 1.0f);
                }
            }
        }
    }
    
    // Time dilation effect visualization
    if (time_dilation > 1.2f) {
        // Show time distortion as ripples
        for (int ripple = 0; ripple < 5; ripple++) {
            float ripple_radius = event_horizon * 2.0f + ripple * 8.0f + sinf(time * time_dilation + ripple) * 4.0f;
            
            for (int angle = 0; angle < 360; angle += 20) {
                float ripple_x = center_x + cosf(angle * 0.0174f) * ripple_radius;
                float ripple_y = center_y + sinf(angle * 0.0174f) * ripple_radius;
                
                if (ripple_x >= 0 && ripple_x < width && ripple_y >= 0 && ripple_y < height) {
                    float ripple_intensity = 1.0f - ripple * 0.2f;
                    if (ripple_intensity > 0.2f && (int)(time * 4.0f + ripple + angle) % 8 == 0) {
                        set_pixel(buffer, zbuffer, width, height, (int)ripple_x, (int)ripple_y, '~', 8.0f);
                    }
                }
            }
        }
    }
}

void scene_cyberpunk_city(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float neon_intensity = params[0].value + 0.5f;
    float rain_amount = params[1].value * 50.0f + 30.0f;
    float building_density = params[2].value * 15.0f + 10.0f;
    
    // Cyberpunk buildings
    for (int b = 0; b < (int)building_density; b++) {
        float building_x = width * (b + 1.0f) / (building_density + 1.0f);
        float building_height = 15.0f + sinf(b * 1.3f) * 10.0f;
        
        for (int h = 0; h < (int)building_height; h++) {
            int y = height - 1 - h;
            if (y >= 0) {
                // Building structure
                set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, '|', 3.0f);
                set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, '|', 3.0f);
                
                // Neon advertisements
                if (h % 4 == 1 && neon_intensity > 0.5f) {
                    float flicker = sinf(time * 8.0f + h + b) * 0.5f + 0.5f;
                    if (flicker > 0.7f) {
                        set_pixel(buffer, zbuffer, width, height, (int)building_x - 2, y, '.', 2.8f);
                        set_pixel(buffer, zbuffer, width, height, (int)building_x + 2, y, '.', 2.8f);
                    }
                }
                
                // Windows
                if (h % 3 == 1 && (int)(time * 2.0f + h + b) % 4 == 0) {
                    set_pixel(buffer, zbuffer, width, height, (int)building_x - 1, y, 'o', 2.7f);
                    set_pixel(buffer, zbuffer, width, height, (int)building_x + 1, y, 'o', 2.7f);
                }
            }
        }
    }
    
    // Cyberpunk rain
    for (int r = 0; r < (int)rain_amount; r++) {
        float rain_x = fmodf(r * 13.7f + time * 2.0f, width);
        float rain_y = fmodf(r * 23.7f + time * 30.0f, height);
        
        char rain_chars[] = ".|/\\";
        int rain_idx = r % 4;
        set_pixel(buffer, zbuffer, width, height, (int)rain_x, (int)rain_y, rain_chars[rain_idx], 0.5f);
    }
    
    // Flying vehicles
    for (int v = 0; v < 5; v++) {
        float vehicle_x = fmodf(v * 37.3f + time * 15.0f, width + 20.0f) - 10.0f;
        float vehicle_y = height * 0.2f + sinf(v + time * 2.0f) * height * 0.1f;
        
        if (vehicle_x >= 0 && vehicle_x < width - 3) {
            set_pixel(buffer, zbuffer, width, height, (int)vehicle_x, (int)vehicle_y, '<', 1.0f);
            set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + 1, (int)vehicle_y, '=', 1.0f);
            set_pixel(buffer, zbuffer, width, height, (int)vehicle_x + 2, (int)vehicle_y, '>', 1.0f);
        }
    }
}

void scene_neon_districts(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float neon_frequency = params[0].value * 5.0f + 3.0f;
    float district_count = params[1].value * 8.0f + 4.0f;
    float energy_flow = params[2].value * 3.0f + 1.0f;
    
    // Neon district zones
    for (int d = 0; d < (int)district_count; d++) {
        float district_x = width * (d + 1.0f) / (district_count + 1.0f);
        float district_size = 15.0f + sinf(d) * 10.0f;
        
        // District boundaries
        for (int y = height * 0.3f; y < height; y++) {
            float boundary_flicker = sinf(time * neon_frequency + d + y * 0.1f) * 0.5f + 0.5f;
            if (boundary_flicker > 0.6f) {
                set_pixel(buffer, zbuffer, width, height, (int)(district_x - district_size/2), y, '|', 2.0f);
                set_pixel(buffer, zbuffer, width, height, (int)(district_x + district_size/2), y, '|', 2.0f);
            }
        }
        
        // Neon signs
        for (int sign = 0; sign < 5; sign++) {
            float sign_x = district_x + sinf(sign + d) * district_size * 0.3f;
            float sign_y = height * 0.5f + sign * 5;
            
            if (sign_x >= 0 && sign_x < width && sign_y < height) {
                float sign_intensity = sinf(time * energy_flow * 2.0f + sign + d) * 0.5f + 0.5f;
                if (sign_intensity > 0.4f) {
                    char neon_chars[] = "@#*%&";
                    int char_idx = (sign + d + (int)(time * 2.0f)) % 5;
                    set_pixel(buffer, zbuffer, width, height, (int)sign_x, (int)sign_y, neon_chars[char_idx], 1.5f);
                }
            }
        }
    }
    
    // Energy streams between districts
    for (int stream = 0; stream < 10; stream++) {
        float stream_x = fmodf(stream * 17.3f + time * energy_flow * 10.0f, width);
        float stream_y = height * 0.6f + sinf(stream + time) * height * 0.2f;
        
        if ((int)(time * 6.0f + stream) % 8 == 0) {
            char energy_chars[] = "-=*";
            int char_idx = stream % 3;
            set_pixel(buffer, zbuffer, width, height, (int)stream_x, (int)stream_y, energy_chars[char_idx], 1.8f);
        }
    }
}

void scene_urban_canyon(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float canyon_depth = params[0].value * 20.0f + 15.0f;
    float building_variation = params[1].value + 0.5f;
    float traffic_density = params[2].value * 20.0f + 10.0f;
    
    // Left wall of canyon
    for (int y = 0; y < height; y++) {
        float wall_x = width * 0.1f + sinf(y * 0.1f + time * 0.3f) * 3.0f;
        float wall_height = canyon_depth + sinf(y * 0.05f) * building_variation * 5.0f;
        
        for (int x = 0; x < (int)wall_x; x++) {
            if (x < width) {
                char wall_chars[] = "|||###";
                int char_idx = (x + y) % 6;
                set_pixel(buffer, zbuffer, width, height, x, y, wall_chars[char_idx], 4.0f);
                
                // Windows
                if (x % 3 == 1 && y % 4 == 2) {
                    float window_light = sinf(time * 1.0f + x + y) * 0.3f + 0.7f;
                    if (window_light > 0.5f) {
                        set_pixel(buffer, zbuffer, width, height, x, y, 'o', 3.5f);
                    }
                }
            }
        }
    }
    
    // Right wall of canyon
    for (int y = 0; y < height; y++) {
        float wall_x = width * 0.9f + sinf(y * 0.1f + time * 0.3f + 3.14f) * 3.0f;
        float wall_height = canyon_depth + sinf(y * 0.05f + 1.0f) * building_variation * 5.0f;
        
        for (int x = (int)wall_x; x < width; x++) {
            char wall_chars[] = "|||###";
            int char_idx = (x + y) % 6;
            set_pixel(buffer, zbuffer, width, height, x, y, wall_chars[char_idx], 4.0f);
            
            // Windows
            if (x % 3 == 1 && y % 4 == 2) {
                float window_light = sinf(time * 1.0f + x + y) * 0.3f + 0.7f;
                if (window_light > 0.5f) {
                    set_pixel(buffer, zbuffer, width, height, x, y, 'o', 3.5f);
                }
            }
        }
    }
    
    // Traffic in the canyon
    for (int t = 0; t < (int)traffic_density; t++) {
        float traffic_x = width * 0.2f + fmodf(t * 23.7f + time * 8.0f, width * 0.6f);
        float traffic_y = height * 0.8f + (t % 3) * 2;
        
        if (traffic_x >= 0 && traffic_x < width - 2 && traffic_y < height) {
            // Vehicles
            set_pixel(buffer, zbuffer, width, height, (int)traffic_x, (int)traffic_y, '<', 2.5f);
            set_pixel(buffer, zbuffer, width, height, (int)traffic_x + 1, (int)traffic_y, '=', 2.5f);
            set_pixel(buffer, zbuffer, width, height, (int)traffic_x + 2, (int)traffic_y, '>', 2.5f);
        }
    }
    
    // Hanging cables and wires
    for (int cable = 0; cable < 8; cable++) {
        float cable_x = width * (cable + 1.0f) / 9.0f;
        
        for (int y = 5; y < height * 0.7f; y += 3) {
            float sag = sinf(cable_x * 0.1f + time * 0.5f + cable) * 2.0f;
            int wire_x = (int)(cable_x + sag);
            
            if (wire_x >= 0 && wire_x < width) {
                set_pixel(buffer, zbuffer, width, height, wire_x, y, '|', 3.0f);
            }
        }
    }
}

void scene_dimensional_rift(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float rift_size = params[0].value * 15.0f + 10.0f;
    float dimensional_drift = params[1].value * 3.0f + 1.0f;
    float reality_stability = 1.0f - params[2].value;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Reality grid (stable space)
    for (int y = 0; y < height; y += 3) {
        for (int x = 0; x < width; x += 5) {
            float dist_to_rift = sqrtf((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
            float distortion = 1.0f / (1.0f + dist_to_rift / rift_size);
            
            // Grid distorts near the rift
            float grid_x = x + sinf(time * dimensional_drift + y * 0.1f) * distortion * 10.0f;
            float grid_y = y + cosf(time * dimensional_drift + x * 0.1f) * distortion * 10.0f;
            
            if (grid_x >= 0 && grid_x < width && grid_y >= 0 && grid_y < height) {
                if (reality_stability > distortion) {
                    set_pixel(buffer, zbuffer, width, height, (int)grid_x, (int)grid_y, '.', 5.0f);
                }
            }
        }
    }
    
    // The rift itself
    for (int angle = 0; angle < 360; angle += 5) {
        float rift_radius = rift_size + sinf(time * 2.0f + angle * 0.1f) * 5.0f;
        float rift_x = center_x + cosf(angle * 0.0174f) * rift_radius;
        float rift_y = center_y + sinf(angle * 0.0174f) * rift_radius;
        
        if (rift_x >= 0 && rift_x < width && rift_y >= 0 && rift_y < height) {
            char rift_chars[] = "\\|/-";
            int char_idx = (angle / 45) % 4;
            set_pixel(buffer, zbuffer, width, height, (int)rift_x, (int)rift_y, rift_chars[char_idx], 2.0f);
        }
    }
    
    // Strange symbols from other realities
    for (int leak = 0; leak < 30; leak++) {
        float leak_angle = leak * 0.2f + time * dimensional_drift;
        float leak_dist = rift_size + sinf(time * 5.0f + leak) * 20.0f;
        
        float leak_x = center_x + cosf(leak_angle) * leak_dist;
        float leak_y = center_y + sinf(leak_angle) * leak_dist;
        
        if (leak_x >= 0 && leak_x < width && leak_y >= 0 && leak_y < height) {
            char alien_chars[] = "@#%&?!~";
            int char_idx = (int)(time * 3.0f + leak) % 7;
            
            float flicker = sinf(time * 10.0f + leak) * 0.5f + 0.5f;
            if (flicker > 0.3f) {
                set_pixel(buffer, zbuffer, width, height, (int)leak_x, (int)leak_y, alien_chars[char_idx], 1.0f);
            }
        }
    }
}

void scene_alien_landscape(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float crystal_density = params[0].value * 30.0f + 20.0f;
    float atmosphere_thickness = params[1].value * 3.0f + 1.0f;
    float alien_life = params[2].value;
    
    // Alien terrain
    for (int x = 0; x < width; x++) {
        float terrain_height = height * 0.7f + sinf(x * 0.1f + time * 0.2f) * 5.0f + sinf(x * 0.03f) * 10.0f;
        
        for (int y = (int)terrain_height; y < height; y++) {
            char terrain_chars[] = "~:=";
            int char_idx = (y - (int)terrain_height) % 3;
            set_pixel(buffer, zbuffer, width, height, x, y, terrain_chars[char_idx], 6.0f);
        }
    }
    
    // Crystal formations with shimmer
    for (int c = 0; c < (int)crystal_density; c++) {
        float crystal_x = fmodf(c * 23.7f, width);
        float crystal_height = 5.0f + sinf(c * 1.3f) * 10.0f;
        float crystal_base = height * 0.8f;
        
        for (int h = 0; h < (int)crystal_height; h++) {
            int y = (int)(crystal_base - h);
            if (y >= 0 && y < height) {
                float shimmer = sinf(time * 5.0f + h + c) * 0.5f + 0.5f;
                if (shimmer > 0.3f) {
                    char crystal_chars[] = "/\\|<>";
                    int char_idx = (h + c + (int)(time * 2.0f)) % 5;
                    set_pixel(buffer, zbuffer, width, height, (int)crystal_x, y, crystal_chars[char_idx], 4.0f);
                }
            }
        }
    }
    
    // Floating alien creatures
    if (alien_life > 0.3f) {
        for (int creature = 0; creature < (int)(alien_life * 10.0f); creature++) {
            float creature_x = fmodf(creature * 47.3f + time * 3.0f, width);
            float creature_y = height * 0.3f + sinf(time * 2.0f + creature) * 10.0f;
            
            if (creature_x >= 0 && creature_x < width - 3 && creature_y >= 0 && creature_y < height) {
                // Jellyfish-like floating creatures
                set_pixel(buffer, zbuffer, width, height, (int)creature_x + 1, (int)creature_y, 'o', 2.0f);
                set_pixel(buffer, zbuffer, width, height, (int)creature_x, (int)creature_y + 1, '(', 2.1f);
                set_pixel(buffer, zbuffer, width, height, (int)creature_x + 2, (int)creature_y + 1, ')', 2.1f);
            }
        }
    }
}

void scene_robot_factory(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float production_speed = params[0].value * 5.0f + 2.0f;
    float robot_count = params[1].value * 10.0f + 5.0f;
    float automation_level = params[2].value;
    
    // Assembly line
    for (int x = 0; x < width; x++) {
        char belt_chars[] = "-=_";
        int belt_idx = (int)(x - time * production_speed) % 3;
        if (belt_idx < 0) belt_idx += 3;
        set_pixel(buffer, zbuffer, width, height, x, height - 5, belt_chars[belt_idx], 4.0f);
    }
    
    // Robots on the belt
    for (int r = 0; r < (int)robot_count; r++) {
        float robot_x = fmodf(r * 20.0f + time * production_speed * 8.0f, width + 10.0f) - 5.0f;
        
        if (robot_x >= 0 && robot_x < width - 4) {
            // Robot body
            set_pixel(buffer, zbuffer, width, height, (int)robot_x + 1, height - 8, '[', 3.0f);
            set_pixel(buffer, zbuffer, width, height, (int)robot_x + 2, height - 8, ']', 3.0f);
            
            // Robot head with blinking eyes
            if ((int)(time * 2.0f + r) % 8 != 0) {
                set_pixel(buffer, zbuffer, width, height, (int)robot_x + 1, height - 9, 'o', 2.8f);
                set_pixel(buffer, zbuffer, width, height, (int)robot_x + 2, height - 9, 'o', 2.8f);
            }
        }
    }
    
    // Robotic arms
    for (int arm = 0; arm < 5; arm++) {
        float arm_x = width * (arm + 1) / 6.0f;
        float arm_y = height * 0.3f + sinf(time * 2.0f + arm) * 10.0f;
        
        // Arm segments
        for (int segment = 0; segment < 5; segment++) {
            int y = (int)(arm_y + segment * 2);
            if (y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)arm_x, y, '|', 2.0f);
            }
        }
    }
}

void scene_time_vortex(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float vortex_speed = params[0].value * 5.0f + 2.0f;
    float temporal_distortion = params[1].value * 3.0f + 1.0f;
    float chronon_particles = params[2].value * 100.0f + 50.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Time spiral
    for (int spiral = 0; spiral < 5; spiral++) {
        for (float angle = 0; angle < 20.0f; angle += 0.2f) {
            float radius = angle * 2.0f + spiral * 5.0f;
            float spiral_angle = angle + time * vortex_speed / (spiral + 1);
            
            float x = center_x + cosf(spiral_angle) * radius;
            float y = center_y + sinf(spiral_angle) * radius * 0.6f;
            
            if (x >= 0 && x < width && y >= 0 && y < height && radius < width * 0.4f) {
                char time_chars[] = "~-=+*";
                int char_idx = (int)(angle + spiral) % 5;
                
                float intensity = 1.0f - radius / (width * 0.4f);
                if (intensity > 0.1f) {
                    set_pixel(buffer, zbuffer, width, height, (int)x, (int)y, time_chars[char_idx], radius * 0.1f);
                }
            }
        }
    }
    
    // Clock fragments
    for (int fragment = 0; fragment < 12; fragment++) {
        float fragment_angle = fragment * 0.523f + time * 0.5f;
        float fragment_dist = 15.0f + sinf(time * 2.0f + fragment) * 10.0f;
        
        float fx = center_x + cosf(fragment_angle) * fragment_dist;
        float fy = center_y + sinf(fragment_angle) * fragment_dist;
        
        if (fx >= 0 && fx < width && fy >= 0 && fy < height) {
            char numerals[] = "123456789X";
            int numeral_idx = fragment % 10;
            
            float flicker = sinf(time * 5.0f + fragment) * 0.5f + 0.5f;
            if (flicker > 0.3f) {
                set_pixel(buffer, zbuffer, width, height, (int)fx, (int)fy, numerals[numeral_idx], 2.0f);
            }
        }
    }
}

void scene_glitch_world(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float glitch_intensity = params[0].value + 0.2f;
    float corruption_level = params[1].value;
    float data_fragmentation = params[2].value * 50.0f + 20.0f;
    
    // Base corrupted landscape
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float terrain_val = sinf(x * 0.1f + y * 0.1f + time) * cosf(y * 0.2f - time * 0.5f);
            
            if (terrain_val > 0.3f) {
                char base_chars[] = "#@%&*+=";
                int char_idx = (int)((terrain_val + 1.0f) * 3.5f) % 7;
                set_pixel(buffer, zbuffer, width, height, x, y, base_chars[char_idx], 5.0f);
            }
        }
    }
    
    // Glitch bands
    for (int band = 0; band < (int)(glitch_intensity * 10.0f); band++) {
        float band_y = fmodf(band * 13.7f + time * 20.0f, height);
        
        for (int x = 0; x < width; x++) {
            if ((int)(time * 50.0f + band + x) % 4 == 0) {
                char corruption_chars[] = "!@#$%^&*()";
                int corrupt_idx = (int)(time * 20.0f + x + band) % 10;
                
                if (corruption_level > 0.5f) {
                    set_pixel(buffer, zbuffer, width, height, x, (int)band_y, corruption_chars[corrupt_idx], 1.0f);
                }
            }
        }
    }
    
    // Error messages
    char* error_msgs[] = {"ERROR", "FAULT", "CORRUPT"};
    
    for (int err = 0; err < 3; err++) {
        if ((int)(time * 2.0f + err) % 10 == 0) {
            float err_x = fmodf(err * 41.3f + time * 5.0f, width - 10);
            float err_y = fmodf(err * 37.7f + time * 3.0f, height);
            
            char* msg = error_msgs[err];
            int msg_len = strlen(msg);
            
            for (int i = 0; i < msg_len && err_x + i < width; i++) {
                float flicker = sinf(time * 20.0f + err + i) * 0.5f + 0.5f;
                if (flicker > 0.3f) {
                    set_pixel(buffer, zbuffer, width, height, (int)(err_x + i), (int)err_y, msg[i], 0.5f);
                }
            }
        }
    }
}

void scene_neural_network(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float network_complexity = params[0].value * 5.0f + 3.0f;
    float signal_speed = params[1].value * 5.0f + 2.0f;
    float learning_rate = params[2].value;
    
    int layers = (int)network_complexity;
    int nodes_per_layer = 6;
    
    // Draw neural network nodes
    for (int layer = 0; layer < layers; layer++) {
        float layer_x = width * (layer + 1.0f) / (layers + 1.0f);
        
        for (int node = 0; node < nodes_per_layer; node++) {
            float node_y = height * (node + 1.0f) / (nodes_per_layer + 1.0f);
            
            // Activation visualization
            float activation = sinf(time * 3.0f + layer + node * 0.5f) * 0.5f + 0.5f;
            char node_char = activation > 0.7f ? '@' : (activation > 0.4f ? 'o' : 'O');
            
            set_pixel(buffer, zbuffer, width, height, (int)layer_x, (int)node_y, node_char, 2.0f);
            
            // Signal connections
            if (layer < layers - 1) {
                float next_layer_x = width * (layer + 2.0f) / (layers + 1.0f);
                float signal_phase = fmodf(time * signal_speed + layer + node, 1.0f);
                
                float signal_x = layer_x + (next_layer_x - layer_x) * signal_phase;
                
                if (signal_x >= 0 && signal_x < width && node_y >= 0 && node_y < height) {
                    set_pixel(buffer, zbuffer, width, height, (int)signal_x, (int)node_y, '.', 3.0f);
                }
            }
        }
    }
    
    // Neural activity sparkles
    for (int sparkle = 0; sparkle < 30; sparkle++) {
        float sparkle_x = fmodf(sparkle * 19.7f + time * 20.0f, width);
        float sparkle_y = fmodf(sparkle * 23.3f + sinf(time * 5.0f + sparkle) * 10.0f, height);
        
        if ((int)(time * 10.0f + sparkle) % 6 == 0) {
            set_pixel(buffer, zbuffer, width, height, (int)sparkle_x, (int)sparkle_y, '*', 0.5f);
        }
    }
}

void scene_cosmic_dance(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float dance_speed = params[0].value * 3.0f + 1.0f;
    float body_count = params[1].value * 8.0f + 4.0f;
    float gravity_strength = params[2].value * 2.0f + 1.0f;
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Celestial bodies in orbital dance
    for (int body = 0; body < (int)body_count; body++) {
        float orbit_radius = 10.0f + body * 5.0f;
        float orbit_speed = dance_speed / (1.0f + body * 0.3f);
        float orbit_angle = time * orbit_speed + body * 0.785f;
        
        float px = center_x + cosf(orbit_angle) * orbit_radius;
        float py = center_y + sinf(orbit_angle) * orbit_radius * 0.6f;
        
        if (px >= 0 && px < width && py >= 0 && py < height) {
            char body_chars[] = "*oO@%";
            int char_idx = body % 5;
            set_pixel(buffer, zbuffer, width, height, (int)px, (int)py, body_chars[char_idx], 5.0f);
            
            // Orbit trails
            for (int trail = 1; trail <= 5; trail++) {
                float trail_angle = orbit_angle - trail * 0.1f;
                float trail_x = center_x + cosf(trail_angle) * orbit_radius;
                float trail_y = center_y + sinf(trail_angle) * orbit_radius * 0.6f;
                
                if (trail_x >= 0 && trail_x < width && trail_y >= 0 && trail_y < height) {
                    float trail_intensity = 1.0f - trail * 0.2f;
                    if (trail_intensity > 0.3f && trail % 2 == 0) {
                        set_pixel(buffer, zbuffer, width, height, (int)trail_x, (int)trail_y, '.', 6.0f + trail * 0.1f);
                    }
                }
            }
        }
    }
    
    // Central massive body
    for (int r = 0; r < 3; r++) {
        for (int angle = 0; angle < 360; angle += 45) {
            float ring_x = center_x + cosf(angle * 0.0174f) * r;
            float ring_y = center_y + sinf(angle * 0.0174f) * r;
            
            if (ring_x >= 0 && ring_x < width && ring_y >= 0 && ring_y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)ring_x, (int)ring_y, '@', 1.0f);
            }
        }
    }
}

void scene_reality_glitch(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float reality_stability = 1.0f - params[0].value;
    float glitch_frequency = params[1].value * 10.0f + 5.0f;
    float parallel_bleed = params[2].value;
    
    // Base reality - simple room
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height - 1, '_', 5.0f);
    }
    
    for (int y = height / 2; y < height; y++) {
        set_pixel(buffer, zbuffer, width, height, 0, y, '|', 5.0f);
        set_pixel(buffer, zbuffer, width, height, width - 1, y, '|', 5.0f);
    }
    
    // Reality tears
    for (int tear = 0; tear < (int)((1.0f - reality_stability) * 10.0f); tear++) {
        float tear_x = width * (tear + 1.0f) / 11.0f;
        float tear_y = height * 0.7f;
        float tear_height = 5.0f + sinf(time * 2.0f + tear) * 3.0f;
        
        for (int h = 0; h < (int)tear_height; h++) {
            int y = (int)(tear_y - tear_height / 2 + h);
            if (y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)tear_x - 1, y, '/', 2.0f);
                set_pixel(buffer, zbuffer, width, height, (int)tear_x + 1, y, '\\', 2.0f);
                set_pixel(buffer, zbuffer, width, height, (int)tear_x, y, ' ', 1.0f);
            }
        }
    }
    
    // Glitching objects
    for (int obj = 0; obj < 5; obj++) {
        float obj_x = width * (obj + 1.0f) / 6.0f;
        float obj_y = height * 0.8f;
        
        float glitch_phase = sinf(time * glitch_frequency + obj) * 0.5f + 0.5f;
        
        if (glitch_phase > 0.5f) {
            // Normal state - chair
            set_pixel(buffer, zbuffer, width, height, (int)obj_x - 1, (int)obj_y, '/', 3.0f);
            set_pixel(buffer, zbuffer, width, height, (int)obj_x, (int)obj_y, '_', 3.0f);
            set_pixel(buffer, zbuffer, width, height, (int)obj_x + 1, (int)obj_y, '\\', 3.0f);
        } else {
            // Glitched state
            char glitch_chars[] = "@#$%^&*";
            for (int dy = 0; dy < 3; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int x = (int)(obj_x + dx);
                    int y = (int)(obj_y + dy);
                    
                    if (x >= 0 && x < width && y < height) {
                        int char_idx = (int)(time * 10.0f + obj + dy + dx) % 7;
                        set_pixel(buffer, zbuffer, width, height, x, y, glitch_chars[char_idx], 2.5f);
                    }
                }
            }
        }
    }
    
    // Error messages
    char* errors[] = {"REALITY.EXE STOPPED", "PHYSICS VIOLATION"};
    
    for (int err = 0; err < 2; err++) {
        if ((int)(time * 1.5f + err) % 10 == 0) {
            float err_x = 5.0f + err * 30.0f;
            float err_y = height * 0.3f + sinf(time * 2.0f + err) * 5.0f;
            
            char* msg = errors[err];
            int msg_len = strlen(msg);
            
            for (int i = 0; i < msg_len && err_x + i < width; i++) {
                float flicker = sinf(time * glitch_frequency + i + err) * 0.5f + 0.5f;
                if (flicker > 0.4f) {
                    set_pixel(buffer, zbuffer, width, height, (int)(err_x + i), (int)err_y, msg[i], 0.5f);
                }
            }
        }
    }
}

// ============= HUMAN FIGURE STRUCTURE =============

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 pos;
    float radius;
} Joint;

typedef struct {
    Joint head;
    Joint neck;
    Joint torso;
    Joint hips;
    Joint left_shoulder, right_shoulder;
    Joint left_elbow, right_elbow;
    Joint left_wrist, right_wrist;
    Joint left_hip, right_hip;
    Joint left_knee, right_knee;
    Joint left_ankle, right_ankle;
    float anim_phase;
} HumanFigure;

// Helper function to draw a limb between two joints
void draw_limb(char* buffer, float* zbuffer, int width, int height, Vec3 start, Vec3 end, char symbol) {
    // Simple line drawing in 3D space
    float steps = 10.0f;
    for (float t = 0; t <= 1.0f; t += 1.0f / steps) {
        float x = start.x + (end.x - start.x) * t;
        float y = start.y + (end.y - start.y) * t;
        float z = start.z + (end.z - start.z) * t;
        
        // Project to 2D
        int px = (int)(width / 2 + x * 10.0f / (z + 5.0f));
        int py = (int)(height / 2 - y * 10.0f / (z + 5.0f));
        
        if (px >= 0 && px < width && py >= 0 && py < height) {
            set_pixel(buffer, zbuffer, width, height, px, py, symbol, z);
        }
    }
}

// Helper function to draw a joint
void draw_joint(char* buffer, float* zbuffer, int width, int height, Joint joint, char symbol) {
    // Project to 2D
    int px = (int)(width / 2 + joint.pos.x * 10.0f / (joint.pos.z + 5.0f));
    int py = (int)(height / 2 - joint.pos.y * 10.0f / (joint.pos.z + 5.0f));
    
    if (px >= 0 && px < width && py >= 0 && py < height) {
        set_pixel(buffer, zbuffer, width, height, px, py, symbol, joint.pos.z);
    }
}

// Initialize human figure in default pose
void init_human_figure(HumanFigure* human, float x, float y, float z) {
    // Head
    human->head.pos = (Vec3){x, y + 3.0f, z};
    human->head.radius = 0.5f;
    
    // Neck
    human->neck.pos = (Vec3){x, y + 2.0f, z};
    
    // Torso
    human->torso.pos = (Vec3){x, y + 0.5f, z};
    
    // Hips
    human->hips.pos = (Vec3){x, y - 0.5f, z};
    
    // Shoulders
    human->left_shoulder.pos = (Vec3){x - 0.8f, y + 1.5f, z};
    human->right_shoulder.pos = (Vec3){x + 0.8f, y + 1.5f, z};
    
    // Elbows
    human->left_elbow.pos = (Vec3){x - 1.2f, y + 0.5f, z};
    human->right_elbow.pos = (Vec3){x + 1.2f, y + 0.5f, z};
    
    // Wrists
    human->left_wrist.pos = (Vec3){x - 1.5f, y - 0.5f, z};
    human->right_wrist.pos = (Vec3){x + 1.5f, y - 0.5f, z};
    
    // Hip joints
    human->left_hip.pos = (Vec3){x - 0.5f, y - 1.0f, z};
    human->right_hip.pos = (Vec3){x + 0.5f, y - 1.0f, z};
    
    // Knees
    human->left_knee.pos = (Vec3){x - 0.5f, y - 2.5f, z};
    human->right_knee.pos = (Vec3){x + 0.5f, y - 2.5f, z};
    
    // Ankles
    human->left_ankle.pos = (Vec3){x - 0.5f, y - 4.0f, z};
    human->right_ankle.pos = (Vec3){x + 0.5f, y - 4.0f, z};
    
    human->anim_phase = 0.0f;
}

// Animate human walking
void animate_human_walk(HumanFigure* human, float phase) {
    float walk_cycle = sinf(phase);
    float walk_cycle2 = sinf(phase + M_PI);
    
    // Animate legs
    human->left_knee.pos.z += walk_cycle * 0.5f;
    human->left_ankle.pos.z += walk_cycle * 0.7f;
    human->right_knee.pos.z += walk_cycle2 * 0.5f;
    human->right_ankle.pos.z += walk_cycle2 * 0.7f;
    
    // Animate arms (opposite to legs)
    human->left_elbow.pos.z += walk_cycle2 * 0.3f;
    human->left_wrist.pos.z += walk_cycle2 * 0.4f;
    human->right_elbow.pos.z += walk_cycle * 0.3f;
    human->right_wrist.pos.z += walk_cycle * 0.4f;
    
    // Slight body sway
    human->torso.pos.x += sinf(phase * 2.0f) * 0.1f;
}

// Draw the human figure
void draw_human_figure(char* buffer, float* zbuffer, int width, int height, HumanFigure* human) {
    // Head (circle)
    for (int i = 0; i < 8; i++) {
        float angle = i * M_PI / 4.0f;
        float hx = human->head.pos.x + cosf(angle) * human->head.radius;
        float hy = human->head.pos.y + sinf(angle) * human->head.radius;
        int px = (int)(width / 2 + hx * 10.0f / (human->head.pos.z + 5.0f));
        int py = (int)(height / 2 - hy * 10.0f / (human->head.pos.z + 5.0f));
        if (px >= 0 && px < width && py >= 0 && py < height) {
            set_pixel(buffer, zbuffer, width, height, px, py, 'O', human->head.pos.z);
        }
    }
    
    // Spine
    draw_limb(buffer, zbuffer, width, height, human->neck.pos, human->torso.pos, '|');
    draw_limb(buffer, zbuffer, width, height, human->torso.pos, human->hips.pos, '|');
    
    // Arms
    draw_limb(buffer, zbuffer, width, height, human->left_shoulder.pos, human->left_elbow.pos, '/');
    draw_limb(buffer, zbuffer, width, height, human->left_elbow.pos, human->left_wrist.pos, '/');
    draw_limb(buffer, zbuffer, width, height, human->right_shoulder.pos, human->right_elbow.pos, '\\');
    draw_limb(buffer, zbuffer, width, height, human->right_elbow.pos, human->right_wrist.pos, '\\');
    
    // Connect shoulders to neck
    draw_limb(buffer, zbuffer, width, height, human->neck.pos, human->left_shoulder.pos, '-');
    draw_limb(buffer, zbuffer, width, height, human->neck.pos, human->right_shoulder.pos, '-');
    
    // Legs
    draw_limb(buffer, zbuffer, width, height, human->hips.pos, human->left_hip.pos, '/');
    draw_limb(buffer, zbuffer, width, height, human->hips.pos, human->right_hip.pos, '\\');
    draw_limb(buffer, zbuffer, width, height, human->left_hip.pos, human->left_knee.pos, '|');
    draw_limb(buffer, zbuffer, width, height, human->left_knee.pos, human->left_ankle.pos, '|');
    draw_limb(buffer, zbuffer, width, height, human->right_hip.pos, human->right_knee.pos, '|');
    draw_limb(buffer, zbuffer, width, height, human->right_knee.pos, human->right_ankle.pos, '|');
    
    // Draw joints
    draw_joint(buffer, zbuffer, width, height, human->neck, '*');
    draw_joint(buffer, zbuffer, width, height, human->left_shoulder, '+');
    draw_joint(buffer, zbuffer, width, height, human->right_shoulder, '+');
    draw_joint(buffer, zbuffer, width, height, human->left_elbow, 'o');
    draw_joint(buffer, zbuffer, width, height, human->right_elbow, 'o');
    draw_joint(buffer, zbuffer, width, height, human->hips, '#');
}

// ============= WARFARE AIRCRAFT SYSTEM =============

typedef struct {
    Vec3 pos;
    Vec3 velocity;
    float heading;
    float bank_angle;
    int aircraft_type; // 0=fighter, 1=bomber, 2=drone, 3=helicopter
    bool is_active;
    float health;
    Vec3 target_pos;
    float attack_cooldown;
} Aircraft;

typedef struct {
    Vec3 pos;
    float size;
    bool is_destroyed;
    int building_type; // 0=military, 1=civilian, 2=factory, 3=airfield
    float damage_timer;
} StrategicTarget;

typedef struct {
    Vec3 start_pos;
    Vec3 end_pos;
    float progress;
    int missile_type; // 0=air-to-air, 1=air-to-ground, 2=cruise
    bool is_active;
    float trail_length;
} Missile;

// Draw aircraft with different shapes based on type
void draw_aircraft(char* buffer, float* zbuffer, int width, int height, Aircraft* aircraft) {
    if (!aircraft->is_active) return;
    
    // Project to 2D
    int x = (int)(width / 2 + aircraft->pos.x * 10.0f / (aircraft->pos.z + 5.0f));
    int y = (int)(height / 2 - aircraft->pos.y * 10.0f / (aircraft->pos.z + 5.0f));
    
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    
    char symbol;
    switch (aircraft->aircraft_type) {
        case 0: symbol = '^'; break; // Fighter
        case 1: symbol = '#'; break; // Bomber
        case 2: symbol = '*'; break; // Drone
        case 3: symbol = 'H'; break; // Helicopter
        default: symbol = '+'; break;
    }
    
    set_pixel(buffer, zbuffer, width, height, x, y, symbol, aircraft->pos.z);
    
    // Draw wings for larger aircraft
    if (aircraft->aircraft_type <= 1) {
        set_pixel(buffer, zbuffer, width, height, x-1, y, '-', aircraft->pos.z + 0.1f);
        set_pixel(buffer, zbuffer, width, height, x+1, y, '-', aircraft->pos.z + 0.1f);
    }
}

// Draw strategic target on map
void draw_strategic_target(char* buffer, float* zbuffer, int width, int height, StrategicTarget* target) {
    int x = (int)(width / 2 + target->pos.x * 10.0f / (target->pos.z + 5.0f));
    int y = (int)(height / 2 - target->pos.y * 10.0f / (target->pos.z + 5.0f));
    
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    
    if (target->is_destroyed) {
        // Show explosion/damage
        char damage_chars[] = "X%@#*";
        int char_idx = (int)(target->damage_timer * 5) % 5;
        set_pixel(buffer, zbuffer, width, height, x, y, damage_chars[char_idx], target->pos.z);
        
        // Scatter effect
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue;
                if (rand() % 3 == 0) {
                    set_pixel(buffer, zbuffer, width, height, x + dx, y + dy, '.', target->pos.z + 0.1f);
                }
            }
        }
    } else {
        // Show intact building
        char building_chars[] = {'M', 'C', 'F', 'A'}; // Military, Civilian, Factory, Airfield
        set_pixel(buffer, zbuffer, width, height, x, y, building_chars[target->building_type], target->pos.z);
        
        // Add structure around building
        set_pixel(buffer, zbuffer, width, height, x-1, y+1, '_', target->pos.z + 0.1f);
        set_pixel(buffer, zbuffer, width, height, x, y+1, '_', target->pos.z + 0.1f);
        set_pixel(buffer, zbuffer, width, height, x+1, y+1, '_', target->pos.z + 0.1f);
    }
}

// Draw missile with trail
void draw_missile(char* buffer, float* zbuffer, int width, int height, Missile* missile) {
    if (!missile->is_active) return;
    
    // Calculate current position
    Vec3 current_pos;
    current_pos.x = missile->start_pos.x + (missile->end_pos.x - missile->start_pos.x) * missile->progress;
    current_pos.y = missile->start_pos.y + (missile->end_pos.y - missile->start_pos.y) * missile->progress;
    current_pos.z = missile->start_pos.z + (missile->end_pos.z - missile->start_pos.z) * missile->progress;
    
    // Project to 2D
    int x = (int)(width / 2 + current_pos.x * 10.0f / (current_pos.z + 5.0f));
    int y = (int)(height / 2 - current_pos.y * 10.0f / (current_pos.z + 5.0f));
    
    if (x >= 0 && x < width && y >= 0 && y < height) {
        set_pixel(buffer, zbuffer, width, height, x, y, 'o', current_pos.z);
        
        // Draw trail
        for (int i = 1; i <= (int)missile->trail_length; i++) {
            float trail_progress = missile->progress - i * 0.05f;
            if (trail_progress < 0) break;
            
            Vec3 trail_pos;
            trail_pos.x = missile->start_pos.x + (missile->end_pos.x - missile->start_pos.x) * trail_progress;
            trail_pos.y = missile->start_pos.y + (missile->end_pos.y - missile->start_pos.y) * trail_progress;
            trail_pos.z = missile->start_pos.z + (missile->end_pos.z - missile->start_pos.z) * trail_progress;
            
            int tx = (int)(width / 2 + trail_pos.x * 10.0f / (trail_pos.z + 5.0f));
            int ty = (int)(height / 2 - trail_pos.y * 10.0f / (trail_pos.z + 5.0f));
            
            if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
                char trail_chars[] = ".:*+";
                int trail_char = (i - 1) % 4;
                set_pixel(buffer, zbuffer, width, height, tx, ty, trail_chars[trail_char], trail_pos.z + 0.1f);
            }
        }
    }
}

// Draw radar screen overlay
void draw_radar_overlay(char* buffer, float* zbuffer, int width, int height, float time, float radar_range) {
    int radar_x = width - 20;
    int radar_y = 5;
    int radar_size = 15;
    
    // Radar circle
    for (int angle = 0; angle < 360; angle += 10) {
        float rad = angle * M_PI / 180.0f;
        int x = radar_x + (int)(cosf(rad) * radar_size);
        int y = radar_y + (int)(sinf(rad) * radar_size);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            set_pixel(buffer, zbuffer, width, height, x, y, '.', 0.0f);
        }
    }
    
    // Radar sweep
    float sweep_angle = fmodf(time * 180.0f, 360.0f) * M_PI / 180.0f;
    for (int r = 0; r < radar_size; r++) {
        int x = radar_x + (int)(cosf(sweep_angle) * r);
        int y = radar_y + (int)(sinf(sweep_angle) * r);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            set_pixel(buffer, zbuffer, width, height, x, y, '|', 0.0f);
        }
    }
    
    // Radar center
    set_pixel(buffer, zbuffer, width, height, radar_x, radar_y, '+', 0.0f);
}

// ============= HUMAN-BASED SCENES (100-109) =============

// Scene 100: Human Walker - Single figure walking
void scene_human_walker(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float walk_speed = params[0].value * 5.0f + 1.0f;
    float stride_length = params[1].value * 2.0f + 0.5f;
    float camera_follow = params[2].value;
    
    HumanFigure human;
    float walk_phase = time * walk_speed;
    float x_pos = sinf(time * 0.3f) * stride_length * 10.0f;
    
    init_human_figure(&human, x_pos, 0.0f, 0.0f);
    animate_human_walk(&human, walk_phase);
    
    // Camera follow effect
    if (camera_follow > 0.5f) {
        human.head.pos.x -= x_pos * 0.8f;
        human.neck.pos.x -= x_pos * 0.8f;
        human.torso.pos.x -= x_pos * 0.8f;
        human.hips.pos.x -= x_pos * 0.8f;
        human.left_shoulder.pos.x -= x_pos * 0.8f;
        human.right_shoulder.pos.x -= x_pos * 0.8f;
        human.left_elbow.pos.x -= x_pos * 0.8f;
        human.right_elbow.pos.x -= x_pos * 0.8f;
        human.left_wrist.pos.x -= x_pos * 0.8f;
        human.right_wrist.pos.x -= x_pos * 0.8f;
        human.left_hip.pos.x -= x_pos * 0.8f;
        human.right_hip.pos.x -= x_pos * 0.8f;
        human.left_knee.pos.x -= x_pos * 0.8f;
        human.right_knee.pos.x -= x_pos * 0.8f;
        human.left_ankle.pos.x -= x_pos * 0.8f;
        human.right_ankle.pos.x -= x_pos * 0.8f;
    }
    
    draw_human_figure(buffer, zbuffer, width, height, &human);
    
    // Ground line
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4, '_', 10.0f);
    }
}

// Scene 101: Dance Party - Multiple figures dancing
void scene_dance_party(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    int num_dancers = (int)(params[0].value * 5.0f) + 1;
    float dance_energy = params[1].value * 2.0f + 0.5f;
    float sync_factor = params[2].value;
    
    for (int i = 0; i < num_dancers; i++) {
        HumanFigure dancer;
        float x_offset = (i - num_dancers / 2.0f) * 4.0f;
        float phase_offset = sync_factor < 0.5f ? i * 0.5f : 0.0f;
        
        init_human_figure(&dancer, x_offset, 0.0f, -2.0f + i * 0.5f);
        
        // Dance moves
        float dance_phase = time * 4.0f + phase_offset;
        
        // Arms up and down
        dancer.left_wrist.pos.y += sinf(dance_phase) * dance_energy * 2.0f;
        dancer.right_wrist.pos.y += sinf(dance_phase + M_PI) * dance_energy * 2.0f;
        dancer.left_elbow.pos.y += sinf(dance_phase) * dance_energy;
        dancer.right_elbow.pos.y += sinf(dance_phase + M_PI) * dance_energy;
        
        // Body sway
        dancer.torso.pos.x += sinf(dance_phase * 0.5f) * dance_energy * 0.5f;
        dancer.hips.pos.x += sinf(dance_phase * 0.5f + 0.2f) * dance_energy * 0.7f;
        
        // Head bob
        dancer.head.pos.y += sinf(dance_phase * 2.0f) * dance_energy * 0.3f;
        
        // Knee bends
        dancer.left_knee.pos.y += sinf(dance_phase * 2.0f) * dance_energy * 0.3f;
        dancer.right_knee.pos.y += sinf(dance_phase * 2.0f + M_PI) * dance_energy * 0.3f;
        
        draw_human_figure(buffer, zbuffer, width, height, &dancer);
    }
    
    // Dance floor
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4, '=', 10.0f);
        if (x % 4 == 0) {
            set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4 + 1, '|', 10.0f);
        }
    }
}

// Scene 102: Martial Arts - Figure performing martial arts moves
void scene_martial_arts(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float move_speed = params[0].value * 3.0f + 0.5f;
    float kick_height = params[1].value * 2.0f + 1.0f;
    float stance_width = params[2].value * 2.0f + 1.0f;
    
    HumanFigure fighter;
    init_human_figure(&fighter, 0.0f, 0.0f, 0.0f);
    
    float move_phase = time * move_speed;
    int move_type = ((int)(move_phase / (M_PI * 2))) % 4;
    float local_phase = fmodf(move_phase, M_PI * 2);
    
    switch (move_type) {
        case 0: // Punch
            fighter.right_wrist.pos.z -= sinf(local_phase * 2.0f) * 3.0f;
            fighter.right_wrist.pos.x += sinf(local_phase * 2.0f) * 2.0f;
            fighter.right_elbow.pos.z -= sinf(local_phase * 2.0f) * 1.5f;
            break;
            
        case 1: // High kick
            fighter.right_ankle.pos.y += sinf(local_phase) * kick_height * 3.0f;
            fighter.right_ankle.pos.z -= sinf(local_phase) * 2.0f;
            fighter.right_knee.pos.y += sinf(local_phase) * kick_height * 1.5f;
            break;
            
        case 2: // Spinning move
            {
                float spin = local_phase * 2.0f;
                float cs = cosf(spin);
                float sn = sinf(spin);
                
                // Rotate all points around center
                Vec3* points[] = {
                    &fighter.head.pos, &fighter.neck.pos, &fighter.torso.pos,
                    &fighter.left_shoulder.pos, &fighter.right_shoulder.pos,
                    &fighter.left_elbow.pos, &fighter.right_elbow.pos,
                    &fighter.left_wrist.pos, &fighter.right_wrist.pos
                };
                
                for (int i = 0; i < 9; i++) {
                    float x = points[i]->x;
                    float z = points[i]->z;
                    points[i]->x = x * cs - z * sn;
                    points[i]->z = x * sn + z * cs;
                }
            }
            break;
            
        case 3: // Wide stance
            fighter.left_ankle.pos.x -= stance_width;
            fighter.right_ankle.pos.x += stance_width;
            fighter.left_knee.pos.x -= stance_width * 0.8f;
            fighter.right_knee.pos.x += stance_width * 0.8f;
            fighter.left_hip.pos.x -= stance_width * 0.5f;
            fighter.right_hip.pos.x += stance_width * 0.5f;
            fighter.torso.pos.y -= 0.5f;
            fighter.hips.pos.y -= 0.5f;
            break;
    }
    
    draw_human_figure(buffer, zbuffer, width, height, &fighter);
    
    // Training mat
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4, '-', 10.0f);
    }
}

// Scene 103: Human Pyramid - Multiple figures forming a pyramid
void scene_human_pyramid(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    int pyramid_size = (int)(params[0].value * 3.0f) + 2;
    float stability = params[1].value;
    float wobble = (1.0f - stability) * sinf(time * 3.0f) * 0.5f;
    
    // Draw pyramid from bottom to top
    for (int row = 0; row < pyramid_size; row++) {
        int figures_in_row = pyramid_size - row;
        float y_offset = row * 3.5f;
        
        for (int col = 0; col < figures_in_row; col++) {
            HumanFigure human;
            float x_offset = (col - figures_in_row / 2.0f) * 3.0f + wobble * row;
            
            init_human_figure(&human, x_offset, y_offset - 2.0f, -row * 0.5f);
            
            // Adjust pose for stability
            if (row > 0) {
                // Figures on higher rows crouch
                human.torso.pos.y -= 0.5f;
                human.head.pos.y -= 0.5f;
                human.neck.pos.y -= 0.5f;
                human.left_knee.pos.y += 1.0f;
                human.right_knee.pos.y += 1.0f;
            }
            
            // Arms out for balance
            human.left_wrist.pos.x -= 1.0f;
            human.right_wrist.pos.x += 1.0f;
            human.left_wrist.pos.y += 0.5f;
            human.right_wrist.pos.y += 0.5f;
            
            draw_human_figure(buffer, zbuffer, width, height, &human);
        }
    }
    
    // Ground
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4, '=', 10.0f);
    }
}

// Scene 104: Yoga Flow - Figure transitioning between yoga poses
void scene_yoga_flow(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float flow_speed = params[0].value * 2.0f + 0.3f;
    float flexibility = params[1].value * 2.0f + 0.5f;
    float balance = params[2].value;
    
    HumanFigure yogi;
    init_human_figure(&yogi, 0.0f, 0.0f, 0.0f);
    
    float pose_phase = time * flow_speed;
    int pose_type = ((int)(pose_phase / (M_PI * 2))) % 5;
    float local_phase = fmodf(pose_phase, M_PI * 2) / (M_PI * 2);
    
    switch (pose_type) {
        case 0: // Tree pose
            yogi.left_knee.pos.x = yogi.right_knee.pos.x - 0.5f;
            yogi.left_ankle.pos.x = yogi.right_ankle.pos.x;
            yogi.left_ankle.pos.y = yogi.right_knee.pos.y;
            yogi.left_wrist.pos.y += 4.0f * flexibility;
            yogi.right_wrist.pos.y += 4.0f * flexibility;
            yogi.left_wrist.pos.x = 0.0f;
            yogi.right_wrist.pos.x = 0.0f;
            break;
            
        case 1: // Warrior pose
            yogi.left_ankle.pos.x -= 3.0f * flexibility;
            yogi.right_ankle.pos.x += 1.0f;
            yogi.left_knee.pos.x -= 2.0f * flexibility;
            yogi.left_wrist.pos.x -= 3.0f;
            yogi.right_wrist.pos.x += 3.0f;
            yogi.left_wrist.pos.y += 1.0f;
            yogi.right_wrist.pos.y += 1.0f;
            yogi.torso.pos.x -= 0.5f;
            break;
            
        case 2: // Downward dog
            yogi.hips.pos.y += 3.0f * flexibility;
            yogi.torso.pos.y += 2.0f * flexibility;
            yogi.head.pos.y -= 1.0f;
            yogi.left_wrist.pos.y -= 3.0f;
            yogi.right_wrist.pos.y -= 3.0f;
            yogi.left_wrist.pos.z -= 2.0f;
            yogi.right_wrist.pos.z -= 2.0f;
            break;
            
        case 3: // Cobra pose
            yogi.torso.pos.z = -2.0f;
            yogi.hips.pos.z = -2.0f;
            yogi.head.pos.y += 1.0f * flexibility;
            yogi.torso.pos.y += 0.5f;
            yogi.left_wrist.pos.z = -1.0f;
            yogi.right_wrist.pos.z = -1.0f;
            yogi.left_wrist.pos.y = -2.0f;
            yogi.right_wrist.pos.y = -2.0f;
            break;
            
        case 4: // Mountain pose (standing)
            // Default position with arms raised
            yogi.left_wrist.pos.y += 3.0f;
            yogi.right_wrist.pos.y += 3.0f;
            break;
    }
    
    // Add balance wobble
    if (balance < 0.5f) {
        float wobble_amount = (0.5f - balance) * 2.0f;
        yogi.head.pos.x += sinf(time * 5.0f) * wobble_amount * 0.2f;
        yogi.torso.pos.x += sinf(time * 5.0f + 0.1f) * wobble_amount * 0.3f;
    }
    
    draw_human_figure(buffer, zbuffer, width, height, &yogi);
    
    // Yoga mat
    for (int y = height * 3 / 4 - 2; y < height * 3 / 4 + 2; y++) {
        for (int x = width / 2 - 15; x < width / 2 + 15; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 10.0f);
            }
        }
    }
}

// Scene 105: Sports Stadium - Multiple figures playing sports
void scene_sports_stadium(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float game_speed = params[0].value * 3.0f + 1.0f;
    int num_players = (int)(params[1].value * 4.0f) + 2;
    float ball_height = params[2].value * 5.0f + 2.0f;
    
    // Draw field lines
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4, '-', 10.0f);
        if (x == width / 4 || x == width * 3 / 4) {
            for (int y = height * 3 / 4 - 5; y < height * 3 / 4; y++) {
                set_pixel(buffer, zbuffer, width, height, x, y, '|', 10.0f);
            }
        }
    }
    
    // Draw players
    for (int i = 0; i < num_players; i++) {
        HumanFigure player;
        float x_pos = (i - num_players / 2.0f) * 6.0f;
        float run_phase = time * game_speed + i * 0.5f;
        
        init_human_figure(&player, x_pos, 0.0f, -1.0f);
        
        // Running animation
        animate_human_walk(&player, run_phase * 2.0f);
        
        // Jumping for ball
        if (i == (int)(time) % num_players) {
            player.head.pos.y += 2.0f;
            player.neck.pos.y += 2.0f;
            player.torso.pos.y += 2.0f;
            player.hips.pos.y += 2.0f;
            player.left_hip.pos.y += 2.0f;
            player.right_hip.pos.y += 2.0f;
            player.left_knee.pos.y += 2.0f;
            player.right_knee.pos.y += 2.0f;
            player.left_ankle.pos.y += 2.0f;
            player.right_ankle.pos.y += 2.0f;
            player.left_shoulder.pos.y += 2.0f;
            player.right_shoulder.pos.y += 2.0f;
            player.left_elbow.pos.y += 2.0f;
            player.right_elbow.pos.y += 2.0f;
            player.left_wrist.pos.y += 3.0f;
            player.right_wrist.pos.y += 3.0f;
        }
        
        draw_human_figure(buffer, zbuffer, width, height, &player);
    }
    
    // Ball
    float ball_x = sinf(time * game_speed) * width * 0.3f;
    float ball_y = height / 2 - ball_height - fabsf(sinf(time * game_speed * 2.0f)) * ball_height;
    int bx = (int)(width / 2 + ball_x);
    int by = (int)ball_y;
    if (bx >= 0 && bx < width && by >= 0 && by < height) {
        set_pixel(buffer, zbuffer, width, height, bx, by, 'O', 0.0f);
    }
}

// Scene 106: Robot Dance - Mechanical human movements
void scene_robot_dance(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float robot_speed = params[0].value * 4.0f + 1.0f;
    float stiffness = params[1].value;
    int num_robots = (int)(params[2].value * 3.0f) + 1;
    
    for (int i = 0; i < num_robots; i++) {
        HumanFigure robot;
        float x_offset = (i - num_robots / 2.0f) * 5.0f;
        
        init_human_figure(&robot, x_offset, 0.0f, -i * 0.5f);
        
        // Robotic movements - sharp, angular
        float move_phase = time * robot_speed;
        int move_step = (int)(move_phase) % 8;
        
        switch (move_step) {
            case 0: // Arms up
                robot.left_wrist.pos.y += 3.0f;
                robot.right_wrist.pos.y += 3.0f;
                robot.left_elbow.pos.y += 1.5f;
                robot.right_elbow.pos.y += 1.5f;
                break;
                
            case 1: // Arms out
                robot.left_wrist.pos.x -= 3.0f;
                robot.right_wrist.pos.x += 3.0f;
                break;
                
            case 2: // Turn head
                robot.head.pos.x += 0.5f;
                break;
                
            case 3: // Bend knees
                robot.left_knee.pos.y += 0.5f;
                robot.right_knee.pos.y += 0.5f;
                robot.torso.pos.y -= 0.5f;
                robot.hips.pos.y -= 0.5f;
                break;
                
            case 4: // Arms down
                robot.left_wrist.pos.y -= 2.0f;
                robot.right_wrist.pos.y -= 2.0f;
                break;
                
            case 5: // Step right
                robot.right_ankle.pos.x += 1.0f;
                robot.right_knee.pos.x += 0.5f;
                break;
                
            case 6: // Step left
                robot.left_ankle.pos.x -= 1.0f;
                robot.left_knee.pos.x -= 0.5f;
                break;
                
            case 7: // Reset position
                break;
        }
        
        // Add mechanical stiffness
        if (stiffness > 0.5f) {
            // No interpolation between moves
        } else {
            // Smooth interpolation
            float interp = fmodf(move_phase, 1.0f);
            robot.head.pos.y += sinf(interp * M_PI) * 0.1f;
        }
        
        draw_human_figure(buffer, zbuffer, width, height, &robot);
    }
    
    // Factory floor
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4, '=', 10.0f);
        if (x % 8 == 0) {
            set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4 + 1, '+', 10.0f);
        }
    }
}

// Scene 107: Crowd Wave - Stadium crowd doing the wave
void scene_crowd_wave(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float wave_speed = params[0].value * 3.0f + 0.5f;
    float wave_height = params[1].value * 2.0f + 0.5f;
    int crowd_density = (int)(params[2].value * 20.0f) + 10;
    
    // Draw multiple rows of crowd
    for (int row = 0; row < 5; row++) {
        float z_offset = -row * 2.0f;
        float y_base = -row * 2.0f;
        
        for (int i = 0; i < crowd_density; i++) {
            HumanFigure person;
            float x_pos = (i - crowd_density / 2.0f) * 2.0f;
            
            init_human_figure(&person, x_pos, y_base, z_offset);
            
            // Wave motion
            float wave_phase = time * wave_speed - i * 0.3f;
            float wave_amount = sinf(wave_phase);
            
            if (wave_amount > 0.0f) {
                // Arms up for the wave
                person.left_wrist.pos.y += wave_amount * wave_height * 3.0f;
                person.right_wrist.pos.y += wave_amount * wave_height * 3.0f;
                person.left_elbow.pos.y += wave_amount * wave_height * 1.5f;
                person.right_elbow.pos.y += wave_amount * wave_height * 1.5f;
                
                // Slight jump
                person.head.pos.y += wave_amount * wave_height * 0.5f;
                person.torso.pos.y += wave_amount * wave_height * 0.5f;
                person.hips.pos.y += wave_amount * wave_height * 0.5f;
            }
            
            // Simplify figure for distant rows
            if (row > 2) {
                // Just draw torso and head for far rows
                draw_joint(buffer, zbuffer, width, height, person.head, 'o');
                draw_joint(buffer, zbuffer, width, height, person.torso, '|');
            } else {
                draw_human_figure(buffer, zbuffer, width, height, &person);
            }
        }
    }
    
    // Stadium
    for (int y = height - 5; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (y == height - 5 || x == 0 || x == width - 1) {
                set_pixel(buffer, zbuffer, width, height, x, y, '#', 15.0f);
            }
        }
    }
}

// Scene 108: Mirror Dance - Figure dancing with mirror reflection
void scene_mirror_dance(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float dance_complexity = params[0].value * 3.0f + 1.0f;
    float mirror_delay = params[1].value * 0.5f;
    float mirror_distance = params[2].value * 5.0f + 3.0f;
    
    // Original dancer
    HumanFigure dancer1;
    init_human_figure(&dancer1, -mirror_distance / 2, 0.0f, 0.0f);
    
    // Mirror reflection
    HumanFigure dancer2;
    init_human_figure(&dancer2, mirror_distance / 2, 0.0f, 0.0f);
    
    // Complex dance moves
    float dance_phase = time * dance_complexity;
    
    // Original dancer movements
    dancer1.left_wrist.pos.x -= sinf(dance_phase) * 2.0f;
    dancer1.left_wrist.pos.y += cosf(dance_phase) * 3.0f;
    dancer1.right_wrist.pos.x += cosf(dance_phase * 1.3f) * 2.0f;
    dancer1.right_wrist.pos.y += sinf(dance_phase * 1.3f) * 3.0f;
    
    dancer1.left_elbow.pos.x -= sinf(dance_phase) * 1.0f;
    dancer1.left_elbow.pos.y += cosf(dance_phase) * 1.5f;
    dancer1.right_elbow.pos.x += cosf(dance_phase * 1.3f) * 1.0f;
    dancer1.right_elbow.pos.y += sinf(dance_phase * 1.3f) * 1.5f;
    
    dancer1.torso.pos.x += sinf(dance_phase * 0.5f) * 0.5f;
    dancer1.hips.pos.x += sinf(dance_phase * 0.5f + 0.2f) * 0.7f;
    dancer1.head.pos.y += sinf(dance_phase * 2.0f) * 0.5f;
    
    // Leg movements
    dancer1.left_knee.pos.x -= sinf(dance_phase * 2.0f) * 0.5f;
    dancer1.right_knee.pos.x += sinf(dance_phase * 2.0f + M_PI) * 0.5f;
    
    // Mirror movements (with delay and X-flip)
    float mirror_phase = (time - mirror_delay) * dance_complexity;
    
    dancer2.left_wrist.pos.x += sinf(mirror_phase) * 2.0f;  // Flipped
    dancer2.left_wrist.pos.y += cosf(mirror_phase) * 3.0f;
    dancer2.right_wrist.pos.x -= cosf(mirror_phase * 1.3f) * 2.0f;  // Flipped
    dancer2.right_wrist.pos.y += sinf(mirror_phase * 1.3f) * 3.0f;
    
    dancer2.left_elbow.pos.x += sinf(mirror_phase) * 1.0f;  // Flipped
    dancer2.left_elbow.pos.y += cosf(mirror_phase) * 1.5f;
    dancer2.right_elbow.pos.x -= cosf(mirror_phase * 1.3f) * 1.0f;  // Flipped
    dancer2.right_elbow.pos.y += sinf(mirror_phase * 1.3f) * 1.5f;
    
    dancer2.torso.pos.x -= sinf(mirror_phase * 0.5f) * 0.5f;  // Flipped
    dancer2.hips.pos.x -= sinf(mirror_phase * 0.5f + 0.2f) * 0.7f;  // Flipped
    dancer2.head.pos.y += sinf(mirror_phase * 2.0f) * 0.5f;
    
    dancer2.left_knee.pos.x += sinf(mirror_phase * 2.0f) * 0.5f;  // Flipped
    dancer2.right_knee.pos.x -= sinf(mirror_phase * 2.0f + M_PI) * 0.5f;  // Flipped
    
    draw_human_figure(buffer, zbuffer, width, height, &dancer1);
    draw_human_figure(buffer, zbuffer, width, height, &dancer2);
    
    // Mirror frame
    int mirror_x = width / 2;
    for (int y = height / 4; y < height * 3 / 4; y++) {
        set_pixel(buffer, zbuffer, width, height, mirror_x, y, '|', 5.0f);
    }
    for (int x = mirror_x - 1; x <= mirror_x + 1; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height / 4 - 1, '-', 5.0f);
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4, '-', 5.0f);
    }
}

// Scene 109: Evolution - Figure evolving from primitive to modern
void scene_human_evolution(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float evolution_speed = params[0].value * 2.0f + 0.5f;
    int num_stages = (int)(params[1].value * 4.0f) + 3;
    float morph_smoothness = params[2].value;
    
    float evolution_phase = fmodf(time * evolution_speed, (float)num_stages);
    int current_stage = (int)evolution_phase;
    float morph_factor = morph_smoothness > 0.5f ? evolution_phase - current_stage : 0.0f;
    
    for (int i = 0; i < num_stages; i++) {
        HumanFigure human;
        float x_pos = (i - num_stages / 2.0f) * 6.0f;
        
        init_human_figure(&human, x_pos, 0.0f, -i * 0.5f);
        
        // Modify based on evolution stage
        float stage_factor = (float)i / (num_stages - 1);
        
        // Posture changes
        float hunch_factor = 1.0f - stage_factor;
        human.head.pos.y -= hunch_factor * 1.0f;
        human.head.pos.z -= hunch_factor * 1.0f;
        human.neck.pos.y -= hunch_factor * 1.2f;
        human.neck.pos.z -= hunch_factor * 1.0f;
        human.torso.pos.y -= hunch_factor * 0.8f;
        human.torso.pos.z -= hunch_factor * 0.5f;
        
        // Arms getting shorter and more upright
        human.left_wrist.pos.y -= hunch_factor * 2.0f;
        human.right_wrist.pos.y -= hunch_factor * 2.0f;
        human.left_wrist.pos.z -= hunch_factor * 1.0f;
        human.right_wrist.pos.z -= hunch_factor * 1.0f;
        
        // Leg straightening
        human.left_knee.pos.z -= hunch_factor * 0.5f;
        human.right_knee.pos.z -= hunch_factor * 0.5f;
        
        // Highlight current evolution stage
        float highlight = 1.0f;
        if (i == current_stage) {
            highlight = 1.0f + morph_factor;
            
            // Add glow effect
            human.head.pos.y += sinf(time * 10.0f) * 0.1f * highlight;
        } else if (i == (current_stage + 1) % num_stages && morph_smoothness > 0.5f) {
            highlight = morph_factor;
        } else {
            highlight = 0.5f;
        }
        
        // Scale based on highlight
        if (highlight > 0.5f) {
            draw_human_figure(buffer, zbuffer, width, height, &human);
        } else {
            // Draw dimmed version
            draw_joint(buffer, zbuffer, width, height, human.head, '.');
            draw_joint(buffer, zbuffer, width, height, human.torso, '.');
        }
    }
    
    // Timeline
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, height * 3 / 4 + 2, '-', 10.0f);
    }
    
    // Era markers
    for (int i = 0; i < num_stages; i++) {
        int marker_x = width / 2 + (i - num_stages / 2.0f) * 12;
        if (marker_x >= 0 && marker_x < width) {
            set_pixel(buffer, zbuffer, width, height, marker_x, height * 3 / 4 + 3, '|', 10.0f);
        }
    }
}

// ============= WARFARE SCENES (110-119) =============

// Scene 110: Fighter Squadron - Multiple fighter jets in formation
void scene_fighter_squadron(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    int squadron_size = (int)(params[0].value * 8.0f) + 3;
    float formation_tightness = params[1].value;
    float combat_speed = params[2].value * 3.0f + 1.0f;
    
    // Draw sky gradient
    for (int y = 0; y < height / 3; y++) {
        for (int x = 0; x < width; x++) {
            if (y % 3 == 0) {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 20.0f);
            }
        }
    }
    
    // Draw fighter formation
    for (int i = 0; i < squadron_size; i++) {
        Aircraft fighter;
        
        // Formation positioning
        float formation_x = (i % 3 - 1) * (3.0f + formation_tightness * 2.0f);
        float formation_y = (i / 3) * 2.0f;
        float formation_z = sinf(time * combat_speed + i * 0.5f) * 3.0f;
        
        fighter.pos.x = formation_x + sinf(time * 0.3f) * 10.0f;
        fighter.pos.y = formation_y;
        fighter.pos.z = formation_z;
        fighter.aircraft_type = 0; // Fighter
        fighter.is_active = true;
        
        draw_aircraft(buffer, zbuffer, width, height, &fighter);
        
        // Contrails
        for (int trail = 0; trail < 20; trail++) {
            float trail_x = fighter.pos.x - trail * 0.3f;
            float trail_y = fighter.pos.y;
            float trail_z = fighter.pos.z + trail * 0.1f;
            
            int tx = (int)(width / 2 + trail_x * 10.0f / (trail_z + 5.0f));
            int ty = (int)(height / 2 - trail_y * 10.0f / (trail_z + 5.0f));
            
            if (tx >= 0 && tx < width && ty >= 0 && ty < height && trail < 15) {
                char trail_char = (trail < 5) ? '.' : (trail < 10) ? ':' : ' ';
                set_pixel(buffer, zbuffer, width, height, tx, ty, trail_char, trail_z + 0.1f);
            }
        }
    }
    
    // Radar overlay
    draw_radar_overlay(buffer, zbuffer, width, height, time, 100.0f);
    
    // Mission info overlay
    char mission_text[] = "SQUADRON PATROL";
    for (int i = 0; i < strlen(mission_text) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, 2 + i, 2, mission_text[i], 0.0f);
    }
}

// Scene 111: Drone Swarm Attack - Multiple small drones
void scene_drone_swarm(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    int swarm_size = (int)(params[0].value * 20.0f) + 10;
    float swarm_aggression = params[1].value;
    float target_density = params[2].value * 5.0f + 2.0f;
    
    // Draw ground targets
    for (int t = 0; t < (int)target_density; t++) {
        StrategicTarget target;
        target.pos.x = (t - target_density / 2) * 8.0f + sinf(t * 2.0f) * 3.0f;
        target.pos.y = -5.0f;
        target.pos.z = 5.0f + t * 2.0f;
        target.building_type = t % 4;
        target.is_destroyed = (sinf(time + t) * swarm_aggression > 0.3f);
        target.damage_timer = time + t;
        
        draw_strategic_target(buffer, zbuffer, width, height, &target);
    }
    
    // Draw drone swarm
    for (int i = 0; i < swarm_size; i++) {
        Aircraft drone;
        
        // Swarm behavior - follow other drones but spread out
        float swarm_phase = time * 2.0f + i * 0.2f;
        float neighbor_influence = sinf(swarm_phase) * 2.0f;
        
        drone.pos.x = sinf(swarm_phase * 0.7f + i) * 8.0f + neighbor_influence;
        drone.pos.y = cosf(swarm_phase * 0.5f + i) * 3.0f + 2.0f;
        drone.pos.z = sinf(swarm_phase * 0.3f + i) * 4.0f + 2.0f;
        drone.aircraft_type = 2; // Drone
        drone.is_active = true;
        
        draw_aircraft(buffer, zbuffer, width, height, &drone);
        
        // Occasionally fire missiles
        if ((int)(time * 3.0f + i) % 20 == 0) {
            Missile missile;
            missile.start_pos = drone.pos;
            missile.end_pos.x = drone.pos.x + (rand() % 10 - 5);
            missile.end_pos.y = -5.0f;
            missile.end_pos.z = drone.pos.z + 5.0f;
            missile.progress = fmodf(time * 5.0f + i, 1.0f);
            missile.is_active = true;
            missile.trail_length = 5.0f;
            
            draw_missile(buffer, zbuffer, width, height, &missile);
        }
    }
    
    // Command overlay
    char status[] = "SWARM ACTIVE";
    for (int i = 0; i < strlen(status) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, 2 + i, height - 3, status[i], 0.0f);
    }
}

// Scene 112: Strategic Bombing - Large bombers attacking cities
void scene_strategic_bombing(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    int bomber_count = (int)(params[0].value * 4.0f) + 2;
    float bombing_intensity = params[1].value;
    float city_size = params[2].value * 10.0f + 5.0f;
    
    // Draw city grid
    for (int x = 0; x < (int)city_size; x++) {
        for (int z = 0; z < (int)city_size; z++) {
            StrategicTarget building;
            building.pos.x = (x - city_size / 2) * 2.0f;
            building.pos.y = -6.0f;
            building.pos.z = z * 2.0f + 5.0f;
            building.building_type = (x + z) % 4;
            
            // Buildings get destroyed over time based on bombing intensity
            float destruction_chance = bombing_intensity * (sinf(time * 0.5f + x + z) * 0.5f + 0.5f);
            building.is_destroyed = destruction_chance > 0.4f;
            building.damage_timer = time + x + z;
            
            draw_strategic_target(buffer, zbuffer, width, height, &building);
        }
    }
    
    // Draw bomber formation
    for (int i = 0; i < bomber_count; i++) {
        Aircraft bomber;
        bomber.pos.x = (i - bomber_count / 2.0f) * 4.0f;
        bomber.pos.y = 5.0f + sinf(time * 0.2f + i) * 1.0f;
        bomber.pos.z = -2.0f + time * 2.0f;
        bomber.aircraft_type = 1; // Bomber
        bomber.is_active = true;
        
        draw_aircraft(buffer, zbuffer, width, height, &bomber);
        
        // Drop bombs periodically
        if ((int)(time * 2.0f + i * 3.0f) % 15 == 0) {
            for (int b = 0; b < 3; b++) {
                Missile bomb;
                bomb.start_pos = bomber.pos;
                bomb.start_pos.x += (b - 1) * 0.5f;
                bomb.end_pos = bomb.start_pos;
                bomb.end_pos.y = -6.0f;
                bomb.end_pos.z += 3.0f;
                bomb.progress = fmodf(time * 3.0f + i + b, 1.0f);
                bomb.is_active = bomb.progress < 1.0f;
                bomb.trail_length = 3.0f;
                
                draw_missile(buffer, zbuffer, width, height, &bomb);
            }
        }
    }
    
    // Mission briefing
    char briefing[] = "STRATEGIC STRIKE";
    for (int i = 0; i < strlen(briefing) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, 2 + i, 2, briefing[i], 0.0f);
    }
}

// Scene 113: Air-to-Air Combat - Dogfighting aircraft
void scene_dogfight(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float combat_intensity = params[0].value;
    int aircraft_count = (int)(params[1].value * 6.0f) + 4;
    float missile_frequency = params[2].value;
    
    // Draw background clouds
    for (int c = 0; c < 20; c++) {
        float cloud_x = sinf(c * 2.0f) * 15.0f;
        float cloud_y = cosf(c * 1.5f) * 8.0f;
        float cloud_z = 15.0f + c * 2.0f;
        
        int cx = (int)(width / 2 + cloud_x * 10.0f / (cloud_z + 5.0f));
        int cy = (int)(height / 2 - cloud_y * 10.0f / (cloud_z + 5.0f));
        
        if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
            set_pixel(buffer, zbuffer, width, height, cx, cy, '~', cloud_z);
            if (cx + 1 < width) set_pixel(buffer, zbuffer, width, height, cx + 1, cy, '~', cloud_z);
        }
    }
    
    // Draw dogfighting aircraft
    for (int i = 0; i < aircraft_count; i++) {
        Aircraft fighter;
        
        // Erratic combat maneuvers
        float combat_phase = time * 3.0f + i * 1.2f;
        float evasion = sinf(combat_phase * 2.0f) * combat_intensity;
        
        fighter.pos.x = sinf(combat_phase + i) * 12.0f + evasion * 3.0f;
        fighter.pos.y = cosf(combat_phase * 1.3f + i) * 6.0f + evasion * 2.0f;
        fighter.pos.z = sinf(combat_phase * 0.7f + i) * 8.0f;
        fighter.aircraft_type = 0; // Fighter
        fighter.is_active = true;
        
        draw_aircraft(buffer, zbuffer, width, height, &fighter);
        
        // Muzzle flashes and missiles
        if (sinf(time * 10.0f + i) > 0.8f) {
            // Muzzle flash
            int mx = (int)(width / 2 + (fighter.pos.x + 1) * 10.0f / (fighter.pos.z + 5.0f));
            int my = (int)(height / 2 - fighter.pos.y * 10.0f / (fighter.pos.z + 5.0f));
            if (mx >= 0 && mx < width && my >= 0 && my < height) {
                set_pixel(buffer, zbuffer, width, height, mx, my, '*', fighter.pos.z - 0.1f);
            }
        }
        
        // Air-to-air missiles
        if (missile_frequency > 0.5f && (int)(time * 4.0f + i * 2.0f) % 25 == 0) {
            Missile missile;
            missile.start_pos = fighter.pos;
            
            // Target another aircraft
            int target_idx = (i + 1) % aircraft_count;
            missile.end_pos.x = sinf(combat_phase + target_idx) * 12.0f;
            missile.end_pos.y = cosf(combat_phase * 1.3f + target_idx) * 6.0f;
            missile.end_pos.z = sinf(combat_phase * 0.7f + target_idx) * 8.0f;
            
            missile.progress = fmodf(time * 6.0f + i, 1.0f);
            missile.is_active = missile.progress < 1.0f;
            missile.trail_length = 8.0f;
            
            draw_missile(buffer, zbuffer, width, height, &missile);
        }
    }
    
    draw_radar_overlay(buffer, zbuffer, width, height, time, 50.0f);
}

// Scene 114: Helicopter Assault - Attack helicopters and ground targets
void scene_helicopter_assault(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    int helo_count = (int)(params[0].value * 4.0f) + 2;
    float assault_intensity = params[1].value;
    float ground_activity = params[2].value;
    
    // Draw terrain
    for (int x = 0; x < width; x += 3) {
        int terrain_height = height * 3 / 4 + (int)(sinf(x * 0.1f) * 3.0f);
        for (int y = terrain_height; y < height; y++) {
            if ((x + y) % 4 == 0) {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 15.0f);
            }
        }
    }
    
    // Draw ground targets
    for (int t = 0; t < 8; t++) {
        StrategicTarget target;
        target.pos.x = (t - 4) * 6.0f + sinf(t * 1.5f) * 2.0f;
        target.pos.y = -4.0f;
        target.pos.z = 8.0f + t * 1.5f;
        target.building_type = 0; // Military targets
        target.is_destroyed = (assault_intensity * sinf(time + t)) > 0.3f;
        target.damage_timer = time + t;
        
        draw_strategic_target(buffer, zbuffer, width, height, &target);
        
        // Ground activity - vehicles moving
        if (ground_activity > 0.3f) {
            float vehicle_x = target.pos.x + sinf(time * 2.0f + t) * 3.0f;
            float vehicle_z = target.pos.z + cosf(time * 1.5f + t) * 2.0f;
            
            int vx = (int)(width / 2 + vehicle_x * 10.0f / (vehicle_z + 5.0f));
            int vy = (int)(height / 2 - (-4.0f) * 10.0f / (vehicle_z + 5.0f));
            
            if (vx >= 0 && vx < width && vy >= 0 && vy < height) {
                set_pixel(buffer, zbuffer, width, height, vx, vy, 'T', vehicle_z); // Tank
            }
        }
    }
    
    // Draw attack helicopters
    for (int i = 0; i < helo_count; i++) {
        Aircraft helo;
        
        // Helicopter hovering and maneuvering
        float hover_phase = time * 1.5f + i * 0.8f;
        helo.pos.x = sinf(hover_phase) * 8.0f + (i - helo_count / 2.0f) * 3.0f;
        helo.pos.y = 2.0f + sinf(hover_phase * 3.0f) * 1.0f; // Bobbing motion
        helo.pos.z = cosf(hover_phase * 0.5f) * 5.0f + 3.0f;
        helo.aircraft_type = 3; // Helicopter
        helo.is_active = true;
        
        draw_aircraft(buffer, zbuffer, width, height, &helo);
        
        // Rotor blur effect
        for (int r = 0; r < 8; r++) {
            float rotor_angle = time * 20.0f + r * M_PI / 4.0f;
            float rotor_x = helo.pos.x + cosf(rotor_angle) * 1.5f;
            float rotor_y = helo.pos.y + sinf(rotor_angle) * 0.3f;
            
            int rx = (int)(width / 2 + rotor_x * 10.0f / (helo.pos.z + 5.0f));
            int ry = (int)(height / 2 - rotor_y * 10.0f / (helo.pos.z + 5.0f));
            
            if (rx >= 0 && rx < width && ry >= 0 && ry < height) {
                set_pixel(buffer, zbuffer, width, height, rx, ry, '~', helo.pos.z - 0.1f);
            }
        }
        
        // Rocket salvos
        if ((int)(time * 3.0f + i * 2.0f) % 20 == 0) {
            for (int r = 0; r < 4; r++) {
                Missile rocket;
                rocket.start_pos = helo.pos;
                rocket.start_pos.x += (r - 2) * 0.3f;
                rocket.end_pos.x = helo.pos.x + (rand() % 10 - 5) * 2.0f;
                rocket.end_pos.y = -4.0f;
                rocket.end_pos.z = helo.pos.z + 8.0f;
                rocket.progress = fmodf(time * 4.0f + i + r, 1.0f);
                rocket.is_active = rocket.progress < 1.0f;
                rocket.trail_length = 6.0f;
                
                draw_missile(buffer, zbuffer, width, height, &rocket);
            }
        }
    }
}

// Scene 115: Stealth Operation - Hard to detect aircraft
void scene_stealth_mission(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float stealth_level = params[0].value;
    float detection_range = params[1].value * 20.0f + 10.0f;
    float mission_progress = params[2].value;
    
    // Draw enemy radar installations
    for (int r = 0; r < 5; r++) {
        float radar_x = (r - 2) * 8.0f;
        float radar_z = 10.0f + r * 3.0f;
        
        // Radar dish
        int rx = (int)(width / 2 + radar_x * 10.0f / (radar_z + 5.0f));
        int ry = (int)(height / 2 - (-3.0f) * 10.0f / (radar_z + 5.0f));
        
        if (rx >= 0 && rx < width && ry >= 0 && ry < height) {
            set_pixel(buffer, zbuffer, width, height, rx, ry, 'R', radar_z);
            
            // Radar sweep
            float sweep = fmodf(time * 2.0f + r, 2.0f * M_PI);
            for (int i = 1; i < detection_range / 2; i++) {
                int sweep_x = rx + (int)(cosf(sweep) * i);
                int sweep_y = ry + (int)(sinf(sweep) * i);
                
                if (sweep_x >= 0 && sweep_x < width && sweep_y >= 0 && sweep_y < height) {
                    if (i % 3 == 0) {
                        set_pixel(buffer, zbuffer, width, height, sweep_x, sweep_y, ':', radar_z - 0.1f);
                    }
                }
            }
        }
    }
    
    // Draw stealth aircraft (visibility depends on stealth level)
    for (int i = 0; i < 2; i++) {
        Aircraft stealth;
        
        float stealth_phase = time * 0.8f + i * M_PI;
        stealth.pos.x = sinf(stealth_phase) * 12.0f;
        stealth.pos.y = 3.0f + sinf(stealth_phase * 2.0f) * 1.0f;
        stealth.pos.z = mission_progress * 20.0f - 5.0f + i * 3.0f;
        stealth.aircraft_type = 0; // Fighter (stealth variant)
        
        // Visibility flickers based on stealth level
        float visibility = sinf(time * 5.0f + i) * (1.0f - stealth_level) + stealth_level;
        stealth.is_active = visibility > 0.4f;
        
        if (stealth.is_active) {
            // Draw with reduced visibility
            char stealth_chars[] = {'.', ':', '*', '^'};
            int visibility_level = (int)(visibility * 4);
            if (visibility_level > 3) visibility_level = 3;
            
            int sx = (int)(width / 2 + stealth.pos.x * 10.0f / (stealth.pos.z + 5.0f));
            int sy = (int)(height / 2 - stealth.pos.y * 10.0f / (stealth.pos.z + 5.0f));
            
            if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                set_pixel(buffer, zbuffer, width, height, sx, sy, stealth_chars[visibility_level], stealth.pos.z);
            }
        }
    }
    
    // Mission objectives
    for (int obj = 0; obj < 3; obj++) {
        StrategicTarget objective;
        objective.pos.x = (obj - 1) * 6.0f;
        objective.pos.y = -2.0f;
        objective.pos.z = 15.0f + obj * 4.0f;
        objective.building_type = 0; // Military
        objective.is_destroyed = mission_progress > (obj + 1) * 0.33f;
        objective.damage_timer = time;
        
        draw_strategic_target(buffer, zbuffer, width, height, &objective);
    }
    
    // Mission status
    char status[] = "STEALTH MODE";
    for (int i = 0; i < strlen(status) && i < width; i++) {
        if (sinf(time * 10.0f) > 0.5f) { // Blinking text
            set_pixel(buffer, zbuffer, width, height, 2 + i, height - 2, status[i], 0.0f);
        }
    }
}

// Scene 116: Carrier Strike - Aircraft launching from carrier
void scene_carrier_strike(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float launch_rate = params[0].value * 3.0f + 1.0f;
    int strike_size = (int)(params[1].value * 8.0f) + 4;
    float sea_state = params[2].value;
    
    // Draw ocean waves
    for (int y = height * 2 / 3; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            float wave = sinf(x * 0.1f + time * 2.0f) * sea_state * 2.0f;
            if (y == height * 2 / 3 + (int)wave) {
                set_pixel(buffer, zbuffer, width, height, x, y, '~', 20.0f);
            }
        }
    }
    
    // Draw aircraft carrier
    int carrier_x = width / 2;
    int carrier_y = height * 3 / 4;
    
    // Carrier deck
    for (int x = -15; x <= 15; x++) {
        if (carrier_x + x >= 0 && carrier_x + x < width) {
            set_pixel(buffer, zbuffer, width, height, carrier_x + x, carrier_y, '=', 10.0f);
            set_pixel(buffer, zbuffer, width, height, carrier_x + x, carrier_y + 1, '=', 10.0f);
        }
    }
    
    // Carrier island
    for (int y = 0; y < 3; y++) {
        set_pixel(buffer, zbuffer, width, height, carrier_x + 12, carrier_y - y, '|', 9.0f);
    }
    
    // Launch aircraft from carrier
    for (int i = 0; i < strike_size; i++) {
        Aircraft naval_fighter;
        
        float launch_time = time * launch_rate - i * 0.5f;
        if (launch_time < 0) continue;
        
        // Catapult launch sequence
        if (launch_time < 2.0f) {
            // On deck accelerating
            naval_fighter.pos.x = -10.0f + launch_time * 15.0f;
            naval_fighter.pos.y = 0.0f;
            naval_fighter.pos.z = 5.0f;
        } else {
            // Airborne
            float flight_time = launch_time - 2.0f;
            naval_fighter.pos.x = sinf(flight_time * 0.5f + i) * 15.0f;
            naval_fighter.pos.y = flight_time * 2.0f + sinf(flight_time * 2.0f + i) * 1.0f;
            naval_fighter.pos.z = flight_time * 3.0f + 5.0f;
        }
        
        naval_fighter.aircraft_type = 0; // Naval fighter
        naval_fighter.is_active = true;
        
        draw_aircraft(buffer, zbuffer, width, height, &naval_fighter);
    }
    
    // Steam catapults
    if (fmodf(time * launch_rate, 2.0f) < 0.2f) {
        for (int s = 0; s < 20; s++) {
            int steam_x = carrier_x - 10 + s;
            if (steam_x >= 0 && steam_x < width) {
                set_pixel(buffer, zbuffer, width, height, steam_x, carrier_y - 1, '*', 8.0f);
            }
        }
    }
    
    char ops[] = "FLIGHT OPS";
    for (int i = 0; i < strlen(ops) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, 2 + i, 2, ops[i], 0.0f);
    }
}

// Scene 117: Missile Defense - Intercepting incoming missiles
void scene_missile_defense(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float threat_level = params[0].value;
    float defense_efficiency = params[1].value;
    int incoming_count = (int)(params[2].value * 10.0f) + 5;
    
    // Draw city skyline to defend
    for (int b = 0; b < 12; b++) {
        int building_height = 5 + (b % 4) * 2;
        int building_x = b * 6 + 5;
        
        for (int h = 0; h < building_height; h++) {
            if (building_x < width) {
                set_pixel(buffer, zbuffer, width, height, building_x, height - 2 - h, '#', 15.0f);
                set_pixel(buffer, zbuffer, width, height, building_x + 1, height - 2 - h, '#', 15.0f);
            }
        }
    }
    
    // Draw incoming ballistic missiles
    for (int i = 0; i < incoming_count; i++) {
        Missile incoming;
        
        float attack_phase = time * 0.8f + i * 0.3f;
        incoming.start_pos.x = (i - incoming_count / 2.0f) * 8.0f;
        incoming.start_pos.y = 15.0f;
        incoming.start_pos.z = -10.0f;
        
        incoming.end_pos.x = incoming.start_pos.x + sinf(i * 2.0f) * 5.0f;
        incoming.end_pos.y = -5.0f;
        incoming.end_pos.z = 15.0f;
        
        incoming.progress = fmodf(attack_phase, 1.5f) / 1.5f;
        incoming.is_active = incoming.progress < 1.0f && incoming.progress > 0.0f;
        incoming.trail_length = 10.0f;
        incoming.missile_type = 2; // Ballistic
        
        if (incoming.is_active) {
            draw_missile(buffer, zbuffer, width, height, &incoming);
        }
        
        // Defense interceptors
        if (defense_efficiency > 0.3f && incoming.progress > 0.5f && incoming.progress < 0.8f) {
            Missile interceptor;
            
            interceptor.start_pos.x = incoming.end_pos.x;
            interceptor.start_pos.y = -3.0f;
            interceptor.start_pos.z = 12.0f;
            
            // Calculate intercept point
            float intercept_progress = (incoming.progress - 0.5f) / 0.3f;
            interceptor.end_pos.x = incoming.start_pos.x + (incoming.end_pos.x - incoming.start_pos.x) * (incoming.progress + 0.2f);
            interceptor.end_pos.y = incoming.start_pos.y + (incoming.end_pos.y - incoming.start_pos.y) * (incoming.progress + 0.2f);
            interceptor.end_pos.z = incoming.start_pos.z + (incoming.end_pos.z - incoming.start_pos.z) * (incoming.progress + 0.2f);
            
            interceptor.progress = intercept_progress;
            interceptor.is_active = intercept_progress >= 0.0f && intercept_progress < 1.0f;
            interceptor.trail_length = 5.0f;
            
            if (interceptor.is_active) {
                draw_missile(buffer, zbuffer, width, height, &interceptor);
                
                // Interception explosion
                if (intercept_progress > 0.8f && defense_efficiency > 0.6f) {
                    Vec3 explosion_pos = interceptor.end_pos;
                    int ex = (int)(width / 2 + explosion_pos.x * 10.0f / (explosion_pos.z + 5.0f));
                    int ey = (int)(height / 2 - explosion_pos.y * 10.0f / (explosion_pos.z + 5.0f));
                    
                    if (ex >= 0 && ex < width && ey >= 0 && ey < height) {
                        char explosion_chars[] = "*X@%#";
                        int exp_frame = (int)(time * 10.0f + i) % 5;
                        set_pixel(buffer, zbuffer, width, height, ex, ey, explosion_chars[exp_frame], explosion_pos.z - 1.0f);
                        
                        // Explosion spread
                        for (int dx = -2; dx <= 2; dx++) {
                            for (int dy = -2; dy <= 2; dy++) {
                                if (abs(dx) + abs(dy) <= 2 && ex + dx >= 0 && ex + dx < width && ey + dy >= 0 && ey + dy < height) {
                                    set_pixel(buffer, zbuffer, width, height, ex + dx, ey + dy, '.', explosion_pos.z);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Defense status
    char defense[] = "SHIELD ACTIVE";
    for (int i = 0; i < strlen(defense) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, width - strlen(defense) - 2 + i, 2, defense[i], 0.0f);
    }
}

// Scene 118: Reconnaissance Drone - Surveillance and intelligence
void scene_recon_drone(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float surveillance_area = params[0].value * 15.0f + 5.0f;
    float intel_gathering = params[1].value;
    float drone_altitude = params[2].value * 8.0f + 3.0f;
    
    // Draw surveillance grid overlay
    for (int x = 0; x < width; x += 8) {
        for (int y = 0; y < height; y += 6) {
            if (intel_gathering > 0.3f) {
                set_pixel(buffer, zbuffer, width, height, x, y, '+', 25.0f);
            }
        }
    }
    
    // Draw terrain with points of interest
    for (int poi = 0; poi < 8; poi++) {
        StrategicTarget target;
        target.pos.x = (poi - 4) * 4.0f + sinf(poi * 1.3f) * 2.0f;
        target.pos.y = -6.0f;
        target.pos.z = 8.0f + poi * 2.0f;
        target.building_type = poi % 4;
        target.is_destroyed = false;
        
        draw_strategic_target(buffer, zbuffer, width, height, &target);
        
        // Intelligence markers
        if (intel_gathering > 0.5f) {
            int marker_x = (int)(width / 2 + target.pos.x * 10.0f / (target.pos.z + 5.0f));
            int marker_y = (int)(height / 2 - target.pos.y * 10.0f / (target.pos.z + 5.0f));
            
            if (marker_x >= 0 && marker_x < width && marker_y >= 0 && marker_y < height) {
                // Blinking target marker
                if ((int)(time * 4.0f + poi) % 2 == 0) {
                    set_pixel(buffer, zbuffer, width, height, marker_x - 1, marker_y - 1, '/', target.pos.z - 1.0f);
                    set_pixel(buffer, zbuffer, width, height, marker_x + 1, marker_y - 1, '\\', target.pos.z - 1.0f);
                    set_pixel(buffer, zbuffer, width, height, marker_x - 1, marker_y + 1, '\\', target.pos.z - 1.0f);
                    set_pixel(buffer, zbuffer, width, height, marker_x + 1, marker_y + 1, '/', target.pos.z - 1.0f);
                }
            }
        }
    }
    
    // Draw reconnaissance drone
    Aircraft recon_drone;
    
    // Systematic search pattern
    float search_phase = time * 0.5f;
    float grid_x = fmodf(search_phase, 4.0f) - 2.0f;
    float grid_z = floorf(search_phase / 4.0f);
    
    recon_drone.pos.x = grid_x * surveillance_area / 2.0f;
    recon_drone.pos.y = drone_altitude + sinf(time * 2.0f) * 0.5f;
    recon_drone.pos.z = fmodf(grid_z * 3.0f, 20.0f);
    recon_drone.aircraft_type = 2; // Drone
    recon_drone.is_active = true;
    
    draw_aircraft(buffer, zbuffer, width, height, &recon_drone);
    
    // Camera gimbal effect
    int camera_x = (int)(width / 2 + recon_drone.pos.x * 10.0f / (recon_drone.pos.z + 5.0f));
    int camera_y = (int)(height / 2 - recon_drone.pos.y * 10.0f / (recon_drone.pos.z + 5.0f));
    
    if (camera_x >= 0 && camera_x < width && camera_y >= 0 && camera_y < height) {
        set_pixel(buffer, zbuffer, width, height, camera_x, camera_y + 1, 'o', recon_drone.pos.z - 0.1f);
    }
    
    // Data link visualization
    if (intel_gathering > 0.4f) {
        for (int link = 0; link < 5; link++) {
            int link_x = camera_x + (int)(sinf(time * 8.0f + link) * 3.0f);
            int link_y = camera_y + link * 2;
            
            if (link_x >= 0 && link_x < width && link_y >= 0 && link_y < height) {
                set_pixel(buffer, zbuffer, width, height, link_x, link_y, '.', recon_drone.pos.z + 1.0f);
            }
        }
    }
    
    // Telemetry display
    char telemetry[] = "RECON ACTIVE";
    for (int i = 0; i < strlen(telemetry) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, 2 + i, height - 2, telemetry[i], 0.0f);
    }
    
    // Altitude indicator
    char alt_display[20];
    snprintf(alt_display, sizeof(alt_display), "ALT: %.0f", drone_altitude * 1000);
    for (int i = 0; i < strlen(alt_display) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, width - strlen(alt_display) - 2 + i, height - 2, alt_display[i], 0.0f);
    }
}

// Scene 119: Air Command Center - Strategic overview with multiple operations
void scene_air_command(char* buffer, float* zbuffer, int width, int height, Parameter* params, float time, AudioData* audio) {
    (void)audio;
    clear_buffer(buffer, zbuffer, width, height);
    
    float operation_scale = params[0].value;
    int active_missions = (int)(params[1].value * 5.0f) + 2;
    float threat_assessment = params[2].value;
    
    // Draw strategic map grid
    for (int x = 0; x < width; x += 10) {
        for (int y = 0; y < height; y += 8) {
            set_pixel(buffer, zbuffer, width, height, x, y, '.', 30.0f);
        }
    }
    
    // Draw multiple operational theaters
    for (int theater = 0; theater < active_missions; theater++) {
        float theater_x = (theater - active_missions / 2.0f) * 15.0f;
        float theater_z = 10.0f + theater * 8.0f;
        
        // Theater of operations boundary
        for (int b = 0; b < 20; b++) {
            float boundary_angle = b * M_PI / 10.0f;
            float boundary_x = theater_x + cosf(boundary_angle) * 8.0f * operation_scale;
            float boundary_y = sinf(boundary_angle) * 6.0f * operation_scale;
            
            int bx = (int)(width / 2 + boundary_x * 10.0f / (theater_z + 5.0f));
            int by = (int)(height / 2 - boundary_y * 10.0f / (theater_z + 5.0f));
            
            if (bx >= 0 && bx < width && by >= 0 && by < height) {
                set_pixel(buffer, zbuffer, width, height, bx, by, ':', theater_z);
            }
        }
        
        // Assets in theater
        for (int asset = 0; asset < 4; asset++) {
            Aircraft theater_asset;
            
            float asset_phase = time * 1.2f + theater * 2.0f + asset * 0.5f;
            theater_asset.pos.x = theater_x + sinf(asset_phase + asset) * 6.0f * operation_scale;
            theater_asset.pos.y = cosf(asset_phase + asset) * 4.0f * operation_scale;
            theater_asset.pos.z = theater_z + sinf(asset_phase * 0.3f + asset) * 3.0f;
            theater_asset.aircraft_type = asset % 4;
            theater_asset.is_active = true;
            
            draw_aircraft(buffer, zbuffer, width, height, &theater_asset);
        }
        
        // Targets in theater
        for (int target = 0; target < 3; target++) {
            StrategicTarget theater_target;
            theater_target.pos.x = theater_x + (target - 1) * 4.0f;
            theater_target.pos.y = -3.0f;
            theater_target.pos.z = theater_z + target * 2.0f;
            theater_target.building_type = target % 4;
            theater_target.is_destroyed = (threat_assessment * sinf(time + theater + target)) > 0.4f;
            theater_target.damage_timer = time + theater + target;
            
            draw_strategic_target(buffer, zbuffer, width, height, &theater_target);
        }
    }
    
    // Command center displays
    // Threat level indicator
    int threat_bar_length = (int)(threat_assessment * 20);
    for (int i = 0; i < threat_bar_length && i < width - 5; i++) {
        char threat_char = (i < 7) ? '-' : (i < 14) ? '+' : '#';
        set_pixel(buffer, zbuffer, width, height, 2 + i, 3, threat_char, 0.0f);
    }
    
    // Mission status
    char missions_text[30];
    snprintf(missions_text, sizeof(missions_text), "MISSIONS: %d", active_missions);
    for (int i = 0; i < strlen(missions_text) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, 2 + i, 5, missions_text[i], 0.0f);
    }
    
    // Real-time clock
    int mission_time = (int)(time * 10) % 2400;
    char time_display[10];
    snprintf(time_display, sizeof(time_display), "%02d:%02d", mission_time / 100, mission_time % 100);
    for (int i = 0; i < strlen(time_display) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, width - strlen(time_display) - 2 + i, 3, time_display[i], 0.0f);
    }
    
    // Command header
    char command[] = "AIR OPERATIONS CENTER";
    for (int i = 0; i < strlen(command) && i < width; i++) {
        set_pixel(buffer, zbuffer, width, height, (width - strlen(command)) / 2 + i, 1, command[i], 0.0f);
    }
    
    // Radar sweep overlay
    draw_radar_overlay(buffer, zbuffer, width, height, time, 200.0f);
}

// ============= REVOLUTION & GEOMETRY STRUCTURES =============

typedef struct {
    Vec3 pos;
    Vec3 velocity;
    float energy;
    int person_type; // 0=protester, 1=riot_police, 2=leader, 3=medic
    bool is_active;
    float action_phase;
    Vec3 target_pos;
} CrowdPerson;

typedef struct {
    Vec3 center;
    float pupil_size;
    float blink_phase;
    float look_direction_x;
    float look_direction_y;
    float eyelid_top;
    float eyelid_bottom;
    bool is_blinking;
    float iris_radius;
    float tension;
} Eye;

typedef struct {
    Vec3 pos;
    Vec3 normal;
    float displacement;
    float color_intensity;
    int vertex_type;
} DisplacedVertex;

// Helper functions for new scenes
void draw_crowd_person(char* buffer, float* zbuffer, int width, int height, CrowdPerson* person) {
    // Project to screen
    float screen_x = width / 2 + person->pos.x * 20.0f / (person->pos.z + 10.0f);
    float screen_y = height / 2 - person->pos.y * 20.0f / (person->pos.z + 10.0f);
    
    int x = (int)screen_x;
    int y = (int)screen_y;
    
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    
    char person_char;
    switch(person->person_type) {
        case 0: person_char = (person->energy > 0.5f) ? '@' : 'o'; break; // protester
        case 1: person_char = '#'; break; // riot police
        case 2: person_char = '*'; break; // leader
        case 3: person_char = '+'; break; // medic
        default: person_char = '.'; break;
    }
    
    set_pixel(buffer, zbuffer, width, height, x, y, person_char, person->pos.z);
    
    // Draw movement trail
    if (person->energy > 0.3f) {
        int trail_x = x - (int)(person->velocity.x * 2.0f);
        int trail_y = y + (int)(person->velocity.y * 2.0f);
        if (trail_x >= 0 && trail_x < width && trail_y >= 0 && trail_y < height) {
            set_pixel(buffer, zbuffer, width, height, trail_x, trail_y, '.', person->pos.z + 0.1f);
        }
    }
}

void draw_eye(char* buffer, float* zbuffer, int width, int height, Eye* eye, float scale) {
    int cx = (int)(width / 2 + eye->center.x * scale);
    int cy = (int)(height / 2 - eye->center.y * scale);
    
    float eye_width = 12.0f * scale;
    float eye_height = 8.0f * scale;
    
    // Draw eye outline
    for (int angle = 0; angle < 360; angle += 10) {
        float rad = angle * M_PI / 180.0f;
        float ex = eye_width * cosf(rad) * 0.5f;
        float ey = eye_height * sinf(rad) * 0.5f;
        
        int px = cx + (int)ex;
        int py = cy + (int)ey;
        
        if (px >= 0 && px < width && py >= 0 && py < height) {
            set_pixel(buffer, zbuffer, width, height, px, py, '-', eye->center.z);
        }
    }
    
    // Draw iris
    float iris_x = cx + eye->look_direction_x * 3.0f * scale;
    float iris_y = cy + eye->look_direction_y * 2.0f * scale;
    
    for (int angle = 0; angle < 360; angle += 15) {
        float rad = angle * M_PI / 180.0f;
        float ix = eye->iris_radius * scale * cosf(rad);
        float iy = eye->iris_radius * scale * sinf(rad);
        
        int px = (int)(iris_x + ix);
        int py = (int)(iris_y + iy);
        
        if (px >= 0 && px < width && py >= 0 && py < height) {
            set_pixel(buffer, zbuffer, width, height, px, py, 'o', eye->center.z - 0.1f);
        }
    }
    
    // Draw pupil
    float pupil_x = iris_x;
    float pupil_y = iris_y;
    float pupil_radius = eye->pupil_size * scale;
    
    for (int py = -2; py <= 2; py++) {
        for (int px = -2; px <= 2; px++) {
            if (px*px + py*py <= pupil_radius*pupil_radius) {
                int screen_x = (int)(pupil_x + px);
                int screen_y = (int)(pupil_y + py);
                if (screen_x >= 0 && screen_x < width && screen_y >= 0 && screen_y < height) {
                    set_pixel(buffer, zbuffer, width, height, screen_x, screen_y, '#', eye->center.z - 0.2f);
                }
            }
        }
    }
    
    // Draw eyelids if blinking
    if (eye->is_blinking) {
        float blink_amount = sinf(eye->blink_phase) * 0.5f + 0.5f;
        int lid_height = (int)(eye_height * blink_amount * 0.3f);
        
        // Top eyelid
        for (int x = -eye_width/2; x <= eye_width/2; x++) {
            for (int y = 0; y <= lid_height; y++) {
                int px = cx + x;
                int py = cy - eye_height/2 + y;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    set_pixel(buffer, zbuffer, width, height, px, py, '=', eye->center.z - 0.3f);
                }
            }
        }
        
        // Bottom eyelid
        for (int x = -eye_width/2; x <= eye_width/2; x++) {
            for (int y = 0; y <= lid_height; y++) {
                int px = cx + x;
                int py = cy + eye_height/2 - y;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    set_pixel(buffer, zbuffer, width, height, px, py, '=', eye->center.z - 0.3f);
                }
            }
        }
    }
}

void draw_displaced_vertex(char* buffer, float* zbuffer, int width, int height, DisplacedVertex* vertex) {
    // Project to screen with displacement
    float displaced_x = vertex->pos.x + vertex->normal.x * vertex->displacement;
    float displaced_y = vertex->pos.y + vertex->normal.y * vertex->displacement;
    float displaced_z = vertex->pos.z + vertex->normal.z * vertex->displacement;
    
    float screen_x = width / 2 + displaced_x * 15.0f / (displaced_z + 8.0f);
    float screen_y = height / 2 - displaced_y * 15.0f / (displaced_z + 8.0f);
    
    int x = (int)screen_x;
    int y = (int)screen_y;
    
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    
    // Choose character based on displacement and shading
    char shade_char;
    float shade_intensity = vertex->color_intensity;
    
    if (shade_intensity > 0.8f) shade_char = '#';
    else if (shade_intensity > 0.6f) shade_char = '@';
    else if (shade_intensity > 0.4f) shade_char = '*';
    else if (shade_intensity > 0.2f) shade_char = '+';
    else shade_char = '.';
    
    set_pixel(buffer, zbuffer, width, height, x, y, shade_char, displaced_z);
}

// ============= NEW SCENES 120-129 =============

// Scene 120: Street Revolution - Crowd dynamics and protest action
void scene_120(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float protest_intensity = params[0].value;
    float police_response = params[1].value;
    float crowd_density = params[2].value;
    
    static CrowdPerson protesters[100];
    static CrowdPerson police[20];
    static bool initialized = false;
    
    if (!initialized) {
        // Initialize protesters
        for (int i = 0; i < 100; i++) {
            protesters[i].pos.x = (rand() % 40) - 20.0f;
            protesters[i].pos.y = (rand() % 20) - 10.0f;
            protesters[i].pos.z = (rand() % 30) + 5.0f;
            protesters[i].person_type = (i < 5) ? 2 : 0; // Some leaders
            protesters[i].is_active = true;
            protesters[i].energy = 0.5f + (rand() % 100) / 200.0f;
        }
        
        // Initialize police
        for (int i = 0; i < 20; i++) {
            police[i].pos.x = (rand() % 20) - 10.0f;
            police[i].pos.y = -15.0f + (rand() % 5);
            police[i].pos.z = (rand() % 20) + 10.0f;
            police[i].person_type = 1;
            police[i].is_active = true;
            police[i].energy = 0.7f;
        }
        initialized = true;
    }
    
    // Update crowd dynamics
    for (int i = 0; i < 100; i++) {
        if (protesters[i].is_active) {
            // Protest movement patterns
            float wave_movement = sinf(time * 2.0f + i * 0.1f) * protest_intensity;
            float group_cohesion = cosf(time * 1.5f + i * 0.05f) * crowd_density;
            
            protesters[i].velocity.x = wave_movement * 2.0f + group_cohesion;
            protesters[i].velocity.y = sinf(time * 1.8f + i * 0.08f) * protest_intensity;
            protesters[i].energy = 0.3f + protest_intensity * 0.7f + sinf(time * 3.0f + i) * 0.2f;
            
            // Move towards action
            protesters[i].pos.x += protesters[i].velocity.x * 0.1f;
            protesters[i].pos.y += protesters[i].velocity.y * 0.1f;
            
            // Boundary constraints
            if (protesters[i].pos.x > 25.0f) protesters[i].velocity.x = -protesters[i].velocity.x;
            if (protesters[i].pos.x < -25.0f) protesters[i].velocity.x = -protesters[i].velocity.x;
        }
        
        draw_crowd_person(buffer, zbuffer, width, height, &protesters[i]);
    }
    
    // Update police response
    for (int i = 0; i < 20; i++) {
        if (police[i].is_active) {
            // Police advance based on response level
            police[i].velocity.x = sinf(time * 1.2f + i * 0.2f) * police_response;
            police[i].velocity.y = police_response * 0.5f;
            police[i].energy = police_response;
            
            police[i].pos.y += police[i].velocity.y * 0.08f;
            
            // Reset position periodically
            if (police[i].pos.y > 10.0f) {
                police[i].pos.y = -15.0f;
            }
        }
        
        draw_crowd_person(buffer, zbuffer, width, height, &police[i]);
    }
    
    // Draw protest signs and banners
    for (int i = 0; i < 5; i++) {
        float banner_x = sinf(time * 0.8f + i * 1.2f) * 15.0f;
        float banner_y = 2.0f + cosf(time * 1.1f + i) * 2.0f;
        float banner_z = 15.0f + i * 3.0f;
        
        int screen_x = width / 2 + banner_x * 20.0f / (banner_z + 10.0f);
        int screen_y = height / 2 - banner_y * 20.0f / (banner_z + 10.0f);
        
        // Draw banner
        for (int j = -3; j <= 3; j++) {
            if (screen_x + j >= 0 && screen_x + j < width && screen_y >= 0 && screen_y < height) {
                set_pixel(buffer, zbuffer, width, height, screen_x + j, screen_y, '=', banner_z);
            }
        }
    }
    
    // Environmental effects - smoke and tear gas
    if (police_response > 0.6f) {
        for (int smoke = 0; smoke < 30; smoke++) {
            float smoke_x = sinf(time * 2.0f + smoke * 0.5f) * 20.0f;
            float smoke_y = cosf(time * 1.5f + smoke * 0.3f) * 8.0f;
            float smoke_z = 10.0f + (smoke % 15);
            
            int sx = width / 2 + smoke_x * 20.0f / (smoke_z + 10.0f);
            int sy = height / 2 - smoke_y * 20.0f / (smoke_z + 10.0f);
            
            if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                set_pixel(buffer, zbuffer, width, height, sx, sy, '~', smoke_z);
            }
        }
    }
}

// Scene 121: Barricade Building - Revolutionary construction and defense
void scene_121(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float construction_speed = params[0].value;
    float barricade_height = params[1].value;
    float defender_count = params[2].value;
    
    // Draw multiple barricades being built
    for (int barricade = 0; barricade < 4; barricade++) {
        float base_x = (barricade - 1.5f) * 15.0f;
        float base_z = 20.0f + barricade * 5.0f;
        
        // Construction progress
        float build_progress = fmodf(time * construction_speed + barricade * 2.0f, 8.0f);
        int max_height = (int)(barricade_height * 8.0f + 4.0f);
        int current_height = (int)(build_progress);
        if (current_height > max_height) current_height = max_height;
        
        // Build barricade from bottom up
        for (int layer = 0; layer < current_height; layer++) {
            int layer_width = 8 - layer;
            for (int x = -layer_width/2; x <= layer_width/2; x++) {
                float block_x = base_x + x * 2.0f;
                float block_y = layer * 2.0f - 4.0f;
                float block_z = base_z;
                
                int screen_x = width / 2 + block_x * 15.0f / (block_z + 10.0f);
                int screen_y = height / 2 - block_y * 15.0f / (block_z + 10.0f);
                
                if (screen_x >= 0 && screen_x < width && screen_y >= 0 && screen_y < height) {
                    char block_char = (layer == current_height - 1) ? '*' : '#';
                    set_pixel(buffer, zbuffer, width, height, screen_x, screen_y, block_char, block_z);
                }
            }
        }
        
        // Defenders on barricade
        int num_defenders = (int)(defender_count * 3) + 1;
        for (int def = 0; def < num_defenders; def++) {
            CrowdPerson defender;
            defender.pos.x = base_x + (def - num_defenders/2) * 3.0f;
            defender.pos.y = current_height * 2.0f - 2.0f;
            defender.pos.z = base_z - 2.0f;
            defender.person_type = 0;
            defender.energy = 0.8f + sinf(time * 3.0f + def) * 0.2f;
            defender.is_active = true;
            
            draw_crowd_person(buffer, zbuffer, width, height, &defender);
        }
    }
    
    // Material gathering animation
    for (int worker = 0; worker < 12; worker++) {
        float worker_phase = time * construction_speed + worker * 0.5f;
        float worker_x = sinf(worker_phase) * 25.0f;
        float worker_y = -8.0f + cosf(worker_phase * 0.3f) * 2.0f;
        float worker_z = 15.0f + worker * 2.0f;
        
        CrowdPerson worker_person;
        worker_person.pos.x = worker_x;
        worker_person.pos.y = worker_y;
        worker_person.pos.z = worker_z;
        worker_person.person_type = 0;
        worker_person.energy = construction_speed;
        worker_person.is_active = true;
        
        draw_crowd_person(buffer, zbuffer, width, height, &worker_person);
        
        // Material blocks being carried
        if (sinf(worker_phase) > 0.0f) {
            int mat_x = width / 2 + worker_x * 15.0f / (worker_z + 10.0f);
            int mat_y = height / 2 - (worker_y + 3.0f) * 15.0f / (worker_z + 10.0f);
            
            if (mat_x >= 0 && mat_x < width && mat_y >= 0 && mat_y < height) {
                set_pixel(buffer, zbuffer, width, height, mat_x, mat_y, 'O', worker_z - 0.1f);
            }
        }
    }
}

// Scene 122: CCTV Camera - Close-up security camera graphic
void scene_122(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float zoom_level = params[0].value;
    float scan_speed = params[1].value;
    float interference = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    // CCTV Camera lens - large circular view filling screen
    int center_x = width / 2;
    int center_y = height / 2;
    int lens_radius = (int)((height * 0.4f) * (1.0f + zoom_level));
    
    // Draw camera lens outline
    for (int angle = 0; angle < 360; angle += 2) {
        float rad = angle * M_PI / 180.0f;
        int x = center_x + (int)(lens_radius * cosf(rad));
        int y = center_y + (int)(lens_radius * sinf(rad) * 0.8f);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            set_pixel(buffer, zbuffer, width, height, x, y, '#', 1.0f);
        }
    }
    
    // Lens reflections - concentric circles
    for (int ring = 1; ring < 4; ring++) {
        int ring_radius = lens_radius * ring / 4;
        for (int angle = 0; angle < 360; angle += 8) {
            float rad = angle * M_PI / 180.0f;
            int x = center_x + (int)(ring_radius * cosf(rad));
            int y = center_y + (int)(ring_radius * sinf(rad) * 0.8f);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char ring_char = (ring == 1) ? '@' : (ring == 2) ? '*' : '+';
                set_pixel(buffer, zbuffer, width, height, x, y, ring_char, 2.0f);
            }
        }
    }
    
    // Scanning line effect
    float scan_angle = time * scan_speed * 360.0f;
    for (int r = 0; r < lens_radius; r++) {
        float rad = scan_angle * M_PI / 180.0f;
        int x = center_x + (int)(r * cosf(rad));
        int y = center_y + (int)(r * sinf(rad) * 0.8f);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            set_pixel(buffer, zbuffer, width, height, x, y, '|', 0.5f);
        }
    }
    
    // CCTV interference static
    if (interference > 0.3f) {
        for (int i = 0; i < (int)(interference * 100); i++) {
            int x = rand() % width;
            int y = rand() % height;
            char static_char = (rand() % 3) ? '.' : (rand() % 2) ? '*' : '#';
            set_pixel(buffer, zbuffer, width, height, x, y, static_char, 0.1f);
        }
    }
    
    // Camera housing - top and bottom
    for (int x = 0; x < width; x++) {
        // Top housing
        for (int y = 0; y < height / 6; y++) {
            if ((x + y) % 3 == 0) {
                set_pixel(buffer, zbuffer, width, height, x, y, '=', 3.0f);
            }
        }
        // Bottom housing
        for (int y = height * 5 / 6; y < height; y++) {
            if ((x + y) % 3 == 0) {
                set_pixel(buffer, zbuffer, width, height, x, y, '=', 3.0f);
            }
        }
    }
    
    // Corner brackets
    char bracket_chars[] = "[]{}";
    for (int corner = 0; corner < 4; corner++) {
        int bx = (corner % 2) ? width - 3 : 0;
        int by = (corner / 2) ? height - 1 : 0;
        
        if (bx >= 0 && bx < width && by >= 0 && by < height) {
            set_pixel(buffer, zbuffer, width, height, bx, by, bracket_chars[corner], 0.5f);
        }
    }
}

// Scene 123: Giant Eye - Single massive eye with detailed movement
void scene_123(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float pupil_dilation = params[0].value;
    float blink_speed = params[1].value;
    float gaze_intensity = params[2].value;
    
    Eye giant_eye;
    giant_eye.center.x = 0.0f;
    giant_eye.center.y = 0.0f;
    giant_eye.center.z = 8.0f;
    giant_eye.iris_radius = 4.0f + gaze_intensity * 2.0f;
    
    // Complex blinking pattern
    float blink_phase = time * blink_speed * 3.0f;
    giant_eye.blink_phase = blink_phase;
    giant_eye.is_blinking = (sinf(blink_phase) > 0.8f) || (sinf(blink_phase * 3.0f) > 0.9f);
    
    // Pupil responds to environment
    giant_eye.pupil_size = 1.0f + pupil_dilation * 3.0f + sinf(time * 5.0f) * 0.5f;
    
    // Gaze tracking with smooth movement
    float gaze_x = sinf(time * 0.8f) * 2.0f + cosf(time * 0.3f) * 1.0f;
    float gaze_y = cosf(time * 0.6f) * 1.5f + sinf(time * 0.4f) * 0.8f;
    
    giant_eye.look_direction_x = gaze_x * gaze_intensity;
    giant_eye.look_direction_y = gaze_y * gaze_intensity;
    
    // Tension and stress lines
    giant_eye.tension = 0.5f + gaze_intensity * 0.5f;
    
    draw_eye(buffer, zbuffer, width, height, &giant_eye, 3.0f);
    
    // Add detail around the eye
    float eye_radius = 15.0f;
    
    // Eyelashes
    for (int lash = 0; lash < 20; lash++) {
        float lash_angle = (lash / 20.0f) * M_PI * 2.0f;
        float lash_length = 3.0f + sinf(lash_angle * 4.0f) * 1.0f;
        
        float lash_start_x = cosf(lash_angle) * eye_radius;
        float lash_start_y = sinf(lash_angle) * eye_radius * 0.6f;
        float lash_end_x = cosf(lash_angle) * (eye_radius + lash_length);
        float lash_end_y = sinf(lash_angle) * (eye_radius + lash_length) * 0.6f;
        
        int start_x = width / 2 + lash_start_x;
        int start_y = height / 2 - lash_start_y;
        int end_x = width / 2 + lash_end_x;
        int end_y = height / 2 - lash_end_y;
        
        if (end_x >= 0 && end_x < width && end_y >= 0 && end_y < height) {
            set_pixel(buffer, zbuffer, width, height, end_x, end_y, '|', 8.5f);
        }
    }
    
    // Veins and blood vessels
    for (int vein = 0; vein < 8; vein++) {
        float vein_phase = time * 2.0f + vein * 0.8f;
        float vein_angle = vein * M_PI / 4.0f;
        
        for (int segment = 0; segment < 10; segment++) {
            float seg_distance = segment * 2.0f + sinf(vein_phase + segment * 0.5f) * 1.0f;
            float vein_x = cosf(vein_angle) * seg_distance;
            float vein_y = sinf(vein_angle) * seg_distance * 0.8f;
            
            int vx = width / 2 + vein_x;
            int vy = height / 2 - vein_y;
            
            if (vx >= 0 && vx < width && vy >= 0 && vy < height && seg_distance < eye_radius) {
                set_pixel(buffer, zbuffer, width, height, vx, vy, '-', 8.2f);
            }
        }
    }
}

// Scene 124: Crowd March - Revolutionary parade and formation
void scene_124(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float march_speed = params[0].value;
    float formation_tightness = params[1].value;
    float revolutionary_fervor = params[2].value;
    
    // Multiple marching columns
    for (int column = 0; column < 5; column++) {
        float column_x = (column - 2) * 8.0f;
        float march_offset = time * march_speed * 3.0f + column * 1.0f;
        
        // People in each column
        for (int person = 0; person < 20; person++) {
            CrowdPerson marcher;
            
            // Formation positioning
            float formation_offset = sinf(person * formation_tightness) * 2.0f;
            marcher.pos.x = column_x + formation_offset;
            marcher.pos.y = (person * 3.0f) - fmodf(march_offset, 60.0f);
            marcher.pos.z = 15.0f + column * 2.0f;
            
            // Marching rhythm
            float step_phase = march_offset + person * 0.5f;
            marcher.energy = 0.6f + revolutionary_fervor * 0.4f + sinf(step_phase * 2.0f) * 0.2f;
            
            // Different person types in formation
            if (person == 0) marcher.person_type = 2; // leader
            else if (person % 5 == 0) marcher.person_type = 0; // regular marcher with flag
            else marcher.person_type = 0;
            
            marcher.is_active = true;
            
            // Only draw if visible
            if (marcher.pos.y > -10.0f && marcher.pos.y < 15.0f) {
                draw_crowd_person(buffer, zbuffer, width, height, &marcher);
                
                // Add flags for every 5th person
                if (person % 5 == 0) {
                    int flag_x = width / 2 + marcher.pos.x * 20.0f / (marcher.pos.z + 10.0f);
                    int flag_y = height / 2 - (marcher.pos.y + 4.0f) * 20.0f / (marcher.pos.z + 10.0f);
                    
                    if (flag_x >= 0 && flag_x < width && flag_y >= 0 && flag_y < height) {
                        set_pixel(buffer, zbuffer, width, height, flag_x, flag_y, 'F', marcher.pos.z - 0.1f);
                    }
                }
            }
        }
    }
    
    // Banners across columns
    for (int banner = 0; banner < 3; banner++) {
        float banner_y = -5.0f + banner * 8.0f - fmodf(time * march_speed * 3.0f, 24.0f);
        
        if (banner_y > -8.0f && banner_y < 12.0f) {
            for (int x = -20; x <= 20; x += 4) {
                int banner_screen_x = width / 2 + x * 20.0f / 25.0f;
                int banner_screen_y = height / 2 - banner_y * 20.0f / 25.0f;
                
                if (banner_screen_x >= 0 && banner_screen_x < width && banner_screen_y >= 0 && banner_screen_y < height) {
                    set_pixel(buffer, zbuffer, width, height, banner_screen_x, banner_screen_y, '=', 20.0f);
                }
            }
        }
    }
    
    // Chanting effect - visual rhythm
    float chant_intensity = revolutionary_fervor * sinf(time * march_speed * 8.0f);
    if (chant_intensity > 0.7f) {
        for (int i = 0; i < 20; i++) {
            int chant_x = (rand() % width);
            int chant_y = (rand() % height);
            set_pixel(buffer, zbuffer, width, height, chant_x, chant_y, '!', 1.0f);
        }
    }
}

// Scene 125: Displaced Sphere - Geometric displacement with shading
void scene_125(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float displacement_amount = params[0].value;
    float geometry_complexity = params[1].value;
    float wave_frequency = params[2].value;
    
    static DisplacedVertex vertices[800];
    static bool initialized = false;
    
    if (!initialized) {
        // Generate sphere vertices
        int vertex_count = 0;
        for (int lat = 0; lat < 20 && vertex_count < 800; lat++) {
            for (int lon = 0; lon < 40 && vertex_count < 800; lon++) {
                float theta = lat * M_PI / 19.0f;
                float phi = lon * 2.0f * M_PI / 39.0f;
                
                float radius = 8.0f;
                vertices[vertex_count].pos.x = radius * sinf(theta) * cosf(phi);
                vertices[vertex_count].pos.y = radius * cosf(theta);
                vertices[vertex_count].pos.z = radius * sinf(theta) * sinf(phi) + 15.0f;
                
                // Surface normal (pointing outward)
                vertices[vertex_count].normal.x = sinf(theta) * cosf(phi);
                vertices[vertex_count].normal.y = cosf(theta);
                vertices[vertex_count].normal.z = sinf(theta) * sinf(phi);
                
                vertices[vertex_count].vertex_type = (lat + lon) % 3;
                vertex_count++;
            }
        }
        initialized = true;
    }
    
    // Update displacement and shading
    for (int i = 0; i < 800; i++) {
        // Complex displacement function
        float wave1 = sinf(time * wave_frequency + vertices[i].pos.x * 0.2f + vertices[i].pos.y * 0.1f);
        float wave2 = cosf(time * wave_frequency * 1.3f + vertices[i].pos.z * 0.15f);
        float wave3 = sinf(time * wave_frequency * 0.7f + vertices[i].pos.x * 0.3f + vertices[i].pos.z * 0.2f);
        
        vertices[i].displacement = displacement_amount * 3.0f * (wave1 + wave2 * 0.5f + wave3 * 0.3f);
        
        // Complexity adds secondary displacement
        if (geometry_complexity > 0.5f) {
            float complex_wave = sinf(time * wave_frequency * 2.0f + i * 0.1f) * geometry_complexity;
            vertices[i].displacement += complex_wave * 2.0f;
        }
        
        // Calculate shading based on displacement and position
        float light_dir_x = sinf(time * 0.5f);
        float light_dir_y = 0.7f;
        float light_dir_z = cosf(time * 0.5f);
        
        // Dot product for lighting
        float dot_product = vertices[i].normal.x * light_dir_x + 
                           vertices[i].normal.y * light_dir_y + 
                           vertices[i].normal.z * light_dir_z;
        
        vertices[i].color_intensity = 0.3f + (dot_product + 1.0f) * 0.35f;
        
        // Displacement affects shading
        float disp_factor = (vertices[i].displacement + 3.0f) / 6.0f;
        vertices[i].color_intensity = vertices[i].color_intensity * 0.7f + disp_factor * 0.3f;
        
        draw_displaced_vertex(buffer, zbuffer, width, height, &vertices[i]);
    }
    
    // Add wireframe overlay
    if (geometry_complexity > 0.7f) {
        for (int i = 0; i < 800; i += 4) {
            DisplacedVertex* v1 = &vertices[i];
            DisplacedVertex* v2 = &vertices[(i + 1) % 800];
            
            // Draw line between vertices
            float dx = v2->pos.x - v1->pos.x;
            float dy = v2->pos.y - v1->pos.y;
            float dz = v2->pos.z - v1->pos.z;
            float distance = sqrtf(dx*dx + dy*dy + dz*dz);
            
            if (distance < 4.0f) { // Only connect close vertices
                for (int step = 0; step < 10; step++) {
                    float t = step / 9.0f;
                    float line_x = v1->pos.x + dx * t;
                    float line_y = v1->pos.y + dy * t;
                    float line_z = v1->pos.z + dz * t;
                    
                    int screen_x = width / 2 + line_x * 15.0f / (line_z + 8.0f);
                    int screen_y = height / 2 - line_y * 15.0f / (line_z + 8.0f);
                    
                    if (screen_x >= 0 && screen_x < width && screen_y >= 0 && screen_y < height) {
                        set_pixel(buffer, zbuffer, width, height, screen_x, screen_y, '-', line_z + 0.1f);
                    }
                }
            }
        }
    }
}

// Scene 126: Morphing Cube - Geometric transformation with displacement
void scene_126(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float morph_speed = params[0].value;
    float displacement_chaos = params[1].value;
    float surface_detail = params[2].value;
    
    static DisplacedVertex cube_vertices[216]; // 6 faces × 6×6 vertices
    static bool initialized = false;
    
    if (!initialized) {
        int vertex_count = 0;
        
        // Generate cube faces with subdivision
        for (int face = 0; face < 6; face++) {
            for (int u = 0; u < 6; u++) {
                for (int v = 0; v < 6; v++) {
                    float size = 6.0f;
                    float uf = (u / 5.0f - 0.5f) * size;
                    float vf = (v / 5.0f - 0.5f) * size;
                    
                    switch(face) {
                        case 0: // Front
                            cube_vertices[vertex_count].pos.x = uf;
                            cube_vertices[vertex_count].pos.y = vf;
                            cube_vertices[vertex_count].pos.z = size/2 + 15.0f;
                            cube_vertices[vertex_count].normal.x = 0; cube_vertices[vertex_count].normal.y = 0; cube_vertices[vertex_count].normal.z = 1;
                            break;
                        case 1: // Back
                            cube_vertices[vertex_count].pos.x = -uf;
                            cube_vertices[vertex_count].pos.y = vf;
                            cube_vertices[vertex_count].pos.z = -size/2 + 15.0f;
                            cube_vertices[vertex_count].normal.x = 0; cube_vertices[vertex_count].normal.y = 0; cube_vertices[vertex_count].normal.z = -1;
                            break;
                        case 2: // Right
                            cube_vertices[vertex_count].pos.x = size/2;
                            cube_vertices[vertex_count].pos.y = vf;
                            cube_vertices[vertex_count].pos.z = uf + 15.0f;
                            cube_vertices[vertex_count].normal.x = 1; cube_vertices[vertex_count].normal.y = 0; cube_vertices[vertex_count].normal.z = 0;
                            break;
                        case 3: // Left
                            cube_vertices[vertex_count].pos.x = -size/2;
                            cube_vertices[vertex_count].pos.y = vf;
                            cube_vertices[vertex_count].pos.z = -uf + 15.0f;
                            cube_vertices[vertex_count].normal.x = -1; cube_vertices[vertex_count].normal.y = 0; cube_vertices[vertex_count].normal.z = 0;
                            break;
                        case 4: // Top
                            cube_vertices[vertex_count].pos.x = uf;
                            cube_vertices[vertex_count].pos.y = size/2;
                            cube_vertices[vertex_count].pos.z = vf + 15.0f;
                            cube_vertices[vertex_count].normal.x = 0; cube_vertices[vertex_count].normal.y = 1; cube_vertices[vertex_count].normal.z = 0;
                            break;
                        case 5: // Bottom
                            cube_vertices[vertex_count].pos.x = uf;
                            cube_vertices[vertex_count].pos.y = -size/2;
                            cube_vertices[vertex_count].pos.z = -vf + 15.0f;
                            cube_vertices[vertex_count].normal.x = 0; cube_vertices[vertex_count].normal.y = -1; cube_vertices[vertex_count].normal.z = 0;
                            break;
                    }
                    
                    cube_vertices[vertex_count].vertex_type = face;
                    vertex_count++;
                    
                    if (vertex_count >= 216) break;
                }
                if (vertex_count >= 216) break;
            }
            if (vertex_count >= 216) break;
        }
        initialized = true;
    }
    
    // Apply morphing transformations
    for (int i = 0; i < 216; i++) {
        // Base rotation
        float rotation_speed = morph_speed * 2.0f;
        float rx = time * rotation_speed;
        float ry = time * rotation_speed * 0.7f;
        float rz = time * rotation_speed * 1.3f;
        
        // Rotation matrices
        float cos_rx = cosf(rx), sin_rx = sinf(rx);
        float cos_ry = cosf(ry), sin_ry = sinf(ry);
        float cos_rz = cosf(rz), sin_rz = sinf(rz);
        
        // Original position
        float ox = cube_vertices[i].pos.x;
        float oy = cube_vertices[i].pos.y;
        float oz = cube_vertices[i].pos.z - 15.0f;
        
        // Apply rotations
        float x1 = ox;
        float y1 = oy * cos_rx - oz * sin_rx;
        float z1 = oy * sin_rx + oz * cos_rx;
        
        float x2 = x1 * cos_ry + z1 * sin_ry;
        float y2 = y1;
        float z2 = -x1 * sin_ry + z1 * cos_ry;
        
        cube_vertices[i].pos.x = x2 * cos_rz - y2 * sin_rz;
        cube_vertices[i].pos.y = x2 * sin_rz + y2 * cos_rz;
        cube_vertices[i].pos.z = z2 + 15.0f;
        
        // Morph displacement
        float morph_phase = time * morph_speed + i * 0.05f;
        float sphere_influence = (sinf(morph_phase) + 1.0f) * 0.5f;
        
        // Calculate distance from center for sphere morph
        float dist_from_center = sqrtf(ox*ox + oy*oy + oz*oz);
        float sphere_radius = 6.0f;
        
        if (dist_from_center > 0.1f) {
            float sphere_factor = sphere_radius / dist_from_center;
            cube_vertices[i].pos.x = cube_vertices[i].pos.x * (1.0f - sphere_influence) + 
                                    (ox * sphere_factor) * sphere_influence;
            cube_vertices[i].pos.y = cube_vertices[i].pos.y * (1.0f - sphere_influence) + 
                                    (oy * sphere_factor) * sphere_influence;
            cube_vertices[i].pos.z = cube_vertices[i].pos.z * (1.0f - sphere_influence) + 
                                    (oz * sphere_factor + 15.0f) * sphere_influence;
        }
        
        // Chaos displacement
        float chaos_x = sinf(time * 3.0f + i * 0.1f + cube_vertices[i].pos.x * 0.3f);
        float chaos_y = cosf(time * 2.5f + i * 0.15f + cube_vertices[i].pos.y * 0.3f);
        float chaos_z = sinf(time * 2.0f + i * 0.12f + cube_vertices[i].pos.z * 0.2f);
        
        cube_vertices[i].displacement = displacement_chaos * 2.0f * (chaos_x + chaos_y + chaos_z) / 3.0f;
        
        // Surface detail
        if (surface_detail > 0.3f) {
            float detail_wave = sinf(time * 4.0f + i * 0.2f) * surface_detail;
            cube_vertices[i].displacement += detail_wave;
        }
        
        // Update normal for rotated face
        float nx = cube_vertices[i].normal.x;
        float ny = cube_vertices[i].normal.y;
        float nz = cube_vertices[i].normal.z;
        
        // Rotate normal
        float nx1 = nx;
        float ny1 = ny * cos_rx - nz * sin_rx;
        float nz1 = ny * sin_rx + nz * cos_rx;
        
        float nx2 = nx1 * cos_ry + nz1 * sin_ry;
        float ny2 = ny1;
        float nz2 = -nx1 * sin_ry + nz1 * cos_ry;
        
        cube_vertices[i].normal.x = nx2 * cos_rz - ny2 * sin_rz;
        cube_vertices[i].normal.y = nx2 * sin_rz + ny2 * cos_rz;
        cube_vertices[i].normal.z = nz2;
        
        // Lighting calculation
        float light_x = sinf(time * 0.3f);
        float light_y = 0.8f;
        float light_z = cosf(time * 0.3f);
        
        float dot_product = cube_vertices[i].normal.x * light_x + 
                           cube_vertices[i].normal.y * light_y + 
                           cube_vertices[i].normal.z * light_z;
        
        cube_vertices[i].color_intensity = 0.2f + (dot_product + 1.0f) * 0.4f;
        
        draw_displaced_vertex(buffer, zbuffer, width, height, &cube_vertices[i]);
    }
}

// Scene 127: Protest Rally - Large gathering with speakers
void scene_127(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float crowd_energy = params[0].value;
    float speaker_volume = params[1].value;
    float rally_size = params[2].value;
    
    // Central stage/platform
    float stage_z = 30.0f;
    for (int x = -6; x <= 6; x++) {
        for (int y = -2; y <= 2; y++) {
            int stage_x = width / 2 + x * 20.0f / (stage_z + 10.0f);
            int stage_y = height / 2 - y * 20.0f / (stage_z + 10.0f);
            
            if (stage_x >= 0 && stage_x < width && stage_y >= 0 && stage_y < height) {
                set_pixel(buffer, zbuffer, width, height, stage_x, stage_y, '=', stage_z);
            }
        }
    }
    
    // Speaker on stage
    CrowdPerson speaker;
    speaker.pos.x = sinf(time * 0.5f) * 2.0f;
    speaker.pos.y = 3.0f;
    speaker.pos.z = stage_z - 3.0f;
    speaker.person_type = 2; // leader
    speaker.energy = 0.8f + speaker_volume * 0.2f + sinf(time * 8.0f) * 0.1f;
    speaker.is_active = true;
    
    draw_crowd_person(buffer, zbuffer, width, height, &speaker);
    
    // Crowd arranged in semi-circle
    int total_crowd = (int)(rally_size * 150) + 50;
    for (int person = 0; person < total_crowd && person < 200; person++) {
        float crowd_angle = (person / (float)total_crowd) * M_PI + M_PI * 0.25f;
        float crowd_distance = 8.0f + (person % 20) * 1.5f;
        
        CrowdPerson crowd_member;
        crowd_member.pos.x = cosf(crowd_angle) * crowd_distance;
        crowd_member.pos.y = sinf(crowd_angle) * crowd_distance * 0.3f - 5.0f;
        crowd_member.pos.z = 20.0f + (person % 15);
        
        // Crowd response to speaker
        float response_phase = time * 2.0f + person * 0.1f;
        crowd_member.energy = 0.4f + crowd_energy * 0.5f + sinf(response_phase) * 0.2f;
        
        // Different types distributed in crowd
        if (person % 20 == 0) crowd_member.person_type = 2; // local leader
        else if (person % 15 == 0) crowd_member.person_type = 3; // medic/organizer
        else crowd_member.person_type = 0; // regular protester
        
        crowd_member.is_active = true;
        
        // Crowd movement - swaying and reactions
        float sway = sinf(time * 1.5f + crowd_angle * 3.0f) * crowd_energy;
        crowd_member.pos.x += sway * 0.5f;
        
        draw_crowd_person(buffer, zbuffer, width, height, &crowd_member);
    }
    
    // Speaker's voice visualization - sound waves
    if (speaker_volume > 0.4f) {
        for (int wave = 0; wave < 8; wave++) {
            float wave_radius = time * 15.0f + wave * 3.0f;
            float wave_strength = speaker_volume * (1.0f - wave * 0.1f);
            
            if (wave_strength > 0.1f) {
                for (int angle = 0; angle < 360; angle += 20) {
                    float rad = angle * M_PI / 180.0f;
                    float wave_x = speaker.pos.x + cosf(rad) * wave_radius * 0.8f;
                    float wave_y = speaker.pos.y + sinf(rad) * wave_radius * 0.3f;
                    float wave_z = speaker.pos.z;
                    
                    int wx = width / 2 + wave_x * 20.0f / (wave_z + 10.0f);
                    int wy = height / 2 - wave_y * 20.0f / (wave_z + 10.0f);
                    
                    if (wx >= 0 && wx < width && wy >= 0 && wy < height) {
                        char wave_char = (wave_strength > 0.7f) ? '~' : '.';
                        set_pixel(buffer, zbuffer, width, height, wx, wy, wave_char, wave_z + 0.5f);
                    }
                }
            }
        }
    }
    
    // Banners and signs held up
    for (int banner = 0; banner < 12; banner++) {
        float banner_angle = banner * M_PI / 6.0f;
        float banner_distance = 10.0f + (banner % 3) * 3.0f;
        
        float banner_x = cosf(banner_angle) * banner_distance;
        float banner_y = sinf(banner_angle) * banner_distance * 0.3f - 3.0f + sinf(time + banner) * 1.0f;
        float banner_z = 15.0f + banner * 2.0f;
        
        // Banner pole
        for (int pole = 0; pole < 6; pole++) {
            int bx = width / 2 + banner_x * 20.0f / (banner_z + 10.0f);
            int by = height / 2 - (banner_y + pole) * 20.0f / (banner_z + 10.0f);
            
            if (bx >= 0 && bx < width && by >= 0 && by < height) {
                set_pixel(buffer, zbuffer, width, height, bx, by, '|', banner_z);
            }
        }
        
        // Banner cloth
        for (int cloth = -2; cloth <= 2; cloth++) {
            int bx = width / 2 + (banner_x + cloth) * 20.0f / (banner_z + 10.0f);
            int by = height / 2 - (banner_y + 6.0f) * 20.0f / (banner_z + 10.0f);
            
            if (bx >= 0 && bx < width && by >= 0 && by < height) {
                set_pixel(buffer, zbuffer, width, height, bx, by, '=', banner_z - 0.1f);
            }
        }
    }
}

// Scene 128: Surveillance Eyes - Multiple tracking eyes with paranoia theme
void scene_128(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float surveillance_intensity = params[0].value;
    float paranoia_level = params[1].value;
    float tracking_precision = params[2].value;
    
    static Eye surveillance_eyes[12];
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < 12; i++) {
            // Position eyes around the perimeter
            float angle = i * M_PI * 2.0f / 12.0f;
            surveillance_eyes[i].center.x = cosf(angle) * 12.0f;
            surveillance_eyes[i].center.y = sinf(angle) * 8.0f;
            surveillance_eyes[i].center.z = 5.0f + (i % 4) * 3.0f;
            surveillance_eyes[i].iris_radius = 1.5f + (i % 3) * 0.5f;
            surveillance_eyes[i].pupil_size = 0.8f;
        }
        initialized = true;
    }
    
    // Central target being watched
    float target_x = sinf(time * 0.8f) * 4.0f;
    float target_y = cosf(time * 0.6f) * 3.0f;
    
    for (int i = 0; i < 12; i++) {
        // All eyes track the central target
        float dx = target_x - surveillance_eyes[i].center.x;
        float dy = target_y - surveillance_eyes[i].center.y;
        float distance = sqrtf(dx*dx + dy*dy);
        
        if (distance > 0.1f) {
            float tracking_speed = tracking_precision * 0.5f + 0.5f;
            surveillance_eyes[i].look_direction_x += (dx / distance - surveillance_eyes[i].look_direction_x) * tracking_speed;
            surveillance_eyes[i].look_direction_y += (dy / distance - surveillance_eyes[i].look_direction_y) * tracking_speed;
        }
        
        // Paranoia affects pupil dilation and blink patterns
        surveillance_eyes[i].pupil_size = 0.5f + paranoia_level * 1.5f + sinf(time * 6.0f + i * 0.5f) * 0.3f;
        
        // Surveillance intensity affects blink rate
        float blink_frequency = 1.0f + surveillance_intensity * 3.0f;
        surveillance_eyes[i].blink_phase = time * blink_frequency + i * 0.8f;
        surveillance_eyes[i].is_blinking = (sinf(surveillance_eyes[i].blink_phase) > 0.85f);
        
        // Tension based on surveillance level
        surveillance_eyes[i].tension = 0.3f + surveillance_intensity * 0.7f;
        
        draw_eye(buffer, zbuffer, width, height, &surveillance_eyes[i], 1.0f + surveillance_intensity * 0.5f);
    }
    
    // Draw the target being watched
    int target_screen_x = width / 2 + target_x * 5.0f;
    int target_screen_y = height / 2 - target_y * 5.0f;
    
    if (target_screen_x >= 0 && target_screen_x < width && target_screen_y >= 0 && target_screen_y < height) {
        set_pixel(buffer, zbuffer, width, height, target_screen_x, target_screen_y, '@', 3.0f);
    }
    
    // Surveillance grid overlay
    if (surveillance_intensity > 0.6f) {
        for (int grid_x = 0; grid_x < width; grid_x += 8) {
            for (int grid_y = 0; grid_y < height; grid_y += 6) {
                if ((grid_x + grid_y) % 2 == 0) {
                    set_pixel(buffer, zbuffer, width, height, grid_x, grid_y, '+', 10.0f);
                }
            }
        }
    }
    
    // Targeting lines converging on target
    if (paranoia_level > 0.7f) {
        for (int i = 0; i < 12; i++) {
            float eye_screen_x = width / 2 + surveillance_eyes[i].center.x * 20.0f / (surveillance_eyes[i].center.z + 10.0f);
            float eye_screen_y = height / 2 - surveillance_eyes[i].center.y * 20.0f / (surveillance_eyes[i].center.z + 10.0f);
            
            // Draw line to target
            float line_dx = target_screen_x - eye_screen_x;
            float line_dy = target_screen_y - eye_screen_y;
            float line_length = sqrtf(line_dx*line_dx + line_dy*line_dy);
            
            if (line_length > 1.0f) {
                for (int step = 0; step < 10; step++) {
                    float t = step / 9.0f;
                    int line_x = (int)(eye_screen_x + line_dx * t);
                    int line_y = (int)(eye_screen_y + line_dy * t);
                    
                    if (line_x >= 0 && line_x < width && line_y >= 0 && line_y < height) {
                        set_pixel(buffer, zbuffer, width, height, line_x, line_y, '.', 8.0f);
                    }
                }
            }
        }
    }
    
    // Scanning effect
    float scan_line = fmodf(time * surveillance_intensity * 5.0f, height);
    for (int x = 0; x < width; x++) {
        int scan_y = (int)scan_line;
        if (scan_y >= 0 && scan_y < height) {
            set_pixel(buffer, zbuffer, width, height, x, scan_y, '-', 2.0f);
        }
    }
}

// Scene 129: Fractal Displacement - Complex geometric patterns
void scene_129(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float fractal_depth = params[0].value;
    float displacement_complexity = params[1].value;
    float pattern_evolution = params[2].value;
    
    static DisplacedVertex fractal_vertices[1000];
    static bool initialized = false;
    
    if (!initialized) {
        // Generate initial fractal pattern
        for (int i = 0; i < 1000; i++) {
            float angle = i * M_PI * 2.0f / 1000.0f;
            float radius = 8.0f + sinf(angle * 5.0f) * 3.0f;
            
            fractal_vertices[i].pos.x = cosf(angle) * radius;
            fractal_vertices[i].pos.y = sinf(angle) * radius;
            fractal_vertices[i].pos.z = 15.0f + sinf(angle * 3.0f) * 5.0f;
            
            // Normal points outward from center
            float normal_length = sqrtf(fractal_vertices[i].pos.x * fractal_vertices[i].pos.x + 
                                       fractal_vertices[i].pos.y * fractal_vertices[i].pos.y);
            if (normal_length > 0.1f) {
                fractal_vertices[i].normal.x = fractal_vertices[i].pos.x / normal_length;
                fractal_vertices[i].normal.y = fractal_vertices[i].pos.y / normal_length;
                fractal_vertices[i].normal.z = 0.0f;
            }
            
            fractal_vertices[i].vertex_type = i % 5;
        }
        initialized = true;
    }
    
    for (int i = 0; i < 1000; i++) {
        // Complex fractal displacement function
        float angle = i * M_PI * 2.0f / 1000.0f;
        float evolution_phase = time * pattern_evolution;
        
        // Multiple levels of detail
        float level1 = sinf(angle * 3.0f + evolution_phase);
        float level2 = sinf(angle * 7.0f + evolution_phase * 1.3f) * 0.5f;
        float level3 = sinf(angle * 13.0f + evolution_phase * 0.7f) * 0.25f;
        float level4 = sinf(angle * 21.0f + evolution_phase * 1.7f) * 0.125f;
        
        // Combine fractal levels based on depth parameter
        float fractal_displacement = level1;
        if (fractal_depth > 0.25f) fractal_displacement += level2;
        if (fractal_depth > 0.5f) fractal_displacement += level3;
        if (fractal_depth > 0.75f) fractal_displacement += level4;
        
        // Additional complexity displacement
        float complexity_wave1 = sinf(time * 2.0f + fractal_vertices[i].pos.x * 0.5f + fractal_vertices[i].pos.y * 0.3f);
        float complexity_wave2 = cosf(time * 1.5f + fractal_vertices[i].pos.z * 0.2f + angle * 5.0f);
        
        float complexity_displacement = (complexity_wave1 + complexity_wave2) * displacement_complexity;
        
        fractal_vertices[i].displacement = fractal_displacement * 3.0f + complexity_displacement * 2.0f;
        
        // Evolving color based on displacement and position
        float color_factor = (fractal_vertices[i].displacement + 5.0f) / 10.0f;
        float position_factor = (sinf(angle * 2.0f + evolution_phase) + 1.0f) / 2.0f;
        
        fractal_vertices[i].color_intensity = color_factor * 0.6f + position_factor * 0.4f;
        
        // Clamp color intensity
        if (fractal_vertices[i].color_intensity > 1.0f) fractal_vertices[i].color_intensity = 1.0f;
        if (fractal_vertices[i].color_intensity < 0.0f) fractal_vertices[i].color_intensity = 0.0f;
        
        draw_displaced_vertex(buffer, zbuffer, width, height, &fractal_vertices[i]);
    }
    
    // Add secondary fractal layer for depth > 0.8
    if (fractal_depth > 0.8f) {
        for (int i = 0; i < 500; i++) {
            float secondary_angle = i * M_PI * 4.0f / 500.0f;
            float secondary_radius = 4.0f + sinf(secondary_angle * 8.0f + time * pattern_evolution * 2.0f) * 2.0f;
            
            DisplacedVertex secondary_vertex;
            secondary_vertex.pos.x = cosf(secondary_angle) * secondary_radius;
            secondary_vertex.pos.y = sinf(secondary_angle) * secondary_radius;
            secondary_vertex.pos.z = 12.0f + cosf(secondary_angle * 4.0f + time * 2.0f) * 3.0f;
            
            // Secondary displacement
            float secondary_disp = sinf(secondary_angle * 11.0f + time * pattern_evolution * 1.5f) * displacement_complexity;
            secondary_vertex.displacement = secondary_disp * 1.5f;
            
            secondary_vertex.color_intensity = 0.3f + (secondary_disp + 1.0f) * 0.35f;
            
            draw_displaced_vertex(buffer, zbuffer, width, height, &secondary_vertex);
        }
    }
    
    // Connecting lines for pattern visualization
    if (displacement_complexity > 0.7f) {
        for (int i = 0; i < 1000; i += 5) {
            DisplacedVertex* v1 = &fractal_vertices[i];
            DisplacedVertex* v2 = &fractal_vertices[(i + 5) % 1000];
            
            // Draw connecting line
            for (int step = 0; step < 8; step++) {
                float t = step / 7.0f;
                float line_x = v1->pos.x + (v2->pos.x - v1->pos.x) * t;
                float line_y = v1->pos.y + (v2->pos.y - v1->pos.y) * t;
                float line_z = v1->pos.z + (v2->pos.z - v1->pos.z) * t;
                
                int screen_x = width / 2 + line_x * 15.0f / (line_z + 8.0f);
                int screen_y = height / 2 - line_y * 15.0f / (line_z + 8.0f);
                
                if (screen_x >= 0 && screen_x < width && screen_y >= 0 && screen_y < height) {
                    set_pixel(buffer, zbuffer, width, height, screen_x, screen_y, '.', line_z + 0.2f);
                }
            }
        }
    }
}

// ============= FILM NOIR SCENES (130-139) =============

// Scene 130: Venetian Blinds - Classic film noir shadow pattern
void scene_130(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float blind_angle = params[0].value;
    float light_intensity = params[1].value;
    float zoom = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float rotation = time * 0.1f + blind_angle * M_PI;
    float scale = 1.0f + zoom * 2.0f;
    
    // Venetian blind slats
    int blind_count = 15;
    int blind_spacing = (int)(height * scale / blind_count);
    
    for (int blind = 0; blind < blind_count; blind++) {
        int base_y = blind * blind_spacing - (int)(time * 20.0f) % (blind_spacing * blind_count);
        
        for (int x = 0; x < width; x++) {
            for (int thickness = 0; thickness < 3; thickness++) {
                int y = base_y + thickness;
                
                // Rotation effect
                int rotated_x = x + (int)(sinf(rotation + x * 0.02f) * 3.0f);
                int rotated_y = y + (int)(cosf(rotation + y * 0.01f) * 2.0f);
                
                if (rotated_x >= 0 && rotated_x < width && rotated_y >= 0 && rotated_y < height) {
                    char blind_char = (thickness == 1) ? '#' : '=';
                    set_pixel(buffer, zbuffer, width, height, rotated_x, rotated_y, blind_char, 1.0f);
                }
            }
        }
    }
    
    // Light rays through blinds
    for (int ray = 0; ray < width; ray += 8) {
        float ray_intensity = light_intensity * (sinf(time * 2.0f + ray * 0.1f) * 0.5f + 0.5f);
        
        if (ray_intensity > 0.4f) {
            for (int y = 0; y < height; y++) {
                if (y % blind_spacing > blind_spacing - 6) { // Between blinds
                    int x = ray + (int)(sinf(y * 0.05f + time) * ray_intensity * 3.0f);
                    if (x >= 0 && x < width) {
                        char light_char = (ray_intensity > 0.7f) ? '|' : '.';
                        set_pixel(buffer, zbuffer, width, height, x, y, light_char, 0.5f);
                    }
                }
            }
        }
    }
    
    // Window frame
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, 0, '#', 0.1f);
        set_pixel(buffer, zbuffer, width, height, x, height-1, '#', 0.1f);
    }
    for (int y = 0; y < height; y++) {
        set_pixel(buffer, zbuffer, width, height, 0, y, '#', 0.1f);
        set_pixel(buffer, zbuffer, width, height, width-1, y, '#', 0.1f);
    }
}

// Scene 131: Silhouette Doorway - Mystery figure in doorframe
void scene_131(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float figure_position = params[0].value;
    float door_angle = params[1].value;
    float atmosphere = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    // Doorframe
    int door_left = width / 3;
    int door_right = width * 2 / 3;
    int door_top = height / 8;
    int door_bottom = height;
    
    // Door frame outline
    for (int y = door_top; y < door_bottom; y++) {
        set_pixel(buffer, zbuffer, width, height, door_left, y, '#', 1.0f);
        set_pixel(buffer, zbuffer, width, height, door_right, y, '#', 1.0f);
    }
    for (int x = door_left; x <= door_right; x++) {
        set_pixel(buffer, zbuffer, width, height, x, door_top, '#', 1.0f);
    }
    
    // Door (slightly ajar)
    int door_edge = door_right - (int)(door_angle * (door_right - door_left) * 0.8f);
    for (int y = door_top + 1; y < door_bottom; y++) {
        for (int x = door_edge; x < door_right; x++) {
            if ((x + y) % 3 != 0) {
                set_pixel(buffer, zbuffer, width, height, x, y, '|', 2.0f);
            }
        }
    }
    
    // Silhouette figure
    int figure_x = door_left + (int)((door_edge - door_left) * figure_position);
    int figure_height = height * 3 / 4;
    int figure_width = width / 12;
    
    // Head
    for (int y = door_top + height / 6; y < door_top + height / 4; y++) {
        for (int x = figure_x - figure_width/4; x <= figure_x + figure_width/4; x++) {
            if (x >= door_left && x < door_edge) {
                set_pixel(buffer, zbuffer, width, height, x, y, '@', 0.5f);
            }
        }
    }
    
    // Body
    for (int y = door_top + height / 4; y < door_bottom - height / 6; y++) {
        for (int x = figure_x - figure_width/3; x <= figure_x + figure_width/3; x++) {
            if (x >= door_left && x < door_edge) {
                char body_char = (y % 3 == 0) ? '#' : '@';
                set_pixel(buffer, zbuffer, width, height, x, y, body_char, 0.5f);
            }
        }
    }
    
    // Legs
    for (int y = door_bottom - height / 6; y < door_bottom; y++) {
        // Left leg
        for (int x = figure_x - figure_width/3; x <= figure_x - figure_width/6; x++) {
            if (x >= door_left && x < door_edge) {
                set_pixel(buffer, zbuffer, width, height, x, y, '|', 0.5f);
            }
        }
        // Right leg
        for (int x = figure_x + figure_width/6; x <= figure_x + figure_width/3; x++) {
            if (x >= door_left && x < door_edge) {
                set_pixel(buffer, zbuffer, width, height, x, y, '|', 0.5f);
            }
        }
    }
    
    // Light spill from doorway
    for (int x = door_left + 1; x < door_edge; x++) {
        for (int y = door_top + 1; y < door_bottom; y++) {
            if ((x + y) % 4 == 0 && atmosphere > 0.3f) {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 3.0f);
            }
        }
    }
    
    // Atmospheric haze
    if (atmosphere > 0.5f) {
        for (int i = 0; i < (int)(atmosphere * 50); i++) {
            int x = rand() % width;
            int y = rand() % height;
            set_pixel(buffer, zbuffer, width, height, x, y, '.', 4.0f);
        }
    }
}

// Scene 132: Rain on Window - Water drops and distortion
void scene_132(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float rain_intensity = params[0].value;
    float wind_speed = params[1].value;
    float zoom = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    static float drops[100][4]; // x, y, size, age
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < 100; i++) {
            drops[i][0] = rand() % width;   // x
            drops[i][1] = rand() % height;  // y
            drops[i][2] = 1 + rand() % 4;   // size
            drops[i][3] = rand() % 100;     // age
        }
        initialized = true;
    }
    
    // Update rain drops
    for (int i = 0; i < (int)(rain_intensity * 100); i++) {
        drops[i][3] += 1.0f; // age
        
        // Rain motion
        drops[i][1] += 2.0f + rain_intensity * 3.0f; // Fall down
        drops[i][0] += wind_speed * sinf(time + i) * 0.5f; // Wind drift
        
        // Reset drop if it falls off screen
        if (drops[i][1] > height || drops[i][0] < 0 || drops[i][0] >= width) {
            drops[i][0] = rand() % width;
            drops[i][1] = -10;
            drops[i][2] = 1 + rand() % 4;
            drops[i][3] = 0;
        }
        
        // Draw rain drop
        int x = (int)drops[i][0];
        int y = (int)drops[i][1];
        int size = (int)drops[i][2];
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            // Drop center
            set_pixel(buffer, zbuffer, width, height, x, y, 'o', 1.0f);
            
            // Drop trail
            for (int trail = 1; trail < size + 2; trail++) {
                int trail_y = y + trail;
                if (trail_y < height) {
                    char trail_char = (trail == 1) ? '*' : '.';
                    set_pixel(buffer, zbuffer, width, height, x, trail_y, trail_char, 1.5f);
                }
            }
        }
    }
    
    // Window condensation pattern
    for (int y = 0; y < height; y += 3) {
        for (int x = 0; x < width; x += 4) {
            float condensation = sinf(x * 0.1f + time * 0.5f) * cosf(y * 0.08f + time * 0.3f);
            if (condensation > 0.3f) {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 2.0f);
            }
        }
    }
    
    // Distorted view through glass
    if (zoom > 0.4f) {
        for (int distort = 0; distort < 20; distort++) {
            float distort_x = (distort * width / 20.0f) + sinf(time + distort) * zoom * 10.0f;
            float distort_y = height / 2 + cosf(time * 1.3f + distort) * zoom * 15.0f;
            
            int dx = (int)distort_x;
            int dy = (int)distort_y;
            
            if (dx >= 0 && dx < width && dy >= 0 && dy < height) {
                char distort_char = (zoom > 0.7f) ? '@' : '*';
                set_pixel(buffer, zbuffer, width, height, dx, dy, distort_char, 0.8f);
            }
        }
    }
}

// Scene 133: Detective Silhouette - Hat and coat figure
void scene_133(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float detective_position = params[0].value;
    float coat_billow = params[1].value;
    float atmosphere = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    int center_x = width / 2 + (int)((detective_position - 0.5f) * width * 0.3f);
    int figure_bottom = height * 9 / 10;
    
    // Detective hat
    int hat_width = width / 8;
    int hat_y = height / 6;
    
    // Hat brim
    for (int x = center_x - hat_width; x <= center_x + hat_width; x++) {
        if (x >= 0 && x < width) {
            set_pixel(buffer, zbuffer, width, height, x, hat_y + hat_width/3, '#', 1.0f);
        }
    }
    
    // Hat crown
    for (int y = hat_y; y <= hat_y + hat_width/2; y++) {
        for (int x = center_x - hat_width/2; x <= center_x + hat_width/2; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, x, y, '#', 1.0f);
            }
        }
    }
    
    // Head/neck
    int neck_start = hat_y + hat_width/2 + 2;
    for (int y = neck_start; y < neck_start + height/12; y++) {
        for (int x = center_x - width/16; x <= center_x + width/16; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, x, y, '@', 1.0f);
            }
        }
    }
    
    // Coat collar
    int collar_start = neck_start + height/12;
    for (int y = collar_start; y < collar_start + height/10; y++) {
        int collar_width = width/8 + (int)(coat_billow * width/16);
        for (int x = center_x - collar_width; x <= center_x + collar_width; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char collar_char = ((x + y) % 3 == 0) ? '#' : '@';
                set_pixel(buffer, zbuffer, width, height, x, y, collar_char, 1.0f);
            }
        }
    }
    
    // Long coat body
    int coat_start = collar_start + height/10;
    for (int y = coat_start; y < figure_bottom; y++) {
        float billow_offset = sinf(time * 2.0f + y * 0.1f) * coat_billow * width/20;
        int coat_width = width/6 + (int)billow_offset + (y - coat_start) / 8; // Widens toward bottom
        
        for (int x = center_x - coat_width; x <= center_x + coat_width; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char coat_char = ((x + y) % 4 == 0) ? '#' : '@';
                set_pixel(buffer, zbuffer, width, height, x, y, coat_char, 1.0f);
            }
        }
    }
    
    // Arms
    int arm_y = collar_start + height/6;
    int arm_length = width/5;
    
    // Left arm
    for (int x = center_x - width/8; x >= center_x - width/8 - arm_length; x--) {
        int arm_y_pos = arm_y + (int)(sinf(time + x * 0.1f) * coat_billow * 3.0f);
        if (x >= 0 && x < width && arm_y_pos >= 0 && arm_y_pos < height) {
            set_pixel(buffer, zbuffer, width, height, x, arm_y_pos, '@', 1.2f);
        }
    }
    
    // Right arm
    for (int x = center_x + width/8; x <= center_x + width/8 + arm_length; x++) {
        int arm_y_pos = arm_y + (int)(sinf(time + x * 0.1f) * coat_billow * 3.0f);
        if (x >= 0 && x < width && arm_y_pos >= 0 && arm_y_pos < height) {
            set_pixel(buffer, zbuffer, width, height, x, arm_y_pos, '@', 1.2f);
        }
    }
    
    // Cigarette smoke
    if (atmosphere > 0.6f) {
        for (int smoke = 0; smoke < 8; smoke++) {
            float smoke_x = center_x + width/8 + arm_length + sinf(time * 2.0f + smoke) * 5.0f;
            float smoke_y = arm_y - smoke * 3 + cosf(time * 1.5f + smoke) * 3.0f;
            
            int sx = (int)smoke_x;
            int sy = (int)smoke_y;
            
            if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                char smoke_char = (smoke < 3) ? '*' : '.';
                set_pixel(buffer, zbuffer, width, height, sx, sy, smoke_char, 0.5f);
            }
        }
    }
}

// Scene 134: Femme Fatale - Elegant silhouette with curves
void scene_134(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float pose_angle = params[0].value;
    float dress_flow = params[1].value;
    float zoom = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float scale = 1.0f + zoom * 0.5f;
    int center_x = width / 2;
    int figure_bottom = height * 9 / 10;
    
    // Hair - flowing and elegant
    int hair_top = (int)(height / 8 * scale);
    for (int strand = 0; strand < 12; strand++) {
        float hair_wave = sinf(time * 1.5f + strand * 0.5f) * dress_flow;
        float hair_angle = pose_angle * M_PI + strand * 0.1f;
        
        for (int seg = 0; seg < 15; seg++) {
            int hair_x = center_x + (int)((strand - 6) * 3 * scale + hair_wave * 8.0f);
            int hair_y = hair_top + seg * 2;
            
            hair_x += (int)(sinf(hair_angle + seg * 0.2f) * 4.0f * scale);
            
            if (hair_x >= 0 && hair_x < width && hair_y >= 0 && hair_y < height) {
                char hair_char = (seg < 5) ? '*' : '.';
                set_pixel(buffer, zbuffer, width, height, hair_x, hair_y, hair_char, 1.0f);
            }
        }
    }
    
    // Head and neck
    int neck_y = hair_top + (int)(30 * scale);
    for (int y = hair_top + (int)(15 * scale); y <= neck_y; y++) {
        for (int x = center_x - (int)(8 * scale); x <= center_x + (int)(8 * scale); x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, x, y, '@', 1.0f);
            }
        }
    }
    
    // Shoulders and upper body - curved
    int shoulder_y = neck_y + (int)(10 * scale);
    for (int y = shoulder_y; y < shoulder_y + (int)(40 * scale); y++) {
        float body_curve = cosf((y - shoulder_y) * 0.1f) * pose_angle * 5.0f;
        int body_width = (int)(12 * scale + sinf((y - shoulder_y) * 0.15f) * 4.0f * scale);
        
        for (int x = center_x - body_width; x <= center_x + body_width; x++) {
            int curved_x = x + (int)(body_curve);
            if (curved_x >= 0 && curved_x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, curved_x, y, '#', 1.0f);
            }
        }
    }
    
    // Waist - narrow
    int waist_y = shoulder_y + (int)(40 * scale);
    for (int y = waist_y; y < waist_y + (int)(20 * scale); y++) {
        float waist_curve = sinf((y - waist_y) * 0.2f + time) * pose_angle * 3.0f;
        int waist_width = (int)(8 * scale);
        
        for (int x = center_x - waist_width; x <= center_x + waist_width; x++) {
            int curved_x = x + (int)(waist_curve);
            if (curved_x >= 0 && curved_x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, curved_x, y, '@', 1.0f);
            }
        }
    }
    
    // Dress - flowing from hips down
    int dress_start = waist_y + (int)(20 * scale);
    for (int y = dress_start; y < figure_bottom; y++) {
        float dress_wave = sinf(time * 2.0f + y * 0.05f) * dress_flow;
        int dress_width = (int)(15 * scale + (y - dress_start) * 0.3f * scale); // Widens toward bottom
        
        for (int x = center_x - dress_width; x <= center_x + dress_width; x++) {
            int flow_x = x + (int)(dress_wave * 6.0f);
            if (flow_x >= 0 && flow_x < width && y >= 0 && y < height) {
                char dress_char = ((x + y) % 3 == 0) ? '#' : '@';
                set_pixel(buffer, zbuffer, width, height, flow_x, y, dress_char, 1.0f);
            }
        }
    }
    
    // Arms - elegant pose
    int arm_y = shoulder_y + (int)(20 * scale);
    
    // Left arm - raised elegantly
    for (int seg = 0; seg < 20; seg++) {
        float arm_angle = -M_PI/3 + pose_angle * 0.5f;
        int arm_x = center_x - (int)(12 * scale) + (int)(seg * cosf(arm_angle) * 2 * scale);
        int arm_y_pos = arm_y + (int)(seg * sinf(arm_angle) * 2 * scale);
        
        if (arm_x >= 0 && arm_x < width && arm_y_pos >= 0 && arm_y_pos < height) {
            set_pixel(buffer, zbuffer, width, height, arm_x, arm_y_pos, '|', 1.2f);
        }
    }
}

// Scene 135: Smoke Room - Atmospheric haze and shadows
void scene_135(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float smoke_density = params[0].value;
    float light_rays = params[1].value;
    float ventilation = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    static float smoke_particles[200][4]; // x, y, z, age
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < 200; i++) {
            smoke_particles[i][0] = rand() % width;
            smoke_particles[i][1] = rand() % height;
            smoke_particles[i][2] = rand() % 100 / 100.0f;
            smoke_particles[i][3] = rand() % 100;
        }
        initialized = true;
    }
    
    // Update smoke particles
    int active_particles = (int)(smoke_density * 200);
    for (int i = 0; i < active_particles; i++) {
        smoke_particles[i][3] += 1.0f; // age
        
        // Smoke motion
        smoke_particles[i][0] += sinf(time * 0.5f + i * 0.1f) * (1.0f - ventilation) + ventilation * 2.0f;
        smoke_particles[i][1] += cosf(time * 0.3f + i * 0.15f) * 0.5f - 0.3f; // Slow rise
        smoke_particles[i][2] += 0.01f;
        
        // Wrap around screen
        if (smoke_particles[i][0] >= width) smoke_particles[i][0] = 0;
        if (smoke_particles[i][0] < 0) smoke_particles[i][0] = width - 1;
        if (smoke_particles[i][1] < 0) smoke_particles[i][1] = height - 1;
        
        // Reset old particles
        if (smoke_particles[i][3] > 300) {
            smoke_particles[i][0] = rand() % width;
            smoke_particles[i][1] = height + rand() % 20;
            smoke_particles[i][2] = 0;
            smoke_particles[i][3] = 0;
        }
        
        // Draw smoke particle
        int x = (int)smoke_particles[i][0];
        int y = (int)smoke_particles[i][1];
        float z = smoke_particles[i][2];
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            char smoke_char = (z < 0.3f) ? '.' : (z < 0.6f) ? '*' : '@';
            set_pixel(buffer, zbuffer, width, height, x, y, smoke_char, z * 10.0f);
        }
    }
    
    // Light rays cutting through smoke
    if (light_rays > 0.4f) {
        for (int ray = 0; ray < 5; ray++) {
            float ray_angle = ray * M_PI / 6.0f + time * 0.2f;
            float ray_start_x = width * 0.8f + sinf(time + ray) * 20.0f;
            float ray_start_y = height * 0.2f;
            
            for (int len = 0; len < width; len++) {
                int ray_x = (int)(ray_start_x + len * cosf(ray_angle));
                int ray_y = (int)(ray_start_y + len * sinf(ray_angle));
                
                if (ray_x >= 0 && ray_x < width && ray_y >= 0 && ray_y < height) {
                    float ray_intensity = light_rays * (1.0f - len / (float)width);
                    if (ray_intensity > 0.2f && (len % 3) == 0) {
                        char ray_char = (ray_intensity > 0.6f) ? '|' : '.';
                        set_pixel(buffer, zbuffer, width, height, ray_x, ray_y, ray_char, 0.1f);
                    }
                }
            }
        }
    }
    
    // Room furniture silhouettes
    // Table
    int table_y = height * 2 / 3;
    for (int x = width / 4; x < width * 3 / 4; x++) {
        for (int y = table_y; y < table_y + 3; y++) {
            if (y < height) {
                set_pixel(buffer, zbuffer, width, height, x, y, '=', 2.0f);
            }
        }
    }
    
    // Chairs
    for (int chair = 0; chair < 3; chair++) {
        int chair_x = width / 4 + chair * width / 6;
        for (int y = table_y + 5; y < height; y++) {
            set_pixel(buffer, zbuffer, width, height, chair_x, y, '|', 2.5f);
        }
        // Chair back
        for (int y = table_y - 10; y < table_y; y++) {
            if (y >= 0) {
                set_pixel(buffer, zbuffer, width, height, chair_x, y, '|', 2.5f);
            }
        }
    }
}

// Scene 136: Staircase Shadows - Dramatic ascending perspective
void scene_136(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float perspective_angle = params[0].value;
    float shadow_length = params[1].value;
    float zoom = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float rotation = perspective_angle * M_PI / 4.0f;
    float scale = 1.0f + zoom;
    
    // Staircase steps
    int num_steps = 12;
    for (int step = 0; step < num_steps; step++) {
        float step_progress = step / (float)num_steps;
        
        // Perspective calculation
        float perspective_scale = 1.0f - step_progress * 0.7f;
        int step_width = (int)(width * perspective_scale * scale);
        int step_height = (int)(8 * perspective_scale * scale);
        
        int step_x = (width - step_width) / 2;
        int step_y = height - (step + 1) * (int)(height * 0.8f / num_steps);
        
        // Add rotation
        step_x += (int)(sinf(rotation) * step * 5);
        step_y += (int)(cosf(rotation) * step * 2);
        
        // Draw step horizontal surface
        for (int x = step_x; x < step_x + step_width && x < width; x++) {
            if (x >= 0 && step_y >= 0 && step_y < height) {
                set_pixel(buffer, zbuffer, width, height, x, step_y, '=', 10.0f - step);
            }
        }
        
        // Draw step vertical surface
        for (int y = step_y; y < step_y + step_height && y < height; y++) {
            if (step_x >= 0 && step_x < width && y >= 0) {
                set_pixel(buffer, zbuffer, width, height, step_x, y, '|', 10.0f - step);
            }
            if (step_x + step_width - 1 >= 0 && step_x + step_width - 1 < width && y >= 0) {
                set_pixel(buffer, zbuffer, width, height, step_x + step_width - 1, y, '|', 10.0f - step);
            }
        }
        
        // Step shadows
        if (shadow_length > 0.3f) {
            int shadow_reach = (int)(shadow_length * step_width * 0.5f);
            for (int shadow = 1; shadow < shadow_reach; shadow++) {
                int shadow_x = step_x + step_width + shadow;
                int shadow_y = step_y + shadow / 2;
                
                if (shadow_x < width && shadow_y < height && shadow_x >= 0 && shadow_y >= 0) {
                    char shadow_char = (shadow < shadow_reach / 2) ? '.' : ' ';
                    if (shadow_char != ' ') {
                        set_pixel(buffer, zbuffer, width, height, shadow_x, shadow_y, shadow_char, 15.0f);
                    }
                }
            }
        }
    }
    
    // Handrail
    for (int step = 0; step < num_steps - 1; step++) {
        float step_progress = step / (float)num_steps;
        float perspective_scale = 1.0f - step_progress * 0.7f;
        
        int rail_x = (width / 2) + (int)(width * perspective_scale * 0.4f * scale);
        int rail_y = height - (step + 1) * (int)(height * 0.8f / num_steps) - (int)(10 * perspective_scale);
        
        rail_x += (int)(sinf(rotation) * step * 5);
        rail_y += (int)(cosf(rotation) * step * 2);
        
        if (rail_x >= 0 && rail_x < width && rail_y >= 0 && rail_y < height) {
            set_pixel(buffer, zbuffer, width, height, rail_x, rail_y, '#', 8.0f - step);
        }
    }
    
    // Figure on stairs (optional)
    if (zoom > 0.6f) {
        int figure_step = (int)(time * 2.0f) % num_steps;
        float step_progress = figure_step / (float)num_steps;
        float perspective_scale = 1.0f - step_progress * 0.7f;
        
        int figure_x = (width - (int)(width * perspective_scale)) / 2;
        int figure_y = height - (figure_step + 1) * (int)(height * 0.8f / num_steps) - (int)(20 * perspective_scale);
        
        figure_x += (int)(sinf(rotation) * figure_step * 5);
        figure_y += (int)(cosf(rotation) * figure_step * 2);
        
        // Simple figure
        if (figure_x >= 0 && figure_x < width && figure_y >= 0 && figure_y < height) {
            set_pixel(buffer, zbuffer, width, height, figure_x, figure_y, '@', 5.0f);
        }
    }
}

// Scene 137: Car Headlights in Fog - Atmospheric night scene
void scene_137(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float fog_density = params[0].value;
    float headlight_intensity = params[1].value;
    float car_distance = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    // Fog particles
    static float fog_particles[150][3]; // x, y, density
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < 150; i++) {
            fog_particles[i][0] = rand() % width;
            fog_particles[i][1] = rand() % height;
            fog_particles[i][2] = rand() % 100 / 100.0f;
        }
        initialized = true;
    }
    
    // Update fog
    for (int i = 0; i < (int)(fog_density * 150); i++) {
        fog_particles[i][0] += sinf(time * 0.3f + i * 0.1f) * 0.5f;
        fog_particles[i][1] += cosf(time * 0.2f + i * 0.15f) * 0.3f;
        
        // Wrap fog
        if (fog_particles[i][0] >= width) fog_particles[i][0] = 0;
        if (fog_particles[i][0] < 0) fog_particles[i][0] = width - 1;
        if (fog_particles[i][1] >= height) fog_particles[i][1] = 0;
        if (fog_particles[i][1] < 0) fog_particles[i][1] = height - 1;
        
        int x = (int)fog_particles[i][0];
        int y = (int)fog_particles[i][1];
        float density = fog_particles[i][2];
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            char fog_char = (density > 0.7f) ? '*' : '.';
            set_pixel(buffer, zbuffer, width, height, x, y, fog_char, 5.0f + density * 5.0f);
        }
    }
    
    // Car headlights
    float car_z = 5.0f + car_distance * 20.0f;
    int car_x = width / 2 + (int)(sinf(time * 0.5f) * width * 0.2f);
    int car_y = height * 2 / 3;
    
    // Headlight cones
    if (headlight_intensity > 0.2f) {
        for (int light = 0; light < 2; light++) {
            int light_x = car_x + (light == 0 ? -width/16 : width/16);
            
            // Light cone expanding outward
            for (int ray = 0; ray < 30; ray++) {
                float ray_angle = (ray - 15) * 0.1f;
                
                for (int distance = 1; distance < (int)(width * 0.8f); distance++) {
                    int beam_x = light_x + (int)(distance * sinf(ray_angle));
                    int beam_y = car_y - distance / 2; // Beam goes forward and slightly up
                    
                    float beam_intensity = headlight_intensity * (1.0f - distance / (width * 0.8f));
                    beam_intensity *= (1.0f - fabsf(ray_angle) * 2.0f); // Dimmer at edges
                    
                    if (beam_x >= 0 && beam_x < width && beam_y >= 0 && beam_y < height && beam_intensity > 0.1f) {
                        char beam_char = (beam_intensity > 0.6f) ? '|' : (beam_intensity > 0.3f) ? '.' : ' ';
                        if (beam_char != ' ') {
                            set_pixel(buffer, zbuffer, width, height, beam_x, beam_y, beam_char, car_z - distance * 0.1f);
                        }
                    }
                }
            }
            
            // Bright headlight source
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    int hx = light_x + dx;
                    int hy = car_y + dy;
                    if (hx >= 0 && hx < width && hy >= 0 && hy < height) {
                        set_pixel(buffer, zbuffer, width, height, hx, hy, '@', car_z);
                    }
                }
            }
        }
    }
    
    // Car body silhouette
    int car_width = width / 8;
    for (int x = car_x - car_width; x <= car_x + car_width; x++) {
        for (int y = car_y; y < car_y + height/12; y++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, x, y, '#', car_z);
            }
        }
    }
    
    // Street surface reflections
    if (headlight_intensity > 0.5f) {
        for (int reflect = 0; reflect < width; reflect += 4) {
            int reflect_y = height - 1 - (int)(sinf(time + reflect * 0.1f) * 3.0f);
            if (reflect_y >= 0 && reflect_y < height) {
                set_pixel(buffer, zbuffer, width, height, reflect, reflect_y, '.', 10.0f);
            }
        }
    }
}

// Scene 138: Neon Signs Rain - Reflected city lights
void scene_138(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float neon_flicker = params[0].value;
    float rain_intensity = params[1].value;
    float zoom = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    // Neon signs - multiple levels
    const char* neon_texts[] = {"HOTEL", "BAR", "CAFE", "CLUB", "DINER"};
    int num_signs = 5;
    
    for (int sign = 0; sign < num_signs; sign++) {
        int sign_x = (sign * width / num_signs) + (int)(sinf(time * 0.3f + sign) * 20.0f);
        int sign_y = height / 8 + sign * height / 12;
        
        // Flicker effect
        float flicker = sinf(time * 10.0f + sign * 2.0f) * neon_flicker;
        bool is_on = (flicker > -0.3f);
        
        if (is_on) {
            const char* text = neon_texts[sign];
            for (int c = 0; c < (int)strlen(text); c++) {
                int char_x = sign_x + c * 4;
                
                if (char_x >= 0 && char_x < width - 3 && sign_y >= 0 && sign_y < height - 2) {
                    // Draw neon character
                    char neon_char = text[c];
                    set_pixel(buffer, zbuffer, width, height, char_x, sign_y, neon_char, 1.0f);
                    
                    // Neon glow effect
                    for (int glow = 1; glow <= 2; glow++) {
                        for (int dx = -glow; dx <= glow; dx++) {
                            for (int dy = -glow; dy <= glow; dy++) {
                                int gx = char_x + dx;
                                int gy = sign_y + dy;
                                if (gx >= 0 && gx < width && gy >= 0 && gy < height) {
                                    char glow_char = (glow == 1) ? '*' : '.';
                                    set_pixel(buffer, zbuffer, width, height, gx, gy, glow_char, 2.0f + glow);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Rain drops
    static float rain_drops[80][3]; // x, y, speed
    static bool initialized = false;
    
    if (!initialized) {
        for (int i = 0; i < 80; i++) {
            rain_drops[i][0] = rand() % width;
            rain_drops[i][1] = rand() % height;
            rain_drops[i][2] = 1 + rand() % 3;
        }
        initialized = true;
    }
    
    // Update rain
    for (int i = 0; i < (int)(rain_intensity * 80); i++) {
        rain_drops[i][1] += rain_drops[i][2] + rain_intensity * 2.0f;
        rain_drops[i][0] += sinf(time + i) * 0.5f; // Wind drift
        
        if (rain_drops[i][1] > height) {
            rain_drops[i][0] = rand() % width;
            rain_drops[i][1] = -5;
            rain_drops[i][2] = 1 + rand() % 3;
        }
        
        int x = (int)rain_drops[i][0];
        int y = (int)rain_drops[i][1];
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            set_pixel(buffer, zbuffer, width, height, x, y, '|', 8.0f);
        }
    }
    
    // Wet street reflections
    if (rain_intensity > 0.4f && zoom > 0.3f) {
        for (int sign = 0; sign < num_signs; sign++) {
            int sign_x = (sign * width / num_signs) + (int)(sinf(time * 0.3f + sign) * 20.0f);
            int reflection_y = height - height / 8 - sign * height / 20;
            
            float flicker = sinf(time * 10.0f + sign * 2.0f) * neon_flicker;
            bool is_on = (flicker > -0.3f);
            
            if (is_on && reflection_y >= 0 && reflection_y < height) {
                const char* text = neon_texts[sign];
                for (int c = 0; c < (int)strlen(text); c++) {
                    int char_x = sign_x + c * 4;
                    
                    if (char_x >= 0 && char_x < width) {
                        // Distorted reflection
                        int distort_y = reflection_y + (int)(sinf(time * 3.0f + char_x * 0.1f) * 2.0f);
                        if (distort_y >= 0 && distort_y < height) {
                            char reflect_char = (rand() % 3) ? '.' : text[c];
                            set_pixel(buffer, zbuffer, width, height, char_x, distort_y, reflect_char, 12.0f);
                        }
                    }
                }
            }
        }
    }
}

// Scene 139: Film Strip - Movie camera effect with frames
void scene_139(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float film_speed = params[0].value;
    float frame_zoom = params[1].value;
    float vintage_effect = params[2].value;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    // Film strip holes on sides
    int hole_spacing = height / 12;
    float film_scroll = time * film_speed * 50.0f;
    
    for (int side = 0; side < 2; side++) {
        int hole_x = (side == 0) ? width / 16 : width * 15 / 16;
        
        for (int hole = -2; hole < height / hole_spacing + 2; hole++) {
            int hole_y = (int)(hole * hole_spacing - fmodf(film_scroll, hole_spacing));
            
            if (hole_y >= -hole_spacing/2 && hole_y < height + hole_spacing/2) {
                // Sprocket hole
                for (int dx = -2; dx <= 2; dx++) {
                    for (int dy = -3; dy <= 3; dy++) {
                        int hx = hole_x + dx;
                        int hy = hole_y + dy;
                        if (hx >= 0 && hx < width && hy >= 0 && hy < height) {
                            set_pixel(buffer, zbuffer, width, height, hx, hy, ' ', 1.0f);
                        }
                    }
                }
                
                // Hole outline
                for (int dx = -3; dx <= 3; dx++) {
                    for (int dy = -4; dy <= 4; dy++) {
                        if (abs(dx) == 3 || abs(dy) == 4) {
                            int hx = hole_x + dx;
                            int hy = hole_y + dy;
                            if (hx >= 0 && hx < width && hy >= 0 && hy < height) {
                                set_pixel(buffer, zbuffer, width, height, hx, hy, '#', 0.5f);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Film strip edges
    int strip_left = width / 8;
    int strip_right = width * 7 / 8;
    
    for (int y = 0; y < height; y++) {
        set_pixel(buffer, zbuffer, width, height, strip_left, y, '#', 0.5f);
        set_pixel(buffer, zbuffer, width, height, strip_right, y, '#', 0.5f);
    }
    
    // Film frames
    int frame_height = height / 4;
    int frame_width = strip_right - strip_left;
    
    for (int frame = -1; frame < 5; frame++) {
        int frame_y = (int)(frame * frame_height - fmodf(film_scroll, frame_height));
        
        if (frame_y >= -frame_height && frame_y < height) {
            // Frame outline
            for (int x = strip_left; x <= strip_right; x++) {
                if (frame_y >= 0 && frame_y < height) {
                    set_pixel(buffer, zbuffer, width, height, x, frame_y, '#', 0.8f);
                }
                if (frame_y + frame_height >= 0 && frame_y + frame_height < height) {
                    set_pixel(buffer, zbuffer, width, height, x, frame_y + frame_height, '#', 0.8f);
                }
            }
            
            // Frame content - different patterns for each frame
            int content_type = (frame + (int)(time * film_speed)) % 4;
            
            for (int y = frame_y + 1; y < frame_y + frame_height - 1 && y < height; y++) {
                for (int x = strip_left + 1; x < strip_right - 1 && x < width; x++) {
                    if (y >= 0) {
                        char content_char = ' ';
                        float zoom_factor = 1.0f + frame_zoom * sinf(time * 2.0f);
                        
                        int rel_x = (int)((x - width/2) / zoom_factor + width/2);
                        int rel_y = (int)((y - frame_y - frame_height/2) / zoom_factor + frame_y + frame_height/2);
                        
                        switch (content_type) {
                            case 0: // Geometric pattern
                                if ((rel_x + rel_y) % 8 < 4) content_char = '#';
                                break;
                            case 1: // Circles
                                {
                                    int cx = width / 2;
                                    int cy = frame_y + frame_height / 2;
                                    float dist = sqrtf((rel_x - cx) * (rel_x - cx) + (rel_y - cy) * (rel_y - cy));
                                    if ((int)dist % 10 < 2) content_char = '*';
                                }
                                break;
                            case 2: // Lines
                                if (rel_x % 6 == 0) content_char = '|';
                                break;
                            case 3: // Noise
                                if ((rel_x * 7 + rel_y * 11) % 13 < 3) content_char = '.';
                                break;
                        }
                        
                        if (content_char != ' ') {
                            set_pixel(buffer, zbuffer, width, height, x, y, content_char, 2.0f);
                        }
                    }
                }
            }
        }
    }
    
    // Vintage scratches and dust
    if (vintage_effect > 0.3f) {
        for (int scratch = 0; scratch < (int)(vintage_effect * 20); scratch++) {
            int scratch_x = strip_left + 1 + rand() % (frame_width - 2);
            int scratch_length = rand() % (height / 4);
            int scratch_start_y = rand() % height;
            
            for (int s = 0; s < scratch_length; s++) {
                int sy = scratch_start_y + s;
                if (sy >= 0 && sy < height) {
                    set_pixel(buffer, zbuffer, width, height, scratch_x, sy, '|', 0.2f);
                }
            }
        }
        
        // Dust spots
        for (int dust = 0; dust < (int)(vintage_effect * 30); dust++) {
            int dust_x = strip_left + 1 + rand() % (frame_width - 2);
            int dust_y = rand() % height;
            
            if (dust_x >= 0 && dust_x < width && dust_y >= 0 && dust_y < height) {
                set_pixel(buffer, zbuffer, width, height, dust_x, dust_y, '.', 0.3f);
            }
        }
    }
}

// ============= ESCHER 3D ILLUSION SCENES (140-149) =============

// 140: Impossible Stairs - Ascending stairs that loop infinitely
void scene_impossible_stairs(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float speed = params[1].value;
    int cx = width / 2, cy = height / 2;
    float t = time * speed;
    
    // Simple staircase pattern that always renders
    for (int step = 0; step < 15; step++) {
        float angle = step * 0.4f + t;
        int radius = 8 + step * 2;
        int x = cx + (int)(radius * cos(angle));
        int y = cy + (int)(radius * sin(angle) * 0.6f) + step;
        
        // Draw step
        for (int i = 0; i < 4; i++) {
            int sx = x + i - 2;
            int sy = y;
            if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                char stair_char = (step % 2 == 0) ? '#' : '=';
                set_pixel(buffer, zbuffer, width, height, sx, sy, stair_char, 10.0f);
            }
        }
        
        // Draw vertical connection
        if (step > 0) {
            int prev_x = cx + (int)((8 + (step-1) * 2) * cos((step-1) * 0.4f + t));
            int prev_y = cy + (int)((8 + (step-1) * 2) * sin((step-1) * 0.4f + t) * 0.6f) + step - 1;
            
            for (int i = 0; i < 3; i++) {
                int ly = prev_y + i;
                if (prev_x >= 0 && prev_x < width && ly >= 0 && ly < height) {
                    set_pixel(buffer, zbuffer, width, height, prev_x, ly, '|', 10.0f);
                }
            }
        }
    }
}

// 141: Möbius Strip - Continuous surface with only one side
void scene_mobius_strip(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float scale = 0.5f + params[0].value * 1.5f;
    float rotation = time * params[1].value;
    float twist = params[2].value * 2.0f;
    
    for (int i = 0; i < width * height; i++) {
        buffer[i] = ' ';
        zbuffer[i] = 1000.0f;
    }
    
    int cx = width / 2, cy = height / 2;
    
    // Parametric Möbius strip
    for (int u = 0; u < 360; u += 2) {
        for (int v = -10; v <= 10; v += 1) {
            float u_rad = u * M_PI / 180.0f;
            float v_scale = v * 0.1f * scale;
            
            // Möbius strip parametric equations
            float radius = 30.0f + v_scale * cos(u_rad / 2.0f + twist);
            float x3d = radius * cos(u_rad + rotation);
            float y3d = radius * sin(u_rad + rotation);
            float z3d = v_scale * sin(u_rad / 2.0f + twist) + 50.0f + params[3].value * 20.0f;
            
            // Perspective projection
            if (z3d > 1.0f) {
                int x2d = cx + (int)(x3d * 30.0f / z3d);
                int y2d = cy + (int)(y3d * 15.0f / z3d);
                
                if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                    char strip_char = (abs(v) < 2) ? '#' : (abs(v) < 5) ? '=' : '.';
                    set_pixel(buffer, zbuffer, width, height, x2d, y2d, strip_char, z3d);
                }
            }
        }
    }
}

// 142: Impossible Cube - Wireframe cube with impossible geometry
void scene_impossible_cube(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float scale = 0.5f + params[0].value * 1.5f;
    float rotation_speed = params[1].value;
    float wireframe_density = 1.0f + params[2].value * 3.0f;
    
    for (int i = 0; i < width * height; i++) {
        buffer[i] = ' ';
        zbuffer[i] = 1000.0f;
    }
    
    int cx = width / 2, cy = height / 2;
    float t = time * rotation_speed;
    
    // Define cube vertices with impossible twist
    float vertices[8][3];
    float size = 25.0f * scale;
    
    for (int i = 0; i < 8; i++) {
        float x = (i & 1) ? size : -size;
        float y = (i & 2) ? size : -size;
        float z = (i & 4) ? size : -size;
        
        // Apply impossible rotation per face
        float face_twist = (i / 2) * M_PI / 4.0f * params[3].value;
        
        vertices[i][0] = x * cos(t + face_twist) - z * sin(t + face_twist);
        vertices[i][1] = y;
        vertices[i][2] = x * sin(t + face_twist) + z * cos(t + face_twist) + 60.0f;
    }
    
    // Draw cube edges with impossible connections
    int edges[12][2] = {
        {0,1}, {1,3}, {3,2}, {2,0}, // bottom face
        {4,5}, {5,7}, {7,6}, {6,4}, // top face
        {0,4}, {1,5}, {2,6}, {3,7}  // vertical edges
    };
    
    for (int e = 0; e < 12; e++) {
        int v1 = edges[e][0], v2 = edges[e][1];
        
        // Draw line between vertices
        for (int i = 0; i < (int)wireframe_density * 20; i++) {
            float t_line = i / (wireframe_density * 20.0f);
            float x3d = vertices[v1][0] * (1-t_line) + vertices[v2][0] * t_line;
            float y3d = vertices[v1][1] * (1-t_line) + vertices[v2][1] * t_line;
            float z3d = vertices[v1][2] * (1-t_line) + vertices[v2][2] * t_line;
            
            if (z3d > 1.0f) {
                int x2d = cx + (int)(x3d * 40.0f / z3d);
                int y2d = cy + (int)(y3d * 20.0f / z3d);
                
                if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                    char edge_char = (e < 4) ? '#' : (e < 8) ? '=' : '|';
                    set_pixel(buffer, zbuffer, width, height, x2d, y2d, edge_char, z3d);
                }
            }
        }
    }
}

// 143: Penrose Triangle - Impossible triangle that appears solid
void scene_penrose_triangle(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float scale = 0.5f + params[0].value * 1.5f;
    float rotation = time * params[1].value * 0.3f;
    float thickness = 2.0f + params[2].value * 6.0f;
    
    for (int i = 0; i < width * height; i++) {
        buffer[i] = ' ';
        zbuffer[i] = 1000.0f;
    }
    
    int cx = width / 2, cy = height / 2;
    
    // Three beams of the impossible triangle
    for (int beam = 0; beam < 3; beam++) {
        float beam_angle = beam * 2.0f * M_PI / 3.0f + rotation;
        float beam_length = 40.0f * scale;
        
        // Each beam is drawn as a 3D rectangular beam
        for (int seg = 0; seg < 50; seg++) {
            float t = seg / 49.0f;
            float beam_progress = t * beam_length;
            
            // Calculate beam position with impossible connections
            float base_x = beam_progress * cos(beam_angle);
            float base_y = beam_progress * sin(beam_angle);
            float base_z = sin(beam_progress * 0.1f + time + beam * M_PI) * 10.0f * params[3].value;
            
            // Draw beam cross-section
            for (int thick = -(int)thickness; thick <= (int)thickness; thick++) {
                for (int thick2 = -(int)thickness; thick2 <= (int)thickness; thick2++) {
                    float x3d = base_x + thick * cos(beam_angle + M_PI/2) * 2.0f;
                    float y3d = base_y + thick * sin(beam_angle + M_PI/2) * 2.0f;
                    float z3d = base_z + thick2 * 2.0f + 50.0f;
                    
                    if (z3d > 1.0f) {
                        int x2d = cx + (int)(x3d * 35.0f / z3d);
                        int y2d = cy + (int)(y3d * 17.5f / z3d);
                        
                        if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                            char beam_char = (abs(thick) <= 1 && abs(thick2) <= 1) ? '#' : '=';
                            set_pixel(buffer, zbuffer, width, height, x2d, y2d, beam_char, z3d);
                        }
                    }
                }
            }
        }
    }
}

// 144: Infinite Corridor - Recursive hallway effect
void scene_infinite_corridor(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float movement = time * params[1].value * 10.0f;
    float perspective = 1.0f + params[0].value * 2.0f;
    float wall_detail = params[2].value;
    
    for (int i = 0; i < width * height; i++) {
        buffer[i] = ' ';
        zbuffer[i] = 1000.0f;
    }
    
    int cx = width / 2, cy = height / 2;
    
    // Draw recursive corridor segments
    for (int segment = 0; segment < 20; segment++) {
        float z = segment * 15.0f + fmod(movement, 15.0f);
        if (z <= 0) continue;
        
        float corridor_width = 30.0f / (z * 0.02f * perspective);
        float corridor_height = 20.0f / (z * 0.02f * perspective);
        
        // Draw corridor walls, floor, and ceiling
        for (int side = 0; side < 4; side++) {
            for (int i = 0; i < 40; i++) {
                float t = i / 39.0f;
                int x2d, y2d;
                char wall_char;
                
                switch (side) {
                    case 0: // Left wall
                        x2d = cx - (int)corridor_width;
                        y2d = cy - (int)corridor_height + (int)(t * 2 * corridor_height);
                        wall_char = '|';
                        break;
                    case 1: // Right wall
                        x2d = cx + (int)corridor_width;
                        y2d = cy - (int)corridor_height + (int)(t * 2 * corridor_height);
                        wall_char = '|';
                        break;
                    case 2: // Floor
                        x2d = cx - (int)corridor_width + (int)(t * 2 * corridor_width);
                        y2d = cy + (int)corridor_height;
                        wall_char = (wall_detail > 0.5f) ? '=' : '_';
                        break;
                    case 3: // Ceiling
                        x2d = cx - (int)corridor_width + (int)(t * 2 * corridor_width);
                        y2d = cy - (int)corridor_height;
                        wall_char = (wall_detail > 0.5f) ? '=' : '-';
                        break;
                }
                
                if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                    set_pixel(buffer, zbuffer, width, height, x2d, y2d, wall_char, z);
                }
            }
        }
        
        // Add door frames for impossible depth
        if (segment % 3 == 0 && params[3].value > 0.3f) {
            float frame_width = corridor_width * 0.8f;
            float frame_height = corridor_height * 0.8f;
            
            // Door frame
            for (int i = 0; i < 20; i++) {
                float t = i / 19.0f;
                
                // Top and bottom of frame
                int x_frame = cx - (int)frame_width + (int)(t * 2 * frame_width);
                set_pixel(buffer, zbuffer, width, height, x_frame, cy - (int)frame_height, '#', z - 1);
                set_pixel(buffer, zbuffer, width, height, x_frame, cy + (int)frame_height, '#', z - 1);
            }
        }
    }
}

// 145: Tessellated Reality - MC Escher-style tessellation
void scene_tessellated_reality(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float scale = 0.3f + params[0].value * 1.0f;
    float morph = sin(time * params[1].value) * 0.5f + 0.5f;
    float complexity = params[2].value;
    
    for (int i = 0; i < width * height; i++) {
        buffer[i] = ' ';
        zbuffer[i] = 1000.0f;
    }
    
    // Create tessellated pattern that morphs between shapes
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float fx = (x - width/2) * scale;
            float fy = (y - height/2) * scale;
            
            // Hexagonal tessellation coordinates
            float hex_x = fx * 2.0f / 3.0f;
            float hex_y = (fy - fx / 3.0f) / sqrt(3.0f) * 2.0f;
            
            // Round to nearest hexagon center
            float qx = round(hex_x);
            float qy = round(hex_y);
            
            // Distance to hexagon center
            float dx = hex_x - qx;
            float dy = hex_y - qy;
            float dist = sqrt(dx*dx + dy*dy);
            
            // Morph between different tessellation patterns
            float pattern1 = sin(qx * 2.0f + time) * sin(qy * 2.0f + time);
            float pattern2 = cos((qx + qy) * 1.5f + time * 0.7f);
            float pattern = pattern1 * (1-morph) + pattern2 * morph;
            
            // Apply complexity and create impossible transitions
            if (complexity > 0.5f) {
                pattern += sin(dist * 10.0f + time * 2.0f) * (complexity - 0.5f) * 2.0f;
            }
            
            // Determine character based on pattern
            char tessellation_char = ' ';
            if (dist < 0.4f) {
                if (pattern > 0.5f) tessellation_char = '#';
                else if (pattern > 0.0f) tessellation_char = '=';
                else if (pattern > -0.5f) tessellation_char = '.';
                else tessellation_char = ' ';
            } else if (dist < 0.6f && params[3].value > 0.3f) {
                tessellation_char = '|';
            }
            
            if (tessellation_char != ' ') {
                float z = 10.0f + sin(pattern * 5.0f) * 5.0f;
                set_pixel(buffer, zbuffer, width, height, x, y, tessellation_char, z);
            }
        }
    }
}

// 146: Gravity Wells - Curved space visualization
void scene_gravity_wells(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params || width <= 0 || height <= 0) return;
    
    float well_strength = 1.0f + (params ? params[0].value : 0.5f) * 3.0f;
    float rotation = time * (params ? params[1].value : 0.5f) * 0.5f;
    float grid_density = 10.0f + (params ? params[2].value : 0.5f) * 20.0f;
    
    for (int i = 0; i < width * height; i++) {
        buffer[i] = ' ';
        zbuffer[i] = 1000.0f;
    }
    
    int cx = width / 2, cy = height / 2;
    
    // Multiple gravity wells
    float wells[4][3] = {
        {cos(rotation) * 20.0f, sin(rotation) * 15.0f, well_strength},
        {cos(rotation + M_PI) * 15.0f, sin(rotation + M_PI) * 10.0f, well_strength * 0.7f},
        {cos(rotation + M_PI/2) * 25.0f, sin(rotation + M_PI/2) * 12.0f, well_strength * 0.5f},
        {cos(rotation + 3*M_PI/2) * 18.0f, sin(rotation + 3*M_PI/2) * 8.0f, well_strength * 0.3f}
    };
    
    // Draw space-time grid warped by gravity wells
    int x_step = (int)(50.0f / grid_density);
    int y_step = (int)(25.0f / grid_density);
    if (x_step <= 0) x_step = 1;  // Prevent infinite loops
    if (y_step <= 0) y_step = 1;
    
    for (int grid_x = -50; grid_x <= 50; grid_x += x_step) {
        for (int grid_y = -25; grid_y <= 25; grid_y += y_step) {
            float x = grid_x;
            float y = grid_y;
            
            // Apply gravitational warping
            for (int w = 0; w < 4; w++) {
                float dx = x - wells[w][0];
                float dy = y - wells[w][1];
                float dist = sqrt(dx*dx + dy*dy) + 1.0f;
                float force = wells[w][2] / (dist * dist);
                
                x += dx * force * params[3].value;
                y += dy * force * params[3].value;
            }
            
            // Project to screen
            float z = 50.0f + sin(x * 0.1f) * sin(y * 0.1f) * 10.0f;
            int x2d = cx + (int)(x * 40.0f / z);
            int y2d = cy + (int)(y * 20.0f / z);
            
            if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                char grid_char = ((grid_x % 10 == 0) || (grid_y % 5 == 0)) ? '+' : '.';
                set_pixel(buffer, zbuffer, width, height, x2d, y2d, grid_char, z);
            }
        }
    }
    
    // Draw the gravity wells themselves
    for (int w = 0; w < 4; w++) {
        for (int r = 1; r < 8; r++) {
            for (int angle = 0; angle < 360; angle += 10) {
                float well_x = wells[w][0] + r * 2.0f * cos(angle * M_PI / 180.0f);
                float well_y = wells[w][1] + r * 1.0f * sin(angle * M_PI / 180.0f);
                float z = 45.0f - r * wells[w][2];
                
                int x2d = cx + (int)(well_x * 40.0f / z);
                int y2d = cy + (int)(well_y * 20.0f / z);
                
                if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                    char well_char = (r < 3) ? '#' : (r < 5) ? '=' : '.';
                    set_pixel(buffer, zbuffer, width, height, x2d, y2d, well_char, z);
                }
            }
        }
    }
}

// 147: Dimensional Shift - Reality folding and unfolding
void scene_dimensional_shift(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    float fold_intensity = params[0].value;
    float shift_speed = params[1].value;
    float layer_count = 3.0f + params[2].value * 7.0f;
    
    for (int i = 0; i < width * height; i++) {
        buffer[i] = ' ';
        zbuffer[i] = 1000.0f;
    }
    
    int cx = width / 2, cy = height / 2;
    float t = time * shift_speed;
    
    // Multiple reality layers that fold into each other
    for (int layer = 0; layer < (int)layer_count; layer++) {
        float layer_offset = layer * 2.0f * M_PI / layer_count;
        float fold_phase = sin(t + layer_offset) * fold_intensity;
        
        // Draw geometric shapes that fold between dimensions
        for (int angle = 0; angle < 360; angle += 5) {
            float base_radius = 20.0f + layer * 8.0f;
            float radius = base_radius + sin(angle * M_PI / 180.0f * 6.0f + t) * 10.0f * fold_intensity;
            
            float x3d = radius * cos(angle * M_PI / 180.0f + layer_offset);
            float y3d = radius * sin(angle * M_PI / 180.0f + layer_offset);
            float z3d = 40.0f + layer * 15.0f + cos(fold_phase * 2.0f) * 20.0f * params[3].value;
            
            // Apply dimensional folding transformation
            if (fold_intensity > 0.5f) {
                float fold_factor = (fold_intensity - 0.5f) * 2.0f;
                float temp_x = x3d * cos(fold_phase) - z3d * sin(fold_phase) * fold_factor;
                float temp_z = x3d * sin(fold_phase) + z3d * cos(fold_phase);
                x3d = temp_x;
                z3d = temp_z;
            }
            
            if (z3d > 1.0f) {
                // Clamp z3d to prevent extreme values
                float safe_z = fmax(z3d, 2.0f);
                int x2d = cx + (int)(x3d * 35.0f / safe_z);
                int y2d = cy + (int)(y3d * 17.5f / safe_z);
                
                if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                    char layer_char = (layer % 3 == 0) ? '#' : (layer % 3 == 1) ? '=' : '.';
                    set_pixel(buffer, zbuffer, width, height, x2d, y2d, layer_char, z3d);
                }
            }
        }
        
        // Connect layers with impossible bridges
        if (layer > 0 && params[3].value > 0.4f) {
            for (int bridge = 0; bridge < 6; bridge++) {
                float bridge_angle = bridge * M_PI / 3.0f + t * 0.3f;
                float bridge_length = 15.0f;
                
                for (int seg = 0; seg < 20; seg++) {
                    float seg_t = seg / 19.0f;
                    float x3d = bridge_length * cos(bridge_angle) * seg_t;
                    float y3d = bridge_length * sin(bridge_angle) * seg_t;
                    float z3d = 40.0f + (layer - 0.5f) * 15.0f + sin(seg_t * M_PI) * 5.0f;
                    
                    if (z3d > 1.0f) {
                        int x2d = cx + (int)(x3d * 35.0f / z3d);
                        int y2d = cy + (int)(y3d * 17.5f / z3d);
                        
                        if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                            set_pixel(buffer, zbuffer, width, height, x2d, y2d, '|', z3d);
                        }
                    }
                }
            }
        }
    }
}

// Helper function for fractal architecture (must be outside scene function)
void draw_fractal_building(char* buffer, float* zbuffer, int width, int height, 
                          float x, float y, float z, float size, int depth,
                          float t, float twist_factor) {
    // Safety checks to prevent segmentation faults
    if (!buffer || !zbuffer) {
        fprintf(stderr, "ERROR: draw_fractal_building called with NULL buffer\n");
        return;
    }
    if (width <= 0 || height <= 0 || width > 10000 || height > 10000) {
        fprintf(stderr, "ERROR: draw_fractal_building called with invalid dimensions: %dx%d\n", width, height);
        return;
    }
    if (depth <= 0 || size < 2.0f || depth > 5) return;  // Limit max depth to prevent stack overflow
    
    int cx = width / 2, cy = height / 2;
    
    // Draw building base
    for (int wall = 0; wall < 4; wall++) {
        for (int i = 0; i < 20; i++) {
            float wall_t = i / 19.0f;
            float wall_x, wall_y, wall_z;
            
            switch (wall) {
                case 0: // Front wall
                    wall_x = x - size + wall_t * size * 2;
                    wall_y = y + size;
                    wall_z = z;
                    break;
                case 1: // Back wall
                    wall_x = x - size + wall_t * size * 2;
                    wall_y = y - size;
                    wall_z = z;
                    break;
                case 2: // Left wall
                    wall_x = x - size;
                    wall_y = y - size + wall_t * size * 2;
                    wall_z = z;
                    break;
                case 3: // Right wall
                    wall_x = x + size;
                    wall_y = y - size + wall_t * size * 2;
                    wall_z = z;
                    break;
            }
            
            if (wall_z > 1.0f) {
                // Clamp wall_z to prevent extreme values from division
                float safe_z = fmax(wall_z, 2.0f);
                int x2d = cx + (int)(wall_x * 30.0f / safe_z);
                int y2d = cy + (int)(wall_y * 15.0f / safe_z);
                
                if (x2d >= 0 && x2d < width && y2d >= 0 && y2d < height) {
                    char wall_char = (wall % 2 == 0) ? '#' : '=';
                    set_pixel(buffer, zbuffer, width, height, x2d, y2d, wall_char, safe_z);
                }
            }
        }
    }
    
    // Recursive smaller buildings on top and sides with impossible physics
    float next_size = size * 0.6f;
    float twist = sin(t + depth) * twist_factor;
    
    // Top building (defying gravity)
    draw_fractal_building(buffer, zbuffer, width, height,
                         x + sin(twist) * size, y + cos(twist) * size, z - size * 1.5f,
                         next_size, depth - 1, t, twist_factor);
    
    // Side buildings (floating)
    for (int side = 0; side < 4; side++) {
        float side_angle = side * M_PI / 2.0f + twist;
        float side_x = x + cos(side_angle) * size * 1.5f;
        float side_y = y + sin(side_angle) * size * 1.5f;
        draw_fractal_building(buffer, zbuffer, width, height,
                             side_x, side_y, z + size,
                             next_size * 0.8f, depth - 1, t, twist_factor);
    }
}

// 148: Fractal Architecture - Recursive impossible buildings
void scene_fractal_architecture(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float animation_speed = params[1].value;
    int cx = width / 2, cy = height / 2;
    float t = time * animation_speed;
    
    // Simple geometric building pattern to avoid recursive crashes
    for (int i = 0; i < 80; i++) {
        float angle = i * 0.2f + t * 0.5f;
        int radius = 10 + i / 3;
        int x = cx + (int)(radius * cos(angle));
        int y = cy + (int)(radius * sin(angle) * 0.3f);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            char building_char = (i % 4 == 0) ? '#' : (i % 4 == 1) ? '|' : (i % 4 == 2) ? '=' : '.';
            set_pixel(buffer, zbuffer, width, height, x, y, building_char, 10.0f);
        }
    }
}

// 149: Escher Waterfall - Impossible water flow uphill
void scene_escher_waterfall(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float flow_speed = params[1].value;
    int cx = width / 2, cy = height / 2;
    float t = time * flow_speed;
    
    // Simple spiral water pattern to avoid complex math that might cause issues
    for (int i = 0; i < 100; i++) {
        float angle = i * 0.3f + t;
        float radius = i * 0.5f;
        int x = cx + (int)(radius * cos(angle));
        int y = cy + (int)(radius * sin(angle) * 0.5f);
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            set_pixel(buffer, zbuffer, width, height, x, y, '~', 10.0f);
        }
    }
}

// ============= IKEDA-INSPIRED SCENES (150-159) =============

// Scene 150: Ikeda Data Matrix - Binary data streams in grid formations
void scene_ikeda_data_matrix(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float scan_speed = params[0].value;
    float data_density = params[1].value;
    float glitch_amount = params[2].value;
    
    // Binary data grid
    int grid_size = 4;
    for (int gy = 0; gy < height / grid_size; gy++) {
        for (int gx = 0; gx < width / grid_size; gx++) {
            // Data block
            float block_time = time * scan_speed + gx * 0.1f + gy * 0.2f;
            int block_state = ((int)(block_time * 10) + gx * 7 + gy * 13) % 16;
            
            // Binary representation
            for (int bit = 0; bit < 4; bit++) {
                int px = gx * grid_size + bit;
                int py = gy * grid_size;
                
                if (px < width && py < height) {
                    char bit_char = (block_state & (1 << bit)) ? '1' : '0';
                    
                    // Glitch effect
                    if (((float)rand() / RAND_MAX) < glitch_amount * 0.1f) {
                        bit_char = "!@#$%^&*"[rand() % 8];
                    }
                    
                    set_pixel(buffer, zbuffer, width, height, px, py, bit_char, 5.0f);
                }
            }
            
            // Data density lines
            if (((float)rand() / RAND_MAX) < data_density) {
                for (int dy = 0; dy < grid_size; dy++) {
                    int py = gy * grid_size + dy;
                    if (py < height) {
                        for (int dx = 0; dx < grid_size; dx++) {
                            int px = gx * grid_size + dx;
                            if (px < width && (dx == 0 || dy == 0)) {
                                set_pixel(buffer, zbuffer, width, height, px, py, '-', 6.0f);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Scanning lines
    int scan_y = (int)(time * scan_speed * 20) % height;
    for (int x = 0; x < width; x++) {
        set_pixel(buffer, zbuffer, width, height, x, scan_y, '=', 1.0f);
        if (scan_y + 1 < height) {
            set_pixel(buffer, zbuffer, width, height, x, scan_y + 1, '=', 1.0f);
        }
    }
}

// Scene 151: Ikeda Test Pattern - Minimalist geometric test patterns
void scene_ikeda_test_pattern(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float pattern_shift = params[0].value;
    float line_density = params[1].value;
    float phase_shift = params[2].value;
    
    int pattern_type = ((int)(time * pattern_shift)) % 4;
    
    switch (pattern_type) {
        case 0: // Vertical bars
            for (int x = 0; x < width; x++) {
                if ((x + (int)(time * 10)) % (int)(8 / line_density) < 2) {
                    for (int y = 0; y < height; y++) {
                        set_pixel(buffer, zbuffer, width, height, x, y, '|', 5.0f);
                    }
                }
            }
            break;
            
        case 1: // Horizontal bars
            for (int y = 0; y < height; y++) {
                if ((y + (int)(time * 10)) % (int)(4 / line_density) < 1) {
                    for (int x = 0; x < width; x++) {
                        set_pixel(buffer, zbuffer, width, height, x, y, '-', 5.0f);
                    }
                }
            }
            break;
            
        case 2: // Grid pattern
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    if ((x % (int)(8 / line_density) == 0) || (y % (int)(4 / line_density) == 0)) {
                        set_pixel(buffer, zbuffer, width, height, x, y, '+', 5.0f);
                    }
                }
            }
            break;
            
        case 3: // Diagonal scan
            int offset = (int)(time * 20 * phase_shift) % (width + height);
            for (int i = 0; i < width + height; i++) {
                for (int w = 0; w < 3; w++) {
                    int x = i - offset + w;
                    int y = offset - w;
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        set_pixel(buffer, zbuffer, width, height, x, y, '\\', 3.0f);
                    }
                }
            }
            break;
    }
}

// Scene 152: Ikeda Sine Wave - Pure sine wave visualizations
void scene_ikeda_sine_wave(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float frequency = params[0].value * 10.0f;
    float amplitude = params[1].value;
    float phase_mod = params[2].value;
    
    // Multiple overlapping sine waves
    for (int wave = 0; wave < 5; wave++) {
        float wave_freq = frequency * (1.0f + wave * 0.2f);
        float wave_phase = time * wave_freq + wave * phase_mod;
        
        for (int x = 0; x < width; x++) {
            float t = (float)x / width * 4 * M_PI;
            int y = height / 2 + (int)(sinf(t + wave_phase) * height * amplitude * 0.3f);
            
            if (y >= 0 && y < height) {
                char wave_char = ".-+*#"[wave];
                set_pixel(buffer, zbuffer, width, height, x, y, wave_char, 5.0f - wave);
                
                // Vertical traces
                if (x % 8 == 0) {
                    for (int dy = -2; dy <= 2; dy++) {
                        int trace_y = y + dy;
                        if (trace_y >= 0 && trace_y < height) {
                            set_pixel(buffer, zbuffer, width, height, x, trace_y, '|', 6.0f);
                        }
                    }
                }
            }
        }
    }
    
    // Reference grid
    for (int y = 0; y < height; y += height / 8) {
        for (int x = 0; x < width; x++) {
            if (x % 4 == 0) {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 10.0f);
            }
        }
    }
}

// Scene 153: Ikeda Barcode - Dynamic barcode patterns
void scene_ikeda_barcode(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float scan_rate = params[0].value;
    float bar_width = params[1].value * 5.0f + 1.0f;
    float noise_level = params[2].value;
    
    // Generate barcode pattern
    int num_bars = width / (int)bar_width;
    
    for (int bar = 0; bar < num_bars; bar++) {
        // Dynamic bar data
        float bar_time = time * scan_rate + bar * 0.1f;
        int bar_value = ((int)(bar_time * 10) + bar * 17) % 256;
        
        // Draw vertical bar
        int bar_start = bar * (int)bar_width;
        int bar_end = bar_start + (int)bar_width - 1;
        
        for (int x = bar_start; x < bar_end && x < width; x++) {
            if ((bar_value >> ((x - bar_start) % 8)) & 1) {
                for (int y = height / 4; y < height * 3 / 4; y++) {
                    char bar_char = '|';
                    
                    // Add noise
                    if (((float)rand() / RAND_MAX) < noise_level * 0.1f) {
                        bar_char = "!/#\\|"[rand() % 5];
                    }
                    
                    set_pixel(buffer, zbuffer, width, height, x, y, bar_char, 5.0f);
                }
            }
        }
    }
    
    // Scan line
    int scan_x = (int)(time * scan_rate * 40) % width;
    for (int y = 0; y < height; y++) {
        set_pixel(buffer, zbuffer, width, height, scan_x, y, ':', 2.0f);
        if (scan_x + 1 < width) {
            set_pixel(buffer, zbuffer, width, height, scan_x + 1, y, ':', 2.0f);
        }
    }
    
    // Data readout at bottom
    int readout_y = height - 2;
    for (int x = 0; x < width && x < 40; x++) {
        char data = '0' + ((int)(time * 100 + x * 13) % 10);
        set_pixel(buffer, zbuffer, width, height, x, readout_y, data, 3.0f);
    }
}

// Scene 154: Ikeda Pulse - Rhythmic pulse patterns
void scene_ikeda_pulse(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float pulse_rate = params[0].value * 5.0f;
    float pulse_width = params[1].value;
    float echo_count = params[2].value * 5.0f;
    
    // Central pulse
    float pulse_phase = time * pulse_rate;
    float pulse_intensity = (sinf(pulse_phase * 2 * M_PI) + 1.0f) * 0.5f;
    
    if (pulse_intensity > 1.0f - pulse_width) {
        // Main pulse circle
        int radius = (int)(pulse_intensity * height * 0.4f);
        draw_circle(buffer, zbuffer, width, height, width / 2, height / 2, radius, '#', 3.0f);
        
        // Inner details
        if (radius > 5) {
            draw_circle(buffer, zbuffer, width, height, width / 2, height / 2, radius - 2, '+', 4.0f);
        }
    }
    
    // Echo pulses
    for (int echo = 1; echo <= (int)echo_count; echo++) {
        float echo_delay = echo * 0.2f;
        float echo_phase = time * pulse_rate - echo_delay;
        float echo_intensity = (sinf(echo_phase * 2 * M_PI) + 1.0f) * 0.5f;
        echo_intensity *= (1.0f - echo / (echo_count + 1)); // Fade with distance
        
        if (echo_intensity > 0.5f) {
            int echo_radius = (int)(echo_intensity * height * 0.3f);
            char echo_char = ".:-=+"[echo % 5];
            draw_circle(buffer, zbuffer, width, height, width / 2, height / 2, echo_radius, echo_char, 5.0f + echo);
        }
    }
    
    // Grid markers
    for (int x = 0; x < width; x += width / 16) {
        for (int y = 0; y < height; y += height / 8) {
            set_pixel(buffer, zbuffer, width, height, x, y, '.', 10.0f);
        }
    }
}

// Scene 155: Ikeda Glitch - Digital glitch artifacts
void scene_ikeda_glitch(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float glitch_rate = params[0].value;
    float corruption = params[1].value;
    float block_size = params[2].value * 10.0f + 2.0f;
    
    // Base pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if ((x / (int)block_size + y / (int)block_size) % 2 == 0) {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 8.0f);
            }
        }
    }
    
    // Glitch blocks
    int num_glitches = (int)(glitch_rate * 20);
    
    for (int g = 0; g < num_glitches; g++) {
        float glitch_time = time * (1.0f + g * 0.1f);
        int glitch_x = ((int)(glitch_time * 31 + g * 47)) % (width - (int)block_size);
        int glitch_y = ((int)(glitch_time * 37 + g * 53)) % (height - (int)block_size);
        
        // Glitch block
        const char* glitch_chars = "!@#$%^&*()_+-=[]{}|;:',.<>?/~`";
        int char_count = strlen(glitch_chars);
        
        for (int dy = 0; dy < (int)block_size; dy++) {
            for (int dx = 0; dx < (int)block_size; dx++) {
                int px = glitch_x + dx;
                int py = glitch_y + dy;
                
                if (px < width && py < height) {
                    if (((float)rand() / RAND_MAX) < corruption) {
                        char glitch_char = glitch_chars[((int)(glitch_time * 100) + dx + dy * 10) % char_count];
                        set_pixel(buffer, zbuffer, width, height, px, py, glitch_char, 2.0f);
                    }
                }
            }
        }
    }
    
    // Scan lines
    int scan_offset = (int)(time * 50) % height;
    for (int i = 0; i < 3; i++) {
        int scan_y = (scan_offset + i * height / 3) % height;
        for (int x = 0; x < width; x++) {
            if (((float)rand() / RAND_MAX) < 0.8f) {
                set_pixel(buffer, zbuffer, width, height, x, scan_y, '-', 1.0f);
            }
        }
    }
}

// Scene 156: Ikeda Spectrum - Frequency spectrum visualization
void scene_ikeda_spectrum(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float freq_shift = params[0].value * 10.0f;
    float amplitude = params[1].value;
    float band_count = params[2].value * 20.0f + 5.0f;
    
    // Frequency bands
    float band_width = (float)width / band_count;
    
    for (int band = 0; band < (int)band_count; band++) {
        float band_freq = (band + 1) * freq_shift;
        float band_phase = time * band_freq;
        float band_height = (sinf(band_phase) + 1.0f) * 0.5f * amplitude * height * 0.8f;
        
        int band_x_start = (int)(band * band_width);
        int band_x_end = (int)((band + 1) * band_width);
        
        // Draw frequency band
        for (int x = band_x_start; x < band_x_end && x < width; x++) {
            int bar_height = (int)band_height;
            
            for (int y = height - 1; y >= height - bar_height && y >= 0; y--) {
                char bar_char;
                if (y == height - bar_height) {
                    bar_char = '_';
                } else if (x == band_x_start || x == band_x_end - 1) {
                    bar_char = '|';
                } else {
                    bar_char = '#';
                }
                
                set_pixel(buffer, zbuffer, width, height, x, y, bar_char, 5.0f);
            }
            
            // Frequency label
            if (x == band_x_start + (int)(band_width / 2) && height > 10) {
                char freq_label = '0' + (band % 10);
                set_pixel(buffer, zbuffer, width, height, x, height - 1, freq_label, 3.0f);
            }
        }
    }
    
    // Reference grid
    for (int y = 0; y < height; y += height / 8) {
        for (int x = 0; x < width; x++) {
            if (get_pixel(buffer, width, height, x, y) == ' ') {
                set_pixel(buffer, zbuffer, width, height, x, y, '.', 10.0f);
            }
        }
    }
}

// Scene 157: Ikeda Phase - Phase shift patterns
void scene_ikeda_phase(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float phase_speed = params[0].value * 5.0f;
    float phase_count = params[1].value * 8.0f + 2.0f;
    float interference = params[2].value;
    
    // Phase pattern layers
    for (int layer = 0; layer < (int)phase_count; layer++) {
        float layer_phase = time * phase_speed * (1.0f + layer * 0.1f);
        float layer_offset = layer * 2 * M_PI / phase_count;
        
        for (int x = 0; x < width; x++) {
            float x_norm = (float)x / width * 4 * M_PI;
            int y = height / 2 + (int)(sinf(x_norm + layer_phase + layer_offset) * height * 0.3f);
            
            if (y >= 0 && y < height) {
                char phase_char = ".-+=#"[layer % 5];
                set_pixel(buffer, zbuffer, width, height, x, y, phase_char, 5.0f);
                
                // Interference patterns
                if (interference > 0.5f && layer > 0) {
                    int y2 = height / 2 - (int)(sinf(x_norm + layer_phase + layer_offset) * height * 0.3f);
                    if (y2 >= 0 && y2 < height) {
                        set_pixel(buffer, zbuffer, width, height, x, y2, ':', 6.0f);
                    }
                }
            }
        }
    }
    
    // Phase markers
    int marker_x = (int)(time * phase_speed * 10) % width;
    for (int y = 0; y < height; y++) {
        set_pixel(buffer, zbuffer, width, height, marker_x, y, '|', 2.0f);
    }
}

// Scene 158: Ikeda Binary - Binary number patterns
void scene_ikeda_binary(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float scroll_speed = params[0].value * 10.0f;
    float bit_density = params[1].value;
    float pattern_type = params[2].value * 3.0f;
    
    int pattern = (int)pattern_type % 3;
    int time_offset = (int)(time * scroll_speed);
    
    switch (pattern) {
        case 0: // Binary counter
            for (int y = 0; y < height; y++) {
                unsigned int value = (y + time_offset) * 137; // Prime for interesting patterns
                
                for (int bit = 0; bit < 32 && bit * 2 < width; bit++) {
                    if (((float)rand() / RAND_MAX) < bit_density) {
                        char bit_char = (value & (1 << bit)) ? '1' : '0';
                        set_pixel(buffer, zbuffer, width, height, bit * 2, y, bit_char, 5.0f);
                    }
                }
            }
            break;
            
        case 1: // Binary rain
            for (int x = 0; x < width; x += 2) {
                int column_seed = x * 31 + time_offset;
                
                for (int y = 0; y < height; y++) {
                    if (((column_seed + y * 17) % 100) < (int)(bit_density * 30)) {
                        char bit = ((column_seed + y) % 2) ? '1' : '0';
                        set_pixel(buffer, zbuffer, width, height, x, y, bit, 5.0f);
                    }
                }
            }
            break;
            
        case 2: // Binary blocks
            int block_size = 8;
            for (int by = 0; by < height / block_size; by++) {
                for (int bx = 0; bx < width / block_size; bx++) {
                    unsigned int block_value = (bx + by * 16 + time_offset) * 73;
                    
                    // Convert to binary string
                    for (int i = 0; i < 8 && i < block_size; i++) {
                        int px = bx * block_size + i;
                        int py = by * block_size + block_size / 2;
                        
                        if (px < width && py < height) {
                            char bit = (block_value & (1 << i)) ? '1' : '0';
                            set_pixel(buffer, zbuffer, width, height, px, py, bit, 5.0f);
                        }
                    }
                    
                    // Block borders
                    for (int i = 0; i < block_size; i++) {
                        int px1 = bx * block_size;
                        int py1 = by * block_size + i;
                        int px2 = bx * block_size + i;
                        int py2 = by * block_size;
                        
                        if (px1 < width && py1 < height) {
                            set_pixel(buffer, zbuffer, width, height, px1, py1, '|', 7.0f);
                        }
                        if (px2 < width && py2 < height) {
                            set_pixel(buffer, zbuffer, width, height, px2, py2, '-', 7.0f);
                        }
                    }
                }
            }
            break;
    }
}

// Scene 159: Ikeda Circuit - Circuit board patterns
void scene_ikeda_circuit(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float signal_speed = params[0].value * 5.0f;
    float complexity = params[1].value * 10.0f + 3.0f;
    float signal_density = params[2].value;
    
    // Circuit paths
    int num_paths = (int)complexity;
    
    for (int path = 0; path < num_paths; path++) {
        int start_x = (path * width / num_paths) + (int)(sinf(path * 1.3f) * 10);
        int start_y = height / 4 + (int)(cosf(path * 0.7f) * height / 4);
        
        int current_x = start_x;
        int current_y = start_y;
        
        // Trace path
        for (int step = 0; step < 50; step++) {
            if (current_x >= 0 && current_x < width && current_y >= 0 && current_y < height) {
                // Path character
                char path_char = '-';
                int direction = (step + path * 7) % 4;
                
                switch (direction) {
                    case 0: // Right
                        current_x++;
                        path_char = '-';
                        break;
                    case 1: // Down
                        current_y++;
                        path_char = '|';
                        break;
                    case 2: // Left
                        current_x--;
                        path_char = '-';
                        break;
                    case 3: // Up
                        current_y--;
                        path_char = '|';
                        break;
                }
                
                // Junction points
                if (step % 8 == 0) {
                    path_char = '+';
                    
                    // Node
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = current_x + dx;
                            int ny = current_y + dy;
                            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                if (abs(dx) + abs(dy) == 1) {
                                    set_pixel(buffer, zbuffer, width, height, nx, ny, '*', 4.0f);
                                }
                            }
                        }
                    }
                }
                
                set_pixel(buffer, zbuffer, width, height, current_x, current_y, path_char, 5.0f);
                
                // Signal flow
                if (((float)rand() / RAND_MAX) < signal_density) {
                    float signal_pos = fmodf(time * signal_speed + path * 0.5f, 50.0f);
                    if (fabsf(step - signal_pos) < 2.0f) {
                        set_pixel(buffer, zbuffer, width, height, current_x, current_y, 'o', 2.0f);
                    }
                }
            }
        }
    }
    
    // Component markers
    for (int i = 0; i < (int)(complexity * 2); i++) {
        int comp_x = ((i * 47 + (int)(time * 0.1f)) % (width - 4)) + 2;
        int comp_y = ((i * 31 + (int)(time * 0.1f)) % (height - 2)) + 1;
        
        // Draw component
        const char* components[] = {"[=]", "<o>", "{#}", "|*|"};
        const char* comp = components[i % 4];
        
        for (int c = 0; c < 3; c++) {
            if (comp_x + c < width) {
                set_pixel(buffer, zbuffer, width, height, comp_x + c, comp_y, comp[c], 3.0f);
            }
        }
    }
}

// ============= GIGER-INSPIRED SCENES (160-169) =============

// Scene 160: Biomechanical Spine - Animated vertebrae structure
void scene_giger_spine(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float spine_wave = params[0].value * 2.0f;
    float bone_size = params[1].value * 5.0f + 3.0f;
    float mutation_rate = params[2].value;
    
    int spine_x = width / 2;
    
    // Animated spine
    for (int segment = 0; segment < height / (int)bone_size; segment++) {
        float segment_time = time + segment * 0.3f;
        float wave_offset = sinf(segment_time * spine_wave) * 10.0f;
        float twist = sinf(segment_time * 1.5f) * mutation_rate;
        
        int y = segment * (int)bone_size + (int)bone_size / 2;
        int x = spine_x + (int)wave_offset;
        
        // Vertebra structure
        for (int dx = -(int)bone_size; dx <= (int)bone_size; dx++) {
            int px = x + dx;
            if (px >= 0 && px < width) {
                // Main bone
                set_pixel(buffer, zbuffer, width, height, px, y, '=', 5.0f);
                
                // Ribs
                if (abs(dx) > 2) {
                    int rib_y = y + (int)(sinf(dx * 0.5f + twist) * 3);
                    if (rib_y >= 0 && rib_y < height) {
                        char rib_char = (dx > 0) ? '\\' : '/';
                        set_pixel(buffer, zbuffer, width, height, px, rib_y, rib_char, 6.0f);
                        
                        // Rib connections
                        for (int ry = -1; ry <= 1; ry++) {
                            if (rib_y + ry >= 0 && rib_y + ry < height) {
                                set_pixel(buffer, zbuffer, width, height, px, rib_y + ry, '|', 7.0f);
                            }
                        }
                    }
                }
            }
        }
        
        // Spinal cord
        for (int dy = 0; dy < (int)bone_size; dy++) {
            int cord_y = y - (int)bone_size / 2 + dy;
            if (cord_y >= 0 && cord_y < height && x >= 0 && x < width) {
                set_pixel(buffer, zbuffer, width, height, x, cord_y, '|', 4.0f);
                
                // Neural connections
                if (dy % 2 == 0) {
                    int nerve_x1 = x - (int)(sinf(segment_time * 3) * 5);
                    int nerve_x2 = x + (int)(cosf(segment_time * 3) * 5);
                    if (nerve_x1 >= 0 && nerve_x1 < width) {
                        set_pixel(buffer, zbuffer, width, height, nerve_x1, cord_y, '.', 8.0f);
                    }
                    if (nerve_x2 >= 0 && nerve_x2 < width) {
                        set_pixel(buffer, zbuffer, width, height, nerve_x2, cord_y, '.', 8.0f);
                    }
                }
            }
        }
    }
}

// Scene 161: Alien Egg Chamber - Pulsating organic pods
void scene_giger_eggs(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float pulse_rate = params[0].value * 3.0f;
    float egg_count = params[1].value * 8.0f + 4.0f;
    float hatch_progress = params[2].value;
    
    int eggs_per_row = (int)sqrtf(egg_count);
    float egg_spacing_x = (float)width / (eggs_per_row + 1);
    float egg_spacing_y = (float)height / (eggs_per_row + 1);
    
    for (int row = 0; row < eggs_per_row; row++) {
        for (int col = 0; col < eggs_per_row; col++) {
            int egg_x = (int)((col + 1) * egg_spacing_x);
            int egg_y = (int)((row + 1) * egg_spacing_y);
            
            // Individual egg pulse
            float egg_phase = time * pulse_rate + row * 0.3f + col * 0.5f;
            float pulse = (sinf(egg_phase) + 1.0f) * 0.5f;
            int radius = (int)(5 + pulse * 3);
            
            // Egg shell
            for (int angle = 0; angle < 360; angle += 10) {
                float rad = angle * M_PI / 180.0f;
                int x = egg_x + (int)(radius * cosf(rad) * 1.5f);
                int y = egg_y + (int)(radius * sinf(rad));
                
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    char egg_char = (pulse > 0.5f) ? '@' : 'O';
                    
                    // Cracking eggs
                    if (hatch_progress > 0.5f && ((angle + (int)(time * 50)) % 60 < 20)) {
                        egg_char = (angle % 40 < 20) ? '/' : '\\';
                    }
                    
                    set_pixel(buffer, zbuffer, width, height, x, y, egg_char, 5.0f);
                }
            }
            
            // Inner creature movement
            if (pulse > 0.7f) {
                int creature_x = egg_x + (int)(sinf(egg_phase * 2) * 2);
                int creature_y = egg_y + (int)(cosf(egg_phase * 2) * 1);
                if (creature_x >= 0 && creature_x < width && creature_y >= 0 && creature_y < height) {
                    set_pixel(buffer, zbuffer, width, height, creature_x, creature_y, '*', 3.0f);
                }
            }
            
            // Slime trails
            for (int trail = 0; trail < 5; trail++) {
                int trail_y = egg_y + radius + trail;
                if (trail_y < height && egg_x >= 0 && egg_x < width) {
                    char slime = (trail % 2) ? '.' : ':';
                    set_pixel(buffer, zbuffer, width, height, egg_x + (trail % 3) - 1, trail_y, slime, 7.0f);
                }
            }
        }
    }
}

// Scene 162: Mechanical Tentacles - Writhing biomechanical appendages
void scene_giger_tentacles(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float writhe_speed = params[0].value * 2.0f;
    float tentacle_count = params[1].value * 6.0f + 3.0f;
    float segment_detail = params[2].value * 3.0f + 1.0f;
    
    for (int t = 0; t < (int)tentacle_count; t++) {
        float base_x = width * (t + 1) / (tentacle_count + 1);
        float base_y = height - 5;
        
        // Tentacle segments
        int segments = 20;
        for (int seg = 0; seg < segments; seg++) {
            float seg_phase = time * writhe_speed + t * 0.5f + seg * 0.1f;
            
            // Undulating motion
            float x = base_x + sinf(seg_phase) * (20 + seg);
            float y = base_y - seg * 2.5f;
            
            // Twist motion
            x += cosf(seg_phase * 2 + seg * 0.2f) * 10;
            
            int px = (int)x;
            int py = (int)y;
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                // Main tentacle body
                char body_char = (seg % 2) ? 'O' : 'o';
                set_pixel(buffer, zbuffer, width, height, px, py, body_char, 5.0f - seg * 0.1f);
                
                // Mechanical segments
                if (seg % (int)segment_detail == 0) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int sx = px + dx;
                        if (sx >= 0 && sx < width) {
                            char seg_char = (dx == 0) ? '|' : (abs(dx) == 1) ? '[' : ']';
                            if (dx < 0) seg_char = '[';
                            if (dx > 0) seg_char = ']';
                            set_pixel(buffer, zbuffer, width, height, sx, py, seg_char, 4.0f);
                        }
                    }
                }
                
                // Suction cups
                if (seg % 3 == 0) {
                    int sucker_x = px + (seg % 2 ? 3 : -3);
                    if (sucker_x >= 0 && sucker_x < width) {
                        set_pixel(buffer, zbuffer, width, height, sucker_x, py, '0', 6.0f);
                    }
                }
            }
        }
        
        // Tentacle tip
        float tip_x = base_x + sinf(time * writhe_speed * 2 + t) * 30;
        float tip_y = base_y - segments * 2.5f;
        
        int tx = (int)tip_x;
        int ty = (int)tip_y;
        
        if (tx >= 0 && tx < width - 2 && ty >= 0 && ty < height) {
            set_pixel(buffer, zbuffer, width, height, tx, ty, '<', 2.0f);
            set_pixel(buffer, zbuffer, width, height, tx + 1, ty, '*', 2.0f);
            set_pixel(buffer, zbuffer, width, height, tx + 2, ty, '>', 2.0f);
        }
    }
}

// Scene 163: Xenomorph Hive - Organic architecture with movement
void scene_giger_hive(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float growth_rate = params[0].value;
    float density = params[1].value * 0.8f + 0.2f;
    float creature_activity = params[2].value;
    
    // Organic walls
    for (int x = 0; x < width; x++) {
        float wall_phase = x * 0.1f + time * growth_rate;
        int wall_height = height / 3 + (int)(sinf(wall_phase) * height / 6);
        
        // Top resin structure
        for (int y = 0; y < wall_height; y++) {
            if (((x + y) % 3 == 0) && ((float)rand() / RAND_MAX < density)) {
                char wall_char = "{}[]|/"[(x + y + (int)(time * 2)) % 6];
                set_pixel(buffer, zbuffer, width, height, x, y, wall_char, 8.0f);
            }
        }
        
        // Bottom structure
        wall_height = height / 3 + (int)(cosf(wall_phase + M_PI) * height / 6);
        for (int y = height - wall_height; y < height; y++) {
            if (((x + y) % 3 == 0) && ((float)rand() / RAND_MAX < density)) {
                char wall_char = "{}[]|\\"[(x + y + (int)(time * 2)) % 6];
                set_pixel(buffer, zbuffer, width, height, x, y, wall_char, 8.0f);
            }
        }
    }
    
    // Resin strands
    for (int strand = 0; strand < 15; strand++) {
        float strand_x = width * (strand + 1) / 16.0f;
        float sway = sinf(time * 0.5f + strand * 0.3f) * 5;
        
        for (int y = 0; y < height; y++) {
            int x = (int)(strand_x + sway * sinf(y * 0.1f));
            if (x >= 0 && x < width) {
                if (y % 3 != 0) {
                    set_pixel(buffer, zbuffer, width, height, x, y, '|', 6.0f);
                }
            }
        }
    }
    
    // Moving creatures
    if (creature_activity > 0.1f) {
        for (int c = 0; c < (int)(creature_activity * 5); c++) {
            float creature_time = time * 2 + c * 1.7f;
            int cx = (int)(width * 0.5f + sinf(creature_time) * width * 0.4f);
            int cy = (int)(height * 0.5f + cosf(creature_time * 0.7f) * height * 0.3f);
            
            if (cx >= 1 && cx < width - 1 && cy >= 0 && cy < height) {
                // Alien body
                set_pixel(buffer, zbuffer, width, height, cx, cy, '@', 2.0f);
                set_pixel(buffer, zbuffer, width, height, cx - 1, cy, '<', 2.1f);
                set_pixel(buffer, zbuffer, width, height, cx + 1, cy, '>', 2.1f);
                
                // Tail
                for (int tail = 1; tail < 5; tail++) {
                    int tx = cx - (int)(sinf(creature_time - tail * 0.2f) * 2);
                    int ty = cy + tail;
                    if (tx >= 0 && tx < width && ty < height) {
                        set_pixel(buffer, zbuffer, width, height, tx, ty, '~', 2.5f);
                    }
                }
            }
        }
    }
}

// Scene 164: Biomech Skull - Animated skull with mechanical parts
void scene_giger_skull(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float rotation = params[0].value * 2.0f;
    float jaw_movement = params[1].value;
    float decay_level = params[2].value;
    
    int cx = width / 2;
    int cy = height / 2;
    
    // Rotating view
    float angle = time * rotation;
    float face_width = 15 + sinf(angle) * 5;
    
    // Skull dome
    for (int y = -10; y < 0; y++) {
        int dome_width = (int)(face_width * (1.0f - (float)(-y) / 10.0f));
        for (int x = -dome_width; x <= dome_width; x++) {
            int px = cx + x;
            int py = cy + y;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char dome_char = (abs(x) == dome_width) ? '(' : (x == 0 && y % 2 == 0) ? '|' : '.';
                if (abs(x) == dome_width && x < 0) dome_char = '(';
                if (abs(x) == dome_width && x > 0) dome_char = ')';
                set_pixel(buffer, zbuffer, width, height, px, py, dome_char, 5.0f);
            }
        }
    }
    
    // Eye sockets with mechanical eyes
    int eye_y = cy - 3;
    for (int eye = -1; eye <= 1; eye += 2) {
        int eye_x = cx + eye * 6;
        
        // Socket
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                int px = eye_x + dx;
                int py = eye_y + dy;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    if (abs(dx) + abs(dy) <= 4) {
                        char socket_char = ' ';
                        if (abs(dx) + abs(dy) == 4) socket_char = 'O';
                        else if (abs(dx) + abs(dy) == 3) socket_char = '0';
                        set_pixel(buffer, zbuffer, width, height, px, py, socket_char, 4.0f);
                    }
                }
            }
        }
        
        // Mechanical eye
        float eye_track = time * 3 + eye;
        int pupil_x = eye_x + (int)(sinf(eye_track) * 2);
        int pupil_y = eye_y + (int)(cosf(eye_track) * 1);
        if (pupil_x >= 0 && pupil_x < width && pupil_y >= 0 && pupil_y < height) {
            set_pixel(buffer, zbuffer, width, height, pupil_x, pupil_y, '*', 2.0f);
        }
    }
    
    // Nasal cavity
    for (int y = 0; y < 4; y++) {
        int nose_width = 3 - y;
        for (int x = -nose_width; x <= nose_width; x++) {
            int px = cx + x;
            int py = cy + y;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char nose_char = (abs(x) == nose_width) ? '/' : ' ';
                if (x < 0) nose_char = '\\';
                if (x > 0) nose_char = '/';
                if (x == 0 && y == 3) nose_char = 'v';
                set_pixel(buffer, zbuffer, width, height, px, py, nose_char, 4.5f);
            }
        }
    }
    
    // Jaw with animation
    float jaw_angle = sinf(time * 5) * jaw_movement * 0.5f + 0.2f;
    int jaw_y = cy + 5 + (int)(jaw_angle * 5);
    
    // Teeth
    for (int tooth = -8; tooth <= 8; tooth += 2) {
        int tx = cx + tooth;
        int ty = jaw_y - 1;
        if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
            set_pixel(buffer, zbuffer, width, height, tx, ty, 'V', 3.0f);
        }
        ty = jaw_y + 1;
        if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
            set_pixel(buffer, zbuffer, width, height, tx, ty, '^', 3.0f);
        }
    }
    
    // Jaw bone
    for (int x = -10; x <= 10; x++) {
        int px = cx + x;
        int py = jaw_y;
        if (px >= 0 && px < width && py >= 0 && py < height) {
            set_pixel(buffer, zbuffer, width, height, px, py, '=', 3.5f);
        }
    }
    
    // Mechanical parts
    if (decay_level > 0.3f) {
        // Pistons
        for (int side = -1; side <= 1; side += 2) {
            int piston_x = cx + side * 12;
            for (int y = cy - 5; y <= jaw_y; y++) {
                if (piston_x >= 0 && piston_x < width && y >= 0 && y < height) {
                    char piston_char = (y % 3 == 0) ? '[' : '|';
                    if (side > 0) piston_char = (y % 3 == 0) ? ']' : '|';
                    set_pixel(buffer, zbuffer, width, height, piston_x, y, piston_char, 6.0f);
                }
            }
        }
    }
}

// Scene 165: Face Hugger - Animated parasitic creature
void scene_giger_facehugger(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float scuttle_speed = params[0].value * 3.0f;
    float leg_movement = params[1].value * 2.0f;
    float tail_whip = params[2].value * 3.0f;
    
    // Creature position
    float creature_x = width * 0.5f + sinf(time * scuttle_speed * 0.5f) * width * 0.3f;
    float creature_y = height * 0.5f + cosf(time * scuttle_speed * 0.3f) * height * 0.2f;
    
    int cx = (int)creature_x;
    int cy = (int)creature_y;
    
    // Central body
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            int px = cx + dx;
            int py = cy + dy;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                if (abs(dx) + abs(dy) <= 4) {
                    char body_char = '@';
                    if (abs(dx) + abs(dy) == 4) body_char = '0';
                    set_pixel(buffer, zbuffer, width, height, px, py, body_char, 3.0f);
                }
            }
        }
    }
    
    // Eight legs with movement
    for (int leg = 0; leg < 8; leg++) {
        float leg_angle = (leg * M_PI / 4) + time * leg_movement;
        float leg_extend = 8 + sinf(time * leg_movement * 2 + leg) * 3;
        
        // Leg segments
        for (int segment = 0; segment < 3; segment++) {
            float seg_dist = leg_extend * (segment + 1) / 3.0f;
            int lx = cx + (int)(cosf(leg_angle) * seg_dist);
            int ly = cy + (int)(sinf(leg_angle) * seg_dist);
            
            if (lx >= 0 && lx < width && ly >= 0 && ly < height) {
                char leg_char = (segment == 2) ? '*' : 'o';
                set_pixel(buffer, zbuffer, width, height, lx, ly, leg_char, 4.0f + segment);
                
                // Connecting joint
                if (segment < 2) {
                    int jx = cx + (int)(cosf(leg_angle) * seg_dist * 1.3f);
                    int jy = cy + (int)(sinf(leg_angle) * seg_dist * 1.3f);
                    if (jx >= 0 && jx < width && jy >= 0 && jy < height) {
                        set_pixel(buffer, zbuffer, width, height, jx, jy, '-', 5.0f);
                    }
                }
            }
        }
    }
    
    // Whipping tail
    float tail_base_angle = M_PI + time * 0.5f;
    for (int tail_seg = 0; tail_seg < 15; tail_seg++) {
        float tail_wave = sinf(time * tail_whip + tail_seg * 0.3f) * 0.5f;
        float seg_angle = tail_base_angle + tail_wave;
        
        int tx = cx + (int)(cosf(seg_angle) * (tail_seg + 3));
        int ty = cy + (int)(sinf(seg_angle) * (tail_seg + 3));
        
        if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
            char tail_char = (tail_seg < 10) ? '~' : ':';
            set_pixel(buffer, zbuffer, width, height, tx, ty, tail_char, 6.0f + tail_seg * 0.1f);
        }
    }
    
    // Breathing sacs
    float breathe = (sinf(time * 4) + 1.0f) * 0.5f;
    for (int sac = 0; sac < 4; sac++) {
        float sac_angle = sac * M_PI / 2 + M_PI / 4;
        int sac_dist = 4 + (int)(breathe * 2);
        int sx = cx + (int)(cosf(sac_angle) * sac_dist);
        int sy = cy + (int)(sinf(sac_angle) * sac_dist);
        
        if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
            char sac_char = breathe > 0.5f ? 'O' : 'o';
            set_pixel(buffer, zbuffer, width, height, sx, sy, sac_char, 2.5f);
        }
    }
}

// Scene 166: Biomech Heart - Pulsating mechanical organ
void scene_giger_heart(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float beat_rate = params[0].value * 2.0f + 0.5f;
    float valve_count = params[1].value * 3.0f + 2.0f;
    float blood_flow = params[2].value;
    
    int cx = width / 2;
    int cy = height / 2;
    
    // Heart beat cycle
    float beat_phase = time * beat_rate;
    float pulse = (sinf(beat_phase * 2 * M_PI) + 1.0f) * 0.5f;
    pulse = pulse * pulse; // Sharp beats
    
    // Heart chambers
    int heart_size = 8 + (int)(pulse * 4);
    
    // Left ventricle
    for (int y = -heart_size; y <= heart_size / 2; y++) {
        int chamber_width = heart_size - abs(y);
        for (int x = -chamber_width; x < 0; x++) {
            int px = cx + x;
            int py = cy + y;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char chamber_char = (abs(x) == chamber_width - 1 || y == -heart_size || y == heart_size / 2) ? '#' : '@';
                set_pixel(buffer, zbuffer, width, height, px, py, chamber_char, 5.0f);
            }
        }
    }
    
    // Right ventricle
    for (int y = -heart_size; y <= heart_size / 2; y++) {
        int chamber_width = heart_size - abs(y);
        for (int x = 1; x <= chamber_width; x++) {
            int px = cx + x;
            int py = cy + y;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char chamber_char = (x == chamber_width || y == -heart_size || y == heart_size / 2) ? '#' : '@';
                set_pixel(buffer, zbuffer, width, height, px, py, chamber_char, 5.0f);
            }
        }
    }
    
    // Mechanical valves
    for (int v = 0; v < (int)valve_count; v++) {
        float valve_angle = v * 2 * M_PI / valve_count + time * 0.5f;
        int valve_dist = heart_size + 3;
        int vx = cx + (int)(cosf(valve_angle) * valve_dist);
        int vy = cy + (int)(sinf(valve_angle) * valve_dist);
        
        if (vx >= 0 && vx < width && vy >= 0 && vy < height) {
            // Valve mechanism
            char valve_char = (pulse > 0.5f) ? 'X' : '+';
            set_pixel(buffer, zbuffer, width, height, vx, vy, valve_char, 3.0f);
            
            // Connecting tubes
            int dx = (vx > cx) ? -1 : 1;
            int dy = (vy > cy) ? -1 : 1;
            for (int i = 1; i < 4; i++) {
                int tx = vx + dx * i;
                int ty = vy + dy * i;
                if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
                    set_pixel(buffer, zbuffer, width, height, tx, ty, '=', 6.0f);
                }
            }
        }
    }
    
    // Blood flow particles
    if (blood_flow > 0.1f) {
        for (int p = 0; p < 20; p++) {
            float particle_phase = beat_phase + p * 0.1f;
            float flow_angle = particle_phase * 3;
            int flow_radius = heart_size + 5 + (int)(sinf(particle_phase * 5) * 3);
            
            int px = cx + (int)(cosf(flow_angle) * flow_radius);
            int py = cy + (int)(sinf(flow_angle) * flow_radius);
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char blood_char = ".o*"[(int)(particle_phase * 10) % 3];
                set_pixel(buffer, zbuffer, width, height, px, py, blood_char, 7.0f);
            }
        }
    }
    
    // Arteries
    const int directions[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (int d = 0; d < 4; d++) {
        int ax = cx + directions[d][0] * (heart_size + 2);
        int ay = cy + directions[d][1] * (heart_size + 2);
        
        for (int i = 0; i < 8; i++) {
            int px = ax + directions[d][0] * i;
            int py = ay + directions[d][1] * i;
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char artery_char = (i % 2) ? '|' : 'H';
                if (directions[d][0] != 0) artery_char = (i % 2) ? '-' : 'H';
                set_pixel(buffer, zbuffer, width, height, px, py, artery_char, 8.0f);
            }
        }
    }
}

// Scene 167: Alien Architecture - Living building structures
void scene_giger_architecture(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float growth_phase = params[0].value * time * 0.5f;
    float structure_complexity = params[1].value * 5.0f + 3.0f;
    float organic_factor = params[2].value;
    
    // Base columns with organic growth
    int num_columns = (int)structure_complexity;
    for (int col = 0; col < num_columns; col++) {
        int col_x = (col + 1) * width / (num_columns + 1);
        float col_phase = growth_phase + col * 0.3f;
        
        // Column with breathing effect
        for (int y = 0; y < height; y++) {
            float breathe = sinf(col_phase + y * 0.1f) * organic_factor;
            int width_mod = (int)(2 + breathe * 2);
            
            for (int dx = -width_mod; dx <= width_mod; dx++) {
                int px = col_x + dx;
                if (px >= 0 && px < width) {
                    char col_char = '|';
                    if (abs(dx) == width_mod) col_char = (dx < 0) ? '[' : ']';
                    if (y % 5 == 0) col_char = '=';
                    
                    set_pixel(buffer, zbuffer, width, height, px, y, col_char, 8.0f);
                }
            }
            
            // Organic tendrils
            if (y % 8 == 0 && organic_factor > 0.3f) {
                for (int tendril = -1; tendril <= 1; tendril += 2) {
                    for (int t = 0; t < 10; t++) {
                        int tx = col_x + tendril * (5 + t) + (int)(sinf(col_phase + t * 0.3f) * 3);
                        int ty = y + (int)(cosf(col_phase + t * 0.3f) * 2);
                        
                        if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
                            char tendril_char = (t < 5) ? '~' : '.';
                            set_pixel(buffer, zbuffer, width, height, tx, ty, tendril_char, 7.0f - t * 0.1f);
                        }
                    }
                }
            }
        }
    }
    
    // Ribbed ceiling
    for (int x = 0; x < width; x++) {
        float rib_curve = sinf(x * 0.1f + growth_phase) * 5 + 5;
        int rib_y = (int)rib_curve;
        
        if (rib_y >= 0 && rib_y < height) {
            set_pixel(buffer, zbuffer, width, height, x, rib_y, '~', 6.0f);
            
            // Hanging structures
            if (x % 6 == 0) {
                int hang_length = 3 + (int)(sinf(growth_phase + x) * 2);
                for (int h = 1; h <= hang_length; h++) {
                    int hy = rib_y + h;
                    if (hy < height) {
                        char hang_char = (h == hang_length) ? 'v' : '|';
                        set_pixel(buffer, zbuffer, width, height, x, hy, hang_char, 5.0f);
                    }
                }
            }
        }
    }
    
    // Floor with biomechanical grating
    int floor_y = height - 5;
    for (int x = 0; x < width; x++) {
        for (int y = floor_y; y < height; y++) {
            if ((x + y) % 3 == 0) {
                char floor_char = ((x % 6) < 3) ? '#' : '=';
                set_pixel(buffer, zbuffer, width, height, x, y, floor_char, 9.0f);
            }
        }
    }
    
    // Pulsating doorways
    for (int door = 1; door < num_columns; door++) {
        int door_x = door * width / num_columns;
        float door_pulse = (sinf(growth_phase + door) + 1.0f) * 0.5f;
        int door_width = 5 + (int)(door_pulse * 3);
        int door_height = 15 + (int)(door_pulse * 5);
        
        // Organic door frame
        for (int dy = floor_y - door_height; dy < floor_y; dy++) {
            for (int dx = -door_width; dx <= door_width; dx++) {
                int px = door_x + dx;
                if (px >= 0 && px < width && dy >= 0) {
                    if (abs(dx) == door_width || dy == floor_y - door_height) {
                        char door_char = (abs(dx) == door_width) ? ')' : '_';
                        if (dx < 0) door_char = '(';
                        set_pixel(buffer, zbuffer, width, height, px, dy, door_char, 4.0f);
                    }
                }
            }
        }
    }
}

// Scene 168: Chestburster - Emerging creature animation
void scene_giger_chestburster(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float emergence_progress = fmodf(time * params[0].value * 0.3f, 1.0f);
    float writhe_speed = params[1].value * 3.0f;
    float gore_level = params[2].value;
    
    int cx = width / 2;
    int cy = height / 2;
    
    // Host body outline
    int body_width = 20;
    int body_height = 10;
    
    for (int y = -body_height; y <= body_height; y++) {
        for (int x = -body_width; x <= body_width; x++) {
            int px = cx + x;
            int py = cy + y + 5;
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                // Body outline
                if (abs(x) == body_width || abs(y) == body_height) {
                    set_pixel(buffer, zbuffer, width, height, px, py, '#', 8.0f);
                }
                
                // Ribs
                if (abs(y) < body_height - 2 && abs(x) < body_width - 2) {
                    if (x % 3 == 0 && y >= -5 && y <= 5) {
                        char rib_char = (y == 0 && emergence_progress > 0.3f) ? ' ' : '|';
                        set_pixel(buffer, zbuffer, width, height, px, py, rib_char, 7.0f);
                    }
                }
            }
        }
    }
    
    // Emergence wound
    if (emergence_progress > 0.2f) {
        float wound_size = (emergence_progress - 0.2f) * 10;
        for (int angle = 0; angle < 360; angle += 30) {
            float rad = angle * M_PI / 180.0f;
            int wx = cx + (int)(cosf(rad) * wound_size);
            int wy = cy + 5 + (int)(sinf(rad) * wound_size * 0.5f);
            
            if (wx >= 0 && wx < width && wy >= 0 && wy < height) {
                char wound_char = "*x+."[angle / 90];
                set_pixel(buffer, zbuffer, width, height, wx, wy, wound_char, 5.0f);
            }
        }
    }
    
    // Chestburster creature
    if (emergence_progress > 0.3f) {
        float creature_height = (emergence_progress - 0.3f) * 20;
        float writhe = sinf(time * writhe_speed) * 3;
        
        // Serpentine body
        for (int seg = 0; seg < (int)creature_height; seg++) {
            float seg_wave = sinf(time * writhe_speed + seg * 0.3f) * writhe;
            int sx = cx + (int)seg_wave;
            int sy = cy + 5 - seg;
            
            if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                char body_char = (seg % 2) ? 'S' : 's';
                set_pixel(buffer, zbuffer, width, height, sx, sy, body_char, 2.0f);
                
                // Side details
                if (seg % 3 == 0) {
                    if (sx - 1 >= 0) set_pixel(buffer, zbuffer, width, height, sx - 1, sy, '(', 2.1f);
                    if (sx + 1 < width) set_pixel(buffer, zbuffer, width, height, sx + 1, sy, ')', 2.1f);
                }
            }
        }
        
        // Head
        if (creature_height > 5) {
            int head_x = cx + (int)(sinf(time * writhe_speed) * writhe);
            int head_y = cy + 5 - (int)creature_height - 2;
            
            if (head_x >= 1 && head_x < width - 1 && head_y >= 0 && head_y < height - 1) {
                // Elongated head
                set_pixel(buffer, zbuffer, width, height, head_x, head_y - 1, '^', 1.0f);
                set_pixel(buffer, zbuffer, width, height, head_x - 1, head_y, '<', 1.0f);
                set_pixel(buffer, zbuffer, width, height, head_x, head_y, '@', 1.0f);
                set_pixel(buffer, zbuffer, width, height, head_x + 1, head_y, '>', 1.0f);
                
                // Inner jaw
                if (fmodf(time * 5, 1.0f) > 0.5f) {
                    set_pixel(buffer, zbuffer, width, height, head_x, head_y + 1, 'v', 0.5f);
                }
            }
        }
    }
    
    // Blood splatter
    if (gore_level > 0.1f && emergence_progress > 0.4f) {
        for (int splat = 0; splat < (int)(gore_level * 20); splat++) {
            float splat_angle = splat * 0.8f + time;
            float splat_dist = 5 + (emergence_progress - 0.4f) * 30 + splat % 10;
            
            int sx = cx + (int)(cosf(splat_angle) * splat_dist);
            int sy = cy + 5 + (int)(sinf(splat_angle) * splat_dist * 0.7f);
            
            if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                char blood_char = ".,:*"[splat % 4];
                set_pixel(buffer, zbuffer, width, height, sx, sy, blood_char, 6.0f);
            }
        }
    }
}

// Scene 169: Space Jockey - Giant biomechanical pilot
void scene_giger_space_jockey(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    if (!buffer || !zbuffer || !params) return;
    if (width <= 0 || height <= 0) return;
    
    clear_buffer(buffer, zbuffer, width, height);
    
    float chair_tilt = params[0].value * 0.3f;
    float trunk_sway = params[1].value * 2.0f;
    float control_activity = params[2].value;
    
    int cx = width / 2;
    int base_y = height - 10;
    
    // Pilot chair base
    for (int y = base_y; y < height; y++) {
        int chair_width = 25 - (y - base_y) * 2;
        for (int x = -chair_width; x <= chair_width; x++) {
            int px = cx + x;
            if (px >= 0 && px < width) {
                char base_char = '=';
                if (abs(x) == chair_width) base_char = (x < 0) ? '/' : '\\';
                if (y == base_y) base_char = '_';
                set_pixel(buffer, zbuffer, width, height, px, y, base_char, 9.0f);
            }
        }
    }
    
    // Chair back with tilt
    float tilt = sinf(time * 0.5f) * chair_tilt;
    for (int y = 0; y < base_y; y++) {
        int back_x = cx + (int)((y - base_y) * tilt);
        int back_width = 15 - y / 3;
        
        for (int x = -back_width; x <= back_width; x++) {
            int px = back_x + x;
            if (px >= 0 && px < width && y >= 0) {
                if (abs(x) == back_width || y % 5 == 0) {
                    char chair_char = '|';
                    if (y % 5 == 0) chair_char = '-';
                    if (abs(x) == back_width) chair_char = (x < 0) ? '(' : ')';
                    set_pixel(buffer, zbuffer, width, height, px, y, chair_char, 8.0f);
                }
            }
        }
    }
    
    // Space Jockey body
    int body_y = base_y - 15;
    
    // Torso
    for (int y = -8; y <= 8; y++) {
        int torso_width = 10 - abs(y) / 2;
        for (int x = -torso_width; x <= torso_width; x++) {
            int px = cx + x;
            int py = body_y + y;
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char torso_char = '@';
                if (abs(x) == torso_width) torso_char = (x < 0) ? '{' : '}';
                if (y % 3 == 0) torso_char = '=';
                set_pixel(buffer, zbuffer, width, height, px, py, torso_char, 5.0f);
            }
        }
    }
    
    // Elephant trunk
    float trunk_base_angle = -M_PI / 2 + sinf(time * trunk_sway) * 0.3f;
    for (int seg = 0; seg < 20; seg++) {
        float seg_angle = trunk_base_angle + seg * 0.1f + sinf(time * trunk_sway + seg * 0.2f) * 0.2f;
        int tx = cx + (int)(cosf(seg_angle) * (seg + 5));
        int ty = body_y - 8 + (int)(sinf(seg_angle) * (seg + 5));
        
        if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
            char trunk_char = 'O';
            if (seg < 5) trunk_char = '@';
            else if (seg < 15) trunk_char = 'o';
            else trunk_char = '.';
            
            set_pixel(buffer, zbuffer, width, height, tx, ty, trunk_char, 4.0f - seg * 0.1f);
            
            // Trunk segments
            if (seg % 3 == 0) {
                if (tx - 1 >= 0) set_pixel(buffer, zbuffer, width, height, tx - 1, ty, '(', 4.1f);
                if (tx + 1 < width) set_pixel(buffer, zbuffer, width, height, tx + 1, ty, ')', 4.1f);
            }
        }
    }
    
    // Control panel arms
    for (int arm = -1; arm <= 1; arm += 2) {
        int arm_base_x = cx + arm * 12;
        int arm_base_y = body_y;
        
        // Upper arm
        for (int i = 0; i < 8; i++) {
            int ax = arm_base_x + arm * i;
            int ay = arm_base_y + i / 2;
            
            if (ax >= 0 && ax < width && ay >= 0 && ay < height) {
                set_pixel(buffer, zbuffer, width, height, ax, ay, '=', 6.0f);
            }
        }
        
        // Forearm reaching to controls
        int elbow_x = arm_base_x + arm * 8;
        int elbow_y = arm_base_y + 4;
        
        for (int i = 0; i < 10; i++) {
            int fx = elbow_x + arm * (i / 2);
            int fy = elbow_y + i;
            
            if (fx >= 0 && fx < width && fy >= 0 && fy < height) {
                set_pixel(buffer, zbuffer, width, height, fx, fy, '|', 6.0f);
            }
        }
        
        // Control interface
        if (control_activity > 0.1f) {
            int control_x = elbow_x + arm * 5;
            int control_y = elbow_y + 10;
            
            if (control_x >= 1 && control_x < width - 1 && control_y >= 0 && control_y < height) {
                // Glowing controls
                char control_char = "*@#"[(int)(time * control_activity * 10) % 3];
                set_pixel(buffer, zbuffer, width, height, control_x, control_y, control_char, 2.0f);
                set_pixel(buffer, zbuffer, width, height, control_x - 1, control_y, '[', 2.1f);
                set_pixel(buffer, zbuffer, width, height, control_x + 1, control_y, ']', 2.1f);
            }
        }
    }
    
    // Fossilized details
    if (chair_tilt < 0.5f) {
        // Dust particles
        for (int dust = 0; dust < 30; dust++) {
            int dx = cx + (int)((dust - 15) * 3 + sinf(time * 0.1f + dust) * 5);
            int dy = (int)(dust * height / 30.0f + sinf(time * 0.2f + dust) * 2);
            
            if (dx >= 0 && dx < width && dy >= 0 && dy < height) {
                set_pixel(buffer, zbuffer, width, height, dx, dy, '.', 10.0f);
            }
        }
    }
}

// ============= REVOLT SCENES (170-179) =============

// Scene 170: Rising Fists - Multiple fists rising in protest
void scene_revolt_rising_fists(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float rise_speed = params[0].value; // 0.5-2.0
    float spread = params[1].value;     // 0.5-1.5
    float intensity = params[2].value;  // 0.5-1.0
    
    int num_fists = 20 + (int)(intensity * 30);
    
    for (int i = 0; i < num_fists; i++) {
        float phase = (float)i / num_fists;
        float x = sinf(phase * M_PI * 2 + time * 0.5f) * spread * width * 0.4f;
        float y_offset = sinf(time * rise_speed + phase * M_PI * 2) * 0.5f + 0.5f;
        
        int fist_x = width / 2 + (int)x;
        int fist_y = height - (int)(y_offset * height * 0.8f);
        
        // Draw fist
        const char* fist[] = {
            " ___ ",
            "/   \\",
            "|fst|",
            "\\___/",
            "  |  ",
            "  |  "
        };
        
        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 5; col++) {
                int px = fist_x + col - 2;
                int py = fist_y + row;
                
                if (px >= 0 && px < width && py >= 0 && py < height && fist[row][col] != ' ') {
                    float z = 10.0f - i * 0.1f;
                    set_pixel(buffer, zbuffer, width, height, px, py, fist[row][col], z);
                }
            }
        }
        
        // Motion lines
        if (y_offset > 0.3f) {
            for (int line = 0; line < 3; line++) {
                int ly = fist_y + 6 + line * 2;
                if (ly < height) {
                    set_pixel(buffer, zbuffer, width, height, fist_x, ly, '|', 15.0f);
                }
            }
        }
    }
    
    // Add protest signs
    const char* slogans[] = {"REVOLT!", "RESIST!", "FREEDOM!", "JUSTICE!"};
    for (int s = 0; s < 4; s++) {
        int sign_x = width / 4 + (s * width / 4);
        int sign_y = (int)(sinf(time * 0.8f + s) * 5 + 10);
        draw_text(buffer, zbuffer, width, height, sign_x - strlen(slogans[s])/2, sign_y, slogans[s], 5.0f);
    }
}

// Scene 171: Breaking Chains - Chains breaking apart symbolically
void scene_revolt_breaking_chains(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float break_progress = params[0].value; // 0.0-1.0
    float shake = params[1].value;          // 0.0-2.0
    float chain_count = params[2].value;    // 1-5
    
    int num_chains = (int)(chain_count * 5);
    
    for (int chain = 0; chain < num_chains; chain++) {
        float chain_x = (float)(chain + 1) * width / (num_chains + 1);
        float break_point = height * (0.3f + sinf(time * break_progress + chain) * 0.2f);
        
        // Draw chain links
        for (int y = 0; y < height; y++) {
            float link_offset = sinf(y * 0.5f) * 2.0f;
            int x = (int)(chain_x + link_offset);
            
            // Add shake effect near break point
            if (fabs(y - break_point) < 10) {
                x += (int)(sinf(time * shake * 20 + y) * shake * 3);
            }
            
            if (x >= 0 && x < width) {
                char link_char = (y % 4 < 2) ? 'O' : '8';
                
                // Break the chain
                if (y > break_point - 2 && y < break_point + 2) {
                    if (break_progress > 0.5f) {
                        // Broken pieces
                        link_char = "~*#"[(int)(time * 10) % 3];
                        x += (int)((y - break_point) * 3);
                    }
                }
                
                set_pixel(buffer, zbuffer, width, height, x, y, link_char, 5.0f);
            }
        }
    }
    
    // Add liberation effects
    if (break_progress > 0.7f) {
        for (int i = 0; i < 50; i++) {
            float particle_time = time * 2 + i * 0.1f;
            int px = (int)(width / 2 + sinf(particle_time) * width * 0.4f);
            int py = (int)(height / 2 + cosf(particle_time) * height * 0.3f - particle_time * 5);
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                set_pixel(buffer, zbuffer, width, height, px, py, '*', 2.0f);
            }
        }
    }
}

// Scene 172: Crowd March - ASCII crowd marching forward
void scene_revolt_crowd_march(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float march_speed = params[0].value;  // 0.5-2.0
    float crowd_density = params[1].value; // 0.5-1.5
    float unity = params[2].value;        // 0.0-1.0
    
    int rows = 8;
    int people_per_row = (int)(width / 4 * crowd_density);
    
    for (int row = 0; row < rows; row++) {
        float row_offset = time * march_speed * (1.0f + row * 0.1f);
        float row_y = height - 10 - row * 5;
        float z = 20.0f - row * 2.0f;
        
        for (int person = 0; person < people_per_row; person++) {
            float person_phase = (float)person / people_per_row;
            float x_base = fmodf(person_phase * width + row_offset, width);
            
            // Walking animation
            float walk_phase = time * march_speed * 4 + person;
            int walk_frame = (int)walk_phase % 4;
            
            // Add unity effect - people march more in sync
            if (unity > 0.5f) {
                walk_frame = (int)(time * march_speed * 4) % 4;
            }
            
            // Draw person
            const char* frames[4] = {
                " o  \n/|\\ \n/ \\ ",
                " o  \n/|\\ \n | |",
                " o  \n/|\\ \n \\ /",
                " o  \n/|\\ \n | |"
            };
            
            int px = (int)x_base;
            int py = (int)row_y;
            
            // Draw each part
            if (px >= 0 && px < width - 3) {
                // Head
                set_pixel(buffer, zbuffer, width, height, px + 1, py, 'o', z);
                // Body
                set_pixel(buffer, zbuffer, width, height, px, py + 1, '/', z);
                set_pixel(buffer, zbuffer, width, height, px + 1, py + 1, '|', z);
                set_pixel(buffer, zbuffer, width, height, px + 2, py + 1, '\\', z);
                // Legs based on walk frame
                if (walk_frame == 0 || walk_frame == 2) {
                    set_pixel(buffer, zbuffer, width, height, px, py + 2, '/', z);
                    set_pixel(buffer, zbuffer, width, height, px + 2, py + 2, '\\', z);
                } else {
                    set_pixel(buffer, zbuffer, width, height, px + 1, py + 2, '|', z);
                    set_pixel(buffer, zbuffer, width, height, px + 1, py + 2, '|', z);
                }
            }
        }
    }
    
    // Add banners
    int banner_y = 5;
    draw_text(buffer, zbuffer, width, height, width/2 - 15, banner_y, "WE MARCH FOR CHANGE", 1.0f);
}

// Scene 173: Barricade Building - Constructing barriers
void scene_revolt_barricade_building(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float build_progress = params[0].value; // 0.0-1.0
    float chaos = params[1].value;          // 0.0-1.0
    float fortification = params[2].value;  // 0.0-1.0
    
    int barricade_height = (int)(height * 0.6f * build_progress);
    int barricade_y = height - barricade_height;
    
    // Draw barricade structure
    for (int y = barricade_y; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float noise = sinf(x * 0.3f + y * 0.5f + time) * chaos;
            
            // Different materials based on position
            char material = ' ';
            float material_choice = (x + y + (int)(noise * 10)) % 20;
            
            if (material_choice < 5) material = '#';
            else if (material_choice < 10) material = '=';
            else if (material_choice < 15) material = '-';
            else if (material_choice < 18) material = '|';
            else material = '+';
            
            // Add gaps unless fortified
            if (fortification < 0.8f && (x + y) % 7 == 0) {
                material = ' ';
            }
            
            if (material != ' ') {
                float z = 10.0f - (y - barricade_y) * 0.1f;
                set_pixel(buffer, zbuffer, width, height, x, y, material, z);
            }
        }
    }
    
    // Add people building
    int num_builders = 5 + (int)(build_progress * 10);
    for (int i = 0; i < num_builders; i++) {
        int builder_x = (int)(sinf(time * 0.5f + i) * width * 0.3f + width / 2);
        int builder_y = barricade_y - 4;
        
        if (builder_x >= 1 && builder_x < width - 1 && builder_y >= 0) {
            // Builder carrying materials
            set_pixel(buffer, zbuffer, width, height, builder_x, builder_y, 'o', 5.0f);
            set_pixel(buffer, zbuffer, width, height, builder_x - 1, builder_y + 1, '/', 5.0f);
            set_pixel(buffer, zbuffer, width, height, builder_x, builder_y + 1, '|', 5.0f);
            set_pixel(buffer, zbuffer, width, height, builder_x + 1, builder_y + 1, '\\', 5.0f);
            
            // Material in hands
            if ((int)(time * 2) % 2 == 0) {
                set_pixel(buffer, zbuffer, width, height, builder_x + 2, builder_y, '=', 4.0f);
            }
        }
    }
    
    // Add thrown objects if chaos is high
    if (chaos > 0.5f) {
        for (int i = 0; i < 10; i++) {
            float throw_time = time * 2 + i * 0.5f;
            int obj_x = (int)(i * width / 10 + throw_time * 20) % width;
            int obj_y = (int)(barricade_y - 10 - sinf(throw_time) * 10);
            
            if (obj_x >= 0 && obj_x < width && obj_y >= 0 && obj_y < height) {
                set_pixel(buffer, zbuffer, width, height, obj_x, obj_y, '*', 3.0f);
            }
        }
    }
}

// Scene 174: Molotov Cocktails - Flaming bottles in arc trajectories
void scene_revolt_molotov_cocktails(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float throw_rate = params[0].value;    // 0.5-2.0
    float fire_intensity = params[1].value; // 0.5-1.5
    float spread = params[2].value;        // 0.5-1.5
    
    // Multiple bottles in flight
    int num_bottles = 5;
    for (int i = 0; i < num_bottles; i++) {
        float bottle_time = time * throw_rate + i * 1.5f;
        float t = fmodf(bottle_time, 3.0f) / 3.0f; // 0-1 flight progress
        
        // Parabolic trajectory
        float start_x = width * 0.1f;
        float end_x = width * (0.6f + i * 0.08f * spread);
        float max_height = height * 0.3f;
        
        float x = start_x + (end_x - start_x) * t;
        float y = height - 5 - (4 * max_height * t * (1 - t));
        
        if (t < 0.9f) {
            // Bottle in flight
            if (x >= 0 && x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, (int)x, (int)y, 'o', 5.0f);
                
                // Flame trail
                for (int trail = 1; trail < 5; trail++) {
                    int trail_x = (int)(x - trail * 2);
                    int trail_y = (int)(y - trail);
                    if (trail_x >= 0 && trail_x < width && trail_y >= 0 && trail_y < height) {
                        char flame = "*#~"[trail / 2];
                        set_pixel(buffer, zbuffer, width, height, trail_x, trail_y, flame, 6.0f);
                    }
                }
            }
        } else {
            // Impact and fire spread
            int impact_x = (int)end_x;
            int impact_y = height - 5;
            
            float fire_time = (t - 0.9f) * 10.0f;
            int fire_radius = (int)(fire_time * fire_intensity * 10);
            
            for (int fy = -fire_radius; fy <= 0; fy++) {
                for (int fx = -fire_radius; fx <= fire_radius; fx++) {
                    int fire_x = impact_x + fx;
                    int fire_y = impact_y + fy;
                    
                    if (fire_x >= 0 && fire_x < width && fire_y >= 0 && fire_y < height) {
                        float dist = sqrtf(fx * fx + fy * fy);
                        if (dist <= fire_radius) {
                            char fire_char = (dist < fire_radius * 0.3f) ? '#' :
                                           (dist < fire_radius * 0.6f) ? '*' : '~';
                            set_pixel(buffer, zbuffer, width, height, fire_x, fire_y, fire_char, 4.0f);
                        }
                    }
                }
            }
        }
    }
    
    // Add throwers
    for (int i = 0; i < 3; i++) {
        int thrower_x = width * 0.1f - 5 + i * 3;
        int thrower_y = height - 8;
        
        if (thrower_x >= 0 && thrower_x < width - 2) {
            set_pixel(buffer, zbuffer, width, height, thrower_x, thrower_y, 'o', 10.0f);
            set_pixel(buffer, zbuffer, width, height, thrower_x, thrower_y + 1, '|', 10.0f);
            set_pixel(buffer, zbuffer, width, height, thrower_x - 1, thrower_y + 2, '/', 10.0f);
            set_pixel(buffer, zbuffer, width, height, thrower_x + 1, thrower_y + 2, '\\', 10.0f);
        }
    }
}

// Scene 175: Tear Gas - Smoke clouds and people covering faces
void scene_revolt_tear_gas(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float gas_density = params[0].value;   // 0.3-1.0
    float wind_speed = params[1].value;    // 0.0-2.0
    float dispersion = params[2].value;    // 0.5-1.5
    
    // Gas clouds
    for (int cloud = 0; cloud < 5; cloud++) {
        float cloud_x = width * (0.2f + cloud * 0.15f);
        float cloud_y = height * 0.7f - cloud * 5;
        float cloud_time = time + cloud * 0.3f;
        
        // Wind effect
        cloud_x += sinf(cloud_time * wind_speed) * 10;
        
        int radius = (int)(15 * dispersion + sinf(cloud_time) * 5);
        
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                float dist = sqrtf(x * x + y * y);
                if (dist <= radius) {
                    int px = (int)cloud_x + x;
                    int py = (int)cloud_y + y;
                    
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        float density = (1.0f - dist / radius) * gas_density;
                        if (density > 0.3f) {
                            char gas_char = (density > 0.7f) ? '@' :
                                          (density > 0.5f) ? 'o' : '.';
                            float z = 5.0f + dist * 0.1f;
                            
                            // Only draw if not already occupied by denser gas
                            if (zbuffer[py * width + px] > z) {
                                set_pixel(buffer, zbuffer, width, height, px, py, gas_char, z);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // People covering faces
    for (int person = 0; person < 8; person++) {
        int px = (int)(person * width / 8 + sinf(time * 0.5f + person) * 5);
        int py = height - 10 + (person % 2) * 3;
        
        if (px >= 2 && px < width - 2) {
            // Person with arm over face
            set_pixel(buffer, zbuffer, width, height, px, py, 'o', 2.0f);
            set_pixel(buffer, zbuffer, width, height, px - 1, py, '-', 2.0f); // Arm
            set_pixel(buffer, zbuffer, width, height, px, py + 1, '|', 2.0f);
            set_pixel(buffer, zbuffer, width, height, px - 1, py + 2, '/', 2.0f);
            set_pixel(buffer, zbuffer, width, height, px + 1, py + 2, '\\', 2.0f);
            
            // Running away motion
            if ((int)(time * 3) % 2 == 0) {
                set_pixel(buffer, zbuffer, width, height, px - 2, py + 1, '~', 3.0f);
            }
        }
    }
    
    // Gas canisters on ground
    for (int can = 0; can < 3; can++) {
        int can_x = width * (0.3f + can * 0.2f);
        int can_y = height - 3;
        
        set_pixel(buffer, zbuffer, width, height, can_x, can_y, '[', 1.0f);
        set_pixel(buffer, zbuffer, width, height, can_x + 1, can_y, '=', 1.0f);
        set_pixel(buffer, zbuffer, width, height, can_x + 2, can_y, ']', 1.0f);
        
        // Smoke coming out
        for (int smoke = 0; smoke < 5; smoke++) {
            int sx = can_x + 1 + (int)(sinf(time * 3 + smoke) * 2);
            int sy = can_y - smoke - 1;
            if (sx >= 0 && sx < width && sy >= 0) {
                set_pixel(buffer, zbuffer, width, height, sx, sy, '~', 1.5f);
            }
        }
    }
}

// Scene 176: Graffiti Wall - Revolutionary messages being spray painted
void scene_revolt_graffiti_wall(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float write_speed = params[0].value;   // 0.5-2.0
    float layer_count = params[1].value;   // 1-3
    float vandalism = params[2].value;     // 0.0-1.0
    
    // Draw brick wall
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (y % 3 == 0) {
                buffer[y * width + x] = '-';
            } else if ((x + (y / 3) * 4) % 8 == 0) {
                buffer[y * width + x] = '|';
            }
            zbuffer[y * width + x] = 20.0f;
        }
    }
    
    // Graffiti messages
    const char* messages[] = {
        "REVOLT NOW",
        "FREEDOM",
        "NO MASTERS",
        "RESIST",
        "POWER TO THE PEOPLE",
        "BREAK THE SYSTEM",
        "RISE UP",
        "ANARCHY"
    };
    
    int num_layers = (int)(layer_count * 3);
    
    for (int layer = 0; layer < num_layers; layer++) {
        float layer_time = time * write_speed - layer * 2.0f;
        if (layer_time < 0) continue;
        
        int msg_idx = (layer + (int)(time * 0.1f)) % 8;
        const char* msg = messages[msg_idx];
        int msg_len = strlen(msg);
        
        int write_progress = (int)(fmodf(layer_time, msg_len + 3) * 2);
        if (write_progress > msg_len) write_progress = msg_len;
        
        int msg_x = 5 + (layer * 15) % (width - 20);
        int msg_y = 5 + (layer * 7) % (height - 10);
        
        // Draw message with spray paint effect
        for (int i = 0; i < write_progress && i < msg_len; i++) {
            for (int spray_y = -1; spray_y <= 1; spray_y++) {
                for (int spray_x = -1; spray_x <= 1; spray_x++) {
                    int px = msg_x + i * 2 + spray_x;
                    int py = msg_y + spray_y;
                    
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        if (spray_x == 0 && spray_y == 0) {
                            set_pixel(buffer, zbuffer, width, height, px, py, msg[i], 5.0f - layer);
                        } else if (vandalism > 0.5f) {
                            // Spray overspray
                            set_pixel(buffer, zbuffer, width, height, px, py, '.', 6.0f - layer);
                        }
                    }
                }
            }
        }
        
        // Add tagger if currently writing
        if (write_progress < msg_len && write_progress > 0) {
            int tagger_x = msg_x + write_progress * 2 + 3;
            int tagger_y = msg_y;
            
            if (tagger_x < width - 3 && tagger_y < height - 3) {
                // Person with spray can
                set_pixel(buffer, zbuffer, width, height, tagger_x, tagger_y, 'o', 2.0f);
                set_pixel(buffer, zbuffer, width, height, tagger_x, tagger_y + 1, '|', 2.0f);
                set_pixel(buffer, zbuffer, width, height, tagger_x - 1, tagger_y + 1, '/', 2.0f);
                set_pixel(buffer, zbuffer, width, height, tagger_x - 2, tagger_y, 'D', 2.0f); // Spray can
                
                // Spray effect
                for (int s = 0; s < 3; s++) {
                    int spray_x = tagger_x - 3 - s;
                    int spray_y = tagger_y + (int)(sinf(time * 10 + s) * 0.5f);
                    if (spray_x >= 0 && spray_y >= 0 && spray_y < height) {
                        set_pixel(buffer, zbuffer, width, height, spray_x, spray_y, '~', 1.0f);
                    }
                }
            }
        }
    }
}

// Scene 177: Police Line Breaking - Protesters pushing through barriers
void scene_revolt_police_line_breaking(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float push_force = params[0].value;    // 0.0-1.0
    float line_stability = params[1].value; // 1.0-0.0 (inverse)
    float chaos_level = params[2].value;   // 0.0-1.0
    
    // Police line position (moves back with push force)
    int line_x = width / 2 + (int)(push_force * width * 0.3f);
    
    // Draw police shields
    int num_shields = height / 4;
    for (int i = 0; i < num_shields; i++) {
        int shield_y = i * 4 + 2;
        int shield_x = line_x + (int)(sinf(time * 5 + i) * (1.0f - line_stability) * 5);
        
        if (shield_x >= 0 && shield_x < width - 3 && shield_y < height - 2) {
            // Shield
            set_pixel(buffer, zbuffer, width, height, shield_x, shield_y, '[', 5.0f);
            set_pixel(buffer, zbuffer, width, height, shield_x + 1, shield_y, '=', 5.0f);
            set_pixel(buffer, zbuffer, width, height, shield_x + 2, shield_y, ']', 5.0f);
            set_pixel(buffer, zbuffer, width, height, shield_x, shield_y + 1, '[', 5.0f);
            set_pixel(buffer, zbuffer, width, height, shield_x + 1, shield_y + 1, '=', 5.0f);
            set_pixel(buffer, zbuffer, width, height, shield_x + 2, shield_y + 1, ']', 5.0f);
            
            // Officer behind shield
            if (shield_x + 3 < width) {
                set_pixel(buffer, zbuffer, width, height, shield_x + 3, shield_y, 'O', 6.0f);
            }
        }
    }
    
    // Protesters pushing
    int num_protesters = 15 + (int)(chaos_level * 20);
    for (int i = 0; i < num_protesters; i++) {
        float protester_phase = (float)i / num_protesters;
        int base_x = (int)(protester_phase * width * 0.4f);
        int protester_x = base_x + (int)(sinf(time * 2 + i) * 5);
        int protester_y = 5 + (i % (height - 10));
        
        // Move forward based on push force
        protester_x += (int)(push_force * 10);
        
        if (protester_x >= 0 && protester_x < line_x - 2) {
            // Protester pushing
            set_pixel(buffer, zbuffer, width, height, protester_x, protester_y, 'o', 3.0f);
            set_pixel(buffer, zbuffer, width, height, protester_x, protester_y + 1, '>', 3.0f);
            set_pixel(buffer, zbuffer, width, height, protester_x - 1, protester_y + 2, '/', 3.0f);
            set_pixel(buffer, zbuffer, width, height, protester_x + 1, protester_y + 2, '\\', 3.0f);
        }
    }
    
    // Conflict zone effects
    if (chaos_level > 0.5f) {
        for (int i = 0; i < 20; i++) {
            int conflict_x = line_x - 5 + (int)(sinf(time * 10 + i) * 10);
            int conflict_y = 5 + (i * 2) % (height - 10);
            
            if (conflict_x >= 0 && conflict_x < width && conflict_y >= 0 && conflict_y < height) {
                char conflict_char = "!*#@"[i % 4];
                set_pixel(buffer, zbuffer, width, height, conflict_x, conflict_y, conflict_char, 1.0f);
            }
        }
    }
    
    // Broken barriers on ground
    if (push_force > 0.6f) {
        for (int debris = 0; debris < 10; debris++) {
            int debris_x = line_x + (debris - 5) * 3;
            int debris_y = height - 2 - (debris % 3);
            
            if (debris_x >= 0 && debris_x < width && debris_y >= 0) {
                char debris_char = "=-~"[debris % 3];
                set_pixel(buffer, zbuffer, width, height, debris_x, debris_y, debris_char, 8.0f);
            }
        }
    }
}

// Scene 178: Flag Burning - Symbolic burning of oppressive flags
void scene_revolt_flag_burning(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float burn_progress = params[0].value;  // 0.0-1.0
    float flame_height = params[1].value;   // 0.5-2.0
    float smoke_density = params[2].value;  // 0.5-1.5
    
    int flag_width = 30;
    int flag_height = 15;
    int flag_x = width / 2 - flag_width / 2;
    int flag_y = height / 2 - flag_height / 2;
    
    // Draw flag (what's left of it)
    for (int y = 0; y < flag_height; y++) {
        for (int x = 0; x < flag_width; x++) {
            int px = flag_x + x;
            int py = flag_y + y;
            
            // Burn from bottom up
            float burn_line = flag_height * (1.0f - burn_progress);
            
            if (y < burn_line && px >= 0 && px < width && py >= 0 && py < height) {
                // Flag pattern (generic stripes)
                char flag_char = (y % 3 == 0) ? '=' : '-';
                set_pixel(buffer, zbuffer, width, height, px, py, flag_char, 10.0f);
            }
            
            // Burning edge
            if (fabs(y - burn_line) < 2 && px >= 0 && px < width && py >= 0 && py < height) {
                char burn_char = "*#@"[(int)(time * 10 + x) % 3];
                set_pixel(buffer, zbuffer, width, height, px, py, burn_char, 5.0f);
            }
        }
    }
    
    // Flames below flag
    int flame_base_y = flag_y + flag_height;
    for (int x = -10; x < flag_width + 10; x++) {
        for (int y = 0; y < flame_height * 10; y++) {
            int fx = flag_x + x;
            int fy = flame_base_y + y;
            
            float flame_shape = sinf((x + flag_width/2) * 0.2f) * 5 + sinf(time * 5 + x * 0.1f) * 3;
            float flame_top = flame_height * 10 - flame_shape;
            
            if (y < flame_top && fx >= 0 && fx < width && fy >= 0 && fy < height) {
                float intensity = 1.0f - y / flame_top;
                char flame_char = (intensity > 0.7f) ? '#' :
                                (intensity > 0.4f) ? '*' : '~';
                set_pixel(buffer, zbuffer, width, height, fx, fy, flame_char, 3.0f);
            }
        }
    }
    
    // Smoke rising
    for (int smoke = 0; smoke < smoke_density * 30; smoke++) {
        float smoke_time = time * 0.5f + smoke * 0.1f;
        int sx = flag_x + flag_width / 2 + (int)(sinf(smoke_time + smoke) * 20);
        int sy = flag_y - (int)(smoke_time * 10) % (flag_y + 10);
        
        if (sx >= 0 && sx < width && sy >= 0) {
            char smoke_char = "..oo"[smoke % 4];
            set_pixel(buffer, zbuffer, width, height, sx, sy, smoke_char, 15.0f);
        }
    }
    
    // People watching
    for (int person = 0; person < 5; person++) {
        int px = width / 4 + person * width / 8;
        int py = height - 8;
        
        if (px >= 0 && px < width - 2 && py >= 0) {
            // Simple person outline
            set_pixel(buffer, zbuffer, width, height, px, py, 'o', 2.0f);
            set_pixel(buffer, zbuffer, width, height, px, py + 1, '|', 2.0f);
            set_pixel(buffer, zbuffer, width, height, px - 1, py + 2, '/', 2.0f);
            set_pixel(buffer, zbuffer, width, height, px + 1, py + 2, '\\', 2.0f);
            
            // Raised fist
            if (person % 2 == 0) {
                set_pixel(buffer, zbuffer, width, height, px + 2, py - 1, '*', 2.0f);
            }
        }
    }
}

// Scene 179: Victory Dance - Celebration of successful revolt
void scene_revolt_victory_dance(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float celebration = params[0].value;    // 0.5-1.0
    float dance_speed = params[1].value;    // 0.5-2.0
    float fireworks = params[2].value;      // 0.0-1.0
    
    // Dancing crowd
    int num_dancers = (int)(20 * celebration);
    for (int i = 0; i < num_dancers; i++) {
        float dancer_phase = (float)i / num_dancers * M_PI * 2;
        float dance_time = time * dance_speed + dancer_phase;
        
        int base_x = width / 2 + (int)(cosf(dancer_phase) * width * 0.3f);
        int base_y = height - 15 + (int)(sinf(dancer_phase) * 5);
        
        // Dance movement
        int dx = (int)(sinf(dance_time * 2) * 3);
        int dy = (int)(fabs(sinf(dance_time * 4)) * -3);
        
        int dancer_x = base_x + dx;
        int dancer_y = base_y + dy;
        
        if (dancer_x >= 1 && dancer_x < width - 1 && dancer_y >= 0 && dancer_y < height - 3) {
            // Dancer body
            set_pixel(buffer, zbuffer, width, height, dancer_x, dancer_y, 'o', 5.0f);
            
            // Arms up in celebration
            if ((int)(dance_time * 2) % 2 == 0) {
                set_pixel(buffer, zbuffer, width, height, dancer_x - 1, dancer_y - 1, '\\', 5.0f);
                set_pixel(buffer, zbuffer, width, height, dancer_x + 1, dancer_y - 1, '/', 5.0f);
            } else {
                set_pixel(buffer, zbuffer, width, height, dancer_x - 1, dancer_y, '-', 5.0f);
                set_pixel(buffer, zbuffer, width, height, dancer_x + 1, dancer_y, '-', 5.0f);
            }
            
            // Body
            set_pixel(buffer, zbuffer, width, height, dancer_x, dancer_y + 1, '|', 5.0f);
            
            // Dancing legs
            if ((int)(dance_time * 4) % 2 == 0) {
                set_pixel(buffer, zbuffer, width, height, dancer_x - 1, dancer_y + 2, '/', 5.0f);
                set_pixel(buffer, zbuffer, width, height, dancer_x + 1, dancer_y + 2, '\\', 5.0f);
            } else {
                set_pixel(buffer, zbuffer, width, height, dancer_x, dancer_y + 2, '|', 5.0f);
                set_pixel(buffer, zbuffer, width, height, dancer_x, dancer_y + 3, '|', 5.0f);
            }
        }
    }
    
    // Fireworks
    if (fireworks > 0.3f) {
        for (int fw = 0; fw < 5; fw++) {
            float fw_time = time * 0.7f + fw * 0.8f;
            float explosion_time = fmodf(fw_time, 3.0f);
            
            if (explosion_time < 2.0f) {
                // Rising
                int fw_x = width / 4 + fw * width / 6;
                int fw_y = height - (int)(explosion_time * height * 0.5f);
                
                if (fw_y >= 0) {
                    set_pixel(buffer, zbuffer, width, height, fw_x, fw_y, '|', 2.0f);
                    set_pixel(buffer, zbuffer, width, height, fw_x, fw_y + 1, '*', 2.0f);
                }
            } else {
                // Explosion
                int center_x = width / 4 + fw * width / 6;
                int center_y = height / 2;
                float burst = (explosion_time - 2.0f) * 20;
                
                for (int angle = 0; angle < 16; angle++) {
                    float a = angle * M_PI / 8;
                    int bx = center_x + (int)(cosf(a) * burst);
                    int by = center_y + (int)(sinf(a) * burst);
                    
                    if (bx >= 0 && bx < width && by >= 0 && by < height) {
                        char burst_char = "*+."[(int)burst % 3];
                        set_pixel(buffer, zbuffer, width, height, bx, by, burst_char, 1.0f);
                    }
                }
            }
        }
    }
    
    // Victory banner
    const char* victory_text = "FREEDOM ACHIEVED!";
    int text_x = width / 2 - strlen(victory_text) / 2;
    int text_y = 5 + (int)(sinf(time * 2) * 2);
    
    draw_text(buffer, zbuffer, width, height, text_x, text_y, victory_text, 1.0f);
    
    // Confetti effect
    for (int conf = 0; conf < celebration * 50; conf++) {
        int cx = (int)(conf * 7 + time * 30) % width;
        int cy = (int)(conf * 3 + time * 20) % height;
        char confetti = ".*+o"[conf % 4];
        
        if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
            set_pixel(buffer, zbuffer, width, height, cx, cy, confetti, 20.0f);
        }
    }
}

// ============= AUDIO REACTIVE SCENES (180-189) =============

// Scene 180: Audio Reactive 3D Cubes - Multiple cubes react to different frequency bands
void scene_audio_reactive_cubes(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float cube_scale = params[0].value * 15.0f + 5.0f;       // 5-20
    float rotation_speed = params[1].value * 2.0f + 0.5f;    // 0.5-2.5
    float audio_sensitivity = params[2].value * 2.0f + 0.5f; // 0.5-2.5
    
    // Default values when no audio
    float bass = 0.5f, mid = 0.5f, treble = 0.5f, volume = 0.5f;
    
    if (audio && audio->valid) {
        bass = audio->bass * audio_sensitivity;
        mid = audio->mid * audio_sensitivity;
        treble = audio->treble * audio_sensitivity;
        volume = audio->volume;
    } else {
        // Simulate audio with sine waves when no input
        bass = (sinf(time * 1.5f) + 1.0f) * 0.5f;
        mid = (sinf(time * 2.3f + M_PI/3) + 1.0f) * 0.5f;
        treble = (sinf(time * 3.7f + M_PI/2) + 1.0f) * 0.5f;
    }
    
    // Three cubes for bass, mid, treble
    float positions[][3] = {
        {-width/3.0f, 0, 0},   // Bass cube (left)
        {0, 0, 0},             // Mid cube (center)
        {width/3.0f, 0, 0}     // Treble cube (right)
    };
    
    float sizes[] = {bass, mid, treble};
    char chars[] = {'#', '*', '+'};
    
    for (int cube = 0; cube < 3; cube++) {
        float size = cube_scale * (0.5f + sizes[cube]);
        float cx = positions[cube][0];
        float angle = time * rotation_speed + cube * M_PI * 2 / 3;
        
        // Cube vertices
        float vertices[][3] = {
            {-size, -size, -size}, {size, -size, -size},
            {size, size, -size}, {-size, size, -size},
            {-size, -size, size}, {size, -size, size},
            {size, size, size}, {-size, size, size}
        };
        
        // Rotate vertices
        for (int i = 0; i < 8; i++) {
            float x = vertices[i][0];
            float y = vertices[i][1];
            float z = vertices[i][2];
            
            // Rotate Y
            float rx = x * cosf(angle) - z * sinf(angle);
            float rz = x * sinf(angle) + z * cosf(angle);
            
            // Rotate X with audio influence
            float ax = angle * 0.5f + sizes[cube] * M_PI;
            float ry = y * cosf(ax) - rz * sinf(ax);
            float frz = y * sinf(ax) + rz * cosf(ax);
            
            vertices[i][0] = rx + cx;
            vertices[i][1] = ry;
            vertices[i][2] = frz;
        }
        
        // Draw cube edges
        int edges[][2] = {
            {0,1}, {1,2}, {2,3}, {3,0},  // Back face
            {4,5}, {5,6}, {6,7}, {7,4},  // Front face
            {0,4}, {1,5}, {2,6}, {3,7}   // Connecting edges
        };
        
        for (int i = 0; i < 12; i++) {
            int v1 = edges[i][0];
            int v2 = edges[i][1];
            
            // Project to 2D
            float z1 = vertices[v1][2] + 50;
            float z2 = vertices[v2][2] + 50;
            
            int x1 = width/2 + (int)(vertices[v1][0] * 40 / z1);
            int y1 = height/2 + (int)(vertices[v1][1] * 20 / z1);
            int x2 = width/2 + (int)(vertices[v2][0] * 40 / z2);
            int y2 = height/2 + (int)(vertices[v2][1] * 20 / z2);
            
            // Draw line between vertices
            int steps = fmaxf(abs(x2 - x1), abs(y2 - y1));
            if (steps > 0) {
                for (int s = 0; s <= steps; s++) {
                    int x = x1 + (x2 - x1) * s / steps;
                    int y = y1 + (y2 - y1) * s / steps;
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        set_pixel(buffer, zbuffer, width, height, x, y, chars[cube], (z1 + z2) / 2);
                    }
                }
            }
        }
        
        // Draw cube faces with audio-reactive fill
        if (sizes[cube] > 0.6f) {
            // Fill some faces when audio is strong
            int face_y = height/2 + (int)(positions[cube][1]);
            for (int dy = -cube_scale/4; dy < cube_scale/4; dy++) {
                for (int dx = -cube_scale/4; dx < cube_scale/4; dx++) {
                    int px = width/2 + cx + dx;
                    int py = face_y + dy;
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        if (buffer[py * width + px] == ' ') {
                            set_pixel(buffer, zbuffer, width, height, px, py, '.', 10.0f);
                        }
                    }
                }
            }
        }
    }
    
    // Beat flash effect
    if (audio && audio->valid && audio->beat_detected) {
        for (int i = 0; i < 20; i++) {
            int fx = rand() % width;
            int fy = rand() % height;
            set_pixel(buffer, zbuffer, width, height, fx, fy, '*', 100.0f);
        }
    }
}

// Scene 181: Audio Flash Strobes - Intense flashing patterns synced to beats
void scene_audio_flash_strobes(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float strobe_speed = params[0].value * 10.0f + 2.0f;     // 2-12 Hz
    float pattern_type = params[1].value * 5.0f;             // 0-5 patterns
    float audio_trigger = params[2].value;                   // 0-1 audio sensitivity
    
    bool flash_on = false;
    float intensity = 1.0f;
    
    if (audio && audio->valid) {
        // Audio-triggered flashes
        if (audio->beat_detected) {
            flash_on = true;
            intensity = audio->beat_intensity;
        } else if (audio->volume > audio_trigger) {
            flash_on = (int)(time * strobe_speed) % 2 == 0;
            intensity = audio->volume;
        }
    } else {
        // Regular strobe without audio
        flash_on = (int)(time * strobe_speed) % 2 == 0;
        intensity = (sinf(time * 3.0f) + 1.0f) * 0.5f;
    }
    
    if (flash_on) {
        int pattern = (int)pattern_type % 6;
        
        switch (pattern) {
            case 0: // Full screen flash
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        char c = (intensity > 0.8f) ? '#' : (intensity > 0.5f) ? '*' : '+';
                        set_pixel(buffer, zbuffer, width, height, x, y, c, 50.0f);
                    }
                }
                break;
                
            case 1: // Horizontal bars
                for (int y = 0; y < height; y++) {
                    if (y % 4 < 2) {
                        for (int x = 0; x < width; x++) {
                            set_pixel(buffer, zbuffer, width, height, x, y, '#', 50.0f);
                        }
                    }
                }
                break;
                
            case 2: // Vertical bars
                for (int x = 0; x < width; x++) {
                    if (x % 8 < 4) {
                        for (int y = 0; y < height; y++) {
                            set_pixel(buffer, zbuffer, width, height, x, y, '#', 50.0f);
                        }
                    }
                }
                break;
                
            case 3: // Checkerboard
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        if ((x/4 + y/2) % 2 == 0) {
                            set_pixel(buffer, zbuffer, width, height, x, y, '#', 50.0f);
                        }
                    }
                }
                break;
                
            case 4: // Radial burst
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        float dx = x - width/2.0f;
                        float dy = (y - height/2.0f) * 2;
                        float dist = sqrtf(dx*dx + dy*dy);
                        
                        if ((int)(dist / 5) % 2 == 0) {
                            set_pixel(buffer, zbuffer, width, height, x, y, '*', 50.0f);
                        }
                    }
                }
                break;
                
            case 5: // Random pixels
                int num_pixels = (int)(width * height * intensity * 0.5f);
                for (int i = 0; i < num_pixels; i++) {
                    int x = rand() % width;
                    int y = rand() % height;
                    set_pixel(buffer, zbuffer, width, height, x, y, '#', 50.0f);
                }
                break;
        }
    }
    
    // Add subtle animation even when not flashing
    if (!flash_on && intensity > 0.3f) {
        for (int i = 0; i < 10; i++) {
            int x = width/2 + (int)(cosf(time * 2 + i) * width * 0.3f);
            int y = height/2 + (int)(sinf(time * 2 + i) * height * 0.3f);
            set_pixel(buffer, zbuffer, width, height, x, y, '.', 20.0f);
        }
    }
}

// Scene 182: Audio Explosions - Particle explosions triggered by beats
void scene_audio_explosions(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float explosion_size = params[0].value * 20.0f + 10.0f;    // 10-30
    float particle_count = params[1].value * 100.0f + 50.0f;   // 50-150
    float gravity = params[2].value * 2.0f;                    // 0-2
    
    static float last_beat_time = 0;
    static float explosions[10][5]; // x, y, time, intensity, type
    static int explosion_count = 0;
    
    // Check for new explosions
    if (audio && audio->valid && audio->beat_detected) {
        if (time - last_beat_time > 0.1f) { // Debounce beats
            // Add new explosion
            explosions[explosion_count % 10][0] = width * 0.2f + (rand() % (int)(width * 0.6f));
            explosions[explosion_count % 10][1] = height * 0.2f + (rand() % (int)(height * 0.6f));
            explosions[explosion_count % 10][2] = time;
            explosions[explosion_count % 10][3] = audio->beat_intensity;
            explosions[explosion_count % 10][4] = rand() % 3; // explosion type
            explosion_count++;
            last_beat_time = time;
        }
    } else if (!audio || !audio->valid) {
        // Simulate explosions without audio
        if ((int)(time * 2) % 2 == 0 && time - last_beat_time > 0.5f) {
            explosions[explosion_count % 10][0] = width * 0.2f + (rand() % (int)(width * 0.6f));
            explosions[explosion_count % 10][1] = height * 0.2f + (rand() % (int)(height * 0.6f));
            explosions[explosion_count % 10][2] = time;
            explosions[explosion_count % 10][3] = 0.8f;
            explosions[explosion_count % 10][4] = rand() % 3;
            explosion_count++;
            last_beat_time = time;
        }
    }
    
    // Render active explosions
    for (int e = 0; e < 10; e++) {
        if (explosions[e][2] > 0) {
            float age = time - explosions[e][2];
            if (age < 2.0f) {
                float ex = explosions[e][0];
                float ey = explosions[e][1];
                float intensity = explosions[e][3];
                int type = (int)explosions[e][4];
                
                int particles = (int)(particle_count * intensity);
                
                for (int p = 0; p < particles; p++) {
                    float angle = (p / (float)particles) * M_PI * 2;
                    float speed = explosion_size * (0.5f + (p % 3) * 0.3f);
                    
                    // Particle position with gravity
                    float px = ex + cosf(angle) * speed * age;
                    float py = ey + sinf(angle) * speed * age * 0.5f + gravity * age * age * 5;
                    
                    int x = (int)px;
                    int y = (int)py;
                    
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        char c;
                        if (age < 0.2f) c = '#';
                        else if (age < 0.5f) c = '*';
                        else if (age < 1.0f) c = '+';
                        else c = '.';
                        
                        // Different explosion types
                        switch (type) {
                            case 0: // Standard
                                set_pixel(buffer, zbuffer, width, height, x, y, c, 50.0f - age * 20);
                                break;
                            case 1: // Ring
                                if (age > 0.3f && age < 1.0f) {
                                    set_pixel(buffer, zbuffer, width, height, x, y, 'o', 50.0f - age * 20);
                                }
                                break;
                            case 2: // Sparkles
                                if (p % 3 == 0) {
                                    set_pixel(buffer, zbuffer, width, height, x, y, '*', 50.0f - age * 20);
                                }
                                break;
                        }
                    }
                }
                
                // Central flash
                if (age < 0.1f) {
                    for (int dy = -3; dy <= 3; dy++) {
                        for (int dx = -5; dx <= 5; dx++) {
                            int x = (int)ex + dx;
                            int y = (int)ey + dy;
                            if (x >= 0 && x < width && y >= 0 && y < height) {
                                set_pixel(buffer, zbuffer, width, height, x, y, '#', 100.0f);
                            }
                        }
                    }
                }
            }
        }
    }
}

// Scene 183: Audio Wave Tunnel - 3D tunnel that pulses with audio
void scene_audio_wave_tunnel(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float tunnel_speed = params[0].value * 3.0f + 0.5f;      // 0.5-3.5
    float wave_amplitude = params[1].value * 10.0f + 2.0f;   // 2-12
    float rotation_speed = params[2].value * 2.0f;           // 0-2
    
    float audio_mod = 1.0f;
    
    if (audio && audio->valid) {
        audio_mod = 0.5f + audio->volume * 1.5f;
    } else {
        // Simulate audio modulation
        audio_mod = 1.0f + sinf(time * 3.0f) * 0.3f;
    }
    
    // Draw tunnel rings
    for (float z = 2; z < 50; z += 2) {
        float ring_time = time * tunnel_speed - z * 0.1f;
        float radius = (10 + z * 0.5f) * audio_mod;
        
        // Audio reactive deformation
        float deform = 0;
        if (audio && audio->valid) {
            deform = audio->bass * wave_amplitude * sinf(z * 0.2f + time * 2);
        }
        
        int segments = 32;
        for (int i = 0; i < segments; i++) {
            float angle = (i / (float)segments) * M_PI * 2 + time * rotation_speed;
            
            // Apply wave deformation
            float wave = sinf(angle * 3 + ring_time) * wave_amplitude * 0.1f;
            float r = radius + wave + deform;
            
            // 3D to 2D projection
            float x3d = cosf(angle) * r;
            float y3d = sinf(angle) * r;
            
            int x = width/2 + (int)(x3d * 40 / z);
            int y = height/2 + (int)(y3d * 20 / z);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char c;
                if (z < 10) c = '#';
                else if (z < 20) c = '*';
                else if (z < 30) c = '+';
                else c = '.';
                
                set_pixel(buffer, zbuffer, width, height, x, y, c, z);
            }
        }
    }
    
    // Add beat reactive flashes
    if (audio && audio->valid && audio->beat_detected) {
        for (int i = 0; i < 10; i++) {
            float angle = (i / 10.0f) * M_PI * 2;
            int x = width/2 + (int)(cosf(angle) * 20);
            int y = height/2 + (int)(sinf(angle) * 10);
            set_pixel(buffer, zbuffer, width, height, x, y, '*', 1.0f);
        }
    }
}

// Scene 184: Audio Spectrum 3D - 3D visualization of frequency spectrum
void scene_audio_spectrum_3d(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float bar_height_scale = params[0].value * 20.0f + 5.0f;  // 5-25
    float perspective = params[1].value * 30.0f + 20.0f;      // 20-50
    float rotation = params[2].value * time * 2.0f;           // rotation angle
    
    int num_bands = 32; // Use half of the 64 bands for cleaner display
    
    for (int band = 0; band < num_bands; band++) {
        float value = 0.3f; // Default value
        
        if (audio && audio->valid && band < 32) {
            // Average two adjacent bands for smoother display
            value = (audio->spectrum[band * 2] + audio->spectrum[band * 2 + 1]) * 0.5f;
        } else {
            // Simulate spectrum
            value = (sinf(time * (band + 1) * 0.5f) + 1.0f) * 0.25f + 0.25f;
        }
        
        float bar_height = value * bar_height_scale;
        
        // Position in 3D space
        float x3d = (band - num_bands/2.0f) * 3.0f;
        float z3d = 10.0f;
        
        // Apply rotation
        float rx = x3d * cosf(rotation) - z3d * sinf(rotation);
        float rz = x3d * sinf(rotation) + z3d * cosf(rotation) + 20;
        
        // Draw vertical bar
        for (int h = 0; h < bar_height; h++) {
            float y3d = -h + 5;
            
            // Project to 2D
            int x = width/2 + (int)(rx * perspective / rz);
            int y = height/2 + (int)(y3d * perspective / rz);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char c;
                if (value > 0.8f) c = '#';
                else if (value > 0.6f) c = '=';
                else if (value > 0.4f) c = '-';
                else c = '.';
                
                set_pixel(buffer, zbuffer, width, height, x, y, c, rz);
            }
        }
        
        // Draw bar top
        if (bar_height > 0) {
            int x = width/2 + (int)(rx * perspective / rz);
            int y = height/2 + (int)((-bar_height + 5) * perspective / rz);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                set_pixel(buffer, zbuffer, width, height, x, y, '*', rz - 0.1f);
            }
        }
    }
    
    // Draw frequency labels
    const char* labels[] = {"BASS", "MID", "TREBLE"};
    int label_positions[] = {width/4, width/2, 3*width/4};
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; labels[i][j]; j++) {
            set_pixel(buffer, zbuffer, width, height, 
                     label_positions[i] - 2 + j, height - 2, 
                     labels[i][j], 0.1f);
        }
    }
}

// Scene 185: Audio Reactive Particles - Particles that dance to the music
void scene_audio_reactive_particles(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float particle_count = params[0].value * 200.0f + 50.0f;   // 50-250
    float movement_speed = params[1].value * 5.0f + 1.0f;      // 1-6
    float audio_influence = params[2].value * 3.0f + 0.5f;     // 0.5-3.5
    
    static float particle_x[250];
    static float particle_y[250];
    static float particle_vx[250];
    static float particle_vy[250];
    static int initialized = 0;
    
    int count = (int)particle_count;
    
    // Initialize particles
    if (!initialized) {
        for (int i = 0; i < 250; i++) {
            particle_x[i] = rand() % width;
            particle_y[i] = rand() % height;
            particle_vx[i] = (rand() % 100 - 50) / 50.0f;
            particle_vy[i] = (rand() % 100 - 50) / 50.0f;
        }
        initialized = 1;
    }
    
    float bass_force = 0, mid_force = 0, treble_force = 0;
    
    if (audio && audio->valid) {
        bass_force = audio->bass * audio_influence;
        mid_force = audio->mid * audio_influence;
        treble_force = audio->treble * audio_influence;
    } else {
        // Simulate audio forces
        bass_force = (sinf(time * 1.5f) + 1.0f) * 0.5f;
        mid_force = (sinf(time * 2.3f) + 1.0f) * 0.5f;
        treble_force = (sinf(time * 3.7f) + 1.0f) * 0.5f;
    }
    
    // Update and draw particles
    for (int i = 0; i < count; i++) {
        // Audio-influenced movement
        float angle = atan2f(particle_y[i] - height/2.0f, particle_x[i] - width/2.0f);
        
        // Different frequency bands affect particles differently
        if (i % 3 == 0) {
            // Bass particles - move radially
            particle_vx[i] += cosf(angle) * bass_force * 0.5f;
            particle_vy[i] += sinf(angle) * bass_force * 0.5f;
        } else if (i % 3 == 1) {
            // Mid particles - circular motion
            particle_vx[i] += -sinf(angle) * mid_force * 0.3f;
            particle_vy[i] += cosf(angle) * mid_force * 0.3f;
        } else {
            // Treble particles - random jitter
            particle_vx[i] += (rand() % 100 - 50) / 50.0f * treble_force;
            particle_vy[i] += (rand() % 100 - 50) / 50.0f * treble_force;
        }
        
        // Apply velocity with damping
        particle_x[i] += particle_vx[i] * movement_speed * 0.1f;
        particle_y[i] += particle_vy[i] * movement_speed * 0.1f;
        particle_vx[i] *= 0.95f;
        particle_vy[i] *= 0.95f;
        
        // Wrap around screen
        if (particle_x[i] < 0) particle_x[i] = width - 1;
        if (particle_x[i] >= width) particle_x[i] = 0;
        if (particle_y[i] < 0) particle_y[i] = height - 1;
        if (particle_y[i] >= height) particle_y[i] = 0;
        
        // Draw particle
        int x = (int)particle_x[i];
        int y = (int)particle_y[i];
        
        char c;
        if (i % 3 == 0) c = '#';  // Bass
        else if (i % 3 == 1) c = '*';  // Mid
        else c = '.';  // Treble
        
        set_pixel(buffer, zbuffer, width, height, x, y, c, 10.0f);
        
        // Trail effect for moving particles
        float speed = sqrtf(particle_vx[i]*particle_vx[i] + particle_vy[i]*particle_vy[i]);
        if (speed > 2.0f) {
            int tx = x - (int)(particle_vx[i] * 0.5f);
            int ty = y - (int)(particle_vy[i] * 0.5f);
            if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
                set_pixel(buffer, zbuffer, width, height, tx, ty, '.', 15.0f);
            }
        }
    }
}

// Scene 186: Audio Pulse Rings - Concentric rings that pulse with audio
void scene_audio_pulse_rings(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float ring_count = params[0].value * 10.0f + 3.0f;        // 3-13 rings
    float pulse_speed = params[1].value * 3.0f + 0.5f;        // 0.5-3.5
    float audio_scale = params[2].value * 2.0f + 0.5f;        // 0.5-2.5
    
    float audio_mod = 1.0f;
    float beat_expand = 0.0f;
    
    if (audio && audio->valid) {
        audio_mod = 0.7f + audio->volume * audio_scale;
        if (audio->beat_detected) {
            beat_expand = audio->beat_intensity * 10.0f;
        }
    } else {
        // Simulate audio
        audio_mod = 1.0f + sinf(time * 2.0f) * 0.3f;
        if ((int)(time * 4) % 4 == 0) {
            beat_expand = 5.0f * (1.0f - fmodf(time * 4, 1.0f));
        }
    }
    
    int rings = (int)ring_count;
    
    for (int ring = 0; ring < rings; ring++) {
        float base_radius = (ring + 1) * (fminf(width, height) / (rings * 2.0f));
        float pulse_phase = time * pulse_speed - ring * 0.5f;
        float radius = base_radius * audio_mod + sinf(pulse_phase) * 5 + beat_expand;
        
        // Draw ring with varying density based on audio
        int segments = (int)(radius * M_PI * 0.5f);
        
        for (int i = 0; i < segments; i++) {
            float angle = (i / (float)segments) * M_PI * 2;
            
            int x = width/2 + (int)(cosf(angle) * radius);
            int y = height/2 + (int)(sinf(angle) * radius * 0.5f);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                char c;
                if (ring % 3 == 0) c = '#';
                else if (ring % 3 == 1) c = '*';
                else c = '+';
                
                // Make inner rings brighter
                float z = rings - ring;
                set_pixel(buffer, zbuffer, width, height, x, y, c, z);
            }
        }
        
        // Add radial spokes on beat
        if (beat_expand > 0 && ring == rings / 2) {
            for (int spoke = 0; spoke < 8; spoke++) {
                float angle = (spoke / 8.0f) * M_PI * 2;
                for (float r = 0; r < radius; r += 2) {
                    int x = width/2 + (int)(cosf(angle) * r);
                    int y = height/2 + (int)(sinf(angle) * r * 0.5f);
                    
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        if (buffer[y * width + x] == ' ') {
                            set_pixel(buffer, zbuffer, width, height, x, y, '-', 5.0f);
                        }
                    }
                }
            }
        }
    }
}

// Scene 187: Audio Waveform 3D - 3D waveform visualization
void scene_audio_waveform_3d(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float wave_height = params[0].value * 15.0f + 5.0f;       // 5-20
    float wave_depth = params[1].value * 20.0f + 10.0f;       // 10-30
    float rotation_speed = params[2].value * 2.0f - 1.0f;     // -1 to 1
    
    float angle = time * rotation_speed;
    
    // Draw multiple waveform layers in 3D
    for (int layer = 0; layer < 5; layer++) {
        float z = wave_depth - layer * 5;
        float layer_scale = 1.0f - layer * 0.15f;
        
        for (int x = 0; x < width; x++) {
            float wave_value = 0.5f;
            
            if (audio && audio->valid) {
                // Map x position to spectrum bands
                int band = (x * 64) / width;
                wave_value = audio->spectrum[band] * layer_scale;
            } else {
                // Simulate waveform
                float freq = 0.1f + layer * 0.05f;
                wave_value = (sinf(x * freq + time * 2) + 1.0f) * 0.25f * layer_scale;
            }
            
            int wave_h = (int)(wave_value * wave_height);
            
            // Apply 3D rotation
            float x3d = (x - width/2.0f) * 0.8f;
            float y3d = -wave_h;
            float z3d = z;
            
            // Rotate around Y axis
            float rx = x3d * cosf(angle) - z3d * sinf(angle);
            float rz = x3d * sinf(angle) + z3d * cosf(angle) + 30;
            
            // Project to 2D
            int px = width/2 + (int)(rx * 40 / rz);
            int py = height/2 + (int)(y3d * 20 / rz);
            
            if (px >= 0 && px < width && py >= 0 && py < height) {
                char c;
                if (layer == 0) c = '#';
                else if (layer == 1) c = '=';
                else if (layer == 2) c = '-';
                else c = '.';
                
                set_pixel(buffer, zbuffer, width, height, px, py, c, rz);
                
                // Draw vertical lines
                if (x % 4 == 0) {
                    for (int h = 0; h < wave_h; h++) {
                        y3d = -h;
                        py = height/2 + (int)(y3d * 20 / rz);
                        if (py >= 0 && py < height) {
                            set_pixel(buffer, zbuffer, width, height, px, py, '|', rz + 0.1f);
                        }
                    }
                }
            }
        }
    }
    
    // Add beat markers
    if (audio && audio->valid && audio->beat_detected) {
        for (int i = 0; i < 5; i++) {
            int x = rand() % width;
            int y = rand() % height;
            set_pixel(buffer, zbuffer, width, height, x, y, '*', 0.1f);
        }
    }
}

// Scene 188: Audio Matrix Grid - Matrix of cells that react to audio
void scene_audio_matrix_grid(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float grid_size = params[0].value * 8.0f + 4.0f;          // 4-12
    float reaction_speed = params[1].value * 5.0f + 1.0f;     // 1-6
    float threshold = params[2].value * 0.7f + 0.1f;          // 0.1-0.8
    
    int cell_size = (int)grid_size;
    int grid_w = width / cell_size;
    int grid_h = height / cell_size;
    
    static float cell_energy[50][50];
    static int initialized = 0;
    
    if (!initialized) {
        memset(cell_energy, 0, sizeof(cell_energy));
        initialized = 1;
    }
    
    // Update cell energy based on audio
    for (int gy = 0; gy < grid_h && gy < 50; gy++) {
        for (int gx = 0; gx < grid_w && gx < 50; gx++) {
            float target_energy = 0;
            
            if (audio && audio->valid) {
                // Map grid position to frequency bands
                int band = (gx * 64) / grid_w;
                target_energy = audio->spectrum[band];
                
                // Add some spatial variation
                float dist = sqrtf((gx - grid_w/2.0f)*(gx - grid_w/2.0f) + 
                                 (gy - grid_h/2.0f)*(gy - grid_h/2.0f));
                target_energy *= 1.0f - dist / (grid_w + grid_h) * 0.5f;
            } else {
                // Simulate with patterns
                float wave1 = sinf(gx * 0.5f + time * 2) * sinf(gy * 0.5f + time * 1.5f);
                target_energy = (wave1 + 1.0f) * 0.5f;
            }
            
            // Smooth energy changes
            float diff = target_energy - cell_energy[gy][gx];
            cell_energy[gy][gx] += diff * reaction_speed * 0.1f;
            
            // Draw cell
            if (cell_energy[gy][gx] > threshold) {
                int cx = gx * cell_size + cell_size/2;
                int cy = gy * cell_size + cell_size/2;
                
                // Fill cell based on energy
                char fill;
                if (cell_energy[gy][gx] > 0.8f) fill = '#';
                else if (cell_energy[gy][gx] > 0.6f) fill = '*';
                else if (cell_energy[gy][gx] > 0.4f) fill = '+';
                else fill = '.';
                
                // Draw cell border
                for (int y = gy * cell_size; y < (gy + 1) * cell_size && y < height; y++) {
                    for (int x = gx * cell_size; x < (gx + 1) * cell_size && x < width; x++) {
                        if (y == gy * cell_size || y == (gy + 1) * cell_size - 1 ||
                            x == gx * cell_size || x == (gx + 1) * cell_size - 1) {
                            set_pixel(buffer, zbuffer, width, height, x, y, '-', 20.0f);
                        } else if (cell_energy[gy][gx] > 0.7f) {
                            // Fill high-energy cells
                            set_pixel(buffer, zbuffer, width, height, x, y, fill, 25.0f);
                        }
                    }
                }
                
                // Center marker
                set_pixel(buffer, zbuffer, width, height, cx, cy, fill, 15.0f);
            }
        }
    }
    
    // Beat effect - flash random cells
    if (audio && audio->valid && audio->beat_detected) {
        for (int i = 0; i < 5; i++) {
            int gx = rand() % grid_w;
            int gy = rand() % grid_h;
            
            for (int y = gy * cell_size; y < (gy + 1) * cell_size && y < height; y++) {
                for (int x = gx * cell_size; x < (gx + 1) * cell_size && x < width; x++) {
                    set_pixel(buffer, zbuffer, width, height, x, y, '*', 5.0f);
                }
            }
        }
    }
}

// Scene 189: Audio Reactive Fractals - Fractals that morph with audio
void scene_audio_reactive_fractals(char* buffer, float* zbuffer, int width, int height, float time, Parameter* params, AudioData* audio) {
    clear_buffer(buffer, zbuffer, width, height);
    
    float zoom = params[0].value * 3.0f + 0.5f;               // 0.5-3.5
    float iterations = params[1].value * 20.0f + 10.0f;       // 10-30
    float audio_morph = params[2].value * 2.0f;               // 0-2
    
    float audio_mod = 0;
    float cx_mod = 0, cy_mod = 0;
    
    if (audio && audio->valid) {
        audio_mod = audio->volume * audio_morph;
        cx_mod = (audio->bass - 0.5f) * 0.5f;
        cy_mod = (audio->treble - 0.5f) * 0.5f;
    } else {
        // Simulate audio modulation
        audio_mod = sinf(time * 2.0f) * 0.3f;
        cx_mod = sinf(time * 1.5f) * 0.2f;
        cy_mod = cosf(time * 1.7f) * 0.2f;
    }
    
    // Julia set with audio-modulated parameters
    float cx = -0.7f + cx_mod + sinf(time * 0.5f) * 0.1f;
    float cy = 0.27f + cy_mod + cosf(time * 0.5f) * 0.1f;
    
    int max_iter = (int)iterations;
    
    for (int py = 0; py < height; py++) {
        for (int px = 0; px < width; px++) {
            // Convert pixel to complex plane
            float x = (px - width/2.0f) / (width * zoom * 0.25f);
            float y = (py - height/2.0f) / (height * zoom * 0.25f);
            
            // Apply audio distortion
            x += audio_mod * sinf(y * 5) * 0.1f;
            y += audio_mod * cosf(x * 5) * 0.1f;
            
            // Julia set iteration
            int iter;
            for (iter = 0; iter < max_iter; iter++) {
                float x2 = x * x;
                float y2 = y * y;
                
                if (x2 + y2 > 4.0f) break;
                
                float xtemp = x2 - y2 + cx;
                y = 2 * x * y + cy;
                x = xtemp;
            }
            
            // Color based on iteration count and audio
            if (iter < max_iter) {
                char c;
                float norm_iter = (float)iter / max_iter;
                
                if (audio && audio->valid) {
                    // Audio-reactive coloring
                    if (norm_iter < audio->bass * 0.3f) c = '#';
                    else if (norm_iter < audio->mid * 0.6f) c = '*';
                    else if (norm_iter < audio->treble * 0.9f) c = '+';
                    else c = '.';
                } else {
                    // Default coloring
                    if (norm_iter < 0.25f) c = '#';
                    else if (norm_iter < 0.5f) c = '*';
                    else if (norm_iter < 0.75f) c = '+';
                    else c = '.';
                }
                
                set_pixel(buffer, zbuffer, width, height, px, py, c, 50.0f - iter);
            }
        }
    }
    
    // Beat effect - invert center
    if (audio && audio->valid && audio->beat_detected) {
        int size = (int)(audio->beat_intensity * 20);
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                int x = width/2 + dx;
                int y = height/2 + dy;
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    int idx = y * width + x;
                    if (buffer[idx] == ' ') buffer[idx] = '.';
                    else if (buffer[idx] == '.') buffer[idx] = ' ';
                    else if (buffer[idx] == '+') buffer[idx] = '*';
                    else if (buffer[idx] == '*') buffer[idx] = '#';
                    else if (buffer[idx] == '#') buffer[idx] = '+';
                }
            }
        }
    }
}

// ============= ALL POST EFFECTS =============

void post_effect_glow(char* buffer, int width, int height, float time) {
    (void)time;
    memcpy(vj.temp_buffer, buffer, width * height);
    
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            char center = vj.temp_buffer[y * width + x];
            if (center != ' ') {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int idx = (y + dy) * width + (x + dx);
                        if (buffer[idx] == ' ') {
                            buffer[idx] = '.';
                        }
                    }
                }
            }
        }
    }
}

void post_effect_blur(char* buffer, int width, int height, float time) {
    (void)time;
    const char blur_chars[] = " .:+*#";
    
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int count = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (get_pixel(buffer, width, height, x + dx, y + dy) != ' ') {
                        count++;
                    }
                }
            }
            
            if (count > 0) {
                vj.temp_buffer[y * width + x] = blur_chars[count > 5 ? 5 : count];
            } else {
                vj.temp_buffer[y * width + x] = ' ';
            }
        }
    }
    
    memcpy(buffer, vj.temp_buffer, width * height);
}

void post_effect_wave_warp(char* buffer, int width, int height, float time) {
    memset(vj.temp_buffer, ' ', width * height);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            char c = buffer[y * width + x];
            if (c != ' ') {
                float wave_x = sinf((float)y * 0.1f + time * 2.0f) * 3.0f;
                float wave_y = cosf((float)x * 0.08f + time * 1.5f) * 2.0f;
                
                int new_x = x + (int)wave_x;
                int new_y = y + (int)wave_y;
                
                if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
                    vj.temp_buffer[new_y * width + new_x] = c;
                }
            }
        }
    }
    
    memcpy(buffer, vj.temp_buffer, width * height);
}

void post_effect_char_emission(char* buffer, int width, int height, float time) {
    memcpy(vj.temp_buffer, buffer, width * height);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            char c = buffer[y * width + x];
            if (c != ' ') {
                for (int dir = 0; dir < 8; dir++) {
                    float angle = dir * M_PI / 4.0f;
                    float wave_strength = sinf(time * 3.0f + dir * 0.5f) * 0.5f + 0.5f;
                    
                    int trail_length = (int)(wave_strength * 6) + 2;
                    
                    for (int step = 1; step <= trail_length; step++) {
                        int emit_x = x + (int)(cosf(angle) * step);
                        int emit_y = y + (int)(sinf(angle) * step);
                        
                        if (emit_x >= 0 && emit_x < width && emit_y >= 0 && emit_y < height) {
                            if (vj.temp_buffer[emit_y * width + emit_x] == ' ') {
                                char emit_chars[] = "*+.:-";
                                int char_idx = step < 5 ? step - 1 : 4;
                                vj.temp_buffer[emit_y * width + emit_x] = emit_chars[char_idx];
                            }
                        }
                    }
                }
            }
        }
    }
    
    memcpy(buffer, vj.temp_buffer, width * height);
}

void post_effect_ripple(char* buffer, int width, int height, float time) {
    memset(vj.temp_buffer, ' ', width * height);
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            char c = buffer[y * width + x];
            if (c != ' ') {
                float dx = x - center_x;
                float dy = y - center_y;
                float dist = sqrtf(dx * dx + dy * dy);
                
                float ripple = sinf(dist * 0.3f - time * 5.0f) * 2.0f;
                
                float angle = atan2f(dy, dx);
                int new_x = x + (int)(cosf(angle) * ripple);
                int new_y = y + (int)(sinf(angle) * ripple);
                
                if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
                    vj.temp_buffer[new_y * width + new_x] = c;
                }
            }
        }
    }
    
    memcpy(buffer, vj.temp_buffer, width * height);
}

// Simplified versions of remaining effects for space
void post_effect_edge(char* buffer, int width, int height, float time) {
    (void)time;
    memset(vj.temp_buffer, ' ', width * height);
    
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            char center = buffer[y * width + x];
            if (center != ' ') {
                bool is_edge = false;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (get_pixel(buffer, width, height, x + dx, y + dy) == ' ') {
                            is_edge = true;
                            break;
                        }
                    }
                }
                if (is_edge) {
                    vj.temp_buffer[y * width + x] = '#';
                }
            }
        }
    }
    
    memcpy(buffer, vj.temp_buffer, width * height);
}

void post_effect_spiral_warp(char* buffer, int width, int height, float time) {
    memset(vj.temp_buffer, ' ', width * height);
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            char c = buffer[y * width + x];
            if (c != ' ') {
                float dx = x - center_x;
                float dy = y - center_y;
                float dist = sqrtf(dx * dx + dy * dy);
                float angle = atan2f(dy, dx);
                
                angle += dist * 0.1f + time;
                
                int new_x = center_x + (int)(dist * cosf(angle));
                int new_y = center_y + (int)(dist * sinf(angle));
                
                if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
                    vj.temp_buffer[new_y * width + new_x] = c;
                }
            }
        }
    }
    
    memcpy(buffer, vj.temp_buffer, width * height);
}

// Implemented effects
void post_effect_invert(char* buffer, int width, int height, float time) {
    (void)time;
    const char* invert_map = " .:-=+*#%@";
    int map_len = strlen(invert_map);
    
    for (int i = 0; i < width * height; i++) {
        char c = buffer[i];
        if (c != ' ') {
            // Find character in map and invert position
            const char* pos = strchr(invert_map, c);
            if (pos) {
                int idx = pos - invert_map;
                buffer[i] = invert_map[map_len - 1 - idx];
            } else {
                // For characters not in map, swap with complementary ASCII
                if (c >= 33 && c <= 126) {
                    buffer[i] = 126 - (c - 33);
                }
            }
        }
    }
}

void post_effect_ascii_gradient(char* buffer, int width, int height, float time) {
    (void)time;
    const char* gradient = " .:-=+*#%@";
    
    for (int i = 0; i < width * height; i++) {
        char c = buffer[i];
        if (c != ' ') {
            // Map any character to gradient based on ASCII value
            int val = (c - 32) % 10;
            buffer[i] = gradient[val];
        }
    }
}

void post_effect_scanlines(char* buffer, int width, int height, float time) {
    // CRT-style scanlines with animation
    int scanline_offset = (int)(time * 5) % 4;
    
    for (int y = 0; y < height; y++) {
        // Every 3rd line with offset for animation
        if ((y + scanline_offset) % 3 == 0) {
            for (int x = 0; x < width; x++) {
                int idx = y * width + x;
                char c = buffer[idx];
                
                if (c != ' ') {
                    // Dim the character
                    if (c == '@' || c == '#') buffer[idx] = '*';
                    else if (c == '*' || c == '%') buffer[idx] = '+';
                    else if (c == '+' || c == '=') buffer[idx] = '-';
                    else if (c == '-' || c == ':') buffer[idx] = '.';
                    else buffer[idx] = '.';
                }
            }
        }
        
        // Add interference lines
        if ((y + (int)(time * 20)) % 15 == 0) {
            for (int x = 0; x < width; x++) {
                if (x % 2 == 0) {
                    buffer[y * width + x] = '-';
                }
            }
        }
    }
}

void post_effect_chromatic(char* buffer, int width, int height, float time) {
    // Chromatic aberration - shift characters to simulate color separation
    memcpy(vj.temp_buffer, buffer, width * height);
    
    // Shift amount based on time for animation
    int shift_r = (int)(sinf(time * 2) * 2) + 1;
    int shift_b = (int)(cosf(time * 2) * 2) - 1;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            char c = vj.temp_buffer[y * width + x];
            
            if (c != ' ') {
                // Red channel shift right
                int red_x = x + shift_r;
                if (red_x >= 0 && red_x < width) {
                    if (buffer[y * width + red_x] == ' ') {
                        buffer[y * width + red_x] = (c == '@' || c == '#') ? '+' : '.';
                    }
                }
                
                // Blue channel shift left
                int blue_x = x + shift_b;
                if (blue_x >= 0 && blue_x < width) {
                    if (buffer[y * width + blue_x] == ' ') {
                        buffer[y * width + blue_x] = (c == '@' || c == '#') ? '*' : ':';
                    }
                }
                
                // Add glitch artifacts
                if ((x + y + (int)(time * 10)) % 50 == 0) {
                    buffer[y * width + x] = "|]}>?"[rand() % 5];
                }
            }
        }
    }
}

// New effect: Echo - Creates trailing echoes of characters
void post_effect_echo(char* buffer, int width, int height, float time) {
    static char echo_buffer1[MAX_WIDTH * MAX_HEIGHT];
    static char echo_buffer2[MAX_WIDTH * MAX_HEIGHT];
    static char echo_buffer3[MAX_WIDTH * MAX_HEIGHT];
    static bool echo_initialized = false;
    
    if (!echo_initialized) {
        memset(echo_buffer1, ' ', width * height);
        memset(echo_buffer2, ' ', width * height);
        memset(echo_buffer3, ' ', width * height);
        echo_initialized = true;
    }
    
    // Shift echo buffers
    memcpy(echo_buffer3, echo_buffer2, width * height);
    memcpy(echo_buffer2, echo_buffer1, width * height);
    memcpy(echo_buffer1, buffer, width * height);
    
    // Apply echo effect with decay
    for (int i = 0; i < width * height; i++) {
        if (buffer[i] == ' ') {
            // Check previous frames for echo
            if (echo_buffer1[i] != ' ') {
                buffer[i] = '.';
            } else if (echo_buffer2[i] != ' ') {
                buffer[i] = ':';
            } else if (echo_buffer3[i] != ' ') {
                buffer[i] = '.';
            }
        }
    }
    
    // Add motion blur based on time
    int offset = (int)(sinf(time * 2.0f) * 2.0f);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int echo_x = x - offset;
            if (echo_x >= 0 && echo_x < width && buffer[y * width + x] == ' ') {
                if (echo_buffer1[y * width + echo_x] != ' ') {
                    buffer[y * width + x] = ',';
                }
            }
        }
    }
}

// New effect: Kaleidoscope - Mirrors and rotates the image in segments
void post_effect_kaleidoscope(char* buffer, int width, int height, float time) {
    memcpy(vj.temp_buffer, buffer, width * height);
    
    int center_x = width / 2;
    int center_y = height / 2;
    int segments = 6; // Hexagonal kaleidoscope
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = y - center_y;
            
            if (dx == 0 && dy == 0) continue;
            
            // Convert to polar coordinates
            float dist = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx) + M_PI;
            
            // Apply kaleidoscope transformation
            float segment_angle = 2 * M_PI / segments;
            float rotated_angle = fmodf(angle + time * 0.5f, segment_angle);
            
            // Mirror every other segment
            int segment = (int)(angle / segment_angle);
            if (segment % 2 == 1) {
                rotated_angle = segment_angle - rotated_angle;
            }
            
            // Convert back to cartesian
            int src_x = center_x + (int)(dist * cosf(rotated_angle));
            int src_y = center_y + (int)(dist * sinf(rotated_angle));
            
            if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                char c = vj.temp_buffer[src_y * width + src_x];
                if (c != ' ') {
                    buffer[y * width + x] = c;
                }
            }
        }
    }
}

// New effect: Droste - Recursive spiral effect
void post_effect_droste(char* buffer, int width, int height, float time) {
    memcpy(vj.temp_buffer, buffer, width * height);
    memset(buffer, ' ', width * height);
    
    int center_x = width / 2;
    int center_y = height / 2;
    
    float zoom_factor = 1.5f + sinf(time * 0.5f) * 0.5f;
    float spiral_factor = 0.1f;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - center_x;
            float dy = y - center_y;
            
            if (dx == 0 && dy == 0) continue;
            
            // Convert to polar
            float dist = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx);
            
            // Apply Droste transformation
            float log_dist = logf(dist / 10.0f + 1.0f);
            float new_dist = expf(fmodf(log_dist * zoom_factor + time * 0.3f, logf(zoom_factor))) * 10.0f - 10.0f;
            float new_angle = angle + log_dist * spiral_factor + time * 0.2f;
            
            // Convert back to cartesian
            int src_x = center_x + (int)(new_dist * cosf(new_angle));
            int src_y = center_y + (int)(new_dist * sinf(new_angle));
            
            if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                char c = vj.temp_buffer[src_y * width + src_x];
                if (c != ' ') {
                    buffer[y * width + x] = c;
                    
                    // Add recursive detail
                    if (dist < height / 4) {
                        int detail_x = center_x + (int)(dist * 2 * cosf(angle + time));
                        int detail_y = center_y + (int)(dist * 2 * sinf(angle + time));
                        if (detail_x >= 0 && detail_x < width && detail_y >= 0 && detail_y < height) {
                            if (buffer[detail_y * width + detail_x] == ' ') {
                                buffer[detail_y * width + detail_x] = '.';
                            }
                        }
                    }
                }
            }
        }
    }
}

// ============= CLIFT ENGINE =============

void apply_post_effect(char* buffer, PostEffect effect, int width, int height) {
    switch (effect) {
        case POST_GLOW: post_effect_glow(buffer, width, height, vj.effect_time); break;
        case POST_BLUR: post_effect_blur(buffer, width, height, vj.effect_time); break;
        case POST_EDGE: post_effect_edge(buffer, width, height, vj.effect_time); break;
        case POST_WAVE_WARP: post_effect_wave_warp(buffer, width, height, vj.effect_time); break;
        case POST_CHAR_EMISSION: post_effect_char_emission(buffer, width, height, vj.effect_time); break;
        case POST_RIPPLE: post_effect_ripple(buffer, width, height, vj.effect_time); break;
        case POST_SPIRAL_WARP: post_effect_spiral_warp(buffer, width, height, vj.effect_time); break;
        case POST_INVERT: post_effect_invert(buffer, width, height, vj.effect_time); break;
        case POST_ASCII_GRADIENT: post_effect_ascii_gradient(buffer, width, height, vj.effect_time); break;
        case POST_SCANLINES: post_effect_scanlines(buffer, width, height, vj.effect_time); break;
        case POST_CHROMATIC: post_effect_chromatic(buffer, width, height, vj.effect_time); break;
        case POST_ECHO: post_effect_echo(buffer, width, height, vj.effect_time); break;
        case POST_KALEIDOSCOPE: post_effect_kaleidoscope(buffer, width, height, vj.effect_time); break;
        case POST_DROSTE: post_effect_droste(buffer, width, height, vj.effect_time); break;
        default: break;
    }
}

void vj_init(int width, int height, bool start_hidden) {
    fprintf(stderr, "DEBUG: vj_init called with %dx%d\n", width, height);
    fflush(stderr);
    
    vj.width = width;
    vj.height = height - 10;  // Leave room for enhanced UI (10 lines)
    vj.time = 0.0f;
    
    // Safety check for dimensions
    if (vj.width <= 0 || vj.height <= 0) {
        fprintf(stderr, "ERROR: Invalid dimensions in vj_init: %dx%d\n", vj.width, vj.height);
        exit(1);
    }
    
    int buffer_size = vj.width * vj.height;
    fprintf(stderr, "DEBUG: Allocating buffers, size=%d (%dx%d)\n", buffer_size, vj.width, vj.height);
    fflush(stderr);
    
    vj.deck_a.buffer = malloc(buffer_size);
    if (!vj.deck_a.buffer) {
        fprintf(stderr, "ERROR: Failed to allocate deck_a.buffer (%d bytes)\n", buffer_size);
        exit(1);
    }
    
    vj.deck_a.zbuffer = malloc(buffer_size * sizeof(float));
    if (!vj.deck_a.zbuffer) {
        fprintf(stderr, "ERROR: Failed to allocate deck_a.zbuffer (%zu bytes)\n", buffer_size * sizeof(float));
        exit(1);
    }
    
    vj.deck_b.buffer = malloc(buffer_size);
    if (!vj.deck_b.buffer) {
        fprintf(stderr, "ERROR: Failed to allocate deck_b.buffer (%d bytes)\n", buffer_size);
        exit(1);
    }
    
    vj.deck_b.zbuffer = malloc(buffer_size * sizeof(float));
    if (!vj.deck_b.zbuffer) {
        fprintf(stderr, "ERROR: Failed to allocate deck_b.zbuffer (%zu bytes)\n", buffer_size * sizeof(float));
        exit(1);
    }
    
    vj.output_buffer = malloc(buffer_size);
    if (!vj.output_buffer) {
        fprintf(stderr, "ERROR: Failed to allocate output_buffer (%d bytes)\n", buffer_size);
        exit(1);
    }
    
    vj.temp_buffer = malloc(buffer_size);
    if (!vj.temp_buffer) {
        fprintf(stderr, "ERROR: Failed to allocate temp_buffer (%d bytes)\n", buffer_size);
        exit(1);
    }
    
    vj.output_zbuffer = malloc(buffer_size * sizeof(float));
    if (!vj.output_zbuffer) {
        fprintf(stderr, "ERROR: Failed to allocate output_zbuffer (%zu bytes)\n", buffer_size * sizeof(float));
        exit(1);
    }
    
    fprintf(stderr, "DEBUG: All buffers allocated successfully\n");
    fflush(stderr);
    
    vj.crossfade_state = XFADE_FULL_A;  // Start with deck A only
    param_init(&vj.master_volume, "Master Vol", 1.0f, 0.0f, 2.0f);
    param_init(&vj.master_speed, "Master Speed", 1.0f, 0.1f, 5.0f);
    
    // Initialize BPM system
    vj.bpm_system.bpm = 120.0f;  // Default BPM
    vj.bpm_system.tap_count = 0;
    vj.bpm_system.auto_crossfade_enabled = false;
    vj.bpm_system.crossfade_beat_interval = 16.0f;  // Change every 16 beats
    vj.bpm_system.last_crossfade_time = 0.0f;
    
    // Initialize decks
    vj.deck_a.active = true;
    vj.deck_a.scene_id = 1;  // Start with scene 1
    vj.deck_a.post_effect = POST_NONE;
    vj.deck_a.selected = true;
    vj.deck_a.primary_color = 1;    // Red
    vj.deck_a.secondary_color = 4;  // Yellow
    vj.deck_a.gradient_type = GRADIENT_LINEAR_H;  // Horizontal gradient
    
    vj.deck_b.active = true;
    vj.deck_b.scene_id = 53;  // Start with Wormhole
    vj.deck_b.post_effect = POST_NONE;
    vj.deck_b.selected = false;
    vj.deck_b.primary_color = 3;    // Blue
    vj.deck_b.secondary_color = 6;  // Cyan
    vj.deck_b.gradient_type = GRADIENT_RADIAL;  // Radial gradient
    
    // Initialize parameters for each deck
    for (int i = 0; i < 8; i++) {
        param_init(&vj.deck_a.params[i], "Param", 1.0f, 0.0f, 3.0f);
        param_init(&vj.deck_b.params[i], "Param", 1.0f, 0.0f, 3.0f);
    }
    
    vj.performance_mode = false;
    vj.selected_deck = 0;
    vj.selected_param = 0;
    vj.current_ui_page = UI_PAGE_PERFORMANCE;  // Start with performance page
    vj.show_help = false;
    vj.hide_ui = start_hidden;
    
    // Initialize preset system
    init_presets();
    
    // Initialize live coding monitor
    init_live_coding_monitor();
    
    // Initialize auto mode
    vj.full_auto_mode = false;
    vj.last_auto_change = 0.0f;
    vj.auto_change_interval = 8.0f; // Default 8 seconds
    vj.last_effect_change = 0.0f;
    vj.effect_change_interval = 1.0f; // Default 1 second (will be beat-synced)
    
    // Initialize Ableton Link state
    vj.link.enabled = false;
    vj.link.connected = false;
    vj.link.link_bpm = 120.0f;
    vj.link.link_beat = 0.0f;
    vj.link.link_phase = 0.0f;
    vj.link.num_peers = 0;
    vj.link.quantum = 4.0f;  // 4 beat bar
    vj.link.start_stop_sync = false;
    vj.link.is_playing = false;
    vj.link.link_handle = link_create(120.0f);  // Create Link instance
    
    // Initialize audio system
    vj.audio_enabled = false;
    vj.audio_gain = 5.0f;
    vj.audio_smoothing = 0.1f;
    vj.audio_device_id = -1;  // Default device
    strcpy(vj.audio_device_name, "Default Audio Input");
    vj.audio_thread_running = false;
    vj.selected_audio_source = 0;
    pthread_mutex_init(&vj.audio_mutex, NULL);
    
    // Initialize audio data
    vj.audio_data.bass = 0.0f;
    vj.audio_data.mid = 0.0f;
    vj.audio_data.treble = 0.0f;
    vj.audio_data.volume = 0.0f;
    vj.audio_data.bpm = 120.0f;
    vj.audio_data.beat_detected = false;
    vj.audio_data.beat_intensity = 0.0f;
    vj.audio_data.valid = false;
    memset(vj.audio_data.spectrum, 0, sizeof(vj.audio_data.spectrum));
}

// Audio input functions
void* audio_capture_thread(void* arg) {
    (void)arg;
    
    // Initialize audio system
    bool use_real_audio = audio_pipewire_init("CLIFT");
    float* audio_buffer = malloc(AUDIO_BUFFER_SIZE * AUDIO_CHANNELS * sizeof(float));
    
    if (!audio_buffer) {
        return NULL;
    }
    
    while (vj.audio_thread_running) {
        pthread_mutex_lock(&vj.audio_mutex);
        
        if (vj.audio_enabled) {
            if (use_real_audio) {
                // Get audio from PipeWire
                int frames = audio_pipewire_get_buffer(audio_buffer, AUDIO_BUFFER_SIZE);
                
                if (frames > 0) {
                    // Compute spectrum
                    audio_compute_spectrum(audio_buffer, frames, vj.audio_data.spectrum, 64);
                    
                    // Apply gain and smoothing
                    static float smooth_spectrum[64] = {0};
                    for (int i = 0; i < 64; i++) {
                        vj.audio_data.spectrum[i] *= vj.audio_gain;
                        smooth_spectrum[i] = smooth_spectrum[i] * vj.audio_smoothing + 
                                           vj.audio_data.spectrum[i] * (1.0f - vj.audio_smoothing);
                        vj.audio_data.spectrum[i] = smooth_spectrum[i];
                    }
                    
                    // Compute levels
                    audio_compute_levels(vj.audio_data.spectrum, 64,
                                       &vj.audio_data.bass, &vj.audio_data.mid,
                                       &vj.audio_data.treble, &vj.audio_data.volume);
                    
                    // Beat detection
                    vj.audio_data.beat_detected = audio_detect_beat(vj.audio_data.volume, 
                                                                   &vj.audio_data.beat_intensity);
                    
                    vj.audio_data.valid = true;
                }
            } else {
                // Fallback to simulation if audio init failed
                float time = vj.time;
                
                // Simulate varying audio levels
                vj.audio_data.volume = (sinf(time * 2.0f) + 1.0f) * 0.5f * vj.audio_gain;
                vj.audio_data.bass = (sinf(time * 1.5f) + 1.0f) * 0.5f * vj.audio_gain;
                vj.audio_data.mid = (sinf(time * 3.0f) + 1.0f) * 0.5f * vj.audio_gain;
                vj.audio_data.treble = (sinf(time * 5.0f) + 1.0f) * 0.5f * vj.audio_gain;
                
                // Simulate beat detection
                float beat_phase = fmodf(time * (vj.audio_data.bpm / 60.0f), 1.0f);
                vj.audio_data.beat_detected = (beat_phase < 0.1f);
                vj.audio_data.beat_intensity = vj.audio_data.beat_detected ? 1.0f : 0.0f;
                
                // Simulate spectrum
                for (int i = 0; i < 64; i++) {
                    vj.audio_data.spectrum[i] = sinf(time * (i + 1) * 0.5f) * 0.5f + 0.5f;
                    vj.audio_data.spectrum[i] *= vj.audio_gain;
                    
                    // Apply smoothing
                    static float smooth_spectrum[64] = {0};
                    smooth_spectrum[i] = smooth_spectrum[i] * vj.audio_smoothing + 
                                       vj.audio_data.spectrum[i] * (1.0f - vj.audio_smoothing);
                    vj.audio_data.spectrum[i] = smooth_spectrum[i];
                }
                
                vj.audio_data.valid = true;
            }
        } else {
            vj.audio_data.valid = false;
        }
        
        pthread_mutex_unlock(&vj.audio_mutex);
        
        // Sleep for ~60fps update rate
        usleep(16667);
    }
    
    // Cleanup
    if (use_real_audio) {
        audio_pipewire_cleanup();
    }
    free(audio_buffer);
    
    return NULL;
}

void start_audio_capture() {
    if (!vj.audio_thread_running) {
        vj.audio_thread_running = true;
        pthread_create(&vj.audio_thread, NULL, audio_capture_thread, NULL);
    }
}

void stop_audio_capture() {
    if (vj.audio_thread_running) {
        vj.audio_thread_running = false;
        pthread_join(vj.audio_thread, NULL);
    }
}

// Audio connection management functions
typedef struct {
    char name[256];
    char port_fl[256];
    char port_fr[256];
    bool is_monitor;
} AudioSource;

// Execute a command and return the output
bool exec_command(const char* cmd, char* output, size_t output_size) {
    // Create a modified command that redirects stderr to /dev/null
    char safe_cmd[1024];
    snprintf(safe_cmd, sizeof(safe_cmd), "%s 2>/dev/null", cmd);
    
    // Save current stdout and stderr
    fflush(stdout);
    fflush(stderr);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);
    
    // Redirect stdout and stderr to /dev/null during command execution
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
    }
    
    FILE *fp = popen(safe_cmd, "r");
    
    // Restore stdout and stderr immediately after popen
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    if (null_fd >= 0) {
        close(null_fd);
    }
    
    if (!fp) return false;
    
    if (output && output_size > 0) {
        output[0] = '\0';
        char line[256];
        while (fgets(line, sizeof(line), fp) != NULL) {
            size_t len = strlen(output);
            if (len + strlen(line) < output_size - 1) {
                strcat(output, line);
            }
        }
    }
    
    int status = pclose(fp);
    return (status == 0 || status == 256); // 256 = exit code 1 (no matches found)
}

// Get available audio sources
int get_audio_sources(AudioSource* sources, int max_sources) {
    char output[8192];
    if (!exec_command("pw-link -o | grep -E '_FL$|:output_FL$' | sort", output, sizeof(output))) {
        return 0;
    }
    
    int count = 0;
    char* line = strtok(output, "\n");
    
    while (line != NULL && count < max_sources) {
        // Extract the base name without _FL suffix
        char base_name[256];
        strncpy(base_name, line, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
        
        // Remove _FL suffix
        char* fl_suffix = strstr(base_name, "_FL");
        if (fl_suffix) {
            *fl_suffix = '\0';
            
            // Create source entry
            strncpy(sources[count].name, base_name, sizeof(sources[count].name) - 1);
            snprintf(sources[count].port_fl, sizeof(sources[count].port_fl), "%s_FL", base_name);
            snprintf(sources[count].port_fr, sizeof(sources[count].port_fr), "%s_FR", base_name);
            sources[count].is_monitor = (strstr(base_name, "Monitor") != NULL || strstr(base_name, "monitor") != NULL);
            count++;
        }
        
        line = strtok(NULL, "\n");
    }
    
    return count;
}

// Connect to a specific audio source
bool audio_connect_to_source(const AudioSource* source) {
    if (!source) return false;
    
    char cmd[512];
    bool success = true;
    
    // Connect left channel
    snprintf(cmd, sizeof(cmd), "pw-link \"%s\" \"CLIFT:input_FL\" 2>/dev/null", source->port_fl);
    success &= exec_command(cmd, NULL, 0);
    
    // Connect right channel
    snprintf(cmd, sizeof(cmd), "pw-link \"%s\" \"CLIFT:input_FR\" 2>/dev/null", source->port_fr);
    success &= exec_command(cmd, NULL, 0);
    
    return success;
}

// Connect to system monitor (first available)
bool audio_connect_to_monitor() {
    AudioSource sources[50];
    int count = get_audio_sources(sources, 50);
    
    // Find first monitor
    for (int i = 0; i < count; i++) {
        if (sources[i].is_monitor) {
            return audio_connect_to_source(&sources[i]);
        }
    }
    
    // If no monitor found, try alternative approach
    char output[1024];
    if (exec_command("pw-link -o | grep -iE 'monitor.*FL' | head -1", output, sizeof(output))) {
        char* newline = strchr(output, '\n');
        if (newline) *newline = '\0';
        
        if (strlen(output) > 0) {
            char base_name[256];
            strncpy(base_name, output, sizeof(base_name) - 1);
            char* fl_suffix = strstr(base_name, "_FL");
            if (fl_suffix) {
                *fl_suffix = '\0';
                
                AudioSource monitor;
                strncpy(monitor.name, base_name, sizeof(monitor.name) - 1);
                snprintf(monitor.port_fl, sizeof(monitor.port_fl), "%s_FL", base_name);
                snprintf(monitor.port_fr, sizeof(monitor.port_fr), "%s_FR", base_name);
                monitor.is_monitor = true;
                
                return audio_connect_to_source(&monitor);
            }
        }
    }
    
    return false;
}

// Disconnect all audio connections
bool audio_disconnect_all() {
    // Get current connections
    char output[4096];
    if (!exec_command("pw-link | grep 'CLIFT:input'", output, sizeof(output))) {
        return true; // No connections to disconnect
    }
    
    // Parse and disconnect each connection
    char* line = strtok(output, "\n");
    while (line != NULL) {
        // Format: "source -> destination"
        char src[256], dst[256];
        if (sscanf(line, "%s -> %s", src, dst) == 2) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "pw-link -d \"%s\" \"%s\" 2>/dev/null", src, dst);
            exec_command(cmd, NULL, 0);
        }
        line = strtok(NULL, "\n");
    }
    
    return true;
}

// Count current audio connections
int audio_count_connections() {
    char output[32];
    if (!exec_command("pw-link | grep -c 'CLIFT:input' || echo 0", output, sizeof(output))) {
        return 0;
    }
    
    return atoi(output);
}

// Get current connection info
bool audio_get_connection_info(char* info, size_t info_size) {
    if (!info || info_size == 0) return false;
    
    info[0] = '\0';
    char output[2048];
    
    if (exec_command("pw-link | grep 'CLIFT:input' | head -2", output, sizeof(output))) {
        // Extract source names
        char* line = strtok(output, "\n");
        if (line) {
            char src[256];
            if (sscanf(line, "%s ->", src) == 1) {
                // Remove port suffix to get base name
                char* colon = strrchr(src, ':');
                if (colon) *colon = '\0';
                
                snprintf(info, info_size, "Connected to: %s", src);
                return true;
            }
        }
    }
    
    snprintf(info, info_size, "No audio connections");
    return false;
}

// BPM functions
void tap_bpm(float current_time) {
    if (vj.bpm_system.tap_count == 0) {
        vj.bpm_system.last_tap_time = current_time;
        vj.bpm_system.tap_times[0] = current_time;
        vj.bpm_system.tap_count = 1;
        return;
    }
    
    // Add new tap
    vj.bpm_system.tap_times[vj.bpm_system.tap_count % 8] = current_time;
    vj.bpm_system.tap_count++;
    
    if (vj.bpm_system.tap_count >= 2) {
        // Calculate BPM from recent taps
        int count = vj.bpm_system.tap_count > 8 ? 8 : vj.bpm_system.tap_count;
        float total_interval = 0.0f;
        int intervals = 0;
        
        for (int i = 1; i < count; i++) {
            float interval = vj.bpm_system.tap_times[i] - vj.bpm_system.tap_times[i-1];
            if (interval > 0.2f && interval < 2.0f) {  // Reasonable BPM range (30-300)
                total_interval += interval;
                intervals++;
            }
        }
        
        if (intervals > 0) {
            float avg_interval = total_interval / intervals;
            vj.bpm_system.bpm = 60.0f / avg_interval;
            
            // Clamp to reasonable range
            if (vj.bpm_system.bpm < 60.0f) vj.bpm_system.bpm = 60.0f;
            if (vj.bpm_system.bpm > 200.0f) vj.bpm_system.bpm = 200.0f;
            
            // Update Link tempo if enabled
            if (vj.link.enabled && vj.link.link_handle) {
                link_set_tempo(vj.link.link_handle, vj.bpm_system.bpm);
            }
        }
    }
    
    vj.bpm_system.last_tap_time = current_time;
}

void update_auto_crossfade(float current_time) {
    if (!vj.bpm_system.auto_crossfade_enabled) return;
    
    float beat_duration = 60.0f / vj.bpm_system.bpm;
    float crossfade_interval = beat_duration * vj.bpm_system.crossfade_beat_interval;
    
    if (current_time - vj.bpm_system.last_crossfade_time >= crossfade_interval) {
        // Cycle through crossfade states
        vj.crossfade_state = (vj.crossfade_state + 1) % 3;
        vj.bpm_system.last_crossfade_time = current_time;
    }
}

// Full Auto Mode Functions
void randomize_deck_scene(CLIFTDeck* deck) {
    // Choose random scene (0-189)
    deck->scene_id = rand() % 190;
}

void randomize_deck_colors(CLIFTDeck* deck) {
    // Random colors (1-7, avoid 0 which is black)
    deck->primary_color = (rand() % 7) + 1;
    deck->secondary_color = (rand() % 7) + 1;
    // Random gradient type
    deck->gradient_type = rand() % GRADIENT_COUNT;
}

void randomize_deck_parameters(CLIFTDeck* deck) {
    // Randomize all 8 parameters within their ranges
    for (int i = 0; i < 8; i++) {
        Parameter* param = &deck->params[i];
        float range = param->max - param->min;
        param->target = param->min + (rand() / (float)RAND_MAX) * range;
    }
}

void randomize_deck_post_effect(CLIFTDeck* deck) {
    // Random post effect (0-15 to include new effects)
    deck->post_effect = rand() % POST_COUNT;
}

// Update Ableton Link state using real Link API
void update_link_state(float current_time) {
    if (!vj.link.link_handle) return;
    
    // Get current Link state
    LinkState state = link_get_state(vj.link.link_handle);
    
    // Update our local state from Link
    vj.link.connected = state.connected;
    vj.link.link_bpm = state.link_bpm;
    vj.link.link_beat = state.link_beat;
    vj.link.link_phase = state.link_phase;
    vj.link.num_peers = state.num_peers;
    vj.link.is_playing = state.is_playing;
    // quantum is managed locally, not by Link
    
    // If Link is enabled and connected, override local BPM
    if (vj.link.enabled && vj.link.connected) {
        vj.bpm_system.bpm = vj.link.link_bpm;
        // TODO: Add beat_phase to BPMSystem for tighter sync
    }
}

void update_full_auto_mode(float current_time) {
    if (!vj.full_auto_mode) return;
    
    // Check if it's time for a change
    if (current_time - vj.last_auto_change >= vj.auto_change_interval) {
        // Always change scenes AND colors together for maximum impact
        int change_mode = rand() % 3;
        
        switch (change_mode) {
            case 0: // Change deck A, set crossfade to show it
                randomize_deck_scene(&vj.deck_a);
                randomize_deck_colors(&vj.deck_a);
                randomize_deck_parameters(&vj.deck_a);
                vj.crossfade_state = XFADE_FULL_A; // Show deck A
                break;
                
            case 1: // Change deck B, set crossfade to show it  
                randomize_deck_scene(&vj.deck_b);
                randomize_deck_colors(&vj.deck_b);
                randomize_deck_parameters(&vj.deck_b);
                vj.crossfade_state = XFADE_FULL_B; // Show deck B
                break;
                
            case 2: // Change BOTH decks, mix them
                randomize_deck_scene(&vj.deck_a);
                randomize_deck_colors(&vj.deck_a);
                randomize_deck_parameters(&vj.deck_a);
                randomize_deck_scene(&vj.deck_b);
                randomize_deck_colors(&vj.deck_b);
                randomize_deck_parameters(&vj.deck_b);
                vj.crossfade_state = XFADE_MIX; // Show both mixed
                break;
        }
        
        vj.last_auto_change = current_time;
        
        // Randomize next change interval (4-16 beats)
        float beat_duration = 60.0f / vj.bpm_system.bpm;
        float base_beats = 4.0f + (rand() / (float)RAND_MAX) * 12.0f; // 4-16 beats
        vj.auto_change_interval = beat_duration * base_beats;
    }
    
    // FAST EFFECT CHANGES - Every beat
    if (current_time - vj.last_effect_change >= vj.effect_change_interval) {
        // Randomly change effects on both decks every beat
        if (rand() % 3 == 0) { // 33% chance each beat
            randomize_deck_post_effect(&vj.deck_a);
        }
        if (rand() % 3 == 0) { // 33% chance each beat  
            randomize_deck_post_effect(&vj.deck_b);
        }
        
        vj.last_effect_change = current_time;
        float beat_duration = 60.0f / vj.bpm_system.bpm;
        vj.effect_change_interval = beat_duration; // Every beat
    }
}

void vj_update(float dt) {
    vj.time += dt * vj.master_speed.value;
    vj.effect_time = vj.time;
    
    param_update(&vj.master_volume, dt);
    param_update(&vj.master_speed, dt);
    
    // Update automatic crossfade based on BPM
    update_auto_crossfade(vj.time);
    
    // Update full auto mode
    update_full_auto_mode(vj.time);
    
    // Update Ableton Link state
    update_link_state(vj.time);
    
    // Update live coding monitor
    update_cpu_usage();
    
    for (int i = 0; i < 8; i++) {
        param_update(&vj.deck_a.params[i], dt);
        param_update(&vj.deck_b.params[i], dt);
    }
}

void vj_render() {
    CLIFTDeck* decks[] = {&vj.deck_a, &vj.deck_b};
    
    for (int d = 0; d < 2; d++) {
        CLIFTDeck* deck = decks[d];
        if (!deck->active) continue;
        
        // Debug: Check for valid deck and buffers
        if (!deck) {
            fprintf(stderr, "ERROR: NULL deck in vj_render\n");
            continue;
        }
        if (!deck->buffer || !deck->zbuffer) {
            fprintf(stderr, "ERROR: NULL buffer in deck %d (scene %d)\n", d, deck->scene_id);
            continue;
        }
        
        if (deck->scene_id < 0 || deck->scene_id > 189) {
            deck->scene_id = 0;  // Reset to safe scene
        }
        
        // Render scene
        switch (deck->scene_id) {
            // Basic scenes (0-9)
            case 0: scene_audio_bars(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 1: scene_cube(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 2: scene_dna_helix(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 3: scene_particle_field(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 4: scene_torus(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 5: scene_fractal_tree(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 6: scene_wave_mesh(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 7: scene_sphere(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 8: scene_spirograph(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 9: scene_matrix_rain(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Geometric scenes (10-19)
            case 10: scene_tunnels(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 11: scene_kaleidoscope(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 12: scene_mandala(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 13: scene_sierpinski(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 14: scene_hexagon_grid(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 15: scene_tessellations(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 16: scene_voronoi_cells(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 17: scene_sacred_geometry(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 18: scene_polyhedra(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 19: scene_maze_generator(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Organic scenes (20-29)
            case 20: scene_fire_simulation(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 21: scene_water_waves(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 22: scene_lightning(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 23: scene_plasma_clouds(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 24: scene_galaxy_spiral(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 25: scene_tree_of_life(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 26: scene_cellular_automata(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 27: scene_flocking_birds(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 28: scene_wind_patterns(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 29: scene_neural_networks(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Text/Code scenes (30-39)
            case 30: scene_matrix_rain(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 31: scene_ascii_art_generator(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 32: scene_code_rain(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 33: scene_binary_stream(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 34: scene_terminal_glitch(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 35: scene_syntax_highlighting(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 36: scene_data_visualization(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 37: scene_network_nodes(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 38: scene_system_monitor(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 39: scene_command_line(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Abstract scenes (40-49)
            case 40: scene_noise_field(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 41: scene_swarm_intelligence(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 42: scene_fractal_zoom(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 43: scene_morphing_shapes(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 44: scene_glitch_corruption(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 45: scene_energy_waves(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 46: scene_digital_rain(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 47: scene_psychedelic_patterns(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 48: scene_quantum_field(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 49: scene_abstract_flow(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Infinite Tunnel scenes (50-59)
            case 50: scene_spiral_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 51: scene_hex_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 52: scene_star_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 53: scene_wormhole(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 54: scene_cyber_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 55: scene_ring_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 56: scene_matrix_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 57: scene_speed_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 58: scene_pulse_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 59: scene_vortex_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Nature scenes (60-69)
            case 60: scene_ocean_waves(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 61: scene_rain_storm(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 62: scene_infinite_forest(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 63: scene_growing_trees(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 64: scene_mountain_range(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 65: scene_aurora_borealis(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 66: scene_flowing_river(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 67: scene_desert_dunes(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 68: scene_coral_reef(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 69: scene_butterfly_garden(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Explosion scenes (70-79)
            case 70: scene_nuclear_blast(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 71: scene_building_collapse(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 72: scene_meteor_impact(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 73: scene_chain_explosions(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 74: scene_volcanic_eruption(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 75: scene_shockwave_blast(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 76: scene_glass_shatter(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 77: scene_demolition_blast(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 78: scene_supernova_burst(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 79: scene_plasma_discharge(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // City scenes (80-89)
            case 80: scene_cyberpunk_city(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 81: scene_city_lights(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 82: scene_skyscraper_forest(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 83: scene_urban_decay(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 84: scene_future_metropolis(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 85: scene_city_grid(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 86: scene_digital_city(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 87: scene_city_flythrough(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 88: scene_neon_districts(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 89: scene_urban_canyon(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Freestyle scenes (90-99)
            case 90: scene_black_hole(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 91: scene_quantum_field(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 92: scene_dimensional_rift(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 93: scene_alien_landscape(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 94: scene_robot_factory(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 95: scene_time_vortex(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 96: scene_glitch_world(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 97: scene_neural_network(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 98: scene_cosmic_dance(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 99: scene_reality_glitch(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Human scenes (100-109)
            case 100: scene_human_walker(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 101: scene_dance_party(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 102: scene_martial_arts(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 103: scene_human_pyramid(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 104: scene_yoga_flow(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 105: scene_sports_stadium(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 106: scene_robot_dance(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 107: scene_crowd_wave(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 108: scene_mirror_dance(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 109: scene_human_evolution(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Warfare scenes (110-119)
            case 110: scene_fighter_squadron(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 111: scene_drone_swarm(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 112: scene_strategic_bombing(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 113: scene_dogfight(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 114: scene_helicopter_assault(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 115: scene_stealth_mission(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 116: scene_carrier_strike(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 117: scene_missile_defense(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 118: scene_recon_drone(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            case 119: scene_air_command(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL); break;
            
            // Revolution & Eyes scenes (120-129)
            case 120: scene_120(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 121: scene_121(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 122: scene_122(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 123: scene_123(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 124: scene_124(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 125: scene_125(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 126: scene_126(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 127: scene_127(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 128: scene_128(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 129: scene_129(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            
            // Film Noir scenes (130-139)
            case 130: scene_130(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 131: scene_131(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 132: scene_132(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 133: scene_133(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 134: scene_134(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 135: scene_135(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 136: scene_136(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 137: scene_137(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 138: scene_138(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 139: scene_139(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
                
            // Escher 3D Illusion scenes (140-149)
            case 140: scene_impossible_stairs(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 141: scene_mobius_strip(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 142: scene_impossible_cube(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 143: scene_penrose_triangle(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 144: scene_infinite_corridor(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 145: scene_tessellated_reality(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 146: scene_gravity_wells(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 147: scene_dimensional_shift(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 148: scene_fractal_architecture(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 149: scene_escher_waterfall(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            
            // Ikeda-inspired scenes (150-159)
            case 150: scene_ikeda_data_matrix(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 151: scene_ikeda_test_pattern(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 152: scene_ikeda_sine_wave(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 153: scene_ikeda_barcode(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 154: scene_ikeda_pulse(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 155: scene_ikeda_glitch(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 156: scene_ikeda_spectrum(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 157: scene_ikeda_phase(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 158: scene_ikeda_binary(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 159: scene_ikeda_circuit(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            
            // Giger-Inspired scenes (160-169)
            case 160: scene_giger_spine(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 161: scene_giger_eggs(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 162: scene_giger_tentacles(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 163: scene_giger_hive(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 164: scene_giger_skull(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 165: scene_giger_facehugger(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 166: scene_giger_heart(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 167: scene_giger_architecture(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 168: scene_giger_chestburster(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 169: scene_giger_space_jockey(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            
            // Revolt scenes (170-179)
            case 170: scene_revolt_rising_fists(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 171: scene_revolt_breaking_chains(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 172: scene_revolt_crowd_march(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 173: scene_revolt_barricade_building(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 174: scene_revolt_molotov_cocktails(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 175: scene_revolt_tear_gas(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 176: scene_revolt_graffiti_wall(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 177: scene_revolt_police_line_breaking(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 178: scene_revolt_flag_burning(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            case 179: scene_revolt_victory_dance(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params); break;
            
            // Audio reactive scenes (180-189)
            case 180: scene_audio_reactive_cubes(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 181: scene_audio_flash_strobes(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 182: scene_audio_explosions(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 183: scene_audio_wave_tunnel(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 184: scene_audio_spectrum_3d(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 185: scene_audio_reactive_particles(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 186: scene_audio_pulse_rings(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 187: scene_audio_waveform_3d(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 188: scene_audio_matrix_grid(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
            case 189: scene_audio_reactive_fractals(deck->buffer, deck->zbuffer, vj.width, vj.height, vj.time, deck->params, vj.audio_enabled ? &vj.audio_data : NULL); break;
                
            default:
                // Fallback to audio bars for any undefined scenes
                scene_audio_bars(deck->buffer, deck->zbuffer, vj.width, vj.height, deck->params, vj.time, NULL);
                break;
        }
        
        // Apply post effect
        apply_post_effect(deck->buffer, deck->post_effect, vj.width, vj.height);
    }
    
    // Simple 3-state crossfade mixing
    for (int i = 0; i < vj.width * vj.height; i++) {
        char a = vj.deck_a.buffer[i];
        char b = vj.deck_b.buffer[i];
        
        switch (vj.crossfade_state) {
            case XFADE_FULL_A:
                vj.output_buffer[i] = a;
                break;
                
            case XFADE_FULL_B:
                vj.output_buffer[i] = b;
                break;
                
            case XFADE_MIX:
                // Mix both decks - alternate characters for blend effect
                if (a != ' ' && b != ' ') {
                    // Both have content - use pattern for mixing
                    vj.output_buffer[i] = ((i + (int)(vj.time * 8)) % 2) ? a : b;
                } else if (a != ' ') {
                    vj.output_buffer[i] = a;
                } else {
                    vj.output_buffer[i] = b;
                }
                break;
        }
    }
}

// ============= ENHANCED USER INTERFACE =============

// Get color pair based on deck settings and audio
// Calculate gradient factor based on position and gradient type
float calculate_gradient_factor(int x, int y, int width, int height, GradientType type, float time) {
    float factor = 0.0f;
    float cx = width * 0.5f;
    float cy = height * 0.5f;
    float nx = (float)x / width;   // Normalized x (0-1)
    float ny = (float)y / height;  // Normalized y (0-1)
    
    switch (type) {
        case GRADIENT_LINEAR_H:
            factor = nx;
            break;
            
        case GRADIENT_LINEAR_V:
            factor = ny;
            break;
            
        case GRADIENT_LINEAR_D1:
            factor = (nx + ny) * 0.5f;
            break;
            
        case GRADIENT_LINEAR_D2:
            factor = (nx + (1.0f - ny)) * 0.5f;
            break;
            
        case GRADIENT_RADIAL:
            factor = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) / (width * 0.5f);
            factor = fminf(factor, 1.0f);
            break;
            
        case GRADIENT_DIAMOND:
            factor = (fabsf(x - cx) + fabsf(y - cy)) / (width * 0.5f);
            factor = fminf(factor, 1.0f);
            break;
            
        case GRADIENT_WAVE_H:
            factor = 0.5f + 0.5f * sinf(nx * 6.28f + time * 2.0f);
            break;
            
        case GRADIENT_WAVE_V:
            factor = 0.5f + 0.5f * sinf(ny * 6.28f + time * 2.0f);
            break;
            
        case GRADIENT_NOISE:
            // Simple pseudo-noise based on position
            factor = 0.5f + 0.5f * sinf(nx * 12.34f + ny * 56.78f + time);
            break;
            
        case GRADIENT_SPIRAL:
            {
                float angle = atan2f(y - cy, x - cx);
                float radius = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
                factor = fmodf(angle / 6.28f + radius * 0.02f + time * 0.5f, 1.0f);
            }
            break;
            
        default:
            factor = nx; // Default to horizontal
            break;
    }
    
    return fmaxf(0.0f, fminf(1.0f, factor));
}

int get_visual_color(char c, CLIFTDeck* deck, float audio_intensity, int x, int y, int width, int height, float time) {
    if (c == ' ') return 0; // No color for spaces
    
    // Special case for Matrix rain - always green
    if (deck->scene_id == 9) {
        return 2; // Green for Matrix effect
    }
    
    // Calculate gradient factor for this position
    float gradient_factor = calculate_gradient_factor(x, y, width, height, deck->gradient_type, time);
    
    // Apply audio intensity modulation to gradient
    gradient_factor += audio_intensity * 0.3f;
    gradient_factor = fmaxf(0.0f, fminf(1.0f, gradient_factor));
    
    // Blend between primary and secondary colors based on gradient
    if (gradient_factor > 0.5f) {
        return deck->secondary_color;
    } else {
        return deck->primary_color;
    }
}

void vj_render_ui() {
    // Render live coding overlay first (before main buffer rendering)
    render_live_coding_overlay();
    
    // Render main output with enhanced color mapping
    for (int y = 0; y < vj.height; y++) {
        for (int x = 0; x < vj.width; x++) {
            char c = vj.output_buffer[y * vj.width + x];
            
            if (has_colors() && c != ' ') {
                // Enhanced color selection based on crossfade state
                int color_pair;
                
                switch (vj.crossfade_state) {
                    case XFADE_FULL_A:
                        color_pair = get_visual_color(c, &vj.deck_a, 0.7f, x, y, vj.width, vj.height, vj.time);
                        break;
                        
                    case XFADE_FULL_B:
                        color_pair = get_visual_color(c, &vj.deck_b, 0.7f, x, y, vj.width, vj.height, vj.time);
                        break;
                        
                    case XFADE_MIX:
                        // Mix colors from both decks based on position and time
                        {
                            // Determine which deck's color to use based on pattern
                            bool use_deck_a;
                            
                            // Create different mixing patterns
                            int pattern = ((int)(vj.time * 0.1f)) % 4;
                            switch (pattern) {
                                case 0: // Checkerboard pattern
                                    use_deck_a = ((x + y) % 2) == 0;
                                    break;
                                case 1: // Horizontal stripes
                                    use_deck_a = (y % 4) < 2;
                                    break;
                                case 2: // Vertical stripes  
                                    use_deck_a = (x % 4) < 2;
                                    break;
                                case 3: // Time-based alternating
                                    use_deck_a = ((int)(vj.time * 2.0f + x + y)) % 2 == 0;
                                    break;
                                default:
                                    use_deck_a = true;
                                    break;
                            }
                            
                            // Get color from appropriate deck
                            if (use_deck_a) {
                                color_pair = get_visual_color(c, &vj.deck_a, 0.7f, x, y, vj.width, vj.height, vj.time);
                            } else {
                                color_pair = get_visual_color(c, &vj.deck_b, 0.7f, x, y, vj.width, vj.height, vj.time);
                            }
                        }
                        break;
                        
                    default:
                        color_pair = get_visual_color(c, &vj.deck_a, 0.7f, x, y, vj.width, vj.height, vj.time);
                        break;
                }
                
                attron(COLOR_PAIR(color_pair));
                mvaddch(y, x, c);
                attroff(COLOR_PAIR(color_pair));
                
                // Removed audio beat detection
            } else {
                mvaddch(y, x, c);
            }
        }
    }
    
    // Skip UI rendering if hidden
    if (vj.hide_ui) {
        refresh();
        return;
    }
    
    // Modern Glass-Style UI
    int ui_y = vj.height;
    const char* color_names[] = {"", "Red", "Grn", "Blu", "Yel", "Mag", "Cyn", "Wht"};
    
    // Performance metrics and system status
    static int frame_count = 0;
    static time_t last_fps_time = 0;
    static int fps = 60;
    frame_count++;
    time_t current_time = time(NULL);
    if (current_time != last_fps_time) {
        fps = frame_count;
        frame_count = 0;
        last_fps_time = current_time;
    }
    
    // Top status bar with glass-like appearance
    if (has_colors()) {
        attron(COLOR_PAIR(7) | A_REVERSE);
    } else {
        attron(A_REVERSE);
    }
    mvprintw(ui_y, 0, "+--- CLIFT v2.1 ----------------------------------------------------------+");
    mvprintw(ui_y + 1, 0, "| FPS:%3d | Speed:%.1fx | BPM:%.0f | Auto:%s | FullAuto:%s | XFade:%s    |",
             fps, vj.master_speed.value, 
             vj.bpm_system.bpm > 0 ? vj.bpm_system.bpm : 0.0f,
             vj.bpm_system.auto_crossfade_enabled ? "ON " : "OFF",
             vj.full_auto_mode ? "ON " : "OFF",
             vj.crossfade_state == XFADE_FULL_A ? " A " : 
             (vj.crossfade_state == XFADE_MIX ? "MIX" : " B "));
    
    if (has_colors()) {
        attroff(COLOR_PAIR(7) | A_REVERSE);
    } else {
        attroff(A_REVERSE);
    }
    
    // Deck status with clean visual indicators
    const char* deck_a_indicator = vj.selected_deck == 0 ? "*" : " ";
    const char* deck_b_indicator = vj.selected_deck == 1 ? "*" : " ";
    
    // Color-coded deck status
    if (has_colors()) {
        // Deck A status
        attron(COLOR_PAIR(vj.deck_a.primary_color));
        mvprintw(ui_y + 2, 0, "| DECK A");
        attroff(COLOR_PAIR(vj.deck_a.primary_color));
        
        mvprintw(ui_y + 2, 8, " %s | %02d:%-12.12s | %s>%s | %s | FX:%-8s",
                 deck_a_indicator, vj.deck_a.scene_id, 
                 scene_names[vj.deck_a.scene_id],
                 color_names[vj.deck_a.primary_color], 
                 color_names[vj.deck_a.secondary_color],
                 gradient_names[vj.deck_a.gradient_type],
                 post_effect_names[vj.deck_a.post_effect]);
        
        // Add closing border
        mvprintw(ui_y + 2, 78, " |");
        
        // Deck B status
        attron(COLOR_PAIR(vj.deck_b.primary_color));
        mvprintw(ui_y + 3, 0, "| DECK B");
        attroff(COLOR_PAIR(vj.deck_b.primary_color));
        
        mvprintw(ui_y + 3, 8, " %s | %02d:%-12.12s | %s>%s | %s | FX:%-8s",
                 deck_b_indicator, vj.deck_b.scene_id,
                 scene_names[vj.deck_b.scene_id],
                 color_names[vj.deck_b.primary_color], 
                 color_names[vj.deck_b.secondary_color],
                 gradient_names[vj.deck_b.gradient_type],
                 post_effect_names[vj.deck_b.post_effect]);
        
        // Add closing border
        mvprintw(ui_y + 3, 78, " |");
    } else {
        // Fallback for no color
        mvprintw(ui_y + 2, 0, "| DECK A %s | %02d:%-12.12s | %s>%s | %s | FX:%-8s |",
                 deck_a_indicator, vj.deck_a.scene_id, 
                 scene_names[vj.deck_a.scene_id],
                 color_names[vj.deck_a.primary_color], 
                 color_names[vj.deck_a.secondary_color],
                 gradient_names[vj.deck_a.gradient_type],
                 post_effect_names[vj.deck_a.post_effect]);
        mvprintw(ui_y + 3, 0, "| DECK B %s | %02d:%-12.12s | %s>%s | %s | FX:%-8s |",
                 deck_b_indicator, vj.deck_b.scene_id,
                 scene_names[vj.deck_b.scene_id],
                 color_names[vj.deck_b.primary_color], 
                 color_names[vj.deck_b.secondary_color],
                 gradient_names[vj.deck_b.gradient_type],
                 post_effect_names[vj.deck_b.post_effect]);
    }
    
    // Simple 3-state crossfader with BPM display
    mvprintw(ui_y + 4, 0, "+------------------------------------------------------------------------+");
    
    const char* xfade_states[] = { "FULL-A", "MIX", "FULL-B" };
    const char* auto_status = vj.bpm_system.auto_crossfade_enabled ? "AUTO" : "MANUAL";
    
    mvprintw(ui_y + 5, 0, "| XFADE: %-6s | BPM: %6.1f | Mode: %s | Interval: %.0f beats       |",
             xfade_states[vj.crossfade_state], 
             vj.bpm_system.bpm,
             auto_status,
             vj.bpm_system.crossfade_beat_interval);
    
    // Paginated UI system
    const char* page_names[] = {"Performance", "Presets", "Settings", "Monitor", "Help", "Link", "Audio"};
    
    // Page header with navigation
    mvprintw(ui_y + 6, 0, "+--- %s Page (%d/%d) | Tab=Next | Shift+Tab=Prev | Q=Quit --------------+",
             page_names[vj.current_ui_page], vj.current_ui_page + 1, UI_PAGE_COUNT);
    
    // Page-specific content
    switch (vj.current_ui_page) {
        case UI_PAGE_PERFORMANCE:
        {
            const char* category_names[] = {"Basic", "Geometric", "Organic", "Text/Code", "Abstract", "Tunnels", "Nature", "Explosions", "Cities", "Freestyle", "Human", "Warfare", "Revolution&Eyes", "Film Noir", "Escher 3D", "Ikeda", "Giger", "Revolt", "Audio React"};
            int current_scene_id = vj.selected_deck == 0 ? vj.deck_a.scene_id : vj.deck_b.scene_id;
            int current_category = current_scene_id / 10;
            
            mvprintw(ui_y + 7, 0, "| %s (%d0-%d9) | Active: DECK %c | Scene: %02d-%-15.15s        |", 
                     category_names[current_category], 
                     current_category, current_category,
                     vj.selected_deck == 0 ? 'A' : 'B',
                     current_scene_id,
                     scene_names[current_scene_id]);
            mvprintw(ui_y + 8, 0, "| A/B=Deck | 0-9=Scene | PgUp/Dn=Category | V/N=Colors | G=Grad | T=TapBPM |");
            mvprintw(ui_y + 9, 0, "| X/Z/C/M=XFade | Left/Right=Interval±4 | [/]=Interval±1 | F=FullAuto      |");
            break;
        }
        
        case UI_PAGE_PRESETS:
        {
            mvprintw(ui_y + 7, 0, "| PRESETS | Selected: %02d | Page: %d/4 | S=Save L=Load D=Del N=Name PgUp/Dn=Page |",
                     vj.selected_preset, vj.preset_page + 1);
            
            // Show 5 presets per page
            int start_preset = vj.preset_page * 5;
            for (int i = 0; i < 5; i++) {
                int preset_id = start_preset + i;
                if (preset_id >= MAX_PRESETS) break;
                
                char indicator = (preset_id == vj.selected_preset) ? '*' : ' ';
                const char* status = vj.presets[preset_id].is_used ? vj.presets[preset_id].name : "Empty";
                mvprintw(ui_y + 8 + i, 0, "|%c%02d: %-30.30s                                    |", 
                         indicator, preset_id, status);
            }
            break;
        }
        
        case UI_PAGE_SETTINGS:
        {
            if (vj.performance_mode) {
                CLIFTDeck* selected_deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
                Parameter* param = &selected_deck->params[vj.selected_param];
                
                mvprintw(ui_y + 7, 0, "| PARAMETER CONTROL | Deck %c | Param %d: %.2f [%.1f-%.1f] %s         |", 
                         'A' + vj.selected_deck, vj.selected_param + 1,
                         param->value, param->min, param->max,
                         param->auto_mode ? "[AUTO]" : "[MANUAL]");
                mvprintw(ui_y + 8, 0, "| Controls: UP/DN=Parameter | <->=Value | Enter=Auto | R=Reset | P=Exit |");
            } else {
                mvprintw(ui_y + 7, 0, "| SETTINGS & PARAMETERS | Press P to enter Parameter Control Mode         |");
                mvprintw(ui_y + 8, 0, "| Master Speed: %.1fx | BPM: %.0f | Auto Interval: %.0f beats            |",
                         vj.master_speed.value, vj.bpm_system.bpm, vj.bpm_system.crossfade_beat_interval);
            }
            break;
        }
        
        case UI_PAGE_MONITOR:
        {
            const char* ws_status = vj.live_coding.enabled ? "ENABLED" : "DISABLED";
            const char* overlay_status = vj.live_coding.display_overlay ? "ON" : "OFF";
            
            mvprintw(ui_y + 7, 0, "| LIVE CODING | WebSocket: %s | Port: %d | Overlay: %s               |",
                     ws_status, vj.live_coding.port, overlay_status);
            
            mvprintw(ui_y + 8, 0, "| %s: %s | %s: %s |",
                     vj.live_coding.players[0].player_name,
                     vj.live_coding.players[0].is_active ? "ACTIVE  " : "inactive",
                     vj.live_coding.players[1].player_name,
                     vj.live_coding.players[1].is_active ? "ACTIVE  " : "inactive");
                     
            mvprintw(ui_y + 9, 0, "| CPU: %.1f%% | Memory: %.1f%% | BPM: %.1f | W=Toggle WS | O=Toggle Overlay  |",
                     vj.live_coding.cpu_usage, vj.live_coding.memory_usage, vj.bpm_system.bpm);
            
            // Show live coding input areas when websocket is enabled
            if (vj.live_coding.enabled) {
                mvprintw(ui_y + 10, 0, "+==============================================================================+");
                mvprintw(ui_y + 11, 0, "| LIVE CODING INPUT AREAS                                                      |");
                mvprintw(ui_y + 12, 0, "+==============================================================================+");
                
                // Player 1 code area
                attron(vj.live_coding.players[0].is_active ? COLOR_PAIR(2) : COLOR_PAIR(3));
                mvprintw(ui_y + 13, 0, "| %s [%s]", 
                         vj.live_coding.players[0].player_name,
                         vj.live_coding.players[0].is_active ? "ACTIVE" : "idle  ");
                attroff(COLOR_PAIR(2) | COLOR_PAIR(3));
                
                // Split code into lines and display (max 3 lines)
                char code_copy[512];
                strncpy(code_copy, vj.live_coding.players[0].current_code, sizeof(code_copy) - 1);
                code_copy[sizeof(code_copy) - 1] = '\0';
                
                char* line = strtok(code_copy, "\n");
                int line_count = 0;
                while (line && line_count < 3) {
                    mvprintw(ui_y + 14 + line_count, 0, "| %.74s |", line);
                    line = strtok(NULL, "\n");
                    line_count++;
                }
                
                // Fill remaining lines if needed
                for (int i = line_count; i < 3; i++) {
                    mvprintw(ui_y + 14 + i, 0, "|                                                                          |");
                }
                
                mvprintw(ui_y + 17, 0, "| Last: %.66s |", vj.live_coding.players[0].last_executed);
                mvprintw(ui_y + 18, 0, "+------------------------------------------------------------------------+");
                
                // Player 2 code area
                attron(vj.live_coding.players[1].is_active ? COLOR_PAIR(2) : COLOR_PAIR(3));
                mvprintw(ui_y + 19, 0, "| %s [%s]", 
                         vj.live_coding.players[1].player_name,
                         vj.live_coding.players[1].is_active ? "ACTIVE" : "idle  ");
                attroff(COLOR_PAIR(2) | COLOR_PAIR(3));
                
                // Split Player 2 code into lines
                strncpy(code_copy, vj.live_coding.players[1].current_code, sizeof(code_copy) - 1);
                code_copy[sizeof(code_copy) - 1] = '\0';
                
                line = strtok(code_copy, "\n");
                line_count = 0;
                while (line && line_count < 3) {
                    mvprintw(ui_y + 20 + line_count, 0, "| %.74s |", line);
                    line = strtok(NULL, "\n");
                    line_count++;
                }
                
                // Fill remaining lines if needed
                for (int i = line_count; i < 3; i++) {
                    mvprintw(ui_y + 20 + i, 0, "|                                                                          |");
                }
                
                mvprintw(ui_y + 23, 0, "| Last: %.66s |", vj.live_coding.players[1].last_executed);
                mvprintw(ui_y + 24, 0, "+==============================================================================+");
            }
            break;
        }
        
        case UI_PAGE_HELP:
        {
            mvprintw(ui_y + 7, 0, "| HELP | A/B=Deck | 0-9=Scene | PgUp/Dn=Category | V/N=Colors | G=Grad   |");
            mvprintw(ui_y + 8, 0, "| E=Effects | X/Z/C/M=XFade | T=TapBPM | F=FullAuto | U=HideUI | Q=Quit  |");
            mvprintw(ui_y + 9, 0, "| Tab=NextPage | S/L/D=Save/Load/Delete Preset | P=Parameter Mode       |");
            break;
        }
        
        case UI_PAGE_LINK:
        {
            mvprintw(ui_y + 7, 0, "| ABLETON LINK | Status: %s | Peers: %d | BPM: %.1f | Beat: %.1f %s |",
                     vj.link.enabled ? (vj.link.connected ? "Connected" : "Enabled") : "Disabled",
                     vj.link.num_peers,
                     vj.link.enabled ? vj.link.link_bpm : vj.bpm_system.bpm,
                     vj.link.link_beat,
                     vj.link.is_playing ? "[PLAY]" : "[STOP]");
            mvprintw(ui_y + 8, 0, "| L=Toggle Link | K=Sync to Link BPM | J=Start/Stop Sync | Quantum: %.0f |",
                     vj.link.quantum);
            mvprintw(ui_y + 9, 0, "| Phase: %.2f | StartStop: %s | Space=Play/Stop when sync enabled        |",
                     vj.link.link_phase,
                     vj.link.start_stop_sync ? "ON" : "OFF");
            break;
        }
        
        case UI_PAGE_AUDIO:
        {
            // Get connection info
            char conn_info[256] = "No connections";
            int conn_count = 0;
            if (vj.audio_enabled) {
                conn_count = audio_count_connections();
                if (conn_count > 0) {
                    audio_get_connection_info(conn_info, sizeof(conn_info));
                }
            }
            
            // Audio visualization using spectrum data
            mvprintw(ui_y + 7, 0, "| AUDIO INPUT | Status: %s | Gain: %.1f | Smooth: %.1f | %s",
                     vj.audio_enabled ? "ACTIVE" : "INACTIVE",
                     vj.audio_gain,
                     vj.audio_smoothing,
                     conn_info);
            
            // Show frequency spectrum
            mvprintw(ui_y + 8, 0, "| ");
            if (vj.audio_enabled && vj.audio_data.valid) {
                pthread_mutex_lock(&vj.audio_mutex);
                
                // Draw mini spectrum analyzer (64 bands compressed to fit)
                for (int i = 0; i < 32; i++) {
                    float val1 = vj.audio_data.spectrum[i * 2];
                    float val2 = vj.audio_data.spectrum[i * 2 + 1];
                    float val = (val1 + val2) * 0.5f;
                    
                    // Convert to bar height (0-8)
                    int height = (int)(val * 8.0f);
                    char bar = ' ';
                    if (height > 0) {
                        const char bars[] = "_▁▂▃▄▅▆▇█";
                        bar = bars[height < 9 ? height : 8];
                    }
                    printw("%c%c", bar, bar);
                }
                
                pthread_mutex_unlock(&vj.audio_mutex);
            } else {
                printw("Audio Disabled - Press 'i' to enable audio input");
            }
            printw(" |");
            
            // Audio levels and controls
            if (vj.audio_enabled && vj.audio_data.valid) {
                pthread_mutex_lock(&vj.audio_mutex);
                
                // Convert levels to 1-4 scale visual representation
                char bass_bar[7], mid_bar[7], treble_bar[7], vol_bar[7];
                
                // Helper to convert level to bar representation
                const char* bars[] = {"[....]", "[x...]", "[xx..]", "[xxx.]", "[xxxx]"};
                int bass_idx = (int)(vj.audio_data.bass * 4.0f);
                int mid_idx = (int)(vj.audio_data.mid * 4.0f);
                int treble_idx = (int)(vj.audio_data.treble * 4.0f);
                int vol_idx = (int)(vj.audio_data.volume * 4.0f);
                
                // Clamp to valid range
                bass_idx = bass_idx > 4 ? 4 : bass_idx;
                mid_idx = mid_idx > 4 ? 4 : mid_idx;
                treble_idx = treble_idx > 4 ? 4 : treble_idx;
                vol_idx = vol_idx > 4 ? 4 : vol_idx;
                
                strcpy(bass_bar, bars[bass_idx]);
                strcpy(mid_bar, bars[mid_idx]);
                strcpy(treble_bar, bars[treble_idx]);
                strcpy(vol_bar, bars[vol_idx]);
                
                mvprintw(ui_y + 9, 0, "| B:%s M:%s T:%s V:%s | %s | A=Mon S=Src D=Disc I=Off |",
                         bass_bar,
                         mid_bar,
                         treble_bar,
                         vol_bar,
                         vj.audio_data.beat_detected ? "BEAT!" : "     ");
                pthread_mutex_unlock(&vj.audio_mutex);
            } else {
                mvprintw(ui_y + 9, 0, "| I=Enable Audio | A=Monitor S=Select Source | +/-=Gain | [/]=Smoothing |");
            }
            break;
        }
        
        default:
            // Should not happen, but handle gracefully
            mvprintw(ui_y + 7, 0, "| UNKNOWN PAGE | Press Tab to navigate to a valid page                   |");
            break;
    }
    
    // Bottom border (adjust based on page content)
    int last_line = ui_y + 9;
    if (vj.current_ui_page == UI_PAGE_PRESETS) last_line = ui_y + 12;
    mvprintw(last_line, 0, "+------------------------------------------------------------------------+");
    
    refresh();
}

// ============= PRESET MANAGEMENT =============

void init_presets() {
    for (int i = 0; i < MAX_PRESETS; i++) {
        vj.presets[i].is_used = false;
        snprintf(vj.presets[i].name, sizeof(vj.presets[i].name), "Empty");
    }
    vj.selected_preset = 0;
    vj.preset_page = 0;
}

void save_preset(int preset_id, const char* name) {
    if (preset_id < 0 || preset_id >= MAX_PRESETS) return;
    
    Preset* preset = &vj.presets[preset_id];
    
    // Save current state to preset
    strncpy(preset->name, name, sizeof(preset->name) - 1);
    preset->name[sizeof(preset->name) - 1] = '\0';
    
    preset->deck_a_scene = vj.deck_a.scene_id;
    preset->deck_b_scene = vj.deck_b.scene_id;
    preset->deck_a_primary_color = vj.deck_a.primary_color;
    preset->deck_a_secondary_color = vj.deck_a.secondary_color;
    preset->deck_b_primary_color = vj.deck_b.primary_color;
    preset->deck_b_secondary_color = vj.deck_b.secondary_color;
    preset->deck_a_gradient = vj.deck_a.gradient_type;
    preset->deck_b_gradient = vj.deck_b.gradient_type;
    preset->deck_a_effect = vj.deck_a.post_effect;
    preset->deck_b_effect = vj.deck_b.post_effect;
    preset->crossfade_state = vj.crossfade_state;
    
    // Save parameter values
    for (int i = 0; i < 8; i++) {
        preset->deck_a_params[i] = vj.deck_a.params[i].value;
        preset->deck_b_params[i] = vj.deck_b.params[i].value;
    }
    
    preset->is_used = true;
}

void load_preset(int preset_id) {
    if (preset_id < 0 || preset_id >= MAX_PRESETS) return;
    if (!vj.presets[preset_id].is_used) return;
    
    Preset* preset = &vj.presets[preset_id];
    
    // Load preset state
    vj.deck_a.scene_id = preset->deck_a_scene;
    vj.deck_b.scene_id = preset->deck_b_scene;
    vj.deck_a.primary_color = preset->deck_a_primary_color;
    vj.deck_a.secondary_color = preset->deck_a_secondary_color;
    vj.deck_b.primary_color = preset->deck_b_primary_color;
    vj.deck_b.secondary_color = preset->deck_b_secondary_color;
    vj.deck_a.gradient_type = preset->deck_a_gradient;
    vj.deck_b.gradient_type = preset->deck_b_gradient;
    vj.deck_a.post_effect = preset->deck_a_effect;
    vj.deck_b.post_effect = preset->deck_b_effect;
    vj.crossfade_state = preset->crossfade_state;
    
    // Load parameter values
    for (int i = 0; i < 8; i++) {
        vj.deck_a.params[i].value = preset->deck_a_params[i];
        vj.deck_b.params[i].value = preset->deck_b_params[i];
    }
}

void clear_preset(int preset_id) {
    if (preset_id < 0 || preset_id >= MAX_PRESETS) return;
    vj.presets[preset_id].is_used = false;
    snprintf(vj.presets[preset_id].name, sizeof(vj.presets[preset_id].name), "Empty");
}

void cycle_preset_name(int preset_id) {
    if (preset_id < 0 || preset_id >= MAX_PRESETS) return;
    if (!vj.presets[preset_id].is_used) return;  // Can only name used presets
    
    // Predefined preset names to cycle through
    const char* preset_names[] = {
        "Main Mix",
        "Breakdowns", 
        "Build Up",
        "Drop Zone",
        "Chill Vibes",
        "Psychedelic",
        "Hard Hitting",
        "Ambient Flow",
        "Peak Time",
        "Warm Up",
        "Cool Down",
        "Techno Storm",
        "Deep House",
        "Trance Journey",
        "Bass Heavy",
        "Experimental",
        "Retro Wave",
        "Cyber Punk",
        "Liquid Flow",
        "Energy Boost",
        "Neon Dreams",
        "Space Odyssey",
        "Crystal Cave",
        "Digital Rain",
        "Fire Storm",
        "Ocean Deep",
        "Electric Maze",
        "Quantum Leap",
        "Laser Show",
        "Midnight Hour",
        "Solar Flare",
        "Ice Palace",
        "Thunder Road",
        "Purple Haze",
        "Golden Hour",
        "Red Alert",
        "Blue Moon",
        "Green Machine",
        "White Noise",
        "Black Hole",
        "Silver Screen",
        "Neon Noir",
        "Acid Trip",
        "Vinyl Vibes",
        "Glitch City",
        "Pixel Storm",
        "Data Stream",
        "Code Matrix",
        "Byte Dance",
        "Algo Rhythm"
    };
    
    int num_names = sizeof(preset_names) / sizeof(preset_names[0]);
    
    // Find current name index or start at 0
    int current_index = 0;
    for (int i = 0; i < num_names; i++) {
        if (strcmp(vj.presets[preset_id].name, preset_names[i]) == 0) {
            current_index = i;
            break;
        }
    }
    
    // Cycle to next name
    int next_index = (current_index + 1) % num_names;
    strncpy(vj.presets[preset_id].name, preset_names[next_index], sizeof(vj.presets[preset_id].name) - 1);
    vj.presets[preset_id].name[sizeof(vj.presets[preset_id].name) - 1] = '\0';
}

// ============= LIVE CODING MONITOR =============

void init_live_coding_monitor() {
    vj.live_coding.enabled = false;
    vj.live_coding.port = 8080;
    vj.live_coding.display_overlay = false;
    vj.live_coding.overlay_opacity = 80;
    vj.live_coding.cpu_usage = 0.0f;
    vj.live_coding.memory_usage = 0.0f;
    
    // Initialize websocket server internals
    vj.live_coding.server_socket = -1;
    vj.live_coding.server_running = false;
    vj.live_coding.client_count = 0;
    for (int i = 0; i < 8; i++) {
        vj.live_coding.client_sockets[i] = -1;
    }
    
    // Initialize players
    for (int i = 0; i < 2; i++) {
        snprintf(vj.live_coding.players[i].player_name, sizeof(vj.live_coding.players[i].player_name), "Player %d", i + 1);
        strcpy(vj.live_coding.players[i].current_code, "// Waiting for connection...");
        strcpy(vj.live_coding.players[i].last_executed, "");
        vj.live_coding.players[i].is_active = false;
        vj.live_coding.players[i].last_update_time = 0.0f;
    }
}

void update_cpu_usage() {
    // Simple CPU usage estimation based on rendering performance
    static float last_time = 0.0f;
    static int frame_counter = 0;
    float current_time = vj.time;
    
    frame_counter++;
    if (current_time - last_time >= 1.0f) {
        // Estimate CPU usage based on frame rate
        float fps = frame_counter / (current_time - last_time);
        vj.live_coding.cpu_usage = (60.0f - fps) / 60.0f * 100.0f;
        if (vj.live_coding.cpu_usage < 0) vj.live_coding.cpu_usage = 0;
        if (vj.live_coding.cpu_usage > 100) vj.live_coding.cpu_usage = 100;
        
        // Simple memory usage simulation
        vj.live_coding.memory_usage = 45.0f + sinf(current_time * 0.1f) * 15.0f;
        
        frame_counter = 0;
        last_time = current_time;
    }
}

void parse_websocket_message(const char* message) {
    // Debug check
    if (!message) {
        fprintf(stderr, "ERROR: parse_websocket_message called with NULL message\n");
        return;
    }
    
    // Simple JSON-like parsing for live coding data
    // Expected format: {"player":0,"code":"synth :saw","executed":"play :kick","active":true}
    
    if (strstr(message, "\"player\":0") || strstr(message, "\"player\": 0")) {
        // Player 1 data
        LiveCodingPlayer* player = &vj.live_coding.players[0];
        
        // Extract code
        char* code_start = strstr(message, "\"code\":\"");
        if (code_start) {
            code_start += 8; // Skip "code":"
            char* code_end = strchr(code_start, '"');
            if (code_end) {
                int len = code_end - code_start;
                if (len < sizeof(player->current_code) - 1) {
                    strncpy(player->current_code, code_start, len);
                    player->current_code[len] = '\0';
                }
            }
        }
        
        // Extract executed line
        char* exec_start = strstr(message, "\"executed\":\"");
        if (exec_start) {
            exec_start += 12; // Skip "executed":"
            char* exec_end = strchr(exec_start, '"');
            if (exec_end) {
                int len = exec_end - exec_start;
                if (len < sizeof(player->last_executed) - 1) {
                    strncpy(player->last_executed, exec_start, len);
                    player->last_executed[len] = '\0';
                }
            }
        }
        
        // Check if active
        player->is_active = strstr(message, "\"active\":true") != NULL;
        player->last_update_time = vj.time;
        
    } else if (strstr(message, "\"player\":1") || strstr(message, "\"player\": 1")) {
        // Player 2 data - similar parsing
        LiveCodingPlayer* player = &vj.live_coding.players[1];
        
        char* code_start = strstr(message, "\"code\":\"");
        if (code_start) {
            code_start += 8;
            char* code_end = strchr(code_start, '"');
            if (code_end) {
                int len = code_end - code_start;
                if (len < sizeof(player->current_code) - 1) {
                    strncpy(player->current_code, code_start, len);
                    player->current_code[len] = '\0';
                }
            }
        }
        
        char* exec_start = strstr(message, "\"executed\":\"");
        if (exec_start) {
            exec_start += 12;
            char* exec_end = strchr(exec_start, '"');
            if (exec_end) {
                int len = exec_end - exec_start;
                if (len < sizeof(player->last_executed) - 1) {
                    strncpy(player->last_executed, exec_start, len);
                    player->last_executed[len] = '\0';
                }
            }
        }
        
        player->is_active = strstr(message, "\"active\":true") != NULL;
        player->last_update_time = vj.time;
    }
}

void render_live_coding_overlay() {
    if (!vj.live_coding.display_overlay) return;
    
    // Debug: Check for valid buffer
    if (!vj.output_buffer) {
        fprintf(stderr, "ERROR: render_live_coding_overlay called with NULL output_buffer\n");
        return;
    }
    
    // Compact overlay - just 2 lines at bottom of screen with black background
    int start_y = vj.height - 4;  // Position near bottom
    int overlay_width = vj.width;
    
    // Player 1 line with black background
    if (start_y >= 0 && start_y < vj.height) {
        // Clear the line with spaces (black background)
        for (int x = 0; x < overlay_width && x < vj.width; x++) {
            vj.output_buffer[start_y * vj.width + x] = ' ';
        }
        
        // Format Player 1 info - get first line of code only
        char p1_line[256];
        char code_first_line[128];
        strncpy(code_first_line, vj.live_coding.players[0].current_code, sizeof(code_first_line) - 1);
        code_first_line[sizeof(code_first_line) - 1] = '\0';
        
        // Remove newlines from the code to show just first line
        char* newline = strchr(code_first_line, '\n');
        if (newline) *newline = '\0';
        
        snprintf(p1_line, sizeof(p1_line), "P1: %.60s", code_first_line);
        
        // Write Player 1 line
        for (int x = 0; x < strlen(p1_line) && x < vj.width; x++) {
            vj.output_buffer[start_y * vj.width + x] = p1_line[x];
        }
    }
    
    // Player 2 line with black background
    if (start_y + 1 >= 0 && start_y + 1 < vj.height) {
        // Clear the line with spaces (black background)
        for (int x = 0; x < overlay_width && x < vj.width; x++) {
            vj.output_buffer[(start_y + 1) * vj.width + x] = ' ';
        }
        
        // Format Player 2 info - get first line of code only
        char p2_line[256];
        char code_first_line[128];
        strncpy(code_first_line, vj.live_coding.players[1].current_code, sizeof(code_first_line) - 1);
        code_first_line[sizeof(code_first_line) - 1] = '\0';
        
        // Remove newlines from the code to show just first line
        char* newline = strchr(code_first_line, '\n');
        if (newline) *newline = '\0';
        
        snprintf(p2_line, sizeof(p2_line), "P2: %.60s", code_first_line);
        
        // Write Player 2 line
        for (int x = 0; x < strlen(p2_line) && x < vj.width; x++) {
            vj.output_buffer[(start_y + 1) * vj.width + x] = p2_line[x];
        }
    }
}

// Websocket Server Implementation
void* websocket_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    fd_set read_fds;
    int max_fd;
    struct timeval timeout;
    
    // Create socket
    vj.live_coding.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (vj.live_coding.server_socket < 0) {
        vj.live_coding.server_running = false;
        return NULL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(vj.live_coding.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(vj.live_coding.server_socket, F_SETFL, O_NONBLOCK);
    
    // Bind socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(vj.live_coding.port);
    
    if (bind(vj.live_coding.server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(vj.live_coding.server_socket);
        vj.live_coding.server_socket = -1;
        vj.live_coding.server_running = false;
        return NULL;
    }
    
    // Listen
    if (listen(vj.live_coding.server_socket, 8) < 0) {
        close(vj.live_coding.server_socket);
        vj.live_coding.server_socket = -1;
        vj.live_coding.server_running = false;
        return NULL;
    }
    
    vj.live_coding.server_running = true;
    
    // Main server loop
    while (vj.live_coding.server_running) {
        FD_ZERO(&read_fds);
        FD_SET(vj.live_coding.server_socket, &read_fds);
        max_fd = vj.live_coding.server_socket;
        
        // Add client sockets to fd_set
        for (int i = 0; i < vj.live_coding.client_count; i++) {
            if (vj.live_coding.client_sockets[i] > 0) {
                FD_SET(vj.live_coding.client_sockets[i], &read_fds);
                if (vj.live_coding.client_sockets[i] > max_fd) {
                    max_fd = vj.live_coding.client_sockets[i];
                }
            }
        }
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0 && errno != EINTR) break;
        if (!vj.live_coding.server_running) break;
        
        // New connection
        if (FD_ISSET(vj.live_coding.server_socket, &read_fds)) {
            int client_socket = accept(vj.live_coding.server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket >= 0 && vj.live_coding.client_count < 8) {
                fcntl(client_socket, F_SETFL, O_NONBLOCK);
                int client_index = vj.live_coding.client_count++;
                vj.live_coding.client_sockets[client_index] = client_socket;
                vj.live_coding.client_handshake_done[client_index] = false;
                // Don't call handshake immediately - wait for data to arrive
            }
        }
        
        // Handle client messages
        for (int i = 0; i < vj.live_coding.client_count; i++) {
            int client_socket = vj.live_coding.client_sockets[i];
            if (client_socket > 0 && FD_ISSET(client_socket, &read_fds)) {
                if (!vj.live_coding.client_handshake_done[i]) {
                    // Handle WebSocket handshake
                    handle_websocket_handshake(client_socket);
                    vj.live_coding.client_handshake_done[i] = true;
                } else {
                    // Handle WebSocket frames
                    unsigned char buffer[4096];
                    ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                    if (bytes <= 0) {
                        // Client disconnected
                        close(client_socket);
                        vj.live_coding.client_sockets[i] = vj.live_coding.client_sockets[--vj.live_coding.client_count];
                        vj.live_coding.client_handshake_done[i] = vj.live_coding.client_handshake_done[vj.live_coding.client_count];
                        i--; // Recheck this index
                    } else {
                        buffer[bytes] = '\0';
                        process_websocket_frame(client_socket, buffer, bytes);
                    }
                }
            }
        }
    }
    
    // Cleanup
    for (int i = 0; i < vj.live_coding.client_count; i++) {
        if (vj.live_coding.client_sockets[i] > 0) {
            close(vj.live_coding.client_sockets[i]);
        }
    }
    close(vj.live_coding.server_socket);
    vj.live_coding.server_socket = -1;
    vj.live_coding.client_count = 0;
    vj.live_coding.server_running = false;
    
    return NULL;
}

void start_websocket_server() {
    if (vj.live_coding.server_running) return;
    
    if (pthread_create(&vj.live_coding.server_thread, NULL, websocket_server_thread, NULL) != 0) {
        vj.live_coding.enabled = false;
    }
}

void stop_websocket_server() {
    if (!vj.live_coding.server_running) return;
    
    vj.live_coding.server_running = false;
    pthread_join(vj.live_coding.server_thread, NULL);
}

// Base64 encoding function
char* base64_encode(const unsigned char* input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    
    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);
    
    char* result = malloc(bptr->length + 1);
    memcpy(result, bptr->data, bptr->length);
    result[bptr->length] = '\0';
    
    BIO_free_all(b64);
    return result;
}

// Calculate WebSocket accept key according to RFC 6455
char* calculate_websocket_accept(const char* key) {
    const char* magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, magic);
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)combined, strlen(combined), hash);
    
    return base64_encode(hash, SHA_DIGEST_LENGTH);
}

void handle_websocket_handshake(int client_socket) {
    char buffer[4096];
    ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    if (bytes <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Socket would block, try again later
            return;
        }
        close(client_socket);
        return;
    }
    
    buffer[bytes] = '\0';
    
    // Simple WebSocket handshake - look for the key
    char* key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) {
        close(client_socket);
        return;
    }
    
    key_start += 19; // Skip "Sec-WebSocket-Key: "
    char* key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        close(client_socket);
        return;
    }
    
    // Extract the key
    size_t key_len = key_end - key_start;
    char key[128];
    strncpy(key, key_start, key_len);
    key[key_len] = '\0';
    
    // Calculate proper WebSocket accept key
    char* accept_key = calculate_websocket_accept(key);
    if (!accept_key) {
        close(client_socket);
        return;
    }
    
    // Send proper handshake response
    char response[512];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_key);
    
    send(client_socket, response, strlen(response), 0);
    free(accept_key);
}

void process_websocket_frame(int client_socket, unsigned char* frame, size_t len) {
    // Debug check
    if (!frame) {
        fprintf(stderr, "ERROR: process_websocket_frame called with NULL frame\n");
        return;
    }
    
    // Simple frame processing - in a full implementation you'd handle masking, opcodes, etc.
    // For now, we'll assume the payload starts after a minimal frame header
    
    if (len < 2) return;
    
    // Skip minimal WebSocket frame header (this is simplified)
    unsigned char* payload = frame + 2;
    size_t payload_len = len - 2;
    
    // If frame is masked (which client frames should be), skip mask
    if (frame[1] & 0x80) {
        if (payload_len < 4) return;
        payload += 4;
        payload_len -= 4;
        
        // Unmask payload (XOR with 4-byte mask)
        for (size_t i = 0; i < payload_len; i++) {
            payload[i] ^= frame[2 + (i % 4)];
        }
    }
    
    // Null-terminate payload and parse as message
    if (payload_len > 0 && payload_len < 4000) {
        char message[4000];
        memcpy(message, payload, payload_len);
        message[payload_len] = '\0';
        parse_websocket_message(message);
    }
}

void vj_handle_input() {
    int ch = getch();
    if (ch == ERR) return;
    
    switch (ch) {
        case 'q': case 'Q':
            running = false;
            break;
            
        // Deck selection with visual feedback / Audio connection
        case 'a': case 'A':
            if (vj.current_ui_page == UI_PAGE_AUDIO && vj.audio_enabled) {
                // Connect to system monitor
                bool success = audio_connect_to_monitor();
                if (!success) {
                    // Connection failed - we'll show this in the UI status
                    // The UI will update automatically to show "No connections"
                }
            } else {
                // Normal deck selection
                vj.selected_deck = 0;
                vj.deck_a.selected = true;
                vj.deck_b.selected = false;
            }
            break;
        case 'b': case 'B':
            vj.selected_deck = 1;
            vj.deck_a.selected = false;
            vj.deck_b.selected = true;
            break;
            
        // Space bar - Switch decks or control Link play/stop
        case ' ':
            if (vj.current_ui_page == UI_PAGE_LINK && vj.link.start_stop_sync && vj.link.link_handle) {
                // Toggle play/stop in Link when start/stop sync is enabled
                vj.link.is_playing = !vj.link.is_playing;
                link_set_is_playing(vj.link.link_handle, vj.link.is_playing ? 1 : 0);
            } else {
                // Normal deck switching behavior
                vj.selected_deck = 1 - vj.selected_deck;
                vj.deck_a.selected = (vj.selected_deck == 0);
                vj.deck_b.selected = (vj.selected_deck == 1);
            }
            break;
            
        // Scene selection (0-9) within current category
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': {
            int scene_number = ch - '0';
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            
            // Get current category (0-9) and keep it, just change the scene number within that category
            int current_category = deck->scene_id / 10;
            int new_scene_id = current_category * 10 + scene_number;
            
            // Ensure the new scene ID is valid (0-169)
            if (new_scene_id >= 0 && new_scene_id <= 189) {
                deck->scene_id = new_scene_id;
            }
            break;
        }
        
        // Direct post effect selection (Shift+numbers)
        case ')': {  // Shift+0 - None
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_NONE;
            break;
        }
        case '!': {  // Shift+1 - Glow
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_GLOW;
            break;
        }
        case '@': {  // Shift+2 - Blur
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_BLUR;
            break;
        }
        case '#': {  // Shift+3 - Edge
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_EDGE;
            break;
        }
        case '$': {  // Shift+4 - Invert
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_INVERT;
            break;
        }
        case '%': {  // Shift+5 - ASCII Gradient
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_ASCII_GRADIENT;
            break;
        }
        case '^': {  // Shift+6 - Scanlines
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_SCANLINES;
            break;
        }
        case '&': {  // Shift+7 - Chromatic
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_CHROMATIC;
            break;
        }
        case '*': {  // Shift+8 - Wave Warp
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_WAVE_WARP;
            break;
        }
        case '(': {  // Shift+9 - Character Emission
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = POST_CHAR_EMISSION;
            break;
        }
        
        // Live coding monitor controls
        case 'w': case 'W':
            if (vj.current_ui_page == UI_PAGE_MONITOR) {
                // Toggle websocket server
                vj.live_coding.enabled = !vj.live_coding.enabled;
                if (vj.live_coding.enabled) {
                    start_websocket_server();
                } else {
                    stop_websocket_server();
                }
            }
            break;
        case 'o': case 'O':
            if (vj.current_ui_page == UI_PAGE_MONITOR) {
                // Toggle overlay display
                vj.live_coding.display_overlay = !vj.live_coding.display_overlay;
            }
            break;
        
        // Post effect cycling
        case 'e': case 'E': {
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->post_effect = (deck->post_effect + 1) % POST_COUNT;
            break;
        }
        
        // Crossfader controls (3-state)
        case 'x': case 'X':
            // Cycle through crossfade states
            vj.crossfade_state = (vj.crossfade_state + 1) % 3;
            break;
        case 'z': case 'Z':
            vj.crossfade_state = XFADE_FULL_A;  // Full A
            break;
        case 'c': case 'C':
            vj.crossfade_state = XFADE_FULL_B;  // Full B
            break;
        case 'm': case 'M':
            vj.crossfade_state = XFADE_MIX;     // Mix both
            break;
            
        // BPM controls
        case 't': case 'T':
            // Tap BPM
            tap_bpm(vj.time);
            break;
        case 'r': case 'R':
            if (vj.performance_mode) {
                // Reset parameter in performance mode
                CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
                Parameter* param = &deck->params[vj.selected_param];
                param->target = (param->min + param->max) * 0.5f;
            } else {
                // Toggle auto crossfade when not in performance mode
                vj.bpm_system.auto_crossfade_enabled = !vj.bpm_system.auto_crossfade_enabled;
                if (vj.bpm_system.auto_crossfade_enabled) {
                    vj.bpm_system.last_crossfade_time = vj.time;
                }
            }
            break;
            
        // Color controls for selected deck
        case 'v': case 'V': {
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->primary_color = (deck->primary_color % 7) + 1; // Cycle 1-7
            break;
        }
        case 'n': case 'N': {
            if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Cycle through preset names for the selected preset
                cycle_preset_name(vj.selected_preset);
            } else {
                // Normal color control
                CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
                deck->secondary_color = (deck->secondary_color % 7) + 1; // Cycle 1-7
            }
            break;
        }
        case 'g': case 'G': {
            CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
            deck->gradient_type = (deck->gradient_type + 1) % GRADIENT_COUNT; // Cycle through gradients
            break;
        }
        
        // Audio input controls
        case 'i': case 'I':
            if (vj.current_ui_page == UI_PAGE_AUDIO) {
                vj.audio_enabled = !vj.audio_enabled;
                if (vj.audio_enabled) {
                    start_audio_capture();
                } else {
                    stop_audio_capture();
                }
            }
            break;
            
        
        // Ableton Link controls
        case 'k': case 'K':
            if (vj.current_ui_page == UI_PAGE_LINK && vj.link.enabled) {
                // Sync local BPM to Link BPM
                vj.bpm_system.bpm = vj.link.link_bpm;
                // vj.bpm_system.beat_phase = vj.link.link_phase; // TODO: Add beat_phase to BPMSystem
            }
            break;
            
        case 'j': case 'J':
            if (vj.current_ui_page == UI_PAGE_LINK) {
                // Toggle start/stop sync
                vj.link.start_stop_sync = !vj.link.start_stop_sync;
                if (vj.link.link_handle) {
                    link_set_start_stop_sync(vj.link.link_handle, vj.link.start_stop_sync ? 1 : 0);
                }
            }
            break;
        
        // Master controls
        case '+': case '=':
            if (vj.current_ui_page == UI_PAGE_AUDIO) {
                // Adjust audio gain
                vj.audio_gain += 0.1f;
                if (vj.audio_gain > 10.0f) vj.audio_gain = 10.0f;
            } else {
                vj.master_speed.target = (vj.master_speed.target + 0.1f > vj.master_speed.max) ? 
                    vj.master_speed.max : vj.master_speed.target + 0.1f;
            }
            break;
        case '-': case '_':
            if (vj.current_ui_page == UI_PAGE_AUDIO) {
                // Adjust audio gain
                vj.audio_gain -= 0.1f;
                if (vj.audio_gain < 0.1f) vj.audio_gain = 0.1f;
            } else {
                vj.master_speed.target = (vj.master_speed.target - 0.1f < vj.master_speed.min) ? 
                    vj.master_speed.min : vj.master_speed.target - 0.1f;
            }
            break;
            
        // Performance mode
        case 'p': case 'P':
            vj.performance_mode = !vj.performance_mode;
            break;
            
        // Help toggle
        case 'h': case 'H':
            vj.show_help = !vj.show_help;
            break;
            
        // UI hiding
        case 'u': case 'U':
            vj.hide_ui = !vj.hide_ui;
            break;
            
        // Full Auto Mode
        case 'f': case 'F':
            vj.full_auto_mode = !vj.full_auto_mode;
            if (vj.full_auto_mode) {
                vj.last_auto_change = vj.time;
                vj.last_effect_change = vj.time;
                // Initialize beat-synchronized intervals
                float beat_duration = 60.0f / vj.bpm_system.bpm;
                vj.effect_change_interval = beat_duration; // Effects every beat
                vj.auto_change_interval = beat_duration * 8.0f; // Scenes every 8 beats
                
                // Randomize initial state
                srand((unsigned int)(vj.time * 1000));
                randomize_deck_scene(&vj.deck_a);
                randomize_deck_scene(&vj.deck_b);
                randomize_deck_colors(&vj.deck_a);
                randomize_deck_colors(&vj.deck_b);
                randomize_deck_post_effect(&vj.deck_a);
                randomize_deck_post_effect(&vj.deck_b);
            }
            break;
            
        // UI page navigation
        case '\t':  // Tab key - Next page
            vj.current_ui_page = (vj.current_ui_page + 1) % UI_PAGE_COUNT;
            break;
        case KEY_BTAB:  // Shift+Tab - Previous page
            vj.current_ui_page = (vj.current_ui_page - 1 + UI_PAGE_COUNT) % UI_PAGE_COUNT;
            break;
        
        // Preset management controls
        case 's': case 'S':
            if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Save current state to selected preset
                char preset_name[32];
                snprintf(preset_name, sizeof(preset_name), "Preset %02d", vj.selected_preset);
                save_preset(vj.selected_preset, preset_name);
            } else if (vj.current_ui_page == UI_PAGE_AUDIO && vj.audio_enabled) {
                // Cycle through available audio sources and connect
                AudioSource sources[50];
                int count = get_audio_sources(sources, 50);
                
                if (count > 0) {
                    // Disconnect current connections first
                    audio_disconnect_all();
                    
                    // Move to next source
                    vj.selected_audio_source = (vj.selected_audio_source + 1) % count;
                    
                    // Connect to selected source
                    audio_connect_to_source(&sources[vj.selected_audio_source]);
                }
            }
            break;
        case 'l': case 'L':
            if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Load selected preset
                load_preset(vj.selected_preset);
            } else if (vj.current_ui_page == UI_PAGE_LINK) {
                // Toggle Ableton Link
                vj.link.enabled = !vj.link.enabled;
                if (vj.link.link_handle) {
                    link_enable(vj.link.link_handle, vj.link.enabled ? 1 : 0);
                    link_set_quantum(vj.link.link_handle, vj.link.quantum);
                }
            }
            break;
        case 'd': case 'D':
            if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Delete/clear selected preset
                clear_preset(vj.selected_preset);
            } else if (vj.current_ui_page == UI_PAGE_AUDIO && vj.audio_enabled) {
                // Disconnect all audio connections
                audio_disconnect_all();
            }
            break;
            
        // Arrow key navigation - multi-purpose
        case KEY_UP:
            if (vj.performance_mode) {
                vj.selected_param = (vj.selected_param - 1 + 8) % 8;
            } else if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Navigate presets
                vj.selected_preset = (vj.selected_preset - 1 + MAX_PRESETS) % MAX_PRESETS;
                vj.preset_page = vj.selected_preset / 5;  // Update page if needed
            }
            break;
        case KEY_DOWN:
            if (vj.performance_mode) {
                vj.selected_param = (vj.selected_param + 1) % 8;
            } else if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Navigate presets
                vj.selected_preset = (vj.selected_preset + 1) % MAX_PRESETS;
                vj.preset_page = vj.selected_preset / 5;  // Update page if needed
            }
            break;
        case KEY_LEFT: {
            if (vj.performance_mode) {
                CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
                Parameter* param = &deck->params[vj.selected_param];
                param->target = clamp(param->target - 0.1f, param->min, param->max);
            } else {
                // Adjust BPM crossfade interval
                vj.bpm_system.crossfade_beat_interval = fmax(4.0f, vj.bpm_system.crossfade_beat_interval - 4.0f);
            }
            break;
        }
        case KEY_RIGHT: {
            if (vj.performance_mode) {
                CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
                Parameter* param = &deck->params[vj.selected_param];
                param->target = clamp(param->target + 0.1f, param->min, param->max);
            } else {
                // Adjust BPM crossfade interval
                vj.bpm_system.crossfade_beat_interval = fmin(64.0f, vj.bpm_system.crossfade_beat_interval + 4.0f);
            }
            break;
        }
        
        // Fine interval adjustment / Audio smoothing
        case '[':
            if (vj.current_ui_page == UI_PAGE_AUDIO) {
                vj.audio_smoothing -= 0.1f;
                if (vj.audio_smoothing < 0.0f) vj.audio_smoothing = 0.0f;
            } else {
                vj.bpm_system.crossfade_beat_interval = fmax(1.0f, vj.bpm_system.crossfade_beat_interval - 1.0f);
            }
            break;
        case ']':
            if (vj.current_ui_page == UI_PAGE_AUDIO) {
                vj.audio_smoothing += 0.1f;
                if (vj.audio_smoothing > 0.95f) vj.audio_smoothing = 0.95f;
            } else {
                vj.bpm_system.crossfade_beat_interval = fmin(128.0f, vj.bpm_system.crossfade_beat_interval + 1.0f);
            }
            break;
            
        // Page Up/Down - dual purpose (category navigation or preset pages)
        case KEY_PPAGE: {  // Page Up
            if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Navigate preset pages
                vj.preset_page = (vj.preset_page - 1 + 4) % 4;  // 4 pages (0-3)
                // Update selected preset to stay on the same relative position
                int position_in_page = vj.selected_preset % 5;
                vj.selected_preset = vj.preset_page * 5 + position_in_page;
                if (vj.selected_preset >= MAX_PRESETS) {
                    vj.selected_preset = MAX_PRESETS - 1;
                }
            } else {
                // Previous scene category
                CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
                int current_category = deck->scene_id / 10;
                int scene_in_category = deck->scene_id % 10;
                int new_category = (current_category - 1 + 19) % 19;  // Wrap around to 18 if at 0
                deck->scene_id = new_category * 10 + scene_in_category;
            }
            break;
        }
        case KEY_NPAGE: {  // Page Down
            if (vj.current_ui_page == UI_PAGE_PRESETS) {
                // Navigate preset pages
                vj.preset_page = (vj.preset_page + 1) % 4;  // 4 pages (0-3)
                // Update selected preset to stay on the same relative position
                int position_in_page = vj.selected_preset % 5;
                vj.selected_preset = vj.preset_page * 5 + position_in_page;
                if (vj.selected_preset >= MAX_PRESETS) {
                    vj.selected_preset = MAX_PRESETS - 1;
                }
            } else {
                // Next scene category
                CLIFTDeck* deck = vj.selected_deck == 0 ? &vj.deck_a : &vj.deck_b;
                int current_category = deck->scene_id / 10;
                int scene_in_category = deck->scene_id % 10;
                int new_category = (current_category + 1) % 19;  // Wrap around to 0 if at 18
                deck->scene_id = new_category * 10 + scene_in_category;
            }
            break;
        }
    }
}

// ============= MAIN APPLICATION =============

int main(int argc, char* argv[]) {
    // Debug: Very early debug output
    fprintf(stderr, "DEBUG: CLIFT starting...\n");
    fflush(stderr);
    
    // Parse command line arguments
    bool start_hidden = false;
    fprintf(stderr, "DEBUG: Parsing %d command line arguments\n", argc);
    fflush(stderr);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hide-ui") == 0 || strcmp(argv[i], "--fullscreen") == 0 || strcmp(argv[i], "-h") == 0) {
            start_hidden = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("CLIFT VJ Software\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --hide-ui, --fullscreen, -h  Start with UI hidden (full-screen mode)\n");
            printf("  --help                       Show this help message\n");
            printf("\nControls:\n");
            printf("  U - Toggle UI visibility\n");
            printf("  F - Full auto mode\n");
            printf("  H - Show/hide help\n");
            printf("  Q - Quit\n");
            return 0;
        }
    }
    
    fprintf(stderr, "DEBUG: Initializing ncurses...\n");
    fflush(stderr);
    
    WINDOW* screen = initscr();
    if (!screen || !stdscr) {
        fprintf(stderr, "ERROR: Failed to initialize ncurses!\n");
        fprintf(stderr, "TERM=%s\n", getenv("TERM") ? getenv("TERM") : "NULL");
        return 1;
    }
    
    fprintf(stderr, "DEBUG: ncurses initialized successfully\n");
    fflush(stderr);
    
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);
    
    // Initialize color support
    if (has_colors()) {
        start_color();
        // Define color pairs for visual effects
        init_pair(1, COLOR_RED, COLOR_BLACK);        // Beats/energy
        init_pair(2, COLOR_GREEN, COLOR_BLACK);      // Matrix rain
        init_pair(3, COLOR_BLUE, COLOR_BLACK);       // Cool effects
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);     // Highlights
        init_pair(5, COLOR_MAGENTA, COLOR_BLACK);    // Purple effects
        init_pair(6, COLOR_CYAN, COLOR_BLACK);       // Bright blue
        init_pair(7, COLOR_WHITE, COLOR_BLACK);      // Default white
        init_pair(8, COLOR_BLACK, COLOR_RED);        // Inverse for beats
        init_pair(9, COLOR_BLACK, COLOR_GREEN);      // Inverse green
        init_pair(10, COLOR_BLACK, COLOR_BLUE);      // Inverse blue
    }
    
    int height, width;
    getmaxyx(stdscr, height, width);
    
    fprintf(stderr, "DEBUG: Terminal size: %dx%d\n", width, height);
    fflush(stderr);
    
    fprintf(stderr, "DEBUG: Calling vj_init...\n");
    fflush(stderr);
    
    vj_init(width, height, start_hidden);
    
    fprintf(stderr, "DEBUG: vj_init completed successfully\n");
    fflush(stderr);
    
    // Animated CLIFT splash screen
    if (!start_hidden) {
        clear();
        
        // ASCII art for CLIFT logo (using regular ASCII)
        const char* clift_logo[] = {
            " ####  #      ### ##### #####",
            "#      #       #  #       #  ",
            "#      #       #  ####    #  ",
            "#      #       #  #       #  ",
            " ####  ##### ### #####    #  "
        };
        
        // Rotating ASCII spinner characters
        const char* spinners = "|/-\\";
        
        // Animation loop for splash screen
        for (int frame = 0; frame < 30; frame++) {
            clear();
            
            // Draw CLIFT logo
            int logo_y = height/2 - 4;
            int logo_x = width/2 - 15;
            
            if (has_colors()) {
                // Cycle through colors for animation effect
                int color = (frame / 5) % 6 + 1;
                attron(COLOR_PAIR(color) | A_BOLD);
            }
            
            for (int i = 0; i < 5; i++) {
                mvprintw(logo_y + i, logo_x, "%s", clift_logo[i]);
            }
            
            if (has_colors()) {
                attroff(A_BOLD);
            }
            
            // Draw initializing with rotating spinner
            char spinner = spinners[frame % 4];
            if (has_colors()) {
                attron(COLOR_PAIR(2));
            }
            mvprintw(height/2 + 2, width/2 - 12, "Initializing... %c", spinner);
            
            // Draw rotating ASCII border
            int border_frame = frame % 4;
            const char* border_chars = "-|\\/-|\\/-|\\/-|\\/";
            
            // Draw animated corner brackets
            const char corners[] = {'+', '*', 'o', '+'};
            char corner = corners[border_frame];
            mvaddch(height/2 - 8, width/2 - 20, corner);
            mvaddch(height/2 - 8, width/2 + 18, corner);
            mvaddch(height/2 + 4, width/2 - 20, corner);
            mvaddch(height/2 + 4, width/2 + 18, corner);
            
            if (has_colors()) {
                attroff(COLOR_PAIR(2));
            }
            
            refresh();
            usleep(50000); // 50ms per frame
        }
    }
    
    refresh();
    
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    
    while (running) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        float dt = (current_time.tv_sec - last_time.tv_sec) + 
                   (current_time.tv_nsec - last_time.tv_nsec) / 1000000000.0f;
        last_time = current_time;
        
        vj_update(dt);
        vj_render();
        vj_render_ui();
        vj_handle_input();
        
        usleep(16667);  // ~60 FPS
    }
    
    // Cleanup
    running = false;
    
    // Stop websocket server
    stop_websocket_server();
    
    // Stop audio capture
    stop_audio_capture();
    pthread_mutex_destroy(&vj.audio_mutex);
    
    free(vj.deck_a.buffer);
    free(vj.deck_a.zbuffer);
    free(vj.deck_b.buffer);
    free(vj.deck_b.zbuffer);
    free(vj.output_buffer);
    free(vj.temp_buffer);
    free(vj.output_zbuffer);
    
    // Cleanup Ableton Link
    if (vj.link.link_handle) {
        link_destroy(vj.link.link_handle);
    }
    
    endwin();
    return 0;
}