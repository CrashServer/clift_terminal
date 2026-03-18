#ifndef SIMULATED_AUDIO_H
#define SIMULATED_AUDIO_H

// Fill AudioData with procedural curves when PipeWire not available
// audio: pointer to AudioData struct (void* to avoid type conflict)
// time: total elapsed seconds
// intensity: director global intensity 0.0-1.0
void simulated_audio_update(void* audio, float time, float intensity);

#endif // SIMULATED_AUDIO_H
