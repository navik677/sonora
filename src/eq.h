#ifndef EQ_H
#define EQ_H

#include <stdbool.h>

#define EQ_NUM_BANDS 10

typedef struct {
    float preamp; // preamp value in dB
    float gains[EQ_NUM_BANDS]; // band gains in dB
    float a[EQ_NUM_BANDS][2]; /* A weights */
    float b[EQ_NUM_BANDS][2]; /* B weights */
    float wqv[2][EQ_NUM_BANDS][2]; /* Circular buffer for W data: [channel][band][history] */
    float gv[2][EQ_NUM_BANDS]; /* Gain factor for each channel and band */
    int active_bands; /* Number of used EQ bands */
    int sample_rate;
    bool enabled;
} Equalizer;

void eq_init(Equalizer *eq, int sample_rate);
void eq_set_sample_rate(Equalizer *eq, int sample_rate);
void eq_set_band_gain(Equalizer *eq, int band, float gain_db);
void eq_set_preamp(Equalizer *eq, float preamp_db);
void eq_process(Equalizer *eq, float *samples, int num_samples, int num_channels);
void eq_reset_state(Equalizer *eq);

#endif // EQ_H
