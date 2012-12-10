#!/bin/sh
# $Id; chklibmpeg.sh $
#
# Author: Tomi Ollila <>
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Sat May 03 22:23:46 EEST 2008 too
# Last modified: Mon 10 Dec 2012 13:02:20 EET too

set -eu

test -d config || mkdir config
rm -f config/mpeg2.conf

flags_only=`pkg-config --cflags --libs libmpeg2 || :`
flags_both=`pkg-config --cflags --libs libmpeg2 libmpeg2convert || :`

case $flags_both in
'')
	test -d /usr/include/mpeg2dec
	# this is showing its age -- /usr/lib is not so common anymore...
	test -f /usr/lib/libmpeg2.a -o -f /usr/lib/libmpeg2.so
	test -f /usr/lib/libmpeg2convert.a -o -f /usr/lib/libmpeg2convert.so
	flags_only='-I /usr/include/mpeg2dec -lmpeg2'
	flags_both='-I /usr/include/mpeg2dec -lmpeg2 -lmpeg2convert'
esac

trap 'rm -f tstlibmpeg.c tstlibmpeg' 0

cat > tstlibmpeg.c <<EOF
#include <stdint.h>
#include <mpeg2.h>
int main(void) { return !(MPEG2_RELEASE >= (5 << 8)); }
EOF

gcc -o tstlibmpeg tstlibmpeg.c $flags_both
./tstlibmpeg && {
	echo mpeg2_only=\"$flags_only\"
	echo mpeg2_both=\"$flags_both\"
} > config/mpeg2.conf
