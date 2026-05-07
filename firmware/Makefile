# Firmware backend for mquickjs on LiteX.
#
# Most users should use ../make.py. This Makefile stays small and
# focused on compiling the C firmware once a LiteX build directory exists.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

# --- Inputs ----------------------------------------------------------

# `clean` and `help` can run without a LiteX SoC build directory — only
# compile / link targets need one. Including variables.mak with an empty
# BUILD_DIRECTORY yields a noisy cascade of failures; guard explicitly
# instead but let the clean path through.

NO_BUILDDIR_TARGETS := clean help

ifeq ($(filter $(NO_BUILDDIR_TARGETS),$(MAKECMDGOALS)),)
ifeq ($(BUILD_DIRECTORY),)
$(error BUILD_DIRECTORY is not set. Use ../make.py sim or pass a LiteX \
build directory explicitly.)
endif

BUILD_DIR := $(abspath $(BUILD_DIRECTORY))

# LiteX provides variables.mak (SOC_DIRECTORY, CPUFLAGS, LIBS, TRIPLE, …)
# and common.mak (CFLAGS/LDFLAGS/compile helpers).
include $(BUILD_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak
endif

MQJS_DIR := $(abspath third_party/mquickjs)
FW_DIR   := $(abspath .)
GEN_DIR  := $(abspath build)

# --- User script --------------------------------------------------------

# SCRIPT points to a JavaScript source file to embed. When unset, the
# firmware is built in interactive REPL mode.
SCRIPT ?=

ifeq ($(SCRIPT),)
EMBED_CFLAGS  :=
USER_SCRIPT_H :=
else
EMBED_CFLAGS  := -DMQJS_SCRIPT_EMBEDDED
USER_SCRIPT_H := $(GEN_DIR)/user_script.h
endif

# Optional: override the mquickjs heap size at build time. Drops the
# define straight into CFLAGS; the header's #ifndef guard lets us win.
ifneq ($(HEAP_SIZE),)
EMBED_CFLAGS += -DLITEX_MQJS_HEAP_SIZE=$(HEAP_SIZE)
endif
ifeq ($(MEMORY_DUMP),1)
EMBED_CFLAGS += -DLITEX_MQJS_DUMP_MEMORY=1
endif

# --- Sources ------------------------------------------------------------

MQJS_OBJS := mquickjs.o cutils.o dtoa.o libm.o
FW_OBJS   := main.o mqjs_port.o crt0.o
OBJECTS   := $(FW_OBJS) $(MQJS_OBJS)
GENERATED_SOC_HEADERS := \
	$(BUILD_DIR)/software/include/generated/csr.h \
	$(BUILD_DIR)/software/include/generated/soc.h \
	$(BUILD_DIR)/software/include/generated/variables.mak
BUILD_CONTEXT := $(GEN_DIR)/build_context.mk

# Tell make where to find .c / .S sources without also declaring .o
# search paths. A plain VPATH= would also match .o files, and the
# submodule's host-side `make mqjs` build leaves native x86 object
# files in the submodule directory — make would happily "find" them
# there and skip the cross-compile.
vpath %.c $(FW_DIR) $(MQJS_DIR) $(BIOS_DIRECTORY) $(BIOS_DIRECTORY)/cmds $(CPU_DIRECTORY)
vpath %.S $(FW_DIR) $(MQJS_DIR) $(BIOS_DIRECTORY) $(BIOS_DIRECTORY)/cmds $(CPU_DIRECTORY)

# GEN_DIR must come first: if the user has built the host-side `mqjs`
# interpreter inside third_party/mquickjs (`make -C third_party/mquickjs mqjs`),
# that build leaves a `mqjs_stdlib.h` and `mquickjs_atom.h` in the
# submodule directory. Those are tailored to the host's JSW, not ours,
# and would silently shadow our cross-compiled versions.
MQJS_INC := -I$(GEN_DIR) -I$(FW_DIR) -I$(MQJS_DIR)
CFLAGS   += $(MQJS_INC) $(EMBED_CFLAGS)

# We need libgcc for a handful of soft-float / long-long helpers that
# neither libcompiler_rt nor picolibc ship. LiteX's CPUFLAGS can use
# versioned RISC-V ISA strings, which GCC's multilib matcher rejects.
# Resolve libgcc with a compact ISA derived from the ABI instead.
ifeq ($(filter $(NO_BUILDDIR_TARGETS),$(MAKECMDGOALS)),)
TARGET_ABI     := $(shell echo "$(CPUFLAGS)" | sed -n 's/.*-mabi=\([^ ]*\).*/\1/p')
TARGET_XLEN    := $(if $(findstring lp64,$(TARGET_ABI)),64,32)
TARGET_RV_ARCH := rv$(TARGET_XLEN)im
ifneq ($(findstring f,$(TARGET_ABI)),)
TARGET_RV_ARCH := $(TARGET_RV_ARCH)af
endif
ifneq ($(findstring d,$(TARGET_ABI)),)
TARGET_RV_ARCH := $(TARGET_RV_ARCH)d
endif
TARGET_LIBGCC  := $(shell $(TARGET_PREFIX)gcc -march=$(TARGET_RV_ARCH) -mabi=$(TARGET_ABI) \
                     -print-libgcc-file-name)
endif

# Silence warnings that aren't relevant for the vendored mquickjs C
# files on this rv32 target (format typedef differences, intentionally
# unprototyped internals, unused debug helpers, large static tables).
# Keep -Wall elsewhere.
MQJS_EXTRA_CFLAGS := -Wno-unused-parameter -Wno-sign-compare \
                     -Wno-unused-function -Wno-unused-variable \
                     -Wno-unused-but-set-variable -Wno-format \
                     -Wno-missing-prototypes

# --- Host-side stdlib generator (runs on the build machine) ------------

HOST_CC     ?= gcc
HOST_CFLAGS ?= -Wall -O2 -g -D_GNU_SOURCE
STDLIB_GEN  := $(GEN_DIR)/mqjs_stdlib_litex
STDLIB_BUILD_C := $(GEN_DIR)/mquickjs_build_litex.c
STDLIB_H    := $(GEN_DIR)/mqjs_stdlib.h
ATOM_H      := $(GEN_DIR)/mquickjs_atom.h
MQJS_PATCH  := $(FW_DIR)/patches/mquickjs-typed-array-buffer.patch
MQJS_PATCHED_STAMP := $(GEN_DIR)/mquickjs-patched.stamp

# Force 32-bit layout so the generated tables match the JSW=4 target
# (RISC-V rv32). The generator itself is native.
STDLIB_GEN_FLAGS := -m32

# --- Top-level targets --------------------------------------------------

.PHONY: all clean help FORCE

all: firmware.bin

help:
	@echo "Usage:"
	@echo "  make BUILD_DIRECTORY=<litex build dir> [SCRIPT=<file.js>]"
	@echo "  make clean"
	@echo ""
	@echo "Variables:"
	@echo "  SCRIPT        JavaScript source to embed (default: REPL)"
	@echo "  HEAP_SIZE     mquickjs heap size override"
	@echo "  MEMORY_DUMP=1 print mquickjs heap statistics at exit"
	@echo "  V=1           verbose compile lines"

firmware.elf: $(OBJECTS) linker.ld
	$(CC) $(LDFLAGS) -T linker.ld -N -o $@ \
	    $(OBJECTS) \
	    $(PACKAGES:%=-L$(BUILD_DIR)/software/%) \
	    -Wl,--start-group \
	    $(LIBS:lib%=-l%) \
	    $(TARGET_LIBGCC) \
	    -Wl,--end-group \
	    -Wl,--gc-sections \
	    -Wl,-Map,$@.map
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif

$(BUILD_CONTEXT): FORCE | $(GEN_DIR)
	@{ \
	    echo "BUILD_DIRECTORY=$(BUILD_DIR)"; \
	    echo "TRIPLE=$(TRIPLE)"; \
	    echo "CPUFLAGS=$(CPUFLAGS)"; \
	    echo "TARGET_ABI=$(TARGET_ABI)"; \
	    echo "PACKAGES=$(PACKAGES)"; \
	    echo "LIBS=$(LIBS)"; \
	} | cmp -s - $@ || { \
	    { \
	        echo "BUILD_DIRECTORY=$(BUILD_DIR)"; \
	        echo "TRIPLE=$(TRIPLE)"; \
	        echo "CPUFLAGS=$(CPUFLAGS)"; \
	        echo "TARGET_ABI=$(TARGET_ABI)"; \
	        echo "PACKAGES=$(PACKAGES)"; \
	        echo "LIBS=$(LIBS)"; \
	    } > $@; \
	}

$(FW_OBJS): $(GENERATED_SOC_HEADERS) $(BUILD_CONTEXT)
$(filter-out mquickjs.o,$(MQJS_OBJS)): $(BUILD_CONTEXT)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif

# --- Generated files ----------------------------------------------------

$(GEN_DIR):
	mkdir -p $@

$(STDLIB_BUILD_C): $(MQJS_DIR)/mquickjs_build.c | $(GEN_DIR)
	@sed 's/#define ATOM_ALIGN 64/#define ATOM_ALIGN 128/' $< > $@

$(STDLIB_GEN): $(FW_DIR)/mqjs_stdlib_litex.c $(STDLIB_BUILD_C) | $(GEN_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -I$(MQJS_DIR) -o $@ \
	    $(FW_DIR)/mqjs_stdlib_litex.c \
	    $(STDLIB_BUILD_C)

$(STDLIB_H): $(STDLIB_GEN)
	$< $(STDLIB_GEN_FLAGS) > $@
$(ATOM_H): $(STDLIB_GEN)
	$< -a $(STDLIB_GEN_FLAGS) > $@

$(MQJS_PATCHED_STAMP): $(MQJS_DIR)/mquickjs.c $(MQJS_DIR)/mquickjs.h $(MQJS_PATCH) | $(GEN_DIR)
	cp $(MQJS_DIR)/mquickjs.c $(GEN_DIR)/mquickjs.c
	cp $(MQJS_DIR)/mquickjs.h $(GEN_DIR)/mquickjs.h
	patch -d $(GEN_DIR) -p0 < $(MQJS_PATCH)
	touch $@

$(GEN_DIR)/mquickjs.c $(GEN_DIR)/mquickjs.h: $(MQJS_PATCHED_STAMP)

ifneq ($(SCRIPT),)
# Always rebuild user_script.h when SCRIPT is supplied: the path is a
# make variable that isn't tracked as a dependency, so regenerating
# unconditionally is the only way to avoid running the previous
# script when the user passes a different SCRIPT= on the command line.
# .PHONY would fight the "pre-req for main.o" relationship; an empty
# FORCE target is the idiomatic workaround.
FORCE:

$(USER_SCRIPT_H): $(SCRIPT) FORCE | $(GEN_DIR)
	@echo " GEN     $@"
	@$(PYTHON) $(FW_DIR)/embed_script.py $< $@
endif

# Header dependencies.
main.o:      $(STDLIB_H) $(ATOM_H) $(USER_SCRIPT_H)
mqjs_port.o: $(STDLIB_H) $(ATOM_H)
mquickjs.o:  $(GEN_DIR)/mquickjs.c $(GEN_DIR)/mquickjs.h $(ATOM_H) \
             clean-mqjs-generated-headers $(BUILD_CONTEXT)

# --- Compile rules ------------------------------------------------------

.PHONY: clean-mqjs-generated-headers
clean-mqjs-generated-headers:
	$(RM) $(MQJS_DIR)/mquickjs_atom.h

mquickjs.o: CFLAGS += $(MQJS_EXTRA_CFLAGS)
dtoa.o    : CFLAGS += $(MQJS_EXTRA_CFLAGS)
libm.o    : CFLAGS += $(MQJS_EXTRA_CFLAGS)
cutils.o  : CFLAGS += $(MQJS_EXTRA_CFLAGS)

%.o: %.c
	$(compile)

mquickjs.o:
	$(compile)

%.o: %.S
	$(assemble)

clean:
	$(RM) *.o *.d *~ firmware.elf firmware.bin firmware.elf.map
	$(RM) -r $(GEN_DIR)

-include $(wildcard *.d)
