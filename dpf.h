#ifndef _DPF_H_
#define _DPF_H_

extern void cti_debug_printf_register(const char *file, int line, int *enabled, const char *fmt);
extern void cti_debug_printf(const char *fmt, ...);
extern void cti_debug_printf_list(void);
extern void cti_debug_printf_toggle(int index);

/* Debug printfs.  FIXME: This requires at least 2 arguments, fmt and one more. */
#define dpf(fmt, ...) \
do { \
  static int enabled=1; \
  static int registered; \
  if (enabled) { \
    if (!registered) { \
      registered = 1; \
      enabled = 0; \
      cti_debug_printf_register(__FILE__, __LINE__, &enabled, fmt); \
    } \
    else { \
      cti_debug_printf(fmt, __VA_ARGS__); \
    } \
  } \
 } while(0)

#endif
