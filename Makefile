# Convenience entry points for mquickjs on LiteX.
#
# The real build logic lives in sim/run_sim.py and firmware/Makefile.
# These targets keep the common demo flows short.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

SCRIPT ?= examples/hello.js
SIM_BUILD_DIR ?= build/sim
BOARD_TARGET ?=
BOARD_BUILD_DIR ?= build/board
BOARD_SERIAL ?=
BOARD_EXTRA ?=
BOARD_SDCARD ?=
HEAP_SIZE ?=
MEMORY_DUMP ?=
TIMEOUT ?= 120

FIRMWARE_ARGS = BUILD_DIRECTORY=$(abspath $(BOARD_BUILD_DIR))
ifneq ($(SCRIPT),)
FIRMWARE_ARGS += SCRIPT=$(abspath $(SCRIPT))
endif
ifneq ($(HEAP_SIZE),)
FIRMWARE_ARGS += HEAP_SIZE=$(HEAP_SIZE)
endif
ifneq ($(MEMORY_DUMP),)
FIRMWARE_ARGS += MEMORY_DUMP=$(MEMORY_DUMP)
endif

.PHONY: help sim sim-repl firmware board-build board-load board-run board-sdcard-prepare clean

define require_var
	@test -n "$($(1))" || (echo "Missing $(1). Pass $(1)=..."; exit 1)
endef

help:
	@echo "mquickjs on LiteX demo targets"
	@echo ""
	@echo "Simulation:"
	@echo "  make sim SCRIPT=examples/hello.js"
	@echo "  make sim-repl"
	@echo ""
	@echo "Hardware:"
	@echo "  make board-build BOARD_TARGET=litex_boards.targets.<board>"
	@echo "  make firmware SCRIPT=examples/demo.js"
	@echo "  make board-load BOARD_TARGET=litex_boards.targets.<board>"
	@echo "  make board-run BOARD_SERIAL=/dev/ttyUSBn"
	@echo "  make board-sdcard-prepare BOARD_SDCARD=<mounted-fat-root>"
	@echo ""
	@echo "Useful variables:"
	@echo "  SCRIPT=$(SCRIPT)"
	@echo "  HEAP_SIZE=$(HEAP_SIZE)"
	@echo "  MEMORY_DUMP=$(MEMORY_DUMP)"
	@echo "  SIM_BUILD_DIR=$(SIM_BUILD_DIR)"
	@echo "  BOARD_TARGET=$(BOARD_TARGET)"
	@echo "  BOARD_BUILD_DIR=$(BOARD_BUILD_DIR)"
	@echo "  BOARD_SERIAL=$(BOARD_SERIAL)"
	@echo "  BOARD_EXTRA=$(BOARD_EXTRA)"
	@echo "  BOARD_SDCARD=$(BOARD_SDCARD)"

sim:
	@./sim/run_sim.py --output-dir $(SIM_BUILD_DIR) --script $(SCRIPT) --timeout $(TIMEOUT) $(if $(HEAP_SIZE),--heap-size $(HEAP_SIZE),) $(if $(MEMORY_DUMP),--memory-dump,)

sim-repl:
	@./sim/run_sim.py --output-dir $(SIM_BUILD_DIR) --keep-running

firmware:
	@$(MAKE) -C firmware $(FIRMWARE_ARGS)

board-build:
	$(call require_var,BOARD_TARGET)
	@python3 -m $(BOARD_TARGET) \
		--build \
		--cpu-type=vexriscv \
		--libc-mode=full \
		--timer-uptime \
		$(BOARD_EXTRA) \
		--output-dir=$(BOARD_BUILD_DIR)

board-load:
	$(call require_var,BOARD_TARGET)
	@python3 -m $(BOARD_TARGET) \
		--load \
		--output-dir=$(BOARD_BUILD_DIR) \
		$(BOARD_EXTRA)

board-run:
	$(call require_var,BOARD_SERIAL)
	@litex_term $(BOARD_SERIAL) --kernel=firmware/firmware.bin

board-sdcard-prepare: SCRIPT=examples/sdcard_loader.js
board-sdcard-prepare: firmware
	$(call require_var,BOARD_SDCARD)
	@tools/prepare_sdcard.py $(BOARD_SDCARD) --clean --boot firmware/firmware.bin --main examples/sdcard/main.js

clean:
	@$(MAKE) -C firmware clean
