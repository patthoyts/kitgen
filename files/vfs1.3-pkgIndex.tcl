package ifneeded vfs        1.3   [list load {} vfs]
package ifneeded starkit    1.3.1 [list source [file join $dir starkit.tcl]]
package ifneeded vfslib     1.3.1 [list source [file join $dir vfslib.tcl]]
package ifneeded vfs::mk4    1.10 [list source [file join $dir mk4vfs.tcl]]
package ifneeded vfs::zip     1.0 [list source [file join $dir zipvfs.tcl]]
#compat
package ifneeded mk4vfs      1.10 [list source [file join $dir mk4vfs.tcl]]
