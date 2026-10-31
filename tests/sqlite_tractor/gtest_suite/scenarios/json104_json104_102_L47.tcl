source $::suite_root/glue/tester.tcl
do_execsql_test json104-102 {
  SELECT json_patch('{
       a: "b",
       c: {
         d: "e",
         f: "g"
       }
     }','{
       "a":"z",
       "c": {
         "f": null
       }
     }');
} {{{"a":"z","c":{"d":"e"}}}}
finish_test
