source $::suite_root/glue/tester.tcl
do_execsql_test func-33.10 {
  CREATE TABLE t33a(a,b);
  CREATE TABLE t33b(x,y);
  CREATE TRIGGER r1 AFTER INSERT ON t33a BEGIN
    INSERT INTO t33b(x,y) VALUES(testdirectonly(new.a),new.b);
  END;
} {}
finish_test
