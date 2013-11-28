#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -e; TRG=`basename $0 .c`; rm -f "$TRG"
 WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 eval `cat config/mpeg2.conf`
 xo=`pkg-config --cflags --libs gtk+-2.0 | sed 's/-I/-isystem /g'`
 xo="$xo -lutil $mpeg2_both -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
 case $1 in '') set x -ggdb
 #case $1 in '') set x -O2
	shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$TRG" "$0" $xo
 exit 0
 */
#endif
/*
 * $ merge-gui.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 2012 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Fri 29 Mar 2013 13:59:17 EET too
 * Last modified: Fri 29 Mar 2013 14:01:41 EET too
 */

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#define execvp xexecvp

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <sys/wait.h>
#include <signal.h>

#include <gtk/gtk.h>

#undef execvp // tired of compilation warnings -- this generally works...
int execvp(const char * file, const char * argv[]);

#if (__GNUC__ >= 4)
#define GCCATTR_SENTINEL __attribute ((sentinel))
#else
#define GCCATTR_SENTINEL
#endif

#if (__GNUC__ >= 3)
#define GCCATTR_PRINTF(m, n) __attribute__ ((format (printf, m, n)))
#define GCCATTR_NORETURN __attribute ((noreturn))
#define GCCATTR_CONST    __attribute ((const))
#else
#define GCCATTR_PRINTF(m, n)
#define GCCATTR_NORETURN
#define GCCATTR_CONST
#endif

#define null ((void*)0)

#define isizeof (int)sizeof

void die(const char * format, ...) GCCATTR_NORETURN GCCATTR_PRINTF(1, 2);
void die(const char * format, ...)
{
    int err = errno;
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    if (format[strlen(format) - 1] == ':')
	fprintf(stderr, " %s.\n", strerror(err));
    else
	fprintf(stderr, "\n");
    exit(1);
}

void movefd(int o, int n)
{
    if (o == n)
	return;
    dup2(o, n);
    close(o);
}

struct {
    const char * srcdir;
    pid_t childpid;
    char * assel_pl;
    int audiofc;
    struct {
	char * name;
	GtkWidget * lw;
	GtkWidget * tw; // could be dropped if done like lw //
	GtkWidget * lang;
	GtkWidget * enabled;
    } adata[10];
    GtkWidget * atable;
    // The code above and below happen to be identical ATM... //
    int subtfc;
    struct {
	char * name;
	GtkWidget * lw;
	GtkWidget * tw; // could be dropped if done like lw //
	GtkWidget * lang;
	GtkWidget * enabled;
    } sdata[10];
    GtkWidget * stable;

} G;

void dev_error(const char * e, int line)
{
    die("!!! Dev error '%s' in " __FILE__ " line %d !!!\n", e, line);
}
#define dev_check(e) do { if (!(e)) dev_error(#e, __LINE__); } while (0)

void run_fast_vcmd(const char * av[])
{
    pid_t pid;
    switch( (pid = fork()) )
    {
    case -1:
	die("fork failed:");
    case 0:
	execvp(av[0], av); // XXX const works at least on unix systems.
	exit(1);
    };
    waitpid(pid, null, 0);
}

int run_ipipe(const char * command, ...) GCCATTR_SENTINEL;
int run_ipipe(const char * command, ...)
{
    int pfd[2];
    if (pipe(pfd) < 0)
	die("pipe failed:");

    switch( (G.childpid = fork()) )
    {
    case -1:
	die("fork failed:");
    case 0:
	/* child */
	close(pfd[0]);
	movefd(pfd[1], 1);
    {
	va_list ap;
	const char * pp[512]; // XXX fixed.
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
    }}
    /* parent */
    close(pfd[1]);
    return pfd[0];
}

void iwait(void)
{
    wait(null);
    G.childpid = 0;
}

void * xmalloc(size_t size)
{
    void * p = malloc(size);
    if (p == null)
	die("Out of Memory!");
    return p;
}


void sigchld_handler(int sig)
{
    (void)sig;
    wait(null);
    G.childpid = 0;
}

void init_G(const char * srcdir)
{
    memset(&G, 0, sizeof G);

    G.srcdir = srcdir;

    const char * p = getenv("M2VMP2CUT_CMD_PATH");
    if (p == null)
	die("env var 'M2VMP2CUT_CMD_PATH' not set");
    int l = strlen(p);
    G.assel_pl = (char *)xmalloc(l + 1 + 9 + 1);
    sprintf(G.assel_pl, "%s/assel.pl", p);

    struct sigaction action;

    memset(&action, 0, sizeof action);
    action.sa_handler = sigchld_handler;
    action.sa_flags = SA_RESTART|SA_NOCLDSTOP; /* NOCLDSTOP needed if ptraced */
    sigemptyset(&action.sa_mask);
    sigaction(SIGCHLD, &action, NULL);
}

void swap_contents(char ** n1, GtkWidget * l1, GtkWidget * b1, GtkWidget * e1,
		   char ** n2, GtkWidget * l2, GtkWidget * b2, GtkWidget * e2)
{
    char * text = strdup(gtk_label_get_text(GTK_LABEL(l1)));
    int enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b1));
    char * etxt = strdup(gtk_entry_get_text(GTK_ENTRY(e1)));

    gtk_label_set_text(GTK_LABEL(l1), gtk_label_get_text(GTK_LABEL(l2)));
    gtk_label_set_text(GTK_LABEL(l2), text);
    g_free(text);
    gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(b1),
	 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b2)));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b2), enabled);
    gtk_entry_set_text(GTK_ENTRY(e1), gtk_entry_get_text(GTK_ENTRY(e2)));
    gtk_entry_set_text(GTK_ENTRY(e2), etxt);
    g_free(etxt);

    char * p = *n1; *n1 = *n2; *n2 = p;
}

#if 0
void up_atable(GtkWidget * w, void * p)
{
    (void)w;
    int i = GPOINTER_TO_INT(p);
    GtkWidget * table;
    if (i >= 512) {
	table = G.stable;
	i -= 512;
    }
    else
	table = G.atable;

    dev_check(i > 0 && i < 10);

    int h = i - 1;
    int enabled =
}
#endif

void up_atable(GtkWidget * w, void * p)
{
    (void)w;
    int i = GPOINTER_TO_INT(p);
    dev_check(i > 0 && i < 10);
    int h = i - 1;
    char ** hnp = &G.adata[h].name;
    char ** inp = &G.adata[i].name;
    swap_contents(hnp, G.adata[h].lw, G.adata[h].enabled, G.adata[h].lang,
		  inp, G.adata[i].lw, G.adata[i].enabled, G.adata[i].lang);
}


void up_stable(GtkWidget * w, void * p)
{
    (void)w;
    int i = GPOINTER_TO_INT(p);
    dev_check(i > 0 && i < 10);
    int h = i - 1;
    char ** hnp = &G.sdata[h].name;
    char ** inp = &G.sdata[i].name;
    swap_contents(hnp, G.sdata[h].lw, G.sdata[h].enabled, G.sdata[h].lang,
		  inp, G.sdata[i].lw, G.sdata[i].enabled, G.sdata[i].lang);
}

GtkWidget * aLabel(const char * label)
{
    return gtk_label_new(label);
}

GtkWidget * aaLabel(const char * label, GtkWidget ** lwp)
{
    GtkWidget * aw = gtk_alignment_new(0, 0, 0, 0);
    GtkWidget * lw = gtk_label_new(label);

    gtk_container_add(GTK_CONTAINER(aw), lw);
    gtk_alignment_set_padding(GTK_ALIGNMENT(aw), 0, 0, 2, 0);
    *lwp = lw;
    return aw;
}

// some space below label -- I guess this work ok with different font( size)s
GtkWidget * abLabel(const char * label)
{
    GtkWidget * lw = gtk_label_new(label);
    GtkRequisition req;
    //gtk_misc_set_padding(GTK_MISC(lw), 0, 10);
    gtk_widget_size_request(lw, &req);
    gtk_widget_set_size_request(lw, req.width, req.height + 6);
    gtk_misc_set_alignment(GTK_MISC(lw), 0.5, 0.0);
    return lw;
}

GtkWidget * anEntry(const char * text)
{
    GtkEntry * entry = GTK_ENTRY(gtk_entry_new());

    gtk_entry_set_width_chars(entry, 3);

    if (text)
	gtk_entry_set_text(entry, text);
    gtk_entry_set_max_length(entry, 3);
    return GTK_WIDGET(entry);
}

GtkWidget * aLabeledEntry(const char * label, const char * text,
			  GtkWidget ** entryp)
{
    GtkBox * box = GTK_BOX(gtk_hbox_new(false, 1));
    gtk_box_pack_start(box, aLabel(label), true, true, 0);
    GtkWidget * entry = anEntry(text);
    gtk_box_pack_start(box, entry, true, true, 0);
    *entryp = entry;
    return GTK_WIDGET(box);
}



GtkWidget * aCheckButton(const char * text)
{
    return gtk_check_button_new_with_label(text);
}

void read_info(void)
{
    int fd = run_ipipe(G.assel_pl, G.srcdir, "info", null);
    FILE * fh = fdopen(fd, "r");
    if (fh == null)
	die("fdopen failed:");
    char buf[128];
    while (fgets(buf, sizeof buf, fh)) {
	if (strchr(buf, '\n') == null)
	    die("too long line: '%s'", buf);
	char * t = strtok(buf, " \n");
	if (t == null) continue;
	char * f = strtok(null, " \n");
	if (f == null) continue;
	char * l = strtok(null, " \n");
	if (l == null) continue;
	char * e = strtok(null, " \n");
	if (e == null) continue;
	if ( (*t != 'a' && *t != 's') || t[1] != '\0')
	    continue;
	if (*t == 'a') {
	    int i = G.audiofc;
	    if (i >= isizeof G.adata / isizeof G.adata[0])
		continue;
	    G.adata[i].name = strdup(f);
	    G.adata[i].tw = aLabeledEntry("  lang:", l, &G.adata[i].lang);
	    G.adata[i].enabled = aCheckButton("mux");
	    if (*e != '0')
		gtk_toggle_button_set_active
		    (GTK_TOGGLE_BUTTON(G.adata[i].enabled), true);
	    G.audiofc = i + 1;
	}
	else {
	    int i = G.subtfc;
	    if (i >= isizeof G.sdata / isizeof G.sdata[0])
		continue;
	    G.sdata[i].name = strdup(f);
	    G.sdata[i].tw = aLabeledEntry("  lang:", l, &G.sdata[i].lang);
	    G.sdata[i].enabled = aCheckButton("mux");
	    if (*e != '0')
		gtk_toggle_button_set_active
		    (GTK_TOGGLE_BUTTON(G.sdata[i].enabled), true);
	    G.subtfc = i + 1;
	}
    }
    iwait();
}

void main_quit(GtkWidget * w, void * p)
{
    (void)w; (void)p;
    gtk_main_quit();
}

void main_quit_0(GtkWidget * w, void * evp)
{
    (void)w;
    *((int *)evp) = 0;
    gtk_main_quit();
}

void save_and_quit_0(GtkWidget * w, void * evp)
{
    (void)w;
    const char * av[25];

    av[0] = G.assel_pl;
    av[1] = G.srcdir;
    av[2] = "save";
    int ac = 3;

    // add save code //
    int len = 4096;
    char * buf = (char *)xmalloc(len);
    char * p = buf;
    for (int i = 0; i < G.audiofc; i++) {
	snprintf(p, len, "%s %s %d", G.adata[i].name,
		 gtk_entry_get_text(GTK_ENTRY(G.adata[i].lang)),
		 gtk_toggle_button_get_active
		 /**/ (GTK_TOGGLE_BUTTON(G.adata[i].enabled))? 1: 0);
	av[ac++] = p;
	int l = strlen(p) + 1;
	p += l;
	len -= l;
	dev_check(len > 100);
    }
    for (int i = 0; i < G.subtfc; i++) {
	snprintf(p, len, "%s %s %d", G.sdata[i].name,
		 gtk_entry_get_text(GTK_ENTRY(G.sdata[i].lang)),
		 gtk_toggle_button_get_active
		 /**/ (GTK_TOGGLE_BUTTON(G.sdata[i].enabled))? 1: 0);
	av[ac++] = p;
	int l = strlen(p) + 1;
	p += l;
	len -= l;
	dev_check(len > 100);
    }
    dev_check(ac < 24);

    av[ac++] = null;
    run_fast_vcmd(av);

    main_quit_0(0, evp);
}

#ifndef DISABLE_WARP // see discussion in m2cvut-gui.c

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


GtkWidget * aButton(const char * label,
		    void (*cb)(GtkWidget * w, void * p), void * p)
{
    GtkWidget * button = GTK_WIDGET(gtk_button_new_with_label(label));
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(cb), p);
    return button;
}

gboolean grab_focus(void * p)
{
    gtk_widget_grab_focus(GTK_WIDGET(p));

    return false;
}

GtkWidget * agButton(const char * label,
		     void (*cb)(GtkWidget * w, void * p), void * p)
{
    GtkWidget * button = aButton(label, cb, p);
    gtk_idle_add(grab_focus, button);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(cb), p);
    return button;
}

void addFrame(GtkBox * box, const char * text, GtkWidget * child)
{
    GtkWidget * frame = gtk_frame_new(text);
    gtk_container_add(GTK_CONTAINER(frame), child);
    gtk_box_pack_start(box, frame, true, true, 2);
}

void toTable(GtkTable * t, GtkWidget * w, int c, int r, int e)
{
    if (e) e = GTK_EXPAND;

    gtk_table_attach(t, w, c, c+1, r, r+1, GTK_FILL | e, 0, 0, 2);
}

GtkWidget * amButton(int i, int t)
{
#if 0
    if (i == 0) {
	GtkWidget * label = aLabel("\302\267");
	gtk_widget_set_size_request(label, 20, 1);
	// XXX  should maybe do test button,
	// execute gtk_widget_size_request() and then destroy it.
	return label;
    }
    // else
#endif
    void * p = GINT_TO_POINTER(i);
    // it would be nice to know whether one label could be shared with
    // multiple buttons...
    GtkWidget * label = gtk_label_new(" v ");
    gtk_label_set_angle(GTK_LABEL(label), 180);

    GtkWidget * button = GTK_WIDGET(gtk_button_new());
    gtk_container_add(GTK_CONTAINER(button), label);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(t? up_stable: up_atable), p);
    return button;
}

void lorpClicked(const char * cmd, void * p)
{
    if (G.childpid)
	return;
    close(run_ipipe(G.assel_pl, G.srcdir, cmd, (char *)p, null));
}
void lookClicked(GtkWidget * w, void * p) { (void)w; lorpClicked("look", p); }
void playClicked(GtkWidget * w, void * p) { (void)w; lorpClicked("play", p); }

GtkWidget * lorpButton(const char * text,
		       void (*f)(GtkWidget *, void *), void * p)
{
    GtkWidget * button = GTK_WIDGET(gtk_button_new_with_label(text));
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(f), p);
    return button;
}

// there might be better way but I don't want to spend time investigating...
static const char look[] = "    look    ";
static const char play[] = "    play    ";
GtkWidget * lookButton(void * p) { return lorpButton(look, lookClicked, p); }
GtkWidget * playButton(void * p) { return lorpButton(play, playClicked, p); }

const char * helptext =
    "There are more than one..";

int main(int argc, char * argv[])
{
    gtk_init(&argc, &argv);

    if (argc != 2)
	die("Usage: merge-gui srcdir\n");

    init_G(argv[1]);
    int ev = 1;

    read_info();

    GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER (window), 4);
    gtk_window_set_title(GTK_WINDOW(window), "Select muxable content");
    g_signal_connect(G_OBJECT(window), "delete_event",
		     G_CALLBACK(main_quit), null);

    GtkBox * vbox = GTK_BOX(gtk_vbox_new(false, 4));
    gtk_box_pack_start(vbox, aLabel(helptext), true, true, 2);
    GtkWidget * w;
    if (G.audiofc) {
	GtkTable * table = GTK_TABLE(gtk_table_new(G.audiofc, 5, false));
	gtk_container_set_border_width(GTK_CONTAINER (table), 2);
	gtk_table_set_col_spacings(table, 4);
	for (int i = 0; i < G.audiofc; i++) {
	    char * name = G.adata[i].name;
	    int c = 0;
	    toTable(table, aaLabel(name, &G.adata[i].lw), c++, i, 1);
	    if (i)
		toTable(table, amButton(i, 0), c, i, 0);
	    c++;
	    toTable(table, G.adata[i].enabled, c++, i, 0);
	    toTable(table, G.adata[i].tw, c++, i, 0);
	    toTable(table, playButton(name), c++, i, 0);
	}
	w = G.atable = GTK_WIDGET(table);
    }
    else
	w = abLabel("No audio content to mux");

    addFrame(vbox, "Audio", w);

    if (G.subtfc) {
	GtkTable * table = GTK_TABLE(gtk_table_new(G.subtfc, 5, false));
	gtk_container_set_border_width(GTK_CONTAINER (table), 2);
	gtk_table_set_col_spacings(table, 4);
	for (int i = 0; i < G.subtfc; i++) {
	    char * name = G.sdata[i].name;
	    int c = 0;
	    toTable(table, aaLabel(name, &G.sdata[i].lw), c++, i, 1);
	    if (i)
		toTable(table, amButton(i, 1), c, i, 0);
	    c++;
	    toTable(table, G.sdata[i].enabled, c++, i, 0);
	    toTable(table, G.sdata[i].tw, c++, i, 0);
	    toTable(table, lookButton(name), c++, i, 0);
	}
	w = G.stable = GTK_WIDGET(table);
    }
    else
	w = abLabel("No subtitle content to mux");

    addFrame(vbox, "Subtitles", w);

    GtkWidget * bbox = gtk_hbutton_box_new();
    gtk_box_pack_start(vbox, bbox, true, true, 2);
#define gka(c, w) gtk_container_add(GTK_CONTAINER(c), w)
    gka(bbox, aButton("Quit", main_quit, null));
    gka(bbox, agButton("Continue without saving", main_quit_0, &ev));
    gka(bbox, aButton("Save and continue", save_and_quit_0, &ev));
#undef gka

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox));
    gtk_widget_show_all(window);
#ifndef DISABLE_WARP
    g_signal_connect(G_OBJECT(window), "configure-event",
		     G_CALLBACK(warp_pointer), (gpointer)argv); // any ptr
#endif
    gtk_main();

    return ev;
}
