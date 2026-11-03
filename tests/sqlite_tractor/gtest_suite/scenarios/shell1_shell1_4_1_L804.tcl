# 2009 Nov 11
#
# The author disclaims copyright to this source code.  In place of
# a legal notice, here is a blessing:
#
#    May you do good and not evil.
#    May you find forgiveness for yourself and forgive others.
#    May you share freely, never taking more than you give.
#
#***********************************************************************
#
# The focus of this file is testing the CLI shell tool.
#
# TESTRUNNER: shell
#

# Test plan:
#
#   shell1-1.*: Basic command line option handling.
#   shell1-2.*: Basic "dot" command token parsing.
#   shell1-3.*: Basic test that "dot" command can be called.
#   shell1-{4-8}.*: Test various "dot" commands's functionality.
#   shell1-9.*: Basic test that "dot" commands and SQL intermix ok.
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_cli_invocation]
db close
forcedelete test.db test.db-journal test.db-wal
sqlite3 db test.db


# Test the output of the ".dump" command
#
do_test shell1-4.1 {
  db close
  forcedelete test.db
  sqlite3 db test.db
  db eval {
    PRAGMA encoding=UTF16;
    CREATE TABLE t1(x);
    INSERT INTO t1 VALUES(null), (''), (1), (2.25), ('hello'), (x'807f');
    CREATE TABLE t3(x,y);
    INSERT INTO t3 VALUES(1,null), (2,''), (3,1),
                         (4,2.25), (5,'hello'), (6,x'807f');
  }
  catchcmd test.db {.dump}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE t1(x);
INSERT INTO t1 VALUES(NULL);
INSERT INTO t1 VALUES('');
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2.25);
INSERT INTO t1 VALUES('hello');
INSERT INTO t1 VALUES(x'807f');
CREATE TABLE t3(x,y);
INSERT INTO t3 VALUES(1,NULL);
INSERT INTO t3 VALUES(2,'');
INSERT INTO t3 VALUES(3,1);
INSERT INTO t3 VALUES(4,2.25);
INSERT INTO t3 VALUES(5,'hello');
INSERT INTO t3 VALUES(6,x'807f');
COMMIT;}}


ifcapable vtab {

# The --preserve-rowids option to .dump
#
do_test shell1-4.1.1 {
  catchcmd test.db {.dump --preserve-rowids}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE t1(x);
INSERT INTO t1(rowid,x) VALUES(1,NULL);
INSERT INTO t1(rowid,x) VALUES(2,'');
INSERT INTO t1(rowid,x) VALUES(3,1);
INSERT INTO t1(rowid,x) VALUES(4,2.25);
INSERT INTO t1(rowid,x) VALUES(5,'hello');
INSERT INTO t1(rowid,x) VALUES(6,x'807f');
CREATE TABLE t3(x,y);
INSERT INTO t3(rowid,x,y) VALUES(1,1,NULL);
INSERT INTO t3(rowid,x,y) VALUES(2,2,'');
INSERT INTO t3(rowid,x,y) VALUES(3,3,1);
INSERT INTO t3(rowid,x,y) VALUES(4,4,2.25);
INSERT INTO t3(rowid,x,y) VALUES(5,5,'hello');
INSERT INTO t3(rowid,x,y) VALUES(6,6,x'807f');
COMMIT;}}

# If the table contains an INTEGER PRIMARY KEY, do not record a separate
# rowid column in the output.
#
do_test shell1-4.1.2 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE t1(x INTEGER PRIMARY KEY, y);
    INSERT INTO t1 VALUES(1,null), (2,''), (3,1),
                         (4,2.25), (5,'hello'), (6,x'807f');
  }
  catchcmd test2.db {.dump --preserve-rowids}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE t1(x INTEGER PRIMARY KEY, y);
INSERT INTO t1(x,y) VALUES(1,NULL);
INSERT INTO t1(x,y) VALUES(2,'');
INSERT INTO t1(x,y) VALUES(3,1);
INSERT INTO t1(x,y) VALUES(4,2.25);
INSERT INTO t1(x,y) VALUES(5,'hello');
INSERT INTO t1(x,y) VALUES(6,x'807f');
COMMIT;}}

# Verify that the table named [table] is correctly quoted and that
# an INTEGER PRIMARY KEY DESC is not an alias for the rowid.
#
do_test shell1-4.1.3 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE [table](x INTEGER PRIMARY KEY DESC, y);
    INSERT INTO [table] VALUES(1,null), (12,''), (23,1),
                         (34,2.25), (45,'hello'), (56,x'807f');
  }
  catchcmd test2.db {.dump --preserve-rowids}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE [table](x INTEGER PRIMARY KEY DESC, y);
INSERT INTO "table"(rowid,x,y) VALUES(1,1,NULL);
INSERT INTO "table"(rowid,x,y) VALUES(2,12,'');
INSERT INTO "table"(rowid,x,y) VALUES(3,23,1);
INSERT INTO "table"(rowid,x,y) VALUES(4,34,2.25);
INSERT INTO "table"(rowid,x,y) VALUES(5,45,'hello');
INSERT INTO "table"(rowid,x,y) VALUES(6,56,x'807f');
COMMIT;}}

# Do not record rowids for a WITHOUT ROWID table.  Also check correct quoting
# of table names that contain odd characters.
#
do_test shell1-4.1.4 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE [ta<>ble](x INTEGER PRIMARY KEY, y) WITHOUT ROWID;
    INSERT INTO [ta<>ble] VALUES(1,null), (12,''), (23,1),
                         (34,2.25), (45,'hello'), (56,x'807f');
  }
  catchcmd test2.db {.dump --preserve-rowids}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE [ta<>ble](x INTEGER PRIMARY KEY, y) WITHOUT ROWID;
INSERT INTO "ta<>ble"(x,y) VALUES(1,NULL);
INSERT INTO "ta<>ble"(x,y) VALUES(12,'');
INSERT INTO "ta<>ble"(x,y) VALUES(23,1);
INSERT INTO "ta<>ble"(x,y) VALUES(34,2.25);
INSERT INTO "ta<>ble"(x,y) VALUES(45,'hello');
INSERT INTO "ta<>ble"(x,y) VALUES(56,x'807f');
COMMIT;}}

# Do not record rowids if the rowid is inaccessible
#
do_test shell1-4.1.5 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE t1(_ROWID_,rowid,oid);
    INSERT INTO t1 VALUES(1,null,'alpha'), (12,'',99), (23,1,x'b0b1b2');
  }
  catchcmd test2.db {.dump --preserve-rowids}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE t1(_ROWID_,rowid,oid);
INSERT INTO t1(_ROWID_,rowid,oid) VALUES(1,NULL,'alpha');
INSERT INTO t1(_ROWID_,rowid,oid) VALUES(12,'',99);
INSERT INTO t1(_ROWID_,rowid,oid) VALUES(23,1,x'b0b1b2');
COMMIT;}}

} else {

do_test shell1-4.1.6 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE t1(x INTEGER PRIMARY KEY, y);
    INSERT INTO t1 VALUES(1,null), (2,''), (3,1),
                         (4,2.25), (5,'hello'), (6,x'807f');
  }
  catchcmd test2.db {.dump --preserve-rowids}
} {/.* --preserve-rowids option is not compatible with SQLITE_OMIT_VIRTUALTABLE/}

}

# DELETE content of sqlite_sequence prior to repopulating,
# but only if the sqlite_sequence table is non-empty.
# Forum: 2024-10-13T17:10:01z and 2025-10-29T19:38:43z
#
do_test shell1-4.1.7 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE t1(a INTEGER PRIMARY KEY AUTOINCREMENT, b);
    INSERT INTO t1 VALUES(1,2),(20,21),(15,16);
  }
  catchcmd test2.db {.dump}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE t1(a INTEGER PRIMARY KEY AUTOINCREMENT, b);
INSERT INTO t1 VALUES(1,2);
INSERT INTO t1 VALUES(15,16);
INSERT INTO t1 VALUES(20,21);
PRAGMA writable_schema=ON;
CREATE TABLE IF NOT EXISTS sqlite_sequence(name,seq);
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('t1',20);
PRAGMA writable_schema=OFF;
COMMIT;}}
do_test shell1-4.1.8 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE t1(a INTEGER PRIMARY KEY AUTOINCREMENT, b);
    INSERT INTO t1 VALUES(1,2),(20,21),(15,16);
    CREATE TABLE t2(x,y);
    INSERT INTO t2 VALUES(99,88);
    DROP TABLE t1;
  }
  catchcmd test2.db {.dump}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE t2(x,y);
INSERT INTO t2 VALUES(99,88);
COMMIT;}}
do_test shell1-4.1.9 {
  db close
  forcedelete test2.db
  sqlite3 db test2.db
  db eval {
    CREATE TABLE t1(a INTEGER PRIMARY KEY AUTOINCREMENT, b);
    INSERT INTO t1 VALUES(1,2),(20,21),(15,16);
    CREATE TABLE t2(x,y);
    INSERT INTO t2 VALUES(99,88);
    INSERT INTO sqlite_sequence VALUES('extra',999);
    DROP TABLE t1;
  }
  catchcmd test2.db {.dump}
} {0 {PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE t2(x,y);
INSERT INTO t2 VALUES(99,88);
PRAGMA writable_schema=ON;
CREATE TABLE IF NOT EXISTS sqlite_sequence(name,seq);
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('extra',999);
PRAGMA writable_schema=OFF;
COMMIT;}}

# Test the output of ".mode insert"
#
do_test shell1-4.2.1 {
  catchcmd test.db ".mode insert t1\nselect * from t1;"
} {0 {INSERT INTO t1 VALUES(NULL);
INSERT INTO t1 VALUES('');
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2.25);
INSERT INTO t1 VALUES('hello');
INSERT INTO t1 VALUES(x'807f');}}

# Test the output of ".mode insert" with headers
#
do_test shell1-4.2.2 {
  catchcmd test.db ".mode insert t1\n.headers on\nselect * from t1;"
} {0 {INSERT INTO t1(x) VALUES(NULL);
INSERT INTO t1(x) VALUES('');
INSERT INTO t1(x) VALUES(1);
INSERT INTO t1(x) VALUES(2.25);
INSERT INTO t1(x) VALUES('hello');
INSERT INTO t1(x) VALUES(x'807f');}}

# Test the output of ".mode insert"
#
do_test shell1-4.2.3 {
  catchcmd test.db ".mode insert t3\nselect * from t3;"
} {0 {INSERT INTO t3 VALUES(1,NULL);
INSERT INTO t3 VALUES(2,'');
INSERT INTO t3 VALUES(3,1);
INSERT INTO t3 VALUES(4,2.25);
INSERT INTO t3 VALUES(5,'hello');
INSERT INTO t3 VALUES(6,x'807f');}}

# Test the output of ".mode insert" with headers
#
do_test shell1-4.2.4 {
  catchcmd test.db ".mode insert t3\n.headers on\nselect * from t3;"
} {0 {INSERT INTO t3(x,y) VALUES(1,NULL);
INSERT INTO t3(x,y) VALUES(2,'');
INSERT INTO t3(x,y) VALUES(3,1);
INSERT INTO t3(x,y) VALUES(4,2.25);
INSERT INTO t3(x,y) VALUES(5,'hello');
INSERT INTO t3(x,y) VALUES(6,x'807f');}}

# Test the output of ".mode tcl"
#
do_test shell1-4.3 {
  db close
  forcedelete test.db
  sqlite3 db test.db
  db eval {
    PRAGMA encoding=UTF8;
    CREATE TABLE t1(x);
    INSERT INTO t1 VALUES(null), (''), (1), (2.25), ('hello'), (x'807f');
  }
  catchcmd test.db ".mode tcl\nselect * from t1;"
} {0 {""
""
1
2.25
"hello"
"\200\177"}}

# Test the output of ".mode tcl" with multiple columns
#
do_test shell1-4.4 {
  db eval {
    CREATE TABLE t2(x,y);
    INSERT INTO t2 VALUES(null, ''), (1, 2.25), ('hello', x'807f');
  }
  catchcmd test.db ".mode tcl\nselect * from t2;"
} {0 {"" ""
1 2.25
"hello" "\200\177"}}

# Test the output of ".mode tcl" with ".nullvalue"
#
do_test shell1-4.5 {
  catchcmd test.db ".mode tcl\n.nullvalue NULL\nselect * from t2;"
} {0 {NULL ""
1 2.25
"hello" "\200\177"}}

# Test the output of ".mode tcl" with Tcl reserved characters
#
do_test shell1-4.6 {
  db eval {
    CREATE TABLE tcl1(x);
    INSERT INTO tcl1 VALUES('"'), ('['), (']'), ('\{'), ('\}'), (';'), ('$');
  }
  foreach {x y} [catchcmd test.db ".mode tcl\nselect * from tcl1;"] break
  list $x $y [llength $y]
} {0 {"\""
"["
"]"
"\\{"
"\\}"
";"
"$"} 7}

# Test the output of ".mode quote"
#
do_test shell1-4.7 {
  catchcmd test.db ".mode quote\nselect x'0123456789ABCDEF';"
} {0 x'0123456789abcdef'}
finish_test
