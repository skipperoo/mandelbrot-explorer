#ifndef _MANDELBROT_H_
#define _MANDELBROT_H_
#include <immintrin.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
void render_simd(uint16_t *buffer, int width, int start_row, int end_row,
                 int max_interations, double x_min, double y_min,
                 double x_scale, double y_scale);
void render_scalar(uint16_t *buffer, int width, int start_row, int end_row,
                   int max_iterations, double x_min, double y_min,
                   double x_scale, double y_scale);
void run_benchmark(const char *name, int threads, int use_avx, double width,
                   double height, uint16_t *buffer);

#endif // _MANDELBROT_H_
