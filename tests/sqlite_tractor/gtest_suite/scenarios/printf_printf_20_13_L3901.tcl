source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.13 {
  SELECT format('<%!-6J>','いち');
} {{<"いち"  >}}
finish_test
