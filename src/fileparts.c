/*
 * fileparts.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sun Sep 26 08:51:22 EEST 2004 too
 * Last modified: 2008-08-17 20:03:32.000000000 +0200 (patch from Göran)
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define DBGS 0
#include "x.h"
#include "zzob.h"
#include "ghdrs/fileparts_priv.h"

int main(int argc, char ** argv)
{
  char * prgname = argv[0];
  char * partstring;
  char * ifile;
  int fd;
  char * p;
  off_t tlen = 0;
  bool quiet = false;
  bool closegop = false;

  while (argv[1] && argv[1][0] == '-')
    {
      if (strcmp(argv[1], "-q") == 0)
        quiet = true;
      if (strcmp(argv[1], "--close-gop") == 0)
        closegop = true;
      argc--; argv++;
    }

  if (argc < 3)
    xerrf("Usage: %s [(-q|--close-gop)] parts ifile\n", prgname);

  partstring = argv[1];
  ifile = argv[2];

  if (! s__chkpartstring(partstring))
    xerrf("fileparts: %s: incorrect string.\n", partstring);

#if 1
  if ((fd = open(ifile, O_RDONLY)) < 0)
    xerrf("open O_DRONLY failed:");
#endif
  p = partstring;
  while (true)
    {
      off_t start = atoll(p);
      off_t len;

#if 1
      if (lseek(fd, start, SEEK_SET) < 0)
	xerrf("lseek failed:");
#endif
      p = strchr(p, '-') + 1;
      len = atoll(p) - start;

#if 1
      if (closegop) {
	off_t olen = len;
        len = s__closegop(fd, 1, len);
	if (len < 0)
	  xerrf("Data copy failure:");
	tlen += (olen - len);
      }
      if (s__fdcopy(fd, 1, len) != len)
	xerrf("Data copy failure:");
#else
      printf("start %lld, len %lld\n", start, len);
#endif
      tlen += len;

      p = strchr(p, ',');
      if (p == NULL)
	break;
      p++;
    }

  if (! quiet)
    fprintf(stderr, "fileparts: Transferred %lld bytes of data.\n", tlen);

  return 0;
}

static bool s__chkpartstring(const char * partstring)
{
  const char * p = partstring;
  char c;
  int state;
  off_t start, end; /* unavoidable warnings here -- but don't suppress those !!*/

  /* using regexp /^(\d+-\d+)(,\d+-\d+)*$/) would have been easier. */

  for (state = 0, c = *p;   c;   p++, c = *p)
    switch (state)
      {
      case 0: /* [0-9] */
	if (isdigit(c)) { start = atoll(p); state = 1; }
	else		return false;
	break;

      case 1: /* [0-9]*-? */
	if (isdigit(c))	continue;
	if (c == '-')	state = 2;
	else		return false;
	break;

      case 2: /* [0-9] */
	if (isdigit(c)) { end = atoll(p); state = 3; }
	else		return false;
	break;

      case 3:  /* [0-9]*,? */
	if (isdigit(c)) continue;
	if (c == ',')	{ if (start > end) return false; state = 0; }
	else		return false;
      }

  if (state != 3)  return false;
  if (start > end) return false;

  return true;
}

static off_t s__fdcopy(int in, int out, off_t len)
{
  unsigned char megabuf[1024 * 1024];
  off_t tlen = 0;
  ssize_t fl;

  for(; len > 0; )
    {
      int l = read(in, megabuf, len > sizeof megabuf? sizeof megabuf: len);

      if (l <= 0)
	{
	  if (errno == EINTR)
	    continue;
	  return tlen;
	}

      fl = writefully(out, megabuf, l);
      tlen += fl;

      if (fl != l)
	break;

      len -= l;
    }

  return tlen;
}

static off_t s__closegop(int in, int out, off_t len)
{
  ZeroZeroOneBuf zzob;
  unsigned char ibuf[512 * 1024];
  unsigned char obuf[512 * 1024];
  int oadded = 0;
  int iframetsn = -1;
  int outputdata = 0;

  // maxread arg here, off_t arg
  zzob_init(&zzob, in, ibuf, sizeof ibuf, len);

  // need a limit.

  while (zzob_data(&zzob, false))
    {
      if (zzob.len >= 3)
        {
	  switch (zzob.data[3])
	    {
	    case 0x00: /* picture header */
              if (zzob.len < 7)
                xerrf("!!!\n");
              int frame_type = ((zzob.data[5] & 0x38) >> 3);
              int tsn = (zzob.data[4] << 2) + ((zzob.data[5] & 0xc0) >> 6);
              if (frame_type == 1) { /* i-frame */
                if (iframetsn < 0)
                  iframetsn = tsn;
              }
              else { /* not i-frame */
                if (iframetsn < 0)
                  xerrf("!!!\n");
              }
              if (tsn >= iframetsn) {
                outputdata = 1;
                tsn -= iframetsn;
                zzob.data[4] = (tsn >> 2);
                zzob.data[5] = (zzob.data[5] & 0x3f) | (tsn << 6);
              }
              else
                outputdata = 0;
	      break;
	    case 0xb3:	/* sequence header */
              if (iframetsn >= 0)
                goto breakwhile;
              outputdata = 1;
	      break;
	    case 0xb8: /* GOP header */
              outputdata = 1;
              break;
            }
          if (outputdata) {
            if (oadded + zzob.len >= sizeof obuf) {
              if (writefully(out, obuf, oadded) != oadded)
		return -1;
              oadded = 0;
            }
            memcpy(obuf + oadded, zzob.data, zzob.len);
            oadded += zzob.len;
          }
        }
    }
 breakwhile:
  if (oadded)
    if (writefully(out, obuf, oadded) != oadded)
      return -1;
  if (zzob_rest(&zzob, false))
    writefully(out, zzob.data, zzob.len);

  return len - (zzob.pos + zzob.len);
}

/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
