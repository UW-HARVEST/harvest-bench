source $::suite_root/glue/tester.tcl
do_execsql_test printf2-3.2 {
  SELECT printf('|%-110.100c|','*');
} {{|****************************************************************************************************          |}}
finish_test
