source $::suite_root/glue/tester.tcl
do_execsql_test json102-130b {
  SELECT json(jsonb(' { "this" : "is", "a": [ "test" ] } '));
} {{{"this":"is","a":["test"]}}}
finish_test
