#ifndef _QUEUE_H
#define _QUEUE_H
#include <pthread.h>
#include <stdint.h>
#include "common.h"
#include "tpool.h"

// GPU Render Job
typedef struct gpu_job {
  uint16_t *buffer;
  int width;
  int height;
  int iterations;
  double x_min;
  double y_min;
  double x_scale;
  double y_scale;

  // Synchronization for this specific job
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int done;
} gpu_job_t;

// Job Queue Node
typedef struct gpu_job_node {
  gpu_job_t *job;
  struct gpu_job_node *next;
} gpu_job_node_t;

// Thread-safe Job Queue
typedef struct {
  gpu_job_node_t *head;
  gpu_job_node_t *tail;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} job_queue_t;


// Context passed to the worker pool (Single Image Slice - CPU)
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
  RenderMode mode;
} render_job_t;
void queue_init(job_queue_t *q);
void queue_push(job_queue_t *q, gpu_job_t *job);
gpu_job_t *queue_pop(job_queue_t *q);
#endif // _QUEUE_H

