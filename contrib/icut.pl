#!/usr/bin/perl
# cut without re-encoding anything, cutpoint restrictions apply
# -*- cperl -*-
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2012 Tomi Ollila
#	    All rights reserved
#
# Created: Fri 26 Oct 2012 18:55:56 EEST too
# Last modified: Thu 28 Nov 2013 22:41:35 +0200 too

use 5.8.1;
use strict;
use warnings;

my $LIBPATH;
use Cwd 'abs_path';
BEGIN { $LIBPATH = abs_path($0); $LIBPATH =~ s|[^/]+/[^/]+$|bin|; }
use lib $LIBPATH;
use m2vmp2cut;

if (@ARGV == 0 or ($ARGV[0] ne '4:3' and $ARGV[0] ne '16:9')) {
    $0 =~ s|.*/||;
    die "
 Usage: $0 [4:3|16:9] (note to self: add probe option)

 Cut without re-encoding. Start frames needs to be I-frames
 End frames must be I or P frames. If this is not the case
 cut will not happen. Note that in current m2vcut gui tool
 selecting end frames selects the start of cutout frames
 i.e. end selection must be one-after the last included frame.

 FIXME: options for dvd/blu-ray subtitles (and also end cut
        in m2vcut gui tool!)
\n";
}

my $dir = $ENV{M2VMP2CUT_MEDIA_DIRECTORY};
my $bindir = $ENV{M2VMP2CUT_CMD_PATH};

my $videofile = "$dir/video.m2v"; needfile $videofile;
my $indexfile = "$dir/video.index"; needfile $indexfile;
my $cutpoints = "$dir/cutpoints"; needfile $cutpoints;

needfile "$dir/audio.mp2";

my @cutpoints;
getcutpoints $cutpoints, \@cutpoints;

openI $indexfile;
$" = ',';
my @cpargs;
foreach (@cutpoints)
{
    my ($s, $e) = split('-');

    my ($gop, $frametypes);
    while (<I>) {
	#        offset   gop   frame  iframe-pos asr ... frametypes
	if (/^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d)\s.*\s([BDIPX_]+)\s*$/) {
	    my $ifn = $3 + $4;
	    next if ($ifn < $s );
	    if ($s == $ifn) {
		my $frames = $e - $s;
		push @cpargs, "$1,$frames";
		last;
	    }
	    die "Frame #$s is not an I-frame (or $s slipped through)!\n";
	}
    }
    while (<I>) {
	#        offset   gop   frame ... #of-frames  frametypes
	if (/^\s*(\d+)\s+(\d+)\s+(\d+)\s.*?\s(\d+)\s+([BDIPX_]+)\s*$/) {
	    my $lfn = $3 + $4;
	    next if ($lfn < $e);
	    my $off = $e - $3 - 1;
	    die "Frame #$e slipped through!\n" if $off < 0;
	    my @frametypes = split '', $5;
	    my $ftype = $frametypes[$off];
	    $e = '', last if $ftype eq 'I' or $ftype eq 'P';
	    $e--;
	    die "Frame #$e is not an I-frame or P-frame (is $ftype-frame)!\n";
	}
    }
    die "Frame #$e not checked/found!\n" if $e;
}
close I;

unless (@cpargs) {
    die "
 There was either no selections made to be cut or the video index file
 was generated using older version of m2vmp2cut. If the latter is the
 case, remove '$indexfile' and then execute
 'm2vmp2cut $dir select' again.
 This will recreate new index file which is compatible with this tool.
\n";
}

my ($videofifo, $audiofifo) = ( "fifo.video.$$", "fifo.audio.$$" );

eval 'END { unlink $videofifo, $audiofifo }';

system 'mkfifo', $videofifo;
system 'mkfifo', $audiofifo;

unless (xfork) {
    open STDOUT, '>', $audiofifo or die $!;
    exec "$bindir/getmp2.sh", $dir;
}

unless (xfork) {
    open STDOUT, '>', $videofifo or die $!;
    exec "$bindir/m2vstream", ($ARGV[0] eq "4:3"? 2: 3), $videofile, @cpargs;
}

system qw/mplex -f 8 -o out.mpg/, $videofifo, $audiofifo;
