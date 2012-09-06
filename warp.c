#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gmime/gmime.h>

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

GHashTable* subject_table;
GHashTable* message_id_table;

char *clean_subject(char *subject) {
  char *string, *last;
  
  while (*subject == '\t' || *subject == '\n' || *subject == ' ')
    subject++;

  string = subject;
  last = subject;
  while (*string) {
    if (*string != '\t' && *string != '\n' && *string != ' ')
      last = string + 1;
    string++;
  }
  *last = 0;
  return subject;
}

char *clean_from(char *from) {
  InternetAddress *iaddr;
  InternetAddressList *iaddr_list;

  if ((iaddr_list = internet_address_list_parse_string(from)) != NULL) {
    iaddr = internet_address_list_get_address(iaddr_list, 0);
    if (iaddr->name != NULL) {
      strcpy(from, iaddr->name);
      
      /* There's a bug in gmimelib that may leave a closing paren in
	 the name field. */
      if (strrchr(from, ')') == from + strlen(from) - 1) 
	*strrchr(from, ')') = 0;
    }
    g_object_unref(iaddr_list);
  }
  return from;
}

int parse_date(char *date) {
  return g_mime_utils_header_decode_date(date, 0);
}

typedef struct article article;

struct article {
  char *from;
  char *subject;
  char *message_id;
  int number;
  int time;
  struct article *next;
};

char *thread_line(char *buffer) {
  int number = atoi(read_elem(&buffer));
  char *subject = clean_subject(read_elem(&buffer));
  char *from = clean_from(read_elem(&buffer));
  int time = parse_date(read_elem(&buffer));
  char *message_id = read_elem(&buffer);
  char *references = read_elem(&buffer);
  article *art = calloc(1, sizeof(article));
  article *parent = NULL;
  char *ref;
  
  while (*buffer != '\n' &&
	 *buffer != 0)
    buffer++;

  if (*buffer)
    buffer++;

  if (g_hash_table_lookup(message_id_table, message_id) != NULL) {
    printf("Seen article %s before\n", message_id);
    return buffer;
  }

  ref = references + strlen(references);
  while (ref > references) {
    while (ref > references &&
	   *ref != ' ')
      ref--;
    if (ref - 1 > references)
      *(ref - 1) = 0;
    parent = (article*)g_hash_table_lookup(message_id_table, ref);
    if (parent) {
      printf("%s is parent of %s\n",
	     parent->message_id, ref);
    }
  }

  if (! parent)
    parent = (article*)g_hash_table_lookup(subject_table, subject);

  if (! parent) {
    g_hash_table_insert(subject_table, subject, (gpointer)art);
    g_hash_table_insert(message_id_table, message_id, (gpointer)art);
  } else {
    // If there is a parent, then just add this one to the last entry
    // in the list.
    while (parent->next)
      parent = parent->next;
    parent->next = art;
  }

  art->number = number;
  art->subject = subject;
  art->from = from;
  art->time = time;
  art->message_id = message_id;
  
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

  g_type_init();
  g_mime_init(0);
  
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

  subject_table = g_hash_table_new(g_str_hash, g_str_equal);
  message_id_table = g_hash_table_new(g_str_hash, g_str_equal);
  
  thread_file(nov, output);

  close(output);
  rename(tmp_name, output_name);
  
  return 0;
}
