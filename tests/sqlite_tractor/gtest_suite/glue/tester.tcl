if {[info exists ::tester_tcl_has_run]} return
set ::tester_tcl_has_run 1
source $::suite_root/glue/tester-procs.tcl
source $::suite_root/glue/capabilities.tcl
array set ::cmdlinearg {verbose 0 maxerror 1 binarylog 0 malloctrace 0 soft-heap-limit 0 hard-heap-limit 0 backtrace 0 output {} testdir {} start {} stop {}}
array set ::TC {errors 0 count 0 fail_list {} omit_list {} warn_list {}}
proc set_test_counter {counter args} {
 if {[llength $args]} {set ::TC($counter) [lindex $args 0]}
 return $::TC($counter)
}
set ::tcl_precision 15
set ::AUTOVACUUM 0
set ::SQLITE_MAX_LENGTH 1000000000
set ::sqlite_fts3_enable_parentheses 0
set ::sqlite_pending_byte 1073741824
# Diagnostic label, never used to decide a verdict.
proc sqlite3_memdebug_settitle {name} {}
namespace eval ::harvest_inputs {}
proc ::harvest_inputs::random {} {
 variable state
 set state [expr {($state ^ ($state << 13)) & 0xffffffffffffffff}]
 set state [expr {$state ^ ($state >> 7)}]
 set state [expr {($state ^ ($state << 17)) & 0xffffffffffffffff}]
 expr {$state >= 0x8000000000000000 ? $state - 0x10000000000000000 : $state}
}
proc reset_db {} {
 foreach cmd {db db2 db3} {if {[info commands $cmd] ne ""} {catch {$cmd close}}}
 foreach f [glob -nocomplain test.db*] {file delete -force $f}
 sqlite3 db ./test.db
 db cache size 3
 if {[info exists ::harvest_inputs::enabled]} {
  db function random -argcount 0 -returntype integer ::harvest_inputs::random
 }
 set ::DB [sqlite3_connection_pointer db]
 if {[info exists ::SETUP_SQL]} {db eval $::SETUP_SQL}
}
proc finish_test {} {set ::finished 1; catch {db close}}
proc finalize_testing {} {error "upstream failure limit reached"}
rename do_test upstream_do_test
proc do_test {name cmd expected} {
 lappend ::assertion_names $name
 upstream_do_test $name $cmd $expected
 if {$::TC(errors)} {error "upstream assertion failed: $name"}
}
# No runtime feature detection controls the gtest denominator. The exact
# assertion-name trace is also checked against the committed reference trace.
reset_db

# fuzz_common.tcl writes generated SQL only for diagnosis; discard that log.
# All 5000 iterations, generated SQL inputs and comparisons remain upstream.
rename open native_open
proc open {path args} {
 if {$path eq "fuzzy.log"} {set path /dev/null}
 uplevel 1 [list native_open $path {*}$args]
}

# A caught missing test command must not masquerade as an expected SQL error.
set ::missing_commands {}
rename unknown native_unknown
proc unknown {args} {
 set code [catch {uplevel 1 [list native_unknown {*}$args]} value opts]
 if {$code && [string match {invalid command name *} $value] && ![regexp {^(db[0-9]*|DB[0-9]*)$} [lindex $args 0]]} {
  lappend ::missing_commands [lindex $args 0]
 }
 return -options $opts $value
}
