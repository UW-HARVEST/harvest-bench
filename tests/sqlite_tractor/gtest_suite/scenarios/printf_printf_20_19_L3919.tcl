source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.19 {
  SELECT format('<%!-6j>','いち');
} {{<いち    >}}
finish_test
