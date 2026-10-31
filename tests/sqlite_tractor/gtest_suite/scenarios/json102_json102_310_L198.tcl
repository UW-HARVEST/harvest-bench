source $::suite_root/glue/tester.tcl
do_execsql_test json102-310 {
  SELECT json_extract('{"a":2,"c":[4,5,{"f":7}]}', '$.x', '$.a');
} {{[null,2]}}
finish_test
