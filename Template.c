#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <stdint.h>		/* uint8_t */
#include <string.h>		/* strcmp */
#include <unistd.h>		/* sleep */
#include <time.h>		/* nanosleep */

#define streq(a, b)  (strcmp(a, b) == 0)

#include "Template.h"
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
}


void _PostMessage(void *message, Input *input)
{
  fprintf(stderr, "obsolete %s called!\n", __func__); exit(1);
}

int CheckMessage(Instance *pi, int wait)
{
  /* For instances that need to periodically check hardware, like V4L2
     and ALSA, pass wait=0.  For instances that only operate on
     inputs, pass wait=1, and they will block here. */
  int n = 0;

  fprintf(stderr, "obsolete %s called from %s!\n", __func__, pi->label); exit(1);

  return n;
}

void * PopMessage(Input *input)
{
  /* Lock queue, remove first message. */
  void *msg = 0L;

  fprintf(stderr, "obsolete %s called!\n", __func__);

  return msg;
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
  /* Return handler message in available.  If no message available, return NULL if
     wait_flag is 0, else wait until a message is available and return it. */

  Handler_message *hm = 0L;

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


Instance * Instantiate(const char *label)
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

      Instance_loop_thread(pi);
      return pi;
    }
  }

  fprintf(stderr, "No such template: %s\n", label);
  return 0L;
}

Config_buffer *Config_buffer_new(const char *label, const char *value)
{
  Config_buffer *cb = Mem_malloc(sizeof(*cb));
  cb->label = String_new(label);
  cb->value = String_new(value);
  return cb;
}

void Config_buffer_discard(Config_buffer **cb)
{
  String_free(&(*cb)->label);
  String_free(&(*cb)->value);
  Mem_free(*cb);
  *cb = 0L;
}


void GetConfigRange(Instance *pi, const char *label, Range **range)
{
#if 0
  int i;

  if (!pi->config_table) {
    fprintf(stderr, "%s has no config table!\n", pi->label);
    return;
  }

  for (i=0; i < pi->num_config_table_entries; i++) {
    if (streq(pi->config_table[i].label, label)) {
      pi->config_table[i].get_range(pi, range);
      break;
    }
  }

  if (i == pi->num_config_table_entries) {
    fprintf(stderr, "could not find config entry for '%s'\n", label);
  }
#endif
}


void Connect(Instance *from, const char *label, Instance *to)
{
  int i;
  int from_index = -1;
  int to_index = -1;
  
  for (i=0; i < from->num_outputs ; i++) {
    if (from->outputs[i].type_label && streq(from->outputs[i].type_label, label)) {
      from_index = i;
      break;
    }
  }

  if (from_index == -1) {
    fprintf(stderr, "Instance does not have an output labelled '%s'\n", label);
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
    exit(1);
    fprintf(stderr, "Instance does not have an input labelled '%s'\n", label);
    return;
  }

  from->outputs[from_index].destination = &to->inputs[to_index];

  fprintf(stderr, "connected: %s.%d [%s] %s.%d\n", from->label, from_index, label, to->label, to_index);
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
    String_free(&v->_x.string_value);
  }
  Mem_free(v);
}


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
