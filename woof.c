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
#include <unistd.h>
#include <glib.h>
#include <gmime/gmime.h>

void output_file(int output, char **inputs) {
}

int main(int argc, char **argv) {
  char *output_name, *tmp_name;
  int output;
  
  g_type_init();
  g_mime_init(0);
  
  if (argc < 3) {
    printf("Usage: woof <output-file> <input-files...>\n");
    exit(-1);
  }

  output_name = argv[1];
  tmp_name = malloc(strlen(output_name) + 4);
  strcpy(tmp_name, output_name);
  strcat(tmp_name, ".tmp");

  output = open(tmp_name, O_WRONLY);
  if (! output) {
    perror("Opening output file");
    exit(-1);
  }

  output_file(output, argv + 3);

  close(output);
  rename(tmp_name, output_name);
  
  return 0;
}
