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

typedef struct {
  const char *bytes;
  int len;
  int available;
} StringConst;


/* Macros for quick access to bytes. */
#define s(x) ((x)->bytes)
#define sl(x) ((x).bytes)  	/* sl for 's'tring 'l'ocal variable */

/* "Upcast" a char* to a String for easy access to the functions here. */
#define S(x) (& (String){ .bytes = x, .len = strlen(x), .available = 0} )
#define SC(x) (& (StringConst){ .bytes = x, .len = strlen(x), .available = 0} )

/* _new and _free are for free-standing strings that might be passed around independently */
extern String * String_new(const char *init);
//extern String * String_set_none(void);

extern void String_free(String **s);

/*
 * These are for static variables, stack variables, and structure
 * members.  s->bytes is freed if already set.
 */
extern void String_set_local(String *s, const char *init);
extern void String_clear_local(String *s);
//extern void String_sprintf_local(String *s, const char *fmt, ...);

/*
 * These are similar but for pointer variables.  The String is created
 * if not already allocated.  s->bytes is freed if already set.
 * These are mostly to avoid having to do "if (not string) ..."
 * when I want to set the string regardless of current value.
 */
extern void String_set(String **s, const char *init);
extern void String_clear(String **s);
extern void String_set_sprintf(String **s, const char *fmt, ...);

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
extern int String_cmp(const String *s1, const String *s2);
extern int String_begins_with(String *s, const char *s1);
extern int String_ends_with(String *s, const char *s1);
extern int String_len(String *str);
extern int String_is_none(String *str);
extern String * String_value_none(void);
extern int String_get_char(String *str, int index);
extern String * String_unescape(StringConst *str);
extern String * String_basename(String *str);


/* List of strings. */
typedef struct {
  String **_strings;
  int len;
  int available;
} String_list;

extern String_list * String_list_new(void);
extern String_list * String_list_value_none(void);
extern int String_list_is_none(String_list *slst);

extern String_list * String_split_s(const char *src, const char *splitter);
extern String_list * String_split(String *str, const char *splitter);
extern int String_list_len(String_list *slst);
extern String * String_list_get(String_list *slst, int n);
extern String * String_list_find_val(String_list *slst, String *key, int skip);
extern void String_list_add(String_list *slst, String **add);
extern String * String_list_find(String_list *slst, String *target);
extern void String_list_free(String_list **slst);
extern String * String_list_pull_at(String_list * slst, int i);
extern void String_list_del_at(String_list * slst, int i);
extern void String_list_trim(String_list * slst);

#ifndef streq
#define streq(a,b) (strcmp((a),(b))==0)
#endif

#define String_eq(s1, s2)  streq((s1)->bytes, (s2)->bytes)

#endif
