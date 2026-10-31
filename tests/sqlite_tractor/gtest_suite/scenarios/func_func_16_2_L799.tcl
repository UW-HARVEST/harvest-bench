source $::suite_root/glue/tester.tcl
do_execsql_test func-16.2 {
  SELECT quote(4.2e+859), quote(-7.8e+904);
} {9.0e+999 -9.0e+999}
finish_test
