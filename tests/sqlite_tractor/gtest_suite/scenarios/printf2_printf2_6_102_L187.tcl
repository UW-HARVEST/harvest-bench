source $::suite_root/glue/tester.tcl
do_execsql_test printf2-6.102 {
  SELECT printf('(%!5.3s)','הנה מה־טוב ומה־נעים שבת אחים גם־יחד');
} {{(  הנה)}}
finish_test
