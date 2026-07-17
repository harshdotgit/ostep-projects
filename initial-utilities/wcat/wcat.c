#include <stdio.h>
#include <stdlib.h>

#define MAX_BUFLEN 127

int main(int argc, char **argv) {
  FILE *fin;
  char buf[127];

  if (argc == 0) {
    exit(0);
  }

  for (int i = 1; i < argc; i++) {

    fin = fopen(argv[i], "r");

    if (fin == NULL) {
      printf("wcat: cannot open file\n");
      exit(1);
    }

    while (fgets(buf, MAX_BUFLEN, fin) != NULL) {
      printf("%s", buf);
    }

    fclose(fin);
  }
  exit(0);
}
