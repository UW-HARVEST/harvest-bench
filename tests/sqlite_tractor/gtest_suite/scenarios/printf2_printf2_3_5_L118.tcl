source $::suite_root/glue/tester.tcl
do_execsql_test printf2-3.5 {
  SELECT printf('|%7.8c|%-7.8c|','*','*');
} {|********|********|}
finish_test
