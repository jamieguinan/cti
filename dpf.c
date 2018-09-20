#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "Mem.h"
#include "dpf.h"
#ifndef streq
#define streq(a,b) (strcmp((a),(b)) == 0)
#endif

typedef struct {
  const char *file;		/* use streq rather than == for comparison, to allow
				   registering before first call */
  int line;
  int *enabled;
  const char *fmt;
} DebugPrintfRecord;

struct {
  DebugPrintfRecord *records;
  int count;
  int available;
} printfRecords;


void cti_debug_printf_register(const char *file, int line, int *enabled, const char *fmt)
{
  int i;
  for (i=0; i < printfRecords.count; i++) {
    if (streq(file, printfRecords.records[i].file)
	&& line == printfRecords.records[i].line
	&& streq(fmt, printfRecords.records[i].fmt))
      {
	/* This entry was pre-registered. */
	printfRecords.records[i].enabled = enabled;
	/* Enable it! */
	*enabled = 1;
	return;
      }
  }

  if (!printfRecords.available) {
    printfRecords.available = 32;
    printfRecords.records = Mem_calloc(printfRecords.available, sizeof(*printfRecords.records));
  }
  else if (printfRecords.available == (printfRecords.count+1)) {
    printfRecords.available *= 2;
    printfRecords.records = Mem_realloc(printfRecords.records,
				    printfRecords.available*sizeof(*printfRecords.records));
  }

  printfRecords.records[printfRecords.count].file = file;
  printfRecords.records[printfRecords.count].line = line;
  printfRecords.records[printfRecords.count].enabled = enabled;
  printfRecords.records[printfRecords.count].fmt = fmt;

  printfRecords.count += 1;
}


void cti_debug_printf_list(void)
{
  int i;
  int n;
  for (i=0; i < printfRecords.count; i++) {
    printf("[%d] %s %s:%d %s",
	   i,
	   *printfRecords.records[i].enabled ? " *":"",
	   printfRecords.records[i].file,
	   printfRecords.records[i].line,
	   printfRecords.records[i].fmt);

    n = strlen(printfRecords.records[i].fmt);
    if (n && printfRecords.records[i].fmt[n-1] != '\n') {
      fputc('\n', stdout);
    }
  }
}

void cti_debug_printf_toggle(int index)
{
  if (index < printfRecords.count) {
    int e = *printfRecords.records[index].enabled;
    if (!e) {
      *printfRecords.records[index].enabled = 1;
    }
    else {
      *printfRecords.records[index].enabled = 0;
    }
  }
}


void cti_debug_printf(const char *fmt, ...)
{
  int rc;
  va_list ap;
  va_start(ap, fmt);
  rc = vprintf(fmt, ap);
  if (-1 == rc) {
    perror("vsprintf");
  }
  va_end(ap);
}
