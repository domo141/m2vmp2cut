#if 0 /*
	archos=`uname -s -m | awk '{ print tolower($2) "-" tolower($1) }'`
	libc_so=`ldd /usr/bin/env | awk '/libc.so/ { print $3 }'`
	case $1 in -o) imf=$2.o trg=$2; shift 2
		;; *)  imf=`basename "$0" .c`; trg=$imf.so imf=$imf.o ;; esac
	trap "rm -f '$imf'" 0
	WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
	WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
	WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
	set -xeu
	case ${1-} in '') set x -O2; shift; esac
	gcc -fPIC -rdynamic -std=c99 "$@" -c $WARN "$0" -o "$imf" -DDBG=0 \
		-DARCHOS="\"$archos\"" -DLIBC_SO="\"$libc_so\""
	gcc -shared -Wl,-soname,"$trg" -o "$trg" "$imf" -lc -ldl
	chmod 644 "$trg"
	#gcc -fPIC -rdynamic -g -c $WARN "$0" -o "$bn.o" -DDBG=1 \
	#	-DARCHOS="\"$archos\"" -DLIBC_SO="\"$libc_so\""
	#gcc -shared -Wl,-soname,$dlbn.so -o $dlbn.so $bn.o -lc -ldl
	exit $?
      */
#endif
/*
 * $ libpreload_ffm2vtoyuv4mpeghax.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 2012 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Tue 09 Oct 2012 17:19:48 EEST too
 * Last modified: Sat 14 Feb 2015 23:25:20 +0200 too
 */
/*
 * Compile this as: sh libpreload_ffm2vtoyuv4mpeghax.c
 *
 * Use as:
 *   [DROP_YUV4MPEG_HEADER=yes] [DROP_YUV4MPEG_FRAMES=<count>] \
 *	LD_PRELOAD=libpreload_ffm2vtoyuv4mpeghax.so \
 *	ffmpeg -i pipe0: -f yuv4mpegpipe <other-args> -
 *
 * This program is licensed under GNU Public License (GPL) v2
 */

/*
 * This is somewhat hacky filter for ffmpeg(1) to be used in very particular
 * case: reading m2v input and producing yuv4mpeg output.
 * This does both m2v input filtering and yuv4mpeg output filtering.
 * The input filter patches read() c library function which provides interface
 * to read() system call by patching the aspect ratio field in m2v sequence
 * header to return constant value (2, representing 4:3 ratio) so that ffmpeg
 * does not do any additional processing there (like mangling output frames).
 * The output filter patches write() call. This used the environment variables
 * DROP_YUV4MPEG_HEADER and DROP_YUV4MPEG_FRAMES. In case DROP_YUV4MPEG_HEADER
 * is set to 'yes' the "YUV4MPEG2 W<width> H<height> F<format>..." is left out.
 * this is used to "concatenate" output from many ffmpeg(1) invocations to
 * a pipe so that reader doesn't consider stream to end... The other variable,
 * DROP_YUV4MPEG_FRAMES instruct this filter to drop given amount of first
 * frames before starting to produce output. This is needed so that ffmpeg(1)
 * can be given more m2v input (like 2 mpeg2 I-frames before first needed
 * output frame to sync with. Probably due to ffmpeg(1)'s realtime streaming
 * feature (or something??!!) if ffmpeg's select output filter were used
 * instead of this dropping feature ffmpeg just duplicates first selected frame
 * in place of dropped frames...
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#define null ((void *)0)

#if 0
#define DN1(s, n) do { char xb[32]; sprintf(xb, "%s: %d\n", s, n); \
	write_orig(2, xb, strlen(xb)); } while (0)
#define DN0(s, n) do {} while (0)
#endif

//static void fail(const char * str) GCCATTR_NORETURN;
static void fail(const char * str)
{
    write(2, str, strlen(str));
    write(2, ".\n", 2);
    exit(1);
}

ssize_t read(int fd, void * buf, size_t count)
{
    static int (*read_orig)(int, void *, size_t) = null;

    if (! read_orig)
    {
	void * handle = dlopen(LIBC_SO, RTLD_LAZY);

	if (!handle)
	    fail(dlerror());

	/* read_orig = dlsym(handle, "read"); */
	/* read_orig = (int(*)(int, ...))dlsym(handle, "read"); */
	*(void **)(&read_orig) = dlsym(handle, "read");

	char * error = dlerror();
	if (error != NULL)
	    fail(error);
    }

    ssize_t len = read_orig(fd, buf, count);

    if (fd == 0) {
	// sequence header (01B3)
	// 00 00 01 b3 <12bit hsize><12bit vsize> <asr-nibble:fr-nibble> ...
	static int m2vs = 0;
	unsigned char * p = buf;
	ssize_t l = len;
	while (l > 0) {
	    // "fix" constant m2v asr for ffmpeg
	    unsigned char c = *p;
	    switch (m2vs) {
	    case 0: if (c == 0)	m2vs = 1; break;
	    case 1: if (c == 0)	m2vs = 2; else m2vs = 0; break;
	    case 2: if (c == 1)	m2vs = 3; else m2vs = (c == 0)? 1: 0; break;
	    case 3: if (c == 0xb3) m2vs = 4; else m2vs = (c == 0)? 1: 0; break;
	    case 4: if (c != 0)	m2vs = 5; else m2vs = 8; break;
	    case 5: if (c != 0)	m2vs = 6; else m2vs = 9; break;
	    case 6: m2vs = 7; break;

	    // zero here would mean asr & frame rate 0...
	    case 7: if (c != 0) { *p = 0x20 + (c & 0x0f); m2vs = 0; }
		    else m2vs = 1; /* <^> */ break;
	    // if there are 2 consecutive \0:s here either h or v size is 0...
	    case 8: if (c != 0)	m2vs = 6; else m2vs = 1; break;
	    case 9: if (c != 0)	m2vs = 7; else m2vs = 1; break;
	    }
	    p++; l--;
	}
    }
    return len;
}

ssize_t write(int fd, const void * buf, size_t count)
{
    static int (*write_orig)(int, const void *, size_t) = null;

    if (! write_orig)
    {
	void * handle = dlopen(LIBC_SO, RTLD_LAZY);

	if (!handle)
	    fail(dlerror());

	*(void **)(&write_orig) = dlsym(handle, "write");

	char * error = dlerror();
	if (error != NULL)
	    fail(error);
    }

    static int stbf = 0;

    if (stbf || fd != 1)
	return write_orig(fd, buf, count);

    // else

    static int dfh = -1;
    if (dfh < 0) {
	const char * p = getenv("DROP_YUV4MPEG_HEADER");
	dfh = (p != null && strcmp(p, "yes") == 0);
    }
    static int dfc = -1;
    if (dfc < 0) {
	const char * p = getenv("DROP_YUV4MPEG_FRAMES");
	if (p == null)
	    dfc = 0;
	else dfc = atoi(p);
    }

    static int framesize = 0;
    char * hdrend = null;
    if (framesize == 0) {
	if (memcmp(buf, "YUV4MPEG2 ", 10) != 0)
	    fail("Cannot find YUV4MPEG2 header in first write");
	hdrend = memchr(buf, '\n', count);
	if (hdrend == null)
	    fail("Partial YUV4MPEG2 header in first write");
	char format[32];
	int width, height;
	if (sscanf(buf, "YUV4MPEG2 W%d H%d F%*s I%*s A%*s %30s",
		   &width, &height, format) != 3)
	    fail("Cannot parse YUV4MPEG2 header");
	framesize = width * height * 3 / 2;
	if (framesize == 0)
	    fail("Cannot parse frame size from YUV4MPEG2 header");
	if (strcmp(format, "C420jpeg") != 0
	    && strcmp(format, "C420mpeg2") != 0)
	    fail("YUV4MPEG2 format not C420jpeg nor C420mpeg2");
    }

    static int skipbytes = 0;
    if (dfc) {
	const char * p = buf;
	int l = count;
	if (skipbytes) {
	    if (l < skipbytes) {
		skipbytes -= l;
		return count;
	    }
	    p += skipbytes;
	    l -= skipbytes;
	    skipbytes = 0;
	    dfc--;
	    if (dfc == 0)
		goto wout;
	}

	if (hdrend) {
	    int hl = (hdrend - (const char *)buf) + 1;
	    if (dfh == 0)
		if (write_orig(fd, buf, hl) != hl)
		    fail("Partial YUV4MPEG2 header write");
	    p += hl;
	    l -= hl;
	}

	while (l > 0) {
	    if (l < 6)
		fail("Partial FRAME header in write");
	    if (memcmp(p, "FRAME\n", 6) != 0)
		fail("FRAME header missing");
	    l -= 6;
	    if (l < framesize) {
		skipbytes = framesize - l;
		break;
	    }
	    p += 6 + framesize;
	    dfc--;
	    if (dfc == 0) {
	    wout:
		if (l > 0) {
		    int ll = write_orig(fd, p, l);
		    if (ll >= 0)
			count -= (l - ll);
		    else
			count = -1;
		}
		stbf = 1;
		break;
	    }
	}
	return count;
    }
    stbf = 1;

    if (hdrend && dfh) {
	int hl = hdrend - (const char *)buf;
	int ll = write_orig(fd, (const char *)buf + hl, count - hl);
	if (ll >= 0)
	    return hl + ll;
	return -1;
    }
    return write_orig(fd, buf, count);
}
