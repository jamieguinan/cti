#ifndef _SCRIPTV00_H_
#define _SCRIPTV00_H_

extern void ScriptV00_init(void);

/* This can be used outside "cti". */
extern void ScriptV00_Config(Instance *inst, const char *token2, const char *token3);

#endif
