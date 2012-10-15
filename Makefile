
# This program is licensed under the GPL v2. See file COPYING for details.

.PHONY:	ALWAYS
.SUFFIXES:

all: ALWAYS
	cd src && make all
	@echo; echo Build done.; echo

# From command line to subproceses...
export PREFIX
export LIBEXECDIR
export PJXJAR

TRG_BINS = m2vcut-gui m2vfilter m2vscan m2vtoyuv mp2cutpoints \
	fileparts filerotate textdisp \
	libpreload_ffm2vtoyuv4mpeghax.so \
	m2vmp2cut.pl m2vmp2cut.sh m2v_catfiltered.py m2v_somehdrinfo.py \
	wrapper.sh lvev6frames.pl getyuv.pl getmp2.sh m2vmp2cut.pm

TRG_DOCS = Examples m2vcut_help-default m2vcut_help-fi_FI Options Usage

install: all
	./makehelper install "$(TRG_BINS:%=bin/%)" "$(TRG_DOCS:%=doc/%)"

release: tools/tarlisted32
	./makehelper release

snapshot: tools/tarlisted32
	./makehelper snapshot

tools/tarlisted32:
	cd tools && sh tarlisted32.c

clean: ALWAYS
	rm -f *~
	cd src; make $(MAKECMDGOALS)

distclean: clean
	rm -f tools/tarlisted32
