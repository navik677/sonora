#include "audio.h"
#include "fft.h"
#include <pulse/simple.h>
#include <pulse/error.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

AudioEngine g_audio;
static pthread_t audio_thread;
static bool thread_running = false;
static bool terminate_thread = false;

static inline float soft_limit_float(float x) {
    const float threshold = 0.95f;
    const float limit = 1.0f;
    const float margin = limit - threshold;
    if (x > threshold) {
        return threshold + margin * tanhf((x - threshold) / margin);
    } else if (x < -threshold) {
        return -threshold - margin * tanhf((-x - threshold) / margin);
    }
    return x;
}

static void *audio_thread_func(void *arg) {
    (void)arg;
    
    SNDFILE *sf = NULL;
    SF_INFO sf_info;
    pa_simple *pa = NULL;
    pa_sample_spec ss;
    
    float *buffer = NULL;
    int buffer_frames = 1024;
    int current_samplerate = 0;
    int current_channels = 0;
    char current_path[512] = {0};
    
    while (!terminate_thread) {
        PlaybackState current_state = PLAYBACK_STOPPED;
        bool seek_req = false;
        double seek_tgt = 0.0;
        float vol = 1.0f;
        char local_path[512] = {0};

        pthread_mutex_lock(&g_audio.mutex);
        current_state = g_audio.state;
        seek_req = g_audio.seek_requested;
        seek_tgt = g_audio.seek_time;
        vol = g_audio.volume;
        strncpy(local_path, g_audio.path, sizeof(local_path) - 1);
        pthread_mutex_unlock(&g_audio.mutex);

        if (current_state == PLAYBACK_STOPPED) {
            // Close resources on stop
            if (pa) {
                pa_simple_free(pa);
                pa = NULL;
                current_samplerate = 0;
                current_channels = 0;
            }
            if (sf) {
                sf_close(sf);
                sf = NULL;
                current_path[0] = '\0';
            }
            pthread_mutex_lock(&g_audio.mutex);
            g_audio.bass_rms = 0.0f;
            g_audio.treble_rms = 0.0f;
            for (int i = 0; i < 64; i++) g_audio.fft_bins[i] = 0.0f;
            pthread_mutex_unlock(&g_audio.mutex);
            
            usleep(10000); // 10ms
            continue;
        }

        if (current_state == PLAYBACK_PAUSED) {
            pthread_mutex_lock(&g_audio.mutex);
            g_audio.bass_rms = 0.0f;
            g_audio.treble_rms = 0.0f;
            for (int i = 0; i < 64; i++) g_audio.fft_bins[i] = 0.0f;
            pthread_mutex_unlock(&g_audio.mutex);
            
            // Sleep on pause but keep file open to retain position
            usleep(10000); // 10ms
            continue;
        }

        bool do_reload = false;
        pthread_mutex_lock(&g_audio.mutex);
        if (g_audio.force_reload) {
            do_reload = true;
            g_audio.force_reload = false;
        }
        pthread_mutex_unlock(&g_audio.mutex);

        // We are supposed to be playing. Check if we need to open a new file or if it's already open.
        if (!sf || strcmp(local_path, current_path) != 0 || do_reload) {
            if (sf) {
                sf_close(sf);
                sf = NULL;
            }
            if (pa) {
                pa_simple_free(pa);
                pa = NULL;
                current_samplerate = 0;
                current_channels = 0;
            }
            
            memset(&sf_info, 0, sizeof(sf_info));
            sf = sf_open(local_path, SFM_READ, &sf_info);
            if (!sf) {
                fprintf(stderr, "Failed to open audio file %s: %s\n", local_path, sf_strerror(NULL));
                pthread_mutex_lock(&g_audio.mutex);
                g_audio.state = PLAYBACK_STOPPED;
                g_audio.track_ended = true;
                pthread_mutex_unlock(&g_audio.mutex);
                current_path[0] = '\0';
                continue;
            }

            strncpy(current_path, local_path, sizeof(current_path) - 1);

            pthread_mutex_lock(&g_audio.mutex);
            g_audio.sample_rate = sf_info.samplerate;
            g_audio.channels = sf_info.channels;
            g_audio.duration = (double)sf_info.frames / sf_info.samplerate;
            g_audio.current_time = 0.0;
            g_audio.track_ended = false;
            
            // Update sample rate and recalculate filter coefficients (retains user's settings)
            eq_set_sample_rate(&g_audio.eq, sf_info.samplerate);
            
            pthread_mutex_unlock(&g_audio.mutex);

            buffer = realloc(buffer, buffer_frames * sf_info.channels * sizeof(float));
        }

        // Check if seek was requested
        if (seek_req) {
            sf_count_t target_frame = (sf_count_t)(seek_tgt * sf_info.samplerate);
            sf_seek(sf, target_frame, SEEK_SET);
            
            if (pa) {
                int err;
                pa_simple_flush(pa, &err); // flush pulseaudio buffer to apply seek instantly
            }

            pthread_mutex_lock(&g_audio.mutex);
            g_audio.seek_requested = false;
            g_audio.current_time = seek_tgt;
            eq_reset_state(&g_audio.eq); // clear filter history to prevent clicks
            pthread_mutex_unlock(&g_audio.mutex);
        }

        // Check if PulseAudio needs to be initialized/re-initialized
        if (!pa || current_samplerate != sf_info.samplerate || current_channels != sf_info.channels) {
            if (pa) {
                pa_simple_free(pa);
            }
            
            ss.format = PA_SAMPLE_FLOAT32NE; // Native float format (same as Audacious)
            ss.rate = sf_info.samplerate;
            ss.channels = sf_info.channels;
            
            pa_buffer_attr attr;
            attr.maxlength = (uint32_t)-1;
            attr.tlength = pa_usec_to_bytes(50 * 1000, &ss); // 50ms buffer target for low latency (Audacious feel)
            attr.prebuf = (uint32_t)-1;
            attr.minreq = pa_usec_to_bytes(10 * 1000, &ss);   // 10ms request size
            attr.fragsize = (uint32_t)-1;
            
            int error = 0;
            pa = pa_simple_new(NULL, "Sonora Player", PA_STREAM_PLAYBACK, NULL, "Music Playback", &ss, NULL, &attr, &error);
            if (!pa) {
                fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
                pthread_mutex_lock(&g_audio.mutex);
                g_audio.state = PLAYBACK_STOPPED;
                pthread_mutex_unlock(&g_audio.mutex);
                continue;
            }
            
            current_samplerate = sf_info.samplerate;
            current_channels = sf_info.channels;
        }

        // Read a chunk of samples
        sf_count_t read_frames = sf_readf_float(sf, buffer, buffer_frames);
        if (read_frames <= 0) {
            // Track ended
            pthread_mutex_lock(&g_audio.mutex);
            g_audio.state = PLAYBACK_STOPPED;
            g_audio.track_ended = true;
            g_audio.current_time = g_audio.duration;
            pthread_mutex_unlock(&g_audio.mutex);
            continue;
        }

        // Process audio through EQ (outputs to buffer)
        pthread_mutex_lock(&g_audio.mutex);
        if (g_audio.eq_enabled) {
            eq_process(&g_audio.eq, buffer, read_frames * sf_info.channels, sf_info.channels);
        }
        pthread_mutex_unlock(&g_audio.mutex);

        // Apply volume
        if (vol != 1.0f) {
            for (int i = 0; i < read_frames * sf_info.channels; i++) {
                buffer[i] *= vol;
            }
        }

        // Push mono mix into sliding window
        static float fft_window[2048] = {0};
        static int fft_win_pos = 0;
        
        for (int i = 0; i < read_frames; i++) {
            float mono = 0;
            for (int ch = 0; ch < sf_info.channels; ch++) {
                mono += buffer[i * sf_info.channels + ch];
            }
            mono /= sf_info.channels;
            
            fft_window[fft_win_pos++] = mono;
            if (fft_win_pos >= 2048) {
                // Compute FFT
                float real[2048];
                float imag[2048] = {0};
                for (int j = 0; j < 2048; j++) real[j] = fft_window[j];
                
                fft_apply_window(real, 2048);
                fft_compute(real, imag, 2048);
                
                // Group into 64 bins logarithmically
                float local_bins[64] = {0};
                float nyquist = sf_info.samplerate / 2.0f;
                float min_freq = 20.0f;
                float max_freq = 20000.0f;
                if (nyquist < max_freq) max_freq = nyquist;
                
                for (int b = 0; b < 64; b++) {
                    float f_start = min_freq * powf(max_freq / min_freq, (float)b / 64.0f);
                    float f_end = min_freq * powf(max_freq / min_freq, (float)(b + 1) / 64.0f);
                    
                    int i_start = (int)(f_start / nyquist * 1024); // 1024 is Nyquist bin
                    int i_end = (int)(f_end / nyquist * 1024);
                    if (i_start < 1) i_start = 1; // skip DC
                    if (i_end <= i_start) i_end = i_start + 1;
                    if (i_end > 1024) i_end = 1024;
                    
                    float max_mag = 0;
                    for (int j = i_start; j < i_end; j++) {
                        float mag = sqrtf(real[j]*real[j] + imag[j]*imag[j]);
                        if (mag > max_mag) max_mag = mag;
                    }
                    local_bins[b] = sqrtf(max_mag) * 0.05f; // normalize
                }
                
                pthread_mutex_lock(&g_audio.mutex);
                for (int b = 0; b < 64; b++) {
                    g_audio.fft_bins[b] = local_bins[b];
                }
                pthread_mutex_unlock(&g_audio.mutex);
                
                fft_win_pos = 0; // reset window
                
                // Keep the overlapping for smoother visualizer (overlap by 1024)
                for (int j = 0; j < 1024; j++) {
                    fft_window[j] = fft_window[j + 1024];
                }
                fft_win_pos = 1024;
            }
        }

        // Apply final soft-limiting safety barrier to prevent any DAC clipping
        for (int i = 0; i < read_frames * sf_info.channels; i++) {
            buffer[i] = soft_limit_float(buffer[i]);
        }

        // Write to PulseAudio (sending float samples directly, zero conversions!)
        int error = 0;
        if (pa_simple_write(pa, buffer, read_frames * sf_info.channels * sizeof(float), &error) < 0) {
            fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
        }

        // Update current time and visualizer levels
        pthread_mutex_lock(&g_audio.mutex);
        if (!g_audio.seek_requested) {
            g_audio.current_time += (double)read_frames / sf_info.samplerate;
        }

        pthread_mutex_unlock(&g_audio.mutex);
    }

    if (pa) pa_simple_free(pa);
    if (sf) sf_close(sf);
    free(buffer);
    return NULL;
}

bool audio_init(void) {
    memset(&g_audio, 0, sizeof(g_audio));
    g_audio.volume = 1.0f;
    g_audio.state = PLAYBACK_STOPPED;
    g_audio.eq_enabled = true;
    pthread_mutex_init(&g_audio.mutex, NULL);
    
    // Init EQ with 44100 rate initially
    eq_init(&g_audio.eq, 44100);

    terminate_thread = false;
    if (pthread_create(&audio_thread, NULL, audio_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create audio thread\n");
        return false;
    }
    thread_running = true;
    return true;
}

void audio_destroy(void) {
    if (thread_running) {
        terminate_thread = true;
        pthread_join(audio_thread, NULL);
        thread_running = false;
    }
    pthread_mutex_destroy(&g_audio.mutex);
}

bool audio_play_file(const char *path) {
    pthread_mutex_lock(&g_audio.mutex);
    strncpy(g_audio.path, path, sizeof(g_audio.path) - 1);
    g_audio.state = PLAYBACK_PLAYING;
    g_audio.seek_requested = false;
    g_audio.track_ended = false;
    g_audio.force_reload = true;
    pthread_mutex_unlock(&g_audio.mutex);
    return true;
}

void audio_pause(void) {
    pthread_mutex_lock(&g_audio.mutex);
    if (g_audio.state == PLAYBACK_PLAYING) {
        g_audio.state = PLAYBACK_PAUSED;
    }
    pthread_mutex_unlock(&g_audio.mutex);
}

void audio_resume(void) {
    pthread_mutex_lock(&g_audio.mutex);
    if (g_audio.state == PLAYBACK_PAUSED) {
        g_audio.state = PLAYBACK_PLAYING;
    }
    pthread_mutex_unlock(&g_audio.mutex);
}

void audio_stop(void) {
    pthread_mutex_lock(&g_audio.mutex);
    g_audio.state = PLAYBACK_STOPPED;
    g_audio.current_time = 0.0;
    pthread_mutex_unlock(&g_audio.mutex);
}

void audio_seek(double time_seconds) {
    pthread_mutex_lock(&g_audio.mutex);
    if (strncmp(g_audio.path, "/tmp/sonora_stream", 18) == 0) {
        pthread_mutex_unlock(&g_audio.mutex);
        return; // Seeking not supported on streams
    }
    if (time_seconds < 0.0) time_seconds = 0.0;
    if (time_seconds > g_audio.duration) time_seconds = g_audio.duration;
    g_audio.seek_time = time_seconds;
    g_audio.seek_requested = true;
    pthread_mutex_unlock(&g_audio.mutex);
}

void audio_set_volume(float vol) {
    pthread_mutex_lock(&g_audio.mutex);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.5f) vol = 1.5f; // allow slight boost
    g_audio.volume = vol;
    pthread_mutex_unlock(&g_audio.mutex);
}

void audio_get_status(PlaybackState *state, double *current, double *total, int *sr, int *ch, float *vol) {
    pthread_mutex_lock(&g_audio.mutex);
    if (state) *state = g_audio.state;
    if (current) *current = g_audio.current_time;
    if (total) *total = g_audio.duration;
    if (sr) *sr = g_audio.sample_rate;
    if (ch) *ch = g_audio.channels;
    if (vol) *vol = g_audio.volume;
    pthread_mutex_unlock(&g_audio.mutex);
}
