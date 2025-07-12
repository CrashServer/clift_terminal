#ifndef AUDIO_PIPEWIRE_H
#define AUDIO_PIPEWIRE_H

#include <stdbool.h>
#include <pthread.h>

// Audio configuration
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFER_SIZE 1024

// Initialize PipeWire audio capture
bool audio_pipewire_init(const char* app_name);

// Cleanup PipeWire
void audio_pipewire_cleanup(void);

// Get current audio buffer (returns number of frames available)
int audio_pipewire_get_buffer(float* buffer, int max_frames);

// Simple FFT for spectrum analysis
void audio_compute_spectrum(const float* audio_buffer, int frames, float* spectrum, int spectrum_size);

// Compute audio levels (bass, mid, treble, volume)
void audio_compute_levels(const float* spectrum, int spectrum_size, 
                         float* bass, float* mid, float* treble, float* volume);

// Simple beat detection
bool audio_detect_beat(float current_volume, float* beat_intensity);

#endif // AUDIO_PIPEWIRE_H