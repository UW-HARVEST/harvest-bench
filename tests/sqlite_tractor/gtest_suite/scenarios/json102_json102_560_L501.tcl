source $::suite_root/glue/tester.tcl
do_execsql_test json102-560 {
  SELECT json_type('{"a":[2,3.5,true,false,null,"x"]}','$.a[2]');
} {{true}}
finish_test
