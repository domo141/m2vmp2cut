#!/bin/sh
# $Id; chklibmpeg-051.sh $
#
# Author: Tomi Ollila <>
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Sun Jul 20 19:41:17 EEST 2008 too
# Last modified: Mon 10 Dec 2012 13:02:24 EET too

# In case your system has old version of libmpeg2 libraries,
# or it doesn't have it at all, please follow these instructions:
#
# Download libmpeg2-0.5.1.tar.gz (unless already done so)
# (http://libmpeg2.sourceforge.net/files/libmpeg2-0.5.1.tar.gz ...).
#
# Enter the path of libmpeg2-0.5.1.tar.gz as an argument for
# './tools/buildlibmpeg-051.sh' to build and install internal copy
# of libmpeg2 for use of some m2vmp2cut tools.

die () { echo "$@" >&2; exit 1; }

case $1 in '') die Usage: $0 rootdir ;; esac

test -d config \
	|| die "Directory 'config' missing, probably wrong current directory"

dn0=`cd \`dirname "$0" \`; pwd` bn0=`basename "$0"`

dir=`cd "$1"; pwd`/libmpeg2-0.5.1-bin

chkfile ()
{
	if test -f "$1"
	then return 0
	fi
	echo "'$1' missing (ignore unless building libpeg2 from source).">&2
	cut -d: -f2- >&2 <<.
	:
	:
	: This system lacks a recent enough libmpeg2 version (0.5.0 or newer).
	:
	: In order to build some m2vmp2cut tools the old (or nonexistent)
	: version is not good enough.
	:
	: Usually installing libmpeg2 development package provided in
	: distribution packages is enough.
	:
	: If there is no such package or the package is nonexistent, check
	: "$dn0/$bn0"
	: for more information.
	:
.
	exit 1
}

chkfile "$dir/include/mpeg2dec/mpeg2.h"
chkfile "$dir/include/mpeg2dec/mpeg2convert.h"

chkfile "$dir/lib/libmpeg2.a"
chkfile "$dir/lib/libmpeg2convert.a"

exec > config/mpeg2.conf

only=-I$dir/include/mpeg2dec
only="$only -L$dir/lib -lmpeg2"
echo mpeg2_only=\"$only\"
echo mpeg2_both=\"$only\ -lmpeg2convert\"
exit 0
