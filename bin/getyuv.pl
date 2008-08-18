#!/usr/bin/env perl
#
# Author: Tomi Ollila -- tomi.ollila ät iki piste fi
#
#	Copyright (c) 2004 Tomi Ollila
#	    All rights reserved
#
# Created: Sun Aug 03 20:11:33 EEST 2008 too
# Last modified: Sun Aug 10 12:52:02 EEST 2008 too
#

use strict;
use warnings;

use File::Basename;
my $MYPATH; BEGIN { $MYPATH = dirname($0); }
use lib $MYPATH;
use m2vmp2cut;

die "Usage: $0 <directory>\n" unless defined ($ARGV[0]);

if ($ARGV[0] ne 'examples') {
    die "'$ARGV[0]': not a directory\n" unless -d $ARGV[0];
}
elsif (! -d 'examples') {
    exec 'sed',  '1,/^# -- examples /d; s/^/ /', $0;
}

my $videofile = "$ARGV[0]/video.m2v"; needfile $videofile;
my $indexfile = "$ARGV[0]/video.index"; needfile $indexfile;
my $cutpoints = "$ARGV[0]/cutpoints"; needfile $cutpoints;

die "Will not output to a tty; please pipe output\n" if -t STDOUT;

select STDERR;

my @cutpoints;
getcutpoints $cutpoints, \@cutpoints;
#print "@cutpoints\n";

my (@opts, @fr);
my ($lastegop, $startgopff) = (0, 0);

openI $indexfile;
$" = ',';
foreach (@cutpoints)
{
    my ($s, $e) = split('-');
    my ($sf, $ef);

    seekgopframesI($s + 1, 0);
    #print "# ===== $::currgop - $::prevgopff $::currgopff ($s) $::nextgopff ",
    #  "-- $::prevgoppos $::currgoppos $::nextgoppos\n";
    #print "cg $::currgop  leg $lastegop\n";

    if ($::currgop - $lastegop < 3) {
	($sf, $ef) = ($s - $startgopff, $e - $startgopff);
	push @fr, "$sf-$ef";
    }
    else {
	push @opts, "@fr" if @fr;
	$startgopff = $::prevgopff;
	($sf, $ef) = ($s - $startgopff, $e - $startgopff);
	@fr = ( "$sf-$ef" );
	push @opts, ('-sb', "$::prevgoppos", '-c' );
    }
    seekgopframesI($e, 1);
    #print "# ----- $::currgop - $::prevgopff $::currgopff ($e) $::nextgopff ",
    #  "-- $::prevgoppos $::currgoppos $::nextgoppos\n";
    $lastegop = $::currgop;

}
close I;

push @opts, "@fr" if @fr;
$" = ' ';
push @opts, $videofile;
#print " @opts\n";

exec "$MYPATH/m2vtoyuv", @opts;
die "exec failed: $!\n";

__END__;

# -- examples --



Pipe output to mplayer:  | mplayer -

Re-encode to mpeg2, using mpeg2enc, with denoise and deinterlace:

| yuvdenoise | yuvdeinterlace | mpeg2enc ...

 above: kvcd, 2-pass, jne.


Re-encode to mpeg2, using ffmpeg ...


Re-encode to mpeg2, using mencoder ...

transcode (loads of options) & gstreamer tba?

( ehkä kaikki muxaus mp2 -puolelle kun tähän tullee niin paljon.

--- add muxing: mpeg2, ogmmerge, mkvmerge, (TS...)
--- mpeg2, TS to mp2 section, ogmmerge, mkvmerge here.


