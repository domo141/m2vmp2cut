#!/bin/sh
# -*- mode: shell-script; sh-basic-offset: 8; tab-width: 8 -*-
# encode to vp8 video and vorbis audio into webm container
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2012 Tomi Ollila
#	    All rights reserved
#
# Created: Wed 19 Sep 2012 17:24:05 EEST too
# Last modified: Sat 14 Feb 2015 12:09:11 +0200 too

set -eu
#set -x

# LANG=C LC_ALL=C; export LANG LC_ALL
#PATH='/sbin:/usr/sbin:/bin:/usr/bin'; export PATH

saved_IFS=$IFS
readonly saved_IFS

warn () { echo "$@" >&2; }
die () { echo "$@" >&2; exit 1; }

x () { ( set -x; exec "$@" ); }

_cmds='ffmpeg'
__cmds=`env which $_cmds 2>/dev/null` || :
__miss=
for c in $_cmds
do	case $__cmds in */$c?/*|*/$c) ;; *) __miss=${__miss:+$__miss }$c; esac
done
case $__miss in '') ;; *)
	warn Install the following missing tools: $__miss
	die Then try again
esac
unset _cmds __cmds __miss

# Heuristic for checking shell probably implements posix features.
# Not tested whether these work with more traditional (bourne) shell.
case ~ in
'~')	getval () { val=`echo "$1" | sed 's/^[^=]*=//'`; }
;;
*)	getval () { val=${1#*=}; }
esac

test -f "$M2VMP2CUT_MEDIA_DIRECTORY"/video.m2v ||
	die "'$M2VMP2CUT_MEDIA_DIRECTORY/video.m2v' missing"

aspect= vframes= vbs=1042k aq=2 vf= af=
of=out.webm
while case ${1-} in
	4:3 | 16:9) aspect=$1 ;;
	vframes=*) getval "$1"; vframes=$val ;;
	vbs=*) getval "$1"; vbs=$val ;;
	aq=*) getval "$1"; aq=$val ;;
	vf=*) getval "$1"; vf=$val ;;
	af=*) getval "$1"; af=$val ;;
	of=*) getval "$1"; of=$val ;;
	'') break ;;
	*) die "$0: unknown option -- '$1'"
 esac
do shift; done

case $vf in example) echo "
Overlay text from 'textfile' to video, over 100 first frames.

m2vmp2cut <dir> x webm 16:9 vf='drawtext=fontfile=/usr/share/fonts/dejavu/DejaVuSans-BoldOblique.ttf:textfile=textfile:fontsize=24:fontcolor=royalblue:shadowx=3:shadowy=3:x=30:y=100:draw=lte(n\,100)'
"
	exit
esac

case $aspect in '') echo "
 Usage $0 (4:3|16:9) [vframes=<num>] [vbs=<val>] [aq=<num>] [vf=<vfgraph>] [af=<afgraph>] [of=<filename>]

 Encode to webm.

 Aspect option (4:3 or 16:9) is mandatory, others optional.

 Settings:
	vframes='$vframes'
	vbs='$vbs'
	aq='$aq'
	vf='$vf'
	af='$af'
	of='$of'

 Audio quality 2 gives 96-112 bps vorbis; higher more.

 vframes: encode up to <num> frames. Useful for testing.
 vbs: video bitrate (2 pass video encoding)
 vf adds video filter; for example 'vf=yadif' adds deinterlace filter.
 af adds audio filter...

 Note: currently af & vf values may not contain spaces.

 Somewhat complex vf example can be seen by command line option 'vf=example'
"
	exit 0
esac

case $of in *.webm) ;; *) die "Filename '$of' does not end with '.webm'."; esac

if ls "$of" >/dev/null 2>&1
then	die "Output file '$of' exists."
fi

if ls "$of.wip" >/dev/null 2>&1
then	die "Work in progress file '$of.wip' exists."
fi

printtimediff ()
{
	st=$1 et=$2; shift 2
	awk -v s=$st -v e=$et 'BEGIN {
		t = e - s; h = int(t / 3600); m = int(t / 60) % 60; s = t % 60
		printf "%s", "'"$*"' "; if (h > 0) printf "%d hours ", h
		if (m > 0) printf "%d minutes ", m; print s, "seconds"
	}'

}

m2vmp2cut_bindir=$M2VMP2CUT_CMD_PATH
m2vmp2cut_cntdir=$M2VMP2CUT_X_PATH

denoisefilt='| yuvdenoise'
#deintfilter='| yuvdeinterlace -s1' # m2vtoyuv provides "full" frames...
deintfilter= # well, currently yadif used.

filters="$denoisefilt $deintfilter"

src=$M2VMP2CUT_MEDIA_DIRECTORY

case $vframes in  '')
#	vframes=-frames:v\ `$m2vmp2cut_bindir/getyuv.pl "$src" --framecount +1`
;; *)	vframes=-frames:v\ $vframes
esac

RUNTIME_DIR=
case ${XDG_RUNTIME_DIR-} in '' | *["$IFS"]*) ;; *)
	if test -d $XDG_RUNTIME_DIR -a test -x $XDG_RUNTIME_DIR
	then RUNTIME_DIR=$XDG_RUNTIME_DIR
	fi
esac
case ${RUNTIME_DIR} in '')
	uid () { uid=$2; }
	IFS="=($IFS"
	uid `/usr/bin/id`
	IFS=$_saved_IFS
	RUNTIME_DIR=/tmp/runtime-$uid
	mkdir $RUNTIME_DIR 2>/dev/null || :
	chmod 700 $RUNTIME_DIR
esac

fifo_video=$RUNTIME_DIR/fifo.video.$$
fifo_audio=$RUNTIME_DIR/fifo.audio.$$

rmfifos () { rm -f $fifo_video $fifo_audio; }
exitrap () {
	ev=$?
	set +e
	rmfifos
	rm -f "$of.wip"
	case ${apid-}${vpid-} in '') ;; *) kill ${apid-} ${vpid-} 2>/dev/null
	esac
	exit $ev
}
trap exitrap 0
mkfifos ()
{
	rmfifos
	mkfifo "$@"
}
fifoaudio ()
{
	$m2vmp2cut_bindir/getmp2.sh "$src" > $fifo_audio &
	apid=$!
}
fifovideo ()
{
	eval "$m2vmp2cut_cntdir/ffgetyuv.pl $filters" > $fifo_video &
	vpid=$!
}

# presets normally exist...
vopts="-aspect $aspect -vcodec libvpx -vpre libvpx-720p -b:v $vbs ${vf:+-vf $vf}"
vopts="$vopts $vframes -f webm"

# -q 2 	~96 	~96 - ~112 	point/lossless 	yes
aopts="-acodec libvorbis -q:a $aq ${af:+-af $af}"

eval `date "+sdate='%c' ss=%s"`
echo
echo '***' Starting pass1 at $sdate
echo
mkfifos $fifo_video
fifovideo
x ffmpeg -i $fifo_video -pass 1 $vopts -an -y "$of.wip"

eval `date "+date='%c' ms=%s"`
echo
echo '***' Starting pass2 at $date
echo
mkfifos $fifo_video $fifo_audio
fifoaudio
fifovideo
x ffmpeg -i $fifo_video -i $fifo_audio -pass 2 $vopts $aopts -y "$of.wip"

eval `date "+date='%c' es=%s"`
echo
echo Started at $sdate
printtimediff $ss $ms Pass 1 execution time:
printtimediff $ms $es Pass 2 execution time:
printtimediff $ss $es Total execution time:' '
echo Completed at $date
echo
mv "$of".wip "$of"
ls -o "$of"
echo
