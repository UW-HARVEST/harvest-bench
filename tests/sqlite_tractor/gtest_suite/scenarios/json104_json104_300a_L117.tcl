source $::suite_root/glue/tester.tcl
do_execsql_test json104-300a {
  SELECT coalesce(json_patch(null,'{"a":"c"}'), 'real-null');
} {{real-null}}
finish_test
