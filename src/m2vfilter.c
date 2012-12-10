/*
 * m2vfilter.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sat Sep 25 16:35:59 EEST 2004 too
 * Last modified: Wed 24 Oct 2012 18:03:54 EEST too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdint.h>

#define DBGS 0
#include "x.h"
#include "zzob.h"
#include "bufwrite.h"

#include "ghdrs/m2vfilter_priv.h"

int main(int argc, char ** argv)
{
  ZeroZeroOneBuf zzob;
  unsigned char ibuf[65536];

  BufWrite bw;
  unsigned char obuf[65536];

  int asr;
  int frames;
  char * ofile;
  int fd;

  if (argc < 4)
    xerrf("Usage: %s asr frames ofile\n", argv[0]);

  asr = atoi(argv[1]);
  frames = atoi(argv[2]);
  ofile = argv[3];

  if (frames <= 0)
    xerrf("frames <= 0 (%d)\n", frames);

  if (asr <= 0 && argv[3][0] != '0')
    xerrf("asr incorrect ('%s')\n", argv[3]);

  if ((fd = open(ofile, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0)
    xerrf("open (write) failed:");

  bufwrite_init(&bw, fd, obuf, sizeof obuf);
  zzob_init(&zzob, 0, ibuf, sizeof ibuf, -1);

  s__thefilter(&zzob, &bw, asr, frames);
  return 0;
}

static void s__thefilter(ZeroZeroOneBuf * zzob, BufWrite * bw,
			 int asr, int frames) /* protoadd GCCATTR_NORETURN */
{
  int t1;

  int fcount = 0;
  int prevpc = 0;

  /* Ignore unknown aspect ratios... */
  if (asr > 4)
    asr = 0;
  else /* tune it to be already suitable... to write in. */
    asr <<= 4;

  while (zzob_data(zzob, false))
    {
      if (zzob->len >= 3)
	{
	  /*printf("%d\n", mpgbuf->data[3]); */
	  switch (zzob->data[3])
	    {
	    case 0x00: /* picture header */
	      fcount++;
	      t1 = fcount * 100 / frames;
	      if (t1 != prevpc)
		{
		  printf("\r- Picture %d of %d total (%d%%) ",
			 fcount, frames, fcount * 100 / frames);
		  fflush(stdout);
		  prevpc = t1;
		}
	      break;
	    case 0xb3:	/* sequence header */
	      if (asr > 0)
		if (zzob->len > 7)
		  zzob->data[7] = (zzob->data[7] & 0x0f) | asr;
	      break;
	    case 0xb5: /* sequence extension header */
	      if ((zzob->data[4] & 0xf0) == 0x20) /* sequence display ext. */
		{
		  static int video_format = -1;
		  if (video_format < 0)
		    video_format = zzob->data[4] & 0x0e;
		  else
		    zzob->data[4] = (zzob->data[4] & 0xf1) | video_format;
		}
	      break;
	    case 0xb8: /* GOP header */
	      if (zzob->len >= 7)
	      {
		int hour = fcount / (25 * 60 * 60);
		int min = (fcount / (25 * 60)) % 60;
		int sec = (fcount / 25) % 60;
		int frame = fcount % 25;

		zzob->data[4] = (zzob->data[4] & ~0x7f)
		  | (hour << 2) | (min >> 4);
		zzob->data[5] = (min << 4) | 0x08 | (sec >> 3);
		zzob->data[6] = (sec << 5) | (frame >> 1);
		zzob->data[7] = (zzob->data[7] & 0x7f) | (frame << 7);
	      }
	      break;
	    }
	}
      if (! bufwrite(bw, zzob->data, zzob->len))
	/* The value written below may not be exact */
	xerrf("mpg video data write failure after writing %jd bytes:",
	      (intmax_t)zzob->pos + zzob->len);
    }

  printf("\n");
  if (! bufwrite_exit(bw))
    /* The value written below may not be exact */
    xerrf("mpg video data write failure after writing %jd bytes:",
	  (intmax_t)zzob->pos);

  fprintf(stderr, "Transferred %jd bytes of mpeg video data.\n",
	  (intmax_t)zzob->pos);

  if (fcount == frames)
    fprintf(stderr, "Total frame count %d is what was expected.\n", fcount);
  else
    fprintf(stderr, "NOTE: total frame count %d does not match expected %d.\n",
	    fcount, frames);

  exit(0);
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
