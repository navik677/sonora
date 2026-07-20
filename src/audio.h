#ifndef AUDIO_H
#define AUDIO_H

#include "eq.h"
#include <pthread.h>
#include <stdbool.h>

typedef enum {
    PLAYBACK_STOPPED,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED
} PlaybackState;

typedef struct {
    char path[512];
    int sample_rate;
    int channels;
    double duration;
    double current_time;
    float volume; // 0.0 to 1.5
    PlaybackState state;
    bool seek_requested;
    double seek_time;
    bool track_ended;
    bool force_reload;
    
    // Equalizer
    Equalizer eq;
    bool eq_enabled;
    
    // Realtime levels for visualizer
    float bass_rms;
    float treble_rms;
    float fft_bins[64];
    
    pthread_mutex_t mutex;
} AudioEngine;

extern AudioEngine g_audio;

bool audio_init(void);
void audio_destroy(void);

bool audio_play_file(const char *path);
void audio_pause(void);
void audio_resume(void);
void audio_stop(void);
void audio_seek(double time_seconds);
void audio_set_volume(float vol);
void audio_get_status(PlaybackState *state, double *current, double *total, int *sr, int *ch, float *vol);

#endif // AUDIO_H
