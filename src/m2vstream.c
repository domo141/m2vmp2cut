/*
 * m2vstream.c
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2012 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Wed 24 Oct 2012 18:26:32 EEST too
 * Last modified: Mon 24 Jun 2013 23:17:55 EEST too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define DBGS 0
#include "x.h"
#include "zzob.h"
#include "bufwrite.h"

#include "ghdrs/m2vstream_priv.h"

// (variable) block begin/end -- explicit liveness...
#define BB {
#define BE }

int main(int argc, char ** argv)
{
    if (argc < 4)
	xerrf("Usage: %s asr ifile offset,frames [offset,frames...]\n",*argv);

    int asr = atoi(argv[1]);

    if ( (asr <= 0 && argv[1][0] != '0') || argv[1][1] != '\0')
	xerrf("Asr value '%s' incorrect.\n", argv[1]);

    int fd = open(argv[2], O_RDONLY);
    if (fd < 0)
	xerrf("Opening input file '%s' failed:", argv[2]);

    int tframes;
    BB;
    struct stat st;
    intmax_t aframes = 0;

    if (fstat(fd, &st) < 0)
	xerrf("fstat:");

    if (! S_ISREG(st.st_mode))
	xerrf("'%s': not a file\n", argv[2]);

    for (int i = 3; i < argc; i++) {
	intmax_t offset, frames;
	if (sscanf(argv[i], "%zd,%zd", &offset, &frames) != 2)
	    xerrf("Arg %d: '%s' not in 'offset,frames' format.\n", i, argv[i]);
	if (offset < 0 || frames < 0)
	    xerrf("Arg %d: '%s' offset or frames negative.\n", i, argv[i]);
	if (offset >= st.st_size)
	    xerrf("Arg %d: '%s' offset %zd >= file size %zd.\n", i, argv[i],
		  offset, st.st_size);
	aframes += frames;
    }
    if (aframes > INT_MAX)
	xerrf("Total frame count '%zd' too large.\n", aframes);

    tframes = (int)aframes;
    BE;

    ZeroZeroOneBuf zzob;
    unsigned char ibuf[65536];
    zzob_set(&zzob, fd, ibuf, sizeof ibuf, -1);

    BufWrite bw;
    unsigned char obuf[65536];
    bufwrite_init(&bw, 1, obuf, sizeof obuf);

    s__stream(&zzob, &bw, tframes, asr, &argv[3]);
    return 0;
}

static void s__stream(ZeroZeroOneBuf * zzob, BufWrite * bw, int tframes,
		      int asr, char ** av) /* protoadd GCCATTR_NORETURN */
{
    int tframenum = 0;
    intmax_t movedbytes = 0;

    /* Ignore unknown aspect ratios... */
    if (asr > 4)
	asr = 0;
    else /* tune it to be already suitable... to write in. */
	asr <<= 4;

    const char cs_mpg_vdff[] =
	"mpg video data write failure after writing %jd bytes:";

    for (int i = 0; av[i]; i++) {
	intmax_t offset, frames;
	if (sscanf(av[i], "%zd,%zd", &offset, &frames) != 2)
	    xerrf("ERROR: '%s' not in 'offset,frames' format.\n", av[i]);

	// aargh ;/ maybe zzob_sseek()
	if (lseek(zzob->p.c_fd, offset, SEEK_SET) < 0)
	    xerrf("lseek:");
	zzob_reset(zzob);

	// until sequence header //
	while (zzob_data(zzob, false))
	{
	    if (zzob->len >= 3) {
		if (zzob->data[3] == 0xb3) { /* sequence header */
		    if (asr > 0)
			if (zzob->len > 7)
			    zzob->data[7] = (zzob->data[7] & 0x0f) | asr;
		    if (! bufwrite(bw, zzob->data, zzob->len))
			/* The value written below may not be exact */
			xerrf(cs_mpg_vdff, (intmax_t)zzob->pos + zzob->len);
		    goto _c1;
		}
	    }
	}
	xerrf("EOF too early (before first sequence header).\n");
    _c1:
	do {} while (0);
	static int video_fmt = -1;
	// expect first iframe, prepare for skip... //
	int skipframes = 0;
	int cframenum = 0;
	BB;
	int havegob = 0;
	while (zzob_data(zzob, false))
	{
	    if (zzob->len >= 3) {
		switch (zzob->data[3])
		{
#if 0
		case 0xb5: /* sequence extension header */
		    if ((zzob->data[4] & 0xf0) == 0x20) /* sequence display ext. */
		    {
			if (video_fmt < 0)
			    video_fmt = zzob->data[4] & 0x0e;
			else
			    zzob->data[4] = (zzob->data[4] & 0xf1) | video_fmt;
		    }
		    break;
#endif
		case 0xb8: /* GOP header */
		    if (zzob->len >= 7)
		    {
			int hour = tframenum / (25 * 60 * 60);
			int min = (tframenum / (25 * 60)) % 60;
			int sec = (tframenum / 25) % 60;
			int frame = tframenum % 25;
			// write time, zero drop, broken flags, set closed flag
			zzob->data[4] = (hour << 2) | (min >> 4);
			zzob->data[5] = (min << 4) | 0x08 | (sec >> 3);
			zzob->data[6] = (sec << 5) | (frame >> 1);
			zzob->data[7] = (frame << 7) | 0x40;
			havegob = 1;
		    }
		    break;
		case 0x00: /* picture */
		    if (! havegob)
			xerrf("No GOP header before picture\n");

		    // temperal sequence number
		    skipframes = (zzob->data[4] << 2) + (zzob->data[5] >> 6);
		    if (skipframes) {
			zzob->data[4] = 0;
			zzob->data[5] &= 0x3f;
		    }
		    tframenum++;
		    cframenum = 1;

		    if (! bufwrite(bw, zzob->data, zzob->len))
			/* The value written below may not be exact */
			xerrf(cs_mpg_vdff, (intmax_t)zzob->pos + zzob->len);
		    goto _c2;
		}
	    }
	    if (! bufwrite(bw, zzob->data, zzob->len))
		/* The value written below may not be exact */
		xerrf(cs_mpg_vdff, (intmax_t)zzob->pos + zzob->len);
	}
	BE;
	xerrf("EOF too early.\n");
    _c2:
	if (skipframes) {
	    // write out slices of first frame
	    while (zzob_data(zzob, false))
	    {
		if (zzob->len >= 3)
		{
		    if (zzob->data[3] == 0x00) /* (next) picture */
			goto _c3;
		}
		if (! bufwrite(bw, zzob->data, zzob->len))
		    /* The value written below may not be exact */
		    xerrf(cs_mpg_vdff, (intmax_t)zzob->pos + zzob->len);
	    }
	    xerrf("EOF too early (while skipping \"open\" frames).\n");
	_c3:
	    if (skipframes > 1) {
		int drop = skipframes - 1;
		while (zzob_data(zzob, false))
		{
		    if (zzob->len >= 3)
		    {
			if (zzob->data[3] == 0x00) { /* picture */
			    drop--;
			    if (drop == 0)
				goto _c4;
			}
		    }
		}
		xerrf("EOF too early.\n");
	    }
	_c4:
	    // skip the slices of the last frame to be skipped
	    while (zzob_data(zzob, false))
	    {
		if (zzob->len < 3)
		    continue;
		if ((zzob->data[3] == 0 || zzob->data[3] >= 0xB0)
		    && zzob->data[3] != 0xb5) {
		    zzob->len = 0; // XXX add func to zzob (zzob_hold()?)
		    goto _c5;
		}
	    }
	    xerrf("EOF too early.\n");
	}
    _c5:
	do {} while (0);
	BB;
	int prevpc = 0;
	while (zzob_data(zzob, false))
	{
	    if (zzob->len >= 3)
	    {
		/*printf("%d\n", mpgbuf->data[3]); */
		switch (zzob->data[3])
		{
		case 0x00: /* picture header */
		    tframenum++;
		    cframenum++;
		    int t1 = (int)((intmax_t)tframenum * 100 / tframes);
		    if (t1 != prevpc)
		    {
			fprintf(stderr, "\rPicture %d of %d total (%d%%) ",
				tframenum, tframes, t1);
			prevpc = t1;
		    }
		    if (skipframes) {
			unsigned char d5 = zzob->data[5];
			int tsr = (zzob->data[4] << 2) + (d5 >> 6);
			tsr -= skipframes;
			zzob->data[4] = (tsr >> 2);
			zzob->data[5] = ((tsr & 0x03) << 6) | (d5 & 0x3f);
		    }
		    if (cframenum >= frames) {
			if (! bufwrite(bw, zzob->data, zzob->len))
			    /* The value written below may not be exact */
			    xerrf(cs_mpg_vdff, (intmax_t)zzob->pos + zzob->len);
			goto _c6;
		    }
		break;
		case 0xb3:	/* sequence header */
		    if (asr > 0)
			if (zzob->len > 7)
			    zzob->data[7] = (zzob->data[7] & 0x0f) | asr;
		    break;
#if 0
		case 0xb5: /* sequence extension header */
		    if ((zzob->data[4] & 0xf0) == 0x20) /* sequence display ext. */
		    {
			if (video_fmt < 0)
			    video_fmt = zzob->data[4] & 0x0e;
			else
			    zzob->data[4] = (zzob->data[4] & 0xf1) | video_fmt;
		    }
		    break;
#endif
		case 0xb8: /* GOP header */
		    if (zzob->len >= 7)
		    {
			int hour = tframenum / (25 * 60 * 60);
			int min = (tframenum / (25 * 60)) % 60;
			int sec = (tframenum / 25) % 60;
			int frame = tframenum % 25;

			zzob->data[4] = (zzob->data[4] & ~0x7f)
			    | (hour << 2) | (min >> 4);
			zzob->data[5] = (min << 4) | 0x08 | (sec >> 3);
			zzob->data[6] = (sec << 5) | (frame >> 1);
			zzob->data[7] = (zzob->data[7] & 0x7f) | (frame << 7);

			skipframes = 0;
		    }
		    break;
		}
		if (! bufwrite(bw, zzob->data, zzob->len))
		    /* The value written below may not be exact */
		    xerrf(cs_mpg_vdff, (intmax_t)zzob->pos + zzob->len);
	    }
	}
	BE;
	xerrf("EOF too early.\n");
    _c6:
	// fill last slices...
	while (zzob_data(zzob, false))
	{
	    if (zzob->len >= 3)
	    {
		if ((zzob->data[3] == 0x00 || zzob->data[3] >= 0xB0)
		    && zzob->data[3] != 0xb5)
		    goto _c7;
	    }
	    if (! bufwrite(bw, zzob->data, zzob->len))
		/* The value written below may not be exact */
		xerrf(cs_mpg_vdff, (intmax_t)zzob->pos + zzob->len);
	}
	xerrf("EOF too early.\n");
    _c7:
	movedbytes += zzob->pos;
	continue; // compiler requires something after label.
    }
    // add sequence end //
    if (! bufwrite(bw, (const unsigned char *)"\000\000\001\267", 4)
	|| ! bufwrite_exit(bw))
	/* The value written below may not be exact */
	xerrf(cs_mpg_vdff, (intmax_t)zzob->pos);

    fprintf(stderr,
	    "\rTransferred %d pictures, %jd bytes of mpeg video data.\n",
	    tframenum, movedbytes);

    exit(0);
}

/*
 * Local variables:
 * mode: c
 * c-file-style: "stroustrup"
 * tab-width: 8
 * End:
 */
