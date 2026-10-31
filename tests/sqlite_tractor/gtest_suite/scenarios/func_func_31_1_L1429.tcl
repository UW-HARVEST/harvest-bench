source $::suite_root/glue/tester.tcl
do_execsql_test func-31.1 { 
  SELECT char(), length(char()), typeof(char()) 
} {{} 0 text}
finish_test
