# validate.tcl - Copyright (C) 2009 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Basic build validation for TclKit.
#
# Check that the static packages are all properly loaded into the main, child
# and any thread interpreters.
#
puts "version : Tcl [info patchlevel] $tcl_platform(osVersion) $tcl_platform(machine)"

# check packages
set ext {rechan starkit}
if {$tcl_platform(platform) eq "windows"} { lappend ext registry dde }
foreach lib [info loaded] {
    if {[lindex $lib 1] eq "zlib"} {lappend ext zlib}
    if {[lindex $lib 1] eq "Mk4tcl"} {lappend ext Mk4tcl}
    if {[lindex $lib 1] eq "Itcl"} {lappend ext Itcl}
}
set r {}
foreach pkg $ext {lappend r $pkg [package require $pkg]}
puts "main  : $r"

# check seeking on vfs file
if {![info exists ::tcl::kitpath]} {set ::tcl::kitpath [info nameofexecutable]}
set f [open [file join $::tcl::kitpath  boot.tcl] r]
list [seek $f 0 end] [tell $f] [close $f]

# check child interps
interp create slave
set r {}
foreach pkg $ext {
    lappend r $pkg [slave eval [list package require $pkg]]
}
puts "child : $r"

# check thread
set r {}
if {![catch {package require Thread}]} {
    set tid [thread::create]
    foreach pkg $ext {
        lappend r $pkg \
            [thread::send $tid [list package require $pkg]]
    }
    puts "thread: $r"
}

# check pkgconfig in 8.5+
if {[package vsatisfies [package provide Tcl] 8.5]} {
    tcl::pkgconfig list
    set wd [expr {[tcl::pkgconfig get 64bit] ? "64bit" : "32bit"}]
    puts "pkgconfig: $wd [tcl::pkgconfig get {bindir,runtime}]"
}
