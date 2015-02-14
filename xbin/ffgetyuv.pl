#!/usr/bin/env perl
# -*- cperl -*-#
# getyuv where m2v is decoding done by ffmpeg
#
# Author: Tomi Ollila -- tomi.ollila ät iki piste fi
#
#	Copyright (c) 2012 Tomi Ollila
#	    All rights reserved
#
# Created: Sun Aug 03 20:11:33 EEST 2008 too (as getyuv.pl)
# Last modified: Sat 14 Feb 2015 12:08:40 +0200 too
#

use 5.8.1;
use strict;
use warnings;

my $LIBPATH;
use Cwd 'abs_path';
BEGIN { $LIBPATH = abs_path($0); $LIBPATH =~ s|[^/]+/[^/]+$|bin|; }
use lib $LIBPATH;
use m2vmp2cut;

my $dir = $ENV{M2VMP2CUT_MEDIA_DIRECTORY};
my $bindir = $ENV{M2VMP2CUT_CMD_PATH};

my $videofile = "$dir/video.m2v"; needfile $videofile;
my $indexfile = "$dir/video.index"; needfile $indexfile;
my $cutpoints = "$dir/cutpoints"; needfile $cutpoints;

die "Will not output to a tty; please pipe output\n" if -t STDOUT;

select STDERR;

my @cutpoints;
getcutpoints $cutpoints, \@cutpoints;
#print "@cutpoints\n";

my ($lastegop, $startgopff) = (0, 0);

my (@opts, @fr, $vpos);
my $vframes = 0;

openI $indexfile;
$" = ',';
foreach (@cutpoints)
{
    my ($s, $e) = split('-');

    seekgopframesI($s + 1, 0);
    #print "# ===== $::currgop - $::prevgopff $::currgopff ($s) $::nextgopff ",
    #  "-- $::prevgoppos $::currgoppos $::nextgoppos\n";
    #print "cg $::currgop  leg $lastegop\n";

    my $sf = $s - $::prevgopff;
    my $ef = $e - $::prevgopff;
    push @opts, [ $::prevgoppos, $sf, $ef ];
}
close I;

delete $ENV{GNOME_KEYRING_CONTROL};
delete $ENV{DROP_YUV4MPEG_HEADER};

$ENV{LD_PRELOAD} = $bindir . '/libpreload_ffm2vtoyuv4mpeghax.so';

open STDIN, '<', $videofile or die $!;
foreach (@opts) {
    sysseek STDIN, $_->[0], 0;
    $ENV{DROP_YUV4MPEG_FRAMES} = "$_->[1]";
    system qw/ffmpeg -i pipe:0 -f yuv4mpegpipe -vframes/, $_->[2],
	qw/-loglevel error -an -/;
    last if $?;
    $ENV{DROP_YUV4MPEG_HEADER} = 'yes';
}
