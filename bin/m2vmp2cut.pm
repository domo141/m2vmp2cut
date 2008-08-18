#!/usr/bin/env perl
#
# Author: Tomi Ollila -- tomi.ollila ät iki piste fi
#
#	Copyright (c) 2004 Tomi Ollila
#	    All rights reserved
#
# Created: Sun Aug 03 20:12:37 EEST 2008 too
# Last modified: Wed Aug 06 18:36:18 EEST 2008 too
#

use strict;
use warnings;

sub needfile($)
{
    die "'$_[0]': no such file\n" unless -f $_[0];
}

sub openI($)
{
    open I, '<', $_[0] or die "Can not open file '$_[0]' for reading: $!\n";
}

sub getcutpoints($$)
{
    openI $_[0]; $_ = <I>; close I;

    /(^|\s)((\d+-\d+)(,\d+-\d+)*)\s*$/ or die "Incorrect cutfile format\n";

    my ($framelist, $pe) = ($2, -1);
    #$framelist = "9402-64172,72867-110977,119965-158618,165849-201601";
    #$framelist = "9402-64172,64175-110977,119965-158618,165849-201601";

    foreach (split(',', $framelist))
    {
	my ($s, $e) = split('-');

	next if ($s == $e);
	die "$s greater than $e in a range in $ARGV[0].\n" if ($s > $e);

	die "\nRange begin $s is less than previous range end $pe in $ARGV[0].\n",
	  "Which is not currently supported (as usually this is not desired).\n\n"
	    if ($s < $pe);

	if ($s == $pe)
	{
	    my $pr = pop(@{$_[1]});
	    $pr =~ s/-.*/-$e/;
	    push @{$_[1]}, $pr;
	}
	else { push @{$_[1]}, "$s-$e"; }

	$pe = $e;
    }
}

# too lazy to learn perl 5 object syntax...
our ($prevgopff, $prevgoppos, $currgopff, $currgoppos, $currgop,
     $nextgopff, $nextgoppos, $nextiframeoffset) = (0,0, 0,0,0, 0,0, 0);

our @asrs = (0,0,0,0,0,0,0,0,0,0);
sub seekgopframesI($$)
{
    my ($frame, $getasr) = @_;

    return if ($nextgopff >= $frame);

    while (<I>)
    {
	# Currently we can manage with information up to frame number.
	# format: (filepos) (gop) (frame) ts (asr) ...
	if (/^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d)\s/) {
	    $currgop = $2;
	    ($prevgopff, $prevgoppos) = ($currgopff, $currgoppos);
	    ($currgopff, $currgoppos) = ($nextgopff, $nextgoppos);
	    ($nextgopff, $nextgoppos) = ($3, $1);
	    $nextiframeoffset = $4;

	    $asrs[$5] = $asrs[$5] + 1 if ($getasr);
	    return if ($3 >= $frame);
	}
	if (/^end:\s+size\s+(\d+)\s+frames\s+(\d+)\s+gops\s+(\d+)\s/) {
	    $currgop = $3;
	    ($prevgopff, $prevgoppos) = ($currgopff, $currgoppos);
	    ($currgopff, $currgoppos) = ($nextgopff, $nextgoppos);
	    ($nextgopff, $nextgoppos) = ($2, $1);
	    $nextiframeoffset = 0;
	    return;
	}
    }
}

sub palframe2timecode($$)
{
    my $frametimems = $_[0] * 40 + $_[1];

    die "A/V sync correction requires adding silence audio, ",
      "which is currently not supported.\n" if ($frametimems < 0);

    my $ms =   ($frametimems % 1000);
    my $s = int($frametimems / 1000) % 60;
    my $m = int($frametimems / 1000 / 60) % 60;
    my $h = int($frametimems / 1000 / 60 / 60);

    return sprintf "%02d:%02d:%02d.%03d", $h, $m, $s, $ms if ($h > 0);
    return sprintf "%02d:%02d.%03d", $m, $s, $ms if ($m > 0);
    return sprintf "%02d.%03d", $s, $ms;
}


1;
