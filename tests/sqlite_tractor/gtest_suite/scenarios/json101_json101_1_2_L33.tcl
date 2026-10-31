source $::suite_root/glue/tester.tcl
do_execsql_test json101-1.2 {
  SELECT hex(json_array('String "\ Test'));
} {5B22537472696E67205C225C5C2054657374225D}
finish_test
