source $::suite_root/glue/tester.tcl
do_execsql_test json102-620 {
  SELECT json_valid(char(123)||'"x":35');
} {{0}}
finish_test
