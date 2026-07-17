#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {

  if (argc <= 1) {
    printf("wgrep: searchterm [file ...]\n");
    exit(1);
  }

  FILE *fin = NULL;
  char *target = argv[1];
  char *buf = NULL;
  size_t maxlen = 0;
  ssize_t buflen;

  // TODO : goes into infinite loop. figure how getline works with stdin and fix
  // this case
  if (argc == 2) {
    while ((buflen = getline(&buf, &maxlen, stdin)) > 0) {
      if (strstr(buf, target) != NULL)
        printf("%s\n", buf);
    }
    exit(0);
  }

  for (int i = 2; i < argc; i++) {
    fin = fopen(argv[i], "r");

    if (fin == NULL) {
      printf("wgrep: cannot open file\n");
      exit(1);
    }

    while ((buflen = getline(&buf, &maxlen, fin)) > 0) {
      if (strstr(buf, target) != NULL)
        printf("%s\n", buf);
    }

    fclose(fin);
  }

  exit(0);
}
