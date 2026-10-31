source $::suite_root/glue/tester.tcl
do_execsql_test json101-4.4 {
  SELECT json_valid(''), json_valid(char(0x20,0x09,0x0a,0x0d));
} {0 0}
finish_test
