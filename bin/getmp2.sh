#!/bin/sh
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Mon Aug 04 20:39:55 EEST 2008 too
# Last modified: Sat 14 May 2016 17:33:44 +0300 too

set -eu

warn () { echo "$@" >&2; }
die () { echo "$@" >&2; exit 1; }

M2VMP2CUT_CMD_PATH=`cd \`exec dirname "$0"\`; pwd`
case $M2VMP2CUT_CMD_PATH in
	*["$IFS"]*) die "Whitespace in '$M2VMP2CUT_CMD_PATH'"
esac
export M2VMP2CUT_CMD_PATH

case ${1-} in --list-only) list_only=true; shift ;; *) list_only=false ;; esac

test $# = 1 || die Usage: $0 '[--list-only] (directory | file)'

#case $1 in examp*) test -d examples || \
#	exec sed '1,/^# -- examples /d; s/^/ /' "$0"
#esac

setfile () {
	test -f "$2" || die "'"$2"'": no such file
	eval $1='$'2
}

if test -d "$1"
then
	setfile audiofile "$1/audio.mp2"
	setfile scanfile "$1/audio.scan"
	setfile cutpoints "$1/cutpoints"
else
	setfile audiofile "$1"
	setfile scanfile "${1%.mp2}.scan"
	case $1 in */*) setfile cutpoints "${1%/*}/cutpoints"
		;; *)	setfile cutpoints "cutpoints"
	esac
fi

sync=0

exec 3>&0 0>&1
#ls -l /proc/$$/fd; test also with </dev/null
timecodes=`exec perl -x "$0" "$cutpoints" $sync`
exec 0>&3 3>&-
#ls -l /proc/$$/fd; exit 0
case $timecodes in '') exit 1; esac
mp2cutpoints=$M2VMP2CUT_CMD_PATH/mp2cutpoints
cutsfile=${audiofile%.mp2}.cuts
cutpos=`exec $mp2cutpoints "$timecodes" "$audiofile" "$cutsfile" "$scanfile"`

if $list_only
then
	echo $audiofile: $cutpos
	exit
fi
if test -t 1
then	die 'Will not output to a tty; please pipe output';
fi
exec $M2VMP2CUT_CMD_PATH/fileparts "$cutpos" $audiofile
die not reached

#!perl
# line 73

use strict;
use warnings;

use File::Basename;
use lib dirname($0);
use m2vmp2cut;

my $sync = $ARGV[1] + 0;

my (@cutpoints, @timecodes);
getcutpoints $ARGV[0], \@cutpoints;
foreach (@cutpoints) {
    my ($s, $e) = split('-');
    push @timecodes,
	(palframe2timecode $s, $sync) . "-" . (palframe2timecode $e, $sync);
}
print join ',', @timecodes;

__END__;

# -- examples --

tba. some (0ld) ideas (?)

no encoding, just for muxing

encogind to vorbis

encoding to mp2
