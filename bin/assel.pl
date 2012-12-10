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
# Last modified: Tue 06 Nov 2012 19:16:09 EET too

# helper for assel gui

use 5.8.1;
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
    sub xs {
	return -1 if $a =~ /\bin[.]/;
	return 1 if $b =~ /\bin[.]/;
	return $a cmp $b;
    }
    save ((sort xs grep { -f $_ and $_ = "$_ n/a 1" } <in*.mp2>),
      sort xs grep { -f $_ and $_ = "$_ n/a 1" } <in_sp/in*.suptime>);

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
	    push @l, 'n/a' unless @l > 1;
	    push @l, '1' unless @l > 2;
	    push @afiles, "a @l";
	}
	else { # well, expect suptime...
	    push @l, 'n/a' unless @l > 1;
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
    my $stf = $ARGV[0];
    my $ipfx = $stf; $ipfx =~ s/[.]suptime$//;

    die "'$stf': not '.suptime' file\n" if $stf eq $ipfx;

    xopenI $stf;
    eval 'END { exec qw/rm -rf test-st/; }';
    unless (mkdir 'test-st') {
	system qw/rm -rf test-st/;
	mkdir 'test-st' or die "Cannot mkdir 'test-st': $!\n";
    }

    while (<I>) {
	next unless /image='(\d+)'\s+start='(\S+)'\s+end='(\S+)'\s+x='(\d+)'\s+y='(\d+)'/;
	my ($image, $start, $end, $x, $y) = ($1, $2, $3, $4, $5);
	my $if = sprintf "${ipfx}_st%05dp1.bmp", $image;
	symlink "../$if", "test-st/$start" if -f $if;
    }
    unless (fork) {
	## need to have change running without stdout redirection...
	open STDOUT, '>&', \*STDERR;
	chdir 'test-st' or die $!;
	exec qw/feh -q --draw-tinted -^/, '%f %u/%l', <*>;
	exit 1;
    }
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
