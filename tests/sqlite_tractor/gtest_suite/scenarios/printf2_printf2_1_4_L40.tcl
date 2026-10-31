source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.4 {
  SELECT printf('%d,%d,%d',55,'-11',3421);
} {55,-11,3421}
finish_test
