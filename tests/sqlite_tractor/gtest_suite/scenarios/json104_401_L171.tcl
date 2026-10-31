source $::suite_root/glue/tester.tcl
do_execsql_test 401 {
  CREATE TABLE obj(x);
  INSERT INTO obj VALUES('{"a":1,"b":2}');
  SELECT * FROM obj;
} {{{"a":1,"b":2}}}
finish_test
