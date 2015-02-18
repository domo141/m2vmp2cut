
# This program is licensed under the GPL v2. See file COPYING for details.

# XXX mixed code from old & new system. to be fixed later (maybe)

MSRCS=	m2vscan.c m2vfilter.c m2vstream.c fileparts.c mp2cutpoints.c \
	eteen.c zzob.c filerotate.c bufwrite.c x.c

SRCS=	$(MSRCS) #embeds.c

HDRS=	zzob.h bufwrite.h x.h

TOCCODE= m2vmp2cut.pl lvev6frames.pl \
	mpg_somehdrinfo.py  mpg_catfiltered.py
#	mpg_somehdrinfo.pyc mpg_catfiltered.pyc

EGHDRS=$(TOCCODE:%=$(GD)/%.h)

BINS=	m2vscan m2vfilter m2vstream fileparts filerotate merge-gui eteen \
	mp2cutpoints assel-gui m2vcut-gui m2vtoyuv textdisp pgssupout

BSTMP=	../_build/.stamp
CD=	../_build/config
BD=	../_build/bin
OD=	../_build/obj_b
MD=	../_build/obj_m
GD=	../_build/ghdrs

BINARIES= $(BINS:%=$(BD)/%)

PRELOADS= $(BD)/libpreload_ffm2vtoyuv4mpeghax.so

all:	$(BINARIES) $(PRELOADS)

#obj_b/embeds.o: $(EGHDRS)

TXTS= Usage Options Examples

obj_b/showdoc.o obj_m/showdoc.o: $(TXTS:%=$(GD)/%.txtgz.h)

# To get less warnings (some probably unnecessary), enter `make WARN1=   ...'

WARN0=	-Wall -Wstrict-prototypes -pedantic -Wno-long-long \
	-Wcast-align -Wpointer-arith #-Wfloat-equal #-Werror
WARN1?=	-W -Wwrite-strings -Wcast-qual -Wshadow #-Wconversion

WOPTS=	$(WARN0) $(WARN1)

PGHDRS=	$(SRCS:%.c=$(GD)/%_priv.h)
GHDRS=	$(PGHDRS) $(EGHDRS) $(GD)/version.h

MOBJS=	$(MSRCS:%.c=$(OM)/%.o)
OBJS=	$(SRCS:%.c=$(OD)/%.o)

# Any of the headers change, let's compile all objs, for sure.
$(MOBJS) $(OBJS):  $(BSTMP) $(HDRS) $(GD)/version.h Makefile

$(BSTMP):
	mkdir -p ../_build $(BD) $(OD) $(GD)
	touch $@

$(GD)/version.h: Makefile
	sed 's/^/const unsigned char version[] = "/; s/$$/";/; q' ../NEWS > $@

# Touching file to required directories which modification time does
# not change unlike the directory itself.
#$(MOBJS): obj_m/.dx
#$(OBJS): $(OD)/.dx
#$(GHDRS): $(GD)/.dx

#obj_m/.dx $(OD)/.dx $(GD)/.dx:
#	d=`dirname $@`; test -d $$d || mkdir $$d
#	touch $@

CC=gcc

LF_OPTS= -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

CFLAGS=	-std=c99 $(OPTFLAGS) $(LF_OPTS) $(WOPTS) $(INCLS)

OPTFLAGS=-s -O3

#$(BD)/m2vmp2cut.bin: $(OBJS)
#	$(CC) -o $@ $(OBJS)

$(BD)/m2vscan: $(OBJS)
	$(CC) -o $@ $(OD)/m2vscan.o $(OD)/zzob.o $(OD)/x.o

$(BD)/m2vfilter: $(OBJS)
	$(CC) -o $@ $(OD)/m2vfilter.o $(OD)/zzob.o $(OD)/x.o $(OD)/bufwrite.o

$(BD)/m2vstream: $(OBJS)
	$(CC) -o $@ $(OD)/m2vstream.o $(OD)/zzob.o $(OD)/x.o $(OD)/bufwrite.o

$(BD)/mp2cutpoints: $(OBJS)
	$(CC) -o $@ $(OD)/mp2cutpoints.o $(OD)/x.o -lm

$(BD)/fileparts: $(OBJS)
	$(CC) -o $@ $(OD)/fileparts.o $(OD)/zzob.o $(OD)/x.o

$(BD)/filerotate: $(OBJS)
	$(CC) -o $@ $(OD)/filerotate.o $(OD)/x.o

$(BD)/assel-gui: assel-gui.c
	sh $< -o $@ $(OPTFLAGS)

$(BD)/merge-gui: merge-gui.c
	sh $< -o $@ $(OPTFLAGS)

$(BD)/m2vcut-gui: m2vcut-gui.c $(CD)/mpeg2.conf
	sh $< -o $@ $(OPTFLAGS)

#$(BD)/wavgraph: wavgraph.c $(OD)/x.o
#	sh wavgraph.c && mv wavgraph $(BD)

$(BD)/m2vtoyuv: m2vtoyuv.c $(CD)/mpeg2.conf
	sh $< -o $@ $(OPTFLAGS)

#$(BD)/warpxpointer: warpxpointer.c
#	sh warpxpointer.c && mv warpxpointer $(BD)

$(BD)/textdisp: textdisp.c
	sh $< -o $@ $(OPTFLAGS)

$(BD)/pgssupout: pgssupout.c
	sh $< -o $@ $(OPTFLAGS)

$(BD)/eteen: eteen.c
	sh $< -o $@ $(OPTFLAGS)

$(BD)/libpreload_ffm2vtoyuv4mpeghax.so: libpreload_ffm2vtoyuv4mpeghax.c
	sh $< -o $@ $(OPTFLAGS)

$(CD)/mpeg2.conf: $(BSTMP) ../tools/chksyslibmpeg.sh ../tools/chklibmpeg-051.sh
	cd ../_build;../tools/chksyslibmpeg.sh || ../tools/chklibmpeg-051.sh ..

$(GD)/%_priv.h: %.c
	perl -x Makefile $< > $@

$(OD)/%.o: %.c $(GD)/%_priv.h
	$(CC) $(CFLAGS) -I../_build -c $< -o $@

#obj_m/%.o: %.c $(GD)/%_priv.h
#	$(CC) $(CFLAGS) -DMINI -c $< -o $@


#.py.pyc:  # probably SUFFIXES modification would be needed for this/these.
%.pyc:	%.py
	python -c 'import py_compile; py_compile.compile("$<");'

#
# Well, python (2.3.3) could not read byte-compiled program from stdin :(
#	-- so the rule below is useless
#%.pyc.h: ../%.pyc Makefile
#	@sed -n '/^file2code.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
#		| sh -es -- $@ $<

$(GD)/%.py.h: ../%.py Makefile
	@sed -n '/^file2code.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
		| sh -es -- $@ $<

$(GD)/%.pl.h: ../%.pl Makefile
	@sed -n '/^file2code.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
		| sh -es -- $@ $<

$(GD)/%.txtgz.h: ../%
	@sed -n '/^file2txtgz.sh:/,/^ *$$/ p' Makefile | tail -n +3 \
		| sh -es -- $@ $<

FILES=Makefile $(SRCS) $(HDRS)

filelist:
	@echo $(FILES:%=$(DIR)%)

clean:
	rm -rf *~ *.o ../_build/[!b]*  # not b(in/)

distclean: clean
	rm -rf ../_build/ libmpeg2-svn-trunk

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
