source $::suite_root/glue/tester.tcl
do_execsql_test json101-13.100 {
  DROP TABLE IF EXISTS t1;
  DROP TABLE IF EXISTS t2;
  CREATE TABLE t1(id, json);
  INSERT INTO t1(id,json) VALUES(1,'{"items":[3,5]}');
  CREATE TABLE t2(id, json);
  INSERT INTO t2(id,json) VALUES(2,'{"value":2}');
  INSERT INTO t2(id,json) VALUES(3,'{"value":3}');
  INSERT INTO t2(id,json) VALUES(4,'{"value":4}');
  INSERT INTO t2(id,json) VALUES(5,'{"value":5}');
  INSERT INTO t2(id,json) VALUES(6,'{"value":6}');
  SELECT *, 'NL' FROM t1 CROSS JOIN t2
   WHERE EXISTS(SELECT 1 FROM json_each(t1.json,'$.items') AS Z
                 WHERE Z.value==t2.id);
} {1 {{"items":[3,5]}} 3 {{"value":3}} NL 1 {{"items":[3,5]}} 5 {{"value":5}} NL}
finish_test
