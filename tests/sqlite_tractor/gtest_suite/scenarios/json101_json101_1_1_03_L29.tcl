source $::suite_root/glue/tester.tcl
do_execsql_test json101-1.1.03 {
  SELECT json_array(1,json_object('abc',2.5,'def',null,'ghi','hello'),99);
  -- the second term goes in as JSON
} {[1,{"abc":2.5,"def":null,"ghi":"hello"},99]}
finish_test
