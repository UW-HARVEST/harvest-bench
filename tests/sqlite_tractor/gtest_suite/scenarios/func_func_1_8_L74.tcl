source $::suite_root/glue/tester.tcl
do_execsql_test func-1.8 {
  SELECT octet_length(x'30313233');
} {4}
finish_test
