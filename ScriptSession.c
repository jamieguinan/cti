#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"

static Input ScriptSession_inputs[] = {
};

static Output ScriptSession_outputs[] = {
};

typedef struct {
  InstanceGroup *ig;
  int is_stdin;
} ScriptSession_private;

static void assign(ScriptSession_private *ss)
{
}

static void show(ScriptSession_private *ss)
{
}

static void set(ScriptSession_private *ss)
{
}

static void connect(ScriptSession_private *ss)
{
}

static void wait(ScriptSession_private *ss)
{
}

enum TOKENS { NAME, EQ, EOL, SHOW, SET, CONNECT, DQUOTE, COMMA, WAIT };

/* <name> = <InstanceLabel> EOL */
/* show <name> EOL */
/* set <name> <comma> <dquote>ConfigItem<dquote> <commna> <dquote>Value<dquote> EOL */
/* connect <name> <comma> <dquote>InputOutputLabel<dquote> <comma> <name>  EOL */
/* wait  EOL */

#define MAX_TOKENS 11
static struct {
  void (*func)(ScriptSession_private *);
  int tokens[MAX_TOKENS];
} Language[] = { 
  { assign, { NAME, EQ, NAME, EOL } } ,
  { show, { SHOW, NAME, EOL } } ,
  { set, { SET, NAME, COMMA, DQUOTE, NAME, DQUOTE, COMMA, DQUOTE, NAME, DQUOTE, EOL } },
  { connect, { CONNECT, NAME, COMMA, DQUOTE, NAME, DQUOTE, COMMA, NAME, EOL } } ,
  { wait, { WAIT, EOL } },
};

/* Either use the above or make a "tree" using [] indexing to jump around in the tree. */
typedef struct {
  /* ??? */
  void (*func)(void);
} ParseNode;

static ParseNode LanguageTree[] = {
  [0] = {},
  [5] = {},
  [100] = {},
};



static void parse_cmd(ScriptSession_private *ss, const char *cmd)
{
  char *localcmd = strdup(cmd);
  struct {
    int t;
    const char *string;    
  } tokens[MAX_TOKENS];

  free(localcmd);
}

static int set_input(Instance *pi, const char *value)
{
  FILE *f;
  const char *prompt = 0L;
  ScriptSession_private *priv = pi->data;

  if (strlen(value) == 0) {
    /* If "", use stdin. */
    f = stdin;
    priv->is_stdin = 1;
    prompt = "cti> ";
  }
  else {
    f = fopen(value, "r");
    if (!f) {
      return -1;
    }
  }

  while (1) {
    char line[256];
    char *s;

    if (prompt) {
      fprintf(stdout, "%s", prompt); fflush(stdout);
    }

    s = fgets(line, sizeof(line), f);
    if (!s) {
      break;
    }
    parse_cmd(priv, line);
  }

  if (prompt) {
    fprintf(stdout, "\n");
  }

  return 0;
}

static Config config_table[] = {
  { "input",    set_input, 0L, 0L },
};


static void ScriptSession_tick(Instance *pi)
{
  // ScriptSession_private *priv = pi->data;

  /* Nothing to do here? */
}


static void ScriptSession_loop(Instance *pi)
{
  while (1) {
    Instance_wait(pi);
    ScriptSession_tick(pi);
  }
}


static Instance *ScriptSession_new(Template *t)
{
  Instance *pi = Mem_calloc(1, sizeof(*pi));
  int i;

  pi->label = t->label;

  pi->config_table = config_table;
  pi->num_config_table_entries = table_size(config_table);

  copy_list(t->inputs, t->num_inputs, &pi->inputs, &pi->num_inputs);
  copy_list(t->outputs, t->num_outputs, &pi->outputs, &pi->num_outputs);

  for (i=0; i < t->num_inputs; i++) {
    pi->inputs[i].parent = pi;
  }

  ScriptSession_private *priv = Mem_calloc(1, sizeof(*priv));
  // priv->... = ...;

  pi->data = priv;
  pi->tick = ScriptSession_tick;
  pi->loop = ScriptSession_loop;

  return pi;
}

static Template ScriptSession_template; /* Set up in _init() function. */

void ScriptSession_init(void)
{
  int i;

  /* Since can't set up some things statically, do it here. */
  ScriptSession_template.label = "ScriptSession";

  ScriptSession_template.inputs = ScriptSession_inputs;
  ScriptSession_template.num_inputs = table_size(ScriptSession_inputs);
  for (i=0; i < ScriptSession_template.num_inputs; i++) {
  }

  ScriptSession_template.outputs = ScriptSession_outputs;
  ScriptSession_template.num_outputs = table_size(ScriptSession_outputs);

  ScriptSession_template.new = ScriptSession_new;

  Template_register(&ScriptSession_template);
}
