#ifndef FFT_H
#define FFT_H

#include <stddef.h>

void fft_compute(float *in_out_real, float *in_out_imag, size_t n);
void fft_apply_window(float *buffer, size_t n);

#endif // FFT_H
