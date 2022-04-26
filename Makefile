# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# binary name
APP = httpfake

# all source are stored in SRCS-y
SRCDIR=src
OBJDIR=obj
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJECTS  := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
#OBJECTS += src/main.o

PKGCONF ?= pkg-config

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

all: shared
.PHONY: shared static
shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)
static: build/$(APP)-static
	ln -sf $(APP)-static build/$(APP)

CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
#CFLAGS += -O0 -g  $(shell $(PKGCONF) --cflags libdpdk) -mavx512vl
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk) lib/libconfig.a
LDFLAGS_STATIC = $(shell $(PKGCONF) --static --libs libdpdk)

CFLAGS += -DALLOW_EXPERIMENTAL_API


$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

build/$(APP)-shared: $(OBJECTS) | build
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/$(APP)-static: $(OBJECTS) | build
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

build:
	@mkdir -p $@
	@mkdir -p obj

.PHONY: clean
clean:
	rm -f build/$(APP) build/$(APP)-static build/$(APP)-shared
	rm -fr obj/*
	test -d build && rmdir -p build || true
