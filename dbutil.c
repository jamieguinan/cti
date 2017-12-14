/*
 * Convenience functions around sqlite3.
 */
#include "CTI.h"
#include "String.h"
#include "dbutil.h"
#include "localptr.h"
#include <stdio.h>

/* Verbosity flag. FIXME: Make this a push/pop. */
static int v = 0;

int sql_exec_free_query(sqlite3 *pdb,
			 char * query,
			 int (*callback)(void*,int,char**,char**),
			 void * data,
			 char **errmsg)
{
  /* Convenience wrapper around the sqlite3 convenience wrapper. It is
     assumed that the "query" input parameter is the result of a call
     to sqlite3_mprintf(), and it is freed here. */
  //printf("%s\n", query);
  int rc = sqlite3_exec(pdb, query, callback, data, errmsg);
  if (rc != 0) {
    fprintf(stderr, "%s rc=%d error=%s\n", query,  rc, sqlite3_errmsg(pdb));
  }
  sqlite3_free(query);
  return rc;
}

static int schema_check_callback(void *i_ptr, 
				 int num_columns, char **column_strings, char **column_headers)
{
  SchemaColumn_check * sdc = i_ptr;
  char * existing_column_name = column_strings[1];
  char * existing_column_type = column_strings[2];

  if (sdc->index > 0) { String_cat1(sdc->existing_schema, ",");  }
  String_cat2(sdc->existing_schema, " ", existing_column_name); /* Column name */
  String_cat2(sdc->existing_schema, " ", existing_column_type); /* Column type */
  
  if (sdc->error != 0) {
    /* Already seen one error, skip remaining checks. */
    goto out;
  }

  if (sdc->index >= sdc->num_columns) {
    fprintf(stderr, "too many columns\n");
    sdc->error = 1;
    goto out;
  }

  if (!streq(sdc->columns[sdc->index].name, existing_column_name)) {
    fprintf(stderr, "column name mismatch: %s %s\n", sdc->columns[sdc->index].name, existing_column_name);
    sdc->error = 2;
    goto out;
  }

  if (!streq(sdc->columns[sdc->index].type, existing_column_type)) {
    fprintf(stderr, "column type mismatch: %s %s\n", sdc->columns[sdc->index].type, existing_column_type);
    sdc->error = 3;
    goto out;
  }

  if (v) fprintf(stderr, "%s %s matches (%d %s)\n",
	 sdc->columns[sdc->index].name,
	 sdc->columns[sdc->index].type,
	 sdc->index, 
	 column_strings[0]);

 out:
  if (sdc->error == 0) {
    /* Append common columns. */
    if (sdc->common_count > 0) { String_cat1(sdc->common_columns, ","); }
    String_cat1(sdc->common_columns, existing_column_name);
    sdc->common_count += 1;
  }
  else {
    /* Search for current column among remaining schema columns. */
    int i;
    for (i=sdc->index; i < sdc->num_columns; i++) {
      if (streq(sdc->columns[i].name, existing_column_name)
	  && streq(sdc->columns[i].type, existing_column_type)) {
	if (sdc->common_count > 0) { String_cat1(sdc->common_columns, ","); }
	String_cat1(sdc->common_columns, existing_column_name);
	sdc->common_count += 1;
	break;
      }
    }
  }
  
  sdc->index += 1;
  return 0;
}


int db_check(sqlite3 *db, const char * table_name, 
	     SchemaColumn * schema, int num_columns, const char * constraints)
{
  /*
   * This function is more {complicated, obtuse, overloaded} than I
   * like, but it works well enough and I have several programs using
   * it, so I'm keeping it. See also db_schema_test() below for a simpler
   * function that only tests whether a table matches a schema.
   * 
   * This function does several things:
   *   . Tests whether named (existing) table matches the supplied schema.
   *   . Creates the table if it doesn't already exist.
   *   . Reorganizes existing table columns to match current schema.
   */
  
  /* Check for same number of columns, with same names and types. */
  SchemaColumn_check sdc = { schema, 0, num_columns, 0, String_new(""), String_new("") };
  int rc;
  rc = sql_exec_free_query(db,
			   sqlite3_mprintf("PRAGMA main.table_info('%s')", table_name),
			   schema_check_callback,
			   &sdc,
			   no_errmsg);

  if (sdc.index != sdc.num_columns) {
    fprintf(stderr, "schema has %d columns, table has %d\n", sdc.num_columns, sdc.index);
    sdc.error = 4;
  }

  if (sdc.index > 0) {
    if (v) fprintf(stderr, "existing_schema: %s\n", s(sdc.existing_schema));
    if (v) fprintf(stderr, "common_columns: %s\n", s(sdc.common_columns));
  }

  if (sdc.error) {
    int i;

    String * table_schema = String_new("");
    for (i=0; i < num_columns; i++) {
      if (i > 0) { String_cat1(table_schema, ", "); }
      String_cat2(table_schema, schema[i].name, " ");
      String_cat1(table_schema, schema[i].type);
      if (schema[i].constraints) {
	String_cat2(table_schema, " ", schema[i].constraints);
      }
    }
    if (constraints && !streq(constraints, "")) {
      String_cat2(table_schema, ", ", constraints);
    }

    if (sdc.index == 0) {
      /* Table was not found */
      fprintf(stderr, "Creating table %s\n", table_name);
      sql_exec_free_query(db, sqlite3_mprintf("CREATE TABLE %s (%s);", 
					      table_name, s(table_schema)),
			  no_callback, 0, no_errmsg);
    }
    else {
      /* Table exists, but schema has changed. */
      fprintf(stderr, "Regenerating table '%s'\n", table_name);
      // db_push_verbose(1);
      sql_exec_free_query(db, sqlite3_mprintf("DROP TABLE backup_tbl"),
			  no_callback, 0, no_errmsg);		
      sql_exec_free_query(db, sqlite3_mprintf("CREATE TABLE backup_tbl(%s);", s(sdc.existing_schema)),
			  no_callback, 0, no_errmsg);
      sql_exec_free_query(db, sqlite3_mprintf("INSERT INTO backup_tbl SELECT * FROM %s;", table_name),
			  no_callback, 0, no_errmsg);
      sql_exec_free_query(db, sqlite3_mprintf("DROP TABLE %s;", table_name),
			  no_callback, 0, no_errmsg);
      sql_exec_free_query(db, sqlite3_mprintf("CREATE TABLE %s (%s);",
					      table_name, s(table_schema)),
			  no_callback, 0, no_errmsg);
      sql_exec_free_query(db, sqlite3_mprintf("INSERT INTO %s (%s) SELECT %s FROM backup_tbl", 
					      table_name, s(sdc.common_columns), s(sdc.common_columns)),
			  no_callback, 0, no_errmsg);
      // db_pop_verbose();
    }
    sdc.error = 0;
    String_free(&table_schema);
  }

  String_free(&sdc.existing_schema);
  String_free(&sdc.common_columns);

  return rc;
}


int db_schema_test(sqlite3 *db, const char * table_name, SchemaColumn * schema, int num_columns)
{
  /* Test whether table matches schema. Returns 0 if match, error code if not match. */
  
  /* Check for same number of columns, with same names and types. */
  localptr(String, existing_schema) = String_new("");
  localptr(String, existing_columns) = String_new("");
  SchemaColumn_check sdc = { schema, 0, num_columns, 0, existing_schema, existing_columns };
  int rc;
  rc = sql_exec_free_query(db,
			   sqlite3_mprintf("PRAGMA main.table_info('%s')", table_name),
			   schema_check_callback,
			   &sdc,
			   no_errmsg);

  return sdc.error;
}


int sql_String_callback(void *i_ptr, int num_columns, char **column_strings, char **column_headers)
{
  String * s = i_ptr;
  if (num_columns != 1) {
    fprintf(stderr, "%s: expected only 1 column in results\n", __func__);
    return 1;
  }
  String_set_local(s, column_strings[0]);
  return 0;
}


int db_int_string_callback(void *i_ptr, int num_columns, char **column_strings, char **column_headers)
{
  int expected_columns = 2;
  if (num_columns != expected_columns) {
    fprintf(stderr, "%s: expected %d columns in result\n", __func__, expected_columns);
    return 1;
  }
  db_int_String * p = i_ptr;
  if (sscanf(column_strings[0], "%d", &p->i0) != 1) {
    fprintf(stderr, "%s: result is not an integer\n", __func__);
    return 1;
  }
  String_set(&p->s, column_strings[1]);
  p->count += 1;
  return 0;
}

int db_int_int_callback(void *i_ptr, int num_columns, char **column_strings, char **column_headers)
{
  int expected_columns = 2;
  if (num_columns != expected_columns) {
    fprintf(stderr, "%s: expected %d columns in result\n", __func__, expected_columns);
    return 1;
  }

  db_int_int * p = i_ptr;
  if (sscanf(column_strings[0], "%d", &p->i0) != 1) {
    fprintf(stderr, "%s: result is not an integer\n", __func__);
    return 1;
  }

  if (sscanf(column_strings[1], "%d", &p->i1) != 1) {
    fprintf(stderr, "%s: result is not an integer\n", __func__);
    return 1;
  }

  p->count += 1;
  return 0;
}


int db_int_callback(void *i_ptr, int num_columns, char **column_strings, char **column_headers)
{
  int expected_columns = 1;
  if (num_columns != expected_columns) {
    fprintf(stderr, "%s: expected %d columns in result\n", __func__, expected_columns);
    return 1;
  }

  db_int * p = i_ptr;
  if (sscanf(column_strings[0], "%d", &p->i0) != 1) {
    fprintf(stderr, "%s: result is not an integer\n", __func__);
    return 1;
  }

  p->count += 1;
  return 0;
}
