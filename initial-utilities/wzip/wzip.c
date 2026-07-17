#include <stdio.h>
#include <stdlib.h>

// read until next char is different.
// if char is the first char:
// 1. store char
// 2. increment counter
// if next char = stored char, increment counter
// else,
// 1. store number in int array, char into char array at same position
// (maintains invariant of output = 5bytes)
// 2. store new char
// reset counter to 1

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
