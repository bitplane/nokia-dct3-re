SHELL := /bin/bash

# Pinned MAME — fetched from upstream, our single driver overlaid onto it.
MAME_REPO   ?= https://github.com/mamedev/mame.git
MAME_COMMIT ?= 58fca9a8a20f75ac2010980e1a2ec0465c595583
MAME_DIR    ?= mame

PYTHON ?= python3
VENV   := .venv
DRIVER := driver/nokia_3310.cpp

# Bring-your-own firmware (see roms/README.md). Git-ignored.
ROM  ?= roms/3210f600a.fls
SWAP ?= roms/3210f600a_swap16.bin

RUN_DIR ?= run
SECONDS ?= 20

# Stable, git-ignored PNG of the latest LCD frame — promoted after every run so an
# external `watch chafa progress_latest_frame.png` updates live.
FRAME_PNG ?= progress_latest_frame.png

# Regression oracle: sha256 prefix of the promoted LCD frame from `make run`.
# A blank/un-provisioned 3210 deterministically reaches CONTACT SERVICE here.
ORACLE_FRAME_SHA ?= d8a9a7a58e587be8

# Canonical "boot-progress" run profile — the knob values that drive the firmware
# to the CONTACT SERVICE milestone the oracle frame captures. Every NOKI3210_* var
# the driver reads is an env knob; override any on the command line. The forces here
# are inert against the oracle (audited — docs/removed_forcing_knobs.md); the traces
# are read-only. See docs/service_bootstrap.md for what each group does.
BOOT_ENV := \
	NOKI3210_DISABLE_CCONT_WATCHDOG=1 \
	NOKI3210_DISPLAY_TYPE=4 \
	NOKI3210_POWER_IRQ_MS=120 \
	NOKI3210_POWER_IRQ_ASSERT=1 \
	NOKI3210_ADC_PROFILE=sane \
	NOKI3210_BATTERY_PROFILE=charged \
	NOKI3210_TIMER0_HZ=20000000 \
	NOKI3210_TIMER1_HZ=1057 \
	NOKI3210_TIMER0_CATCHUP=1 \
	NOKI3210_CCONT_EVENT15_DELAY=1 \
	NOKI3210_EEPROM_PROFILE=selftest \
	NOKI3210_SUPPRESS_SIM_CONTEXT_EVENTS=1 \
	NOKI3210_CONTACT_STATUS65_FLAGS=0x48 \
	NOKI3210_CONTACT_CHANNEL_MAP_FLAGS=0x100 \
	NOKI3210_CONTACT_D9_TIMEOUT_DELAY=0xffff \
	NOKI3210_STARTUP_EVENT15_DELAY_CLAMP=65535 \
	NOKI3210_TRACE_STARTUP_BRANCHES=1 \
	NOKI3210_TRACE_SCHED_EVENT15=1 \
	NOKI3210_TRACE_CONTACT_SERVICE=1 \
	NOKI3210_TRACE_CONTACT_RESPONSE=1 \
	NOKI3210_TRACE_SERVICE_TRANSPORT=1 \
	NOKI3210_TRACE_TASK14_READY=1 \
	NOKI3210_LUA_QUIET=1 \
	NOKI3210_INPUT_EXERCISER_PRESS=0

MAME_ARGS := noki3210 -rompath roms -log -video none -sound none \
	-keyboardprovider none -mouseprovider none -lightgunprovider none \
	-joystickprovider none -midiprovider none -skip_gameinfo -nothrottle \
	-autoboot_script ../mame_noki3210_input_exerciser.lua

.PHONY: help venv download-mame overlay roms build swap16 run frame watch verify clean

help:
	@echo "make venv           create .venv from requirements.txt (for tools/)"
	@echo "make build          clone MAME at the pin, overlay $(DRIVER), build"
	@echo "make swap16         derive $(SWAP) from $(ROM) (16-bit byteswap, for the static tools)"
	@echo "make run            boot to the CONTACT SERVICE oracle frame into RUN_DIR=$(RUN_DIR)"
	@echo "make verify         run, then check the promoted frame SHA == $(ORACLE_FRAME_SHA)"
	@echo "make watch          live chafa preview of $(FRAME_PNG) (updated each run)"
	@echo "make clean          remove build/run state (keeps the MAME clone)"
	@echo "Override any knob on the command line, e.g.  make run NOKI3210_TRACE_PM=1"

venv:
	$(PYTHON) -m venv $(VENV)
	$(VENV)/bin/pip install -r requirements.txt

download-mame:
	@if [ ! -d $(MAME_DIR)/.git ]; then \
		git clone $(MAME_REPO) $(MAME_DIR) && git -C $(MAME_DIR) checkout $(MAME_COMMIT); \
	fi

# Overlay our single driver onto the upstream tree (MAME is not vendored).
overlay: download-mame
	install -D $(DRIVER) $(MAME_DIR)/src/mame/nokia/nokia_3310.cpp

roms:
	@test -f $(ROM) || { echo "Missing $(ROM) — bring your own dump (see roms/README.md)"; exit 1; }
	mkdir -p $(MAME_DIR)/roms/noki3210
	cp -a roms/noki3210/. $(MAME_DIR)/roms/noki3210/

build: overlay roms
	$(MAKE) -C $(MAME_DIR) SOURCES=src/mame/nokia/nokia_3310.cpp USE_QTDEBUG=0 -j$$(nproc)

swap16:
	@test -f $(ROM) || { echo "Missing $(ROM) — see roms/README.md"; exit 1; }
	@$(PYTHON) -c "d=open('$(ROM)','rb').read(); b=bytearray(d); b[0::2],b[1::2]=d[1::2],d[0::2]; open('$(SWAP)','wb').write(bytes(b)); print('wrote $(SWAP) (%d bytes)'%len(b))"

run: build
	@mkdir -p $(RUN_DIR)
	cd $(MAME_DIR) && env $(BOOT_ENV) NOKI3210_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		./mame $(MAME_ARGS) -seconds_to_run $(SECONDS)
	@$(MAKE) --no-print-directory frame RUN_DIR=$(RUN_DIR)

# Promote the latest non-blank LCD frame in RUN_DIR to FRAME_PNG (for chafa/preview).
frame:
	@f=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' ! -name '*_o000.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	if [ -z "$$f" ]; then echo "frame: no LCD frame in $(RUN_DIR) yet"; else \
		( magick "$$f" $(FRAME_PNG) 2>/dev/null || convert "$$f" $(FRAME_PNG) 2>/dev/null || pnmtopng "$$f" > $(FRAME_PNG) ) \
		&& echo "frame: $(FRAME_PNG) <- $$f"; fi

# Live preview in this terminal (Ctrl-C to stop). External equivalent:
#   watch -n0.5 chafa --size=84x48 progress_latest_frame.png
watch:
	@command -v chafa >/dev/null || { echo "chafa not installed"; exit 1; }
	@while :; do clear; chafa --size=84x48 $(FRAME_PNG) 2>/dev/null || echo "no $(FRAME_PNG) yet"; sleep 0.5; done

verify: run
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' ! -name '*_o000.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	got=$$(sha256sum "$$frame" | cut -c1-16); \
	echo "frame  : $$frame"; echo "sha256 : $$got"; echo "oracle : $(ORACLE_FRAME_SHA)"; \
	if [ "$$got" = "$(ORACLE_FRAME_SHA)" ]; then echo "OK — oracle reproduced"; \
	else echo "MISMATCH — boot diverged from the recorded CONTACT SERVICE state"; exit 1; fi

clean:
	rm -rf $(RUN_DIR) run run_* $(MAME_DIR)/obj progress_latest_frame.*
