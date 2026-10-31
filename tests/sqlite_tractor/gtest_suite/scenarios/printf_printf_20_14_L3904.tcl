source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.14 {
  SELECT format('<%6.8J>','いち, に, さん');
} {{<"いち, ">}}
finish_test
