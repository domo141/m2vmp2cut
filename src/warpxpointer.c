#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -e; TRG=`basename $0 .c`; rm -f "$TRG"
 WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 date=`date`; set -x
 #${CC:-gcc} -ggdb $WARN "$@" -o "$TRG" "$0" -DCDATE="\"$date\""
 ${CC:-gcc} -O2 $WARN "$@" -o "$TRG" "$0" -lX11 -DCDATE="\"$date\""
 exit $?
 */
#endif
/*
 * $Id; warpxpointer.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 2008 Tomi Ollila
 *	Copyright (c) 2008
 *	    All rights reserved
 *
 * Created: Thu Jun 12 16:35:18 EEST 2008 too
 * Last modified: Thu Jun 12 21:40:29 EEST 2008 too
 */

#include <X11/Xlib.h>
#if 0
#include <X11/Xutil.h>
#include <X11/Xos.h>
#endif

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <poll.h>

#define null 0

struct {
    const char * prgname;
    Display * display;
} G;

static void msleep(int msecs)
{
    poll(null, 0, msecs);
}

static Window findwin(Window win, const char * name)
{
    Window r, p;
    Window * chlds;
    unsigned int nchld;

    if (XQueryTree(G.display, win, &r, &p, &chlds, &nchld) != 0)
    {
	unsigned int i;
	Window rw = None;

	for (i = 0; i < nchld; i++)
	{
	    char * wn;
	    if (XFetchName(G.display, chlds[i], &wn) != 0) {
		if (strncmp(wn, name, strlen(name)) == 0) {
		    rw = chlds[i];
		    XFree(wn);
		    break;
		}
		XFree(wn);
	    }
	    if ((rw = findwin(chlds[i], name)) != None)
		break;
	}
	XFree(chlds);
	return rw;
    }
    return None;
}

static void closeDisplay(void)
{
    XCloseDisplay(G.display);
}

int main(int argc, char * argv[])
{
    int screen, window_width, window_height;
    int x, y;
    Window rootw, win;
    int tries = 0;
    char * name = null;

    G.prgname = argv[0];

    while (argc > 2) {
	if (strcmp(argv[1], "-trysecs") == 0) {
	    tries = atoi(argv[2]) * 2;
	    argc-= 2; argv+= 2;
	    continue;
	}
	if (strcmp(argv[1], "-name") == 0) {
	    name = argv[2];
	    argc-= 2; argv += 2;
	    continue;
	}
	break;
    }

    if (argc < 3) return 1;

    if ((G.display = XOpenDisplay(null)) == null)
	return 2;

    atexit(closeDisplay);

    screen = DefaultScreen(G.display);

    win = rootw = RootWindow(G.display, screen);

    if (name) {
	int i;
	XWindowAttributes xwinattrs;

	if (tries > 0) {
	    for (i = 0; i < tries; i++) {
		msleep(500);
		if ((win = findwin(rootw, name)) != None)
		    break;
	    }
	}
	else
	    win = findwin(rootw, name);
	if (!win)
	    exit(1);
	if (!XGetWindowAttributes(G.display, win, &xwinattrs))
	    exit(2);
	window_width = xwinattrs.width;
	window_height = xwinattrs.height;
    }
    else {
	window_width = DisplayWidth(G.display, screen);
	window_height = DisplayHeight(G.display, screen);
    }

    if (argv[1][0] == 'c') x = window_width / 2;
    else x = atoi(argv[1]);

    if (argv[2][0] == 'c') y = window_height / 2;
    else y = atoi(argv[2]);

    if (x < 0) x = window_width + x;
    if (y < 0) y = window_height + y;

    XWarpPointer(G.display, None, win, 0,0,0,0, x, y);

    return 0;
}
