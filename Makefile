SHELL := /bin/bash

# Pinned MAME — fetched from upstream, our single driver overlaid onto it.
MAME_REPO   ?= https://github.com/mamedev/mame.git
MAME_COMMIT ?= 58fca9a8a20f75ac2010980e1a2ec0465c595583
MAME_DIR    ?= mame

PYTHON ?= python3
VENV   := .venv
DRIVER := driver/nokia_3310.cpp
DRIVER_COMPONENTS := driver/nokia_ccont.cpp driver/nokia_ccont.h \
	driver/nokia_sim_card.cpp driver/nokia_sim_card.h \
	driver/nokia_service_transport.cpp driver/nokia_service_transport.h
PHONE ?= noki3210
BIOS ?=

# Bring-your-own firmware (see roms/README.md). Git-ignored.
ROM  ?= roms/3210f600a.fls
SWAP ?= roms/3210f600a_swap16.bin

RUN_DIR ?= run
SECONDS ?= 20
RUN_ENV ?=
RUN_NVRAM_DIR ?= $(abspath $(RUN_DIR))/nvram
PRESERVE_NVRAM ?= 0
CENSUS_LOG ?=
CENSUS_MANIFESTS ?= tools/run_manifests/contact-service.json tools/run_manifests/deep-gsm.json

# Stable, git-ignored PNG of the latest LCD frame — promoted after every run so an
# external `watch chafa progress_latest_frame.png` updates live.
FRAME_PNG ?= progress_latest_frame.png

# Regression oracle: sha256 prefix of the promoted LCD frame from `make run`.
# A blank/un-provisioned 3210 deterministically reaches CONTACT SERVICE here.
ORACLE_FRAME_SHA ?= d8a9a7a58e587be8
ORACLE_STRUCT ?= oracles/noki3210-default.struct
ORACLE_DEEP_FRAME_SHA ?= 90eb19a5478483ca
ORACLE_DEEP_STRUCT ?= oracles/noki3210-deep.struct
ORACLE_FRONTIER_FRAME_SHA ?= 6471d1a5803619c2
ORACLE_FRONTIER_STRUCT ?= oracles/noki3210-frontier.struct

DEEP_ENV := \
	NOKI3210_MODEL_DSP_SERVICE=1 \
	NOKI3210_MODEL_CCONT_PRESENT=1 \
	NOKI3210_MODEL_SVC_RESPONDER=1 \
	NOKI3210_MODEL_SVC_CHANNEL_DRAIN=1

# Current forcing-free research frontier.  The request-driven contact peer
# subsumes DSP D0 discovery and TX-ring consumption, while the SIM model stays
# behind the ordinary SIMI/FIQ6 boundary.  Keep DEEP_ENV above only for the
# historical Insert SIM oracle.
FRONTIER_ENV := \
	NOKI3210_MODEL_DSP_SERVICE=1 \
	NOKI3210_MODEL_CCONT_PRESENT=1 \
	NOKI3210_MODEL_DSP_CONTACT_PEER=1 \
	NOKI3210_MODEL_SIM_DEVICE=1

# Canonical "boot-progress" run profile — the minimal knob set that reproduces the
# CONTACT SERVICE oracle frame: genuine hardware config (display/clocks/power/adc/
# battery/eeprom), the CCONT watchdog guard, and CCONT_EVENT15_DELAY (needed by the
# deeper MODEL_* boot). Every NOKI3210_* var the driver reads is an env knob; override
# any on the command line — e.g. add MODEL_DSP_SERVICE/MODEL_CCONT_PRESENT/
# MODEL_SVC_RESPONDER to clear CONTACT SERVICE, or any TRACE_* for diagnostics.
# Trimmed 2026-07 (leave-one-out audit): removed 6 baked-in TRACE_* and the forcing
# shims proven inert against both the oracle and the deep boot. See docs/driver_vision.md.
BOOT_ENV := \
	NOKI3210_DISPLAY_TYPE=4 \
	NOKI3210_POWER_IRQ_MS=120 \
	NOKI3210_POWER_IRQ_ASSERT=1 \
	NOKI3210_ADC_PROFILE=sane \
	NOKI3210_BATTERY_PROFILE=charged \
	NOKI3210_TIMER0_HZ=20000000 \
	NOKI3210_TIMER1_HZ=1057 \
	NOKI3210_TIMER0_CATCHUP=1 \
	NOKI3210_CCONT_EVENT15_DELAY=1 \
	NOKI3210_DISABLE_CCONT_WATCHDOG=1 \
	NOKI3210_LUA_QUIET=1 \
	NOKI3210_INPUT_EXERCISER_PRESS=0

MAME_ARGS := $(PHONE) -rompath roms -log -video none -sound none \
	-keyboardprovider none -mouseprovider none -lightgunprovider none \
	-joystickprovider none -midiprovider none -skip_gameinfo -nothrottle \
	-autoboot_script ../mame_noki3210_input_exerciser.lua $(if $(BIOS),-bios $(BIOS))

.PHONY: help venv download-mame overlay eeprom-profile normalize-3330 roms build swap16 census controller-census census-docs evidence-check prepare-run-nvram run run-deep run-frontier smoke smoke-3330e audit-roms frame watch verify verify-deep verify-frontier verify-structure verify-structure-subset run-manifest-default run-manifest-deep-gsm run-manifest-contact run-manifest-3330 clean

help:
	@echo "make venv           create .venv from requirements.txt (for tools/)"
	@echo "make build          clone MAME at the pin, overlay $(DRIVER), build"
	@echo "make swap16         derive $(SWAP) from $(ROM) (16-bit byteswap, for the static tools)"
	@echo "make census         build the 3210 v6.00 message-topology JSON/report"
	@echo "make controller-census exhaustively verify the 3210 controller dispatcher"
	@echo "make census-docs    refresh the committed report; refuses missing scoped runtime"
	@echo "make evidence-check validate reviewed evidence ledgers and runtime manifests"
	@echo "make run-manifest-* reproduce named default/deep-gsm/contact/3330 evidence runs"
	@echo "make eeprom-profile build the synthetic 3210 24C128 image used by the oracle"
	@echo "make normalize-3330 extract the local Wintesla MCU/PPM/PMM record streams"
	@echo "make run            boot to the CONTACT SERVICE oracle frame into RUN_DIR=$(RUN_DIR)"
	@echo "make verify         run, then check the promoted frame SHA == $(ORACLE_FRAME_SHA)"
	@echo "make verify-deep    reproduce the Insert SIM frame and deep structural oracle"
	@echo "make run-frontier   run the current coherent contact/SIM research profile"
	@echo "make verify-frontier reproduce the current coherent frontier oracles"
	@echo "PRESERVE_NVRAM=1    retain EEPROM writes between runs (default reseeds the fixture)"
	@echo "make verify-structure  compare deterministic boot milestones with $(ORACLE_STRUCT)"
	@echo "make smoke PHONE=noki3330  bounded non-oracle boot for another local ROM set"
	@echo "make smoke-3330e     normalize and boot the local v4.50 PPM E service files"
	@echo "make audit-roms PHONE=noki3330  report missing/mismatched files for a local set"
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
	@for src in $(DRIVER_COMPONENTS); do install -D "$$src" "$(MAME_DIR)/src/mame/nokia/$$(basename "$$src")"; done

eeprom-profile:
	@test -f $(ROM) || { echo "Missing $(ROM) — bring your own dump (see roms/README.md)"; exit 1; }
	$(PYTHON) tools/make_eeprom_profile.py --flash $(ROM) --output "roms/noki3210/3210 selftest eeprom.bin"

normalize-3330:
	$(PYTHON) tools/extract_dct3_wintesla.py \
		--mcu roms/3330-nhm6-v450/NHM6NX04.500 \
		--ppm roms/3330-nhm6-v450/NHM6NX04.50E \
		--pmm "roms/3330-nhm6-v450/3330 virgin eeprom.pmm" \
		--flash-output roms/noki3330/3330f450e.fls \
		--eeprom-output "roms/noki3330/3330 virgin eeprom 005f0000.fls" \
		--expect-flash-sha1 7e88caa4963c57ebbd4d919023e38103ff8b528a \
		--expect-eeprom-sha1 68481effb39d90a1639e8f261009c66e97d3e668
	cp roms/noki3210/boot_rom roms/noki3210/dsp_prom roms/noki3210/dsp_drom roms/noki3210/dsp_pdrom roms/noki3330/

roms: $(if $(filter noki3210,$(PHONE)),eeprom-profile)
	@for src in roms/noki*/; do \
		[ -d "$$src" ] || continue; \
		dst="$(MAME_DIR)/roms/$$(basename "$$src")"; \
		mkdir -p "$$dst"; \
		cp -a "$$src". "$$dst"/; \
	done

build: overlay roms
	$(MAKE) -C $(MAME_DIR) REGENIE=1 SOURCES=src/mame/nokia/nokia_3310.cpp,src/mame/nokia/nokia_ccont.cpp,src/mame/nokia/nokia_sim_card.cpp,src/mame/nokia/nokia_service_transport.cpp USE_QTDEBUG=0 -j$$(nproc)

swap16:
	@test -f $(ROM) || { echo "Missing $(ROM) — see roms/README.md"; exit 1; }
	@$(PYTHON) -c "d=open('$(ROM)','rb').read(); b=bytearray(d); b[0::2],b[1::2]=d[1::2],d[0::2]; open('$(SWAP)','wb').write(bytes(b)); print('wrote $(SWAP) (%d bytes)'%len(b))"

census:
	@mkdir -p run_census
	$(PYTHON) tools/validate_evidence.py
	$(VENV)/bin/python tools/message_census.py --check \
		$(foreach manifest,$(CENSUS_MANIFESTS),--runtime-manifest $(manifest)) \
		$(if $(CENSUS_LOG),--runtime-log $(CENSUS_LOG)) \
		--json run_census/noki3210_v600.json \
		--report run_census/noki3210_v600.md
	@echo "census: run_census/noki3210_v600.json and run_census/noki3210_v600.md"

controller-census:
	$(VENV)/bin/python tools/controller_dispatch_census.py --check

census-docs:
	@mkdir -p run_census
	$(PYTHON) tools/validate_evidence.py
	$(VENV)/bin/python tools/controller_dispatch_census.py --check
	$(VENV)/bin/python tools/message_census.py --check \
		$(foreach manifest,$(CENSUS_MANIFESTS),--runtime-manifest $(manifest)) \
		--require-runtime-subsystem contact_service \
		--require-runtime-subsystem generic_service \
		--json run_census/noki3210_v600.json \
		--report docs/message_topology_census.md
	@echo "census-docs: docs/message_topology_census.md"

evidence-check:
	$(PYTHON) tools/validate_evidence.py

run-manifest-default:
	@$(MAKE) --no-print-directory verify RUN_DIR=run_manifest_default SECONDS=4
	cp $(MAME_DIR)/error.log run_manifest_default/error.log

run-manifest-deep-gsm: build
	@mkdir -p run_manifest_deep_gsm
	@$(MAKE) --no-print-directory prepare-run-nvram PHONE=noki3210 RUN_DIR=run_manifest_deep_gsm
	cd $(MAME_DIR) && env $(BOOT_ENV) $(FRONTIER_ENV) \
		NOKI3210_TRACE_TASKS=1 NOKI3210_TRACE_SIM_RX=1 NOKI3210_TRACE_GSM_SERVICE=1 NOKI3210_TRACE_GSM_LOWER=1 \
		NOKI3210_TRACE_DSP_BOUNDARY=1 \
		NOKI3210_SNAPSHOT_DIR=$(abspath run_manifest_deep_gsm) \
		NOKI3210_BOOT_SUMMARY=$(abspath run_manifest_deep_gsm)/boot_summary.txt \
		./mame $(MAME_ARGS) -nvram_directory $(abspath run_manifest_deep_gsm)/nvram -seconds_to_run 8
	cp $(MAME_DIR)/error.log run_manifest_deep_gsm/error.log

run-manifest-contact: build
	@mkdir -p run_manifest_contact_default run_manifest_contact_deep
	@$(MAKE) --no-print-directory prepare-run-nvram PHONE=noki3210 RUN_DIR=run_manifest_contact_default
	cd $(MAME_DIR) && env $(BOOT_ENV) NOKI3210_TRACE_CSCMD=1 \
		NOKI3210_SNAPSHOT_DIR=$(abspath run_manifest_contact_default) \
		./mame $(MAME_ARGS) -nvram_directory $(abspath run_manifest_contact_default)/nvram -seconds_to_run 1
	cp $(MAME_DIR)/error.log run_manifest_contact_default/error.log
	@$(MAKE) --no-print-directory prepare-run-nvram PHONE=noki3210 RUN_DIR=run_manifest_contact_deep
	cd $(MAME_DIR) && env $(BOOT_ENV) $(FRONTIER_ENV) NOKI3210_TRACE_CSCMD=1 \
		NOKI3210_SNAPSHOT_DIR=$(abspath run_manifest_contact_deep) \
		./mame $(MAME_ARGS) -nvram_directory $(abspath run_manifest_contact_deep)/nvram -seconds_to_run 6
	cp $(MAME_DIR)/error.log run_manifest_contact_deep/error.log

run-manifest-3330:
	@$(MAKE) --no-print-directory smoke-3330e RUN_DIR=run_manifest_3330 SECONDS=3
	cp $(MAME_DIR)/error.log run_manifest_3330/error.log

prepare-run-nvram:
	@if [ "$(PHONE)" = "noki3210" ]; then \
		mkdir -p "$(RUN_NVRAM_DIR)/$(PHONE)"; \
		if [ "$(PRESERVE_NVRAM)" != "1" ]; then \
			cp "$(MAME_DIR)/roms/noki3210/3210 selftest eeprom.bin" \
				"$(RUN_NVRAM_DIR)/$(PHONE)/eeprom"; \
		fi; \
	fi

run: build prepare-run-nvram
	@mkdir -p $(RUN_DIR)
	cd $(MAME_DIR) && env $(BOOT_ENV) $(RUN_ENV) NOKI3210_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKI3210_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		./mame $(MAME_ARGS) -nvram_directory $(RUN_NVRAM_DIR) -seconds_to_run $(SECONDS)
	@$(MAKE) --no-print-directory frame RUN_DIR=$(RUN_DIR)

run-deep:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS) RUN_ENV='$(DEEP_ENV)'

run-frontier:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS) RUN_ENV='$(FRONTIER_ENV)'

smoke: build
	@mkdir -p $(RUN_DIR)
	cd $(MAME_DIR) && env $(BOOT_ENV) NOKI3210_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		./mame $(MAME_ARGS) -seconds_to_run $(SECONDS)

smoke-3330e: normalize-3330
	@$(MAKE) --no-print-directory smoke PHONE=noki3330 BIOS=450e RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS)

audit-roms: build
	cd $(MAME_DIR) && ./mame -rompath roms -verifyroms $(PHONE)

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

verify: PHONE=noki3210
verify: run
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' ! -name '*_o000.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	got=$$(sha256sum "$$frame" | cut -c1-16); \
	echo "frame  : $$frame"; echo "sha256 : $$got"; echo "oracle : $(ORACLE_FRAME_SHA)"; \
	if [ "$$got" = "$(ORACLE_FRAME_SHA)" ]; then echo "OK — oracle reproduced"; \
	else echo "MISMATCH — boot diverged from the recorded CONTACT SERVICE state"; exit 1; fi
	@$(MAKE) --no-print-directory verify-structure RUN_DIR=$(RUN_DIR)

verify-deep: PHONE=noki3210
verify-deep: run-deep
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' ! -name '*_o000.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	got=$$(sha256sum "$$frame" | cut -c1-16); \
	echo "frame  : $$frame"; echo "sha256 : $$got"; echo "oracle : $(ORACLE_DEEP_FRAME_SHA)"; \
	if [ "$$got" = "$(ORACLE_DEEP_FRAME_SHA)" ]; then echo "OK — Insert SIM oracle reproduced"; \
	else echo "MISMATCH — boot diverged from the recorded Insert SIM state"; exit 1; fi
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_DEEP_STRUCT)

verify-frontier: PHONE=noki3210
verify-frontier: run-frontier
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' ! -name '*_o000.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	got=$$(sha256sum "$$frame" | cut -c1-16); \
	echo "frame  : $$frame"; echo "sha256 : $$got"; echo "oracle : $(ORACLE_FRONTIER_FRAME_SHA)"; \
	if [ "$$got" = "$(ORACLE_FRONTIER_FRAME_SHA)" ]; then echo "OK — coherent frontier oracle reproduced"; \
	else echo "MISMATCH — boot diverged from the coherent frontier state"; exit 1; fi
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_FRONTIER_STRUCT)

verify-structure-subset:
	@test -f $(RUN_DIR)/boot_summary.txt || { echo "missing $(RUN_DIR)/boot_summary.txt; run make run first"; exit 1; }
	@test -f $(ORACLE_STRUCT) || { echo "missing structural oracle $(ORACLE_STRUCT)"; exit 1; }
	@while IFS= read -r expected; do \
		grep -Fqx -- "$$expected" $(RUN_DIR)/boot_summary.txt || { echo "missing structural predicate: $$expected"; exit 1; }; \
	done < $(ORACLE_STRUCT)
	@echo "OK — semantic structural predicates reproduced"

verify-structure:
	@test -f $(RUN_DIR)/boot_summary.txt || { echo "missing $(RUN_DIR)/boot_summary.txt; run make run first"; exit 1; }
	@test -f $(ORACLE_STRUCT) || { echo "missing structural oracle $(ORACLE_STRUCT)"; exit 1; }
	@diff -u $(ORACLE_STRUCT) $(RUN_DIR)/boot_summary.txt
	@echo "OK — structural boot oracle reproduced"

clean:
	rm -rf $(RUN_DIR) run run_* $(MAME_DIR)/obj progress_latest_frame.*
