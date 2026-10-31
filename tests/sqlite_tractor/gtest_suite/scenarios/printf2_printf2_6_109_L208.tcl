source $::suite_root/glue/tester.tcl
do_execsql_test printf2-6.109 {
  SELECT printf('(%10.6Q)','הנה מה־טוב ומה־נעים שבת אחים גם־יחד');
} {{(  'הנה')}}
finish_test
