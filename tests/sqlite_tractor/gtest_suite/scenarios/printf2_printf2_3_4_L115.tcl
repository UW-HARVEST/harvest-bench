source $::suite_root/glue/tester.tcl
do_execsql_test printf2-3.4 {
  SELECT printf('|%8.8c|%-8.8c|','*','*');
} {|********|********|}
finish_test
