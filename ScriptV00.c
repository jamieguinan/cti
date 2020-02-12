#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "CTI.h"
#include "Cfg.h"
#include "localptr.h"

extern char *argv0;

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

enum { C_tl, C_tlv,
       C_new, C_config, C_connect, C_include, C_load,
       C_il, C_ilv,
       C_system, C_exit, C_mt3, C_mdump,
       C_g_synchronous, C_abort, C_p, C_ignoreeof,
       C_dpl, C_dpt, C_dpe, C_v, C_mpm, C_apm, C_md5, C_help
};
static struct {
  const char *command;
  const char *help;
  int num_args;                 /* negative value means ">=" of positive value */
#define ENTRY(c, _n, _help) [ C_ ## c ] = { .command = #c, .help = _help, .num_args = _n }
} command_table[] = {
  ENTRY(tl, 1, "- list registered template types")
  , ENTRY(tlv, 1, "- list registered template types, with input/output names")
  , ENTRY(new, 3, "type label - instantiate type with instance name label")
  , ENTRY(connect, 4, "instance label instance - connect 2 instances by common label"
          "\nconnect instance:label instance:label - connect 2 instances by different label names (must be same type)")
  , ENTRY(config, 4, "instance key value - set a value in an instance")
  , ENTRY(il, 1, "- list instances in global instance group")
  , ENTRY(ilv, 1, "- list instances in global instance group, with connected outputs")
  , ENTRY(include, 2, "file - read commands (as described here) from named file")
  , ENTRY(load, 2, "modulename - load shared library containing CTI registration for new type(s)")
  , ENTRY(system, -1, "cmd [args...] - run command synchronously")
  , ENTRY(exit, 1, "- exit program")
  , ENTRY(mt3, 1, "- toggle memory tracking")
  , ENTRY(mdump, 1, "- dump recorded memory allocations")
  , ENTRY(g_synchronous, 1, "- toggle synchronous operation")
  , ENTRY(abort, 1, "- abort program immediately")
  , ENTRY(p, 1, "- set global pause flag, wait for enter, then unset")
  , ENTRY(ignoreeof, -1, "- keep running after EOF on command file")
  , ENTRY(dpl, 1, "- list dynamic debug prints")
  , ENTRY(dpt, 2, "n - toggle dynamic debug print number n, wait for enter, then turn off")
  , ENTRY(dpe, 2, "n - toggle dynamic debug print number n")
  , ENTRY(v, 2, "v - set verbose level, alive until hit enter")
  , ENTRY(mpm, 2, "n - set max pending messages to n")
  , ENTRY(apm, 1, "- show all pending messages")
  , ENTRY(md5, 1, "- show md5 checksum of executable")
  , ENTRY(help, 1, "- show this help text")
};

static int check_command(char * token, int index, int num_args) {
  if (streq(token, command_table[index].command)) {
    if (command_table[index].num_args < 0 && (-1 * num_args) <= command_table[index].num_args) {
      return 1;
    }
    else if (command_table[index].num_args == num_args) {
      return 1;
    }
    else {
      printf("%s %s\n", command_table[index].command, command_table[index].help);
      /* fall through and return 0 */
    }
  }
  return 0;
}


static void scan_file(ScriptV00_private *priv, const char *filename);

static void scan_line(ScriptV00_private *priv, String *line, int is_stdin)
{
  char token0[256] = {}, token1[256] = {}, token2[256], token3[256] = {};
  int n = 0;

  /* Commands with special formatting tested first. */
  if ((sscanf(line->bytes, "connect %255[A-Za-z0-9._]:%255[A-Za-z0-9._] %255[A-Za-z0-9._]:%255[A-Za-z0-9._]", token0, token1, token2, token3) == 4)) {
    /* instance:label, instance:label */
    InstanceGroup_connect2(priv->g, String_new(token0), token1, String_new(token2), token3);
    return;
  }

  /* The rest are tested based on a single sscanf. */
  n = sscanf(line->bytes, "%255s %255s %255s %255s", token0, token1, token2, token3);

  if (check_command(token0, C_new, n)) {
    /* template, label */
    InstanceGroup_add(priv->g, token1, String_new(token2));
    return;
  }

  if (check_command(token0, C_connect, n)) {
    /* instance, label, instance */
    InstanceGroup_connect(priv->g, String_new(token1), token2, String_new(token3));
    return;
  }

  if (check_command(token0, C_config, n)) {
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

  if (check_command(token0, C_include, n)) {
    scan_file(priv, token1);
    return;
  }

  if (check_command(token0, C_load, n)) {
    load_module(priv, token1);
    return;
  }

  if (check_command(token0, C_system, n)) {
    int rc = system(line->bytes + strlen("system "));
    if (rc == -1) {
      perror("system");
    }
    return;
  }

  if (check_command(token0, C_exit, n)) {
    exit(1);
    return;
  }

  if (check_command(token0, C_mt3, n)) {
    int v = mt3_toggle();
    fprintf(stderr, "mem_tracking_3 %s\n", (v ? "on":"off"));
    return;
  }

  if (check_command(token0, C_mdump, n)) {
    mdump();
    return;
  }

  if (check_command(token0, C_tlv, n)) {
    Template_list(1);
    return;
  }

  if (check_command(token0, C_tl, n)) {
    Template_list(0);
    return;
  }

  if (check_command(token0, C_il, n)) {
    Instance_list(0);
    return;
  }

  if (check_command(token0, C_ilv, n)) {
    Instance_list(1);
    return;
  }

  if (check_command(token0, C_g_synchronous, n)) {
    g_synchronous = !g_synchronous;
    return;
  }

  if (check_command(token0, C_abort, n)) {
    puts("");                   /* Wrap line on console. */
    abort();
    return;
  }

  if (check_command(token0, C_p, n)) {
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

  if (check_command(token0, C_ignoreeof, n)) {
    if (n == 1) {
      priv->exit_on_eof = 0;
    }
    else {
      priv->exit_on_eof = !(atoi(token1));
    }
    if (!priv->exit_on_eof) {
      printf("exit_on_eof disabled!\n");
    }
    else {
      printf("exit_on_eof enabled\n");
    }
    return;
  }

  if (check_command(token0, C_dpl, n)) {
    cti_debug_printf_list();
    return;
  }

  if (check_command(token0, C_v, n)) {
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

  if (check_command(token0, C_dpt, n)) {
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

  if (check_command(token0, C_dpe, n)) {
    /* Enable until dpe called again, without blocking command line. */
    int index = atoi(token1);
    cti_debug_printf_toggle(index);
    return;
  }

  if (check_command(token0, C_mpm, n)) {
    cfg.max_pending_messages = atoi(token1);
    printf("max_pending_messages set to %d\n", cfg.max_pending_messages);
    return;
  }

  if (check_command(token0, C_apm, n)) {
    /* Show all pending messages. */
    CTI_pending_messages();
    return;
  }

  if (check_command(token0, C_md5, n)) {
    /* Show md5sum of executable. */
    localptr(String, cmd) = String_sprintf("md5sum %s", argv0);
    system(s(cmd));
    return;
  }

  if (check_command(token0, C_help, n)) {
    int i;
    for (i=0; i < cti_table_size(command_table); i++) {
      printf("%s %s", command_table[i].command, command_table[i].help);
      printf("\n");
    }
    return;
  }

}


static void scan_lines(ScriptV00_private *priv, FILE *f, const char *prompt)
{
  while (1) {
    char line[256] = {};
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

  priv->g = gig;                /* Use global instance group. */
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
