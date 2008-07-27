/*
 * embeds.h
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Wed Sep 29 18:12:27 EEST 2004 too
 * Last modified: Sat Oct 22 14:23:29 EEST 2005 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#ifndef EMBEDS_H
#define EMBEDS_H

#ifndef X_H
#include "x.h"
#endif

void run_m2vmp2cut(int argc, char ** argv) GCCATTR_NORETURN;

void builtin_lvev6frames(int argc, char ** argv) GCCATTR_NORETURN;
void builtin_mpg_somehdrinfo(int argc, char ** argv) GCCATTR_NORETURN;
void builtin_mpg_catfiltered(int argc, char ** argv) GCCATTR_NORETURN;

#endif /* EMBEDS_H */


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
