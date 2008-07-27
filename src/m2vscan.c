/*
 * m2vscan.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sun Sep 26 13:23:29 EEST 2004 too
 * Last modified: Fri May 09 20:01:42 EEST 2008 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DBGS 0
#include "x.h"
#include "zzob.h"
#include "bufwrite.h"

#include "ghdrs/m2vscan_priv.h"

int main(int argc, char ** argv)
{
  char * ifile;
  char * ofile;
  int ifd, ofd;
  struct stat st;

  if (argc < 3) // XXX to be fixed
    xerrf("argc < 3\n");

  ifile = argv[1];
  ofile = argv[2];

  if ((ifd = open(ifile, O_RDONLY)) < 0)
    xerrf("open O_DRONLY failed:");

  if (fstat(ifd, &st) < 0)
    xerrf("fstat failed:");

  if ((ofd = open(ofile, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0)
    xerrf("open (for write) failed:");

  s__scan(ifd, ofd, st.st_size);
  return 0;
}

static void s__scan(int ifd, int ofd,
		    off_t fsize) /* protoadd GCCATTR_NORETURN */
{
  ZeroZeroOneBuf zzob;
  unsigned char ibuf[65536];

  FILE * ofh = fdopen(ofd, "w");

  int pictures = 0;
  int gops = -1;
  int goppic = 0;
  off_t seqpos = 0;
  int asr = 0;
  int maxwidth = 0, maxheight = 0;

  /* should put all other vars to their structs too */
  struct {
    int hour, minute, second, frame, closed, time;
  } gop = { 0, 0, 0, 0, 0, 0 };
  int printgopinfo = 0;

  int prevpc = 0;

  if (ofh == NULL)
    xerrf("fdopen \"w\" failed:");

  zzob_init(&zzob, ifd, ibuf, sizeof ibuf, -1);

  while (zzob_data(&zzob, false))
    {
      int cpc = zzob.pos * 100 / fsize;

      if (cpc != prevpc)
	{
	  printf("\r- Scanning video at %lld of %lld (%d%%) ",
		 zzob.pos, fsize, cpc);
	  fflush(stdout);
	  prevpc = cpc;
	}

      if (zzob.len >= 3)
	{
	  unsigned char * data = zzob.data; /*printf("%d\n", data[3]); */

	  switch (data[3])
	    {
	    case 0xb3:	/* sequence header */
	      seqpos = zzob.pos;
	      if (zzob.len > 7)
		{
		  int frame_rate = data[7] & 0x0f;
		  int width = (data[4] << 4) + (data[5] >> 4);
		  int height = ((data[5] & 0x0f) << 8) + data[6];

		  if (frame_rate != 3)
		    {
		      xerrf("\rCurrently we can only support PAL (25fps) "
			    "frame rate.\n");
		    }
		  asr = data[7] >> 4;
		  if (width > maxwidth) maxwidth = width;
		  if (height > maxheight) maxheight = height;

		}
	      break;

	    case 0xb8: /* group of pictures (GOP) header */
	      if (printgopinfo) {
		/* XXX currently a problem */
		fprintf(stderr,"\nZero picture gop at filepos %lld..., ",
			zzob.pos);
		exit(1);
	      }

	      if (zzob.len > 7)
		{
                  /* int drop_frame_flag = data[4] & 0x80; */
                  gop.hour   = (data[4] & 0x7f) >> 2;
                  gop.minute = ((data[4] & 0x03) << 4) + (data[5] >> 4);
                  gop.second = ((data[5] & 0x07) << 3) + (data[6] >> 5);
                  gop.frame  = ((data[6] & 0x1f) << 1) + (data[7] >> 7);
                  gop.closed = data[7] & 0x40;
		  /* int broken_gop_flag = data[7] & 0x20; */

		  gop.time = (((gop.hour * 60 + gop.minute) * 60 + gop.second) * 25)
		    + gop.frame;
		  if (pictures)
		    fprintf(ofh, "%d\n", pictures - goppic);
		  else
		    fprintf(ofh, "#fileoffset  gop  frame ts asr _ time _   "
			    "c/o  # of frames ver2\n");

		  if (gop.time != pictures)
		    {
		      fprintf(stderr,"\nTimecode problem at filepos %lld ",
			      zzob.pos);
		      fprintf(stderr, " %02d:%02d:%02d.%02d -- ",
			      gop.hour, gop.minute, gop.second, gop.frame);
		      fprintf(stderr, "%d %d\n", gop.time, pictures);
		      exit(1);
		    }
		  printgopinfo = 1;

		  gops++;
		}
	      break;
	    case 0x00: /* picture header */
	      if (zzob.len > 7)
		{
		  int temperal_seq_number = (data[4] << 2) + ((data[5] & 0xc0) >> 6);
		  int frame_type = ((data[5] & 0x38) >> 3);

		  if (printgopinfo)
		    {
		      char c;
		      if (frame_type == 1 && temperal_seq_number < 10)
			c = temperal_seq_number + '0';
		      else
			c = '-';
		      goppic = pictures;
		      fprintf(ofh, "%10lld %5d %6d %c  %d  %02d:%02d:%02d.%02d  %c  ",
			      seqpos, gops, goppic, c, asr,
			      gop.hour, gop.minute, gop.second, gop.frame,
			      gop.closed? 'c': 'o');
		    }
		  printgopinfo = 0;
		}
	      pictures++;
	      break;
	    }
	}
    }

  fprintf(ofh, "%d\n", pictures - goppic);
  fprintf(ofh, "end: size %lld frames %d gops %d abr %ld  w %d h %d\n",
	  zzob.pos, pictures, ++gops, (long)(zzob.pos / (pictures / 25) * 8),
	  maxwidth, maxheight);
  printf("\r- Scanning video at %lld of %lld (100%%).\n", zzob.pos, fsize);
  exit(0);
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
