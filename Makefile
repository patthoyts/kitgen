TCL_CVS   = :pserver:anonymous@tcl.cvs.sourceforge.net:/cvsroot/tcl
TK_CVS    = :pserver:anonymous@tktoolkit.cvs.sourceforge.net:/cvsroot/tktoolkit
VFS_CVS   = :pserver:anonymous@tclvfs.cvs.sourceforge.net:/cvsroot/tclvfs
VLERQ_CVS = :pserver:anonymous@equi4.com:/home/cvs
ZLIB_CVS  = :pserver:anonymous@equi4.com:/home/cvs
ZLIB_URL  = http://www.zlib.net/zlib-1.2.3.tar.gz

unspecified-target:

8.4:
	mkdir -p $@ && cd $@ && \
	  cvs -d $(TCL_CVS) co -r core-8-4-branch tcl && \
	  cvs -d $(TK_CVS) co -r core-8-4-branch tk
	#sh config.sh 8.4/base-aqua univ aqua
	#sh config.sh 8.4/base-x11 univ
	sh config.sh 8.4/base-std

8.5:
	mkdir -p $@ && cd $@ && \
	  cvs -d $(TCL_CVS) co tcl && \
	  cvs -d $(TK_CVS) co tk
	#sh config.sh 8.5/base-aqua univ aqua thread
	#sh config.sh 8.5/base-x11 univ thread
	sh config.sh 8.5/base-std thread

8.x:
	mkdir -p $@ && cd $@ && \
	  cvs -d $(TCL_CVS) co thread && \
	  cvs -d $(VFS_CVS) co tclvfs && \
	  cvs -d $(VLERQ_CVS) co -d vlerq vlerq/tcl && \
	  cvs -d $(ZLIB_CVS) co zlib
	
cvs:
	for i in 8*/*/CVS; do (cd `dirname $$i`; cvs up); done

small: 8.x 8.4
	sh config.sh 8.4/kit-small cli dyn
	cd 8.4/kit-small && $(MAKE) && $(MAKE) clean

large: 8.x 8.5
	sh config.sh 8.5/kit-large aqua univ thread allenc allmsgs tzdata
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

.PHONY: all base tidy clean distclean cvs small large docs
