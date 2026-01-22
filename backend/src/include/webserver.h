#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <pthread.h>

typedef struct {
  int client_fd;
  char *request;
} http_request_t;

typedef struct {
  http_request_t *queue;
  int head;
  int tail;
  int size;
  int count;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  pthread_t thread;
  char *root_path;
} webserver_t;

void webserver_init(webserver_t *server, const char *root_path, int queue_size);
void webserver_start(webserver_t *server);
void webserver_enqueue(webserver_t *server, int client_fd, const char *request);

#endif // _WEBSERVER_H_
