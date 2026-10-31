source $::suite_root/glue/tester.tcl
do_execsql_test printf2-2.1 {
  CREATE TABLE t1(a,b,c);
  INSERT INTO t1 VALUES(1,2,3);
  INSERT INTO t1 VALUES(-1,-2,-3);
  INSERT INTO t1 VALUES('abc','def','ghi');
  INSERT INTO t1 VALUES(1.5,2.25,3.125);
  SELECT printf('(%s)-%n-(%s)',a,b,c) FROM t1 ORDER BY rowid;
} {(1)--(2) (-1)--(-2) (abc)--(def) (1.5)--(2.25)}
finish_test
