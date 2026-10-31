source $::suite_root/glue/tester.tcl
do_execsql_test json101-8.1 {
  DROP TABLE IF EXISTS t8;
  CREATE TABLE t8(a,b);
  INSERT INTO t8(a) VALUES('abc' || char(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35) || 'xyz');
  UPDATE t8 SET b=json_array(a);
  SELECT b FROM t8;
} {{["abc\u0001\u0002\u0003\u0004\u0005\u0006\u0007\b\t\n\u000b\f\r\u000e\u000f\u0010\u0011\u0012\u0013\u0014\u0015\u0016\u0017\u0018\u0019\u001a\u001b\u001c\u001d\u001e\u001f !\"#xyz"]}}
finish_test
