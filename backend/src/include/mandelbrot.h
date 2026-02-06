#ifndef _MANDELBROT_H_
#define _MANDELBROT_H_
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <immintrin.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tpool.h"
#include "queue.h"
#include "common.h"
// ==========================================
// BENCHMARK CONFIGURATION
// ==========================================
#define MAX_ITER 5000
#define THREAD_COUNT 12

#define CENTER_X  -0.743643887037158704752191506114774
#define CENTER_Y  0.131825904205311970493132056385139
#ifndef ZOOM
#define ZOOM  0.5
#endif

// END CONFIGURATION

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

#ifndef EGL_DRM_DEVICE_FILE_EXT
#define EGL_DRM_DEVICE_FILE_EXT 0x3233
#endif

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif


typedef struct {
  uint16_t *buffer;
  int start_row;
  int end_row;
  int width;
  int height;
  double x_min;
  double y_min;
  double x_scale;
  double y_scale;
} ThreadData;

void render_simd(uint16_t *buffer, int width, int start_row, int end_row,
                 int max_interations, double x_min, double y_min,
                 double x_scale, double y_scale);
void render_scalar(uint16_t *buffer, int width, int start_row, int end_row,
                   int max_iterations, double x_min, double y_min,
                   double x_scale, double y_scale);
void run_benchmark(const char *name, int threads, RenderMode mode, double width,
                   double height, uint16_t *buffer);

void render_opengl(uint16_t *buffer, int width, int start_row, int end_row,
                   int max_iterations, double x_min, double y_min,
                   double x_scale, double y_scale, RenderMode mode);

int init_opengl_for_vendor(const char *target_vendor);
void render_opengl_frame(uint16_t *buffer, int width, int start_row,
                         int end_row, int max_iterations, double x_min,
                         double y_min, double x_scale, double y_scale);

#endif // _MANDELBROT_H_
