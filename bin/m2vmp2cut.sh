#!/bin/sh
# $Id; m2vmp2cut.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Wed Apr 23 21:40:17 EEST 2008 too
# Last modified: Wed Jul 23 12:03:42 EEST 2008 too

e2 () { echo "$@" >&2; }
die () { e2 "$@"; exit 1; }
usage () { e2; e2 Usage: m2vmp2cut '(file|dir)' $cc "$@"; die; }
needvar () { [ x"$1" = x ] && { shift 1; "$@"; }; }

x () { echo + "$@"; "$@"; }

M2VMP2CUT_CMD_PATH=`cd \`dirname "$0"\`; pwd`
case $M2VMP2CUT_CMD_PATH in
    *' '*) die Too lazy to support Spaces in \'$M2VMP2CUT_CMD_PATH\' ;; esac
export M2VMP2CUT_CMD_PATH

#echo $M2VMP2CUT_CMD_PATH

# no interactive behaviour in batch mode...
case $1 in --batch) batch=1; shift ;; *) batch= ;; esac

case $1 in ''|h|he|hel|help|vermatch) ;;
    e|ex|exa|exam|examp|exampl|example) ;;
    lve|lvev|lvev6|lvev6f|lvev6fr|lvev6fra|lvev6fram|lvev6frame|lvev6frames) ;;
    *)	if test -f "$1"
	then
		dn=`dirname "$1"`; basename=`basename "$1"`
		od=`echo $basename | sed 's/\.[^.]*$//'`.d
		file=$1	dir=$dn/$od
	elif test -d "$1"
	then
		file= dir=$1
	else
		die "'$1': not a file or directory"
	fi
	shift ;;
esac

# do not show this in command list
cmd_vermatch ()
{
	case $1 in 5) exit 0;; *) exit 1 ;; esac
}

cmd_lvev6frames () # Legacy m2vmp2cut support; dig cutpoints from ~/.lve/* file
{
	$M2VMP2CUT_CMD_PATH/lvev6frames.pl
}

chkindexes ()
{
	test -f "$1/video.index" \
		|| $M2VMP2CUT_CMD_PATH/m2vscan "$1/video.m2v" "$1/video.index"
	test -f "$1/audio.scan" \
		|| $M2VMP2CUT_CMD_PATH/mp2cutpoints \
			--scan "$1/audio.mp2" "$1/audio.scan"
}

cmd_demux () # Demux mpeg2 file with ProjectX for further editing...
{
	case $file in '')
		die "No file to demux given" ;;
	esac
	test -h $M2VMP2CUT_CMD_PATH/ProjectX.jar || { \
	  e2 Symbolic link \'$M2VMP2CUT_CMD_PATH/ProjectX.jar\' does not exist
	  die Please provide link and try again
	}
	pjxjar=`LC_ALL=C ls -l $M2VMP2CUT_CMD_PATH/ProjectX.jar | sed 's/.* //'`
	test -f $pjxjar || {
	  e2 ProjectX jar file \'$pjxjar\' does not exist
	  die Fix this or it\'s symbolic link reference \'$M2VMP2CUT_CMD_PATH/ProjectX.jar\'
	}
	test -d "$dir" && die "Directory '$dir' is on the way (demuxed already)?"
	mkdir "$dir"
	ln -s "../$basename" "$dir/$basename"
	x java -jar "$pjxjar" -ini /dev/null "$dir/$basename"
	cd "$dir"
	ln -s *.m2v video.m2v
	ln -s *.mp2 audio.mp2
	chkindexes .
}

cmd_select () # Select parts from video with a graphical tool
{
	test -f "$dir/video.m2v" || die "'$dir/video.m2v' does not exist"
	chkindexes "$dir"
	x $M2VMP2CUT_CMD_PATH/m2vcut-gui \
		"$dir/video.index" "$dir/video.m2v" "$dir/cutpoints"
	test -f "$dir/cutpoints.1" && { case `wc "$dir/cutpoints"` in
		*' '1' '*) cat "$dir/cutpoints.1" >> "$dir/cutpoints";; esac; }
}

cmd_cut () # Cut using m2vmp2cut.pl for the work...
{
	x $M2VMP2CUT_CMD_PATH/m2vmp2cut.pl --dir="$dir" "$@"
}

cmd_play () # Play resulting file with mplayer
{
	f="$dir"/m2vmp2cut-work/out.mpg
	test -f "$f" || die "'$f' does not exist"
	x mplayer "$@" "$f"
}

cmd_move () # Move final file to a new location (and name)
{
	needvar "$1" usage '<destfile>'
	f="$dir"/m2vmp2cut-work/out.mpg
	test -f "$f" || die "'$f' does not exist"
	x mv "$f" "$1"
}

cmd_help () # Help of all or some of the commands above
{
	echo
	sed -n "s|^#h $1[^:]*:||p" "$0"
}

cmd_example () # simple example commands
{
	cut -d: -f 2- >&2 <<.
	:
	: Simple example commands. In select/cut/play '<dir>' can be used.
	: (<file> is for user convenience...)
	:
	: m2vmp2cut <file> demux
	: m2vmp2cut <file> select
	: m2vmp2cut <file> cut
	: m2vmp2cut <file> play
	:
	: In above, there was basic workflow. 'cut' gui provides a test
	: option -- but if you want to re-test, run these.
	:
	: m2vmp2cut <file> cut --test=200
	: m2vmp2cut <file> play
	:
.
}

# ---

[ x"$1" = x ] && {
        echo
        echo Usage: m2vmp2cut '[-batch] (file|dir) <command> [args]'
        echo
        echo m2vmp2cut commands available:
        echo
        sed -n '/^cmd_/ { s/cmd_/ /; s/ () [ -#]*/                            /
                          s/\(.\{15\}\) */\1/p; }' $0
        echo
        echo Commands may be abbreviated down to no ambiguity
        echo
        exit 0
}

cmd=$1; shift

#case $cmd in
#        c) cmd=colorset ;;
#esac


cc= cp=
for m in `LC_ALL=C sed -n 's/^cmd_\([a-z0-9_]*\) (.*/\1/p' $0`
do
        case $m in
                $cmd) cp= cc=$cmd cm=$cmd; break ;;
                $cmd*) cp=$cc; cc="$m $cc"; cm=$m ;;
        esac
done

[ x"$cc" = x ] && { echo $0: $cmd -- command not found.; exit 1; }

[ x"$cp" != x ] && { echo $0: $cmd -- ambiquous command: matches $cc; exit 1; }

cmd=$cm
cmd_$cm "$@"
exit $?

# fixme: move these to separate doc file (w/ locale extension)

#h lvev6frames: lvev6frames (no options)
#h lvev6frames:
#h lvev6frames: Old versions of m2vmp2cut supported using lve-generated
#h lvev6frames: "edit lists" for cutpoint information. With lvev6frames
#h lvev6frames: these old edits can be used with this m2vmp2cut version
#h lvev6frames:

#h demux: <filename> demux
#h demux:
#h demux: m2vmp2cut reguires mpeg files to be demuxed to elementary streams
#h demux: before cutting. This command uses ProjectX to do the demuxing.
#h demux: A separate directory (based on source filename) is created for
#h demux: output files. Note that this doubles the disk usage of a particular
#h demux: source file.
#h demux:

#h select: <directory> select
#h select:
#h select: This command uses new m2vcut-gui graphical utility for searching
#h select: cutpoints. This work is done frame-accurately.
#h select:

#h cut: <directory> cut [options] ...
#h cut:
#h cut: This command is wrapper to m2vmp2cut.pl (which used to be the frontend
#h cut: of m2vmp2cut in old versions). This command has extensive help of
#h cut: it's own. Note that this (again) adds one third of disk usage so far
#h cut: when this creates final output file.
#h cut:

#h play: <directory> play [options]
#h play:
#h play: This command runs mplayer for the file created with cut command
#h play:
