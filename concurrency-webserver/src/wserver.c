#include "io_helper.h"
#include "request.h"
#include <pthread.h>
#include <stdio.h>

char default_root[] = ".";

typedef struct {
  int connfd;
  off_t size; // filesize in Bytes
} item_t;

typedef struct {
  item_t *buf;
  int count, size;
  pthread_mutex_t mutex;
  pthread_cond_t full, empty;
} buf_t;

void buf_init(buf_t *buffer, const int *bufsize) {
  buffer->size = *bufsize;
  buffer->buf = malloc(buffer->size * sizeof(item_t));
  if (!buffer->buf)
    exit(1);
  buffer->count = 0;
  pthread_mutex_init(&(buffer->mutex), NULL);
  pthread_cond_init(&(buffer->full), NULL);
  pthread_cond_init(&(buffer->empty), NULL);
}

void buf_destroy(buf_t *buffer) {
  free(buffer->buf);
  pthread_mutex_destroy(&buffer->mutex);
  pthread_cond_destroy(&buffer->full);
  pthread_cond_destroy(&buffer->empty);
}

void buf_insert(buf_t *buffer, int connfd, off_t size, int schedule) {
  pthread_mutex_lock(&buffer->mutex);
  while (buffer->count == buffer->size)
    pthread_cond_wait(&buffer->empty,
                      &buffer->mutex); // wait until consumers empty the buffer
  item_t item;
  item.connfd = connfd;
  item.size = size;
  if (schedule == 0)
    buffer->buf[buffer->count] = item;
  else {
    int i = buffer->count - 1;
    while (i >= 0 && buffer->buf[i].size > size) {
      buffer->buf[i + 1] = buffer->buf[i];
      i--;
    }
    buffer->buf[i + 1] = item;
  }
  buffer->count++;
  pthread_cond_signal(
      &buffer->full); // signal to consumers that buffer is filled
  pthread_mutex_unlock(&buffer->mutex);
}

int buf_remove(buf_t *buffer) {
  pthread_mutex_lock(&buffer->mutex);
  while (buffer->count == 0)
    pthread_cond_wait(&buffer->full, &buffer->mutex);
  int connfd = buffer->buf[0].connfd;
  for (int i = 1; i < buffer->count; i++)
    buffer->buf[i - 1] = buffer->buf[i];
  buffer->count--;
  pthread_cond_signal(
      &buffer->empty); // signal to consumers that buffer is filled
  pthread_mutex_unlock(&buffer->mutex);
  return connfd;
}

//
// ./wserver [-d <basedir>] [-p <portnum>]
//

void *worker(void *arg) {
  request_handle(conn_fd);
  return NULL;
}

int main(int argc, char *argv[]) {
  int c;
  char *root_dir = default_root;
  int port = 10000;
  int n_threads = 1 * sizeof(pthread_t); // 1 thread by defauylt
  int schedule = 0;                      // FIFO default
  int bufsize = 1;                       // stores 1 fd by default

  while ((c = getopt(argc, argv, "d:p:t:b:s")) != -1)
    switch (c) {
    case 'd':
      root_dir = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 't':
      n_threads = atoi(optarg);
      break;
    case 'b':
      bufsize = atoi(optarg);
      break;
    case 's':
      if (strcmp(optarg, "SFF") == 0)
        schedule = 1;
      break;
    default:
      fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b "
                      "buffers] [-s schedalg]\n");
      exit(1);
    }

  // run out of this directory
  chdir_or_die(root_dir);

  // create and spawn n_threads
  pthread_t *threads = malloc(n_threads);

  int listen_fd = open_listen_fd_or_die(port);

  for (int i = 0; i < n_threads; i++)
    pthread_create(&threads[i], NULL, worker, NULL);

  buf_t buf;
  buf_init(&buf, &bufsize);
  while (1) {
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr,
                                (socklen_t *)&client_len);
    // TODO: figure out how to get the file size. probably check requests.h
    buf_insert(buf_t * buffer, int connfd, off_t size, int schedule)
        close_or_die(conn_fd);
  }

  free(threads);
  return 0;
}
