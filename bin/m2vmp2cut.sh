#!/bin/sh
# $Id; m2vmp2cut.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Wed Apr 23 21:40:17 EEST 2008 too
# Last modified: Mon 05 Mar 2012 14:50:52 EET too

set -eu
case ${1-} in -x) set -x; shift; esac # debug help, passed thru wrapper.

warn () { echo "$@" >&2; }
die () { warn "$@"; warn; exit 1; }
needvar () { case ${1-} in '') shift 1; "$@"; esac; }

usage () {
	warn; die Usage: m2vmp2cut '(file|directory)' $cmd "$@"
}

x () { echo + "$@"; "$@"; }

M2VMP2CUT_CMD_PATH=`cd \`dirname "$0"\`; pwd`
case $M2VMP2CUT_CMD_PATH in
    *' '*) die "Too lazy to support Spaces in '$M2VMP2CUT_CMD_PATH'."; esac
export M2VMP2CUT_CMD_PATH

#echo $M2VMP2CUT_CMD_PATH

# do not show this in command list
cmd_vermatch ()
{
	case $1 in 6) exit 0 ;; *) exit 1 ;; esac
}

cmd_lvev6frames () # Legacy m2vmp2cut support; dig cutpoints from ~/.lve/* file
{
	$M2VMP2CUT_CMD_PATH/lvev6frames.pl
}

chkindexes ()
{
	test -s "$1/video.index" ||
		$M2VMP2CUT_CMD_PATH/m2vscan "$1/video.m2v" "$1/video.index"

	test -s "$1/audio.scan" -a -s "$1/audio.levels" ||
		$M2VMP2CUT_CMD_PATH/mp2cutpoints \
			--scan "$1/audio.mp2" "$1/audio.scan" "$1/audio.levels"
}

getcmds ()
{
	cmds=`env which projectx java mplex 2>/dev/null | tr '\012' :`
}
needcmd ()
{
	case $cmds in */$1:) ;; *)
		c=$1; shift
		die "Command '$c' missing ($*)"
	esac
}


chkpjx () # getcmds run before
{
	case $cmds in */projectx:*) pjx=projectx; return; esac

	test -h $M2VMP2CUT_CMD_PATH/ProjectX.jar || {
	  warn "Symbolic link '$M2VMP2CUT_CMD_PATH/ProjectX.jar' does not exist"
	  die 'Please provide link and try again'
	}
	pjxjar=`LC_ALL=C ls -l $M2VMP2CUT_CMD_PATH/ProjectX.jar | sed 's/.* //'`
	test -f $pjxjar || {
		warn "ProjectX jar file '$pjxjar' does not exist."
		die "Fix this or it's symbolic link reference '$M2VMP2CUT_CMD_PATH/ProjectX.jar'."
	}
	pjx="java -jar $pjxjar"
}

findlargestaudio ()
{
	largestaudio=nothing largestsize=0
	for f
	do	currentsize=`stat -c%s "$f"`
		test $currentsize -gt $largestsize || continue
		largestaudio=$f
		largestsize=$currentsize
	done
}

cmd_tmp ()
{
	cd "$dir"
	rm audio.scan
	chkindexes .
}

cmd_demux () # Demux mpeg2 file[s] with ProjectX for further editing...
{
	case $file in '') die "demux reguires 'file' argument."; esac
	getcmds
	chkpjx
	needcmd mplex needed after cutting when multiplexing final image

	if test -d "$dir"
	then	die "Directory '$dir' is on the way (demuxed already)?"
	fi
	mkdir "$dir"
	x $pjx -ini /dev/null -out "$dir" -name in "$file" "$@"
	cd "$dir"

#	ac3=`ls *.ac3 2>/dev/null`
	mp2=`ls *.mp2 2>/dev/null`
#	if [ -n "$ac3" ] ; then
#		findlargestaudio $ac3
#		ln -s $largestaudio audio.ac3
#	elif [ -n "$mp2" ] ; then
	case $mp2 in '')
		die "No audio files to process"
	;; *)
		findlargestaudio $mp2
		ln -s $largestaudio audio.mp2
	esac
	ln -s in.m2v video.m2v

	chkindexes .
	echo
	echo "Files demuxed in '$dir'"
}

cmd_select () # Select parts from video with a graphical tool
{
	test -f "$dir/video.m2v" || die "'$dir/video.m2v' does not exist"
	chkindexes "$dir"
	x $M2VMP2CUT_CMD_PATH/m2vcut-gui "$dir/video.index" "$dir/video.m2v" \
		"$dir/cutpoints" "$dir/audio.levels"
	# append cutpoint history from backup file made by m2vcut-gui
	if test -f "$dir/cutpoints.1"
	then	case `wc "$dir/cutpoints"` in
			*' '1' '*) cat "$dir/cutpoints.1" >> "$dir/cutpoints"
		esac
	fi
}

cmd_cut () # Cut using m2vmp2cut.pl for the work...
{
	x exec $M2VMP2CUT_CMD_PATH/m2vmp2cut.pl --dir="$dir" "$@"
}

cmd_play () # Play resulting file with mplayer
{
	f="$dir"/m2vmp2cut-work/out.mpg
	test -f "$f" || die "'$f' does not exist"
	x exec mplayer "$@" "$f"
}

cmd_move () # Move final file to a new location (and name)
{
	needvar "${1-}" usage '<destfile>'
	f="$dir"/m2vmp2cut-work/out.mpg
	test -f "$f" || die "'$f' does not exist"
	x mv "$f" "$1"
}

cmd_getyuv () # Get selected parts of mpeg2 video as yuv(4mpeg) frames
{
	case "${1-}" in examp*) dir=examples; esac
	x exec $M2VMP2CUT_CMD_PATH/getyuv.pl "$dir"
}

cmd_getmp2 () # Get selected parts of mp2 audio
{
	case "${1-}" in examp*) dir=examples; esac
	x exec $M2VMP2CUT_CMD_PATH/getmp2.sh "$dir"
}

cmd_contrib () # Contrib material, encoding scripts etc...
{
	M2VMP2CUT_CMD_DIRNAME=`dirname "$M2VMP2CUT_CMD_PATH"`
	case ${1-} in '')
		echo
		echo Append one of these to your command line to continue.
		echo Unambiquous prefix will do...
		echo
		cd $M2VMP2CUT_CMD_DIRNAME/contrib
		ls -1 | while read line
		do
			sed -n '2 { s/./ '"$line"'                    /
				    s/\(.\{16\}\) */\1/p; q; }' $line
		done
		echo; exit 0
	esac
	for f in `ls -1 $M2VMP2CUT_CMD_DIRNAME/contrib`
	do
		case $f in
		    $1) fp= ff=$1 fm=$1 break ;;
		    $1*) fp=$ff; ff="$f $ff"; fm=$f ;;
		esac
	done
	case $ff in '') die "'$1': not found." ;; esac
	case $fp in '') ;; *) die "contrib: ambiquous match: $ff." ;; esac
	shift
	export M2VMP2CUT_CMD_DIRNAME
	x exec $M2VMP2CUT_CMD_DIRNAME/contrib/$fm "$@"
}

cmd_help () # Help of all or some of the commands above
{
	case ${1-} in '') cut -d: -f 2- <<.
	:
	: Enter help <command-name> or '.' to see help of all commands at once.
	:
	: The <file>/<directory> argument can be given before or after the
	: command. Also, if <file> arg is given when <directory> is expected
	: the needed directory is tried to be deduced from given file name.
	:
.
	exit 0 ;; esac
	echo
	sed -n "s|^#h $1[^:]*:||p" "$0"
}

cmd_example () # Simple example commands
{
	cut -d: -f 2- >&2 <<.
	:
	: Simple example commands. In select/cut/play '<dir>' can be used.
	: (<file> is for user convenience...).
	: The <file>/<dir> option can also be given after command name...
	:
	: m2vmp2cut <file> demux
	: m2vmp2cut <directory> select
	: m2vmp2cut <directory> cut
	: m2vmp2cut <directory> play
	:
	: In above, there was basic workflow. 'select' gui provides a test
	: option -- but if you want to re-test, run these.
	:
	: m2vmp2cut <directory> cut --test=200
	: m2vmp2cut <directory> play
	:
	: getyuv and getmp2 has their own examples. Enter 'example' to their
	: command lines to see those.
	:
.
}

# ---

# no interactive behaviour in batch mode... (if ever implemented).
case ${1-} in --batch) batch=1; shift ;; *) batch= ;; esac

case $# in 0)
;; 1)	set x '' one; shift
;; *)
	case $1 in *' '*) warn
		die "Support for spaces in '$1' may not have been implemented."
	esac
	if test -f "$1"
	then
 		file=$1
 		dir=`basename "$file" | sed 's/\.[^.]*$//'`.d
		shift
	elif test -d "$1"
	then
		file=
		dir=$1
		shift
	else
		warn; die "'$1': no such file or directory."
	fi
esac

# ---

case ${1-} in '')
	bn=`basename "$0" .sh`
	echo
	echo Usage: $bn '[-batch] (file|directory) <command> [args]'
	echo
	echo $bn commands available:
	echo
	sed -n '/^cmd_[a-z0-9_]/ { s/cmd_/ /; s/ () [ -#]*/                   /
		s/$0/'"`exec basename "$0"`"'/; s/\(.\{14\}\) */\1/p; }' "$0"
	echo
	echo Commands may be abbreviated until ambiguous.
	echo
	case ${2-} in one)
	 warn "$bn requires 2 arguments, 'file/directory' and 'command'."
	 warn "Enter '.' as file/directory argument in case it is irrelevant."
	 warn "For example: $bn . help"
	esac
	exit 0
esac

cm=$1; shift

# case $cm in
# 	d) cm=diff ;;
# esac

cc= cp=
for m in `LC_ALL=C exec sed -n 's/^cmd_\([a-z0-9_]*\) (.*/\1/p' "$0"`
do
	case $m in
		$cm) cp= cc=1 cmd=$cm; break ;;
		$cm*) cp=$cc; cc="$m $cc"; cmd=$m ;;
	esac
done

case $cc in '') echo $0: $cm -- command not found.; exit 1
esac
case $cp in '') ;; *) echo $0: $cm -- ambiguous command: matches $cc; exit 1
esac
unset cc cp cm
#set -x
cmd_$cmd "$@"
exit

# xxxx

# fixme: move these to separate doc file (w/ locale extension)

#h lvev6frames: lvev6frames (no options)
#h lvev6frames:
#h lvev6frames: Old versions of m2vmp2cut supported using lve-generated
#h lvev6frames: "edit lists" for cutpoint information. With lvev6frames
#h lvev6frames: these old edits can be used with this m2vmp2cut version
#h lvev6frames:

#h demux: demux
#h demux:
#h demux: m2vmp2cut reguires mpeg files to be demuxed to elementary streams
#h demux: before cutting. This command uses ProjectX to do the demuxing.
#h demux: A separate directory (based on source filename) is created for
#h demux: output files. Note that this doubles the disk usage of a particular
#h demux: source file.
#h demux: (Not yet counting the space needed for the final output file.)
#h demux:

#h select: select
#h select:
#h select: This command uses new m2vcut-gui graphical utility for searching
#h select: cutpoints. This work is done frame-accurately.
#h select:

#h cut: cut [options] ...
#h cut:
#h cut: This command is wrapper to m2vmp2cut.pl (which used to be the frontend
#h cut: of m2vmp2cut in old versions). This command has extensive help of
#h cut: it's own. Note that this (again) adds one third of disk usage so far
#h cut: when this creates final output file.
#h cut:

#h play: play [options]
#h play:
#h play: This command runs mplayer for the file created with cut command
#h play:

#h move: move <destfile>
#h move:
#h move: Moves final output file to a new destination.
#h move:

#h getyuv: getyuv [examples]
#h getyuv:
#h getyuv: Decodes selected mpeg2 frames as a stream of yuv4mpeg pictures.
#h getyuv: useful for further encoding.
#h getyuv: If examples argument is given this command provides example output.
#h getyuv:

#h getmp2: getmp2 [examples]
#h getmp2:
#h getmp2: Extracts selected mp2 audio data, to be muxed with (re-encoded)
#h getmp2: video. Mp2 data can be used as is, or as encoded to mp3 or vorbis...
#h getmp2: If examples argument is given this command provides example output.
#h getmp2:

#h contrib: contrib
#h contrib:
#h contrib: Contribution material. Mostly encoding scripts. The command line
#h contrib: interface of the programs these scripts invoke may change over
#h contrib: time -- there is less quarantees that these work in future than
#h contrib: other m2vmp2cut functionality, in short term period...
#h contrib:
