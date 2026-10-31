#ifndef SQLITE_EXT_ROT13_H
#define SQLITE_EXT_ROT13_H

typedef struct sqlite3 sqlite3;
struct sqlite3_api_routines;

int sqlite3_rot_init(sqlite3*, char **, const struct sqlite3_api_routines*);

#endif
