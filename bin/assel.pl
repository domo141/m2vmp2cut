#!/usr/bin/perl
# -*- cperl -*-
# $ assel.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2012 Tomi Ollila
#	    All rights reserved
#
# Created: Sat 03 Nov 2012 13:32:23 EET too
# Last modified: Mon 26 Jan 2015 23:20:43 +0200 too

# helper for assel gui

use 5.14.1;
use strict;
use warnings;

die "Usage: $0 <srcdir> <cmd> <args>\n" unless @ARGV > 1;

my $srcdir = shift;
die "'$srcdir': not a directory\n" unless -d $srcdir;
chdir $srcdir or die "Cannot use '$srcdir': $!\n";

my $cmd = shift;

sub xopenI($)
{
    open I, '<', $_[0] or die "Opening '$srcdir/$_[0]' for read failed: $!\n";
}

sub save(@)
{
    open O, '>', 'a+st.conf.wip' or die $!;
    print O "# file  lang  enabled\n";
    foreach (@_) {
	my @l = split ' ';
	print O "$l[0]  $l[1]  $l[2]\n";
    }
    close O;
    return rename 'a+st.conf.wip', 'a+st.conf';
}

sub isave()
{
    sub xs { # audio1-...1... etc.
	my $av = ( $a =~ tr/[0-9]//cdr); my $bv = ($b =~ tr/[0-9]//cdr);
	#warn "$av cmp $bv\n";
	return $av <=> $bv
    }
    save ( (grep { -f $_ and $_ = "$_ und 1" } sort xs <audio[0-9]*.mp2>),
	   (grep { -f $_ and $_ = "$_ und 1" } <st*.suptime>));
}

if ($cmd eq 'info')
{
    isave unless -f 'a+st.conf';

    my (@afiles, @sfiles);
    xopenI 'a+st.conf';
    while (<I>) {
	next if /^\s*#/;
	my @l = split ' ';

	if ($l[0] =~ /[.]mp2/) {
	    push @l, 'und' unless @l > 1;
	    push @l, '1' unless @l > 2;
	    push @afiles, "a @l";
	}
	else { # well, expect suptime...
	    push @l, 'und' unless @l > 1;
	    push @l, '1' unless @l > 2;
	    push @sfiles, "s @l";
	}
    }
    close I;
    print "$_\n" foreach (@afiles, @sfiles);
    exit 0
}

if ($cmd eq 'play')
{
    ## need to have change running without stdout redirection...
    open STDOUT, '>&', \*STDERR;
    exec 'mplayer', $ARGV[0];
    exit 1;
}

if ($cmd eq 'look')
{
    $ARGV[0] =~ /(\d+)-(\d+).suptime$/ or
      die "'$ARGV[0]': not '.suptime' file\n";
    my $ipfx = "in$1/st$1-$2";

    xopenI $ARGV[0];
    eval 'END { exec qw/rm -rf test-st/; }';
    unless (mkdir 'test-st') {
	system qw/rm -rf test-st/;
	mkdir 'test-st' or die "Cannot mkdir 'test-st': $!\n";
    }

    while (<I>) {
	next unless /image='(\d+)'\s+start='(\S+)'\s+end='(\S+)'\s+x='(\d+)'\s+y='(\d+)'/;
	my ($image, $start, $end, $x, $y) = ($1, $2, $3, $4, $5);
	my $if = sprintf "${ipfx}-%05d.bmp", $image;
	symlink "../$if", "test-st/$start" if -f $if;
    }
    my $pid;
    unless ($pid = fork) {
	## need to have change running without stdout redirection...
	open STDOUT, '>&', \*STDERR;
	chdir 'test-st' or die $!;
	exec qw/feh -q --draw-tinted -^/, '%f %u/%l', <*>;
	exit 1;
    }
    $SIG{TERM} = sub { kill 'TERM', $pid; };
    wait;
    exit $?;
}

if ($cmd eq 'save')
{
    exit ! save @ARGV;
}

if ($cmd eq 'linkfirstaudio')
{
    xopenI 'a+st.conf';
    while (<I>) {
	next if /^\s*#/;
	my @l = split ' ';

	if ($l[0] =~ /[.]mp2/) {
	    symlink $l[0], 'audio.mp2';
	    exit 0;
	}
    }
    close I;
    die "No audio content available\n";
}
