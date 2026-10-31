source $::suite_root/glue/tester.tcl
do_execsql_test printf2-6.100 {
  SELECT printf('(%!.3s)','הנה מה־טוב ומה־נעים שבת אחים גם־יחד');
} {(הנה)}
finish_test
