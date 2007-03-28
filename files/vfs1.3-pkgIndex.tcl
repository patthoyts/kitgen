namespace eval ::vfs {}
variable vfs::dll [file join $dir libvfs1.3.dylib]

proc loadvfs {dll} {
    global auto_path
    if {![file exists $dll]} { return }
    set dir [file dirname $dll]
    if {[lsearch -exact $auto_path $dir] == -1} {
    	lappend auto_path $dir
    }
    load $dll
}

package ifneeded vfs        1.3 [list loadvfs $vfs::dll]

# Allow optional redirect of VFS_LIBRARY components.  Only necessary
# for testing, but could be used elsewhere.
if {[info exists ::env(VFS_LIBRARY)]} { set dir $::env(VFS_LIBRARY) }
package ifneeded starkit    1.3.1 [list source [file join $dir starkit.tcl]]
package ifneeded vfslib     1.3.1 [list source [file join $dir vfslib.tcl]]
package ifneeded vfs::mk4    1.10 [list source [file join $dir mk4vfs.tcl]]
package ifneeded vfs::zip     1.0 [list source [file join $dir zipvfs.tcl]]

#compat
package ifneeded mk4vfs      1.10 [list source [file join $dir mk4vfs.tcl]]
