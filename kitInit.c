/* 
 * tclAppInit.c --
 *
 *  Provides a default version of the main program and Tcl_AppInit
 *  procedure for Tcl applications (without Tk).  Note that this
 *  program must be built in Win32 console mode to work properly.
 *
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 2000-2006 Jean-Claude Wippler <jcw@equi4.com>
 * Copyright (c) 2003-2006 ActiveState Software Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifdef KIT_INCLUDES_TK
#include <tk.h>
#else
#include <tcl.h>
#endif

#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

/* defined in tclInt.h */
extern Tcl_Obj* TclGetStartupScriptPath();
extern void TclSetStartupScriptPath(Tcl_Obj*);
extern char* TclSetPreInitScript (char*);

Tcl_AppInitProc	Pwb_Init, Rechan_Init, Vfs_Init, Zlib_Init;
#ifdef KIT_LITE
Tcl_AppInitProc	Vlerq_Init, Vlerq_SafeInit;
#else
Tcl_AppInitProc	Mk4tcl_Init;
#endif
#ifdef TCL_THREADS
Tcl_AppInitProc	Thread_Init;
#endif
#ifdef KIT_INCLUDES_ITCL
Tcl_AppInitProc	Itcl_Init;
#endif
#ifdef _WIN32
Tcl_AppInitProc	Dde_Init, Registry_Init;
#endif

/* insert custom prototypes here */

#ifdef WIN32
#define DEV_NULL "NUL"
#else
#define DEV_NULL "/dev/null"
#endif

static void TclKit_InitStdChannels(void);

/*
 *  Attempt to load a "boot.tcl" entry from the embedded MetaKit file.
 *  This code uses either the Mk4tcl or the vlerq extension (-DKIT_LITE).
 *  If there isn't one, try to open a regular "setup.tcl" file instead.
 *  If that fails, this code will throw an error, using a message box.
 *
 * The appInitCmd will only be run once in the main (initial) interpreter.
 * The preInitCmd will be registered to run in any created interpreter.
 */

static char appInitCmd[] = 
"proc tclKitInit {} {\n"
    "rename tclKitInit {}\n"
    "catch {load {} zlib}\n"
    "if {![info exists ::tcl::basekit]} {\n"
        "namespace eval ::tcl { variable basekit [info nameofexecutable] }\n"
    "}\n"
#ifdef KIT_LITE
    "load {} vlerq\n"
    "namespace eval ::vlerq {}\n"
    "if {[catch { vlerq open $::tcl::basekit } ::vlerq::starkit_root]} {\n"
      "set n -1\n"
    "} else {\n"
      "set files [vlerq get $::vlerq::starkit_root 0 dirs 0 files]\n"
      "set n [lsearch [vlerq get $files * name] boot.tcl]\n"
    "}\n"
    "if {$n >= 0} {\n"
        "array set a [vlerq get $files $n]\n"
#else
    "load {} Mk4tcl\n"
    "mk::file open exe $::tcl::basekit -readonly\n"
    "set n [mk::select exe.dirs!0.files name boot.tcl]\n"
    "if {[llength $n] == 1} {\n"
        "array set a [mk::get exe.dirs!0.files!$n]\n"
#endif
        "if {![info exists a(contents)]} { error {no boot.tcl file} }\n"
        "if {$a(size) != [string length $a(contents)]} {\n"
        	"set a(contents) [zlib decompress $a(contents)]\n"
        "}\n"
        "if {$a(contents) eq \"\"} { error {empty boot.tcl} }\n"
        "uplevel #0 $a(contents)\n"
    "} elseif {[lindex $::argv 0] eq \"-init-\"} {\n"
        "uplevel #0 { source [lindex $::argv 1] }\n"
        "exit\n"
    "} else {\n"
        "error \"\n  $::tcl::basekit has no VFS data to start up\"\n"
    "}\n"
"}\n"
"tclKitInit"
;

static char preInitCmd[] =
"proc tclKitPreInit {} {\n"
    "rename tclKitPreInit {}\n"
    /* In 8.5 we need to set these paths for child interps */
    "global tcl_library tcl_libPath tcl_version\n"
    "set noe [info nameofexecutable]\n"
    "set tcl_library [file join $noe lib tcl$tcl_version]\n"
    "set tcl_libPath [list $tcl_library [file join $noe lib]]\n"
    "set tcl_pkgPath [list $tcl_library [file join $noe lib]]\n"
"}\n"
"tclKitPreInit"
;

static const char initScript[] =
"if {[file isfile [file join $::tcl::basekit main.tcl]]} {\n"
    "if {[info commands console] != {}} { console hide }\n"
    "set tcl_interactive 0\n"
    "incr argc\n"
    "set argv [linsert $argv 0 $argv0]\n"
    "set argv0 [file join $::tcl::basekit main.tcl]\n"
"} else continue\n"
;

/*
 * If set, this is the path to the base kit
 */
static char *tclKitPath = NULL;

#ifdef WIN32
__declspec(dllexport) int
#else
extern int
#endif
TclKit_AppInit(Tcl_Interp *interp)
{
    /*
     * Ensure that std channels exist (creating them if necessary)
     */
    TclKit_InitStdChannels();

#ifdef KIT_INCLUDES_ITCL
    Tcl_StaticPackage(0, "Itcl", Itcl_Init, NULL);
#endif
#ifdef KIT_LITE
    Tcl_StaticPackage(0, "vlerq", Vlerq_Init, Vlerq_SafeInit);
#else
    Tcl_StaticPackage(0, "Mk4tcl", Mk4tcl_Init, NULL);
#endif
#if 10 * TCL_MAJOR_VERSION + TCL_MINOR_VERSION < 85
    Tcl_StaticPackage(0, "pwb", Pwb_Init, NULL);
#endif
    Tcl_StaticPackage(0, "rechan", Rechan_Init, NULL);
    Tcl_StaticPackage(0, "vfs", Vfs_Init, NULL);
    Tcl_StaticPackage(0, "zlib", Zlib_Init, NULL);
#ifdef TCL_THREADS
    Tcl_StaticPackage(0, "Thread", Thread_Init, NULL);
#endif
#ifdef _WIN32
    Tcl_StaticPackage(0, "dde", Dde_Init, NULL);
    Tcl_StaticPackage(0, "registry", Registry_Init, NULL);
#endif
#ifdef KIT_INCLUDES_TK
    Tcl_StaticPackage(0, "Tk", Tk_Init, Tk_SafeInit);
#endif

    /* insert custom packages here */

    /* the tcl_rcFileName variable only exists in the initial interpreter */
#ifdef _WIN32
    Tcl_SetVar(interp, "tcl_rcFileName", "~/tclkitrc.tcl", TCL_GLOBAL_ONLY);
#else
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclkitrc", TCL_GLOBAL_ONLY);
#endif

    if (tclKitPath != NULL) {
      	/*
      	 * If we have a tclKitPath set, then set that to ::tcl::basekit.
      	 * This will be used instead of 'info nameofexecutable' for
      	 * determining the location of the base kit.  This is necessary
      	 * for DLL-based starkits.
      	 *
      	 * This code equates to:
      	 *   namespace eval ::tcl [list variable basekit $tclKitPath]
      	 * Could consider using Tcl_LinkVar instead.
      	 */
      	Tcl_Obj *objPtr;
      	Tcl_Obj *evobjPtr;

      	evobjPtr = Tcl_NewObj();
      	Tcl_ListObjAppendElement(interp, evobjPtr,
      		Tcl_NewStringObj("variable", -1));
      	Tcl_ListObjAppendElement(interp, evobjPtr,
      		Tcl_NewStringObj("basekit", -1));
      	Tcl_ListObjAppendElement(interp, evobjPtr,
      		Tcl_NewStringObj(tclKitPath, -1));
      	Tcl_IncrRefCount(evobjPtr);

      	objPtr = Tcl_NewObj();
      	Tcl_ListObjAppendElement(interp, objPtr,
      		Tcl_NewStringObj("namespace", -1));
      	Tcl_ListObjAppendElement(interp, objPtr,
      		Tcl_NewStringObj("eval", -1));
      	Tcl_ListObjAppendElement(interp, objPtr,
      		Tcl_NewStringObj("::tcl", -1));
      	Tcl_ListObjAppendElement(interp, objPtr, evobjPtr);
      	Tcl_IncrRefCount(objPtr);
      	if (Tcl_EvalObjEx(interp, objPtr, TCL_GLOBAL_ONLY) != TCL_OK) {
      	    Tcl_DecrRefCount(objPtr);
      	    Tcl_DecrRefCount(evobjPtr);
      	    goto error;
      	}
      	Tcl_DecrRefCount(objPtr);
      	Tcl_DecrRefCount(evobjPtr);
    }

#ifdef KIT_DLL
    TclSetPreInitScript(preInitCmd);
    if ((Tcl_EvalEx(interp, appInitCmd, -1, TCL_EVAL_GLOBAL) == TCL_ERROR)
           || (Tcl_Init(interp) == TCL_ERROR))
        goto error;
#else /* not a dll */
    TclSetPreInitScript(appInitCmd);
    if (Tcl_Init(interp) == TCL_ERROR)
        goto error;
#endif

#if defined(KIT_INCLUDES_TK) && defined(_WIN32)
    if (Tk_Init(interp) == TCL_ERROR)
        goto error;
    if (Tk_CreateConsoleWindow(interp) == TCL_ERROR)
        goto error;
#endif

    /* messy because TclSetStartupScriptPath is called slightly too late */
    if (Tcl_Eval(interp, initScript) == TCL_OK) {
        Tcl_Obj* path = TclGetStartupScriptPath();
      	TclSetStartupScriptPath(Tcl_GetObjResult(interp));
      	if (path == NULL)
        	  Tcl_Eval(interp, "incr argc -1; set argv [lrange $argv 1 end]");
    }

    Tcl_SetVar(interp, "errorInfo", "", TCL_GLOBAL_ONLY);
    Tcl_ResetResult(interp);
    return TCL_OK;

error:
#if defined(KIT_INCLUDES_TK) && defined(_WIN32)
    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, Tcl_GetStringResult(interp), "Error in Tclkit",
        MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    ExitProcess(1);
    /* we won't reach this, but we need the return */
#endif
    return TCL_ERROR;
}

#ifdef WIN32
__declspec(dllexport) char *
#else
extern char *
#endif
TclKit_SetKitPath(CONST char *kitPath)
{
    /*
     * Allow someone to define an alternate path to the base kit
     * than 'info nameofexecutable'.
     */
    if (kitPath) {
      	int len = (int)strlen(kitPath);
      	if (tclKitPath) {
      	    ckfree(tclKitPath);
      	}
      	tclKitPath = (char *) ckalloc(len + 1);
      	memcpy(tclKitPath, kitPath, len);
      	tclKitPath[len] = '\0';
    }
    return tclKitPath;
}

static void
TclKit_InitStdChannels(void)
{
    Tcl_Channel chan;

    /*
     * We need to verify if we have the standard channels and create them if
     * not.  Otherwise internals channels may get used as standard channels
     * (like for encodings) and panic.
     */
    chan = Tcl_GetStdChannel(TCL_STDIN);
    if (chan == NULL) {
      	chan = Tcl_OpenFileChannel(NULL, DEV_NULL, "r", 0);
      	if (chan != NULL) {
      	    Tcl_SetChannelOption(NULL, chan, "-encoding", "utf-8");
      	}
      	Tcl_SetStdChannel(chan, TCL_STDIN);
    }
    chan = Tcl_GetStdChannel(TCL_STDOUT);
    if (chan == NULL) {
      	chan = Tcl_OpenFileChannel(NULL, DEV_NULL, "w", 0);
      	if (chan != NULL) {
      	    Tcl_SetChannelOption(NULL, chan, "-encoding", "utf-8");
      	}
      	Tcl_SetStdChannel(chan, TCL_STDOUT);
    }
    chan = Tcl_GetStdChannel(TCL_STDERR);
    if (chan == NULL) {
      	chan = Tcl_OpenFileChannel(NULL, DEV_NULL, "w", 0);
      	if (chan != NULL) {
      	    Tcl_SetChannelOption(NULL, chan, "-encoding", "utf-8");
      	}
      	Tcl_SetStdChannel(chan, TCL_STDERR);
    }
}
