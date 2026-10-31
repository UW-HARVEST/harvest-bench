source $::suite_root/glue/tester.tcl
do_execsql_test 13.1 {
  SELECT json('{x:''a "b" c''}');
} {{{"x":"a \"b\" c"}}}
finish_test
