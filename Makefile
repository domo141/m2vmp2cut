
# This program is licensed under the GPL v2. See file COPYING for details.

VERSION=0.79-dev

.PHONY:	ALWAYS

install chkprefix: VER=`sed -n 's/ver=//p' m2vmp2cut`
#release:  VER=`sed -n '1s/ .*/-dev/p' VERSION`
release: VER=$(VERSION)
snapshot: VER=$(VERSION)+`git log -1 --pretty=format:%ai \
	| tr -d :- | sed 's/ /-/; s/+.*//; s/ //g'`

all: ALWAYS HISTORY
	cd src && make all
	@echo; echo Build done.; echo

chkprefix: ALWAYS
	@case "$(PREFIX)" in \
		'') sed -n 's/^#msg1: \?//p;' Makefile; exit 1;; \
		*' '*) echo "Prefix '$(PREFIX)' has spaces!"; exit 1;; \
	esac

chkpjxjar: ALWAYS
	@case `env which projectx 2>/dev/null` in /*) exit 0 ;; esac; \
	 case "$(PJXJAR)" in \
		'') sed -n 's/^#msg2: \?//p;' Makefile; exit 1 ;; /*) ;; \
		*) echo "'$(PJXJAR)' does not start with '/'"; exit 1 ;; \
	 esac; \
	 test -f "$(PJXJAR)" || { echo "'$(PJXJAR)': no such file"; exit 1; }


TRG_BINS = m2vcut-gui m2vfilter m2vscan m2vtoyuv mp2cutpoints \
	fileparts filerotate wavgraph warpxpointer \
	m2vmp2cut.pl m2vmp2cut.sh m2v_catfiltered.py m2v_somehdrinfo.py \
	wrapper.sh lvev6frames.pl getyuv.pl getmp2.sh m2vmp2cut.pm

TRG_DOCS = Examples m2vcut_help-default m2vcut_help-fi_FI Options Usage

chkfiles: all
	@for i in $(TRG_BINS:%=bin/%) $(TRG_DOCS:%=doc/%); \
		do test -f "$$i" || { echo "'$$i' missing"; exit 1; }; done
	@test -f m2vmp2cut

install: chkfiles chkprefix chkpjxjar
	@rm -rf $(PREFIX)/lib/m2vmp2cut-$(VER)
	@chkdir() { [ -d "$$1" ] || mkdir -m 755 -p "$$1"; }; \
	chkdir $(PREFIX)/lib/m2vmp2cut-$(VER); chkdir $(PREFIX)/bin;  \
	chkdir $(PREFIX)/lib/m2vmp2cut-$(VER)/bin; \
	chkdir $(PREFIX)/lib/m2vmp2cut-$(VER)/doc; \
	chkdir $(PREFIX)/lib/m2vmp2cut-$(VER)/contrib
	cp $(TRG_BINS:%=bin/%) $(PREFIX)/lib/m2vmp2cut-$(VER)/bin
	cp $(TRG_DOCS:%=doc/%) $(PREFIX)/lib/m2vmp2cut-$(VER)/doc
	cp contrib/* $(PREFIX)/lib/m2vmp2cut-$(VER)/contrib
	cp m2vmp2cut $(PREFIX)/bin/m2vmp2cut
	@test x'$(PJXJAR)' = x || \
	  ln -s $(PJXJAR) $(PREFIX)/lib/m2vmp2cut-$(VER)/bin/ProjectX.jar
#	v=$(VER); sed "s/=devel/=$$v/" m2vmp2cut > $(PREFIX)/bin/m2vmp2cut
#	chmod 755 $(PREFIX)/bin/m2vmp2cut

HISTORY: ALWAYS #SvnVersion_unmodified #ALWAYS #$(FILES)
	( echo '# Created using "git log --name-status"' ;\
	  echo '# with logs of lost subversion repository' ;\
	  echo '# This file is not version controlled' ;\
	  git log -1 --pretty='tformat:%nHead date: %ai%n' ; \
	  git log --name-status ;\
	  echo -----------------------------------------------------------;\
	  cat HISTORY.crash ) > $@

ALL_IN:
	#git status | : awk ' \
	git  status | awk ' \
		/# Changes to be committed/ { print; exit 1 } \
		/# Changed but not updated/ { print; exit 1 } \
		/# Your branch is ahead of/ { print; exit 1 }'

chkrelease: ALL_IN HISTORY
	@case $(VER) in \
		*-dev) ;; *[2468]) ;; \
		*) echo Version $(VER) not suitable for release.; exit 1 ;; \
	esac

# XXX _dist depends on chkrelease. Parallel build would fail...
.NOTPARALLEL:

release: chkrelease _dist

snapshot: HISTORY _dist

tools/tarlisted22:
	cd tools && sh tarlisted22.c

_dist: tools/tarlisted22
	v=$(VER); sed "s/=devel/=$$v/" m2vmp2cut > m2vmp2cut.mod
	version=m2vmp2cut-$(VER); { echo 755 root root . $$version /; \
	grep '^#,#' Makefile | while read _ f x; do p=755; d=$$f; \
		test -d $$f && d=/ || { test -f $$d.mod && d=$$d.mod; \
					case $$x in '') p=644;; esac; }; \
		echo $$p root root . $$version/$$f $$d; done; } \
	| tools/tarlisted22 -Vz -o $$version.tar.gz
	rm m2vmp2cut.mod
	@echo Created m2vmp2cut-$(VER).tar.gz

# XXX unify
clean: ALWAYS
	rm -f *~
	cd src; make $@

distclean: ALWAYS
	rm -f *~
	rm -f HISTORY tools/tarlisted22
	cd src; make $@

# Embedded messages follow...

#msg1:
#msg1:  Can not install: PREFIX missing.
#msg1:
#msg1:  Try something like `make install PREFIX=/usr/local [PJXJAR=...]'
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

#,# bin
#,# bin/getmp2.sh x
#,# bin/getyuv.pl x
#,# bin/lvev6frames.pl x
#,# bin/m2v_catfiltered.py x
#,# bin/m2vmp2cut.pl x
#,# bin/m2vmp2cut.pm
#,# bin/m2vmp2cut.sh x
#,# bin/m2v_somehdrinfo.py x
#,# bin/wrapper.sh x

#,# contrib
#,# contrib/mpeg2.sh x

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
#,# tools/tarlisted22.c x

#EOF
