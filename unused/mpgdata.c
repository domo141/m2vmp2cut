#if 0 /* -*- mode: c; c-file-style: "gnu"; tab-width: 8; -*-
 NAME=`basename $0 .c` DATE=`date` TRG=$NAME
 [ x"$1" = xclean ] && { set -x; exec rm -f $TRG; }
 [ x"$1" = x ] && set -- -s -O2
 rm $TRG
 set -x; exec ${CC:-gcc} -Wall "$@" -o $TRG $NAME.c -DDATE="\"$DATE\"" -DTEST
 #*/
#endif
/*
 * mpgdata.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Thu Sep 23 19:28:18 EEST 2004 too
 * Last modified: Fri Oct 01 15:01:59 EEST 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <stdio.h> /* for NULL */
#include <string.h> /* for memmove() */
#include <unistd.h> /* for read() */

#define DBGS 0
#include "mpgdata.h"

#if DBGS
static char * printmpgbuf(MpgBuf * mb) /* ; to avoid prototype generation */
{
  static char buf[128];
  sprintf(buf, "{%x, %d -> %d}", mb->p.start - mb->p.c_buf, mb->p.len,mb->len);
  return buf;
}
#define PMD printmpgbuf(mpgbuf)
#endif

#if TEST
static bool s__fillbuf(MpgBuf * mpgbuf, int rest);
static char * s__memstr(const char * s, int l, const char * b, int bl);
#else
#include "ghdrs/mpgdata_priv.h"
#endif

/* This is pretty easy to do after python implementation in m2vscan.py */

bool mpgdata_init(MpgBuf * mpgbuf, int fd, unsigned char * buf, int buflen)
{
  mpgbuf->p.c_fd     = fd;
  mpgbuf->p.c_buf    = buf;
  mpgbuf->p.c_buflen = buflen;

  mpgbuf->p.start    = buf;
  mpgbuf->p.len      = 0;

  mpgbuf->data = buf;
  mpgbuf->len = 0;
  
  D(1, ("mpgdata_init(fd=%d, %s)\n", fd, PMD));
  
  while (true)
    {
      if (s__fillbuf(mpgbuf, mpgbuf->p.len))
	{
	  mpgbuf->data = s__memstr(mpgbuf->p.start,
				   mpgbuf->p.len,
				   (const unsigned char *)"\000\000\001", 3);
	  if (mpgbuf->data != NULL)
	    return true;
	}
      else
	return false;
    }
}

static bool s__fillbuf(MpgBuf * mpgbuf, int rest)
{
  int l;

  D(1, ("s__fillbuf(%s, rest = %d)\n", PMD, rest));

  if (rest > 0)
    {
      int nr = mpgbuf->p.c_buflen - rest;
      if (nr <= 0)
	return false;

      if (mpgbuf->p.c_buf != mpgbuf->p.start)
	memmove(mpgbuf->p.c_buf, mpgbuf->p.start, rest);

      l = read(mpgbuf->p.c_fd, mpgbuf->p.c_buf + rest, nr);
      D(1, ("fillbuf read %d bytes from fd %d.\n", l, mpgbuf->p.c_fd));
      if (l <= 0)
	{
	  mpgbuf->p.len = rest;
	  mpgbuf->p.start = mpgbuf->p.c_buf;
	  return false;
	}
    }
  else
    {
      l = read(mpgbuf->p.c_fd, mpgbuf->p.c_buf, mpgbuf->p.c_buflen);
      D(1, ("fillbuf read %d bytes from fd %d.\n", l, mpgbuf->p.c_fd));
      if (l <= 0)
	{
	  mpgbuf->p.len = rest;
	  mpgbuf->p.start = mpgbuf->p.c_buf;
	  return false;
	}
    }
  mpgbuf->p.len = rest + l;
  mpgbuf->p.start = mpgbuf->p.c_buf;

  return true;
}

bool mpgdata(MpgBuf * mpgbuf)
{
  unsigned char * p;
  
  D(1, ("mpgdata(%s)\n", PMD));

  while (true)
    {
      if ((p = s__memstr(mpgbuf->p.start + 3, mpgbuf->p.len - 3,
			 (const unsigned char *)"\000\000\001", 3)) != NULL)
	break;

      if (! s__fillbuf(mpgbuf, mpgbuf->p.len))
	{
	  D(1,
	    ("s__fillbuf(mpgbuf, mpgbuf->p.len) returned false (%s).\n", PMD));
	  if (mpgbuf->p.len)
	    {
	      p = mpgbuf->p.start + mpgbuf->p.len;
	      break;
	    }
	  return false;
	}
    }

  mpgbuf->data = mpgbuf->p.start;
  mpgbuf->len = p - mpgbuf->p.start;
  mpgbuf->p.start = p;
  mpgbuf->p.len -= mpgbuf->len;
  
  D(1, ("mpgdata end: %s\n", PMD));
  return true;
      
}

/* --- util funcs --- */

static unsigned char * s__memstr(const unsigned char * s, int l,
				 const unsigned char * b, int bl)
{
  const unsigned char * ss = s;

  D(1, ("s__memstr(l = %d)\n", l));

  while ((s = memchr(ss, b[0], l)) != NULL)
    {
      l -= (s - ss);
      D(0, ("s - ss = %d, l = %d\n", s - ss, l));
      
      if (l < bl)
	return NULL;
      
      if (memcmp(s, b, bl) == 0)
	{
	  D(1, ("s\n"));
	  return (unsigned char *)s; /* unavoidable compiler warnings ??? */
	}
      ss = s + 1;
      l--;
    }
  return NULL;
}


#if TEST

int main(int argc, char ** argv)
{
  MpgBuf mpgbuf;
  char buf[65536];

  setlinebuf(stdout);
  
  if (! mpgdata_init(&mpgbuf, 0, buf, sizeof buf))
    return 1;

  while (mpgdata(&mpgbuf))
    {
      write(1, mpgbuf.data, mpgbuf.len);
      continue;
      
      if (1 && mpgbuf.data[3] < 128)  continue;
      if (0 && mpgbuf.data[3] != 0xb3) continue;
      
      printf("%02x %02x %02x %02x\n",
	     mpgbuf.data[0], mpgbuf.data[1], mpgbuf.data[2], mpgbuf.data[3]);
    }

  return 0;
}  

#endif


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
