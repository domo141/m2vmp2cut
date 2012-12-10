/*
 * filerotate.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Wed Oct 20 23:26:55 EEST 2004 too
 * Last modified: Wed 24 Oct 2012 18:01:51 EEST too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#define DBGS 0
#include "x.h"
#include "ghdrs/filerotate_priv.h"

int main(int argc, char ** argv)
{
  char fn[64];
  char nl[64];
  char ho[64];
  DIR * dir;
  struct dirent * de;
  int i;

  if (argc < 3)
    xerrf("Usage: %s <directory> <filenames...>\n", argv[0]);

  if (chdir(argv[1]) < 0)
    xerrf("chdir('%s') failed:", argv[1]);

  dir = opendir(".");

  if (dir == NULL)
    xerrf("opendir('.') failed:");

  argc -= 2;
  argv += 2;
  if (argc > 64)
    argc = 64;

  for (i = 0; i < argc; i++)
    {
      fn[i] = 0;
      nl[i] = strlen(argv[i]);
      ho[i] = 0;
    }

  while ((de = readdir(dir)) != NULL)
    {
      for (i = 0; i < argc; i++)
	{
	  int j = nl[i];
	  int c;
	  if (strncmp(de->d_name, argv[i], j) == 0
	      && de->d_name[j] == '.'
	      && (c = de->d_name[j + 1]) > '0' && c <= '8'
	      && de->d_name[j + 2] == '\0')
	    {
	      if (c == '1')
		ho[i] = 1;
	      if (c - '0' > fn[i])
		fn[i] = c - '0';
	    }
	}
    }

  for (i = 0; i < argc; i++)
    {
      int j;
      char b1[64];
      char b2[64];

      if (ho[i])
	for (j = fn[i]; j > 0; j--)
	  {
	    snprintf(b1, sizeof b1, "%s.%d", argv[i], j);
	    snprintf(b2, sizeof b2, "%s.%d", argv[i], j + 1);
	    rename(b1, b2);
	  }

      snprintf(b2, sizeof b2, "%s.1", argv[i]);
      rename(argv[i], b2);
    }

  return 0;
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
