package ifneeded Tk 8.5a6 \
  [string map [list @@ [file join $dir .. libtk8.5[info sharedlibext]]] {
    if {[lsearch -exact [info loaded] {{} Tk}] >= 0} {
      load "" Tk
    } else {
      load @@ Tk
    }
  }]
