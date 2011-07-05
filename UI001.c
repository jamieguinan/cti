#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "UI001.h"
#include "Pointer.h"

static void Config_handler(Instance *pi, void *msg);
static void pointer_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_POINTER };
static Input UI001_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_POINTER ] = { .type_label = "Pointer_event", .handler = pointer_handler },
};

enum { OUTPUT_CAIRO_CONFIG, OUTPUT_CAIRO_COMMAND };
static Output UI001_outputs[] = {
  [ OUTPUT_CAIRO_CONFIG] = { .type_label = "Cairo_Config_msg", .destination = 0L },
  [ OUTPUT_CAIRO_COMMAND ] =  { .type_label = "CairoCommand", .destination = 0L },
};

enum { UI_BUTTON };
typedef struct {
  int type;
  int x, y;
  unsigned int width, height;
  String *text;
  uint32_t fgcolor;
  uint32_t bgcolor;
  uint32_t border;
} UIWidget;

typedef struct {
  ISet(UIWidget) widgets;
  int down_flag, down_x, down_y;
} UI001_private;


enum { PARSING_KEY, PARSING_VALUE };

static void set_kv(UIWidget *w, const char *key, const char *value, const char *endp)
{
  int key_len = value - key - 1;
  int value_len = endp - value;
  char key_init[key_len+1];
  char value_init[value_len+1];

  strncpy(key_init, key, key_len); key_init[key_len] = 0;
  strncpy(value_init, value, value_len); value_init[value_len] = 0;

  if (streq(key_init, "text")) {
    if (w->text) {
      String_free(&w->text);
    }
    w->text = String_new(value_init);
    fprintf(stderr, "text set to %s\n", value_init);
  }
  else if (streq(key_init, "width")) {
    w->width = strtoul(value_init, NULL, 0);
    fprintf(stderr, "width set to %d\n", w->width);
  }
  else if (streq(key_init, "height")) {
    w->height = strtoul(value_init, NULL, 0);
    fprintf(stderr, "height set to %d\n", w->height);
  }
  else if (streq(key_init, "fgcolor")) {
    w->fgcolor = strtoul(value_init, NULL, 0);
    fprintf(stderr, "fgcolor set to %d\n", w->fgcolor);
  }
  else if (streq(key_init, "bgcolor")) {
    w->bgcolor = strtoul(value_init, NULL, 0);
    fprintf(stderr, "bgcolor set to %d\n", w->bgcolor);
  }
  else if (streq(key_init, "border")) {
    w->border = strtoul(value_init, NULL, 0);
    fprintf(stderr, "border set to %d\n", w->border);
  }
}


static int add_button(Instance *pi, const char *init)
{
  UI001_private *priv = pi->data;
  UIWidget *w = Mem_calloc(1, sizeof(*w));
  const char *key = NULL;
  const char *value = NULL;
  const char *p;
  int state = PARSING_KEY;

  w->type = UI_BUTTON;

  /* Assign non-zero defaults. */
  w->fgcolor = 0x808080;
  w->width = 40;
  w->height = 30;
  
  p = key = init;
  while (*p) {
    if (state == PARSING_KEY) {
      if (*p == '=') {
	value = p+1;
	state = PARSING_VALUE;
      }	
    }
    else if (state == PARSING_VALUE) {
      if (*p == ';') {
	state = PARSING_KEY;
	set_kv(w, key, value, p);
	key = p+1;
	value = NULL;
      }	
    }
    p++;
  }

  if (key && value) {
    set_kv(w, key, value, p);
  }

  ISet_add(priv->widgets, w);
  return 0;
}


static void pointer_handler(Instance *pi, void *msg)
{
  UI001_private *priv = pi->data;
  Pointer_event *p = msg;
  int i;

  //printf("%s: %d,%d [button %d, state %d]\n", __func__, p->x, p->y, p->button, p->state);
  //printf("%s: %d objects to search\n", __func__, priv->widgets.count);

  /* Search widget regions to see if clicked on one of htem. */
  for (i=0; i < priv->widgets.count; i++) {
    UIWidget *w = priv->widgets.items[i];
    // if (w->type == UI_BUTTON)
    if (w->x <= p->x && p->x <= (w->x + w->width) && w->y <= p->y && p->y <= (w->y + w->height)) {
      if (w->text) printf("FOUND: %s\n", w->text->bytes);
      if (p->state == 1) {
	priv->down_flag = 1;
	priv->down_x = p->x;
	priv->down_y = p->y;
      }
      else {
	priv->down_flag = 0;
	priv->down_x = 0;
	priv->down_y = 0;
      }
      break;
    }
  }  
}


static int sync_widgets(Instance *pi, const char *init)
{
  UI001_private *priv = pi->data;
  int i;

  if (!pi->outputs[OUTPUT_CAIRO_CONFIG].destination) {
    fprintf(stderr, "%s: no output connected\n", __func__);
    return 0;
  }

  for (i=0; i < priv->widgets.count; i++) {
    UIWidget *w = priv->widgets.items[i];
    switch(w->type) {
    case UI_BUTTON:
      // command = CC_COMMAND_SET_SOURCE_RGBA 0.3 0.3 0.3 0.5
      // command = CC_COMMAND_IDENTITY_MATRIX
      // command = CC_COMMAND_MOVE_TO, args[0] = w->x, args[1] = w->y
      // command = CC_COMMAND_REL_LINE_TO, args[0] = w->width, args[1] = 0
      // command = CC_COMMAND_REL_LINE_TO, args[0] = 0, args[1] = w->height
      // command = CC_COMMAND_REL_LINE_TO, args[0] = -w->width, args[1] = 0
      // command = CC_COMMAND_CLOSE_PATH
      // command = CC_COMMAND_FILL

      // command = CC_COMMAND_SET_SOURCE_RGB 0.9 0.9 0.9
      // command = CC_COMMAND_IDENTITY_MATRIX
      // command = CC_COMMAND_MOVE_TO
      // command = CC_COMMAND_SET_FONT_SIZE
      // command = CC_COMMAND_SHOW_TEXT [ Need .text in struct CairoCommand ??? ]
      break;
    }
  }

  return 0;
}


static Config config_table[] = {
  { "add_button",   add_button,  0L, 0L },
  { "sync_widgets", sync_widgets,  0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void UI001_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void UI001_instance_init(Instance *pi)
{
  UI001_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template UI001_template = {
  .label = "UI001",
  .inputs = UI001_inputs,
  .num_inputs = table_size(UI001_inputs),
  .outputs = UI001_outputs,
  .num_outputs = table_size(UI001_outputs),
  .tick = UI001_tick,
  .instance_init = UI001_instance_init,
};

void UI001_init(void)
{
  Template_register(&UI001_template);
}
