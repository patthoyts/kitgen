# setupvfs.tcl -- new tclkit-{cli,dyn,gui} generation bootstrap
#
# jcw, 2006-11-16

proc history {args} {} ;# since this runs so early, all debugging support helps

if {[lindex $argv 0] ne "-init-"} {
  puts stderr "setupvfs.tcl has to be run by kit-cli with the '-init-' flag"
  exit 1
}

set argv [lrange $argv 2 end] ;# strip off the leading "-init- setupvfs.tcl"

set debugOpt 0
set encOpt 0 
set msgsOpt 0 
set threadOpt 0
set tzOpt 0 

while {1} {
  switch -- [lindex $argv 0] {
    -d { incr debugOpt }
    -e { incr encOpt }
    -m { incr msgsOpt }
    -t { incr threadOpt }
    -z { incr tzOpt }
    default { break }
  }
  set argv [lrange $argv 1 end]
}

if {[llength $argv] != 2} {
  puts stderr "Usage: [file tail [info nameofexe]] -init- [info script]\
    ?-d? ?-e? ?-m? ?-t? ?-z? destfile (cli|dyn|gui)
    -d    output some debugging info from this setup script
    -e    include all encodings i.s.o. 7 basic ones (encodings/)
    -m    include all localized message files (tcl 8.5, msgs/)
    -t    include the thread extension as shared lib in vfs
    -z    include timezone data files (tcl 8.5, tzdata/)"
  exit 1
}

load {} vfs ;# vlerq is already loaded by now

# map of proper version numbers to replace @ markers in paths given to vfscopy
# this relies on having all necessary extensions already loaded at this point
set versmap [list tcl8@ tcl$tcl_version tk8@ tk$tcl_version \
                  vfs1@ vfs[package require vfs] \
                  vqtcl4@ vqtcl[package require vlerq]]
               
if {$debugOpt} {
  puts "Starting [info script]"
  puts "     exe: [info nameofexe]"
  puts "    argv: $argv"
  puts "   tcltk: $tcl_version"
  puts "  loaded: [info loaded]"
  puts " versmap: $versmap"
  puts ""
}

set tcl_library ../tcl/library
source ../tcl/library/init.tcl ;# for tcl::CopyDirectory
source ../../8.x/tclvfs/library/vfsUtils.tcl
source ../../8.x/tclvfs/library/vfslib.tcl ;# override vfs::memchan/vfsUtils.tcl
source ../../8.x/vqtcl/library/m2mvfs.tcl

set clifiles {
  boot.tcl
  config.tcl
  lib/tcl8@/auto.tcl
  lib/tcl8@/history.tcl
  lib/tcl8@/init.tcl
  lib/tcl8@/opt0.4
  lib/tcl8@/package.tcl
  lib/tcl8@/parray.tcl
  lib/tcl8@/safe.tcl
  lib/tcl8@/tclIndex
  lib/tcl8@/word.tcl
  lib/vfs1@/mk4vfs.tcl
  lib/vfs1@/pkgIndex.tcl
  lib/vfs1@/starkit.tcl
  lib/vfs1@/vfslib.tcl
  lib/vfs1@/vfsUtils.tcl
  lib/vfs1@/zipvfs.tcl
  lib/vqtcl4@/m2mvfs.tcl
  lib/vqtcl4@/mkclvfs.tcl
  lib/vqtcl4@/mklite.tcl
  lib/vqtcl4@/pkgIndex.tcl
  lib/vqtcl4@/ratcl.tcl
}

set guifiles {
  tclkit.ico
  lib/tk8@/bgerror.tcl
  lib/tk8@/button.tcl
  lib/tk8@/choosedir.tcl
  lib/tk8@/clrpick.tcl
  lib/tk8@/comdlg.tcl
  lib/tk8@/console.tcl
  lib/tk8@/dialog.tcl
  lib/tk8@/entry.tcl
  lib/tk8@/focus.tcl
  lib/tk8@/listbox.tcl
  lib/tk8@/menu.tcl
  lib/tk8@/mkpsenc.tcl
  lib/tk8@/msgbox.tcl
  lib/tk8@/msgs
  lib/tk8@/obsolete.tcl
  lib/tk8@/optMenu.tcl
  lib/tk8@/palette.tcl
  lib/tk8@/panedwindow.tcl
  lib/tk8@/pkgIndex.tcl
  lib/tk8@/prolog.ps
  lib/tk8@/safetk.tcl
  lib/tk8@/scale.tcl
  lib/tk8@/scrlbar.tcl
  lib/tk8@/spinbox.tcl
  lib/tk8@/tclIndex
  lib/tk8@/tearoff.tcl
  lib/tk8@/text.tcl
  lib/tk8@/tk.tcl
  lib/tk8@/tkfbox.tcl
  lib/tk8@/unsupported.tcl
  lib/tk8@/xmfbox.tcl
}

if {$encOpt} {
  lappend clifiles lib/tcl8@/encoding
} else {
  lappend clifiles lib/tcl8@/encoding/ascii.enc \
                   lib/tcl8@/encoding/cp1252.enc \
                   lib/tcl8@/encoding/iso8859-1.enc \
                   lib/tcl8@/encoding/iso8859-15.enc \
                   lib/tcl8@/encoding/iso8859-2.enc \
                   lib/tcl8@/encoding/koi8-r.enc \
                   lib/tcl8@/encoding/macRoman.enc
}

if {$threadOpt} {
  lappend clifiles lib/[glob -tails -dir build/lib thread2*]
}

if {$tcl_version eq "8.4"} {
  lappend clifiles lib/tcl8@/http2.5 \
            		   lib/tcl8@/ldAout.tcl \
            		   lib/tcl8@/msgcat1.3 \
            		   lib/tcl8@/tcltest2.2
} else {
  lappend clifiles lib/tcl8 \
                   lib/tcl8@/clock.tcl \
                   lib/tcl8@/tm.tcl

  lappend guifiles lib/tk8@/ttk

  if {$msgsOpt} {
    lappend clifiles lib/tcl8@/msgs
  }
  if {$tzOpt} {
    lappend clifiles lib/tcl8@/tzdata
  }
}

# look for a/b/c in three places:
#   1) build/files/b-c
#   2) build/files/a/b/c
#   3) build/a/b/c

proc locatefile {f} {
  set a [file split $f]
  set n "build/files/[lindex $a end-1]-[lindex $a end]"
  if {[file exists $n]} {
    if {$::debugOpt} {
      puts "  $n  ==>  \$vfs/$f"
    }
  } else {
    set n build/files/$f
    if {[file exists $n]} {
      if {$::debugOpt} {
        puts "  $n  ==>  \$vfs/$f"
      }
    } else {
      set n build/$f
    }
  }
  return $n
}

# copy file to m2m-mounted vfs
proc vfscopy {argv} {
  global vfs versmap
  
  foreach f $argv {
    set f [string map $versmap $f]
    
    set d $vfs/[file dirname $f]
    if {![file isdir $d]} {
      file mkdir $d
    }

    set src [locatefile $f]
    set dest $vfs/$f
    
    switch -- [file extension $src] {
      .tcl - .txt - .msg - .test {
        # get line-endings right for text files - this is crucial for boot.tcl
        # and several scripts in lib/vlerq4/ which are loaded before vfs works
        set fin [open $src r]
        set fout [open $dest w]
        fcopy $fin $fout
        close $fin
        close $fout
      }
      default {
        file copy $src $dest
      }
    }
    
    file mtime $dest [file mtime $src]
  }
}

set vfs [lindex $argv 0]
vfs::m2m::Mount $vfs $vfs

switch [info sharedlibext] {
  .dll {
    catch {
      # avoid hard-wiring a Thread extension version number in here
      set dll [glob build/bin/thread2*.dll]
      load $dll
      set vsn [package require Thread]
      file copy -force $dll build/lib/libthread$vsn.dll
      unset dll vsn
    }
    # create dde and registry pkgIndex files with the right version
    foreach ext {dde registry} {
      if {[catch {
          load {} $ext
          set extdir [file join $vfs lib $ext]
          file mkdir $extdir
          set f [open $extdir/pkgIndex.tcl w]
          puts $f "package ifneeded $ext [package provide $ext] {load {} $ext}"
          close $f
      } err]} { puts "ERROR: $err"}
    }
    catch {
      file delete [glob build/lib/libtk8?.a] ;# so only libtk8?s.a will be found
    }
    catch {
      file copy -force [glob build/bin/tk8*.dll] build/lib/libtk$tcl_version.dll
    }
  }
  .so {
    catch {
      # for some *BSD's, lib names have no dot and/or end with a version number
      file rename [glob build/lib/libtk8*.so*] build/lib/libtk$tcl_version.so
    }
  }
}

switch [lindex $argv 1] {
  cli {
    vfscopy $clifiles
  }
  gui {
    vfscopy $clifiles
    vfscopy $guifiles
  }
  dyn {
    vfscopy $clifiles
    vfscopy $guifiles
    vfscopy lib/libtk$tcl_version[info sharedlibext]
  }
  default {
    puts stderr "Unknown type, must be one of: cli, dyn, gui"
    exit 1
  }
}

vfs::unmount $vfs

if {$debugOpt} {
  puts "\nDone with [info script]"
}
