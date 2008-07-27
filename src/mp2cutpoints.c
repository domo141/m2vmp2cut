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
 * Last modified: Tue Jun 03 22:51:36 EEST 2008 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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
      if ((l = read(self->fd, self->buf + rest, sizeof self->buf - rest)) <= 0)
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
static const int siarr[] = { 44100, 48000, 32000 };

/*                    84218421 84218421 84218421 84218421
 *                    76543210 76543210 76543210 76543210
 * Mpeg audio header: AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM
 */

typedef unsigned char u8;
typedef struct
{
  u8 id; /*  B above (mpeg audio version id)  */
  u8 ld; /*  C above (layer description)  */

  u8 bi; /*  E above (bitrate index)  */
  u8 si; /*  F above (sampling rate frequency index)  */
  u8 pad; /* G above (padding bit)  */
  int afn;
  int ms;
  int bitrate;
  int samplerate;
  simplefilebuf * sfb;
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
  /*u8 pb = p[0] & 0x01; / *  D above (Protection bit)  */

  m2i->bi = p[1] & 0xf0; /*  E above (bitrate index)  */
  m2i->si = p[1] & 0x0c; /*  F above (sampling rate frequency index)  */
  m2i->pad= p[1] & 0x02; /*  G above (padding bit)  */

  m2i->bitrate = biarr[m2i->bi >> 4];
  m2i->samplerate = siarr[m2i->si >> 2];

  m2i->afn++;

  if (m2i->id != 0x18)
    xerrf("Frame %d (pos %d) not mpeg version 1.\n", m2i->afn,
	  simplefilebuf_filepos(m2i->sfb) - 4);
  if (m2i->ld != 0x04)
    xerrf("Frame %d (pos %d) not mpeg layer 2.\n", m2i->afn,
	  simplefilebuf_filepos(m2i->sfb) - 4);

  flib = 144000 * m2i->bitrate / m2i->samplerate + (m2i->pad >> 1);
  m2i->ms = flib * 8 / m2i->bitrate;

  (void)simplefilebuf_needbytes(m2i->sfb, flib - 4);

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
      printf("%lld-%lld", positions[0], positions[1]);
      for (i = 2; i < c; i += 2)
	printf(",%lld-%lld", positions[i], positions[i + 1]);
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

void scan(char * ifile, char * sfile)
{
  int sfd = wopen(sfile);
  simplefilebuf sfb;
  Mp2Info m2i;
  int fsize;

  int totaltime = 0, prevtotal = 0;
  int position = 0, prevposx = 0;
  int cpc, ppc = 0;
  int pbr = 0, psr = 0;

  memset(&m2i, 0, sizeof m2i);
  simplefilebuf_init(&sfb, ifile);
  m2i.sfb = &sfb;
  fsize = filesize(sfb.fd);

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
	  exit(0);
	}

      if (skipped > 0)
	{
	  fprintf(stderr, "Skipped %d bytes of garbage before audio frame %d.",
		  skipped, m2i.afn + 1);
	  skipped = 0;
	}
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
	  prevtotal = totaltime;
	  totaltime += m2i.ms;
	}
      else
	simplefilebuf_unusedbytes(&sfb, 3);
    }
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
		   i & 1? "out": "in ", times[i], simplefilebuf_filepos(&sfb));
	  fdprintf(ofd,
		   "File ended: audio frames: %d Total time: %d ms (%s).\n",
		   m2i.afn, totaltime, ms2tcode(totaltime));
	  fdprintf(0, "\r- Scanning audio at %d ms of %d ms (100%%).\n",
		   lasttime, lasttime);
	  printpositions(positions, i);
	  exit(0);
	}

      if (skipped > 0)
	{
	  fprintf(stderr, "Skipped %d bytes of garbage before audio frame %d.",
		  skipped, m2i.afn + 1);
	  skipped = 0;
	}
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
    xerrf("Usage: %s (--scan|timespec) ifile ofile [sfile]\n", argv[0]);

  if (strcmp(argv[1], "--scan") == 0)
    scan(argv[2], argv[3]);
  else
    cutpoints(argv[1], argv[2], argv[3], argv[4]);
  return 0;
}

/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
