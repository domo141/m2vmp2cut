/*
 * zzob.h
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Fri Mar 04 21:09:14 EET 2005 too
 * Last modified: Mon May 05 17:13:47 EEST 2008 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */


#ifndef ZZOB_H
#define ZZOB_H

#include <sys/types.h> /* for off_t */

#ifndef X_H
#include "x.h"
#endif


typedef struct _ZeroZeroOneBuf ZeroZeroOneBuf;
struct _ZeroZeroOneBuf
{
#define uchar unsigned char
  /* public output variables -- read only (writing these will break things) */
  uchar * data;
  int	  len;
  off_t	  pos;
  /* private */
  struct { /* c_* variables constant after initialized */
    int	    c_fd;
    uchar * c_buf;	/* pointer to start of whole buffer area */
    int	    c_buflen;	/* bytes of whole buffer area */
    int	    len;	/* bytes of active data in buffer */
    off_t   maxread;	/* read this many bytes at maximum */
  } p;
#undef uchar
};

bool zzob_init(ZeroZeroOneBuf * zzob, int fd,
	       unsigned char * buf, int buflen, off_t maxread);
bool zzob_data(ZeroZeroOneBuf * zzob, bool locked);
int zzob_rest(ZeroZeroOneBuf * zzob, bool lastconsumed);

#endif /* ZZOB_H */

/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
