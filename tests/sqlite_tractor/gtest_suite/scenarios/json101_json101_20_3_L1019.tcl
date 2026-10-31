source $::suite_root/glue/tester.tcl
do_execsql_test json101-20.3 {
  SELECT json_object('a',2e370,'b',-3e380)->>'b';
} {-Inf}
finish_test
