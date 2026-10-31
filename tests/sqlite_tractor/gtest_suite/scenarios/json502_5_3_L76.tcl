source $::suite_root/glue/tester.tcl
do_execsql_test 5.3 {
  SELECT JSON_SET('{}', '$."\"Key"', 1);
} {{{"\"Key":1}}}
finish_test
