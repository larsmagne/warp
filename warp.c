#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <gmime/gmime.h>

#define ROOTS_PER_PAGE 30

typedef struct article article;

struct article {
  char *from;
  char *subject;
  char *message_id;
  int number;
  time_t time;
  struct article *next_article;
  struct article *next_root;
  struct article *prev_root;
  int offset;
};

char *strcasestr(const char *haystack, const char *needle);

article *first_root = NULL;
article *last_root = NULL;
int last_article = 0;
int number_of_roots = 0;

GHashTable* subject_table;
GHashTable* message_id_table;


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

// Returns the next NOV elem, but also advances the buffer pointer to
// the start of the next element.
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

char *clean_subject(char *subject) {
  char *string, *last;
  
  while (*subject == '\t' || *subject == '\n' || *subject == ' ')
    subject++;

  string = subject;
  last = subject;
  // Find the last non-blank character.
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
    if (iaddr->name != NULL)
      strcpy(from, g_mime_utils_header_decode_text(iaddr->name));
    g_object_unref(iaddr_list);
  }
  return from;
}

int parse_date(char *date) {
  return g_mime_utils_header_decode_date(date, 0);
}

char *thread_line(char *buffer) {
  int number = atoi(read_elem(&buffer));
  char *subject = clean_subject(g_mime_utils_header_decode_text(read_elem(&buffer)));
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

  art->number = number;
  art->subject = subject;
  art->from = from;
  art->time = time;
  art->message_id = message_id;

  last_article = number;
  
  // See if we can find a parent message by looking at the References
  // header.  Start at the end and work towards the beginning.
  ref = references + strlen(references);
  while (ref > references) {
    while (ref > references &&
	   *ref != ' ')
      ref--;
    if (ref - 1 > references)
      *(ref - 1) = 0;
    parent = (article*)g_hash_table_lookup(message_id_table, ref);
  }

  if (! parent && strcasestr(subject, "re:") == subject) {
    parent = (article*)g_hash_table_lookup(subject_table, subject);
    if (! parent) {
      // Skip past "Re:"
      subject += 3;
      while (*subject == ' ')
	subject++;
      parent = (article*)g_hash_table_lookup(subject_table, subject);
    }
  }

  if (! parent) {
    g_hash_table_insert(subject_table, subject, (gpointer)art);
    g_hash_table_insert(message_id_table, message_id, (gpointer)art);
    if (! first_root)
      first_root = art;
    else {
      art->prev_root = last_root;
      last_root->next_root = art;
    }
    last_root = art;
    number_of_roots++;
  } else {
    // If there is a parent, then just add this one to the last entry
    // in the list.
    while (parent->next_article)
      parent = parent->next_article;
    parent->next_article = art;
  }

  return buffer;
}

void thread_file(int nov, int output) {
  char *buffer = read_file(nov);

  while (*buffer)
    buffer = thread_line(buffer);
}

int data_size() {
  article *art = last_root, *a;
  int size = 0;
  
  while (art) {
    // Record the start point of each thread.
    art->offset = size;
    size += strlen(art->from) +
      strlen(art->subject) +
      2 +
      sizeof(time_t) +
      // We need a terminator to end the list of articles.
      sizeof(int);
    a = art;
    // We need to store all the article numbers for the thread.
    do {
      size += sizeof(int);
      a = a->next_article;
    } while (a);

    art = art->prev_root;
  }

  return size;
}

void write_data(int output) {
  int index_size = sizeof(int) * (2 + last_article +
				  number_of_roots / ROOTS_PER_PAGE + 2);
  int d_size = data_size();
  int pages;
  int total_size = index_size + d_size;
  char *index = calloc(total_size, 1);
  article *art = first_root, *a;
  int i, j;
  char *data;

  *((int*)index) = number_of_roots;
  *((int*)index + 1) = last_article;

  // Write the index that maps from article numbers to thread roots.
  for (i = 0; i < last_article && art; i++) {
    if (art->number == i) {
      *((int*)index + 2 + i) = index_size + art->offset;
      art = art->next_root;
    }
  }

  // Write the index that maps from overview page to thread roots.
  art = last_root;
  if (number_of_roots / ROOTS_PER_PAGE * ROOTS_PER_PAGE == number_of_roots)
    pages = number_of_roots / ROOTS_PER_PAGE;
  else
    pages = number_of_roots / ROOTS_PER_PAGE + 1;
  for (i = 0; i < pages; i++) {
    *((int*)index + 2 + last_article + i) = index_size + art->offset;
    for (j = 0; j < ROOTS_PER_PAGE && art; j++) 
      art = art->prev_root;
  }
  *((int*)index + 2 + last_article + i) = total_size;

  // Write the data portion.
  art = last_root;
  data = index + index_size;

  while (art) {
    data += sprintf(data, "%s\n%s\n", art->from, art->subject);
    *((time_t*)data) = art->time;
    data += sizeof(time_t);
    a = art;
    // We need to store all the article numbers for the thread.
    do {
      *((int*)data) = a->number;
      data += sizeof(int);
      a = a->next_article;
    } while (a);
    // Zero-terminate the list of articles.
    *((int*)data) = 0;
    data += sizeof(int);

    art = art->prev_root;
  }

  if (write(output, index, total_size) < total_size) {
    perror("Writing index");
  }
  free(index);
}

int main(int argc, char **argv) {
  int nov, output;
  char *output_name, *tmp_name;

  g_mime_init(GMIME_ENABLE_RFC2047_WORKAROUNDS);
  
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
  tmp_name = malloc(strlen(output_name) + 5);
  strcpy(tmp_name, output_name);
  strcat(tmp_name, ".tmp");

  output = open(tmp_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);
  if (output < 0) {
    perror("Opening output file");
    exit(-1);
  }

  subject_table = g_hash_table_new(g_str_hash, g_str_equal);
  message_id_table = g_hash_table_new(g_str_hash, g_str_equal);

  // Read all the NOV data and create all the structures.
  thread_file(nov, output);

  // Write out the index data and the root data to the file.
  write_data(output);
  
  close(output);
  rename(tmp_name, output_name);
  
  return 0;
}
