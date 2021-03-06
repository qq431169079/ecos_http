/*
 * EAPOL variables
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * 
 * $Id: keapol.h,v 1.4 2010-07-06 02:12:15 Exp $
 */
#ifndef __KEAPOL_H__
#define __KEAPOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h>

#ifdef _KERNEL
struct keapol {
	struct keapol *next;

	char pathname[32];
	struct ifnet *ifp;
	unsigned short type;
	void *devtab;
};

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __KEAPOL_H__ */
