source $::suite_root/glue/tester.tcl
do_execsql_test json101-18.3 {
  SELECT json_extract('[3,{"a":4,"":[5,{"hi":6},7]},8]', '$[1].""[1].hi');
} {6}
finish_test
