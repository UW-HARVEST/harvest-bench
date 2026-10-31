source $::suite_root/glue/tester.tcl
do_execsql_test printf2-5.102 {
  SELECT printf('(%5.3c)',char(1492));
} {{(  ההה)}}
finish_test
