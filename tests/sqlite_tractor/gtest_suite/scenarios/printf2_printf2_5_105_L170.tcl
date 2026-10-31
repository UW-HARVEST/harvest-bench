source $::suite_root/glue/tester.tcl
do_execsql_test printf2-5.105 {
  SELECT printf('(%-3.3c)',char(1492));
} {{(ההה)}}
finish_test
