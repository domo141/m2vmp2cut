#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ testedl.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2016 Tomi Ollila
#	    All rights reserved
#
# Created: Sat 16 Apr 2016 18:27:49 EEST too
# Last modified: Sat 16 Apr 2016 20:40:32 +0300 too

# this program creates test.edl which can be used with mplayer
# to post-test cuts (and how those behave). naturally, it cannot
# detect if there were more stuff to be cut out (or in).
# this requires that every part is at least 1 minute long (1500 frames)

# the "wide" clip areas are for mpg compatibility with the first mplayer
# i tried this (still 40 sec was not enough for the last, w/ 60 last part
# worked. with .mkv files this seems to work better (better seek info in
# container)

use 5.8.1;
use strict;
use warnings;

die "Usage: $0 <directory>\n" unless @ARGV == 1;

chdir $ARGV[0] or die "cannot cd to '$ARGV[0]': $!\n";

open I, '<', 'cutpoints' or die "Cannot open '$ARGV[0]/cutpoints': $!\n";
$_ = <I>;
close I;
unlink 'test.edl';
s/.* //;
die unless /^(?:\d+-\d+[,\n])+$/;
my @lengths;
foreach (split /,/) {
    my ($b, $e) = split /-/;
    my $l = $e - $b;
    exit 0 if $l < 1500; # need at least 1 minute long parts
    push @lengths, $l;
}
open O, '>', 'test.edl.wip' or die "$!";
my $prev = 40;
my $tlen = 0;

sub pedl($$) {
    $tlen += $_[0];
    my $t = $tlen / 25;
    # warn $tlen, " $t \n";
    if ($_[1]) {
	printf O "%.2f1 %.2f0 0\n", $prev, $t - 20;
	printf O "%.2f1 %.2f0 1\n", $t - 20, $t - 19;
	printf O "%.2f1 %.2f0 1\n", $t + 15, $t + 16;
	$prev = $t + 16;
    }
    else {
	printf O "%.2f1 %.2f0 0\n", $prev, $t - 40;
	printf O "%.2f1 %.2f0 1\n", $t - 40, $t - 39;
    }
}

my $last = pop @lengths;
pedl $_, 1 foreach (@lengths);
pedl $last, 0;
close O;
rename 'test.edl.wip', 'test.edl';
