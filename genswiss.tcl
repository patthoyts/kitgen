#!/usr/bin/env tclkit

# Generate swisskit from 8.5/kit-large and 8.5/kit-x11, created via:
#
#   sh config.sh 8.5/kit-large aqua univ thread allenc allmsgs tzdata
#   sh config.sh 8.5/kit-x11 univ thread dyn
#
# The result is an 8.5-based threaded universal binary for Mac OS X with all
# available encodings, message catalogs, and timezone datafiles included.
#
# This binary will launch Tk under X11 if $env(DISPLAY) is set, else Tk Aqua.
#
# jcw, 2006-11-19

package require vfs::mk4

file copy -force 8.5/kit-large/tclkit-dyn swisskit

vfs::mk4::Mount 8.5/kit-x11/tclkit-dyn x11 -readonly
vfs::mk4::Mount swisskit swisskit

file copy x11/lib/libtk8.5.dylib swisskit/lib/libtk8.5-x11.dylib

set fd [open swisskit/lib/tk8.5/pkgIndex.tcl w]

puts $fd {
package ifneeded Tk 8.5a6 \
  [string map [list @A [file join $dir .. libtk8.5[info sharedlibext]] \
		    @X [file join $dir .. libtk8.5-x11[info sharedlibext]]] {
    if {[lsearch -exact [info loaded] {{} Tk}] >= 0} {
      load "" Tk
    } elseif {[info exists ::env(DISPLAY)]} {
      load @X Tk
    } else {
      load @A Tk
    }
  }]
}

close $fd

vfs::unmount swisskit
vfs::unmount x11
