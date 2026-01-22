#include "include/tpool.h"

tpool_t *tpool_create(int num_threads, int max_queue_size) {
  tpool_t *pool = (tpool_t *)malloc(sizeof(tpool_t));
  pool->thread_count = num_threads;
  pool->queue_size = max_queue_size;
  pool->queue_head = 0;
  pool->queue_tail = 0;
  pool->count = 0;
  pool->shutdown = 0;
  pool->queue = (thread_task_t *)malloc(sizeof(thread_task_t) * max_queue_size);
  pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);

  pthread_mutex_init(&pool->lock, NULL);
  pthread_cond_init(&pool->notify, NULL);

  for (int i = 0; i < num_threads; i++) {
    pthread_create(&(pool->threads[i]), NULL, thread_worker, (void *)pool);
  }

  return pool;
}

void tpool_shutdown(tpool_t *pool) {
  pthread_mutex_lock(&pool->lock);
  pool->shutdown = 1;
  pthread_cond_broadcast(&pool->notify);

  // Remember from OS, we have to wakeup alla the threads, so we have to
  // broadcast
  pthread_mutex_unlock(&pool->lock);
  for (int i = 0; i < pool->thread_count; i++) {
    pthread_join(pool->threads[i], NULL);
  }

  // Once the thread exited, we can cleanup the resources
  for (int i = 0; i < pool->queue_size; i++) {
    if (pool->queue->argument) {
      free(pool->queue->argument);
    }
  }
  free(pool->queue);
  free(pool->threads);
  free(pool);
}

void tpool_add_work(tpool_t *pool, void (*func)(void *), void *arg) {
  pthread_mutex_lock(&pool->lock);

  // Add task
  pool->queue[pool->queue_tail] =
      (thread_task_t){.function = func, .argument = arg};
  pool->queue_tail = (pool->queue_tail + 1) % pool->queue_size;
  pool->count++;

  pthread_cond_signal(&pool->notify);
  pthread_mutex_unlock(&pool->lock);
}
