#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CTI.h"
#include "Cfg.h"

static void Config_handler(Instance *pi, void *data);

enum { INPUT_CONFIG };
static Input ScriptV00_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler},
};

// enum {  };
static Output ScriptV00_outputs[] = {
};

typedef struct {
  InstanceGroup *g;
  int is_stdin;
  int exit_on_eof;
}  ScriptV00_private;

static void expand(char token[256])
{
  if (token[0] == '$') {
    strcpy(token, getenv(token+1));
  }
}

static void scan_file(ScriptV00_private *priv, const char *filename);

static void scan_line(ScriptV00_private *priv, String *line, int is_stdin)
{
  char token1[256], token2[256], token3[256], token4[256];

  if ((sscanf(line->bytes, "new %255s %255s", token1, token2) == 2)) {
    /* template, label */
    InstanceGroup_add(priv->g, token1, String_new(token2));
  }
  else if ((sscanf(line->bytes, "connect %255[A-Za-z0-9._]:%255[A-Za-z0-9._] %255[A-Za-z0-9._]:%255[A-Za-z0-9._]", token1, token2, token3, token4) == 4)) {
    /* instance:label, instance:label */
    InstanceGroup_connect2(priv->g, String_new(token1), token2, String_new(token3), token4);
  }
  else if ((sscanf(line->bytes, "connect %255s %255s %255s", token1, token2, token3) == 3)) {
    /* instance, label, instance */
    InstanceGroup_connect(priv->g, String_new(token1), token2, String_new(token3));
  }
  else if ((sscanf(line->bytes, "config %255s %255s %255s", token1, token2, token3) == 3)) {
    /* label, key, value */
    Instance *inst = InstanceGroup_find(priv->g, String_new(token1));
    if (inst) {
      char *p = line->bytes;
      p = strchr(p, ' ')+1;
      p = strchr(p, ' ')+1;
      p = strchr(p, ' ')+1;
      strcpy(token3, p);
      expand(token3);
      Config_buffer *c = Config_buffer_new(token2, token3);
      printf("posting config message %s %s to %s\n", token2, token3, inst->label);
      PostData(c, &inst->inputs[0]);
    }
  }
  else if ((sscanf(line->bytes, "include %255s", token1) == 1)) {
    scan_file(priv, token1);
  }
  else if ((strstr(line->bytes, "system ") == line->bytes)) {
    int rc = system(line->bytes + strlen("system "));
    if (rc == -1) {
      perror("system");
    }
  }
  else if ((strstr(line->bytes, "exit") == line->bytes)) {
    exit(1);
  }
  else if ((strstr(line->bytes, "mt") == line->bytes)) {
    cfg.mem_tracking = !cfg.mem_tracking;
  }
  else if ((strstr(line->bytes, "tl") == line->bytes)) {
    Template_list();
  }
  else if ((strstr(line->bytes, "abort") == line->bytes)) {
    abort();
  }
  else if ((strstr(line->bytes, "ignoreeof") == line->bytes)) {
    priv->exit_on_eof = 0;
    printf("exit_on_eof disabled!\n");
  }
  else if ((sscanf(line->bytes, "%255s %255s", token1, token2) == 2)) {
    if (streq(token1, "v")) {
      cfg.verbosity  = atoi(token2);
      if (is_stdin) {
	/* Wait until newline then reset. */
	if (fgets(token1, 255, stdin) == NULL) {
	  /* Handle EOF. */
	}
	cfg.verbosity = 0;
      }
    }
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
  ScriptV00_private *priv = pi->data;

  if (strlen(value) != 0) {
    priv->is_stdin = 0;
    scan_file(priv, value);
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


static void ScriptV00_tick(Instance *pi)
{
  Handler_message *hm;

  /* Check for and handle any messages. */
  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }
}

static void ScriptV00_instance_init(Instance *pi)
{
  ScriptV00_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
  priv->g = InstanceGroup_new();
  priv->exit_on_eof = 1;
}


/* Not sure if actually need/want this... */
static Template ScriptV00_template = {
  .label = "ScriptV00",
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
