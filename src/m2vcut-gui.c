#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -eu; case $1 in -o) trg=$2; shift 2
	;;	*) trg=`exec basename "$0" .c` ;; esac; rm -f "$trg"
 WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 eval `exec cat ../_build/config/mpeg2.conf` # XXX
 xo=`pkg-config --cflags --libs gtk+-2.0 | sed 's/-I/-isystem /g'`
 xo="$xo -lutil $mpeg2_both -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
 case $1 in '') set x -ggdb
 #case $1 in '') set x -O2
	shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$trg" "$0" $xo
 exit 0
 */
#endif
/*
 * $Id; m2vcut-gui.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 2008 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sun Dec 30 14:17:12 EET 2007 too
 * Last modified: Wed 01 Apr 2015 01:02:07 +0300 too
 */

// later (maybe?) test, undo, append-cut/merge to file (w/htonl()))
//       and esc-bs...

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _POSIX_SOURCE

// In unix-like system I've never seen execvp() fail with const argv
#define execvp(a,b) xexecvp(a,b)

#define index indx
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
/* #include <sys/stat.h> */
#include <fcntl.h>

#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <signal.h>
#include <sys/wait.h>

#include <sys/stat.h>
#include <sys/mman.h>

//#define __GTK_ITEM_FACTORY_H__ // XXX
#include <gtk/gtk.h>
//#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include "mpeg2.h"
#include "mpeg2convert.h"
#undef index

#undef execvp
int execvp(const char * file, const char * argv[]);

#define null ((void *)0)
typedef enum { false = 0, true = 1 } bool;
typedef char bool8;

typedef unsigned int u_int; // c99 blocks this definition in <sys/types.h>

#if 0
#define DEBUG 1
#define d1(x) do { printf("%d ", __LINE__); printf x; printf("\n"); } while (0)
//#define d1(x) do {} while (0)
#define d0(x) do {} while (0)
#define dx(x) do { x; } while (0)
#else
#define DEBUG 0
#define d1(x) do {} while (0)
#define d0(x) do {} while (0)
#define dx(x) do {} while (0)
#endif

#if 0
#define d2(x) do { printf("%d: ",__LINE__); printf x; printf("\n"); } while (0)
#else
#define d2(x) do {} while (0)
#endif

#if (__GNUC__ >= 4)
#define GCCATTR_SENTINEL __attribute ((sentinel))
#else
#define GCCATTR_SENTINEL
#endif

#if (__GNUC__ >= 3)
#define GCCATTR_PRINTF(m, n) __attribute__ ((format (printf, m, n)))
#define GCCATTR_NORETURN __attribute ((noreturn))
#define GCCATTR_CONST    __attribute ((const))

// XXX problems with these and c99
#define S2U(v, t, i, o) \
  __builtin_choose_expr (__builtin_types_compatible_p (typeof (v), t i), \
			 ((unsigned t o)(v)), (void)0)
#define U2S(v, t, i, o) \
  __builtin_choose_expr (__builtin_types_compatible_p \
			 (typeof (v), unsigned t i), /**/ ((t o)(v)), (void)0)
#else
#define GCCATTR_PRINTF(m, n)
#define GCCATTR_NORETURN
#define GCCATTR_CONST

#define S2U(v, t, i, o) ((unsigned t o)(v))
#define U2S(v, t, i, o) ((t o)(v))
#endif

#if 0 // c99 can use inline functions (instead)
static inline int u2i(unsigned int u) { return (int)u; }
#endif
static inline unsigned int i2u(int i) { return (unsigned int)i; }

/* can't do these as inlines (without template support) */
//#define MAX(a, b) ((a) > (b)? (a): (b)) // defined in gmacros.h
//#define MIN(a, b) ((a) > (b)? (b): (a)) // defined in gmacros.h

#if (__GNUC__ >= 4 && ! __clang__) // compiler warning avoidance nonsense
static inline void WUR(ssize_t x) { x = x; }
#else
#define WUR(x) x
#endif

/* write constant string */
# define WriteCS(f, s) WUR(write(f, s, sizeof s - 1))



//static inline char * str_unconst(const char * p) { return p; } // like GNU mc

/* --------------------- amiga list code --------------------- */

typedef struct Node
{
    struct Node * succ;
    struct Node * pred;
} Node;

typedef struct List
{
    Node * head;
    Node * tail;
    Node * tailpred;
} List;


static inline void initList(List * list)
{
    list->head = (Node *)&list->tail;
    list->tail = 0;
    list->tailpred = (Node *)&list->head;
}
static inline void addTail(List * list, Node * node)
{
    /* order is importart, for atomicity (one writer, many readers) */
    node->pred = list->tailpred;
    node->succ = (Node *)&list->tail;
    list->tailpred->succ = node;
    list->tailpred = node;
}

static inline Node * remHead(List * list)
{
    Node * node = list->head;

    if (node->succ == 0)
	return 0;

    list->head = node->succ;
    node->succ->pred = (Node *)&list->head;

    return node;
}

static inline void remNode(Node * node)
{
    Node * node2;

    node2 = node->succ;
    node = node->pred;
    node->succ = node2;
    node2->pred = node;
}

#if DEBUG && 1
static List * _dbg_list = null; static Node * _dbg_node = null;
static int _listd(List * list) {
    if (!_dbg_list) _dbg_list = list; return list - _dbg_list; }
static int _noded(Node * node) {
    if (!_dbg_node) _dbg_node = node; return (node - _dbg_node) / 4; }

static inline void xInitList(int line, List * list) {
    printf("%d initList: list %d\n", line, _listd(list)); initList(list);}
#define initList(list) xInitList(__LINE__, (list))

static inline void xAddTail(int line, List * list, Node * node) {
    printf("%d addTail: list %d, node %d\n", line, _listd(list), _noded(node));
    addTail(list, node); }
#define addTail(list, node) xAddTail(__LINE__, (list), (node))

static inline Node * xRemHead(int line, List * list) {
    Node * node = remHead(list);
    printf("%d remHead: list %d, node %d\n", line, _listd(list), _noded(node));
    return node; }
#define remHead(list) xRemHead(__LINE__, (list))

static inline void xRemNode(int line, Node * node) {
    printf("%d remNode: node %d\n", line, _noded(node)); remNode(node); }
#define remNode(node) xRemNode(__LINE__, (node))
#endif

/* ----------------- end of amiga list code ------------------ */

#define AUDIOVIEW_HEIGHT 31

typedef struct
{
    uint64_t offset;
    uint32_t iframeno;
} Index;


typedef struct _Frame
{
    Node listnode;
    int32_t frameno;
    uint8_t trn; // temporal reference number
//    uint8_t asr; // aspect ratio * 100
    uint8_t ftype; // frame type (I, P, B)
    uint8_t iadd; // add to index (to make gop)
    uint16_t pixwidth;
    uint16_t pixheight;
    uint16_t width;
    uint16_t height;
    uint8_t * rgb[3];
} Frame;


typedef struct
{
    int32_t frameno;
    int32_t index;
} Cutpoint;


struct /* (most generic) globals packaged ... */
{
    const char * prgname;
    int childpid;
    List free_frames;
    List mpeglib_frames;
    void * framemem;
    void * rgbmem;
    Frame ** framecache;
    int framecachesize;
    Frame * iframecache[6];
    Frame * frame;
    int currentframe;
    int firstframe;
    int lastframe;
    int maxwidth;
    int maxheight;
    const char * indexfile;
    Index * index;
    int lastindex;
    int lastdecodedindex;
    Frame * lastdecodediframe;
    int currentindex;
    int currentinner;
    const char * cutpointfile;
    Cutpoint cutpoint[1000];
    int cutpoints;
    int currentcutpoint;
    int redsum;
    int greenframes;
    //Undo undo[32];
} G;

#define G_iframecachesize (sizeof G.iframecache / sizeof G.iframecache[0])

struct
{
    mpeg2dec_t * decoder;
    const mpeg2_info_t * info;
    int reference_frames;
    uint8_t buffer[4096];
    const char * mpeg2filename;
    int fd;
} M;

/* Audio info */
struct {
    char * leveldata;
    int levelsize;
    uint8_t * rgb;
} A;


/* ---------------- */

/* XXX avoid warnings without 'void **' arg */
void zfree(void * memp) { void ** mempp = (void**)memp; free(*mempp); *mempp = null; }

gboolean sigchld_handler2(void * uu)
{
    (void)uu;
    wait(null);
    d1(("reaped!"));
    G.childpid = 0;
    return 0;
}

void sigchld_handler(int sig)
{
    (void)sig;
    g_idle_add(sigchld_handler2, null);
#if 0
    wait(null);
    WriteCS(2, "reaped!\n");
    G.childpid = 0;
#endif
}

static void sigact(int sig, void (*handler)(int))
{
  struct sigaction action;

  memset(&action, 0, sizeof action);
  action.sa_handler = handler;
  action.sa_flags = SA_RESTART|SA_NOCLDSTOP; /* NOCLDSTOP needed if ptraced */
  sigemptyset(&action.sa_mask);
  sigaction(sig, &action, NULL);
}

void run_command(const char * command, ...)
{
    d1(("run_command(%s, ...), childpid = %d", command, G.childpid));

    if (G.childpid > 0)
	return;

    // XXX some problems with reaper launching (gtk problem ?)
    sigact(SIGCHLD, sigchld_handler);

    switch( (G.childpid = fork()) )
    {
    case -1: return;
    case 0:
	/* child */
    {
	va_list ap;
	const char * pp[8];
	const char ** argv = pp;
	char * arg;
	*argv++ = command;
	va_start(ap, command);
	while ((arg = va_arg(ap, char *)) != null)
	    *argv++ = arg;
	*argv = null;

	//sigact(SIGCHLD, SIG_DFL);
	execvp(command, pp); // XXX const works at least on unix systems.
	exit(1);
    }
    }
    /* parent */
    return;
}

char * get_tmp_buffer(void) // ## increase mpeg input buffer for this...
{
    Frame * f;

    if (G.rgbmem == null) {
	/* just before program exit -- test outofmem */
	void * mem = malloc(2 * FILENAME_MAX); // XXX define...
	if (mem == null) {
	    WriteCS(2, "Out of Memory!");
	    exit(1);
	}
	return (char *)mem;
    }
    if (G.free_frames.head != (Node *)&G.free_frames.tail)
	f = (Frame *)G.free_frames.head;
    else {
	/* should never get here; G.free_frames */
	int ii = G.currentframe % G_iframecachesize;
	f = G.iframecache[ii];
    }
    f->frameno = -1;
    return (char *)f->rgb[0];
}

const char * m2vmp2cut_command(const char * plainname)
{
    char * p;
    char * tb = get_tmp_buffer();

    if ((p = getenv("M2VMP2CUT_CMD_PATH")) != null) {
	sprintf(tb, "%s/%s", p, plainname);
	return tb;
    }
    if ((p = strrchr(G.prgname, '/')) == null)
	return plainname;

    memcpy(tb, G.prgname, (p - G.prgname) + 1);
    strcpy(tb + (p - G.prgname) + 1, plainname);
    return tb;
}

void die(const char * fmt, ...) GCCATTR_PRINTF(1, 2) GCCATTR_NORETURN;
void die(const char * fmt, ...)
{
    va_list ap;
    int e = errno;
    char * p = (char *)M.buffer;
    int l = sizeof M.buffer;
    int i;

    // clear *some* memory buffers in case tight memory situation //
    zfree(&G.framemem);
    zfree(&G.rgbmem);
    zfree(&G.framecache);

    va_start(ap, fmt);
    vsnprintf(p, l, fmt, ap);
    va_end(ap);
    i = strlen(p); // ...
    p += i; l -= i;

    if (fmt[strlen(fmt) - 1] == ':')
	snprintf(p, l, " %s", strerror(e));

    WriteCS(2, "\nFatal error: ");
    WUR(write(2, M.buffer, i + strlen(p)));
    WriteCS(2, "\n\n");

    // kill(G.childpid);
    G.childpid = 0; // force!
    run_command(m2vmp2cut_command("wrapper.sh"), "die", M.buffer, null);
    exit(1);
}

static void init_G(const char * arg0)
{
    memset(&G, 0, sizeof G);
    G.prgname = arg0;
    initList(&G.free_frames);
    initList(&G.mpeglib_frames);
    G.lastdecodedindex = -1;
}

static void init_M(const char * filename)
{
    memset(&M, 0, sizeof M);

    M.fd = open(filename, O_RDONLY);
    if (M.fd < 0)
	die("Can not open '%s':", filename);

    M.mpeg2filename = filename;

    M.decoder = mpeg2_init();
    if (M.decoder == null)
	die("Could not allocate a decoder object");

    M.info = mpeg2_info(M.decoder);
}

static void * _alloc_avis_buffer(void)
{
    int siz = (G.maxwidth * AUDIOVIEW_HEIGHT * 3 + 3) & ~3;
    void * ptr = malloc( siz );
    if (ptr == null)
	die("Out of Memory (audio visualization buffer allocation)!");
    memset(ptr, 0, siz);
    return ptr;
}

static Frame * get_frame(void)
{
    Frame * frame = (Frame *)remHead(&G.free_frames);
    if (frame) return frame;
    die("no free frames");
}

static void set_seq(void)
{
    Frame * frame;
    d1(("sequence"));
    mpeg2_convert (M.decoder, mpeg2convert_rgb24, NULL);
    mpeg2_custom_fbuf (M.decoder, 1);

    frame = get_frame();
    //dx( frame->frameno = 111 );
    mpeg2_set_buf (M.decoder, frame->rgb, frame);
    addTail(&G.mpeglib_frames, &frame->listnode);

    frame = get_frame();
    //dx( frame->frameno = 222 );
    mpeg2_set_buf (M.decoder, frame->rgb, frame);
    addTail(&G.mpeglib_frames, &frame->listnode);
}


static void set_frameinfo(Frame * frame, const mpeg2_picture_t * picture)
{
    frame->trn = picture->temporal_reference;
    frame->width = M.info->sequence->picture_width;
    frame->height = M.info->sequence->picture_height;
    frame->ftype = picture->flags & PIC_MASK_CODING_TYPE;
#if 1
    frame->pixwidth = M.info->sequence->pixel_width;
    frame->pixheight = M.info->sequence->pixel_height;
#else
    uint32_t divd = M.info->sequence->pixel_height * frame->height;
    uint32_t asr32 = (frame->width * M.info->sequence->pixel_width * 100
		      + divd / 2) / divd;
    frame->asr = asr32 > 255? 0: asr32; // MIN(asr32, 255);
    d0(("asr::: %u ", asr32));
#endif

}

/* sample6 modified */

/* needs i-frame after seq header. scanner better make sure of it. */

static Frame * decode_iframe(off_t offset)
{
    mpeg2_state_t state;
    size_t size, prevsize;
    Frame * frame, *discframe;
    Node * node;

    if (lseek(M.fd, offset, SEEK_SET) < 0)
	die("lseek:");
    mpeg2_reset(M.decoder, 1);
    while ((node = remHead(&G.mpeglib_frames)) != null)
	addTail(&G.free_frames, node);

    /* M.reference_frames = -1; XXX may be needed in case of error... */
    size = prevsize = (size_t)-1;
    do {
	state = mpeg2_parse (M.decoder);
	switch (state)
	{
	case STATE_BUFFER:
	    size = read (M.fd, M.buffer, sizeof M.buffer);
	    if (size > 0)
		prevsize = size;
	    else if (prevsize > 0) {
		memcpy(M.buffer, "\000\000\001\267", 4);
		size = 4; prevsize = 0;
	    }
	    mpeg2_buffer (M.decoder, M.buffer, M.buffer + size);
	    break;
	case STATE_SEQUENCE:
	    set_seq(/*frameno*/);
	    break;

	case STATE_PICTURE:
	    d1(("picture %d %x", M.info->current_picture->temporal_reference,
		/*            */ M.info->current_picture->flags));

	    if ((M.info->current_picture->flags & PIC_MASK_CODING_TYPE)
		!= PIC_FLAG_CODING_TYPE_I)
		return null; /* need channel for error msgs */

	    /* mpeg2_skip(decoder, 0); XXX hmm */
	    frame = get_frame();
	    mpeg2_set_buf (M.decoder, frame->rgb, frame);
	    addTail(&G.mpeglib_frames, &frame->listnode);
	    break;
	case STATE_SLICE:
	    d1(("slice %d", M.info->current_picture->temporal_reference));

	    if (M.info->current_fbuf == null)
		return null; /* need channel for error msgs */

	    if ((M.info->current_picture->flags & PIC_MASK_CODING_TYPE)
		!= PIC_FLAG_CODING_TYPE_I)
		return null; /* need channel for error msgs */

	    if (M.info->discard_fbuf == null)
		return null; /* need channel for error msgs */

	    d1(("display_fbuf %p", (const void*)M.info->display_fbuf));
	    if (M.info->display_fbuf
		&& (M.info->display_fbuf->id != M.info->discard_fbuf->id))
		return null; /* need channel for error msgs */

	    M.reference_frames = 1;

	    frame = (Frame *)M.info->current_fbuf->id;
	    discframe = (Frame *)M.info->discard_fbuf->id;
	    remNode(&discframe->listnode);

	    set_frameinfo(frame, M.info->current_picture);
	    frame->iadd = 0;

	    d1(("discframe - frame %d", discframe - frame));
	    if (discframe == frame)
		/* show that image returned */ // XXX store by caller
		frame->listnode.succ = null;
	    else {
		M.reference_frames = 0;
		addTail(&G.free_frames, &discframe->listnode);
	    }
	    return frame; // XXX added 11.2.2008...
	    break;
	case STATE_END:
	case STATE_INVALID_END:
	    d1(("STATE_(INVALID?)END: %d", state));

	    die(("fixme: current/display_fbuf we may have..."));

	    while ((node = remHead(&G.mpeglib_frames)) != null)
		addTail(&G.free_frames, node);
	    /* free frames node count -- still needed ??? */
	default:
	    d1(("-- state %d", state));
	    break;
	}
    } while (size);

    return null;
}

// XXX is this called always right (frameno when closed/open gop ... !!! )
static void decode_frames(int frameno, int untilframe)
{
    mpeg2_state_t state;
    size_t size, prevsize;
    Frame * frame;
    int iadd = 0;
    int ptrn = -1;

    d1(("decode_frames(%d, %d)", frameno, untilframe));

    size = prevsize = (size_t)-1;
    do {
	state = mpeg2_parse (M.decoder);
	switch (state)
	{
	case STATE_BUFFER:
	    size = read (M.fd, M.buffer, sizeof M.buffer);
	    if (size > 0)
		prevsize = size;
	    else if (prevsize > 0) {
		memcpy(M.buffer, "\000\000\001\267", 4);
		size = 4; prevsize = 0;
	    }
	    mpeg2_buffer (M.decoder, M.buffer, M.buffer + size);
	    break;
	case STATE_SEQUENCE:
	    set_seq();
	    break;

	case STATE_PICTURE:
	    d1(("picture (current_picture info: tr: %d f: %x) rf %d",
		M.info->current_picture->temporal_reference,
		M.info->current_picture->flags, M.reference_frames));
	    if (M.reference_frames <= 0) {
		// XXX != PIC_FLAG_CODING_TYPE_I does not work ...
		// XXX so heuristics, that BB:s until next P...
		// well, P frame needs one and B frame 2 reference frames...
		// but we try to cope this by just dropping until I frame...
		if ((M.info->current_picture->flags & PIC_MASK_CODING_TYPE)
		    == PIC_FLAG_CODING_TYPE_B)
		    mpeg2_skip(M.decoder, 1);
		else {
		    M.reference_frames++;
		    mpeg2_skip(M.decoder, 0);
		}
	    }
	    frame = get_frame();
	    //dx( frame->frameno = 3 ); // XXX hmm, what was this
	    mpeg2_set_buf (M.decoder, frame->rgb, frame);
	    addTail(&G.mpeglib_frames, &frame->listnode);
	    break;
	case STATE_SLICE:
	    d1(("slice %d", M.info->current_picture->temporal_reference));

	    if (M.info->discard_fbuf == null)
		die("Frame decode error");

	    frame = (Frame *)M.info->discard_fbuf->id;
	    remNode(&frame->listnode);

	    if (M.info->display_fbuf && M.reference_frames > 0) {
		d1(("display fbuf (prev fno %d)",
		    ((Frame *)M.info->display_fbuf->id)->frameno));
		d1(("*Picture (display picture info: tr: %d f: %x)",
		    M.info->display_picture->temporal_reference,
		    M.info->display_picture->flags));

		if (M.info->discard_fbuf->id != M.info->display_fbuf->id)
		    die("yyy");
		frame->listnode.succ = null; //inform not listed

		set_frameinfo(frame, M.info->display_picture);
		frame->frameno = frameno;

		if (ptrn >= frame->trn) {
		    iadd = 1; ptrn++; }
		else
		    ptrn = frame->trn;

		frame->iadd = iadd;

		int bf = frame->frameno % G.framecachesize;
		if (G.framecache[bf]->listnode.succ == null)
		    addTail(&G.free_frames, &G.framecache[bf]->listnode);
		G.framecache[bf] = frame;

		if (++frameno == untilframe)
		    return;
	    }
	    else
		addTail(&G.free_frames, &frame->listnode);
	    break;
	case STATE_END:
	case STATE_INVALID_END:
	    d1(("STATE_(INVALID?)END: %d", state));

	    // XXX duplicate code. clean up later, maybe.
	    if (M.info->display_fbuf && M.reference_frames > 0) {
		d1(("display fbuf (prev fno %d)",
		    ((Frame *)M.info->display_fbuf->id)->frameno));
		d1(("*Picture (display picture info: tr: %d f: %x)",
		    M.info->display_picture->temporal_reference,
		    M.info->display_picture->flags));

		frame = (Frame *)M.info->display_fbuf->id;
		remNode(&frame->listnode);
		frame->listnode.succ = null; //inform not listed

		set_frameinfo(frame, M.info->display_picture);
		frame->frameno = frameno;

		if (ptrn >= frame->trn) {
		    iadd = 1; ptrn++; }
		else
		    ptrn = frame->trn;

		frame->iadd = iadd;

		int bf = frame->frameno % G.framecachesize;
		if (G.framecache[bf]->listnode.succ == null)
		    addTail(&G.free_frames, &G.framecache[bf]->listnode);
		G.framecache[bf] = frame;

		frameno++;
	    }
	    // XXX move frames to free list and continue loop...
	    while (frameno < untilframe) {
		int bf = frameno % G.framecachesize;
		G.framecache[bf]->frameno = frameno++;
		G.framecache[bf]->trn = G.framecache[bf]->iadd = 0;
		G.framecache[bf]->width = G.framecache[bf]->height = 1;
		G.framecache[bf]->pixwidth = G.framecache[bf]->pixheight = 1;
	    }
	    G.lastdecodedindex = -1;
	    Node * node; while ((node = remHead(&G.mpeglib_frames)) != null)
		addTail(&G.free_frames, node);
	    return;

	default:
	    d1(("-- state %d", state));
	    break;
	}
    } while (size);
}

/* ---------- */

FILE * open_indexfile(const char * filename)
{
    FILE * fh = fopen(filename, "r");
    char line[200];
    int gops = 0, frames;
    int width, height;

    G.indexfile = filename;

    if (fh == null)
	die("fopen(%s):", filename); /* fix later */

    if (fseek(fh, -200, SEEK_END) < 0)
	die("fseek(-200):"); /* fix later */

    while (fgets(line, sizeof line, fh) != null) {
	d0(("%s", line));
	if (sscanf(line, "end: size %*d frames %d gops %d  abr %*d w %d h %d",
		   &frames, &gops, &width, &height) == 3)
	    break;
    }
    if (gops == 0)
	die("No gops!!!");

    if (gops > 100 * 1000)
	die("currently 100 000 gop maximum (<%d)", gops);

    if (width > 800)   die("Currently max width 800 (<%d)", width);
    if (height > 600)  die("Currently max width 600 (<%d)", height);

    if (width < 720) width = 720; // XXX hack to make label fit...

    G.maxwidth  = width;
    G.maxheight = height;
    G.lastindex = gops - 1;
    G.lastframe = frames - 1;

    return fh;
}

void read_indexfile(FILE * fh)
{
    char line[200];
    u_int gops = G.lastindex + 1;
    Index * index;
    u_int i, gop, pframe = 0, maxfig = 0;

    index = calloc(gops + 1, sizeof (Index));
    if (index == null)
	die("Out of Memory (index file allocation)!");

    if (fseek(fh, 0, SEEK_SET) < 0)
	die("fseek(0):"); /* XXX fix later */

    i = 0;
    while (fgets(line, sizeof line, fh) != null) {
	uint32_t frame, tsr, fig;
	d0(("%s", line));
	if (sscanf(line, "%lu %u %u %u", &index[i].offset, &gop,
		   &frame, &tsr) == 4) {
	    if (gop != i)
		die("gop discontinuity in index file '%s' at line %d (gop %d)",
		    G.indexfile, i, gop);
	    fig = frame - pframe;
	    if (fig > maxfig) {
		enum { FIGMAX = 32 };
		if (fig >= FIGMAX)
		    die("Gop %d in index file '%s' has too many frames "
			"(%d > %d)", i, G.indexfile, fig, FIGMAX);
		maxfig = fig;
	    }
	    if (i == gops)
		die("More gops in index file '%s' than expected", G.indexfile);
	    index[i++].iframeno = frame + tsr;
	    pframe = frame;
	}
    }
    G.firstframe = index[0].iframeno;
    index[i].iframeno = G.lastframe + 1;
    G.index = index;
    //if (G.lastindex + 1 != i) die("xxx");
    if (i != gops)
	die("Not enough gops in index file '%s' (%d < %d)",
	    G.indexfile, i, gops);

    d1(("maxfig = %d", maxfig));
    G.framecachesize = maxfig * 2;

#if 0 && DEBUG
    for (int j = 0; j < i; j++)
    {
	printf("%llu %u\n",
	       index[j].offset, index[j].iframeno);
    }
#endif

}

/* -*- */

typedef struct
{
    unsigned int prevframe;
    int previndex;
    int prevmax;
    int test;
} CutO;


int cutpoint_frameindex(CutO * cuto, unsigned int frameno)
{
    if (frameno < cuto->prevframe)
	cuto->previndex = 0;
    cuto->prevframe = frameno;

    while (cuto->previndex <= G.lastindex) {
	if (G.index[cuto->previndex].iframeno > cuto->prevframe) {
	    cuto->previndex--;
	    return cuto->previndex;
	}
	else
	    cuto->previndex++;
    }
    return G.lastindex;
}

int check_cutpoint_range(CutO * cuto, int fval, int eval, int nextstate)
{
    if (eval < fval) {
	return 0;
    }
    if (fval < cuto->prevmax)
	G.cutpoints = 1;

    if (fval == eval)
	return 1;

    cuto->prevmax = eval;

    if (fval > G.lastframe)
	return 1;

    if (i2u(G.cutpoints) < sizeof G.cutpoint / sizeof G.cutpoint[0] - 1) {
	if (cuto->test) {
	    if (G.cutpoint[G.cutpoints++].frameno != fval) cuto->test = -1;
	    if (G.cutpoint[G.cutpoints++].frameno != eval) cuto->test = -1;
	}
	else {
	    G.cutpoint[G.cutpoints].frameno = fval;
	    G.cutpoint[G.cutpoints++].index = cutpoint_frameindex(cuto, fval);
	    d1(("%d,%d-", fval, cutpoint_frameindex(cuto, fval)));
	    if (eval > G.lastframe)
		return nextstate;
	    G.cutpoint[G.cutpoints].frameno = eval;
	    G.cutpoint[G.cutpoints++].index = cutpoint_frameindex(cuto, eval);
	    d1(("-%d,%d", eval, cutpoint_frameindex(cuto, eval)));
	}
    }
    return nextstate;
}

void parse_cutpoint_input(CutO * cuto, FILE * fh)
{
    int state = 0;
    int c;
    int fval = 0, eval = 0; // silence compiler

    while ((c = fgetc(fh)) >= 0)
    {
	switch (state) {
	case 0:
	    cuto->prevmax = -1;
	    G.cutpoints = 1;
	    state = 1;
	    /* fall through */
	case 1:
	    if (!isdigit(c))
		continue;
	    fval = c - '0';
	    state = 2;
	    continue;
	case 2:
	    if (isdigit(c)) {
		fval = fval * 10 + (c - '0');
		if (fval >= 1000 * 1000)
		    state = 0;
		continue;
	    }
	    if (c == '-')
		state = 3;
	    else
		state = 0;
	    continue;
	case 3:
	    if (!isdigit(c)) {
		state = 0;
		continue;
	    }
	    eval = c - '0';
	    state = 4;
	    continue;
	case 4:
	    if (isdigit(c)) {
		eval = eval * 10 + (c - '0');
		if (eval >= 1000 * 1000)
		    state = 0;
		continue;
	    }
	    if (c == ',')
		state = check_cutpoint_range(cuto, fval, eval, 1);
	    else if (c == '\n') {
		(void)check_cutpoint_range(cuto, fval, eval, 0);
		return;
	    }
	    else if (isspace(c))
		state = check_cutpoint_range(cuto, fval, eval, 5);
	    else
		state = 0;
	    continue;
	case 5:
	    if (c == '\n')
		return;
	    if (isspace(c))
		continue;
	    if (isdigit(c)) {
		fval = c - '0';
		state = 2;
	    }
	    else
		state = 0;
	}
    }
}

int load_cutpoints(const char * filename, int test)
{
    CutO cuto;

    memset(&cuto, 0, sizeof cuto);
    cuto.test = test;

    G.cutpoint[0].frameno = -1;
    G.cutpoint[0].index = -1;
    G.cutpoints = 1;

    if (strlen(filename) > FILENAME_MAX - 32)
	die("Cutpoint filename too long");

    FILE * fh = fopen(filename, "r");
    if (fh) {
	parse_cutpoint_input(&cuto, fh);
	fclose(fh);
    }
    G.cutpointfile = filename;
    if (! test) {
	G.cutpoint[G.cutpoints].frameno = G.lastframe + 1;
	G.cutpoint[G.cutpoints].index = G.lastindex + 1;
    }
    G.cutpoints++;
    G.redsum = -1;
    return cuto.test;
}

/* -*- */

void alloc_frames(void)
{
    unsigned int i;
    const unsigned int count = 3 + G_iframecachesize + G.framecachesize;

    int pixbytes = (G.maxheight * G.maxwidth * 3 + 3) & ~3;
    if (pixbytes < 2 * FILENAME_MAX) pixbytes = FILENAME_MAX; // ...
    Frame * frames = (Frame *)calloc(count, sizeof *frames);
    unsigned char * rgb = calloc(count, pixbytes);

    G.framecache = (Frame **)calloc(G.framecachesize, sizeof *G.framecache);

    if (frames == null || rgb == null || G.framecache == null) {
	zfree(&G.framecache); free(rgb); free(frames);
	die("Out of Memory (frame cache allocation)!");
    }
    G.framemem = frames;
    G.rgbmem = rgb;

    for (i = 0; i < count; i++) {
	frames[i].frameno = -1;
	frames[i].rgb[0] = rgb + i * pixbytes;
	frames[i].rgb[1] = frames[i].rgb[2] = 0;
    }
    for (i = 0; i < 3; i++)
	addTail(&G.free_frames, &frames[i].listnode);
    for (i = 3; i < 3 + G_iframecachesize; i++)
	G.iframecache[i - 3] = &frames[i];
    for (i = 3 + G_iframecachesize; i < count; i++)
	G.framecache[i - 3 - G_iframecachesize] = &frames[i];
}


/* -----------------------------------------------------------------------
   A set of GTK Widget wrappers that are suitable enough for this
   program. When this code is cut/pasted elsewhere the code is
   adjusted to fit that project.
   Note that not much has been done to make the wrappers have consistent
   arguments -- some functionality is hardcoded in one wrapper where other
   takes same thing as argument.
*/

GtkWidget * aWindow(const char * title,
		    gboolean (*delete_cb)(void * w, void * e, void * d),
		    gpointer deletedata,
		    int borderwidth, bool resizable, bool show,
		    GtkWidget * child)
{
    GtkWidget * window;

    window =  gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), title);

    g_signal_connect(G_OBJECT(window), "delete_event",
		     G_CALLBACK(delete_cb), deletedata);
#if 0
    g_signal_connect(G_OBJECT(window), "destroy",
		     G_CALLBACK(delete_cb), ...);
#endif
    gtk_container_set_border_width (GTK_CONTAINER (window), borderwidth);

    gtk_container_add(GTK_CONTAINER(window), child);
    gtk_window_set_resizable(GTK_WINDOW(window), resizable);
#if 0
    gtk_widget_set_size_request(window, 400, 300);
#endif
    if (show)
	gtk_widget_show_all(window);

  return window;
}

GtkBox * _aBox(bool hbox, gboolean homogeneous, gint spacing, va_list ap)
{
    GtkBox * box;
    GtkWidget * child;

    if (hbox)
	box = GTK_BOX(gtk_hbox_new(homogeneous, spacing));
    else
	box = GTK_BOX(gtk_vbox_new(homogeneous, spacing));

    while ((child = va_arg(ap, GtkWidget *)) != null) {
	gboolean expand = va_arg(ap, gboolean);
	gboolean fill = va_arg(ap, gboolean);
	guint padding = va_arg(ap, guint);
	d0(("aBox child %p", child));

	gtk_box_pack_start(box, child, expand, fill, padding);
    }
    return box;
}

GtkWidget * aVBox(gboolean homogeneous, gint spacing, ...) GCCATTR_SENTINEL;
GtkWidget * aVBox(gboolean homogeneous, gint spacing, ...)
{
    GtkBox * vbox;
    va_list ap;

    va_start(ap, spacing);
    vbox = _aBox(false, homogeneous, spacing, ap);
    va_end(ap);
    return GTK_WIDGET(vbox);
}

GtkWidget * aLabel(const char * label)
{
    return gtk_label_new(label);
}


GtkWidget * aDrawingArea(int width, int height)
{
  GtkWidget * drawingarea = gtk_drawing_area_new();

  gtk_widget_set_size_request(drawingarea, width, height);

  return drawingarea;
}

/* ----------- */

struct {
    GtkWidget * w;
    GtkWidget * da;
    GtkWidget * l;
    GtkWidget * i;
    PangoFontDescription * fd;
    PangoLayout * layout;
    PangoRenderer * renderer;
    GdkGC * gct;
    GdkGC * gcs;
    GdkGC * gcz[2];
    GdkPixmap * pm;
    char labeltext[144];
    bool drawing;
} W;

/* ----------- */

static inline void _put_apixel(unsigned int x, unsigned int y,
			       uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= (unsigned int)G.maxwidth || y >= AUDIOVIEW_HEIGHT)
	return;
    unsigned int pos = (y * G.maxwidth + x) * 3;
    A.rgb[ pos + 0 ] = r;
    A.rgb[ pos + 1 ] = g;
    A.rgb[ pos + 2 ] = b;
}

#if AUDIOVIEW_HEIGHT != 31
#error AUDIOVIEW_HEIGHT != 31
#endif
void update_alevels(void)
{
    int pos = G.currentframe * 4;
    int middle = G.maxwidth / 2;
    int midp8 = G.maxwidth / 8;
    int i;

    if (A.leveldata == null)
	return;

    for (i =- midp8; i < midp8; i++) {
	int j;
	uint8_t c;
	j = pos + i;
	if (j < 0 || j >= A.levelsize)
	    c = 0x12;
	else
	    c = A.leveldata[j];

	uint8_t y1 = 30 - (c & 15) * 2;
	uint8_t y2 = 30 - (c >> 4) * 2;
	int x0 = middle + i * 4;
	int x1 = x0 + 1;
	int x2 = x0 + 2;
	int x3 = x0 + 3;
#if 0
	if (abs (i) < 16)
	    printf("%3d %02x %3d %3d\n", i, c, y1, y2);
#endif
	int rc, gc;
	if ((unsigned int)i < 4)
	{
	    if (G.currentcutpoint & 1) {
		rc = 0; gc = 112;
	    }
	    else {
		rc = 112; gc = 0;
	    }
	}
	else
	    rc = gc = 0;

	for (int h = 0; h < 30; h++) {
	    _put_apixel (x0, h, rc, gc, h > y1? 255:0);
	    _put_apixel (x1, h, rc, gc, h > y1? 255:0);
	    _put_apixel (x2, h, rc, gc, h > y2? 255:0);
	    _put_apixel (x3, h, rc, gc, h > y2? 255:0);
	}
	_put_apixel (x0, 30, 0, 128, 192);
	_put_apixel (x1, 30, 0, 128, 192);
	_put_apixel (x2, 30, 0, 128, 192);
	_put_apixel (x3, 30, 0, 128, 192);

    }
    for (i = 0; i < 31; i += 3)
	_put_apixel(middle, i, 160,160,255);

    _put_apixel(middle + 15, 0, 160,160,255);
    //_put_apixel(middle + 15, 3, 160,160,255);
    //_put_apixel(middle + 15, 27, 160,160,255);
    _put_apixel(middle + 15, 30, 160,160,255);

    for (i = 16; i < middle; i += 16) {
	_put_apixel(middle - i, 15, 160,160,255);
	_put_apixel(middle + i, 15, 160,160,255);
    }
}

void draw_triangle(void)
{
    static int oldp = 10;
    int newp = G.currentframe * G.maxwidth / G.lastframe;
    int lp, rp;

    d1(("newp %d %d %d %d\n", newp, G.currentframe, G.lastframe, G.maxwidth));

    lp = oldp - 6;
    rp = oldp + 6;
    if (lp < 0) lp = 0;
    if (rp >= G.maxwidth) rp = G.maxwidth;

    /* draw background over old triangle */
    gdk_draw_drawable(W.pm, W.gct, W.pm, lp, 11, lp, 0, rp - lp, 11);

    int f = (G.currentframe != G.cutpoint[G.currentcutpoint].frameno);
    lp = newp - 4 - f;
    rp = newp + 4 + f;
    if (lp < 0) lp = 0;
    if (rp >= G.maxwidth) rp = G.maxwidth - 1;
    GdkPoint points[3] = { { lp, 0 }, { newp, f?10:8}, { rp, 0 } };

    gdk_draw_polygon(W.pm, W.gcs, f, points, 3);
    oldp = newp;
    gtk_widget_queue_draw(W.i);
}

/* itoa(), except backwards (args and output), for unsigned and only base 10 */
static inline int _utosb(char buf[12], uint32_t i)
{
    char * out = buf;

    do { *(out++) = "0123456789"[ i % 10 ]; i /= 10; } while ( i );
    *out = '\0';

    /*puts(buf); */
    return out - buf;
}

/* uint-to-str, max len chars, left-padded */
static inline void utosl(char * p, uint32_t i, int len)
{
    char buf[12];
    char * q;
    int l = _utosb(buf, i);
    if (l > len) l = len;
    len -= l;
    q = buf + l - 1;
    while (len--) *p++ = ' ';
    while (l--)   *p++ = *q--;
}

/* uint-to-str, max len chars, left-padded with zeroes */
static inline void utosl0(char * p, uint32_t i, int len)
{
    char buf[12];
    char * q;
    int l = _utosb(buf, i);
    if (l > len) l = len;
    len -= l;
    q = buf + l - 1;
    while (len--) *p++ = '0';
    while (l--)   *p++ = *q--;
}


/* uint-to-str, max len chars, right-padded */
static inline void utosr(char * p, uint32_t i, int len)
{
    char buf[12];
    char * q;
    int l = _utosb(buf, i);
    if (l > len) l = len;
    len -= l;
    q = buf + l - 1;
    while (l--)   *p++ = *q--;
    while (len--) *p++ = ' ';
}

void label_frametime(char * p, int frameno)
{
    int h = frameno / (60 * 60 * 25); if (h >= 1000) h = 999;
    int m = (frameno / (60 * 25)) % 60;
    int s = (frameno / 25) % 60;
    //int f = frameno % 25;
    int ms = frameno % 25 * 4;

    // XXX add 3rd hour digit.
#if 0
    sprintf(buf, "%03d:%02d:%02d.%02d", h, m, s, f);
    if (h < 100) buf[0] = ' ';
    return buf;
#endif
    utosl0(p + 0, h, 2);
    utosl0(p + 3, m, 2);
    utosl0(p + 6, s, 2);
    utosl0(p + 9, ms, 2);

    utosl(p + 12, frameno, 6);
}

#if 0
//char * time_code(char buf[16], Frame * frame) /* 000:00:00.00 */
char * time_code(char buf[16], int fn) /* 000:00:00.00 */
{
//    int fn = frame->frameno;

    int h = fn / (60 * 60 * 25); if (h >= 1000) h = 999;
    int m = (fn / (60 * 25)) % 60;
    int s = (fn / 25) % 60;
    int f = fn % 25;

    sprintf(buf, "%03d:%02d:%02d.%02d", h, m, s, f);
    if (h < 100) buf[0] = ' ';
    return buf;
}
#endif

void label_frametype(char * p, uint8_t ftype)
{
    static const char * s = "_IPBD X";
    if (ftype > 4) { ftype = 5; }
    *p = s[ftype];
}

void update_label(void)
{
    static int prev_cutpoint = -1;

    int frameno = G.frame->frameno;
    if (frameno >= 1000 * 1000) frameno = 999999;

    if (G.currentframe < G.cutpoint[G.currentcutpoint].frameno) {
	int fno;
	do {
	    G.currentcutpoint--;
	    fno = G.cutpoint[G.currentcutpoint].frameno;
	    if (!(G.currentcutpoint & 1))
		G.redsum -= G.cutpoint[G.currentcutpoint+1].frameno - fno;
	} while (G.currentframe < fno);
    }
    else
	while (G.currentframe >= G.cutpoint[G.currentcutpoint + 1].frameno) {
	    G.currentcutpoint++;
	    if (G.currentcutpoint & 1)
		G.redsum += G.cutpoint[G.currentcutpoint].frameno
		    - G.cutpoint[G.currentcutpoint-1].frameno;
	}

    int frameno2;
    if (G.currentcutpoint & 1)
	frameno2 = frameno - G.redsum;
    else
	frameno2 = G.cutpoint[G.currentcutpoint].frameno - G.redsum;

    d1(("currentcutpoint %d (all %d)", G.currentcutpoint, G.cutpoints));

    label_frametime(W.labeltext, G.currentframe);
    utosl(W.labeltext + 42, G.currentindex + G.frame->iadd, 5);
    utosl(W.labeltext + 48, G.frame->trn, 2);
    label_frametype(W.labeltext + 51, G.frame->ftype);
    if (G.currentcutpoint != prev_cutpoint) {
	W.labeltext[113] = (G.currentcutpoint & 1)? '+': '-';
	utosl(W.labeltext + 115, G.currentcutpoint, 3);
	prev_cutpoint = G.currentcutpoint;
	utosl(W.labeltext + 120, G.cutpoints - 2, 3);
    }
    label_frametime(W.labeltext + 71, frameno2);
    label_frametime(W.labeltext + 92, G.greenframes);

    // FIXME: some info does not need to be updated always, like greenframes)

    int wxp = G.frame->pixwidth * G.frame->width;
    int hxp = G.frame->pixheight * G.frame->height;

    float asr = (double)wxp /(double)hxp;

    int width, height;

    if (G.frame->pixwidth > G.frame->pixheight) {
	width = wxp / G.frame->pixheight;
	height = G.frame->height;
    }
    else {
	width = G.frame->width;
	height = hxp / G.frame->pixwidth;
    }

    utosl(W.labeltext + 55, G.frame->width, 4);
    utosr(W.labeltext + 60, G.frame->height, 4);

    utosl(W.labeltext + 65, G.frame->pixwidth, 2);
    utosr(W.labeltext + 68, G.frame->pixheight, 2);

    utosl(W.labeltext + 126, width, 4);
    utosr(W.labeltext + 131, height, 4);

    // XXX limit up to [9]9.99
    sprintf(W.labeltext + 136, "%.2f", asr);

#if 0
    sprintf(buf, "%s %6d %5d  (%2d, %c) | %c%3d | %s %6d ",
	    time_code(tcb1, frameno), frameno, G.currentindex + G.frame->iadd,
	    G.frame->trn, frame_type(G.frame->ftype),

	    time_code(tcb2, frameno2), frameno2);
//#else
    strcpy(buf,
	   "00:00:00.00 123456 / 00:00:00.00 123456 | 12345 00 I |  720x576  16/15\n" // 71
	   "00:00:00.00 123456 / 00:00:00.00 123456 | + 001  006 |  768x576  1.33");
    d1(("strlen %d\n", strlen(buf)));
#endif
    gtk_label_set_text(GTK_LABEL(W.l), W.labeltext);
}

void update_image(void)
{
    int i;
#if DEBUG
    for (i = 0; i < G.cutpoints; i++)
	d1(("cutpoint %d: %d %d\n", i,
	    G.cutpoint[i].frameno, G.cutpoint[i].index));
#endif
    gdk_draw_rectangle(W.pm, W.gcz[0], true, 0, 0,
		       G.cutpoint[1].frameno*G.maxwidth/G.lastframe + 1, 22);

    G.greenframes = 0;
    int x0 = G.cutpoint[1].frameno == 0? -1: 0;
    for (i = 1; i < G.cutpoints - 1; i++) {
	int fb = G.cutpoint[i].frameno, fe = G.cutpoint[i + 1].frameno;
	int x = fb * G.maxwidth / G.lastframe;
	int w = fe * G.maxwidth / G.lastframe - x + 1;
	if (i & 1) G.greenframes += fe - fb;
	if (x == x0) x++;
	else x0 = x;
	gdk_draw_rectangle(W.pm, W.gcz[i & 1], true, x, 0, w, 22);
    }
    draw_triangle();
}

void update_window(void)
{
    update_label();
    update_alevels();
    W.drawing = true;
    gtk_widget_queue_draw(W.da);
}

// cut change to include endpoint was done by localized hack inside
// cut_here() & merge_here() 2015-02-16...
void cut_here(void)
{
    if (G.cutpoints == sizeof G.cutpoint / sizeof G.cutpoint[0])
	return;

    int cutframe = G.currentframe + (G.currentcutpoint & 1);

    if (cutframe >= G.lastframe)
	return;

#if 1
    if (G.currentcutpoint
	&& G.currentindex - G.cutpoint[G.currentcutpoint].index < 4)
	return;

    if (G.currentcutpoint != G.cutpoints - 2
	&& G.cutpoint[G.currentcutpoint + 1].index - G.currentindex < 4)
	return;
#else
    if (G.cutpoint[G.currentcutpoint].frameno == cutframe) return;
#endif
    int i = G.currentcutpoint + 1;

    if (i & 1) {
	G.redsum += cutframe - G.cutpoint[G.currentcutpoint].frameno;
	G.currentcutpoint = i; // moved 2015-02-16 / see similar below
    }
    for (int j = G.cutpoints; j > i; j--)
	G.cutpoint[j] = G.cutpoint[j - 1];
    G.cutpoint[i].frameno = cutframe;
    G.cutpoint[i].index = G.currentindex;
    G.cutpoints++;
    update_image();
    update_window();
}

void merge_here(void)
{
    for (int i = 0; i < G.cutpoints; i++)
    {
	int cutframe = G.currentframe;
	if (G.cutpoint[i].frameno != cutframe &&
	    G.cutpoint[i].frameno != ++cutframe) {
	    //if (G.cutpoint[i].frameno > G.currentframe)
	    //	break;
	    continue;
	}
	if (i & 1)
	    G.redsum -= cutframe - G.cutpoint[i - 1].frameno;
	G.cutpoints--;
	for (; i < G.cutpoints; i++)
	    G.cutpoint[i] = G.cutpoint[i + 1];
	if (cutframe == G.currentframe) // added 2015-02-16 / see similar above
	    G.currentcutpoint--;
	update_image();
	update_window();
	break;
    }
}

void need_iframe(int index, bool force)
{
    off_t offset = G.index[index].offset;
    int frameno = G.index[index].iframeno;

    d1(("succ: %p ldi %d", (void*)G.frame->listnode.succ, G.lastdecodedindex));

    if (G.lastdecodediframe && G.lastdecodedindex == index) {
	G.frame = G.lastdecodediframe;
	return;
    }

    int ii = index % G_iframecachesize;
    if (!force || G.lastdecodedindex == index) {
	d1(("in iframecache: %d, frameno %d == %d ?",
	    ii, G.iframecache[ii]->frameno, frameno));
	if (G.iframecache[ii]->frameno == frameno) {
	    G.frame = G.iframecache[ii];
	    return;
	}

	int fi = frameno % G.framecachesize;

	d1(("in framecache: %d, frameno %d == %d ?",
	    fi, G.framecache[fi]->frameno, frameno));
	if (G.framecache[fi]->frameno == frameno) {
	    G.frame = G.framecache[fi];
	    return;
	}
    }
    if (G.lastdecodediframe) {
	if (G.lastdecodediframe->listnode.succ != null) {
	    remNode(&G.lastdecodediframe->listnode);
	    G.lastdecodediframe->listnode.succ = null;
	}
	int iii = G.lastdecodedindex % G_iframecachesize;
	addTail(&G.free_frames, &G.iframecache[iii]->listnode);
	G.iframecache[iii] = G.lastdecodediframe;
    }

    G.frame = decode_iframe(offset);
    G.frame->frameno = frameno;

    if (G.frame->listnode.succ == null) {
	addTail(&G.free_frames, &G.iframecache[ii]->listnode);
	G.iframecache[ii] = G.frame;
	G.lastdecodediframe = null;
    }
    else
	G.lastdecodediframe = G.frame;

    G.lastdecodedindex = index;
}

void need_frame(void)
{
    int fi = G.currentframe % G.framecachesize;

    d1(("need_frame: %d %d", G.framecache[fi]->frameno, G.currentframe));
    if (G.framecache[fi]->frameno != G.currentframe) {
	if (G.currentindex != G.lastdecodedindex)
	    need_iframe(G.currentindex, true);

	int iframeno = G.index[G.currentindex].iframeno;

	d1((".. %d %d", G.currentindex, iframeno));

	int ii = G.currentindex % G_iframecachesize;
	int f1 = iframeno % G.framecachesize;

	if (G.iframecache[ii]->frameno == iframeno) {
	    G.frame = G.iframecache[ii];
	    G.iframecache[ii] = G.framecache[f1];
	    G.framecache[f1] = G.frame;
	}
	// XXX a bit xxx, decode_frames may invalidate lastdecodedindex..
	G.lastdecodedindex = G.currentindex + 1;
	G.lastdecodediframe = null;
	decode_frames(iframeno, G.index[G.currentindex + 1].iframeno);
    }
    G.frame = G.framecache[fi];

    d1(("frame mem index %d. currentframe %d",
	G.frame - (Frame *)G.framemem, G.currentframe));

    update_window();
}

void next_frame(void)
{
    if (G.currentframe == G.lastframe)
	return;
    G.currentframe++;

    int n = G.currentinner + 1;
    int l = G.index[G.currentindex + 1].iframeno
	- G.index[G.currentindex].iframeno;
    if (n >= l) {
	G.currentindex++;
	G.currentinner = n - l; // should be zero always !!
    }
    else
	G.currentinner = n;

    need_frame();
}

void previous_frame(void)
{
    if (G.currentframe == G.firstframe)
	return;
    G.currentframe--;

    if (G.currentinner == 0) {
	G.currentinner = G.index[G.currentindex].iframeno
	    - G.index[G.currentindex - 1].iframeno - 1;
	G.currentindex--;
    }
    else
	G.currentinner--;

    need_frame();
}

void show_iframe(int i)
{
    G.currentindex = i;
    G.currentinner = 0;
    G.currentframe = G.index[i].iframeno;
    need_iframe(i, false);
    update_window();
}

void next_gop(int n)
{
    if (G.currentindex == G.lastindex) return;
    n += G.currentindex;
    if (n > G.lastindex) n = G.lastindex;
    show_iframe(n);
}

void previous_gop(int n)
{
    if (G.currentindex == 0) return;
    if (G.currentinner > 0) n--;
    n = G.currentindex - n;
    if (n < 0) n = 0;
    show_iframe(n);
}

void goto_percent(int pc)
{
    int n = G.lastindex * pc / 100;
    show_iframe(n);
}

void goto_cutpoint(int cutpoint)
{
    int i = cutpoint;
    int frameno = G.cutpoint[i].frameno;
    int index = G.cutpoint[i].index;

    G.currentframe = frameno;
    G.currentindex = index;
    G.currentinner = frameno - G.index[index].iframeno;
    //G.currentcutpoint = i; // set by update_label()

    if (G.currentinner == 0) {
	need_iframe(G.currentindex, false);
	update_window();
    }
    else
	need_frame();
}

void next_cutpoint(void)
{
    d1(("next_cutpoint: G.currentcutpoint %d, G.cutpoints %d",
	G.currentcutpoint, G.cutpoints));

    if (G.currentcutpoint == G.cutpoints - 2)
	return;
    goto_cutpoint(G.currentcutpoint + 1);
}

void previous_cutpoint(void)
{
    int i = (G.cutpoint[G.currentcutpoint].frameno == G.currentframe)
	? G.currentcutpoint - 1: G.currentcutpoint;

    if (i == 0) return;

    goto_cutpoint(i);
}

static void test_cutpoint0(char framearg1[20], char framearg2[20])
{
    int ccp = G.currentcutpoint;

    framearg1[0] = framearg2[0] = '\0';

    int fn0 = G.cutpoint[ccp].frameno;

    if (ccp & 1) { // "hack" when currentframe on green, but show other too
	if ( G.currentframe == fn0) {
	    ccp--; // on first green at cutpoint
	    fn0 = G.cutpoint[ccp].frameno;
	}
	else if (G.currentframe + 1 == G.cutpoint[ccp + 1].frameno) {
	    ccp++; // on last green before cutpoint
	    fn0 = G.cutpoint[ccp].frameno;
	}
    }
    int fn1 = G.cutpoint[ccp + 1].frameno;

    d0(("%d %d %d %d %d\n", fn0, fn1, ccp, G.currentframe, G.cutpoints));

    if ((ccp & 1) == 0) { // on red area (or ccp-- or ccp++ ...)
	if (ccp > 0)
	    sprintf(framearg1, "%d", fn0);
	if (ccp < G.cutpoints - 2)
	    sprintf(framearg2, "%d", fn1);
	return;
    }
    // on green area
    if (ccp >= G.cutpoints - 2) fn1--; // used to compare == no diff. should be
    int dist1 = G.currentframe - fn0;
    int dist2 = fn1 - G.currentframe;

    if (dist1 < dist2)
	sprintf(framearg2, "%d", fn0);
    else
	sprintf(framearg1, "%d", fn1);
}

static void test_cutpoint(void)
{
    char framearg1[20], framearg2[20];
    if (G.cutpoints == 2) return;
    test_cutpoint0(framearg1, framearg2);
    run_command(m2vmp2cut_command("wrapper.sh"), "m2vcut_test",
		framearg1, framearg2, G.indexfile, M.mpeg2filename, null);
}


void save_cutpointfile(void)
{
    if (G.cutpoints == 2)
	return;

    int cutpoints = G.cutpoints;

    if (load_cutpoints(G.cutpointfile, 1) > 0 && G.cutpoints == cutpoints)
	return;

    time_t time_ = time(null);
    struct tm * tmp = gmtime(&time_);
    static char wn[] = "SunMonTueWedThuFriSat";

    char * filename = get_tmp_buffer();
    char * filename2 = filename + FILENAME_MAX;

    sprintf(filename, "%s.new", G.cutpointfile);
    FILE * fh = fopen(filename, "w");
    if (fh == null)
	die("Opening %s for writing failed:", filename);

    if (tmp->tm_wday > 6) tmp->tm_wday = 6; // XXX too paranoid ?
    fprintf(fh, "%d-%02d-%02d (%.3s) %02d:%02d:%02dZ  ",
	    tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday,
	    wn + tmp->tm_wday * 3, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

    int i;
    //G.cutpoint[cutpoints - 1].frameno--;
    for (i = 1; i < cutpoints - 3; i+= 2)
    {
	fprintf(fh,"%d-%d,",
		G.cutpoint[i].frameno, G.cutpoint[i+1].frameno);
    }
    fprintf(fh, "%d-%d\n", G.cutpoint[i].frameno, G.cutpoint[i+1].frameno);

    fclose(fh);

    /* rotate */
    strcpy(filename2, filename);
    int l = strlen(G.cutpointfile);

    strcpy(filename  + l, ".3");
    strcpy(filename2 + l, ".2");
    rename(filename2, filename);
    strcpy(filename  + l, ".1");
    rename(filename, filename2);
    filename2[l] = '\0';
    rename(filename2, filename);
    strcpy(filename  + l, ".new");
    rename(filename, filename2);

    //G.cutpoint[G.cutpoints - 1].frameno++;
}

static inline void show_help(void)
{
    run_command(m2vmp2cut_command("wrapper.sh"), "m2vcut_help", null);
}

static void show_info(void)
{
    char allframes[20], greenframes[20];

    // XXX quick hack, tune label_frametime() interface
    memcpy(allframes, "00:00:00.00\0     0\0\0", 20);
    memcpy(greenframes, allframes, 20);
    label_frametime(allframes, G.lastframe);
    label_frametime(greenframes, G.greenframes);

    char * afp = allframes + 12; while (isspace(*afp)) afp++;
    char * gfp = greenframes + 12; while (isspace(*gfp)) gfp++;

    run_command(m2vmp2cut_command("wrapper.sh"), "m2vcut_info",
		M.mpeg2filename, afp, gfp, allframes, greenframes, null);
}

#if 0
static void show_agraph(void)
{
    char cfs[12], lfs[12];
    sprintf(cfs, "%d", G.currentframe);
    sprintf(lfs, "%d", G.lastframe);
    run_command(m2vmp2cut_command("wrapper.sh"), "m2vcut_agraph",
		M.mpeg2filename, cfs, lfs, null);
}
#endif

gboolean main_window_delete_event(void * w, void * e, void * d)
{
    (void)w; (void)e; (void)d;

    zfree(&G.framecache); zfree(&G.rgbmem); zfree(&G.framemem);

    save_cutpointfile();

    gtk_main_quit();
    return false;
}

#if 0
gboolean on_key_release(GtkWidget * w, GdkEventKey * k,
			gpointer user_data)
{
    (void)w; (void)k; (void)user_data;

    if (A.drawstate <= 0) {
	A.drawstate = 2;
	update_window();
    }
    return true;
}
#endif

gboolean on_key_press(GtkWidget * w, GdkEventKey * k,
		      gpointer user_data)
{
    static int prevkey = 0;
    static int pagestep = 256;

    (void)w; (void)user_data;

    static struct timeval ptv = { 0, 0 };
    struct timeval tv;
    gettimeofday(&tv, null);
    unsigned int timediff = (tv.tv_sec - ptv.tv_sec) * 1000
	+ (tv.tv_usec - ptv.tv_usec) / 1000;

    d1(("key press: %x %d, %d %d", k->keyval, k->keyval, W.drawing, timediff));

    if (timediff < 100) return false;
    if (W.drawing) return false;
    ptv = tv;

    // check
    // /usr/include/gtk-2.0/gdk/gdkkeysyms.h
    // http://library.gnome.org/devel/gdk/stable/gdk-Event-Structures.html
    // ( .../#GdkEventKey to the above )

    int shift = k->state & GDK_SHIFT_MASK; // XXX undocumented.
    int ctrl = k->state & GDK_CONTROL_MASK; // XXX undocumented.
    int alt = k->state & GDK_MOD1_MASK; // XXX undocumented.
    int i = 0, keycount = 0;

    switch (k->keyval)
    {
    case GDK_Right: 
	if (ctrl) {
	    next_gop(3);
	} else if (shift) {
	    next_gop(1);
	} else if (alt) {
	    next_frame();
	} else {
	    for (i = 0; i < 3; i++)
		next_frame();
	}
	break;

    case GDK_Left:
	if (ctrl) {
	    previous_gop(3);
	} else if (shift) {
	    previous_gop(1);
	} else if (alt) {
	    previous_frame();
	} else {
	    for (i = 0; i < 3; i++)
		previous_frame();
	}
	break;

    case GDK_Up:
	if (ctrl)
	    next_gop(1240);
	else if (shift)
	    next_gop(124);
	else if (alt) 
	    next_gop(10);
	else
	    next_gop(24);
	break;

    case GDK_Down:
	if (ctrl)
	    previous_gop(1240);
	else if (shift)
	    previous_gop(124);
	else if (alt) 
	    previous_gop(10);
	else
	    previous_gop(24);
	break;

    case GDK_Page_Up:
	if (prevkey == GDK_Page_Up) {
	    keycount++;
	    next_gop(keycount > 20? 25: 5);
	    break;
	}
	else if (prevkey == GDK_Page_Down && keycount > 20) {
	    keycount += 10 * 1000 * 1000;
	    if (keycount < 20 * 1000 * 1000) { next_gop(25); break; }
	}
	keycount = 0; next_gop(5); break;

    case GDK_Page_Down:
	if (prevkey == GDK_Page_Down) {
	    keycount++;
	    previous_gop(keycount > 20? 25: 5);
	    break;
	}
	else if (prevkey == GDK_Page_Up && keycount > 20) {
	    keycount += 10 * 1000 * 1000;
	    if (keycount < 20 * 1000 * 1000) { previous_gop(25); break; }
	}
	keycount = 0; previous_gop(5); break;

    case '.': next_cutpoint(); break;
    case ',': previous_cutpoint(); break;

    case GDK_Tab: cut_here(); break;
    case GDK_BackSpace: merge_here(); break;

    case GDK_h: show_help(); break;
    case GDK_i: show_info(); break;
    case GDK_t: test_cutpoint(); break;
    //case GDK_a: show_agraph(); break;

    case GDK_q:
	if (prevkey == GDK_q)
	    main_window_delete_event(null,null,null);
	break;

    case GDK_1: case GDK_2: case GDK_3:
    case GDK_4: case GDK_5: case GDK_6:
    case GDK_7: case GDK_8: case GDK_9:
	goto_percent((k->keyval - GDK_0) * 10 - 5);
	break;
    case GDK_0: goto_percent(95);
	break;
    }
    prevkey = k->keyval;
    return true;
}

gboolean on_darea_expose(GtkWidget * w, GdkEventExpose * e, gpointer user_data)
{
    Frame * f = G.frame;
#if 0 // XXX poista
    char str[32];
    int l;
#endif
    (void)e; (void)user_data;

    d1(("expose: frame %d,  trn %d", f->frameno, f->trn));

    if (A.leveldata)
	gdk_draw_rgb_image(w->window, w->style->fg_gc[GTK_STATE_NORMAL],
			   0, 0, G.maxwidth, AUDIOVIEW_HEIGHT,
			   GDK_RGB_DITHER_MAX, A.rgb, G.maxwidth * 3);

    //f->width = 640;
    //f->height = 480;
    gdk_draw_rgb_image(w->window, w->style->fg_gc[GTK_STATE_NORMAL],
		       (G.maxwidth - f->width) / 2,
		       (G.maxheight - f->height) / 2 + AUDIOVIEW_HEIGHT,
		       f->width, f->height,
		       GDK_RGB_DITHER_MAX, f->rgb[0], f->width * 3);

#if 0
    gdk_pango_renderer_set_drawable(GDK_PANGO_RENDERER(W.renderer), w->window);
    gdk_pango_renderer_set_gc(GDK_PANGO_RENDERER(W.renderer), W.gcs);

    int wxp = f->pixwidth * f->width;
    int hxp = f->pixheight * f->height;

    float asr = (double)wxp /(double)hxp;

    int width, height;

    if (f->pixwidth > f->pixheight) {
	width = wxp / f->pixheight;
	height = f->height;
    }
    else {
	width = f->width;
	height = hxp / f->pixwidth;
    }

    l = snprintf(str, sizeof str,
		 "%dx%d  %d/%d  %dx%d  %.2f\n", f->width, f->height,
		 f->pixwidth, f->pixheight, width, height, asr);

    pango_layout_set_text (W.layout, str, l);
    //int width, height;
    pango_layout_get_size(W.layout, &width, &height);

    pango_renderer_draw_layout (W.renderer, W.layout,
				(G.maxwidth - 50) * PANGO_SCALE - width,
				7 * PANGO_SCALE);
    //			(G.maxheight - 10) * PANGO_SCALE - height);
    gdk_pango_renderer_set_gc(GDK_PANGO_RENDERER(W.renderer), W.gct);

    pango_renderer_draw_layout (W.renderer, W.layout,
				(G.maxwidth - 52) * PANGO_SCALE - width,
				5 * PANGO_SCALE);
#endif
    draw_triangle();

//done:
    //usleep(100 * 1000);
    W.drawing = false;
    return true;
}

/* grabbed code from gdk-Pango-Interaction.html */
void make_layout_etc(GdkDrawable * drawable)
{
    // tries, tries...
    //GdkDisplay * display = gdk_display_get_default();
    //GdkScreen * screen = gdk_display_get_default_screen();
    //GdkPixmap * pixmap = gdk_pixmap_new(null, 1, 1,

    GdkScreen * screen = gdk_drawable_get_screen (drawable);

    W.renderer = gdk_pango_renderer_get_default (screen);
    W.gct = gdk_gc_new (drawable);
    W.gcs = gdk_gc_new (drawable);

    W.gcz[0] = gdk_gc_new (drawable);
    W.gcz[1] = gdk_gc_new (drawable);

#if 0
    PangoContext * context = gdk_pango_context_get_for_screen (screen);
    W.layout = pango_layout_new (context);

    pango_layout_set_font_description (W.layout, W.fd);
#endif
    // figure out better color
    GdkColor color = { .red = 32768, .green = 32768, .blue  = 65535 };
    gdk_gc_set_rgb_fg_color(W.gct, &color);
    color.red = color.green = color.blue = 0;
    gdk_gc_set_rgb_fg_color(W.gcs, &color);

    GdkGCValues values;
    values.line_width = 2;
    gdk_gc_set_values(W.gcs, &values, GDK_GC_LINE_WIDTH);

    color.red = 65535;
    gdk_gc_set_rgb_fg_color(W.gcz[0], &color);
    color.red = 0; color.green = 65535;
    gdk_gc_set_rgb_fg_color(W.gcz[1], &color);

}

#ifndef DISABLE_WARP

/* This function is added as my desktop has focus-follows-mouse
   set; when this window opens, mouse is warped there so it can
   preserve focus. This should actually be done by window mangager
   which has this 'focus-follows-mouse' configured -- but well,
   I wanted to test this warping thing... */

gboolean warp_pointer(GtkWidget * wid, GdkEvent * e, gpointer any_ptr)
{
    (void)e;
    GdkDisplay * display = gdk_display_get_default();
    GdkScreen * screen = gdk_display_get_default_screen(display);

    gint x, y, w, h;
    gdk_window_get_root_origin(wid->window, &x, &y);
    gdk_drawable_get_size(wid->window, &w, &h);

    // not exactly centered, but good enough
    gdk_display_warp_pointer(display, screen, x + w / 2, y + h / 2);

    g_signal_handlers_disconnect_by_data(wid, any_ptr);
    return false;
}
#endif /* DISABLE_WARP */

int init_W(void)
{
    memset(W.labeltext, 0, sizeof W.labeltext);
    strcpy(W.labeltext,
	   "00:00:00.00 123456 / 00:00:00.00 123456 | 12345 00 I |  000x000  00/00\n"
	   "00:00:00.00      0 / 00:00:00.00      0 | -   0    0 |  000x000  0.00");
    label_frametime(W.labeltext + 21, G.lastframe);

    gtk_widget_show(W.da = aDrawingArea(G.maxwidth,
					G.maxheight + AUDIOVIEW_HEIGHT));
    gtk_signal_connect(GTK_OBJECT(W.da), "expose-event",
		       GTK_SIGNAL_FUNC(on_darea_expose), null);

    /* note: W.da->window is null until parent widget has delivered some
       messages to it */

    //W.i = gtk_image_new();//_from_pixmap(W.pm, null);

    W.w = aWindow("M2vCut", main_window_delete_event, null, 6, false, true,
		  aVBox(false, 6, W.da,
			/*af = aFrame("Video/Messages", sw),*/ true, true, 0,
			W.l = aLabel("Press 'h' for help\n"), false, false, 0,
			W.i = gtk_image_new(), false, false, 0,
			null));

    W.fd = pango_font_description_from_string("Monospace bold 8");
    gtk_widget_modify_font(W.l, W.fd);
    make_layout_etc(W.da->window);

    W.pm = gdk_pixmap_new(W.da->window, G.maxwidth, 22, -1);
    gtk_image_set_from_pixmap(GTK_IMAGE(W.i), W.pm, null);

    //gdk_draw_rectangle(W.pm, W.gct, true, 0, 0, G.maxwidth, 22);
    //gtk_widget_queue_draw(W.i);

#ifndef DISABLE_WARP
    g_signal_connect(G_OBJECT(W.w), "configure-event",
		     G_CALLBACK(warp_pointer), (gpointer)&W.w); // any ptr
#endif

#if 0
    int xpad, ypad;
    gtk_misc_get_padding(GTK_MISC(W.l), &xpad, &ypad);
    gtk_misc_set_padding(GTK_MISC(W.l), 10, ypad);
#endif
    // da and af did not get these events (no window?)
    //gtk_widget_add_events(w, GDK_KEY_PRESS_MASK);
    gtk_signal_connect(GTK_OBJECT(W.w), "key-press-event",
		       GTK_SIGNAL_FUNC(on_key_press), null);
#if 0
    gtk_signal_connect(GTK_OBJECT(W.w), "key-release-event",
		       GTK_SIGNAL_FUNC(on_key_release), null);
#endif
/* printf("%x\n", GDK_WINDOW_XID(da->window)); */
    return 0;
}

static void init_A(const char * filename)
{
    int fd;
    struct stat st;

    memset(&A, 0, sizeof A);
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
	// warn("Cannot open '%s':", filename);
	return;
    }
    if (fstat(fd, &st) < 0)
	die("fstat on file '%s' failed:", filename);
    // XXX check type...
    if (st.st_size == 0) {
	// warn...
	close(fd);
	return;
    }
    A.levelsize = st.st_size;
    A.leveldata = mmap(null, A.levelsize, PROT_READ, MAP_SHARED, fd, 0);
    if (A.leveldata == MAP_FAILED)
	die("mmap on file '%s' failed:", filename);
    close(fd);

    A.rgb = _alloc_avis_buffer(); // active
    update_alevels();

    d1(("init_A(): A.levelsize: %d\n", A.levelsize));
}


void init(int argc, char * argv[])
{
    init_G(argv[0]);
    gtk_init(&argc, &argv);
    FILE * fh = open_indexfile(argv[1]);
    init_W();
    init_M(argv[2]);
    read_indexfile(fh);
    (void)load_cutpoints(argv[3], 0);
    init_A(argv[4]);
    update_image();
    alloc_frames();
    G.frame = G.framecache[0];
    need_iframe(0, true);
}

int main(int argc, char * argv[])
{
    if (argc != 5) {
	WriteCS(2, "Usage: m2cut-gui indexfile videofile "
		/**/ "cutpointfile audiolevelfilename\n");
	return 1;
    }
    dx(printf(" --- debugging output enabled ---\n"));

    init(argc, argv);
    gtk_main();
    return 0;
}
