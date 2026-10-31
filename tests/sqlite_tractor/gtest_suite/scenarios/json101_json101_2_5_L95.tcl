source $::suite_root/glue/tester.tcl
do_execsql_test json101-2.5 {
  SELECT json_object('a',printf('%.10c','x'),'b',jsonb_array(1,2,3));
} {{{"a":"xxxxxxxxxx","b":[1,2,3]}}}
finish_test
