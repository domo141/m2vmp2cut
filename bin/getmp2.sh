#!/bin/sh
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Mon Aug 04 20:39:55 EEST 2008 too
# Last modified: Sun Aug 10 12:16:12 EEST 2008 too

e2 () { echo "$@" >&2; }
die () { e2 "$@"; exit 1; }

M2VMP2CUT_CMD_PATH=`cd \`dirname "$0"\`; pwd`
case $M2VMP2CUT_CMD_PATH in
    *' '*) die Too lazy to support Spaces in \'$M2VMP2CUT_CMD_PATH\' ;; esac
export M2VMP2CUT_CMD_PATH

case $1 in '') die Usage: $0 '<directory>' ;; esac

case $1 in examp*) test -d examples || \
	exec sed '1,/^# -- examples /d; s/^/ /' $0 ;;
esac


needfile () { test -f "$1" || die "'"$1"'": no such file; }

audiofile="$1/audio.mp2"; needfile "$audiofile"
scanfile="$1/audio.scan"; needfile "$scanfile"
cutpoints="$1/cutpoints"; needfile "$cutpoints"

sync=0

exec 3>&0 0>&1
#ls -l /proc/$$/fd; test also with </dev/null
timecodes=`perl -x $0 "$cutpoints" $sync`
exec 0>&3 3>&-
#ls -l /proc/$$/fd; exit 0
case $timecodes in '') exit 1;; esac
dn=`dirname $0`
cutpos=`$dn/mp2cutpoints "$timecodes" "$audiofile" "$1/audio.cuts" "$scanfile"`
exec $dn/fileparts "$cutpos" $audiofile

#!perl

use strict;
use warnings;

use File::Basename;
use lib dirname($0);
use m2vmp2cut;

die "Will not output to a tty; please pipe output\n" if -t STDIN;

my $sync = $ARGV[1] + 0;

my (@cutpoints, @timecodes);
getcutpoints $ARGV[0], \@cutpoints;
foreach (@cutpoints) {
    my ($s, $e) = split('-');
    push @timecodes,
	(palframe2timecode $s, $sync) . "-" . (palframe2timecode $e,$sync);
}
$" = ',';
print "@timecodes";

__END__;

# -- examples --

no encoding, just for muxing

encogind to vorbis

encoding to mp2
