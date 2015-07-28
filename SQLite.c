/* Convenience wrapper around sqlite3. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "SQLite.h"
#include "sqlite3.h"

#define no_callback NULL
#define no_errmsg NULL

typedef struct _SQLite {
  sqlite3 *db;
} SQLite;

SQLite * SQLite_open(String * dbschema, String * dbfile)
{
  int rc;
  SQLite * sql = NULL;
  puts(__func__);

  if (String_is_none(dbschema)) {
    fprintf(stderr, "%s:%s schema is unset\n", __FILE__, __func__);
    rc = 1;
    goto out;
  }

  sql = Mem_calloc(1, sizeof(*sql));

  rc = sqlite3_open(s(dbfile), &sql->db);
  if(rc != 0) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(sql->db));
    sqlite3_close(sql->db); Mem_free(sql);
    goto out;
  }

  String * q = String_sprintf("CREATE TABLE IF NOT EXISTS %s;", s(dbschema));
  rc = sqlite3_exec(sql->db, s(q), no_callback, 0, no_errmsg);
  if(rc != 0) {
    fprintf(stderr, "%s\nerror %d: %s\n", s(q), rc, sqlite3_errmsg(sql->db));
    goto out;
  }
  String_free(&q);

 out:
  return sql;
}


void SQLite_init(void)
{
}
