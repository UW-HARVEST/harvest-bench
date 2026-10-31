source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.12 {
  SELECT printf('%n',0);
} {{}}
finish_test
