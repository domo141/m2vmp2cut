/*
 * mpgdata.h
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sat Sep 25 09:50:53 EEST 2004 too
 * Last modified: Thu Sep 30 18:34:15 EEST 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#ifndef MPGDATA_H
#define MPGDATA_H

#ifndef X_H
#include "x.h"
#endif

typedef struct _MpgBuf MpgBuf;
struct _MpgBuf
{
#define uchar unsigned char  
  /* public output variables */
  uchar * data;
  int     len;
  /* private */
  struct { /* c_* variables constant after initialized */
    int     c_fd;
    uchar * c_buf;
    int     c_buflen;
    uchar * start;
    int     len;
  } p;
#undef uchar  
};

bool mpgdata_init(MpgBuf * mpgbuf, int fd, unsigned char * buf, int len);
bool mpgdata(MpgBuf * mpgbuf);

#endif /* MPGDATA_H */


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
