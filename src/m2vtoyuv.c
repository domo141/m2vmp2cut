#if 0 /*
 set -eu; case $1 in -o) trg=$2; shift 2
	;;	*) trg=`exec basename "$0" .c` ;; esac; rm -f "$trg"
 WARN=-Wall' -Wstrict-prototypes -pedantic -Wno-long-long'
 WARN=$WARN' -Wcast-align -Wpointer-arith' # -Wfloat-equl #-Werror
 WARN=$WARN' -W -Wwrite-strings -Wcast-qual -Wshadow' # -Wconversion
 eval `exec cat ../_build/config/mpeg2.conf` # XXX
 xf="$mpeg2_only -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} --std=c99 $WARN "$@" -o "$trg" "$0" $xf
 exit 0
 */
#endif
/*
 * $Id; m2vtoyuv.c $
 *
 * Author: Tomi Ollila  too ät iki fi
 *
 *	Copyright (c) 2008 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Fri Feb 08 17:16:45 EET 2008 too
 * Last modified: Mon 16 Feb 2015 19:08:29 +0200 too
 */

/* this program is originally based on:

 * sample1.c
 * Copyright (C) 2003      Regis Duchesne <hpreg@zoy.org>
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.

 + ... of mpeg2dec-0.4.1 distribution: the following copyright and
 + no-warranty texts are from it, replaced with m2vtoyuv as software name:

 * m2vtoyuv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mm2vtoyuv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 ... and so on ...

 + The copy of GNU General Public License is so commonly know so it
 + might not be distributed along this program. Seek the Internet
 + (or just file COPYING on your machine) for the copy

 + This program reads a MPEG-2 stream, and output each of its frames
 + in Y'Cb'Cr (commonly also known as YUV (4:2:0, planar format))
 + prefixed with (simple) YUV4MPEG headers. Information of YUV4MPEG
 + headers can be read in yuv4mpeg.h and yuv4mpeg.5 files of mjpegtools
 + distribution ('man yuv4mpeg' may work if you have mjpegtools installed).
 */

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "mpeg2.h"

#define null ((void *)0)

#define d0(x) do {} while (0)

#include <sys/uio.h>

/* http://en.wikipedia.org/wiki/YCbCr */

static void output_yuv4mpeg(const mpeg2_sequence_t * sequence,
			    const mpeg2_picture_t * picture,
			    const char ip,
			    uint8_t * const buf[3])
{
    struct iovec iov[4];
    static struct
    {
	int width;
	int height;
	char fr[16];
	char ar[16];
	char il;
    } fi[2] = {{0,0,{0},{0},0},{0,0,{0},{0},0}};
    char yuvhdr[128];
    int octets;
#if 0
    printf("%d %d %d %d\n", sequence->width, sequence->height,
	   sequence->chroma_width, sequence->chroma_height); return;
    /* XXX should check that chroma_%:s are 1/2 of %:s ... */
#endif
    fi[1].width = sequence->width;
    fi[1].height = sequence->height;
    octets = sequence->width * sequence->height;

    if (ip)
	fi[1].il = ip;
    else {
	if (picture && PIC_FLAG_PROGRESSIVE_FRAME)    fi[1].il = 'p';
	else if (picture && PIC_FLAG_TOP_FIELD_FIRST) fi[1].il = 't';
	else					      fi[1].il = 'b'; }

    // frame rate from frame_period ?
    // aspect ratio from pixel width/height ?
    strcpy(fi[1].fr, "25:1");
    strcpy(fi[1].ar, "0:0"); // mpeg2enc problem with 16:9 ???

    if (memcmp(&fi[0], &fi[1], sizeof fi[0]) != 0) {
	int hdrlen = sprintf /* ( */
	    (yuvhdr, "YUV4MPEG2 W%d H%d F%s I%c A%s C420mpeg2\nFRAME\n",
	     fi[1].width, fi[1].height, fi[1].fr, fi[1].il, fi[1].ar);
	iov[0].iov_base = yuvhdr;
	iov[0].iov_len = hdrlen;
	memcpy(&fi[0], &fi[1], sizeof fi[0]);
    }
    else {
	*(const char **)&iov[0].iov_base = "FRAME\n"; /* yeah yeah... */
	iov[0].iov_len = 6;
    }

    /* nice, mpeg2dec outputs info in perfectly suitable format ... */
    iov[1].iov_base = buf[0];
    iov[1].iov_len  = octets;
    iov[2].iov_base = buf[1];
    iov[2].iov_len  = octets / 4;
    iov[3].iov_base = buf[2];
    iov[3].iov_len  = octets / 4;
    writev(1, iov, 4);
}

typedef struct {
    char ** arg0p;
    char ** argv;
    int state;
    int verbose;
    char * p;
    char ip;
    union {
	off_t offset;
	struct {
	    int start;
	    int end;
	} r;
	char * plain;
	float sleep;
    } o;
} Args;

enum { ARG_END, ARG_SEEK, ARG_SLEEP, ARG_RANGE, ARG_PLAIN };

void die(const char * format, ...);
void vdie(const char * format, va_list ap);

static void args_init(Args * args, char ** argv);
static int args_next(Args * args);
static FILE * testargs_getfile(Args * args);

static void msleep(int mseconds)
{
#if 1
    usleep(mseconds * 1000);
#else
    poll(null, 0, mseconds);
#endif
}

void output_frameinfo(const char * od, int framenum,
		      const mpeg2_picture_t * pic)
{
    static const char * s = "_IPBD X";
    unsigned int ftype = pic->flags & PIC_MASK_CODING_TYPE;
    if (ftype > 4) ftype = 5;

    fprintf(stderr, "m2vtoyuv: %s frame %d (%d %c)\n",
	    od, framenum, pic->temporal_reference, s[ftype]);
}

int main (int argc, char ** argv)
{
#define BUFFER_SIZE 4096
    uint8_t buffer[BUFFER_SIZE];
    mpeg2dec_t * decoder;
    const mpeg2_info_t * info;
    const mpeg2_sequence_t * sequence;
    mpeg2_state_t state;
    size_t size, prevsize;
    Args args;
    FILE * mpgfile;
    int type;
    int start = 0, end = 1000 * 1000 * 1000;
    unsigned int mseconds = 0;
    int framenum = 0;

    (void)argc;

    args_init(&args, argv);
    mpgfile = testargs_getfile(&args);
    args_init(&args, argv);

    while (( type = args_next(&args)) != ARG_END)
	switch (type)
	{
	case ARG_SEEK:
	    if (fseeko(mpgfile, args.o.offset, SEEK_SET) < 0)
		die("fseeko(%jd):", (intmax_t)args.o.offset);
	    continue;
	case ARG_SLEEP:
	    mseconds = (int)(args.o.sleep * 1000);
	    continue;
	case ARG_RANGE:
	    start = args.o.r.start; end = args.o.r.end;
	    d0(("range %d %d\n", start, end));
	    goto breakwhile;
	}
breakwhile:

    decoder = mpeg2_init ();
    if (decoder == NULL) {
	fprintf (stderr, "Could not allocate a decoder object.\n");
	exit (1);
    }
    info = mpeg2_info (decoder);

    size = prevsize = (size_t)-1;
    do {
	state = mpeg2_parse (decoder);
	sequence = info->sequence;
	switch (state) {
	case STATE_BUFFER:
	    size = fread (buffer, 1, BUFFER_SIZE, mpgfile);
	    if (size != 0)
		prevsize = size;
	    else if (prevsize != 0) {
		memcpy(buffer, "\000\000\001\267", 4);
		size = 4; prevsize = 0;
	    }
	    mpeg2_buffer (decoder, buffer, buffer + size);
	    break;
	case STATE_SLICE:
	case STATE_END:
	case STATE_INVALID_END:
	    if (info->display_fbuf) {
		if (framenum++ < start) {
		    if (args.verbose > 0)
			output_frameinfo("drop", framenum - 1,
					 info->display_picture);
		    break;
		}
		/* note: framenum off by one now */
		if (framenum > end) {
		    while (type = args_next(&args), 1)
			switch (type)
			{
			case ARG_SEEK:
			    if (fseeko(mpgfile, args.o.offset, SEEK_SET) < 0)
				die("fseeko(%jd):", (intmax_t)args.o.offset);
			    framenum = 0;
			    continue;
			case ARG_SLEEP:
			    mseconds = (int)(args.o.sleep * 1000);
			    continue;
			case ARG_RANGE:
			    start = args.o.r.start; end = args.o.r.end;
			    d0(("range %d %d\n", start, end));
			    if (framenum == 0) {
				mpeg2_reset(decoder, 1);
				goto breakwhile3;
			    }
			    goto breakwhile2;
			case ARG_END:
			    goto breakwhile4;
			}
		breakwhile2:
		    /* note: framenum off by one */
		    if (framenum <= start) {
			if (args.verbose > 0)
			    output_frameinfo("drop", framenum - 1,
					     info->display_picture);
			break;
		    }
		    /* assert(framenum < end); */
		}
#if 1
		if (args.verbose > 0)
		    output_frameinfo("output", framenum - 1,
				     info->display_picture);
		output_yuv4mpeg(sequence, info->display_picture,
				args.ip, info->display_fbuf->buf);
#else
		printf("frame %d\n", framenum - 1);
#endif
		if (mseconds)
		    msleep(mseconds);
	    }
	breakwhile3:
	    break;
	default:
	    if (args.verbose > 1)
		fprintf(stderr, "m2vtoyuv: Unused decoder state: %d\n", state);
	    break;
	}
    } while (size);
breakwhile4:
    mpeg2_close (decoder);
    return 0;
}


/* ------------------- args code --------------------- */

static void usage(Args * args)
{
    die("\nUsage: %s [-q] [-v] [-sb pos] [-s sec] [-I (p|b|t)] [-c (s-e|s)[,(s-e|s)...]] file|-"
	"\n\n"
	"  -sb: seek to byte position. may be given multiple times\n"
	"  -c: frame ranges/single frames to decode. may be given multiple times\n"
	"  -s: sleep between frames. float. may be given multiple times\n"
	"  -I: interlaced (top/botton field first) or progressive\n"
	"  -q: quiet operation (decrease verbosity value)\n"
	"  -v: verbose operation (increase verbosity value)\n\n"
	"  first frame number: 0. end frame in range not encoded (like transcode/python)\n"
	"  sleep option is good for slideshows with '| mplayer -', for example\n",
	args->arg0p[0]);
}

static void args_init(Args * args, char ** argv)
{
    memset(args, 0, sizeof *args);
    args->arg0p = argv;
    args->argv = argv + 1;
}

static void argdie(Args * args, const char * format, ...)
{
    va_list ap;
    fprintf(stderr, "Arg #%ld '%s': ",
	    args->argv - args->arg0p, args->argv[0]);
    va_start(ap, format);
    vdie(format, ap);
}

static void rangedie(Args * args, const char * format, ...)
{
    va_list ap;
    fprintf(stderr, "Arg #%ld '%s' near offset %ld: ",
	    args->argv - args->arg0p, args->argv[0],
	    args->p - args->argv[0]);
    va_start(ap, format);
    vdie(format, ap);
}

static int args_next(Args * args)
{
    char * arg;
    char * p;
    char c;

    while (1) switch (args->state) {
    case 0:
#define _args_argno(args)  (args->argv - args->arg0p)
	/*printf("------ %s\n", args->argv[0]); */
	if (args->argv[0] == null)
	    return ARG_END;
	arg = args->argv[0];

	if (strcmp(arg, "-q") == 0) {
	    args->verbose--;
	    args->state = 2;
	    continue;
	}
	if (strcmp(arg, "-v") == 0) {
	    args->verbose++;
	    args->state = 2;
	    continue;
	}
	if (strcmp(arg, "-sb") == 0) {
	    arg = (++args->argv)[0];
	    if (arg == null)
		die("Arg #%d, for '-sb' missing", _args_argno(args));
	    if ( (args->o.offset = strtoll(arg, &p, 10)) < 0 ||
		 ( args->o.offset >= 0 && *p != '\0') )
		die("Arg #%d (%s) for '-sb' not good...", /* XXX */
		    _args_argno(args), arg);
	    args->state = 2;
	    return ARG_SEEK;
	}
	if (strcmp(arg, "-s") == 0) {
	    arg = (++args->argv)[0];
	    if (arg == null)
		die("Arg #%d, for '-s' missing", _args_argno(args));
	    if (sscanf(arg, "%f", &args->o.sleep) != 1)
		die("Arg #%d (%s) for '-s' not good...", /* XXX */
		    _args_argno(args), arg);
	    args->state = 2;
	    return ARG_SLEEP;
	}
	if (strcmp(arg, "-c") == 0) {
	    arg = (++args->argv)[0];
	    if (arg == null)
		die("Arg #%d, for '-c' missing", _args_argno(args));
	    args->state = 1; args->p = args->argv[0]; /* XXX why not args ? */
	    continue;
	}
	if (strcmp(arg, "-I") == 0) {
	    arg = (++args->argv)[0];
	    if (arg == null)
		die("Arg #%d, for '-I' missing", _args_argno(args));
	    if (arg[0] == 'p' || arg[0] == 't' || arg[0] == 'b')
		args->ip = arg[0];
	    else args->ip = 0;
	    args->state = 2;
	    continue;
	}
	if (arg[0] == '-' && arg[1] != '\0')
	    die("Arg #%d, Option '%s' unknown", _args_argno(args), arg);
	args->o.plain = arg;
	args->state = 2;
#undef _args_argno
	return ARG_PLAIN;
/*  range handling... */
    case 1:
	p = args->p;
	if (! isdigit(*p))
	    rangedie(args, "'%c' not digit", *p);
	arg = p;
	while (isdigit(*++p))
	    ;
	c = *p++;
	args->o.r.start = atoi(arg);
	args->p = p;
	if (c == ',' || c == '\0') {
	    args->o.r.end = args->o.r.start + 1;
	}
	else if (c == '-') {
	    if (! isdigit(*p))
		rangedie(args, "'%c' not digit", *p);
	    arg = p;
	    while (isdigit(*++p))
		;
	    c = *p++;
	    args->o.r.end = atoi(arg);
	}
	else rangedie (args, "illegal char '%c'", c);

	args->p = p;
	if (c == '\0')
	    args->state = 2;
	else {
	    if (c != ',')
		rangedie(args, "illegal (delimiter) char '%c'", c);
	}
	return ARG_RANGE;

    case 2:
	args->argv++;
	args->state = 0;
	continue;
    }
}

static FILE * testargs_getfile(Args * args)
{
    int type;
    //int start = 0;
    int end = 0;
    char * filename = null;
    int seeks = 0;

    while (( type = args_next(args)) != ARG_END)
	switch (type)
	{
	case ARG_SEEK:
	    d0(("seek %jd\n", (intmax_t)args->o.offset));
	    /*start =*/ end = 0;
	    seeks = 1;
	    break;
	case ARG_RANGE:
	    if (args->o.r.start < end)
		rangedie(args, "start value %d < previous end (%d)",
			 args->o.r.start,end);
	    if (args->o.r.end <= args->o.r.start)
		rangedie(args, "start value %d >= end (%d)",
			 args->o.r.start, args->o.r.end);

	    /*start = args->o.r.start;*/ end = args->o.r.end;
	    d0(("range %d %d\n", start, end));
	    break;
	case ARG_PLAIN:
	    if (filename != null)
		argdie(args, "filename '%s' already defined.", filename);
	    filename = args->o.plain;
	    d0(("plain '%s'\n", filename));
	    break;
	}
    if (filename == null)
	usage(args);
    if (strcmp(filename, "-") != 0) {
	FILE * fh = fopen(filename, "rb");
	if (fh == null)
	    die("fopen(%s):", filename);
	return fh;
    }
    if (seeks)
	die("Can not seek if source is from stdin");
    return stdin;
}

#if 0
int main(int argc, char ** argv)
{
    Args args;
    int type;
    FILE * fh;
    (void)argc;

    args_init(&args, argv);
    fh = testargs_getfile(&args);
    args_init(&args, argv);

    while (( type = args_next(&args)) != ARG_END)
	switch (type)
	{
	case ARG_SEEK:
	    printf("seek %jd\n", (intmax_t)args.o.offset);
	    break;
	case ARG_RANGE:
	    printf("range %d %d\n", args.o.r.start, args.o.r.end);
	    break;
	case ARG_PLAIN:
	    printf("plain '%s'\n", args.o.plain);
	}
    return 0;
}
#endif

static void verrf(const char * format, va_list ap)
{
    int error = errno; /* XXX is this too late ? */
    vfprintf(stderr, format, ap);
    if (format[strlen(format) - 1] == ':')
	fprintf(stderr, " %s\n", strerror(error));
    else
	fputs("\n", stderr);
}

void vdie(const char * format, va_list ap)
{
    verrf(format, ap);
    exit(1);
}

void die(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    vdie(format, ap);
    //va_end(ap);
}

/*
 * Local variables:
 * mode: c
 * c-file-style: "stroustrup"
 * tab-width: 8
 * compile-command: "sh m2vtoyuv.c"
 * End:
 */
