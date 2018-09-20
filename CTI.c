#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <stdint.h>		/* uint8_t */
#include <string.h>		/* strcmp */
#include <unistd.h>		/* sleep */
#include <time.h>		/* nanosleep */
#include <sys/time.h>		/* gettimeofday */
#include <stdarg.h>		/* vprintf */
#ifdef __linux__
#include <sys/prctl.h>	/* prctl */
#endif

#ifndef streq
#define streq(a, b)  (strcmp(a, b) == 0)
#endif

#include "CTI.h"
#include "Mem.h"
#include "Cfg.h"

int (*ui_main)(int argc, char *argv[]) = NULL;

pthread_key_t instance_key;
int instance_key_initialized;
int g_synchronous = 0;		/* Can be toggled in ScriptV00.c */

/* instance_key_init() should be called only once, by the main thread
   in a CTI application, before any instance threads are
   created. Following these rules obviates the need for
   pthread_once(). */
void instance_key_init(void)
{
  pthread_key_create(&instance_key, NULL); /* No destructor function. */
  pthread_setspecific(instance_key, NULL); /* Pass NULL for main thread. */
  instance_key_initialized = 1;
}


void CTI_register_instance(Instance *pi)
{
#ifdef __linux__
  prctl(PR_SET_NAME, pi->label);
#endif
  pthread_setspecific(instance_key, (void*)pi);	/* For later retrieval */
}


static void * Instance_thread_main(void *vp)
{
  Instance *pi = vp;

  CTI_register_instance(pi);

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


void PostDataGetResult(void *data, Input *input, int * result)
{
  Handler_message *hm = Mem_calloc(1, sizeof(*hm));

  if (!input->handler) {
    /* Error, send calling thread to tar pit... */
    while (1) {
      fprintf(stderr, "instance %s input %s does not have a handler!\n",
	      input->parent->label, input->type_label);
      sleep(1);
    }
  }

  Lock reply_lock;
  Sem reply_sem;

  hm->handler = input->handler;
  hm->data = data;

  if (result) {
    Sem_init(&reply_sem);
    Lock_init(&reply_lock);
    Lock_acquire(&reply_lock);
    hm->reply_sem = &reply_sem;
    hm->result = result;
  }

  {
    /* Wait for input lock, lock... */
    Lock_acquire(&input->parent->inputs_lock);

    {
      /* Add node to list. */
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
    }

    if (input->parent->waiting) {
      Event_signal(&input->parent->inputs_event);
    }

    input->parent->pending_messages += 1;

    if (input->parent->pending_messages > 5) {
      dpf("%s (%p): %d queued messages\n",
	  input->parent->label,
	  input,
	  input->parent->pending_messages);
    }

    Lock_release(&input->parent->inputs_lock);
  }

  while (input->parent->pending_messages > cfg.max_pending_messages) {
    /* This one indicates a configuration problem or misestimation, so
       make it an fprintf instead of a dpf. */
    fprintf(stderr, "sleeping to drain %s message queue (%p) (%d)\n",
	    input->parent->label,
	    input,
	    input->parent->pending_messages);
    sleep(1);
  }

  if (result) {
    /* Wait for message recipient to signal back. */
    Sem_wait(&reply_sem);
    Sem_destroy(&reply_sem);
    Lock_destroy(&reply_lock);
  }
}


void PostData(void *data, Input *input)
{
  if (!g_synchronous) {
    PostDataGetResult(data, input, NULL);
  }
  else {
    int throwaway;
    PostDataGetResult(data, input, &throwaway);
  }
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

  /*
     I thought about doing the if (...) check before the
     Lock_acquire(), and changing the goto to a return, thus avoiding
     the acquire/release cycle when there are no messages pending and
     wait flag is zero.  But I am unsure about ARM memory behavior, in
     particular I worry that memory might not be synchronized properly
     without the memory barrier events imposed by the locking, which
     could make a thread return when there IS actually data available.
     So I am leaving the lock acquire/release.  Links,

     http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0472c/CJAEGDEA.html
     http://iphonedevsdk.com/forum/iphone-sdk-development/115122-multi-core-arm-and-sharing-global-variable-between-threads.html

  */

  Lock_acquire(&pi->inputs_lock);

  if (pi->msg_first == 0L && pi->msg_last == 0L && wait_flag == 0)  {
    goto out;
  }

  while (pi->msg_first == 0L && pi->msg_last == 0L) {
    pi->waiting = 1;
    Lock_release__event_wait__lock_acquire(&pi->inputs_lock, &pi->inputs_event);
  }

  pi->waiting = 0;

  {
    /* Take node, remove from list. */
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
  }

  pi->pending_messages -= 1;

out:
  Lock_release(&pi->inputs_lock);

  return hm;
}


void ReleaseMessage(Handler_message **msg, Instance *pi)
{
  Handler_message *hm = *msg;
  if (hm->reply_sem) {
    /* Sender is waiting for a reply. */
    if (hm->result) {
      /* Sender supplied a result destination. */
      /* FIXME: Could also provide a string result, or even a binary (ArrayU8) result. */
      *(hm->result) = pi->result;
    }
    Sem_post(hm->reply_sem);
  }
  Mem_free(*msg);
  *msg = 0L;
}


static IndexedSet(Template) templates = {};

void Template_register(Template *t)
{
  IndexedSet_add(templates, t);
}

void Template_list(int verbose)
{
  int i, j;
  printf("Templates:\n");

  for (i=0; i < templates.count; i++) {
    printf("[%d] %s\n", i, templates.items[i]->label);
    if (!verbose) {
      continue;
    }
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


static void Instance_print(Index_node *node)
{
  Instance * pi = node->value;
  printf("  %s (%s)\n", s(node->stringKey), pi->label);
}

static void Instance_print_verbose(Index_node *node)
{
  int i;
  Instance * pi = node->value;
  printf("  %s (%s)\n", s(node->stringKey), pi->label);
  for (i=0; i < pi->num_outputs; i++) {
    if (pi->outputs[i].destination) {
      printf("    %s.%s -> %s.%s\n"
	     , s(node->stringKey)
	     , pi->outputs[i].type_label
	     , s(pi->outputs[i].destination->parent->instance_label)
	     , pi->outputs[i].destination->type_label
	     );
    }
  }
}

void Instance_list(int verbose)
{
  /* List instances in global instance group. */
  if (gig->instances.index) {
    if (verbose) {
      Index_walk(gig->instances.index, Instance_print_verbose);
    }
    else {
      Index_walk(gig->instances.index, Instance_print);
    }
  }
}

static void Instance_pending_messages(Index_node *node)
{
  Instance * pi = node->value;
  printf("%s (%s) has %d pending messsages\n",
         s(node->stringKey), pi->label, pi->pending_messages);
}

void CTI_pending_messages(void)
{
  if (gig->instances.index) {
    Index_walk(gig->instances.index, Instance_pending_messages);
  }
}


static Instance * _Instantiate_local(const char *label, String * instanceLabel, int run)
{
  int i;

  for (i=0; i < templates.count; i++) {
    if (strcmp(templates.items[i]->label, label) == 0) {
      Template *t = templates.items[i];
      Instance *pi;
      if (t->priv_size) {
	/* New-style, where priv structure includes Instance member. */
	pi = Mem_calloc(1, t->priv_size);
      }
      else {
	pi = Mem_calloc(1, sizeof(*pi));
      }

      pi->label = t->label;
      pi->instance_label = String_dup(instanceLabel);
      pi->tick = t->tick;
      pi->priv_size = t->priv_size;

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

      /* Type-specific instance initialization. */
      if (t->instance_init) {
	t->instance_init(pi);
      }

      /* Run or start thread, only if tick function is defined. */
      if (pi->tick) {
	if (run) {
	  /* Run instance main loop, blocking the caller. */
	  Instance_thread_main(pi);
	}
	else {
	  /* Start a new thread. */
	  Instance_loop_thread(pi);
	}
      }
      return pi;
    }
  }

  fprintf(stderr, "No such template: %s\n", label);
  return 0L;
}


void Instantiate_and_run(const char *label, String * instanceLabel)
{
  _Instantiate_local(label, instanceLabel, 1);
}


Instance * Instantiate(const char *label, String * instanceLabel)
{
  return _Instantiate_local(label, instanceLabel, 0);
}


Line_msg *Line_msg_new(const char *value)
{
  Line_msg *lm = Mem_calloc(1, sizeof(*lm));
  lm->value = String_new(value);
  return lm;
}


void Line_msg_release(Line_msg **plm)
{
  Line_msg *lm = *plm;
  String_free(&lm->value);
  Mem_free(lm);
  *plm = 0L;
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


void Config_buffer_release(Config_buffer **cb)
{
  if ((*cb)->label) {
    String_free(&(*cb)->label);
  }
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
    if (streq(config_table[i].label, s(cb_in->label))) {

      /* If value is passed in, call the set function. */
      if (cb_in->value && config_table[i].vset) {
	/* Generic setter. */
	config_table[i].vset((uint8_t*)pi + config_table[i].value_offset,
			     s(cb_in->value));
      }
      else if (cb_in->value && config_table[i].set) {
	/* Template-specific setter. */
	// int rc = ...		/* FIXME: What to do with this? */
	config_table[i].set(pi, s(cb_in->value));
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

  Config_buffer_release(&cb_in);
}


void cti_set_int(void *addr, const char *value)
{
  *( (int *)addr) = atoi(value);
}


void cti_set_uint(void *addr, const char *value)
{
  *( (unsigned int *)addr) = (unsigned int) strtoul(value, NULL, 0);
}


void cti_set_long(void *addr, const char *value)
{
  *( (long *)addr) = atol(value);
}


void cti_set_ulong(void *addr, const char *value)
{
  *( (int *)addr) = strtoull(value, NULL, 0);
}


void cti_set_string_local(void *addr, const char *value)
{
  String_set_local((String *)addr, value);
}

void cti_set_string(void *addr, const char *value)
{
  String_set((String **)addr, value);
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

  pi = Instantiate(typeLabel, instanceLabel);

  if (!pi) {
    return;
  }

  /* Add pi to g's set of instances. */
  /* Pass "instanceLabel" as "key" parameter, for later lookup. */
  int err;
  IndexedSet_add_keyed(g->instances, instanceLabel, pi, &err);
}


Instance *InstanceGroup_find(InstanceGroup *g, String *ilabel)
{
  if (g->instances.index) {
    /* Look up using hash index. */
    int found = 0;
    Instance *pi = Index_find_string(g->instances.index, ilabel, &found);
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
  if (!pi1) {  fprintf(stderr, "could not find instance '%s'\n", s(instanceLabel1)); return; }
  Instance *pi2 = InstanceGroup_find(g, instanceLabel2);
  if (!pi2) {  fprintf(stderr, "could not find instance '%s'\n", s(instanceLabel2)); return; }
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
  if (!pi1) {  fprintf(stderr, "could not find instance '%s'\n", s(instanceLabel1)); return; }
  Instance *pi2 = InstanceGroup_find(g, instanceLabel2);
  if (!pi2) {  fprintf(stderr, "could not find instance '%s'\n", s(instanceLabel2)); return; }
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
  printf("%s(%p)\n", __func__, func);
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

void RawData_buffer_release(RawData_buffer *raw)
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

void Feedback_buffer_release(Feedback_buffer *fb)
{
  Mem_free(fb);
}


/* key=value command-line parameter support. */
typedef struct {
  const char *key;
  const char *value;
} CmdlineParam;

static IndexedSet(CmdlineParam) cmdline_params;

void CTI_cmdline_add(const char *key, const char *value)
{
  CmdlineParam *p = Mem_malloc(sizeof(*p));
  p->key = strdup(key);
  p->value = strdup(value);
  IndexedSet_add(cmdline_params, p);
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

