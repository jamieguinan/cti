#ifndef _CSV_H
#define _CSV_H

#include "CTI.h"

typedef struct {
  int _columns;
  int _rows;
  ISet(String) _entries;
} CSV_table;

String *CSV_get(CSV_table *csv, int row, int column);

CSV_table *CSV_load(String *path);

#endif
