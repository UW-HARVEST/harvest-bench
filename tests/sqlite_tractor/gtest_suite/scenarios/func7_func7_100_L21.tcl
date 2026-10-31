source $::suite_root/glue/tester.tcl
do_execsql_test func7-100 {
  SELECT ceil(99.9), ceiling(-99.01), floor(17), floor(-17.99);
} {100.0 -99.0 17 -18.0}
finish_test
