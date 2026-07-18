#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("wunzip: file1 [file2 ...]\n");
    exit(1);
  }

  FILE *fin;
  char current;
  int repeats;
  int j;

  for (int i = 1; i < argc; i++) {
    fin = fopen(argv[i], "r");
    if (fin == NULL) {
      continue;
    }

    while (fread(&repeats, sizeof(int), 1, fin)) {
      fread(&current, sizeof(char), 1, fin);
      for (j = 0; j < repeats; j++)
        printf("%c", current);
    }
    fclose(fin);
  }
  exit(0);
}
