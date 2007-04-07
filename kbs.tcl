# kbs.tcl -- Kitgen Build System
#
#       Launch as "8.4/base-std/tclkit-cli kbs.tcl" to get a brief help text
#
# jcw, 2007-03-30

cd [file dirname [info nameofexe]]

proc kbs {command args} {
    uplevel 1 ::kbs::$command $args
}

namespace eval kbs {
    variable seq 0
}
    
proc kbs::help {} {
    puts "Kitgen Build System

  kbs build target      build the specified extension
  kbs ?help?            this text
  kbs list              list all the extensions which can be built
  kbs make              build all extensions which haven't been built before
"
}

proc kbs::list {} {
    puts [lsort -dict [glob -directory ../../extdefs -tails *.kbs]]
}

proc kbs::make {} {
    foreach f [lsort -dict [glob -directory ../../extdefs -tails *.kbs]] {
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
        namespace eval v {}
        set v::package $name
        set v::maindir [pwd]
        file mkdir build/$name
        cd build/$name
        source [Topdir]/extdefs/$name.kbs
    }
    
    proc Version {{ver ""}} {
        if {$ver ne ""} { set v::version $ver }
        return $v::version
    }
    
    proc Requires {args} {
        # build all the other required extensions first, then resume this one
        # this recurses into kbs::build, so we need to save/restore all state
        foreach target $args {
            if {![file exists $v::maindir/build/$target]} {
                puts ">>> $target (required by $v::package)"
                set pwd [pwd]
                cd $v::maindir
                
                set keep {}
                foreach x [info vars v::*] { lappend keep $x [set $x] }
                namespace delete ::config::v

                set r [catch { kbs build $target } err]
                
                namespace delete ::config::v
                namespace eval v {}
                foreach {x y} $keep { set $x $y }
                
                cd $pwd
                if {$r} { return -code error $err}
                puts "<<< $target (resuming build of $v::package)"
            }
        }
    }
    
    proc Sources {type args} {
        if {![file exists [Srcdir]]} {
            eval [linsert $args 0 src-$type]
        }
    }
    
    proc src-cvs {path {module ""}} {
        if {$module eq ""} { set module [file tail $path]}
        if {[string first @ $path] < 0} { set path :pserver:anonymous@$path }
        Run cvs -d $path -z3 co -P -d tmp $module
        file rename tmp [Srcdir]
    }
    
    proc src-svn {path} {
        Run svn co $path [Srcdir]
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
        Untar $file [Srcdir]
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
        file mkdir [Topdir]/sources
        return [Topdir]/sources/$v::package-$v::version
    }
    
    proc Destdir {} {
        return $v::maindir/build
    }
    
    proc Incdir {} {
        return $v::maindir/build/include
    }
    
    proc Libdir {} {
        return $v::maindir/build/lib
    }
    
    proc Unglob {match} {
        set paths [glob $match]
        if {[llength $paths] != 1} { error "not unique: $match" }
        lindex $paths 0
    }

    proc Untar {file dest} {
        set path [file normalize $file]
        file mkdir tmp
        cd tmp
        # use explicit gzip in case tar command doesn't understand the z flag
        set r [catch {exec gzip -dc $path | tar xf -} err]
        cd ..
        if {$r} { 
            file delete -force tmp
            return -code error $err
        }
        # cover both cases: untar to single dir and untar all into current dir
        set untarred [glob tmp/*]
        if {[llength $untarred] == 1 && [file isdir [lindex $untarred 0]]} {
            file rename [lindex $untarred 0] $dest
            file delete tmp
        } else {
            file rename tmp $dest
        }
    }
    
    proc Run {args} {
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
