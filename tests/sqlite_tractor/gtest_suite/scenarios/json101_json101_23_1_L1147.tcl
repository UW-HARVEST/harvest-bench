source $::suite_root/glue/tester.tcl
do_execsql_test json101-23.1 {
  SELECT j, j->>0, j->>1
    FROM (SELECT json_set(json_set('[]','$[#]',0), '$[#]',1) AS j);
} {{[0,1]} 0 1}
finish_test
