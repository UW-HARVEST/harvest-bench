source $::suite_root/glue/tester.tcl
do_execsql_test json104-200 {
  SELECT json_patch('[1,2,3]','{"x":null}');
} {{{}}}
finish_test
