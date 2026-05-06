# Convenience entry points for mquickjs on LiteX.
#
# The real build logic lives in sim/run_sim.py, sim/gen_soc.py and
# firmware/Makefile. These targets keep the common demo flows short.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

SCRIPT ?= examples/hello.js
SIM_BUILD_DIR ?= build/sim
BOARD_TARGET ?= litex_boards.targets.digilent_arty
BOARD_BUILD_DIR ?= $(if $(ARTY_BUILD_DIR),$(ARTY_BUILD_DIR),/tmp/mquickjs_on_litex)
BOARD_BITSTREAM ?= $(BOARD_BUILD_DIR)/gateware/digilent_arty.bit
BOARD_SERIAL ?= $(if $(ARTY_SERIAL),$(ARTY_SERIAL),/dev/ttyUSB2)
BOARD_CABLE ?= $(if $(ARTY_CABLE),$(ARTY_CABLE),digilent)
BOARD_EXTRA ?= $(ARTY_EXTRA)
BOARD_SDCARD ?= $(if $(ARTY_SDCARD),$(ARTY_SDCARD),/media/$(USER)/LITEX)
HEAP_SIZE ?=
TIMEOUT ?= 120

FIRMWARE_ARGS = BUILD_DIRECTORY=$(abspath $(BOARD_BUILD_DIR))
ifneq ($(SCRIPT),)
FIRMWARE_ARGS += SCRIPT=$(abspath $(SCRIPT))
endif
ifneq ($(HEAP_SIZE),)
FIRMWARE_ARGS += HEAP_SIZE=$(HEAP_SIZE)
endif

.PHONY: help check-env sim-soc sim sim-repl firmware board-gateware board-load board-run board-demo board-sdcard-demo board-sdcard-prepare board-sdcard-clean-prepare board-sdcard-check arty-gateware arty-load arty-run arty-demo arty-sdcard-demo arty-sdcard-prepare arty-sdcard-clean-prepare arty-sdcard-check clean

help:
	@echo "mquickjs on LiteX demo targets"
	@echo ""
	@echo "Simulation:"
	@echo "  make check-env"
	@echo "  make sim SCRIPT=examples/hello.js"
	@echo "  make sim-repl"
	@echo ""
	@echo "Hardware:"
	@echo "  make board-gateware"
	@echo "  make firmware SCRIPT=examples/board_showcase.js"
	@echo "  make board-load"
	@echo "  make board-run BOARD_SERIAL=/dev/ttyUSB2"
	@echo "  make board-demo"
	@echo "  make board-sdcard-demo"
	@echo "  make board-sdcard-prepare BOARD_SDCARD=/media/$(USER)/LITEX"
	@echo "  make board-sdcard-clean-prepare BOARD_SDCARD=/media/$(USER)/LITEX"
	@echo "  make board-sdcard-check BOARD_SDCARD=/media/$(USER)/LITEX"
	@echo ""
	@echo "Useful variables:"
	@echo "  SCRIPT=$(SCRIPT)"
	@echo "  HEAP_SIZE=$(HEAP_SIZE)"
	@echo "  SIM_BUILD_DIR=$(SIM_BUILD_DIR)"
	@echo "  BOARD_TARGET=$(BOARD_TARGET)"
	@echo "  BOARD_BUILD_DIR=$(BOARD_BUILD_DIR)"
	@echo "  BOARD_BITSTREAM=$(BOARD_BITSTREAM)"
	@echo "  BOARD_SERIAL=$(BOARD_SERIAL)"
	@echo "  BOARD_EXTRA=$(BOARD_EXTRA)"
	@echo "  BOARD_SDCARD=$(BOARD_SDCARD)"

check-env:
	@python3 tools/check_env.py

sim-soc:
	@./sim/gen_soc.py --output-dir $(SIM_BUILD_DIR)

sim:
	@./sim/run_sim.py --output-dir $(SIM_BUILD_DIR) --script $(SCRIPT) --timeout $(TIMEOUT) $(if $(HEAP_SIZE),--heap-size $(HEAP_SIZE),)

sim-repl:
	@./sim/run_sim.py --output-dir $(SIM_BUILD_DIR) --keep-running

firmware:
	@$(MAKE) -C firmware $(FIRMWARE_ARGS)

board-gateware:
	@python3 -m $(BOARD_TARGET) \
		--build \
		--cpu-type=vexriscv \
		--libc-mode=full \
		--timer-uptime \
		$(BOARD_EXTRA) \
		--output-dir=$(BOARD_BUILD_DIR)

board-load:
	@openFPGALoader -c $(BOARD_CABLE) $(BOARD_BITSTREAM)

board-run:
	@litex_term $(BOARD_SERIAL) --kernel=firmware/firmware.bin

board-demo: SCRIPT=examples/board_showcase.js
board-demo: firmware board-load board-run

board-sdcard-demo: SCRIPT=examples/sdcard_loader.js
board-sdcard-demo: BOARD_EXTRA=--with-sdcard --with-ethernet
board-sdcard-demo: board-gateware firmware board-load board-run

board-sdcard-prepare: SCRIPT=examples/sdcard_loader.js
board-sdcard-prepare: firmware
	@tools/prepare_sdcard.py $(BOARD_SDCARD) --boot firmware/firmware.bin --main examples/sdcard/main.js

board-sdcard-clean-prepare: SCRIPT=examples/sdcard_loader.js
board-sdcard-clean-prepare: firmware
	@tools/prepare_sdcard.py $(BOARD_SDCARD) --clean --boot firmware/firmware.bin --main examples/sdcard/main.js

board-sdcard-check:
	@tools/prepare_sdcard.py $(BOARD_SDCARD) --check-only

arty-gateware: board-gateware
arty-load: board-load
arty-run: board-run
arty-demo: board-demo
arty-sdcard-demo: board-sdcard-demo
arty-sdcard-prepare: board-sdcard-prepare
arty-sdcard-clean-prepare: board-sdcard-clean-prepare
arty-sdcard-check: board-sdcard-check

clean:
	@$(MAKE) -C firmware clean
