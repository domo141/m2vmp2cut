#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -eu; case $1 in -o) trg=$2; shift 2
        ;;      *) trg=`exec basename "$0" .c` ;; esac; rm -f "$trg"
 WARN="-Wall -Wno-long-long -Wstrict-prototypes -pedantic"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$trg" "$0"
 exit $?
 */
#endif
/*
 * $ eteen.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2015 Tomi Ollila
 *          All rights reserved
 *
 * Created: Thu 22 Jan 2015 21:06:55 EET too
 * Last modified: Wed 18 Feb 2015 18:16:03 +0200 too
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// print stderr to file / fd n -- with '+' as first arg print cmd line too

#if (__GNUC__ >= 4 && ! __clang__) // compiler warning avoidance nonsense
static inline void WUR(ssize_t x) { x = x; }
#else
#define WUR(x) x
#endif

void die(const char * format, ...)
{
    int err = errno;
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    if (format[strlen(format) - 1] == ':')
        fprintf(stderr, " %s.\n", strerror(err));
    exit(1);
}

int waitchild(void)
{
    int status;
    while (wait(&status) < 0)
        sleep(1);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return WTERMSIG(status);

    // XXX //
    return 127;
}

void printcmdline(char ** args)
{
    char buf[1024];
    char * p = buf;
    *p++ = '+'; *p++ = ' ';
    while (*args) {
        const char * arg = *args;
        int l = strlen(arg);
        if (l == 0) {
            arg="''";
            l = 2;
        }
        if (p + l >= buf + sizeof buf - 3)
            break;
        memcpy(p, arg, l);
        args++;
        p += l;
        *p++ = ' ';
    }
    *p++ = '\n';
    WUR(write(2, buf, p - buf));
}

int getfd(char * arg)
{
    char * ep;
    errno = 0;
    long fd = strtol(arg, &ep, 10);
    if (errno == 0 && ep[0] == '/' && ep[1] == '\0')
        return fd;

    fd = open(arg, O_WRONLY|O_CREAT|O_APPEND, 0644);
    if (fd < 0)
        die("Opening file '%s' for writing:", arg);
    return fd;
}

int main(int argc, char * argv[])
{
    if (argc < 4)
        die("Usage: %s (+|-) (filename|fd/) command [args]\n", argv[0]);

    if ( (argv[1][0] != '-' && argv[1][0] != '+') || argv[1][1] != '\0' )
        die("arg '%s' not '-' nor '+'\n", argv[1]);

    int fd = getfd(argv[2]);

    int pipefd[2];
    if (pipe(pipefd) < 0)
        die("pipe:");

    switch (fork()) {
    case -1:
        die("fork:");
    case 0:
        // child
        close(fd);
        close(pipefd[0]);
        dup2(pipefd[1], 2);
        if (pipefd[1] != 2)
            close(pipefd[1]);
        if (argv[1][0] == '+')
            printcmdline(&argv[3]);
        execvp(argv[3], &argv[3]);
        die("Cannot execute '%s':", argv[3]);
    }
    // parent
    close(pipefd[1]);
    char buf[4096];

    while (1) {
        int l = read(pipefd[0], buf, sizeof buf);
        if (l > 0) {
            WUR(write(fd, buf, l));
            WUR(write(2, buf, l));
            continue;
        }
        if (l < 0)
            if (errno == EINTR)
                continue;

        exit(waitchild());
    }
    return 0;
}
