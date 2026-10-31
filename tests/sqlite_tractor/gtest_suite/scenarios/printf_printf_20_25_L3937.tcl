source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.25 {
  SELECT format('<%.4j>','a"b"c');
} {{<a\"b\">}}
finish_test
