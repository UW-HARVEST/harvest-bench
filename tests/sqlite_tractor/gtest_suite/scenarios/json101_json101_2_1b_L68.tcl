source $::suite_root/glue/tester.tcl
do_execsql_test json101-2.1b {
  SELECT json(jsonb_object('a',1,'b',2.5,'c',null,'d','String Test'));
} {{{"a":1,"b":2.5,"c":null,"d":"String Test"}}}
finish_test
