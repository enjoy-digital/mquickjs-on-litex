# Convenience entry points for litex_mquickjs.
#
# The real build logic lives in sim/run_sim.py, sim/gen_soc.py and
# firmware/Makefile. These targets keep the common demo flows short.

SCRIPT ?= examples/hello.js
SIM_BUILD_DIR ?= build/sim
ARTY_BUILD_DIR ?= /tmp/arty_mqjs
ARTY_SERIAL ?= /dev/ttyUSB2
ARTY_CABLE ?= digilent
HEAP_SIZE ?=
TIMEOUT ?= 120

FIRMWARE_ARGS = BUILD_DIRECTORY=$(abspath $(ARTY_BUILD_DIR))
ifneq ($(SCRIPT),)
FIRMWARE_ARGS += SCRIPT=$(abspath $(SCRIPT))
endif
ifneq ($(HEAP_SIZE),)
FIRMWARE_ARGS += HEAP_SIZE=$(HEAP_SIZE)
endif

.PHONY: help check-env sim-soc sim sim-repl firmware arty-gateware arty-load arty-run arty-demo clean

help:
	@echo "litex_mquickjs demo targets"
	@echo ""
	@echo "Simulation:"
	@echo "  make check-env"
	@echo "  make sim SCRIPT=examples/hello.js"
	@echo "  make sim-repl"
	@echo ""
	@echo "Digilent Arty A7:"
	@echo "  make arty-gateware"
	@echo "  make firmware SCRIPT=examples/leds.js"
	@echo "  make arty-load"
	@echo "  make arty-run ARTY_SERIAL=/dev/ttyUSB2"
	@echo "  make arty-demo"
	@echo ""
	@echo "Useful variables:"
	@echo "  SCRIPT=$(SCRIPT)"
	@echo "  HEAP_SIZE=$(HEAP_SIZE)"
	@echo "  SIM_BUILD_DIR=$(SIM_BUILD_DIR)"
	@echo "  ARTY_BUILD_DIR=$(ARTY_BUILD_DIR)"
	@echo "  ARTY_SERIAL=$(ARTY_SERIAL)"

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

arty-gateware:
	@python3 -m litex_boards.targets.digilent_arty \
		--build \
		--cpu-type=vexriscv \
		--libc-mode=full \
		--timer-uptime \
		--output-dir=$(ARTY_BUILD_DIR)

arty-load:
	@openFPGALoader -c $(ARTY_CABLE) $(ARTY_BUILD_DIR)/gateware/digilent_arty.bit

arty-run:
	@litex_term $(ARTY_SERIAL) --kernel=firmware/firmware.bin

arty-demo: SCRIPT=examples/leds.js
arty-demo: firmware arty-load arty-run

clean:
	@$(MAKE) -C firmware clean
