#include "CSV.h"
#include "File.h"
#include <string.h>		/* strchr */
#include <stdio.h>		/* fprintf */

String *CSV_get(CSV_table *csv, int row, int column)
{
  if (row > csv->_rows-1) {
    return NULL;
  }
  if (column > csv->_columns-1) {
    return NULL;
  }

  int i = (row * csv->_columns) + column;
  
  return csv->_entries.items[i];
}


CSV_table *CSV_load(String *path)
{
  if (!path) {
    return NULL;
  }

  String *contents = File_load_text(path);
  if (!contents) {
    return NULL;
  }

  CSV_table *csv = Mem_calloc(1, sizeof(*csv));

  char *p = contents->bytes;
  char *newline;
  char *comma;
  int columns = 0;
  int rows = 0;

  while (1) {
    //puts("-------");
    int ccount = 0;
    newline = strchr(p, '\n');
    if (!newline) {
      break;
    }

    *newline = 0;

    while (p < newline) {
      ccount += 1;
      comma = strchr(p, ',');
      if (!comma) {
	comma = newline;	/* allow trailing comma or newline to end a row */
      }

      int len = comma - p + 1; 	/* leave room to null-terminate tmp */
      char tmp[len];
      strncpy(tmp, p, len);	/* does not copy the comma */
      tmp[len-1] = 0;		/* null-terminate */

      //printf("%s\n", tmp);

#if 1
      IndexedSet_add(csv->_entries, String_new(tmp));
#else
      { 
	if (csv->_entries.items == 0L) {
	  printf("%d\n", __LINE__);
	  csv->_entries.avail = 2; 
	  printf("%d\n", __LINE__);
	  csv->_entries.items = _Mem_calloc(csv->_entries.avail, sizeof(String_new(tmp)), __func__); 
	  printf("%d\n", __LINE__);
	}
	else if (csv->_entries.avail == csv->_entries.count) {
	  printf("%d\n", __LINE__);
	  csv->_entries.avail *= 2; 
	  printf("%d\n", __LINE__);
	  csv->_entries.items = _Mem_realloc(csv->_entries.items, csv->_entries.avail * sizeof(String_new(tmp)), __func__); 
	  printf("%d\n", __LINE__);
	}
	printf("%d\n", __LINE__);
	csv->_entries.items[csv->_entries.count] = String_new(tmp); 
	printf("%d\n", __LINE__);
	csv->_entries.count += 1; 
	printf("%d\n", __LINE__);
      }
#endif
      p = comma + 1;
    }

    if (columns == 0) {
      columns = ccount;
    }

    if (columns != ccount) {
      fprintf(stderr, "found %d columns, expected %d!\n", columns, ccount);
      return NULL;
    }

    rows += 1;

    if (*p == 0) {
      break;
    }
  }

  csv->_rows = rows;
  csv->_columns = columns;
  
  return csv;
}
