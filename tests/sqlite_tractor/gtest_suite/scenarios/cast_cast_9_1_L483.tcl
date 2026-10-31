source $::suite_root/glue/tester.tcl
do_execsql_test cast-9.1 {
  CREATE TABLE dual(dummy TEXT);
  INSERT INTO dual VALUES('X');
  SELECT CAST(4 AS NUMERIC);
} {4}
finish_test
