source $::suite_root/glue/tester.tcl
do_execsql_test cast-5.3 {
  SELECT CAST('123e+5' AS INTEGER);
  SELECT CAST('123e+5' AS NUMERIC);
  SELECT CAST('123e+5' AS REAL);
} {123 12300000 12300000.0}
finish_test
