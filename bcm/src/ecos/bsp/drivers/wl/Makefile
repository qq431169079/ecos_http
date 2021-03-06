#
# Broadcom wl driver Makefile for ecos bsp
#
# Copyright (C) 2010, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile,v 1.5 2010-08-20 02:36:21 Exp $
#

include $(BSPDIR)/.config

# wl_generic.mk has other "all" target
all : cp_variant

# Generate list of supported variants
VLIST_BASE     ?= AP STA APSTA

VLIST          += $(VLIST_BASE)

ifeq ($(CONFIG_WL_USBAP),y)
# Do variants because we need two drivers for dual band operation
WL_VARIANTS    := 1
VLIST          += HIGH
# Note that we don't use CONFIG_WL_USE_HIGH because we need to leave WL_SUFFIX for
# the other driver. HIGH driver is always called wl_high and loaded upon hotplut
# event.
endif

ifeq ($(BUILD_WL_ONCHIP_G),true)
  VLIST        += $(foreach base,$(VLIST_BASE),$(base)_ONCHIP_G)
endif # BUILD_WL_ONCHIP_G

# Set suffix and config based on the selected variant
WL_SUFFIX      := $(strip $(foreach v,$(VLIST),$(if $(CONFIG_WL_USE_$(v)),$(v))))
CONFIG_WL_CONF := $(CONFIG_WL_$(WL_SUFFIX))

# Module file names use lowercase of the suffix
wl_suffix      := $(shell echo "$(WL_SUFFIX)" | tr A-Z a-z)

# Include generic make file
include ./wl_generic.mk

#
# Leave rule to make
#



# Copy variant prebuild to target
cp_variant:
ifneq ($(wildcard $(MODULE).gz),$(MODULE).gz)
	@echo "Ignore copy variant..."
else
	cp wl_$(wl_suffix)/obj-wl_$(wl_suffix).o.gz ./$(MODULE).gz
endif

# Target rule
ifneq ($(wildcard $(MODULE).gz),$(MODULE).gz)
$(MODULE): wlconf.h $(ALL_OBJS)
	$(XLD) -r $(ENDIAN) -o $(MODULE) $(filter $(ALL_OBJS), $^)
endif
