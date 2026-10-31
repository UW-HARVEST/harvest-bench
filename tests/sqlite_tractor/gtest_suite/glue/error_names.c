#include <sqlite3.h>
const char *harvest_error_name(int rc){
  const char *zName = 0;
  int i, origRc = rc;
  for(i=0; i<2 && zName==0; i++, rc &= 0xff){
    switch( rc ){
#ifdef SQLITE_OK
      case SQLITE_OK:                 zName = "SQLITE_OK";                break;
#endif
#ifdef SQLITE_ERROR
      case SQLITE_ERROR:              zName = "SQLITE_ERROR";             break;
#endif
#ifdef SQLITE_ERROR_SNAPSHOT
      case SQLITE_ERROR_SNAPSHOT:     zName = "SQLITE_ERROR_SNAPSHOT";    break;
#endif
#ifdef SQLITE_ERROR_RETRY
      case SQLITE_ERROR_RETRY:        zName = "SQLITE_ERROR_RETRY";       break;
#endif
#ifdef SQLITE_ERROR_MISSING_COLLSEQ
      case SQLITE_ERROR_MISSING_COLLSEQ:
#endif
                                zName = "SQLITE_ERROR_MISSING_COLLSEQ";   break;
#ifdef SQLITE_INTERNAL
      case SQLITE_INTERNAL:           zName = "SQLITE_INTERNAL";          break;
#endif
#ifdef SQLITE_PERM
      case SQLITE_PERM:               zName = "SQLITE_PERM";              break;
#endif
#ifdef SQLITE_ABORT
      case SQLITE_ABORT:              zName = "SQLITE_ABORT";             break;
#endif
#ifdef SQLITE_ABORT_ROLLBACK
      case SQLITE_ABORT_ROLLBACK:     zName = "SQLITE_ABORT_ROLLBACK";    break;
#endif
#ifdef SQLITE_BUSY
      case SQLITE_BUSY:               zName = "SQLITE_BUSY";              break;
#endif
#ifdef SQLITE_BUSY_RECOVERY
      case SQLITE_BUSY_RECOVERY:      zName = "SQLITE_BUSY_RECOVERY";     break;
#endif
#ifdef SQLITE_BUSY_SNAPSHOT
      case SQLITE_BUSY_SNAPSHOT:      zName = "SQLITE_BUSY_SNAPSHOT";     break;
#endif
#ifdef SQLITE_LOCKED
      case SQLITE_LOCKED:             zName = "SQLITE_LOCKED";            break;
#endif
#ifdef SQLITE_LOCKED_SHAREDCACHE
      case SQLITE_LOCKED_SHAREDCACHE: zName = "SQLITE_LOCKED_SHAREDCACHE";break;
#endif
#ifdef SQLITE_NOMEM
      case SQLITE_NOMEM:              zName = "SQLITE_NOMEM";             break;
#endif
#ifdef SQLITE_READONLY
      case SQLITE_READONLY:           zName = "SQLITE_READONLY";          break;
#endif
#ifdef SQLITE_READONLY_RECOVERY
      case SQLITE_READONLY_RECOVERY:  zName = "SQLITE_READONLY_RECOVERY"; break;
#endif
#ifdef SQLITE_READONLY_CANTINIT
      case SQLITE_READONLY_CANTINIT:  zName = "SQLITE_READONLY_CANTINIT"; break;
#endif
#ifdef SQLITE_READONLY_ROLLBACK
      case SQLITE_READONLY_ROLLBACK:  zName = "SQLITE_READONLY_ROLLBACK"; break;
#endif
#ifdef SQLITE_READONLY_DBMOVED
      case SQLITE_READONLY_DBMOVED:   zName = "SQLITE_READONLY_DBMOVED";  break;
#endif
#ifdef SQLITE_READONLY_DIRECTORY
      case SQLITE_READONLY_DIRECTORY: zName = "SQLITE_READONLY_DIRECTORY";break;
#endif
#ifdef SQLITE_INTERRUPT
      case SQLITE_INTERRUPT:          zName = "SQLITE_INTERRUPT";         break;
#endif
#ifdef SQLITE_IOERR
      case SQLITE_IOERR:              zName = "SQLITE_IOERR";             break;
#endif
#ifdef SQLITE_IOERR_READ
      case SQLITE_IOERR_READ:         zName = "SQLITE_IOERR_READ";        break;
#endif
#ifdef SQLITE_IOERR_SHORT_READ
      case SQLITE_IOERR_SHORT_READ:   zName = "SQLITE_IOERR_SHORT_READ";  break;
#endif
#ifdef SQLITE_IOERR_WRITE
      case SQLITE_IOERR_WRITE:        zName = "SQLITE_IOERR_WRITE";       break;
#endif
#ifdef SQLITE_IOERR_FSYNC
      case SQLITE_IOERR_FSYNC:        zName = "SQLITE_IOERR_FSYNC";       break;
#endif
#ifdef SQLITE_IOERR_DIR_FSYNC
      case SQLITE_IOERR_DIR_FSYNC:    zName = "SQLITE_IOERR_DIR_FSYNC";   break;
#endif
#ifdef SQLITE_IOERR_TRUNCATE
      case SQLITE_IOERR_TRUNCATE:     zName = "SQLITE_IOERR_TRUNCATE";    break;
#endif
#ifdef SQLITE_IOERR_FSTAT
      case SQLITE_IOERR_FSTAT:        zName = "SQLITE_IOERR_FSTAT";       break;
#endif
#ifdef SQLITE_IOERR_UNLOCK
      case SQLITE_IOERR_UNLOCK:       zName = "SQLITE_IOERR_UNLOCK";      break;
#endif
#ifdef SQLITE_IOERR_RDLOCK
      case SQLITE_IOERR_RDLOCK:       zName = "SQLITE_IOERR_RDLOCK";      break;
#endif
#ifdef SQLITE_IOERR_DELETE
      case SQLITE_IOERR_DELETE:       zName = "SQLITE_IOERR_DELETE";      break;
#endif
#ifdef SQLITE_IOERR_NOMEM
      case SQLITE_IOERR_NOMEM:        zName = "SQLITE_IOERR_NOMEM";       break;
#endif
#ifdef SQLITE_IOERR_ACCESS
      case SQLITE_IOERR_ACCESS:       zName = "SQLITE_IOERR_ACCESS";      break;
#endif
#ifdef SQLITE_IOERR_CHECKRESERVEDLOCK
      case SQLITE_IOERR_CHECKRESERVEDLOCK:
#endif
                                zName = "SQLITE_IOERR_CHECKRESERVEDLOCK"; break;
#ifdef SQLITE_IOERR_LOCK
      case SQLITE_IOERR_LOCK:         zName = "SQLITE_IOERR_LOCK";        break;
#endif
#ifdef SQLITE_IOERR_CLOSE
      case SQLITE_IOERR_CLOSE:        zName = "SQLITE_IOERR_CLOSE";       break;
#endif
#ifdef SQLITE_IOERR_DIR_CLOSE
      case SQLITE_IOERR_DIR_CLOSE:    zName = "SQLITE_IOERR_DIR_CLOSE";   break;
#endif
#ifdef SQLITE_IOERR_SHMOPEN
      case SQLITE_IOERR_SHMOPEN:      zName = "SQLITE_IOERR_SHMOPEN";     break;
#endif
#ifdef SQLITE_IOERR_SHMSIZE
      case SQLITE_IOERR_SHMSIZE:      zName = "SQLITE_IOERR_SHMSIZE";     break;
#endif
#ifdef SQLITE_IOERR_SHMLOCK
      case SQLITE_IOERR_SHMLOCK:      zName = "SQLITE_IOERR_SHMLOCK";     break;
#endif
#ifdef SQLITE_IOERR_SHMMAP
      case SQLITE_IOERR_SHMMAP:       zName = "SQLITE_IOERR_SHMMAP";      break;
#endif
#ifdef SQLITE_IOERR_SEEK
      case SQLITE_IOERR_SEEK:         zName = "SQLITE_IOERR_SEEK";        break;
#endif
#ifdef SQLITE_IOERR_DELETE_NOENT
      case SQLITE_IOERR_DELETE_NOENT: zName = "SQLITE_IOERR_DELETE_NOENT";break;
#endif
#ifdef SQLITE_IOERR_MMAP
      case SQLITE_IOERR_MMAP:         zName = "SQLITE_IOERR_MMAP";        break;
#endif
#ifdef SQLITE_IOERR_GETTEMPPATH
      case SQLITE_IOERR_GETTEMPPATH:  zName = "SQLITE_IOERR_GETTEMPPATH"; break;
#endif
#ifdef SQLITE_IOERR_CONVPATH
      case SQLITE_IOERR_CONVPATH:     zName = "SQLITE_IOERR_CONVPATH";    break;
#endif
#ifdef SQLITE_CORRUPT
      case SQLITE_CORRUPT:            zName = "SQLITE_CORRUPT";           break;
#endif
#ifdef SQLITE_CORRUPT_VTAB
      case SQLITE_CORRUPT_VTAB:       zName = "SQLITE_CORRUPT_VTAB";      break;
#endif
#ifdef SQLITE_NOTFOUND
      case SQLITE_NOTFOUND:           zName = "SQLITE_NOTFOUND";          break;
#endif
#ifdef SQLITE_FULL
      case SQLITE_FULL:               zName = "SQLITE_FULL";              break;
#endif
#ifdef SQLITE_CANTOPEN
      case SQLITE_CANTOPEN:           zName = "SQLITE_CANTOPEN";          break;
#endif
#ifdef SQLITE_CANTOPEN_NOTEMPDIR
      case SQLITE_CANTOPEN_NOTEMPDIR: zName = "SQLITE_CANTOPEN_NOTEMPDIR";break;
#endif
#ifdef SQLITE_CANTOPEN_ISDIR
      case SQLITE_CANTOPEN_ISDIR:     zName = "SQLITE_CANTOPEN_ISDIR";    break;
#endif
#ifdef SQLITE_CANTOPEN_FULLPATH
      case SQLITE_CANTOPEN_FULLPATH:  zName = "SQLITE_CANTOPEN_FULLPATH"; break;
#endif
#ifdef SQLITE_CANTOPEN_CONVPATH
      case SQLITE_CANTOPEN_CONVPATH:  zName = "SQLITE_CANTOPEN_CONVPATH"; break;
#endif
#ifdef SQLITE_CANTOPEN_SYMLINK
      case SQLITE_CANTOPEN_SYMLINK:   zName = "SQLITE_CANTOPEN_SYMLINK";  break;
#endif
#ifdef SQLITE_PROTOCOL
      case SQLITE_PROTOCOL:           zName = "SQLITE_PROTOCOL";          break;
#endif
#ifdef SQLITE_EMPTY
      case SQLITE_EMPTY:              zName = "SQLITE_EMPTY";             break;
#endif
#ifdef SQLITE_SCHEMA
      case SQLITE_SCHEMA:             zName = "SQLITE_SCHEMA";            break;
#endif
#ifdef SQLITE_TOOBIG
      case SQLITE_TOOBIG:             zName = "SQLITE_TOOBIG";            break;
#endif
#ifdef SQLITE_CONSTRAINT
      case SQLITE_CONSTRAINT:         zName = "SQLITE_CONSTRAINT";        break;
#endif
#ifdef SQLITE_CONSTRAINT_UNIQUE
      case SQLITE_CONSTRAINT_UNIQUE:  zName = "SQLITE_CONSTRAINT_UNIQUE"; break;
#endif
#ifdef SQLITE_CONSTRAINT_TRIGGER
      case SQLITE_CONSTRAINT_TRIGGER: zName = "SQLITE_CONSTRAINT_TRIGGER";break;
#endif
#ifdef SQLITE_CONSTRAINT_FOREIGNKEY
      case SQLITE_CONSTRAINT_FOREIGNKEY:
#endif
                                zName = "SQLITE_CONSTRAINT_FOREIGNKEY";   break;
#ifdef SQLITE_CONSTRAINT_CHECK
      case SQLITE_CONSTRAINT_CHECK:   zName = "SQLITE_CONSTRAINT_CHECK";  break;
#endif
#ifdef SQLITE_CONSTRAINT_PRIMARYKEY
      case SQLITE_CONSTRAINT_PRIMARYKEY:
#endif
                                zName = "SQLITE_CONSTRAINT_PRIMARYKEY";   break;
#ifdef SQLITE_CONSTRAINT_NOTNULL
      case SQLITE_CONSTRAINT_NOTNULL: zName = "SQLITE_CONSTRAINT_NOTNULL";break;
#endif
#ifdef SQLITE_CONSTRAINT_COMMITHOOK
      case SQLITE_CONSTRAINT_COMMITHOOK:
#endif
                                zName = "SQLITE_CONSTRAINT_COMMITHOOK";   break;
#ifdef SQLITE_CONSTRAINT_VTAB
      case SQLITE_CONSTRAINT_VTAB:    zName = "SQLITE_CONSTRAINT_VTAB";   break;
#endif
#ifdef SQLITE_CONSTRAINT_FUNCTION
      case SQLITE_CONSTRAINT_FUNCTION:
#endif
                                zName = "SQLITE_CONSTRAINT_FUNCTION";     break;
#ifdef SQLITE_CONSTRAINT_ROWID
      case SQLITE_CONSTRAINT_ROWID:   zName = "SQLITE_CONSTRAINT_ROWID";  break;
#endif
#ifdef SQLITE_MISMATCH
      case SQLITE_MISMATCH:           zName = "SQLITE_MISMATCH";          break;
#endif
#ifdef SQLITE_MISUSE
      case SQLITE_MISUSE:             zName = "SQLITE_MISUSE";            break;
#endif
#ifdef SQLITE_NOLFS
      case SQLITE_NOLFS:              zName = "SQLITE_NOLFS";             break;
#endif
#ifdef SQLITE_AUTH
      case SQLITE_AUTH:               zName = "SQLITE_AUTH";              break;
#endif
#ifdef SQLITE_FORMAT
      case SQLITE_FORMAT:             zName = "SQLITE_FORMAT";            break;
#endif
#ifdef SQLITE_RANGE
      case SQLITE_RANGE:              zName = "SQLITE_RANGE";             break;
#endif
#ifdef SQLITE_NOTADB
      case SQLITE_NOTADB:             zName = "SQLITE_NOTADB";            break;
#endif
#ifdef SQLITE_ROW
      case SQLITE_ROW:                zName = "SQLITE_ROW";               break;
#endif
#ifdef SQLITE_NOTICE
      case SQLITE_NOTICE:             zName = "SQLITE_NOTICE";            break;
#endif
#ifdef SQLITE_NOTICE_RECOVER_WAL
      case SQLITE_NOTICE_RECOVER_WAL: zName = "SQLITE_NOTICE_RECOVER_WAL";break;
#endif
#ifdef SQLITE_NOTICE_RECOVER_ROLLBACK
      case SQLITE_NOTICE_RECOVER_ROLLBACK:
#endif
                                zName = "SQLITE_NOTICE_RECOVER_ROLLBACK"; break;
#ifdef SQLITE_NOTICE_RBU
      case SQLITE_NOTICE_RBU:         zName = "SQLITE_NOTICE_RBU"; break;
#endif
#ifdef SQLITE_WARNING
      case SQLITE_WARNING:            zName = "SQLITE_WARNING";           break;
#endif
#ifdef SQLITE_WARNING_AUTOINDEX
      case SQLITE_WARNING_AUTOINDEX:  zName = "SQLITE_WARNING_AUTOINDEX"; break;
#endif
#ifdef SQLITE_DONE
      case SQLITE_DONE:               zName = "SQLITE_DONE";              break;
#endif
    }
  }
  if( zName==0 ){
    static char zBuf[50];
    sqlite3_snprintf(sizeof(zBuf), zBuf, "SQLITE_UNKNOWN(%d)", origRc);
    zName = zBuf;
  }
  return zName;
}
