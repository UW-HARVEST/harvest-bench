/*
** 2022-11-20
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This combines the base64.c and base85.c extensions.
*/

#include "base64.c"
#include "base85.c"

int sqlite3_basexx_init(sqlite3 *db, char **pzErr,
                               const sqlite3_api_routines *pApi){
  int rc1;
  int rc2;

  rc1 = BASE64_INIT(db);
  rc2 = BASE85_INIT(db);

  if( rc1==SQLITE_OK && rc2==SQLITE_OK ){
    BASE64_EXPOSE(db, pzErr);
    BASE64_EXPOSE(db, pzErr);
    return SQLITE_OK;
  }else{
    return SQLITE_ERROR;
  }
}
