source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.20 {
  SELECT format('<%6.8j>','いち, に, さん');
} {{<いち,   >}}
finish_test
