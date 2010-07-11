#!/usr/bin/env tclkit

source [file join [file dir [info script]] initests.tcl]

runAllTests

eval unset [info vars ?]
eval unset [info vars ??]
#puts [info vars *]