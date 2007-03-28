/* Written by Matt Newman and Jean-Claude Wippler, as part of Tclkit.
 * March 2003 - placed in the public domain by the authors.
 *
 * Expose TclSetLibraryPath to scripts (in 8.4 only, 8.5 has "encoding dirs").
 */

#if 10 * TCL_MAJOR_VERSION + TCL_MINOR_VERSION < 85

#include <tcl.h>

/* in tclInt.h: */
Tcl_Obj* TclGetLibraryPath();

/* Support for encodings, from Vince Darley <vince.darley@eurobios.com> */
static int
LibraryPathObjCmd(dummy, interp, objc, objv)
  ClientData dummy;
  Tcl_Interp *interp;
  int objc;
  Tcl_Obj *CONST objv[];
{
  if (objc == 1) {
  	Tcl_SetObjResult(interp, TclGetLibraryPath());
  } else {
  	Tcl_Obj *path=Tcl_DuplicateObj(objv[1]);
  	TclSetLibraryPath(Tcl_NewListObj(1,&path));
  	TclpSetInitialEncodings();
  	Tcl_FindExecutable(Tcl_GetVar(interp, "argv0", TCL_GLOBAL_ONLY));
  }
  return TCL_OK;
}

/*
 * Public Entrypoint
 */

DLLEXPORT int Pwb_Init(Tcl_Interp *interp)
{
  Tcl_CreateObjCommand(interp, "librarypath", LibraryPathObjCmd, 0, 0);
  return Tcl_PkgProvide( interp, "pwb", "1.1");
}

#endif
