source $::suite_root/glue/tester.tcl
do_execsql_test func9-300 {
  SELECT hex( unistr('\UFFFFFFFF') )
} {F7BFBFBF}
finish_test
