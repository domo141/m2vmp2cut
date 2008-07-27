/*
 * m2vmp2cut.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sat Sep 25 09:11:08 EEST 2004 too
 * Last modified: Sun Oct 23 09:17:45 EEST 2005 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBGS 0
#include "x.h"

#include "ghdrs/m2vmp2cut_priv.h"

/* includes for builtin modules */
#include "m2vscan.h"
#include "mpgfilter.h"
#include "fileparts.h"
#include "mp2cutpoints.h"
#include "filerotate.h"
#include "showdoc.h"
#if ! MINI
#include "embeds.h"
#include "ghdrs/version.h"
#endif


void main(int argc, char ** argv) GCCATTR_NORETURN;
void main(int argc, char ** argv)
{
  if (argc >= 2 && strcmp(argv[1], "builtin") == 0)
    s__builtin(argc, argv);

#if MINI
  xerrf("%s does only `builtin' commands.\n", argv[0]);
#else
  WriteCS(1, "\n*** "); WriteCS(1, version);
  WriteCS(1, "\n"
	  "*** This program is licensed under the GNU Public License v2.\n");

  if (argc < 2)
    {
      WriteCS(2, "\nEnter `");
      write(2, argv[0], strlen(argv[0]));
      WriteCS(2, " --help' for more information.\n\n");
      exit(1);
    }

  WriteCS(1, "*** ...running embedded m2vmp2cut perl driver program...\n");
  run_m2vmp2cut(argc, argv);
#endif
}


static void s__builtin(int argc, char ** argv) /* protoadd GCCATTR_NORETURN */
{
  if (argc >= 3)
    {
#if ! MINI
      if (strcmp(argv[2], "mpg_somehdrinfo.py") == 0)
	builtin_mpg_somehdrinfo(argc, argv);

      if (strcmp(argv[2], "mpg_catfiltered.py") == 0)
	builtin_mpg_catfiltered(argc, argv);

      if (strcmp(argv[2], "lvev6frames.pl") == 0)
	builtin_lvev6frames(argc, argv);
#endif
      if (strcmp(argv[2], "m2vscan") == 0)
	builtin_m2vscan(argc, argv);

      if (strcmp(argv[2], "mpgfilter") == 0)
	builtin_mpgfilter(argc, argv);

      if (strcmp(argv[2], "fileparts") == 0)
	builtin_fileparts(argc, argv);

      if (strcmp(argv[2], "mp2cutpoints") == 0)
	builtin_mp2cutpoints(argc, argv);

      if (strcmp(argv[2], "filerotate") == 0)
	builtin_filerotate(argc, argv);

      if (strcmp(argv[2], "vermatch") == 0)
	s__builtin_vermatch(argc, argv);

      if (strcmp(argv[2], "showdoc") == 0)
	builtin_showdoc(argc, argv);
    }

  xerrf("%s: Unknown builtin command `%s'.\n", argv[0], argv[2]);
}

static void s__builtin_vermatch(int argc, char ** argv)
{
  if (argc == 4 && strcmp(argv[3], "3") == 0)
    exit(0);

  WriteCS(2, "\n!!! m2vmp2cut internal version mismatch !!!\n\n");
  exit(1);
}

/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
