source $::suite_root/glue/tester.tcl
do_execsql_test json101-22.1 {
  SELECT json_set(
    '{}',
    '$.a', json('1'),
    '$.a', json('2'),
    '$.b', json('3'),
    '$.b', json('4'),
    '$.c', json('5'),
    '$.c', json('6')
  );
} {{{"a":2,"b":4,"c":6}}}
finish_test
