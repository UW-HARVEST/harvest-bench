source $::suite_root/glue/tester.tcl
do_execsql_test json101-26.2 {
  SELECT json_valid(x'CC141761133117621332176313331764133437656565',8);
} {0}
finish_test
