TCL_URL	= http://prdownloads.sourceforge.net/tcl/
TK_URL	= http://prdownloads.sourceforge.net/tcl/

TCL_CVS   =":pserver:anonymous@tcl.cvs.sourceforge.net/cvsroot/tcl"
TK_CVS    =":pserver:anonymous@tktoolkit.cvs.sourceforge.net/cvsroot/tktoolkit"
VFS_CVS   =":pserver:anonymous@tclvfs.cvs.sourceforge.net/cvsroot/tclvfs"
ZLIB_CVS  =":pserver:anonymous@tkimg.cvs.sourceforge.net/cvsroot/tkimg"
ITCL_CVS  =":pserver:anonymous@incrtcl.cvs.sourceforge.net/cvsroot/incrtcl"
VQTCL_SVN ="http://tclkit.googlecode.com/svn/trunk/vqtcl"
MK_SVN    ="svn://svn.equi4.com/metakit/trunk"

unspecified-target:

checkout:
	mkdir 8.x && cd 8.x && \
	  cvs -qz6 -d$(VFS_CVS) co -d vfs tclvfs && \
	  cvs -qz6 -d$(ZLIB_CVS) co -d zlib tkimg/libz && \
	  cvs -qz6 -d$(TCL_CVS) co thread && \
	  cvs -qz6 -d$(ITCL_CVS) co -d itcl incrTcl/itcl && \
	  svn checkout $(VQTCL_SVN) vqtcl && \
	  svn checkout $(MK_SVN) mk
	mkdir 8.4 && cd 8.4 && \
	  cvs -qz6 -d$(TCL_CVS) co -r core-8-4-branch tcl && \
	  cvs -qz6 -d$(TK_CVS) co -r core-8-4-branch tk
	mkdir 8.5 && cd 8.5 && \
	  cvs -qz6 -d$(TCL_CVS) co -r core-8-5-branch tcl && \
	  cvs -qz6 -d$(TK_CVS) co -r core-8-5-branch tk

tars:
	mkdir 8.x && cd 8.x && \
	  wget -q $(TAR_URL)/vfs.tar.gz && tar xfz vfs.tar.gz && \
	  wget -q $(TAR_URL)/zlib.tar.gz && tar xfz zlib.tar.gz && \
	  wget -q $(TAR_URL)/vqtcl.tgz && tar xfz vqtcl.tgz && \
	  rm *gz && mv vfs tclvfs
	mkdir 8.4 && cd 8.4 && \
	  wget -q $(TCL_URL)/tcl8.4.19-src.tar.gz && tar xfz tcl8.4.19-src.tar.gz && \
	  wget -q $(TK_URL)/tk8.4.19-src.tar.gz && tar xfz tk8.4.19-src.tar.gz && \
	  rm *gz
	mkdir 8.5 && cd 8.5 && \
	  wget -q $(TCL_URL)/tcl8.5.6-src.tar.gz && tar xfz tcl8.5.6-src.tar.gz && \
	  wget -q $(TK_URL)/tk8.5.6-src.tar.gz && tar xfz tk8.5.6-src.tar.gz && \
	  rm *gz

configs:
	sh config.sh 8.4/base-std
	sh config.sh 8.4/kit-small cli dyn
	sh config.sh 8.5/base-std thread
	sh config.sh 8.5/kit-large aqua univ thread allenc allmsgs tzdata

small: configs
	cd 8.4/kit-small && $(MAKE) && $(MAKE) clean

large: configs
	cd 8.5/kit-large && $(MAKE) && $(MAKE) clean

base tidy:
	for i in 8*/base-*/Makefile; do (cd `dirname $$i`; $(MAKE) $@); done
all clean distclean tclkit-cli tclkit-dyn tclkit-gui:
	for i in 8*/kit-*/Makefile; do (cd `dirname $$i`; $(MAKE) $@); done

# this is not for general use, due to the custom script and hard-wired paths
docs:
	markdown-tm 'Kitgen - Tclkit Lite executable builder' \
	  <README >~/Sites/www.equi4.com/kitgen.html
	markdown-tm 'Swisskit - a big single-file Tcl/Tk for Mac OS X' \
	  <README.swisskit >~/Sites/www.equi4.com/swisskit.html

.PHONY: all base tidy clean distclean small large docs tars configs
