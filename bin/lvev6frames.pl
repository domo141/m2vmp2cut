#!/usr/bin/env perl
#
# Author: Tomi Ollila -- too ät iki fi
#
#	Copyright (c) 2003,2004 Tomi Ollila
#	    All rights reserved
#
# Created: Sat Mar  8 10:58:25 2003 too
# Last modified: Sat Dec 11 00:00:01 EET 2004 too
#
# This program is licensed under the GPL v2. See file COPYING for details.

#
# This code can do LVE[MED|ETV]6 file format...
#
# This is a quick hack.
#

use strict;
use warnings;

#c# $0 = 'm2vmp2cut builtin lvev6frames.pl';

#c# wait;



sub LVEHDRSIZE () { 12 + 4 + 4 + 4 }

sub LVEBUFSIZE () { 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 1024 + 88 * 72 * 2 }


sub readhdr # returns ( $id, undef, $cnt, $dist_sum, $osize_mb )
{
    my $len = read I, my $buf, LVEHDRSIZE;

    die "$len, LVEHDRSIZE" unless ($len == LVEHDRSIZE);

    return unpack("a8a4jjj", $buf);
}

sub readrec
{
    my $len = read I, my $buf, LVEBUFSIZE;

    return ( ) if ($len == 0);

    die "$len, LVEBUFSIZE" unless ($len == LVEBUFSIZE);

    return unpack("jjjjjjjjjZ1024", $buf);
}

my %filehash = ();

sub loadmediainfo
{
    my $file = "$ENV{HOME}/.lve/lvemedia.lst";

    open(I, $file) || die "Cannot open $file: $!\n";

    my ( $id, undef, $cnt, $dist_sum, $osize_mb ) = readhdr();

    die "$id != LVEMEDV6\n" unless ($id eq "LVEMEDV6");

    while (my ( $mid, $vs, $as, $sr, $er, $dst, $s, $e, $avdb, $fname )
	   = readrec())
    {
	#print "$mid, $vs, $as, $sr, $er, $dst, $s, $e, $avdb, $fname\n";

	my $hashref = $filehash{$fname};
	unless (defined $hashref)
	{
	    my %hash = ( $sr, $er );
	    $filehash{$fname} = \%hash;
	}
	else
	{
	    ${$hashref}{$sr} = $er;
	}

    }
    close I;
}

loadmediainfo();

#for my $key1 (keys %filehash) {
#    print "key1: $key1\n";
#    my $hashref = $filehash{$key1};
#    for my $key2 (keys %{$hashref}) {
#	print "$key2 => ${$hashref}{$key2}\n"; }}

my $prevfname = '';
my @line = ();
$" = ',';

sub loadeditinfo
{
    my $file = "$ENV{HOME}/.lve/lveedit.lst";

    open(I, $file) || die "Cannot open $file: $!\n";

    my ( $id, undef, $cnt, $dist_sum, $osize_mb ) = readhdr();

    die "$id != LVEEDTV6\n" unless ($id eq "LVEEDTV6");

    while (my ( $mid, $vs, $as, $sr, $er, $dst, $s, $e, $avdb, $fname )
	   = readrec())
    {
	#print "$mid, $vs, $as, $sr, $er, $dst, $s, $e, $avdb, $fname\n";

	if ($prevfname && $fname ne $prevfname)
	{
	    select STDERR;
	    print "Currently we can handle only lve edits that contains ",
	      "only one file.\n";
	    print "$prevfname @line\n";
	    @line = ();
	}
	my $er2 = ${$filehash{$fname}}{$sr};
	if (defined $er2) { $er = $er2; }
	else {
	    warn "Endpoint for $sr not found in media list. ";
	    warn "Using edit list value $er.\n";
	}
	$prevfname = $fname;
	$er += 1; # end is noninclusive in transcode, python, in this tool, ...
	push @line, "$sr-$er";
    }
    close I;
}

loadeditinfo();
print "$prevfname @line\n" if (@line);
