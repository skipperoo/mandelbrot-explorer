#include "include/mandelbrot.h"
#include "include/queue.h"
#include "include/tpool.h"
#include "include/webserver.h"
#include "include/websocket.h"
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#if defined(DEBUG) && DEBUG == 1
#undef DEBUG
#define DEBUG(...) printf(__VA_ARGS__)
#else
#undef DEBUG
#define DEBUG(...)                                                             \
  do {                                                                         \
  } while (0)
#endif

// Context passed to the client thread
typedef struct {
  int client_fd;
  tpool_t *pool;
  RenderMode mode;
  int num_worker_threads; // Needed to calculate slice count
} client_context_t;

// Global Queue
job_queue_t g_gpu_queue;
webserver_t g_webserver;

// CPU Worker (Executed by Thread Pool)
void render_slice_wrapper(void *arg) {
  render_job_t *job = (render_job_t *)arg;

  if (job->mode == SCALAR) {
    render_scalar(job->buffer, job->width, job->start_row, job->end_row,
                  job->iterations, job->x_min, job->y_min, job->x_scale,
                  job->y_scale);
  } else {
    render_simd(job->buffer, job->width, job->start_row, job->end_row,
                job->iterations, job->x_min, job->y_min, job->x_scale,
                job->y_scale);
  }

  // Sync: Decrement task count and signal if finished
  pthread_mutex_lock(&job->barrier->mutex);
  job->barrier->tasks_remaining--;
  if (job->barrier->tasks_remaining == 0) {
    pthread_cond_signal(&job->barrier->cond);
  }
  pthread_mutex_unlock(&job->barrier->mutex);

  free(job);
}

// Dedicated GPU Worker Thread
void *gpu_worker_thread(void *arg) {
  const char *vendor = (const char *)arg;

  DEBUG("[GPU Worker] Initializing for vendor: %s\n", vendor);
  if (!init_opengl_for_vendor(vendor)) {
    fprintf(stderr, "[GPU Worker] Failed to initialize OpenGL for %s\n",
            vendor);
    // In a real app we might want to exit or handle this gracefully.
    // For now, we loop but do nothing or exit?
    // Let's exit to avoid spinning.
    return NULL;
  }
  DEBUG("[GPU Worker] Ready and waiting for jobs...\n");

  while (1) {
    gpu_job_t *job = queue_pop(&g_gpu_queue);

    // Render using the persistent context
    // We assume init_opengl_for_vendor leaves the context active (or we
    // reactivate it) The current implementation of init_opengl_for_vendor sets
    // the context active. However, robust code should probably
    // EnsureContextCurrent() here if needed. Since we are the only thread, it
    // should stay active.

    // render_opengl_frame assumes context is active.
    render_opengl_frame(job->buffer, job->width, 0, job->height,
                        job->iterations, job->x_min, job->y_min, job->x_scale,
                        job->y_scale);

    // Notify client thread
    pthread_mutex_lock(&job->mutex);
    job->done = 1;
    pthread_cond_signal(&job->cond);
    pthread_mutex_unlock(&job->mutex);
  }
  return NULL;
}

void *handle_client(void *arg) {
  // 1. Unpack Arguments
  client_context_t *ctx = (client_context_t *)arg;
  int client_fd = ctx->client_fd;
  tpool_t *pool = ctx->pool;
  int mode = ctx->mode;
  int num_workers = ctx->num_worker_threads;
  free(ctx);

  char buffer[4097] = {0};
  uint16_t *img_buffer = NULL;
  size_t current_capacity = 0;

  // 2. Handshake
  ssize_t bytes_read = read(client_fd, buffer, 4096);
  if (bytes_read <= 0) {
    close(client_fd);
    return NULL;
  }

  if (perform_handshake(client_fd, buffer) != 0) {
    if (strstr(buffer, "GET ") == buffer) {
      webserver_enqueue(&g_webserver, client_fd, buffer);
      return NULL;
    }
    fprintf(stderr, "Handshake failed\n");
    close(client_fd);
    return NULL;
  }

  DEBUG("Client connected (FD: %d)\n", client_fd);

  char payload[1024];

  // 3. Request Loop
  while (receive_frame(client_fd, payload)) {
    double x_min, y_min, x_scale, y_scale;
    int width, height, iterations;

    if (sscanf(payload, "%lf,%lf,%lf,%lf,%d,%d,%d", &x_min, &y_min, &x_scale,
               &y_scale, &width, &height, &iterations) != 7) {
      DEBUG("Invalid request format from FD %d\n", client_fd);
      continue;
    }

    size_t size_bytes = width * height * sizeof(uint16_t);
    size_t padded_size = (size_bytes + 31) & ~31;

    if (padded_size > current_capacity) {
      free(img_buffer);
      img_buffer = aligned_alloc(32, padded_size);
      if (!img_buffer) {
        perror("Allocation failed");
        break;
      }
      current_capacity = padded_size;
      DEBUG("[FD %d] Buffer resized to %zu bytes\n", client_fd, padded_size);
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    if (mode == INTEL_GPU || mode == NVIDIA_GPU) {
      // Create GPU Job
      gpu_job_t job;
      job.buffer = img_buffer;
      job.width = width;
      job.height = height;
      job.iterations = iterations;
      job.x_min = x_min;
      job.y_min = y_min;
      job.x_scale = x_scale;
      job.y_scale = y_scale;
      job.done = 0;
      pthread_mutex_init(&job.mutex, NULL);
      pthread_cond_init(&job.cond, NULL);

      // Submit to Queue
      queue_push(&g_gpu_queue, &job);

      // Wait for completion
      pthread_mutex_lock(&job.mutex);
      while (!job.done) {
        pthread_cond_wait(&job.cond, &job.mutex);
      }
      pthread_mutex_unlock(&job.mutex);

      pthread_mutex_destroy(&job.mutex);
      pthread_cond_destroy(&job.cond);

    } else {
      // CPU mode: Split the image into slices and dispatch to the thread pool
      int num_slices = num_workers * 16;
      frame_barrier_t barrier;
      barrier_init(&barrier, num_slices);

      int rows_per_slice = height / num_slices;

      // Cyclic allocation
      // To better distribuete the work and avoid hotspots
      for (int i = 0; i < num_slices; i++) {
        int start = i * rows_per_slice;
        int end = (i == num_slices - 1) ? height : (i + 1) * rows_per_slice;

        render_job_t *job = malloc(sizeof(render_job_t));
        job->buffer = img_buffer;
        job->width = width;
        job->start_row = start;
        job->end_row = end;
        job->iterations = iterations;
        job->x_min = x_min;
        job->y_min = y_min;
        job->x_scale = x_scale;
        job->y_scale = y_scale;
        job->barrier = &barrier;
        job->mode = mode;

        tpool_add_work(pool, render_slice_wrapper, job);
      }

      // Wait for all slices to complete
      barrier_wait(&barrier);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double time_taken = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    DEBUG("[FD %d] Rendered in %.2f ms\n", client_fd, time_taken);

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    send_binary_frame(client_fd, (uint8_t *)img_buffer,
                      width * height * sizeof(uint16_t));

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    time_taken = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                 (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    DEBUG("[FD %d] Sent in in %.2f ms\n", client_fd, time_taken);
    fflush(stdout);
  }
  free(img_buffer);

  DEBUG("Client disconnected (FD: %d)\n", client_fd);
  close(client_fd);
  return NULL;
}

// --- Main ---

int main(int argc, char *argv[]) {
  int server_fd;
  struct sockaddr_in address;
  int opt_val = 1;
  int benchmark = 0;

  // A. Defaults
  int num_threads = get_nprocs();
  int mode = 0;

  // B. Environment Variables
  char *env_threads = getenv("MANDELBROT_THREADS");
  if (env_threads) {
    int t = atoi(env_threads);
    if (t > 0)
      num_threads = t;
  }

  char *env_scalar = getenv("MANDELBROT_SIMD");
  if (env_scalar) {
    if (strcmp(env_scalar, "0") != 0 && strcasecmp(env_scalar, "false") != 0) {
      mode = AVX;
    }
  }

  char *env_intel_gpu = getenv("MANDELBROT_INTEL_GPU");
  if (env_intel_gpu) {
    if (strcmp(env_intel_gpu, "0") != 0 &&
        strcasecmp(env_intel_gpu, "false") != 0) {
      mode = INTEL_GPU;
    }
  }

  char *env_nvidia_gpu = getenv("MANDELBROT_NVIDIA_GPU");
  if (env_nvidia_gpu) {
    if (strcmp(env_nvidia_gpu, "0") != 0 &&
        strcasecmp(env_nvidia_gpu, "false") != 0) {
      mode = NVIDIA_GPU;
    }
  }

  // C. Command Line Arguments (Overrides Env Vars)
  int opt;
  while ((opt = getopt(argc, argv, "t:sb")) != -1) {
    switch (opt) {
    case 't':
      num_threads = atoi(optarg);
      if (num_threads < 1)
        exit(EXIT_FAILURE);
      break;
    case 's':
      mode = 1;
      break;
    case 'b':
      benchmark = 1;
      break;
    default:
      fprintf(stderr, "Usage: %s [-t threads] [-s (scalar mode)]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  if (benchmark) {
    double base_width = 960.0;
    double base_height = 540.0;
    for (int scale = 1; scale <= 4; scale *= 2) {
      double width = base_width * scale;
      double height = base_height * scale;
      uint16_t *buffer =
          (uint16_t *)aligned_alloc(64, width * height * sizeof(uint16_t));

      for (int i = 2; i <= get_nprocs(); i += 2) {
        printf("######### Benchmark (%.0lfx%.0lf) with %d threads #########\n",
               width, height, i);
        memset(buffer, 0, width * height * sizeof(uint16_t));
        run_benchmark("Scalar", i, SCALAR, width, height, buffer);
        memset(buffer, 0, width * height * sizeof(uint16_t));
        run_benchmark("SIMD", i, AVX, width, height, buffer);
        printf("######### ###################################\n");
      }
      free(buffer);
    }
    base_width = 960.0;
    base_height = 540.0;
    for (int scale = 1; scale <= 4; scale *= 2) {
      double width = base_width * scale;
      double height = base_height * scale;
      uint16_t *buffer =
          (uint16_t *)aligned_alloc(64, width * height * sizeof(uint16_t));

      printf("######### GPU Benchmark (%.0lfx%.0lf) #########\n", width,
             height);
      memset(buffer, 0, width * height * sizeof(uint16_t));
      run_benchmark("Intel GPU", 1, INTEL_GPU, width, height, buffer);
      memset(buffer, 0, width * height * sizeof(uint16_t));
      run_benchmark("NVIDIA GPU", 1, NVIDIA_GPU, width, height, buffer);
      printf("######### ###################################\n");
      free(buffer);
    }

    return 0;
  }

  if (mode == SCALAR)
    printf("Server Config: %d Threads | Mode: Scalar\n", num_threads);
  else if (mode == NVIDIA_GPU || mode == INTEL_GPU)
    printf("Server Config: %d Threads | Mode: GPU\n", num_threads);
  else {
#if defined(__AVX512F__)
    printf("Server Config: %d Threads | Mode: AVX-512\n", num_threads);
#elif defined(__AVX2__)
    printf("Server Config: %d Threads | Mode: AVX2\n", num_threads);
#elif defined(__AVX__)
    printf("Server Config: %d Threads | Mode: AVX\n", num_threads);
#endif
  }
  fflush(stdout);

  // 2. Initialize Shared Thread Pool (for CPU tasks)
  tpool_t *pool = tpool_create(num_threads, 1024);

  // 3. Initialize Webserver
  webserver_init(&g_webserver, "/var/www", 100);
  webserver_start(&g_webserver);

  // 4. Initialize GPU Worker if needed
  if (mode == INTEL_GPU || mode == NVIDIA_GPU) {
    queue_init(&g_gpu_queue);
    const char *vendor = (mode == INTEL_GPU) ? "Intel" : "NVIDIA";
    pthread_t gpu_thread;
    if (pthread_create(&gpu_thread, NULL, gpu_worker_thread, (void *)vendor) !=
        0) {
      perror("Failed to create GPU worker thread");
      exit(EXIT_FAILURE);
    }
    pthread_detach(gpu_thread); // Run in background
  }

  // 4. Socket Setup
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }
  listen(server_fd, 10);
  printf("Listening on port 8080...\n");
  fflush(stdout);

  // 5. Accept Loop
  while (1) {
    int addrlen = sizeof(address);
    int client_fd =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

    if (client_fd < 0) {
      perror("Accept failed");
      continue;
    }

    // Allocate context for the new thread
    client_context_t *ctx = malloc(sizeof(client_context_t));
    ctx->client_fd = client_fd;
    ctx->pool = pool;
    ctx->mode = mode;
    ctx->num_worker_threads = num_threads;

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, ctx) != 0) {
      perror("Failed to create client thread");
      close(client_fd);
      free(ctx);
    } else {
      pthread_detach(thread_id);
    }
  }

  tpool_shutdown(pool);

  return 0;
}
