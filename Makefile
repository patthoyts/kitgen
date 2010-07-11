# Initial setup of kitgen for building tclkit.
#
#  usage: make tars
#         make largs
#
# Once setup, read the notes and use config.sh to adjust your build.

URL	= http://prdownloads.sourceforge.net/tcl

unspecified-target:

tars:
	[ -d 8.5 ] || mkdir 8.5
	[ -f tcl8.5.8-src.tar.gz ] || wget -q $(URL)/tcl8.5.8-src.tar.gz
	tar -C 8.5 -xzf tcl8.5.8-src.tar.gz
	[ -f tk8.5.8-src.tar.gz ] || wget -q $(URL)/tk8.5.8-src.tar.gz
	tar -C 8.5 -xzf tk8.5.8-src.tar.gz
	ln -sf tcl8.5.8  8.5/tcl
	ln -sf tk8.5.8   8.5/tk

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

.PHONY: all base tidy clean distclean small large tars configs
