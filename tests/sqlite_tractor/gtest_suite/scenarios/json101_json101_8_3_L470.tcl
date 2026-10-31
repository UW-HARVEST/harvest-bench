source $::suite_root/glue/tester.tcl
do_execsql_test json101-8.3 {
  SELECT json_valid(char(0x22,0xe4,0x22));
} {1}
finish_test
