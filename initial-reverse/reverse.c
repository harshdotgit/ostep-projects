#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
  if (argc >= 4) {
    fprintf(stderr, "usage: reverse <input> <output>\n");
    exit(1);
  }

  if (argc == 3 && (strcmp(argv[1], argv[2]) == 0)) {
    fprintf(stderr, "reverse: input and output file must differ\n");
    exit(1);
  }

  char *line = NULL;
  size_t linecapp = 0;
  size_t bufsize = 1024;
  size_t idx = 1;
  char **buf = malloc(bufsize * sizeof(char *));
  if (buf == NULL) {
    fprintf(stderr, "malloc failed\n");
    exit(1);
  }

  FILE *fin = stdin;
  FILE *fout = stdout;

  if (argc >= 2) {
    fin = fopen(argv[1], "r");
    if (!fin) {
      fprintf(stderr, "reverse: cannot open file '%s'\n", argv[1]);
      exit(1);
    }
  }

  if (argc == 3) {
    fout = fopen(argv[2], "w");
    if (!fout) {
      fprintf(stderr, "reverse: cannot open file '%s'\n", argv[2]);
      exit(1);
    }

    struct stat sin, sout;
    if (fstat(fileno(fin), &sin) == 0 && fstat(fileno(fout), &sout) == 0 &&
        sin.st_ino == sout.st_ino && sin.st_dev == sout.st_dev) {
      fprintf(stderr, "reverse: input and output file must differ\n");
      exit(1);
    }
  }

  while (getline(&line, &linecapp, fin) != -1) {
    if (idx >= bufsize) {
      bufsize *= 2;
      char **tmp = realloc(buf, bufsize * sizeof(char *));
      if (!tmp) {
        fprintf(stderr, "malloc failed\n");
        free(buf);
        free(line);
        exit(1);
      }
      buf = tmp;
    }
    buf[idx++] = strdup(line);
  }

  for (size_t i = idx - 1; i > 0; i--)
    fprintf(fout, "%s", buf[i]);

  for (size_t i = 0; i < idx; i++)
    free(buf[i]);
  free(line);
  free(buf);

  if (fin != stdin)
    fclose(fin);
  if (fout != stdout)
    fclose(fout);

  exit(0);
}
