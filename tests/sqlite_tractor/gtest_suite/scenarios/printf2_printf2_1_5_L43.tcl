source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.5 {
  SELECT format('%d,%d,%d,%d',55,'-11',3421);
} {55,-11,3421,0}
finish_test
