source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.21 {
  SELECT format('<%!6.2j>','いち, に, さん');
} {{<    いち>}}
finish_test
