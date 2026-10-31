source $::suite_root/glue/tester.tcl
do_execsql_test json102-570 {
  SELECT json_type('{"a":[2,3.5,true,false,null,"x"]}','$.a[3]');
} {{false}}
finish_test
