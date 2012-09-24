#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -e; TRG=`basename $0 .c`; rm -f "$TRG"
 WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 set -x
 #${CC:-gcc} -ggdb -std=c99 $WARN "$@" -o "$TRG" "$0"
 exec ${CC:-gcc} -O2 -std=c99 $WARN "$@" -o "$TRG" "$0" obj_b/x.o
 exit $?
 */
#endif
/*
 * $Id; wavgraph.c $
 *
 * Author: Tomi Ollila -- too ät iki fi
 *
 *	Copyright (c) 2008 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Fri Jun 06 17:45:42 EEST 2008 too
 * Last modified: Wed 19 Sep 2012 17:54:32 EEST too
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

#include "x.h"

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

bool simplefilebuf_fillbuf(simplefilebuf * self, unsigned int rest)
{
  int l;

  if (rest > 0) {
      memmove(self->buf, self->buf + self->offset, rest);
      if ((l = read(self->fd, self->buf + rest, sizeof self->buf - rest)) <= 0)
	  return false;
      self->gone += self->offset;
  }
  else {
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

unsigned char * simplefilebuf_needbytes(simplefilebuf * self, int bytes)
{
    while (true)
    {
	int rest = self->len - self->offset;

	if (rest >= bytes) {
	    int oo = self->offset;
	    self->offset += bytes;

	    return self->buf + oo;
	}
	if (! simplefilebuf_fillbuf(self, rest))
	    return NULL;
    }
}

#if 0
int simplefilebuf_rest(simplefilebuf * self, unsigned char ** pp)
{
    int rest = self->len - self->offset;
    *pp = simplefilebuf_needbytes(self, rest);
    return rest;
}
#endif

static unsigned char * xneedbytes(simplefilebuf * self, int bytes)
{
    unsigned char * p = simplefilebuf_needbytes(self, bytes);
    if (p == NULL)
	xerrf("No needed bytes (%d) in input\n", bytes);
    return p;
}

static void * xcmalloc(int size, char c)
{
    char * p = malloc(size);
    if (p == NULL)
	xerrf("Out of Memory!");
    memset(p, c, size);
    return p;
}

struct AState {
    int channels;
    int samplerate;
    int blocksize;
    int bitspersample;
    int currentblock;
    int samplesperchannel;
    int msecs;
    int graphwidth;
    int *graphdata[2];
    int maxval[2];
};

static void dograph(struct AState * astate, int height, int centerpos,
		    const char * ofile, int fps1, int fps2)
{
    int i, width = astate->graphwidth;
    char * gd = xcmalloc(width * height, ' ');

    height /= 2;
    int middleleft = height * width;

    int div0 = astate->maxval[0] / height;
    int div1 = astate->maxval[1] / height;

    for (i = 0; i < width; i++)
    {
	int c;

	if (div0 > 0)
	{
	    int v = astate->graphdata[0][i];
	    if (v < 0) { c = '|'; v = -v; } else c = '!';
	    int h = v / div0;
	    //printf("vg:%d: %d %d %d\n", __LINE__, v, height * v / 65535, h);
	    for (int k = 0; k < h; k++)
		gd[i + (height - k) * width] = c;
	}
	if (div1 > 0)
	{
	    int v = astate->graphdata[1][i];
	    if (v < 0) { c = '|'; v = -v; } else c = '!';
	    int h = v / div1;

	    //printf("vg:%d: %d %d %d\n", __LINE__, v, height * v / 65535, h);
	    for (int k = 0; k < h; k++)
		gd[i + (height + k) * width] = c;

	    gd[i + middleleft] = '-';
	}
    }

    height *= 2;

    for (i = 0; i < height; i+= 2)
	gd[centerpos + i * width] = ':';

    if (fps1 > 0 && fps2 > 0) {
	for (i = 1; i < 22; i++) {
	    int dist =
		(i * astate->graphwidth * fps2) / fps1 * 1000 / astate->msecs;
	    if (centerpos - dist >= 0)
		gd[middleleft + centerpos - dist] = ':';
	    if (centerpos + dist < astate->graphwidth)
		gd[middleleft + centerpos + dist] = ':';
	    //printf("%d\n", dist);
	}
    }

    FILE * of = fopen(ofile, "w");
    if (of == NULL)
	xerrf("Can not open '%s':", ofile);

    fprintf(of, "/* XPM */\n"
	    "static char *graph[] = {\n"
	    "/* width height num_colors chars_per_pixel */\n"
	    "\"   %3d    %3d     5            1\",\n"
	    "/* colors */\n"
	    "\"  c #000000\",\n"
	    "\": c #e0e0e0\",\n"
	    "\"- c #0080c0\",\n"
	    "\"| c #770077\",\n"
	    "\"! c #660066\",\n"
	    "/* pixels */\n", width, height);

    for (i = 0; i < height; i++) {
	fputc('"', of);
	fwrite(gd + i * width, 1, width, of);
	fputs("\",\n", of);
    }
    fputs("};\n", of);
    fclose(of);
}


static void storedata(struct AState * astate,
		       int channel, int block, int value)
{
    if (channel > 1)
	return;

    switch (astate->bitspersample)
    {
    case 8:  value = (int)((int8_t)value); break ;
    case 16: value = (int)((int16_t)value); break ;
    case 32: break ;
    default:
	xerrf("Unsupported bytes per sample value %d\n",astate->bitspersample);
    }
    int gw = astate->graphwidth;
    int cb = astate->currentblock;

    int pos = (cb * astate->blocksize + block) *gw / astate->samplesperchannel;
    int maxval = astate->graphdata[channel][pos];

    //if (value < 0) value = -value; // XXX

    // if value positive, overwrite any negative maxval.
    if (value > 0) {
	if (value > maxval) astate->graphdata[channel][pos] = value;
	if (value > astate->maxval[channel]) astate->maxval[channel] = value;
    }
    else if (maxval <= 0)
	if (value < maxval) astate->graphdata[channel][pos] = value;
	if (-value > astate->maxval[channel]) astate->maxval[channel] = -value;
}

int main(int argc, char * argv[])
{
    simplefilebuf sfb;

    if (argc < 6)
	xerrf("usage: %s [-d] ifile ofile "
	      "picwidth picheight centerpos [vfps1 vfps2]\n", argv[0]);

    int graphwidth = atoi(argv[3]);
    int graphheight = atoi(argv[4]);
    int centerpos = atoi(argv[5]);

    if (centerpos < 0) centerpos = 0;
    else if (centerpos >= graphwidth)
	centerpos = graphwidth - 1;

    if (graphwidth < 200)
	xerrf("Requested width (%d) too narrow (<200)\n", graphwidth);
    if (graphwidth > 1200) {
	centerpos = centerpos * 1200 / graphwidth;
	graphwidth = 1200;
    }
    if (graphheight < 100)
	xerrf("Requested height (%d) too short (<100)\n", graphheight);
    if (graphheight > 600)
	graphheight = 600;

    int * graphdata = xcmalloc(2 * graphwidth * sizeof graphdata[0], 0);

    int fps1, fps2;
    if (argv[6] && argv[7]) {
	fps1 = atoi(argv[6]);
	fps2 = atoi(argv[7]);
    }
    else fps1 = fps2 = 0;

    simplefilebuf_init(&sfb, argv[1]);

    unsigned char * p = xneedbytes(&sfb, 4);
    if (memcmp(p, "RIFF", 4))
	xerrf("No RIFF header in '%s'\n", argv[1]);

    p = xneedbytes(&sfb, 4);
    //int rifflen = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);

    //printf("rifflen %d\n", rifflen);

    p = xneedbytes(&sfb, 4);
    if (memcmp(p, "WAVE", 4))
	xerrf("No WAVE header in '%s'\n", argv[1]);

    p = xneedbytes(&sfb, 4);
    if (memcmp(p, "fmt ", 4))
	xerrf("No 'fmt ' header in '%s'\n", argv[1]);

    p = xneedbytes(&sfb, 4);
    if (memcmp(p, "\020\000\000\000", 4))
	xerrf("'fmt ' subchunk size not 16 in '%s'\n", argv[1]);

    p = xneedbytes(&sfb, 0x10);

    if (p[0] != 1 || p[1] != 0)
	xerrf("WAVE audio format not PCM\n");

    int channels = p[2] + (p[3] << 8);
    int samplerate = p[4] + (p[5] << 8) + (p[6] << 16) + (p[7] << 24);
    //int byterate = p[8] + (p[9] << 8) + (p[10] << 16) + (p[11] << 24);
    int blockalign = p[12] + (p[13] << 8);
    int bitspersample = p[14] + (p[15] << 8);

    //printf("c %d sr %d br %d ba %d bps %d\n",
    //   channels, samplerate, byterate, blockalign, bitspersample);

    p = xneedbytes(&sfb, 4);
    if (memcmp(p, "data", 4))
	xerrf("No 'data' header in '%s'\n", argv[1]);
    p = xneedbytes(&sfb, 4);
    int datalen = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);

    //printf("datalen %d\n", datalen);

    //int rest = simplefilebuf_rest(&sfb, &p);

    int bytespersample = bitspersample / 8;
    struct AState astate = {
	.channels = channels,
	.samplerate = samplerate,
	.blocksize = blockalign,
	.bitspersample = bitspersample,
	.currentblock = 0,
	.samplesperchannel = datalen / (channels * bytespersample),
	.msecs = datalen / (channels * samplerate * bytespersample / 1000),
	.graphwidth = graphwidth,
	.graphdata = { graphdata, graphdata + graphwidth },
	.maxval = { 0, 0 }
    };

    while (datalen > 0) {
	p = xneedbytes(&sfb, channels * blockalign * bytespersample);
	datalen -= channels * blockalign * bytespersample;
	for (int i = 0; i < channels; i++)
	    for (int j = 0, val = 0; j < blockalign; j++) {
		for (int k = 0; k < bytespersample; k++) {
		    val = val | (*p++ << 8 * k);
		    //printf(" xx %d xx\n", *p);
		}
		storedata(&astate, i, j, val);
	    }
	astate.currentblock++;
    }
    dograph(&astate, graphheight, centerpos, argv[2], fps1, fps2);
    //printf("vg:%d: %d %d\n", __LINE__, astate.maxval[0], astate.maxval[1]);
    return 0;
}
