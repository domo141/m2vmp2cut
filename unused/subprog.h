/*
 * $Id: subprog.h $
 *
 * Author: Tomi Ollila  too ät iki fi
 *
 *	Copyright (c) 2004 Tomi Ollila
 *	Copyright (c) 2004 OY Stinghorn LTD
 *	    All rights reserved
 *
 * Created: Mon Oct 25 20:58:07 EEST 2004 too
 * Last modified: Tue Oct 26 22:50:58 EEST 2004 too
 *
 * This program is licensed under the GPL v2. See file COPYING for details.
 *
 */

#ifndef SUBPROG_H
#define SUBPROG_H

#if 0
int launch_program(int rest_argc, char ** rest_argv, ...);
void close_wait_and_exit(int fd) GCCATTR_NORETURN;
#endif

void launch_write_close_setsid_and_exit(const unsigned char * buf, int len,
					int rest_argc, char ** rest_argv, ...)
  GCCATTR_NORETURN;


#endif /* SUBPROG_H */
