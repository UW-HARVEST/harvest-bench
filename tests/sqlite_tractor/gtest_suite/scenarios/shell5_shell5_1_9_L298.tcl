# 2010 August 4
#
# The author disclaims copyright to this source code.  In place of
# a legal notice, here is a blessing:
#
#    May you do good and not evil.
#    May you find forgiveness for yourself and forgive others.
#    May you share freely, never taking more than you give.
#
#***********************************************************************
# TESTRUNNER: shell
#
# The focus of this file is testing the CLI shell tool.
# These tests are specific to the .import command.
#
# $Id: shell5.test,v 1.7 2009/07/17 16:54:48 shaneh Exp $
#

# Test plan:
#
#   shell5-1.*: Basic tests specific to the ".import" command.
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_cli_invocation]
db close
forcedelete test.db test.db-journal test.db-wal

# Import columns containing quoted strings
do_test shell5-1.9 {
  set out [open shell5.csv w]
  fconfigure $out -translation lf
  puts $out {1,"",11}
  puts $out {2,"x",22}
  puts $out {3,"""",33}
  puts $out {4,"hello",44}
  puts $out "5,55,\"\"\r"
  puts $out {6,66,"x"}
  puts $out {7,77,""""}
  puts $out {8,88,"hello"}
  puts $out {"",9,99}
  puts $out {"x",10,110}
  puts $out {"""",11,121}
  puts $out {"hello",12,132}
  close $out
  catch {db close}
  forcedelete test.db
  catchcmd test.db {.mode csv
    CREATE TABLE t1(a,b,c);
.import shell5.csv t1
  }
  sqlite3 db test.db
  db eval {SELECT *, '|' FROM t1 ORDER BY rowid}
} {1 {} 11 | 2 x 22 | 3 {"} 33 | 4 hello 44 | 5 55 {} | 6 66 x | 7 77 {"} | 8 88 hello | {} 9 99 | x 10 110 | {"} 11 121 | hello 12 132 |}
db close

# Import columns containing quoted strings
do_test shell5-1.10 {
  set out [open shell5.csv w]
  fconfigure $out -translation lf
  puts $out {column1,column2,column3,column4}
  puts $out "field1,field2,\"x3 \"\"\r\ndata\"\" 3\",field4"
  puts $out "x1,x2,\"x3 \"\"\ndata\"\" 3\",x4"
  close $out
  forcedelete test.db
  catchcmd test.db {.mode csv
    CREATE TABLE t1(a,b,c,d);
.import shell5.csv t1
  }
  sqlite3 db test.db
  db eval {SELECT hex(c) FROM t1 ORDER BY rowid}
} {636F6C756D6E33 783320220D0A64617461222033 783320220A64617461222033}

# The --escape option
#
do_test shell5-1.10.1 {
  set out [open shell5.csv w]
  fconfigure $out -translation lf
  puts $out {column1,column2,column3,column4}
  puts $out "x1,x2%\"x3,\"x3\\\"data\\\"3\",x4"
  close $out
  db close
  forcedelete test.db
  catchcmd test.db {
    CREATE TABLE t1(a,b,c,d);
.import --csv --qesc \\ --esc % shell5.csv t1
  }
  sqlite3 db test.db
  db eval {SELECT b, c FROM t1 ORDER BY rowid}
} {column2 column3 x2\"x3 x3\"data\"3}

# Blank last column with \r\n line endings.
do_test shell5-1.11 {
  set out [open shell5.csv w]
  fconfigure $out -translation binary
  puts $out "column1,column2,column3\r"
  puts $out "a,b, \r"
  puts $out "x,y,\r"
  puts $out "p,q,r\r"
  close $out
  catch {db close}
  forcedelete test.db
  catchcmd test.db {.mode csv
.import shell5.csv t1
  }
  sqlite3 db test.db
  db eval {SELECT *, '|' FROM t1}
} {a b { } | x y {} | p q r |}
db close

#----------------------------------------------------------------------------
# 
reset_db
sqlite3 db test.db
do_test shell5-2.1 {
  set fd [open shell5.csv w]
  puts $fd ",hello"
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t1(a, b);
.import shell5.csv t1
  }]
  db eval { SELECT * FROM t1 }
} {{} hello}

do_test shell5-2.2 {
  set fd [open shell5.csv w]
  puts $fd {"",hello}
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t2(a, b);
.import shell5.csv t2
  }]
  db eval { SELECT * FROM t2 }
} {{} hello}

do_test shell5-2.3 {
  set fd [open shell5.csv w]
  puts $fd {"x""y",hello}
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t3(a, b);
.import shell5.csv t3
  }]
  db eval { SELECT * FROM t3 }
} {x\"y hello}

do_test shell5-2.4 {
  set fd [open shell5.csv w]
  puts $fd {"xy""",hello}
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t4(a, b);
.import shell5.csv t4
  }]
  db eval { SELECT * FROM t4 }
} {xy\" hello}

do_test shell5-2.5 {
  set fd [open shell5.csv w]
  puts $fd {"one","2"}
  puts $fd {}
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t4(a, b);
.import shell5.csv t4
  }]
  db eval { SELECT * FROM t4 }
} {xy\" hello one 2 {} {}}

#----------------------------------------------------------------------------
# Tests for the shell "ascii" import/export mode.
#
do_test shell5-3.1 {
  set fd [open shell5.csv w]
  fconfigure $fd -translation binary
  puts -nonewline $fd "\"test 1\"\x1F,test 2\r\n\x1E"
  puts -nonewline $fd "test 3\x1Ftest 4\n"
  close $fd
  catchcmd test.db {
.mode ascii
CREATE TABLE t5(a, b);
.import shell5.csv t5
  }
  db eval { SELECT * FROM t5 }
} "\{\"test 1\"} \{,test 2\r\n\} \{test 3\} \{test 4\n\}"

do_test shell5-3.2 {
  set x [catchcmd test.db {
.mode ascii
SELECT * FROM t5;
  }]
  # Handle platform end-of-line differences
  regsub -all {[\n\r]?\n} $x <EOL> x
  set x
} "0 \{\"test 1\"\x1F,test 2<EOL>\x1Etest 3\x1Ftest 4<EOL>\x1E\}"

do_test shell5-4.1 {
  forcedelete shell5.csv
  set fd [open shell5.csv w]
  puts $fd "1,2,3"
  puts $fd "4,5"
  puts $fd "6,7,8"
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t6(a, b, c);
.import shell5.csv t6
  }]
  db eval { SELECT * FROM t6 ORDER BY a }
} {1 2 3 4 5 {} 6 7 8}

do_test shell5-4.2 {
  forcedelete shell5.csv
  set fd [open shell5.csv w]
  puts $fd "1,2,3"
  puts $fd "4,5"
  puts $fd "6,7,8,9"
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t7(a, b, c);
.import shell5.csv t7
  }]
  db eval { SELECT * FROM t7 ORDER BY a }
} {1 2 3 4 5 {} 6 7 8}

do_test shell5-4.3 {
  forcedelete shell5.csv
  set fd [open shell5.csv w]
  puts $fd ",,"
  puts $fd "1,2,3"
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TABLE t8(a, b, c);
.import -skip 1 shell5.csv t8
.nullvalue #
  }]
  db eval { SELECT * FROM t8 }
} {1 2 3}

do_test shell5-4.4 {
  forcedelete shell5.csv
  set fd [open shell5.csv w]
  puts $fd "1,2,3"
  close $fd
  catchcmd test.db [string trim {
.mode csv
CREATE TEMP TABLE t8(a, b, c);
.import shell5.csv t8
.nullvalue #
.mode quote
SELECT * FROM temp.t8
  }]
} {0 '1','2','3'}

#----------------------------------------------------------------------------
# Tests for the shell automatic column rename.
#
db close

# Import columns containing duplicates
do_test shell5-5.1 {
  set out [open shell5.csv w]
  fconfigure $out -translation lf
  puts $out {"","x","x","y","z","z_0","z_5","z"}
  puts $out {0,"x2","x3","y4","z5","z6","z7","z8"}
  close $out
  forcedelete test.db
  catchcmd test.db {.import -csv shell5.csv t1
.mode line --colsep ' = '
SELECT * FROM t1;}
} {1 {    ? = 0
 x_02 = x2
 x_03 = x3
    y = y4
 z_05 = z5
  z_0 = z6
  z_5 = z7
 z_08 = z8
Columns renamed during .import shell5.csv due to duplicates:
"x" to "x_02",
"x" to "x_03",
"z" to "z_05",
"z" to "z_08"}}
finish_test
