#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <stdint.h>		/* uint8_t */
#include <string.h>		/* strcmp */
#include <unistd.h>		/* sleep */
#include <time.h>		/* nanosleep */
#include <stdarg.h>		/* vprintf */

#define streq(a, b)  (strcmp(a, b) == 0)

#include "CTI.h"
#include "Mem.h"
#include "Cfg.h"



static void * Instance_thread_main(void *vp)
{
  Instance *pi = vp;

  while (1) {
    pi->tick(pi);
  }
  return 0L;
}

void Instance_loop_thread(Instance *pi)
{
  /* FIXME: Abstract this out one layer... */
  pthread_t thread;
  pthread_create(&thread, NULL, Instance_thread_main, (void*)pi);
  pthread_detach(thread);
}

void PostData(void *data, Input *input)
{
  Handler_message *hm = Mem_calloc(1, sizeof(*hm));

  if (!input->handler) {
    while (1) {
      fprintf(stderr, "instance %s input %s does not have a handler!\n", 
	      input->parent->label, input->type_label);
      sleep(1);
    }
  }
  hm->handler = input->handler;
  hm->data = data;

  /* Wait for lock, lock... */
  Lock_acquire(&input->parent->inputs_lock);

  if (input->parent->msg_first == 0L) {
    /* input->parent->msg_last should also be 0L in this case... */
    input->parent->msg_first = hm;
    input->parent->msg_last = hm;
  }
  else {
    /* At least one node. */
    input->parent->msg_last->prev = hm;
    input->parent->msg_last = hm;
  }

  if (input->parent->waiting) {
    Event_signal(&input->parent->inputs_event);
  }

  input->parent->pending_messages += 1;

  if (cfg.verbosity & 0x01000000) {
    fprintf(stderr, "%s: %d queued messages\n", input->parent->label, input->parent->pending_messages);
  }

  Lock_release(&input->parent->inputs_lock);
}

Handler_message *GetData(Instance *pi, int wait_flag)
{
  /* Return handler message if available.  If no message available, return NULL if
     wait_flag is 0, else wait until a message is available and return it. */
  Handler_message *hm = 0L;

  /* This is a quick hack "pause all the things" implementation. */
  while (cfg.pause) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/50}, NULL);
  }

  Lock_acquire(&pi->inputs_lock);

  if (pi->msg_first == 0L && pi->msg_last == 0L && wait_flag == 0)  {
    goto out;
  }

  while (pi->msg_first == 0L && pi->msg_last == 0L) {
    pi->waiting = 1;
    Lock_release__event_wait__lock_acquire(&pi->inputs_lock, &pi->inputs_event);
  }

  pi->waiting = 0;

  if (pi->msg_first == pi->msg_last) {
    /* Single node */
    hm = pi->msg_first;
    hm->prev = 0L;
    pi->msg_first = 0L;
    pi->msg_last = 0L;
  }
  else {
    /* >1 nodes. */
    hm = pi->msg_first;
    pi->msg_first = hm->prev;
    hm->prev = 0L;
  }

  pi->pending_messages -= 1;

out:
  Lock_release(&pi->inputs_lock);

  return hm;
}


int CountPendingMessages(Instance *pi)
{
  int count = 0;
  Handler_message *p;

  Lock_acquire(&pi->inputs_lock);

  if (pi->msg_first == 0L) {
    goto out;
  }

  for (p = pi->msg_last; p; p = p->prev) {
    count += 1;
  }

 out:
  Lock_release(&pi->inputs_lock);
  return count;
}

void ReleaseMessage(Handler_message **msg)
{
  Mem_free(*msg);
  *msg = 0L;
}


static ISet(Template) templates = {};

void Template_register(Template *t)
{
  ISet_add(templates, t);
}

void Template_list(void)
{
  int i, j;
  printf("Templates:\n");

  for (i=0; i < templates.count; i++) {
    printf("[%d] %s\n", i, templates.items[i]->label);
    printf("  Inputs:\n");
    for (j=0; j < templates.items[i]->num_inputs; j++) {
      if (templates.items[i]->inputs[j].type_label) {
	printf("  [%d] %s\n", j, templates.items[i]->inputs[j].type_label);
      }
      else {
	printf("  [%d] *** UNINITIALIZED\n", j);
      }
    }
    
    printf("  Outputs:\n");
    for (j=0; j < templates.items[i]->num_outputs; j++) {
      if (templates.items[i]->outputs[j].type_label) {
	printf("  [%d] %s\n", j, templates.items[i]->outputs[j].type_label);      
      }
      else {
	printf("  [%d] *** UNINITIALIZED\n", j);
      }
    }
  }
}

static Instance * _Instantiate_local(const char *label, int run)
{
  int i;

  for (i=0; i < templates.count; i++) {
    if (strcmp(templates.items[i]->label, label) == 0) {
      Template *t = templates.items[i];
      Instance *pi = Mem_calloc(1, sizeof(*pi));
      pi->label = t->label;
      pi->tick = t->tick;

      /* Inputs and Ouputs are declared statically in each module
	 file.  Here we make a copy and initialize instance-specific
	 structure fields. */
      copy_list(t->inputs, t->num_inputs, &pi->inputs, &pi->num_inputs);
      copy_list(t->outputs, t->num_outputs, &pi->outputs, &pi->num_outputs);
      /* When a message is posted, the caller needs to find the Input's parent
	 and add to the notification queue. */
      for (i=0; i < t->num_inputs; i++) {
	pi->inputs[i].parent = pi;
      }

      Lock_init(&pi->inputs_lock);
      WaitLock_init(&pi->notification_lock);

      /* Type-specific instance initialization. */
      if (t->instance_init) {
	t->instance_init(pi);
      }

      if (run) {
	/* Run instance main loop, blocking the caller. */
	Instance_thread_main(pi);
      }
      else {
	/* Start a new thread. */
	Instance_loop_thread(pi);
      }
      return pi;
    }
  }

  fprintf(stderr, "No such template: %s\n", label);
  return 0L;
}


void Instantiate_and_run(const char *label)
{
  _Instantiate_local(label, 1);
}


Instance * Instantiate(const char *label)
{
  return _Instantiate_local(label, 0);
}


Config_buffer *Config_buffer_new(const char *label, const char *value)
{
  Config_buffer *cb = Mem_calloc(1, sizeof(*cb));
  cb->label = String_new(label);
  if (value) {
    cb->value = String_new(value);
  }
  return cb;
}


Config_buffer *Config_buffer_vrreq_new(const char *label, const char *value, Value *vreq, Range *rreq,
				       Event *event)
{
  Config_buffer *cb = Config_buffer_new(label, value);
  cb->vreq = vreq;
  cb->rreq = rreq;
  cb->wake = event;
  return cb;
}


void Config_buffer_discard(Config_buffer **cb)
{
  String_free(&(*cb)->label);
  if ((*cb)->value) {
    String_free(&(*cb)->value);
  }
  Mem_free(*cb);
  *cb = 0L;
}


void Generic_config_handler(Instance *pi, void *data, Config *config_table, int config_table_size)
{
  /* Most modules can use this function to handle config messages. */
  Config_buffer *cb_in = data;
  int i;

  /* Walk the config table. */
  for (i=0; i < config_table_size; i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {

      /* If value is passed in, call the set function. */
      if (cb_in->value && config_table[i].vset) {
	/* Generic setter. */
	config_table[i].vset((uint8_t*)pi->data + config_table[i].value_offset, 
			     cb_in->value->bytes);
      }
      else if (cb_in->value && config_table[i].set) {
	/* Template-specific setter. */
	int rc;		/* FIXME: What to do with this? */
	rc = config_table[i].set(pi, cb_in->value->bytes);
      }

      /* Check and fill range/value requests. */
      if (cb_in->vreq && config_table[i].get_value) {
	config_table[i].get_value(pi, cb_in->vreq);
      }

      if (cb_in->rreq && config_table[i].get_range) {
	config_table[i].get_range(pi, cb_in->rreq);
      }

      /* Wake caller if they asked for it. */
      if (cb_in->wake) {
	Event_signal(cb_in->wake); // FIXME: Is this necessary?
      }
      break;
    }
  }
  
  Config_buffer_discard(&cb_in);
}


void cti_set_int(void *addr, const char *value)
{
  *( (int *)addr) = atoi(value);
}

void GetConfigValue(Input *pi, const char *label, Value *vreq)
{
  Lock lock = {};
  Event event = {};
  Config_buffer *cb;

  Lock_init(&lock);
  cb = Config_buffer_vrreq_new(label, 0L, vreq, 0L, &event);
  Lock_acquire(&lock);
  PostData(cb, pi);
  Lock_release__event_wait__lock_acquire(&lock, &event);
  Lock_release(&lock);
}

void GetConfigRange(Input *pi, const char *label, Range *rreq)
{
  Lock lock = {};
  Event event = {};
  Config_buffer *cb;

  Lock_init(&lock);
  cb = Config_buffer_vrreq_new(label, 0L, 0L, rreq, &event);
  Lock_acquire(&lock);
  PostData(cb, pi);
  Lock_release__event_wait__lock_acquire(&lock, &event);
  Lock_release(&lock);
}


void Connect(Instance *from, const char *label, Instance *to)
{
  int i;
  int from_index = -1;
  int to_index = -1;
  
  for (i=0; i < from->num_outputs ; i++) {
    if (from->outputs[i].type_label && streq(from->outputs[i].type_label, label)) {
      from_index = i;
      if (from->outputs[i].destination == 0L) {
	/* This is a bit subtle.  The code will set the output to the first matching
	   and unset output, or override the last set output. */
	break;
      }
    }
  }

  if (from_index == -1) {
    fprintf(stderr, "Instance of %s does not have an output labelled '%s'\n", from->label, label);
    fflush(stderr);
    exit(1);
    return;
  }

  for (i=0; i < to->num_inputs ; i++) {
    if (to->inputs[i].type_label && streq(to->inputs[i].type_label, label)) {
      to_index = i;
      break;
    }
  }

  if (to_index == -1) {
    fprintf(stderr, "Instance of %s does not have an input labelled '%s'\n", to->label, label);
    exit(1);
    return;
  }

  from->outputs[from_index].destination = &to->inputs[to_index];

  fprintf(stderr, "connected: %s.%d [%s] %s.%d\n", from->label, from_index, label, to->label, to_index);
}


void Connect2(Instance *from, const char *fromlabel, Instance *to, const char *tolabel)
{
  int i;
  int from_index = -1;
  int to_index = -1;
  
  for (i=0; i < from->num_outputs ; i++) {
    if (from->outputs[i].type_label && streq(from->outputs[i].type_label, fromlabel)) {
      from_index = i;
      if (from->outputs[i].destination == 0L) {
	/* This is a bit subtle.  The code will set the output to the first matching
	   and unset output, or override the last set output. */
	break;
      }
    }
  }

  if (from_index == -1) {
    fprintf(stderr, "Instance of %s does not have an output labelled '%s'\n", from->label, fromlabel);
    fflush(stderr);
    exit(1);
    return;
  }

  for (i=0; i < to->num_inputs ; i++) {
    if (to->inputs[i].type_label && streq(to->inputs[i].type_label, tolabel)) {
      to_index = i;
      break;
    }
  }

  if (to_index == -1) {
    fprintf(stderr, "Instance of %s does not have an input labelled '%s'\n", to->label, tolabel);
    exit(1);
    return;
  }

  /* Verify that labels match up... */


  from->outputs[from_index].destination = &to->inputs[to_index];

  fprintf(stderr, "connected: %s:%s %s:%s\n", from->label, fromlabel, to->label, tolabel);
}


InstanceGroup *InstanceGroup_new(void)
{
  InstanceGroup *g = Mem_calloc(1, sizeof(*g));
  return g;
}


void InstanceGroup_add(InstanceGroup *g, const char *typeLabel, String *instanceLabel)
{
  Instance *pi = 0L;
  Instance *check;

  check = 0L;  /* FIXME: Make sure there isn't already an instance
		  with the same instanceLabel. */
  if (check) {
    return;
  }

  pi = Instantiate(typeLabel);

  if (!pi) {
    return;
  }

  /* Add pi to g's set of instances. */
  /* Pass "instanceLabel" as "key" parameter, for later lookup. */
  ISet_add_keyed(g->instances, instanceLabel, pi);
}


Instance *InstanceGroup_find(InstanceGroup *g, String *ilabel)
{
  if (g->instances.index) {
    /* Look up using hash index. */
    int found = 0;
    Instance *pi = Index_find(g->instances.index, ilabel, &found);
    return pi;
  }
  else {
    /* Walk the list... */
  }
  return 0L;
}


void InstanceGroup_connect(InstanceGroup *g, 
			   String * instanceLabel1,
			   const char *ioLabel,
			   String * instanceLabel2)
{
  Instance *pi1 = InstanceGroup_find(g, instanceLabel1);
  if (!pi1) {  fprintf(stderr, "could not find instance '%s'\n", instanceLabel1->bytes); return; }
  Instance *pi2 = InstanceGroup_find(g, instanceLabel2);
  if (!pi2) {  fprintf(stderr, "could not find instance '%s'\n", instanceLabel2->bytes); return; }
  Connect(pi1, ioLabel, pi2);
}


void InstanceGroup_connect2(InstanceGroup *g, 
			    String * instanceLabel1,
			    const char *oLabel,
			    String * instanceLabel2,
			    const char *iLabel
			    )
{
  Instance *pi1 = InstanceGroup_find(g, instanceLabel1);
  if (!pi1) {  fprintf(stderr, "could not find instance '%s'\n", instanceLabel1->bytes); return; }
  Instance *pi2 = InstanceGroup_find(g, instanceLabel2);
  if (!pi2) {  fprintf(stderr, "could not find instance '%s'\n", instanceLabel2->bytes); return; }
  Connect2(pi1, oLabel, pi2, iLabel);
}


void InstanceGroup_wait(InstanceGroup *g)
{
  while (1) {
    /* FIXME: Wait for all instances to "finish", whatever that turns out to mean. */
    nanosleep(&(struct timespec){.tv_sec = 1, .tv_nsec = 0}, NULL);
  }
}

void InstanceGroup_free(InstanceGroup **g)
{
  /* FIXME: Free Instances.  Maybe check their status first? */
  Mem_free(*g);
  *g = 0L;
}

Value *Value_new(int type)
{
  Value *v = Mem_calloc(1, sizeof(*v));
  v->type = type;
  return v;
}


void Value_free(Value *v)
{
  if (v->type == RANGE_STRINGS) {
    String_free(&v->u.string_value);
  }
  Mem_free(v);
}


/* Callback API  */
Callback *Callback_new(void)
{
  Callback *cb = Mem_calloc(1, sizeof(*cb));
  Lock_init(&cb->lock);
  return cb;  
}


void Callback_wait(Callback *cb)
{
  /* This just waits someone else to lock/unlock the lock. */
  Lock_acquire(&cb->lock);
  Lock_release__event_wait__lock_acquire(&cb->lock, &cb->event);
  Lock_release(&cb->lock);
}


void Callback_fill(Callback *cb, int (*func)(void *), void *data)
{
  Lock_acquire(&cb->lock);  
  cb->func = func;
  cb->data = data;
  Event_signal(&cb->event);  // Is this even necessary?  It probably doesn't hurt.
  Lock_release(&cb->lock);  
}

/* Raw data buffer API. */
RawData_buffer *RawData_buffer_new(int size)
{
  RawData_buffer *raw = Mem_malloc(sizeof(*raw));
  raw->data_length = size;
  raw->data = Mem_calloc(1, raw->data_length); 	/* Caller must fill in data! */
  return raw;
}

void RawData_buffer_discard(RawData_buffer *raw)
{
  Mem_free(raw->data);
  memset(raw, 0, sizeof(*raw));
  Mem_free(raw);
}


Feedback_buffer *Feedback_buffer_new(void)
{
  Feedback_buffer *fb = Mem_malloc(sizeof(*fb));
  /* Caller must fill in fields. */
  return fb;
}

void Feedback_buffer_discard(Feedback_buffer *fb)
{
  Mem_free(fb);
}


typedef struct {
  const char *file;		/* use streq rather than == for comparison, to allow
				   registering before first call */
  int line;
  int *enabled;
  const char *fmt;
} DebugPrintfRecord;

struct {
  DebugPrintfRecord *records;
  int count;
  int available;
} printfRecords;


void cti_debug_printf_register(const char *file, int line, int *enabled, const char *fmt)
{
  int i;
  for (i=0; i < printfRecords.count; i++) {
    if (streq(file, printfRecords.records[i].file)
	&& line == printfRecords.records[i].line
	&& streq(fmt, printfRecords.records[i].fmt))
      {
	/* This entry was pre-registered. */
	printfRecords.records[i].enabled = enabled;
	/* Enable it! */
	*enabled = 1;
	return;
      }
  }

  if (!printfRecords.available) {
    printfRecords.available = 32;
    printfRecords.records = calloc(printfRecords.available, sizeof(*printfRecords.records));
  }
  else if (printfRecords.available == (printfRecords.count+1)) {
    printfRecords.available *= 2;
    printfRecords.records = realloc(printfRecords.records, 
				    printfRecords.available*sizeof(*printfRecords.records));
  }
  
  printfRecords.records[printfRecords.count].file = file;
  printfRecords.records[printfRecords.count].line = line;
  printfRecords.records[printfRecords.count].enabled = enabled;
  printfRecords.records[printfRecords.count].fmt = fmt;
  
  printfRecords.count += 1;
}


void cti_debug_printf_list(void)
{
  int i;
  int n;
  for (i=0; i < printfRecords.count; i++) {
    printf("[%d] %s %s:%d %s",
	   i,
	   *printfRecords.records[i].enabled ? " *":"",
	   printfRecords.records[i].file,
	   printfRecords.records[i].line,
	   printfRecords.records[i].fmt);
  
    n = strlen(printfRecords.records[i].fmt);
    if (n && printfRecords.records[i].fmt[n-1] != '\n') {
      putchar('\n');
    }
  }
}

void cti_debug_printf_toggle(int index)
{
  if (index < printfRecords.count) {
    int e = *printfRecords.records[index].enabled;
    if (!e) { 
      *printfRecords.records[index].enabled = 1;
    }
    else {
      *printfRecords.records[index].enabled = 0;
    }
  }
}


void cti_debug_printf(const char *fmt, ...)
{
  int rc;
  va_list ap;
  va_start(ap, fmt);
  rc = vprintf(fmt, ap);
  if (-1 == rc) {
    perror("vsprintf");
  }
  va_end(ap);
}

/* key=value command-line parameter support. */
typedef struct {
  const char *key;
  const char *value;
} CmdlineParam;

static ISet(CmdlineParam) cmdline_params;

void CTI_cmdline_add(const char *key, const char *value)
{
  CmdlineParam *p = Mem_malloc(sizeof(*p));
  p->key = strdup(key);
  p->value = strdup(value);
  ISet_add(cmdline_params, p);
  // printf("%s: found %s=%s, count=%d\n", __func__, key, value, cmdline_params.count);
}

const char *CTI_cmdline_get(const char *key)
{
  int i;
  /* NOTE: No locking, it is assumed that all parameters are set
     before instances start looking for them. */
  // printf("%s: looking for key %s\n", __func__, key);
  for (i=0; i < cmdline_params.count; i++) {
    if (streq(cmdline_params.items[i]->key, key)) {
      return cmdline_params.items[i]->value;
    }
  }
  return NULL;
}

