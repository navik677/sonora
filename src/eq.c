#include "eq.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Q value for band-pass filters 1.2247 = (3/2)^(1/2)
 * Gives 4 dB suppression at Fc*2 and Fc/2 */
#define Q 1.2247449f

/* Center frequencies for band-pass filters (Hz) */
static const float CF[EQ_NUM_BANDS] = {31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
                                        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};

/* 2nd order band-pass filter design */
static void bp2(float *a, float *b, float fc) {
    float th = 2.0f * (float)M_PI * fc;
    float C = (1.0f - tanf(th * Q / 2.0f)) / (1.0f + tanf(th * Q / 2.0f));

    a[0] = (1.0f + C) * cosf(th);
    a[1] = -C;
    b[0] = (1.0f - C) / 2.0f;
    b[1] = -1.005f;
}

static void eq_update_gain_factors(Equalizer *eq) {
    float adj[EQ_NUM_BANDS];

    for (int i = 0; i < EQ_NUM_BANDS; i++) {
        adj[i] = eq->preamp + eq->gains[i];
    }

    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < EQ_NUM_BANDS; i++) {
            eq->gv[c][i] = powf(10.0f, adj[i] / 20.0f) - 1.0f;
        }
    }
}

static void eq_set_format(Equalizer *eq) {
    /* Calculate number of active filters: the center frequency must be less
     * than rate/2Q to avoid singularities in the tangent used in bp2() */
    eq->active_bands = EQ_NUM_BANDS;

    while (eq->active_bands > 0 && CF[eq->active_bands - 1] > (float)eq->sample_rate / (2.005f * Q)) {
        eq->active_bands--;
    }

    /* Generate filter taps */
    for (int k = 0; k < eq->active_bands; k++) {
        bp2(eq->a[k], eq->b[k], CF[k] / (float)eq->sample_rate);
    }

    /* Reset state */
    memset(eq->wqv, 0, sizeof(eq->wqv));
    
    eq_update_gain_factors(eq);
}

void eq_init(Equalizer *eq, int sample_rate) {
    if (sample_rate <= 0) sample_rate = 44100;
    eq->sample_rate = sample_rate;
    eq->preamp = 0.0f;
    eq->enabled = true;
    memset(eq->gains, 0, sizeof(eq->gains));
    eq_set_format(eq);
}

void eq_set_sample_rate(Equalizer *eq, int sample_rate) {
    if (sample_rate <= 0) sample_rate = 44100;
    eq->sample_rate = sample_rate;
    eq_set_format(eq);
}

void eq_set_band_gain(Equalizer *eq, int band, float gain_db) {
    if (band < 0 || band >= EQ_NUM_BANDS) return;
    eq->gains[band] = gain_db;
    eq_update_gain_factors(eq);
}

void eq_set_preamp(Equalizer *eq, float preamp_db) {
    eq->preamp = preamp_db;
    eq_update_gain_factors(eq);
}

void eq_reset_state(Equalizer *eq) {
    memset(eq->wqv, 0, sizeof(eq->wqv));
}

void eq_process(Equalizer *eq, float *samples, int num_samples, int num_channels) {
    if (!eq->enabled || num_channels > 2 || num_channels <= 0) return;

    for (int channel = 0; channel < num_channels; channel++) {
        float *g = eq->gv[channel]; /* Gain factor */
        float *end = samples + num_samples;

        for (float *f = samples + channel; f < end; f += num_channels) {
            float yt = *f; /* Current input sample */

            for (int k = 0; k < eq->active_bands; k++) {
                /* Pointer to circular buffer wq */
                float *wq = eq->wqv[channel][k];
                
                /* Calculate output from AR part of current filter */
                float w = yt * eq->b[k][0] + wq[0] * eq->a[k][0] + wq[1] * eq->a[k][1];

                /* Calculate output from MA part of current filter */
                yt += (w + wq[1] * eq->b[k][1]) * g[k];

                /* Update circular buffer */
                wq[1] = wq[0];
                wq[0] = w;
            }

            /* Calculate output */
            *f = yt;
        }
    }
}
