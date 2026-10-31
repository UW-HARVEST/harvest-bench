/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Main file for the SQLite library.  The routines in this file
** implement the programmer interface to the library.  Routines in
** other files are for internal use by SQLite and should not be
** accessed by users of the library.
*/
#include "sqliteInt.h"

#ifdef SQLITE_ENABLE_FTS3
# include "fts3.h"
#endif
#ifdef SQLITE_ENABLE_RTREE
# include "rtree.h"
#endif

/*
** This is an extension initializer that is a no-op and always succeeds.
*/
static int sqlite3TestExtInit(sqlite3 *db){
  (void)db;
  return SQLITE_OK;
}


/*
** Forward declarations of external module initializer functions
** for modules that need them.
*/
#ifdef SQLITE_ENABLE_FTS5
int sqlite3Fts5Init(sqlite3*);
#endif
#ifdef SQLITE_ENABLE_STMTVTAB
int sqlite3StmtVtabInit(sqlite3*);
#endif
#ifdef SQLITE_EXTRA_AUTOEXT
int SQLITE_EXTRA_AUTOEXT(sqlite3*);
#endif
/*
** An array of pointers to extension initializer functions for
** built-in extensions.
*/
static int (*const sqlite3BuiltinExtensions[])(sqlite3*) = {
#ifdef SQLITE_ENABLE_FTS3
  sqlite3Fts3Init,
#endif
#ifdef SQLITE_ENABLE_FTS5
  sqlite3Fts5Init,
#endif
#ifdef SQLITE_ENABLE_RTREE
  sqlite3RtreeInit,
#endif
#ifdef SQLITE_ENABLE_DBPAGE_VTAB
  sqlite3DbpageRegister,
#endif
#ifdef SQLITE_ENABLE_DBSTAT_VTAB
  sqlite3DbstatRegister,
#endif
  sqlite3TestExtInit,
#ifdef SQLITE_ENABLE_STMTVTAB
  sqlite3StmtVtabInit,
#endif
#ifdef SQLITE_ENABLE_BYTECODE_VTAB
  sqlite3VdbeBytecodeVtabInit,
#endif
#ifdef SQLITE_EXTRA_AUTOEXT
  SQLITE_EXTRA_AUTOEXT,
#endif
};

/* IMPLEMENTATION-OF: R-46656-45156 The sqlite3_version[] string constant
** contains the text of SQLITE_VERSION macro.
*/
const char sqlite3_version[] = SQLITE_VERSION;

/* IMPLEMENTATION-OF: R-53536-42575 The sqlite3_libversion() function returns
** a pointer to the to the sqlite3_version[] string constant.
*/
const char *sqlite3_libversion(void){ return sqlite3_version; }

/* IMPLEMENTATION-OF: R-25063-23286 The sqlite3_sourceid() function returns a
** pointer to a string constant whose value is the same as the
** SQLITE_SOURCE_ID C preprocessor macro. Except if SQLite is built using
** an edited copy of the amalgamation, then the last four characters of
** the hash might be different from SQLITE_SOURCE_ID.
*/
const char *sqlite3_sourceid(void){ return SQLITE_SOURCE_ID; }

/* IMPLEMENTATION-OF: R-35210-63508 The sqlite3_libversion_number() function
** returns an integer equal to SQLITE_VERSION_NUMBER.
*/
int sqlite3_libversion_number(void){ return SQLITE_VERSION_NUMBER; }

/*
** When compiling the test fixture or with debugging enabled,
** this variable being set to non-zero will cause OSTRACE macros to emit
** extra diagnostic information.
*/

#if !defined(SQLITE_OMIT_TRACE) && defined(SQLITE_ENABLE_IOTRACE)
/*
** If the following function pointer is not NULL and if
** SQLITE_ENABLE_IOTRACE is enabled, then messages describing
** I/O active are written using this function.  These messages
** are intended for debugging activity only.
*/
SQLITE_API void (*sqlite3IoTrace)(const char*, ...) = 0;
#endif

/*
** If the following global variable points to a string which is the
** name of a directory, then that directory will be used to store
** temporary files.
**
** See also the "PRAGMA temp_store_directory" SQL command.
*/
char *sqlite3_temp_directory = 0;

/*
** Initialize SQLite. 
**
** The sqlite3_initialize() routine must be called to initialize the
** memory allocation, and VFS prior to doing any
** serious work.  As long as you do not compile with SQLITE_OMIT_AUTOINIT
** this routine will be called automatically by key routines such as
** sqlite3_open(). 
**
** This routine is a no-op except on its very first call for the process,
** or for the first call after a call to sqlite3_shutdown.  Most calls
** to sqlite3_initialize() are, in fact, no-ops.  For that reason, the
** routine is broken into two pieces:
**
**     sqlite3Initialize()     Does the actual work of initialization
**
**     sqlite3_initialize()    Checks to see if initialization is needed
**                             and invokes sqlite3Initialize() if it is.
**
** The sqlite3_initialize() interface is called frequently, but
** sqlite3Initialize() runs rarely.  The function is broken up this way
** to avoid wasting CPU cycles with unnecessary stack setup for local
** variables in cases where it is not needed.
**
** The first caller to call sqlite3_initialize() runs the initialization to
** completion. If subsequent calls arrive before the first has finished the
** initialization process, then they must wait until initialization finishes.
**
** The first caller might call this routine recursively. Recursive
** calls to this routine should not block, of course.  Otherwise the
** initialization process would never complete.
**
** Nested calls to sqlite3_initialize() may occur while initialization is in
** progress. Recursive calls from the active initialization path return
** immediately.
*/
static SQLITE_NOINLINE int sqlite3Initialize(void){
  int rc;                                   /* Result code */
#ifdef SQLITE_EXTRA_INIT
  int bRunExtraInit = 0;                    /* Extra initialization needed */
#endif

  /* Make sure we're on a 64-bit machine */
  assert( SQLITE_PTRSIZE==sizeof(char*) );

  rc = SQLITE_OK;

  /* Initialize the malloc() system */
  if( !sqlite3GlobalConfig.isMallocInit ){
    rc = sqlite3MallocInit();
  }
  if( rc==SQLITE_OK ){
    sqlite3GlobalConfig.isMallocInit = 1;
  }

  if( rc!=SQLITE_OK ){
    return rc;
  }

  if( sqlite3GlobalConfig.isInit==0 && sqlite3GlobalConfig.inProgress==0 ){
    sqlite3GlobalConfig.inProgress = 1;
#ifdef SQLITE_ENABLE_SQLLOG
    {
      extern void sqlite3_init_sqllog(void);
      sqlite3_init_sqllog();
    }
#endif
    memset(&sqlite3BuiltinFunctions, 0, sizeof(sqlite3BuiltinFunctions));
    sqlite3RegisterBuiltinFunctions();
    if( sqlite3GlobalConfig.isPCacheInit==0 ){
      rc = sqlite3PcacheInitialize();
    }
    if( rc==SQLITE_OK ){
      sqlite3GlobalConfig.isPCacheInit = 1;
      rc = sqlite3OsInit();
    }
#ifndef SQLITE_OMIT_DESERIALIZE
    if( rc==SQLITE_OK ){
      rc = sqlite3MemdbInit();
    }
#endif
    if( rc==SQLITE_OK ){
      sqlite3PCacheBufferSetup( sqlite3GlobalConfig.pPage,
          sqlite3GlobalConfig.szPage, sqlite3GlobalConfig.nPage);
    }
    if( rc==SQLITE_OK ){
      sqlite3GlobalConfig.isInit = 1;
#ifdef SQLITE_EXTRA_INIT
      bRunExtraInit = 1;
#endif
    }
    sqlite3GlobalConfig.inProgress = 0;
  }

  /* The following is just a sanity check to make sure SQLite has
  ** been compiled correctly.  It is important to run this code, but
  ** we don't want to run it too often and soak up CPU cycles for no
  ** reason.  So we run it once during initialization.
  */
#ifndef NDEBUG
  /* This section of code's only "output" is via assert() statements. */
  if( rc==SQLITE_OK ){
    u64 x = (((u64)1)<<63)-1;
    double y;
    assert(sizeof(x)==8);
    assert(sizeof(x)==sizeof(y));
    memcpy(&y, &x, 8);
    assert( sqlite3IsNaN(y) );
  }
#endif

  /* Do extra initialization steps requested by the SQLITE_EXTRA_INIT
  ** compile-time option.
  */
#ifdef SQLITE_EXTRA_INIT
  if( bRunExtraInit ){
    int SQLITE_EXTRA_INIT(const char*);
    rc = SQLITE_EXTRA_INIT(0);
  }
#endif
  return rc;
}
SQLITE_API int sqlite3_initialize(void){
  if( sqlite3GlobalConfig.isInit ){
    /* SQLite has already been initialized.  Fast early-out. */
    return SQLITE_OK;
  }else{
    /* Invoke sqlite3Initialize() to do the actual work. */
    return sqlite3Initialize();
  }
}

/*
** Undo the effects of sqlite3_initialize().  Must not be called while
** there are outstanding database handles or memory allocations or while any
** part of SQLite is otherwise in use. This
** routine is not reentrant. But it is safe to invoke this routine
** on when SQLite is already shut down.  If SQLite is already shut down
** when this routine is invoked, then this routine is a harmless no-op.
*/
int sqlite3_shutdown(void){
  if( sqlite3GlobalConfig.isInit ){
#ifdef SQLITE_EXTRA_SHUTDOWN
    void SQLITE_EXTRA_SHUTDOWN(void);
    SQLITE_EXTRA_SHUTDOWN();
#endif
    sqlite3_os_end();
    sqlite3_reset_auto_extension();
    sqlite3GlobalConfig.isInit = 0;
  }
  if( sqlite3GlobalConfig.isPCacheInit ){
    sqlite3PcacheShutdown();
    sqlite3GlobalConfig.isPCacheInit = 0;
  }
  if( sqlite3GlobalConfig.isMallocInit ){
    sqlite3MallocEnd();
    sqlite3GlobalConfig.isMallocInit = 0;

#ifndef SQLITE_OMIT_SHUTDOWN_DIRECTORIES
    /* The heap subsystem has now been shutdown and these values are supposed
    ** to be NULL or point to memory that was obtained from sqlite3_malloc(),
    ** which would rely on that heap subsystem; therefore, make sure these
    ** values cannot refer to heap memory that was just invalidated when the
    ** heap subsystem was shutdown.  This is only done if the current call to
    ** this function resulted in the heap subsystem actually being shutdown.
    */
    sqlite3_temp_directory = 0;
#endif
  }

  return SQLITE_OK;
}

/*
** This API allows applications to modify the global configuration of
** the SQLite library at run-time.
**
** This routine should only be called when there are no outstanding
** database handles or memory allocations. Failure to heed these
** warnings can lead to unpredictable behavior.
*/
int sqlite3_config(int op, ...){
  va_list ap;
  int rc = SQLITE_OK;

  /* sqlite3_config() normally returns SQLITE_MISUSE if it is invoked while
  ** the SQLite library is in use.  Except, a few selected opcodes
  ** are allowed.
  */
  if( sqlite3GlobalConfig.isInit ){
    static const u64 mAnytimeConfigOption = 0
       | MASKBIT64( SQLITE_CONFIG_LOG )
       | MASKBIT64( SQLITE_CONFIG_PCACHE_HDRSZ )
    ;
    if( op<0 || op>63 || (MASKBIT64(op) & mAnytimeConfigOption)==0 ){
      return SQLITE_MISUSE_BKPT;
    }
  }

  va_start(ap, op);
  switch( op ){
    case SQLITE_CONFIG_MALLOC: {
      /* EVIDENCE-OF: R-55594-21030 The SQLITE_CONFIG_MALLOC option takes a
      ** single argument which is a pointer to an instance of the
      ** sqlite3_mem_methods structure. The argument specifies alternative
      ** low-level memory allocation routines to be used in place of the memory
      ** allocation routines built into SQLite. */
      sqlite3GlobalConfig.m = *va_arg(ap, sqlite3_mem_methods*);
      break;
    }
    case SQLITE_CONFIG_GETMALLOC: {
      /* EVIDENCE-OF: R-51213-46414 The SQLITE_CONFIG_GETMALLOC option takes a
      ** single argument which is a pointer to an instance of the
      ** sqlite3_mem_methods structure. The sqlite3_mem_methods structure is
      ** filled with the currently defined memory allocation routines. */
      if( sqlite3GlobalConfig.m.xMalloc==0 ) sqlite3MemSetDefault();
      *va_arg(ap, sqlite3_mem_methods*) = sqlite3GlobalConfig.m;
      break;
    }
    case SQLITE_CONFIG_MEMSTATUS: {
      assert( !sqlite3GlobalConfig.isInit );  /* Cannot change at runtime */
      /* EVIDENCE-OF: R-61275-35157 The SQLITE_CONFIG_MEMSTATUS option takes
      ** single argument of type int, interpreted as a boolean, which enables
      ** or disables the collection of memory allocation statistics. */
      sqlite3GlobalConfig.bMemstat = va_arg(ap, int);
      break;
    }
    case SQLITE_CONFIG_SMALL_MALLOC: {
      sqlite3GlobalConfig.bSmallMalloc = va_arg(ap, int)!=0;
      break;
    }
    case SQLITE_CONFIG_PAGECACHE: {
      /* EVIDENCE-OF: R-18761-36601 There are three arguments to
      ** SQLITE_CONFIG_PAGECACHE: A pointer to 8-byte aligned memory (pMem),
      ** the size of each page cache line (sz), and the number of cache lines
      ** (N). */
      sqlite3GlobalConfig.pPage = va_arg(ap, void*);
      sqlite3GlobalConfig.szPage = va_arg(ap, int);
      sqlite3GlobalConfig.nPage = va_arg(ap, int);
      break;
    }
    case SQLITE_CONFIG_PCACHE_HDRSZ: {
      /* EVIDENCE-OF: R-39100-27317 The SQLITE_CONFIG_PCACHE_HDRSZ option takes
      ** a single parameter which is a pointer to an integer and writes into
      ** that integer the number of extra bytes per page required for each page
      ** in SQLITE_CONFIG_PAGECACHE. */
      *va_arg(ap, int*) =
          sqlite3HeaderSizeBtree() +
          sqlite3HeaderSizePcache() +
          sqlite3HeaderSizePcache1();
      break;
    }

    case SQLITE_CONFIG_PCACHE: {
      /* no-op */
      break;
    }
    case SQLITE_CONFIG_GETPCACHE: {
      /* now an error */
      rc = SQLITE_ERROR;
      break;
    }

    case SQLITE_CONFIG_PCACHE2: {
      /* EVIDENCE-OF: R-63325-48378 The SQLITE_CONFIG_PCACHE2 option takes a
      ** single argument which is a pointer to an sqlite3_pcache_methods2
      ** object. This object specifies the interface to a custom page cache
      ** implementation. */
      sqlite3GlobalConfig.pcache2 = *va_arg(ap, sqlite3_pcache_methods2*);
      break;
    }
    case SQLITE_CONFIG_GETPCACHE2: {
      /* EVIDENCE-OF: R-22035-46182 The SQLITE_CONFIG_GETPCACHE2 option takes a
      ** single argument which is a pointer to an sqlite3_pcache_methods2
      ** object. SQLite copies of the current page cache implementation into
      ** that object. */
      if( sqlite3GlobalConfig.pcache2.xInit==0 ){
        sqlite3PCacheSetDefault();
      }
      *va_arg(ap, sqlite3_pcache_methods2*) = sqlite3GlobalConfig.pcache2;
      break;
    }

/* EVIDENCE-OF: R-06626-12911 The SQLITE_CONFIG_HEAP option is only
** available if SQLite is compiled with SQLITE_ENABLE_MEMSYS5 and returns
** SQLITE_ERROR if invoked otherwise. */
#if defined(SQLITE_ENABLE_MEMSYS5)
    case SQLITE_CONFIG_HEAP: {
      /* EVIDENCE-OF: R-19854-42126 There are three arguments to
      ** SQLITE_CONFIG_HEAP: An 8-byte aligned pointer to the memory, the
      ** number of bytes in the memory buffer, and the minimum allocation size.
      */
      sqlite3GlobalConfig.pHeap = va_arg(ap, void*);
      sqlite3GlobalConfig.nHeap = va_arg(ap, int);
      sqlite3GlobalConfig.mnReq = va_arg(ap, int);

      if( sqlite3GlobalConfig.mnReq<1 ){
        sqlite3GlobalConfig.mnReq = 1;
      }else if( sqlite3GlobalConfig.mnReq>(1<<12) ){
        /* cap min request size at 2^12 */
        sqlite3GlobalConfig.mnReq = (1<<12);
      }

      if( sqlite3GlobalConfig.pHeap==0 ){
        /* EVIDENCE-OF: R-49920-60189 If the first pointer (the memory pointer)
        ** is NULL, then SQLite reverts to using its default memory allocator
        ** (the system malloc() implementation), undoing any prior invocation of
        ** SQLITE_CONFIG_MALLOC.
        **
        ** Setting sqlite3GlobalConfig.m to all zeros will cause malloc to
        ** revert to its default implementation when sqlite3_initialize() is run
        */
        memset(&sqlite3GlobalConfig.m, 0, sizeof(sqlite3GlobalConfig.m));
      }else{
        /* EVIDENCE-OF: R-61006-08918 If the memory pointer is not NULL then the
        ** alternative memory allocator is engaged to handle all of SQLites
        ** memory allocation needs. */
#ifdef SQLITE_ENABLE_MEMSYS5
        sqlite3GlobalConfig.m = *sqlite3MemGetMemsys5();
#endif
      }
      break;
    }
#endif

    case SQLITE_CONFIG_LOOKASIDE: {
      sqlite3GlobalConfig.szLookaside = va_arg(ap, int);
      sqlite3GlobalConfig.nLookaside = va_arg(ap, int);
      break;
    }
   
    /* Record a pointer to the logger function and its first argument.
    ** The default is NULL.  Logging is disabled if the function pointer is
    ** NULL.
    */
    case SQLITE_CONFIG_LOG: {
      /* Use a typedef when extracting function pointers from va_arg().
      ** sqlite3GlobalConfig.xLog = va_arg(ap, void(*)(void*,int,const char*));
      */
      typedef void(*LOGFUNC_t)(void*,int,const char*);
      LOGFUNC_t xLog = va_arg(ap, LOGFUNC_t);
      void *pLogArg = va_arg(ap, void*);
      sqlite3GlobalConfig.xLog = xLog;
      sqlite3GlobalConfig.pLogArg = pLogArg;
      break;
    }

    /* EVIDENCE-OF: R-55548-33817 The compile-time setting for URI filenames
    ** can be changed at start-time using the
    ** sqlite3_config(SQLITE_CONFIG_URI,1) or
    ** sqlite3_config(SQLITE_CONFIG_URI,0) configuration calls.
    */
    case SQLITE_CONFIG_URI: {
      /* EVIDENCE-OF: R-25451-61125 The SQLITE_CONFIG_URI option takes a single
      ** argument of type int. If non-zero, then URI handling is globally
      ** enabled. If the parameter is zero, then URI handling is globally
      ** disabled. */
      int bOpenUri = va_arg(ap, int);
      sqlite3GlobalConfig.bOpenUri = bOpenUri;
      break;
    }

#ifdef SQLITE_ENABLE_SQLLOG
    case SQLITE_CONFIG_SQLLOG: {
      typedef void(*SQLLOGFUNC_t)(void*, sqlite3*, const char*, int);
      sqlite3GlobalConfig.xSqllog = va_arg(ap, SQLLOGFUNC_t);
      sqlite3GlobalConfig.pSqllogArg = va_arg(ap, void *);
      break;
    }
#endif

    case SQLITE_CONFIG_PMASZ: {
      sqlite3GlobalConfig.szPma = va_arg(ap, unsigned int);
      break;
    }

    case SQLITE_CONFIG_STMTJRNL_SPILL: {
      sqlite3GlobalConfig.nStmtSpill = va_arg(ap, int);
      break;
    }

#ifdef SQLITE_ENABLE_SORTER_REFERENCES
    case SQLITE_CONFIG_SORTERREF_SIZE: {
      int iVal = va_arg(ap, int);
      if( iVal<0 ){
        iVal = SQLITE_DEFAULT_SORTERREF_SIZE;
      }
      sqlite3GlobalConfig.szSorterRef = (u32)iVal;
      break;
    }
#endif /* SQLITE_ENABLE_SORTER_REFERENCES */

#ifndef SQLITE_OMIT_DESERIALIZE
    case SQLITE_CONFIG_MEMDB_MAXSIZE: {
      sqlite3GlobalConfig.mxMemdbSize = va_arg(ap, sqlite3_int64);
      break;
    }
#endif /* SQLITE_OMIT_DESERIALIZE */

    case SQLITE_CONFIG_ROWID_IN_VIEW: {
      int *pVal = va_arg(ap,int*);
#ifdef SQLITE_ALLOW_ROWID_IN_VIEW
      if( 0==*pVal ) sqlite3GlobalConfig.mNoVisibleRowid = TF_NoVisibleRowid;
      if( 1==*pVal ) sqlite3GlobalConfig.mNoVisibleRowid = 0;
      *pVal = (sqlite3GlobalConfig.mNoVisibleRowid==0);
#else
      *pVal = 0;
#endif
      break;
    }

    default: {
      rc = SQLITE_ERROR;
      break;
    }
  }
  va_end(ap);
  return rc;
}

/*
** Set up the lookaside buffers for a database connection.
** Return SQLITE_OK on success. 
** If lookaside is already active, return SQLITE_UNAVAILABLE.
**
** The sz parameter is the number of bytes in each lookaside slot.
** The cnt parameter is the number of slots.  If pBuf is NULL the
** space for the lookaside memory is obtained from sqlite3_malloc()
** or similar.  If pBuf is not NULL then it is sz*cnt bytes of memory
** to use for the lookaside memory.
*/
static int setupLookaside(
  sqlite3 *db,    /* Database connection being configured */
  void *pBuf,     /* Memory to use for lookaside.  May be NULL */
  int sz,         /* Desired size of each lookaside memory slot */
  int cnt         /* Number of slots to allocate */
){
#ifndef SQLITE_OMIT_LOOKASIDE
  void *pStart;          /* Start of the lookaside buffer */
  sqlite3_int64 szAlloc; /* Total space set aside for lookaside memory */
  int nBig;              /* Number of full-size slots */
  int nSm;               /* Number smaller LOOKASIDE_SMALL-byte slots */
 
  if( sqlite3LookasideUsed(db,0)>0 ){
    return SQLITE_UNAVAILABLE;
  }
  /* Free any existing lookaside buffer for this handle before
  ** allocating a new one so we don't have to have space for
  ** both at the same time.
  */
  if( db->lookaside.bMalloced ){
    sqlite3_free(db->lookaside.pStart);
  }
  /* The size of a lookaside slot after ROUNDDOWN8 needs to be larger
  ** than a pointer and small enough to fit in a u16.
  */
  sz = ROUNDDOWN8(sz);
  if( sz<=(int)sizeof(LookasideSlot*) ) sz = 0;
  if( sz>65528 ) sz = 65528;
  /* Count must be at least 1 to be useful, but not so large as to use
  ** more than 0x7fff0000 total bytes for lookaside. */
  if( cnt<1 ) cnt = 0;
  if( sz>0 && cnt>(0x7fff0000/sz) ) cnt = 0x7fff0000/sz;
  szAlloc = (i64)sz*(i64)cnt;
  if( szAlloc==0 ){
    sz = 0;
    pStart = 0;
  }else if( pBuf==0 ){
    pStart = sqlite3Malloc( szAlloc );
    if( pStart ) szAlloc = sqlite3MallocSize(pStart);
  }else{
    pStart = pBuf;
  }
#ifndef SQLITE_OMIT_TWOSIZE_LOOKASIDE
  if( sz>=LOOKASIDE_SMALL*3 ){
    nBig = szAlloc/(3*LOOKASIDE_SMALL+sz);
    nSm = (szAlloc - (i64)sz*(i64)nBig)/LOOKASIDE_SMALL;
  }else if( sz>=LOOKASIDE_SMALL*2 ){
    nBig = szAlloc/(LOOKASIDE_SMALL+sz);
    nSm = (szAlloc - (i64)sz*(i64)nBig)/LOOKASIDE_SMALL;
  }else
#endif /* SQLITE_OMIT_TWOSIZE_LOOKASIDE */
  if( sz>0 ){
    nBig = szAlloc/sz;
    nSm = 0;
  }else{
    nBig = nSm = 0;
  }
  db->lookaside.pStart = pStart;
  db->lookaside.pInit = 0;
  db->lookaside.pFree = 0;
  db->lookaside.sz = (u16)sz;
  db->lookaside.szTrue = (u16)sz;
  if( pStart ){
    int i;
    LookasideSlot *p;
    assert( sz > (int)sizeof(LookasideSlot*) );
    p = (LookasideSlot*)pStart;
    for(i=0; i<nBig; i++){
      p->pNext = db->lookaside.pInit;
      db->lookaside.pInit = p;
      p = (LookasideSlot*)&((u8*)p)[sz];
    }
#ifndef SQLITE_OMIT_TWOSIZE_LOOKASIDE
    db->lookaside.pSmallInit = 0;
    db->lookaside.pSmallFree = 0;
    db->lookaside.pMiddle = p;
    for(i=0; i<nSm; i++){
      p->pNext = db->lookaside.pSmallInit;
      db->lookaside.pSmallInit = p;
      p = (LookasideSlot*)&((u8*)p)[LOOKASIDE_SMALL];
    }
#endif /* SQLITE_OMIT_TWOSIZE_LOOKASIDE */
    assert( ((uptr)p)<=szAlloc + (uptr)pStart );
    db->lookaside.pEnd = p;
    db->lookaside.bDisable = 0;
    db->lookaside.bMalloced = pBuf==0 ?1:0;
    db->lookaside.nSlot = nBig+nSm;
  }else{
    db->lookaside.pStart = 0;
#ifndef SQLITE_OMIT_TWOSIZE_LOOKASIDE
    db->lookaside.pSmallInit = 0;
    db->lookaside.pSmallFree = 0;
    db->lookaside.pMiddle = 0;
#endif /* SQLITE_OMIT_TWOSIZE_LOOKASIDE */
    db->lookaside.pEnd = 0;
    db->lookaside.bDisable = 1;
    db->lookaside.sz = 0;
    db->lookaside.bMalloced = 0;
    db->lookaside.nSlot = 0;
  }
  db->lookaside.pTrueEnd = db->lookaside.pEnd;
  assert( sqlite3LookasideUsed(db,0)==0 );
#endif /* SQLITE_OMIT_LOOKASIDE */
  return SQLITE_OK;
}

/*
** Flush any dirty pages in the pager-cache for any attached database
** to disk.
*/
int sqlite3_db_cacheflush(sqlite3 *db){
  int i;
  int rc = SQLITE_OK;
  int bSeenBusy = 0;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  for(i=0; rc==SQLITE_OK && i<db->nDb; i++){
    Btree *pBt = db->aDb[i].pBt;
    if( pBt && sqlite3BtreeTxnState(pBt)==SQLITE_TXN_WRITE ){
      Pager *pPager = sqlite3BtreePager(pBt);
      rc = sqlite3PagerFlush(pPager);
      if( rc==SQLITE_UNAVAILABLE ){
        bSeenBusy = 1;
        rc = SQLITE_OK;
      }
    }
  }
  return ((rc==SQLITE_OK && bSeenBusy) ? SQLITE_UNAVAILABLE : rc);
}

/*
** Configuration settings for an individual database connection
*/
int sqlite3_db_config(sqlite3 *db, int op, ...){
  va_list ap;
  int rc;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  va_start(ap, op);
  switch( op ){
    case SQLITE_DBCONFIG_MAINDBNAME: {
      /* IMP: R-06824-28531 */
      /* IMP: R-36257-52125 */
      db->aDb[0].zDbSName = va_arg(ap,char*);
      rc = SQLITE_OK;
      break;
    }
    case SQLITE_DBCONFIG_LOOKASIDE: {
      void *pBuf = va_arg(ap, void*); /* IMP: R-26835-10964 */
      int sz = va_arg(ap, int);       /* IMP: R-47871-25994 */
      int cnt = va_arg(ap, int);      /* IMP: R-04460-53386 */
      rc = setupLookaside(db, pBuf, sz, cnt);
      break;
    }
    case SQLITE_DBCONFIG_FP_DIGITS: {
      int nIn = va_arg(ap, int);
      int *pOut = va_arg(ap, int*);
      if( nIn>3 && nIn<24 ) db->nFpDigit = (u8)nIn;
      if( pOut ) *pOut = db->nFpDigit;
      rc = SQLITE_OK;
      break;
    }
    default: {
      static const struct {
        int op;      /* The opcode */
        u64 mask;    /* Mask of the bit in sqlite3.flags to set/clear */
      } aFlagOp[] = {
        { SQLITE_DBCONFIG_ENABLE_FKEY,           SQLITE_ForeignKeys    },
        { SQLITE_DBCONFIG_ENABLE_TRIGGER,        SQLITE_EnableTrigger  },
        { SQLITE_DBCONFIG_ENABLE_VIEW,           SQLITE_EnableView     },
        { SQLITE_DBCONFIG_ENABLE_FTS3_TOKENIZER, SQLITE_Fts3Tokenizer  },
        { SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, SQLITE_LoadExtension  },
        { SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE,      SQLITE_NoCkptOnClose  },
        { SQLITE_DBCONFIG_ENABLE_QPSG,           SQLITE_EnableQPSG     },
        { SQLITE_DBCONFIG_TRIGGER_EQP,           SQLITE_TriggerEQP     },
        { SQLITE_DBCONFIG_RESET_DATABASE,        SQLITE_ResetDatabase  },
        { SQLITE_DBCONFIG_DEFENSIVE,             SQLITE_Defensive      },
        { SQLITE_DBCONFIG_WRITABLE_SCHEMA,       SQLITE_WriteSchema|
                                                 SQLITE_NoSchemaError  },
        { SQLITE_DBCONFIG_LEGACY_ALTER_TABLE,    SQLITE_LegacyAlter    },
        { SQLITE_DBCONFIG_DQS_DDL,               SQLITE_DqsDDL         },
        { SQLITE_DBCONFIG_DQS_DML,               SQLITE_DqsDML         },
        { SQLITE_DBCONFIG_LEGACY_FILE_FORMAT,    SQLITE_LegacyFileFmt  },
        { SQLITE_DBCONFIG_TRUSTED_SCHEMA,        SQLITE_TrustedSchema  },
        { SQLITE_DBCONFIG_STMT_SCANSTATUS,       SQLITE_StmtScanStatus },
        { SQLITE_DBCONFIG_REVERSE_SCANORDER,     SQLITE_ReverseOrder   },
        { SQLITE_DBCONFIG_ENABLE_ATTACH_CREATE,  SQLITE_AttachCreate   },
        { SQLITE_DBCONFIG_ENABLE_ATTACH_WRITE,   SQLITE_AttachWrite    },
        { SQLITE_DBCONFIG_ENABLE_COMMENTS,       SQLITE_Comments       },
      };
      unsigned int i;
      rc = SQLITE_ERROR; /* IMP: R-42790-23372 */
      for(i=0; i<ArraySize(aFlagOp); i++){
        if( aFlagOp[i].op==op ){
          int onoff = va_arg(ap, int);
          int *pRes = va_arg(ap, int*);
          u64 oldFlags = db->flags;
          if( onoff>0 ){
            db->flags |= aFlagOp[i].mask;
          }else if( onoff==0 ){
            db->flags &= ~(u64)aFlagOp[i].mask;
          }
          if( oldFlags!=db->flags ){
            sqlite3ExpirePreparedStatements(db, 0);
          }
          if( pRes ){
            *pRes = (db->flags & aFlagOp[i].mask)!=0;
          }
          rc = SQLITE_OK;
          break;
        }
      }
      break;
    }
  }
  va_end(ap);
  return rc;
}

/*
** This is the default collating function named "BINARY" which is always
** available.
*/
static int binCollFunc(
  void *NotUsed,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  int rc, n;
  UNUSED_PARAMETER(NotUsed);
  n = nKey1<nKey2 ? nKey1 : nKey2;
  /* EVIDENCE-OF: R-65033-28449 The built-in BINARY collation compares
  ** strings byte by byte using the memcmp() function from the standard C
  ** library. */
  assert( pKey1 && pKey2 );
  rc = memcmp(pKey1, pKey2, n);
  if( rc==0 ){
    rc = nKey1 - nKey2;
  }
  return rc;
}

/*
** This is the collating function named "RTRIM" which is always
** available.  Ignore trailing spaces.
*/
static int rtrimCollFunc(
  void *pUser,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  const u8 *pK1 = (const u8*)pKey1;
  const u8 *pK2 = (const u8*)pKey2;
  while( nKey1 && pK1[nKey1-1]==' ' ) nKey1--;
  while( nKey2 && pK2[nKey2-1]==' ' ) nKey2--;
  return binCollFunc(pUser, nKey1, pKey1, nKey2, pKey2);
}

/*
** Return true if CollSeq is the default built-in BINARY.
*/
int sqlite3IsBinary(const CollSeq *p){
  assert( p==0 || p->xCmp!=binCollFunc || strcmp(p->zName,"BINARY")==0 );
  return p==0 || p->xCmp==binCollFunc;
}

/*
** Another built-in collating sequence: NOCASE.
**
** This collating sequence is intended to be used for "case independent
** comparison". SQLite's knowledge of upper and lower case equivalents
** extends only to the 26 characters used in the English language.
**
** At the moment there is only a UTF-8 implementation.
*/
static int nocaseCollatingFunc(
  void *NotUsed,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  int r = sqlite3StrNICmp(
      (const char *)pKey1, (const char *)pKey2, (nKey1<nKey2)?nKey1:nKey2);
  UNUSED_PARAMETER(NotUsed);
  if( 0==r ){
    r = nKey1-nKey2;
  }
  return r;
}

/*
** Return the ROWID of the most recent insert
*/
sqlite_int64 sqlite3_last_insert_rowid(sqlite3 *db){
  i64 iRet;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  iRet = db->lastRowid;
  return iRet;
}

/*
** Set the value returned by the sqlite3_last_insert_rowid() API function.
*/
void sqlite3_set_last_insert_rowid(sqlite3 *db, sqlite3_int64 iRowid){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return;
  }
#endif
  db->lastRowid = iRowid;
}

/*
** Return the number of changes in the most recently executed DML
** statement.
*/
sqlite3_int64 sqlite3_changes64(sqlite3 *db){
  i64 iRet;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  iRet = db->nChange;
  return iRet;
}
int sqlite3_changes(sqlite3 *db){
  return (int)sqlite3_changes64(db);
}

/*
** Return the number of changes since the database handle was opened.
*/
sqlite3_int64 sqlite3_total_changes64(sqlite3 *db){
  i64 iRet;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  iRet = db->nTotalChange;
  return iRet;
}
int sqlite3_total_changes(sqlite3 *db){
  return (int)sqlite3_total_changes64(db);
}

/*
** Close all open savepoints. This function only manipulates fields of the
** database handle object, it does not close any savepoints that may be open
** at the b-tree/pager level.
*/
void sqlite3CloseSavepoints(sqlite3 *db){
  while( db->pSavepoint ){
    Savepoint *pTmp = db->pSavepoint;
    db->pSavepoint = pTmp->pNext;
    sqlite3DbFree(db, pTmp);
  }
  db->nSavepoint = 0;
  db->nStatement = 0;
  db->isTransactionSavepoint = 0;
}

/*
** Invoke the destructor function associated with FuncDef p, if any. Except,
** if this is not the last copy of the function, do not invoke it. Multiple
** copies of a single function are created when create_function() is called
** with SQLITE_ANY as the encoding.
*/
static void functionDestroy(sqlite3 *db, FuncDef *p){
  FuncDestructor *pDestructor;
  assert( (p->funcFlags & SQLITE_FUNC_BUILTIN)==0 );
  pDestructor = p->u.pDestructor;
  if( pDestructor ){
    pDestructor->nRef--;
    if( pDestructor->nRef==0 ){
      pDestructor->xDestroy(pDestructor->pUserData);
      sqlite3DbFree(db, pDestructor);
    }
  }
}

/*
** Disconnect all sqlite3_vtab objects that belong to database connection
** db. This is called when db is being closed.
*/
static void disconnectAllVtab(sqlite3 *db){
  int i;
  HashElem *p;
  for(i=0; i<db->nDb; i++){
    Schema *pSchema = db->aDb[i].pSchema;
    if( pSchema ){
      for(p=sqliteHashFirst(&pSchema->tblHash); p; p=sqliteHashNext(p)){
        Table *pTab = (Table *)sqliteHashData(p);
        if( IsVirtual(pTab) ) sqlite3VtabDisconnect(db, pTab);
      }
    }
  }
  for(p=sqliteHashFirst(&db->aModule); p; p=sqliteHashNext(p)){
    Module *pMod = (Module *)sqliteHashData(p);
    if( pMod->pEpoTab ){
      sqlite3VtabDisconnect(db, pMod->pEpoTab);
    }
  }
  sqlite3VtabCleanupList(db);
}

/*
** Return TRUE if database connection db has unfinalized prepared
** statements or unfinished sqlite3_backup objects. 
*/
static int connectionIsBusy(sqlite3 *db){
  int j;
  if( db->pVdbe ) return 1;
  for(j=0; j<db->nDb; j++){
    Btree *pBt = db->aDb[j].pBt;
    if( pBt && sqlite3BtreeIsInBackup(pBt) ) return 1;
  }
  return 0;
}

/*
** Close an existing SQLite database
*/
static int sqlite3Close(sqlite3 *db, int forceZombie){
  if( !db ){
    /* EVIDENCE-OF: R-63257-11740 Calling sqlite3_close() or
    ** sqlite3_close_v2() with a NULL pointer argument is a harmless no-op. */
    return SQLITE_OK;
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  if( db->mTrace & SQLITE_TRACE_CLOSE ){
    db->trace.xV2(SQLITE_TRACE_CLOSE, db->pTraceArg, db, 0);
  }

  /* Force xDisconnect calls on all virtual tables */
  disconnectAllVtab(db);

  /* If a transaction is open, the disconnectAllVtab() call above
  ** will not have called the xDisconnect() method on any virtual
  ** tables in the db->aVTrans[] array. The following sqlite3VtabRollback()
  ** call will do so. We need to do this before the check for active
  ** SQL statements below, as the v-table implementation may be storing
  ** some prepared statements internally.
  */
  sqlite3VtabRollback(db);

  /* Legacy behavior (sqlite3_close() behavior) is to return
  ** SQLITE_UNAVAILABLE if the connection can not be closed immediately.
  */
  if( !forceZombie && connectionIsBusy(db) ){
    sqlite3ErrorWithMsg(db, SQLITE_UNAVAILABLE, "unable to close due to unfinalized "
       "statements or unfinished backups");
    return SQLITE_UNAVAILABLE;
  }

#ifdef SQLITE_ENABLE_SQLLOG
  if( sqlite3GlobalConfig.xSqllog ){
    /* Closing the handle. Fourth parameter is passed the value 2. */
    sqlite3GlobalConfig.xSqllog(sqlite3GlobalConfig.pSqllogArg, db, 0, 2);
  }
#endif

  while( db->pDbData ){
    DbClientData *p = db->pDbData;
    db->pDbData = p->pNext;
    assert( p->pData!=0 );
    if( p->xDestructor ) p->xDestructor(p->pData);
    sqlite3_free(p);
  }

  /* Convert the connection into a zombie and then close it.
  */
  db->eOpenState = SQLITE_STATE_ZOMBIE;
  sqlite3CloseZombie(db);
  return SQLITE_OK;
}

/*
** Return the transaction state for a single databse, or the maximum
** transaction state over all attached databases if zSchema is null.
*/
int sqlite3_txn_state(sqlite3 *db, const char *zSchema){
  int iDb, nDb;
  int iTxn = -1;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return -1;
  }
#endif
  if( zSchema ){
    nDb = iDb = sqlite3FindDbName(db, zSchema);
    if( iDb<0 ) nDb--;
  }else{
    iDb = 0;
    nDb = db->nDb-1;
  }
  for(; iDb<=nDb; iDb++){
    Btree *pBt = db->aDb[iDb].pBt;
    int x = pBt!=0 ? sqlite3BtreeTxnState(pBt) : SQLITE_TXN_NONE;
    if( x>iTxn ) iTxn = x;
  }
  return iTxn;
}

/*
** Two variations on the public interface for closing a database
** connection. The sqlite3_close() version returns SQLITE_UNAVAILABLE and
** leaves the connection open if there are unfinalized prepared
** statements or unfinished sqlite3_backups.  The sqlite3_close_v2()
** version forces the connection to become a zombie if there are
** unclosed resources, and arranges for deallocation when the last
** prepare statement or sqlite3_backup closes.
*/
int sqlite3_close(sqlite3 *db){ return sqlite3Close(db,0); }
int sqlite3_close_v2(sqlite3 *db){ return sqlite3Close(db,1); }


/*
** If database connection db is a zombie (meaning that there
** has been a prior call to sqlite3_close(db) or sqlite3_close_v2(db)) and
** every sqlite3_stmt has now been finalized and every sqlite3_backup has
** finished, then free all resources.
*/
void sqlite3CloseZombie(sqlite3 *db){
  HashElem *i;                    /* Hash table iterator */
  int j;

  /* If we reach this point, it means that the database connection has
  ** closed all sqlite3_stmt and sqlite3_backup objects and has been
  ** passed to sqlite3_close (meaning that it is a zombie).  Therefore,
  ** go ahead and free all resources.
  */

  /* If a transaction is open, roll it back. This also ensures that if
  ** any database schemas have been modified by an uncommitted transaction
  ** they are reset. 
  */
  sqlite3RollbackAll(db, SQLITE_OK);

  /* Free any outstanding Savepoint structures. */
  sqlite3CloseSavepoints(db);

  /* Close all database backends */
  for(j=0; j<db->nDb; j++){
    struct Db *pDb = &db->aDb[j];
    if( pDb->pBt ){
      sqlite3BtreeClose(pDb->pBt);
      pDb->pBt = 0;
      if( j!=1 ){
        pDb->pSchema = 0;
      }
    }
  }
  /* Clear the TEMP schema separately and last */
  if( db->aDb[1].pSchema ){
    sqlite3SchemaClear(db->aDb[1].pSchema);
    assert( db->aDb[1].pSchema->trigHash.count==0 );
  }
  sqlite3VtabCleanupList(db);

  /* Free up the array of auxiliary databases */
  sqlite3CollapseDatabaseArray(db);
  assert( db->nDb<=2 );
  assert( db->aDb==db->aDbStatic );

  for(i=sqliteHashFirst(&db->aFunc); i; i=sqliteHashNext(i)){
    FuncDef *pNext, *p;
    p = sqliteHashData(i);
    do{
      functionDestroy(db, p);
      pNext = p->pNext;
      sqlite3DbFree(db, p);
      p = pNext;
    }while( p );
  }
  sqlite3HashClear(&db->aFunc);
  for(i=sqliteHashFirst(&db->aCollSeq); i; i=sqliteHashNext(i)){
    CollSeq *pColl = (CollSeq *)sqliteHashData(i);
    /* Invoke any destructors registered for collation sequence user data. */
    for(j=0; j<3; j++){
      if( pColl[j].xDel ){
        pColl[j].xDel(pColl[j].pUser);
      }
    }
    sqlite3DbFree(db, pColl);
  }
  sqlite3HashClear(&db->aCollSeq);
  for(i=sqliteHashFirst(&db->aModule); i; i=sqliteHashNext(i)){
    Module *pMod = (Module *)sqliteHashData(i);
    sqlite3VtabEponymousTableClear(db, pMod);
    sqlite3VtabModuleUnref(db, pMod);
  }
  sqlite3HashClear(&db->aModule);

  sqlite3Error(db, SQLITE_OK); /* Deallocates any cached error strings. */
  sqlite3ValueFree(db->pErr);
  sqlite3CloseExtensions(db);

  db->eOpenState = SQLITE_STATE_ERROR;

  /* The temp-database schema is allocated differently from the other schema
  ** objects (using sqliteMalloc() directly, instead of sqlite3BtreeSchema()).
  ** So it needs to be freed here. Todo: Why not roll the temp schema into
  ** the same sqliteMalloc() as the one that allocates the database
  ** structure?
  */
  sqlite3DbFree(db, db->aDb[1].pSchema);
  if( db->xAutovacDestr ){
    db->xAutovacDestr(db->pAutovacPagesArg);
  }
  db->eOpenState = SQLITE_STATE_CLOSED;
  assert( sqlite3LookasideUsed(db,0)==0 );
  if( db->lookaside.bMalloced ){
    sqlite3_free(db->lookaside.pStart);
  }
  sqlite3_free(db);
}

/*
** Rollback all database files.  If tripCode is not SQLITE_OK, then
** any write cursors are invalidated ("tripped" - as in "tripping a circuit
** breaker") and made to return tripCode if there are any further
** attempts to use that cursor.  Read cursors remain open and valid
** but are "saved" in case the table pages are moved around.
*/
void sqlite3RollbackAll(sqlite3 *db, int tripCode){
  int i;
  int inTrans = 0;
  int schemaChange;

  schemaChange = (db->mDbFlags & DBFLAG_SchemaChange)!=0 && db->init.busy==0;

  for(i=0; i<db->nDb; i++){
    Btree *p = db->aDb[i].pBt;
    if( p ){
      if( sqlite3BtreeTxnState(p)==SQLITE_TXN_WRITE ){
        inTrans = 1;
      }
      sqlite3BtreeRollback(p, tripCode, !schemaChange);
    }
  }
  sqlite3VtabRollback(db);

  if( schemaChange ){
    sqlite3ExpirePreparedStatements(db, 0);
    sqlite3ResetAllSchemasOfConnection(db);
  }
  /* Any deferred constraint violations have now been resolved. */
  db->nDeferredCons = 0;
  db->nDeferredImmCons = 0;
  db->flags &= ~(u64)(SQLITE_DeferFKs|SQLITE_CorruptRdOnly);

  /* If one has been configured, invoke the rollback-hook callback */
  if( db->xRollbackCallback && (inTrans || !db->autoCommit) ){
    db->xRollbackCallback(db->pRollbackArg);
  }
}

/*
** Return a static string that describes the kind of error specified in the
** argument.
*/
const char *sqlite3ErrStr(int rc){
  static const char* const aMsg[] = {
    /* SQLITE_OK          */ "not an error",
    /* SQLITE_ERROR       */ "SQL logic error",
    /* SQLITE_INTERNAL    */ 0,
    /* SQLITE_PERM        */ "access permission denied",
    /* SQLITE_ABORT       */ "query aborted",
    /* SQLITE_UNAVAILABLE */ "database is unavailable",
    /* SQLITE_INUSE       */ "database object is in use",
    /* SQLITE_NOMEM       */ "out of memory",
    /* SQLITE_READONLY    */ "attempt to write a readonly database",
    /* SQLITE_INTERRUPT   */ "interrupted",
    /* SQLITE_IOERR       */ "disk I/O error",
    /* SQLITE_CORRUPT     */ "database disk image is malformed",
    /* SQLITE_NOTFOUND    */ "unknown operation",
    /* SQLITE_FULL        */ "database or disk is full",
    /* SQLITE_CANTOPEN    */ "unable to open database file",
    /* SQLITE_PROTOCOL    */ "access protocol",
    /* SQLITE_EMPTY       */ 0,
    /* SQLITE_SCHEMA      */ "database schema has changed",
    /* SQLITE_TOOBIG      */ "string or blob too big",
    /* SQLITE_CONSTRAINT  */ "constraint failed",
    /* SQLITE_MISMATCH    */ "datatype mismatch",
    /* SQLITE_MISUSE      */ "bad parameter or other API misuse",
    /* SQLITE_NOLFS       */ 0,
    /* SQLITE_AUTH        */ "authorization denied",
    /* SQLITE_FORMAT      */ 0,
    /* SQLITE_RANGE       */ "column index out of range",
    /* SQLITE_NOTADB      */ "file is not a database",
    /* SQLITE_NOTICE      */ "notification message",
    /* SQLITE_WARNING     */ "warning message",
  };
  const char *zErr = "unknown error";
  switch( rc ){
    case SQLITE_ABORT_ROLLBACK: {
      zErr = "abort due to ROLLBACK";
      break;
    }
    case SQLITE_ROW: {
      zErr = "another row available";
      break;
    }
    case SQLITE_DONE: {
      zErr = "no more rows available";
      break;
    }
    default: {
      rc &= 0xff;
      if( ALWAYS(rc>=0) && rc<ArraySize(aMsg) && aMsg[rc]!=0 ){
        zErr = aMsg[rc];
      }
      break;
    }
  }
  return zErr;
}

#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
/*
** This routine sets the progress callback for an Sqlite database to the
** given callback function with the given argument. The progress callback will
** be invoked every nOps opcodes.
*/
void sqlite3_progress_handler(
  sqlite3 *db,
  int nOps,
  int (*xProgress)(void*),
  void *pArg
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return;
  }
#endif
  if( nOps>0 ){
    db->xProgress = xProgress;
    db->nProgressOps = (unsigned)nOps;
    db->pProgressArg = pArg;
  }else{
    db->xProgress = 0;
    db->nProgressOps = 0;
    db->pProgressArg = 0;
  }
}
#endif
/*
** This function is exactly the same as sqlite3_create_function(), except
** that it is designed to be called by internal code.
*/
int sqlite3CreateFunc(
  sqlite3 *db,
  const char *zFunctionName,
  int nArg,
  int enc,
  void *pUserData,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*),
  void (*xValue)(sqlite3_context*),
  void (*xInverse)(sqlite3_context*,int,sqlite3_value **),
  FuncDestructor *pDestructor
){
  FuncDef *p;
  int extraFlags;

  assert( xValue==0 || xSFunc==0 );
  if( zFunctionName==0                /* Must have a valid name */
   || (xSFunc!=0 && xFinal!=0)        /* Not both xSFunc and xFinal */
   || ((xFinal==0)!=(xStep==0))       /* Both or neither of xFinal and xStep */
   || ((xValue==0)!=(xInverse==0))    /* Both or neither of xValue, xInverse */
   || (nArg<-1 || nArg>SQLITE_MAX_FUNCTION_ARG)
   || (255<sqlite3Strlen30(zFunctionName))
  ){
    return SQLITE_MISUSE_BKPT;
  }

  assert( SQLITE_FUNC_CONSTANT==SQLITE_DETERMINISTIC );
  assert( SQLITE_FUNC_DIRECT==SQLITE_DIRECTONLY );
  extraFlags = enc &  (SQLITE_DETERMINISTIC|SQLITE_DIRECTONLY|
                       SQLITE_SUBTYPE|SQLITE_INNOCUOUS|
                       SQLITE_RESULT_SUBTYPE|SQLITE_SELFORDER1);
  enc &= (SQLITE_FUNC_ENCMASK|SQLITE_ANY);

  /* The SQLITE_INNOCUOUS flag is the same bit as SQLITE_FUNC_UNSAFE.  But
  ** the meaning is inverted.  So flip the bit. */
  assert( SQLITE_FUNC_UNSAFE==SQLITE_INNOCUOUS );
  extraFlags ^= SQLITE_FUNC_UNSAFE;  /* tag-20230109-1 */

 
#ifndef SQLITE_OMIT_UTF16
  /* If SQLITE_UTF16 is specified as the encoding type, transform this
  ** to one of SQLITE_UTF16LE or SQLITE_UTF16BE using the
  ** SQLITE_UTF16NATIVE macro. SQLITE_UTF16 is not used internally.
  **
  ** If SQLITE_ANY is specified, add three versions of the function
  ** to the hash table.
  */
  switch( enc ){
    case SQLITE_UTF16:
      enc = SQLITE_UTF16NATIVE;
      break;
    case SQLITE_ANY: {
      int rc;
      rc = sqlite3CreateFunc(db, zFunctionName, nArg,
           (SQLITE_UTF8|extraFlags)^SQLITE_FUNC_UNSAFE, /* tag-20230109-1 */
           pUserData, xSFunc, xStep, xFinal, xValue, xInverse, pDestructor);
      if( rc==SQLITE_OK ){
        rc = sqlite3CreateFunc(db, zFunctionName, nArg,
             (SQLITE_UTF16LE|extraFlags)^SQLITE_FUNC_UNSAFE, /* tag-20230109-1*/
             pUserData, xSFunc, xStep, xFinal, xValue, xInverse, pDestructor);
      }
      if( rc!=SQLITE_OK ){
        return rc;
      }
      enc = SQLITE_UTF16BE;
      break;
    }
    case SQLITE_UTF8:
    case SQLITE_UTF16LE:
    case SQLITE_UTF16BE:
      break;
    default:
      enc = SQLITE_UTF8;
      break;
  }
#else
  enc = SQLITE_UTF8;
#endif
 
  /* Check if an existing function is being overridden or deleted. If so,
  ** and there are active VMs, then return SQLITE_UNAVAILABLE. If a function
  ** is being overridden/deleted but there are no active VMs, allow the
  ** operation to continue but invalidate all precompiled statements.
  */
  p = sqlite3FindFunction(db, zFunctionName, nArg, (u8)enc, 0);
  if( p && (p->funcFlags & SQLITE_FUNC_ENCMASK)==(u32)enc && p->nArg==nArg ){
    if( db->nVdbeActive ){
      sqlite3ErrorWithMsg(db, SQLITE_UNAVAILABLE,
        "unable to delete/modify user-function due to active statements");
      return SQLITE_UNAVAILABLE;
    }else{
      sqlite3ExpirePreparedStatements(db, 0);
    }
  }else if( xSFunc==0 && xFinal==0 ){
    /* Trying to delete a function that does not exist.  This is a no-op.
    ** https://sqlite.org/forum/forumpost/726219164b */
    return SQLITE_OK;
  }

  p = sqlite3FindFunction(db, zFunctionName, nArg, (u8)enc, 1);
  assert( p!=0 || db->errCode==SQLITE_NOMEM );
  if( !p ){
    return SQLITE_NOMEM_BKPT;
  }

  /* If an older version of the function with a configured destructor is
  ** being replaced invoke the destructor function here. */
  functionDestroy(db, p);

  if( pDestructor ){
    pDestructor->nRef++;
  }
  p->u.pDestructor = pDestructor;
  p->funcFlags = (p->funcFlags & SQLITE_FUNC_ENCMASK) | extraFlags;
  p->xSFunc = xSFunc ? xSFunc : xStep;
  p->xFinalize = xFinal;
  p->xValue = xValue;
  p->xInverse = xInverse;
  p->pUserData = pUserData;
  p->nArg = (u16)nArg;
  return SQLITE_OK;
}

/*
** Worker function used by utf-8 APIs that create new functions:
**
**    sqlite3_create_function()
**    sqlite3_create_function_v2()
**    sqlite3_create_window_function()
*/
static int createFunctionApi(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xStep)(sqlite3_context*,int,sqlite3_value**),
  void (*xFinal)(sqlite3_context*),
  void (*xValue)(sqlite3_context*),
  void (*xInverse)(sqlite3_context*,int,sqlite3_value**),
  void(*xDestroy)(void*)
){
  int rc = SQLITE_ERROR;
  FuncDestructor *pArg = 0;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  if( xDestroy ){
    pArg = (FuncDestructor *)sqlite3Malloc(sizeof(FuncDestructor));
    if( !pArg ){
      xDestroy(p);
      goto out;
    }
    pArg->nRef = 0;
    pArg->xDestroy = xDestroy;
    pArg->pUserData = p;
  }
  rc = sqlite3CreateFunc(db, zFunc, nArg, enc, p,
      xSFunc, xStep, xFinal, xValue, xInverse, pArg
  );
  if( pArg && pArg->nRef==0 ){
    assert( rc!=SQLITE_OK || (xStep==0 && xFinal==0) );
    xDestroy(p);
    sqlite3_free(pArg);
  }

 out:
  rc = sqlite3ApiExit(db, rc);
  return rc;
}

/*
** Create new user functions.
*/
int sqlite3_create_function(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*)
){
  return createFunctionApi(db, zFunc, nArg, enc, p, xSFunc, xStep,
                                    xFinal, 0, 0, 0);
}
int sqlite3_create_function_v2(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*),
  void (*xDestroy)(void *)
){
  return createFunctionApi(db, zFunc, nArg, enc, p, xSFunc, xStep,
                                    xFinal, 0, 0, xDestroy);
}
int sqlite3_create_window_function(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*),
  void (*xValue)(sqlite3_context*),
  void (*xInverse)(sqlite3_context*,int,sqlite3_value **),
  void (*xDestroy)(void *)
){
  return createFunctionApi(db, zFunc, nArg, enc, p, 0, xStep,
                                    xFinal, xValue, xInverse, xDestroy);
}

#ifndef SQLITE_OMIT_UTF16
int sqlite3_create_function16(
  sqlite3 *db,
  const void *zFunctionName,
  int nArg,
  int eTextRep,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xStep)(sqlite3_context*,int,sqlite3_value**),
  void (*xFinal)(sqlite3_context*)
){
  int rc;
  char *zFunc8;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zFunctionName==0 ) return SQLITE_MISUSE_BKPT;
#endif
  zFunc8 = sqlite3Utf16to8(db, zFunctionName, -1, SQLITE_UTF16NATIVE);
  rc = sqlite3CreateFunc(db, zFunc8, nArg, eTextRep, p, xSFunc,xStep,xFinal,0,0,0);
  sqlite3DbFree(db, zFunc8);
  rc = sqlite3ApiExit(db, rc);
  return rc;
}
#endif


/*
** The following is the implementation of an SQL function that always
** fails with an error message stating that the function is used in the
** wrong context.  The sqlite3_overload_function() API might construct
** SQL function that use this routine so that the functions will exist
** for name resolution but are actually overloaded by the xFindFunction
** method of virtual tables.
*/
static void sqlite3InvalidFunction(
  sqlite3_context *context,  /* The function calling context */
  int NotUsed,               /* Number of arguments to the function */
  sqlite3_value **NotUsed2   /* Value of each argument */
){
  const char *zName = (const char*)sqlite3_user_data(context);
  char *zErr;
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  zErr = sqlite3_mprintf(
      "unable to use function %s in the requested context", zName);
  sqlite3_result_error(context, zErr, -1);
  sqlite3_free(zErr);
}

/*
** Declare that a function has been overloaded by a virtual table.
**
** If the function already exists as a regular global function, then
** this routine is a no-op.  If the function does not exist, then create
** a new one that always throws a run-time error. 
**
** When virtual tables intend to provide an overloaded function, they
** should call this routine to make sure the global function exists.
** A global function must exist in order for name resolution to work
** properly.
*/
int sqlite3_overload_function(
  sqlite3 *db,
  const char *zName,
  int nArg
){
  int rc;
  char *zCopy;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zName==0 || nArg<-2 ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  rc = sqlite3FindFunction(db, zName, nArg, SQLITE_UTF8, 0)!=0;
  if( rc ) return SQLITE_OK;
  zCopy = sqlite3_mprintf("%s", zName);
  if( zCopy==0 ) return SQLITE_NOMEM;
  return sqlite3_create_function_v2(db, zName, nArg, SQLITE_UTF8,
                           zCopy, sqlite3InvalidFunction, 0, 0, sqlite3_free);
}

#ifndef SQLITE_OMIT_TRACE
/*
** Register a trace function.  The pArg from the previously registered trace
** is returned. 
**
** A NULL trace function means that no tracing is executes.  A non-NULL
** trace is a pointer to a function that is invoked at the start of each
** SQL statement.
*/
#ifndef SQLITE_OMIT_DEPRECATED
void *sqlite3_trace(sqlite3 *db, void(*xTrace)(void*,const char*), void *pArg){
  void *pOld;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  pOld = db->pTraceArg;
  db->mTrace = xTrace ? SQLITE_TRACE_LEGACY : 0;
  db->trace.xLegacy = xTrace;
  db->pTraceArg = pArg;
  return pOld;
}
#endif /* SQLITE_OMIT_DEPRECATED */

/* Register a trace callback using the version-2 interface.
*/
int sqlite3_trace_v2(
  sqlite3 *db,                               /* Trace this connection */
  unsigned mTrace,                           /* Mask of events to be traced */
  int(*xTrace)(unsigned,void*,void*,void*),  /* Callback to invoke */
  void *pArg                                 /* Context */
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  if( mTrace==0 ) xTrace = 0;
  if( xTrace==0 ) mTrace = 0;
  db->mTrace = mTrace;
  db->trace.xV2 = xTrace;
  db->pTraceArg = pArg;
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_DEPRECATED
/*
** Register a profile function.  The pArg from the previously registered
** profile function is returned. 
**
** A NULL profile function means that no profiling is executes.  A non-NULL
** profile is a pointer to a function that is invoked at the conclusion of
** each SQL statement that is run.
*/
void *sqlite3_profile(
  sqlite3 *db,
  void (*xProfile)(void*,const char*,sqlite_uint64),
  void *pArg
){
  void *pOld;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  pOld = db->pProfileArg;
  db->xProfile = xProfile;
  db->pProfileArg = pArg;
  db->mTrace &= SQLITE_TRACE_NONLEGACY_MASK;
  if( db->xProfile ) db->mTrace |= SQLITE_TRACE_XPROFILE;
  return pOld;
}
#endif /* SQLITE_OMIT_DEPRECATED */
#endif /* SQLITE_OMIT_TRACE */

/*
** Register a function to be invoked when a transaction commits.
** If the invoked function returns non-zero, then the commit becomes a
** rollback.
*/
void *sqlite3_commit_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  int (*xCallback)(void*),  /* Function to invoke on each commit */
  void *pArg                /* Argument to the function */
){
  void *pOld;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  pOld = db->pCommitArg;
  db->xCommitCallback = xCallback;
  db->pCommitArg = pArg;
  return pOld;
}

/*
** Register a callback to be invoked each time a row is updated,
** inserted or deleted using this database connection.
*/
void *sqlite3_update_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void (*xCallback)(void*,int,char const *,char const *,sqlite_int64),
  void *pArg                /* Argument to the function */
){
  void *pRet;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  pRet = db->pUpdateArg;
  db->xUpdateCallback = xCallback;
  db->pUpdateArg = pArg;
  return pRet;
}

/*
** Register a callback to be invoked each time a transaction is rolled
** back by this database connection.
*/
void *sqlite3_rollback_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void (*xCallback)(void*), /* Callback function */
  void *pArg                /* Argument to the function */
){
  void *pRet;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  pRet = db->pRollbackArg;
  db->xRollbackCallback = xCallback;
  db->pRollbackArg = pArg;
  return pRet;
}

#ifdef SQLITE_ENABLE_PREUPDATE_HOOK
/*
** Register a callback to be invoked each time a row is updated,
** inserted or deleted using this database connection.
*/
void *sqlite3_preupdate_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void(*xCallback)(         /* Callback function */
    void*,sqlite3*,int,char const*,char const*,sqlite3_int64,sqlite3_int64),
  void *pArg                /* First callback argument */
){
  void *pRet;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( db==0 ){
    return 0;
  }
#endif
  pRet = db->pPreUpdateArg;
  db->xPreUpdateCallback = xCallback;
  db->pPreUpdateArg = pArg;
  return pRet;
}
#endif /* SQLITE_ENABLE_PREUPDATE_HOOK */

/*
** Register a function to be invoked prior to each autovacuum that
** determines the number of pages to vacuum.
*/
int sqlite3_autovacuum_pages(
  sqlite3 *db,                 /* Attach the hook to this database */
  unsigned int (*xCallback)(void*,const char*,u32,u32,u32),
  void *pArg,                  /* Argument to the function */
  void (*xDestructor)(void*)   /* Destructor for pArg */
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    if( xDestructor ) xDestructor(pArg);
    return SQLITE_MISUSE_BKPT;
  }
#endif
  if( db->xAutovacDestr ){
    db->xAutovacDestr(db->pAutovacPagesArg);
  }
  db->xAutovacPages = xCallback;
  db->pAutovacPagesArg = pArg;
  db->xAutovacDestr = xDestructor;
  return SQLITE_OK;
}


/*
** This function returns true if main-memory should be used instead of
** a temporary file for transient pager files and statement journals.
** The value returned depends on the value of db->temp_store (runtime
** parameter) and the compile time value of SQLITE_TEMP_STORE. The
** following table describes the relationship between these two values
** and this functions return value.
**
**   SQLITE_TEMP_STORE     db->temp_store     Location of temporary database
**   -----------------     --------------     ------------------------------
**   0                     any                file      (return 0)
**   1                     1                  file      (return 0)
**   1                     2                  memory    (return 1)
**   1                     0                  file      (return 0)
**   2                     1                  file      (return 0)
**   2                     2                  memory    (return 1)
**   2                     0                  memory    (return 1)
**   3                     any                memory    (return 1)
*/
int sqlite3TempInMemory(const sqlite3 *db){
#if SQLITE_TEMP_STORE==1
  return ( db->temp_store==2 );
#endif
#if SQLITE_TEMP_STORE==2
  return ( db->temp_store!=1 );
#endif
#if SQLITE_TEMP_STORE==3
  UNUSED_PARAMETER(db);
  return 1;
#endif
#if SQLITE_TEMP_STORE<1 || SQLITE_TEMP_STORE>3
  UNUSED_PARAMETER(db);
  return 0;
#endif
}

/*
** Return UTF-8 encoded English language explanation of the most recent
** error.
*/
const char *sqlite3_errmsg(sqlite3 *db){
  const char *z;
  if( !db ){
    return sqlite3ErrStr(SQLITE_NOMEM_BKPT);
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return sqlite3ErrStr(SQLITE_MISUSE_BKPT);
  }
  z = db->errCode ? (char*)sqlite3_value_text(db->pErr) : 0;
  if( z==0 ){
    z = sqlite3ErrStr(db->errCode);
  }
  return z;
}

/*
** Set the error code and error message associated with the database handle.
**
** This routine is intended to be called by outside extensions (ex: the
** Session extension). Internal logic should invoke sqlite3Error() or
** sqlite3ErrorWithMsg() directly.
*/
int sqlite3_set_errmsg(sqlite3 *db, int errcode, const char *zMsg){
  int rc = SQLITE_OK;
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  if( zMsg ){
    sqlite3ErrorWithMsg(db, errcode, "%s", zMsg);
  }else{
    sqlite3Error(db, errcode);
  }
  rc = sqlite3ApiExit(db, rc);
  return rc;
}

/*
** Return the byte offset of the most recent error
*/
int sqlite3_error_offset(sqlite3 *db){
  int iOffset = -1;
  if( db && sqlite3SafetyCheckSickOrOk(db) ){
    if( db->errCode ){
      iOffset = db->errByteOffset;
    }
  }
  return iOffset;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Return UTF-16 encoded English language explanation of the most recent
** error.
*/
const void *sqlite3_errmsg16(sqlite3 *db){
  static const u16 outOfMem[] = {
    'o', 'u', 't', ' ', 'o', 'f', ' ', 'm', 'e', 'm', 'o', 'r', 'y', 0
  };
  static const u16 misuse[] = {
    'b', 'a', 'd', ' ', 'p', 'a', 'r', 'a', 'm', 'e', 't', 'e', 'r', ' ',
    'o', 'r', ' ', 'o', 't', 'h', 'e', 'r', ' ', 'A', 'P', 'I', ' ',
    'm', 'i', 's', 'u', 's', 'e', 0
  };

  const void *z;
  if( !db ){
    return (void *)outOfMem;
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return (void *)misuse;
  }
  z = sqlite3_value_text16(db->pErr);
  if( z==0 ){
    sqlite3ErrorWithMsg(db, db->errCode, sqlite3ErrStr(db->errCode));
    z = sqlite3_value_text16(db->pErr);
  }
  return z;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Return the most recent error code generated by an SQLite routine. If NULL is
** passed to this function, we assume a malloc() failed during sqlite3_open().
*/
int sqlite3_errcode(sqlite3 *db){
  if( !db ) return SQLITE_NOMEM_BKPT;
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  return db->errCode & db->errMask;
}
int sqlite3_extended_errcode(sqlite3 *db){
  if( !db ) return SQLITE_NOMEM_BKPT;
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  return db->errCode;
}
int sqlite3_system_errno(sqlite3 *db){
  int iRet = 0;
  if( db ){
    iRet = db->iSysErrno;
  }
  return iRet;
} 

/*
** Return a string that describes the kind of error specified in the
** argument.  For now, this simply calls the internal sqlite3ErrStr()
** function.
*/
const char *sqlite3_errstr(int rc){
  return sqlite3ErrStr(rc);
}

/*
** Create a new collating function for database "db".  The name is zName
** and the encoding is enc.
*/
static int createCollation(
  sqlite3* db,
  const char *zName,
  u8 enc,
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*),
  void(*xDel)(void*)
){
  CollSeq *pColl;
  int enc2;
 
  /* If SQLITE_UTF16 is specified as the encoding type, transform this
  ** to one of SQLITE_UTF16LE or SQLITE_UTF16BE using the
  ** SQLITE_UTF16NATIVE macro. SQLITE_UTF16 is not used internally.
  */
  enc2 = enc;
  if( enc2==SQLITE_UTF16 || enc2==SQLITE_UTF16_ALIGNED ){
    enc2 = SQLITE_UTF16NATIVE;
  }
  if( enc2<SQLITE_UTF8 || enc2>SQLITE_UTF16BE ){
    return SQLITE_MISUSE_BKPT;
  }

  /* Check if this call is removing or replacing an existing collation
  ** sequence. If so, and there are active VMs, return busy. If there
  ** are no active VMs, invalidate any pre-compiled statements.
  */
  pColl = sqlite3FindCollSeq(db, (u8)enc2, zName, 0);
  if( pColl && pColl->xCmp ){
    if( db->nVdbeActive ){
      sqlite3ErrorWithMsg(db, SQLITE_UNAVAILABLE,
        "unable to delete/modify collation sequence due to active statements");
      return SQLITE_UNAVAILABLE;
    }
    sqlite3ExpirePreparedStatements(db, 0);

    /* If collation sequence pColl was created directly by a call to
    ** sqlite3_create_collation, and not generated by synthCollSeq(),
    ** then any copies made by synthCollSeq() need to be invalidated.
    ** Also, collation destructor - CollSeq.xDel() - function may need
    ** to be called.
    */
    if( (pColl->enc & ~SQLITE_UTF16_ALIGNED)==enc2 ){
      CollSeq *aColl = sqlite3HashFind(&db->aCollSeq, zName);
      int j;
      for(j=0; j<3; j++){
        CollSeq *p = &aColl[j];
        if( p->enc==pColl->enc ){
          if( p->xDel ){
            p->xDel(p->pUser);
          }
          p->xCmp = 0;
        }
      }
    }
  }

  pColl = sqlite3FindCollSeq(db, (u8)enc2, zName, 1);
  if( pColl==0 ) return SQLITE_NOMEM_BKPT;
  pColl->xCmp = xCompare;
  pColl->pUser = pCtx;
  pColl->xDel = xDel;
  pColl->enc = (u8)(enc2 | (enc & SQLITE_UTF16_ALIGNED));
  sqlite3Error(db, SQLITE_OK);
  return SQLITE_OK;
}


/*
** This array defines hard upper bounds on limit values.  The
** initializer must be kept in sync with the SQLITE_LIMIT_*
** #defines in sqlite3.h.
*/
static const int aHardLimit[] = {
  SQLITE_MAX_LENGTH,
  SQLITE_MAX_SQL_LENGTH,
  SQLITE_MAX_COLUMN,
  SQLITE_MAX_EXPR_DEPTH,
  SQLITE_MAX_COMPOUND_SELECT,
  SQLITE_MAX_VDBE_OP,
  SQLITE_MAX_FUNCTION_ARG,
  SQLITE_MAX_ATTACHED,
  SQLITE_MAX_LIKE_PATTERN_LENGTH,
  SQLITE_MAX_VARIABLE_NUMBER,      /* IMP: R-38091-32352 */
  SQLITE_MAX_TRIGGER_DEPTH,
  SQLITE_MAX_PARSER_DEPTH,
  SQLITE_MAX_SCHEMA,
};

/*
** Make sure the hard limits are set to reasonable values
*/
#if SQLITE_MAX_LENGTH<100
# error SQLITE_MAX_LENGTH must be at least 100
#endif
#if SQLITE_MAX_SQL_LENGTH<100
# error SQLITE_MAX_SQL_LENGTH must be at least 100
#endif
#if SQLITE_MAX_SQL_LENGTH>SQLITE_MAX_LENGTH
# error SQLITE_MAX_SQL_LENGTH must not be greater than SQLITE_MAX_LENGTH
#endif
#if SQLITE_MAX_SQL_LENGTH>2147482624     /* 1024 less than 2^31 */
# error SQLITE_MAX_SQL_LENGTH must not be greater than 2147482624
#endif
#if SQLITE_MAX_COMPOUND_SELECT<2
# error SQLITE_MAX_COMPOUND_SELECT must be at least 2
#endif
#if SQLITE_MAX_VDBE_OP<40
# error SQLITE_MAX_VDBE_OP must be at least 40
#endif
#if SQLITE_MAX_FUNCTION_ARG<0 || SQLITE_MAX_FUNCTION_ARG>32767
# error SQLITE_MAX_FUNCTION_ARG must be between 0 and 32767
#endif
#if SQLITE_MAX_ATTACHED<0 || SQLITE_MAX_ATTACHED>125
# error SQLITE_MAX_ATTACHED must be between 0 and 125
#endif
#if SQLITE_MAX_LIKE_PATTERN_LENGTH<1
# error SQLITE_MAX_LIKE_PATTERN_LENGTH must be at least 1
#endif
#if SQLITE_MAX_COLUMN>32767
# error SQLITE_MAX_COLUMN must not exceed 32767
#endif
#if SQLITE_MAX_TRIGGER_DEPTH<1
# error SQLITE_MAX_TRIGGER_DEPTH must be at least 1
#endif


/*
** Change the value of a limit.  Report the old value.
** If an invalid limit index is supplied, report -1.
** Make no changes but still report the old value if the
** new limit is negative.
**
** A new lower limit does not shrink existing constructs.
** It merely prevents new constructs that exceed the limit
** from forming.
*/
int sqlite3_limit(sqlite3 *db, int limitId, int newLimit){
  int oldLimit;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return -1;
  }
#endif

  /* EVIDENCE-OF: R-30189-54097 For each limit category SQLITE_LIMIT_NAME
  ** there is a hard upper bound set at compile-time by a C preprocessor
  ** macro called SQLITE_MAX_NAME. (The "_LIMIT_" in the name is changed to
  ** "_MAX_".)
  */
  assert( aHardLimit[SQLITE_LIMIT_LENGTH]==SQLITE_MAX_LENGTH );
  assert( aHardLimit[SQLITE_LIMIT_SQL_LENGTH]==SQLITE_MAX_SQL_LENGTH );
  assert( aHardLimit[SQLITE_LIMIT_COLUMN]==SQLITE_MAX_COLUMN );
  assert( aHardLimit[SQLITE_LIMIT_EXPR_DEPTH]==SQLITE_MAX_EXPR_DEPTH );
  assert( aHardLimit[SQLITE_LIMIT_PARSER_DEPTH]==SQLITE_MAX_PARSER_DEPTH );
  assert( aHardLimit[SQLITE_LIMIT_COMPOUND_SELECT]==SQLITE_MAX_COMPOUND_SELECT);
  assert( aHardLimit[SQLITE_LIMIT_VDBE_OP]==SQLITE_MAX_VDBE_OP );
  assert( aHardLimit[SQLITE_LIMIT_FUNCTION_ARG]==SQLITE_MAX_FUNCTION_ARG );
  assert( aHardLimit[SQLITE_LIMIT_ATTACHED]==SQLITE_MAX_ATTACHED );
  assert( aHardLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH]==
                                               SQLITE_MAX_LIKE_PATTERN_LENGTH );
  assert( aHardLimit[SQLITE_LIMIT_VARIABLE_NUMBER]==SQLITE_MAX_VARIABLE_NUMBER);
  assert( aHardLimit[SQLITE_LIMIT_TRIGGER_DEPTH]==SQLITE_MAX_TRIGGER_DEPTH );
  assert( aHardLimit[SQLITE_LIMIT_SCHEMA]==SQLITE_MAX_SCHEMA );
  assert( SQLITE_LIMIT_SCHEMA==(SQLITE_N_LIMIT-1) );


  if( limitId<0 || limitId>=SQLITE_N_LIMIT ){
    return -1;
  }
  oldLimit = db->aLimit[limitId];
  if( newLimit>=0 ){                   /* IMP: R-52476-28732 */
    if( newLimit>aHardLimit[limitId] ){
      newLimit = aHardLimit[limitId];  /* IMP: R-51463-25634 */
    }else if( newLimit<SQLITE_MIN_LENGTH && limitId==SQLITE_LIMIT_LENGTH ){
      newLimit = SQLITE_MIN_LENGTH;
    }
    db->aLimit[limitId] = newLimit;
  }
  return oldLimit;                     /* IMP: R-53341-35419 */
}

/*
** This function is used to parse both URIs and non-URI filenames passed by the
** user to API functions sqlite3_open() or sqlite3_open_v2(), and for database
** URIs specified as part of ATTACH statements.
**
** The first argument to this function is the name of the VFS to use (or
** a NULL to signify the default VFS) if the URI does not contain a "vfs=xxx"
** query parameter. The second argument contains the URI (or non-URI filename)
** itself. When this function is called the *pFlags variable should contain
** the default flags to open the database handle with. The value stored in
** *pFlags may be updated before returning if the URI filename contains
** "cache=xxx" or "mode=xxx" query parameters.
**
** If successful, SQLITE_OK is returned. In this case *ppVfs is set to point to
** the VFS that should be used to open the database file. *pzFile is set to
** point to a buffer containing the name of the file to open.  The value
** stored in *pzFile is a database name acceptable to sqlite3_uri_parameter()
** and is in the same format as names created using sqlite3_create_filename().
** The caller must invoke sqlite3_free_filename() (not sqlite3_free()!) on
** the value returned in *pzFile to avoid a memory leak.
**
** If an error occurs, then an SQLite error code is returned and *pzErrMsg
** may be set to point to a buffer containing an English language error
** message. It is the responsibility of the caller to eventually release
** this buffer by calling sqlite3_free().
*/
int sqlite3ParseUri(
  const char *zDefaultVfs,        /* VFS to use if no "vfs=xxx" query option */
  const char *zUri,               /* Nul-terminated URI to parse */
  unsigned int *pFlags,           /* IN/OUT: SQLITE_OPEN_XXX flags */
  sqlite3_vfs **ppVfs,            /* OUT: VFS to use */
  char **pzFile,                  /* OUT: Filename component of URI */
  char **pzErrMsg                 /* OUT: Error message (if rc!=SQLITE_OK) */
){
  int rc = SQLITE_OK;
  unsigned int flags = *pFlags;
  const char *zVfs = zDefaultVfs;
  char *zFile;
  char c;
  i64 nUri = strlen(zUri);

  assert( *pzErrMsg==0 );

  if( ((flags & SQLITE_OPEN_URI)                     /* IMP: R-48725-32206 */
       || sqlite3GlobalConfig.bOpenUri) /* IMP: R-51689-46548 */
   && nUri>=5 && memcmp(zUri, "file:", 5)==0         /* IMP: R-57884-37496 */
  ){
    char *zOpt;
    int eState;                   /* Parser state when parsing URI */
    i64 iIn;                      /* Input character index */
    i64 iOut = 0;                 /* Output character index */
    u64 nByte = nUri+8;           /* Bytes of space to allocate */

    /* Make sure the SQLITE_OPEN_URI flag is set to indicate to the VFS xOpen
    ** method that there may be extra parameters following the file-name.  */
    flags |= SQLITE_OPEN_URI;

    for(iIn=0; iIn<nUri; iIn++) nByte += (zUri[iIn]=='&');
    zFile = sqlite3_malloc64(nByte);
    if( !zFile ) return SQLITE_NOMEM_BKPT;

    memset(zFile, 0, 4);  /* 4-byte of 0x00 is the start of DB name marker */
    zFile += 4;

    iIn = 5;
#ifdef SQLITE_ALLOW_URI_AUTHORITY
    if( strncmp(zUri+5, "///", 3)==0 ){
      iIn = 7;
      /* The following condition causes URIs with five leading / characters
      ** like file://///host/path to be converted into UNCs like //host/path.
      ** The correct URI for that UNC has only two or four leading / characters
      ** file://host/path or file:////host/path.  But 5 leading slashes is a
      ** common error, we are told, so we handle it as a special case. */
      if( strncmp(zUri+7, "///", 3)==0 ){ iIn++; }
    }else if( strncmp(zUri+5, "//localhost/", 12)==0 ){
      iIn = 16;
    }
#else
    /* Discard the scheme and authority segments of the URI. */
    if( zUri[5]=='/' && zUri[6]=='/' ){
      iIn = 7;
      while( zUri[iIn] && zUri[iIn]!='/' ) iIn++;
      if( iIn!=7 && (iIn!=16 || memcmp("localhost", &zUri[7], 9)) ){
        *pzErrMsg = sqlite3_mprintf("invalid uri authority: %.*s",
            (int)(iIn-7), &zUri[7]);
        rc = SQLITE_ERROR;
        goto parse_uri_out;
      }
    }
#endif

    /* Copy the filename and any query parameters into the zFile buffer.
    ** Decode %HH escape codes along the way.
    **
    ** Within this loop, variable eState may be set to 0, 1 or 2, depending
    ** on the parsing context. As follows:
    **
    **   0: Parsing file-name.
    **   1: Parsing name section of a name=value query parameter.
    **   2: Parsing value section of a name=value query parameter.
    */
    eState = 0;
    while( (c = zUri[iIn])!=0 && c!='#' ){
      iIn++;
      if( c=='%'
       && sqlite3Isxdigit(zUri[iIn])
       && sqlite3Isxdigit(zUri[iIn+1])
      ){
        int octet = (sqlite3HexToInt(zUri[iIn++]) << 4);
        octet += sqlite3HexToInt(zUri[iIn++]);

        assert( octet>=0 && octet<256 );
        if( octet==0 ){
#ifndef SQLITE_ENABLE_URI_00_ERROR
          /* This branch is taken when "%00" appears within the URI. In this
          ** case we ignore all text in the remainder of the path, name or
          ** value currently being parsed. So ignore the current character
          ** and skip to the next "?", "=" or "&", as appropriate. */
          while( (c = zUri[iIn])!=0 && c!='#'
              && (eState!=0 || c!='?')
              && (eState!=1 || (c!='=' && c!='&'))
              && (eState!=2 || c!='&')
          ){
            iIn++;
          }
          continue;
#else
          /* If ENABLE_URI_00_ERROR is defined, "%00" in a URI is an error. */
          *pzErrMsg = sqlite3_mprintf("unexpected %%00 in uri");
          rc = SQLITE_ERROR;
          goto parse_uri_out;
#endif
        }
        c = octet;
      }else if( eState==1 && (c=='&' || c=='=') ){
        if( zFile[iOut-1]==0 ){
          /* An empty option name. Ignore this option altogether. */
          while( zUri[iIn] && zUri[iIn]!='#' && zUri[iIn-1]!='&' ) iIn++;
          continue;
        }
        if( c=='&' ){
          zFile[iOut++] = '\0';
        }else{
          eState = 2;
        }
        c = 0;
      }else if( (eState==0 && c=='?') || (eState==2 && c=='&') ){
        c = 0;
        eState = 1;
      }
      zFile[iOut++] = c;
    }
    if( eState==1 ) zFile[iOut++] = '\0';
    memset(zFile+iOut, 0, 4); /* end-of-options + empty journal filenames */

    /* Check if there were any options specified that should be interpreted
    ** here. Options that are interpreted here include "vfs" and those that
    ** correspond to flags that may be passed to the sqlite3_open_v2()
    ** method. */
    zOpt = &zFile[strlen(zFile)+1];
    while( zOpt[0] ){
      i64 nOpt = strlen(zOpt);
      char *zVal = &zOpt[nOpt+1];
      i64 nVal = strlen(zVal);

      if( nOpt==3 && memcmp("vfs", zOpt, 3)==0 ){
        zVfs = zVal;
      }else{
        struct OpenMode {
          const char *z;
          int mode;
        } *aMode = 0;
        char *zModeType = 0;
        int mask = 0;
        int limit = 0;

        if( nOpt==4 && memcmp("mode", zOpt, 4)==0 ){
          static struct OpenMode aOpenMode[] = {
            { "ro",  SQLITE_OPEN_READONLY },
            { "rw",  SQLITE_OPEN_READWRITE },
            { "rwc", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE },
            { "memory", SQLITE_OPEN_MEMORY },
            { 0, 0 }
          };

          mask = SQLITE_OPEN_READONLY | SQLITE_OPEN_READWRITE
                   | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY;
          aMode = aOpenMode;
          limit = mask & flags;
          zModeType = "access";
        }

        if( aMode ){
          int i;
          int mode = 0;
          for(i=0; aMode[i].z; i++){
            const char *z = aMode[i].z;
            if( nVal==(i64)strlen(z) && 0==memcmp(zVal, z, nVal) ){
              mode = aMode[i].mode;
              break;
            }
          }
          if( mode==0 ){
            *pzErrMsg = sqlite3_mprintf("no such %s mode: %s", zModeType, zVal);
            rc = SQLITE_ERROR;
            goto parse_uri_out;
          }
          if( (mode & ~SQLITE_OPEN_MEMORY)>limit ){
            *pzErrMsg = sqlite3_mprintf("%s mode not allowed: %s",
                                        zModeType, zVal);
            rc = SQLITE_PERM;
            goto parse_uri_out;
          }
          flags = (flags & ~mask) | mode;
        }
      }

      zOpt = &zVal[nVal+1];
    }

  }else{
    zFile = sqlite3_malloc64(nUri+8);
    if( !zFile ) return SQLITE_NOMEM_BKPT;
    memset(zFile, 0, 4);
    zFile += 4;
    if( nUri ){
      memcpy(zFile, zUri, nUri);
    }
    memset(zFile+nUri, 0, 4);
    flags &= ~SQLITE_OPEN_URI;
  }

  *ppVfs = sqlite3_vfs_find(zVfs);
  if( *ppVfs==0 ){
    *pzErrMsg = sqlite3_mprintf("no such vfs: %s", zVfs);
    rc = SQLITE_ERROR;
  }
 parse_uri_out:
  if( rc!=SQLITE_OK ){
    sqlite3_free_filename(zFile);
    zFile = 0;
  }
  *pFlags = flags;
  *pzFile = zFile;
  return rc;
}

/*
** This routine does the core work of extracting URI parameters from a
** database filename for the sqlite3_uri_parameter() interface.
*/
static const char *uriParameter(const char *zFilename, const char *zParam){
  zFilename += sqlite3Strlen30(zFilename) + 1;
  while( ALWAYS(zFilename!=0) && zFilename[0] ){
    int x = strcmp(zFilename, zParam);
    zFilename += sqlite3Strlen30(zFilename) + 1;
    if( x==0 ) return zFilename;
    zFilename += sqlite3Strlen30(zFilename) + 1;
  }
  return 0;
}



/*
** This routine does the work of opening a database on behalf of
** sqlite3_open() and sqlite3_open16(). The database filename "zFilename" 
** is UTF-8 encoded.
*/
static int openDatabase(
  const char *zFilename, /* Database filename UTF-8 encoded */
  sqlite3 **ppDb,        /* OUT: Returned database handle */
  unsigned int flags,    /* Operational flags */
  const char *zVfs       /* Name of the VFS to use */
){
  sqlite3 *db;                    /* Store allocated handle here */
  int rc;                         /* Return code */
  char *zOpen = 0;                /* Filename argument to pass to BtreeOpen() */
  char *zErrMsg = 0;              /* Error message from sqlite3ParseUri() */
  int i;                          /* Loop counter */

#ifdef SQLITE_ENABLE_API_ARMOR
  if( ppDb==0 ) return SQLITE_MISUSE_BKPT;
#endif
  *ppDb = 0;
#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif

  /* Remove harmful bits from the flags parameter
  ** The only
  ** valid input flags for sqlite3_open_v2() are SQLITE_OPEN_READONLY,
   ** SQLITE_OPEN_READWRITE, SQLITE_OPEN_CREATE, SQLITE_OPEN_EXRESCODE,
   ** and some reserved
   ** bits.  Silently mask off all other flags.
   */
  flags &=  ~( SQLITE_OPEN_DELETEONCLOSE |
               SQLITE_OPEN_EXCLUSIVE |
               SQLITE_OPEN_MAIN_DB |
               SQLITE_OPEN_TEMP_DB |
               SQLITE_OPEN_TRANSIENT_DB |
               SQLITE_OPEN_MAIN_JOURNAL |
               SQLITE_OPEN_TEMP_JOURNAL |
               SQLITE_OPEN_SUBJOURNAL |
               SQLITE_OPEN_SUPER_JOURNAL
             );

  /* Allocate the sqlite data structure */
  db = sqlite3MallocZero( sizeof(sqlite3) );
  if( db==0 ) goto opendb_out;
  db->errMask = (flags & SQLITE_OPEN_EXRESCODE)!=0 ? 0xffffffff : 0xff;
  db->nDb = 2;
  db->eOpenState = SQLITE_STATE_BUSY;
  db->aDb = db->aDbStatic;
  db->lookaside.bDisable = 1;
  db->lookaside.sz = 0;
  db->nFpDigit = 17;

  assert( sizeof(db->aLimit)==sizeof(aHardLimit) );
  memcpy(db->aLimit, aHardLimit, sizeof(db->aLimit));
  db->autoCommit = 1;
  db->nextAutovac = -1;
  db->nextPagesize = 0;
  db->init.azInit = sqlite3StdType; /* Any array of string ptrs will do */
  db->flags |= SQLITE_ShortColNames
                 | SQLITE_EnableTrigger
                 | SQLITE_EnableView
                 | SQLITE_CacheSpill
                 | SQLITE_AttachCreate
                 | SQLITE_AttachWrite
                 | SQLITE_Comments
#if !defined(SQLITE_TRUSTED_SCHEMA) || SQLITE_TRUSTED_SCHEMA+0!=0
                 | SQLITE_TrustedSchema
#endif
/* The SQLITE_DQS compile-time option determines the default settings
** for SQLITE_DBCONFIG_DQS_DDL and SQLITE_DBCONFIG_DQS_DML.
**
**    SQLITE_DQS     SQLITE_DBCONFIG_DQS_DDL    SQLITE_DBCONFIG_DQS_DML
**    ----------     -----------------------    -----------------------
**     undefined               on                          on
**         3                   on                          on
**         2                   on                         off
**         1                  off                          on
**         0                  off                         off
**
** Legacy behavior is 3 (double-quoted string literals are allowed anywhere)
** and so that is the default.  But developers are encouraged to use
** -DSQLITE_DQS=0 (best) or -DSQLITE_DQS=1 (second choice) if possible.
*/
#if !defined(SQLITE_DQS)
# define SQLITE_DQS 3
#endif
#if (SQLITE_DQS&1)==1
                 | SQLITE_DqsDML
#endif
#if (SQLITE_DQS&2)==2
                 | SQLITE_DqsDDL
#endif

#if !defined(SQLITE_DEFAULT_AUTOMATIC_INDEX) || SQLITE_DEFAULT_AUTOMATIC_INDEX
                 | SQLITE_AutoIndex
#endif
#if SQLITE_DEFAULT_CKPTFULLFSYNC
                 | SQLITE_CkptFullFSync
#endif
#if SQLITE_DEFAULT_FILE_FORMAT<4
                 | SQLITE_LegacyFileFmt
#endif
#ifdef SQLITE_ENABLE_LOAD_EXTENSION
                 | SQLITE_LoadExtension
#endif
#if SQLITE_DEFAULT_RECURSIVE_TRIGGERS
                 | SQLITE_RecTriggers
#endif
#if defined(SQLITE_DEFAULT_FOREIGN_KEYS) && SQLITE_DEFAULT_FOREIGN_KEYS
                 | SQLITE_ForeignKeys
#endif
#if defined(SQLITE_REVERSE_UNORDERED_SELECTS)
                 | SQLITE_ReverseOrder
#endif
#if defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
                 | SQLITE_CellSizeCk
#endif
#if defined(SQLITE_ENABLE_FTS3_TOKENIZER)
                 | SQLITE_Fts3Tokenizer
#endif
#if defined(SQLITE_ENABLE_QPSG)
                 | SQLITE_EnableQPSG
#endif
#if defined(SQLITE_DEFAULT_DEFENSIVE)
                 | SQLITE_Defensive
#endif
#if defined(SQLITE_DEFAULT_LEGACY_ALTER_TABLE)
                 | SQLITE_LegacyAlter
#endif
#if defined(SQLITE_ENABLE_STMT_SCANSTATUS)
                 | SQLITE_StmtScanStatus
#endif
      ;
  sqlite3HashInit(&db->aCollSeq);
  sqlite3HashInit(&db->aModule);

  /* Add the default collation sequence BINARY. BINARY works for both UTF-8
  ** and UTF-16, so add a version for each to avoid any unnecessary
  ** conversions. The only error that can occur here is a malloc() failure.
  **
  ** EVIDENCE-OF: R-52786-44878 SQLite defines three built-in collating
  ** functions:
  */
  createCollation(db, sqlite3StrBINARY, SQLITE_UTF8, 0, binCollFunc, 0);
  createCollation(db, sqlite3StrBINARY, SQLITE_UTF16BE, 0, binCollFunc, 0);
  createCollation(db, sqlite3StrBINARY, SQLITE_UTF16LE, 0, binCollFunc, 0);
  createCollation(db, "NOCASE", SQLITE_UTF8, 0, nocaseCollatingFunc, 0);
  createCollation(db, "RTRIM", SQLITE_UTF8, 0, rtrimCollFunc, 0);
  /* Parse the filename/URI argument
  **
  ** Only allow sensible combinations of bits in the flags argument. 
  ** Throw an error if any non-sense combination is used.  If we
  ** do not block illegal combinations here, it could trigger
  ** assert() statements in deeper layers.  Sensible combinations
  ** are:
  **
  **  1:  SQLITE_OPEN_READONLY
  **  2:  SQLITE_OPEN_READWRITE
  **  6:  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
  */
  db->openFlags = flags;
  assert( SQLITE_OPEN_READONLY  == 0x01 );
  assert( SQLITE_OPEN_READWRITE == 0x02 );
  assert( SQLITE_OPEN_CREATE    == 0x04 );
  if( ((1<<(flags&7)) & 0x46)==0 ){
    rc = SQLITE_MISUSE_BKPT;  /* IMP: R-18321-05872 */
  }else{
    if( zFilename==0 ) zFilename = ":memory:";
    rc = sqlite3ParseUri(zVfs, zFilename, &flags, &db->pVfs, &zOpen, &zErrMsg);
  }
  if( rc!=SQLITE_OK ){
    sqlite3ErrorWithMsg(db, rc, zErrMsg ? "%s" : 0, zErrMsg);
    sqlite3_free(zErrMsg);
    goto opendb_out;
  }
  assert( db->pVfs!=0 );
  /* Open the backend database driver */
  rc = sqlite3BtreeOpen(db->pVfs, zOpen, db, &db->aDb[0].pBt, 0,
                        flags | SQLITE_OPEN_MAIN_DB);
  if( rc!=SQLITE_OK ){
    sqlite3Error(db, rc);
    goto opendb_out;
  }
  db->aDb[0].pSchema = sqlite3SchemaGet(db, db->aDb[0].pBt);
  sqlite3SetTextEncoding(db, SCHEMA_ENC(db));
  db->aDb[1].pSchema = sqlite3SchemaGet(db, 0);

  /* The default safety_level for the main database is FULL; for the temp
  ** database it is OFF. This matches the pager layer defaults. 
  */
  db->aDb[0].zDbSName = "main";
  db->aDb[0].safety_level = SQLITE_DEFAULT_SYNCHRONOUS+1;
  db->aDb[1].zDbSName = "temp";
  db->aDb[1].safety_level = PAGER_SYNCHRONOUS_OFF;

  db->eOpenState = SQLITE_STATE_OPEN;

  /* Register all built-in functions, but do not attempt to read the
  ** database schema yet. This is delayed until the first time the database
  ** is accessed.
  */
  sqlite3Error(db, SQLITE_OK);
  sqlite3RegisterPerConnectionBuiltinFunctions(db);
  rc = sqlite3_errcode(db);


  /* Load compiled-in extensions */
  for(i=0; rc==SQLITE_OK && i<ArraySize(sqlite3BuiltinExtensions); i++){
    rc = sqlite3BuiltinExtensions[i](db);
  }

  /* Load automatic extensions - extensions that have been registered
  ** using the sqlite3_automatic_extension() API.
  */
  if( rc==SQLITE_OK ){
    sqlite3AutoLoadExtensions(db);
    rc = sqlite3_errcode(db);
    if( rc!=SQLITE_OK ){
      goto opendb_out;
    }
  }

  if( rc ) sqlite3Error(db, rc);

  /* Enable the lookaside-malloc subsystem */
  setupLookaside(db, 0, sqlite3GlobalConfig.szLookaside,
                        sqlite3GlobalConfig.nLookaside);

opendb_out:
  rc = sqlite3_errcode(db);
  assert( db!=0 || (rc&0xff)==SQLITE_NOMEM );
  if( (rc&0xff)==SQLITE_NOMEM ){
    sqlite3_close(db);
    db = 0;
  }else if( rc!=SQLITE_OK ){
    db->eOpenState = SQLITE_STATE_SICK;
  }
  *ppDb = db;
#ifdef SQLITE_ENABLE_SQLLOG
  if( sqlite3GlobalConfig.xSqllog ){
    /* Opening a db handle. Fourth parameter is passed 0. */
    void *pArg = sqlite3GlobalConfig.pSqllogArg;
    sqlite3GlobalConfig.xSqllog(pArg, db, zFilename, 0);
  }
#endif
  sqlite3_free_filename(zOpen);
  return rc;
}


/*
** Open a new database handle.
*/
int sqlite3_open(
  const char *zFilename,
  sqlite3 **ppDb
){
  return openDatabase(zFilename, ppDb,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
}
int sqlite3_open_v2(
  const char *filename,   /* Database filename (UTF-8) */
  sqlite3 **ppDb,         /* OUT: SQLite db handle */
  int flags,              /* Flags */
  const char *zVfs        /* Name of VFS module to use */
){
  return openDatabase(filename, ppDb, (unsigned int)flags, zVfs);
}

#ifndef SQLITE_OMIT_UTF16
/*
** Open a new database handle.
*/
int sqlite3_open16(
  const void *zFilename,
  sqlite3 **ppDb
){
  char const *zFilename8;   /* zFilename encoded in UTF-8 instead of UTF-16 */
  sqlite3_value *pVal;
  int rc;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( ppDb==0 ) return SQLITE_MISUSE_BKPT;
#endif
  *ppDb = 0;
#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif
  if( zFilename==0 ) zFilename = "\000\000";
  pVal = sqlite3ValueNew(0);
  sqlite3ValueSetStr(pVal, -1, zFilename, SQLITE_UTF16NATIVE, SQLITE_STATIC);
  zFilename8 = sqlite3ValueText(pVal, SQLITE_UTF8);
  if( zFilename8 ){
    rc = openDatabase(zFilename8, ppDb,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    assert( *ppDb || rc==SQLITE_NOMEM );
    if( rc==SQLITE_OK && !DbHasProperty(*ppDb, 0, DB_SchemaLoaded) ){
      SCHEMA_ENC(*ppDb) = ENC(*ppDb) = SQLITE_UTF16NATIVE;
    }
  }else{
    rc = SQLITE_NOMEM_BKPT;
  }
  sqlite3ValueFree(pVal);

  return rc & 0xff;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation(
  sqlite3* db,
  const char *zName,
  int enc,
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*)
){
  return sqlite3_create_collation_v2(db, zName, enc, pCtx, xCompare, 0);
}

/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation_v2(
  sqlite3* db,
  const char *zName,
  int enc,
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*),
  void(*xDel)(void*)
){
  int rc;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zName==0 ) return SQLITE_MISUSE_BKPT;
#endif
  rc = createCollation(db, zName, (u8)enc, pCtx, xCompare, xDel);
  rc = sqlite3ApiExit(db, rc);
  return rc;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation16(
  sqlite3* db,
  const void *zName,
  int enc,
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*)
){
  int rc = SQLITE_OK;
  char *zName8;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zName==0 ) return SQLITE_MISUSE_BKPT;
#endif
  zName8 = sqlite3Utf16to8(db, zName, -1, SQLITE_UTF16NATIVE);
  if( zName8 ){
    rc = createCollation(db, zName8, (u8)enc, pCtx, xCompare, 0);
    sqlite3DbFree(db, zName8);
  }
  rc = sqlite3ApiExit(db, rc);
  return rc;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Register a collation sequence factory callback with the database handle
** db. Replace any previously installed collation sequence factory.
*/
int sqlite3_collation_needed(
  sqlite3 *db,
  void *pCollNeededArg,
  void(*xCollNeeded)(void*,sqlite3*,int eTextRep,const char*)
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  db->xCollNeeded = xCollNeeded;
  db->xCollNeeded16 = 0;
  db->pCollNeededArg = pCollNeededArg;
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Register a collation sequence factory callback with the database handle
** db. Replace any previously installed collation sequence factory.
*/
int sqlite3_collation_needed16(
  sqlite3 *db,
  void *pCollNeededArg,
  void(*xCollNeeded16)(void*,sqlite3*,int eTextRep,const void*)
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  db->xCollNeeded = 0;
  db->xCollNeeded16 = xCollNeeded16;
  db->pCollNeededArg = pCollNeededArg;
  return SQLITE_OK;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Find existing client data.
*/
void *sqlite3_get_clientdata(sqlite3 *db, const char *zName){
  DbClientData *p;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !zName || !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  for(p=db->pDbData; p; p=p->pNext){
    if( strcmp(p->zName, zName)==0 ){
      void *pResult = p->pData;
      return pResult;
    }
  }
  return 0;
}

/*
** Add new client data to a database connection.
*/
int sqlite3_set_clientdata(
  sqlite3 *db,                   /* Attach client data to this connection */
  const char *zName,             /* Name of the client data */
  void *pData,                   /* The client data itself */
  void (*xDestructor)(void*)     /* Destructor */
){
  DbClientData *p, **pp;
  pp = &db->pDbData;
  for(p=db->pDbData; p && strcmp(p->zName,zName); p=p->pNext){
    pp = &p->pNext;
  }
  if( p ){
    assert( p->pData!=0 );
    if( p->xDestructor ) p->xDestructor(p->pData);
    if( pData==0 ){
      *pp = p->pNext;
      sqlite3_free(p);
      return SQLITE_OK;
    }
  }else if( pData==0 ){
    return SQLITE_OK;
  }else{
    size_t n = strlen(zName);
    p = sqlite3_malloc64( SZ_DBCLIENTDATA(n+1) );
    if( p==0 ){
      if( xDestructor ) xDestructor(pData);
      return SQLITE_NOMEM;
    }
    memcpy(p->zName, zName, n+1);
    p->pNext = db->pDbData;
    db->pDbData = p;
  }
  p->pData = pData;
  p->xDestructor = xDestructor;
  return SQLITE_OK;
}


#ifndef SQLITE_OMIT_DEPRECATED
/*
** This function is now an anachronism. It used to be used to recover from a
** malloc() failure, but SQLite now does this automatically.
*/
int sqlite3_global_recover(void){
  return SQLITE_OK;
}
#endif

/*
** Test to see whether or not the database connection is in autocommit
** mode.  Return TRUE if it is and FALSE if not.  Autocommit mode is on
** by default.  Autocommit is disabled by a BEGIN statement and reenabled
** by the next COMMIT or ROLLBACK.
*/
int sqlite3_get_autocommit(sqlite3 *db){
  int iRet;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  iRet = db->autoCommit;
  return iRet;
}

/*
** The following routines are substitutes for constants SQLITE_CORRUPT,
** SQLITE_MISUSE, SQLITE_CANTOPEN, SQLITE_NOMEM and possibly other error
** constants.  They serve two purposes:
**
**   1.  Serve as a convenient place to set a breakpoint in a debugger
**       to detect when version error conditions occurs.
**
**   2.  Invoke sqlite3_log() to provide the source code location where
**       a low-level error is first detected.
*/
int sqlite3ReportError(int iErr, int lineno, const char *zType){
  sqlite3_log(iErr, "%s at line %d of [%.10s]",
              zType, lineno, 20+sqlite3_sourceid());
  return iErr;
}
int sqlite3CorruptError(int lineno){
  return sqlite3ReportError(SQLITE_CORRUPT, lineno, "database corruption");
}
int sqlite3MisuseError(int lineno){
  return sqlite3ReportError(SQLITE_MISUSE, lineno, "misuse");
}
int sqlite3CantopenError(int lineno){
  return sqlite3ReportError(SQLITE_CANTOPEN, lineno, "cannot open file");
}
#if defined(SQLITE_ENABLE_CORRUPT_PGNO)
int sqlite3CorruptPgnoError(int lineno, Pgno pgno){
  char zMsg[100];
  sqlite3_snprintf(sizeof(zMsg), zMsg, "database corruption page %d", pgno);
  return sqlite3ReportError(SQLITE_CORRUPT, lineno, zMsg);
}
#endif

/*
** Return meta information about a specific column of a database table.
** See comment in sqlite3.h (sqlite.h.in) for details.
*/
int sqlite3_table_column_metadata(
  sqlite3 *db,                /* Connection handle */
  const char *zDbName,        /* Database name or NULL */
  const char *zTableName,     /* Table name */
  const char *zColumnName,    /* Column name */
  char const **pzDataType,    /* OUTPUT: Declared data type */
  char const **pzCollSeq,     /* OUTPUT: Collation sequence name */
  int *pNotNull,              /* OUTPUT: True if NOT NULL constraint exists */
  int *pPrimaryKey,           /* OUTPUT: True if column part of PK */
  int *pAutoinc               /* OUTPUT: True if column is auto-increment */
){
  int rc;
  char *zErrMsg = 0;
  Table *pTab = 0;
  Column *pCol = 0;
  int iCol = 0;
  char const *zDataType = 0;
  char const *zCollSeq = 0;
  int notnull = 0;
  int primarykey = 0;
  int autoinc = 0;


#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zTableName==0 ){
    return SQLITE_MISUSE_BKPT;
  }
#endif

  /* Ensure the database schema has been loaded */
  rc = sqlite3Init(db, &zErrMsg);
  if( SQLITE_OK!=rc ){
    goto error_out;
  }

  /* Locate the table in question */
  pTab = sqlite3FindTable(db, zTableName, zDbName);
  if( !pTab || IsView(pTab) ){
    pTab = 0;
    goto error_out;
  }

  /* Find the column for which info is requested */
  if( zColumnName==0 ){
    /* Query for existence of table only */
  }else{
    iCol = sqlite3ColumnIndex(pTab, zColumnName);
    if( iCol>=0 ){
      pCol = &pTab->aCol[iCol];
    }else{
      if( HasRowid(pTab) && sqlite3IsRowid(zColumnName) ){
        iCol = pTab->iPKey;
        pCol = iCol>=0 ? &pTab->aCol[iCol] : 0;
      }else{
        pTab = 0;
        goto error_out;
      }
    }
  }

  /* The following block stores the meta information that will be returned
  ** to the caller in local variables zDataType, zCollSeq, notnull, primarykey
  ** and autoinc. At this point there are two possibilities:
  **
  **     1. The specified column name was rowid", "oid" or "_rowid_"
  **        and there is no explicitly declared IPK column.
  **
  **     2. The table is not a view and the column name identified an
  **        explicitly declared column. Copy meta information from *pCol.
  */
  if( pCol ){
    zDataType = sqlite3ColumnType(pCol,0);
    zCollSeq = sqlite3ColumnColl(pCol);
    notnull = pCol->notNull!=0;
    primarykey  = (pCol->colFlags & COLFLAG_PRIMKEY)!=0;
    autoinc = pTab->iPKey==iCol && (pTab->tabFlags & TF_Autoincrement)!=0;
  }else{
    zDataType = "INTEGER";
    primarykey = 1;
  }
  if( !zCollSeq ){
    zCollSeq = sqlite3StrBINARY;
  }

error_out:
  /* Whether the function call succeeded or failed, set the output parameters
  ** to whatever their local counterparts contain. If an error did occur,
  ** this has the effect of zeroing all output parameters.
  */
  if( pzDataType ) *pzDataType = zDataType;
  if( pzCollSeq ) *pzCollSeq = zCollSeq;
  if( pNotNull ) *pNotNull = notnull;
  if( pPrimaryKey ) *pPrimaryKey = primarykey;
  if( pAutoinc ) *pAutoinc = autoinc;

  if( SQLITE_OK==rc && !pTab ){
    sqlite3DbFree(db, zErrMsg);
    zErrMsg = sqlite3MPrintf(db, "no such table column: %s.%s", zTableName,
        zColumnName);
    rc = SQLITE_ERROR;
  }
  sqlite3ErrorWithMsg(db, rc, (zErrMsg?"%s":0), zErrMsg);
  sqlite3DbFree(db, zErrMsg);
  rc = sqlite3ApiExit(db, rc);
  return rc;
}

/*
** Sleep for a little while.  Return the amount of time slept.
*/
int sqlite3_sleep(int ms){
  sqlite3_vfs *pVfs;
  int rc;
  pVfs = sqlite3_vfs_find(0);
  if( pVfs==0 ) return 0;

  /* This function works in milliseconds, but the underlying OsSleep()
  ** API uses microseconds. Hence the 1000's.
  */
  rc = (sqlite3OsSleep(pVfs, ms<0 ? 0 : 1000*ms)/1000);
  return rc;
}

/*
** Enable or disable the extended result codes.
*/
int sqlite3_extended_result_codes(sqlite3 *db, int onoff){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  db->errMask = onoff ? 0xffffffff : 0xff;
  return SQLITE_OK;
}

/*
** Invoke the xFileControl method on a particular database.
*/
int sqlite3_file_control(sqlite3 *db, const char *zDbName, int op, void *pArg){
  int rc = SQLITE_ERROR;
  Btree *pBtree;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  pBtree = sqlite3DbNameToBtree(db, zDbName);
  if( pBtree ){
    Pager *pPager;
    sqlite3_file *fd;
    pPager = sqlite3BtreePager(pBtree);
    assert( pPager!=0 );
    fd = sqlite3PagerFile(pPager);
    assert( fd!=0 );
    if( op==SQLITE_FCNTL_FILE_POINTER ){
      *(sqlite3_file**)pArg = fd;
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_VFS_POINTER ){
      *(sqlite3_vfs**)pArg = sqlite3PagerVfs(pPager);
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_JOURNAL_POINTER ){
      *(sqlite3_file**)pArg = sqlite3PagerJrnlFile(pPager);
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_DATA_VERSION ){
      *(unsigned int*)pArg = sqlite3PagerDataVersion(pPager);
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_RESERVE_BYTES ){
      int iNew = *(int*)pArg;
      *(int*)pArg = sqlite3BtreeGetRequestedReserve(pBtree);
      if( iNew>=0 && iNew<=255 ){
        sqlite3BtreeSetPageSize(pBtree, 0, iNew, 0);
      }
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_RESET_CACHE ){
      sqlite3BtreeClearCache(pBtree);
      rc = SQLITE_OK;
    }else{
      rc = sqlite3OsFileControl(fd, op, pArg);
    }
  }
  return rc;
}

/*
** The Pager stores the Database filename and Journal filename consecutively
** in memory, in that order. The database filename is prefixed
** by four zero bytes.  Locate the start of the database filename by searching
** backwards for the first byte following four consecutive zero bytes.
**
** This only works if the filename passed in was obtained from the Pager.
*/
static const char *databaseName(const char *zName){
  while( zName[-1]!=0 || zName[-2]!=0 || zName[-3]!=0 || zName[-4]!=0 ){
    zName--;
  }
  return zName;
}

/*
** Append text z[] to the end of p[].  Return a pointer to the first
** character after then zero terminator on the new text in p[].
*/
static char *appendText(char *p, const char *z){
  size_t n = strlen(z);
  memcpy(p, z, n+1);
  return p+n+1;
}

/*
** Allocate memory to hold names for a database, journal file, and query
** parameters. The pointer returned is valid for use by
** sqlite3_filename_database() and sqlite3_uri_parameter() and related
** functions.
**
** Memory layout must be compatible with that generated by the pager
** and expected by sqlite3_uri_parameter() and databaseName().
*/
const char *sqlite3_create_filename(
  const char *zDatabase,
  const char *zJournal,
  int nParam,
  const char **azParam
){
  sqlite3_int64 nByte;
  int i;
  char *pResult, *p;
  nByte = strlen(zDatabase) + strlen(zJournal) + 9;
  for(i=0; i<nParam*2; i++){
    nByte += strlen(azParam[i])+1;
  }
  pResult = p = sqlite3_malloc64( nByte );
  if( p==0 ) return 0;
  memset(p, 0, 4);
  p += 4;
  p = appendText(p, zDatabase);
  for(i=0; i<nParam*2; i++){
    p = appendText(p, azParam[i]);
  }
  *(p++) = 0;
  p = appendText(p, zJournal);
  *(p++) = 0;
  *(p++) = 0;
  assert( (sqlite3_int64)(p - pResult)==nByte );
  return pResult + 4;
}

/*
** Free memory obtained from sqlite3_create_filename().  It is a severe
** error to call this routine with any parameter other than a pointer
** previously obtained from sqlite3_create_filename() or a NULL pointer.
*/
void sqlite3_free_filename(const char *p){
  if( p==0 ) return;
  p = databaseName(p);
  sqlite3_free((char*)p - 4);
}


/*
** This is a utility routine, useful to VFS implementations, that checks
** to see if a database file was a URI that contained a specific query
** parameter, and if so obtains the value of the query parameter.
**
** The zFilename argument is the filename pointer passed into the xOpen()
** method of a VFS implementation.  The zParam argument is the name of the
** query parameter we seek.  This routine returns the value of the zParam
** parameter if it exists.  If the parameter does not exist, this routine
** returns a NULL pointer.
*/
const char *sqlite3_uri_parameter(const char *zFilename, const char *zParam){
  if( zFilename==0 || zParam==0 ) return 0;
  zFilename = databaseName(zFilename);
  return uriParameter(zFilename, zParam);
}

/*
** Return a pointer to the name of Nth query parameter of the filename.
*/
const char *sqlite3_uri_key(const char *zFilename, int N){
  if( zFilename==0 || N<0 ) return 0;
  zFilename = databaseName(zFilename);
  zFilename += sqlite3Strlen30(zFilename) + 1;
  while( ALWAYS(zFilename) && zFilename[0] && (N--)>0 ){
    zFilename += sqlite3Strlen30(zFilename) + 1;
    zFilename += sqlite3Strlen30(zFilename) + 1;
  }
  return zFilename[0] ? zFilename : 0;
}

/*
** Return a boolean value for a query parameter.
*/
int sqlite3_uri_boolean(const char *zFilename, const char *zParam, int bDflt){
  const char *z = sqlite3_uri_parameter(zFilename, zParam);
  bDflt = bDflt!=0;
  return z ? sqlite3GetBoolean(z, bDflt) : bDflt;
}

/*
** Return a 64-bit integer value for a query parameter.
*/
sqlite3_int64 sqlite3_uri_int64(
  const char *zFilename,    /* Filename as passed to xOpen */
  const char *zParam,       /* URI parameter sought */
  sqlite3_int64 bDflt       /* return if parameter is missing */
){
  const char *z = sqlite3_uri_parameter(zFilename, zParam);
  sqlite3_int64 v;
  if( z && sqlite3DecOrHexToI64(z, &v)==0 ){
    bDflt = v;
  }
  return bDflt;
}

/*
** Translate a filename that was handed to a VFS routine into the corresponding
** database or journal file.
**
** It is an error to pass this routine a filename string that was not
** passed into the VFS from the SQLite core.  Doing so is similar to
** passing free() a pointer that was not obtained from malloc() - it is
** an error that we cannot easily detect but that will likely cause memory
** corruption.
*/
const char *sqlite3_filename_database(const char *zFilename){
  if( zFilename==0 ) return 0;
  return databaseName(zFilename);
}
const char *sqlite3_filename_journal(const char *zFilename){
  if( zFilename==0 ) return 0;
  zFilename = databaseName(zFilename);
  zFilename += sqlite3Strlen30(zFilename) + 1;
  while( ALWAYS(zFilename) && zFilename[0] ){
    zFilename += sqlite3Strlen30(zFilename) + 1;
    zFilename += sqlite3Strlen30(zFilename) + 1;
  }
  return zFilename + 1;
}

/*
** Return the Btree pointer identified by zDbName.  Return NULL if not found.
*/
Btree *sqlite3DbNameToBtree(sqlite3 *db, const char *zDbName){
  int iDb = zDbName ? sqlite3FindDbName(db, zDbName) : 0;
  return iDb<0 ? 0 : db->aDb[iDb].pBt;
}

/*
** Return the name of the N-th database schema.  Return NULL if N is out
** of range.
*/
const char *sqlite3_db_name(sqlite3 *db, int N){
  const char *zRet = 0;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  if( N>=0 && N<db->nDb ){
    zRet = db->aDb[N].zDbSName;
  }
  return zRet;
}

/*
** Return the filename of the database associated with a database
** connection.
*/
const char *sqlite3_db_filename(sqlite3 *db, const char *zDbName){
  Btree *pBt;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  pBt = sqlite3DbNameToBtree(db, zDbName);
  return pBt ? sqlite3BtreeGetFilename(pBt) : 0;
}

/*
** Return 1 if database is read-only or 0 if read/write.  Return -1 if
** no such database exists.
*/
int sqlite3_db_readonly(sqlite3 *db, const char *zDbName){
  Btree *pBt;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return -1;
  }
#endif
  pBt = sqlite3DbNameToBtree(db, zDbName);
  return pBt ? sqlite3BtreeIsReadonly(pBt) : -1;
}

#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
/*
** Given the name of a compile-time option, return true if that option
** was used and false if not.
**
** The name can optionally begin with "SQLITE_" but the "SQLITE_" prefix
** is not required for a match.
*/
int sqlite3_compileoption_used(const char *zOptName){
  int i, n;
  int nOpt;
  const char **azCompileOpt;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( zOptName==0 ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif

  azCompileOpt = sqlite3CompileOptions(&nOpt);

  if( sqlite3StrNICmp(zOptName, "SQLITE_", 7)==0 ) zOptName += 7;
  n = sqlite3Strlen30(zOptName);

  /* Since nOpt is normally in single digits, a linear search is
  ** adequate. No need for a binary search. */
  for(i=0; i<nOpt; i++){
    if( sqlite3StrNICmp(zOptName, azCompileOpt[i], n)==0
     && sqlite3IsIdChar((unsigned char)azCompileOpt[i][n])==0
    ){
      return 1;
    }
  }
  return 0;
}

/*
** Return the N-th compile-time option string.  If N is out of range,
** return a NULL pointer.
*/
const char *sqlite3_compileoption_get(int N){
  int nOpt;
  const char **azCompileOpt;
  azCompileOpt = sqlite3CompileOptions(&nOpt);
  if( N>=0 && N<nOpt ){
    return azCompileOpt[N];
  }
  return 0;
}
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */
