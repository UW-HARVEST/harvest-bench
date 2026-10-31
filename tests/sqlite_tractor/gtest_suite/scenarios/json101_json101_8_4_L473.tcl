source $::suite_root/glue/tester.tcl
do_execsql_test json101-8.4 {
  SELECT unicode(json_extract(char(0x22,228,0x22),'$'));
} {228}
finish_test
