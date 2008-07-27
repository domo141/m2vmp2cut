/*
 * $Id: showdoc.c $
 *
 * Author: Tomi Ollila  too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Mon Oct 25 20:44:04 EEST 2004 too
 * Last modified: Tue Oct 26 22:59:48 EEST 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <stdio.h>
#include <string.h>

#define DBGS 0
#include "x.h"
#include "showdoc.h"
#include "ghdrs/showdoc_priv.h"

#include "ghdrs/Usage.txtgz.h"
#include "ghdrs/Options.txtgz.h"
#include "ghdrs/Examples.txtgz.h"

#include "subprog.h"

void s__show_usage(char * pname)
{
  char buf[256];
  
  snprintf(buf, sizeof buf, "gzip -dc | exec sed 's|$0|%s|g'", pname);

  launch_write_close_setsid_and_exit(Usage_code, sizeof Usage_code,
				     0, NULL, "/bin/sh", "-c", buf, NULL);
}

void builtin_showdoc(int argc, char ** argv)
{
  const unsigned char * doc;
  int doclen;
  
  if (argc < 4)
    xerrf("argc < 4\n");

  if (strcmp(argv[3], "Usage") == 0)
    s__show_usage(argv[0]);

  else if (strcmp(argv[3], "Options") == 0)
    {
      doc = Options_code; doclen = sizeof Options_code;
    }
  else if (strcmp(argv[3], "Examples") == 0)
    {
      doc = Examples_code; doclen = sizeof Examples_code;
    }
  else
    xerrf("%s: unknown doc\n", argv[3]);

  launch_write_close_setsid_and_exit(doc, doclen,
				     0, NULL, "/bin/sh", "-c", 
				     "gzip -dc | exec ${PAGER:-more}", NULL);
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
