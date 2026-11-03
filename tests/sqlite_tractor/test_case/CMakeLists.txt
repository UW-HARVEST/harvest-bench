# © 2026 Massachusetts Institute of Technology
# MIT License

cmake_minimum_required(VERSION 3.16)

project(SQLite VERSION 3.54.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

#
# Build options
#
option(SQLITE_BUILD_SHARED                "Build libsqlite3.so"                      ON)
option(SQLITE_BUILD_CLI                   "Build the sqlite3 CLI shell"              ON)
option(SQLITE_BUILD_DYNAMIC_EXTENSIONS    "Build the sqlite loadable extensions"     OFF)
option(SQLITE_TEST                        "Build Tcl/test integration code"          OFF)
option(SQLITE_ENABLE_MALLOC               "Use system malloc"                        ON)
option(SQLITE_WITH_ZLIB                   "Enable zlib (.archive, zipfile, sqlar)"   ON)
option(SQLITE_WITH_EDITLINE               "CLI line editing via BSD editline"        OFF)
option(SQLITE_SHELL_ENABLE_RECOVER        "Build the CLI .recover command"           ON)
option(SQLITE_ENABLE_DEBUG                "Enable assert statements (slow)"          OFF)

# lib-features: extensions and core feature toggles
option(SQLITE_ENABLE_FTS3                 "Enable the FTS3 extension"                     ON)
option(SQLITE_ENABLE_FTS5                 "Enable the FTS5 extension"                     ON)
option(SQLITE_ENABLE_RTREE                "Enable the RTREE extension"                    ON)
option(SQLITE_ENABLE_GEOPOLY              "Enable the GEOPOLY extension (implies RTREE)"  ON)
option(SQLITE_ENABLE_SESSION              "Enable the SESSION extension"                  ON)
option(SQLITE_ENABLE_DBPAGE_VTAB          "Enable the sqlite3_dbpage extension"           ON)
option(SQLITE_ENABLE_DBSTAT_VTAB          "Enable the sqlite3_dbstat extension"           ON)
option(SQLITE_ENABLE_CARRAY               "Enable the CARRAY extension"                   ON)
option(SQLITE_ENABLE_COLUMN_METADATA      "Enable the column metadata APIs"               OFF)
option(SQLITE_ENABLE_UPDATE_DELETE_LIMIT  "Enable the UPDATE/DELETE LIMIT clause"         OFF)
option(SQLITE_ENABLE_MATH_FUNCTIONS       "Enable math functions (and percentile())"      ON)
option(SQLITE_ENABLE_JSON                 "Enable JSON functions"                         ON)
option(SQLITE_ENABLE_LOAD_EXTENSION       "Enable loading of external extensions"         ON)
option(SQLITE_ENABLE_MEMSYS5              "Enable MEMSYS5"                                OFF)
option(SQLITE_ENABLE_STMT_SCANSTATUS      "Enable the SQLITE_ENABLE_STMT_SCANSTATUS feature" OFF)
option(SQLITE_OMIT_AUTOINIT               "Remove autoinit feature, callers must call init"  OFF)
set(SQLITE_TEMP_STORE "no" CACHE STRING "In-RAM temp tables: never, no, yes or always")

if(SQLITE_ENABLE_GEOPOLY AND NOT SQLITE_ENABLE_RTREE)
  set(SQLITE_ENABLE_RTREE ON)
endif()

#
# Compile flags
#
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

#
# Preprocessor configuration
#
add_definitions(
  # Extensions are statically linked, not loadable.
  -DSQLITE_CORE

  -DSQLITE_ENABLE_BYTECODE_VTAB
  -DSQLITE_ENABLE_STMTVTAB
  -DSQLITE_ENABLE_EXPLAIN_COMMENTS
  -DSQLITE_ENABLE_OFFSET_SQL_FUNC
  -DSQLITE_ENABLE_UNKNOWN_SQL_FUNCTION
  -DSQLITE_DQS=0
  -DSQLITE_STRICT_SUBTYPE=1
)

if(SQLITE_ENABLE_MALLOC)
  add_definitions(-DSQLITE_SYSTEM_MALLOC)
else()
  add_definitions(-DSQLITE_ZERO_MALLOC)
endif()
if(SQLITE_WITH_ZLIB)
  add_definitions(-DSQLITE_HAVE_ZLIB=1)
endif()
if(SQLITE_ENABLE_DEBUG)
  add_definitions(-UNDEBUG)
else()
  add_definitions(-DNDEBUG)
endif()

# lib-features: extensions and core feature toggles
if(SQLITE_ENABLE_FTS3)
  add_definitions(-DSQLITE_ENABLE_FTS3)
endif()
if(SQLITE_ENABLE_FTS5)
  add_definitions(-DSQLITE_ENABLE_FTS5)
endif()
if(SQLITE_ENABLE_RTREE)
  add_definitions(-DSQLITE_ENABLE_RTREE)
endif()
if(SQLITE_ENABLE_GEOPOLY)
  add_definitions(-DSQLITE_ENABLE_GEOPOLY)
endif()
if(SQLITE_ENABLE_SESSION)
  add_definitions(-DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK)
endif()
if(SQLITE_ENABLE_DBPAGE_VTAB)
  add_definitions(-DSQLITE_ENABLE_DBPAGE_VTAB)
endif()
if(SQLITE_ENABLE_DBSTAT_VTAB)
  add_definitions(-DSQLITE_ENABLE_DBSTAT_VTAB)
endif()
if(SQLITE_ENABLE_CARRAY)
  add_definitions(-DSQLITE_ENABLE_CARRAY)
endif()
if(SQLITE_ENABLE_COLUMN_METADATA)
  add_definitions(-DSQLITE_ENABLE_COLUMN_METADATA)
endif()
if(SQLITE_ENABLE_UPDATE_DELETE_LIMIT)
  add_definitions(-DSQLITE_ENABLE_UPDATE_DELETE_LIMIT)
endif()
if(SQLITE_ENABLE_MATH_FUNCTIONS)
  add_definitions(-DSQLITE_ENABLE_MATH_FUNCTIONS -DSQLITE_ENABLE_PERCENTILE)
endif()
if(NOT SQLITE_ENABLE_JSON)
  add_definitions(-DSQLITE_OMIT_JSON)
endif()
if(NOT SQLITE_ENABLE_LOAD_EXTENSION)
  add_definitions(-DSQLITE_OMIT_LOAD_EXTENSION=1)
endif()
if(SQLITE_ENABLE_MEMSYS5)
  add_definitions(-DSQLITE_ENABLE_MEMSYS5)
endif()
if(SQLITE_ENABLE_STMT_SCANSTATUS)
  add_definitions(-DSQLITE_ENABLE_STMT_SCANSTATUS)
endif()
if(SQLITE_SHELL_ENABLE_RECOVER)
  add_definitions(-DSQLITE_SHELL_HAVE_RECOVER)
endif()
if(SQLITE_OMIT_AUTOINIT)
  add_definitions(-DSQLITE_OMIT_AUTOINIT)
endif()

if(SQLITE_TEMP_STORE STREQUAL "never")
  add_definitions(-DSQLITE_TEMP_STORE=0)
elseif(SQLITE_TEMP_STORE STREQUAL "no")
  add_definitions(-DSQLITE_TEMP_STORE=1)
elseif(SQLITE_TEMP_STORE STREQUAL "yes")
  add_definitions(-DSQLITE_TEMP_STORE=2)
elseif(SQLITE_TEMP_STORE STREQUAL "always")
  add_definitions(-DSQLITE_TEMP_STORE=3)
endif()

if(SQLITE_WITH_EDITLINE)
  add_definitions(-DHAVE_EDITLINE=1)
endif()

#
# Include directories
#
include_directories(
  include
  src
  src/ext/fts3
  src/ext/fts5
  src/ext/misc
  src/ext/qrf
  src/ext/rtree
  src/ext/session
  src/ext/recover
)

#
# Libraries
#
set(SQLITE_LIBS dl m)

if(SQLITE_WITH_ZLIB)
  list(APPEND SQLITE_LIBS z)
endif()

set(SQLITE_CLI_LIBS ${SQLITE_LIBS})

if(SQLITE_WITH_EDITLINE)
  list(APPEND SQLITE_CLI_LIBS edit)
endif()

#
# Sources
#
set(SQLITE_CORE_SOURCES
  src/alter.c      src/analyze.c    src/attach.c     src/auth.c
  src/backup.c     src/bitvec.c     src/btree.c      src/build.c      
  src/callback.c   src/carray.c     src/complete.c   src/ctime.c
  src/date.c       src/dbpage.c     src/dbstat.c     src/delete.c     
  src/expr.c       src/fkey.c       src/func.c       src/global.c     
  src/hash.c       src/insert.c     src/json.c       src/legacy.c     
  src/loadext.c    src/main.c       src/malloc.c     src/mem0.c       
  src/mem1.c       src/mem5.c       src/memdb.c      src/memjournal.c 
  src/opcodes.c    src/os.c         src/os_unix.c    src/pager.c      
  src/parse.c      src/pcache.c     src/pcache1.c    src/pragma.c     
  src/prepare.c    src/printf.c     src/random.c     src/resolve.c    
  src/rowset.c     src/select.c     src/status.c     src/table.c      
  src/tokenize.c   src/trigger.c    src/update.c     src/upsert.c     
  src/utf.c        src/util.c       src/vacuum.c     src/vdbe.c       
  src/vdbeapi.c    src/vdbeaux.c    src/vdbeblob.c   src/vdbemem.c    
  src/vdbesort.c   src/vdbetrace.c  src/vdbevtab.c   src/vtab.c       
  src/walker.c     src/where.c      src/wherecode.c  src/whereexpr.c  
  src/window.c
)

set(SQLITE_COMMON_EXTENSION_SOURCES
  src/ext/misc/amatch.c      src/ext/misc/appendvfs.c    src/ext/misc/basexx.c # Includes base64.c and base85.c
  src/ext/misc/closure.c     src/ext/misc/completion.c   src/ext/misc/csv.c
  src/ext/misc/decimal.c     src/ext/misc/diskused.c     src/ext/misc/eval.c
  src/ext/misc/explain.c     src/ext/misc/fileio.c       src/ext/misc/fuzzer.c
  src/ext/misc/ieee754.c     src/ext/misc/nextchar.c     src/ext/misc/prefixes.c
  src/ext/misc/qpvtab.c      src/ext/misc/randomjson.c   src/ext/misc/regexp.c
  src/ext/misc/remember.c    src/ext/misc/series.c       src/ext/misc/sha1.c
  src/ext/misc/shathree.c    src/ext/misc/spellfix.c     src/ext/misc/sqlar.c
  src/ext/misc/stmt.c        src/ext/misc/stmtrand.c
  src/ext/misc/totype.c      src/ext/misc/uint.c         src/ext/misc/unionvtab.c    
  src/ext/misc/wholenumber.c src/ext/misc/zipfile.c

  src/ext/fts3/fts3.c               src/ext/fts3/fts3_aux.c
  src/ext/fts3/fts3_expr.c          src/ext/fts3/fts3_hash.c
  src/ext/fts3/fts3_porter.c        src/ext/fts3/fts3_snippet.c     
  src/ext/fts3/fts3_tokenizer.c     src/ext/fts3/fts3_tokenizer1.c  
  src/ext/fts3/fts3_tokenize_vtab.c src/ext/fts3/fts3_unicode.c     
  src/ext/fts3/fts3_unicode2.c      src/ext/fts3/fts3_write.c

  src/ext/fts5/fts5_aux.c           src/ext/fts5/fts5_buffer.c
  src/ext/fts5/fts5_config.c        src/ext/fts5/fts5_expr.c
  src/ext/fts5/fts5_hash.c          src/ext/fts5/fts5_index.c
  src/ext/fts5/fts5_main.c          src/ext/fts5/fts5_storage.c
  src/ext/fts5/fts5_tokenize.c      src/ext/fts5/fts5_unicode2.c
  src/ext/fts5/fts5_varint.c        src/ext/fts5/fts5_vocab.c
  src/ext/fts5/fts5parse.c

  src/ext/qrf/qrf.c
  src/ext/rtree/rtree.c             # #includes geopoly.c; not a separate TU
  src/ext/session/sqlite3session.c
  src/ext/recover/sqlite3recover.c
  src/ext/recover/dbdata.c

)

set(SQLITE_TCL_TEST_SOURCES
  src/tclsqlite.c
  src/ext/fts3/fts3_tcl.c
  src/ext/fts5/fts5_tcl.c
  src/ext/rtree/test_rtreedoc.c
  src/ext/session/test_session.c
  src/ext/recover/test_recover.c
)
set(SQLITE_CLI_SOURCES src/shell.c)

#
# Targets
#
add_library(
  sqlite3_objs
  OBJECT
  ${SQLITE_CORE_SOURCES}
  ${SQLITE_COMMON_EXTENSION_SOURCES}
)

if(SQLITE_BUILD_SHARED)
  add_library(sqlite3_shared SHARED $<TARGET_OBJECTS:sqlite3_objs>)
  set_target_properties(sqlite3_shared PROPERTIES OUTPUT_NAME sqlite3)
  target_link_libraries(sqlite3_shared PRIVATE ${SQLITE_LIBS})

  if(SQLITE_TEST)
    find_package(TCL)
    target_sources(sqlite3_shared PRIVATE ${SQLITE_TCL_TEST_SOURCES})
    target_include_directories(sqlite3_shared PRIVATE include ${TCL_INCLUDE_PATH})
    target_link_libraries(sqlite3_shared PRIVATE ${TCL_LIBRARY})
  endif()
endif()

if(SQLITE_BUILD_CLI)
  add_executable(sqlite3_cli ${SQLITE_CLI_SOURCES} $<TARGET_OBJECTS:sqlite3_objs>)
  set_target_properties(sqlite3_cli PROPERTIES OUTPUT_NAME driver)
  target_link_libraries(sqlite3_cli ${SQLITE_CLI_LIBS})
endif()

if(SQLITE_BUILD_DYNAMIC_EXTENSIONS)
  add_subdirectory(ext/anycollseq)
  add_subdirectory(ext/btreeinfo)
  add_subdirectory(ext/cksumvfs)
  add_subdirectory(ext/compress)
  add_subdirectory(ext/percentile)
  add_subdirectory(ext/rot13)
  add_subdirectory(ext/uuid)
  add_subdirectory(ext/zorder)
endif()
