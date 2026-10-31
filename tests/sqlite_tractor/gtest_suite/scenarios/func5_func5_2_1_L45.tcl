source $::suite_root/glue/tester.tcl
do_execsql_test func5-2.1 {
  CREATE TABLE t2(x,y);
  INSERT INTO t2 VALUES(1,2),(3,4),(5,6),(7,8);
  SELECT x, y FROM t2 WHERE x+5=5+x ORDER BY +x;
} {1 2 3 4 5 6 7 8}
finish_test
