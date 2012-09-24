#!/bin/sh
# $Id; wrapper.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Tue Apr 22 19:10:35 EEST 2008 too
# Last modified: Wed 15 Feb 2012 19:01:55 EET too

eae () { echo; echo Press ENTER to close this window '' | tr -d \\012
	read _; exit $1; }

die () { exec 1>&2; echo; echo "$@"; eae 1; }

#numtune () { echo $1 | sed 's/\(...\)\(...\)\(...\)$/ \1 \2 \3/;
#			s/\(...\)\(...\)$/ \1 \2/; s/\(...\)$/ \1/'; }

numtune () {
	echo $1 | sed 's/\(...\)$/ \1/; s/\(...\) / \1 /; s/\(...\) / \1 /'
}

#secstoday () { eval expr `date '+ 3600 \* %H + 60 \* %M + %S'`; }

# feh does not grok proper -geometry string. need to make warpxpointer find...
#warp_to_m2vcutgui () {
#    eval `xwininfo -all -name M2vCut \
#	| awk ' /Abs.*upp.*X:/ { x = $4 } /Width:/ { w = $2 }
#		/Abs.*upp.*Y:/ { y = $4 } /Height:/ { h = $2 }
#		END { print "x=" x + w / 2, "y=" y + h / 2 }'`
#
#    "`dirname $0`"/warpxpointer $x $y
#}

m2vcut_warp_exit ()
{
    "`dirname $0`"/warpxpointer -name M2vCut c c
    exit 0
}

exec_xterm () {
    geometry=$1
    title=$2
    #font=-misc-fixed-medium-r-normal--20-200-75-75-c-100-iso10646-1
    font=fixed
    shift 2
    exec xterm -fg black -bg gray70 -g $geometry -T "$title" \
	+sb -fn $font -u8 -e "$0" "$@"
}

case $1 in
    m2vcut_help)
	exec_xterm 76x35-0+0 'M2vCut Help' m2vcut_outputhelp
	;;
    m2vcut_outputhelp)
	f=`echo $LANG |sed 's/[^A-Za-z_].*//'`
	pd=`cd \`dirname "$0"\`/..; pwd`
	helpprefix="$pd/doc/m2vcut_help"
	test -f "$helpprefix-$f" && cat "$helpprefix-$f" || cat "$helpprefix-default"
	read line
	;;
    m2vcut_info)
	shift
	exec_xterm 76x20-0+0 'M2vCut Info' m2vcut_runinfo "$@"
	;;
    m2vcut_runinfo)
	#echo "$@" - $5
	case $5 in [0-9]*) ;;
		*) die usage: $1 videofile allframes cutframes alltime cuttime
	;; esac
	shift; #set -x
	test -f "$1" || die "'$1'": not a file.
	fdn=`dirname "$1"` vbn=`basename "$1"`
	case $vbn in
		video.m2v) abn=audio.mp2 ;;
		*.m2v) abn=`basename "$vbn" .m2v`.mp2 ;;
		*) die File "'$vbn'" has unknown suffix. ;;
	esac
	test -f "$fdn/$abn" || die "'$fdn/$abn'": no such file.
	vsize=`stat -L -c %s "$1"`
	case $vsize in '') die File "'$1'" could not be read ;; esac
	asize=`stat -L -c %s "$fdn/$abn"`
	case $asize in '') die File "'$1'" could not be read ;; esac
	echo
	echo Input length: '' `echo $4 | sed 's/\..*//'` "($2 frames)"
	echo Output length: `echo $5 | sed 's/\..*//'` "($3 frames)"
	echo
	echo Input video: $vbn '' `numtune $vsize` bytes
	echo Input audio: $abn '' `numtune $asize` bytes
	echo
	tss=`expr $vsize + $asize`
	echo Total source size: `numtune $tss` bytes
	eds=`expr $tss \* $3 / $2`
	echo Estimated target size: `numtune $eds` bytes
	wmo=`expr $eds \* 1033 / 1000`
	echo With estimated muxing '"overhead" (3.3%)': `numtune $wmo` bytes
	#echo $fdn $vbn $abn $vsize $asize
	eae 0
	;;
    m2vcut_test)
	shift
	exec_xterm 76x28-0+0 'M2vCut Test' m2vcut_runtest "$@"
	;;
    m2vcut_runtest)
	vd=`dirname "$5"`
	test -f "$vd"/m2vcut-test/$2-$3 -a -f "$vd"/m2vcut-test/out.mpg || {
	  rm -rf "$vd"/m2vcut-test
	  to=--test=0
	  case $2 in '') range= rx= to= ;;
	  *) rs=`expr $2 - 200`; test $rs -lt 0 && rs=0; range=$rs-$2 rx=, ;; esac
	  case $3 in '') to= ;; *) range="$range$rx$3-`expr $3 + 200`" ;; esac

	  cwd=`cd \`dirname "$0"\`/; pwd`
	  "$cwd"/m2vmp2cut.pl $to "$vd" -r $range -v "$5" -d m2vcut-test
	  touch "$vd"/m2vcut-test/$2-$3
	}
	sleep 3 &
	set -x
	test -f "$vd"/m2vcut-test/out.mpg && mplayer "$vd"/m2vcut-test/out.mpg
	wait
	;;
    m2vcut_agraph)
	shift
	exec_xterm 76x28-0+0 'M2vCut Test' m2vcut_runagraph "$@"
	;;
    m2vcut_runagraph)
	shift
	vd=`dirname "$1"`
	eval `xdpyinfo | awk '/dimensions:/ { sub("x", " h=", $2); print "w=" $2; exit }'`
	case $w in '') die Can not resolve x display width  ;; esac
	case $h in '') die Can not resolve x display height ;; esac
	test $w -ge 400 || die X display width too small
	test $h -ge 300 || die X display height too small
	w=`expr $w - 48`; h=100
	ms2timecode () { awk 'BEGIN { ms='"$1"'; printf "%02d:%02d:%02d.%03d",
				int(ms / 1000 / 60 / 60),
				int(ms / 1000 / 60) % 60,
				int(ms / 1000) % 60, (ms % 1000) }'; }
	ms=`expr $2 \* 40`; l=`expr $3 \* 40`
	s=`expr $ms - 1000`; test $s -lt 0 && s=0
	e=`expr $ms + 1000`; test $e -gt $l && e=$l
	ss=`expr $ms - $s`; ee=`expr $e - $s`
	ss=`expr $ss \* $w`; m=`expr $ss / $ee`
	#
	ss=`ms2timecode $s`; ee=`ms2timecode $e`
	#echo $s $e $ss, $ee
	cwd=`cd \`dirname "$0"\`/; pwd`
	byterange=`"$cwd"/mp2cutpoints "$ss-$ee" "$vd"/audio.mp2 /dev/null "$vd"/audio.scan`
	dd="$vd"/m2vcut-test
	test -d "$dd" && rm -rf "$dd"
	mkdir "$dd" || die Can not create directory "$dd"
	set -x
	"$cwd"/fileparts $byterange "$vd"/audio.mp2 > "$dd"/audio.mp2
	mplayer -vo null -vc null -af format=s32le -ao pcm:file="$dd"/audio.wav "$dd"/audio.mp2
	"$cwd"/wavgraph "$dd"/audio.wav "$dd"/audio.xpm $w $h $m 25 1
	test -f "$dd"/audio.xpm \
		|| { set +x; die Creating graph failed. See above.; }
	case `env which feh 2>/dev/null` in /*)
		:; : Press 'q' to close graph window ;:
		"$cwd"/warpxpointer -trysecs 4 -name feh c c &
		feh "$dd"/audio.xpm ; wait; m2vcut_warp_exit ;; esac
	case `env which display 2>/dev/null` in /*)
		:; : Press 'q' to close graph window ;:
		"$cwd"/warpxpointer -trysecs 4 -name ImageMagick c c &
		display "$dd"/audio.xpm ; wait; m2vcut_warp_exit ;; esac
	set +x
	die "Can not find image viewer. Install 'feh' or 'display' (or tune this script)".
	;;
    die)
	exec zenity --error --title "Fatal Error!" --text "$2"
	;;
    '') die Usage: $0 command '[args]'
	;;
    *) die "'$1'": unknown command.
	;;
esac
