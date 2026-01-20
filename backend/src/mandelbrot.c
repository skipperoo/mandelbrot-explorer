#include "include/mandelbrot.h"

// ==========================================
// CONFIGURATION
// ==========================================
#define WIDTH 1920
#define HEIGHT 1080
#define MAX_ITER 1000
#define THREAD_COUNT 12 // Set this to your logical core count

static char current_gpu_vendor[64] = "None";

// Viewport settings
const double CENTER_X = -0.743643887037158704752191506114774;
const double CENTER_Y = 0.131825904205311970493132056385139;
const double ZOOM = 0.5; // Scale
// Shared Data Structure for Threads
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

// ==========================================
// 1. SCALAR KERNEL (Reference Implementation)
// ==========================================
void render_scalar(uint16_t *buffer, int width, int start_row, int end_row,
                   int max_iterations, double x_min, double y_min,
                   double x_scale, double y_scale) {

  for (int py = start_row; py < end_row; py++) {
    double y0 = y_min + py * y_scale;
    for (int px = 0; px < width; px++) {
      double x0 = x_min + px * x_scale;

      double x = 0.0, y = 0.0, x2 = 0.0, y2 = 0.0;
      int iter = 0;

      while ((x2 + y2 <= 4.0) && (iter < max_iterations)) {
        y = 2 * x * y + y0;
        x = x2 - y2 + x0;
        x2 = x * x;
        y2 = y * y;
        iter++;
      }

      // Store Iteration Count directly (2 bytes)
      buffer[py * width + px] = (uint16_t)iter;
    }
  }
}

void render_simd(uint16_t *buffer, int width, int start_row, int end_row,
                 int max_iterations, double x_min, double y_min, double x_scale,
                 double y_scale) {

/* -------------------------------------------------------------------------
   PATH 1: AVX-512 (Processes 8 pixels per loop)
   Requires: -mavx512f (GCC/Clang) or /arch:AVX512 (MSVC)
   ------------------------------------------------------------------------- */
#if defined(__AVX512F__)
  __m512d v_four = _mm512_set1_pd(4.0);
  __m512d v_x_scale = _mm512_set1_pd(x_scale);
  __m512d v_x_offsets = _mm512_set_pd(7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0);
  v_x_offsets = _mm512_mul_pd(v_x_offsets, v_x_scale);

  for (int py = start_row; py < end_row; py++) {
    double y0_scalar = y_min + py * y_scale;
    __m512d v_y0 = _mm512_set1_pd(y0_scalar);

    for (int px = 0; px < width; px += 8) {
      double base_x = x_min + px * x_scale;
      __m512d v_x0 = _mm512_add_pd(_mm512_set1_pd(base_x), v_x_offsets);

      __m512d v_x = _mm512_setzero_pd();
      __m512d v_y = _mm512_setzero_pd();
      __m512d v_x2 = _mm512_setzero_pd();
      __m512d v_y2 = _mm512_setzero_pd();

      // AVX-512 allows efficient integer counting in 512-bit registers
      __m512i v_iter = _mm512_setzero_si512();
      __m512i v_one_int = _mm512_set1_epi64(1);
      __mmask8 mask = 0xFF; // 8 bits, one for each double

      for (int i = 0; i < max_iterations; i++) {
        __m512d v_xy = _mm512_mul_pd(v_x, v_y);
        v_y = _mm512_add_pd(_mm512_add_pd(v_xy, v_xy), v_y0);
        v_x = _mm512_add_pd(_mm512_sub_pd(v_x2, v_y2), v_x0);
        v_x2 = _mm512_mul_pd(v_x, v_x);
        v_y2 = _mm512_mul_pd(v_y, v_y);
        __m512d v_mag = _mm512_add_pd(v_x2, v_y2);

        // Update mask: 1 if mag <= 4.0
        mask = _mm512_cmp_pd_mask(v_mag, v_four, _CMP_LE_OQ);

        if (mask == 0)
          break;

        // Add 1 to v_iter wherever the mask is still active
        v_iter = _mm512_mask_add_epi64(v_iter, mask, v_iter, v_one_int);
      }

      // Downcast 64-bit int -> 32-bit int -> 16-bit int
      __m256i v_iter_32 =
          _mm512_cvtepi64_epi32(v_iter); // 512b (64x8) -> 256b (32x8)
      __m128i v_iter_16 =
          _mm256_cvtepi32_epi16(v_iter_32); // 256b (32x8) -> 128b (16x8)

      _mm_storeu_si128((__m128i *)&buffer[py * width + px], v_iter_16);
    }
  }

/* -------------------------------------------------------------------------
   PATH 2: AVX2 (Processes 4 pixels per loop)
   Requires: -mavx2 (GCC/Clang) or /arch:AVX2 (MSVC)
   ------------------------------------------------------------------------- */
#elif defined(__AVX2__)
  __m256d v_four = _mm256_set1_pd(4.0);
  __m256d v_x_scale = _mm256_set1_pd(x_scale);
  __m256d v_x_offsets = _mm256_set_pd(3.0, 2.0, 1.0, 0.0);
  v_x_offsets = _mm256_mul_pd(v_x_offsets, v_x_scale);

  // Original Index Permutation Logic
  __m256i v_pack_idx = _mm256_set_epi32(0, 0, 0, 0, 6, 4, 2, 0);

  for (int py = start_row; py < end_row; py++) {
    double y0_scalar = y_min + py * y_scale;
    __m256d v_y0 = _mm256_set1_pd(y0_scalar);

    for (int px = 0; px < width; px += 4) {
      double base_x = x_min + px * x_scale;
      __m256d v_x0 = _mm256_add_pd(_mm256_set1_pd(base_x), v_x_offsets);
      __m256d v_x = _mm256_setzero_pd();
      __m256d v_y = _mm256_setzero_pd();
      __m256d v_x2 = _mm256_setzero_pd();
      __m256d v_y2 = _mm256_setzero_pd();

      __m256i v_iter = _mm256_setzero_si256();
      __m256d mask = _mm256_castsi256_pd(_mm256_set1_epi64x(-1));

      for (int i = 0; i < max_iterations; i++) {
        if (_mm256_movemask_pd(mask) == 0)
          break;

        __m256d v_xy = _mm256_mul_pd(v_x, v_y);
        v_y = _mm256_add_pd(_mm256_add_pd(v_xy, v_xy), v_y0);
        v_x = _mm256_add_pd(_mm256_sub_pd(v_x2, v_y2), v_x0);
        v_x2 = _mm256_mul_pd(v_x, v_x);
        v_y2 = _mm256_mul_pd(v_y, v_y);
        __m256d v_mag = _mm256_add_pd(v_x2, v_y2);

        mask = _mm256_cmp_pd(v_mag, v_four, _CMP_LE_OQ);

        // AVX2 Specific: Integer subtraction on 256-bit registers
        v_iter = _mm256_sub_epi64(v_iter, _mm256_castpd_si256(mask));
      }

      // Packing AVX2
      __m256i v_iter_32 = _mm256_permutevar8x32_epi32(v_iter, v_pack_idx);
      __m128i v_low_128 = _mm256_castsi256_si128(v_iter_32);
      __m128i v_iter_16 = _mm_packus_epi32(v_low_128, _mm_setzero_si128());
      _mm_storel_epi64((__m128i *)&buffer[py * width + px], v_iter_16);
    }
  }

/* -------------------------------------------------------------------------
   PATH 3: AVX (v1) Legacy (Processes 4 pixels per loop)
   Requires: -mavx (GCC/Clang) or /arch:AVX (MSVC)
   NOTE: AVX1 cannot do 256-bit integer math. We use doubles for counting.
   ------------------------------------------------------------------------- */
#elif defined(__AVX__)
  __m256d v_four = _mm256_set1_pd(4.0);
  __m256d v_x_scale = _mm256_set1_pd(x_scale);
  __m256d v_x_offsets = _mm256_set_pd(3.0, 2.0, 1.0, 0.0);
  v_x_offsets = _mm256_mul_pd(v_x_offsets, v_x_scale);

  // Constant 1.0 to increment our floating point counter
  __m256d v_one_pd = _mm256_set1_pd(1.0);

  for (int py = start_row; py < end_row; py++) {
    double y0_scalar = y_min + py * y_scale;
    __m256d v_y0 = _mm256_set1_pd(y0_scalar);

    for (int px = 0; px < width; px += 4) {
      double base_x = x_min + px * x_scale;
      __m256d v_x0 = _mm256_add_pd(_mm256_set1_pd(base_x), v_x_offsets);
      __m256d v_x = _mm256_setzero_pd();
      __m256d v_y = _mm256_setzero_pd();
      __m256d v_x2 = _mm256_setzero_pd();
      __m256d v_y2 = _mm256_setzero_pd();

      // KEY DIFFERENCE: Use double precision float as counter
      __m256d v_iter_dbl = _mm256_setzero_pd();

      // Mask is all 1s (NaN in float representation, but bits are all 1)
      __m256d mask = _mm256_cmp_pd(v_four, v_four, _CMP_EQ_OQ);

      for (int i = 0; i < max_iterations; i++) {
        if (_mm256_movemask_pd(mask) == 0)
          break;

        __m256d v_xy = _mm256_mul_pd(v_x, v_y);
        v_y = _mm256_add_pd(_mm256_add_pd(v_xy, v_xy), v_y0);
        v_x = _mm256_add_pd(_mm256_sub_pd(v_x2, v_y2), v_x0);
        v_x2 = _mm256_mul_pd(v_x, v_x);
        v_y2 = _mm256_mul_pd(v_y, v_y);
        __m256d v_mag = _mm256_add_pd(v_x2, v_y2);

        mask = _mm256_cmp_pd(v_mag, v_four, _CMP_LE_OQ);

        // AVX1 Workaround:
        // 1. mask has all bits set (NaN) or zero.
        // 2. We use _mm256_and_pd to effectively select '1.0' where mask is
        // true
        __m256d inc = _mm256_and_pd(mask, v_one_pd);

        // 3. Add the floating point increment
        v_iter_dbl = _mm256_add_pd(v_iter_dbl, inc);
      }

      // Convert doubles to 32-bit integers
      // _mm256_cvttpd_epi32 converts 4 doubles to 4 ints (in the lower 128
      // bits)
      __m128i v_iter_32 = _mm256_cvttpd_epi32(v_iter_dbl);

      // Pack 32-bit to 16-bit
      __m128i v_iter_16 = _mm_packus_epi32(v_iter_32, _mm_setzero_si128());

      // Store low 64 bits
      _mm_storel_epi64((__m128i *)&buffer[py * width + px], v_iter_16);
    }
  }
#endif
}

// ==========================================
// 4. OPENGL KERNEL (GPU Acceleration)
// ==========================================

// Global GL function pointers
static PFNGLCREATESHADERPROC glCreateShader_ptr;
static PFNGLSHADERSOURCEPROC glShaderSource_ptr;
static PFNGLCOMPILESHADERPROC glCompileShader_ptr;
static PFNGLGETSHADERIVPROC glGetShaderiv_ptr;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_ptr;
static PFNGLCREATEPROGRAMPROC glCreateProgram_ptr;
static PFNGLATTACHSHADERPROC glAttachShader_ptr;
static PFNGLLINKPROGRAMPROC glLinkProgram_ptr;
static PFNGLGETPROGRAMIVPROC glGetProgramiv_ptr;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog_ptr;
static PFNGLUSEPROGRAMPROC glUseProgram_ptr;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation_ptr;
static PFNGLUNIFORM1IPROC glUniform1i_ptr;
static PFNGLUNIFORM1DPROC glUniform1d_ptr;
static PFNGLGENBUFFERSPROC glGenBuffers_ptr;
static PFNGLBINDBUFFERPROC glBindBuffer_ptr;
static PFNGLBUFFERDATAPROC glBufferData_ptr;
static PFNGLDISPATCHCOMPUTEPROC glDispatchCompute_ptr;
static PFNGLMEMORYBARRIERPROC glMemoryBarrier_ptr;
static PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData_ptr;
static PFNGLBINDBUFFERBASEPROC glBindBufferBase_ptr;

static EGLDisplay egl_display = EGL_NO_DISPLAY;
static EGLContext egl_context = EGL_NO_CONTEXT;
static GLuint gl_program = 0;
static GLuint gl_ssbo = 0;
static size_t gl_ssbo_size = 0;
static pthread_mutex_t gl_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef const GLubyte *(*PFNGLGETSTRINGPROC)(GLenum);

const char *compute_shader_source =
    "#version 430 core\n"
    "#extension GL_ARB_gpu_shader_fp64 : enable\n"
    "layout(local_size_x = 16, local_size_y = 16) in;\n"
    "layout(std430, binding = 0) writeonly buffer OutputBuffer {\n"
    "    uint iterations[];\n"
    "};\n"
    "uniform int width;\n"
    "uniform int rows;\n"
    "uniform int max_iterations;\n"
    "uniform double x_min;\n"
    "uniform double y_min;\n"
    "uniform double x_scale;\n"
    "uniform double y_scale;\n"
    "uniform int start_row;\n"
    "void main() {\n"
    "    uint px = gl_GlobalInvocationID.x;\n"
    "    uint py = gl_GlobalInvocationID.y;\n"
    "    if (px >= width || py >= uint(rows)) return;\n"
    "    double x0 = x_min + double(px) * x_scale;\n"
    "    double y0 = y_min + double(py + uint(start_row)) * y_scale;\n"
    "    double x = 0.0, y = 0.0, x2 = 0.0, y2 = 0.0;\n"
    "    int iter = 0;\n"
    "    while (x2 + y2 <= 4.0 && iter < max_iterations) {\n"
    "        y = 2.0 * x * y + y0;\n"
    "        x = x2 - y2 + x0;\n"
    "        x2 = x * x;\n"
    "        y2 = y * y;\n"
    "        iter++;\n"
    "    }\n"
    "    iterations[py * width + px] = uint(iter);\n"
    "}\n";

void shutdown_opengl() {
  if (egl_display != EGL_NO_DISPLAY) {
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl_context != EGL_NO_CONTEXT)
      eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);
    egl_display = EGL_NO_DISPLAY;
    egl_context = EGL_NO_CONTEXT;
  }
  strcpy(current_gpu_vendor, "None");
}

void setup_opengl_resources() {
  // 1. Load Pointers (Crucial! Pointers might change between drivers)
  glCreateShader_ptr =
      (PFNGLCREATESHADERPROC)eglGetProcAddress("glCreateShader");
  glShaderSource_ptr =
      (PFNGLSHADERSOURCEPROC)eglGetProcAddress("glShaderSource");
  glCompileShader_ptr =
      (PFNGLCOMPILESHADERPROC)eglGetProcAddress("glCompileShader");
  glGetShaderiv_ptr = (PFNGLGETSHADERIVPROC)eglGetProcAddress("glGetShaderiv");
  glGetShaderInfoLog_ptr =
      (PFNGLGETSHADERINFOLOGPROC)eglGetProcAddress("glGetShaderInfoLog");
  glCreateProgram_ptr =
      (PFNGLCREATEPROGRAMPROC)eglGetProcAddress("glCreateProgram");
  glAttachShader_ptr =
      (PFNGLATTACHSHADERPROC)eglGetProcAddress("glAttachShader");
  glLinkProgram_ptr = (PFNGLLINKPROGRAMPROC)eglGetProcAddress("glLinkProgram");
  glGetProgramiv_ptr =
      (PFNGLGETPROGRAMIVPROC)eglGetProcAddress("glGetProgramiv");
  glGetProgramInfoLog_ptr =
      (PFNGLGETPROGRAMINFOLOGPROC)eglGetProcAddress("glGetProgramInfoLog");
  glUseProgram_ptr = (PFNGLUSEPROGRAMPROC)eglGetProcAddress("glUseProgram");
  glUniform1i_ptr = (PFNGLUNIFORM1IPROC)eglGetProcAddress("glUniform1i");
  glUniform1d_ptr = (PFNGLUNIFORM1DPROC)eglGetProcAddress("glUniform1d");
  glGenBuffers_ptr = (PFNGLGENBUFFERSPROC)eglGetProcAddress("glGenBuffers");
  glBindBuffer_ptr = (PFNGLBINDBUFFERPROC)eglGetProcAddress("glBindBuffer");
  glBufferData_ptr = (PFNGLBUFFERDATAPROC)eglGetProcAddress("glBufferData");
  glDispatchCompute_ptr =
      (PFNGLDISPATCHCOMPUTEPROC)eglGetProcAddress("glDispatchCompute");
  glMemoryBarrier_ptr =
      (PFNGLMEMORYBARRIERPROC)eglGetProcAddress("glMemoryBarrier");
  glGetBufferSubData_ptr =
      (PFNGLGETBUFFERSUBDATAPROC)eglGetProcAddress("glGetBufferSubData");
  glBindBufferBase_ptr =
      (PFNGLBINDBUFFERBASEPROC)eglGetProcAddress("glBindBufferBase");
  glGetUniformLocation_ptr =
      (PFNGLGETUNIFORMLOCATIONPROC)eglGetProcAddress("glGetUniformLocation");

  // 2. Compile Shader (Code from your original init_opengl_internal)
  GLuint shader = glCreateShader_ptr(GL_COMPUTE_SHADER);
  glShaderSource_ptr(shader, 1, &compute_shader_source, NULL);
  glCompileShader_ptr(shader);

  // ... (Add your error checking here) ...

  gl_program = glCreateProgram_ptr();
  glAttachShader_ptr(gl_program, shader);
  glLinkProgram_ptr(gl_program);

  // ... (Add your linking error checking here) ...

  // 3. Create Buffer
  glGenBuffers_ptr(1, &gl_ssbo);
}

static int init_opengl(const char *target_vendor) {
  // If already initialized for this vendor, do nothing
  if (strstr(current_gpu_vendor, target_vendor) != NULL) {
    return 1; // Success, already loaded
  }

  // Otherwise, perform a full teardown and search
  shutdown_opengl();
  // printf("EGL: Switching to %s GPU...\n", target_vendor);

  // Get EGL Extension pointers
  PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT =
      (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  PFNEGLGETPLATFORMDISPLAYPROC eglGetPlatformDisplay =
      (PFNEGLGETPLATFORMDISPLAYPROC)eglGetProcAddress("eglGetPlatformDisplay");

  if (!eglQueryDevicesEXT || !eglGetPlatformDisplay) {
    fprintf(stderr, "EGL: Device enumeration extensions missing.\n");
    return 0;
  }

#define MAX_DEVICES 16
  EGLDeviceEXT devices[MAX_DEVICES];
  EGLint num_devices;
  eglQueryDevicesEXT(MAX_DEVICES, devices, &num_devices);

  for (int i = 0; i < num_devices; i++) {
    // Try to initialize this device to peek at its Vendor String
    EGLDisplay attempt_dpy =
        eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[i], NULL);
    if (attempt_dpy == EGL_NO_DISPLAY)
      continue;

    if (eglInitialize(attempt_dpy, NULL, NULL)) {

      // Context binding is required to read GL_VENDOR strings in some drivers
      // We create a temp config/context just to check the name
      eglBindAPI(EGL_OPENGL_API);
      EGLConfig config;
      EGLint num_config;
      EGLint attr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                       EGL_OPENGL_BIT, EGL_NONE};
      eglChooseConfig(attempt_dpy, attr, &config, 1, &num_config);
      if (!eglChooseConfig(attempt_dpy, attr, &config, 1, &num_config) ||
          num_config == 0) {
        // Fallback: Try "Surfaceless" (common in Mesa for pure compute)
        // Some drivers don't even support PBUFFER, but support 0 (Surfaceless)
        EGLint surfaceless_attr[] = {EGL_SURFACE_TYPE, EGL_NONE,
                                     EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                     EGL_NONE};
        eglChooseConfig(attempt_dpy, surfaceless_attr, &config, 1, &num_config);
      }
      if (num_config > 0) {

        EGLContext temp_ctx = eglCreateContext(
            attempt_dpy, config, EGL_NO_CONTEXT,
            (EGLint[]){EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE});
        if (eglMakeCurrent(attempt_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           temp_ctx)) {

          // --- CHECK VENDOR ---
          const char *vendor = (const char *)glGetString(GL_VENDOR);
          const char *renderer = (const char *)glGetString(GL_RENDERER);

          // Case-insensitive check (simplified)
          int match = 0;
          // printf("%s - %s\n", vendor, renderer);
          if (target_vendor && vendor && strstr(vendor, target_vendor))
            match = 1;
          if (target_vendor && renderer && strstr(renderer, target_vendor))
            match = 1;
          // Special case: Intel is sometimes "Mesa" or "Iris"
          if (strcmp(target_vendor, "Intel") == 0 && renderer &&
              strstr(renderer, "Intel"))
            match = 1;

          if (match) {
            // printf("EGL: Match Found! Vendor: %s | Renderer: %s\n", vendor,
            // renderer);

            // 1. Destroy the temporary check-context
            eglMakeCurrent(attempt_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
            eglDestroyContext(attempt_dpy, temp_ctx);

            // 2. Create the REAL Context (OpenGL 4.3+)
            EGLint real_context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 4,
                                             EGL_CONTEXT_MINOR_VERSION, 3,
                                             EGL_NONE};

            // Re-use the config we found earlier
            EGLContext real_ctx = eglCreateContext(
                attempt_dpy, config, EGL_NO_CONTEXT, real_context_attribs);

            if (real_ctx == EGL_NO_CONTEXT) {
              fprintf(stderr,
                      "EGL: Failed to create 4.3 context on target device.\n");
              return 0;
            }

            if (!eglMakeCurrent(attempt_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                                real_ctx)) {
              fprintf(stderr, "EGL: Failed to make real context current.\n");
              return 0;
            }

            // 3. Set Global State
            egl_display = attempt_dpy;
            egl_context = real_ctx;
            strncpy(current_gpu_vendor, target_vendor, 63);

            // 4. CRITICAL: Re-load functions and Re-compile shaders for this
            // new context
            setup_opengl_resources();

            return 1;
          }
        }
        // Cleanup temp context if no match
        eglMakeCurrent(attempt_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroyContext(attempt_dpy, temp_ctx);
      }
      eglTerminate(attempt_dpy);
    }
  }

  fprintf(stderr, "EGL: Could not find a GPU matching '%s'\n", target_vendor);
  return 0;
}
void render_opengl(uint16_t *buffer, int width, int start_row, int end_row,
                   int max_iterations, double x_min, double y_min,
                   double x_scale, double y_scale, RenderMode mode) {
  pthread_mutex_lock(&gl_mutex);
  const char *target = "Unknown";
  if (mode == INTEL_GPU)
    target = "Intel";
  if (mode == NVIDIA_GPU)
    target = "NVIDIA";

  if (!init_opengl(target)) {
    return; // Skip if hardware not found
  }

  int rows = end_row - start_row;
  if (rows <= 0 || width <= 0) {
    pthread_mutex_unlock(&gl_mutex);
    return;
  }

  size_t required_size = width * rows * sizeof(uint32_t);

  if (!eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      egl_context)) {
    pthread_mutex_unlock(&gl_mutex);
    return;
  }

  if (required_size > gl_ssbo_size) {
    glBindBuffer_ptr(GL_SHADER_STORAGE_BUFFER, gl_ssbo);
    glBufferData_ptr(GL_SHADER_STORAGE_BUFFER, required_size, NULL,
                     GL_DYNAMIC_COPY);
    gl_ssbo_size = required_size;
  }

  glUseProgram_ptr(gl_program);
  glUniform1i_ptr(glGetUniformLocation_ptr(gl_program, "width"), width);
  glUniform1i_ptr(glGetUniformLocation_ptr(gl_program, "rows"), rows);
  glUniform1i_ptr(glGetUniformLocation_ptr(gl_program, "max_iterations"),
                  max_iterations);
  glUniform1d_ptr(glGetUniformLocation_ptr(gl_program, "x_min"), x_min);
  glUniform1d_ptr(glGetUniformLocation_ptr(gl_program, "y_min"), y_min);
  glUniform1d_ptr(glGetUniformLocation_ptr(gl_program, "x_scale"), x_scale);
  glUniform1d_ptr(glGetUniformLocation_ptr(gl_program, "y_scale"), y_scale);
  glUniform1i_ptr(glGetUniformLocation_ptr(gl_program, "start_row"), start_row);

  glBindBuffer_ptr(GL_SHADER_STORAGE_BUFFER, gl_ssbo);
  glBindBufferBase_ptr(GL_SHADER_STORAGE_BUFFER, 0, gl_ssbo);

  glDispatchCompute_ptr((width + 15) / 16, (rows + 15) / 16, 1);
  glMemoryBarrier_ptr(GL_SHADER_STORAGE_BARRIER_BIT);

  uint32_t *temp_buf = malloc(required_size);
  if (temp_buf) {
    glGetBufferSubData_ptr(GL_SHADER_STORAGE_BUFFER, 0, required_size,
                           temp_buf);
    for (int i = 0; i < width * rows; i++) {
      buffer[start_row * width + i] = (uint16_t)temp_buf[i];
    }
    free(temp_buf);
  }

  eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  pthread_mutex_unlock(&gl_mutex);
}

// ==========================================
// THREAD WRAPPERS
// ==========================================

void *thread_scalar(void *arg) {
  ThreadData *data = (ThreadData *)arg;
  render_scalar(data->buffer, data->width, data->start_row, data->end_row,
                MAX_ITER, data->x_min, data->y_min, data->x_scale,
                data->y_scale);
  return NULL;
}

void *thread_avx(void *arg) {
  ThreadData *data = (ThreadData *)arg;
  render_simd(data->buffer, data->width, data->start_row, data->end_row,
              MAX_ITER, data->x_min, data->y_min, data->x_scale, data->y_scale);
  return NULL;
}

// ==========================================
// HELPERS
// ==========================================

double get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

void run_benchmark(const char *name, int threads, RenderMode mode, double width,
                   double height, uint16_t *buffer) {
  double start_time = get_time();
  double end_time = 0;

  // Viewport math
  double aspect = (double)width / height;
  double target_width = 3.5 / ZOOM;
  double target_height = target_width / aspect;
  double x_min = CENTER_X - target_width / 2.0;
  double y_min = CENTER_Y - target_height / 2.0;
  double x_scale = target_width / width;
  double y_scale = target_height / height;

  pthread_t thread_ids[threads];
  ThreadData t_data[threads];

  int rows_per_thread = height / threads;
  if (mode == INTEL_GPU || mode == NVIDIA_GPU) {
    render_opengl(buffer, width, 0, height, MAX_ITER, x_min, y_min, x_scale,
                  y_scale, mode);
    goto bench_done;
  }

  for (int i = 0; i < threads; i++) {
    t_data[i].buffer = buffer;
    t_data[i].width = width;
    t_data[i].height = height;
    t_data[i].start_row = i * rows_per_thread;
    t_data[i].end_row = (i == threads - 1) ? height : (i + 1) * rows_per_thread;
    t_data[i].x_min = x_min;
    t_data[i].y_min = y_min;
    t_data[i].x_scale = x_scale;
    t_data[i].y_scale = y_scale;

    switch (mode) {
    case SCALAR:
      pthread_create(&thread_ids[i], NULL, thread_scalar, &t_data[i]);
      break;
    case AVX:
      pthread_create(&thread_ids[i], NULL, thread_avx, &t_data[i]);
      break;
    default:
      break;
    }
  }

  for (int i = 0; i < threads; i++) {
    pthread_join(thread_ids[i], NULL);
  }

bench_done:

  end_time = get_time();
  printf("[%s] Time: %.4f seconds | FPS: %.2f\n", name, end_time - start_time,
         1.0 / (end_time - start_time));
}

// int main() {
//   // Check AVX support (basic runtime check or just assume for this HW)
//   printf("Initializing Benchmark...\n");
//   printf("Resolution: %dx%d | Max Iter: %d\n", WIDTH, HEIGHT, MAX_ITER);
//
//   // Align memory to 64 bytes for AVX-512 performance
//   uint32_t *buffer =
//       (uint32_t *)aligned_alloc(64, WIDTH * HEIGHT * sizeof(uint32_t));
//
//   // 1. Single Thread Scalar
//   run_benchmark("Single Core Scalar  ", 1, 0, buffer);
//
//   // 2. Multicore Scalar
//   run_benchmark("Multi Core Scalar   ", THREAD_COUNT, 0, buffer);
//
//   // 3. Multicore AVX-512
//   run_benchmark("Multi Core AVX2  ", THREAD_COUNT, 1, buffer);
//
//   free(buffer);
//   return 0;
// }
