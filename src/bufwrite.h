/*
 * bufwrite.h
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sat Sep 25 16:03:14 EEST 2004 too
 * Last modified: Thu 25 Oct 2012 21:22:50 EEST too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#ifndef BUFWRITE_H
#define BUFWRITE_H

#ifndef X_H
#include "x.h"
#endif

typedef struct _BufWrite BufWrite;
struct _BufWrite
{
  /* all private */
  int fd;
  unsigned char * buf;
  int buflen;
  int len;
};

void bufwrite_init(BufWrite * bw, int fd, unsigned char * buf, int buflen);
bool bufwrite(BufWrite * bw, const unsigned char * buf, int len);
bool bufwrite_exit(BufWrite * bw);

#endif /* BUFWRITE_H */


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
