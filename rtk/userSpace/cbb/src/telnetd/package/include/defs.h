/*
 * def.h
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:
 */

/*	$NetBSD: defs.h,v 1.16 2007/02/21 21:14:07 hubertf Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)defs.h	8.1 (Berkeley) 6/4/93
 */

/*
 * Telnet server defines
 */
#include <sys/types.h>
#include <sys/param.h>

#define DIAGNOSTICS

#if defined(PRINTOPTIONS) && defined(DIAGNOSTICS)
#define TELOPTS
#define TELCMDS
#define	SLC_NAMES
#endif

#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <netinet/in.h>

#include <telnet.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*
 * I/O data buffers defines
 */

#define BUFFER_SZ 0x4000
#define	NETSLOP	64

#define TELNETD_PRINT(a, b...)            diag_printf(a, ##b)


#define MAXHOSTNAMELEN  256             /* max hostname size */

#define	NIACCUM(c)	do { \
			    *netip++ = c; \
			    ncc++; \
			} while (0)

/* clock manipulations */
#define	settimer(x)	(clocks.x = ++clocks.system)
#define	sequenceIs(x, y)	(clocks.x < clocks.y)

/*
 * Linemode support states, in decreasing order of importance
 */
#define REAL_LINEMODE	0x04
#define KLUDGE_OK	0x03
#define	NO_AUTOKLUDGE	0x02
#define KLUDGE_LINEMODE	0x01
#define NO_LINEMODE	0x00

#define EXTPROC         0x00000800
#define _POSIX_VDISABLE ((unsigned char)'\377')

/*
 * Structures of information for each special character function.
 */
typedef struct {
	unsigned char	flag;		/* the flags for this function */
	cc_t		val;		/* the value of the special character */
} slcent, *Slcent;

typedef struct {
	slcent		defset;		/* the default settings */
	slcent		current;	/* the current settings */
	cc_t		*sptr;		/* a pointer to the char in */
					/* system data structures */
} slcfun, *Slcfun;

/*
 * Window/terminal size structure.  This information is stored by the kernel
 * in order to provide a consistent interface, but is not used by the kernel.
 */
struct winsize {
	unsigned short  ws_row;         /* rows, in characters */
	unsigned short  ws_col;         /* columns, in characters */
	unsigned short  ws_xpixel;      /* horizontal size, pixels */
	unsigned short  ws_ypixel;      /* vertical size, pixels */
};

#ifdef DIAGNOSTICS
/*
 * Diagnostics capabilities
 */
#define	TD_REPORT	0x01	/* Report operations to client */
#define TD_EXERCISE	0x02	/* Exercise client's implementation */
#define TD_NETDATA	0x04	/* Display received data stream */
#define TD_PTYDATA	0x08	/* Display data passed to pty */
#define	TD_OPTIONS	0x10	/* Report just telnet options */
#endif /* DIAGNOSTICS */

/*
 * We keep track of each side of the option negotiation.
 */

#define	MY_STATE_WILL		0x01
#define	MY_WANT_STATE_WILL	0x02
#define	MY_STATE_DO		0x04
#define	MY_WANT_STATE_DO	0x08

/*
 * Macros to check the current state of things
 */

#define	my_state_is_do(opt)		(options[opt]&MY_STATE_DO)
#define	my_state_is_will(opt)		(options[opt]&MY_STATE_WILL)
#define my_want_state_is_do(opt)	(options[opt]&MY_WANT_STATE_DO)
#define my_want_state_is_will(opt)	(options[opt]&MY_WANT_STATE_WILL)

#define	my_state_is_dont(opt)		(!my_state_is_do(opt))
#define	my_state_is_wont(opt)		(!my_state_is_will(opt))
#define my_want_state_is_dont(opt)	(!my_want_state_is_do(opt))
#define my_want_state_is_wont(opt)	(!my_want_state_is_will(opt))

#define	set_my_state_do(opt)		(options[opt] |= MY_STATE_DO)
#define	set_my_state_will(opt)		(options[opt] |= MY_STATE_WILL)
#define	set_my_want_state_do(opt)	(options[opt] |= MY_WANT_STATE_DO)
#define	set_my_want_state_will(opt)	(options[opt] |= MY_WANT_STATE_WILL)

#define	set_my_state_dont(opt)		(options[opt] &= ~MY_STATE_DO)
#define	set_my_state_wont(opt)		(options[opt] &= ~MY_STATE_WILL)

#define	set_my_want_state_dont(opt)	(options[opt] &= ~MY_WANT_STATE_DO)
#define	set_my_want_state_wont(opt)	(options[opt] &= ~MY_WANT_STATE_WILL)

/*
 * Tricky code here.  What we want to know is if the MY_STATE_WILL
 * and MY_WANT_STATE_WILL bits have the same value.  Since the two
 * bits are adjacent, a little arithmetic will show that by adding
 * in the lower bit, the upper bit will be set if the two bits were
 * different, and clear if they were the same.
 */
#define my_will_wont_is_changing(opt) \
			((options[opt]+MY_STATE_WILL) & MY_WANT_STATE_WILL)

#define my_do_dont_is_changing(opt) \
			((options[opt]+MY_STATE_DO) & MY_WANT_STATE_DO)

/*
 * Make everything symmetrical
 */

#define	HIS_STATE_WILL			MY_STATE_DO
#define	HIS_WANT_STATE_WILL		MY_WANT_STATE_DO
#define HIS_STATE_DO			MY_STATE_WILL
#define HIS_WANT_STATE_DO		MY_WANT_STATE_WILL

#define	his_state_is_do			my_state_is_will
#define	his_state_is_will		my_state_is_do
#define his_want_state_is_do		my_want_state_is_will
#define his_want_state_is_will		my_want_state_is_do

#define	his_state_is_dont		my_state_is_wont
#define	his_state_is_wont		my_state_is_dont
#define his_want_state_is_dont		my_want_state_is_wont
#define his_want_state_is_wont		my_want_state_is_dont

#define	set_his_state_do		set_my_state_will
#define	set_his_state_will		set_my_state_do
#define	set_his_want_state_do		set_my_want_state_will
#define	set_his_want_state_will		set_my_want_state_do

#define	set_his_state_dont		set_my_state_wont
#define	set_his_state_wont		set_my_state_dont
#define	set_his_want_state_dont		set_my_want_state_wont
#define	set_his_want_state_wont		set_my_want_state_dont

#define his_will_wont_is_changing	my_do_dont_is_changing
#define his_do_dont_is_changing		my_will_wont_is_changing

/*
 * Initialization buffer for tty device [16 characters long]
 */
#define NULL16STR	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
