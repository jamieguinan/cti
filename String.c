#include "String.h"
#include "CTI.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

int String_cmp(String *s1, String *s2)
{
  return strcmp(s1->bytes, s2->bytes);
}

int String_begins_with(String *s, const char *s1)
{
  int i=0;
  while (*s1) {
    if (s->bytes[i] == 0) {
      return 0;
    }
    else if (s->bytes[i] != *s1) {
      return 0;
    }
    s1++;
  }
  return 1;
}

void String_cat1(String *s, const char *s1)
{
  int len = s->len + strlen(s1);

  while (s->available <= len) {
    s->available *= 2;
  }

  s->bytes = Mem_realloc(s->bytes, s->available);
  strcat(s->bytes, s1);
}


void String_cat2(String *s, const char *s1, const char *s2)
{
  int len = s->len + strlen(s1) + strlen(s2);

  while (s->available <= len) {
    s->available *= 2;
  }

  s->bytes = Mem_realloc(s->bytes, s->available);
  strcat(s->bytes, s1);
  strcat(s->bytes, s2);
}


void String_cat3(String *s, const char *s1, const char *s2, const char *s3)
{
  int len = s->len + strlen(s1) + strlen(s2) + strlen(s3);

  while (s->available <= len) {
    s->available *= 2;
  }

  s->bytes = Mem_realloc(s->bytes, s->available);
  strcat(s->bytes, s1);
  strcat(s->bytes, s2);
  strcat(s->bytes, s3);
}


String *String_new(const char *init)
{
  String *s = Mem_calloc(1, sizeof(*s));
  if (!init) {
    fprintf(stderr, "String_new needs an initial string, even if \"\"...\n");
    exit(1);
  }

  int init_len = strlen(init);
  s->available = 2;
  while (s->available <= init_len) {
    s->available *= 2;
  }
  s->bytes = Mem_malloc(s->available);
  strcpy(s->bytes, init);
  s->len = init_len;
  return s;
}


void String_free(String **s)
{
  Mem_free((*s)->bytes);
  Mem_free(*s);
  *s = 0L;
}


void _String_list_append(List *l, String **s, void (*free)(String **))
{
  if (!l->free) {
    l->free = String_free;
  }
  List_append(l, *s); /* List takes ownership. */
  *s = 0L;			
}


void String_trim_right(String *s)
{
  while (s->len && 
	 ( s->bytes[s->len-1] == ' '
	   || s->bytes[s->len-1] == '\t'
	   || s->bytes[s->len-1] == '\r'
	   || s->bytes[s->len-1] == '\n' )) 
    {
      s->bytes[s->len-1] = '\0';
      s->len -= 1;
    }

}


void List_append(List *l, void *thing)
{
  if (l->things == 0L) {
    l->things_max = 2;
    l->things = Mem_calloc(l->things_max, sizeof(*l->things));
  }
  else if (l->things_count == l->things_max) {
    l->things_max *= 2;
    l->things = Mem_realloc(l->things, l->things_max * sizeof(*l->things));
  }
  l->things[l->things_count] = thing;
  l->things_count += 1;
}


String * String_sprintf(const char *fmt, ...)
{
  String *s = NULL;
  char *p = NULL;
  va_list ap;
  int rc;
  va_start(ap, fmt);
  rc = vasprintf(&p, fmt, ap);
  if (-1 == rc) {
    perror("vasprintf");
  }
  va_end(ap);
  s = String_new(p);
  free(p);
  return s;
}


void List_free(List *l)
{
  int i;
  for (i=0; i < l->things_count; i++) {
    void *p = l->things[i];
    if (l->free) {
      l->free(&p);
    }
  }
  Mem_free(l->things);
  memset(l, 0, sizeof(l));
}


int String_find(String *s, int offset, const char *s1, int *end)
{
  int i, j;
  int s1len = strlen(s1);
  for (i = offset; i < (s->len - s1len); i++) {
    for (j = 0; j < s1len; j++) {
      if (s->bytes[i+j] != s1[j]) {
	break;
      }
    }
    if (j == s1len) {
      *end = (i+j);
      return i;
    }
  }
  return -1;
}


String *String_dup(String *s)
{
  return String_new(s->bytes);
}


int String_parse_string(String *s, int offset, String **s1)
{
  /* Allow and skip spaces at beginning. */
  while (s->bytes[offset] == ' ') { offset++; }
  *s1 = String_new(s->bytes + offset);
  return 0;
}


int String_parse_double(String *s, int offset, double *d)
{
  double result = 0.0;
  double divisor = 1.0;
  /* Allow and skip spaces at beginning. */
  while (s->bytes[offset] == ' ') { offset++; }

  while (s->bytes[offset] >= '0' && s->bytes[offset] <= '9') { 
    result = result * 10 + (s->bytes[offset] - '0');
    offset += 1;
  }
  if (s->bytes[offset] == '.') {
    offset += 1;
    while (s->bytes[offset] >= '0' && s->bytes[offset] <= '9') { 
      result = result * 10 + (s->bytes[offset] - '0'); 
      divisor *= 10;
      offset += 1;
    }
  }

  result /= divisor;

  *d = result;

  return 0;
}

int String_parse_int(String *s, int offset, int *i)
{
  int result = 0;
  /* Allow and skip spaces at beginning. */
  while (s->bytes[offset] == ' ') { offset++; }

  while (s->bytes[offset] >= '0' && s->bytes[offset] <= '9') { 
    result = result * 10 + (s->bytes[offset] - '0'); 
    offset += 1;
  }

  *i = result;

  return 0;
}


void String_append_bytes(String *s, const char *bytes, int count)
{
  int newlen = s->len + count;
  s->bytes = Mem_realloc(s->bytes, newlen+1);
  // if (Cfg.memDebug) { fprintf(stderr, "*%p (String storage)\n", s->mo.u.bytes); }
  memcpy(s->bytes + s->len, bytes, count);
  s->len = newlen;
  s->bytes[newlen] = 0;

}


List *List_new(void)
{
  List *ls = Mem_calloc(1, sizeof(*ls));
  return ls;
}

List *String_split(String *s, const char *splitter)
{
  List *ls = List_new();

  char *p1 = s->bytes;
  while (1) {
    char *p2 = strstr(p1, splitter);
    if (p2) {
      String *tmp = String_new("");
      String_append_bytes(tmp, p1, p2 - p1);
      List_append(ls, tmp);
      p1 = p2 + strlen(splitter);
    }
    else {
      String *tmp = String_new("");
      String_append_bytes(tmp, p1, strlen(p1));
      List_append(ls, tmp);
      break;
    }
  }

  return ls;
}
