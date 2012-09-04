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

char *read_file(int fd) {
  struct stat buf;
  char *buffer;

  fstat(fd, &buf);
  buffer = calloc(buf.st_size + 1, 1);
  if (read(fd, buffer, buf.st_size) < buf.st_size) {
    perror("Read error");
    exit(-1);
  }

  return buffer;
}

char *read_elem(char **buffer) {
  char *elem = *buffer;
  while (**buffer != '\n' &&
	 **buffer != '\t' &&
	 **buffer != 0)
    *buffer = *buffer + 1;
  // End the elem by turning the TAB into a zero.
  **buffer = 0;
  *buffer = *buffer + 1;
  return elem;
}

char *thread_line(char *buffer) {
  char *number = read_elem(&buffer);

  while (*buffer != '\n' &&
	 *buffer != 0)
    buffer++;

  if (*buffer)
    buffer++;

  //printf("%s\n", number);
  return buffer;
}

void thread_file(int nov, int output) {
  char *buffer = read_file(nov);

  while (*buffer)
    buffer = thread_line(buffer);
}

int main(int argc, char **argv) {
  int nov, output;
  char *output_name, *tmp_name;
  
  if (argc != 3) {
    printf("Usage: warp <nov-file> <output-file>\n");
    exit(-1);
  }

  nov = open(argv[1], O_RDONLY);
  if (nov < 0) {
    perror("Opening NOV file");
    exit(-1);
  }

  output_name = argv[2];
  tmp_name = malloc(strlen(output_name) + 4);
  strcpy(tmp_name, output_name);
  strcat(tmp_name, ".tmp");

  output = open(tmp_name, O_WRONLY);
  if (! output) {
    perror("Opening output file");
    exit(-1);
  }

  thread_file(nov, output);

  close(output);
  rename(tmp_name, output_name);
  
  return 0;
}
