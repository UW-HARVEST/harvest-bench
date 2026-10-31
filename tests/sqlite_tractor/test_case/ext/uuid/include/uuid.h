#ifndef SQLITE_EXT_UUID_H
#define SQLITE_EXT_UUID_H

typedef struct sqlite3 sqlite3;
struct sqlite3_api_routines;

int sqlite3_uuid_init(sqlite3*, char **, const struct sqlite3_api_routines*);

#endif
