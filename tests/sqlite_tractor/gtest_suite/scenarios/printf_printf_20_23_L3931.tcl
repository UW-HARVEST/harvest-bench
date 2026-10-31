source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.23 {
  SELECT format('<%.7j>','abc"""def');
} {{<abc\"\"\"d>}}
finish_test
