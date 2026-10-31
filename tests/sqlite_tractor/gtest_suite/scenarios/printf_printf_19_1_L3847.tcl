source $::suite_root/glue/tester.tcl
do_execsql_test printf-19.1 {
  SELECT format('%0.0f %0.0g %0.0g', 0.9, 0.09, 1.9);
} {{1 0.09 2}}
finish_test
