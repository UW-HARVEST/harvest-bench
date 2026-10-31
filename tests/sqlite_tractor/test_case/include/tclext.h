#ifndef SQLITE_STATIC_EXT_H
#define SQLITE_STATIC_EXT_H 

/* 
 * Prototypes for init function for SQLITE extension that are used 
 * by both the TCL tests and the shell.
*/

#include "sqlite3.h"

int sqlite3_amatch_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_appendvfs_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_base64_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_base85_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_closure_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_completion_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_csv_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_decimal_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_eval_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_explain_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_diskused_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_fileio_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_fuzzer_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_ieee_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_nextchar_init(sqlite3*, char **, const struct sqlite3_api_routines*);
#ifndef SQLITE_OMIT_VIRTUALTABLE
int sqlite3_prefixes_init(sqlite3*, char **, const struct sqlite3_api_routines*);
#endif
int sqlite3_qpvtab_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_randomjson_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_regexp_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_remember_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_series_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_shellfix_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_sha_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_shathree_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_sqlar_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_stmtrand_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_totype_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_uint_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_unionvtab_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_wholenumber_init(sqlite3*, char **, const struct sqlite3_api_routines*);
int sqlite3_zipfile_init(sqlite3*, char **, const struct sqlite3_api_routines*);

#endif
