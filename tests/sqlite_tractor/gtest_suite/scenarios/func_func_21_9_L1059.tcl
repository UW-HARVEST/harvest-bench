source $::suite_root/glue/tester.tcl
do_execsql_test func-21.9 {
  SELECT typeof(replace(1,'',0));
} {text}
finish_test
