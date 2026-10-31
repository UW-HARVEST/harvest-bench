source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.24 {
  SELECT format('<%j>','abc'||char(0x1f)||'def');
} {{<abc\u001fdef>}}
finish_test
