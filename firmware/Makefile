# Firmware build for mquickjs on LiteX.
#
# Invocation:
#   make BUILD_DIRECTORY=<litex_sim output dir>               # REPL mode
#   make BUILD_DIRECTORY=<...> SCRIPT=../examples/hello.js    # embed script
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
$(error BUILD_DIRECTORY is not set. Point it at a litex_sim output directory \
(e.g. make BUILD_DIRECTORY=/path/to/sim-build). Run sim/gen_soc.py to \
create one.)
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

# SCRIPT may point to a .js source *or* a pre-compiled .bin bytecode blob.
# When unset, the firmware is built in interactive REPL mode.
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
# neither libcompiler_rt nor picolibc ship (`__extendsfdf2`,
# `__truncdfsf2`, `__clzdi2`, ...). LiteX's CPUFLAGS uses the versioned
# `-march=rv32i2p0_m` spelling which gcc's multilib matcher rejects
# (only the un-versioned `rv32im` maps to a multilib directory). Resolve
# it manually using an un-versioned flavour so the driver returns the
# correct rv32im/ilp32 libgcc.a path. */
TARGET_LIBGCC := $(shell $(TARGET_PREFIX)gcc -march=rv32im -mabi=ilp32 \
                     -print-libgcc-file-name)

# Silence warnings that aren't relevant on an embedded 32-bit target
# (unused callback parameters, signed/unsigned comparisons inside large
# static tables, etc.). Keep -Wall elsewhere.
MQJS_EXTRA_CFLAGS := -Wno-unused-parameter -Wno-sign-compare \
                     -Wno-unused-function -Wno-unused-variable \
                     -Wno-unused-but-set-variable

# --- Host-side stdlib generator (runs on the build machine) ------------

HOST_CC     ?= gcc
HOST_CFLAGS ?= -Wall -O2 -g -D_GNU_SOURCE
STDLIB_GEN  := $(GEN_DIR)/mqjs_stdlib_litex
STDLIB_BUILD_C := $(GEN_DIR)/mquickjs_build_litex.c
STDLIB_H    := $(GEN_DIR)/mqjs_stdlib.h
ATOM_H      := $(GEN_DIR)/mquickjs_atom.h

# Force 32-bit layout so the generated tables match the JSW=4 target
# (RISC-V rv32). The generator itself is native.
STDLIB_GEN_FLAGS := -m32

# --- Top-level targets --------------------------------------------------

.PHONY: all clean help FORCE

all: firmware.bin

help:
	@echo "Usage:"
	@echo "  make BUILD_DIRECTORY=<litex_sim output dir>               # REPL firmware"
	@echo "  make BUILD_DIRECTORY=<...> SCRIPT=../examples/hello.js    # embed script"
	@echo "  make BUILD_DIRECTORY=<...> SCRIPT=hello.bin               # embed bytecode"
	@echo "  make clean                                                # no BUILD_DIRECTORY needed"
	@echo ""
	@echo "Variables:"
	@echo "  SCRIPT   path to .js source or .bin bytecode to embed (default: REPL)"
	@echo "  V=1      verbose compile lines"

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
	    echo "PACKAGES=$(PACKAGES)"; \
	    echo "LIBS=$(LIBS)"; \
	} | cmp -s - $@ || { \
	    { \
	        echo "BUILD_DIRECTORY=$(BUILD_DIR)"; \
	        echo "PACKAGES=$(PACKAGES)"; \
	        echo "LIBS=$(LIBS)"; \
	    } > $@; \
	}

main.o mqjs_port.o: $(GENERATED_SOC_HEADERS) $(BUILD_CONTEXT)

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
	@$(PYTHON) $(FW_DIR)/../tools/embed_script.py $< $@
endif

# Header dependencies.
main.o:      $(STDLIB_H) $(ATOM_H) $(USER_SCRIPT_H)
mqjs_port.o: $(STDLIB_H) $(ATOM_H)
mquickjs.o:  $(ATOM_H) clean-mqjs-generated-headers

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

%.o: %.S
	$(assemble)

clean:
	$(RM) *.o *.d *~ firmware.elf firmware.bin firmware.elf.map
	$(RM) -r $(GEN_DIR)

-include $(wildcard *.d)
