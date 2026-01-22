#ifndef TPOOL_H
#define TPOOL_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  void (*function)(void *);
  void *argument;
} thread_task_t;

// Thread Pool Structure
typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  thread_task_t *queue;
  int queue_size;
  int queue_head;
  int queue_tail;
  int count;
  int shutdown;
  int thread_count;
} tpool_t;

// Synchronization Barrier for Frame Completion
typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int tasks_remaining;
} frame_barrier_t;

tpool_t *tpool_create(int num_threads, int max_queue_size);
void tpool_add_work(tpool_t *pool, void (*func)(void *), void *arg);
void tpool_shutdown(tpool_t *pool_ptr);

static inline void barrier_init(frame_barrier_t *barrier, int count) {
  pthread_mutex_init(&barrier->mutex, NULL);
  pthread_cond_init(&barrier->cond, NULL);
  barrier->tasks_remaining = count;
}

static inline void barrier_wait(frame_barrier_t *barrier) {
  pthread_mutex_lock(&barrier->mutex);
  while (barrier->tasks_remaining > 0) {
    pthread_cond_wait(&barrier->cond, &barrier->mutex);
  }
  pthread_mutex_unlock(&barrier->mutex);
}

// Worker Thread Function
static void *thread_worker(void *pool_ptr) {
  tpool_t *pool = (tpool_t *)pool_ptr;

  while (1) {
    pthread_mutex_lock(&pool->lock);

    // Wait for tasks
    while (pool->count == 0 && !pool->shutdown) {
      pthread_cond_wait(&pool->notify, &pool->lock);
    }

    if (pool->shutdown) {
      pthread_mutex_unlock(&pool->lock);
      break;
    }

    // Grab task
    thread_task_t task = pool->queue[pool->queue_head];
    pool->queue_head = (pool->queue_head + 1) % pool->queue_size;
    pool->count--;

    pthread_mutex_unlock(&pool->lock);

    // Execute task
    (*(task.function))(task.argument);
  }
  return NULL;
}

#endif
