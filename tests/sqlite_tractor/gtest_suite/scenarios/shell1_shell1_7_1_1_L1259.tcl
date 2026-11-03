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

# Test using arbitrary byte data with the shell via standard input/output.
#
if 0 {  # Causes a valgrind error in TCL.  Seems to be a TCL problem.
do_test shell1-5.0 {
  #
  # NOTE: Skip NUL byte because it appears to be incompatible with command
  #       shell argument parsing.
  #
  for {set i 1} {$i < 256} {incr i} {
    #
    # NOTE: Due to how the Tcl [exec] command works (i.e. where it treats
    #       command channels opened for it as textual ones), the carriage
    #       return character (and on Windows, the end-of-file character)
    #       cannot be used here.
    #
    if {$i==0x0D || ($tcl_platform(platform) eq "windows" && $i==0x1A)} {
      continue
    }
    # Tcl 8.7 maps 0x80 through 0x9f into valid UTF8.  So skip those tests.
    if {$i>=0x80} {
      if {$i<=0x9F || $tcl_version>=9.0} continue
      if {$tcl_platform(platform) eq "windows"} continue
    }
    if {$i>=0xE0 && $tcl_platform(os) eq "OpenBSD"}  continue
    if {$i>=0xE0 && $i<=0xEF && $tcl_platform(os) eq "Linux"}  continue
    set hex [format %02X $i]
    set char [subst \\x$hex]; set oldChar $char
    set escapes [list]
    if {$tcl_platform(platform) eq "windows"} {
      #
      # NOTE: On Windows, we need to escape all the whitespace characters,
      #       the alarm (\a) character, and those with special meaning to
      #       the SQLite shell itself.
      #
      set escapes [list \
          \a \\a \b \\b \t \\t \n \\n \v \\v \f \\f \r \\r \
          " " "\" \"" \" \\\" \\ \\\\]
    } else {
      #
      # NOTE: On Unix, we need to escape most of the whitespace characters
      #       and those with special meaning to the SQLite shell itself.
      #       The alarm (\a), backspace (\b), and carriage-return (\r)
      #       characters do not appear to require escaping on Unix.  For
      #       the alarm and backspace characters, this is probably due to
      #       differences in the command shell.  For the carriage-return,
      #       it is probably due to differences in how Tcl handles command
      #       channel end-of-line translations.
      #
      set escapes [list \
          \t \\t \n \\n \v \\v \f \\f \
          " " "\" \"" \" \\\" \\ \\\\]
    }
    set char [string map $escapes $char]
    set x [catchcmdex test.db ".print \"$char\"\n"]
    set code [lindex $x 0]
    set res [lindex $x 1]
    if {$code ne "0"} {
      error "failed with error: $res"
    }
    if {$res ne "$oldChar\n"} {
      if {[llength $res] > 0} {
        set got [format %02X [scan $res %c]]
      } else {
        set got <empty>
      }
      error "failed with byte $hex mismatch, got $got"
    }
  }
} {}
}

# These test cases do not work on MinGW
if 0 {

# The string used here is the word "test" in Chinese.
# In UTF-8, it is encoded as: \xE6\xB5\x8B\xE8\xAF\x95
set test \u6D4B\u8BD5

do_test shell1-6.0 {
  set fileName $test; append fileName .db
  catch {forcedelete $fileName}
  set x [catchcmdex $fileName "CREATE TABLE t1(x);\n.schema\n"]
  set code [lindex $x 0]
  set res [string trim [lindex $x 1]]
  if {$code ne "0"} {
    error "failed with error: $res"
  }
  if {$res ne "CREATE TABLE t1(x);"} {
    error "failed with mismatch: $res"
  }
  if {![file exists $fileName]} {
    error "file \"$fileName\" (Unicode) does not exist"
  }
  forcedelete $fileName
} {}

do_test shell1-6.1 {
  catch {forcedelete test3.db}
  set x [catchcmdex test3.db \
      "CREATE TABLE [encoding convertto utf-8 $test](x);\n.schema\n"]
  set code [lindex $x 0]
  set res [string trim [lindex $x 1]]
  if {$code ne "0"} {
    error "failed with error: $res"
  }
  if {$res ne "CREATE TABLE ${test}(x);"} {
    error "failed with mismatch: $res"
  }
  forcedelete test3.db
} {}
}

db close
forcedelete test.db test.db-journal test.db-wal
sqlite3 db test.db
ifcapable vtab {

do_test shell1-7.1.1 {
  db eval {
    CREATE TABLE Z (x TEXT PRIMARY KEY);
    CREATE TABLE _ (x TEXT PRIMARY KEY);
    CREATE TABLE YY (x TEXT PRIMARY KEY);
    CREATE TABLE __ (x TEXT PRIMARY KEY);
    CREATE TABLE WWW (x TEXT PRIMARY KEY);
    CREATE TABLE ___ (x TEXT PRIMARY KEY);
  }
} {}
do_test shell1-7.1.2 {
  catchcmd "test.db" ".schema _"
} {0 {CREATE TABLE Z (x TEXT PRIMARY KEY);
CREATE TABLE _ (x TEXT PRIMARY KEY);}}
do_test shell1-7.1.3 {
  catchcmd "test.db" ".schema \"\\\\_\""
} {0 {CREATE TABLE _ (x TEXT PRIMARY KEY);}}
do_test shell1-7.1.4 {
  catchcmd "test.db" ".schema __"
} {0 {CREATE TABLE YY (x TEXT PRIMARY KEY);
CREATE TABLE __ (x TEXT PRIMARY KEY);}}
do_test shell1-7.1.5 {
  catchcmd "test.db" ".schema \"\\\\_\\\\_\""
} {0 {CREATE TABLE __ (x TEXT PRIMARY KEY);}}
do_test shell1-7.1.6 {
  catchcmd "test.db" ".schema ___"
} {0 {CREATE TABLE WWW (x TEXT PRIMARY KEY);
CREATE TABLE ___ (x TEXT PRIMARY KEY);}}
do_test shell1-7.1.7 {
  catchcmd "test.db" ".schema \"\\\\_\\\\_\\\\_\""
} {0 {CREATE TABLE ___ (x TEXT PRIMARY KEY);}}
}
finish_test
