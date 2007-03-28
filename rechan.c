/* Written by Matt Newman and Jean-Claude Wippler, as part of Tclkit.
 * March 2003 - placed in the public domain by the authors.
 *
 * Reflecting channel interface
 */

#include <tcl.h>

#ifndef TCL_DECLARE_MUTEX
#define TCL_DECLARE_MUTEX(v)
#define Tcl_MutexLock(v)
#define Tcl_MutexUnlock(v)
#endif

  static int mkChanSeq = 0;
  TCL_DECLARE_MUTEX(rechanMutex)

/* Uncomment for Linux or other non-Solaris OS's for memcpy declaration */
#include <memory.h>

/* Uncomment for Solaris (and comment above) for memcpy declaration */
/* #include <string.h> */

#ifndef EINVAL
#define EINVAL 9
#endif

typedef struct
{
  Tcl_Channel _chan;
  int _validMask;
  int _watchMask;
  Tcl_Interp* _interp;
  Tcl_Obj* _context;
  Tcl_Obj* _seek;
  Tcl_Obj* _read;
  Tcl_Obj* _write;
  Tcl_Obj* _name;
  Tcl_TimerToken _timer;
} ReflectingChannel;

static ReflectingChannel*
rcCreate (Tcl_Interp* ip_, Tcl_Obj* context_, int mode_, const char* name_)
{
  ReflectingChannel* cp = (ReflectingChannel*) Tcl_Alloc (sizeof *cp);

  cp->_validMask = mode_;
  cp->_watchMask = 0;
  cp->_chan = 0;
  cp->_context = context_;
  cp->_interp = ip_;
  cp->_name = Tcl_NewStringObj(name_, -1);
  cp->_timer = NULL;

    /* support Tcl_GetIndexFromObj by keeping these objectified */
  cp->_seek = Tcl_NewStringObj("seek", -1);
  cp->_read = Tcl_NewStringObj("read", -1);
  cp->_write = Tcl_NewStringObj("write", -1);

  Tcl_IncrRefCount(cp->_context);
  Tcl_IncrRefCount(cp->_seek);
  Tcl_IncrRefCount(cp->_read);
  Tcl_IncrRefCount(cp->_write);
  Tcl_IncrRefCount(cp->_name);

  return cp;
}

static Tcl_Obj*
rcBuildCmdList(ReflectingChannel* chan_, Tcl_Obj* cmd_)
{
  Tcl_Obj* vec = Tcl_DuplicateObj(chan_->_context);
  Tcl_IncrRefCount(vec);

  Tcl_ListObjAppendElement(chan_->_interp, vec, cmd_);
  Tcl_ListObjAppendElement(chan_->_interp, vec, chan_->_name);

  return vec; /* with refcount 1 */
}

static int
rcClose (ClientData cd_, Tcl_Interp* interp)
{
  ReflectingChannel* chan = (ReflectingChannel*) cd_;
  int n = -1;

  Tcl_SavedResult sr;
  Tcl_Obj* cmd = rcBuildCmdList(chan, Tcl_NewStringObj("close", -1));
  Tcl_Interp* ip = chan->_interp;

  Tcl_SaveResult(ip, &sr);

  if (Tcl_EvalObjEx(ip, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT) == TCL_OK)
    Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(ip), &n);

  Tcl_RestoreResult(ip, &sr);
  Tcl_DecrRefCount(cmd);

  if (chan->_timer != NULL) {
    Tcl_DeleteTimerHandler(chan->_timer);
    chan->_timer = NULL;
  }

  Tcl_DecrRefCount(chan->_context);
  Tcl_DecrRefCount(chan->_seek);
  Tcl_DecrRefCount(chan->_read);
  Tcl_DecrRefCount(chan->_write);
  Tcl_DecrRefCount(chan->_name);
  Tcl_Free((char*) chan);

  return TCL_OK;
}

static int
rcInput (ClientData cd_, char* buf, int toRead, int* errorCodePtr)
{
  ReflectingChannel* chan = (ReflectingChannel*) cd_;
  int n = -1;

  if (chan->_validMask & TCL_READABLE) {
    Tcl_SavedResult sr;
    Tcl_Obj* cmd = rcBuildCmdList(chan, chan->_read);
    Tcl_Interp* ip = chan->_interp;

    Tcl_ListObjAppendElement(NULL, cmd, Tcl_NewIntObj(toRead));
    Tcl_SaveResult(ip, &sr);

    if (Tcl_EvalObjEx(ip, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT) == TCL_OK) {
      void* s = Tcl_GetByteArrayFromObj(Tcl_GetObjResult(ip), &n);
      if (0 <= n && n <= toRead)
      	if (n > 0)
      	  memcpy(buf, s, n);
      	else
      	  chan->_watchMask &= ~TCL_READABLE;
      else
      	n = -1;
    }

    Tcl_RestoreResult(ip, &sr);
    Tcl_DecrRefCount(cmd);
  }

  if (n < 0)
    *errorCodePtr = EINVAL;
  return n;
}

static int
rcOutput (ClientData cd_, const char* buf, int toWrite, int* errorCodePtr)
{
  ReflectingChannel* chan = (ReflectingChannel*) cd_;
  int n = -1;

  if (chan->_validMask & TCL_WRITABLE) {
    Tcl_SavedResult sr;
    Tcl_Obj* cmd = rcBuildCmdList(chan, chan->_write);
    Tcl_Interp* ip = chan->_interp;

    Tcl_ListObjAppendElement(NULL, cmd,
	      		Tcl_NewByteArrayObj((unsigned char*) buf, toWrite));
    Tcl_SaveResult(ip, &sr);

    if (Tcl_EvalObjEx(ip, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT) == TCL_OK &&
      	Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(ip), &n) == TCL_OK)
      if (0 <= n && n <= toWrite)
      	chan->_watchMask = chan->_validMask;
      else
      	n = -1;

    Tcl_RestoreResult(ip, &sr);
    Tcl_DecrRefCount(cmd);
  }

  if (n < 0)
    *errorCodePtr = EINVAL;
  return n;
}

static int
rcSeek (ClientData cd_, long offset, int seekMode, int* errorCodePtr)
{
  ReflectingChannel* chan = (ReflectingChannel*) cd_;
  int n = -1;

  Tcl_SavedResult sr;
  Tcl_Obj* cmd = rcBuildCmdList(chan, chan->_seek);
  Tcl_Interp* ip = chan->_interp;

  Tcl_ListObjAppendElement(NULL, cmd, Tcl_NewLongObj(offset));
  Tcl_ListObjAppendElement(NULL, cmd, Tcl_NewIntObj(seekMode));
  Tcl_SaveResult(ip, &sr);

  if (Tcl_EvalObjEx(ip, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT) == TCL_OK &&
      Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(ip), &n) == TCL_OK)
    chan->_watchMask = chan->_validMask;

  Tcl_RestoreResult(ip, &sr);
  Tcl_DecrRefCount(cmd);

  if (n < 0)
    *errorCodePtr = EINVAL;
  return n;
}

static void
rcTimerProc (ClientData cd_)
{
  ReflectingChannel* chan = (ReflectingChannel*) cd_;

  if (chan->_timer != NULL)
    Tcl_DeleteTimerHandler(chan->_timer);
  chan->_timer = NULL;
  Tcl_NotifyChannel(chan->_chan, chan->_watchMask);
}

static void
rcWatchChannel (ClientData cd_, int mask)
{
  ReflectingChannel* chan = (ReflectingChannel*) cd_;

  /* Dec 2001: adopting logic used in Andreas Kupries' memchan, i.e. timers */

  if (mask) {
    chan->_watchMask = mask & chan->_validMask;
    if (chan->_watchMask && chan->_timer == NULL)
      chan->_timer = Tcl_CreateTimerHandler(5, rcTimerProc, cd_);
  } else if (chan->_timer != NULL) {
    Tcl_DeleteTimerHandler(chan->_timer);
    chan->_timer = NULL;
  }
}

static int
rcGetFile (ClientData cd_, int direction, ClientData* handlePtr)
{
  return TCL_ERROR;
}

static int
rcBlock (ClientData cd_, int mode)
{
  return 0;
}

static Tcl_ChannelType reChannelType = {
  "rechan",       /* Type name.                                    */
  0/*rcBlock*/,	  /* Set blocking/nonblocking behaviour. NULL'able */
  rcClose,        /* Close channel, clean instance data            */
  rcInput,        /* Handle read request                           */
  rcOutput,       /* Handle write request                          */
  rcSeek,         /* Move location of access point.    NULL'able   */
  0,              /* Set options.                      NULL'able   */
  0,              /* Get options.                      NULL'able   */
  rcWatchChannel, /* Initialize notifier                           */
  rcGetFile       /* Get OS handle from the channel.               */
};

static int
cmd_rechan(ClientData cd_, Tcl_Interp* ip_, int objc_, Tcl_Obj*const* objv_)
{
  ReflectingChannel *rc;
  int mode;
  char buffer [20];

  if (objc_ != 3) {
    Tcl_WrongNumArgs(ip_, 1, objv_, "command mode");
    return TCL_ERROR;
  }

  if (Tcl_ListObjLength(ip_, objv_[1], &mode) == TCL_ERROR ||
      Tcl_GetIntFromObj(ip_, objv_[2], &mode) == TCL_ERROR)
    return TCL_ERROR;

  Tcl_MutexLock(&rechanMutex);
  sprintf(buffer, "rechan%d", ++mkChanSeq);
  Tcl_MutexUnlock(&rechanMutex);

  rc = rcCreate (ip_, objv_[1], mode, buffer);
  rc->_chan = Tcl_CreateChannel(&reChannelType, buffer, (ClientData) rc, mode);

  Tcl_RegisterChannel(ip_, rc->_chan);
  Tcl_SetChannelOption(ip_, rc->_chan, "-buffering", "none");
  Tcl_SetChannelOption(ip_, rc->_chan, "-blocking", "0");

  Tcl_SetResult(ip_, buffer, TCL_VOLATILE);
  return TCL_OK;
}

DLLEXPORT int Rechan_Init(Tcl_Interp* interp)
{
  if (!Tcl_InitStubs(interp, "8.4", 0))
    return TCL_ERROR;
  Tcl_CreateObjCommand(interp, "rechan", cmd_rechan, 0, 0);
  return Tcl_PkgProvide(interp, "rechan", "1.0");
}
