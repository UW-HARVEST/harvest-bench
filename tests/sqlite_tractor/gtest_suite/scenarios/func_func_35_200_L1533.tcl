source $::suite_root/glue/tester.tcl
do_execsql_test func-35.200 {
  CREATE TABLE t0(c0 CHECK(ABS(-9223372036854775808)));
  PRAGMA integrity_check;
} {ok}
finish_test
