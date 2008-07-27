#!/bin/sh
# $Id; buildlibmpeg-trunk.sh $
#
# Author: Tomi Ollila <>
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Sun May 04 09:44:18 EEST 2008 too
# Last modified: Sun Jul 20 18:14:44 EEST 2008 too

die () { echo "$@" >&2; exit 1; }

x () { echo "$@"; "$@"; }

bd=`dirname "$0"`

rev=HEAD

chkfile () { test -f "$1" || die "File '$1' missing"; }
chkdir () { test -d "$1" || die "Directory '$1' missing"; }

case $1 in
	'') die Usage: $0 dirprefix ;;
	*' '*) die Dirprefix "'$1'" has spaces ;;
esac

test -d libmpeg-svn-trunk || {
	test -f libmpeg-svn-trunk.sfx.sh \
		&& x sh libmpeg-svn-trunk.sfx.sh \
		|| x svn checkout -r $rev \
			svn://svn.videolan.org/libmpeg2/trunk libmpeg-svn-trunk
	chkdir libmpeg-svn-trunk
	test -f libmpeg-svn-trunk.sfx.sh || $bd/xdist.pl libmpeg-svn-trunk
}

x cd libmpeg-svn-trunk || die ...

writeconfig () {
	only=-I$1/libmpeg-svn-trunk/inst/include/mpeg2dec
	only="$only -L$1/libmpeg-svn-trunk/inst/lib -lmpeg2"
	echo mpeg2_only=\"$only\"
	echo mpeg2_both=\"$only\ -lmpeg2convert\"
	exit 0
} > ../config/mpeg2.conf

test -f inst/include/mpeg2dec/mpeg2.h && \
test -f inst/include/mpeg2dec/mpeg2convert.h && \
test -f inst/lib/libmpeg2.a && \
test -f inst/lib/libmpeg2convert.a && writeconfig "$1"

test -f configure || x ./bootstrap || die "'bootstrap'" failed.

x ./configure --prefix=`pwd`/inst || die "'configure'" failed.

x make install

chkfile inst/include/mpeg2dec/mpeg2.h
chkfile inst/include/mpeg2dec/mpeg2convert.h
chkfile inst/lib/libmpeg2.a
chkfile inst/lib/libmpeg2convert.a

writeconfig "$1"

exit 0
