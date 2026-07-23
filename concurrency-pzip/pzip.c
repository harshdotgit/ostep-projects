#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CAP 8192

typedef struct {
  FILE *f;
  char *fname;
} item_t;

typedef struct {
  item_t *BUF;
  int count;
  int done;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty, not_full;
} FILE_BUF;

void filebuf_init(FILE_BUF *buf) {
  buf->BUF = malloc(CAP * sizeof(item_t));
  buf->count = 0;
  buf->done = 0;
  pthread_mutex_init(&(buf->mutex), NULL);
  pthread_cond_init(&(buf->not_empty), NULL);
  pthread_cond_init(&(buf->not_full), NULL);
}

void filebuf_destroy(FILE_BUF *buf) {
  free(buf->BUF);
  pthread_mutex_destroy(&(buf->mutex));
  pthread_cond_destroy(&(buf->not_empty));
  pthread_cond_destroy(&(buf->not_full));
}

void add_file(FILE_BUF *fbuf, item_t *it) {
  pthread_mutex_lock(&(fbuf->mutex));
  while (fbuf->count == CAP)
    pthread_cond_wait(&(fbuf->not_full), &(fbuf->mutex));
  fbuf->BUF[fbuf->count++] = *it;
  pthread_cond_signal(&(fbuf->not_empty));
  pthread_mutex_unlock(&(fbuf->mutex));
}

void filebuf_close(FILE_BUF *fbuf) {
  pthread_mutex_lock(&(fbuf->mutex));
  fbuf->done = 1;
  pthread_cond_broadcast(&(fbuf->not_empty));
  pthread_mutex_unlock(&(fbuf->mutex));
}

item_t remove_file(FILE_BUF *fbuf) { return fbuf->BUF[--fbuf->count]; }

void zip(FILE *fin, FILE *fout) {
  int current;
  int previous = EOF;
  int counter = 0;
  char prev;
  while ((current = getc(fin)) != EOF) {
    if (previous == EOF) {
      counter = 1;
      previous = current;
    } else if (current != previous) {
      fwrite(&counter, sizeof(int), 1, fout);
      prev = (char)previous;
      fwrite(&prev, sizeof(char), 1, fout);
      counter = 1;
      previous = current;
    } else
      counter++;
  }
  if (previous != EOF) {
    fwrite(&counter, sizeof(int), 1, fout);
    prev = (char)previous;
    fwrite(&prev, sizeof(char), 1, fout);
  }
}

void *worker(void *args) {
  FILE_BUF *fbuf = (FILE_BUF *)args;
  while (1) {
    item_t item;
    pthread_mutex_lock(&(fbuf->mutex));
    while (fbuf->count == 0 && !fbuf->done)
      pthread_cond_wait(&(fbuf->not_empty), &(fbuf->mutex));
    if (fbuf->count == 0 && fbuf->done) {
      pthread_mutex_unlock(&(fbuf->mutex));
      break;
    }
    item = remove_file(fbuf);
    pthread_cond_signal(&(fbuf->not_full));
    pthread_mutex_unlock(&(fbuf->mutex));

    size_t n = strlen(item.fname);
    char *outname = malloc(n + 10);
    snprintf(outname, n + 10, "./%s_out.txt", item.fname);
    FILE *fout = fopen(outname, "w");
    if (fout) {
      zip(item.f, fout);
      fclose(fout);
    }
    fclose(item.f);
    free(outname);
    free(item.fname);
  }
  return NULL;
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("wzip: file1 [file2 ...]\n");
    exit(1);
  }
  long n_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (n_cores < 1)
    n_cores = 1;
  pthread_t *threads = malloc(n_cores * sizeof(pthread_t));
  FILE_BUF fbuf;
  filebuf_init(&fbuf);

  for (long i = 0; i < n_cores; i++)
    pthread_create(&threads[i], NULL, worker, &fbuf);

  for (int i = 1; i < argc; i++) {
    FILE *fin = fopen(argv[i], "r");
    if (fin == NULL)
      continue;
    const char *dot = strchr(argv[i], '.');
    size_t len = dot ? (size_t)(dot - argv[i]) : strlen(argv[i]);
    char *base = malloc(len + 1);
    memcpy(base, argv[i], len);
    base[len] = '\0';
    item_t item;
    item.f = fin;
    item.fname = base;
    add_file(&fbuf, &item);
  }

  filebuf_close(&fbuf);

  for (long i = 0; i < n_cores; i++)
    pthread_join(threads[i], NULL);

  filebuf_destroy(&fbuf);
  free(threads);
  exit(0);
}
