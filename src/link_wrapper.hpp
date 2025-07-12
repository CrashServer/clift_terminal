#ifndef LINK_WRAPPER_HPP
#define LINK_WRAPPER_HPP

#ifdef __cplusplus
extern "C" {
#endif

// C-compatible structure for Link state
typedef struct {
    int enabled;
    int connected;
    float link_bpm;
    float link_beat;
    float link_phase;
    int num_peers;
    float quantum;
    int start_stop_sync;
    int is_playing;
} LinkState;

// Initialize Ableton Link with initial tempo
void* link_create(float bpm);

// Destroy Link instance
void link_destroy(void* link_handle);

// Enable/disable Link
void link_enable(void* link_handle, int enable);

// Set tempo
void link_set_tempo(void* link_handle, float bpm);

// Set quantum (bar length in beats)
void link_set_quantum(void* link_handle, float quantum);

// Enable/disable start/stop sync
void link_set_start_stop_sync(void* link_handle, int enable);

// Start/stop playing
void link_set_is_playing(void* link_handle, int is_playing);

// Get current Link state
LinkState link_get_state(void* link_handle);

// Force peers rescan (useful for UI updates)
void link_force_peers_rescan(void* link_handle);

#ifdef __cplusplus
}
#endif

#endif // LINK_WRAPPER_HPP