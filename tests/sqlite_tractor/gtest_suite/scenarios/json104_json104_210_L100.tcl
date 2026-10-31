source $::suite_root/glue/tester.tcl
do_execsql_test json104-210 {
  SELECT json_patch('[1,2,3]','{"x":null,"y":1,"z":null}');
} {{{"y":1}}}
finish_test
