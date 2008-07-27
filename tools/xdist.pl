#!/usr/bin/env perl
# $Id; xdist.pl $
#
# Author: Tomi Ollila <>
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Sat May 03 15:48:14 EEST 2008 too
# Last modified: Sat May 03 21:36:25 EEST 2008 too

# A hack, supports only regular files and directories.

use strict;
use warnings;

die "Usage: $0 directory\n" unless defined $ARGV[0];

die "$ARGV[0]: not a directory\n" unless -d $ARGV[0];

my $px = $ARGV[0]; $px =~ 's|/\s*||'; $px =~ 's|.*/||';

die "$px.sfx.sh already exists\n" if -e "$px.sfx.sh";

my $pwd = `pwd`; chomp $pwd;

chdir $ARGV[0] or die "chdir: $!\n";

die "$px.sfx.sh is on the way\n" if -e "$px.sfx.sh";


my @sums;

die "xdist-storage exists\n" if -d 'xdist-storage';
mkdir 'xdist-storage' or die;

open I, '-|', 'find . -type f | xargs md5sum' or die "foo\n";

while (<I>)
{
    /(\S+)\s+(.*?)\s+/;

    push @sums, [$2, $1];
}

close I;

#foreach (@sums) { print $_->[1], " ", $_->[0], "\n"; }

open D, '>', "xdist-storage/Directories" or die;
open F, '>', "xdist-storage/Files" or die;

open I, '-|', 'find . -name xdist-storage -prune -o -print' or die "bar\n";

my $i = 0;
while (<I>)
{
    chomp;
    #my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
    #	$atime, $mtime, $ctime, $blksize, $blocks) = stat;
    my $mode = (stat)[2] & 07777;

    if (-d $_)
    {
	printf D "%o $_\n", $mode;
    }
    elsif (-f $_)
    {
	my $file = $sums[$i]->[0];
	my $sum  = $sums[$i]->[1];
	die "XXX $file != $_\n" if ($file ne $_);

	if (-f "xdist-storage/$sum") {
	    my $rv = system '/usr/bin/cmp', '-l', $file, "xdist-storage/$sum";
	    if ($rv) {
		print "Files '$file' and 'xdist-storage/$sum'. ",
		  "report this and become famous\n";
	    }
	}
	else { system '/bin/cp', $file, "xdist-storage/$sum"; }

	printf F "$sum %o $_\n", $mode;
	$i++;
    }
    else { die "Can not handle element '$_'\n"; }
}

close F;
close D;

open O, '>', "$px.sfx.sh" || die "Opening '$px.sfx.sh' failed: $!\n";

print O <<"EOF";
#!/bin/sh

die () { echo "\$@" >&2; exit 1; }

test -d "$px" && die \$0: Directory "'$px'" exists.

mkdir "$px"

SKIP=`awk '/^__EOP1__/ { print NR + 1; exit 0; }' "\$0"`
[ x\$SKIP = x ] && die \$0: No content separator found.

tail -n +\$SKIP "\$0" | gzip -dc | tar -C "$px" -xf -

cd "$px"

_end () { cd ..; rm -rf "$px"; }

trap _end 0;

test -f xdist-storage/Files || die "'xdist-storage/Files'" missing.
test -f xdist-storage/Directories || die "'xdist-storage/Directories'" missing.

tail -n +2 xdist-storage/Directories | while read mode name
do
        mkdir \$name;
        chmod \$mode \$name;
done

cat xdist-storage/Files | while read sum mode name
do
        cp xdist-storage/\$sum \$name
        chmod \$mode \$name;
done

echo Extracted $px

rm -rf xdist-storage
trap - 0
exit 0
__EOP1__
EOF

close O;

system "tar zcf - xdist-storage >> $px.sfx.sh";
chmod 0755, "$px.sfx.sh";
system 'mv', "$px.sfx.sh", $pwd;
system 'rm', '-rf', 'xdist-storage';
