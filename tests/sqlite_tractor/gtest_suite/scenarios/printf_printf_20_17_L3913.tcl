source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.17 {
  SELECT format('<%6j>','いち');
} {{<いち>}}
finish_test
