source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-110 {
   SELECT acos(1.0001);
} {{}}
finish_test
