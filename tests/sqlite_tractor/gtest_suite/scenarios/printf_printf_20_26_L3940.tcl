source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.26 {
  SELECT format('<%.1J>','abcdef');
} {{<"a">}}
finish_test
