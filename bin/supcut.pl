#!/usr/bin/perl
# -*- cperl -*-
# $ supcut.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2012 Tomi Ollila
#	    All rights reserved
#
# Created: Wed 31 Oct 2012 17:29:30 EET too
# Last modified: Wed 31 Oct 2012 19:05:04 EET too

use 5.8.1;
use strict;
use warnings;

die "Usage: $0 <srcdir>\n" unless defined $ARGV[0];

use File::Basename;
my $MYPATH; BEGIN { $MYPATH = dirname($0); }
use lib $MYPATH;
use m2vmp2cut;

my $cutpoints = "$ARGV[0]/cutpoints"; needfile $cutpoints;

my (@cutpoints, @timeadjust);
getcutpoints $cutpoints, \@cutpoints;
exit unless @cutpoints;
my $pe = 0;
foreach (@cutpoints) {
    my ($s, $e) = split('-');
    my ($sms, $ems) = ($s * 40, $e * 40);

    push @timeadjust, [ $sms, $ems, $sms - $pe ];
    $pe += ($ems - $sms);
}

sub tc2ms($)
{
    my @tv = split /:/, $_[0];
    die "Unknown timecode format '$_[0]'\n" unless @tv == 3;
    return int (( $tv[0] * 3600 + $tv[1] * 60 + $tv[2]) * 1000);
}
sub ms2tc($)
{
    my $h = int ($_[0] / 3600000);
    my $m = int ($_[0] / 60000) % 60;
    my $s = int ($_[0] / 1000) % 60;
    my $ms = $_[0] % 1000;
    return sprintf "%02d:%02d:%02d.%03d", $h, $m, $s, $ms;
}

foreach (<$ARGV[0]/in_sp/*.suptime>) {
    my $fn = $_;
    my @at = @timeadjust;
    my ($sms, $ems, $dist) = @{$at[0]};
    shift @at;
    open I, '<', $fn or die "$fn: $!\n";
    my @olist;
    L: while (<I>) {
	next unless /image='(\d+)'\s+start='(\S+)'\s+end='(\S+)'\s+x='(\d+)'\s+y='(\d+)'/;
	my ($image, $start, $end, $x, $y) = ($1, tc2ms $2, tc2ms $3, $4, $5);

	#print "$sms $ems - $dist - $image - $start $end - $x $y\n";

	while ($start > ($ems - 50)) { # at least 50 ms to see.
	    my $aref = shift @at;
	    last L unless defined $aref;
	    ($sms, $ems, $dist) = @{$aref};
	}
	$end = $ems if $end > $ems;

	if ($start < $sms) {
	    next if $end <= ($sms + 50); # at least 50 ms to see.
	    $start = $sms;
	}
	push @olist, (sprintf "image='%d' start='%s' end='%s' x='%d' y='%d'\n",
		      $image, ms2tc $start - $dist, ms2tc $end - $dist, $x, $y);
    }
    close I;
    if (@olist) {
	$fn =~ s/[.]?[^.]*$/.supcut/;
	print 'Writing ', scalar @olist, " subtitle entries to $fn\n";
	open O, '>', $fn or die ">'$fn': $!\n";
	print O @olist;
    }
}
