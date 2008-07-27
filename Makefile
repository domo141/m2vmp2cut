
# This program is licensed under the GPL v2. See file COPYING for details.

.PHONY:	ALWAYS

install chkprefix: VER=`sed -n 's/ver=//p' m2vmp2cut`
#release:  VER=`sed -n '1s/ .*/-dev/p' VERSION`
release: VER=`sed -n '1s/ .*//p' VERSION`
snapshot: VER=`sed -n '1s/ .*//p' VERSION`+r`svnversion . | tr : =`

all: ALWAYS
	cd src && make all
	@echo; echo Build done.; echo

chkprefix: ALWAYS
	@case "$(PREFIX)" in \
		'') sed -n 's/^#msg1: \?//p;' Makefile; exit 1;; \
		*' '*) echo "Prefix '$(PREFIX)' has spaces!"; exit 1;; \
	esac

chkpjxjar: ALWAYS
	@case "$(PJXJAR)" in \
		'') sed -n 's/^#msg2: \?//p;' Makefile; exit 1 ;; /*) ;; \
		*) echo "'$(PJXJAR)' does not start with '/'"; exit 1 ;; \
	esac
	@test -f "$(PJXJAR)" || { echo "'$(PJXJAR)': no such file"; exit 1; }


TRG_BINS = m2vcut-gui m2vfilter m2vscan m2vtoyuv mp2cutpoints \
	fileparts filerotate wavgraph warpxpointer \
	m2vmp2cut.pl m2vmp2cut.sh m2v_catfiltered.py m2v_somehdrinfo.py \
	wrapper.sh lvev6frames.pl

#TRG_DOCS = Examples m2vcut_help-default m2vcut_help-fi_FI Options Usage
TRG_DOCS = m2vcut_help-default m2vcut_help-fi_FI Options Usage

chkfiles:
	@for i in $(TRG_BINS:%=bin/%) $(TRG_DOCS:%=doc/%); \
		do test -f "$$i" || { echo "'$$i' missing"; exit 1; }; done
	@test -f m2vmp2cut

install: chkfiles chkprefix chkpjxjar
	@rm -rf $(PREFIX)/lib/m2vmp2cut-$(VER)
	@chkdir() { [ -d "$$1" ] || mkdir -m 755 -p "$$1"; }; \
	chkdir $(PREFIX)/lib/m2vmp2cut-$(VER); chkdir $(PREFIX)/bin;  \
	chkdir $(PREFIX)/lib/m2vmp2cut-$(VER)/bin; \
	chkdir $(PREFIX)/lib/m2vmp2cut-$(VER)/doc
	cp $(TRG_BINS:%=bin/%) $(PREFIX)/lib/m2vmp2cut-$(VER)/bin
	cp $(TRG_DOCS:%=doc/%) $(PREFIX)/lib/m2vmp2cut-$(VER)/doc
	cp m2vmp2cut $(PREFIX)/bin/m2vmp2cut
	ln -s $(PJXJAR) $(PREFIX)/lib/m2vmp2cut-$(VER)/bin/ProjectX.jar
#	v=$(VER); sed "s/=devel/=$$v/" m2vmp2cut > $(PREFIX)/bin/m2vmp2cut
#	chmod 755 $(PREFIX)/bin/m2vmp2cut

HISTORY: ALWAYS #SvnVersion_unmodified #ALWAYS #$(FILES)
	( echo "# Created using svn -v log" | tr '\012' ' ';\
	  echo "| sed -n -e '\$${x;p;x;p;q};/^--*\$$/{x;/./p;d};x;p'" ;\
	  echo '# This file is not version controlled' ;\
	  echo ;\
	  LC_ALL=C svn -v log -r HEAD:700 \
		| sed -n -e '$${x;p;x;p;q};/^--*$$/{x;/./p;d};x;p' ;\
	  cat HISTORY.crash ) > $@

SvnVersion_unmodified: SvnVersion
	@[ x"`sed 's/[0-9]*//' SvnVersion`" = x ] || \
	  {  echo Version number `cat SvnVersion` not single, unmodified.; \
	     exit 1; } # exit 0 when testing

SvnVersion: ALWAYS
	svnversion . > $@

chkrelease: SvnVersion_unmodified HISTORY
	rm -f SvnVersion
	@case $(VER) in \
		*-dev) ;; *[2468]) ;; \
		*) echo Version $(VER) not suitable for release.; exit 1 ;; \
	esac

# XXX _dist depends on chkrelease. Parallel build would fail...
.NOTPARALLEL:

release: chkrelease _dist

snapshot: SvnVersion HISTORY _dist

tools/tarlisted:
	cd tools && sh tarlisted.c

_dist: tools/tarlisted
	v=$(VER); sed "s/=devel/=$$v/" m2vmp2cut > m2vmp2cut.mod
	version=m2vmp2cut-$(VER); { echo 755 root root . $$version /; \
	grep '^#,#' Makefile | while read _ f x; do p=755; d=$$f; \
		test -d $$f && d=/ || { test -f $$d.mod && d=$$d.mod; \
					case $$x in '') p=644;; esac; }; \
		echo $$p root root . $$version/$$f $$d; done; } \
	| tools/tarlisted -Vz -o $$version.tar.gz
	rm m2vmp2cut.mod
	@echo Created m2vmp2cut-$(VER).tar.gz

# XXX unify
clean: ALWAYS
	rm -f *~
	cd src; make $@

distclean: ALWAYS 
	rm -f *~
	rm -f HISTORY SvnVersion tools/tarlisted
	cd src; make $@

# Embedded messages follow...

#msg1:
#msg1:  Can not install: PREFIX missing.
#msg1:
#msg1:  Try something like `make install PREFIX=/usr/local PJXJAR=...'
#msg1:

#msg2:
#msg2:  Can not install: PJXJAR missing.
#msg2:
#msg2:  Try `make install PREFIX=... PJXJAR=/path/to/ProjectX.jar'
#msg2:



# Dist files:

#,# ANNOUNCE
#,# COPYING
#,# HISTORY
#,# INSTALL
#,# m2vmp2cut x
#,# Makefile
#,# README
#,# TODO
#,# VERSION

#,# bin
#,# bin/lvev6frames.pl x
#,# bin/m2v_catfiltered.py x
#,# bin/m2vmp2cut.pl x
#,# bin/m2vmp2cut.sh x
#,# bin/m2v_somehdrinfo.py x
#,# bin/wrapper.sh x

#,# doc
#,# doc/Examples
#,# doc/m2vcut_help-default
#,# doc/m2vcut_help-fi_FI
#,# doc/Options
#,# doc/Usage

#,# src
#,# src/bufwrite.c
#,# src/bufwrite.h
#,# src/fileparts.c
#,# src/filerotate.c
#,# src/m2vcut-gui.c
#,# src/m2vfilter.c
#,# src/m2vscan.c
#,# src/m2vtoyuv.c
#,# src/Makefile
#,# src/mp2cutpoints.c
#,# src/warpxpointer.c
#,# src/wavgraph.c
#,# src/x.c
#,# src/x.h
#,# src/zzob.c
#,# src/zzob.h

#,# tools
#,# tools/buildlibmpeg-051.sh x
#,# tools/chksyslibmpeg.sh x
#,# tools/chklibmpeg-051.sh x
#,# tools/tarlisted.c x

#EOF
