source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.27 {
  SELECT format('<%.0J>','abcdef');
} {{<"">}}
finish_test
