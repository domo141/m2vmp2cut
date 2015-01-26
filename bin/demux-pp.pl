#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ demux-pp.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2015 Tomi Ollila
#	    All rights reserved
#
# Created: Wed 21 Jan 2015 21:45:28 EET too
# Last modified: Mon 26 Jan 2015 23:20:12 +0200 too

# postprocess directory after m2vmp2cut.sh demux

use 5.8.1;
use strict;
use warnings;

#use subs qw/rename symlink/;
#sub rename($$) { print "rename: '$_[0]' -> '$_[1]'\n"; }
#sub symlink($$) { print "symlink: '$_[0]' <- '$_[1]'\n"; }

die "Usage: $0 {basedir} {indir} \n" unless @ARGV == 2;
die "'$ARGV[0]': not a directory\n" unless -d $ARGV[0];

sub pdie (@) {
    die "$!\n" unless @_;
    die "@_ $!\n" if $_[$#_] =~ /:$/;
    die "@_\n";
}

unless ($ARGV[0] eq '.') {
    chdir $ARGV[0] || pdie "chdir '$ARGV[0]':";
}

die "'$ARGV[1]': not a directory\n" unless -d $ARGV[1];

die "XXX '$ARGV[1]' contans slashes (/)\n" if index($ARGV[1], '/') >= 0;

my $dd = '..';
my $id = $ARGV[1];
my $of;

chdir $id || pdie "chdir '$id':";

my @sups = sort { my ($aa, $bb) = (length $a, length $b);
		  return $aa <=> $bb if $aa != $bb;
		  return $a cmp $b; } <*.sup>;

my %sc;
my $c = 0;
foreach (@sups) {
    /^in(\d+)(.*?)[.]sup$/ || next;
    $c += 1;
    #print qq'sc{"$1$2"} = "$1-$c";', "\n";
    $sc{"$1$2"} = "$1-$c";
}

for (<*.sup>) {
    /^in(\d+)(.*?)[.]sup$/ || next;
    my $n = $sc{"$1$2"};
    $of = "sp$n.sup";
    rename $_, $of;
    #symlink "$id/$of", "$dd/in$1.$of";
}

foreach (<*.bmp>) {
    /^in(.*)_st(\d+).*[.]bmp/ || next;
    my $n = $sc{$1};
    if (defined $n) {
	my $u = sprintf "st$n-%05d.bmp", $2 + 1;
	rename $_, $u;
    }
}

# audio files rename

#my @mp2s = sort { my ($aa, $bb) = (length $a, length $b);
#		  return $aa <=> $bb if $aa != $bb;
#		  return $a cmp $b; } <*.mp2>;

my @mp2s = sort { -M $b <=> -M $a } <*.mp2>;

$c = 0;
foreach (@mp2s) {
    my $s = $_;
    s/^in(\w*).*/audio$1/;
    $c += 1;
    my $of = "$_-$c.mp2";
    rename $s, "$of";
    symlink "$id/$of", "$dd/$of";
}

# just one .m2v supported
my @v = <*.m2v>;
die "Video files '@v' -- not just one file\n" unless @v == 1;
$v[0] =~ /^in(\d+)/;
$of = "video$1.m2v";
rename $v[0], $of;
symlink "$id/$of", "$dd/$of";
