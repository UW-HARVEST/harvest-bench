source $::suite_root/glue/tester.tcl
do_execsql_test date2-100 {
  CREATE TABLE t1(x, y, CHECK( date(x) BETWEEN '2017-07-01' AND '2017-07-31' ));
  INSERT INTO t1(x,y) VALUES('2017-07-20','one');
} {}
finish_test
