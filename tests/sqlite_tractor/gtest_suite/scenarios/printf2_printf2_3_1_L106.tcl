source $::suite_root/glue/tester.tcl
do_execsql_test printf2-3.1 {
  SELECT printf('|%110.100c|','*');
} {{|          ****************************************************************************************************|}}
finish_test
