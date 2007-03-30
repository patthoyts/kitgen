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
    puts {Kitgen Build System [$Id:$]
    
  kbs ?help?            this text
  kbs list              list all the extensions which can be built
}
}

proc kbs::list {} {
    puts [glob -directory ../../8.x -tails *.kbs]
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
