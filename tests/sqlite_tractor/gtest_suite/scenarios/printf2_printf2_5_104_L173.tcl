source $::suite_root/glue/tester.tcl
do_execsql_test printf2-5.104 {
  SELECT printf('(%2c)',char(1513));
} {{( ש)}}
finish_test
