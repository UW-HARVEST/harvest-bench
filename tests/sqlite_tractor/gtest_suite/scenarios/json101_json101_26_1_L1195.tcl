source $::suite_root/glue/tester.tcl
do_execsql_test json101-26.1 {
  SELECT value FROM json_each(x'CC141761133117621332176313331764133437656565') WHERE key='eee';
} {eee}
finish_test
