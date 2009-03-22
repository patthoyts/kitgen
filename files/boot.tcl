proc tclInit {} {
    rename tclInit {}

    global auto_path tcl_library tcl_libPath tcl_version tclkit_system_encoding

    # find the file to mount.
    set noe $::tcl::kitpath
    # resolve symlinks
    set noe [file dirname [file normalize [file join $noe __dummy__]]]
    set tcl_library [file join $noe lib tcl$tcl_version]
    set tcl_libPath [list $tcl_library [file join $noe lib]]

    # get rid of a build residue
    unset -nocomplain ::tclDefaultLibrary

    # The following code only gets executed if we don't have our exe
    # already mounted.  This should only happen once per thread.
    # We could use [vfs::filesystem info], but that would require
    # loading vfs into every interp.
    if {![file isdirectory $noe]} {
        load {} vfs

        # lookup and emulate "source" of lib/vfs1*/{vfs*.tcl,mk4vfs.tcl}
        if {[llength [info command mk::file]]} {
            set driver mk4

            # must use raw Metakit calls because VFS is not yet in place
            set d [mk::select exe.dirs parent 0 name lib]
            set d [mk::select exe.dirs parent $d -glob name vfs1*]
            
            foreach x {vfsUtils vfslib mk4vfs} {
                set n [mk::select exe.dirs!$d.files name $x.tcl]
                if {[llength $n] != 1} { error "$x: cannot find startup script"}
                
                set s [mk::get exe.dirs!$d.files!$n contents]
                catch {set s [zlib decompress $s]}
                uplevel #0 $s
            }

            # use on-the-fly decompression, if mk4vfs understands that
            # Note: 8.6 core zlib does not support this for mk4vfs
            if {![package vsatisfies [package require Tcl] 8.6]} {
                set mk4vfs::zstreamed 1
            }
        } else {
            set driver mkcl

            # use raw Vlerq calls if Mk4tcl is not available
            # $::vlerq::starkit_root is set in the init script in kitInit.c
            set rootv [vlerq get $::vlerq::starkit_root 0 dirs]
            set dname [vlerq get $rootv * name]
            set prows [vlerq get $rootv * parent]
            foreach r [lsearch -int -all $prows 0] {
                if {[lindex $dname $r] eq "lib"} break
            }
            
            # glob for a subdir in "lib", then source the specified file inside it
            foreach {d f} {
                vfs1* vfsUtils.tcl vfs1* vfslib.tcl vqtcl4* mkclvfs.tcl
            } {
                foreach z [lsearch -int -all $prows $r] {
                    if {[string match $d [lindex $dname $z]]} break
                }

                set files [vlerq get $rootv $z files]
                set names [vlerq get $files * name]

                set n [lsearch $names $f]
                if {$n < 0} { error "$d/$f: cannot find startup script"}
                
                set s [vlerq get $files $n contents]
                catch {set s [zlib decompress $s]}
                uplevel #0 $s
            }

            # hack the mkcl info so it will know this mount point as "exe"
            set vfs::mkcl::v::rootv(exe) $rootv
            set vfs::mkcl::v::dname(exe) $dname
            set vfs::mkcl::v::prows(exe) $prows
        }

        # mount the executable, i.e. make all runtime files available
        vfs::filesystem mount $noe [list ::vfs::${driver}::handler exe]

        # alter path to find encodings
        if {[info tclversion] eq "8.4"} {
            load {} pwb
            librarypath [info library]
        } else {
            encoding dirs [list [file join [info library] encoding]] ;# TIP 258
        }
        # if the C code passed us a system encoding, apply it here.
        if {[info exists tclkit_system_encoding]} {
            # It is possible the chosen encoding is unavailable in which case
            # we will be left with 'identity' to be handled below.
            catch {encoding system $tclkit_system_encoding}
            unset tclkit_system_encoding
        }
        # fix system encoding, if it wasn't properly set up (200207.004 bug)
        if {[encoding system] eq "identity"} {
            switch $::tcl_platform(platform) {
                windows		{ encoding system cp1252 }
                macintosh	{ encoding system macRoman }
                default		{ encoding system iso8859-1 }
            }
        }

        # now remount the executable with the correct encoding
        vfs::filesystem unmount $noe
        set noe $::tcl::kitpath
        # resolve symlinks
        set noe [file dirname [file normalize [file join $noe __dummy__]]]

        set tcl_library [file join $noe lib tcl$tcl_version]
        set tcl_libPath [list $tcl_library [file join $noe lib]]
        vfs::filesystem mount $noe [list ::vfs::${driver}::handler exe]
    }
    
    # load config settings file if present
    namespace eval ::vfs { variable tclkit_version 1 }
    catch { uplevel #0 [list source [file join $noe config.tcl]] }

    uplevel #0 [list source [file join $tcl_library init.tcl]]
    
    # reset auto_path, so that init.tcl's search outside of tclkit is cancelled
    set auto_path $tcl_libPath
}
