source $::suite_root/glue/tester.tcl
do_execsql_test json101-4.9 {
  SELECT json_insert('{"a":1}','$.b',CAST(x'0000' AS text));
} {{{"a":1,"b":"\u0000\u0000"}}}
finish_test
