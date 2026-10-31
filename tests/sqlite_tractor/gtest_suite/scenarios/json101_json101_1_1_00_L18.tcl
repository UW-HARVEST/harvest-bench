source $::suite_root/glue/tester.tcl
do_execsql_test json101-1.1.00 {
  SELECT json_array(1,2.5,null,'hello');
} {[1,2.5,null,"hello"]}
finish_test
