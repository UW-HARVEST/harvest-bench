source $::suite_root/glue/tester.tcl
do_execsql_test json104-313 {
  SELECT json_patch('[1,2]','{"a":"b","c":null}');
} {{{"a":"b"}}}
finish_test
