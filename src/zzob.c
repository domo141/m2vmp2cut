#if 0 /*
 TRG=`basename $0 .c`; rm -f "$TRG" #"${TRG}_defs.hg"
 #trap 'rm -f "${TRG}_defs.hg"' 0; perl -x "$0" "$0" > "${TRG}_defs.hg"
 WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 FLAGS='-O2 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64' # -pg
 lastrun() { echo "$@"; exec "$@"; exit $?; }
 lastrun ${CC:-gcc} $WARN "$@" -o "$TRG" "$0" $FLAGS -DCDATE="\"`date`\""
 #*/
#endif
/*
 * zzob.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Thu Sep 23 19:28:18 EEST 2004 too
 * Last modified: Wed 19 Sep 2012 17:41:27 EEST too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */


#include <stdio.h> /* for NULL */
#include <string.h> /* for memmove() */
#include <unistd.h> /* for read() */

#define DBGS 0
#include "zzob.h"

#if 0
void __cyg_profile_func_enter(void *this_fn, void *call_site)
  __attribute ((no_instrument_function));
void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
  write(1, "enter\n", 6);
}
void __cyg_profile_func_exit(void *this_fn, void *call_site)
  __attribute ((no_instrument_function));
void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
  write(1, "exit\n", 5);
}
#endif

#if DBGS
static char * printzzob(ZeroZeroOneBuf * zb)
/* ; to avoid prototype generation */
{
  static char buf[128];
  sprintf(buf, "{%x, %d -> %d}", mb->data - mb->p.c_buf, mb->p.len, mb->len);
  return buf;
}
#define PMD printzzob(zzob)
#endif

#if TEST
static bool s__fillbuf(ZeroZeroOneBuf * zzob);
static unsigned char * s__memmem(const unsigned char * s, size_t l,
				 const unsigned char * b, size_t bl);
#endif

#include "ghdrs/zzob_priv.h"

static bool s__fillbuf(ZeroZeroOneBuf * zzob)
{
  int l;

  unsigned char * bufend = zzob->p.c_buf + zzob->p.c_buflen;
  unsigned char * rpos = zzob->data + zzob->p.len;

  D(1, ("s__fillbuf(%s)\n", PMD));

  if (rpos - bufend >= 0)
    {
      if (zzob->p.len == zzob->p.c_buflen)
	return false; /* raise BufferFullException */

      if (zzob->p.len)
	memmove(zzob->p.c_buf, zzob->data, zzob->p.len);
      zzob->data = zzob->p.c_buf;
      rpos = zzob->data + zzob->p.len;
    }
  int rlen = (bufend - rpos);
  if (rlen > zzob->p.maxread) {
    if (zzob->p.maxread == 0)
      return false;  /* raise EOFException */
    rlen = zzob->p.maxread;
  }

  l = read(zzob->p.c_fd, rpos, rlen /*> 3? 3: rlen */);
  if (l <= 0)
    return false; /* raise EOFException */

  zzob->p.len += l;
  zzob->p.maxread -= l;

  return true;
}

/* Used when \0\0\1 not found in first full buf... */
static bool s__zzob_init__searchmore(ZeroZeroOneBuf * zzob)
{
  if (zzob->p.len == zzob->p.c_buflen)
    {
      int i = zzob->p.len - 2;
      memcpy(zzob->p.c_buf, zzob->p.c_buf + i, 2);
      zzob->data = zzob->p.c_buf;
      zzob->pos += i;
      zzob->p.len = 2;
      return true;
    }
  else
    return false;
}

bool zzob_init(ZeroZeroOneBuf * zzob, int fd,
	       unsigned char * buf, int buflen, off_t maxread)
{
  unsigned char * data;

  zzob->p.c_fd     = fd;
  zzob->p.c_buf    = buf;
  zzob->p.c_buflen = buflen;

  zzob->p.len      = 0;

  if (maxread < 0) {
      maxread = 1; maxread <<= 8 * sizeof maxread - 1; maxread--; }
  zzob->p.maxread  = maxread;

  zzob->pos  = 0;
  zzob->data = buf;
  zzob->len  = 0;

  D(1, ("zzob_init(fd=%d, %s)\n", fd, PMD));

  while (true)
    {
      if (s__fillbuf(zzob))
	{
	  data = s__memmem(zzob->data, zzob->p.len,
			   (const unsigned char *)"\000\000\001", 3);
	  if (data != NULL)
	    {
	      zzob->pos += data - zzob->data;
	      zzob->p.len -= data - zzob->data;
	      zzob->data = data;
	      return true;
	    }
	}
      else
	if (!s__zzob_init__searchmore(zzob))
	  return false;
    }
}

bool zzob_data(ZeroZeroOneBuf * zzob, bool locked)
{
  unsigned char * p;
  int o;

  D(1, ("zzob_data(%s)\n", PMD));

  if (locked == false)
    {
      zzob->p.len -= zzob->len;
      zzob->data += zzob->len;
      zzob->pos += zzob->len;
      zzob->len = 0;
    }

  /* if (locked) outf("locked!\n"); */

  o = zzob->len + 3;

  while (true)
    {
      if (zzob->p.len - o > 0
	  && (p = s__memmem(zzob->data + o, zzob->p.len - o,
			    (const unsigned char *)"\000\000\001", 3)) != NULL)
	  break;

      if (! s__fillbuf(zzob))
      {
	D(1,
	  ("s__fillbuf(zzob, zzob->p.len) returned false (%s).\n", PMD));
	if (locked || zzob->p.len < 4)
	  return false;

	p = zzob->data + zzob->p.len;
	break;
      }
    }

  zzob->len = p - zzob->data;

  D(1, ("zzob_data end: %s\n", PMD));
  return true;
}

int zzob_rest(ZeroZeroOneBuf * zzob, bool lastconsumed)
{
  if (lastconsumed)
    {
      zzob->data += zzob->len;
      zzob->p.len -= zzob->len;
      zzob->pos += zzob->len;
    }
  zzob->len = zzob->p.len;
  return zzob->len;
}

/* --- util funcs --- */

static unsigned char * s__memmem(const unsigned char * s, size_t l,
				 const unsigned char * b, size_t bl)
{
  const unsigned char * ss = s;

  D(1, ("s__memmem(l = %d)\n", l));

  while ((s = memchr(ss, b[0], l)) != NULL)
    {
      l -= (s - ss);
      D(0, ("s - ss = %d, l = %d\n", s - ss, l));

      if (l < bl)
	return NULL;

      if (memcmp(s, b, bl) == 0)
	{
	  D(1, ("s\n"));
	  return (unsigned char *)s; /* unavoidable compiler warnings !!! */
	}
      ss = s + 1;
      l--;
    }
  return NULL;
}
