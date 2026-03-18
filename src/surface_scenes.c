#include "surface_scenes.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// AudioData layout (must match clift_engine.c)
typedef struct {
    float bass, mid, treble, volume;
    float bpm;
    bool beat_detected;
    float beat_intensity;
    float spectrum[64];
    bool valid;
    float attack, spectral_flux, spectral_centroid;
    float low_mid, high_mid;
    float energy_history[32];
    int energy_history_idx;
    float energy_momentum, buildup_intensity, drop_intensity;
    float smooth_bass, smooth_mid, smooth_treble, smooth_volume;
} SurfaceAudio;

// Parameter layout (must match clift_engine.c)
typedef struct {
    float value, target, min, max, speed;
    bool auto_mode;
    float lfo_freq, lfo_amount;
    const char* name;
} SurfaceParam;

// ncurses color indices:
// 0=BLACK, 1=RED, 2=GREEN, 3=YELLOW, 4=BLUE, 5=MAGENTA, 6=CYAN, 7=WHITE

static inline void surface_fill(uint8_t* s, int size, uint8_t color) {
    memset(s, color, size);
}

// --- Scene 280: Color Wash ---
// Full screen slowly shifting single color
void surface_scene_color_wash(uint8_t* surface, int width, int pixel_height,
                              void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    float audio_mod = (aud && aud->valid) ? aud->smooth_bass * 2.0f : 0.0f;
    int color = ((int)(time * 0.3f + audio_mod)) % 7 + 1;  // 1-7, skip black
    surface_fill(surface, width * pixel_height, (uint8_t)color);
}

// --- Scene 281: Gradient Sweep ---
// Rotating linear gradient with 4-color ramp
void surface_scene_gradient_sweep(uint8_t* surface, int width, int pixel_height,
                                  void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    float audio_boost = (aud && aud->valid) ? aud->smooth_volume : 0.5f;

    float angle = time * 0.4f;
    float cos_a = cosf(angle), sin_a = sinf(angle);

    // Color ramp: black → blue → cyan → white
    static const uint8_t ramp[] = {0, 4, 6, 7};

    for (int py = 0; py < pixel_height; py++) {
        float ny = (float)py / pixel_height;
        for (int x = 0; x < width; x++) {
            float nx = (float)x / width;
            float grad = (nx * cos_a + ny * sin_a) * 0.5f + 0.5f;
            grad = fmodf(grad + time * 0.05f, 1.0f);
            grad *= (0.7f + audio_boost * 0.3f);
            int idx = (int)(grad * 3.99f);
            if (idx > 3) idx = 3;
            if (idx < 0) idx = 0;
            surface[py * width + x] = ramp[idx];
        }
    }
}

// --- Scene 282: Audio Pulse ---
// Bass drives screen color intensity, radial from center
void surface_scene_audio_pulse(uint8_t* surface, int width, int pixel_height,
                               void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    float bass = (aud && aud->valid) ? aud->smooth_bass : (0.5f + 0.3f * sinf(time * 2.0f));
    float mid = (aud && aud->valid) ? aud->smooth_mid : (0.3f + 0.2f * sinf(time * 3.0f));
    bool beat = (aud && aud->valid) ? aud->beat_detected : false;

    float cx = width * 0.5f;
    float cy = pixel_height * 0.5f;
    float max_r = sqrtf(cx * cx + cy * cy);

    // Expanding ring on beat
    float ring_phase = fmodf(time * 2.0f, 1.0f);

    for (int py = 0; py < pixel_height; py++) {
        for (int x = 0; x < width; x++) {
            float dx = x - cx, dy = py - cy;
            float r = sqrtf(dx * dx + dy * dy) / max_r;

            // Radial pulse modulated by bass
            float intensity = bass * (1.0f - r) + mid * 0.3f;

            // Add expanding ring
            float ring_dist = fabsf(r - ring_phase);
            if (ring_dist < 0.08f) {
                intensity += (1.0f - ring_dist / 0.08f) * 0.5f;
            }

            // Beat flash
            if (beat) intensity += 0.3f;

            if (intensity < 0.0f) intensity = 0.0f;
            if (intensity > 1.0f) intensity = 1.0f;

            // Map to color: black → red → yellow → white
            uint8_t color;
            if (intensity < 0.25f) color = 0;       // BLACK
            else if (intensity < 0.5f) color = 1;   // RED
            else if (intensity < 0.75f) color = 3;  // YELLOW
            else color = 7;                          // WHITE
            surface[py * width + x] = color;
        }
    }
}

// --- Scene 283: Color Blocks ---
// Animated rectangles of solid color
void surface_scene_color_blocks(uint8_t* surface, int width, int pixel_height,
                                void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    float bass = (aud && aud->valid) ? aud->smooth_bass : 0.5f;

    // Start with black
    surface_fill(surface, width * pixel_height, 0);

    // Draw several animated rectangles
    int num_blocks = 5 + (int)(bass * 5.0f);
    for (int i = 0; i < num_blocks; i++) {
        float phase = time * (0.2f + i * 0.1f) + i * 1.7f;
        int bx = (int)((0.5f + 0.4f * sinf(phase)) * width);
        int by = (int)((0.5f + 0.4f * cosf(phase * 0.7f)) * pixel_height);
        int bw = width / (3 + i % 4);
        int bh = pixel_height / (3 + (i + 1) % 4);
        uint8_t color = (i % 7) + 1;

        int x0 = bx - bw / 2, x1 = bx + bw / 2;
        int y0 = by - bh / 2, y1 = by + bh / 2;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > width) x1 = width;
        if (y1 > pixel_height) y1 = pixel_height;

        for (int py = y0; py < y1; py++)
            for (int x = x0; x < x1; x++)
                surface[py * width + x] = color;
    }
}

// --- Scene 284: Wave Surface ---
// Sine wave color bands flowing across screen
void surface_scene_wave_surface(uint8_t* surface, int width, int pixel_height,
                                void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    float audio_mod = (aud && aud->valid) ? aud->smooth_volume : 0.5f;
    static const uint8_t colors[] = {0, 4, 6, 2, 3, 1, 5, 7};

    for (int py = 0; py < pixel_height; py++) {
        float ny = (float)py / pixel_height;
        for (int x = 0; x < width; x++) {
            float nx = (float)x / width;
            float wave = sinf(nx * 8.0f + time * 1.5f + ny * 4.0f) * 0.5f + 0.5f;
            wave += sinf(ny * 6.0f - time * 1.2f) * 0.3f * audio_mod;
            wave = fmodf(fabsf(wave), 1.0f);
            int idx = (int)(wave * 7.99f);
            if (idx > 7) idx = 7;
            surface[py * width + x] = colors[idx];
        }
    }
}

// --- Scene 285: Breathing ---
// Two-color pulse synced to BPM
void surface_scene_breathing(uint8_t* surface, int width, int pixel_height,
                             void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    float bpm = (aud && aud->valid) ? aud->bpm : 120.0f;
    if (bpm < 40.0f) bpm = 120.0f;
    float beat_phase = fmodf(time * bpm / 60.0f, 1.0f);
    float breath = (sinf(beat_phase * 2.0f * M_PI) + 1.0f) * 0.5f;  // 0-1 smooth

    uint8_t color_a = 4;  // BLUE
    uint8_t color_b = 1;  // RED
    uint8_t color = (breath > 0.5f) ? color_a : color_b;
    surface_fill(surface, width * pixel_height, color);
}

// --- Scene 286: Mondrian ---
// Geometric colored rectangles (Piet Mondrian style)
void surface_scene_mondrian(uint8_t* surface, int width, int pixel_height,
                            void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;

    // White background
    surface_fill(surface, width * pixel_height, 7);

    // Animated grid lines (black)
    int grid_x = width / 5;
    int grid_y = pixel_height / 4;
    int shift = (int)(time * 3.0f) % (grid_x / 2);

    // Draw grid lines
    for (int py = 0; py < pixel_height; py++) {
        for (int x = 0; x < width; x++) {
            int gx = (x + shift) % grid_x;
            int gy = py % grid_y;
            if (gx < 2 || gy < 2) {
                surface[py * width + x] = 0;  // BLACK lines
            }
        }
    }

    // Fill some cells with primary colors
    static const uint8_t mondrian_colors[] = {1, 4, 3, 7, 7, 7};  // RED, BLUE, YELLOW, WHITE...
    int cell_idx = 0;
    for (int cy = 0; cy < 4; cy++) {
        for (int cx = 0; cx < 5; cx++) {
            uint8_t fill = mondrian_colors[(cell_idx + (int)(time * 0.5f)) % 6];
            cell_idx++;
            if (fill == 7) continue;  // Skip white cells

            int x0 = cx * grid_x + 2 - shift;
            int y0 = cy * grid_y + 2;
            int x1 = (cx + 1) * grid_x - shift;
            int y1 = (cy + 1) * grid_y;

            for (int py = y0; py < y1 && py < pixel_height; py++) {
                for (int x = x0; x < x1 && x < width; x++) {
                    if (x >= 0 && py >= 0)
                        surface[py * width + x] = fill;
                }
            }
        }
    }
}

// --- Scene 287: Aurora ---
// Flowing organic color bands
void surface_scene_aurora(uint8_t* surface, int width, int pixel_height,
                          void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    float audio_mod = (aud && aud->valid) ? aud->smooth_mid : 0.4f;

    static const uint8_t aurora_colors[] = {0, 4, 6, 2, 6, 4, 0};

    for (int py = 0; py < pixel_height; py++) {
        float ny = (float)py / pixel_height;
        for (int x = 0; x < width; x++) {
            float nx = (float)x / width;

            // Layered sine waves for organic flow
            float wave1 = sinf(nx * 3.0f + time * 0.8f + ny * 2.0f) * 0.5f + 0.5f;
            float wave2 = sinf(nx * 5.0f - time * 0.5f + ny * 1.5f) * 0.3f;
            float wave3 = sinf(nx * 1.5f + time * 1.2f) * 0.2f * audio_mod;

            float val = wave1 + wave2 + wave3;
            val = fmodf(fabsf(val), 1.0f);

            int idx = (int)(val * 6.99f);
            if (idx > 6) idx = 6;
            surface[py * width + x] = aurora_colors[idx];
        }
    }
}

// --- Scene 288: Heat Map ---
// Audio spectrum mapped as heat colors across screen
void surface_scene_heat_map(uint8_t* surface, int width, int pixel_height,
                            void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;

    for (int py = 0; py < pixel_height; py++) {
        float ny = (float)py / pixel_height;
        for (int x = 0; x < width; x++) {
            float nx = (float)x / width;

            // Map x position to spectrum band
            float intensity;
            if (aud && aud->valid) {
                int band = (int)(nx * 63.0f);
                if (band > 63) band = 63;
                intensity = aud->spectrum[band] * 3.0f;

                // Vertical falloff from bottom
                intensity *= (1.0f - ny * 0.7f);
            } else {
                // Simulated spectrum
                intensity = (sinf(nx * 12.0f + time * 2.0f) + 1.0f) * 0.3f;
                intensity *= (1.0f - ny * 0.7f);
            }

            if (intensity > 1.0f) intensity = 1.0f;
            if (intensity < 0.0f) intensity = 0.0f;

            // Heat ramp: BLACK → BLUE → RED → YELLOW → WHITE
            uint8_t color;
            if (intensity < 0.2f) color = 0;       // BLACK
            else if (intensity < 0.4f) color = 4;   // BLUE
            else if (intensity < 0.6f) color = 1;   // RED
            else if (intensity < 0.8f) color = 3;   // YELLOW
            else color = 7;                          // WHITE
            surface[py * width + x] = color;
        }
    }
}

// --- Scene 289: Strobe Fields ---
// Alternating color blocks on beat
void surface_scene_strobe_fields(uint8_t* surface, int width, int pixel_height,
                                 void* params, float time, void* audio) {
    SurfaceAudio* aud = (SurfaceAudio*)audio;
    bool beat = (aud && aud->valid) ? aud->beat_detected : ((int)(time * 4.0f) % 2 == 0);
    float bass = (aud && aud->valid) ? aud->smooth_bass : 0.5f;

    int divisions = 2 + (int)(bass * 4.0f);
    int cell_w = width / divisions;
    int cell_h = pixel_height / divisions;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;

    int phase = beat ? 1 : 0;

    for (int py = 0; py < pixel_height; py++) {
        int cy = py / cell_h;
        for (int x = 0; x < width; x++) {
            int cx = x / cell_w;
            int checkerboard = (cx + cy + phase) % 2;
            uint8_t color = checkerboard ? 7 : 0;  // WHITE / BLACK
            surface[py * width + x] = color;
        }
    }
}
