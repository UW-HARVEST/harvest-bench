source $::suite_root/glue/tester.tcl
do_execsql_test printf2-3.3 {
  SELECT printf('|%9.8c|%-9.8c|','*','*');
} {{| ********|******** |}}
finish_test
