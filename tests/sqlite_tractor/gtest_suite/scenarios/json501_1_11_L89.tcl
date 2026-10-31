source $::suite_root/glue/tester.tcl
do_execsql_test 1.11 {
  SELECT '{ MNO_123æxyz : 789 }'->>'MNO_123æxyz';
} {789}
finish_test
