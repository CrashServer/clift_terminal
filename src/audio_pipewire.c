#include "audio_pipewire.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef USE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>

// PipeWire state
static struct pw_thread_loop *pw_loop = NULL;
static struct pw_stream *pw_stream = NULL;
static bool pw_initialized = false;

// Ring buffer for thread-safe audio transfer
static float* audio_ring_buffer = NULL;
static int ring_buffer_size = 0;
static int write_pos = 0;
static int read_pos = 0;
static pthread_mutex_t ring_mutex = PTHREAD_MUTEX_INITIALIZER;

// PipeWire stream events
static void on_process(void *userdata) {
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples;
    uint32_t n_samples;
    
    if ((b = pw_stream_dequeue_buffer(pw_stream)) == NULL) {
        return;
    }
    
    buf = b->buffer;
    if (buf->datas[0].data == NULL) {
        pw_stream_queue_buffer(pw_stream, b);
        return;
    }
    
    samples = (float*)buf->datas[0].data;
    n_samples = buf->datas[0].chunk->size / sizeof(float);
    
    // Copy to ring buffer
    pthread_mutex_lock(&ring_mutex);
    
    for (uint32_t i = 0; i < n_samples && i < ring_buffer_size; i++) {
        audio_ring_buffer[write_pos] = samples[i];
        write_pos = (write_pos + 1) % ring_buffer_size;
        
        // Handle buffer overflow by moving read position
        if (write_pos == read_pos) {
            read_pos = (read_pos + 1) % ring_buffer_size;
        }
    }
    
    pthread_mutex_unlock(&ring_mutex);
    
    pw_stream_queue_buffer(pw_stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

bool audio_pipewire_init(const char* app_name) {
    // Save stdout and stderr, redirect to /dev/null during init
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
        close(null_fd);
    }
    
    // Initialize PipeWire
    pw_init(NULL, NULL);
    
    // Allocate ring buffer
    ring_buffer_size = AUDIO_BUFFER_SIZE * 8;  // 8x buffer for smooth operation
    audio_ring_buffer = calloc(ring_buffer_size, sizeof(float));
    if (!audio_ring_buffer) {
        // Restore stdout/stderr
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (saved_stderr >= 0) {
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
        }
        return false;
    }
    
    // Create thread loop
    pw_loop = pw_thread_loop_new("clift-audio", NULL);
    if (!pw_loop) {
        free(audio_ring_buffer);
        // Restore stdout/stderr
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (saved_stderr >= 0) {
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
        }
        return false;
    }
    
    // Create stream
    struct spa_audio_info_raw spa_audio_info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .rate = AUDIO_SAMPLE_RATE,
        .channels = AUDIO_CHANNELS,
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR}
    };
    
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &spa_audio_info);
    
    pw_stream = pw_stream_new_simple(
        pw_thread_loop_get_loop(pw_loop),
        app_name,
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Music",
            NULL),
        &stream_events,
        NULL);
    
    if (!pw_stream) {
        pw_thread_loop_destroy(pw_loop);
        free(audio_ring_buffer);
        // Restore stdout/stderr
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (saved_stderr >= 0) {
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
        }
        return false;
    }
    
    // Connect stream
    if (pw_stream_connect(pw_stream,
                         PW_DIRECTION_INPUT,
                         PW_ID_ANY,
                         PW_STREAM_FLAG_AUTOCONNECT |
                         PW_STREAM_FLAG_MAP_BUFFERS |
                         PW_STREAM_FLAG_RT_PROCESS,
                         params, 1) < 0) {
        pw_stream_destroy(pw_stream);
        pw_thread_loop_destroy(pw_loop);
        free(audio_ring_buffer);
        // Restore stdout/stderr
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (saved_stderr >= 0) {
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
        }
        return false;
    }
    
    // Start the loop
    pw_thread_loop_start(pw_loop);
    
    // Restore stdout/stderr now that PipeWire is initialized
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    
    pw_initialized = true;
    return true;
}

#else // !USE_PIPEWIRE

// Fallback implementation without PipeWire
static float* audio_ring_buffer = NULL;
static int ring_buffer_size = 0;
static int write_pos = 0;
static int read_pos = 0;

bool audio_pipewire_init(const char* app_name) {
    (void)app_name;
    
    // Allocate ring buffer for audio
    ring_buffer_size = AUDIO_BUFFER_SIZE * 4;  // 4x buffer for smooth playback
    audio_ring_buffer = calloc(ring_buffer_size * AUDIO_CHANNELS, sizeof(float));
    if (!audio_ring_buffer) {
        return false;
    }
    
    write_pos = 0;
    read_pos = 0;
    
    fprintf(stderr, "CLIFT: PipeWire support not compiled in. Using test signal.\n");
    fprintf(stderr, "CLIFT: To enable PipeWire, install libpipewire-0.3-dev and rebuild with USE_PIPEWIRE=1\n");
    
    return true;
}

#endif // USE_PIPEWIRE

void audio_pipewire_cleanup(void) {
#ifdef USE_PIPEWIRE
    if (pw_initialized) {
        if (pw_stream) {
            pw_thread_loop_stop(pw_loop);
            pw_stream_destroy(pw_stream);
        }
        if (pw_loop) {
            pw_thread_loop_destroy(pw_loop);
        }
        pw_initialized = false;
    }
    pthread_mutex_destroy(&ring_mutex);
#endif
    
    if (audio_ring_buffer) {
        free(audio_ring_buffer);
        audio_ring_buffer = NULL;
    }
}

int audio_pipewire_get_buffer(float* buffer, int max_frames) {
    if (!audio_ring_buffer) return 0;
    
#ifdef USE_PIPEWIRE
    if (pw_initialized) {
        // Copy from ring buffer
        pthread_mutex_lock(&ring_mutex);
        
        int frames_available = 0;
        int temp_read = read_pos;
        
        // Calculate available frames
        if (write_pos >= read_pos) {
            frames_available = (write_pos - read_pos) / AUDIO_CHANNELS;
        } else {
            frames_available = (ring_buffer_size - read_pos + write_pos) / AUDIO_CHANNELS;
        }
        
        int frames_to_copy = frames_available < max_frames ? frames_available : max_frames;
        
        // Copy audio data
        for (int i = 0; i < frames_to_copy; i++) {
            for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
                buffer[i * AUDIO_CHANNELS + ch] = audio_ring_buffer[read_pos];
                read_pos = (read_pos + 1) % ring_buffer_size;
            }
        }
        
        pthread_mutex_unlock(&ring_mutex);
        
        // If not enough data, pad with silence
        if (frames_to_copy < max_frames) {
            memset(&buffer[frames_to_copy * AUDIO_CHANNELS], 0, 
                   (max_frames - frames_to_copy) * AUDIO_CHANNELS * sizeof(float));
        }
        
        return frames_to_copy > 0 ? max_frames : 0;
    }
#endif
    
    // Fallback: generate test signal
    static float phase = 0.0f;
    int frames = max_frames < AUDIO_BUFFER_SIZE ? max_frames : AUDIO_BUFFER_SIZE;
    
    for (int i = 0; i < frames; i++) {
        // Generate a test tone with harmonics
        float sample = 0.0f;
        sample += 0.3f * sinf(phase);                    // Fundamental
        sample += 0.2f * sinf(phase * 2.0f);            // 2nd harmonic
        sample += 0.1f * sinf(phase * 3.0f);            // 3rd harmonic
        sample += 0.05f * sinf(phase * 0.5f);           // Sub bass
        
        // Add some noise for high frequency content
        sample += 0.02f * ((float)rand() / RAND_MAX - 0.5f);
        
        // Stereo
        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
        
        phase += 2.0f * M_PI * 440.0f / AUDIO_SAMPLE_RATE;
        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    
    return frames;
}

// Simple DFT for spectrum analysis (replace with FFT in production)
void audio_compute_spectrum(const float* audio_buffer, int frames, float* spectrum, int spectrum_size) {
    // Clear spectrum
    memset(spectrum, 0, spectrum_size * sizeof(float));
    
    // Simple frequency bins
    for (int bin = 0; bin < spectrum_size; bin++) {
        float freq = (float)bin / spectrum_size * (AUDIO_SAMPLE_RATE / 2.0f);
        float real = 0.0f, imag = 0.0f;
        
        // Simple DFT for this frequency bin
        for (int i = 0; i < frames && i < 512; i++) {
            float t = (float)i / AUDIO_SAMPLE_RATE;
            float angle = 2.0f * M_PI * freq * t;
            real += audio_buffer[i * 2] * cosf(angle);
            imag += audio_buffer[i * 2] * sinf(angle);
        }
        
        // Magnitude
        spectrum[bin] = sqrtf(real * real + imag * imag) / frames;
        
        // Apply logarithmic scaling
        spectrum[bin] = logf(1.0f + spectrum[bin] * 10.0f) / logf(11.0f);
    }
}

void audio_compute_levels(const float* spectrum, int spectrum_size, 
                         float* bass, float* mid, float* treble, float* volume) {
    *bass = 0.0f;
    *mid = 0.0f;
    *treble = 0.0f;
    *volume = 0.0f;
    
    int bass_end = spectrum_size / 8;      // 0-1/8 for bass
    int mid_end = spectrum_size / 2;       // 1/8-1/2 for mid
    
    // Compute averages for each band
    for (int i = 0; i < spectrum_size; i++) {
        float val = spectrum[i];
        *volume += val;
        
        if (i < bass_end) {
            *bass += val;
        } else if (i < mid_end) {
            *mid += val;
        } else {
            *treble += val;
        }
    }
    
    // Normalize
    *bass /= bass_end;
    *mid /= (mid_end - bass_end);
    *treble /= (spectrum_size - mid_end);
    *volume /= spectrum_size;
    
    // Boost bass frequencies (they typically have less energy)
    *bass *= 2.0f;
    
    // Clamp to 0-1 range
    *bass = fminf(1.0f, *bass);
    *mid = fminf(1.0f, *mid);
    *treble = fminf(1.0f, *treble);
    *volume = fminf(1.0f, *volume);
}

bool audio_detect_beat(float current_volume, float* beat_intensity) {
    static float volume_history[8] = {0};
    static int history_index = 0;
    static float last_beat_time = 0.0f;
    static float beat_threshold = 0.0f;
    
    // Update history
    volume_history[history_index] = current_volume;
    history_index = (history_index + 1) % 8;
    
    // Calculate average
    float avg = 0.0f;
    for (int i = 0; i < 8; i++) {
        avg += volume_history[i];
    }
    avg /= 8.0f;
    
    // Dynamic threshold
    beat_threshold = avg * 1.5f;
    
    // Detect beat
    bool beat = false;
    if (current_volume > beat_threshold && current_volume > 0.3f) {
        beat = true;
        *beat_intensity = (current_volume - beat_threshold) / (1.0f - beat_threshold);
    } else {
        *beat_intensity = 0.0f;
    }
    
    return beat;
}