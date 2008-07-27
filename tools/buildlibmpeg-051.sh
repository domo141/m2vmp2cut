#!/bin/sh
# $Id; buildlibmpeg-051.sh $
#
# Author: Tomi Ollila <>
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Sun Jul 20 18:15:19 EEST 2008 too
# Last modified: Fri Jul 25 11:56:34 EEST 2008 too

die () { echo; echo "$@"; echo; exit 1; } >&2;

x () { echo "$@"; "$@"; }


bd=`dirname "$0"`

# XXX doex '-ef'  exist in all 'test' commands ?
test . -ef "$bd/.." || die Current directory not `cd "$bd/.."; pwd`

libmpeg=libmpeg2-0.5.1
libmpegtgz=$libmpeg.tar.gz
libmpegsum=0f92c7454e58379b4a5a378485bbd8ef

case $1 in '') die Usage: $0 "<path/to/$libmpegtgz>" ;; esac

libmpegtgz=$1


chkfile () { test -f "$1" || die "File '$1' missing"; }
chkdir () { test -d "$1" || die "Directory '$1' missing"; }

chkfile $libmpegtgz

case `md5sum $libmpegtgz` in
    ${libmpegsum}' '*) ;;
    *) die File "'"$libmpegtgz"'" checksum mismatch '(wrong or broken file?)'
esac

test -d $libmpeg-bin && die "Directory '$libmpeg-bin' is on the way"
test -d $libmpeg-work && die "Directory '$libmpeg-work' is on the way"
mkdir $libmpeg-work || die "Could not create directory '$libmpeg-work'"

prefix=`pwd`/$libmpeg-bin
trap "rm -rf $libmpeg-work" 0
(
  cd $libmpeg-work || die "Could not chdir to '$libmpeg-work'"
  x tar zxf ../$libmpegtgz
  x cd $libmpeg || die "Could not chdir to '$libmpeg-work/$libmpeg'"

  x ./configure --prefix=$prefix --disable-shared || die Configure failed
  x make install
  echo
  echo Now you need to rerun "'make'"
  echo
)


