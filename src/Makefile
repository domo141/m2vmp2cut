
# This program is licensed under the GPL v2. See file COPYING for details.

# XXX mixed code from old & new system. to be fixed later (maybe)

MSRCS=	m2vscan.c m2vfilter.c fileparts.c mp2cutpoints.c \
	zzob.c filerotate.c bufwrite.c x.c

SRCS=	$(MSRCS) #embeds.c

HDRS=	zzob.h bufwrite.h x.h

TOCCODE= m2vmp2cut.pl lvev6frames.pl \
	mpg_somehdrinfo.py  mpg_catfiltered.py
#	mpg_somehdrinfo.pyc mpg_catfiltered.pyc

EGHDRS=$(TOCCODE:%=ghdrs/%.h)

BINARIES= ../bin/m2vscan ../bin/m2vfilter ../bin/fileparts ../bin/filerotate \
	../bin/mp2cutpoints ../bin/m2vcut-gui ../bin/m2vtoyuv ../bin/textdisp

PRELOADS= ../bin/libpreload_ffm2vtoyuv4mpeghax.so

all:	$(BINARIES) $(PRELOADS)

#obj_b/embeds.o: $(EGHDRS)

TXTS= Usage Options Examples

obj_b/showdoc.o obj_m/showdoc.o: $(TXTS:%=ghdrs/%.txtgz.h)

# To get less warnings (some probably unnecessary), enter `make WARN1=   ...'

WARN0=	-Wall -Wstrict-prototypes -pedantic -Wno-long-long \
	-Wcast-align -Wpointer-arith #-Wfloat-equal #-Werror
WARN1?= -W -Wwrite-strings -Wcast-qual -Wshadow #-Wconversion

WOPTS=	$(WARN0) $(WARN1)

PGHDRS= $(SRCS:%.c=ghdrs/%_priv.h)
GHDRS=	$(PGHDRS) $(EGHDRS) ghdrs/version.h

MOBJS= $(MSRCS:%.c=obj_m/%.o)
OBJS= $(SRCS:%.c=obj_b/%.o)

# Any of the headers change, let's compile all objs, for sure.
$(MOBJS) $(OBJS):  $(HDRS) ghdrs/version.h Makefile

ghdrs/version.h: Makefile
	sed 's/^/const unsigned char version[] = "/; s/$$/";/; q' ../NEWS > $@

# Touching file to required directories which modification time does
# not change unlike the directory itself.
$(MOBJS): obj_m/.dx
$(OBJS): obj_b/.dx
$(GHDRS): ghdrs/.dx

obj_m/.dx obj_b/.dx ghdrs/.dx:
	d=`dirname $@`; test -d $$d || mkdir $$d
	touch $@

CC=gcc

LF_OPTS= -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

#CFLAGS= -ggdb $(LF_OPTS) $(WOPTS) $(INCLS)
#CFLAGS=	-s -O3 $(LF_OPTS) $(WOPTS) $(INCLS)
CFLAGS=	-std=c99 $(OPTFLAGS) $(LF_OPTS) $(WOPTS) $(INCLS)

OPTFLAGS=-s -O3

../bin/m2vmp2cut.bin: $(OBJS)
	$(CC) -o $@ $(OBJS)

../bin/m2vscan: $(OBJS)
	$(CC) -o $@ obj_b/m2vscan.o obj_b/zzob.o obj_b/x.o

../bin/m2vfilter: $(OBJS)
	$(CC) -o $@ obj_b/m2vfilter.o obj_b/zzob.o obj_b/x.o obj_b/bufwrite.o

../bin/mp2cutpoints: $(OBJS)
	$(CC) -o $@ obj_b/mp2cutpoints.o obj_b/x.o -lm

../bin/fileparts: $(OBJS)
	$(CC) -o $@ obj_b/fileparts.o obj_b/zzob.o obj_b/x.o

../bin/filerotate: $(OBJS)
	$(CC) -o $@ obj_b/filerotate.o obj_b/x.o

../bin/m2vcut-gui: m2vcut-gui.c config/mpeg2.conf
	sh m2vcut-gui.c $(OPTFLAGS)
	mv m2vcut-gui ../bin
#	$(CC) $(CFLAGS) -c $< -o $@

#../bin/wavgraph: wavgraph.c obj_b/x.o
#	sh wavgraph.c && mv wavgraph ../bin

../bin/m2vtoyuv: m2vtoyuv.c config/mpeg2.conf
	sh m2vtoyuv.c $(OPTFLAGS)
	mv m2vtoyuv ../bin

#../bin/warpxpointer: warpxpointer.c
#	sh warpxpointer.c && mv warpxpointer ../bin

../bin/textdisp: textdisp.c
	sh textdisp.c $(OPTFLAGS)
	mv textdisp ../bin

../bin/libpreload_ffm2vtoyuv4mpeghax.so: libpreload_ffm2vtoyuv4mpeghax.c
	sh $< $(OPTFLAGS)
	chmod 644 libpreload_ffm2vtoyuv4mpeghax.so
	mv libpreload_ffm2vtoyuv4mpeghax.so ../bin

config/mpeg2.conf: ../tools/chksyslibmpeg.sh ../tools/chklibmpeg-051.sh
	../tools/chksyslibmpeg.sh || ../tools/chklibmpeg-051.sh ..

ghdrs/%_priv.h: %.c
	perl -x Makefile $< > $@

obj_b/%.o: %.c ghdrs/%_priv.h
	$(CC) $(CFLAGS) -c $< -o $@

obj_m/%.o: %.c ghdrs/%_priv.h
	$(CC) $(CFLAGS) -DMINI -c $< -o $@


#.py.pyc:  # probably SUFFIXES modification would be needed for this/these.
%.pyc:	%.py
	python -c 'import py_compile; py_compile.compile("$<");'

#
# Well, python (2.3.3) could not read byte-compiled program from stdin :(
#	-- so the rule below is useless
#%.pyc.h: ../%.pyc Makefile
#	@sed -n '/^file2code.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
#		| sh -es -- $@ $<

ghdrs/%.py.h: ../%.py Makefile
	@sed -n '/^file2code.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
		| sh -es -- $@ $<

ghdrs/%.pl.h: ../%.pl Makefile
	@sed -n '/^file2code.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
		| sh -es -- $@ $<

ghdrs/%.txtgz.h: ../%
	@sed -n '/^file2txtgz.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
		| sh -es -- $@ $<

FILES=Makefile $(SRCS) $(HDRS)

filelist:
	@echo $(FILES:%=$(DIR)%)

clean:
	rm -rf *~ *.o ghdrs obj_m obj_b config

distclean: clean
	rm -f $(BINARIES) $(PRELOADS) libmpeg2-svn-trunk

# Embedded scripts follow...

file2code.sh:
	exit 1 # this target is not to be run.
	bn=`basename $2`
	echo Creating $1 from $2
	(
	  echo const unsigned char ${bn}_code'[] = {' | tr . _
	  sed -n -e '/^#s# SkipWhenEmbedded/,/^#s# End Skip/ d' \
		-e 's/#c# //' -e p $2 | od -v -t x1 \
		| sed -n 's/.......//; s/ \([0-9a-f][0-9a-f]\)/0x\1,/gp'
	  echo '};'
	) > $1
	exit 0

file2txtgz.sh:
	exit 1 # this target is not to be run.
	bn=`basename $2`
	echo Creating $1 from $2
	(
	  echo const unsigned char ${bn}_code'[] = {' | tr . _
	  sed -n -e '/^#s# SkipWhenEmbedded/,/^#s# End Skip/ d' \
		-e 's/#c# //' -e p $2 | gzip -c -9 | od -v -t x1 \
		| sed -n 's/.......//; s/ \([0-9a-f][0-9a-f]\)/0x\1,/gp'
	  echo '};'
	) > $1
	exit 0


# Do not add empty lines in embedded perl program.
perlcode:
	: No-one is supposed to run this target. :
	@exit 1
#! perl
	print "/*\n * Autogenerated by Makefile perlcode -- do not edit.\n */";
	while ($_ = shift @ARGV) {
	  open(I, "$_") || die "Can not open input file $_: $!\n";
	  print "\n\n/*  $_  */\n";
	  $state = 0; while (<I>) {
	  print ("\n$1") if /(.*?)\s*\/\*.*\sprotoline\s.*\*\//;
	  $state = 1 if /^static/;
	  if ($state) {
	    undef @l, $state = 0, next if /;/;
	    chop;
	    $state = 2 if /\(/;
	    s|\s*\/\*\s+protoadd\s+(.*)\s+\*\/\s*|\t$1|;
	    s/\(\)/\(void\)/;
	    if (/{/) {	print (@l, ";") if ($state > 1);
			undef @l; $state = 0; next; }
	    push @l, "\n$_";
	  }}
	  close I;
	}
	print "\n\n";
	__END__;


#EOF
