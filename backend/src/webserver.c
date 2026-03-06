#include "include/webserver.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "include/common.h"

#define MAX_PATH_LEN 1024

static const char *get_content_type(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return "application/octet-stream";
  if (strcmp(ext, ".html") == 0)
    return "text/html";
  if (strcmp(ext, ".css") == 0)
    return "text/css";
  if (strcmp(ext, ".js") == 0)
    return "application/javascript";
  if (strcmp(ext, ".png") == 0)
    return "image/png";
  if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    return "image/jpeg";
  return "application/octet-stream";
}

// Note: Ensure `root_path` is resolved to an absolute path (e.g., using realpath) 
// BEFORE passing it into this function, otherwise the strncmp check will fail.
static void serve_file(int client_fd, const char *root_path, const char *path) {
  char raw_path[PATH_MAX];
  char resolved_path[PATH_MAX];
  const char *target_path = path;

  if (strcmp(path, "/") == 0) {
    target_path = "/index.html";
  }

  snprintf(raw_path, sizeof(raw_path), "%s%s", root_path, target_path);
  realpath(raw_path, resolved_path);

  if (strncmp(resolved_path, root_path, strlen(root_path)) != 0) {
    const char *res = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
    send(client_fd, res, strlen(res), 0);
    return;
  }

  int fd = open(resolved_path, O_RDONLY);
  if (fd < 0) {
    const char *res = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    send(client_fd, res, strlen(res), 0);
    return;
  }

  struct stat st;
  if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
    // If it's a directory or fstat fails, forbid access and close the fd
    close(fd);
    const char *res = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
    send(client_fd, res, strlen(res), 0);
    return;
  }

  // 6. Send HTTP Headers
  off_t file_size = st.st_size;
  const char *content_type = get_content_type(resolved_path);
  char header[512];
  int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: %s\r\n"
                            "Content-Length: %lld\r\n"
                            "Connection: close\r\n\r\n",
                            content_type, (long long)file_size);

  send(client_fd, header, header_len, 0);

  // 7. Send HTTP Body
  char buffer[8192];
  ssize_t bytes_read;
  while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
    // Break early if the client disconnects or the socket errors out
    if (send(client_fd, buffer, bytes_read, 0) < 0) {
      break;
    }
  }

  close(fd);
}

static void *webserver_worker(void *arg) {
  webserver_t *server = (webserver_t *)arg;

  while (1) {
    CHECKPTHREAD(pthread_mutex_lock(&server->mutex));
    while (server->count == 0) {
      CHECKPTHREAD(pthread_cond_wait(&server->cond, &server->mutex));
    }

    http_request_t req = server->queue[server->head];
    server->head = (server->head + 1) % server->size;
    server->count--;
    CHECKPTHREAD(pthread_mutex_unlock(&server->mutex));

    // Process request
    char method[16], path[MAX_PATH_LEN], protocol[16];
    if (sscanf(req.request, "%15s %1023s %15s", method, path, protocol) == 3) {
      if (strcmp(method, "GET") == 0) {
        if (path[0] == '/') {
          serve_file(req.client_fd, server->root_path, path);
        } else {
          const char *res =
              "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
          send(req.client_fd, res, strlen(res), 0);
        }
      } else {
        const char *res =
            "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        send(req.client_fd, res, strlen(res), 0);
      }
    }

    close(req.client_fd);
    free(req.request);
  }
  return NULL;
}

void webserver_init(webserver_t *server, const char *root_path,
                    int queue_size) {
  server->root_path = strdup(root_path);
  server->size = queue_size;
  server->queue = malloc(sizeof(http_request_t) * queue_size);
  CHECKALLOC(server->queue);
  server->head = 0;
  server->tail = 0;
  server->count = 0;
  CHECKPTHREAD(pthread_mutex_init(&server->mutex, NULL));
  CHECKPTHREAD(pthread_cond_init(&server->cond, NULL));
}

void webserver_start(webserver_t *server) {
  CHECKPTHREAD(pthread_create(&server->thread, NULL, webserver_worker, server));
  CHECKPTHREAD(pthread_detach(server->thread));
}

void webserver_enqueue(webserver_t *server, int client_fd,
                       const char *request) {
  CHECKPTHREAD(pthread_mutex_lock(&server->mutex));
  if (server->count < server->size) {
    server->queue[server->tail].client_fd = client_fd;
    server->queue[server->tail].request = strdup(request);
    server->tail = (server->tail + 1) % server->size;
    server->count++;
    CHECKPTHREAD(pthread_cond_signal(&server->cond));
  } else {
    // Queue full, drop connection
    const char *res =
        "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
    send(client_fd, res, strlen(res), 0);
    close(client_fd);
  }
  CHECKPTHREAD(pthread_mutex_unlock(&server->mutex));
}
