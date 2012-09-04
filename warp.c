#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

void thread_file(FILE *nov, FILE *output) {
}

int main(int argc, char **argv) {
  FILE *nov, *output;
  char *output_name, *tmp_name;
  
  if (argc != 3) {
    printf("Usage: warp <nov-file> <output-file>\n");
    exit(-1);
  }

  nov = fopen(argv[1], "r");
  if (! nov) {
    perror("Opening NOV file");
    exit(-1);
  }

  output_name = argv[2];
  tmp_name = malloc(strlen(output_name) + 4);
  strcpy(tmp_name, output_name);
  strcat(tmp_name, ".tmp");

  output = fopen(tmp_name, "w");
  if (! output) {
    perror("Opening output file");
    exit(-1);
  }

  thread_file(nov, output);

  fclose(output);
  rename(tmp_name, output_name);
  
  return 0;
}
