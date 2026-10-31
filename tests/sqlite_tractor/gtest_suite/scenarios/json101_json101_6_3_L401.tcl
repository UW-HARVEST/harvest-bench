source $::suite_root/glue/tester.tcl
do_execsql_test json101-6.3 {
  SELECT json_valid(json('{"a":55,"b":72,}'));
} {1}
finish_test
