source $::suite_root/glue/tester.tcl
do_execsql_test cast-7.11 {
  SELECT 0 - 2851427734582196970;
} {-2851427734582196970}
finish_test
