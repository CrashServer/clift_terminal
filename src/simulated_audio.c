#include "simulated_audio.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// AudioData struct layout matches clift_engine.c definition
typedef struct AudioData {
    float bass, mid, treble, volume;
    float bpm;
    bool beat_detected;
    float beat_intensity;
    float spectrum[64];
    bool valid;
    float attack;
    float spectral_flux;
    float spectral_centroid;
    float low_mid;
    float high_mid;
    float energy_history[32];
    int energy_history_idx;
    float energy_momentum;
    float buildup_intensity;
    float drop_intensity;
    float smooth_bass, smooth_mid, smooth_treble, smooth_volume;
} AudioData;

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

void simulated_audio_update(void* audio_v, float time, float intensity) {
    AudioData* audio = (AudioData*)audio_v;
    if (!audio) return;

    // BPM drifts slowly between 80-140
    float bpm_base = 110.0f + 30.0f * sinf(time * 0.01f);
    float bpm_jitter = sinf(time * 0.37f) * 5.0f;
    audio->bpm = bpm_base + bpm_jitter;

    float beat_interval = 60.0f / audio->bpm;

    // Beat detection: trigger every beat_interval
    // Use fmod to check if we're near a beat boundary
    float beat_phase = fmodf(time, beat_interval);
    float beat_window = 0.05f; // 50ms window
    audio->beat_detected = (beat_phase < beat_window);
    audio->beat_intensity = audio->beat_detected ? (0.7f + randf() * 0.3f) : 0.0f;

    // Bass: low frequency pulses, synced to beat, modulated by intensity
    float bass_beat = audio->beat_detected ? 0.8f : 0.0f;
    float bass_sine = 0.3f + 0.3f * sinf(time * 1.2f) + 0.2f * sinf(time * 0.7f);
    audio->bass = (bass_beat * 0.6f + bass_sine * 0.4f) * (0.3f + intensity * 0.7f);
    if (audio->bass > 1.0f) audio->bass = 1.0f;

    // Mid: layered sines at different frequencies
    float mid_sine = 0.2f + 0.25f * sinf(time * 2.3f) + 0.15f * sinf(time * 3.7f);
    float mid_beat = audio->beat_detected ? 0.3f : 0.0f;
    audio->mid = (mid_sine + mid_beat) * (0.4f + intensity * 0.6f);
    if (audio->mid > 1.0f) audio->mid = 1.0f;

    // Treble: faster oscillation, more sparkly
    float treble_sine = 0.15f + 0.2f * sinf(time * 5.1f) + 0.15f * sinf(time * 7.3f);
    float treble_random = randf() * 0.1f; // shimmer
    audio->treble = (treble_sine + treble_random) * (0.3f + intensity * 0.7f);
    if (audio->treble > 1.0f) audio->treble = 1.0f;

    // Sub-bands
    audio->low_mid = 0.2f + 0.2f * sinf(time * 1.8f) * (0.5f + intensity * 0.5f);
    audio->high_mid = 0.2f + 0.2f * sinf(time * 4.0f) * (0.4f + intensity * 0.6f);

    // Volume: overall energy
    audio->volume = (audio->bass + audio->mid + audio->treble) / 3.0f;

    // Spectrum: 64 bands generated from sine combinations
    for (int i = 0; i < 64; i++) {
        float freq = (float)i / 64.0f;
        float band = 0.0f;

        // Low frequencies (bass region)
        if (i < 8) {
            band = audio->bass * (1.0f - freq * 8.0f) * 0.8f;
            band += 0.1f * sinf(time * (1.0f + i * 0.3f));
        }
        // Mid frequencies
        else if (i < 32) {
            band = audio->mid * 0.6f;
            band += 0.15f * sinf(time * (2.0f + i * 0.2f));
        }
        // High frequencies
        else {
            band = audio->treble * (1.0f - (freq - 0.5f) * 1.5f);
            band += 0.1f * sinf(time * (4.0f + i * 0.15f));
        }

        // Add some noise for organic feel
        band += randf() * 0.05f;
        if (band < 0.0f) band = 0.0f;
        if (band > 1.0f) band = 1.0f;

        audio->spectrum[i] = band;
    }

    // Attack: transient detection simulation
    audio->attack = audio->beat_detected ? (0.5f + randf() * 0.5f) : randf() * 0.1f;

    // Spectral flux: rate of spectral change
    audio->spectral_flux = 0.2f + 0.3f * fabsf(sinf(time * 0.8f));
    if (audio->beat_detected) audio->spectral_flux += 0.3f;
    if (audio->spectral_flux > 1.0f) audio->spectral_flux = 1.0f;

    // Spectral centroid: brightness
    audio->spectral_centroid = 0.3f + 0.3f * sinf(time * 0.5f) + intensity * 0.2f;

    // Energy history and momentum
    float energy = audio->volume;
    audio->energy_history[audio->energy_history_idx] = energy;
    audio->energy_history_idx = (audio->energy_history_idx + 1) % 32;

    // Momentum: compare recent vs older energy
    float recent_avg = 0.0f, older_avg = 0.0f;
    for (int i = 0; i < 16; i++) {
        int recent_idx = (audio->energy_history_idx - 1 - i + 32) % 32;
        int older_idx = (audio->energy_history_idx - 17 - i + 32) % 32;
        recent_avg += audio->energy_history[recent_idx];
        older_avg += audio->energy_history[older_idx];
    }
    recent_avg /= 16.0f;
    older_avg /= 16.0f;
    audio->energy_momentum = (recent_avg - older_avg) * 5.0f;
    if (audio->energy_momentum > 1.0f) audio->energy_momentum = 1.0f;
    if (audio->energy_momentum < -1.0f) audio->energy_momentum = -1.0f;

    // Buildup/drop detection
    audio->buildup_intensity = audio->energy_momentum > 0.3f ? audio->energy_momentum : 0.0f;
    audio->drop_intensity = audio->energy_momentum < -0.3f ? -audio->energy_momentum : 0.0f;

    // Smoothed versions (simple low-pass)
    float smooth_factor = 0.1f;
    audio->smooth_bass += (audio->bass - audio->smooth_bass) * smooth_factor;
    audio->smooth_mid += (audio->mid - audio->smooth_mid) * smooth_factor;
    audio->smooth_treble += (audio->treble - audio->smooth_treble) * smooth_factor;
    audio->smooth_volume += (audio->volume - audio->smooth_volume) * smooth_factor;

    // Occasional random spikes for organic feel
    if (randf() < 0.01f * intensity) {
        audio->bass += randf() * 0.4f;
        if (audio->bass > 1.0f) audio->bass = 1.0f;
    }
    if (randf() < 0.02f * intensity) {
        audio->treble += randf() * 0.3f;
        if (audio->treble > 1.0f) audio->treble = 1.0f;
    }

    audio->valid = true;
}
