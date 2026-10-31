source $::suite_root/glue/tester.tcl
do_execsql_test json101-5.11 {
  SELECT json_insert('{}','$.a',value) FROM json_tree('"[1,2,3]"');
} {{{"a":"[1,2,3]"}}}
finish_test
