#
# Broadcom BCM947XX Makefile for ecos bsp
# 
# Copyright (C) 2010, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile,v 1.2 2010-05-26 14:55:37 Exp $
#

MODULE = obj-$(shell pwd | sed "s/.*\///" ).o

all: $(MODULE)

include $(BSPDIR)/rules.mak

#
# Set CFLAGS or EXTRA_CFLAGS
#
EXTRA_CFLAGS += -DBCMDRIVER

# Enumerate files to compile
#
ALL_OBJS = board_main.o board_misc.o exception.o pcibios.o

#
# Leave rule to make
#
$(MODULE): $(ALL_OBJS:.o=.d)
	$(XLD) -r $(ENDIAN) -o $(MODULE) $(ALL_OBJS)
