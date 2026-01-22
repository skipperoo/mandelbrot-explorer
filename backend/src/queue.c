#include "include/queue.h"

void queue_init(job_queue_t *q) {
  q->head = q->tail = NULL;
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
}

void queue_push(job_queue_t *q, gpu_job_t *job) {
  gpu_job_node_t *node = malloc(sizeof(gpu_job_node_t));
  node->job = job;
  node->next = NULL;

  pthread_mutex_lock(&q->mutex);
  if (q->tail) {
    q->tail->next = node;
    q->tail = node;
  } else {
    q->head = q->tail = node;
  }
  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);
}

gpu_job_t *queue_pop(job_queue_t *q) {
  pthread_mutex_lock(&q->mutex);
  while (q->head == NULL) {
    pthread_cond_wait(&q->cond, &q->mutex);
  }
  gpu_job_node_t *node = q->head;
  gpu_job_t *job = node->job;
  q->head = node->next;
  if (q->head == NULL) {
    q->tail = NULL;
  }
  pthread_mutex_unlock(&q->mutex);
  free(node);
  return job;
}
