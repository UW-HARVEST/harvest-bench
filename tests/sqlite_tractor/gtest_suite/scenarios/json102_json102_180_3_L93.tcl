source $::suite_root/glue/tester.tcl
do_execsql_test json102-180-3 {
  SELECT json(jsonb_array(1,null,'3',json('[4,5]'),json('{"six":7.7}')));
} {{[1,null,"3",[4,5],{"six":7.7}]}}
finish_test
