#ifndef SURFACE_SCENES_H
#define SURFACE_SCENES_H

#include <stdint.h>

// Surface scenes write to uint8_t color buffer (not char buffer)
// Each byte = ncurses color index (0-7): BLACK,RED,GREEN,YELLOW,BLUE,MAGENTA,CYAN,WHITE
// pixel_height = terminal_height * 2 (doubled for half-block rendering)

void surface_scene_color_wash(uint8_t* surface, int width, int pixel_height,
                              void* params, float time, void* audio);

void surface_scene_gradient_sweep(uint8_t* surface, int width, int pixel_height,
                                  void* params, float time, void* audio);

void surface_scene_audio_pulse(uint8_t* surface, int width, int pixel_height,
                               void* params, float time, void* audio);

void surface_scene_color_blocks(uint8_t* surface, int width, int pixel_height,
                                void* params, float time, void* audio);

void surface_scene_wave_surface(uint8_t* surface, int width, int pixel_height,
                                void* params, float time, void* audio);

void surface_scene_breathing(uint8_t* surface, int width, int pixel_height,
                             void* params, float time, void* audio);

void surface_scene_mondrian(uint8_t* surface, int width, int pixel_height,
                            void* params, float time, void* audio);

void surface_scene_aurora(uint8_t* surface, int width, int pixel_height,
                          void* params, float time, void* audio);

void surface_scene_heat_map(uint8_t* surface, int width, int pixel_height,
                            void* params, float time, void* audio);

void surface_scene_strobe_fields(uint8_t* surface, int width, int pixel_height,
                                 void* params, float time, void* audio);

#endif
