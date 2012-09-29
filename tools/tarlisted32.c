#if 0 /*
 # $Id$
 #
 # Author: Tomi Ollila <tomi.ollila@iki.fi>
 #
 #	Copyright (c) 2006 Tomi Ollila
 #	    All rights reserved
 #
 # Created: Thu Apr 20 19:59:29 EEST 2006 too
 # Last modified: Tue 25 Sep 2012 21:25:53 EEST too
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #
 #   1. Redistributions of source code must retain the above copyright
 #      notice, this list of conditions and the following disclaimer.
 #   2. Redistributions in binary form must reproduce the above copyright
 #      notice, this list of conditions and the following disclaimer in
 #      the documentation and/or other materials provided with the
 #      distribution.
 #   4. The names of the authors may not be used to endorse or promote
 #      products derived from this software without specific prior
 #      written permission.
 #
 # THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 # IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 # WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 #
 TRG=`basename $0 .c` DATE=`date`
 [ x"$1" = xclean ] && { set -x; exec rm -f $TRG; }
 [ x"$1" = x ] && set -- -s -O2
 WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 XD="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
 set -x; exec ${CC:-gcc} $WARN "$@" -o $TRG $0
 #set -x; exec ${CC:-gcc} -Wall "$@" -o $TRG $0
 #*/
#endif

#define VERSION "3.2"

/* example content, run
   sed -n 's/#[:]#//p' tarlisted.c | ./tarlisted -V -o test.tar.gz '|' gzip -c

#:# tarlisted file format 3
#:# 755 root root   . /usr/bin/tarlisted ./tarlisted
#:# 744 root root   . /usr/man/man1/tarlisted.1 ./tarlisted.1
#:# 700 too  too    . /tmp/path/to/nowhr /
#:# only bsd unix
#:# --- unski unski . /one\ symlink -> /where/ever/u/want/to/go
#:# all
#:# === wheel wheel . /one\ hrdlink => /bin/echo
#:# 666 root  root  . /dev/zero  // c 1 5
#:# 640 root  disk  . /dev/loop0 // b 7 0
#:# 600 root  root  . /dev/initctl // fifo
#:# tarlisted file end

# lines  "tarlisted file format"  and  "tarlisted file end"  are optional.
# try also with '-O bsd'...
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32
#include <sys/wait.h>

#include <pwd.h>
#include <grp.h>
#endif

#define null ((void*)0)
typedef enum { false = 0, true = 1 } bool; typedef char bool8;

#if 0
#define d1(x) do { errf x; } while (0)
#define d0(x) do {} while (0)
#else
#define d1(x) do {} while (0)
#define d0(x) do {} while (0)
#endif

#if (__GNUC__ >= 4)
#define GCCATTR_SENTINEL __attribute__ ((sentinel))
#else
#define GCCATTR_SENTINEL
#endif

#if (__GNUC__ >= 3)
#define GCCATTR_PRINTF(m, n) __attribute__ ((format (printf, m, n)))
#define GCCATTR_NORETURN __attribute__ ((noreturn))
#define GCCATTR_UNUSED   __attribute__ ((unused))
#define GCCATTR_CONST    __attribute__ ((const))

#else
#define GCCATTR_PRINTF(m, n)
#define GCCATTR_NORETURN
#define GCCATTR_UNUSED
#define GCCATTR_CONST

#endif

#define UU GCCATTR_UNUSED /* convenience macro */

#define int_sizeof (int)sizeof /* most cases safe */

struct {
    const char * progname;
    FILE * stream;
    off_t bytes_written;
    int prevchar;
    int lineno;
    int exitvalue;
    bool8 opt_dry_run;
    bool8 opt_verbose;
    char * opt_only;
    int opt_onlycount;
    char zerobuf[1024];
} G;

void init_G(const char * progname, FILE * stream)
{
    memset(&G, 0, sizeof G);
    G.progname = progname;
    G.stream = stream;
}

/* -- error printing helpers... -- */

static void verrf(const char * format, va_list ap)
{
    int err = errno;

    fprintf(stderr, "%s:  ", G.progname);
    vfprintf(stderr, format, ap);

    if (format[strlen(format) - 1] == ':')
	fprintf(stderr, " %s.\n", strerror(err));
    G.exitvalue = 1;
}

static void xerrf(const char * format, ...) GCCATTR_NORETURN
/*					*/  GCCATTR_PRINTF(1, 2);
static void xerrf(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    verrf(format, ap);
    va_end(ap);

    exit(1);
}

static void errf(const char * format, ...) GCCATTR_PRINTF(1, 2);
static void errf(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    verrf(format, ap);
    va_end(ap);
}


static void inputerror(const char * format, ...) GCCATTR_PRINTF(1, 2);
static void inputerror(const char * format, ...)
{
    va_list ap;
    errf("Error in input line %d: ", G.lineno);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
#if 00000000 /* XXX requires some modificatione elsewhere to make this work... */
    if (! G.opt_dry_run)
	exit(1);
#endif
}

/* -- -- */

#ifndef O_BINARY
#define O_BINARY 0
#endif

static int xopen(const char * path, int flags, mode_t mode)
{
    int fd = open(path, flags | O_BINARY, mode);
    if (fd < 0)
	xerrf("opening %s failed:", path);
    return fd;
}

static FILE * xfopen(const char * path, const char * mode)
{
    FILE * fh = fopen(path, mode);
    if (fh == null)
	xerrf("opening %s failed:", path);
    return fh;
}

static void xlseek(int fd, off_t offset, int whence)
{
    if (lseek(fd, offset, whence) < 0)
	xerrf("lseek failed:");
}

#ifndef WIN32
static void xpipe(int fds[2])
{
    if (pipe(fds) < 0)
	xerrf("pipe:");
}

static int xfork(void)
{
    int r;
    if ( (r = fork()) < 0)
	xerrf("fork:");
    return r;
}

static void movefd(int o, int n)
{
    if (o == n) return;
    dup2(o, n); close(o);
}

#endif

/* -- -- */

/*** input line "tokenizer" (space separated blocks, \ as escape char ) ***/

/* this newer returns value `0' */
int nextchar(void)
{
    while (1) {
	int c = getc(G.stream);

	if (c < 0) {
	    /* fake last line has newline if it didn't. */
	    if (G.prevchar != '\n') { G.prevchar = '\n'; return '\n'; }
	    return c;
	}
	G.prevchar = c;
	if (c == '\n')	return c;
	if (c == '\t')	return ' ';
	if (c < 32 || (c >= 127 && c < 160))
	    continue;
	return c;
    }
}

/* returns eof if no found (but nextchar fakes one for last line...) */
int scan_to_newline(int rv)
{
    int c;

    while ((c = nextchar()) > 0)
	if (c == '\n')
	    return rv;
    return c;
}

int skip_space(void)
{
    int c;

    while ( (c = nextchar()) == ' ' ) /* nextchar converts tab to space... */
	;
    if (c < 0)		return c;
    if (c == '\n')	return 0;
    if (c == '#')	return scan_to_newline(0);
    return c;
}

int linetok(char * toks[10], char buf[4096])
{
    int i = 0;
    int c;
    char * p = buf;
    char * a = buf;

    G.lineno++;
    if ( (c = skip_space()) <= 0 )
	return c;

    do {
	if (c == ' ') {
	    *p++ = '\0';
	    toks[i++] = a; a = p;
	    if ( (c = skip_space()) <= 0)
		return i;
	}
	else if (c == '#') {
	    *p++ = '\0';
	    toks[i++] = a;
	    return scan_to_newline(i);
	}
	else if (c == '\\') {
	    c = nextchar();
	    if (c == '\n') { G.lineno++; c = ' '; continue; }
	}
	else if (c == '\n') {
	    *p++ = '\0';
	    toks[i++] = a;
	    return i;
	}
	*p++ = c;
	c = nextchar();

    } while (c > 0 && i < 10 && p - buf < 4090);

    if (i >= 10)
	inputerror("Too many \"fields\" in this line.");

    if (p - buf >= 4090)
	inputerror("Input line too long.");

    if (c <= 0)
	inputerror("XXX.");

    exit(1);
}

#if 0
int toktest()
{
    int i, t, l;
    char buf[4096];
    char * toks[10];

    init_G("toktest", stdin);

    l = 0;
    while ((t = linetok(toks, buf)) >= 0) {
	printf("\nline: %d tok: %d\n", G.lineno, t);
	for (i = 0; i < t; i++) {
	    printf("  %d %s\n", i, toks[i]);
	}
    }
    return 0;
}
#endif


/* --- --- */

static void writezero1024(int fd)
{
    if (write(fd, G.zerobuf, 1024) != 1024)
	xerrf("write failed:");
    G.bytes_written += 1024;
}

/* static void writezero(int fd, int(*writefunc)(int, char *, int)) */
static void writezero(int fd, int byteboundary)
{
    unsigned int i, nw;

    i = G.bytes_written % byteboundary;
    if (i) {
	nw = byteboundary - i;

	while (nw) {
	    i = (nw > sizeof G.zerobuf? sizeof G.zerobuf: nw);
	    /* writefunc(fd, buf, i); */
	    if (write(fd, G.zerobuf, i) != (int)i )
		xerrf("write failed:");
	    nw -= i;
	    G.bytes_written += i;
	}
    }
}

static int writedata(int fd, char * buf, int len)
{
    len = write(fd, buf, len);
    if (len > 0)
	G.bytes_written += len;
    return len;
}

/* --- --- */


static char * needarg(char ** arg, const char * emsg)
{
    if (*arg == null)
	xerrf(emsg);
    return *arg;
}

static const char helptxt1[] =
    "  Files and directories to be archived are read from input file,\n"
    "  each line in one of the formats:\n\n"
    "  <perm> <uname> <gname> . <tarfilename> <sysfilename>\n"
    "  <perm> <uname> <gname> . <tarfilename> /\n"
    "   ---   <uname> <gname> . <tarfilename> -> <symlink>\n"
    "   ===   <uname> <gname> . <tarfilename> => <hardlink>\n"
    "  <perm> <uname> <gname> . <tarfilename> // c <devmajor> <devminor>\n"
    "  <perm> <uname> <gname> . <tarfilename> // b <devmajor> <devminor>\n"
    "  <perm> <uname> <gname> . <tarfilename> // fifo\n\n"
    "  Where:\n";
static const char helptxt2[] =
    "\tperm:         file permission in octal number format.\n"
    "\tuname:        name of the user this file is owned.\n"
    "\tgname:        name of the group this file belongs.\n"
    "\t'.':          future extension for time...\n"
    "\ttarfilename:  name of the file that is written to the tar archive.\n\n"
    "\tsysfilename:  file to be added from filesystem.\n"
    "\t/:            add directory to the tar archive.\n"
    "\t->:           add tarfilename as symbolic link to given name.\n"
    "\t=>:           add tarfilename as hard link to given name.\n";
static const char helptxt3[] =
    "\t// c ...:     add character device to the tar archive.\n"
    "\t// b ...:     add block device to the tar archive.\n"
    "\t// fifo:      add fifo (named pipe) file to the tar archive.\n"
    "\n"
    "  Backspace \"\\\" must be used when adding [back]spaces to filenames.\n"
    "  Pipeline output to gzip/xz (| gzip -c > outfile.tar.gz) for compression.\n"
    "\n"
    "  Output format is POSIX Unix Standard TAR (ustar, IEEE Std 1003.1-1988).\n"
    "\n";


static void usage(bool help)
{
    FILE * fh = help? stdout: stderr;

    if (help)
	fprintf(fh, "\nTarlisted version " VERSION "\n");

    fprintf(fh, "\n"
#ifndef WIN32
	    "Usage: %s [-nVzjJ] [-C dir] [-i infile] [(-o|-a) outfile] [| cmd [args]]\n\n"
#else
	    "Usage: %s [-nVh] [-C dir] [-i infile] (-o|-a) outfile\n\n"
#endif
	    "\t-n: just check tarlist contents, not doing anything else\n"
	    "\t-V: verbose output. -VV outputs to both stdout and stderr\n"
	    "\t-C: change to directory for input -- does not affect output\n"
	    , G.progname);
    fputs("\t-i: input file, instead of stdin\n"
#ifndef WIN32
	  "\t-o: output file (- = stdout); required if '|' not used\n"
	  "\t-a: append to output archive\n"
	  "\t-A: concatenate to output file\n"
	  "\t-z: compress archive with gzip (mutually exclusive with -j, -z and '|')\n"
	  "\t-j: compress archive with bzip2 (mutually exclusive with -z, -J and '|')\n"
	  "\t-J: compress archive with xz (mutually exclusive with -z, -j and '|')\n"
#else
	  "\t-o: output file (- = stdout)\n"
#endif
	  "\t-O: comma-separated list of items for 'only' lines...\n"
	  "\t-h: help\n"
	  "\n", fh);

    if (help) {
	fputs(helptxt1, fh);
	fputs(helptxt2, fh);
	fputs(helptxt3, fh);
	exit(0);
    }
    exit(1);
}


static void getugids(int * u, int * g)
{
#ifndef WIN32
    struct passwd * pw = getpwnam("nobody");
    struct group *  gr = getgrnam("nobody");

    if (pw == null) xerrf("Can not find user nobody\n");
    *u = pw->pw_uid;
    if (gr == null) gr = getgrnam("nogroup");
    if (gr == null) xerrf("Can not find group nobody\n");
    *g = gr->gr_gid;
#else
    *u = *g = 65534;
#endif
}


int cmpstrs(char ** xa, int xalen, const char * xs, int xslen)
{
    int i = 0;
    int l = strlen(xs) + 1;
    const char * p = xs + xslen;

    while (memcmp(*xa, xs, l) == 0) {
	i++;
	xs += l;
	xalen--;
	if (p - xs <= 0 || xalen <= 0)
	    return i;
	l = strlen(xs) + 1;
	xa++;
    }
    return i;
}

#define cmpstrsCS(xa, xalen, xs) cmpstrs(xa, xalen, xs, sizeof xs)

/* --- --- */

typedef enum { TFT_FILE = '0', TFT_LINK = '1', TFT_SYMLINK = '2',
	       TFT_CHARDEV = '3', TFT_BLOCKDEV = '4', TFT_DIR = '5',
	       TFT_FIFO = '6'
} TarFileType;

/*
	perm user group time pathname filename
	perm user group time pathname /
	perm user group time pathname -> symlink
	perm user group time pathname => hardlink
	perm user group time pathname // c maj min
	perm user group time pathname // b maj min
	perm user group time pathname // fifo
*/

struct fis {
    int    perm;
    char * uname;
    char * gname;
    char * tarfname;
    char * sysfname;
    int    devmajor;
    int    devminor;
    char   type;
};

int _getperm(char ** toks)
{
    unsigned int perm;
    if (sscanf(toks[0], "%o", &perm) != 1)
	inputerror("permissions parse error (not octal number).");
    if (perm >= 512)
	inputerror("permission value too big (or small).");
    return  (int)perm;
}

void _setmajmin(char ** toks, int i, int * majorp, int * minorp)
{
    if (i != 9)			inputerror("major/minor field count error.");
    if (! isdigit(toks[7][0]))	inputerror("major field not number.");
    if (! isdigit(toks[8][0]))	inputerror("minor field not number.");
    *majorp = atoi(toks[7]);
    *minorp = atoi(toks[8]);
}


int readfileinfo(char buf[4096], struct fis * fis)
{
    int skipping = 0;
    while (1) {
	char * toks[10];
	int i;

	memset(fis, 0, sizeof *fis);

	i = linetok(toks, buf);
#if 0
	{ int j; d1(("\nline: %d tok: %d\n", G.lineno, i));
	for (j = 0; j < i; j++) { d1(("  %d %s\n", j, toks[j])); }}
#endif
	if (i == 0)
	    continue;

	if (i < 0) /* usually EOF */
	    return 0;

	/* if (cmpstrs[toks[0], "tarlisted", "file", "version" */

	if (strcmp(toks[0], "tarlisted") == 0) {
	    if (i == 3 && cmpstrsCS(&toks[1], i-1, "file\0end") == 2)
		continue;
	    if (i == 4 && cmpstrsCS(&toks[1], i-1, "file\0format\0003") == 3)
		continue;
	    inputerror("`tarlisted' line error (unsupported version ?)"); }

	if (strcmp(toks[0], "all") == 0) {
	    skipping = 0;
	    continue;
	}
	/* but where is 'else only'? therefore non-documented */
	else if (strcmp(toks[0], "else") == 0) {
	    skipping = !skipping;
	    continue;
	}
	else if (strcmp(toks[0], "only") == 0) {
	    int j, k;
	    char * p = G.opt_only;
	    skipping = 1;
	    for (j = G.opt_onlycount; j > 0; j--) {
		for (k = 1; k < i; k++)
		    if (strcmp(p, toks[k]) == 0) {
			skipping = 0;
			j = 0;
			break;
		    }
		p = p + strlen(p) + 1;
	    }
	    continue;
	}
	if (skipping)
	    continue;

	if (i >= 6) {
	    fis->uname = toks[1];
	    fis->gname = toks[2];
	    /* time currently ignored. - will default to... */
	    fis->tarfname = toks[4];

	    if (i == 6) {
		fis->perm = _getperm(toks);
		fis->sysfname = toks[5];
		fis->type = (strcmp(fis->sysfname, "/") == 0) ?
		    TFT_DIR: TFT_FILE;
	    }
	    else if (strcmp(toks[5], "->") == 0) {
		if (strcmp(toks[0], "---") != 0)
		    inputerror("permissions not `---' for symlink.");
		fis->perm = 0777;
		fis->sysfname = toks[6];
		fis->type = TFT_SYMLINK;
	    }
	    else if (strcmp(toks[5], "=>") == 0) {
		if (strcmp(toks[0], "===") != 0)
		    inputerror("permissions not `===' for hard link.");
		fis->perm = 0777;
		fis->sysfname = toks[6];
		fis->type = TFT_LINK;
	    }
	    else if (strcmp(toks[5], "//") == 0) {
		fis->perm = _getperm(toks);

		if (strcmp(toks[6], "fifo") == 0) {
		    fis->type = TFT_FIFO;
		}
		else if (strcmp(toks[6], "c") == 0) {
		    _setmajmin(toks, i, &fis->devmajor, &fis->devminor);
		    fis->type = TFT_CHARDEV;
		}
		else if (strcmp(toks[6], "b") == 0) {
		    _setmajmin(toks, i, &fis->devmajor, &fis->devminor);
		    fis->type = TFT_BLOCKDEV;
		}
		else inputerror("strange content after //");
	    }
	    else inputerror("strange content, type information error");
	}
	else inputerror("strange content, information missing");

	return i;
    }
}

/* --- --- */

off_t readoctal(char * str)
{
    off_t val = 0;
    while (*str) {
	if (*str >= '0' && *str <= '7')
	    val = (val << 3) + (*str - '0');
	str++;
    }
    return val;
}

/* Unix Standard TAR format */

/* copied from librachive tar.5 documentation page */
struct header_posix_ustar {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

void ustar_add(char * name, int mode, int uid, int gid, off_t fsize,
	       time_t mtime, char type, char * linkname,
	       char * uname, char * gname, int devmajor, int devminor,
	       int input_fd, int output_fd)
{
    struct header_posix_ustar header;
    int preflen = 0;
    int l;

    d1(("ustar_add('%s', o%o, %d, %d, %llo, %lo, '%c', ...)\n",
	name, mode, uid, gid, fsize, mtime, type));

    memset(&header, 0, sizeof header);

    /* I'm being overly safe here... for now... */

    if (linkname && strlen(linkname) > 99)
	inputerror("link string %s too long.", linkname);

    l = strlen(name);
    if (l > 99) {
	char * p;
	for (p = (l > 154)? (name + 154): (name + l - 1); p > name; p--)
	    if (*p == '/')
		break;
	if (l - (p - name) > 99)
	    inputerror("File name %s too long.", name);
	/* else */
	preflen = (p - name); }

#ifndef WIN32
    if (sizeof fsize > 4 && fsize >= (1LL << 33)) /* 8 ** 11 */
	inputerror("File size %llu too large.", (unsigned long long)fsize);
#endif /* XXX can not check size if off_t is 32 bits wide */

    if (output_fd < 0)
	return;  /* dry-run mode; return after all checks done */

    /* the 2 strcpys below may write one extra '\0' but that doesn't matter */
    if (preflen) {
	memcpy(header.prefix, name, preflen);
	strcpy(header.name, name + preflen + 1); }
    else
	strcpy(header.name, name);

    snprintf(header.mode, 8, "%07o", mode);
    snprintf(header.uid, 8, "%07o", uid);
    snprintf(header.gid, 8, "%07o", gid);
#ifndef WIN32 /* FIXME, filloctal() or something */
    snprintf(header.size, 12, "%011llo", (unsigned long long)fsize);
    snprintf(header.mtime, 12, "%011lo", mtime);
#else
    sprintf(header.size, "%011lo", fsize);
    sprintf(header.mtime, "%011lo", mtime);
#endif
    memset(header.checksum, ' ', 8);
    header.typeflag[0] = type; /* XXX did we check already */
    if (linkname)
	strcpy(header.linkname, linkname); /* might write one extra '\0'... */
    strcpy(header.magic, "ustar");
    header.version[0] = header.version[1] = '0';
    strncpy(header.uname, uname, 31);
    strncpy(header.gname, gname, 31);
    if (devmajor >= 0)
	snprintf(header.devmajor, 8, "%07o", devmajor);
    if (devminor >= 0)
	snprintf(header.devminor, 8, "%07o", devminor);

    {   int sum; unsigned char * p = (unsigned char *)&header;
	for (sum = 0, l = 512; l; l--)
	    sum += *p++;
	snprintf(header.checksum, 7, "%06o", sum); }

    d0(("sizeof header = %d", sizeof header));
    if (writedata(output_fd, (char *)&header, 512) != 512)
	xerrf("write failed:");

    while (fsize > 0) {
	char buf[4096];

	l = (fsize > int_sizeof buf)? int_sizeof buf: fsize;
	if ((l = read(input_fd, buf, l)) > 0) {
	    if (writedata(output_fd, buf, l) != l)
		xerrf("write failed:");
	    else
		fsize -= l;
	}
	else
	    xerrf("read failed:");
    }
    writezero(output_fd, 512);
}


/* --- --- */

#ifndef WIN32
static int run_ppcmd(const char ** argv, int out_fd)
{
    int fds[2];

    xpipe(fds);

    if (xfork() == 0) {
	/* child */
	close(fds[1]);
	movefd(out_fd, 1);
	movefd(fds[0], 0);

	/* XXX well, does execve() possibly alter arg content? */
	execvp(argv[0], (char **)argv);
	xerrf("execvp:");
    }
    /* parent */

    return fds[1];
}

void dowait(void) GCCATTR_NORETURN;
void dowait(void)
{
    int status, pid = wait(&status);
    if (pid < 0)
	xerrf("wait:");
    if (WIFEXITED(status)) {
	status = WEXITSTATUS(status);
	if (status == 0)
	    exit(0);
	xerrf("Postprocess command exited with value %d\n", status);
    }
    if (WIFSIGNALED(status))
	xerrf("Postprocess command exited by signal %d\n", WTERMSIG(status));

    xerrf("Postprocess command exited by unknown staus code %d\n", status);
}


void sigchld_handler(int sig UU)
{
    dowait();
}

/* --- --- */


const char * gzipline[] =  { "gzip",  "-c", null };
const char * bzip2line[] = { "bzip2", "-c", null };
const char * xzline[] = { "xz", "-c", null };

void setppcmdp(const char *** ppcmdpp, const char ** p)
{
    if (*ppcmdpp)
	xerrf("Options -z, -j, -J and '|' are mutually exclusive\n");
    *ppcmdpp = p;
}

#endif /* not WIN32 */

void seektolast(int fd)
{
    while (1)
    {
	char buf[4096];
	int l = read(fd, buf, sizeof buf);
	int offset = 0;

	if (l < 512)
	    xerrf("tarfile ended prematurely\n");

	while (1) {
	    struct header_posix_ustar * tarhdr
		= (struct header_posix_ustar *)(buf + offset);
	    if (tarhdr->name[0] == 0) {
		xlseek(fd, offset - l, SEEK_CUR);
		G.bytes_written += offset;
		return;
	    }
	    if (strcmp(tarhdr->magic, "ustar") != 0
		|| tarhdr->version[0] != '0' || tarhdr->version[1] != '0')
		xerrf("One archive member not 'ustar' version '00' format\n");

	    offset += ((readoctal(tarhdr->size) + 511) & ~511) + 512;
#if 0
	    printf("%p %d %s %lld %s\n", tarhdr, offset,
		   tarhdr->size, readoctal(tarhdr->size), tarhdr->name);
#endif
	    if (offset >= l) {
		if (offset > l)
		    xlseek(fd, offset - l, SEEK_CUR);
		/* else offset == l */
		G.bytes_written += offset;
		break;
	    }
	}
    }
}



int main(int argc UU, char * argv[])
{
    char buf[4096];
    struct fis fis;
    int uid, gid;
    const char * ifname = null;
    const char * ofname = null;
    const char * godir = null;
    int out_fd, ifile_fd;
#ifndef WIN32
    const char ** ppcmdp = null;
#endif
    int n, append = 0;
    time_t starttime = time(null);
    time_t ttime;

    init_G(argv[0], stdin);
    argv++;

    if (argv[0] == 0)
	usage(false);

    getugids(&uid, &gid);

    while (argv[0] && argv[0][0] == '-') {
	int i;
	const char * arg = argv[0];

	argv++;
	for (i = 1; arg[i]; i++)
	    switch(arg[i]) {
	    case 'n': G.opt_dry_run = true; break;
	    case 'V': G.opt_verbose++; break;
	    case 'i': ifname = needarg(argv++, "No filename for  -i\n"); break;
	    case 'a': append = 1; /* fall through */
	    case 'A': if (append == 0) append = -1; /* fall through */
	    case 'o': ofname = needarg(argv++, "No filename for  -o\n"); break;
	    case 'C': godir = needarg(argv++, "No directory for -C\n"); break;
	    case 'O': G.opt_only = needarg(argv++, "No names -O\n"); break;
#ifndef WIN32
	    case 'z': setppcmdp(&ppcmdp, gzipline); break;
	    case 'j': setppcmdp(&ppcmdp, bzip2line); break;
	    case 'J': setppcmdp(&ppcmdp, xzline); break;
#endif
	    case 'h': usage(true); break;
	    default:
		errf("%c: unknown option\n", arg[i]);
		usage(false);
	    }}

#ifndef WIN32
    if (argv[0]) {
	if ( argv[0][0] != '|' || argv[0][1] != '\0' )
	    errf("%s: unknown argument\n", argv[0]);
	else
	    setppcmdp(&ppcmdp, (const char **)(argv + 1));
    } else if (ofname == null)
	xerrf("Option -o reguired if postprocessor command (with '|') not used\n");
#else
    if (ofname == null)
	xerrf("Option -o outfile missing\n");
#endif

#ifndef WIN32
    if (append > 0 && ppcmdp)
	xerrf("No compression/postprocessor commands with append mode\n");
#endif

    if (G.opt_only) {
	char * p = G.opt_only;
	while ((p = strchr(p, ',')) != null) {
	    *p++ = '\0';
	    if (*p == '\0')
		break;
	    G.opt_onlycount++;
	}
	G.opt_onlycount++;
    }

    if (G.opt_dry_run)
	out_fd = -1;
    else {
	if (ofname && (ofname[0] != '-' || ofname[1] != '\0')) {
	    if (!append)
		out_fd = xopen(ofname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	    else if (append < 0)
		out_fd = xopen(ofname, O_WRONLY|O_CREAT|O_APPEND, 0644);
	    else
		seektolast(out_fd = xopen(ofname, O_RDWR, 0644));
	}
	else {
	    if (G.opt_verbose > 1) G.opt_verbose = 1;
	    out_fd = 1;
	}
    }

#ifndef WIN32
    if (ppcmdp && ! G.opt_dry_run) {
	signal(SIGCHLD, sigchld_handler);
	out_fd = run_ppcmd(ppcmdp, out_fd);
    }
#endif

    if (godir)
	if (chdir(godir) < 0)
	    xerrf("Can not change to directory '%s':", godir);

    if (ifname)
	G.stream = xfopen(ifname, "r");

    ifile_fd = -1;
    while ((n = readfileinfo(buf, &fis)) > 0) {
	struct stat st;
	off_t fsize = 0;

	while (fis.tarfname[0] == '/')
	    fis.tarfname++;
	if (fis.tarfname[0] == '\0') {
	    inputerror("Tar file name missing.");
	    continue;
	}
	if (fis.type == TFT_FILE) {
	    if (stat(fis.sysfname, &st) < 0) {
		xerrf("Stating file '%s' (in line %d) failed:", fis.sysfname,n);
		/*continue; */
	    }
	    if (! S_ISREG(st.st_mode))
		inputerror("'%s' is not regular file.", fis.sysfname);

	    ifile_fd = xopen(fis.sysfname, O_RDONLY, 0);
	    fsize = st.st_size;
	    ttime = st.st_mtime;

	}
	else {
	    fsize = 0;
	    ttime = starttime /* 1234567890*/;
	}
	ustar_add(fis.tarfname, fis.perm, uid, gid, fsize, ttime,
		  fis.type, (fis.type == TFT_LINK || fis.type == TFT_SYMLINK)?
		  /*            */ fis.sysfname: 0,
		  fis.uname, fis.gname, fis.devmajor, fis.devminor,
		  ifile_fd, out_fd);

	if (G.opt_verbose) {
	    fprintf(stderr, "%s\n", fis.tarfname);
	    if (G.opt_verbose > 1)
		fprintf(stdout, "%s\n", fis.tarfname);
	}

	if (ifile_fd >= 0) {
	    close(ifile_fd);
	    ifile_fd = -1;
	}
    }
    if (out_fd >= 0) {
	writezero1024(out_fd);
	writezero(out_fd, 10240);
	close(out_fd);
    }
#ifndef WIN32
    if (ppcmdp) {
	signal(SIGCHLD, SIG_DFL);
	dowait();
    }
#endif

    return G.exitvalue;
}


/*
 * Local variables:
 * mode: c
 * c-file-style: "stroustrup"
 * tab-width: 8
 * compile-command: "sh tarlisted.c"
 * End:
 */
