#ifndef _DBUTIL_H_
#define _DBUTIL_H_

#include "String.h"
#include "sqlite3.h"

#define no_callback NULL
#define no_errmsg NULL

typedef struct {
  const char * name;
  const char * type;
} SchemaColumn;

typedef struct {
  SchemaColumn * columns;
  int index;
  int num_columns;
  int error;
  String * existing_schema;
  String * common_columns;
  int common_count;
} SchemaColumn_check;

extern int db_check(sqlite3 *db, const char * table_name, 
		    SchemaColumn * schema, int num_columns, const char * constraints);

extern int sql_exec_free_query(sqlite3 *pdb,
			       char * query,
			       int (*callback)(void*,int,char**,char**),
			       void * data,
			       char **errmsg);


#endif
