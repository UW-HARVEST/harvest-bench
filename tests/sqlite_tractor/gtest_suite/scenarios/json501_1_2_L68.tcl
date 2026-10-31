source $::suite_root/glue/tester.tcl
do_execsql_test 1.2 {
  SELECT '[7,null,{a:5,b:6},[8,9]]'->>'$[2].b';
} {6}
finish_test
