source $::suite_root/glue/tester.tcl
do_execsql_test func9-220 {
  SELECT format('%#Q',unistr('G\u00e4ste'));
} {'Gäste'}
finish_test
