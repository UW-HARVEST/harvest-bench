source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.6 {
  SELECT quote(likelihood(x'010203000405', 0.5));
} {X'010203000405'}
finish_test
