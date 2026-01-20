#include "include/mandelbrot.h"
#include "include/tpool.h"
#include "include/websocket.h"
#include <getopt.h>
#include <pthread.h> // Required for client threads
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

// --- Structs ---

// Context passed to the client thread
typedef struct {
  int client_fd;
  tpool_t *pool;
  int use_scalar;
  int num_worker_threads; // Needed to calculate slice count
} client_context_t;

// Context passed to the worker pool (Single Image Slice)
typedef struct {
  uint16_t *buffer;
  int width;
  int start_row;
  int end_row;
  int iterations;
  double x_min;
  double y_min;
  double x_scale;
  double y_scale;
  frame_barrier_t *barrier;
  int use_scalar;
} render_job_t;

// --- Worker Function (Executed by Thread Pool) ---

void render_slice_wrapper(void *arg) {
  render_job_t *job = (render_job_t *)arg;

  if (job->use_scalar) {
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

// --- Client Thread (Executed per Connection) ---

void *handle_client(void *arg) {
  // 1. Unpack Arguments
  client_context_t *ctx = (client_context_t *)arg;
  int client_fd = ctx->client_fd;
  tpool_t *pool = ctx->pool;
  int use_scalar = ctx->use_scalar;
  int num_workers = ctx->num_worker_threads;
  free(ctx); // Free the struct allocated in main

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
    fprintf(stderr, "Handshake failed\n");
    close(client_fd);
    return NULL;
  }

  printf("Client connected (FD: %d)\n", client_fd);

  char payload[1024];

  // 3. Request Loop
  while (receive_frame(client_fd, payload)) {
    double x_min, y_min, x_scale, y_scale;
    int width, height, iterations;

    if (sscanf(payload, "%lf,%lf,%lf,%lf,%d,%d,%d", &x_min, &y_min, &x_scale,
               &y_scale, &width, &height, &iterations) != 7) {
      printf("Invalid request format from FD %d\n", client_fd);
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
      printf("[FD %d] Buffer resized to %zu bytes\n", client_fd, padded_size);
    }

    // B. Setup Barrier
    // We split the image into slices based on total server threads
    int num_slices = num_workers * 8;
    frame_barrier_t barrier;
    barrier_init(&barrier, num_slices);

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // C. Dispatch Jobs to Global Pool
    int rows_per_slice = height / num_slices;

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
      job->use_scalar = use_scalar;

      tpool_add_work(pool, render_slice_wrapper, job);
    }

    // D. Wait for completion
    // This blocks THIS client thread, but not the Main thread or other clients
    barrier_wait(&barrier);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double time_taken = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    printf("[FD %d] Rendered in %.2f ms\n", client_fd, time_taken);

    // E. Send Result
    send_binary_frame(client_fd, (uint8_t *)img_buffer,
                      width * height * sizeof(uint16_t));
  }
  free(img_buffer);

  printf("Client disconnected (FD: %d)\n", client_fd);
  close(client_fd);
  return NULL;
}

// --- Main ---

int main(int argc, char *argv[]) {
  int server_fd;
  struct sockaddr_in address;
  int opt_val = 1;
  int benchmark = 0;

  // 1. Configuration: Defaults -> Env Vars -> Command Line args

  // A. Defaults
  int num_threads = get_nprocs();
  int use_scalar = 0;

  // B. Environment Variables
  char *env_threads = getenv("MANDELBROT_THREADS");
  if (env_threads) {
    int t = atoi(env_threads);
    if (t > 0)
      num_threads = t;
  }

  char *env_scalar = getenv("MANDELBROT_SCALAR");
  if (env_scalar) {
    // If set to anything other than "0" or "false", enable scalar
    if (strcmp(env_scalar, "0") != 0 && strcasecmp(env_scalar, "false") != 0) {
      use_scalar = 1;
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
      use_scalar = 1;
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
        run_benchmark("Scalar", i, 0, width, height, buffer);
        memset(buffer, 0, width * height * sizeof(uint16_t));
        run_benchmark("SIMD", i, 1, width, height, buffer);
        printf("######### ###################################\n");
      }
      free(buffer);
    }
    return 0;
  }

  if (use_scalar)
    printf("Server Config: %d Threads | Mode: Scalar\n", num_threads);
  else {
#if defined(__AVX512F__)
    printf("Server Config: %d Threads | Mode: AVX-512\n", num_threads);

#elif defined(__AVX2__)
    printf("Server Config: %d Threads | Mode: AVX2\n", num_threads);

#elif defined(__AVX__)
    printf("Server Config: %d Threads | Mode: AVX\n", num_threads);

#endif
  }

  // 2. Initialize Shared Thread Pool
  tpool_t *pool = tpool_create(
      num_threads, 1024); // Increased queue size for multiple clients

  // 3. Socket Setup
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }
  listen(server_fd, 10); // Backlog of 10
  printf("Listening on port 8080...\n");

  // 4. Accept Loop
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
    ctx->use_scalar = use_scalar;
    ctx->num_worker_threads = num_threads;

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, ctx) != 0) {
      perror("Failed to create client thread");
      close(client_fd);
      free(ctx);
    } else {
      // Detach so resources are freed automatically when thread exits
      pthread_detach(thread_id);
    }
  }

  return 0;
}
