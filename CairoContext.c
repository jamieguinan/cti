#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <time.h>		/* localtime */
#include <math.h>		/* M_2_PI */

#include "Template.h"
#include "CairoContext.h"
#include "Images.h"
#include "XArray.h"
#include "Cfg.h"

#include "cairo.h"

static void Config_handler(Instance *pi, void *msg);
static void rgb3_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_RGB3 };
static Input CairoContext_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = rgb3_handler },
};

enum { OUTPUT_RGB3 };
static Output CairoContext_outputs[] = {
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
};


#define CC_MAX_DOUBLE_ARGS 6
/* A cairo command with up to 6 double parameters. */
typedef struct {
  int command;
  double args[CC_MAX_DOUBLE_ARGS];
} CairoCommand;

enum {
  CC_COMMAND__ZERO_IS_INVALID,
  CC_COMMAND_SET_SOURCE_RGB,
  CC_COMMAND_SET_SOURCE_RGBA,
  CC_COMMAND_SET_LINE_WIDTH,
  CC_COMMAND_MOVE_TO,
  CC_COMMAND_LINE_TO,
  CC_COMMAND_REL_MOVE_TO,
  CC_COMMAND_REL_LINE_TO,
  CC_COMMAND_CLOSE_PATH,
  CC_COMMAND_TRANSLATE,
  CC_COMMAND_SCALE,
  CC_COMMAND_ROTATE,
  CC_COMMAND_FILL,
  CC_COMMAND_STROKE,
  CC_COMMAND_PUSH_GROUP,
  CC_COMMAND_POP_GROUP,
  CC_COMMAND_IDENTITY_MATRIX,

  CC_COMMAND_SHOW_TEXT,

  /* I made these up, they make use of the date stamp in the input RGB buffer. */
  CC_COMMAND_ROTATE_SUBSECONDS,
  CC_COMMAND_ROTATE_SECONDS,
  CC_COMMAND_ROTATE_MINUTES,
  CC_COMMAND_ROTATE_HOURS,
};

/* Map text strings to commands. */
static struct {
  const char *label;
  int command;
  int num_double_args;
} CairoCommandMap [] = {
  { .label = "set_source_rgb", .command = CC_COMMAND_SET_SOURCE_RGB, .num_double_args = 3 },
  { .label = "set_source_rgba", .command = CC_COMMAND_SET_SOURCE_RGB, .num_double_args = 4 },
  { .label = "set_line_width", .command = CC_COMMAND_SET_LINE_WIDTH, .num_double_args = 1 },

  { .label = "move_to", .command = CC_COMMAND_MOVE_TO, .num_double_args = 2 },
  { .label = "line_to", .command = CC_COMMAND_LINE_TO, .num_double_args = 2 },
  { .label = "rel_move_to", .command = CC_COMMAND_REL_MOVE_TO, .num_double_args = 2 },
  { .label = "rel_line_to", .command = CC_COMMAND_REL_LINE_TO, .num_double_args = 2 },
  { .label = "close_path", .command = CC_COMMAND_CLOSE_PATH, .num_double_args = 0 },

  { .label = "translate", .command = CC_COMMAND_TRANSLATE, .num_double_args = 2 },
  { .label = "scale", .command = CC_COMMAND_SCALE, .num_double_args = 2 },
  { .label = "rotate", .command = CC_COMMAND_ROTATE, .num_double_args = 1 },

  { .label = "fill", .command = CC_COMMAND_FILL, .num_double_args = 0 },
  { .label = "stroke", .command = CC_COMMAND_STROKE, .num_double_args = 0 },

  { .label = "push_group", .command = CC_COMMAND_PUSH_GROUP, .num_double_args = 0 },
  { .label = "pop_group", .command = CC_COMMAND_POP_GROUP, .num_double_args = 0 },
  { .label = "identity_matrix", .command = CC_COMMAND_IDENTITY_MATRIX, .num_double_args = 0 },

  { .label = "show_text", .command = CC_COMMAND_SHOW_TEXT, .num_double_args = 0 },

  { .label = "rotate_subseconds", .command = CC_COMMAND_ROTATE_SUBSECONDS, .num_double_args = 0 },
  { .label = "rotate_seconds", .command = CC_COMMAND_ROTATE_SECONDS, .num_double_args = 0 },
  { .label = "rotate_minutes", .command = CC_COMMAND_ROTATE_MINUTES, .num_double_args = 0 },
  { .label = "rotate_hours", .command = CC_COMMAND_ROTATE_HOURS, .num_double_args = 0 },
};

typedef struct {
  cairo_surface_t *surface;
  cairo_t *context;
  int width;
  int height;
  XArray(CairoCommand, commands);
  char *text;
} CairoContext_private;


static int set_width(Instance *pi, const char *value)
{
  CairoContext_private *priv = pi->data;
  priv->width = atoi(value);
  return 0;
}


static int set_height(Instance *pi, const char *value)
{
  CairoContext_private *priv = pi->data;
  priv->height = atoi(value);
  return 0;
}


static int set_text(Instance *pi, const char *value)
{
  CairoContext_private *priv = pi->data;
  if (priv->text) {
    free(priv->text);
  }
  priv->text = strdup(value);
  return 0;
}


static int add_command(Instance *pi, const char *value)
{
  CairoContext_private *priv = pi->data;
  CairoCommand cmd = {};
  char label[256] = {};
  int i;
  int j;
  int n;

  n = sscanf(value, "%255s %lf %lf %lf %lf %lf %lf",
	     label,
	     &cmd.args[0],
	     &cmd.args[1],
	     &cmd.args[2],
	     &cmd.args[3],
	     &cmd.args[4],
	     &cmd.args[5]);
	     
  for (i=0; i < table_size(CairoCommandMap); i++) {
    if (streq(label, CairoCommandMap[i].label)
	&& (n-1) == CairoCommandMap[i].num_double_args) {
      cmd.command = CairoCommandMap[i].command;
      printf("cairo_%s", label);
      for (j=0; j < CairoCommandMap[i].num_double_args; j++) {
	printf(" %lf", cmd.args[j]);
      }
      printf("\n");
      XArray_append(priv->commands, &cmd);
    }
  }
  
  return 0;
}

static void apply_commands(CairoContext_private *priv, RGB3_buffer * rgb3)
{
  int i;
  time_t t;
  struct tm lt;
  double rval = 0.0;

  priv->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, priv->width, priv->height);
  priv->context = cairo_create(priv->surface);

  for (i=0; i < XArray_count(priv->commands); i++) {
    CairoCommand *cmd;
    XArray_get_ptr(priv->commands, i, cmd);

    switch (cmd->command) {

    case CC_COMMAND_SET_SOURCE_RGB:
      cairo_set_source_rgb(priv->context, cmd->args[0], cmd->args[1], cmd->args[2]);
      break;

    case CC_COMMAND_SET_SOURCE_RGBA:
      cairo_set_source_rgba(priv->context, cmd->args[0], cmd->args[1], cmd->args[2], cmd->args[3]);
      break;

    case CC_COMMAND_SET_LINE_WIDTH:
      cairo_set_line_width(priv->context, cmd->args[0]);
      break;

    case CC_COMMAND_MOVE_TO:
      cairo_move_to(priv->context, cmd->args[0], cmd->args[1]);
      break;
    case CC_COMMAND_LINE_TO:
      cairo_line_to(priv->context, cmd->args[0], cmd->args[1]);
      break;
    case CC_COMMAND_REL_MOVE_TO:
      cairo_rel_move_to(priv->context, cmd->args[0], cmd->args[1]);
      break;
    case CC_COMMAND_REL_LINE_TO:
      cairo_rel_line_to(priv->context, cmd->args[0], cmd->args[1]);
      break;
    case CC_COMMAND_CLOSE_PATH:
      cairo_close_path(priv->context);
      break;

    case CC_COMMAND_TRANSLATE:
      cairo_translate(priv->context, cmd->args[0], cmd->args[1]);
      break;
    case CC_COMMAND_SCALE:
      cairo_scale(priv->context, cmd->args[0], cmd->args[1]);
      break;
    case CC_COMMAND_ROTATE:
      cairo_rotate(priv->context, cmd->args[0]);
      break;

    case CC_COMMAND_FILL:
      cairo_fill(priv->context);
      break;
    case CC_COMMAND_STROKE:
      cairo_stroke(priv->context);
      break;

    case CC_COMMAND_PUSH_GROUP:
      cairo_push_group(priv->context);
      break;
    case CC_COMMAND_POP_GROUP:
      cairo_pop_group(priv->context);
      break;
    case CC_COMMAND_IDENTITY_MATRIX:
      cairo_identity_matrix(priv->context);
      break;

    case CC_COMMAND_SHOW_TEXT:
      if (priv->text) {
	cairo_show_text(priv->context, priv->text);
      }
      break;

    case CC_COMMAND_ROTATE_SUBSECONDS:
      t = rgb3->tv.tv_sec;
      localtime_r(&t, &lt);
      //printf("t=%ld lt.tm_sec=%d lt.tm_min=%d lt.tm_hour=%d\n", t, lt.tm_sec, lt.tm_min, lt.tm_hour);
      rval = (2*M_PI) * (rgb3->tv.tv_usec/1000000.0);
      cairo_rotate(priv->context, rval);
      break;
    case CC_COMMAND_ROTATE_SECONDS:
      t = rgb3->tv.tv_sec;
      localtime_r(&t, &lt);
      //printf("t=%ld lt.tm_sec=%d lt.tm_min=%d lt.tm_hour=%d\n", t, lt.tm_sec, lt.tm_min, lt.tm_hour);
      rval = (2*M_PI / 60) * (lt.tm_sec /* + (rgb3->tv.tv_usec/1000000.0) */);
      cairo_rotate(priv->context, rval);
      break;
    case CC_COMMAND_ROTATE_MINUTES:
      t = rgb3->tv.tv_sec;
      localtime_r(&t, &lt);
      rval = (2*M_PI / 60) * (lt.tm_min + (lt.tm_sec/60.0));
      cairo_rotate(priv->context, rval);
      break;
    case CC_COMMAND_ROTATE_HOURS:
      t = rgb3->tv.tv_sec;
      localtime_r(&t, &lt);
      rval = (2*M_PI / 12) * (lt.tm_hour + (lt.tm_min/60.0) + (lt.tm_sec/3600.0));
      cairo_rotate(priv->context, rval);
      break;

    default:
      break;
    }
  }

  if (cfg.verbosity) {
    printf("%d cairo operations done, status=%d\n", i, cairo_status(priv->context));
  }

  /* Now merge into RGB buffer. */
  RGB_buffer_merge_rgba(rgb3, 
			cairo_image_surface_get_data(priv->surface),
			cairo_image_surface_get_width(priv->surface),
			cairo_image_surface_get_height(priv->surface),
			cairo_image_surface_get_stride(priv->surface));

  cairo_surface_destroy(priv->surface);
  cairo_destroy(priv->context);
}

static void rgb3_handler(Instance *pi, void *msg)
{
  CairoContext_private *priv = pi->data;
  RGB3_buffer * rgb3 = msg;
  
  apply_commands(priv, rgb3);
  
  if (pi->outputs[OUTPUT_RGB3].destination) {
    PostData(rgb3, pi->outputs[OUTPUT_RGB3].destination);
  }
  else {
    RGB3_buffer_discard(rgb3);
  }
}

static Config config_table[] = {
  { "width",     set_width, 0L, 0L },
  { "height",    set_height, 0L, 0L },
  { "command",   add_command, 0L, 0L },
  { "text",      set_text, 0L, 0L },
};

static void Config_handler(Instance *pi, void *data)
{
  Config_buffer *cb_in = data;
  int i;

  /* Walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {
      int rc;		/* FIXME: What to do with this? */
      rc = config_table[i].set(pi, cb_in->value->bytes);
      break;
    }
  }
  
  Config_buffer_discard(&cb_in);
}

static void CairoContext_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void CairoContext_instance_init(Instance *pi)
{
  CairoContext_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template CairoContext_template = {
  .label = "CairoContext",
  .inputs = CairoContext_inputs,
  .num_inputs = table_size(CairoContext_inputs),
  .outputs = CairoContext_outputs,
  .num_outputs = table_size(CairoContext_outputs),
  .tick = CairoContext_tick,
  .instance_init = CairoContext_instance_init,
};

void CairoContext_init(void)
{
  Template_register(&CairoContext_template);
}
