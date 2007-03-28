package ifneeded Tk 8.4 \
  [string map [list @@ [file join $dir .. libtk8.4[info sharedlibext]]] {
    if {[lsearch -exact [info loaded] {{} Tk}] >= 0} {
      load "" Tk
    } else {
      load @@ Tk
    }
  }]
