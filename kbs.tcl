# kbs.tcl -- Kitgen Build System
#
#       Launch as "8.4/base-std/tclkit-cli kbs.tcl" to get a brief help text
#
# jcw, 2007-03-30

cd [file dirname [info nameofexe]]

proc kbs {command args} {
    uplevel 1 ::kbs::$command $args
}

namespace eval kbs {}
    
proc kbs::help {} {
    puts "Kitgen Build System

  kbs build target      build the specified extension
  kbs ?help?            this text
  kbs list              list all the extensions which can be built
  kbs make              build all extensions which haven't been built before
"
}

proc kbs::list {} {
    puts [glob -directory ../../extdefs -tails *.kbs]
}

proc kbs::make {} {
    foreach f [lsort [glob -directory ../../extdefs -tails *.kbs]] {
        set target [file root $f]
        if {![file exists build/$target]} {
            puts $target:
            build $target
        }
    }
}

proc kbs::build {target} {
    set pwd [pwd]
    config::init $target
    cd $pwd
}

namespace eval config {
    proc init {name} {
        namespace eval v [list set package $name]
        namespace eval v [list set maindir [pwd]]
        file mkdir build/$name
        cd build/$name
        source [Topdir]/extdefs/$name.kbs
    }
    
    proc Version {{ver ""}} {
        if {$ver ne ""} { set v::version $ver }
        return $v::version
    }
    
    proc Sources {type args} {
        if {![file exists [Srcdir]]} {
            eval [linsert $args 0 src-$type]
        }
    }
    
    proc src-cvs {path {module ""}} {
        if {$module eq ""} { set module [file tail $path]}
        if {[string first @ $path] < 0} { set path :pserver:anonymous@$path }
        Sh cvs -d $path -z3 co -P -d tmp $module
        file rename tmp [Srcdir]
    }
    
    proc src-svn {path} {
        Sh svn co $path [Srcdir]
    }
    
    proc src-fetch {path} {
        set file [Topdir]/downloads/[file tail $path]
        if {![file exists $file]} {
            package require http
            
            file mkdir [Topdir]/downloads
            puts "  fetching $file"

            set fd [open $file w]
            set t [http::geturl $path -channel $fd]
            close $fd

            scan [http::code $t] {HTTP/%f %d} ver ncode
            #puts [http::status $t]
            http::cleanup $t

            if {$ncode != 200 || [file size $file] == 0} {
                file delete $file
                error "fetch failed"
            }
        }
        puts "  untarring $file"
        file mkdir tmp
        exec tar xfz $file -C tmp
        set untarred [glob tmp/*]
        if {[llength $untarred] == 1 && [file isdir [lindex $untarred 0]]} {
            file rename [lindex $untarred 0] [Srcdir]
            file delete tmp
        } else {
            file rename tmp [Srcdir]
        }
    }
    
    proc src-symlink {path} {
        file link -symbolic [Srcdir] $path
    }
    
    proc Build {script} {
        puts [Srcdir]
        eval $script
    }
    
    proc Topdir {} {
        file normalize $v::maindir/../..
    }
    
    proc Srcdir {} {
        return [Topdir]/8.x/$v::package-$v::version
    }
    
    proc Incdir {} {
        file normalize $v::maindir/build/include
    }
    
    proc Libdir {} {
        file normalize $v::maindir/build/lib
    }
    
    proc Unglob {match} {
        set paths [glob $match]
        if {[llength $paths] != 1} { error "not unique: $match" }
        lindex $paths 0
    }
    
    proc Sh {args} {
        lappend args >@stdout 2>@stderr
        eval [linsert $args 0 exec]
    }
    
    proc Result {path} {
        file mkdir $v::maindir/ext
        file delete -force $v::maindir/ext/$v::package-$v::version
        file copy $path $v::maindir/ext/$v::package-$v::version
    }
}

# now process the command line to call one of the kbs::* procs
namespace eval kbs {
    set cmd [lindex $argv 0]
    if {[info commands ::kbs::$cmd] ne ""} {
        eval $argv
    } elseif {$cmd eq ""} {
        help
    } else {
        set cmdlist {}
        foreach knowncmd [lsort [info commands ::kbs::*]] {
            lappend cmdlist [namespace tail $knowncmd]
        }
        puts "'$cmd' not found, should be one of: [join $cmdlist {, }]"
        exit 1
    }
}
