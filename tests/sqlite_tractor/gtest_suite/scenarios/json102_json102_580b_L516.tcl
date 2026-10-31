source $::suite_root/glue/tester.tcl
do_execsql_test json102-580b {
  SELECT json_type(x'cc0f1761cb0b133235332e350102001778','$.a[4]');
} {{null}}
finish_test
