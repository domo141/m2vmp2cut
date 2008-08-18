#!/bin/sh
# $Id; relhelper.sh $
#
# Created: Mon Jul 28 14:51:44 EEST 2008 too
# Last modified: Mon Jul 28 14:54:29 EEST 2008 too

case $1 in '') cat <<.

Files to edit for release: ANNOUNCE INSTALL README VERSION
	Makefile when switching between -dev and nodev versions.

checklist:
	vermatch running...

.
exit 0 ;; esac
