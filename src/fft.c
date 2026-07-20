#include "fft.h"
#include <math.h>

#define PI 3.14159265358979323846

void fft_apply_window(float *buffer, size_t n) {
    for (size_t i = 0; i < n; i++) {
        // Hann window
        float multiplier = 0.5f * (1.0f - cosf(2.0f * PI * i / (n - 1)));
        buffer[i] *= multiplier;
    }
}

// Bit-reversal permutation
static void bit_reverse(float *real, float *imag, size_t n) {
    size_t j = 0;
    for (size_t i = 0; i < n - 1; i++) {
        if (i < j) {
            float temp_real = real[i];
            float temp_imag = imag[i];
            real[i] = real[j];
            imag[i] = imag[j];
            real[j] = temp_real;
            imag[j] = temp_imag;
        }
        size_t k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
}

// Cooley-Tukey radix-2 FFT (in-place)
void fft_compute(float *real, float *imag, size_t n) {
    bit_reverse(real, imag, n);

    for (size_t len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * PI / len;
        float wlen_real = cosf(angle);
        float wlen_imag = sinf(angle);

        for (size_t i = 0; i < n; i += len) {
            float w_real = 1.0f;
            float w_imag = 0.0f;

            for (size_t j = 0; j < len / 2; j++) {
                float u_real = real[i + j];
                float u_imag = imag[i + j];
                
                float v_real = real[i + j + len / 2] * w_real - imag[i + j + len / 2] * w_imag;
                float v_imag = real[i + j + len / 2] * w_imag + imag[i + j + len / 2] * w_real;

                real[i + j] = u_real + v_real;
                imag[i + j] = u_imag + v_imag;
                real[i + j + len / 2] = u_real - v_real;
                imag[i + j + len / 2] = u_imag - v_imag;

                float next_w_real = w_real * wlen_real - w_imag * wlen_imag;
                float next_w_imag = w_real * wlen_imag + w_imag * wlen_real;
                w_real = next_w_real;
                w_imag = next_w_imag;
            }
        }
    }
}
