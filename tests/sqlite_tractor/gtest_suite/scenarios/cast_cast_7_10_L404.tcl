source $::suite_root/glue/tester.tcl
do_execsql_test cast-7.10 {
  SELECT '' - 2851427734582196970;
} {-2851427734582196970}
finish_test
