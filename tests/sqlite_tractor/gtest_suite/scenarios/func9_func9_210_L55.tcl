source $::suite_root/glue/tester.tcl
do_execsql_test func9-210 {
  SELECT unistr_quote(unistr('G\u00e4ste'));
} {'Gäste'}
finish_test
