set f [open [lindex $argv 0]]
set chunk ""
set line 0
set start 1
while {[gets $f text]>=0} {
 incr line
 append chunk $text \n
 if {![info complete $chunk]} continue
 set code [string trim $chunk]
 regsub -all -line {^\s*#[^\n]*\n?} $code {} code
 if {[regexp {^\s*do_execsql_test\s} $code] && ![catch {llength $code} n] && $n==4} {
  set name [lindex $code 1]
  if {![regexp {[$\[\]]} $name] && [regexp {^\s*do_execsql_test\s+\S+\s+\{} $code] && [regexp {\}\s+\{[^\n]*} $code]} {
   puts [join [list $start $line [binary encode base64 -maxlen 0 [encoding convertto utf-8 $name]] [binary encode base64 -maxlen 0 [encoding convertto utf-8 $code]]] \t]
  }
 }
 set chunk ""
 set start [expr {$line+1}]
}
