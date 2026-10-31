source $::suite_root/glue/tester.tcl
do_execsql_test json101-2.2.2b {
  SELECT json(jsonb_object('a',json_array('xyx',77,4.5),'x',2.5));
} {{{"a":["xyx",77,4.5],"x":2.5}}}
finish_test
