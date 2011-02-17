#ifndef _CTI_STRING_H_
#define _CTI_STRING_H_

typedef struct {
  char *bytes;
  int len;
  int available;
} String;

extern String * String_new(const char *init);
extern void String_free(String **s);

extern void String_trim_right(String *s);
extern int String_cmp(String *s1, String *s2);
extern void String_cat1(String *s, const char *s1);
extern void String_cat2(String *s, const char *s1, const char *s2);
extern void String_cat3(String *s, const char *s1, const char *s2, const char *s3);
extern String * String_sprintf(const char *fmt, ...);
extern void String_free(String **s);
extern void String_append_bytes(String *s, const char *bytes, int count);
extern int String_find(String *s, int offset, const char *s1, int *end);
extern String *String_dup(String *s);
extern int String_parse_string(String *s, int offset, String **s1);
extern int String_parse_double(String *s, int offset, double *d);
extern int String_parse_int(String *s, int offset, int *i);

#endif
