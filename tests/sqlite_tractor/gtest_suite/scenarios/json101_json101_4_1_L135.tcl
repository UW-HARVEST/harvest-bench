source $::suite_root/glue/tester.tcl
do_execsql_test json101-4.1 {
  CREATE TABLE j1(x);
  INSERT INTO j1(x)
   VALUES('true'),('false'),('null'),('123'),('-234'),('34.5e+6'),
         ('""'),('"\""'),('"\\"'),('"abcdefghijlmnopqrstuvwxyz"'),
         ('[]'),('{}'),('[true,false,null,123,-234,34.5e+6,{},[]]'),
         ('{"a":true,"b":{"c":false}}');
  SELECT * FROM j1 WHERE NOT json_valid(x);
} {}
finish_test
