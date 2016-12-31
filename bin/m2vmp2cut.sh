#!/bin/sh
# $Id; m2vmp2cut.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Wed Apr 23 21:40:17 EEST 2008 too
# Last modified: Sat 31 Dec 2016 14:34:51 +0200 too

set -eu

case ~ in '~') exec >&2; echo
	echo "Shell '/bin/sh' lacks some required modern shell functionality."
	echo "Try 'ksh $0${1+ $*}', 'bash $0${1+ $*}'"
	echo " or 'zsh $0${1+ $*}' instead."; echo
	exit 1
esac

set +f

case ${BASH_VERSION-} in *.*) shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate zsh; set -eu; esac

warn () { echo "$@" >&2; }
die ()  { for l; do echo "$l"; done; exit 1; } >&2

usage () {
	die '' "Usage: m2vmp2cut (file|directory) $cmd $*" ''
}

x () { echo + "$@" >&2; "$@"; }
xcd () { echo + cd "$1" >&2; cd "$1"; }

M2VMP2CUT_CMD_PATH=`case $0 in */*) cd "${0%/*}"; esac; pwd`
case $M2VMP2CUT_CMD_PATH in
	*["$IFS"]*) die '' "Path '$M2VMP2CUT_CMD_PATH' contains spaces!" ''
esac
export M2VMP2CUT_CMD_PATH

#echo $M2VMP2CUT_CMD_PATH

# do not show this in command list
cmd_vermatch ()
{
	case $1 in 6) exit 0 ;; *) exit 1 ;; esac
}

cmd_lvev6frames () ## Legacy m2vmp2cut support; dig cutpoints from ~/.lve/*
{
	case ${1-} in '!') ;; *)
		warn; warn "'lvev6frames' command is deprecated"
		die "to use this add '!' to the command line" ''
	esac
	$M2VMP2CUT_CMD_PATH/lvev6frames.pl
}

linkaudio ()
{
	# ac3=`exec ls *.ac3 2>/dev/null`
	# note: ls in pipeline below may fail, sort may not, so no need to \
	mp2s=`ls -1 *.mp2 | sort -k 2 -t - -n` # protect against nonzero exit
	case $mp2s in
		'')	warn "No audio files to process"; return 0 ;;
		*'*'*)	warn "No audio files to process"; return 0 ;;
	esac
	perl -e 'my ($maxl, $maxn) = (-1, "");
		foreach (@ARGV) {
			next if $_ eq "audio.mp2";
			if (-s $_ > $maxl) { $maxl = -s $_, $maxn = $_; }
		}
		symlink $maxn, "audio.mp2";' $mp2s
}

chkindexes ()
{
	test -s "$1/video.index" || {
		$M2VMP2CUT_CMD_PATH/m2vscan "$1/video.m2v" "$1/video.index.wip"
		mv "$1/video.index.wip" "$1/video.index"
	}
	test -f "$1/audio.mp2" || {
		case "$1" in '.') linkaudio
		;;	*) warn No audio'!'
		esac
		return 1
	}

	test -h "$1/audio.mp2" || {
		warn "'$1/audio.mp2' not symlink -- skipping audio scans"
		return 1
	}

	test -s "$1/audio.levels" || (
		xcd "$1"
		lna=`ls -l "audio.mp2" | sed 's/.* //'`
		alevels=
		for f in *.mp2
		do	ap=${f%.mp2}
			case $f in audio.mp2) continue
				;; "$lna") alevels=$ap.levels.wip
			esac
			x $M2VMP2CUT_CMD_PATH/mp2cutpoints --scan \
				"$f" "$ap.scan.wip" "$alevels"
			case $alevels in '') ;; *)
				mv -f "$alevels" "$ap.levels"
				ln -sf "$ap.levels" audio.levels.wip
				alevels=
				ln -sf "$ap.scan" audio.scan
			esac
			mv -f "$ap.scan.wip" "$ap.scan"
		done
		exec mv -f audio.levels.wip audio.levels
	)
}

needcmd ()
{
	command -v "$1" >/dev/null || {
		c=$1; shift
		die '' "Command '$c' missing ($*)" ''
	}
}

xneedfile () {
	test -f "$1" || {
		case $1
		in /*) warn "'$1' does not exist"
		;; *)  warn "'$PWD/$1' does not exist"
		esac
		sleep 2
		exit 1
	}
}


chkpjx ()
{
	if (	cd "$M2VMP2CUT_CMD_PATH"/..
		test -f ProjectX.jar || exit 1
		test -h ProjectX.jar || {
			xneedfile lib/commons-net-1.3.0.jar
			xneedfile lib/jakarta-oro-2.0.8.jar
			exit 0
		}
		pjx=`ls -l ProjectX.jar | sed 's/.* //'`
		xneedfile "$pjx"
		case $pjx in */*) cd "${pjx%/*}"; esac
		xneedfile lib/commons-net-1.3.0.jar
		xneedfile lib/jakarta-oro-2.0.8.jar
		exit 0 )
	then
		pjx="java -jar $pjxjar"
		return 0
	fi
	if command -v projectx >/dev/null
	then 	pjx=projectx
		return 0
	fi
	die '' "Cannot find 'projectx' tool" ''
}


cmd_tmp ()
{
	cd "$dir"
	rm audio.scan
	chkindexes .
}

dodemux ()
{
	mkdir "$dir"/"$1"
	x $pjx -ini "$dir"/X.ini -out "$dir/$1" -name "$1" -demux "$2"
	x $M2VMP2CUT_CMD_PATH/demux-pp.pl "$dir" "$1"
	for f in "$dir/$1"/*.sup
	do	case $f in *'*'* | '') continue; esac
		b=${f%.sup}; b=st${b##*/sp}
		x $M2VMP2CUT_CMD_PATH/pxsuptime.py "$f" "$1/$b" \
		  > "$dir"/$b.suptime
	done

}

cmd_demux () # Demux mpeg2 file[s] with ProjectX for further editing...
{
	case $file in '') die '' "demux reguires 'file' argument." ''; esac
	for f in "$file" "$@"
	do	test -f "$f" || die '' "'$f': no such file" ''
	done
	chkpjx
	#needcmd mplex needed after cutting when multiplexing final image

	if test -d "$dir"
	then	die '' "Directory '$dir' is on the way (demuxed already)?" ''
	fi

	x () {
		exec 8>$dir/demux.log
		x () { $M2VMP2CUT_CMD_PATH/eteen + 8/ "$@"; }
		x "$@"
	}
	xcd () { echo + cd "$1" >&8; echo + cd "$1" >&2; cd "$1"; }

	mkdir "$dir"
	exec 3>&1 1> "$dir"/X.ini
	echo Application.Language=en
	echo ExternPanel.appendLangToFileName=1
	echo SubtitlePanel.SubtitleExportFormat=SRT
	echo SubtitlePanel.SubtitleExportFormat_2=SON
	# get *.sup.idx (someone may find purpose in the future)
	echo SubtitlePanel.exportAsVobSub=1
	# these 3 doesn't seem to make much of a difference
	echo SubtitlePanel.SubpictureColorModel='(2) 256 colours'
	echo SubtitlePanel.enableHDSub=1
	echo SubtitlePanel.keepColourTable=1
	exec 1>&3 3>&-
	dodemux 'in1' "$file"
	case $# in 0) ;; *) c=1; for f in "$@"; do
			c=$((c + 1))
			dodemux 'in'$c "$f"
		done
		echo 'More than one file was demuxed'
		echo "Concatenate media files in '$dir'"
		echo "Before issuing m2vmp2cut 'select' command"; exit 0
	esac
	## XXX all of these needs to be moved to place where select can do too
	## XXX it seems some audio handling is going to gui tool already...
	xcd "$dir"
	ln -s video1.m2v video.m2v
	linkaudio
	chkindexes . && noaudio=false || noaudio=true
	echo
	echo "Files demuxed in '$dir'"
	echo "See also '$dir/demux.log'"
	grep -v '^[+]' 'demux.log' || :
	echo >&8 "+ : See also $dir/in*/*_log.txt"
	exec 8>&-
	# $noaudio ||
	echo 'Next:  pp fixbmps  or  select'
	exit
}

cmd_select () # Select parts from video with a graphical tool
{
	test -f "$dir/video1.m2v" ||
		die '' "'$dir/video1.m2v' does not exist" ''
	# xxx "$dir/in1/audio1-2.mp2" does not check audio2-2.mp2 in case...
	if test -f "$dir/in1/sp1-1.sup" || test -f "$dir/audio.mp2"
	then
		x $M2VMP2CUT_CMD_PATH/assel-gui "$dir"
		x $M2VMP2CUT_CMD_PATH/assel.pl "$dir" linkfirstaudio || :
	fi
	chkindexes "$dir" || :
	x $M2VMP2CUT_CMD_PATH/m2vcut-gui "$dir/video.index" "$dir/video1.m2v" \
		"$dir/cutpoints" "$dir/audio.levels"
	# append cutpoint history from backup file made by m2vcut-gui
	if test -f "$dir/cutpoints.1"
	then	case `exec wc "$dir/cutpoints"` in
			*' '1' '*) cat "$dir/cutpoints.1" >> "$dir/cutpoints"
		esac
	fi
	echo 'Next: pp ... or cut'
}

cmd_pp () # The new post-processing tools (with various quality)
{
	M2VMP2CUT_DIR=${M2VMP2CUT_CMD_PATH%/*}
	M2VMP2CUT_PP_PATH=$M2VMP2CUT_DIR/ppbin
	export M2VMP2CUT_PP_PATH
	case ${1-} in '')
		echo
		echo Append one of these to your command line to continue.
		echo The choice can be abbreviated to any unambiguous prefix.
		echo
		cd $M2VMP2CUT_PP_PATH
		ls -1 | while read line
		do
			case $line in *~) continue; esac
			sed -n '3 { s/./ '"$line"'                    /
				    s/\(.\{14\}\) */\1/p; q; }' $line
		done
		echo; exit 0
	esac
	fp= ff=
	for f in `exec ls -1 $M2VMP2CUT_PP_PATH`
	do
		case $f in *~) continue
			;; $1) fp= ff=$1 fm=$1; break
			;; $1.*) fp= ff=$f fm=$f; break
			;; $1*) fp=$ff; ff="$f $ff"; fm=$f
		esac
	done
	case $ff in '') die '' "'$1': not found." '' ;; esac
	case $fp in '') ;; *) die '' "pp: ambiquous match: $ff" '' ;; esac
	shift
	x exec $M2VMP2CUT_PP_PATH/$fm "$@"
}

cmd_cut () # The old cut using m2vmp2cut.pl to do the work...
{
	eval last_arg=\$$# # $0 being last is ok in this case
	case ${last_arg-} in '!') ;; *)
		set_bn0
		warn
		warn "Suggest doing  $bn0 '$dir' pp icut"
		warn "or  $bn0 '$dir' pp imkvcut  instead."
		warn "If these does not work, execute (with trailing '!')"
		die '' "  $bn0 '$dir' cut${1+ $*} !" ''
	esac
	test $# = 0 || { # drop last arg (!)
		arg=$1; shift
		for na; do set x "$@" $arg; shift 2; arg=$na; done
	}
	x exec $M2VMP2CUT_CMD_PATH/m2vmp2cut.pl --dir="$dir" "$@"
}

cmd_move () # Move final file to a new location (and name)
{
	test $# = 1 || usage '{destfile, without .(mpg|mkv) suffix}'
	files='out.mkv, out.mpg or m2vmp2cut-work/out.mpg'
	for f in $files
	do
		test $f != 'or' || continue; f=${f%,}
		sfx=${f##*.}
		if test -f "$dir"/$f
		then
			if test -f "$1.$sfx"
			then die '' "Destination file '$1.$sfx' exists" ''; fi
			x exec mv "$dir"/$f "$1.$sfx"
		fi
	done
	die '' "Cannot find any of $files in directory" "  '$dir'" ''
}

cmd_play () # Play resulting file with mplayer
{
	files='out.mkv, out.mpg or m2vmp2cut-work/out.mpg'
	for f in $files
	do
		test "$f" != or || continue; f=${f%,}
		test ! -f "$dir"/$f || x exec mplayer "$@" "$dir"/$f
	done
	die '' "Cannot find any of $files in directory" "  '$dir'" ''
}

cmd_getyuv () # Get selected parts of mpeg2 video as yuv(4mpeg) frames (tbm)
{
	case "${1-}" in examp*) dir=examples; esac
	x exec $M2VMP2CUT_CMD_PATH/getyuv.pl "$dir"
}

cmd_getmp2 () # Get selected parts of mp2 audio (to be moved, like above)
{
	case "${1-}" in examp*) dir=examples; esac
	x exec $M2VMP2CUT_CMD_PATH/getmp2.sh "$dir"
}

cmd_help () # Help of all or some of the commands above
{
	case ${1-} in '') echo "
 Enter . help {command-name} (or '.' to see help of all commands at once).

 The  {file}/{directory} argument is given before the command so that
 commands can easily changed at the command line.  If {file} argument
 is given when {directory} is expected, the directory is deduced from
 it the same way 'demux' command creates directory name (the trailing
 'd' of the directory name can be omitted for user command line tab
 completion convenience).
"
	exit 0; esac
	echo
	sed -n "s|^#h $1[^:]*:||p" "$0"
}

cmd_example () # Simple example commands
{
	echo "
 Simple example commands. In select/cut/play the '{dir}' where demuxed
 can also be given instead of original {file} name.

 m2vmp2cut {file} demux
 m2vmp2cut {file/dir} select
 m2vmp2cut {file/dir} cut (well, use m2vmp2cut {file/dir} pp imkvcut instead!)
 m2vmp2cut {file/dir} play

 In above, there was basic workflow. 'select' gui provides a test
 option -- but if you want to re-test, run these.

 m2vmp2cut {file/dir} cut --test=200
 m2vmp2cut {file/dir} play

 getyuv and getmp2 have their own examples. Enter 'example' to their
 command lines to see those.
"
}

cmd_source () # Check source of given '$0' command or function
{
	set +x
	case ${1-} in '') die '' "$0 $cmd cmd-prefix" '' ;; esac
	echo
	exec sed -n "/^cmd_$1/,/^}/p; /^$1/,/^}/p" "$0"
	#exec sed -n "/cmd_$1.*(/,/^}/p" "$0"
}

# ---

unset setx
case ${1-} in -x) setx=t; shift
esac

# no interactive behaviour in batch mode... (if ever implemented).
case ${1-} in --batch) batch=1; shift ;; *) batch= ;; esac

case $# in 0)
;; 1)	set x '' one; shift
;; *)
	#case $1 in *["$IFS"]*) warn
	#	die "Support for spaces in '$1' may not have been implemented."
	#esac
	if test -f "$1"
	then
		file=$1
		dir=${file##*/} # basename, $file cannot end with '/'
		dir=${dir%.*} # remove .suffix (if any)
		dir=$dir.d
		shift
	elif test -d "$1"
	then
		file=
		dir=${1%/} # w/o trailing '/' if one
		shift
	elif test -d "$1"d
	then
		file=
		dir=${1}d
		shift
	else
		die '' "'$1': no such file or directory." ''
	fi
esac

# ---

set_bn0 () {
	bn0=${0##*/}; bn0=${bn0%.sh}
}

case ${1-} in '')
	set_bn0
	echo
	echo Usage: $bn0 '[-batch] (file|directory) {command} [args]'
	echo
	echo $bn0 commands available:
	echo
	sed -n '/^cmd_[a-z0-9_].*() *#[^#]/ { s/cmd_/ /;
		s/ () [ #]*/                   /
		s/$0/'"$bn0"'/; s/\(.\{14\}\) */\1/p; }' "$0"
	echo
	echo Command can be abbreviated to any unambiguous prefix.
	echo
	case ${2-} in one)
	 warn "$bn0 requires 2 arguments, 'file/directory' and 'command'."
	 warn "Enter '.' as file/directory argument in case it is irrelevant."
	 warn "For example: $bn0 . help"
	esac
	exit 0
esac

cm=$1; shift

case $cm in
	mv) cm=move
esac

cc= cp=
for m in `exec sed -n 's/^cmd_\([a-z0-9_]*\) (.*/\1/p' "$0"`
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

M2VMP2CUT_MEDIA_DIRECTORY=$dir
export M2VMP2CUT_MEDIA_DIRECTORY

readonly cmd
case ${setx-} in t) unset setx; set -x
esac
cmd_$cmd "$@"
exit

# xxxx

# fixme: move these to separate doc file (w/ locale extension)

#h lvev6frames: lvev6frames !
#h lvev6frames:
#h lvev6frames: Old versions of m2vmp2cut supported using  lve-generated
#h lvev6frames: "edit lists" for cutpoint information.  With lvev6frames
#h lvev6frames: these old edits can be used with this m2vmp2cut version.
#h lvev6frames: This is deprecated feature. To use this the '!' needs to
#h lvev6frames: be added to the command line.
#h lvev6frames:

#h demux: demux [projectx options]
#h demux:
#h demux: m2vmp2cut reguires mpeg files to be demuxed into elementary streams
#h demux: before cutting. This command uses ProjectX to do the demuxing.
#h demux: A separate directory (in current directory, based on source filename)
#h demux: is created for output files.
#h demux: 'projectx options' may contain e.g. more filenames to be demuxed
#h demux: into output streams.
#h demux:

#h select: select
#h select:
#h select: This command uses m2vcut-gui graphical utility for searching
#h select: cutpoints.
#h select:

#h cut: cut [options] ... !
#h cut:
#h cut: This command is wrapper to m2vmp2cut.pl (which used to be the frontend
#h cut: of m2vmp2cut in old versions). This command has extensive help of
#h cut: it's own. Note that this (again) adds one third of disk usage so far
#h cut: when this creates final output file.
#h cut:
#h cut: This has fallen a bit behind of newer developments; pp icut and imkvcut
#h cut: have more options, like muxing more than one audio file and muxing
#h cut: (blu-ray compatible?) subtitle (picture) files (imkvcut only).
#h cut:

#h play: play [mplayer options]
#h play:
#h play: This command runs mplayer for the file created with cut command
#h play:

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

#h pp: pp
#h pp:
#h pp: The best post-processing tools m2vmp2cut can offer !
#h pp:
