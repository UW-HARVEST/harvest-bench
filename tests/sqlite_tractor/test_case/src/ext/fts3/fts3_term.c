/*
** 2011 Jan 27
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file is not part of the production FTS code. It is only used for
** testing. It contains a virtual table implementation that provides direct 
** access to the full-text index of an FTS table. 
*/

#include "fts3Int.h"
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3)
#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3) */
