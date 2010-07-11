# common test setup script

#puts "vars: [info vars ?] [info vars ??]"

if {[info exists testsInited]} return

package require Tcl 8.4

if {[lsearch [namespace children] ::tcltest] == -1} {
  package require tcltest 2.2
  namespace import tcltest::*
}

singleProcess true ;# run without forking

testsDirectory [file dirname [info script]]

# if run from the tests directory, move one level up
if {[pwd] eq [testsDirectory]} {
  cd ..
}

temporaryDirectory [pwd]
workingDirectory [file dirname [testsDirectory]]

# TextMate support on Mac OS X: run make before running any test from editor
if {[info exists env(TM_FILENAME)]} {
  if {[catch { exec make } msg]} {
    puts stderr $msg
    exit 1
  }
}

proc readfile {filename} {                                                      
  set fd [open $filename]                                                       
  set data [read $fd]                                                           
  close $fd                                                                     
  return $data                                                                  
}

# extract version number from configure.in
regexp {AC_INIT\(\[vqtcl\],\s*\[([.\d]+)\]\)} [readfile configure.in] - version
unset -

# make sure the pkgIndex.tcl is found
if {[lsearch $auto_path [workingDirectory]] < 0} {
  lappend auto_path [workingDirectory]
}

testConstraint 64bit            [expr {$tcl_platform(wordSize) == 8}]
testConstraint bigendian        [expr {$tcl_platform(byteOrder) eq "bigEndian"}]
testConstraint kitten           [file exists [temporaryDirectory]/kitten.kit]
testConstraint metakit          [expr {![catch { package require Mk4tcl }]}]
testConstraint tcl$tcl_version  1
testConstraint vfs              [expr {![catch { package require vfs }]}]

set testsInited 1
