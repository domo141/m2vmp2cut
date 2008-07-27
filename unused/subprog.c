/*
 * $Id: subprog.c $
 *
 * Author: Tomi Ollila  too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Mon Oct 25 20:55:29 EEST 2004 too
 * Last modified: Tue Nov 02 17:44:39 EET 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 */

#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define DBGS 0
#include "x.h"

#include "subprog.h"
#include "ghdrs/subprog_priv.h"

static void fill_argvec(int ac, char ** av, char * arg,
			int rest_argc, char ** rest_argv, va_list ap)
{
  int i, j;

  ac--;
  for (i = 0; arg; arg = va_arg(ap, char *), i++)
    {
      if (i == 255)
	xerrf("Argument list too long.\n");

      av[i] = arg;
    }

  for (j = 0; j < rest_argc; j++, i++)
    {
      if (i == ac)
	xerrf("Argument list too long.\n");

      av[i] = rest_argv[j];
    }

  av[i] = NULL;
}  

#if 0
int launch_program(int rest_argc, char ** rest_argv, ...)
{
  char * argv[256];
  va_list ap;
  char * command;
  char * arg;

  va_start(ap, rest_argv);
  command = arg = va_arg(ap, char *);
  fill_argvec(sizeof argv / sizeof argv[0], argv, arg,
	      rest_argc, rest_argv, ap);
  va_end(ap);

  return s__fork_and_exec(command, argv);
}
#endif

static int s__fork_and_exec(char * command, char ** argv)
{
  int fds[2];
  int e;

  dup2(0, 3);
  if (pipe(fds) < 0)
    return -1;

  switch (fork())
    {
    case 0:
      /* child */
      close(3);
      close(fds[0]);
      return fds[1];
      
    case -1:
      e = errno;
      close(fds[0]);
      close(fds[1]);
      errno = e;
      return -1;
      /* fail */
    }

  /* parent */
  dup2(fds[0], 0);
  close(fds[1]);
  execvp(command, argv);
  return -1;
}      

#if 0
void close_wait_and_exit(int fd)
{
  int status = 1;

  close(fd);
  wait(&status);
  exit(status >> 8); /* XXX */
}
#endif

void launch_write_close_setsid_and_exit(const unsigned char * buf, int len,
					int rest_argc, char ** rest_argv, ...)
{
  char * argv[256];
  va_list ap;
  char * command;
  char * arg;
  int fd; 
  
  va_start(ap, rest_argv);
  command = arg = va_arg(ap, char *);
  fill_argvec(sizeof argv / sizeof argv[0], argv, arg,
	      rest_argc, rest_argv, ap);
  va_end(ap);

  fd = s__fork_and_exec(command, argv);
  if (fd < 0)
    xerrf("prg launch problem:");

  if (writefully(fd, buf, len) != len)
    xerrf("writefully failed:");

  close(fd);
  close(0); close(1); close(2); close(3); setsid();
  exit(0);
}

/*
 * Local variables:
 * mode: c
 * c-file-style: "gnu"
 * tab-width: 8
 * End:
 */
