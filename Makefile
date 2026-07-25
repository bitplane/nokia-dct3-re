SHELL := /bin/bash

# Pinned MAME — fetched from upstream, with the local Nokia driver source set overlaid.
MAME_REPO   ?= https://github.com/mamedev/mame.git
MAME_COMMIT ?= 58fca9a8a20f75ac2010980e1a2ec0465c595583
MAME_DIR    ?= mame
JOBS        ?= 4

PYTHON ?= python3
VENV   := .venv
DRIVER := driver/nokia_dct3.cpp
MAME_PATCHES := patches/mame-nokia-dct3-driver-name.patch \
	patches/mame-intelfsh-dct3.patch patches/mame-pcd8544-geometry.patch \
	patches/mame-pulseaudio-input.patch
DRIVER_COMPONENTS := driver/nokia_ccont.cpp driver/nokia_ccont.h \
	driver/nokia_cobba.cpp driver/nokia_cobba.h \
	driver/nokia_b3_flash.cpp driver/nokia_b3_flash.h \
	driver/nokia_dsp_hle.cpp driver/nokia_dsp_hle.h \
	driver/nokia_dspif.cpp driver/nokia_dspif.h \
	driver/nokia_external_service.cpp driver/nokia_external_service.h \
	driver/nokia_gensio.cpp driver/nokia_gensio.h \
	driver/gsm_tch_f_l1.cpp driver/gsm_tch_f_l1.h \
	driver/nokia_gsm_fr_codec.cpp driver/nokia_gsm_fr_codec.h \
	driver/nokia_gsm_network.cpp driver/nokia_gsm_network.h \
	driver/nokia_gsm_session.cpp driver/nokia_gsm_session.h \
	driver/nokia_gsm_voice_peer.cpp driver/nokia_gsm_voice_peer.h \
	driver/nokia_lapdm_link.cpp driver/nokia_lapdm_link.h \
	driver/nokia_kbgpio.cpp driver/nokia_kbgpio.h \
	driver/nokia_mad2.cpp driver/nokia_mad2.h \
	driver/nokia_mad2_pcm.cpp driver/nokia_mad2_pcm.h \
	driver/nokia_mbus.cpp driver/nokia_mbus.h \
	driver/nokia_pup.cpp driver/nokia_pup.h \
	driver/nokia_radio_peer.cpp driver/nokia_radio_peer.h \
	driver/nokia_simi.cpp driver/nokia_simi.h \
	driver/nokia_sim_card.cpp driver/nokia_sim_card.h \
	driver/nokia_dct3_trace.inc
PHONE ?= noki3210
BIOS ?=

LIBGSM_VERSION := 1.0.24
LIBGSM_TARBALL := third_party/gsm-$(LIBGSM_VERSION).tar.gz
LIBGSM_DIR := third_party/libgsm-$(LIBGSM_VERSION)
LIBGSM_ARCHIVE := $(abspath $(LIBGSM_DIR)/lib/libgsm.a)
LIBGSM_SHA256 := a3c40c6471928383f4abfcb2e8f24012a1f562be2f17b8d672145d5986681a92
LIBGSM_SOURCES := add code decode gsm_create gsm_decode gsm_destroy gsm_encode \
	gsm_option long_term lpc preprocess rpe short_term table

# Bring-your-own firmware (see roms/README.md). Git-ignored.
ROM  ?= roms/3210f600a.fls
SWAP ?= roms/3210f600a_swap16.bin

RUN_DIR ?= run
SECONDS ?= 20
RUN_ENV ?=
RUN_VERBOSE ?= 0
RUN_EXTRA_ARGS ?=
RUN_NVRAM_DIR ?= $(abspath $(RUN_DIR))/nvram
NVRAM_SYSTEM := $(if $(and $(filter noki3210,$(PHONE)),$(filter 501,$(BIOS))),noki3210_1,$(PHONE))
PRESERVE_NVRAM ?= 0
PROVISIONED_IMEI_PREFIX ?=
ERASED_IDENTITY_SECURITY_CODE ?=
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
ORACLE_MMI_MENU_STABLE_SHA ?= 305c12459700027431ef9c04132bcceb7e2157ef87debf2cdf1ae438b8bd8d3f
ORACLE_RADIO_OPERATOR_CROP_SHA ?= 59dd0d4f80f705c98be148c7f60f3171d2b66d7a434fba51feef7a0134ada9a8
ORACLE_STRUCT ?= oracles/noki3210-default.struct
ORACLE_FRONTIER_STRUCT ?= oracles/noki3210-frontier.struct
ORACLE_V501_STRUCT ?= oracles/noki3210-v501-smoke.struct
ORACLE_3310_IDLE_SHA ?= 45cc33c77474936613e4add081d437bafd2f8686e27e3b985ad69283168852f9
ORACLE_3310_MENU_SHA ?= e0890d021f0e11de1978f9ecbcfa0321191ac3741da1379f82337c715079851a
ORACLE_3310_PHONEBOOK_NAV_SHA ?= 06ea6abd47a1c603fc60382a2ba7e78d7a1247de2a13079374b48fd6796e793e
ORACLE_3330_IDLE_SHA ?= f40de8661baf671706ad626bb89a7e2aece9c391597318248ffd592c7cfd867d
ORACLE_3330_MESSAGES_SHA ?= 61d28951699e81a78dbafa8b094cc2690b53f41ec2f61bbb5599e0bb61d569a0
ORACLE_3410_IDLE_SHA ?= 14c1f25e86f21ea7b52909b37fb624fcc9940668f9df19831a1f64b16913fd87
ORACLE_3410_MESSAGES_SHA ?= a44445d8880ee46944e4692d3823ae25dd902882f1f38c51f64dcc27412a279e

# The acquired virgin NHM-6 PMM legitimately requests its stored 12345 phone
# code, then a time and date. These are physical keypad transactions through
# firmware editors; keeping the sequence named makes the lengthy first-boot
# precondition reviewable in every 3330 gate.
NOKI3330_FIRST_BOOT_KEYS := 1,2,3,4,5,enter,wait8000,1,2,0,0,enter,wait1200,0,1,0,1,2,0,0,2,wait600,enter
NOKI3330_FIRST_BOOT_INPUT := NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
	NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280

# The validated 3210 device composition and calibrated boot values are product
# defaults in the machine configuration. Keep this alias while named research
# targets are normalized; it intentionally contributes no state-changing knobs.
FRONTIER_ENV :=

BOOT_ENV := NOKIA_DCT3_LUA_QUIET=1

# Explicit missing-hardware profile retained for the CONTACT SERVICE oracle.
# CCONT readiness is a reset-time device input; the peer devices are disabled
# at their ordinary boundaries. No firmware state is changed.
CONTACT_SERVICE_ARGS := -cfg_directory ../fixtures/contact_service
RADIO_PAGING_ARGS := -cfg_directory ../fixtures/radio_paging
RADIO_INCOMING_CALL_ARGS := -cfg_directory ../fixtures/radio_incoming_call
RADIO_INCOMING_CALL_ANSWERED_ARGS := -cfg_directory ../fixtures/radio_incoming_call_answered
RADIO_PCM_MISSING_ARGS := -cfg_directory ../fixtures/radio_pcm_missing
RADIO_INCOMING_CALL_DEGRADED_ARGS := -cfg_directory ../fixtures/radio_incoming_call_degraded
RADIO_INCOMING_SMS_ARGS := -cfg_directory ../fixtures/radio_incoming_sms
RADIO_INCOMING_SMART_MESSAGE_ARGS := -cfg_directory ../fixtures/radio_incoming_smart_message

MAME_ARGS := $(PHONE) -rompath roms -log -video none -sound none \
	-keyboardprovider none -mouseprovider none -lightgunprovider none \
	-joystickprovider none -midiprovider none -skip_gameinfo -nothrottle \
	-autoboot_script ../mame_nokia_dct3_input_exerciser.lua $(if $(BIOS),-bios $(BIOS)) \
	$(if $(filter 1,$(RUN_VERBOSE)),-verbose)
INTERACTIVE_MAME_ARGS := $(PHONE) -rompath roms -window -resolution 672x384 \
	-keepaspect -skip_gameinfo $(if $(BIOS),-bios $(BIOS))
INTERACTIVE_NVRAM_DIR ?= $(abspath run_interactive/nvram)
INTERACTIVE_EXTRA_ARGS ?=

.PHONY: help venv download-mame overlay eeprom-profile normalize-3330 normalize-3410 roms build swap16 census frontier-event-census controller-census ccont-static-census ccont-runtime-census mad2-census mad2-static-census dsp-census census-docs evidence-check test-tools prepare-run-nvram run run-frontier run-interactive smoke smoke-3310-639 smoke-3330e smoke-3410e smoke-3210-v501 audit-roms audit-dsp-roms frame watch verify verify-ccont verify-ccont-watchdog verify-ccont-rtc verify-ccont-mask verify-alarm verify-power-lifecycle verify-charger-lifecycle verify-charger-wake verify-gensio verify-display verify-dsp-transport verify-dsp-memory-upload verify-dsp-speech-control-static verify-gsm-fr-codec verify-gsm-tch-f-l1 verify-dsp-bootstrap-3310 verify-3310-frontier verify-3310-menu verify-3310-navigation verify-3330-frontier verify-3330-navigation verify-3410-frontier verify-3410-menu verify-3410-navigation verify-dsp-tone verify-radio-camp verify-radio-registration verify-radio-paging verify-radio-incoming-call verify-radio-incoming-ringing verify-radio-incoming-call-answered verify-radio-incoming-call-lifecycle verify-radio-incoming-call-lifecycle-v501 verify-radio-call-state-roundtrip verify-radio-pcm-missing verify-radio-degraded-speech verify-radio-physical-uplink verify-radio-physical-uplink-one verify-radio-incoming-sms verify-radio-incoming-smart-message verify-radio-operator verify-mad2 verify-mad2-interrupts verify-mad2-clocks verify-mad2-sleep verify-mad2-timer1 verify-mad2-reset verify-mbus verify-buzzer verify-vibrator verify-3210-v501 verify-frontier verify-frontier-stability verify-mmi-menu verify-mmi-menu-501 verify-sim-phonebook verify-structure verify-structure-subset run-manifest-default run-manifest-3330 clean

help:
	@echo "make venv           create .venv from requirements.txt (for tools/)"
	@echo "make build          clone MAME at the pin, overlay $(DRIVER), build"
	@echo "make swap16         derive $(SWAP) from $(ROM) (16-bit byteswap, for the static tools)"
	@echo "make census         build the 3210 v6.00 message-topology JSON/report"
	@echo "make controller-census exhaustively verify the 3210 controller dispatcher"
	@echo "make mad2-static-census extract paired-ROM direct MAD2 MMIO accesses"
	@echo "make dsp-census     refresh the paired-ROM DSP shared-memory and packet reports"
	@echo "make census-docs    refresh the committed report; refuses missing scoped runtime"
	@echo "make evidence-check validate reviewed evidence ledgers and runtime manifests"
	@echo "make test-tools     run the static-tool and EEPROM-profile unit tests"
	@echo "make run-manifest-* reproduce named default/deep-gsm/contact/3330 evidence runs"
	@echo "make eeprom-profile build the synthetic 3210 24C128 image used by the oracle"
	@echo "make normalize-3330 extract the local Wintesla MCU/PPM/PMM record streams"
	@echo "make normalize-3410 extract the local Wintesla MCU/PPM/PMM record streams"
	@echo "make run            run the selected phone/profile into RUN_DIR=$(RUN_DIR)"
	@echo "make run-interactive open the provisioned 3210 in a persistent MAME window"
	@echo "make smoke-3310-639 bounded local 3310 v6.39 portability spike"
	@echo "make verify         check the explicit missing-hardware semantic profile"
	@echo "make verify-ccont   check the organic GENSIO/CCONT transaction contract"
	@echo "make ccont-static-census check the five-ROM CCONT descriptor surface"
	@echo "make ccont-runtime-census regenerate five-ROM organic register coverage"
	@echo "make verify-ccont-watchdog check enabled watchdog service beyond 49 seconds"
	@echo "make verify-ccont-rtc check CCONT alarm programming and MAD2 IRQ2 delivery"
	@echo "make verify-ccont-mask check masked-pending delivery on CCONT IRQ2"
	@echo "make verify-alarm    set and ring a user alarm through organic keypad input"
	@echo "make verify-power-lifecycle check short-press UI and long-press shutdown behavior"
	@echo "make verify-charger-lifecycle check charger-present startup and power-key policy"
	@echo "make verify-charger-wake check powered-off charger restart and reset cause"
	@echo "make verify-gensio  check two-ROM endpoint and SELECT-register contracts"
	@echo "make verify-display check display-profile provenance and LCD serial transport"
	@echo "make verify-dsp-transport check DSPIF rings, completion and peer layering"
	@echo "make verify-dsp-memory-upload check paired-ROM DSP-addressed image application"
	@echo "make verify-dsp-speech-control-static verify paired-ROM speech/channel fields"
	@echo "make verify-gsm-fr-codec check the standards-based 20 ms PCM/frame boundary"
	@echo "make verify-gsm-tch-f-l1 check GSM-FR channel coding/interleaving/bursts"
	@echo "make verify-dsp-bootstrap-3310 check the local v6.39 58-exchange bootstrap"
	@echo "make verify-3310-frontier boot local v6.39 to its deterministic idle frame"
	@echo "make verify-3310-menu drive the v6.39 keypad to its Phone book menu"
	@echo "make verify-3310-navigation navigate the v6.39 Phone book and return to idle"
	@echo "make verify-3330-frontier complete virgin-PMM setup and reach v4.50 idle"
	@echo "make verify-3330-navigation navigate v4.50 to Messages and return to idle"
	@echo "make verify-3410-frontier compact the virgin PMM and wake the v5.46 idle UI"
	@echo "make verify-3410-menu open the v5.46 Messages menu through the physical keypad"
	@echo "make verify-3410-navigation open Messages and return to the exact idle UI"
	@echo "make verify-dsp-tone check the organic ROM-4 COBBA tone command"
	@echo "make verify-radio-camp check organic serving-cell selection and SI1-SI4"
	@echo "make verify-radio-registration check Location Updating, release and steady camp"
	@echo "make verify-radio-paging check PCH fill, one IMSI page and organic Paging Response"
	@echo "make verify-radio-incoming-call check organic MT SETUP, Alerting and bounded clearing"
	@echo "make verify-radio-incoming-call-answered check physical Answer, ringing and post-answer DSP traffic"
	@echo "make verify-radio-incoming-call-lifecycle check physical Answer-to-End CC and DSP-control teardown"
	@echo "make verify-radio-incoming-call-lifecycle-v501 check the cross-ROM MCU/DSP audio-control wire"
	@echo "make verify-radio-degraded-speech check burst errors, bad frames and media recovery"
	@echo "make verify-radio-physical-uplink check external microphone through timed Layer 1"
	@echo "make verify-radio-incoming-sms check organic segmented MT text delivery and SIM storage"
	@echo "make verify-radio-incoming-smart-message check part 1 of a queued long Nokia ringtone"
	@echo "make verify-radio-operator check registration plus firmware-rendered operator"
	@echo "make verify-mad2    check timer-0/FIQ and save-state restoration contracts"
	@echo "make verify-mad2-interrupts check simultaneous, masked-pending and extended-FIQ routing"
	@echo "make verify-mad2-clocks check reset/clock/watchdog boot contracts in both 3210 ROMs"
	@echo "make verify-mad2-sleep  check Timer-1 and physical-key wake from MAD2 clock stop"
	@echo "make verify-mad2-timer1 check Timer-1 destination/FIQ5 at accelerated controller time"
	@echo "make verify-mad2-reset check software/MAD2/CCONT watchdog reset domains and retained state"
	@echo "make verify-mbus    check two-ROM MBUS init and external RX/FIQ2 contracts"
	@echo "make run-frontier   run the current external-service/SIM research profile"
	@echo "make verify-frontier reproduce the current coherent frontier predicates"
	@echo "make verify-frontier-stability repeat the frontier and require semantic stability"
	@echo "make verify-mmi-menu reproduce the provisioned interactive Phone book menu"
	@echo "make verify-mmi-menu-501 reproduce the same menu under the v5.01 BIOS"
	@echo "make verify-sim-phonebook save and reload an organic persistent SIM contact"
	@echo "make verify-buzzer exercise the MAD2 piezo gate/divider MMIO contract"
	@echo "make verify-vibrator exercise the MAD2 vibrator gate/control MMIO contract"
	@echo "make mad2-census MAD2_LOG=... summarize a bounded MAD2 ledger trace"
	@echo "PRESERVE_NVRAM=1    retain EEPROM writes between runs (default reseeds the fixture)"
	@echo "JOBS=4              bound MAME build parallelism (override for larger machines)"
	@echo "make verify-structure  compare semantic boot predicates with $(ORACLE_STRUCT)"
	@echo "make smoke PHONE=noki3330  bounded non-oracle boot for another local ROM set"
	@echo "make smoke-3330e     normalize and boot the local v4.50 PPM E service files"
	@echo "make audit-roms PHONE=noki3330  report missing/mismatched files for a local set"
	@echo "make audit-dsp-roms classify local DSP regions and expose placeholder fill files"
	@echo "make watch          live chafa preview of $(FRAME_PNG) (updated each run)"
	@echo "make clean          remove build/run state (keeps the MAME clone)"
	@echo "Enable passive MAME log categories with RUN_VERBOSE=1; harness fixtures use RUN_ENV"

venv:
	$(PYTHON) -m venv $(VENV)
	$(VENV)/bin/pip install -r requirements.txt

download-mame:
	@if [ ! -d $(MAME_DIR)/.git ]; then \
		git clone $(MAME_REPO) $(MAME_DIR) && git -C $(MAME_DIR) checkout $(MAME_COMMIT); \
	fi

# Overlay the local driver and component sources onto the upstream tree (MAME is not vendored).
overlay: download-mame
	@for patch in $(MAME_PATCHES); do \
		if git -C $(MAME_DIR) apply --reverse --check "../$$patch" >/dev/null 2>&1; then :; \
		else git -C $(MAME_DIR) apply "../$$patch"; fi; \
	done
	install -C -D $(DRIVER) $(MAME_DIR)/src/mame/nokia/nokia_dct3.cpp
	@for src in $(DRIVER_COMPONENTS); do install -C -D "$$src" "$(MAME_DIR)/src/mame/nokia/$$(basename "$$src")"; done

$(LIBGSM_TARBALL):
	mkdir -p third_party
	curl --fail --location --output $@ https://www.quut.com/gsm/gsm-$(LIBGSM_VERSION).tar.gz
	echo "$(LIBGSM_SHA256)  $@" | sha256sum --check

$(LIBGSM_ARCHIVE): $(LIBGSM_TARBALL)
	echo "$(LIBGSM_SHA256)  $<" | sha256sum --check
	mkdir -p $(LIBGSM_DIR)/src $(LIBGSM_DIR)/inc $(LIBGSM_DIR)/lib
	tar -xzf $< -C $(LIBGSM_DIR) --strip-components=1
	@set -e; for source in $(LIBGSM_SOURCES); do \
		$(CC) -O2 -DNeedFunctionPrototypes=1 -DSASR -I$(LIBGSM_DIR)/inc \
			-c $(LIBGSM_DIR)/src/$$source.c -o $(LIBGSM_DIR)/lib/$$source.o; \
	done
	$(AR) rcs $@ $(addprefix $(LIBGSM_DIR)/lib/,$(addsuffix .o,$(LIBGSM_SOURCES)))

verify-gsm-fr-codec: $(LIBGSM_ARCHIVE)
	$(CXX) -std=c++17 -O2 driver/nokia_gsm_fr_codec.cpp \
		tools/test_gsm_fr_codec.cpp $(LIBGSM_ARCHIVE) \
		-o $(LIBGSM_DIR)/lib/test_gsm_fr_codec
	$(LIBGSM_DIR)/lib/test_gsm_fr_codec

verify-gsm-tch-f-l1:
	mkdir -p scratchpad
	$(CXX) -std=c++17 -O2 -Wall -Wextra -pedantic \
		driver/gsm_tch_f_l1.cpp tools/test_gsm_tch_f_l1.cpp \
		-o scratchpad/test_gsm_tch_f_l1
	scratchpad/test_gsm_tch_f_l1

verify-dsp-speech-control-static:
	$(VENV)/bin/python tools/dsp_speech_control_static_check.py \
		--v600 roms/3210f600a_swap16.bin \
		--v501 roms/nokia_3210_nse-8_v05_01_full_hu_swap16.bin

eeprom-profile:
	@test -f $(ROM) || { echo "Missing $(ROM) — bring your own dump (see roms/README.md)"; exit 1; }
	$(PYTHON) tools/make_eeprom_profile.py --flash $(ROM) --output "roms/noki3210/$(EEPROM_BASENAME)" \
		$(if $(PROVISIONED_IMEI_PREFIX),--provisioned-imei-prefix $(PROVISIONED_IMEI_PREFIX)) \
		$(if $(ERASED_IDENTITY_SECURITY_CODE),--erased-identity-security-code $(ERASED_IDENTITY_SECURITY_CODE))

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

normalize-3410:
	$(PYTHON) tools/extract_dct3_wintesla.py \
		--mcu roms/3410-nhm2-v546/NHM2NX05.460 \
		--ppm roms/3410-nhm2-v546/NHM2NX05.46E \
		--pmm "roms/3410-nhm2-v546/3410 virgin eeprom.pmm" \
		--flash-output roms/noki3410/3410f546e.fls \
		--eeprom-output "roms/noki3410/3410 virgin eeprom 005f0000.fls" \
		--expect-flash-sha1 e650b8a289b434f2c8260c68e44e70e84e41b4cc \
		--expect-eeprom-sha1 c1cb3a37efc11ea57b96969d2b01ca0f3b0f6bbe
	cp roms/noki3210/boot_rom roms/noki3210/dsp_prom roms/noki3210/dsp_drom roms/noki3210/dsp_pdrom roms/noki3410/

roms: $(if $(filter noki3210,$(PHONE)),eeprom-profile)
	@for src in roms/noki*/; do \
		[ -d "$$src" ] || continue; \
		dst="$(MAME_DIR)/roms/$$(basename "$$src")"; \
		mkdir -p "$$dst"; \
		cp -a "$$src". "$$dst"/; \
	done

build: overlay roms $(LIBGSM_ARCHIVE)
	$(MAKE) -C $(MAME_DIR) REGENIE=1 SOURCES=src/mame/nokia/nokia_dct3.cpp,src/mame/nokia/nokia_b3_flash.cpp,src/mame/nokia/nokia_ccont.cpp,src/mame/nokia/nokia_cobba.cpp,src/mame/nokia/nokia_dsp_hle.cpp,src/mame/nokia/nokia_dspif.cpp,src/mame/nokia/nokia_external_service.cpp,src/mame/nokia/nokia_gensio.cpp,src/mame/nokia/gsm_tch_f_l1.cpp,src/mame/nokia/nokia_gsm_fr_codec.cpp,src/mame/nokia/nokia_gsm_network.cpp,src/mame/nokia/nokia_gsm_session.cpp,src/mame/nokia/nokia_gsm_voice_peer.cpp,src/mame/nokia/nokia_lapdm_link.cpp,src/mame/nokia/nokia_kbgpio.cpp,src/mame/nokia/nokia_mad2.cpp,src/mame/nokia/nokia_mad2_pcm.cpp,src/mame/nokia/nokia_mbus.cpp,src/mame/nokia/nokia_pup.cpp,src/mame/nokia/nokia_radio_peer.cpp,src/mame/nokia/nokia_simi.cpp,src/mame/nokia/nokia_sim_card.cpp USE_QTDEBUG=0 LDFLAGS="-Wl,--whole-archive $(LIBGSM_ARCHIVE) -Wl,--no-whole-archive" -j$(JOBS)

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
	@test -n "$(MAD2_LOG)" || { echo "Set MAD2_LOG to a MAME error log captured with RUN_VERBOSE=1"; exit 1; }
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
	$(VENV)/bin/python -m unittest tools/test_message_census.py tools/test_find_thumb_signature.py tools/test_make_eeprom_profile.py tools/test_mad2_access_census.py tools/test_mad2_static_census.py tools/test_ccont_static_census.py tools/test_ccont_runtime_census.py tools/test_ccont_mask_pending_check.py tools/test_sim_device_split.py tools/test_sim_phonebook_check.py tools/test_b3_flash_device_split.py tools/test_mad2_device_split.py tools/test_mbus_device_split.py tools/test_dsp_device_split.py tools/test_dsp_rom_audit.py tools/test_dsp_upload_extract.py tools/test_dsp_memory_upload_trace_check.py tools/test_dsp_speech_control_static_check.py tools/test_speech_media_boundaries.py tools/test_gensio_device_split.py tools/test_kbgpio_device_split.py tools/test_pup_device_split.py tools/test_display_path.py tools/test_mame_patch_hygiene.py tools/test_mame_source_compliance.py tools/test_check_lcd_frame.py tools/test_keypad_input.py tools/test_machine_profile.py tools/test_ccont_watchdog.py tools/test_ccont_watchdog_trace_check.py tools/test_ccont_watchdog_expiry_check.py tools/test_ccont_rtc_trace_check.py tools/test_alarm_trace_check.py tools/test_power_lifecycle_check.py tools/test_charger_lifecycle_check.py tools/test_charger_wake_check.py tools/test_display_trace_check.py tools/test_gensio_trace_check.py tools/test_mad2_timer_trace_check.py tools/test_mad2_timer1_trace_check.py tools/test_mad2_interrupt_trace_check.py tools/test_mad2_clock_trace_check.py tools/test_mad2_sleep_trace_check.py tools/test_mbus_trace_check.py tools/test_dsp_transport_trace_check.py tools/test_dsp_tone_trace_check.py tools/test_dsp_shared_read_census.py tools/test_dsp_shared_transition_census.py tools/test_dsp_packet_semantics_census.py tools/test_radio_camp_trace_check.py tools/test_radio_registration_trace_check.py tools/test_radio_paging_trace_check.py tools/test_radio_incoming_call_trace_check.py tools/test_radio_incoming_ringing_trace_check.py tools/test_radio_answered_call_trace_check.py tools/test_radio_answered_audio_boundary_trace_check.py tools/test_radio_answered_call_lifecycle_trace_check.py tools/test_radio_call_audio_wire_trace_check.py tools/test_radio_speech_media_trace_check.py tools/test_radio_facch_interruption_trace_check.py tools/test_radio_call_state_roundtrip_trace_check.py tools/test_radio_pcm_missing_trace_check.py tools/test_radio_degraded_speech_trace_check.py tools/test_radio_physical_uplink_trace_check.py tools/test_radio_incoming_sms_trace_check.py tools/test_radio_incoming_smart_message_trace_check.py

ccont-static-census:
	$(VENV)/bin/python tools/ccont_static_census.py --check --json docs/data/ccont_static_census.json

ccont-runtime-census:
	@$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR=$(RUN_DIR)_3210v6 SECONDS=2 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_3210v6/error.log
	@$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_DIR=$(RUN_DIR)_3210v5 SECONDS=2 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_3210v5/error.log
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 RUN_DIR=$(RUN_DIR)_3310 SECONDS=2 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_3310/error.log
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e RUN_DIR=$(RUN_DIR)_3330 SECONDS=2 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_3330/error.log
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR)_3410 SECONDS=2 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_3410/error.log
	$(VENV)/bin/python tools/ccont_runtime_census.py --check --json docs/data/ccont_runtime_census.json \
		--log 3210-v6.00 $(RUN_DIR)_3210v6/error.log --log 3210-v5.01 $(RUN_DIR)_3210v5/error.log \
		--log 3310-v6.39 $(RUN_DIR)_3310/error.log --log 3330-v4.50 $(RUN_DIR)_3330/error.log \
		--log 3410-v5.46 $(RUN_DIR)_3410/error.log

run-manifest-default:
	@$(MAKE) --no-print-directory verify RUN_DIR=run_manifest_default SECONDS=4
	cp $(MAME_DIR)/error.log run_manifest_default/error.log

run-manifest-3330:
	@$(MAKE) --no-print-directory smoke-3330e RUN_DIR=run_manifest_3330 SECONDS=3
	cp $(MAME_DIR)/error.log run_manifest_3330/error.log

prepare-run-nvram: build
	@if [ "$(PHONE)" = "noki3210" ]; then \
		mkdir -p "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)"; \
		if [ "$(PRESERVE_NVRAM)" != "1" ]; then \
			rm -f "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/sim_card"; \
		fi; \
		if [ "$(PRESERVE_NVRAM)" != "1" ] || [ ! -f "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/eeprom" ]; then \
			cp "$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)" \
				"$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/eeprom"; \
		fi; \
	elif [ "$(PHONE)" = "noki3410" ] && [ "$(PRESERVE_NVRAM)" != "1" ]; then \
		rm -f "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/flash" \
			"$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/sim_card" \
			"$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/eeprom"; \
	fi

run: prepare-run-nvram
	@mkdir -p $(RUN_DIR)
	@find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' -delete
	@truncate -s 0 $(MAME_DIR)/error.log
	cd $(MAME_DIR) && env $(BOOT_ENV) $(RUN_ENV) NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		./mame $(MAME_ARGS) $(RUN_EXTRA_ARGS) -nvram_directory $(RUN_NVRAM_DIR) -seconds_to_run $(SECONDS)
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
	cd $(MAME_DIR) && env $(BOOT_ENV) NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		./mame $(MAME_ARGS) -seconds_to_run $(SECONDS)

smoke-3310-639:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS)

smoke-3330e: normalize-3330
	@$(MAKE) --no-print-directory smoke PHONE=noki3330 BIOS=450e RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS)

smoke-3410e: normalize-3410
	@$(MAKE) --no-print-directory smoke PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS)

smoke-3210-v501:
	@$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_DIR=$(RUN_DIR) SECONDS=$(SECONDS) RUN_ENV='$(FRONTIER_ENV)'

verify-3210-v501: smoke-3210-v501
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_V501_STRUCT)
	@echo "OK — v5.01 same-product startup predicates reproduced"

audit-roms: build
	cd $(MAME_DIR) && ./mame -rompath roms -verifyroms $(PHONE)

audit-dsp-roms:
	$(PYTHON) tools/dsp_rom_audit.py \
		roms/$(PHONE)/dsp_prom roms/$(PHONE)/dsp_drom roms/$(PHONE)/dsp_pdrom

verify-dsp-memory-upload:
	@$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR=$(RUN_DIR)_v600 \
		SECONDS=4 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v600/error.log
	$(PYTHON) tools/dsp_memory_upload_trace_check.py $(RUN_DIR)_v600/error.log
	@$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_DIR=$(RUN_DIR)_v501 \
		SECONDS=4 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v501/error.log
	$(PYTHON) tools/dsp_memory_upload_trace_check.py $(RUN_DIR)_v501/error.log

# Promote the latest informative LCD frame, falling back to the latest capture
# so the progress preview never silently remains stale.
frame:
	@f=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' ! -name '*_z918_*' ! -name '*_ff918_*' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	fallback=0; \
	if [ -z "$$f" ]; then \
		f=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
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
verify: RUN_EXTRA_ARGS=$(CONTACT_SERVICE_ARGS)
verify: run
	@$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR)
	@echo "OK — missing-hardware semantic predicates reproduced"

verify-ccont:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=1 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)/error.log --adc-profile sane
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_irq SECONDS=4 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_CCONT_CHARGER_PULSE_AT=2.0'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_irq/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)_irq/error.log \
		--require-charger-irq --summary $(RUN_DIR)_irq/boot_summary.txt

verify-ccont-watchdog:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=55 \
		PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/ccont_watchdog_trace_check.py \
		$(RUN_DIR)/error.log $(RUN_DIR)/boot_summary.txt
	@echo "OK — enabled CCONT watchdog is serviced organically beyond its 49-second window"

verify-gensio:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_gensio_v600 SECONDS=1 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_gensio_v600/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)_gensio_v600/error.log \
		--require-select-contract --require-ccont-boot-status
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_gensio_v501 SECONDS=1 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_gensio_v501/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)_gensio_v501/error.log \
		--require-select-contract --require-ccont-boot-status
	@echo "OK — GENSIO endpoint/status and SELECT-latch contracts reproduced across both 3210 ROMs"

verify-display:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_display_v600 SECONDS=3 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_display_v600/error.log
	$(PYTHON) tools/display_trace_check.py $(RUN_DIR)_display_v600/error.log --firmware v600 \
		--rom roms/3210f600a_swap16.bin --eeprom "roms/noki3210/3210 v600 eeprom.bin" \
		--require-profile-boundary
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_display_v501 SECONDS=1 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_display_v501/error.log
	$(PYTHON) tools/display_trace_check.py $(RUN_DIR)_display_v501/error.log --firmware v501 \
		--rom roms/nokia_3210_nse-8_v05_01_full_hu_swap16.bin \
		--eeprom "roms/noki3210/3210 v501 eeprom.bin"

verify-dsp-transport:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_conformance SECONDS=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/dspif_conformance'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp_conformance/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)_dsp_conformance/error.log --conformance
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp SECONDS=4 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)_dsp/error.log \
		--expected-bootstrap-exchanges 64
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_v501 SECONDS=2 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp_v501/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)_dsp_v501/error.log --bootstrap-only \
		--expected-bootstrap-exchanges 64
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_state SECONDS=2 \
		RUN_ENV='$(FRONTIER_ENV) NOKIA_DCT3_STATE_ROUNDTRIP_AT=0.4'
	@grep -Fqx 'state_roundtrip=pass' $(RUN_DIR)_dsp_state/boot_summary.txt
	@echo "OK — DSPIF transport, split peer composition and active-profile save state reproduced"

verify-dsp-bootstrap-3310:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=1 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)/error.log \
		--completion-only --expected-bootstrap-exchanges 58
	@echo "OK — 3310 v6.39 completed its product-calibrated DSP bootstrap"

verify-3310-frontier:
	@$(MAKE) --no-print-directory smoke-3310-639 RUN_DIR=$(RUN_DIR) SECONDS=15
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3310 LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3310_IDLE_SHA)
	@echo "OK — 3310 v6.39 product profile reached its idle frame"

verify-3310-menu:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 RUN_DIR=$(RUN_DIR) SECONDS=11 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3310 menu frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3310_MENU_SHA)
	@echo "OK — 3310 v6.39 physical keypad opened the Phone book menu"

verify-3310-navigation:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 RUN_DIR=$(RUN_DIR)_forward SECONDS=13 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,enter,wait700,enter,wait700,down NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR)_forward -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3310 navigation frame produced in $(RUN_DIR)_forward"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3310_PHONEBOOK_NAV_SHA)
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 RUN_DIR=$(RUN_DIR)_return SECONDS=14 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,enter,wait700,enter,wait700,down,wait700,c,wait700,c NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR)_return -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3310 return frame produced in $(RUN_DIR)_return"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3310_IDLE_SHA)
	@echo "OK — 3310 v6.39 navigated the Phone book and returned to idle through physical keys"

verify-3330-frontier: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e RUN_DIR=$(RUN_DIR) SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3330 idle frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3330_IDLE_SHA)
	@echo "OK — 3330 v4.50 completed virgin-PMM setup and reached idle"

verify-3330-navigation: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e RUN_DIR=$(RUN_DIR)_forward SECONDS=49 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS),wait4000,enter,wait900,down NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=2500'
	@frame=$$(find $(RUN_DIR)_forward -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3330 navigation frame produced in $(RUN_DIR)_forward"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3330_MESSAGES_SHA)
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e RUN_DIR=$(RUN_DIR)_return SECONDS=51 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS),wait4000,enter,wait900,down,wait900,c NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=3000'
	@frame=$$(find $(RUN_DIR)_return -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3330 return frame produced in $(RUN_DIR)_return"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3330_IDLE_SHA)
	@echo "OK — 3330 v4.50 navigated to Messages and returned to idle through physical keys"

verify-3410-frontier: normalize-3410
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR) \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR))/nvram SECONDS=22 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z918_*' ! -name '*_ff918_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3410 idle frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3410_IDLE_SHA)
	@grep -Fqx 'soft_resets=0' $(RUN_DIR)/boot_summary.txt
	@echo "OK — 3410 v5.46 compacted its virgin PMM and exposed the idle UI"

verify-3410-menu: normalize-3410
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR) \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR))/nvram SECONDS=22 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z918_*' ! -name '*_ff918_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3410 menu frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3410_MESSAGES_SHA)
	@grep -Fqx 'soft_resets=0' $(RUN_DIR)/boot_summary.txt
	@echo "OK — 3410 v5.46 physical Menu key opened Messages"

verify-3410-navigation: normalize-3410
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR)_menu \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_menu)/nvram SECONDS=22 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR)_menu -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z918_*' ! -name '*_ff918_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3410 menu frame produced in $(RUN_DIR)_menu"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3410_MESSAGES_SHA)
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR)_return \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_return)/nvram SECONDS=24 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR)_return -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z918_*' ! -name '*_ff918_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3410 return frame produced in $(RUN_DIR)_return"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3410_IDLE_SHA)
	@grep -Fqx 'soft_resets=0' $(RUN_DIR)_menu/boot_summary.txt
	@grep -Fqx 'soft_resets=0' $(RUN_DIR)_return/boot_summary.txt
	@echo "OK — 3410 v5.46 opened Messages and returned to idle through physical keys"

verify-radio-camp:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=20 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_camp_trace_check.py $(RUN_DIR)/error.log

verify-radio-registration:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=25 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py $(RUN_DIR)/error.log

verify-radio-paging:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=32 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-radio-incoming-call:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=36 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_incoming_call_trace_check.py $(RUN_DIR)/error.log

verify-radio-incoming-ringing: verify-radio-incoming-call-answered

verify-radio-incoming-call-answered:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,waitbuzzer,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_incoming_ringing_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_answered_call_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_answered_audio_boundary_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-incoming-call-lifecycle:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,waitbuzzer,enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-incoming-call-lifecycle-v501:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile BIOS=501 \
			ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls; \
		cp "roms/noki3210/3210 v501 eeprom.bin" \
			"$(MAME_DIR)/roms/noki3210/3210 v501 eeprom.bin"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_DIR=$(RUN_DIR) SECONDS=45 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,waitbuzzer,enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_call_audio_wire_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-call-state-roundtrip:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	replay_env='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,waitbuzzer,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=28.5 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=2000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=3000'; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_v600 SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV="$$replay_env"; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_facch_interruption_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_DIR=$(RUN_DIR)_v501 SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV="$$replay_env"; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_call_audio_wire_trace_check.py \
		$(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_facch_interruption_trace_check.py \
		$(RUN_DIR)_v501/error.log

verify-radio-pcm-missing:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	pcm_missing_env='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,waitbuzzer,enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_v600 SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_PCM_MISSING_ARGS)' \
		RUN_ENV="$$pcm_missing_env"; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_pcm_missing_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_DIR=$(RUN_DIR)_v501 SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_PCM_MISSING_ARGS)' \
		RUN_ENV="$$pcm_missing_env"; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_call_audio_wire_trace_check.py \
		$(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_pcm_missing_trace_check.py \
		$(RUN_DIR)_v501/error.log

verify-radio-degraded-speech:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	degraded_env='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,waitbuzzer,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=28.5 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=2000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=3000'; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_v600 SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_DEGRADED_ARGS)' \
		RUN_ENV="$$degraded_env"; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(PYTHON) tools/radio_degraded_speech_trace_check.py \
		$(RUN_DIR)_v600/error.log; \
	$(MAKE) --no-print-directory run PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_DIR=$(RUN_DIR)_v501 SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_DEGRADED_ARGS)' \
		RUN_ENV="$$degraded_env"; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_call_audio_wire_trace_check.py \
		$(RUN_DIR)_v501/error.log; \
	$(PYTHON) tools/radio_degraded_speech_trace_check.py \
		$(RUN_DIR)_v501/error.log

verify-radio-physical-uplink:
	$(MAKE) --no-print-directory verify-radio-physical-uplink-one \
		RUN_DIR=$(RUN_DIR)_physical_v600 BIOS= ROM=roms/3210f600a.fls \
		EEPROM_BASENAME='3210 v600 eeprom.bin' \
		AUDIO_CONTROL_CHECKER=tools/radio_answered_call_lifecycle_trace_check.py
	$(MAKE) --no-print-directory verify-radio-physical-uplink-one \
		RUN_DIR=$(RUN_DIR)_physical_v501 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		EEPROM_BASENAME='3210 v501 eeprom.bin' \
		AUDIO_CONTROL_CHECKER=tools/radio_call_audio_wire_trace_check.py

verify-radio-physical-uplink-one:
	RUN_DIR=$(RUN_DIR) BIOS=$(BIOS) ROM=$(ROM) \
		EEPROM_BASENAME='$(EEPROM_BASENAME)' \
		AUDIO_CONTROL_CHECKER='$(AUDIO_CONTROL_CHECKER)' \
		tools/run_physical_uplink_gate.sh

verify-radio-incoming-sms:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=40 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_incoming_sms_trace_check.py $(RUN_DIR)/error.log \
		$(RUN_DIR)/nvram/noki3210/sim_card

verify-radio-incoming-smart-message:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=40 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_incoming_smart_message_trace_check.py \
		$(RUN_DIR)/error.log $(RUN_DIR)/nvram/noki3210/sim_card

verify-radio-operator:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" "$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=105 \
		PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_VERBOSE=1; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_registration_trace_check.py $(RUN_DIR)/error.log; \
	frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no registered operator frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --crop 6,0,12,7 \
		--sha256 $(ORACLE_RADIO_OPERATOR_CROP_SHA)
	@echo "OK — registered test-PLMN operator presentation reproduced"

dsp-census:
	@$(MAKE) --no-print-directory run RUN_DIR=run_dsp_census_v600 SECONDS=20 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log run_dsp_census_v600/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=run_dsp_census_v501 SECONDS=20 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log run_dsp_census_v501/error.log
	$(VENV)/bin/python tools/dsp_shared_read_census.py \
		v600=run_dsp_census_v600/error.log v501=run_dsp_census_v501/error.log \
		--json evidence/runtime/dsp_shared_reads.json --report docs/dsp_shared_memory_inventory.md --check
	$(VENV)/bin/python tools/dsp_shared_transition_census.py \
		v600=run_dsp_census_v600/error.log v501=run_dsp_census_v501/error.log \
		--json evidence/runtime/dsp_shared_transitions.json --report docs/dsp_shared_memory_transitions.md --check
	$(VENV)/bin/python tools/dsp_packet_semantics_census.py \
		v600=run_dsp_census_v600/error.log v501=run_dsp_census_v501/error.log \
		--json evidence/runtime/dsp_packets.json --report docs/dsp_packet_semantics.md --check

verify-mad2:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=1 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/mad2_timer_trace_check.py $(RUN_DIR)/error.log \
		--summary $(RUN_DIR)/boot_summary.txt --expected-line 4
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_state SECONDS=1 \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=0.4'
	@grep -Fqx 'state_roundtrip=pass' $(RUN_DIR)_state/boot_summary.txt
	@echo "OK — MAD2 state round trip restored RAM, timer and controller state"

verify-mad2-interrupts:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_overlap SECONDS=4 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_IRQ_OVERLAP_AT=2.0'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_overlap/error.log
	$(PYTHON) tools/mad2_interrupt_trace_check.py overlap $(RUN_DIR)_overlap/error.log \
		--summary $(RUN_DIR)_overlap/boot_summary.txt
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mask SECONDS=1 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_IRQ_MASK_FIXTURE_AT=0.2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mask/error.log
	$(PYTHON) tools/mad2_interrupt_trace_check.py mask $(RUN_DIR)_mask/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_fiq8 SECONDS=1 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_FIQ8_FIXTURE_AT=0.2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_fiq8/error.log
	$(PYTHON) tools/mad2_interrupt_trace_check.py fiq8 $(RUN_DIR)_fiq8/error.log
	@echo "OK — MAD2 simultaneous, masked-pending and extended-FIQ routing contracts reproduced"

verify-mad2-clocks:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_v600 SECONDS=12 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v600/error.log
	$(PYTHON) tools/mad2_clock_trace_check.py $(RUN_DIR)_v600/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_v501 SECONDS=12 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_v501/error.log
	$(PYTHON) tools/mad2_clock_trace_check.py $(RUN_DIR)_v501/error.log
	@echo "OK — MAD2 reset-cause, SIM clock-gate and conditional-watchdog contracts reproduced across both 3210 ROMs"

verify-mad2-sleep:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_sleep_v600 SECONDS=35 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_SLEEP_FIXTURE_AT=0.01 NOKIA_DCT3_MAD2_SLEEP_FIXTURE_SOURCE=timer1 NOKIA_DCT3_STATE_ROUNDTRIP_AT=0.015'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_sleep_v600/error.log
	$(PYTHON) tools/mad2_sleep_trace_check.py $(RUN_DIR)_sleep_v600/error.log \
		--source timer1 --summary $(RUN_DIR)_sleep_v600/boot_summary.txt
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_sleep_v501 SECONDS=35 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_SLEEP_FIXTURE_AT=0.01 NOKIA_DCT3_MAD2_SLEEP_FIXTURE_SOURCE=timer1 NOKIA_DCT3_STATE_ROUNDTRIP_AT=0.015'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_sleep_v501/error.log
	$(PYTHON) tools/mad2_sleep_trace_check.py $(RUN_DIR)_sleep_v501/error.log \
		--source timer1 --summary $(RUN_DIR)_sleep_v501/boot_summary.txt
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_sleep_keypad SECONDS=1 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_SLEEP_FIXTURE_AT=0.2 NOKIA_DCT3_MAD2_SLEEP_FIXTURE_SOURCE=keypad'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_sleep_keypad/error.log
	$(PYTHON) tools/mad2_sleep_trace_check.py $(RUN_DIR)_sleep_keypad/error.log --source keypad
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_sleep_fiq8 SECONDS=1 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_SLEEP_FIXTURE_AT=0.2005 NOKIA_DCT3_MAD2_SLEEP_FIXTURE_SOURCE=fiq8'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_sleep_fiq8/error.log
	$(PYTHON) tools/mad2_sleep_trace_check.py $(RUN_DIR)_sleep_fiq8/error.log --source fiq8
	@echo "OK — MAD2 clock stop, Timer-1/FIQ8/keypad wake and sleep-state restore reproduced"

verify-mad2-timer1:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/mad2_timer1_trace_check.py $(RUN_DIR)/error.log
	@echo "OK — MAD2 Timer-1 reached 0x7fff, asserted FIQ5 and was acknowledged"

verify-mad2-reset:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_software SECONDS=4 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_RESET_FIXTURE_AT=2.0'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_software/error.log
	$(PYTHON) tools/mad2_clock_trace_check.py $(RUN_DIR)_software/error.log \
		--require-software-reset --allow-no-watchdog
	@grep -Eq '^soft_resets=[1-9][0-9]*$$' $(RUN_DIR)_software/boot_summary.txt
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_watchdog SECONDS=3 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MAD2_WATCHDOG_FIXTURE_AT=0.2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_watchdog/error.log
	$(PYTHON) tools/mad2_clock_trace_check.py $(RUN_DIR)_watchdog/error.log \
		--require-watchdog-reset --allow-no-watchdog --allow-incomplete-clock-lifecycle
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_ccont_watchdog SECONDS=6 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_CCONT_WATCHDOG_FIXTURE_AT=2.0'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_ccont_watchdog/error.log
	$(PYTHON) tools/ccont_watchdog_expiry_check.py $(RUN_DIR)_ccont_watchdog/error.log
	@echo "OK — software, MAD2-watchdog and CCONT-watchdog baseband reset contracts reproduced"

verify-mbus:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mbus_v600 SECONDS=1 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mbus_v600/error.log
	$(PYTHON) tools/mbus_trace_check.py boot $(RUN_DIR)_mbus_v600/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mbus_v501 SECONDS=1 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mbus_v501/error.log
	$(PYTHON) tools/mbus_trace_check.py boot $(RUN_DIR)_mbus_v501/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_mbus_rx SECONDS=1 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_MBUS_RX_FIXTURE=0xa5 NOKIA_DCT3_MBUS_RX_FIXTURE_AT_MS=300'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_mbus_rx/error.log
	$(PYTHON) tools/mbus_trace_check.py rx $(RUN_DIR)_mbus_rx/error.log
	@echo "OK — MBUS initialization, idle TX and external RX/FIQ2 contracts reproduced"

verify-buzzer:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_buzzer SECONDS=1 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_BUZZER_FIXTURE_AT=0.3'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_buzzer/error.log
	$(PYTHON) tools/buzzer_trace_check.py $(RUN_DIR)_buzzer/error.log

verify-vibrator:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_vibrator SECONDS=1 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_VIBRATOR_FIXTURE_AT=0.3'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_vibrator/error.log
	$(PYTHON) tools/vibrator_trace_check.py $(RUN_DIR)_vibrator/error.log

verify-dsp-tone:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_tone_v600 SECONDS=3 \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp_tone_v600/error.log
	$(PYTHON) tools/dsp_tone_trace_check.py $(RUN_DIR)_dsp_tone_v600/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_dsp_tone_v501 SECONDS=3 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)_dsp_tone_v501/error.log
	$(PYTHON) tools/dsp_tone_trace_check.py $(RUN_DIR)_dsp_tone_v501/error.log

verify-ccont-rtc:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_rtc SECONDS=19 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_CCONT_RTC_FIXTURE_AT=15'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_rtc/error.log
	$(PYTHON) tools/ccont_rtc_trace_check.py $(RUN_DIR)_rtc/error.log

verify-ccont-mask:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_ccont_mask SECONDS=6 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_CCONT_MASK_FIXTURE_AT=2'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_ccont_mask/error.log
	$(PYTHON) tools/ccont_mask_pending_check.py $(RUN_DIR)_ccont_mask/error.log

verify-alarm:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_alarm SECONDS=135 \
		PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 NOKIA_DCT3_POST_READY_KEY_GAP_MS=180 NOKIA_DCT3_POST_READY_KEYS=enter,wait700,down,wait400,down,wait400,down,wait400,down,wait400,down,wait400,down,wait400,down,wait400,enter,wait700,enter,wait700,1,wait400,1,wait400,2,wait400,0,wait400,1,wait400,enter,wait700,enter,wait700,0,wait400,1,wait400,0,wait400,1,wait400,1,wait400,9,wait400,9,wait400,9,wait400,enter,wait900,c,wait900,enter,wait700,down,wait400,down,wait400,down,wait400,down,wait400,down,wait400,down,wait400,down,wait400,enter,wait700,enter,wait700,1,wait400,2,wait400,0,wait400,2,wait400,enter'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_alarm/error.log
	$(PYTHON) tools/alarm_trace_check.py $(RUN_DIR)_alarm/error.log

verify-power-lifecycle:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_power_short SECONDS=18 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=power NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=250 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1500'
	$(PYTHON) tools/power_lifecycle_check.py short $(RUN_DIR)_power_short/boot_summary.txt
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_power_long SECONDS=20 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=power NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=2000 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1500'
	$(PYTHON) tools/power_lifecycle_check.py long $(RUN_DIR)_power_long/boot_summary.txt
	@echo "OK — physical power-key short/long firmware lifecycles reproduced"

verify-charger-lifecycle:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_charger_connected SECONDS=18 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_CCONT_CHARGER_INITIAL=1'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_charger_connected/error.log
	$(PYTHON) tools/gensio_trace_check.py $(RUN_DIR)_charger_connected/error.log \
		--require-charger-irq --charger-present-only \
		--summary $(RUN_DIR)_charger_connected/boot_summary.txt
	$(PYTHON) tools/charger_lifecycle_check.py connected \
		$(RUN_DIR)_charger_connected/boot_summary.txt
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_acting_dead SECONDS=22 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_CCONT_CHARGER_INITIAL=1 NOKIA_DCT3_POST_READY_KEYS=power NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=4000 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1500'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_acting_dead/error.log
	$(PYTHON) tools/charger_lifecycle_check.py acting-dead \
		$(RUN_DIR)_acting_dead/boot_summary.txt
	@echo "OK — charger-present startup and acting-dead lifecycle reproduced"

verify-charger-wake:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_charger_wake SECONDS=35 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=power NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=4000 NOKIA_DCT3_CCONT_CHARGER_PULSE_AT=16 NOKIA_DCT3_CCONT_CHARGER_PULSE_DURATION=30'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_charger_wake/error.log
	$(PYTHON) tools/charger_wake_check.py $(RUN_DIR)_charger_wake/error.log \
		$(RUN_DIR)_charger_wake/boot_summary.txt
	@echo "OK — powered-off charger edge restarted the digital baseband into acting-dead mode"

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
	$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR=$(RUN_DIR) SECONDS=20 \
		PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_ENV='$(FRONTIER_ENV) NOKIA_DCT3_POST_READY_KEYS=enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=3000'; \
	$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_FRONTIER_STRUCT); \
	frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
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
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls RUN_DIR=$(RUN_DIR) SECONDS=20 \
		PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_ENV='$(FRONTIER_ENV) NOKIA_DCT3_POST_READY_KEYS=enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=3000'; \
	$(MAKE) --no-print-directory verify-structure-subset RUN_DIR=$(RUN_DIR) ORACLE_STRUCT=$(ORACLE_V501_STRUCT); \
	frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative LCD frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --mask 23,23,20,12 \
		--sha256 $(ORACLE_MMI_MENU_STABLE_SHA)
	@echo "OK — v5.01 interactive Phone book menu reproduced"

verify-sim-phonebook:
	@set -e; \
	save_dir="$(RUN_DIR)_phonebook_save"; \
	reload_dir="$(RUN_DIR)_phonebook_reload"; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" "$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR="$$save_dir" SECONDS=32 \
		PRESERVE_NVRAM=0 PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_VERBOSE=1 RUN_ENV='NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 NOKIA_DCT3_POST_READY_KEY_GAP_MS=180 NOKIA_DCT3_POST_READY_KEYS=enter,wait700,enter,wait700,down,wait400,enter,wait700,2,3,2,wait1200,enter,wait800,1,2,3,wait800,enter NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=2500'; \
	cp "$(MAME_DIR)/error.log" "$$save_dir/error.log"; \
	$(PYTHON) tools/sim_phonebook_check.py "$$save_dir/error.log" \
		"$$save_dir/nvram/$(NVRAM_SYSTEM)/sim_card"; \
	$(MAKE) --no-print-directory run PHONE=noki3210 RUN_DIR="$$reload_dir" SECONDS=24 \
		PRESERVE_NVRAM=1 PROVISIONED_IMEI_PREFIX=49015420323751 \
		RUN_NVRAM_DIR="$(abspath $(RUN_DIR)_phonebook_save/nvram)" \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEYS=enter,wait700,enter,wait700,enter,wait700,enter NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=2500'; \
	frame=$$(find "$$reload_dir" -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z504_*' ! -name '*_ff504_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no reloaded phonebook frame produced"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --mask 23,23,20,12 \
		--sha256 c496d0162ee327f35d8e8440535fd70462263f034cdd7fffde5db2872e754a07
	@echo "OK — firmware saved ADA/123 to EF_ADN and rendered it after a SIM-NVRAM reload"

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

mad2-static-census:
	@mkdir -p run_census
	$(VENV)/bin/python tools/mad2_static_census.py --check \
		--json run_census/mad2_static_access.json --markdown docs/mad2_static_access.md
