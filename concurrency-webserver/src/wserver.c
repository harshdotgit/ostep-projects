#include "io_helper.h"
#include "request.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define MAXBUF 8192
#define URIBUF 512 // request-line tokens are bounded well below MAXBUF

char default_root[] = ".";

typedef struct {
  int connfd;
  off_t size; // filesize in Bytes
  char method[URIBUF], uri[URIBUF], version[URIBUF];
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

void buf_insert(buf_t *buffer, item_t *item, int *schedule) {
  pthread_mutex_lock(&buffer->mutex);
  while (buffer->count == buffer->size)
    pthread_cond_wait(&buffer->empty,
                      &buffer->mutex); // wait until consumers empty the buffer
  if (*schedule == 0)
    buffer->buf[buffer->count] = *item;
  else {
    int i = buffer->count - 1;
    while (i >= 0 && buffer->buf[i].size > item->size) {
      buffer->buf[i + 1] = buffer->buf[i];
      i--;
    }
    buffer->buf[i + 1] = *item;
  }
  buffer->count++;
  pthread_cond_signal(
      &buffer->full); // signal to consumers that buffer is filled
  pthread_mutex_unlock(&buffer->mutex);
}

item_t buf_remove(buf_t *buffer) {
  pthread_mutex_lock(&buffer->mutex);
  while (buffer->count == 0)
    pthread_cond_wait(&buffer->full, &buffer->mutex);
  item_t item = buffer->buf[0];
  for (int i = 1; i < buffer->count; i++)
    buffer->buf[i - 1] = buffer->buf[i];
  buffer->count--;
  pthread_cond_signal(
      &buffer->empty); // signal to consumers that buffer is filled
  pthread_mutex_unlock(&buffer->mutex);
  return item;
}

void *worker(void *buffer) {

  while (1) {
    item_t item = buf_remove((buf_t *)buffer);
    request_handle(item.connfd, item.method, item.uri, item.version);
    close_or_die(item.connfd);
  }
  return NULL;
}

//
// read ONLY the request line (leaving the headers in the socket for the
// worker), parse it, and stat the file so SFF has a size. Returns 1 if the
// request should be queued, 0 if it was rejected and the connection already
// dealt with.
int get_filesize(item_t *item) {
  char line[MAXBUF];
  char _method[URIBUF], _uri[URIBUF], _version[URIBUF];

  _method[0] = _uri[0] = _version[0] = '\0';
  readline_or_die(item->connfd, line, MAXBUF); // request line only

  if (sscanf(line, "%511s %511s %511s", _method, _uri, _version) != 3) {
    request_error(item->connfd, line, "400", "Bad Request",
                  "server could not parse this request line");
    return 0;
  }

  // Security: refuse any path that could climb out of the base directory.
  if (strstr(_uri, "..")) {
    request_error(item->connfd, _uri, "403", "Forbidden",
                  "server rejected a path outside the document root");
    return 0;
  }

  strcpy(item->method, _method);
  strcpy(item->uri, _uri);
  strcpy(item->version, _version);

  // request_parse_uri() writes into the uri it is given, so hand it a copy and
  // leave item->uri untouched for the worker.
  char filename[MAXBUF], cgiargs[MAXBUF];
  request_parse_uri(_uri, filename, cgiargs);

  struct stat st;
  item->size = 0;
  if (stat(filename, &st) == 0 && S_ISREG(st.st_mode))
    item->size = st.st_size;

  return 1;
}

void usage() {
  fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] "
                  "[-b buffers] [-s schedalg]\n");
  exit(1);
}

//
// ./wserver [-d <basedir>] [-p <portnum>]
//

int main(int argc, char *argv[]) {
  int c;
  char *root_dir = default_root;
  int port = 10000;
  int n_threads = 1; // 1 thread by default
  int schedule = 0;  // FIFO default
  int bufsize = 1;   // stores 1 fd by default

  while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
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
      else if (strcmp(optarg, "FIFO") == 0)
        schedule = 0;
      else
        usage();
      break;
    default:
      usage();
    }

  // Validate before anything is used as an allocation size or loop bound.
  if (port < 1024 || port > 65535) {
    fprintf(stderr, "wserver: port must be in 1024..65535\n");
    exit(1);
  }
  if (n_threads < 1) {
    fprintf(stderr, "wserver: threads must be a positive integer\n");
    exit(1);
  }
  if (bufsize < 1) {
    fprintf(stderr, "wserver: buffers must be a positive integer\n");
    exit(1);
  }

  // run out of this directory
  chdir_or_die(root_dir);

  // create and spawn n_threads
  pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
  if (!threads) {
    fprintf(stderr, "wserver: out of memory\n");
    exit(1);
  }

  int listen_fd = open_listen_fd_or_die(port);

  buf_t buf;
  buf_init(&buf, &bufsize);

  for (int i = 0; i < n_threads; i++)
    pthread_create(&threads[i], NULL, worker, &buf);

  while (1) {

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int conn_fd =
        accept_or_die(listen_fd, (sockaddr_t *)&client_addr, &client_len);

    item_t item = {0}; // never hand a worker uninitialized stack memory
    item.connfd = conn_fd;

    if (!get_filesize(&item)) {
      close_or_die(conn_fd); // rejected: response already sent, don't queue it
      continue;
    }

    buf_insert(&buf, &item, &schedule);
  }

  free(threads);
  return 0;
}
