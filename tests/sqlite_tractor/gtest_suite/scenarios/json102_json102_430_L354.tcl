source $::suite_root/glue/tester.tcl
do_execsql_test json102-430 {
  SELECT json_object('a',2,'c',json_object('e',5));
} {{{"a":2,"c":{"e":5}}}}
finish_test
