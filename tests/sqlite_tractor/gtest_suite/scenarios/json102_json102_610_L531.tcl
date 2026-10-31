source $::suite_root/glue/tester.tcl
do_execsql_test json102-610 {
  SELECT json_valid(char(123)||'"x":35'||char(125));
} {{1}}
finish_test
