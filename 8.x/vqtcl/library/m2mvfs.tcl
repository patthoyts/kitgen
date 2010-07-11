# m2mvfs.tcl -- In-memory VFS which appends its data as a MK starkit when done.

# This VFS driver collects data in memory which then gets saved to a Metakit
# data file when unmounted, using vlerq calls.  Does not support deletions,
# renames, and such.  Can only be used with a single mountpoint at a time.
#
# 1.4 adjusted for vlerq 3
# 1.5 fixed to actually work
# 1.6 minor cleanup
# 1.7 adjusted for vlerq 4
# 1.8 allow overwriting files

package provide vfs::m2m 1.8

package require vfs
package require vlerq

namespace eval vfs::m2m {
    namespace eval v {
        variable outfn   ;# output file name
        # following lists are all indexed by the same directory ordinal number
        variable dname   ;# list of directory names
        variable prows   ;# list of parent row numbers
        variable files   ;# list of list of (name size date contents) tuples
    }

# public

    proc Mount {mkfile local args} {
        set v::outfn [file normalize $mkfile]
        set v::dname [list <root>]
        set v::prows [list -1]
        set v::files [list {}]
        ::vfs::filesystem mount $local [list ::vfs::m2m::handler -]
        ::vfs::RegisterMount $local [list ::vfs::m2m::Unmount -]
        return m2m
    }

    proc Unmount {db local} {
        ::vfs::filesystem unmount $local
        if {$v::outfn ne ""} {
            set fd [::open $v::outfn a]
            vlerq write [makeDirsView] $fd
            ::close $fd
        }
        unset v::outfn v::dname v::prows v::files
    }

# private

    proc makeDirsView {} {
        set d {}
        foreach f $v::files {
            set x [lsort -index 0 -unique $f]
            lappend d [vlerq def {name size:I date:I contents:B} \
                        [eval concat $x]]
        }
        set desc {name parent:I {files {name size:I date:I contents:B}}}
        set dirs [vlerq data [vlerq mdef $desc] $v::dname $v::prows $d]
        vlerq group $dirs {} dirs
    }

    proc handler {db cmd root path actual args} {
        #puts [list M2M $db <$cmd> r: $root p: $path a: $actual $args]
        switch $cmd {
            matchindirectory { eval [linsert $args 0 $cmd $db $path $actual] }
            fileattributes   { eval [linsert $args 0 $cmd $db $root $path] } 
            default          { eval [linsert $args 0 $cmd $db $path] }
        }
    }

    proc fail {code} {
        ::vfs::filesystem posixerror $::vfs::posix($code)
    }

    proc lookUp {db path} {
        set parent 0
        if {$path ne "."} {
            set elems [file split $path]
            set remain [llength $elems]
            foreach e $elems {
                set r ""
                foreach r [lsearch -exact -int -all $v::prows $parent] {
                    if {$e eq [lindex $v::dname $r]} {
                        set parent $r
                        incr remain -1
                        break
                    }
                }
                if {$parent != $r} {
                    if {$remain == 1} {
                        set i [lsearch -exact \
                                        [subFileNames [list ? ? $parent]] $e]
                        if {$i >= 0} {
                            # eval this 4-item result returns info about a file
                            return [list lindex $v::files $parent $i]
                        }
                    }
                    fail ENOENT
                }
            }
        }
        # evaluating this 3-item result returns the files subview
        return [list lindex $v::files $parent]
    }

    proc isDir {tag} {
        return [expr {[llength $tag] == 3}]
    }

    proc subDirNums {tag} {
        lsearch -exact -int -all $v::prows [lindex $tag 2]
    }

    proc subFileNames {tag} {
        set r {}
        foreach x [lindex $v::files [lindex $tag 2]] { lappend r [lindex $x 0] }
        return $r
    }

# methods

    proc matchindirectory {db path actual pattern type} {
        set o {}
        if {$type == 0} { set type 20 }
        set tag [lookUp $db $path]
        if {$pattern ne ""} {
            set c {}
            if {[isDir $tag]} {
                if {$type & 16} {
                    set c [subFileNames $tag]
                }
                if {$type & 4} {
                    foreach r [subDirNums $tag] {
                        lappend c [lindex $v::dname $r]
                    }
                }
            }
            foreach x $c {
                if {[string match $pattern $x]} {
                    lappend o [file join $actual $x]
                }
            }
        } elseif {$type & ([isDir $tag]?4:16)} {
            set o [list $actual]
        }
        return $o
    }

    proc fileattributes {db root path args} {
        switch -- [llength $args] {
            0 { return [::vfs::listAttributes] }
            1 { return [eval [linsert $args 0 \
                                        ::vfs::attributesGet $root $path]] }
            2 { return [eval [linsert $args 0 \
                                        ::vfs::attributesSet $root $path]] }
        }
    }

    proc open {db file mode permissions} {
        switch -- $mode {
            "" - r {
                set tag [lookUp $db $file]
                if {[isDir $tag]} { fail ENOENT }
                foreach {name size date contents} [eval $tag] break
                if {[string length $contents] != $size} {
                    set contents [vfs::zip -mode decompress $contents]
                }
                set fd [vfs::memchan]
                fconfigure $fd -translation binary
                puts -nonewline $fd $contents
                fconfigure $fd -translation auto
                seek $fd 0
                return [list $fd]
            }
            w { ;# make sure the parent dir exists and create empty
                set tag [lookUp $db [file dirname $file]]
                set dpos [lindex $tag 2]
                set curr [lindex $v::files $dpos]
                set fpos [llength $curr]
                lappend curr [list [file tail $file] 0 [clock seconds] ""]
                lset v::files $dpos $curr
                set fd [vfs::memchan]
                return [list $fd [list ::vfs::m2m::doClose $db $dpos $fpos $fd]]
            }
            default { error "unsupported access mode: $mode" }
        }
    }

    proc doClose {db dpos fpos fd} {
        fconfigure $fd -translation binary
        seek $fd 0
        set d [read $fd]
        set n [string length $d]
        set z [vfs::zip -mode compress $d]
        if {[string length $z] < $n} { set d $z }
        lset v::files $dpos $fpos 1 $n
        lset v::files $dpos $fpos 3 $d
    }

    proc access {db path mode} {
        lookUp $db $path
    }

    proc stat {db path} {
        set tag [lookUp $db $path]
        set l 1
        if {[isDir $tag]} {
            set t directory
            set s 0
            set d 0
            set c ""
            incr l [llength [subFileNames $tag]]
            incr l [llength [subDirNums $tag]]
        } else {
            set t file
            foreach {n s d c} [eval $tag] break
        }
        return [list type $t size $s atime $d ctime $d mtime $d nlink $l \
            csize [string length $c] gid 0 uid 0 ino 0 mode 0777]
    }

    proc createdirectory {db path} {
        set tag [lookUp $db [file dirname $path]]
        lappend v::dname [file tail $path]
        lappend v::prows [lindex $tag 2]
        lappend v::files {}
    }

    proc utime {db path atime mtime} {
        set tag [lookUp $db $path]
        if {![isDir $tag]} {
            lset v::files [lindex $tag 2] [lindex $tag 3] 2 $mtime
        }
    }
}
