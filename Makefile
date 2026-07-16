SHELL := /bin/bash

# Pinned MAME — fetched from upstream, with the local Nokia driver source set overlaid.
MAME_REPO   ?= https://github.com/mamedev/mame.git
MAME_COMMIT ?= 58fca9a8a20f75ac2010980e1a2ec0465c595583
MAME_DIR    ?= mame

PYTHON ?= python3
VENV   := .venv
DRIVER := driver/nokia_3310.cpp
DRIVER_COMPONENTS := driver/nokia_ccont.cpp driver/nokia_ccont.h \
	driver/nokia_dsp_hle.cpp driver/nokia_dsp_hle.h \
	driver/nokia_dspif.cpp driver/nokia_dspif.h \
	driver/nokia_external_service.cpp driver/nokia_external_service.h \
	driver/nokia_gensio.cpp driver/nokia_gensio.h \
	driver/nokia_mad2.cpp driver/nokia_mad2.h \
	driver/nokia_mbus.cpp driver/nokia_mbus.h \
	driver/nokia_simi.cpp driver/nokia_simi.h \
	driver/nokia_sim_card.cpp driver/nokia_sim_card.h \
	driver/nokia_3310_trace.inc
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
PROVISIONED_IMEI_PREFIX ?=
EEPROM_BASENAME ?= $(if $(filter 501,$(BIOS)),3210 v501 eeprom.bin,3210 v600 eeprom.bin)
CENSUS_LOG ?=
CENSUS_MANIFESTS ?= tools/run_manifests/external-service.json tools/run_manifests/deep-gsm.json
FRONTIER_EVENT_INVENTORIES := \
	--inventory-status 0x0732 --inventory-status 0x03ab \
	--inventory-status 0x12b4 --inventory-status 0x32b4 --inventory-status 0x72b4 \
	--inventory-status 0x0bcc --inventory-status 0x13f8 --inventory-status 0x0348 \
	--inventory-status 0x012b --inventory-status 0x212b --inventory-status 0x612b

# Stable, git-ignored PNG of the latest LCD frame — promoted after every run so an
# external `watch chafa progress_latest_frame.png` updates live.
FRAME_PNG ?= progress_latest_frame.png

# Regression oracle: sha256 prefix of the promoted LCD frame from `make run`.
# A blank/un-provisioned 3210 deterministically reaches CONTACT SERVICE here.
ORACLE_FRAME_SHA ?= d8a9a7a58e587be8
ORACLE_MMI_MENU_STABLE_SHA ?= 305c12459700027431ef9c04132bcceb7e2157ef87debf2cdf1ae438b8bd8d3f
ORACLE_STRUCT ?= oracles/noki3210-default.struct
ORACLE_FRONTIER_STRUCT ?= oracles/noki3210-frontier.struct
ORACLE_V501_STRUCT ?= oracles/noki3210-v501-smoke.struct

# The validated 3210 device composition and calibrated boot values are product
# defaults in the machine configuration. Keep this alias while named research
# targets are normalized; it intentionally contributes no state-changing knobs.
FRONTIER_ENV :=

BOOT_ENV := NOKI3210_LUA_QUIET=1

# Explicit negative profile retained for the historical CONTACT SERVICE oracle.
# This proves that the peer devices, rather than firmware-state intervention,
# account for the difference between the failure frame and the working boot.
CONTACT_SERVICE_ENV := \
	NOKI3210_MODEL_DSP_SERVICE=0 \
	NOKI3210_MODEL_CCONT_PRESENT=0 \
	NOKI3210_MODEL_EXTERNAL_SERVICE_PEER=0 \
	NOKI3210_MODEL_SIM_DEVICE=0

MAME_ARGS := $(PHONE) -rompath roms -log -video none -sound none \
	-keyboardprovider none -mouseprovider none -lightgunprovider none \
	-joystickprovider none -midiprovider none -skip_gameinfo -nothrottle \
	-autoboot_script ../mame_noki3210_input_exerciser.lua $(if $(BIOS),-bios $(BIOS))
INTERACTIVE_MAME_ARGS := $(PHONE) -rompath roms -window -resolution 672x384 \
	-keepaspect -skip_gameinfo $(if $(BIOS),-bios $(BIOS))
INTERACTIVE_NVRAM_DIR ?= $(abspath run_interactive/nvram)
INTERACTIVE_EXTRA_ARGS ?=

.PHONY: help venv download-mame overlay eeprom-profile normalize-3330 roms build swap16 census frontier-event-census controller-census mad2-census census-docs evidence-check test-tools prepare-run-nvram run run-frontier run-interactive smoke smoke-3330e smoke-3210-v501 audit-roms frame watch verify verify-ccont verify-gensio verify-display verify-dsp-transport verify-mad2 verify-mad2-interrupts verify-mad2-clocks verify-mbus verify-3210-v501 verify-frontier verify-frontier-stability verify-mmi-menu verify-mmi-menu-501 verify-structure verify-structure-subset run-manifest-default run-manifest-deep-gsm run-manifest-service run-manifest-3330 clean

help:
	@echo "make venv           create .venv from requirements.txt (for tools/)"
	@echo "make build          clone MAME at the pin, overlay $(DRIVER), build"
	@echo "make swap16         derive $(SWAP) from $(ROM) (16-bit byteswap, for the static tools)"
	@echo "make census         build the 3210 v6.00 message-topology JSON/report"
	@echo "make controller-census exhaustively verify the 3210 controller dispatcher"
	@echo "make census-docs    refresh the committed report; refuses missing scoped runtime"
	@echo "make evidence-check validate reviewed evidence ledgers and runtime manifests"
	@echo "make test-tools     run the static-tool and EEPROM-profile unit tests"
	@echo "make run-manifest-* reproduce named default/deep-gsm/contact/3330 evidence runs"
	@echo "make eeprom-profile build the synthetic 3210 24C128 image used by the oracle"
	@echo "make normalize-3330 extract the local Wintesla MCU/PPM/PMM record streams"
	@echo "make run            run the selected phone/profile into RUN_DIR=$(RUN_DIR)"
	@echo "make run-interactive open the provisioned 3210 in a persistent MAME window"
	@echo "make verify         run, then check the promoted frame SHA == $(ORACLE_FRAME_SHA)"
	@echo "make verify-ccont   check the organic GENSIO/CCONT transaction contract"
	@echo "make verify-gensio  check two-ROM endpoint and SELECT-register contracts"
	@echo "make verify-display check display-profile provenance and LCD serial transport"
	@echo "make verify-dsp-transport check DSPIF rings, completion and peer layering"
	@echo "make verify-mad2    check timer-0/FIQ and save-state restoration contracts"
	@echo "make verify-mad2-interrupts check simultaneous, masked-pending and extended-FIQ routing"
	@echo "make verify-mad2-clocks check reset/clock/watchdog boot contracts in both 3210 ROMs"
	@echo "make verify-mbus    check two-ROM MBUS init and external RX/FIQ2 contracts"
	@echo "make run-frontier   run the current external-service/SIM research profile"
	@echo "make verify-frontier reproduce the current coherent frontier predicates"
	@echo "make verify-frontier-stability repeat the frontier and require semantic stability"
	@echo "make verify-mmi-menu reproduce the provisioned interactive Phone book menu"
	@echo "make verify-mmi-menu-501 reproduce the same menu under the v5.01 BIOS"
	@echo "make mad2-census MAD2_LOG=... summarize a bounded MAD2 ledger trace"
	@echo "PRESERVE_NVRAM=1    retain EEPROM writes between runs (default reseeds the fixture)"
	@echo "make verify-structure  compare semantic boot predicates with $(ORACLE_STRUCT)"
	@echo "make smoke PHONE=noki3330  bounded non-oracle boot for another local ROM set"
	@echo "make smoke-3330e     normalize and boot the local v4.50 PPM E service files"
	@echo "make audit-roms PHONE=noki3330  report missing/mismatched files for a local set"
	@echo "make watch          live chafa preview of $(FRAME_PNG) (updated each run)"
	@echo "make clean          remove build/run state (keeps the MAME clone)"
	@echo "Override runtime knobs with RUN_ENV, e.g.  make run RUN_ENV='NOKI3210_TRACE_DISPLAY=1'"

venv:
	$(PYTHON) -m venv $(VENV)
	$(VENV)/bin/pip install -r requirements.txt

download-mame:
	@if [ ! -d $(MAME_DIR)/.git ]; then \
		git clone $(MAME_REPO) $(MAME_DIR) && git -C $(MAME_DIR) checkout $(MAME_COMMIT); \
	fi

# Overlay the local driver and component sources onto the upstream tree (MAME is not vendored).
overlay: download-mame
	install -D $(DRIVER) $(MAME_DIR)/src/mame/nokia/nokia_3310.cpp
	@for src in $(DRIVER_COMPONENTS); do install -D "$$src" "$(MAME_DIR)/src/mame/nokia/$$(basename "$$src")"; done

eeprom-profile:
	@test -f $(ROM) || { echo "Missing $(ROM) — bring your own dump (see roms/README.md)"; exit 1; }
	$(PYTHON) tools/make_eeprom_profile.py --flash $(ROM) --output "roms/noki3210/$(EEPROM_BASENAME)" \
		$(if $(PROVISIONED_IMEI_PREFIX),--provisioned-imei-prefix $(PROVISIONED_IMEI_PREFIX))

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
	$(MAKE) -C $(MAME_DIR) REGENIE=1 SOURCES=src/mame/nokia/nokia_3310.cpp,src/mame/nokia/nokia_ccont.cpp,src/mame/nokia/nokia_dsp_hle.cpp,src/mame/nokia/nokia_dspif.cpp,src/mame/nokia/nokia_external_service.cpp,src/mame/nokia/nokia_gensio.cpp,src/mame/nokia/nokia_mad2.cpp,src/mame/nokia/nokia_mbus.cpp,src/mame/nokia/nokia_simi.cpp,src/mame/nokia/nokia_sim_card.cpp USE_QTDEBUG=0 -j$$(nproc)

swap16:
	@test -f $(ROM) || { echo "Missing $(ROM) — see roms/README.md"; exit 1; }
	@$(PYTHON) -c "d=open('$(ROM)','rb').read(); b=bytearray(d); b[0::2],b[1::2]=d[1::2],d[0::2]; open('$(SWAP)','wb').write(bytes(b)); print('wrote $(SWAP) (%d bytes)'%len(b))"

census:
	@mkdir -p run_census
	$(PYTHON) tools/validate_evidence.py
	$(VENV)/bin/python tools/message_census.py --check \
		$(FRONTIER_EVENT_INVENTORIES) \
		$(foreach manifest,$(CENSUS_MANIFESTS),--runtime-manifest $(manifest)) \
		$(if $(CENSUS_LOG),--runtime-log $(CENSUS_LOG)) \
		--json run_census/noki3210_v600.json \
		--report run_census/noki3210_v600.md
	@echo "census: run_census/noki3210_v600.json and run_census/noki3210_v600.md"

frontier-event-census:
	@mkdir -p run_census
	$(VENV)/bin/python tools/message_census.py \
		$(FRONTIER_EVENT_INVENTORIES) \
		--json run_census/frontier_events.json \
		--report run_census/frontier_events.md
	@echo "frontier-event-census: run_census/frontier_events.json and run_census/frontier_events.md"

controller-census:
	$(VENV)/bin/python tools/controller_dispatch_census.py --check

mad2-census:
	@test -n "$(MAD2_LOG)" || { echo "Set MAD2_LOG to a MAME error log captured with NOKI3210_TRACE_MAD2_LEDGER=1"; exit 1; }
	@mkdir -p run_census
	$(VENV)/bin/python tools/mad2_access_census.py --check "$(MAD2_LOG)" \
		--json run_census/mad2_accesses.json --report run_census/mad2_accesses.md

census-docs:
	@mkdir -p run_census
	$(PYTHON) tools/validate_evidence.py
	$(VENV)/bin/python tools/controller_dispatch_census.py --check
	$(VENV)/bin/python tools/message_census.py --check \
		$(FRONTIER_EVENT_INVENTORIES) \
		$(foreach manifest,$(CENSUS_MANIFESTS),--runtime-manifest $(manifest)) \
		--require-runtime-subsystem external_service \
		--require-runtime-subsystem generic_service \
		--json run_census/noki3210_v600.json \
		--report docs/message_topology_census.md
	@echo "census-docs: docs/message_topology_census.md"

evidence-check:
	$(PYTHON) tools/validate_evidence.py

test-tools:
	$(VENV)/bin/python -m unittest tools/test_message_census.py tools/test_find_thumb_signature.py tools/test_make_eeprom_profile.py tools/test_mad2_access_census.py tools/test_sim_device_split.py tools/test_mad2_device_split.py tools/test_mbus_device_split.py tools/test_dsp_device_split.py tools/test_gensio_device_split.py tools/test_display_path.py tools/test_check_lcd_frame.py tools/test_keypad_input.py tools/test_machine_profile.py tools/test_ccont_watchdog.py tools/test_display_trace_check.py tools/test_gensio_trace_check.py tools/test_mad2_timer_trace_check.py tools/test_mad2_interrupt_trace_check.py tools/test_mad2_clock_trace_check.py tools/test_mbus_trace_check.py tools/test_dsp_transport_trace_check.py

run-manifest-default:
	@$(MAKE) --no-print-directory verify RUN_DIR=run_manifest_default SECONDS=4
	cp $(MAME_DIR)/error.log run_manifest_default/error.log

run-manifest-deep-gsm:
	@$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR=run_manifest_deep_gsm SECONDS=8 \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_TRACE_TASKS=1 NOKI3210_TRACE_SIM_RX=1 NOKI3210_TRACE_GSM_SERVICE=1 NOKI3210_TRACE_DSP_BOUNDARY=1'
	cp $(MAME_DIR)/error.log run_manifest_deep_gsm/error.log

run-manifest-service:
	@$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR=run_manifest_service_default SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_SERVICE_COMMAND=1'
	cp $(MAME_DIR)/error.log run_manifest_service_default/error.log
	@$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR=run_manifest_service_deep SECONDS=6 \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_TRACE_SERVICE_COMMAND=1'
	cp $(MAME_DIR)/error.log run_manifest_service_deep/error.log

run-manifest-3330:
	@$(MAKE) --no-print-directory smoke-3330e RUN_DIR=run_manifest_3330 SECONDS=3
	cp $(MAME_DIR)/error.log run_manifest_3330/error.log

prepare-run-nvram: build
	@if [ "$(PHONE)" = "noki3210" ]; then \
		mkdir -p "$(RUN_NVRAM_DIR)/$(PHONE)"; \
		if [ "$(PRESERVE_NVRAM)" != "1" ] || [ ! -f "$(RUN_NVRAM_DIR)/$(PHONE)/eeprom" ]; then \
			cp "$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)" \
				"$(RUN_NVRAM_DIR)/$(PHONE)/eeprom"; \
		fi; \
	fi

run: prepare-run-nvram
	@mkdir -p $(RUN_DIR)
	cd $(MAME_DIR) && env $(BOOT_ENV) $(RUN_ENV) NOKI3210_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKI3210_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		./mame $(MAME_ARGS) -nvram_directory $(RUN_NVRAM_DIR) -seconds_to_run $(SECONDS)
	@$(MAKE) --no-print-directory frame RUN_DIR=$(RUN_DIR)

run-frontier:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS) RUN_ENV='$(FRONTIER_ENV)'

run-interactive:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile BIOS=$(BIOS) ROM=$(ROM); \
		cp "roms/noki3210/$(EEPROM_BASENAME)" "$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory prepare-run-nvram PHONE=noki3210 BIOS=$(BIOS) ROM=$(ROM) \
		PROVISIONED_IMEI_PREFIX=49015420323751 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(INTERACTIVE_NVRAM_DIR); \
	( cd $(MAME_DIR) && env $(BOOT_ENV) $(FRONTIER_ENV) $(RUN_ENV) \
		./mame $(INTERACTIVE_MAME_ARGS) -nvram_directory $(INTERACTIVE_NVRAM_DIR) \
			$(INTERACTIVE_EXTRA_ARGS) )

smoke: build
	@mkdir -p $(RUN_DIR)
	cd $(MAME_DIR) && env $(BOOT_ENV) NOKI3210_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		./mame $(MAME_ARGS) -seconds_to_run $(SECONDS)

smoke-3330e: normalize-3330
	@$(MAKE) --no-print-directory smoke PHONE=noki3330 BIOS=450e RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS)

smoke-3210-v501:
	@$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS) RUN_ENV='$(FRONTIER_ENV)'

verify-3210-v501: smoke-3210-v501
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_V501_STRUCT)
	@echo "OK — v5.01 same-product startup predicates reproduced"

audit-roms: build
	cd $(MAME_DIR) && ./mame -rompath roms -verifyroms $(PHONE)

# Promote the latest informative LCD frame, falling back to the latest capture
# so the progress preview never silently remains stale.
frame:
	@f=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	fallback=0; \
	if [ -z "$$f" ]; then \
		f=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
		fallback=1; \
	fi; \
	if [ -z "$$f" ]; then echo "frame: no LCD frame in $(RUN_DIR) yet"; else \
		( magick "$$f" $(FRAME_PNG) 2>/dev/null || convert "$$f" $(FRAME_PNG) 2>/dev/null || pnmtopng "$$f" > $(FRAME_PNG) ) \
		&& { if [ $$fallback -eq 1 ]; then suffix=" (latest-capture fallback)"; fi; \
			echo "frame: $(FRAME_PNG) <- $$f$$suffix"; }; fi

# Live preview in this terminal (Ctrl-C to stop). External equivalent:
#   watch -n0.5 chafa --size=84x48 progress_latest_frame.png
watch:
	@command -v chafa >/dev/null || { echo "chafa not installed"; exit 1; }
	@while :; do clear; chafa --size=84x48 $(FRAME_PNG) 2>/dev/null || echo "no $(FRAME_PNG) yet"; sleep 0.5; done

verify: PHONE=noki3210
verify: RUN_ENV=$(CONTACT_SERVICE_ENV)
verify: run
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' ! -name '*_o000.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	got=$$(sha256sum "$$frame" | cut -c1-16); \
	echo "frame  : $$frame"; echo "sha256 : $$got"; echo "oracle : $(ORACLE_FRAME_SHA)"; \
	if [ "$$got" = "$(ORACLE_FRAME_SHA)" ]; then echo "OK — oracle reproduced"; \
	else echo "MISMATCH — boot diverged from the recorded CONTACT SERVICE state"; exit 1; fi
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR)

verify-ccont:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=1 RUN_ENV='NOKI3210_TRACE_GENSIO=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)/error.log --adc-profile sane
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_irq SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_GENSIO=1 NOKI3210_CCONT_CHARGER_PULSE_AT=0.2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_irq/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)_irq/error.log --adc-profile sane \
		--require-charger-irq --summary $(RUN_DIR)_irq/boot_summary.txt

verify-gensio:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_gensio_v600 SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_GENSIO=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_gensio_v600/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)_gensio_v600/error.log \
		--require-select-contract
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_gensio_v501 SECONDS=1 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_ENV='NOKI3210_TRACE_GENSIO=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_gensio_v501/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)_gensio_v501/error.log \
		--require-select-contract
	@echo "OK — GENSIO endpoint/status and SELECT-latch contracts reproduced across both 3210 ROMs"

verify-display:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_display_v600 SECONDS=3 \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_TRACE_DISPLAY_PROFILE=1 NOKI3210_TRACE_DISPLAY_IO=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_display_v600/error.log
	$(PYTHON) tools/display_trace_check.py $(RUN_DIR)_display_v600/error.log --firmware v600 \
		--rom roms/3210f600a_swap16.bin --eeprom "roms/noki3210/3210 v600 eeprom.bin" \
		--require-profile-boundary
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_display_v501 SECONDS=1 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_ENV='NOKI3210_TRACE_DISPLAY_IO=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_display_v501/error.log
	$(PYTHON) tools/display_trace_check.py $(RUN_DIR)_display_v501/error.log --firmware v501 \
		--rom roms/nokia_3210_nse-8_v05_01_full_hu_swap16.bin \
		--eeprom "roms/noki3210/3210 v501 eeprom.bin"

verify-dsp-transport:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_conformance SECONDS=1 \
		RUN_ENV='NOKI3210_DSPIF_CONFORMANCE=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp_conformance/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)_dsp_conformance/error.log --conformance
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp SECONDS=4 \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_TRACE_DSP_BOUNDARY=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)_dsp/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_v501 SECONDS=2 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_TRACE_DSP_BOUNDARY=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp_v501/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)_dsp_v501/error.log --bootstrap-only
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_state SECONDS=2 \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_STATE_ROUNDTRIP_AT=0.4'
	@grep -Fqx 'state_roundtrip=pass' $(RUN_DIR)_dsp_state/boot_summary.txt
	@echo "OK — DSPIF transport, split peer composition and active-profile save state reproduced"

verify-mad2:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=1 RUN_ENV='NOKI3210_TRACE_MAD2_TIMERS=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/mad2_timer_trace_check.py $(RUN_DIR)/error.log \
		--summary $(RUN_DIR)/boot_summary.txt --expected-line 4
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_state SECONDS=1 \
		RUN_ENV='NOKI3210_STATE_ROUNDTRIP_AT=0.4'
	@grep -Fqx 'state_roundtrip=pass' $(RUN_DIR)_state/boot_summary.txt
	@echo "OK — MAD2 state round trip restored RAM, timer and controller state"

verify-mad2-interrupts:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_overlap SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_MAD2_INTERRUPTS=1 NOKI3210_MAD2_IRQ_OVERLAP_AT=0.2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_overlap/error.log
	$(PYTHON) tools/mad2_interrupt_trace_check.py overlap $(RUN_DIR)_overlap/error.log \
		--summary $(RUN_DIR)_overlap/boot_summary.txt
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mask SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_MAD2_INTERRUPTS=1 NOKI3210_MAD2_IRQ_MASK_FIXTURE_AT=0.2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mask/error.log
	$(PYTHON) tools/mad2_interrupt_trace_check.py mask $(RUN_DIR)_mask/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_fiq8 SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_MAD2_INTERRUPTS=1 NOKI3210_MAD2_FIQ8_FIXTURE_AT=0.2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_fiq8/error.log
	$(PYTHON) tools/mad2_interrupt_trace_check.py fiq8 $(RUN_DIR)_fiq8/error.log
	@echo "OK — MAD2 simultaneous, masked-pending and extended-FIQ routing contracts reproduced"

verify-mad2-clocks:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_v600 SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_MAD2_CLOCKS=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v600/error.log
	$(PYTHON) tools/mad2_clock_trace_check.py $(RUN_DIR)_v600/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_v501 SECONDS=1 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_ENV='NOKI3210_TRACE_MAD2_CLOCKS=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v501/error.log
	$(PYTHON) tools/mad2_clock_trace_check.py $(RUN_DIR)_v501/error.log
	@echo "OK — MAD2 reset, clock-control and watchdog boot contracts reproduced across both 3210 ROMs"

verify-mbus:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mbus_v600 SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_MBUS=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mbus_v600/error.log
	$(PYTHON) tools/mbus_trace_check.py boot $(RUN_DIR)_mbus_v600/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mbus_v501 SECONDS=1 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_ENV='NOKI3210_TRACE_MBUS=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mbus_v501/error.log
	$(PYTHON) tools/mbus_trace_check.py boot $(RUN_DIR)_mbus_v501/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mbus_rx SECONDS=1 \
		RUN_ENV='NOKI3210_TRACE_MBUS=1 NOKI3210_MBUS_RX_FIXTURE=0xa5 NOKI3210_MBUS_RX_FIXTURE_AT_MS=300'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mbus_rx/error.log
	$(PYTHON) tools/mbus_trace_check.py rx $(RUN_DIR)_mbus_rx/error.log
	@echo "OK — MBUS initialization, idle TX and external RX/FIQ2 contracts reproduced"

verify-frontier: PHONE=noki3210
verify-frontier: run-frontier
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_FRONTIER_STRUCT)
	@echo "OK — coherent frontier predicates reproduced"

verify-mmi-menu:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" "$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR=$(RUN_DIR) SECONDS=12 \
		PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_POST_READY_KEYS=enter NOKI3210_POST_READY_KEY_DELAY_MS=5000 NOKI3210_POST_READY_CAPTURE_DELAY_MS=1000'; \
	$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_FRONTIER_STRUCT); \
	frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --mask 23,23,20,12 \
		--sha256 $(ORACLE_MMI_MENU_STABLE_SHA)
	@echo "OK — interactive Phone book menu reproduced"

verify-mmi-menu-501:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile BIOS=501 \
			ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls; \
		cp "roms/noki3210/3210 v501 eeprom.bin" "$(MAME_DIR)/roms/noki3210/3210 v501 eeprom.bin"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_DIR=$(RUN_DIR) SECONDS=12 \
		PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_ENV='$(FRONTIER_ENV) NOKI3210_POST_READY_KEYS=enter NOKI3210_POST_READY_KEY_DELAY_MS=5000 NOKI3210_POST_READY_CAPTURE_DELAY_MS=1000'; \
	$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_V501_STRUCT); \
	frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'noki3210_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --mask 23,23,20,12 \
		--sha256 $(ORACLE_MMI_MENU_STABLE_SHA)
	@echo "OK — v5.01 interactive Phone book menu reproduced"

FRONTIER_STABILITY_RUNS ?= 3
FRONTIER_STABILITY_STRICT ?= 0
verify-frontier-stability:
	@set -e; reference=""; \
	for iteration in $$(seq 1 $(FRONTIER_STABILITY_RUNS)); do \
		dir="$(RUN_DIR)_$$iteration"; \
		rm -rf "$$dir"; \
		$(MAKE) --no-print-directory verify-frontier RUN_DIR="$$dir" SECONDS=$(SECONDS); \
		summary=$$(sha256sum "$$dir/boot_summary.txt" | cut -d' ' -f1); \
		echo "frontier stability $$iteration/$(FRONTIER_STABILITY_RUNS): $$summary"; \
		if [ -z "$$reference" ]; then reference="$$summary"; \
		elif [ "$$summary" != "$$reference" ]; then \
			echo "frontier diagnostic-counter drift: $$summary != $$reference"; \
			if [ "$(FRONTIER_STABILITY_STRICT)" = "1" ]; then exit 1; fi; \
		fi; \
	done; \
	echo "OK — $(FRONTIER_STABILITY_RUNS) frontier runs reproduced the semantic predicates"

verify-structure-subset:
	@test -f $(RUN_DIR)/boot_summary.txt || { echo "missing $(RUN_DIR)/boot_summary.txt; run make run first"; exit 1; }
	@test -f $(ORACLE_STRUCT) || { echo "missing structural oracle $(ORACLE_STRUCT)"; exit 1; }
	@while IFS= read -r expected; do \
		grep -Fqx -- "$$expected" $(RUN_DIR)/boot_summary.txt || { echo "missing structural predicate: $$expected"; exit 1; }; \
	done < $(ORACLE_STRUCT)
	@echo "OK — semantic structural predicates reproduced"

verify-structure:
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_STRUCT)

clean:
	rm -rf $(RUN_DIR) run run_* $(MAME_DIR)/obj progress_latest_frame.*
