/*
 * filerotate.h
 *
 * Author: Tomi Ollila  too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Thu Oct 21 00:02:30 EEST 2004 too
 * Last modified: Thu Oct 21 00:04:06 EEST 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#ifndef FILEROTATE_H
#define FILEROTATE_H

#ifndef X_H
#include "x.h"
#endif

void builtin_filerotate(int argc, char ** argv) GCCATTR_NORETURN;

#endif /* FILEROTATE_H */


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
