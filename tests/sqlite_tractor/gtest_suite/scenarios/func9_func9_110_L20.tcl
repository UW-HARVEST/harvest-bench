source $::suite_root/glue/tester.tcl
do_execsql_test func9-110 {
  SELECT typeof(concat(null));
} {text}
finish_test
