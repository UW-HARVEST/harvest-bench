source $::suite_root/glue/tester.tcl
do_execsql_test func9-131 {
  SELECT concat_ws(',',1,2,3,4,'',6,7,8,NULL,9,10,11,12);
} {1,2,3,4,,6,7,8,9,10,11,12}
finish_test
