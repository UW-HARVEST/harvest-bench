source $::suite_root/glue/tester.tcl
do_execsql_test json101-20.1 {
  SELECT json_object('a',2e370,'b',-3e380);
} {{{"a":9.0e+999,"b":-9.0e+999}}}
finish_test
