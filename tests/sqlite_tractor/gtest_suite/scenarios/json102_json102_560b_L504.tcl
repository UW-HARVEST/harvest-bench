source $::suite_root/glue/tester.tcl
do_execsql_test json102-560b {
  SELECT json_type(x'cc0f1761cb0b133235332e350102001778','$.a[2]');
} {{true}}
finish_test
