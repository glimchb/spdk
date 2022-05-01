#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (c) Intel Corporation.
#  All rights reserved.
#

SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

SO_VER := 4
SO_MINOR := 0

LIBNAME = sock_posix
C_SRCS = posix.c

PKGCONF ?= pkg-config
ifeq ($(CONFIG_OPENSSL),y)
LOCAL_SYS_LIBS += -lssl
ifneq ($(strip $(CONFIG_OPENSSL_PATH)),)
CFLAGS += -I$(CONFIG_OPENSSL_PATH)/include
LDFLAGS += -L$(CONFIG_OPENSSL_PATH)
else
# `libssl11` name is unique to Centos7 via EPEL
# So it's safe to add it here without additional check for Centos7
ifeq ($(shell $(PKGCONF) --exists libssl11 && echo 1),1)
CFLAGS  += $(shell $(PKGCONF) --cflags libssl11)
LDFLAGS += $(shell $(PKGCONF) --libs libssl11)
else
CFLAGS  += $(shell $(PKGCONF) --cflags libssl)
LDFLAGS += $(shell $(PKGCONF) --libs libssl)
endif
endif
endif

SPDK_MAP_FILE = $(SPDK_ROOT_DIR)/mk/spdk_blank.map

include $(SPDK_ROOT_DIR)/mk/spdk.lib.mk
