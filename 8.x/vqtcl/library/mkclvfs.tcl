# mkclvfs.tcl -- Metakit Compatibility Layer Virtual File System driver
# Rewritten from mk4vfs.tcl, orig by Matt Newman and Jean-Claude Wippler 

# 1.0 initial release
# 1.1 view size renamed to count
# 1.2 replace view calls by vget (simpler and faster)
# 1.3 modified to use the vlerq extension i.s.o. thrive
# 1.4 minor cleanup
# 1.5 adjusted for vlerq 4

package provide vfs::mkcl 1.5

package require vfs
package require vlerq

namespace eval ::vfs::mkcl {
    interp alias {} ::vfs::mkcl::vopen {} ::vlerq open
    interp alias {} ::vfs::mkcl::vget {} ::vlerq get
        
    namespace eval v {
        variable seq 0  ;# used to generate a unique db handle
        variable rootv  ;# maps handle to the "dirs" root view
        variable dname  ;# maps handle to cached list of directory names
        variable prows  ;# maps handle to cached list of parent row numbers
    }

# public

    proc Mount {mkfile local args} {
        set db mkclvfs[incr v::seq]
        set v::rootv($db) [vget [vopen $mkfile] 0 dirs]
        set v::dname($db) [vget $v::rootv($db) * name]
        set v::prows($db) [vget $v::rootv($db) * parent]
        #parray v::dname
        #parray v::prows
        ::vfs::filesystem mount $local [list ::vfs::mkcl::handler $db]
        ::vfs::RegisterMount $local [list ::vfs::mkcl::Unmount $db]
        return $db
    }
    
    proc Unmount {db local} {
        ::vfs::filesystem unmount $local
        unset v::rootv($db) v::dname($db) v::prows($db)
    }
    
# private

    proc handler {db cmd root path actual args} {
        #puts [list MKCL $db <$cmd> r: $root p: $path a: $actual $args]
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
        set dirs $v::rootv($db)
        set parent 0
        set elems [file split $path]
        set remain [llength $elems]
        foreach e $elems {
            set r ""
            foreach r [lsearch -exact -int -all $v::prows($db) $parent] {
                if {$e eq [lindex $v::dname($db) $r]} {
                    set parent $r
                    incr remain -1
                    break
                }
            }
            if {$parent != $r} {
                if {$remain == 1} {
                    set files [vget $dirs $parent files]
                    set i [lsearch -exact [vget $files * name] $e]
                    if {$i >= 0} {
                        # eval this 3-item result returns the info about 1 file
                        return [list vget $files $i]
                    }
                }
                fail ENOENT
            }
        }
        # evaluating this 4-item result returns the files subview
        list vget $dirs $parent files
    }
    
    proc isDir {tag} {
        expr {[llength $tag] == 4}
    }
    
    if {$::tcl_version eq "8.4"} {
            proc apply {cmd args} { eval [concat $cmd $args] }
    } else {
            proc apply {cmd args} { {*}$cmd {*}$args }
    }
    
# methods

    proc matchindirectory {db path actual pattern type} {
        set o {}
        if {$type == 0} { set type 20 }
        if {[catch {set tag [lookUp $db $path]} err]} { return {} }
        if {$pattern ne ""} {
            set c {}
            if {[isDir $tag]} {
                # collect file names
                if {$type & 16} {
                    set c [apply $tag * 0]
                }
                # collect directory names
                if {$type & 4} {
                    foreach r [lsearch -exact -int -all $v::prows($db) \
                                                            [lindex $tag 2]] {
                        lappend c [lindex $v::dname($db) $r]
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
            1 { set index [lindex $args 0]
                    return [::vfs::attributesGet $root $path $index] }
            2 { fail EROFS }
        }
    }
    
    proc open {db file mode permissions} {
        if {$mode ne "" && $mode ne "r"} { fail EROFS }
        set tag [lookUp $db $file]
        if {[isDir $tag]} { fail ENOENT }
        foreach {name size date contents} [apply $tag *] break
        if {[string length $contents] != $size} {
            set contents [::vfs::zip -mode decompress $contents]
        }
        set fd [::vfs::memchan]
        fconfigure $fd -translation binary
        puts -nonewline $fd $contents
        fconfigure $fd -translation auto -encoding [encoding system]
        seek $fd 0
        list $fd
    }
    
    proc access {db path mode} {
        if {$mode & 2} { fail EROFS }
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
            incr l [apply $tag #]
            incr l [llength [lsearch -exact -int -all $v::prows($db) \
                                                        [lindex $tag 2]]]
        } else {
            set t file
            foreach {n s d c} [apply $tag *] break
        }
        list type $t size $s atime $d ctime $d mtime $d nlink $l \
                    csize [string length $c] gid 0 uid 0 ino 0 mode 0777
    }
}
