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

# import file doesn't exist
do_test shell5-1.4.1 {
  forcedelete FOO
  set res [catchcmd "test.db" {CREATE TABLE t1(a, b);
.import FOO t1}]
} {1 {line 2: cannot open "FOO"}}

# the remainder of these test cases require virtual tables.
#
ifcapable !vtab {
  puts "Skipping subsequent tests due to SQLITE_OMIT_VIRTUALTABLE"
  finish_test
  return
}

# empty import file
do_test shell5-1.4.2 {
  forcedelete shell5.csv
  set in [open shell5.csv w]
  close $in
  set res [catchcmd ":memory:" {
.mode list
ATTACH 'test.db' AS test;
.import -schema test shell5.csv t1
SELECT COUNT(*) FROM test.t1;}]
} {0 0}

# import file with 1 row, 1 column (expecting 2 cols)
do_test shell5-1.4.3 {
  set in [open shell5.csv w]
  puts $in "1"
  close $in
  set res [catchcmd ":memory:" {ATTACH 'test.db' AS test;
.import -schema test shell5.csv t1}]
} {1 {shell5.csv:1: expected 2 columns but found 1 - filling the rest with NULL}}

# import file with 1 row, 3 columns (expecting 2 cols)
do_test shell5-1.4.4 {
  set in [open shell5.csv w]
  puts $in "1|2|3"
  close $in
  set res [catchcmd ":memory: --list" {ATTACH 'test.db' AS test;
.import --schema test shell5.csv t1}]
} {1 {shell5.csv:1: expected 2 columns but found 3 - extras ignored}}

# import file with 1 row, 2 columns
do_test shell5-1.4.5 {
  set in [open shell5.csv w]
  puts $in "1|2"
  close $in
  set res [catchcmd "test.db -list" {DELETE FROM t1;
.import shell5.csv t1
SELECT COUNT(*) FROM t1;}]
} {0 1}

# import file with 2 rows, 2 columns
# note we end up with 3 rows because of the 1 row 
# imported above.
do_test shell5-1.4.6 {
  set in [open shell5.csv w]
  puts $in "2|3"
  puts $in "3|4"
  close $in
  set res [catchcmd ":memory:" {
.mode list
ATTACH 'test.db' AS test;
.import -schema test shell5.csv t1
SELECT COUNT(*) FROM test.t1;}]
} {0 3}

# import file with 1 row, 2 columns, using a comma
do_test shell5-1.4.7 {
  set in [open shell5.csv w]
  puts $in "4,5"
  close $in
  set res [catchcmd ":memory:" {
.mode list
ATTACH 'test.db' AS test;
.separator ,
.import --schema test shell5.csv t1
SELECT COUNT(*) FROM test.t1;}]
} {0 4}

# import file with 1 row, 2 columns, text data
do_test shell5-1.4.8.1 {
  set in [open shell5.csv w]
  puts $in "5|Now is the time for all good men to come to the aid of their country."
  close $in
  set res [catchcmd "test.db" {.mode list
.import shell5.csv t1
SELECT COUNT(*) FROM t1;}]
} {0 5}

do_test shell5-1.4.8.2 {
  catchcmd "test.db -list" {SELECT b FROM t1 WHERE a='5';}
} {0 {Now is the time for all good men to come to the aid of their country.}}

# import file with 1 row, 2 columns, quoted text data
# note that currently sqlite doesn't support quoted fields, and
# imports the entire field, quotes and all.
do_test shell5-1.4.9.1 {
  set in [open shell5.csv w]
  puts $in "6|'Now is the time for all good men to come to the aid of their country.'"
  close $in
  set res [catchcmd "test.db -list" {.import shell5.csv t1
SELECT COUNT(*) FROM t1;}]
} {0 6}

do_test shell5-1.4.9.2 {
  catchcmd "test.db -list" {SELECT b FROM t1 WHERE a='6';}
} {0 {'Now is the time for all good men to come to the aid of their country.'}}

# import file with 1 row, 2 columns, quoted text data
do_test shell5-1.4.10.1 {
  set in [open shell5.csv w]
  puts $in "7|\"Now is the time for all good men to come to the aid of their country.\""
  close $in
  set res [catchcmd "test.db -list" {.import shell5.csv t1
SELECT COUNT(*) FROM t1;}]
} {0 7}

do_test shell5-1.4.10.2 {
  catchcmd "test.db -list" {SELECT b FROM t1 WHERE a='7';}
} {0 {Now is the time for all good men to come to the aid of their country.}}

# import file with 2 rows, 2 columns and an initial BOM
#
do_test shell5-1.4.11 {
  set in [open shell5.csv wb]
  puts -nonewline $in "\xef\xbb\xbf"
  puts $in "2|3"
  puts $in "4|5"
  close $in
  set res [catchcmd "test.db -list" {CREATE TABLE t2(x INT, y INT);
.import shell5.csv t2
.mode quote
.header on
SELECT * FROM t2;}]
 string map {\n | \n\r |} $res
} {0 {'x','y'|2,3|4,5}}

# import file with 2 rows, 2 columns or text with an initial BOM
#
do_test shell5-1.4.12 {
  set in [open shell5.csv wb]
  puts $in "\xef\xbb\xbf\"two\"|3"
  puts $in "4|5"
  close $in
  set res [catchcmd "test.db -list" {DELETE FROM t2;
.import shell5.csv t2
.mode quote
.header on
SELECT * FROM t2;}]
 string map {\n | \n\r |} $res
} {0 {'x','y'|'two',3|4,5}}

# check importing very long field
do_test shell5-1.5.1 {
  set str [string repeat X 999]
  set in [open shell5.csv w]
  puts $in "8|$str"
  close $in
  set res [catchcmd "test.db -list" {.import shell5.csv t1
SELECT length(b) FROM t1 WHERE a='8';}]
} {0 999}

# try importing into a table with a large number of columns.
# This is limited by SQLITE_MAX_VARIABLE_NUMBER, which defaults to 999.
set cols 999
do_test shell5-1.6.1 {
  set data {}
  for {set i 1} {$i<$cols} {incr i} {
    append data "c$i|"
  }
  append data "c$cols\n";
  for {set i 1} {$i<$cols} {incr i} {
    append data "$i|"
  }
  append data "$cols"
  set in [open shell5.csv w]
  puts $in $data
  close $in
  set res [catchcmd "test.db -list" {DROP TABLE IF EXISTS t2;
.import shell5.csv t2
SELECT COUNT(*) FROM t2;}]
} {0 1}

# try importing a large number of rows
set rows 9999
do_test shell5-1.7.1 {
  set in [open shell5.csv w]
  puts $in a
  for {set i 1} {$i<=$rows} {incr i} {
    puts $in $i
  }
  close $in
  set res [catchcmd "test.db" {.mode csv
.import shell5.csv t3
.mode quote
SELECT COUNT(*) FROM t3;}]
} [list 0 $rows]
finish_test
