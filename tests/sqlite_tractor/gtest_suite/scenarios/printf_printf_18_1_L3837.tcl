source $::suite_root/glue/tester.tcl
do_execsql_test printf-18.1 {
  SELECT length( format('%,.249f', -5.0e-300) );
} {252}
finish_test
