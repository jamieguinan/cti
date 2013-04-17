#ifndef _CTI_STRING_H_
#define _CTI_STRING_H_

/* 
 * String data structure and functions.  Written for 7-bit ASCII,
 * although I might allow UTF-8, and functions to convert to/from
 * other encodings.
 */

typedef struct {
  char *bytes;
  int len;
  int available;
} String;

#define s(x) ((x).bytes)

/* _new and _free are for free-standing strings that might be passed around independently */
extern String * String_new(const char *init);
extern void String_free(String **s);

/*
 * _set, _set_sprintf, and _clear are for static variables, stack
 * variables, and structure members.  The _set functions free s->bytes
 * if already set.
 */
extern void String_set(String *s, const char *init);
extern void String_clear(String *s);
extern void String_set_sprintf(String *s, const char *fmt, ...);

/* These return new strings. */
extern String * String_sprintf(const char *fmt, ...);
extern String * String_dup(String *s);

/* These functions work in-place on s, although s->bytes can be reallocated. */
extern void String_cat1(String *s, const char *s1);
extern void String_cat2(String *s, const char *s1, const char *s2);
extern void String_cat3(String *s, const char *s1, const char *s2, const char *s3);
extern void String_trim_right(String *s);
extern void String_append_bytes(String *s, const char *bytes, int count);

/* Utility functions. */
extern int String_find(String *s, int offset, const char *s1, int *end);
extern int String_parse_string(String *s, int offset, String **s1);
extern int String_parse_double(String *s, int offset, double *d);
extern int String_parse_int(String *s, int offset, int *i);
extern int String_cmp(String *s1, String *s2);
extern int String_begins_with(String *s, const char *s1);

#endif
