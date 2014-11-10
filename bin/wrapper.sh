#!/bin/sh
# $Id; wrapper.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Tue Apr 22 19:10:35 EEST 2008 too
# Last modified: Sun 04 May 2014 19:00:44 +0300 too

set -eu

die () { exec 1>&2; echo; echo "$@";
	 echo; echo Press Ctrl-C to close this window ''; sleep 60; exit 1;
}

numtune () {
	echo $1 | sed 's/\(...\)$/ \1/; s/\(...\) / \1 /; s/\(...\) / \1 /'
}

exec_textdisp () {
	exec "`dirname "$0"`"/textdisp -title "$@"
}

case ${1-} in
    m2vcut_help)
	f=`echo $LANG |sed 's/[^A-Za-z_].*//'`
	pd=`cd \`dirname "$0"\`/..; pwd`
	helprefix="$pd/doc/m2vcut_help"
	test -f "$helprefix-$f" && f="$helprefix-$f" || f="$helprefix-default"
	exec_textdisp 'M2vCut Help' -hold cat "$f"
	;;
    m2vcut_info)
	shift
	exec_textdisp 'M2vCut Info' -hold "$0" m2vcut_runinfo "$@"
	;;
    m2vcut_runinfo)
	#echo "$@" - $5
	case ${5-} in [0-9]*) ;;
		*) die usage: $1 videofile allframes cutframes alltime cuttime
	esac
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
	case $vsize in '') die File "'$1'" could not be read. ;; esac
	asize=`stat -L -c %s "$fdn/$abn"`
	case $asize in '') die File "'$1'" could not be read. ;; esac
	echo
	echo Input length: '' `echo $4 | sed 's/\..*//'` "($2 frames)".
	echo Output length: `echo $5 | sed 's/\..*//'` "($3 frames)".
	echo
	echo Input video: $vbn '' `numtune $vsize` bytes.
	echo Input audio: $abn '' `numtune $asize` bytes.
	echo
	tss=`expr $vsize + $asize`
	echo Total source size: `numtune $tss` bytes.
	eds=`expr $tss \* $3 / $2`
	echo Estimated target size: `numtune $eds` bytes.
	wmo=`expr $eds \* 1033 / 1000`
	echo With estimated muxing '"overhead" (3.3%)': `numtune $wmo` bytes.
	#echo $fdn $vbn $abn $vsize $asize
	;;
    m2vcut_test)
	shift
	exec_textdisp 'M2vCut Test' "$0" m2vcut_runtest "$@"
	;;
    m2vcut_runtest)
	vd=`exec dirname "$5"`
	test -f "$vd"/m2vcut-test/$2-$3 -a -f "$vd"/m2vcut-test/out.mpg || {
	  rm -rf "$vd"/m2vcut-test
	  to=--test=0
	  case $2 in '') range= rx= to= ;;
	   *) rs=`exec expr $2 - 200` || :; test $rs -ge 0 || rs=0; range=$rs-$2 rx=,
	  esac
	  case $3 in '') to= ;; *) range="$range$rx$3-`expr $3 + 200`" ;; esac

	  cwd=`cd "\`exec dirname "$0"\`"/; pwd`
	  "$cwd"/m2vmp2cut.pl $to "$vd" -r $range -v "$5" -d m2vcut-test
	  touch "$vd"/m2vcut-test/$2-$3
	}
	sleep 3 &
	set -x
	if	test -f "$vd"/m2vcut-test/out.mpg
	then	mplayer "$vd"/m2vcut-test/out.mpg
	fi
	wait
	;;
    die)
	exec zenity --error --title "Fatal Error!" --text "$2"
	;;
    '') die Usage: $0 command '[args]'
	;;
    *) die "'$1'": unknown command.
esac
