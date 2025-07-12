#include "link_wrapper.hpp"
#include <chrono>
#include <memory>
#include <cmath>

#ifdef USE_FAKE_LINK
// Fake Link implementation when Ableton Link SDK is not available
class LinkWrapper {
public:
    LinkWrapper(float bpm) : fake_bpm(bpm), enabled(false), 
                             start_stop_sync(false), is_playing(false),
                             quantum(4.0f), start_time_ms(get_current_time_ms()) {}
    
    float fake_bpm;
    bool enabled;
    bool start_stop_sync;
    bool is_playing;
    float quantum;
    uint64_t start_time_ms;
    
    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
    
    float get_beat() {
        if (!is_playing) return 0.0f;
        uint64_t elapsed_ms = get_current_time_ms() - start_time_ms;
        float elapsed_seconds = elapsed_ms / 1000.0f;
        float beats_per_second = fake_bpm / 60.0f;
        return elapsed_seconds * beats_per_second;
    }
    
    float get_phase() {
        float beat = get_beat();
        return fmodf(beat, quantum) / quantum;
    }
};
#else
#include <ableton/Link.hpp>
// Real Link implementation
class LinkWrapper {
public:
    LinkWrapper(float bpm) : link(bpm) {
        // Enable Link by default
        link.enable(false);
    }
    
    ableton::Link link;
};
#endif

extern "C" {

void* link_create(float bpm) {
    try {
        return new LinkWrapper(bpm);
    } catch (...) {
        return nullptr;
    }
}

void link_destroy(void* link_handle) {
    if (link_handle) {
        delete static_cast<LinkWrapper*>(link_handle);
    }
}

void link_enable(void* link_handle, int enable) {
    if (link_handle) {
        auto* wrapper = static_cast<LinkWrapper*>(link_handle);
#ifdef USE_FAKE_LINK
        wrapper->enabled = (enable != 0);
#else
        wrapper->link.enable(enable != 0);
#endif
    }
}

void link_set_tempo(void* link_handle, float bpm) {
    if (link_handle) {
        auto* wrapper = static_cast<LinkWrapper*>(link_handle);
#ifdef USE_FAKE_LINK
        wrapper->fake_bpm = bpm;
#else
        auto sessionState = wrapper->link.captureAppSessionState();
        sessionState.setTempo(bpm, wrapper->link.clock().micros());
        wrapper->link.commitAppSessionState(sessionState);
#endif
    }
}

void link_set_quantum(void* link_handle, float quantum) {
    if (link_handle) {
#ifdef USE_FAKE_LINK
        auto* wrapper = static_cast<LinkWrapper*>(link_handle);
        wrapper->quantum = quantum;
#else
        // Note: Quantum is managed by the application, not by Link itself
        // This would typically be stored in the application state
        // For now, we'll just store it in the LinkState when retrieved
#endif
    }
}

void link_set_start_stop_sync(void* link_handle, int enable) {
    if (link_handle) {
        auto* wrapper = static_cast<LinkWrapper*>(link_handle);
#ifdef USE_FAKE_LINK
        wrapper->start_stop_sync = (enable != 0);
#else
        wrapper->link.enableStartStopSync(enable != 0);
#endif
    }
}

void link_set_is_playing(void* link_handle, int is_playing) {
    if (link_handle) {
        auto* wrapper = static_cast<LinkWrapper*>(link_handle);
#ifdef USE_FAKE_LINK
        bool was_playing = wrapper->is_playing;
        wrapper->is_playing = (is_playing != 0);
        if (!was_playing && wrapper->is_playing) {
            // Reset start time when playback starts
            wrapper->start_time_ms = wrapper->get_current_time_ms();
        }
#else
        auto sessionState = wrapper->link.captureAppSessionState();
        const auto time = wrapper->link.clock().micros();
        
        if (is_playing && !sessionState.isPlaying()) {
            sessionState.setIsPlaying(true, time);
        } else if (!is_playing && sessionState.isPlaying()) {
            sessionState.setIsPlaying(false, time);
        }
        
        wrapper->link.commitAppSessionState(sessionState);
#endif
    }
}

LinkState link_get_state(void* link_handle) {
    LinkState state = {0};
    
    if (link_handle) {
        auto* wrapper = static_cast<LinkWrapper*>(link_handle);
#ifdef USE_FAKE_LINK
        state.enabled = wrapper->enabled ? 1 : 0;
        state.connected = 0;  // Always disconnected in fake mode
        state.num_peers = 0;  // No peers in fake mode
        state.link_bpm = wrapper->fake_bpm;
        state.start_stop_sync = wrapper->start_stop_sync ? 1 : 0;
        state.is_playing = wrapper->is_playing ? 1 : 0;
        state.quantum = wrapper->quantum;
        state.link_beat = wrapper->get_beat();
        state.link_phase = wrapper->get_phase();
#else
        const auto time = wrapper->link.clock().micros();
        auto sessionState = wrapper->link.captureAppSessionState();
        
        state.enabled = wrapper->link.isEnabled() ? 1 : 0;
        state.connected = wrapper->link.numPeers() > 0 ? 1 : 0;
        state.num_peers = static_cast<int>(wrapper->link.numPeers());
        state.link_bpm = static_cast<float>(sessionState.tempo());
        state.start_stop_sync = wrapper->link.isStartStopSyncEnabled() ? 1 : 0;
        state.is_playing = sessionState.isPlaying() ? 1 : 0;
        
        // For beat and phase calculation, we use a default quantum of 4
        // This should be managed by the application
        const double quantum = 4.0;
        state.quantum = static_cast<float>(quantum);
        state.link_beat = static_cast<float>(sessionState.beatAtTime(time, quantum));
        state.link_phase = static_cast<float>(sessionState.phaseAtTime(time, quantum));
#endif
    }
    
    return state;
}

void link_force_peers_rescan(void* link_handle) {
    if (link_handle) {
#ifdef USE_FAKE_LINK
        // No-op in fake mode
#else
        // Link automatically discovers peers, but we can enable/disable to force a rescan
        auto* wrapper = static_cast<LinkWrapper*>(link_handle);
        bool was_enabled = wrapper->link.isEnabled();
        if (was_enabled) {
            wrapper->link.enable(false);
            wrapper->link.enable(true);
        }
#endif
    }
}

} // extern "C"