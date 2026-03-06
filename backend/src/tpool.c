#include "include/tpool.h"
#include "include/common.h"

tpool_t *tpool_create(int num_threads, int max_queue_size) {
  tpool_t *pool = (tpool_t *)malloc(sizeof(tpool_t));
  CHECKALLOC(pool);
  pool->thread_count = num_threads;
  pool->queue_size = max_queue_size;
  pool->queue_head = 0;
  pool->queue_tail = 0;
  pool->count = 0;
  pool->shutdown = 0;
  pool->queue = (thread_task_t *)malloc(sizeof(thread_task_t) * max_queue_size);
  CHECKALLOC(pool->queue);
  pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
  CHECKALLOC(pool->threads);
  long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cores < 1)
    num_cores = 1;
  CHECKPTHREAD(pthread_mutex_init(&pool->lock, NULL));
  CHECKPTHREAD(pthread_cond_init(&pool->notify, NULL));

  for (int i = 0; i < num_threads; i++) {
    CHECKPTHREAD(pthread_create(&(pool->threads[i]), NULL, thread_worker, (void *)pool));
#ifdef THREAD_PINNING
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int core_id = i % num_cores;
    CPU_SET(i, &cpuset);

    CHECKPTHREAD(pthread_setaffinity_np(pool->threads[i], sizeof(cpu_set_t), &cpuset));
#endif /* ifdef THREAD_PINNING */
  }

  return pool;
}

void tpool_shutdown(tpool_t *pool) {
  CHECKPTHREAD(pthread_mutex_lock(&pool->lock));
  pool->shutdown = 1;
  CHECKPTHREAD(pthread_cond_broadcast(&pool->notify));

  // Remember from OS, we have to wakeup alla the threads, so we have to
  // broadcast
  CHECKPTHREAD(pthread_mutex_unlock(&pool->lock));
  for (int i = 0; i < pool->thread_count; i++) {
    CHECKPTHREAD(pthread_join(pool->threads[i], NULL));
  }

  // Once the thread exited, we can cleanup the resources
  free(pool->queue);
  free(pool->threads);
  free(pool);
}

void tpool_add_work(tpool_t *pool, void (*func)(void *), void *arg) {
  CHECKPTHREAD(pthread_mutex_lock(&pool->lock));
  // Add task
  pool->queue[pool->queue_tail] =
      (thread_task_t){.function = func, .argument = arg};
  pool->queue_tail = (pool->queue_tail + 1) % pool->queue_size;
  pool->count++;

  CHECKPTHREAD(pthread_cond_signal(&pool->notify));

unlock_tp:
  CHECKPTHREAD(pthread_mutex_unlock(&pool->lock));
}
