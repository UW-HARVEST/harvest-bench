source $::suite_root/glue/tester.tcl
do_execsql_test json102-300b {
  SELECT jsonb_extract('{"a":2,"c":[4,5,{"f":7}]}', '$.x');
} {{}}
finish_test
