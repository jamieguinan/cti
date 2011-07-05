#ifndef _CAIROCONTEXT_H_
#define _CAIROCONTEXT_H_

extern void CairoContext_init(void);

/* The stuff below is to allow passing binary messages between instances. */

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

  CC_COMMAND_SET_TEXT,
  CC_COMMAND_SET_FONT_SIZE,
  CC_COMMAND_SHOW_TEXT,

  /* I made these up, they make use of the date stamp in the input RGB buffer. */
  CC_COMMAND_ROTATE_SUBSECONDS,
  CC_COMMAND_ROTATE_SECONDS,
  CC_COMMAND_ROTATE_MINUTES,
  CC_COMMAND_ROTATE_HOURS,

  CC_COMMAND_TIMEOUT,
};

#define CC_MAX_DOUBLE_ARGS 6
/* A cairo command with up to 6 double parameters, and an optional text field. */
typedef struct {
  int command;
  double args[CC_MAX_DOUBLE_ARGS];
  String *text;
} CairoCommand;

typedef struct {
  CairoCommand *commands;
  int num_commands;
} CairoCommandBlock;

#endif
