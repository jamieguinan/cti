#ifndef _SQLITE_H_
#define _SQLITE_H_

extern void SQLite_init(void);

typedef struct _SQLite SQLite; /* Opaque type. */

extern SQLite * SQLite_open(String * dbschema, String * dbfile );
extern void SQLite_exec(SQLite * sql, const char * query);

#endif
