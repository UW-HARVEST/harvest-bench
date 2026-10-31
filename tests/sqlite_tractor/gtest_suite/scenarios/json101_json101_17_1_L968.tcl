source $::suite_root/glue/tester.tcl
do_execsql_test json101-17.1 {
  DROP TABLE IF EXISTS t1;
  DROP TABLE IF EXISTS t2;
  CREATE TABLE t1(a,b,c);
  CREATE TABLE t2(d);
  SELECT * FROM t1 LEFT JOIN t2 ON (SELECT b FROM json_each ORDER BY 1);
} {}
finish_test
