/*
 * $Id: showdoc.h $
 *
 * Author: Tomi Ollila  too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Mon Oct 25 21:03:52 EEST 2004 too
 * Last modified: Mon Oct 25 21:33:16 EEST 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#ifndef SHOWDOC_H
#define SHOWDOC_H

void builtin_showdoc(int argc, char ** argv) GCCATTR_NORETURN;
void show_usage(int argc, char ** argv) GCCATTR_NORETURN;

#endif /* SHOWDOC_H */
