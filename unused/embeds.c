/*
 * embeds.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Wed Sep 29 17:59:24 EEST 2004 too
 * Last modified: Sun Oct 23 08:40:11 EEST 2005 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define DBGS 0
#include "x.h"

#include "ghdrs/m2vmp2cut.pl.h"
#include "ghdrs/lvev6frames.pl.h"
#include "ghdrs/mpg_somehdrinfo.py.h"
#include "ghdrs/mpg_catfiltered.py.h"

#include "subprog.h"

#include "embeds.h"
#include "ghdrs/embeds_priv.h"

void run_m2vmp2cut(int argc, char ** argv)
{
  launch_write_close_setsid_and_exit
    ( m2vmp2cut_pl_code, sizeof m2vmp2cut_pl_code, argc - 1, argv + 1,
				     "perl", "-", argv[0], NULL );
}


void builtin_lvev6frames(int argc, char ** argv)
{
  launch_write_close_setsid_and_exit(lvev6frames_pl_code,
				     sizeof lvev6frames_pl_code,
				     argc - 3, argv + 3, "perl", "-", NULL);
}

void builtin_mpg_somehdrinfo(int argc, char ** argv)
{
  launch_write_close_setsid_and_exit(mpg_somehdrinfo_py_code,
				     sizeof mpg_somehdrinfo_py_code,
				     argc - 3, argv + 3, "python", "-", NULL);
}

void builtin_mpg_catfiltered(int argc, char ** argv)
{
  launch_write_close_setsid_and_exit(mpg_catfiltered_py_code,
				     sizeof mpg_catfiltered_py_code,
				     argc - 3, argv + 3, "python", "-", NULL);
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
