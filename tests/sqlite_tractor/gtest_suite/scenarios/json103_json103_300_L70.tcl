source $::suite_root/glue/tester.tcl
do_execsql_test json103-300 {
  DROP TABLE IF EXISTS t1;
  CREATE TABLE t1(x);
  INSERT INTO t1 VALUES(1),('abc');
  SELECT
     json_group_array(x),
     json_group_array(json_object('x',x))
    FROM t1;
} {{[1,"abc"]} {[{"x":1},{"x":"abc"}]}}
finish_test
