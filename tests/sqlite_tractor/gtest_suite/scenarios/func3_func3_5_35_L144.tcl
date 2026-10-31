source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.35 {
  SELECT quote(unlikely(x'010203000405'));
} {X'010203000405'}
finish_test
