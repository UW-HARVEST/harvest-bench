source $::suite_root/glue/tester.tcl
do_execsql_test printf2-5.101 {
  SELECT printf('(%-8c)',char(11106));
} {{(⭢       )}}
finish_test
