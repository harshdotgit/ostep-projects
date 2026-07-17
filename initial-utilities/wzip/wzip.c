#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("wzip: file1 [file2 ...]\n");
    exit(1);
  }

  FILE *fin;
  int current;
  int previous = EOF;
  int counter = 0;
  char prev;

  for (int i = 1; i < argc; i++) {

    fin = fopen(argv[i], "r");
    if (fin == NULL) {
      continue;
    }

    while ((current = getc(fin)) != EOF) {
      if (previous == EOF) {
        counter++;
        previous = current;
      } else if (current != previous) {
        fwrite(&counter, sizeof(int), 1, stdout);
        prev = (char)previous;
        fwrite(&prev, sizeof(char), 1, stdout);
        counter = 1;
        previous = current;
      } else
        counter++;
    }

    fclose(fin);
  }

  if (previous != EOF) {
    fwrite(&counter, sizeof(int), 1, stdout);
    prev = (char)previous;
    fwrite(&prev, sizeof(char), 1, stdout);
  }

  exit(0);
}
