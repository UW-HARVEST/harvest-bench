source $::suite_root/glue/tester.tcl
do_execsql_test json102-191 {
  SELECT json_array_length( json_remove('[1,2,3,4]','$[2]') );
} {{3}}
finish_test
