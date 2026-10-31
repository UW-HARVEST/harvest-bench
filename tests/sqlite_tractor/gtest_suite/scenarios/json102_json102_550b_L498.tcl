source $::suite_root/glue/tester.tcl
do_execsql_test json102-550b {
  SELECT json_type(x'cc0f1761cb0b133235332e350102001778','$.a[1]');
} {{real}}
finish_test
