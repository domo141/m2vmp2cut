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
# Last modified: Wed 31 Oct 2012 16:41:13 EET too

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

 Cut without re-encoding. Start frames needs to be I-frames (checked)
 End frames should be I or P frames (this is not checked ATM), if not
 artifacts will appear. Note that the m2vmp2cut select tool ... XXX

 FIXME: options for dvd/blu-ray subtitles.
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

    while (<I>) {
	#        offset   gop    frame iframe-pos asr   time c/0 num-of-frames
	if (/^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d)\s/) {
	    my $ifn = $3 + $4;
	    next if ($ifn < $s );
	    if ($s == $ifn) {
		my $frames = $e - $s;
		push @cpargs, "$1,$frames";
		last;
	    }
	    die "Frame $s not an iframe!\n";
	}
    }
}
close I;

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
    exec "$bindir/m2vstream", $ARGV[0], $videofile, @cpargs;
}

system qw/mplex -f 8 -o out.mpg/, $videofifo, $audiofifo;
