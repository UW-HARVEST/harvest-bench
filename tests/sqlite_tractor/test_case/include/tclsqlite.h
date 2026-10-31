/*
** 2024-07-30
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Tcl-facing SQLite declarations.
*/

#ifndef SQLITE_TCL_TCLSQLITE_H
#define SQLITE_TCL_TCLSQLITE_H

#include <tcl.h>
#include "sqlite3.h"

#define SQLITE_TCLAPI

typedef int Tcl_Size;
#define Tcl_BounceRefCount(X) Tcl_IncrRefCount(X); Tcl_DecrRefCount(X)

/* Main TCL interface */
int Sqlite3_Init(Tcl_Interp *interp);

#ifdef SQLITE_ENABLE_FTS3
int Sqlitetestfts3_Init(Tcl_Interp *interp);
#endif
#ifdef SQLITE_ENABLE_FTS5
int Fts5tcl_Init(Tcl_Interp *interp);
#endif
#ifdef SQLITE_SHELL_HAVE_RECOVER
int TestRecover_Init(Tcl_Interp *interp);
#endif
#ifdef SQLITE_ENABLE_RTREE
int Sqlitetestrtreedoc_Init(Tcl_Interp *interp);
#endif
#ifdef SQLITE_ENABLE_SESSION
int TestSession_Init(Tcl_Interp *interp);
#endif

#endif /* SQLITE_TCL_TCLSQLITE_H */
