source $::suite_root/glue/tester.tcl
do_execsql_test json102-260-3 {
  SELECT json(jsonb_extract('{"a":2,"c":[4,5,{"f":7}]}', '$.c'));
} {{[4,5,{"f":7}]}}
finish_test
