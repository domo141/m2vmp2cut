#!/usr/bin/env perl
#
# Author: Tomi Ollila -- tomi.ollila ät iki piste fi
#
#	Copyright (c) 2004 Tomi Ollila
#	    All rights reserved
#
# Created: Sun Sep 05 11:12:24 EEST 2004 too
# Last modified: Thu 26 Feb 2015 00:07:41 +0200 too
#
# This program is licensed under the GPL v2. See file COPYING for details.

use 5.8.1;
use strict;
use warnings;

my $M2VMP2CUT_CMD_PATH = $ENV{'M2VMP2CUT_CMD_PATH'};
unless ($M2VMP2CUT_CMD_PATH) { # XXX compare with m2vcut-gui.c
    $0 =~ /^(.*)\/.*?$/;
    if ($1) { $M2VMP2CUT_CMD_PATH = $1; }
    else { $M2VMP2CUT_CMD_PATH = ''; }
}

$" = ',';

my $somehdrinfo  = "$M2VMP2CUT_CMD_PATH/m2v_somehdrinfo.py";
my $catfiltered  = "$M2VMP2CUT_CMD_PATH/m2v_catfiltered.py";
my $lvev6frames  = "$M2VMP2CUT_CMD_PATH/lvev6frames.pl";

my $m2vscan   = "$M2VMP2CUT_CMD_PATH/m2vscan";
my $m2vtoyuv   = "$M2VMP2CUT_CMD_PATH/m2vtoyuv";
my $mp2cutpoints = "$M2VMP2CUT_CMD_PATH/mp2cutpoints";
my $m2vfilter = "$M2VMP2CUT_CMD_PATH/m2vfilter";

my $fileparts = "$M2VMP2CUT_CMD_PATH/fileparts";
my $filerotate = "$M2VMP2CUT_CMD_PATH/filerotate";

my $m2vmp2cut_sh = "$M2VMP2CUT_CMD_PATH/m2vmp2cut.sh";

sub showdoc($$)
{
    if ($_[0] != 0)
    {
	open STDOUT, ">&STDERR" || warn "1>&2 redirection failed: $! !!!\n";
    }

    system 'cat', "$M2VMP2CUT_CMD_PATH/../doc/$_[1]";

    exit $_[0];
}

#print $0, " <$toolpath> \n";

my $asr = -1;
my $nomux = 0;
my $author = 0;
my $aonly = 0;
my $stop = 0;
my $sync = 0;
my $test = -1;
my $last = 0;
my $evbr = 0;
my $requant = 0.0;
my $mplexopts = '-M -f 8';

my $dir = undef;
while (defined $ARGV[0] && $ARGV[0] =~ /^--/)
{
    $_ = $ARGV[0];
    $dir = $1, shift @ARGV, next if (/^--dir=(.*)/);
    $asr = $1 + 0, shift @ARGV, next if (/^--asr=(\d)\b/);
    $stop = 1, shift @ARGV, next if (/^--stop\b/);
    $nomux = 1, shift @ARGV, next if (/^--nomux\b/);
    $author = 1, shift @ARGV, next if (/^--author\b/);
    $aonly = 1, shift @ARGV, next if (/^--aonly\b/);
    $sync = $1 + 0, shift @ARGV, next if (/^--sync=(-?\d+)\b/);
    $test = $1 + 0, shift @ARGV, next if (/^--test=(\d+)\b/);
    $last = $1 + 0, shift @ARGV, next if (/^--last=(\d+)\b/);
    $evbr = $1 + 0, shift @ARGV, next if (/^--evbr=(\d+)\b/);
    $requant = $1 + 0.0, shift @ARGV, next if (/^--requant=([1-5].\d+)\b/);
    $mplexopts = $1, shift @ARGV, next if ($ARGV[0] =~ /^--mplexopts=(.*)/);
    showdoc 0, 'Options' if (/^--options\b/);
    showdoc 0, 'Examples' if (/^--examples\b/);

    my $rv = 0;
    $rv = 1, print STDERR
      "\n$0: $ARGV[0]: unknown or incorrectly used option.\n"
	unless ($ARGV[0] =~ /^--help\b/);
    showdoc $rv, 'Usage';
}

die 'Author option disabled -- how does this vob file="command..." work !!!',
    "\n" if $author;

$nomux = 1 if ($aonly);
$nomux = 1 if ($author);
$aonly = 0 if ($author);

unless (defined $dir) {
    showdoc 1, 'Usage' if (@ARGV < 1);
    $dir = shift @ARGV;
}

die "$dir: not a directory.\n", unless -d $dir;

# XXX add parsing of -v, -a, -s, -c, -i...
# symlinks...
my $vfile = "$dir/video.m2v";
my $afile = "$dir/audio.mp2";
my $sfile = 0; # "$dir/subtitles...";
my $cfile = "$dir/cutpoints";
my $ifile = "$dir/video.index";
my $jfile = "$dir/audio.scan";
my $range = 0;
my $workdir = "m2vmp2cut-work";

while (defined $ARGV[0] && $ARGV[0] =~ /^-/)
{
    $_ = shift @ARGV;
    $workdir = shift @ARGV, next if $_ eq '-d';
    $range = shift @ARGV, $cfile = 0, next if $_ eq '-r';
    $vfile = shift @ARGV,next if $_ eq '-v';
}

# XXX some cmdline error handling...

foreach ($vfile, $afile, $sfile, $cfile) {
    -r $_ or die "File $_ not readable\n" if $_;
}

# XXX move to a later place
# also maybe sub eprint($) { print STDERR $_[0]; } # + nl-version
print("Creating $ifile...\n"), system $m2vscan, $vfile, $ifile
  unless -r $ifile;
print("Creating $jfile...\n"), system $mp2cutpoints, '--scan', $afile, $jfile
  unless -r $jfile;

if ($range) {
    $_ = $range;
}
else {
    open I, '<', $cfile or die "Can not open '$cfile': $!\n";
    $_ = <I>;
    close I;
}

/(^|\s)((\d+-\d+)(,\d+-\d+)*)\s*$/ or die "Incorrect cutfile format\n";

my $framelist = $2; # print $framelist, "\n";

if ($last)
{
    my @tlist;

    foreach (split(',', $framelist))
    {
	my ($s, $e) = split('-');
	last if ($s + 0 >= $last);
	$e = $last if ($last < $e + 0);

	push(@tlist, "$s-$e");
    }
    $framelist = "@tlist";
}

if ($test > 0)
{
    $test = 50 if ($test < 50);

    my @tlist;
    foreach (split(',', $framelist))
    {
	my ($s, $e) = split('-');
	my ($ss, $ee) = ($s + $test, $e - $test);
	if ($ee > $ss) {
	    push(@tlist, "$s-$ss,$ee-$e");
	}
	else { push(@tlist, "$s-$e"); }

    }
    $framelist = "@tlist";
    $test = 1;
}

my @ranges;
my $pe = -1;
my $frames = 0;

foreach (split(',', $framelist))
{
    my ($s, $e) = split('-');

    $frames += $e - $s;

    next if ($s == $e);
    die "$s greater than $e in a range in $ARGV[0].\n" if ($s > $e);

    die "\nRange begin $s is less than previous range end $pe in $ARGV[0].\n",
      "Which is not currently supported (as usually this is not desired).\n\n"
	if ($s < $pe);

    if ($s == $pe)
    {
	my $pr = pop(@ranges);
	$pr =~ s/-.*/-$e/;
	push @ranges, $pr;
    }
    else { push @ranges, "$s-$e"; }

    $pe = $e;
}

undef $framelist;
undef $pe;

my $wd = "$dir/$workdir";

system 'rm', '-rf', "$wd.6";
rename "$wd.5", "$wd.6";
rename "$wd.4", "$wd.5";
rename "$wd.3", "$wd.4";
rename "$wd.2", "$wd.3";
rename "$wd.1", "$wd.2";
rename "$wd",   "$wd.1";

mkdir "$wd", 0755; # XXX rotate these (instead of content)

open STDERR, '|-', 'tee', "$wd/m2vmp2cut.log"
  || die "STDERR redirection failed !!!: $! !!!\n";

open STDIN, ">&STDOUT" || die "0>&1 redirection failed: $! !!!\n";

print STDERR "--- Opened '$wd/m2vmp2cut.log' for output.\n";
#  "--- Note that do.sh logs here only when run via this driver.\n";

open I, '<', $ifile || die "Can not open index file $ifile: $!\n";

# find last line from scan output file -- it contains average bit rate.

seek(I, -200, 2) || die "Seek failed: $!.\n";
my ($allframes, $avbr);
{
    my $l;
    $l = $_ while (<I>);
    ($l =~ /frames\s+(\d+)\s.*abr\s+(\d+)/)
      || die "Incorrect index file format.\n";
    $allframes = $1;
    $avbr = int($2 / 1000);
}

seek(I, 0, 0) || die "Seek failed: $!.\n";

my $estimate;
if ($test < 0) {
    my ($vsize, $asize) = ((stat $vfile)[7], (stat $afile)[7]);
    $estimate = int(($vsize + $asize) * $frames / $allframes * 1.033);
}

open SH, '>', "$wd/do.sh" || die "Can not create job script: $!\n";
chmod 0755, "$wd/do.sh";

## shbegin ##

if  ( -f '/bin/bash')   {  print SH "#!/bin/bash\n";  }
elsif ( -f '/bin/zsh')  {  print SH "#!/bin/zsh\n";   }
elsif ( -f '/bin/ksh')  {  print SH "#!/bin/ksh\n";   }
else {
    die "Can not find supported shell; /bin/bash, /bin/zsh nor /bin/ksh.\n";
}

print SH << "EOF";

# This program is licensed under the GPL v2. See file COPYING for details.

LANG=C LC_ALL=C; export LANG LC_ALL

die () { echo "\$@" >&2; exit 1; }

_fileparts () { \"$fileparts\" "\$@"; }
_filerotate () { \"$filerotate\" "\$@"; }
_somehdrinfo () { \"$somehdrinfo\" "\$@"; }
_m2vtoyuv () { \"$m2vtoyuv\" "\$@"; }
_catfiltered () { \"$catfiltered\" "\$@"; }

# mpeg2enc comes also from mjpegtools
command -v mplex >/dev/null ||
	die "'mplex' does not exist; please install mjpegtools."

test -d "$wd" || die "Directory '$wd' does not exist (changed working directory?)."
vfile="$vfile"
afile="$afile"
for f in "\$vfile" "\$afile"
do
	test -f \$f || die "File '\$f' does not exist."
done
EOF

my $RUNTIME_DIR = $ENV{XDG_RUNTIME_DIR} || 0;
unless ($RUNTIME_DIR and -d $RUNTIME_DIR
	and -x $RUNTIME_DIR and $RUNTIME_DIR !~ /\s/) {
    $RUNTIME_DIR = "/tmp/runtime-$<";
    mkdir $RUNTIME_DIR; # ignore if fail.
    chmod 0700, $RUNTIME_DIR or die "Cannot chown '$RUNTIME_DIR': $!\n";
}

print SH << "EOF";

rtwd=$RUNTIME_DIR/m2vmp2cut-$$

trap 'rm -rf \$rtwd; kill -USR1 0' 0 INT HUP TERM
rm -rf \$rtwd; mkdir \$rtwd;

'$m2vmp2cut_sh' . vermatch 6 \\\n\t|| die 'Tool version mismatch. Rerun!';

EOF

if (! $nomux || $author)
{
    print SH qq'rm -f "\$rtwd/fifo-video" "\$rtwd/fifo-audio"\n',
      qq'mkfifo "\$rtwd/fifo-video" "\$rtwd/fifo-audio"\n';
}
print SH << "EOF";

exec 0>&1

_filerotate "$wd" mpeg2enc.out mplex.out
rm -f "$wd"/out.*

#set -x; : '***' `date`

EOF

print SH "# average video bit rate: $avbr\n";
if ($evbr != 0) {
    print SH "# but using $evbr value given on m2vmp2cut command line.\n";
} else {
    $evbr = $avbr;
}
my $met = '';
$met = "-4 4 -2 4", $evbr = int($evbr * 1.2) if ($test >= 0);

print STDERR "video frames: @ranges.\n";
print SH "# video frames: @ranges\n";
print SH "\n(\n /bin/rm -f \"$wd\"/enctmp.*\n\n" unless ($aonly);

my ($prevgopff, $prevgoppos,
    $currgopff, $currgoppos,
    $nextgopff, $nextgoppos,
    $nextiframeoffset) = (0,0, 0,0, 0,0, 0);

my @asrs = (0,0,0,0,0,0,0,0,0,0);
sub lookx($$)
{
    my $frame = $_[0];
    my $getasr = $_[1];

    return if ($nextgopff >= $frame);

    while (<I>)
    {
	# Currently we can manage with information up to frame number.
	# format: (filepos) gop (frame) ts (asr) ...
	if (/^\s*(\d+)\s+\d+\s+(\d+)\s+(\d+)\s+(\d)\s/) {
	    ($prevgopff, $prevgoppos) = ($currgopff, $currgoppos);
	    ($currgopff, $currgoppos) = ($nextgopff, $nextgoppos);
	    ($nextgopff, $nextgoppos) = ($2, $1);
	    $nextiframeoffset = $3;

	    $asrs[$4] = $asrs[$4] + 1 if ($getasr);
	    return if ($2 >= $frame);
	}
	if (/^end:\s+size\s+(\d+)\s+frames\s+(\d+)\s/) {
	    ($prevgopff, $prevgoppos) = ($currgopff, $currgoppos);
	    ($currgopff, $currgoppos) = ($nextgopff, $nextgoppos);
	    ($nextgopff, $nextgoppos) = ($2, $1);
	    $nextiframeoffset = 0;
	    return;
	}
    }
}

my ($copystart, $enccopystart, $encframes) = (0, 0, 0);

my @m2v2yuvrange;

my $efc = 1;

sub enccmdblk($$$)
{
    my ($range, $frames, $firstoffset) = @_;

    print SH << "EOF";

 set x `_somehdrinfo $firstoffset "$vfile" "$wd/enctmp.matrix.$efc"`
 [ x\$3 = x0 ] && I='-I t' || I= # XXX bottom field first not handled (yet)
 [ x\$5 = x1 -o x\$7 = x1 ] && fa='file=$wd/enctmp.matrix.$efc' || fa=default
 [ x\${11} = x0 ] && dc= || dc="-D \${11}"

 { echo; echo $efc \$I $range $frames $evbr; echo; } >> "$wd/mpeg2enc.out"

 _m2vtoyuv -q \$I $range "$vfile" \\
	| mpeg2enc -f 3 -b $evbr $met -R 2 -K "\$fa" \$dc -s \\
	-o "$wd/enctmp.out.$efc.m2v" \\
	>> "$wd/mpeg2enc.out" 2>&1

 _catfiltered \$5 \$7 \$9 $frames "$wd/enctmp.out.$efc.m2v"

EOF
    $efc++;
}

sub begincut($)
{
    my $frame = $_[0];

    return if ($frame == 0); # copystart = 0

    $enccopystart = $prevgoppos;

    my $e = $nextgopff - $prevgopff + $nextiframeoffset;
    my $s = $frame - $prevgopff;

    $encframes += ($e - $s);

    push @m2v2yuvrange, "-sb $enccopystart -c $s-$e";

    $copystart = $nextgoppos;

    if ($test >= 0 && ($test++ & 1) == 0) {
	$frames -= $encframes;
    }
    else {
	enccmdblk("@m2v2yuvrange", $encframes, $enccopystart);
    }
}

sub endcut($)
{
    my $frame = $_[0];

    if ($currgoppos > $copystart)
    {
	print SH " : +\n";
	print SH " _fileparts -q --close-gop $copystart-$currgoppos \"\$vfile\"\n\n";
    }

    $enccopystart = $prevgoppos;

    my $e = $frame - $prevgopff;
    my $s = $currgopff - $prevgopff;
    $encframes = $e - $s;
    @m2v2yuvrange = ("-sb $enccopystart -c $s-$e");
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

die "aonly and test mutually exclusive\n" if ($aonly && $test >= 0);

my @timecodes;
$" = ' ';
my ($s, $e);
foreach (@ranges)
{
    ($s, $e) = split('-');
    my ($ts, $te);

    lookx($s + 1, 0);
    unless ($aonly)
    {
	print SH "# ======== $prevgopff $currgopff ($s) $nextgopff ",
	  "-- $prevgoppos $currgoppos $nextgoppos\n";
	$ts = $nextgopff;
	begincut($s);

	lookx($e, 1);
	print SH "# -------- $prevgopff $currgopff ($e) $nextgopff ",
	  "-- $prevgoppos $currgoppos $nextgoppos\n";
	endcut($e);
    }

    if ($test >= 0) {
	$s = $ts if $test & 1;
	$e = $currgopff unless $test & 1;
    }

    ($ts, $te) = (palframe2timecode($s,$sync), palframe2timecode($e,$sync));

    push @timecodes, "$ts-$te";
}

print SH "# ++++++++ $prevgopff $currgopff ($e) $nextgopff ",
	  "-- $prevgoppos $currgoppos $nextgoppos\n";

if ($test == 2) { # --test=0
    $frames -= $encframes;
}
else {
    enccmdblk("@m2v2yuvrange", $encframes, $enccopystart) unless ($aonly);
}
undef $s; undef $e;
$" = ',';

close I;

print STDERR "Related timecodes: @timecodes\n";

unless ($aonly)
{
    my ($maxasrval, $maxasr, $counter) = (0, 0, -1);
    print STDERR "Aspect ratios found (asr, count):\n";
    foreach (@asrs)
    {
	$counter++;
	next if ($_ == 0);
	print STDERR "  $counter, $_\n";
	if ($_ > $maxasrval)
	{
	    $maxasr = $counter;
	    $maxasrval = $_;
	}
    }
    print STDERR "Most common aspect: $maxasr ($maxasrval gops).\n";
    if ($asr >= 0)
    {
	print STDERR "... but using command line asr $asr.\n";
    }
    else { $asr = $maxasr; }

    undef $maxasrval; undef $maxasr; undef $counter;

    my $reqq = '';
    $reqq = "tcrequant -d 2 -f $requant |" if ($requant);

    print SH qq') | $reqq "$m2vfilter" $asr $frames ',
      $author? qq'"\$rtwd/fifo-video" &': $nomux? qq'"$wd/incomplete.m2v"': qq'"\$rtwd/fifo-video" &', "\n";

    undef $reqq;
}

print STDERR "Scanning audio for cut points.\n";
my $audiocutfilepos =
  qx($mp2cutpoints "@timecodes" "$afile" "$wd/audio.cuts" "$dir/audio.scan")
  || exit 1;

chomp($audiocutfilepos);
print SH "\n\n";
print SH "# audio timecodes: @timecodes,\n#\tcutfilepos: $audiocutfilepos\n";

print SH "\n_fileparts $audiocutfilepos";
print SH qq' "\$afile" > ', $author? qq'"\$rtwd/fifo-audio" &': $nomux? q'"$wd/incomplete.mp2\"': qq'"\$rtwd/fifo-audio" &', "\n";


unless ($nomux)
{
    print SH qq'\nmplex $mplexopts -o "$wd/incomplete.mpg" ',
	qq'"\$rtwd/fifo-video" "\$rtwd/fifo-audio" > "$wd/mplex.out" 2>&1\n';
}

if ($author)
{
    open XML, '>', "$wd/dvdauthor.xml" || die "Can not create dvdauthor xml control file: $!\n";
    chmod 0644, "$wd/dvdauthor.xml";

    print XML  <<"EOF";
    <dvdauthor dest=\"$wd/dvd\">
      <vmgm />
	<titleset>
	  <titles>
	    <pgc>
	    # XXX this is now borken -- \$rtwd cannot be fed there...
	      <vob file=\"mplex $mplexopts -o /dev/stdout \$rtwd/fifo-video \$rtwd/fifo-audio |\"/>
	    </pgc>
	  </titles>
	</titleset>
    </dvdauthor>
EOF

    print SH "\ndvdauthor -x \"$wd/dvdauthor.xml\" > \"$wd/dvdauthor.out\" 2>&1\n";
}

#
# Final lines.
#

print SH "\n", "wait\n", "rm -rf \$rtwd; trap - 0 INT HUP TERM\n";

unless ($author)
{
	if ($nomux) { print SH qq'mv -f "$wd/incomplete.m2v" "$wd/out.m2v"\n',
			qq'mv -f "$wd/incomplete.mp2" "$wd/out.mp2"\n'; }
	else { print SH qq'mv -f "$wd/incomplete.mpg" "$wd/out.mpg"\n'; }
}

#print SH <<"EOF";
#set +x
#ls -lrtac "$wd"
#echo "***" `date`
#EOF
if (! $nomux && $test < 0) {
    print SH "\n", 'numtune () { ', "\n";
    print SH q(    echo $1 | sed 's/\\(...\\)$/ \\1/; s/\\(...\\) / \\1 /; s/\\(...\\) / \\1 /'; }), "\n\n";
    print SH "echo; echo estimated size: `numtune $estimate` bytes\n";
    print SH "size=`ls -l \"$wd\"/out.mpg | awk '{ print \$5 }'`\n";
    print SH "echo target \"'$wd/out.mpg'\" size: `numtune \$size` bytes\n";
    print SH "difference=`expr $estimate - \$size | tr -d -`\n";
    print SH "echo difference: `numtune \$difference` bytes\n";
} elsif (! $author) {
#    print SH "ls -l \"$wd\"/out.*\n";
}

print SH "echo pictures: ", $frames, ", time: ", palframe2timecode($frames, 0),
  "\n\n";

close SH;

if ($stop)
{
    print "\nWork script '$wd/do.sh'\ncreated but not executed by request.\n";
    exit 0;
}

print STDERR "Running work script '$wd/do.sh'...\n";

#$SIG{'INT'} = 'sleep 1; exit 1';

#system 'script', '-c', "$wd/do.sh", "$wd/do.sh-out";
$SIG{'USR1'} = 'IGNORE';
my @cmd = ("$wd/do.sh", ''); # XXX added useless arg to lure perl not use sh...
if (system(@cmd) != 0)
{
    print "\n$wd/do.sh was partly unsuccessful. exiting.\n";
    select undef, undef, undef, 0.25;
    exit 1;
}

print "\nDone, Result (if any) in '$wd/",
  $author? "dvd": $nomux? "out.(m2v|mp2)": "out.mpg", "'\n\n";
