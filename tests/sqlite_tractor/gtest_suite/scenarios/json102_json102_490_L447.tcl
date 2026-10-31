source $::suite_root/glue/tester.tcl
do_execsql_test json102-490 {
  SELECT json_remove('{"x":25,"y":42}','$.y');
} {{{"x":25}}}
finish_test
