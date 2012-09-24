/*
 * bufwrite.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sat Sep 25 16:02:07 EEST 2004 too
 * Last modified: Tue Nov 02 17:44:42 EET 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <unistd.h>
#include <string.h>
#include <errno.h>

#define DBGS 0
#include "x.h"
#include "bufwrite.h"
#include "ghdrs/bufwrite_priv.h"


void bufwrite_init(BufWrite * bw, int fd, unsigned char * buf, int buflen)
{
  bw->fd = fd;
  bw->buf = buf;
  bw->buflen = buflen;
  bw->len = 0;
}

bool bufwrite(BufWrite * bw, unsigned char * buf, int len)
{
  if (len > bw->buflen - bw->len)
    {
      if (writefully(bw->fd, bw->buf, bw->len) != bw->len)
	return false;

      if (len > bw->buflen)
	{
	  if (writefully(bw->fd, buf, len) != len)
	    return false;
	  bw->len = 0;
	}
      else
	{
	  memcpy(bw->buf, buf, len);
	  bw->len = len;
	}
    }
  else
    {
      memcpy(bw->buf + bw->len, buf, len);
      bw->len += len;
    }

  return true;
}

bool bufwrite_exit(BufWrite * bw)
{
  return writefully(bw->fd, bw->buf, bw->len) == bw->len;
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
