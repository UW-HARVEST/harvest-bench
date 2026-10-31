source $::suite_root/glue/tester.tcl
do_execsql_test func9-133 {
  SELECT concat_ws(',',NULL,'',3,4,'',6,7,8,NULL,9,10,11,12);
} {,3,4,,6,7,8,9,10,11,12}
finish_test
