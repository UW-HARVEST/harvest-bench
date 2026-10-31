source $::suite_root/glue/tester.tcl
do_execsql_test json102-130 {
  SELECT json(' { "this" : "is", "a": [ "test" ] } ');
} {{{"this":"is","a":["test"]}}}
finish_test
