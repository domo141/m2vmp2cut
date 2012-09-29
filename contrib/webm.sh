#!/bin/sh
# encode to vp8, vorbis and put into webm (matroska subset) container
# -*- mode: shell-script; sh-basic-offset: 8; tab-width: 8 -*-
# $ webm.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2012 Tomi Ollila
#	    All rights reserved
#
# Created: Wed 19 Sep 2012 17:24:05 EEST too
# Last modified: Sat 29 Sep 2012 15:06:11 EEST too

set -eu
#set -x

# LANG=C LC_ALL=C; export LANG LC_ALL
#PATH='/sbin:/usr/sbin:/bin:/usr/bin'; export PATH

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

m2vmp2cut <dir> contrib webm 16:9 vf='drawtext=fontfile=/usr/share/fonts/dejavu/DejaVuSans-BoldOblique.ttf:textfile=textfile:fontsize=24:fontcolor=royalblue:shadowx=3:shadowy=3:x=30:y=100:draw=lte(n\,100)'
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


m2vmp2cut_bindir=$M2VMP2CUT_CMD_DIRNAME/bin

denoisefilt='| yuvdenoise'
#deintfilter='| yuvdeinterlace -s1' # m2vtoyuv provides "full" frames...
deintfilter= # well, currently yadif used.

filters="$denoisefilt $deintfilter"

src=$M2VMP2CUT_MEDIA_DIRECTORY

case $vframes in  '')
#	vframes=-frames:v\ `$m2vmp2cut_bindir/getyuv.pl "$src" --framecount +1`
;; *)	vframes=-frames:v\ $vframes
esac

rmfifos () { rm -f fifo.video.$$ fifo.audio.$$; }
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
	$m2vmp2cut_bindir/getmp2.sh "$src" > fifo.audio.$$ &
	apid=$!
}
fifovideo ()
{
	eval "$m2vmp2cut_bindir/getyuv.pl '$src' $filters" > fifo.video.$$ &
	vpid=$!
}

# presets normally exist...
vopts="-aspect $aspect -vcodec libvpx -vpre libvpx-720p -b:v $vbs ${vf:+-vf $vf}"
vopts="$vopts $vframes -f webm"

# -q 2 	~96 	~96 - ~112 	point/lossless 	yes
aopts="-acodec libvorbis -q:a $aq ${af:+-af $af}"

echo; echo
mkfifos fifo.video.$$
fifovideo
x ffmpeg -i fifo.video.$$ -pass 1 $vopts -an -y "$of.wip"

echo; echo
mkfifos fifo.video.$$ fifo.audio.$$
fifoaudio
fifovideo
x ffmpeg -i fifo.video.$$ -i fifo.audio.$$ -pass 2 $vopts $aopts -y "$of.wip"

mv "$of".wip "$of"

echo "Result is in '$of'"
