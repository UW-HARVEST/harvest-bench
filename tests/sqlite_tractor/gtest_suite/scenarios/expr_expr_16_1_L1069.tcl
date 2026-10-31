source $::suite_root/glue/tester.tcl
do_execsql_test expr-16.1 {
  CREATE TABLE t1(a,b,c);
  CREATE TABLE dual(dummy);
  INSERT INTO dual VALUES('X');
} {}
finish_test
