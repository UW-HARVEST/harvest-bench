source $::suite_root/glue/tester.tcl
do_execsql_test json104-312 {
  SELECT json_patch('{"e":null}','{"a":1}');
} {{{"e":null,"a":1}}}
finish_test
