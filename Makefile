
# This program is licensed under the GPL v2. See file COPYING for details.

.PHONY:	ALWAYS
.SUFFIXES:

all: ALWAYS
	make -C src all
	@echo; echo Build done.; echo

# From command line to subproceses...
DESTDIR =
export DESTDIR
PREFIX =
export PREFIX
LIBEXECDIR =
export LIBEXECDIR
DATAROOTDIR =
export DATAROOTDIR
PJXJAR =
export PJXJAR

TRG_BINS = m2vcut-gui m2vfilter m2vscan m2vtoyuv mp2cutpoints m2vstream \
	assel-gui fileparts filerotate textdisp pgssupout \
	libpreload_ffm2vtoyuv4mpeghax.so merge-gui \
	m2vmp2cut.pl m2vmp2cut.sh m2v_catfiltered.py m2v_somehdrinfo.py \
	wrapper.sh lvev6frames.pl getyuv.pl getmp2.sh m2vmp2cut.pm \
	pxsuptime.py assel.pl merge.pl

TRG_DOCS = Examples m2vcut_help-default m2vcut_help-fi_FI Options Usage

install: all
	./makehelper install "$(TRG_BINS:%=bin/%)" "$(TRG_DOCS:%=doc/%)"

release:
	./makehelper release
	@echo Remember to do test build from the release archive.

snapshot:
	./makehelper snapshot
	@echo Remember to do test build from the snapshot archive.

P =
N =
diffa:
	./makehelper diffa $P $N

clean: ALWAYS
	rm -f *~
	cd src; make $(MAKECMDGOALS)

distclean: clean
	rm -f tools/tarlisted32
