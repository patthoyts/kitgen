TAR_URL	= http://www.equi4.com/pub/tk/tars

unspecified-target:

tars:
	mkdir 8.x && cd 8.x && \
	  wget -q $(TAR_URL)/vfs.tar.gz && tar xfz vfs.tar.gz && \
	  wget -q $(TAR_URL)/zlib.tar.gz && tar xfz zlib.tar.gz && \
	  wget -q $(TAR_URL)/vqtcl.tgz && tar xfz vqtcl.tgz && \
	  rm *gz && mv vfs tclvfs
	mkdir 8.4 && cd 8.4 && \
	  wget -q $(TAR_URL)/tcl.tar.gz && tar xfz tcl.tar.gz && \
	  wget -q $(TAR_URL)/tk.tar.gz && tar xfz tk.tar.gz && \
	  rm *gz
	mkdir 8.5 && cd 8.5 && \
	  wget -q $(TAR_URL)/tcl85.tar.gz && tar xfz tcl85.tar.gz && \
	  wget -q $(TAR_URL)/tk85.tar.gz && tar xfz tk85.tar.gz && \
	  rm *gz && mv tcl85 tcl && mv tk85 tk

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
