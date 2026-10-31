/*
** 2016-09-07
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
** This file implements an in-memory VFS. A database is held as a contiguous
** block of memory.
**
** This file also implements interface sqlite3_serialize() and
** sqlite3_deserialize().
*/
#include "sqliteInt.h"
#ifndef SQLITE_OMIT_DESERIALIZE

/*
** Forward declaration of objects used by this utility
*/
typedef struct sqlite3_vfs MemVfs;
typedef struct MemFile MemFile;
typedef struct MemStore MemStore;

/* Access to a lower-level VFS that (might) implement dynamic loading,
** access to randomness, etc.
*/
#define ORIGVFS(p) ((sqlite3_vfs*)((p)->pAppData))

/* Storage for a memdb file.
**
** An memdb object can be shared or separate.  Shared memdb objects can be
** used by more than one database connection.  

** Separate memdb objects are created using a name that does not begin
** with "/" or using sqlite3_deserialize().
*/
struct MemStore {
  sqlite3_int64 sz;               /* Size of the file */
  sqlite3_int64 szAlloc;          /* Space allocated to aData */
  sqlite3_int64 szMax;            /* Maximum allowed size of the file */
  unsigned char *aData;           /* content of the file */
  unsigned mFlags;                /* Flags */
  int nRdLock;                    /* Number of readers */
  int nWrLock;                    /* Number of writers.  (Always 0 or 1) */
  int nRef;                       /* Number of users of this MemStore */
  char *zFName;                   /* The filename for shared stores */
};

/* An open file */
struct MemFile {
  sqlite3_file base;              /* IO methods */
  MemStore *pStore;               /* The storage */
};

/*
** File-scope variables for holding named memdb files.
*/
static struct MemFS {
  int nMemStore;                  /* Number of shared MemStore objects */
  MemStore **apMemStore;          /* Array of all shared MemStore objects */
} memdb_g;

/*
** Methods for MemFile
*/
static int memdbClose(sqlite3_file*);
static int memdbRead(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
static int memdbWrite(sqlite3_file*,const void*,int iAmt, sqlite3_int64 iOfst);
static int memdbTruncate(sqlite3_file*, sqlite3_int64 size);
static int memdbSync(sqlite3_file*, int flags);
static int memdbFileSize(sqlite3_file*, sqlite3_int64 *pSize);
static int memdbFileControl(sqlite3_file*, int op, void *pArg);
static int memdbDeviceCharacteristics(sqlite3_file*);

/*
** Methods for MemVfs
*/
static int memdbOpen(sqlite3_vfs*, const char *, sqlite3_file*, int , int *);
static int memdbAccess(sqlite3_vfs*, const char *zName, int flags, int *);
static int memdbFullPathname(sqlite3_vfs*, const char *zName, int, char *zOut);
static void *memdbDlOpen(sqlite3_vfs*, const char *zFilename);
static void memdbDlError(sqlite3_vfs*, int nByte, char *zErrMsg);
static void (*memdbDlSym(sqlite3_vfs *pVfs, void *p, const char*zSym))(void);
static void memdbDlClose(sqlite3_vfs*, void*);
static int memdbRandomness(sqlite3_vfs*, int nByte, char *zOut);
static int memdbSleep(sqlite3_vfs*, int microseconds);
static int memdbGetLastError(sqlite3_vfs*, int, char *);
static int memdbCurrentTimeInt64(sqlite3_vfs*, sqlite3_int64*);

static sqlite3_vfs memdb_vfs = {
  2,                           /* iVersion */
  0,                           /* szOsFile (set when registered) */
  1024,                        /* mxPathname */
  0,                           /* pNext */
  "memdb",                     /* zName */
  0,                           /* pAppData (set when registered) */ 
  memdbOpen,                   /* xOpen */
  0, /* memdbDelete, */        /* xDelete */
  memdbAccess,                 /* xAccess */
  memdbFullPathname,           /* xFullPathname */
  memdbDlOpen,                 /* xDlOpen */
  memdbDlError,                /* xDlError */
  memdbDlSym,                  /* xDlSym */
  memdbDlClose,                /* xDlClose */
  memdbRandomness,             /* xRandomness */
  memdbSleep,                  /* xSleep */
  0, /* memdbCurrentTime, */   /* xCurrentTime */
  memdbGetLastError,           /* xGetLastError */
  memdbCurrentTimeInt64,       /* xCurrentTimeInt64 */
  0,                           /* xSetSystemCall */
  0,                           /* xGetSystemCall */
  0,                           /* xNextSystemCall */
};

static const sqlite3_io_methods memdb_io_methods = {
  3,                              /* iVersion */
  memdbClose,                      /* xClose */
  memdbRead,                       /* xRead */
  memdbWrite,                      /* xWrite */
  memdbTruncate,                   /* xTruncate */
  memdbSync,                       /* xSync */
  memdbFileSize,                   /* xFileSize */
  memdbFileControl,                /* xFileControl */
  0, /* memdbSectorSize,*/         /* xSectorSize */
  memdbDeviceCharacteristics,      /* xDeviceCharacteristics */
};

/*
** Close an memdb-file.
** Free the underlying MemStore object when its refcount drops to zero
** or less.
*/
static int memdbClose(sqlite3_file *pFile){
  MemStore *p = ((MemFile*)pFile)->pStore;
  if( p->zFName ){
    int i;
    for(i=0; ALWAYS(i<memdb_g.nMemStore); i++){
      if( memdb_g.apMemStore[i]==p ){
        if( p->nRef==1 ){
          memdb_g.apMemStore[i] = memdb_g.apMemStore[--memdb_g.nMemStore];
          if( memdb_g.nMemStore==0 ){
            sqlite3_free(memdb_g.apMemStore);
            memdb_g.apMemStore = 0;
          }
        }
        break;
      }
    }
  }
  p->nRef--;
  if( p->nRef<=0 ){
    if( p->mFlags & SQLITE_DESERIALIZE_FREEONCLOSE ){
      sqlite3_free(p->aData);
    }
    sqlite3_free(p);
  }
  return SQLITE_OK;
}

/*
** Read data from an memdb-file.
*/
static int memdbRead(
  sqlite3_file *pFile, 
  void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  MemStore *p = ((MemFile*)pFile)->pStore;
  if( iOfst+iAmt>p->sz ){
    memset(zBuf, 0, iAmt);
    if( iOfst<p->sz ) memcpy(zBuf, p->aData+iOfst, p->sz - iOfst);
    return SQLITE_IOERR_SHORT_READ;
  }
  memcpy(zBuf, p->aData+iOfst, iAmt);
  return SQLITE_OK;
}

/*
** Try to enlarge the memory allocation to hold at least sz bytes
*/
static int memdbEnlarge(MemStore *p, sqlite3_int64 newSz){
  unsigned char *pNew;
  if( (p->mFlags & SQLITE_DESERIALIZE_RESIZEABLE)==0 ){
    return SQLITE_FULL;
  }
  if( newSz>p->szMax ){
    return SQLITE_FULL;
  }
  newSz *= 2;
  if( newSz>p->szMax ) newSz = p->szMax;
  pNew = sqlite3Realloc(p->aData, newSz);
  if( pNew==0 ) return SQLITE_IOERR_NOMEM;
  p->aData = pNew;
  p->szAlloc = newSz;
  return SQLITE_OK;
}

/*
** Write data to an memdb-file.
*/
static int memdbWrite(
  sqlite3_file *pFile,
  const void *z,
  int iAmt,
  sqlite_int64 iOfst
){
  MemStore *p = ((MemFile*)pFile)->pStore;
  if( p->mFlags & SQLITE_DESERIALIZE_READONLY ){
    return SQLITE_READONLY;
  }
  if( iOfst+iAmt>p->sz ){
    int rc;
    if( iOfst+iAmt>p->szAlloc
     && (rc = memdbEnlarge(p, iOfst+iAmt))!=SQLITE_OK
    ){
      return rc;
    }
    if( iOfst>p->sz ) memset(p->aData+p->sz, 0, iOfst-p->sz);
    p->sz = iOfst+iAmt;
  }
  memcpy(p->aData+iOfst, z, iAmt);
  return SQLITE_OK;
}

/*
** Truncate an memdb-file.
**
** In rollback mode the truncate() method is only used to reduce
** the size of a file, never to increase the size.
*/
static int memdbTruncate(sqlite3_file *pFile, sqlite_int64 size){
  MemStore *p = ((MemFile*)pFile)->pStore;
  int rc = SQLITE_OK;
  if( size>p->sz ){
    rc = SQLITE_CORRUPT;
  }else{
    p->sz = size; 
  }
  return rc;
}

/*
** Sync an memdb-file.
*/
static int memdbSync(sqlite3_file *pFile, int flags){
  UNUSED_PARAMETER(pFile);
  UNUSED_PARAMETER(flags);
  return SQLITE_OK;
}

/*
** Return the current file-size of an memdb-file.
*/
static int memdbFileSize(sqlite3_file *pFile, sqlite_int64 *pSize){
  MemStore *p = ((MemFile*)pFile)->pStore;
  *pSize = p->sz;
  return SQLITE_OK;
}

/*
** File control method. For custom operations on an memdb-file.
*/
static int memdbFileControl(sqlite3_file *pFile, int op, void *pArg){
  MemStore *p = ((MemFile*)pFile)->pStore;
  int rc = SQLITE_NOTFOUND;
  if( op==SQLITE_FCNTL_VFSNAME ){
    *(char**)pArg = sqlite3_mprintf("memdb(%p,%lld)", p->aData, p->sz);
    rc = SQLITE_OK;
  }
  if( op==SQLITE_FCNTL_SIZE_LIMIT ){
    sqlite3_int64 iLimit = *(sqlite3_int64*)pArg;
    if( iLimit<p->sz ){
      if( iLimit<0 ){
        iLimit = p->szMax;
      }else{
        iLimit = p->sz;
      }
    }
    p->szMax = iLimit;
    *(sqlite3_int64*)pArg = iLimit;
    rc = SQLITE_OK;
  }
  return rc;
}


/*
** Return the device characteristic flags supported by an memdb-file.
*/
static int memdbDeviceCharacteristics(sqlite3_file *pFile){
  UNUSED_PARAMETER(pFile);
  return SQLITE_IOCAP_ATOMIC | 
         SQLITE_IOCAP_POWERSAFE_OVERWRITE |
         SQLITE_IOCAP_SAFE_APPEND |
         SQLITE_IOCAP_SEQUENTIAL;
}

/*
** Open an mem file handle.
*/
static int memdbOpen(
  sqlite3_vfs *pVfs,
  const char *zName,
  sqlite3_file *pFd,
  int flags,
  int *pOutFlags
){
  MemFile *pFile = (MemFile*)pFd;
  MemStore *p = 0;
  int szName;
  UNUSED_PARAMETER(pVfs);

  memset(pFile, 0, sizeof(*pFile));
  szName = sqlite3Strlen30(zName);
  if( szName>1 && (zName[0]=='/' || zName[0]=='\\') ){
    int i;
    for(i=0; i<memdb_g.nMemStore; i++){
      if( strcmp(memdb_g.apMemStore[i]->zFName,zName)==0 ){
        p = memdb_g.apMemStore[i];
        break;
      }
    }
    if( p==0 ){
      MemStore **apNew;
      p = sqlite3Malloc( sizeof(*p) + (i64)szName + 3 );
      if( p==0 ){
        return SQLITE_NOMEM;
      }
      apNew = sqlite3Realloc(memdb_g.apMemStore,
                             sizeof(apNew[0])*(1+(i64)memdb_g.nMemStore) );
      if( apNew==0 ){
        sqlite3_free(p);
        return SQLITE_NOMEM;
      }
      apNew[memdb_g.nMemStore++] = p;
      memdb_g.apMemStore = apNew;
      memset(p, 0, sizeof(*p));
      p->mFlags = SQLITE_DESERIALIZE_RESIZEABLE|SQLITE_DESERIALIZE_FREEONCLOSE;
      p->szMax = sqlite3GlobalConfig.mxMemdbSize;
      p->zFName = (char*)&p[1];
      memcpy(p->zFName, zName, szName+1);
      p->nRef = 1;
    }else{
      p->nRef++;
    }
  }else{
    p = sqlite3Malloc( sizeof(*p) );
    if( p==0 ){
      return SQLITE_NOMEM;
    }
    memset(p, 0, sizeof(*p));
    p->mFlags = SQLITE_DESERIALIZE_RESIZEABLE | SQLITE_DESERIALIZE_FREEONCLOSE;
    p->szMax = sqlite3GlobalConfig.mxMemdbSize;
  }
  pFile->pStore = p;
  if( pOutFlags!=0 ){
    *pOutFlags = flags | SQLITE_OPEN_MEMORY;
  }
  pFd->pMethods = &memdb_io_methods;
  return SQLITE_OK;
}

/*
** Test for access permissions. Return true if the requested permission
** is available, or false otherwise.
**
** With memdb, no files ever exist on disk.  So always return false.
*/
static int memdbAccess(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int flags, 
  int *pResOut
){
  UNUSED_PARAMETER(pVfs);
  UNUSED_PARAMETER(zPath);
  UNUSED_PARAMETER(flags);
  *pResOut = 0;
  return SQLITE_OK;
}

/*
** Populate buffer zOut with the full canonical pathname corresponding
** to the pathname in zPath. zOut is guaranteed to point to a buffer
** of at least (INST_MAX_PATHNAME+1) bytes.
*/
static int memdbFullPathname(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int nOut, 
  char *zOut
){
  UNUSED_PARAMETER(pVfs);
  sqlite3_snprintf(nOut, zOut, "%s", zPath);
  return SQLITE_OK;
}

/*
** Open the dynamic library located at zPath and return a handle.
*/
static void *memdbDlOpen(sqlite3_vfs *pVfs, const char *zPath){
  return ORIGVFS(pVfs)->xDlOpen(ORIGVFS(pVfs), zPath);
}

/*
** Populate the buffer zErrMsg (size nByte bytes) with a human readable
** utf-8 string describing the most recent error encountered associated 
** with dynamic libraries.
*/
static void memdbDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg){
  ORIGVFS(pVfs)->xDlError(ORIGVFS(pVfs), nByte, zErrMsg);
}

/*
** Return a pointer to the symbol zSymbol in the dynamic library pHandle.
*/
static void (*memdbDlSym(sqlite3_vfs *pVfs, void *p, const char *zSym))(void){
  return ORIGVFS(pVfs)->xDlSym(ORIGVFS(pVfs), p, zSym);
}

/*
** Close the dynamic library handle pHandle.
*/
static void memdbDlClose(sqlite3_vfs *pVfs, void *pHandle){
  ORIGVFS(pVfs)->xDlClose(ORIGVFS(pVfs), pHandle);
}

/*
** Populate the buffer pointed to by zBufOut with nByte bytes of 
** random data.
*/
static int memdbRandomness(sqlite3_vfs *pVfs, int nByte, char *zBufOut){
  return ORIGVFS(pVfs)->xRandomness(ORIGVFS(pVfs), nByte, zBufOut);
}

/*
** Sleep for nMicro microseconds. Return the number of microseconds 
** actually slept.
*/
static int memdbSleep(sqlite3_vfs *pVfs, int nMicro){
  return ORIGVFS(pVfs)->xSleep(ORIGVFS(pVfs), nMicro);
}


static int memdbGetLastError(sqlite3_vfs *pVfs, int a, char *b){
  return ORIGVFS(pVfs)->xGetLastError(ORIGVFS(pVfs), a, b);
}
static int memdbCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *p){
  return ORIGVFS(pVfs)->xCurrentTimeInt64(ORIGVFS(pVfs), p);
}

/*
** Translate a database connection pointer and schema name into a
** MemFile pointer.
*/
static MemFile *memdbFromDbSchema(sqlite3 *db, const char *zSchema){
  MemFile *p = 0;
  MemStore *pStore;
  int rc = sqlite3_file_control(db, zSchema, SQLITE_FCNTL_FILE_POINTER, &p);
  if( rc ) return 0;
  if( p->base.pMethods!=&memdb_io_methods ) return 0;
  pStore = p->pStore;
  if( pStore->zFName!=0 ) p = 0;
  return p;
}

/*
** Return the serialization of a database
*/
unsigned char *sqlite3_serialize(
  sqlite3 *db,              /* The database connection */
  const char *zSchema,      /* Which database within the connection */
  sqlite3_int64 *piSize,    /* Write size here, if not NULL */
  unsigned int mFlags       /* Maybe SQLITE_SERIALIZE_NOCOPY */
){
  MemFile *p;
  int iDb;
  Btree *pBt;
  sqlite3_int64 sz;
  int szPage = 0;
  sqlite3_stmt *pStmt = 0;
  unsigned char *pOut = 0;
  char *zSql;
  int rc;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif

  if( zSchema==0 ) zSchema = db->aDb[0].zDbSName;
  p = memdbFromDbSchema(db, zSchema);
  iDb = sqlite3FindDbName(db, zSchema);
  if( piSize ) *piSize = -1;
  if( iDb<0 ) goto serialize_out;
  if( p ){
    MemStore *pStore = p->pStore;
    if( piSize ) *piSize = pStore->sz;
    if( mFlags & SQLITE_SERIALIZE_NOCOPY ){
      pOut = pStore->aData;
    }else{
      pOut = sqlite3_malloc64( pStore->sz );
      if( pOut ) memcpy(pOut, pStore->aData, pStore->sz);
    }
    goto serialize_out;
  }
  pBt = db->aDb[iDb].pBt;
  if( pBt==0 ) goto serialize_out;
  szPage = sqlite3BtreeGetPageSize(pBt);
  zSql = sqlite3_mprintf("PRAGMA \"%w\".page_count", zSchema);
  rc = zSql ? sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0) : SQLITE_NOMEM;
  sqlite3_free(zSql);
  if( rc ) goto serialize_out;
  rc = sqlite3_step(pStmt);
  if( rc==SQLITE_ROW ){
    sz = sqlite3_column_int64(pStmt, 0)*szPage;
    if( sz==0 ){
      sqlite3_reset(pStmt);
      sqlite3_exec(db, "BEGIN IMMEDIATE; COMMIT;", 0, 0, 0);
      rc = sqlite3_step(pStmt);
      if( rc==SQLITE_ROW ){
        sz = sqlite3_column_int64(pStmt, 0)*szPage;
      }
    }
    if( piSize ) *piSize = sz;
    if( mFlags & SQLITE_SERIALIZE_NOCOPY ){
      pOut = 0;
    }else{
      pOut = sqlite3_malloc64( sz );
      if( pOut ){
        int nPage = sqlite3_column_int(pStmt, 0);
        Pager *pPager = sqlite3BtreePager(pBt);
        int pgno;
        for(pgno=1; pgno<=nPage; pgno++){
          DbPage *pPage = 0;
          unsigned char *pTo = pOut + szPage*(sqlite3_int64)(pgno-1);
          rc = sqlite3PagerGet(pPager, pgno, (DbPage**)&pPage, 0);
          if( rc==SQLITE_OK ){
            memcpy(pTo, sqlite3PagerGetData(pPage), szPage);
          }else{
            memset(pTo, 0, szPage);
          }
          sqlite3PagerUnref(pPage);       
        }
      }
    }
  }
  sqlite3_finalize(pStmt);

 serialize_out:
  return pOut;
}

/* Convert zSchema to a MemDB and initialize its content.
*/
int sqlite3_deserialize(
  sqlite3 *db,            /* The database connection */
  const char *zSchema,    /* Which DB to reopen with the deserialization */
  unsigned char *pData,   /* The serialized database content */
  sqlite3_int64 szDb,     /* Number bytes in the deserialization */
  sqlite3_int64 szBuf,    /* Total size of buffer pData[] */
  unsigned mFlags         /* Zero or more SQLITE_DESERIALIZE_* flags */
){
  MemFile *p;
  char *zSql;
  sqlite3_stmt *pStmt = 0;
  int rc;
  int iDb;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  if( szDb<0 ) return SQLITE_MISUSE_BKPT;
  if( szBuf<0 ) return SQLITE_MISUSE_BKPT;
#endif

  if( zSchema==0 ) zSchema = db->aDb[0].zDbSName;
  iDb = sqlite3FindDbName(db, zSchema);
  if( iDb<2 && iDb!=0 ){
    rc = SQLITE_ERROR;
    goto end_deserialize;
  }
  zSql = sqlite3_mprintf("ATTACH x AS %Q", zSchema);
  if( zSql==0 ){
    rc = SQLITE_NOMEM;
  }else{
    rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
    sqlite3_free(zSql);
  }
  if( rc ) goto end_deserialize;
  db->init.iDb = (u8)iDb;
  db->init.reopenMemdb = 1;
  db->init.reopenMemdbReadonly = (mFlags & SQLITE_DESERIALIZE_READONLY)!=0;
  sqlite3_step(pStmt);
  db->init.reopenMemdb = 0;
  db->init.reopenMemdbReadonly = 0;
  rc = sqlite3_finalize(pStmt);
  if( rc!=SQLITE_OK ){
    goto end_deserialize;
  }
  p = memdbFromDbSchema(db, zSchema);
  if( p==0 ){
    rc = SQLITE_ERROR;
  }else{
    MemStore *pStore = p->pStore;
    pStore->aData = pData;
    pData = 0;
    pStore->sz = szDb;
    pStore->szAlloc = szBuf;
    pStore->szMax = szBuf;
    if( pStore->szMax<sqlite3GlobalConfig.mxMemdbSize ){
      pStore->szMax = sqlite3GlobalConfig.mxMemdbSize;
    }
    pStore->mFlags = mFlags;
    rc = SQLITE_OK;
  }

end_deserialize:
  if( pData && (mFlags & SQLITE_DESERIALIZE_FREEONCLOSE)!=0 ){
    sqlite3_free(pData);
  }
  return rc;
}

/*
** Return true if the VFS is the memvfs.
*/
int sqlite3IsMemdb(const sqlite3_vfs *pVfs){
  return pVfs==&memdb_vfs;
}

/* 
** This routine is called when the extension is loaded.
** Register the new VFS.
*/
int sqlite3MemdbInit(void){
  sqlite3_vfs *pLower = sqlite3_vfs_find(0);
  unsigned int sz;
  if( NEVER(pLower==0) ) return SQLITE_ERROR;
  sz = pLower->szOsFile;
  memdb_vfs.pAppData = pLower;
  /* Keep this size floor even though current builds do not reach it. */
  if( sz<sizeof(MemFile) ) sz = sizeof(MemFile); /*NO_TEST*/
  memdb_vfs.szOsFile = sz;
  return sqlite3_vfs_register(&memdb_vfs, 0);
}
#endif /* SQLITE_OMIT_DESERIALIZE */
