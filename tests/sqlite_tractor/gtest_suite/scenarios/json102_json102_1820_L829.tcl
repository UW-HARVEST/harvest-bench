source $::suite_root/glue/tester.tcl
do_execsql_test json102-1820 {
  SELECT '{"1":"one","2":"two","3":"three"}'->'2';
} {{"two"}}
finish_test
