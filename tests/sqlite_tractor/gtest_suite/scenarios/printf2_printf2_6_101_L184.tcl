source $::suite_root/glue/tester.tcl
do_execsql_test printf2-6.101 {
  SELECT printf('(%.6s)','הנה מה־טוב ומה־נעים שבת אחים גם־יחד');
} {(הנה)}
finish_test
