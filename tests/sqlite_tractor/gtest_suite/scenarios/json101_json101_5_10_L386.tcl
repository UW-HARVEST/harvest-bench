source $::suite_root/glue/tester.tcl
do_execsql_test json101-5.10 {
  SELECT json_insert('{}','$.a',value) FROM json_tree('[1,2,3]') WHERE atom IS NULL;
} {{{"a":[1,2,3]}}}
finish_test
