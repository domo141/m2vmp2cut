#if 0 /*
	LF_OPTS='-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE'
	set -x
	exec ${CC:-gcc} -Wall -ggdb $LF_OPTS -o `basename $0 .c` $0 x.c; */
#endif
/*
 *  mp2cutpoints.c $
 *
 * Author: Tomi Ollila too ät iki fi
 *
 *	Copyright (c) 2005 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Thu Oct 20 19:32:21 EEST 2005 too
 * Last modified: Sun 23 Sep 2012 20:59:17 EEST too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <math.h>

#define DBGS 0
#include "x.h"
#include "bufwrite.h"

/*                     ((1 << (8 * sizeof (v) - 1)) - 1) */
#define MAXINTVAL(v) ((((1 << (8 * sizeof (v) - 2)) - 1) << 1) | 1)


/* hand-converted python->c, with few modifications */

typedef struct _simplefilebuf simplefilebuf;
struct _simplefilebuf
{
    int fd;
    unsigned char buf[65536];
    unsigned int offset;
    unsigned int len;
    off_t gone;
};

void simplefilebuf_init(simplefilebuf * self, char * fname)
{
    if ((self->fd = open(fname, O_RDONLY)) < 0)
	xerrf("Opening file %s for reading failed:", fname);

    self->offset = self->len = self->gone = 0;
}

void simplefilebuf_seekset(simplefilebuf * self, unsigned int pos)
{
    if (lseek(self->fd, pos, SEEK_SET) < 0)
	xerrf("Seek failed:");
    self->offset = self->len = 0;
    self->gone = pos;
}

bool simplefilebuf_fillbuf(simplefilebuf * self, unsigned int rest)
{
    int l;

    if (rest > 0)
    {
	memmove(self->buf, self->buf + self->offset, rest);
	if ((l = read(self->fd,self->buf + rest,sizeof self->buf - rest)) <= 0)
	    return false;
	self->gone += self->offset;
    }
    else
    {
	self->gone += self->len;
	if ((l = read(self->fd, self->buf, sizeof self->buf)) <= 0)
	    return false;
    }
    self->len = rest + l;
    self->offset = 0;
    if (self->len == rest)
	return false;
    return true;
}

int simplefilebuf_filepos(simplefilebuf * self)
{
    return self->gone + self->offset;
}

void simplefilebuf_unusedbytes(simplefilebuf * self, int bytes)
{
    self->offset -= bytes;
}

unsigned char * simplefilebuf_needbytes(simplefilebuf * self, int bytes)
{
    while (true)
    {
	int rest = self->len - self->offset;

	if (rest >= bytes)
	{
	    int oo = self->offset;
	    self->offset += bytes;

	    return self->buf + oo;
	}
	if (! simplefilebuf_fillbuf(self, rest))
	    return NULL;
    }
}

int simplefilebuf_dumpto(simplefilebuf * self, unsigned char c)
{
    int rv = 0;

    while (true)
    {
	unsigned char * s
	    = memchr(self->buf + self->offset, c, self->len - self->offset);
	if (s)
	{
	    int adv = s - (self->buf + self->offset);
	    rv += adv;
	    self->offset += adv + 1;
	    return rv;
	}
	rv = self->len - self->offset;
	if (! simplefilebuf_fillbuf(self, 0))
	    return -1;
    }
}

/* mpeg1 layer 2 bitrate indexes */
static const int biarr[] =
/*      1   2   3   4   5   6    7    8    9   10   11   12   13   14 */
  { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 };

/* mpeg1 sampling rate indexes */
static const int siarr[] = { 44100, 48000, 32000, 0 };

/*                    84218421 84218421 84218421 84218421
 *                    76543210 76543210 76543210 76543210
 * Mpeg audio header: AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM
 * See http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 */

typedef unsigned char u8;
typedef struct
{
    u8 id; /*   B above (mpeg audio version id)  */
    u8 ld; /*   C above (layer description)  */
    u8 pb; /*   D above (protection bit) */

    u8 bi; /*   E above (bitrate index)  */
    u8 si; /*   F above (sampling rate frequency index)  */
    u8 pad; /*  G above (padding bit)  */
    u8 mode; /* I above (mono/stereo/...) */
    u8 mex;  /* J above (mode extension) */
    int afn;
    int ms;
    int bitrate;
    int samplerate;
    simplefilebuf * sfb;
    unsigned char * frame;
} Mp2Info;

static int mp2get(Mp2Info * m2i)
{
    char * p;
    int flib;

    p = (char *)simplefilebuf_needbytes(m2i->sfb, 3);
    if (p == NULL)
	xerrf("XXX ERROR XXX.\n");

    /* Check that we have header (frame sync: 11 bits set (A above).) */
    if ((p[0] & 0xe0) != 0xe0)
	return 0;

    m2i->id = p[0] & 0x18; /*  B above (mpeg audio version id)  */
    m2i->ld = p[0] & 0x06; /*  C above (layer description)  */
    m2i->pb = p[0] & 0x01; /*  D above (Protection bit)  */

    m2i->bi = p[1] & 0xf0; /*  E above (bitrate index)  */
    m2i->si = p[1] & 0x0c; /*  F above (sampling rate frequency index)  */
    m2i->bi >>= 4;
    m2i->si >>= 2;
    m2i->pad= p[1] & 0x02; /*  G above (padding bit)  */
    m2i->mode=p[2] & 0xc0; /*  I above (mode) */
    m2i->mex= p[2] & 0x30; /*  J above (mode extension) */
    m2i->pad  >>= 1;
    m2i->mode >>= 6;
    m2i->mex  >>= 4;

    m2i->bitrate = biarr[m2i->bi];
    m2i->samplerate = siarr[m2i->si];

    m2i->afn++;

#if 0
    printf("m2i->afn %d " "m2i->pb %d " "m2i->bi %d " "m2i->si %d "
	   m2i->afn, m2i->pb, m2i->bi, m2i->si);

    printf("m2i->pad %d " "m2i->mode %d " "m2i->mex %d\n",
	   m2i->pad, m2i->mode, m2i->mex);
#endif

    if (m2i->id != 0x18)
	xerrf("Frame %d (pos %d) not mpeg version 1.\n", m2i->afn,
	      simplefilebuf_filepos(m2i->sfb) - 4);
    if (m2i->ld != 0x04)
	xerrf("Frame %d (pos %d) not mpeg layer 2.\n", m2i->afn,
	      simplefilebuf_filepos(m2i->sfb) - 4);

    flib = 144000 * m2i->bitrate / m2i->samplerate + m2i->pad;
    // FIXME drift will happen when samplerate 44100 XXX //
    m2i->ms = flib * 8 / m2i->bitrate;

    if (m2i->pb == 0) /* drop 16-bit crc in case there is one */
	(void)simplefilebuf_needbytes(m2i->sfb, 2);

    m2i->frame = simplefilebuf_needbytes(m2i->sfb, flib - 4);

    return flib;
}

static const char * ms2tcode(int ms)
{
    static char buf[32];

    sprintf(buf, "%02d:%02d:%02d.%03d",
	    ms / 1000/60/60, (ms / 1000/60) % 60, (ms / 1000) % 60, ms % 1000);
    return buf;
}

static int tcode2ms(char * val)
{
    int secs;
    char * s = val;

    secs = atoi(val);
    val = strpbrk(val, ".:");
    if (val == NULL)   goto msfail;
    if (*val++ == '.') goto msget;
    secs = secs * 60 + atoi(val);
    val = strpbrk(val, ".:");
    if (val == NULL)   goto msfail;
    if (*val++ == '.') goto msget;
    secs = secs * 60 + atoi(val);
    val = strpbrk(val, ".:");
    if (val == NULL || *val++ == ':')
	goto msfail;
msget:
    return secs * 1000 + atoi(val);
msfail:
    xerrf("Incorrect timecode string %s.\n", s);
}

static void printpositions(off_t * positions, int c)
{
    int i;
    c -= 1;
    if (c)
    {
	printf("%ld-%ld", positions[0], positions[1]);
	for (i = 2; i < c; i += 2)
	    printf(",%ld-%ld", positions[i], positions[i + 1]);
	printf("\n");
    }
}

static int wopen(char * ofile)
{
    int ofd = open(ofile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (ofd < 0)
	xerrf("Opening file '%s' for writing failed:", ofile);
    return ofd;
}

static int filesize(int fd)
{
    struct stat st;
    if (fstat(fd, &st) < 0)
	xerrf("stat failed:");
    return st.st_size;
}

struct frame_levels_state {
    int16_t maxval;
    u8 nybblebit;
    int msinbuf;
    int fd;
    u8 buf[4096 + 4]; // so that finish_frame_levels() does not need to check
    int bufi;
};


// Maybe FIXME any of these (alternatives) ??? -- just hand-iterated these...
static inline u8 mv2val(int mv)
{
    int val = (log(pow(mv, 3)) - 13.6); // 1000 -> 7, 10000 -> 14 and so on.
    if (val < 0)  return 0;
    if (val > 15) return 15;
    return (u8)val;
}
// #define mv2val(mv) (mv < 90? 0: (u8)(logf( (float)mv / 60 ) / logf( 1.5 )))
// #define mv2val(mv) ((u8)(mv / 2048))


// 4 bits (16 possible levels) per 5 milliseconds (1/200th of a second).
static void update_frame_levels(struct frame_levels_state * fls,
				int16_t xmonopcm[1152], int tms)
{
    int i;
    int ms = fls->msinbuf % 5; // later, int64_t nanoseconds for less drift.
    int mv = fls->maxval;

    for (i = 0; i < 1152; i++) {
	if (xmonopcm[i] > mv)
	    mv = xmonopcm[i];
	if (ms + ((i+1) * tms) / 1152 >= 5) {
	    u8 val = mv2val(mv);
	    /* printf("mv %d, val %d\n", mv, val); */
	    if (fls->nybblebit++ & 1) {
		fls->buf[fls->bufi++] |= (val << 4);
		if (fls->bufi == sizeof fls->buf - 4) {
		    write(fls->fd, fls->buf, fls->bufi);
		    fls->bufi = 0;
		}
	    }
	    else
		fls->buf[fls->bufi] = val;
	    mv = -1;
	    ms -= 5;
	}
    }
    fls->msinbuf += tms;
    fls->maxval = mv;
}

static void finish_frame_levels(struct frame_levels_state * fls)
{
    if (fls->maxval >= 0) {
	u8 val = mv2val(fls->maxval);
	if (fls->nybblebit++ & 1)
	    fls->buf[fls->bufi++] |= (val << 4);
	else
	    fls->buf[fls->bufi++] = (val | 0x10); // after end 0x12:s are used.
    }
    if (fls->bufi)
	write(fls->fd, fls->buf, fls->bufi);
}

/* stuff derived, then modified from kjmp_* */
struct mp2_ctx_t {
    int V[2][1024];
    int Voffs;
};
static void mp2_init(struct mp2_ctx_t * mp2);
static void mp2_decode_frame(struct mp2_ctx_t * mp2,
			     const Mp2Info * m2i, int16_t * xmonopcm);
/* --- */

void scan(char * ifile, char * sfile, char * levelfile)
{
    int sfd = wopen(sfile);
    simplefilebuf sfb;
    Mp2Info m2i;
    int fsize;

    int totaltime = 0/*, prevtotal = 0*/;
    int position = 0, prevposx = 0;
    int cpc, ppc = 0;
    int pbr = 0, psr = 0;

    struct mp2_ctx_t mp2;
    int16_t xmonopcm[1152];
    struct frame_levels_state fls;

    memset(&m2i, 0, sizeof m2i);
    simplefilebuf_init(&sfb, ifile);
    m2i.sfb = &sfb;
    fsize = filesize(sfb.fd);

    mp2_init(&mp2);
    memset(&fls, 0, sizeof fls);
    fls.fd = wopen(levelfile);

    fdprintf(sfd, "#offset msec fnum brate srate - time\n");

    while (true)
    {
	int skipped = simplefilebuf_dumpto(&sfb, '\377');
	if (skipped < 0)
	{
	    fdprintf(sfd,
		     "#File ended: audio frames: %d Total time: %d ms (%s).\n",
		     m2i.afn, totaltime, ms2tcode(totaltime));
	    fdprintf(0, "\r- Scanning audio at %d of %d bytes (100%%).\n",
		     fsize, fsize);
	    break;
	}

	if (skipped > 0)
	    fprintf(stderr,"Skipped %d bytes of garbage before audio frame %d.",
		    skipped, m2i.afn + 1);

	cpc = position / (fsize / 100);
	if (ppc != cpc)
	{
	    fdprintf(0, "\r- Scanning audio at %d of %d bytes (%d%%)",
		     position, fsize, cpc);
	    ppc = cpc;
	}

	position = simplefilebuf_filepos(&sfb) - 1;

	if (mp2get(&m2i) > 0)
	{
	    mp2_decode_frame(&mp2, &m2i, xmonopcm);
	    update_frame_levels(&fls, xmonopcm, m2i.ms);

	    if (m2i.bitrate != pbr || m2i.samplerate != psr)
	    {
		fdprintf(0, "\rFrame %d (pos %d): bitrate %d, samplerate %d.\n",
			 m2i.afn, position, m2i.bitrate, m2i.samplerate);
		pbr = m2i.bitrate; psr = m2i.samplerate;

		fdprintf(sfd, "%d %d %d %d %d - %s \n", position, totaltime,
			 m2i.afn, pbr, psr, ms2tcode(totaltime));
#define OFFDIST (2 * 1000 * 1000)
		prevposx = position / OFFDIST;
	    }
	    else if (position / OFFDIST != prevposx) {
		fdprintf(sfd, "%d %d %d %d %d - %s\n", position, totaltime,
			 m2i.afn, pbr, psr, ms2tcode(totaltime));
		prevposx = position / OFFDIST;
	    }
	    //prevtotal = totaltime;
	    totaltime += m2i.ms;
	}
	else
	    simplefilebuf_unusedbytes(&sfb, 3);
    }
    finish_frame_levels(&fls);
}

void cutpoints(char * timespec, char * ifile, char * ofile, char * sfile)
{
    int    timev[512];
    char * times[512];
    off_t positions[512];
    int i;
    char * p, *q, *r;

    int lasttime, lastindex;
    int stime;
    int totaltime = 0;
    int prevtotal = 0;
    int position = 0;
    int cpc, ppc = 0;
    int pbr = 0, psr = 0;

    struct { int pos; int time; int afn; } prevscan, currscan;

    int ofd;
    FILE * sfh;
    simplefilebuf sfb;
    Mp2Info m2i;

    memset(&m2i, 0, sizeof m2i);
    m2i.sfb = &sfb;

    memset(&prevscan, 0 , sizeof prevscan);
    memset(&currscan, 0 , sizeof currscan);

    if (sfile) {
	sfh = fopen(sfile, "r");
	if (sfh == NULL)
	    xerrf("Opening '%s' failed:", sfile);
    }
    else {
	currscan.time = MAXINTVAL(currscan.time);
	sfh = NULL;
    }

    q = timespec;
    for (i = 0; i < 510; i += 2)
    {
	p = strchr(q, ',');
	if (p)
	    *p++ = '\0';
	r = strchr(q, '-');
	if (r == NULL)
	    xerrf("Incorrect timecode range %s.\n", q);
	*r++ = '\0';
	timev[i] = tcode2ms(q); times[i] = q;
	lasttime = tcode2ms(r); times[i + 1] = r;
	timev[i + 1] = lasttime;

	if (!p)
	    break;
	q = p;
    }
    if (i == 510)
	xerrf("Too long timecode arg string.\n");
    lastindex = i + 2;

    ofd = wopen(ofile);
    simplefilebuf_init(&sfb, ifile);

    stime = timev[0];
    i = 0;
    while (true)
    {
	int skipped;

	if (stime >= currscan.time) {
	    char linebuf[128];
	    while (true) {
		int offset, msec, afn, brate, srate;
		if (fgets(linebuf, sizeof linebuf, sfh) == NULL) {
		    if (prevscan.pos > position) {
			simplefilebuf_seekset(&sfb, prevscan.pos);
			m2i.afn = prevscan.afn;
			position = prevscan.pos; totaltime = prevscan.time;
		    }
		    currscan.time = MAXINTVAL(currscan.time);
		    break;
		}
		if (sscanf(linebuf, "%d %d %d %d %d",
			   &offset, &msec, &afn, &brate, &srate) == 5) {
		    if (brate != pbr || psr != psr) {
			fdprintf(0, "\rFrame %d (pos %d): bitrate %d, samplerate %d.\n",
				 afn, offset, brate, srate);
			pbr = brate; psr = srate;
			/* XXX better handling (and error message) below */
			if (i & 1)
			    xerrf("Audio rate change in output is problematic ! "
				  "exiting.");
		    }
		}
		else
		    continue;
		currscan.time = msec;
		currscan.pos = offset;
		currscan.afn = afn;
		if (currscan.time > stime) {
		    if (prevscan.pos > position) {
			simplefilebuf_seekset(&sfb, prevscan.pos);
			m2i.afn = prevscan.afn;
			position = prevscan.pos; totaltime = prevscan.time;
		    }
		    break;
		}
		prevscan = currscan;
	    }
	}

	skipped = simplefilebuf_dumpto(&sfb, '\377');
	if (skipped < 0)
	{
	    fdprintf(ofd, "cut %s: %s filepos: %d sync: #EOF!\n",
		     i & 1? "out":"in ", times[i], simplefilebuf_filepos(&sfb));
	    fdprintf(ofd,
		     "File ended: audio frames: %d Total time: %d ms (%s).\n",
		     m2i.afn, totaltime, ms2tcode(totaltime));
	    fdprintf(0, "\r- Scanning audio at %d ms of %d ms (100%%).\n",
		     lasttime, lasttime);
	    printpositions(positions, i);
	    exit(0);
	}

	if (skipped > 0)
	    fprintf(stderr,"Skipped %d bytes of garbage before audio frame %d.",
		    skipped, m2i.afn + 1);

	cpc = totaltime / (lasttime / 100);
	if (ppc != cpc || totaltime == lasttime)
	{
	    fdprintf(0, "\r- Scanning audio at %d ms of %d ms (%d%%)",
		     totaltime, lasttime, cpc);
	    ppc = cpc;
	}
	if (totaltime >= stime)
	{
	    int _pos, _sync;
	    if ( (totaltime - stime) <= (stime - prevtotal) )
	    {
		_pos = simplefilebuf_filepos(&sfb) - 1;
		_sync = totaltime - stime;
	    }
	    else
	    {
		_pos = position;
		_sync = prevtotal - stime;
	    }
	    positions[i] = _pos;

	    fdprintf(ofd, "cut %s: %s filepos: %d sync: %d ms.\n",
		     i & 1? "out": "in ", times[i], _pos, _sync);

	    i += 1;
	    if (i >= lastindex)
	    {
		fdprintf(0, "\n");
		printpositions(positions, i);
		exit(0);
	    }
	    stime = timev[i]  + _sync;
	}
	position = simplefilebuf_filepos(&sfb) - 1;

	if (mp2get(&m2i) > 0)
	{
	    if (m2i.bitrate != pbr || m2i.samplerate != psr)
	    {
		fdprintf(0, "\rFrame %d (pos %d): bitrate %d, samplerate %d.\n",
			 m2i.afn, position, m2i.bitrate, m2i.samplerate);
		pbr = m2i.bitrate; psr = m2i.samplerate;

		/* XXX better handling (and error message) below */
		if (i & 1)
		    xerrf("Audio rate change in output is problematic ! exiting.");
	    }

	    prevtotal = totaltime;
	    totaltime += m2i.ms;
	}
	else
	    simplefilebuf_unusedbytes(&sfb, 3);
    }
}

int main(int argc, char ** argv)
{
    if (argc < 4)
	xerrf("Usage: %s --scan ifile ofile levelfile\n"
	      "  or   %s timespec ifile ofile sfile\n", argv[0], argv[0]);

    if (strcmp(argv[1], "--scan") == 0)
	scan(argv[2], argv[3], argv[4]);
    else
	cutpoints(argv[1], argv[2], argv[3], argv[4]);
    return 0;
}

/* The following code uses parts from kjmp2.c */

/******************************************************************************
** kjmp2 -- a minimal MPEG-1 Audio Layer II decoder library                  **
*******************************************************************************
** Copyright (C) 2006 Martin J. Fiedler <martin.fiedler@gmx.net>             **
**                                                                           **
** This software is provided 'as-is', without any express or implied         **
** warranty. In no event will the authors be held liable for any damages     **
** arising from the use of this software.                                    **
**                                                                           **
** Permission is granted to anyone to use this software for any purpose,     **
** including commercial applications, and to alter it and redistribute it    **
** freely, subject to the following restrictions:                            **
**   1. The origin of this software must not be misrepresented; you must not **
**      claim that you wrote the original software. If you use this software **
**      in a product, an acknowledgment in the product documentation would   **
**      be appreciated but is not required.                                  **
**   2. Altered source versions must be plainly marked as such, and must not **
**      be misrepresented as being the original software.                    **
**   3. This notice may not be removed or altered from any source            **
**      distribution.                                                        **
******************************************************************************/

/* Locally edited by too 2012-02-24-> -- above license holds for this part */

// mode constants
#define STEREO       0
#define JOINT_STEREO 1
#define DUAL_CHANNEL 2
#define MONO         3

// sample rate table
//static const int sample_rates[4] = { 44100, 48000, 32000, 0 };

// bitrate table
//static const int bitrates[14] =
//    { 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 };

// scale factors (24-bit fixed-point)
static const int scf_value[64] = {
    0x02000000, 0x01965FEA, 0x01428A30, 0x01000000,
    0x00CB2FF5, 0x00A14518, 0x00800000, 0x006597FB,
    0x0050A28C, 0x00400000, 0x0032CBFD, 0x00285146,
    0x00200000, 0x001965FF, 0x001428A3, 0x00100000,
    0x000CB2FF, 0x000A1451, 0x00080000, 0x00065980,
    0x00050A29, 0x00040000, 0x00032CC0, 0x00028514,
    0x00020000, 0x00019660, 0x0001428A, 0x00010000,
    0x0000CB30, 0x0000A145, 0x00008000, 0x00006598,
    0x000050A3, 0x00004000, 0x000032CC, 0x00002851,
    0x00002000, 0x00001966, 0x00001429, 0x00001000,
    0x00000CB3, 0x00000A14, 0x00000800, 0x00000659,
    0x0000050A, 0x00000400, 0x0000032D, 0x00000285,
    0x00000200, 0x00000196, 0x00000143, 0x00000100,
    0x000000CB, 0x000000A1, 0x00000080, 0x00000066,
    0x00000051, 0x00000040, 0x00000033, 0x00000028,
    0x00000020, 0x00000019, 0x00000014, 0
};

// synthesis window
static const int D[512] = {
     0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,-0x00001,
    -0x00001,-0x00001,-0x00001,-0x00002,-0x00002,-0x00003,-0x00003,-0x00004,
    -0x00004,-0x00005,-0x00006,-0x00006,-0x00007,-0x00008,-0x00009,-0x0000A,
    -0x0000C,-0x0000D,-0x0000F,-0x00010,-0x00012,-0x00014,-0x00017,-0x00019,
    -0x0001C,-0x0001E,-0x00022,-0x00025,-0x00028,-0x0002C,-0x00030,-0x00034,
    -0x00039,-0x0003E,-0x00043,-0x00048,-0x0004E,-0x00054,-0x0005A,-0x00060,
    -0x00067,-0x0006E,-0x00074,-0x0007C,-0x00083,-0x0008A,-0x00092,-0x00099,
    -0x000A0,-0x000A8,-0x000AF,-0x000B6,-0x000BD,-0x000C3,-0x000C9,-0x000CF,
     0x000D5, 0x000DA, 0x000DE, 0x000E1, 0x000E3, 0x000E4, 0x000E4, 0x000E3,
     0x000E0, 0x000DD, 0x000D7, 0x000D0, 0x000C8, 0x000BD, 0x000B1, 0x000A3,
     0x00092, 0x0007F, 0x0006A, 0x00053, 0x00039, 0x0001D,-0x00001,-0x00023,
    -0x00047,-0x0006E,-0x00098,-0x000C4,-0x000F3,-0x00125,-0x0015A,-0x00190,
    -0x001CA,-0x00206,-0x00244,-0x00284,-0x002C6,-0x0030A,-0x0034F,-0x00396,
    -0x003DE,-0x00427,-0x00470,-0x004B9,-0x00502,-0x0054B,-0x00593,-0x005D9,
    -0x0061E,-0x00661,-0x006A1,-0x006DE,-0x00718,-0x0074D,-0x0077E,-0x007A9,
    -0x007D0,-0x007EF,-0x00808,-0x0081A,-0x00824,-0x00826,-0x0081F,-0x0080E,
     0x007F5, 0x007D0, 0x007A0, 0x00765, 0x0071E, 0x006CB, 0x0066C, 0x005FF,
     0x00586, 0x00500, 0x0046B, 0x003CA, 0x0031A, 0x0025D, 0x00192, 0x000B9,
    -0x0002C,-0x0011F,-0x00220,-0x0032D,-0x00446,-0x0056B,-0x0069B,-0x007D5,
    -0x00919,-0x00A66,-0x00BBB,-0x00D16,-0x00E78,-0x00FDE,-0x01148,-0x012B3,
    -0x01420,-0x0158C,-0x016F6,-0x0185C,-0x019BC,-0x01B16,-0x01C66,-0x01DAC,
    -0x01EE5,-0x02010,-0x0212A,-0x02232,-0x02325,-0x02402,-0x024C7,-0x02570,
    -0x025FE,-0x0266D,-0x026BB,-0x026E6,-0x026ED,-0x026CE,-0x02686,-0x02615,
    -0x02577,-0x024AC,-0x023B2,-0x02287,-0x0212B,-0x01F9B,-0x01DD7,-0x01BDD,
     0x019AE, 0x01747, 0x014A8, 0x011D1, 0x00EC0, 0x00B77, 0x007F5, 0x0043A,
     0x00046,-0x003E5,-0x00849,-0x00CE3,-0x011B4,-0x016B9,-0x01BF1,-0x0215B,
    -0x026F6,-0x02CBE,-0x032B3,-0x038D3,-0x03F1A,-0x04586,-0x04C15,-0x052C4,
    -0x05990,-0x06075,-0x06771,-0x06E80,-0x0759F,-0x07CCA,-0x083FE,-0x08B37,
    -0x09270,-0x099A7,-0x0A0D7,-0x0A7FD,-0x0AF14,-0x0B618,-0x0BD05,-0x0C3D8,
    -0x0CA8C,-0x0D11D,-0x0D789,-0x0DDC9,-0x0E3DC,-0x0E9BD,-0x0EF68,-0x0F4DB,
    -0x0FA12,-0x0FF09,-0x103BD,-0x1082C,-0x10C53,-0x1102E,-0x113BD,-0x116FB,
    -0x119E8,-0x11C82,-0x11EC6,-0x120B3,-0x12248,-0x12385,-0x12467,-0x124EF,
     0x1251E, 0x124F0, 0x12468, 0x12386, 0x12249, 0x120B4, 0x11EC7, 0x11C83,
     0x119E9, 0x116FC, 0x113BE, 0x1102F, 0x10C54, 0x1082D, 0x103BE, 0x0FF0A,
     0x0FA13, 0x0F4DC, 0x0EF69, 0x0E9BE, 0x0E3DD, 0x0DDCA, 0x0D78A, 0x0D11E,
     0x0CA8D, 0x0C3D9, 0x0BD06, 0x0B619, 0x0AF15, 0x0A7FE, 0x0A0D8, 0x099A8,
     0x09271, 0x08B38, 0x083FF, 0x07CCB, 0x075A0, 0x06E81, 0x06772, 0x06076,
     0x05991, 0x052C5, 0x04C16, 0x04587, 0x03F1B, 0x038D4, 0x032B4, 0x02CBF,
     0x026F7, 0x0215C, 0x01BF2, 0x016BA, 0x011B5, 0x00CE4, 0x0084A, 0x003E6,
    -0x00045,-0x00439,-0x007F4,-0x00B76,-0x00EBF,-0x011D0,-0x014A7,-0x01746,
     0x019AE, 0x01BDE, 0x01DD8, 0x01F9C, 0x0212C, 0x02288, 0x023B3, 0x024AD,
     0x02578, 0x02616, 0x02687, 0x026CF, 0x026EE, 0x026E7, 0x026BC, 0x0266E,
     0x025FF, 0x02571, 0x024C8, 0x02403, 0x02326, 0x02233, 0x0212B, 0x02011,
     0x01EE6, 0x01DAD, 0x01C67, 0x01B17, 0x019BD, 0x0185D, 0x016F7, 0x0158D,
     0x01421, 0x012B4, 0x01149, 0x00FDF, 0x00E79, 0x00D17, 0x00BBC, 0x00A67,
     0x0091A, 0x007D6, 0x0069C, 0x0056C, 0x00447, 0x0032E, 0x00221, 0x00120,
     0x0002D,-0x000B8,-0x00191,-0x0025C,-0x00319,-0x003C9,-0x0046A,-0x004FF,
    -0x00585,-0x005FE,-0x0066B,-0x006CA,-0x0071D,-0x00764,-0x0079F,-0x007CF,
     0x007F5, 0x0080F, 0x00820, 0x00827, 0x00825, 0x0081B, 0x00809, 0x007F0,
     0x007D1, 0x007AA, 0x0077F, 0x0074E, 0x00719, 0x006DF, 0x006A2, 0x00662,
     0x0061F, 0x005DA, 0x00594, 0x0054C, 0x00503, 0x004BA, 0x00471, 0x00428,
     0x003DF, 0x00397, 0x00350, 0x0030B, 0x002C7, 0x00285, 0x00245, 0x00207,
     0x001CB, 0x00191, 0x0015B, 0x00126, 0x000F4, 0x000C5, 0x00099, 0x0006F,
     0x00048, 0x00024, 0x00002,-0x0001C,-0x00038,-0x00052,-0x00069,-0x0007E,
    -0x00091,-0x000A2,-0x000B0,-0x000BC,-0x000C7,-0x000CF,-0x000D6,-0x000DC,
    -0x000DF,-0x000E2,-0x000E3,-0x000E3,-0x000E2,-0x000E0,-0x000DD,-0x000D9,
     0x000D5, 0x000D0, 0x000CA, 0x000C4, 0x000BE, 0x000B7, 0x000B0, 0x000A9,
     0x000A1, 0x0009A, 0x00093, 0x0008B, 0x00084, 0x0007D, 0x00075, 0x0006F,
     0x00068, 0x00061, 0x0005B, 0x00055, 0x0004F, 0x00049, 0x00044, 0x0003F,
     0x0003A, 0x00035, 0x00031, 0x0002D, 0x00029, 0x00026, 0x00023, 0x0001F,
     0x0001D, 0x0001A, 0x00018, 0x00015, 0x00013, 0x00011, 0x00010, 0x0000E,
     0x0000D, 0x0000B, 0x0000A, 0x00009, 0x00008, 0x00007, 0x00007, 0x00006,
     0x00005, 0x00005, 0x00004, 0x00004, 0x00003, 0x00003, 0x00002, 0x00002,
     0x00002, 0x00002, 0x00001, 0x00001, 0x00001, 0x00001, 0x00001, 0x00001
};


///////////// Table 3-B.2: Possible quantization per subband ///////////////////

// quantizer lookup, step 1: bitrate classes
static const char quant_lut_step1[2][16] = {
    // 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384 <- bitrate
    {   0,  0,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2 },  // mono
    // 16, 24, 28, 32, 40, 48, 56, 64, 80, 96,112,128,160,192 <- BR / chan
    {   0,  0,  0,  0,  0,  0,  1,  1,  1,  2,  2,  2,  2,  2 }   // stereo
};

// quantizer lookup, step 2: bitrate class, sample rate -> B2 table idx, sblimit
#define QUANT_TAB_A (27 | 64)   // Table 3-B.2a: high-rate, sblimit = 27
#define QUANT_TAB_B (30 | 64)   // Table 3-B.2b: high-rate, sblimit = 30
#define QUANT_TAB_C   8         // Table 3-B.2c:  low-rate, sblimit =  8
#define QUANT_TAB_D  12         // Table 3-B.2d:  low-rate, sblimit = 12
static const char quant_lut_step2[3][4] = {
    //   44.1 kHz,      48 kHz,      32 kHz
    { QUANT_TAB_C, QUANT_TAB_C, QUANT_TAB_D },  // 32 - 48 kbit/sec/ch
    { QUANT_TAB_A, QUANT_TAB_A, QUANT_TAB_A },  // 56 - 80 kbit/sec/ch
    { QUANT_TAB_B, QUANT_TAB_A, QUANT_TAB_B },  // 96+     kbit/sec/ch
};

// quantizer lookup, step 3: B2 table, subband -> nbal, row index
// (upper 4 bits: nbal, lower 4 bits: row index)
static const char quant_lut_step3[2][32] = {
    // low-rate table (3-B.2c and 3-B.2d)
    { 0x44,0x44,                                                   // SB  0 -  1
      0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34            // SB  2 - 12
    },
    // high-rate table (3-B.2a and 3-B.2b)
    { 0x43,0x43,0x43,                                              // SB  0 -  2
      0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,                     // SB  3 - 10
      0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31, // SB 11 - 22
      0x20,0x20,0x20,0x20,0x20,0x20,0x20                           // SB 23 - 29
    }
};

// quantizer lookup, step 4: table row, allocation[] value -> quant table index
static const char quant_lut_step4[5][16] = {
    { 0, 1, 2, 17 },
    { 0, 1, 2, 3, 4, 5, 6, 17 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17 },
    { 0, 1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 },
    { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17 }
};

// quantizer specification structure
struct quantizer_spec {
    uint16_t nlevels;
    char grouping;
    char cw_bits;
    uint16_t Smul, Sdiv;
};

// quantizer table
static const struct quantizer_spec quantizer_table[17] = {
    {     3, 1,  5, 0x7FFF, 0xFFFF },
    {     5, 1,  7, 0x3FFF, 0x0002 },
    {     7, 0,  3, 0x2AAA, 0x0003 },
    {     9, 1, 10, 0x1FFF, 0x0002 },
    {    15, 0,  4, 0x1249, 0xFFFF },
    {    31, 0,  5, 0x0888, 0x0003 },
    {    63, 0,  6, 0x0421, 0xFFFF },
    {   127, 0,  7, 0x0208, 0x0009 },
    {   255, 0,  8, 0x0102, 0x007F },
    {   511, 0,  9, 0x0080, 0x0002 },
    {  1023, 0, 10, 0x0040, 0x0009 },
    {  2047, 0, 11, 0x0020, 0x0021 },
    {  4095, 0, 12, 0x0010, 0x0089 },
    {  8191, 0, 13, 0x0008, 0x0249 },
    { 16383, 0, 14, 0x0004, 0x0AAB },
    { 32767, 0, 15, 0x0002, 0x3FFF },
    { 65535, 0, 16, 0x0001, 0xFFFF }
};


////////////////////////////////////////////////////////////////////////////////
// STATIC VARIABLES AND FUNCTIONS                                             //
////////////////////////////////////////////////////////////////////////////////

static int S_bit_window;
static int S_bits_in_window;
static const unsigned char *S_frame_pos;

#define show_bits(bit_count) (S_bit_window >> (24 - (bit_count)))

static int get_bits(int bit_count) {
    int result = show_bits(bit_count);
    S_bit_window = (S_bit_window << bit_count) & 0xFFFFFF;
    S_bits_in_window -= bit_count;
    while (S_bits_in_window < 16) {
        S_bit_window |= (*S_frame_pos++) << (16 - S_bits_in_window);
        S_bits_in_window += 8;
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////
// INITIALIZATION                                                             //
////////////////////////////////////////////////////////////////////////////////

static int N[64][32];  // N[i][j] as 8-bit fixed-point

static void mp2_init(struct mp2_ctx_t * mp2)
{
    int i, j;
    int *nptr = &N[0][0];

    // compute N[i][j]
    for (i = 0;  i < 64;  ++i)
      for (j = 0;  j < 32;  ++j)
	*nptr++ = (int) (256.0 * cos(((16 + i) * ((j << 1) + 1)) * 0.0490873852123405));

    memset(&mp2->V, 0, sizeof mp2->V);
    mp2->Voffs = 0;
}

////////////////////////////////////////////////////////////////////////////////
// DECODE HELPER FUNCTIONS                                                    //
////////////////////////////////////////////////////////////////////////////////

static const struct quantizer_spec* read_allocation(int sb, int b2_table) {
    int table_idx = quant_lut_step3[b2_table][sb];
    table_idx = quant_lut_step4[table_idx & 15][get_bits(table_idx >> 4)];
    return table_idx ? (&quantizer_table[table_idx - 1]) : 0;
}


static void read_samples(const struct quantizer_spec *q,
			 int scalefactor, int *sample) {
    int idx, adj;
    register int val;
    if (!q) {
        // no bits allocated for this subband
        sample[0] = sample[1] = sample[2] = 0;
        return;
    }
    // resolve scalefactor
        scalefactor = scf_value[scalefactor];

    // decode samples
    adj = q->nlevels;
    if (q->grouping) {
        // decode grouped samples
        val = get_bits(q->cw_bits);
        sample[0] = val % adj;
        val /= adj;
        sample[1] = val % adj;
        sample[2] = val / adj;
    } else {
        // decode direct samples
        for(idx = 0;  idx < 3;  ++idx)
            sample[idx] = get_bits(q->cw_bits);
    }

    // postmultiply samples
    adj = ((adj + 1) >> 1) - 1;
    for (idx = 0;  idx < 3;  ++idx) {
        // step 1: renormalization to [-1..1]
        val = adj - sample[idx];
        val = (val * q->Smul) + (val / q->Sdiv);
        // step 2: apply scalefactor
        sample[idx] = ( val * (scalefactor >> 12)                  // upper part
                    + ((val * (scalefactor & 4095) + 2048) >> 12)) // lower part
                    >> 12;  // scale adjust
    }
}


////////////////////////////////////////////////////////////////////////////////
// FRAME DECODE FUNCTION                                                      //
////////////////////////////////////////////////////////////////////////////////

static const struct quantizer_spec *allocation[2][32];
static int scfsi[2][32];
static int scalefactor[2][32][3];
static int sample[2][32][3];
static int U[512];

static void mp2_decode_frame(struct mp2_ctx_t * mp2,
			     const Mp2Info * m2i, int16_t * xmonopcm)
{
#if 0
    unsigned bit_rate_index_minus1;
    unsigned sampling_frequency;
    unsigned padding_bit;
    unsigned mode;
    unsigned long frame_size;
    int bound, sblimit;
    int sb, ch, gr, part, idx, nch, i, j, sum;
    int table_idx;

    // general sanity check
    if (!initialized || !mp2 || (mp2->id != KJMP2_MAGIC) || !frame)
        return 0;

    // check for valid header: syncword OK, MPEG-Audio Layer 2
    if ((frame[0] != 0xFF) || ((frame[1] & 0xFE) != 0xFC))
        return 0;

    // set up the bitstream reader
    bit_window = frame[2] << 16;
    bits_in_window = 8;
    frame_pos = &frame[3];

    // read the rest of the header
    bit_rate_index_minus1 = get_bits(4) - 1;
    if (bit_rate_index_minus1 > 13)
        return 0;  // invalid bit rate or 'free format'
    sampling_frequency = get_bits(2);
    if (sampling_frequency == 3)
        return 0;
    padding_bit = get_bits(1);
    get_bits(1);  // discard private_bit
    mode = get_bits(2);

    // parse the mode_extension, set up the stereo bound
    if (mode == JOINT_STEREO) {
        bound = (get_bits(2) + 1) << 2;
    } else {
        get_bits(2);
        bound = (mode == MONO) ? 0 : 32;
    }

    // discard the last 4 bits of the header and the CRC value, if present
    get_bits(4);
    if ((frame[1] & 1) == 0)
        get_bits(16);

    // compute the frame size
    frame_size = (144000 * bitrates[bit_rate_index_minus1]
               / sample_rates[sampling_frequency]) + padding_bit;
    if (!pcm)
        return frame_size;  // no decoding
#endif

    //unsigned long frame_size;
    int bound, sblimit;
    int sb, ch, gr, part, idx, nch, i, j, sum;
    int table_idx;

    //frame_size = 144000 * m2i->bitrate / m2i->samplerate + m2i->pad;
    unsigned mode = m2i->mode;
    if (m2i->mode == JOINT_STEREO)
        bound = (m2i->mex + 1) << 2;
    else
        bound = (mode == MONO) ? 0 : 32;

    S_frame_pos = m2i->frame;
    S_bit_window = (*S_frame_pos++) << 16;
    S_bits_in_window = 8;

    // prepare the quantizer table lookups
    table_idx = (mode == MONO) ? 0 : 1;
    table_idx = quant_lut_step1[table_idx][m2i->bi - 1];
    table_idx = quant_lut_step2[table_idx][m2i->si];
    sblimit = table_idx & 63;
    table_idx >>= 6;
    if (bound > sblimit)
        bound = sblimit;

    // read the allocation information
    for (sb = 0;  sb < bound;  ++sb)
        for (ch = 0;  ch < 2;  ++ch)
            allocation[ch][sb] = read_allocation(sb, table_idx);
    for (sb = bound;  sb < sblimit;  ++sb)
        allocation[0][sb] = allocation[1][sb] = read_allocation(sb, table_idx);

    // read scale factor selector information
    nch = (mode == MONO) ? 1 : 2;
    for (sb = 0;  sb < sblimit;  ++sb) {
        for (ch = 0;  ch < nch;  ++ch)
            if (allocation[ch][sb])
                scfsi[ch][sb] = get_bits(2);
        if (mode == MONO)
            scfsi[1][sb] = scfsi[0][sb];
    }

    // read scale factors
    for (sb = 0;  sb < sblimit;  ++sb) {
        for (ch = 0;  ch < nch;  ++ch)
            if (allocation[ch][sb]) {
                switch (scfsi[ch][sb]) {
                    case 0: scalefactor[ch][sb][0] = get_bits(6);
                            scalefactor[ch][sb][1] = get_bits(6);
                            scalefactor[ch][sb][2] = get_bits(6);
                            break;
                    case 1: scalefactor[ch][sb][0] =
                            scalefactor[ch][sb][1] = get_bits(6);
                            scalefactor[ch][sb][2] = get_bits(6);
                            break;
                    case 2: scalefactor[ch][sb][0] =
                            scalefactor[ch][sb][1] =
                            scalefactor[ch][sb][2] = get_bits(6);
                            break;
                    case 3: scalefactor[ch][sb][0] = get_bits(6);
                            scalefactor[ch][sb][1] =
                            scalefactor[ch][sb][2] = get_bits(6);
                            break;
                }
            }
        if (mode == MONO)
            for (part = 0;  part < 3;  ++part)
                scalefactor[1][sb][part] = scalefactor[0][sb][part];
    }

    // coefficient input and reconstruction
    for (part = 0;  part < 3;  ++part)
        for (gr = 0;  gr < 4;  ++gr) {

            // read the samples
            for (sb = 0;  sb < bound;  ++sb)
                for (ch = 0;  ch < 2;  ++ch)
                    read_samples(allocation[ch][sb], scalefactor[ch][sb][part], &sample[ch][sb][0]);
            for (sb = bound;  sb < sblimit;  ++sb) {
                read_samples(allocation[0][sb], scalefactor[0][sb][part], &sample[0][sb][0]);
                for (idx = 0;  idx < 3;  ++idx)
                    sample[1][sb][idx] = sample[0][sb][idx];
            }
            for (ch = 0;  ch < 2;  ++ch)
               for (sb = sblimit;  sb < 32;  ++sb)
                    for (idx = 0;  idx < 3;  ++idx)
                        sample[ch][sb][idx] = 0;

            // synthesis loop
            for (idx = 0;  idx < 3;  ++idx) {
                // shifting step
                mp2->Voffs = table_idx = (mp2->Voffs - 64) & 1023;

                for (ch = 0;  ch < 2;  ++ch) {
                    // matrixing
                    for (i = 0;  i < 64;  ++i) {
                        sum = 0;
                        for (j = 0;  j < 32;  ++j)
                            sum += N[i][j] * sample[ch][j][idx];  // 8b*15b=23b
                        // intermediate value is 28 bit (23 + 5), clamp to 14b
                        mp2->V[ch][table_idx + i] = (sum + 8192) >> 14;
                    }

                    // construction of U
                    for (i = 0;  i < 8;  ++i)
                        for (j = 0;  j < 32;  ++j) {
                            U[(i << 6) + j]      = mp2->V[ch][(table_idx + (i << 7) + j     ) & 1023];
                            U[(i << 6) + j + 32] = mp2->V[ch][(table_idx + (i << 7) + j + 96) & 1023];
                        }

                    // apply window
                    for (i = 0;  i < 512;  ++i)
                        U[i] = (U[i] * D[i] + 32) >> 6;

                    // output samples
                    for (j = 0;  j < 32;  ++j) {
                        sum = 0;
                        for (i = 0;  i < 16;  ++i)
                            sum -= U[(i << 5) + j];
                        sum = (sum + 8) >> 4;
                        if (sum < 0) sum = -sum;
                        if (sum > 32767) sum = 32767;
			int ix = (idx << 5) | j;
			if (ch == 1) {
			  if (xmonopcm[ix] < (int16_t)sum)
			    xmonopcm[ix] = (int16_t)sum;
			}
			else
			  xmonopcm[ix] = (int16_t) sum;
                    }
                } // end of synthesis channel loop
            } // end of synthesis sub-block loop

            // adjust PCM output pointer: decoded 3 * 32 = 96 samples
            xmonopcm += 96;

        } // decoding of the granule finished
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "stroustrup"
 * tab-width: 8
 * End:
 */
