#ifndef _CTI_H_
#define _CTI_H_

#include <stdint.h>

#define cti_min(a, b)  ( (a) < (b) ? (a) : (b) )
#define cti_max(a, b)  ( (a) > (b) ? (a) : (b) )

#include "locks.h"

/* This "List" structure will probably be obviated by ISet.  Or not. */
typedef struct {
  void **things;
  int things_max;
  int things_count;
  void (*free)(/* Arbitary args! */);
} List;

typedef List VPList;

#include "String.h"
#include "Mem.h"
#include "Index.h"

#include "Collection.h"

extern List *String_split(String *s, const char *splitter);

typedef struct {
  /* Intentionally empty struct. */
} Message;


/* 
 * The ISet() macro is for declaring structures compatible with the
 * ISet_* macros.  "index" should initially be 0L, allocated and used
 * as needed.
 */
#define ISet(type) struct { type **items; int avail; int count; Index *index; } 

extern void List_append(List *l, void *thing);
extern void List_free(List *l);
extern void _String_list_append(List *l, String **s, void (*free)(String **));
#define String_list_append(l, s) _String_list_append(l, s, String_free)

struct _Instance;

/* Input, Output, Instance, Template. */
typedef struct {
  const char *type_label;	/* Must match up with corresponding Output. */
  struct _Instance *parent;
  void (*handler)(struct _Instance *pi, void *data);
  int warn_max;
} Input;

typedef struct {
  const char *type_label;	/* Must match up with corresponding Input. */
  Input *destination;		/* Pointer to an Input. */
} Output;

/* Range, set of values that a config item can take. */
enum { 
  RANGE_UNKNOWN=0, 	/* default for unset */
  RANGE_STRINGS, 
  RANGE_INTS, 
  RANGE_FLOATS,
};

typedef struct {
  int min;
  int max;
  int step;
  int _default;
} IntRange;

typedef struct {
  float min;
  float max;
} FloatRange;

typedef struct {
  int type;			/* One of the enum values just above. */
  ISet(String) strings;
  ISet(String) descriptions;
  IntRange ints;
  FloatRange floats;
} Range;

extern Range *Range_new(int type);
extern int Range_match_substring(Range *r, const char *substr);
extern void Range_free(Range *r);

typedef struct {
  int type;
  union {
    String *string_value;  		/* Dynamically allocated. */
    int int_value;
    float float_value;
  } u;				/* Please use accessor functions... */
} Value;

extern Value *Value_new(int type);
extern Value *Value_new_int(int i);
extern Value *Value_new_float(float f);
extern Value *Value_new_string(String *s);
extern void Value_free(Value *v);
/* These accessor functions check the type and (should) signal an error if Value is wrong type,
   and return dummy values in that case. */
extern int Value_get_int(Value *v);
extern float Value_get_float(Value *v);
extern String * Value_get_string(Value *v);


/* Config items.  Look up label in table, call corresponding setter function, which
   should know how to interpret the value.  Thus, don't need to pass label to
   the setter function. */
typedef struct {
  const char *label;
  int (*set)(struct _Instance *pi, const char *value); /* Setter function. */
  void (*get_value)(struct _Instance *pi, Value *value);
  void (*get_range)(struct _Instance *pi, Range *range);
} Config;

enum { INSTANCE_STATE_RUNNING,  INSTANCE_STATE_QUIT };

typedef struct _Handler_message {
  struct _Handler_message *prev;
  void (*handler)(struct _Instance *pi, void *data);
  void *data;
} Handler_message;

typedef struct _Instance {
  const char *label;		/* Copied from corresponding Template */
  /* NO instance label!  Containers should hold label:instance mapping, do not put it here. */

  void *data;			/* Instance-specific data, usally accessed as "priv". */
  void (*tick)(struct _Instance *pi); /* One "unit" of processing. */

  Input *inputs;
  int num_inputs;

  Output *outputs;
  int num_outputs;

  //Config *configs;
  //int num_configs;

  int state;
  int counter;			/* Update in tick function. */

  WaitLock notification_lock;

  Lock inputs_lock;		
  Event inputs_event;
  int waiting; 			/* 0 or 1, better to treat as boolean than counter. */

  Handler_message *msg_first;
  Handler_message *msg_last;
  int pending_messages;		/* FIXME: get rid of this? */
} Instance;

extern void Connect(Instance *from, const char *label, Instance *to);
extern void Connect2(Instance *from, const char *fromlabel, Instance *to, const char *tolabel);

extern void PostData(void *pmsg, Input *destination);
#define PostDataAndClear(m, d) do {PostData(m, d); m = 0L; } while (0)
extern Handler_message *GetData(Instance *pi, int wait_flag);
extern Handler_message *GetData_and_requests(Instance *pi, int wait_flag, Config *config_table, int num_configs);
extern void ReleaseMessage(Handler_message **);
extern int CountPendingMessages(Instance *pi);

extern void Instance_wait(Instance *pi); /* Wait for notification, then go and check inputs. */

/* A Template is usually statically declared in a module, and its
   members set up in an _init() function.   The the .new() function
   allocates an Instance and copies the inputs and outputs, which
   have ".type_label" set, then sets up the other fields. */
typedef struct _Template {
  const char *label;

  Input *inputs;
  int num_inputs;

  Output *outputs;
  int num_outputs;

  //Config *configs;
  //int num_configs;

  void (*tick)(struct _Instance *pi); /* One "unit" of processing. */

  /* Control, info (status)... */

  //Instance *(*new)(struct _Template *);

  void (*instance_init)(Instance *pi);
} Template;

extern Template **all_templates;
extern int num_templates;

extern void Template_register(Template *t);
extern void Template_list(void);

extern Instance * Instantiate(const char *label);

extern void Instantiate_and_run(const char *label);

typedef struct {
  String *label;
  String *value;
  Value *vreq;
  Range *rreq;
  Event *wake;
} Config_buffer;

extern Config_buffer *Config_buffer_new(const char *label, const char *value);
extern Config_buffer *Config_buffer_vrreq_new(const char *label, const char *value, Value *vreq, Range *rreq, Event *event);
extern void Config_buffer_discard(Config_buffer **cb);

extern void Generic_config_handler(Instance *pi, void *data, Config *config_table, int config_table_size);

extern int SetConfig(Instance *pi, const char *label, const char *value);
extern void GetConfigValue(Input *pi, const char *label, Value *vreq);
extern void GetConfigRange(Input *pi, const char *label, Range *rreq);

typedef struct {
  ISet(Instance) instances;
} InstanceGroup;

/* I admit C++ would do a better job at keeping code size down here.  On the
   other hand, data sets are typically huge by comparison, compilers might
   be able to find and coalesce common code, and C++ has much other baggage
   that I'm happy to avoid. */
#define ISet_add(iset, pitem) {  \
  if (iset.items == 0L) { \
    iset.avail = 2; \
    iset.items = Mem_calloc(iset.avail, sizeof(pitem)); \
  } \
  else if (iset.avail == iset.count) { \
    iset.avail *= 2; \
    iset.items = Mem_realloc(iset.items, iset.avail * sizeof(pitem)); \
  } \
  iset.items[iset.count] = pitem; \
  iset.count += 1; \
}

#define ISet_add_keyed(iset, key, pitem) {	\
  ISet_add(iset, pitem); \
  Index_update(&iset.index, key, pitem); \
}

#define ISet_pop(iset, pitem) {  \
  pitem = iset.items[0]; \
  iset.count -= 1; \
  memmove(iset.items, &iset.items[1], iset.count * sizeof(iset.items[0])); \
  iset.items[iset.count] = 0; \
}

#define ISet_clear(iset) { \
  if (iset.items) Mem_free(iset.items); \
  if (iset.index) Index_clear(&iset.index);	\
  memset(&iset, 0, sizeof(iset));	\
  /* FIXME: Clear the index! */ \
}

extern InstanceGroup *InstanceGroup_new(void);
extern void InstanceGroup_add(InstanceGroup *g, const char *typeLabel, String *instanceLabel);
extern Instance * InstanceGroup_find(InstanceGroup *g, String *instanceLabel);
extern void InstanceGroup_wait(InstanceGroup *g);
extern void InstanceGroup_free(InstanceGroup **g);
extern void InstanceGroup_connect(InstanceGroup *g, 
				  String * instanceLabel1,
				  const char *ioLabel,
				  String * instanceLabel2);

extern void InstanceGroup_connect2(InstanceGroup *g, 
				   String * instanceLabel1,
				   const char *ioLabel1,
				   String * instanceLabel2,
				   const char *ioLabel2);


/* Callback function, with one parameter. */
typedef struct {
  void (*func)(void *data);
  void *data;			/* passed to .func */
  Lock lock;
  Event event;
} Callback;

extern Callback *Callback_new(void);
extern void Callback_wait(Callback *cb);
extern void Callback_fill(Callback *cb, int (*func)(void *), void *data);

/* Raw data buffer */
typedef struct {
  uint8_t *data;
  int data_length;
} RawData_buffer;

/* Raw data node */
typedef struct _RawData_node {
  struct _RawData_node *next;
  RawData_buffer *buffer;
  int seq;
} RawData_node;

extern RawData_buffer *RawData_buffer_new(int size);
extern void RawData_buffer_discard(RawData_buffer *raw);

/* Feedback */
typedef struct {
  int seq;
} Feedback_buffer;

extern Feedback_buffer *Feedback_buffer_new(void);
extern void Feedback_buffer_discard(Feedback_buffer *raw);


/* Useful macros. */
#define copy_list(a, b, c, d) do { int n = sizeof(*a)*b; *c = Mem_malloc(n); memcpy(*c, a, n); *d = b;} while (0);
#define table_size(x) (sizeof(x)/sizeof(x[0]))
#define streq(a, b)  (strcmp(a, b) == 0)
#define timeval_to_double(t) ((double)(t.tv_sec + t.tv_usec/1000000.0))
#define double_to_timespec(d) (&(struct timespec) { .tv_sec = floor(d), .tv_nsec = fmod(d, 1.0)  * 1000000000})

extern void cti_debug_printf_register(const char *file, int line, int *enabled, const char *fmt);
extern void cti_debug_printf(const char *fmt, ...);
extern void cti_debug_printf_list(void);
extern void cti_debug_printf_toggle(int index);

/* Debug printfs */
#define dpf(fmt, ...) \
do { \
  static int enabled=1; \
  static int registered; \
  if (enabled) { \
    if (!registered) { \
      registered = 1; \
      enabled = 0; \
      cti_debug_printf_register(__FILE__, __LINE__, &enabled, fmt); \
    } \
    else { \
      cti_debug_printf(fmt, __VA_ARGS__); \
    } \
  } \
 } while(0)

#ifdef CTI_SHARED
#define shared_export(sym) void cti_init(void) { sym(); }
#else
#define shared_export(sym)
#endif

#endif
