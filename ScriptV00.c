#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "CTI.h"
#include "Cfg.h"

static void Config_handler(Instance *pi, void *data);
static void Line_handler(Instance *pi, void *data);
static void RawData_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_LINE, INPUT_RAWDATA };
static Input ScriptV00_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler},
  [ INPUT_LINE ] = { .type_label = "Line_msg", .handler = Line_handler},
  [ INPUT_RAWDATA ] = { .type_label = "RawData_buffer", .handler = RawData_handler },
};

// enum {  };
static Output ScriptV00_outputs[] = {
};

typedef struct {
  Instance i;
  InstanceGroup *g;
  int is_stdin;
  int exit_on_eof;
  /* Rawdata nodes used for socket input. */
  RawData_node *raw_first;
  RawData_node *raw_last;
  int total_buffered_data;
  int raw_seq;
}  ScriptV00_private;

static void expand(char token[256])
{
  if (token[0] == '$') {
    char * e = getenv(token+1);
    if (!e) {
      fprintf(stderr, "$%s is not defined, please set and try again.\n", token+1);
      exit(1);
    }
    strcpy(token, e);
  }
  else if (token[0] == '%') {
    const char *value = CTI_cmdline_get(token+1);
    if (value) {
      strcpy(token, value);
    }
  }
}


#include <dlfcn.h>
static void load_module(ScriptV00_private *priv, const char *filename)
{
  /* File should contain a symbol made from the basename of the file plus "_init()" */
  void * pmod = dlopen(filename, RTLD_NOW);
  if (!pmod) {
    fprintf(stderr, "failed to load %s, error %s\n", filename, dlerror());
    return;
  }

  String * fname2 = String_new(filename);
  String * init_sym_name = String_basename(fname2);
  int dot;
  int i = String_find(init_sym_name, 0, ".so", &dot);
  if (i > 0) {
    String_shorten(init_sym_name, i);
    String_cat1(init_sym_name, "_init");
  }

  void (*init_sym)(void) = dlsym(pmod, s(init_sym_name));
  if (init_sym) {
    init_sym();
  }
  else {
    fprintf(stderr, "could not find symbol %s\n", s(init_sym_name));
  }
}

void ScriptV00_Config(Instance *inst, const char *token2, const char *token3)
{
  {
    if (inst) {
      Config_buffer *c = Config_buffer_new(token2, token3);
      printf("posting config message %s %s to %s\n", token2, token3, inst->label);
      // PostData(c, &inst->inputs[0]);
      int result;
      PostDataGetResult(c, &inst->inputs[0], &result);
    }
  }
}


static void scan_file(ScriptV00_private *priv, const char *filename);

static void scan_line(ScriptV00_private *priv, String *line, int is_stdin)
{
  char token0[256], token1[256], token2[256], token3[256];
  int n = 0;

  /* Commands with special formatting tested first. */
  if ((sscanf(line->bytes, "connect %255[A-Za-z0-9._]:%255[A-Za-z0-9._] %255[A-Za-z0-9._]:%255[A-Za-z0-9._]", token0, token1, token2, token3) == 4)) {
    /* instance:label, instance:label */
    InstanceGroup_connect2(priv->g, String_new(token0), token1, String_new(token2), token3);
    return;
  }

  /* The rest are tested based on a single sscanf. */
  n = sscanf(line->bytes, "%255s %255s %255s %255s", token0, token1, token2, token3);

  if (n == 3 && streq(token0, "new")) {
    /* template, label */
    InstanceGroup_add(priv->g, token1, String_new(token2));
    return;
  }

  if (n == 4 && streq(token0, "connect")) {
    /* instance, label, instance */
    InstanceGroup_connect(priv->g, String_new(token1), token2, String_new(token3));
    return;
  }

  if (n == 4 && streq(token0, "config")) {
    /* label, key, value */
    Instance *inst = InstanceGroup_find(priv->g, String_new(token1));
    if (inst) {
      char *p = line->bytes;
      p = strchr(p, ' ')+1;
      p = strchr(p, ' ')+1;
      p = strchr(p, ' ')+1;
      strcpy(token3, p);
      expand(token3);

#if 0
      Config_buffer *c = Config_buffer_new(token2, token3);
      printf("posting config message %s %s to %s\n", token2, token3, inst->label);
      // PostData(c, &inst->inputs[0]);
      int result;
      PostDataGetResult(c, &inst->inputs[0], &result);
#else
      ScriptV00_Config(inst, token2, token3);
#endif
    }
    return;
  }

  if (n == 2 && streq(token0, "include")) {
    scan_file(priv, token1);
    return;
  }

  if (n == 2 && streq(token0, "load")) {
    load_module(priv, token1);
    return;
  }

  if (streq(token0, "system")) {
    int rc = system(line->bytes + strlen("system "));
    if (rc == -1) {
      perror("system");
    }
    return;
  }

  if (n == 1 && streq(token0, "exit")) {
    exit(1);
    return;
  }

  if (n == 1 && streq(token0, "mt")) {
    cfg.mem_tracking = !cfg.mem_tracking;
    return;
  }

  if (n == 1 && streq(token0, "mt3")) {
    cfg.mem_tracking_3 = !cfg.mem_tracking_3;
    fprintf(stderr, "cfg.mem_tracking_3 %s\n", (cfg.mem_tracking_3 ? "on":"off"));
    return;
  }

  if (n == 1 && streq(token0, "mdump")) {
    mdump();
    return;
  }

  if (n == 1 && streq(token0, "tlv")) {
    Template_list(1);
    return;
  }

  if (n == 1 && streq(token0, "tl")) {
    Template_list(0);
    return;
  }

  if (n == 1 && streq(token0, "il")) {
    Instance_list(0);
    return;
  }

  if (n == 1 && streq(token0, "ilv")) {
    Instance_list(1);
    return;
  }

  if (n == 1 && streq(token0, "g_synchronous")) {
    g_synchronous = !g_synchronous;
    return;
  }

  if (n == 1 && streq(token0, "abort")) {
    puts("");			/* Wrap line on console. */
    abort();
    return;
  }

  if (n == 1 && streq(token0, "p")) {
    if (is_stdin) {
      cfg.pause = 1;
      /* Wait until newline then reset. */
      if (fgets(token1, 255, stdin) == NULL) {
	fprintf(stderr, "note: got EOF after pause.\n");
      }
      cfg.pause = 0;
    }
    return;
  }

  if (n == 1 && streq(token0, "ignoreeof")) {
    priv->exit_on_eof = 0;
    printf("exit_on_eof disabled!\n");
    return;
  }

  if (n == 1 && streq(token0, "dpl")) {
    cti_debug_printf_list();
    return;
  }

  if (n == 2 && streq(token0, "v")) {
    cfg.verbosity = atoi(token1);
    if (is_stdin) {
      /* Wait until newline then reset. */
      if (fgets(token1, 255, stdin) == NULL) {
	fprintf(stderr, "note: got EOF after verbose toggle.\n");
      }
      cfg.verbosity = 0;
    }
    return;
  }

  if (n == 2 && streq(token0, "dpt")) {
    int index = atoi(token1);
    cti_debug_printf_toggle(index);
    if (is_stdin) {
      /* Wait until newline then reset. */
      if (fgets(token1, 255, stdin) == NULL) {
	fprintf(stderr, "note: got EOF after dpt.\n");
      }
    }
    cti_debug_printf_toggle(index);
    return;
  }

  if (n == 2 && streq(token0, "dpe")) {
    int index = atoi(token1);
    cti_debug_printf_toggle(index);
    return;
  }
  
  if (n == 2 && streq(token0, "mpm")) {
    cfg.max_pending_messages = atoi(token1);
    printf("max_pending_messages set to %d\n", cfg.max_pending_messages);
    return;
  }

}


static void scan_lines(ScriptV00_private *priv, FILE *f, const char *prompt)
{
  while (1) {
    char line[256];
    char *s;
    String *st;

    if (prompt) {
      fprintf(stdout, "%s", prompt); fflush(stdout);
    }

    s = fgets(line, sizeof(line), f);
    if (!s) {
      break;
    }

    st = String_new(line);
    String_trim_right(st);

    scan_line(priv, st, priv->is_stdin);
  }
}


static void scan_file(ScriptV00_private *priv, const char *filename)
{
  FILE *f;
  f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "(count not open %s)\n", filename);
    return;
  }

  scan_lines(priv, f, NULL);
  fclose(f);
}


static int set_input(Instance *pi, const char *value)
{
  /* Note that this does NOT return until the input reaches EOF. */
  FILE *f;
  const char *prompt = 0L;
  ScriptV00_private *priv = (ScriptV00_private *)pi;

  if (strlen(value) != 0) {
    char *e;
    priv->is_stdin = 0;
    scan_file(priv, value);
    e = getenv("SCRIPTV00_EXTRA");
    if (e) {
      printf("*** scanning %s\n", e);
      scan_file(priv, e);
    }
  }

  /* Now switch to stdin. */
  f = stdin;
  priv->is_stdin = 1;
  prompt = "cti> ";

  scan_lines(priv, f, prompt);

  if (prompt) {
    fprintf(stdout, "\n");
  }

  if (priv->exit_on_eof) {
    printf("exiting!\n");
    exit(0);
  }
  else {
    printf("got eof, but hanging around anyway...\n");
  }

  return 0;
}

static Config config_table[] = {
  { "input",       set_input, 0L, 0L },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Line_handler(Instance *pi, void *data)
{
  Line_msg *lm = data;
  Line_msg_release(&lm);
}


static void RawData_handler(Instance *pi, void *data)
{
  ScriptV00_private *priv = (ScriptV00_private *)pi;
  RawData_buffer *raw = data;
  RawData_node *rn = Mem_calloc(1, sizeof(*rn));
  rn->buffer = raw;

  if (!priv->raw_first) {
    priv->raw_first = priv->raw_last = rn;
  }
  else {
    priv->raw_last->next = rn;
    priv->raw_last = rn;
  }

  priv->total_buffered_data += priv->raw_last->buffer->data_length;
  priv->raw_seq += 1;
  priv->raw_last->seq = priv->raw_seq;
}

static void ScriptV00_tick(Instance *pi)
{
  Handler_message *hm;

  /* Check for and handle any messages. */
  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}

static void ScriptV00_instance_init(Instance *pi)
{
  ScriptV00_private *priv = (ScriptV00_private *)pi;

  priv->g = gig;		/* Use global instance group. */
  priv->exit_on_eof = 1;
}


/* Not sure if actually need/want this... */
static Template ScriptV00_template = {
  .label = "ScriptV00",
  .priv_size = sizeof(ScriptV00_private),
  .inputs = ScriptV00_inputs,
  .num_inputs = table_size(ScriptV00_inputs),
  .outputs = ScriptV00_outputs,
  .num_outputs = table_size(ScriptV00_outputs),
  .tick = ScriptV00_tick,
  .instance_init = ScriptV00_instance_init,
};

void ScriptV00_init(void)
{
  Template_register(&ScriptV00_template);
}
