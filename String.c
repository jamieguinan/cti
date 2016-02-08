/* String object interface.  Has malloc and function call overhead,
 * but aims to be safer than raw <string.h> operations.  The automatic
 * size alloction for String_sprintf is one of my favorite things
 * about it.
 */

/* Please see accompanying String.licence for copyright and licensing information. */

#include "String.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "String.h"
#include "Mem.h"

static char unset_string[] = "unset_string_or_empty_result";

static String __String_none = {
  .bytes = unset_string,
  .len = sizeof(unset_string),
  .available = 0,
};

String * _String_none = &__String_none;

int String_cmp(const String *s1, const String *s2)
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
    i++;
  }
  return 1;
}


int String_ends_with(String *s, const char *s1)
{
  int i = strlen(s->bytes) - strlen(s1);

  return (streq(s->bytes+i, s1));
}


void String_cat1(String *s, const char *s1)
{
  int len = s->len + strlen(s1);

  while (s->available <= len) {
    s->available *= 2;
  }

  s->bytes = Mem_realloc(s->bytes, s->available);
  strcat(s->bytes, s1);
  s->len = len;
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
  s->len = len;
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
  s->len = len;
}


void String_set_local(String *s, const char *init)
{
  if (!init) {
    fprintf(stderr, "String_set needs an initial string, even if \"\"...\n");
    exit(1);
  }

  int init_len = strlen(init);
  s->available = 2;
  while (s->available <= init_len) {
    s->available *= 2;
  }

  if (s->bytes) { Mem_free(s->bytes); }
  s->bytes = Mem_malloc(s->available);
  strcpy(s->bytes, init);
  s->len = init_len;
}


String *String_new(const char *init)
{
  String *s = Mem_calloc(1, sizeof(*s));
  String_set_local(s, init);
  return s;
}


String * String_value_none(void)
{
  return _String_none;
}


void String_clear_local(String *s)
{
  Mem_free(s->bytes);
  s->bytes = 0L;
}


void String_free(String **s)
{
  if (String_is_none(*s)) {
    fprintf(stderr, "%s: cannot free special NONE string!\n", __func__);
    return;
  }
  if (*s) {
    String_clear_local(*s);
    Mem_free(*s);
  }
  else {
    fprintf(stderr, "%s: string is already free\n", __func__);
  }
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

void String_shorten(String *s, int newlength)
{
  if (newlength < 0) {
    fprintf(stderr, "cannot set negative length string\n");
    return;
  }
  if (newlength <= s->len) {
    s->bytes[newlength] = 0;
    s->len = newlength;
  }
}


String * String_sprintf(const char *fmt, ...)
{
  va_list ap;
  int n;
  int psize = 64;

  while (1) {
    /* p size varies, but its on the stack so no malloc required. */
    char p[psize]; 
    va_start(ap, fmt);
    n = vsnprintf(p, psize, fmt, ap);
    va_end(ap);
    /* Prior to C99, vsnprintf would return -1 if the buffer wasn't large enough.
       In C99, it returns the number of characters that would have been formatted.
       They changed the semantics!  What the fuck were they thinking?
       Either way, if it didn't fit, double the size of the stack buffer. */
    if ((n < 0)	|| (n >= psize)) {
      psize *= 2;
    }
    else {
      return String_new(p);
    }
  }
}


/* 2014-Jun-10 --> How did I not have these before?? */
void String_clear(String **s)
{
  if (*s) {
    if (!String_is_none(*s)) {
      String_free(s);
    }
  }
  *s = String_value_none();
}


void String_set(String **s, const char *init)
{
  String_clear(s);
  *s = String_new(init);
}

//void String_set_sprintf(String **s, const char *fmt, ...) {}

/* <-- 2014-Jun-10 */


int String_find(String *s, int offset, const char *s1, int *end)
{
  int i, j;
  int s1len = strlen(s1);
  for (i = offset; i <= (s->len - s1len); i++) {
    for (j = 0; j < s1len; j++) {
      if (s->bytes[i+j] != s1[j]) {
	break;
      }
    }
    if (j == s1len) {
      if (end) {
	*end = (i+j);
      }
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


int String_len(String * s)
{
  return s->len;
}


int String_is_none(String * s)
{
  return (s == _String_none ? 1 : 0);
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


String *String_replace(String *s, const char *old, const char *new)
{
  String *rs = String_new("");
  char *p1 = s->bytes;
  while (1) {
    char *p2 = strstr(p1, old);
    if (!p2) {
      String_append_bytes(rs, p1, strlen(p1));
      break;
    }
    else {
      String_append_bytes(rs, p1, p2 - p1);
      String_append_bytes(rs, new, strlen(new));
      p1 = p2 + strlen(old);
    }
  }
  return rs;
}


int String_get_char(String *str, int index)
{
  /* This returns the indexed character, as an int, or -1 if out of range. */
  if (str->len == 0) {
    return -1;
  }
  if (index < 0) {
    index = str->len - index;
  }
  if (index >= str->len) {
    return -1;
  }
  return str->bytes[index];
}


static inline int xval(char c)
{
  if (c >= 'A' && c <= 'F') {
    return (c - 'A'+ 10);
  }
  else if (c >= '0' && c <= '9') {
    return (c - '0');
  }
  else {
    return 0;
  }
}

String * String_unescape(StringConst *str)
{
  /* http://en.wikipedia.org/wiki/Percent-encoding */
  String *rs = String_new("");
  const char *p = str->bytes;
  while (*p) {
    if (p[0] == '%' && p[1] && p[2]) {
      char b[1] = { (16 * xval(p[1])) + (1 * xval(p[2])) };
      printf("setting char[%d]\n", b[0]);
      String_append_bytes(rs, b, 1);
      p += 3;
    }
    else {
      printf("setting char[%d]\n", p[0]);
      String_append_bytes(rs, p, 1);
      p += 1;
    }
  }
  return rs;
}


String * String_basename(String *str)
{
  int i;
  for (i=(str->len-2); i >= 0; i--) {
    if (str->bytes[i] == '/') {
      return String_new(&str->bytes[i]+1);
    }
  }
  return String_new(str->bytes);
}

int String_to_int(String * str, int * result)
{
  int i;
  int n = 0;
  for (i=0; i < str->len; i++) {
    int c = str->bytes[i];
    if ('0' <= c && c <= '9') {
      n = (n*10)+(c-'0');
    }
    else {
      fprintf(stderr, "%s: invalid character %c, result unchanged\n", __func__, c);
      return 1;
    }
    //printf("  n=%d\n", n);
  }

  *result = n;
  return 0;
}


int String_to_long(String * str, long * result)
{
  int i;
  long n = 0;
  for (i=0; i < str->len; i++) {
    int c = str->bytes[i];
    if ('0' <= c && c <= '9') {
      n = (n*10)+(c-'0');
    }
    else {
      fprintf(stderr, "%s: invalid character %c, result unchanged\n", __func__, c);
      return 1;
    }
    //printf("  n=%ld\n", n);
  }

  *result = n;
  return 0;
}


String_list * String_list_new(void)
{
  String_list *slst = Mem_calloc(1, sizeof(*slst));
  slst->available = 4;
  slst->_strings = Mem_calloc(1, slst->available * sizeof(String *));
  slst->len = 0;
  return slst;
}

void String_list_add(String_list *slst, String **add)
{
  slst->_strings[slst->len] = *add;
  *add = _String_none;		/* List takes ownership. */
  slst->len += 1;
  if (slst->len == slst->available) {
    slst->available *= 2;
    slst->_strings = Mem_realloc(slst->_strings,  slst->available * sizeof(String *));
  }
}

void String_list_append_s(String_list *slst, const char *svalue)
{
  String * s = String_new(svalue);
  String_list_add(slst, &s);
}


String_list * String_split_s(const char *src, const char *splitter)
{
  String_list *slst = String_list_new();
  const char *p1 = src;
  while (1) {
    char *p2 = strstr(p1, splitter);
    if (p2) {
      String *tmp = String_new("");
      String_append_bytes(tmp, p1, p2 - p1);
      String_list_add(slst, &tmp);
      p1 = p2 + strlen(splitter);
    }
    else {
      String *tmp = String_new("");
      String_append_bytes(tmp, p1, strlen(p1));
      String_list_add(slst, &tmp);
      break;
    }
  }

  return slst;
}


static String_list __String_list_none = {
  .len = 0,
  .available = 0,
};

static String_list * _string_list_none = &__String_list_none;


int String_list_is_none(String_list *slst)
{
  return (slst == _string_list_none ? 1 : 0);
}


String_list * String_split(String *str, const char *splitter)
{
  if (String_is_none(str)) {
    return String_list_value_none();
  }
  return String_split_s(s(str), splitter);
}

int String_list_len(String_list * slst)
{
  return slst->len;
}


String * String_list_get(String_list *slst, int index)
{
  if (slst->len == 0) {
    fprintf(stderr, "String_list is empty (index %d)\n", index);
    return _String_none;
  }
  else if (index >= slst->len) {
    fprintf(stderr, "String_list index %d out of range\n", index);
    return _String_none;
  }
  else if (index < 0) {
    // fprintf(stderr, "-index=%d slst->len=%d\n", -index, slst->len);
    if ((-index) <= (slst->len)) {
      /* Return from top. */
      return slst->_strings[slst->len + index];
    }
    else {
      fprintf(stderr, "String_list index %d out of range\n", index);
      return _String_none;
    }
  }
  else {
    return slst->_strings[index];
  }
}


String * String_list_find(String_list *slst, String *target)
{
  int i;
  for (i=0; i < slst->len; i++) {
    String *str = slst->_strings[i];
    if (streq(s(str), s(target))) {
      return str;
    }
  }
  return _String_none;
}


String * String_list_find_val(String_list *slst, String *key, int skip)
{
  int i;
  String *keyeq = String_dup(key);
  String_cat1(keyeq, "=");
  for (i=0; i < slst->len; i++) {
    String *str = slst->_strings[i];
    if (String_begins_with(str, s(keyeq))) {
      return String_new(s(str) + strlen(s(keyeq)));
    }
  }
  return _String_none;
}


String_list * String_list_value_none(void)
{
  return _string_list_none;
}


void String_list_free(String_list **slst)
{
  int i;
  if (String_list_is_none(*slst)) {
    fprintf(stderr, "%s: cannot free special NONE string list!\n", __func__);
    return;
  }
  String_list *lst = *slst;
  for (i=0; i < lst->len; i++) {
    String_free(&(lst->_strings[i]));
  }
  
  *slst = String_list_value_none();
}


String * String_list_pull_at(String_list * slst, int i)
{
  int i0 = i;

  if (String_list_is_none(slst)) {
    fprintf(stderr, "%s: string list is none!\n", __func__);
    return String_value_none();
  }
  
  if (slst->len == 0) {
    fprintf(stderr, "%s: string list is empty!\n", __func__);
    return String_value_none();
  }

  if (i < 0) {
    i = slst->len + i;
  }

  if (i < 0 || i >= slst->len) {
    fprintf(stderr, "%s: index %d out of range!\n", __func__, i0);
    return String_value_none();
  }  

  String * tmp = slst->_strings[i];

  int move_count = slst->len-i;
  memmove(&slst->_strings[i], &slst->_strings[i+1], sizeof(String *) * move_count);
  slst->len -= 1;

  return tmp;
}


void String_list_del_at(String_list * slst, int i)
{
  String * tmp = String_list_pull_at(slst, i);
  if (!String_is_none(tmp)) {
    String_free(&tmp);
  }
}


void String_list_trim(String_list * slst)
{
  String * tmp;

  /* Trim leading empty string. */
  tmp = String_list_get(slst, 0);
  //printf("%d %d<br>\n", String_is_none(tmp), String_eq(tmp, S("")));
  if (!String_is_none(tmp) && String_eq(tmp, S(""))) {
    //printf("deleting leading empty string<br>\n");
    tmp = String_list_pull_at(slst, 0);
    String_free(&tmp);
  }

  /* Trim trailing empty string. */
  tmp = String_list_get(slst, -1);

  if (!String_is_none(tmp) && String_eq(tmp, S(""))) {
    //printf("deleting trailing empty string<br>\n");
    tmp = String_list_pull_at(slst, -1);
    String_free(&tmp);
  }
}
