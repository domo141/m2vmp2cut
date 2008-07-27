#!/bin/sh
#
# Author: Tomi Ollila -- too ät iki fi
#
#	Copyright (c) 2004 Tomi Ollila
#	    All rights reserved
#
# This program is released under GNU GPL. Check
# http://www.fsf.org/licenses/licenses.html
# to get your own copy of the GPL license.

rm -rf /tmp/m2vmp2cut-$$
trap "rm -rf /tmp/m2vmp2cut-$$" 0 HUP INT TERM QUIT PIPE

SKIP=`awk '/^--8<----8<--/ { print NR + 1; exit 0; }' $0`

tail +$SKIP $0 | ( cd /tmp; pax -rzp e -s ":m2vmp2cut[^/]*:m2vmp2cut-$$:" )

/tmp/m2vmp2cut-$$/m2vmp2cut.pl "$@"

exit 0
--8<----8<----8<----8<----8<----8<----8<----8<----8<----8<----8<----8<--
