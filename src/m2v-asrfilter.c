#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -eu; trg=`basename "$0" .c`; rm -f "$trg"
 WARN="-Wall -Wno-long-long -Wstrict-prototypes -pedantic"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 FLAGS='-O2 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64' # -pg
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN $FLAGS "$@" -o "$trg" "$0"
 exit $?
 */
#endif
/*
 * $Id: m2v-asrfilter.c $
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Thu May 05 15:23:43 EEST 2005 too
 * Last modified: Wed 24 Oct 2012 17:46:14 EEST too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

/*
 * To compile this, enter `sh m2v-asrfilter.c'.
 */

/*
 * See http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html
 *
 * Thanks also to mpeg2enc, bbinfo, mpegcat, python_mpeg and so on...
 */

#include <stdio.h> /* for NULL */
#include <string.h> /* for memmove() */
#include <unistd.h> /* for read() */
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if 0
#define ud(x) typedef unsigned char u ## x
ud(1); ud(2); ud(3); ud(4); ud(5); ud(6); ud(7); ud(8);
#undef ud
#endif

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

#define DBGS 0

#define D(x, y)

typedef enum { false = 0, true = 1 } bool;

#if (__GNUC__ >= 3)
#define GCCATTR_PRINTF(m, n) __attribute__ ((format (printf, m, n)))
#define GCCATTR_UNUSED   __attribute ((unused))
#define GCCATTR_NORETURN __attribute ((noreturn))
#define GCCATTR_CONST    __attribute ((const))
#else
#define GCCATTR_PRINTF(m, n)
#define GCCATTR_UNUSED
#define GCCATTR_NORETURN
#define GCCATTR_CONST
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
    } p;
#undef uchar
};

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

static bool s__fillbuf(ZeroZeroOneBuf * zzob);
static unsigned char * s__memmem(const unsigned char * s, size_t l,
				 const unsigned char * b, size_t bl);

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
    l = read(zzob->p.c_fd, rpos, bufend - rpos /*> 3? 3: bufend - rpos*/);
    if (l <= 0)
	return false; /* raise EOFException */

    zzob->p.len += l;
    return true;
}

/* Used when \0\0\1 not found in first full buf... */
static bool zzob_init__searchmore(ZeroZeroOneBuf * zzob)
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

bool zzob_init(ZeroZeroOneBuf * zzob, int fd, unsigned char * buf, int buflen)
{
    unsigned char * data;

    zzob->p.c_fd     = fd;
    zzob->p.c_buf    = buf;
    zzob->p.c_buflen = buflen;

    zzob->p.len      = 0;

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
	    if (!zzob_init__searchmore(zzob))
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
	if (zzob->p.len - o > 0 &&
	    (p = s__memmem(zzob->data + o, zzob->p.len - o,
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



/* --*-- */

int main(int argc, char ** argv)
{
    ZeroZeroOneBuf zzob;
    unsigned char buf[65536];
    int asrcode;

    /* put stdout in fully buffered mode, we fflush() when needed */
    setvbuf(stdout, NULL, _IOFBF, 0);

    if (argc != 2 || (asrcode = atoi(argv[1])) < 1 || asrcode > 3)
    {
	fprintf(stderr, "Usage: %s (1, 2 or 3)\n", argv[0]);
	return 1;
    }

    if (! zzob_init(&zzob, 0, buf, sizeof buf))
	return 1;

    for (; zzob_data(&zzob, false); )
    {
	if (zzob.data[3] == 0xb3 && zzob.len > 7 /* XXX */)
	    zzob.data[7] = (zzob.data[7] & 0x0f) + asrcode * 0x10;

	fwrite(zzob.data, zzob.len, 1, stdout);
    }

    return 0;
}
