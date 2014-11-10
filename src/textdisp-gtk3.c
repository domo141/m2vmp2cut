#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -eu; trg=`basename "$0" .c`; rm -f "$trg"
 WARN="-Wall -Wno-long-long -Wstrict-prototypes -pedantic"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 xo=`pkg-config --cflags --libs gtk+-3.0 | sed 's/-I/-isystem /g'`
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$trg" "$0" $xo
 exit $?
 */
#endif
/*
 * $ textdisp.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 2012 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Tue 25 Sep 2012 22:10:56 EEST too
 * Last modified: Mon 10 Nov 2014 19:26:14 +0200 too
 */

#define _BSD_SOURCE
#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <gtk/gtk.h>
#include <cairo.h>

#include <stdbool.h>

#define null ((void*)0)

// (variable) block begin/end -- explicit liveness...
#define BB {
#define BE }

// at least we try to achieve these.
#define TLINES 30
#define TCOLUMNS 80

struct {
    int cpid;
    int cfd;
    char hold;
    cairo_t * cr;
    cairo_surface_t * surface;
    double top, advance, height; // these *should* be 14, 9 & 17
    double lineheight; // XXX document...
    int x, y;
    int drawstate;

    GtkWidget * image;
} G;

static void warn(const char * format, va_list ap)
{
    int error = errno; /* XXX is this too late ? */
    vfprintf(stderr, format, ap);
    if (format[strlen(format) - 1] == ':')
	fprintf(stderr, " %s\n", strerror(error));
    else
	fputs("\n", stderr);
}

void __attribute__((noreturn)) vdie(const char * format, va_list ap)
{
    warn(format, ap);
    exit(1);
}

void __attribute__((noreturn)) die(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    vdie(format, ap);
    va_end(ap);
}

int run_prog(char * argv[])
{
    int pfd[2];

    if (pipe(pfd) < 0)
	die("pipe:");

    int pid = fork();
    if (pid > 0) {
	close(pfd[1]);
	G.cpid = pid;
	return pfd[0];
    }
    if (pid < 0)
	die("fork:");
    /* child */
    close(pfd[0]);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);

    execvp(argv[0], argv);
    die("execvp:");
}

void main_quit(void)
{
    if (G.cpid)
	kill(G.cpid, SIGTERM);
    gtk_main_quit();
}
void sigchld_handler(int sig)
{
    (void)sig;
    if (G.cpid && waitpid(G.cpid, null, WNOHANG) == G.cpid)
	G.cpid = 0;
}

gboolean button_press(void * w, void * e, void * d)
{
    (void)w; (void)e; (void)d;
    if (G.cpid == 0)
	gtk_main_quit();
    return true;
}
gboolean key_press(void * w, GdkEventKey * k, void * d)
{
    (void)w; (void)d;
    if (G.cpid == 0)
	gtk_main_quit();
    else if (k->keyval == 'c' && k->state & GDK_CONTROL_MASK)
	main_quit();
    return true;
}

gboolean read_process_input(GIOChannel * source,
			    GIOCondition condition, gpointer data);

#define LBW 4.0 // left border width
#define TBW 4.0 // top border width

int main(int argc, char * argv[])
{
    gtk_init(&argc, &argv);

    BB;
    const char * title = null;
    for (; argc >= 2; argc--, argv++) {
	if (argv[1][0] != '-')
	    break;
	if (strcmp (argv[1], "--") == 0) {
	    argc--;
	    argv++;
	    break;
	}
	if (strcmp (argv[1], "-title") == 0) {
	    title = argv[2];
	    argc--; argv++;
	    continue;
	}
	if (strcmp (argv[1], "-hold") == 0) {
	    G.hold = true;
	    continue;
	}
	die("unregognized option '%s'\n", argv[1]);
    }

    if (title == null)
	die("title not set (option -title)");

    if (argc < 2)
	die("No command to execute");

    GtkWidget * window =  gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(window), title);

    g_signal_connect(G_OBJECT(window), "delete_event",
		     G_CALLBACK(main_quit), null);

    gtk_widget_realize(window);

#if 0
    // this does not work :O -- and i just have no clue...
    G.surface
	= gdk_window_create_similar_surface(gtk_widget_get_window(window),
					    CAIRO_CONTENT_COLOR, 728, 548);
#else
    G.surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 728, 548);
#endif
    G.cr = cairo_create(G.surface);
    G.image = gtk_image_new_from_surface(G.surface);

    gtk_container_add(GTK_CONTAINER(window), G.image);

    cairo_set_source_rgb(G.cr, 0.0, 0.0, 0.2);
    cairo_paint(G.cr);

    cairo_set_source_rgb(G.cr, 1.0, 1.0, 1.0);
    cairo_select_font_face(G.cr, "monospace",
			   CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    // attempt for 80x30, 9 pixels wide glyphs, 18 pixels line height (720x540)
    double trysize = 15.0;
    cairo_set_font_size(G.cr, trysize);

    cairo_font_extents_t fe;
    cairo_font_extents(G.cr, &fe);
    if (fe.max_x_advance < 9 && (fe.ascent + fe.descent) < 18) {
	trysize += 5.0;
	cairo_set_font_size(G.cr, trysize);
	cairo_font_extents(G.cr, &fe);
    }
    while (fe.max_x_advance > 9 || (fe.ascent + fe.descent) > 18) {
	trysize -= 1.0;
	cairo_set_font_size(G.cr, trysize);
	cairo_font_extents(G.cr, &fe);
	if (trysize < 5.0)
	    break;
    }

    G.top = fe.ascent + TBW;
    G.advance = fe.max_x_advance;
    G.lineheight = fe.ascent + fe.descent;

    cairo_move_to(G.cr, LBW, G.top);

#if 0
    printf("%f %f %f %f %f\n", fe.ascent, fe.descent,
	   fe.height, fe.max_x_advance, fe.max_y_advance);
#endif
    gtk_widget_show_all(window);

    struct sigaction action = {
	.sa_handler = sigchld_handler,
	.sa_flags = SA_RESTART|SA_NOCLDSTOP, /* NOCLDSTOP needed if ptraced */
    };
    sigemptyset(&action.sa_mask);
    sigaction(SIGCHLD, &action, NULL);

    G.cfd = run_prog(&argv[1]);

    GIOChannel * iochannel = g_io_channel_unix_new(G.cfd);

    // G.iosrc =
    g_io_add_watch(iochannel, G_IO_IN | G_IO_HUP, read_process_input, null);

    g_signal_connect(G_OBJECT(window), "button-press-event",
		     G_CALLBACK(button_press), null);
    g_signal_connect(G_OBJECT(window), "key-press-event",
		     G_CALLBACK(key_press), null);
    gtk_widget_add_events(GTK_WIDGET(window), GDK_BUTTON_PRESS_MASK);
    gtk_widget_add_events(GTK_WIDGET(window), GDK_KEY_PRESS_MASK);
    BE;
    gtk_main();

    return 0;
}

/* // just that I don't forget...

static void close_connection(void)
{
    if (G.sd < 0)
	return;
    //g_io_channel_close(G.ioc); // closes G.sd //
    g_io_channel_shutdown(G.ioc, false, null); // closes G.sd //

    // I (once) wasted fscking 2 hours for figuring this out !!!
    // Why on earth cannot the unref/shutdown drop this from poll sets ???
    g_source_remove(G.iosrc);

    G.sd = -1;
}
*/

void clear_line(void)
{
#if 0
    static int f = 1, g = 0;
    cairo_set_source_rgb(G.cr, 0.5 * (f++&1), 0.5 * (g++&1), 0.2);
#else
    cairo_set_source_rgb(G.cr, 0.0, 0.0, 0.2);
#endif
    cairo_rectangle(G.cr, LBW, TBW + G.y * G.lineheight, 720, G.lineheight);
    cairo_fill(G.cr);
    cairo_set_source_rgb(G.cr, 1.0, 1.0, 1.0);
    cairo_move_to(G.cr, LBW, G.top + G.y * G.lineheight);
}
void maybe_scroll(void)
{
    //printf("maybe scroll: y: %d\n", G.y);

    if (G.y == TLINES - 1) {
	// for data copying -- using deprecated function -- as attempting
	// to play with window's GdkWindow is even more dangerous...
	// let's see how scroll works... ;/ (overlapping surfaces...)
	cairo_set_source_surface(G.cr, G.surface, 0.0, -G.lineheight);

	cairo_rectangle(G.cr, LBW, TBW, 720, 540 - G.lineheight);
	cairo_fill(G.cr);
	cairo_set_source_rgb(G.cr, 1.0, 1.0, 1.0);
	clear_line();
    }
    else
	G.y++;

    cairo_move_to(G.cr, LBW, G.top + G.y * G.lineheight);
}
void draw_text(char * buf, int l)
{
    if (G.drawstate == 2)
	maybe_scroll();
    if (l > 0) {
	switch (G.drawstate) {
	case 3: maybe_scroll(); break;
	case 1: clear_line();	break;
	}
	char c = buf[l];
	buf[l] = '\0';
	//printf("draw (len %d): %s\n", l, buf);
	cairo_show_text(G.cr, buf);
	gtk_widget_queue_draw(G.image);
	buf[l] = c;
    }
#if 0
    else /* l == 0 and */ if (G.drawstate == 3)
	G.drawstate = 1;
#endif
    G.drawstate = 0;
}

gboolean read_process_input(GIOChannel * source,
			    GIOCondition condition, gpointer data)
{
    (void)source;
    (void)condition;
    (void)data;

    char buf[1024];

    int len = read(G.cfd, buf, sizeof buf);
    if (len <= 0) {
	if (! G.hold)
	    gtk_main_quit();
	/* no more data read -- and too shy to close fd */
	return false;
    }

    //printf("input text: len %d, text '%.*s'\n", len, len, buf);

    /* and now the fun begins -- render text to image */
    int x = G.x;
    char * p = buf;
    int o = 0;
    while (len > 0) {
	len--;
	char c = p[o];
	//fprintf(stderr, "%c", c);
	switch (c) {
	case '\n':
	    x = 0;
	    //printf("NL: %d: '%.*s'\n", o, o, p);
	    draw_text(p, o);
	    p += (o + 1);
	    G.drawstate = 2;
	    o = 0;
	    continue;
	case '\r':
	    //printf("CR: %d: '%.*s'\n", o, o, p);
	    x = 0;
	    int prevdrawstate = G.drawstate;
	    draw_text(p, o);
	    p += (o + 1);
	    if (o != 0 || prevdrawstate != 2)
		G.drawstate = 1;
	    o = 0;
	    continue;
	}
	if ((c & 0xe0) == 0xa0) { // utf-8 continuation char //
	    o++;
	    continue;
	}
	if (x < 79) {
	    x++;
	    o++;
	    continue;
	}
	if (x == 79) {
	    x = 80;
	    o++;
	    //printf("79: %d: '%.*s'\n", o, o, p);
	    draw_text(p, o);
	    p += o;
	    G.drawstate = 3;
	    o = 0;
	    continue;
	}
	if (! isspace(c)) {
	    x = 1;
	    o = 1;
	    continue;
	}
	// and last, drop trailing whitespace (o == 0)
	p++;
    }
    G.x = x;
    if (o) {
	//printf("rest: %d: '%.*s'\n", o, o, p);
	draw_text(p, o);
    }

    return true;
}
