/*
 * x.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sun Sep 26 09:07:06 EEST 2004 too
 * Last modified: Wed 18 Feb 2015 18:14:05 +0200 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DBGS 0
#include "x.h"

#if (__GNUC__ >= 4 && ! __clang__) // compiler warning avoidance nonsense
static inline void WUR(ssize_t x) { x = x; }
#else
#define WUR(x) x
#endif

/* should we use off_t here ? (functions read(2) and write(2) has size_t) */

ssize_t readfully(int fd, unsigned char * buf, ssize_t len)
{
  int tl = 0;

  if (len <= 0)
    return 0;

  while (true)
    {
      int l = read(fd, buf, len);

      if (l == len)
	return tl + l;

      if (l <= 0)
	{
	  if (errno == EINTR)
	    continue;
	  else
	    return tl;
	}
      tl += l;
      buf += l;
      len -= l;
    }
}

ssize_t writefully(int fd, const unsigned char * buf, ssize_t len)
{
  ssize_t slen = len;

  if (len <= 0)
    return 0;

  while (true)
    {
      int l = write(fd, buf, len);

      if (l == len)
	return slen;

      if (l <= 0)
	{
	  if (errno == EINTR)
	    continue;
	  else
	    return slen - len;
	}
      buf += l;
      len -= l;
    }
}

void fdprintf(int fd, const char * format, ...)
{
  char buf[1024];
  unsigned int n;
  va_list ap;

  va_start(ap, format);
  n = (unsigned int)vsnprintf(buf, sizeof buf, format, ap);
  va_end(ap);

  WUR(write(fd, buf, n >= sizeof buf? sizeof buf - 1: n));
}

void xerrf(const char * format, ...)
{
  va_list ap;
  int err = errno;

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);

  if (format[strlen(format) - 1] == ':')
    fprintf(stderr, " %s.\n", strerror(err));

  /* my_exit(1); */
  exit(1);
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
