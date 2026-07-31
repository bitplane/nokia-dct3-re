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
	driver/nokia_gsm_call_adapter.cpp driver/nokia_gsm_call_adapter.h \
	driver/nokia_gensio.cpp driver/nokia_gensio.h \
	driver/gsm_a3a8.cpp driver/gsm_a3a8.h \
	driver/gsm_a5.cpp driver/gsm_a5.h \
	driver/gsm_mobility.h \
	driver/gsm_mm_authentication.cpp driver/gsm_mm_authentication.h \
	driver/gsm_sms_transport.cpp driver/gsm_sms_transport.h \
	driver/gsm_tch_f_l1.cpp driver/gsm_tch_f_l1.h \
	driver/gsm_xcch_l1.cpp driver/gsm_xcch_l1.h \
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
NVRAM_SYSTEM := $(strip $(if $(and $(filter noki3210,$(PHONE)),$(filter 501,$(BIOS))),noki3210_1,\
	$(if $(and $(filter noki3310,$(PHONE)),$(filter 639,$(BIOS))),noki3310_3,\
	$(if $(and $(filter noki3330,$(PHONE)),$(filter 450e,$(BIOS))),noki3330_1,$(PHONE)))))
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
ORACLE_3310_IDLE_SHA ?= 5871dd93badb1fa410dd22a6b7a12cf2d3b8f938e1514e989858dd45a2b35b74
ORACLE_3310_MENU_SHA ?= e0890d021f0e11de1978f9ecbcfa0321191ac3741da1379f82337c715079851a
ORACLE_3310_PHONEBOOK_NAV_SHA ?= 06ea6abd47a1c603fc60382a2ba7e78d7a1247de2a13079374b48fd6796e793e
ORACLE_3310_ANSWERED_UI_SHA ?= a9330101aff6ef85f7bc8625794c707a3a7a45f9b426be0548b2af1a86dfb0f5
ORACLE_3330_IDLE_SHA ?= f40de8661baf671706ad626bb89a7e2aece9c391597318248ffd592c7cfd867d
ORACLE_3330_MESSAGES_SHA ?= 61d28951699e81a78dbafa8b094cc2690b53f41ec2f61bbb5599e0bb61d569a0
ORACLE_3410_IDLE_SHA ?= 14c1f25e86f21ea7b52909b37fb624fcc9940668f9df19831a1f64b16913fd87
ORACLE_3410_MESSAGES_SHA ?= a44445d8880ee46944e4692d3823ae25dd902882f1f38c51f64dcc27412a279e
ORACLE_3330_DSP_MISSING_SHA ?= 7e3ade861af1e0e47c76100c7a7c7f8c7719c1c497e02d1024ab91c1e55c1f8e
ORACLE_3410_DSP_MISSING_SHA ?= dd5322bd6175d71dfea6d222d0572eab6fa787f3e2321f52fc6a7acd08600252

# The acquired virgin NHM-6 PMM legitimately requests its stored 12345 phone
# code, then a time and date. These are physical keypad transactions through
# firmware editors; keeping the sequence named makes the lengthy first-boot
# precondition reviewable in every 3330 gate.
NOKI3330_FIRST_BOOT_KEYS := 1,2,3,4,5,enter,wait8000,1,2,0,0,enter,wait1200,0,1,0,1,2,0,0,2,wait600,enter
NOKI3330_FIRST_BOOT_INPUT := NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
	NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280

# NHM-2 v5.46 radio gates share one product, the physical End action needed to
# dismiss virgin-PMM setup, and the independently evidenced COBBA-GJP digital
# bus geometry. Individual targets still own their fixture, duration and
# semantic checker.
NOKI3410_RUN := PHONE=noki3410 BIOS=546e
NOKI3410_RADIO_INPUT := NOKIA_DCT3_POST_READY_KEYS=end \
	NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 \
	NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200
COBBA_GJP_PCM_CHECK_ARGS := --data-clock 1000000 --frame-clock 8000 \
	--frame-clocks 125 --sync-clocks 1 --word-clocks 16

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
RADIO_RESELECTION_SAME_LAC_ARGS := -cfg_directory ../fixtures/radio_reselection_same_lac
RADIO_RESELECTION_DIFFERENT_LAC_ARGS := -cfg_directory ../fixtures/radio_reselection_different_lac
RADIO_RESELECTION_LOSS_RECOVERY_ARGS := -cfg_directory ../fixtures/radio_reselection_loss_recovery
RADIO_RESELECTION_PAGING_ARGS := -cfg_directory ../fixtures/radio_reselection_paging
RADIO_AUTHENTICATION_ARGS := -cfg_directory ../fixtures/radio_authentication
RADIO_INCOMING_CALL_ARGS := -cfg_directory ../fixtures/radio_incoming_call
RADIO_INCOMING_CALL_ANSWERED_ARGS := -cfg_directory ../fixtures/radio_incoming_call_answered
RADIO_OUTGOING_CALL_ARGS := -cfg_directory ../fixtures/radio_outgoing_call
RADIO_OUTGOING_BUSY_ARGS := -cfg_directory ../fixtures/radio_outgoing_busy
RADIO_OUTGOING_NO_ANSWER_ARGS := -cfg_directory ../fixtures/radio_outgoing_no_answer
RADIO_OUTGOING_SERVICE_REJECT_ARGS := -cfg_directory ../fixtures/radio_outgoing_service_reject
RADIO_OUTGOING_DELAYED_BUSY_ARGS := -cfg_directory ../fixtures/radio_outgoing_delayed_busy
RADIO_OUTGOING_HOST_ADAPTER_ARGS := -cfg_directory ../fixtures/radio_outgoing_host_adapter
HOST_CALL_CONFIG_ARGS ?= $(RADIO_OUTGOING_HOST_ADAPTER_ARGS)
HOST_RELEASE_SECURITY_CHECK ?= :
HOST_TWO_CALL_SECURITY_CHECK ?= :
HOST_CALL_ADAPTER_PORT ?= 18080
HOST_CALL_ADAPTER_THROTTLE ?= -nothrottle
HOST_CALL_TERMINATION_PORT ?= 18081
HOST_CALL_ALERTING_TERMINATION_PORT ?= 18082
HOST_CALL_MEDIA_PORT ?= 18083
HOST_CALL_RECONNECT_PORT ?= 18084
HOST_CALL_TWO_CALLS_PORT ?= 18085
HOST_CALL_3410_PORT ?= 18086
HOST_CALL_3310_PORT ?= 18087
HOST_CALL_3330_PORT ?= 18088
HOST_CALL_PHYSICAL_MEDIA_PORT ?= 18089
NOKI3210_UNLOCK_KEYS := 1,2,3,4,5,enter
NOKI3210_DIAL_KEYS := c,wait500,c,wait1000,5,5,5,1,2,3,4,enter
NOKI3210_INCOMING_READY_KEYS := $(NOKI3210_UNLOCK_KEYS),wait500,waitbuzzer
NOKI3210_OUTGOING_DIAL_KEYS := $(NOKI3210_UNLOCK_KEYS),wait1000,$(NOKI3210_DIAL_KEYS)
NOKI3210_OUTGOING_NO_ANSWER_KEYS := $(NOKI3210_OUTGOING_DIAL_KEYS),waitalerting,wait3000,enter

# Direct WebSocket runners do not enter through `make run`. Give them the same
# generated-artifact and NVRAM preparation contract without rebuilding MAME.
define prepare_host_run
$(MAKE) --no-print-directory prepare-run-files \
	PHONE=$(2) BIOS=$(3) RUN_DIR="$(1)" \
	RUN_NVRAM_DIR="$(abspath $(1))/nvram" PRESERVE_NVRAM=0
endef

RADIO_PCM_MISSING_ARGS := -cfg_directory ../fixtures/radio_pcm_missing
RADIO_INCOMING_CALL_DEGRADED_ARGS := -cfg_directory ../fixtures/radio_incoming_call_degraded
RADIO_INCOMING_SMS_ARGS := -cfg_directory ../fixtures/radio_incoming_sms
RADIO_INCOMING_SMART_MESSAGE_ARGS := -cfg_directory ../fixtures/radio_incoming_smart_message
DSP_SERVICE_MISSING_ARGS := -cfg_directory ../fixtures/dsp_service_missing
NSE8_SMS_UNLOCK_KEYS := $(NOKI3210_UNLOCK_KEYS)
NSE8_SMS_READ_KEYS := $(NSE8_SMS_UNLOCK_KEYS),wait9000,enter,wait1200,enter
NSE8_SMS_DELETE_PROMPT_KEYS := $(NSE8_SMS_READ_KEYS),wait1200,enter,wait1200,enter
NSE8_SMS_DELETE_KEYS := $(NSE8_SMS_DELETE_PROMPT_KEYS),wait1200,enter
NSE8_SMS_COLD_READ_KEYS := $(NSE8_SMS_UNLOCK_KEYS),wait5000,enter,wait800,down,wait800,enter,wait800,down,wait800,enter,wait1200,enter,wait1200,enter

MAME_ARGS := $(PHONE) -rompath roms -log -video none -sound none \
	-keyboardprovider none -mouseprovider none -lightgunprovider none \
	-joystickprovider none -midiprovider none -skip_gameinfo -nothrottle \
	-autoboot_script ../mame_nokia_dct3_input_exerciser.lua $(if $(BIOS),-bios $(BIOS)) \
	$(if $(filter 1,$(RUN_VERBOSE)),-verbose)
INTERACTIVE_MAME_ARGS := $(PHONE) -rompath roms -window -resolution 672x384 \
	-keepaspect -skip_gameinfo $(if $(BIOS),-bios $(BIOS))
INTERACTIVE_NVRAM_DIR ?= $(abspath run_interactive/nvram)
INTERACTIVE_EXTRA_ARGS ?=

.PHONY: help venv download-mame overlay eeprom-profile normalize-3330 normalize-3410 roms build swap16 census controller-census ccont-static-census ccont-runtime-census mad2-census mad2-static-census dsp-census census-docs evidence-check test-tools prepare-run-files prepare-run-nvram run run-prebuilt run-captured run-prebuilt-captured run-frontier run-interactive smoke smoke-3310-639 smoke-3330e smoke-3210-v501 audit-roms audit-dsp-roms frame watch verify verify-ccont verify-ccont-watchdog verify-ccont-rtc verify-ccont-mask verify-alarm verify-power-lifecycle verify-power-lifecycle-v501 verify-charger-lifecycle verify-charger-wake verify-gensio verify-display verify-dsp-transport verify-dsp-memory-upload verify-dsp-speech-control-static verify-gsm-fr-codec verify-gsm-tch-f-l1 verify-gsm-a5 verify-gsm-xcch-l1 verify-gsm-mobility verify-gsm-sms-transport verify-radio-periodic-location-update verify-radio-a5-1-incoming-call verify-radio-a5-1-state verify-dsp-bootstrap-3310 verify-3310-radio-boundary verify-3330-radio-boundary verify-3310-radio-registration verify-3330-radio-registration verify-3330-radio-registration-preserved verify-3330-radio-registration-state verify-3330-radio-unsuitable-cells verify-3310-radio-paging verify-3330-radio-paging verify-3330-radio-paging-preserved verify-3330-radio-paging-state verify-3330-radio-paging-negatives verify-3310-radio-incoming-call-boundary verify-3310-radio-incoming-call-ui verify-3310-radio-incoming-call-lifecycle verify-3310-radio-media-resilience verify-3310-radio-physical-duplex verify-3310-frontier verify-3310-menu verify-3310-navigation verify-3330-frontier verify-3330-navigation verify-3410-frontier verify-3410-menu verify-3410-navigation verify-dsp-tone verify-radio-camp verify-radio-registration verify-radio-paging verify-radio-incoming-call verify-radio-incoming-ringing verify-radio-incoming-call-answered verify-radio-incoming-call-lifecycle verify-radio-incoming-call-lifecycle-v501 verify-radio-call-state-roundtrip verify-radio-pcm-missing verify-radio-degraded-speech verify-radio-physical-uplink verify-radio-physical-uplink-one verify-radio-incoming-sms verify-radio-incoming-smart-message verify-radio-operator verify-mad2 verify-mad2-interrupts verify-mad2-clocks verify-mad2-sleep verify-mad2-timer1 verify-mad2-reset verify-mbus verify-buzzer verify-3210-v501 verify-frontier verify-frontier-stability verify-mmi-menu verify-mmi-menu-501 verify-sim-phonebook verify-structure verify-structure-subset clean clean-build
.PHONY: verify-model-frontier-state verify-model-frontier-negative
.PHONY: verify-radio-periodic-location-update-state verify-3410-radio-periodic-location-update
.PHONY: verify-cobba-control verify-gsm-a3a8 verify-radio-authentication-boundary verify-3310-radio-authentication-boundary normalize-6110 normalize-6110-v548 verify-6110-static verify-6110-v548-static verify-6110-bootstrap-capture
.PHONY: verify-3330-radio-incoming-call-lifecycle
.PHONY: verify-3410-radio-incoming-call-lifecycle verify-3410-radio-a5-1-incoming-call
.PHONY: verify-3330-radio-media-resilience
.PHONY: verify-3410-radio-registration verify-3410-radio-registration-preserved
.PHONY: verify-3410-radio-registration-state verify-3410-radio-unsuitable-cells
.PHONY: verify-3410-radio-paging verify-3410-radio-paging-preserved
.PHONY: verify-3410-radio-paging-state verify-3410-radio-paging-negatives
.PHONY: verify-radio-outgoing-call-lifecycle verify-radio-outgoing-call-state
.PHONY: verify-radio-a5-1-outgoing-call
.PHONY: verify-radio-a5-1-sdcch-state
.PHONY: verify-3310-radio-a5-1-incoming-call verify-3330-radio-a5-1-incoming-call
.PHONY: verify-3310-radio-a5-1-outgoing-call verify-3330-radio-a5-1-outgoing-call
.PHONY: verify-3410-radio-a5-1-outgoing-call
.PHONY: verify-radio-outgoing-call-busy verify-radio-outgoing-call-no-answer
.PHONY: verify-radio-outgoing-call-service-reject
.PHONY: verify-radio-outgoing-call-no-answer-state
.PHONY: verify-radio-outgoing-call-delayed-decision-state
.PHONY: verify-radio-outgoing-call-host-adapter
.PHONY: verify-radio-outgoing-call-host-hostile
.PHONY: verify-radio-outgoing-call-host-local-end
.PHONY: verify-radio-outgoing-call-host-termination
.PHONY: verify-radio-outgoing-call-host-alerting-termination
.PHONY: verify-radio-outgoing-call-host-media
.PHONY: verify-radio-outgoing-call-host-reconnect
.PHONY: verify-radio-outgoing-call-host-alerting-reconnect
.PHONY: verify-radio-outgoing-call-host-media-restore
.PHONY: verify-radio-outgoing-call-host-release-restore
.PHONY: verify-radio-outgoing-call-host-two-calls
.PHONY: verify-radio-a5-1-host-release-restore
.PHONY: verify-radio-a5-1-host-two-calls
.PHONY: verify-radio-outgoing-call-host-physical-media
.PHONY: verify-radio-incoming-smart-message-state verify-radio-smart-message-application verify-radio-smart-message-application-state verify-radio-smart-message-invalid-rtpl verify-radio-smart-message-parser-quirks verify-radio-smart-message-envelopes verify-radio-smart-message-persistence verify-radio-smart-message-persistence-state verify-radio-smart-message-persistence-negatives verify-3310-radio-smart-message-application verify-3310-radio-smart-message-persistence verify-3410-radio-smart-message-application verify-3410-radio-smart-message-persistence
.PHONY: verify-radio-sms-inbox verify-radio-sms-inbox-state verify-radio-sms-inbox-negatives verify-radio-sms-sequential
.PHONY: verify-3410-radio-sms-inbox
.PHONY: verify-3310-radio-sms-inbox
.PHONY: verify-3330-radio-sms-transport
.PHONY: verify-3310-radio-incoming-smart-message
.PHONY: verify-3330-radio-incoming-smart-message
.PHONY: verify-3410-radio-incoming-smart-message
.PHONY: verify-3310-radio-outgoing-call-host-termination
.PHONY: verify-3330-radio-outgoing-call-host-termination
.PHONY: verify-3410-radio-outgoing-call-host-termination
.PHONY: verify-3410-radio-outgoing-call-host-media
.PHONY: verify-3310-radio-outgoing-call-lifecycle
.PHONY: verify-3330-radio-outgoing-call-lifecycle
.PHONY: verify-3410-radio-outgoing-call-lifecycle

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
	@echo "make verify-gsm-a3a8 verify the explicitly profiled laboratory A3/A8 example"
	@echo "make verify-radio-authentication-boundary reproduce organic 3210 authenticated registration"
	@echo "make verify-3310-radio-authentication-boundary reproduce organic 3310 authenticated registration"
	@echo "make verify-6110-static verify the identified NSE-3 v4.06 static boundary"
	@echo "make normalize-6110-v548 extract the evidenced NSE-3 v5.48 ROM3/ROM4 PPM-B images"
	@echo "make verify-6110-v548-static verify distinct NSE-3 v5.48 ROM3/ROM4 bootstrap boundaries"
	@echo "make verify-6110-bootstrap-capture validate a physical NSE-3 trace (CAPTURE=... RAW_TRACE=...)"
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
	@echo "make verify-power-lifecycle-v501 repeat the power gate on NSE-8 v5.01"
	@echo "make verify-charger-lifecycle check charger-present startup and power-key policy"
	@echo "make verify-charger-wake check powered-off charger restart and reset cause"
	@echo "make verify-gensio  check two-ROM endpoint and SELECT-register contracts"
	@echo "make verify-display check display-profile provenance and LCD serial transport"
	@echo "make verify-dsp-transport check DSPIF rings, completion and peer layering"
	@echo "make verify-cobba-control check opaque DSP-to-COBBA control transport"
	@echo "make verify-dsp-memory-upload check paired-ROM DSP-addressed image application"
	@echo "make verify-dsp-speech-control-static verify paired-ROM speech/channel fields"
	@echo "make verify-gsm-fr-codec check the standards-based 20 ms PCM/frame boundary"
	@echo "make verify-gsm-tch-f-l1 check GSM-FR channel coding/interleaving/bursts"
	@echo "make verify-dsp-bootstrap-3310 check the local v6.39 58-exchange bootstrap"
	@echo "make verify-3310-radio-boundary preserve the evidenced NHM-5 packet grammar"
	@echo "make verify-3330-radio-boundary preserve the evidenced NHM-6 DCS/SI frontier"
	@echo "make verify-3310-radio-registration check NHM-5 Location Updating and steady camp"
	@echo "make verify-3410-radio-registration check NHM-2 Location Updating and steady camp"
	@echo "make verify-3410-radio-registration-state check NHM-2 acquisition/SDCCH save-state replay"
	@echo "make verify-3410-radio-unsuitable-cells check NHM-2 cell and assignment rejection"
	@echo "make verify-3410-radio-paging check NHM-2 PCH access and Paging Response"
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
	@echo "make verify-3310-radio-paging check NHM-5 SI scheduling and organic Paging Response"
	@echo "make verify-3310-radio-incoming-call-boundary check NHM-5 through incoming CC SETUP"
	@echo "make verify-3310-radio-incoming-call-ui check NHM-5 ringing and physical Answer UI"
	@echo "make verify-3330-radio-incoming-call-lifecycle check NHM-6 through physical Answer/End"
	@echo "make verify-3410-radio-incoming-call-lifecycle check NHM-2 through physical Answer/End"
	@echo "make verify-3330-radio-media-resilience check NHM-6 media, degradation and save-state replay"
	@echo "make verify-radio-incoming-call check organic MT SETUP, Alerting and bounded clearing"
	@echo "make verify-radio-incoming-call-answered check physical Answer, ringing and post-answer DSP traffic"
	@echo "make verify-radio-incoming-call-lifecycle check physical Answer-to-End CC and DSP-control teardown"
	@echo "make verify-radio-outgoing-call-lifecycle check physical dial-to-End MO call and media"
	@echo "make verify-radio-outgoing-call-state check exact active MO-call save-state replay"
	@echo "make verify-radio-outgoing-call-busy check remote-busy CC release without TCH"
	@echo "make verify-radio-outgoing-call-no-answer check local End before network Connect"
	@echo "make verify-radio-outgoing-call-no-answer-state check alerting save-state replay"
	@echo "make verify-radio-outgoing-call-service-reject check rejected CM service and RR release"
	@echo "make verify-radio-outgoing-call-delayed-decision-state check queued decision across save/load"
	@echo "make verify-radio-outgoing-call-host-adapter check WebSocket request/decision routing"
	@echo "make verify-radio-outgoing-call-host-media check correlated timestamped GSM-FR transport"
	@echo "make verify-radio-outgoing-call-host-reconnect check reconnect/restore epoch resynchronisation"
	@echo "make verify-radio-outgoing-call-host-media-restore check restored media cursors"
	@echo "make verify-radio-outgoing-call-host-release-restore check in-release deterministic replay"
	@echo "make verify-gsm-a5 verify-gsm-xcch-l1 check generic A5 and ciphered xCCH"
	@echo "make verify-radio-a5-1-incoming-call check organic encrypted MT call and media"
	@echo "make verify-radio-a5-1-host-{release-restore,two-calls} check host/cipher isolation"
	@echo "make verify-radio-outgoing-call-host-physical-media check non-silent physical host loopback"
	@echo "make verify-{3310,3330,3410}-radio-outgoing-call-lifecycle check product MO calls"
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
	@echo "make clean          remove run state: gate output trees and stray MAME files"
	@echo "make clean-build    additionally remove the MAME object tree (forces a full rebuild)"
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

verify-gsm-a3a8:
	mkdir -p scratchpad
	$(CXX) -std=c++17 -O2 -Wall -Wextra -pedantic \
		driver/gsm_a3a8.cpp driver/gsm_mm_authentication.cpp \
		tools/test_gsm_a3a8.cpp \
		-o scratchpad/test_gsm_a3a8
	scratchpad/test_gsm_a3a8

verify-gsm-a5:
	mkdir -p scratchpad
	$(CXX) -std=c++17 -O2 -Wall -Wextra -pedantic \
		driver/gsm_a5.cpp tools/test_gsm_a5.cpp \
		-o scratchpad/test_gsm_a5
	scratchpad/test_gsm_a5

verify-gsm-xcch-l1:
	mkdir -p scratchpad
	$(CXX) -std=c++17 -O2 -Wall -Wextra -pedantic \
		driver/gsm_a5.cpp driver/gsm_tch_f_l1.cpp \
		driver/gsm_xcch_l1.cpp tools/test_gsm_xcch_l1.cpp \
		-o scratchpad/test_gsm_xcch_l1
	scratchpad/test_gsm_xcch_l1

verify-gsm-mobility:
	mkdir -p scratchpad
	$(CXX) -std=c++17 -O2 -Wall -Wextra -pedantic \
		tools/test_gsm_mobility.cpp \
		-o scratchpad/test_gsm_mobility
	scratchpad/test_gsm_mobility

verify-gsm-sms-transport:
	mkdir -p scratchpad
	$(CXX) -std=c++17 -O2 -Wall -Wextra -pedantic \
		driver/gsm_sms_transport.cpp tools/test_gsm_sms_transport.cpp \
		-o scratchpad/test_gsm_sms_transport
	scratchpad/test_gsm_sms_transport

verify-radio-periodic-location-update:
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR) SECONDS=450 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_periodic_location_update'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_periodic_location_update_trace_check.py \
		$(RUN_DIR)/error.log

verify-radio-periodic-location-update-state:
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR) SECONDS=450 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_periodic_location_update' \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=398 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=12000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_periodic_location_update_state_trace_check.py \
		$(RUN_DIR)/error.log

verify-3410-radio-periodic-location-update: normalize-3410
	@$(MAKE) --no-print-directory run-captured $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=450 RUN_VERBOSE=1 \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)' \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_periodic_location_update'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3410_periodic_location_update_trace_check.py \
		$(RUN_DIR)/error.log

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

NOKI6110_V406_DIR ?= /home/gaz/tmp/nokia6110_firmware/v4.06/NSE-3
NOKI6110_V548_DIR ?= /home/gaz/tmp/nse3-v548-review/pkg_548

normalize-6110:
	$(PYTHON) tools/extract_dct3_wintesla.py \
		--mcu "$(NOKI6110_V406_DIR)/NSE32514.060" \
		--ppm "$(NOKI6110_V406_DIR)/NSE32514.06B" \
		--flash-output roms/noki6110/6110_nse3_v406_rom3_candidate.fls \
		--expect-flash-sha1 5025a6ac3b4a13714211fde903f27f92cbb7c9b6

normalize-6110-v548:
	$(PYTHON) tools/extract_dct3_wintesla.py \
		--mcu "$(NOKI6110_V548_DIR)/nse3nx_5.480" \
		--ppm "$(NOKI6110_V548_DIR)/nse3nx_5.48b" \
		--flash-output roms/noki6110/6110_nse3_v548_rom3_ppmb.fls \
		--expect-flash-sha1 5768841c9eb39c744f4fa04f0485e4f9ad4553b3
	$(PYTHON) tools/extract_dct3_wintesla.py \
		--mcu "$(NOKI6110_V548_DIR)/nse3nx05.480" \
		--ppm "$(NOKI6110_V548_DIR)/nse3nx05.48b" \
		--flash-output roms/noki6110/6110_nse3_v548_rom4_ppmb.fls \
		--expect-flash-sha1 3bcc5c93ec247c63490e134196aab98a4e60c184

verify-6110-static:
	$(VENV)/bin/python tools/nse3_v406_static_check.py \
		roms/noki6110/6110_nse3_v406_rom3_candidate.fls \
		--nse8-reference roms/3210f600a_swap16.bin \
		--json run_census/nse3_v406_static_boundary.json

verify-6110-v548-static:
	$(VENV)/bin/python tools/nse3_v548_static_check.py \
		--rom3 roms/noki6110/6110_nse3_v548_rom3_ppmb.fls \
		--rom4 roms/noki6110/6110_nse3_v548_rom4_ppmb.fls \
		--json run_census/nse3_v548_static_boundary.json

verify-6110-bootstrap-capture:
	@test -n "$(CAPTURE)" || \
		{ echo "CAPTURE=<provenance JSON> is required" >&2; exit 2; }
	$(PYTHON) tools/nse3_bootstrap_capture_check.py "$(CAPTURE)" \
		$(if $(RAW_TRACE),--raw-trace "$(RAW_TRACE)")

verify-dct3-type-1f-static:
	$(VENV)/bin/python tools/dct3_type_1f_static_check.py \
		--3210 roms/noki3210/3210f600a.fls \
		--3310 roms/noki3310/3310f639e.fls \
		--6110 roms/noki6110/6110_nse3_v406_rom3_candidate.fls \
		--json run_census/dct3_type_1f_static_boundary.json

roms: $(if $(filter noki3210,$(PHONE)),eeprom-profile)
	@for src in roms/noki*/; do \
		[ -d "$$src" ] || continue; \
		dst="$(MAME_DIR)/roms/$$(basename "$$src")"; \
		mkdir -p "$$dst"; \
		cp -a "$$src". "$$dst"/; \
	done

build: overlay roms $(LIBGSM_ARCHIVE)
	$(MAKE) -C $(MAME_DIR) REGENIE=1 SOURCES=src/mame/nokia/nokia_dct3.cpp,src/mame/nokia/nokia_b3_flash.cpp,src/mame/nokia/nokia_ccont.cpp,src/mame/nokia/nokia_cobba.cpp,src/mame/nokia/nokia_dsp_hle.cpp,src/mame/nokia/nokia_dspif.cpp,src/mame/nokia/nokia_external_service.cpp,src/mame/nokia/nokia_gensio.cpp,src/mame/nokia/gsm_a3a8.cpp,src/mame/nokia/gsm_a5.cpp,src/mame/nokia/gsm_mm_authentication.cpp,src/mame/nokia/gsm_tch_f_l1.cpp,src/mame/nokia/gsm_xcch_l1.cpp,src/mame/nokia/nokia_gsm_fr_codec.cpp,src/mame/nokia/nokia_gsm_network.cpp,src/mame/nokia/nokia_gsm_session.cpp,src/mame/nokia/nokia_gsm_voice_peer.cpp,src/mame/nokia/nokia_lapdm_link.cpp,src/mame/nokia/nokia_kbgpio.cpp,src/mame/nokia/nokia_mad2.cpp,src/mame/nokia/nokia_mad2_pcm.cpp,src/mame/nokia/nokia_mbus.cpp,src/mame/nokia/nokia_pup.cpp,src/mame/nokia/nokia_radio_peer.cpp,src/mame/nokia/nokia_simi.cpp,src/mame/nokia/nokia_sim_card.cpp USE_QTDEBUG=0 LDFLAGS="-Wl,--whole-archive $(LIBGSM_ARCHIVE) -Wl,--no-whole-archive" -j$(JOBS)

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
	$(VENV)/bin/python -m unittest tools/test_extract_dct3_wintesla.py
	$(VENV)/bin/python -m unittest tools/test_nse3_v406_static_check.py
	$(VENV)/bin/python -m unittest tools/test_nse3_v548_static_check.py
	$(VENV)/bin/python -m unittest tools/test_dct3_type_1f_static_check.py
	$(VENV)/bin/python -m unittest tools/test_message_census.py tools/test_find_thumb_signature.py tools/test_make_eeprom_profile.py tools/test_mad2_access_census.py tools/test_mad2_static_census.py tools/test_ccont_static_census.py tools/test_ccont_runtime_census.py tools/test_ccont_mask_pending_check.py tools/test_sim_device_split.py tools/test_sim_phonebook_check.py tools/test_gsm_authentication_split.py tools/test_radio_authentication_boundary_trace_check.py tools/test_nse3_bootstrap_capture_check.py tools/test_b3_flash_device_split.py tools/test_mad2_device_split.py tools/test_mbus_device_split.py tools/test_dsp_device_split.py tools/test_dsp_rom_audit.py tools/test_dsp_upload_extract.py tools/test_dsp_memory_upload_trace_check.py tools/test_dsp_speech_control_static_check.py tools/test_speech_media_boundaries.py tools/test_gensio_device_split.py tools/test_kbgpio_device_split.py tools/test_pup_device_split.py tools/test_display_path.py tools/test_mame_patch_hygiene.py tools/test_mame_source_compliance.py tools/test_check_lcd_frame.py tools/test_keypad_input.py tools/test_machine_profile.py tools/test_model_frontier_summary.py tools/test_ccont_watchdog.py tools/test_ccont_watchdog_trace_check.py tools/test_ccont_watchdog_expiry_check.py tools/test_ccont_rtc_trace_check.py tools/test_alarm_trace_check.py tools/test_power_lifecycle_check.py tools/test_charger_lifecycle_check.py tools/test_charger_wake_check.py tools/test_display_trace_check.py tools/test_gensio_trace_check.py tools/test_mad2_timer_trace_check.py tools/test_mad2_timer1_trace_check.py tools/test_mad2_interrupt_trace_check.py tools/test_mad2_clock_trace_check.py tools/test_mad2_sleep_trace_check.py tools/test_mbus_trace_check.py tools/test_dsp_transport_trace_check.py tools/test_dsp_tone_trace_check.py tools/test_dsp_shared_read_census.py tools/test_dsp_shared_transition_census.py tools/test_dsp_packet_semantics_census.py tools/test_dsp_radio_profile_trace_check.py tools/test_radio_camp_trace_check.py tools/test_radio_3330_boundary_trace_check.py tools/test_radio_3330_unsuitable_cell_trace_check.py tools/test_radio_mobile_identity.py tools/test_radio_registration_trace_check.py tools/test_radio_registration_state_roundtrip_trace_check.py tools/test_radio_paging_trace_check.py tools/test_radio_paging_state_roundtrip_trace_check.py tools/test_radio_paging_negative_trace_check.py tools/test_radio_3310_incoming_call_boundary_check.py tools/test_radio_3310_speech_control_trace_check.py tools/test_radio_incoming_call_trace_check.py tools/test_radio_incoming_ringing_trace_check.py tools/test_radio_answered_call_trace_check.py tools/test_radio_answered_audio_boundary_trace_check.py tools/test_radio_answered_call_lifecycle_trace_check.py tools/test_radio_call_audio_wire_trace_check.py tools/test_radio_outgoing_call_trace_check.py tools/test_radio_speech_media_trace_check.py tools/test_radio_facch_interruption_trace_check.py tools/test_radio_sacch_coexistence_trace_check.py tools/test_radio_call_state_roundtrip_trace_check.py tools/test_radio_pcm_missing_trace_check.py tools/test_radio_degraded_speech_trace_check.py tools/test_radio_physical_uplink_trace_check.py tools/test_radio_incoming_sms_trace_check.py tools/test_radio_incoming_smart_message_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_smart_message_application_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_smart_message_envelope_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_smart_message_persistence_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_sms_inbox_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_sms_sequence_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_sms_product_inbox_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_sms_negative_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_sms_two_message_ui_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_3410_registration_negative_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_3330_incoming_call_boundary_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_call_lifecycle_common.py tools/test_gsm_session_call_invariants.py tools/test_gsm_call_adapter_split.py tools/test_acceptance_run_isolation.py tools/test_radio_outgoing_call_outcome_trace_check.py tools/test_radio_outgoing_host_adapter_trace_check.py tools/test_radio_outgoing_host_termination_trace_check.py tools/test_radio_outgoing_host_media_trace_check.py tools/test_radio_outgoing_host_reconnect_trace_check.py tools/test_radio_outgoing_host_two_calls_trace_check.py tools/test_radio_outgoing_host_hostile_trace_check.py tools/test_radio_outgoing_host_local_end_trace_check.py tools/test_radio_outgoing_host_media_restore_trace_check.py tools/test_radio_outgoing_host_release_restore_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_cobba_control_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_a5_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_a5_state_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_a5_two_calls_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_periodic_location_update_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_periodic_location_update_state_trace_check.py
	$(VENV)/bin/python -m unittest tools/test_radio_3410_periodic_location_update_trace_check.py tools/test_dsp_code_block_lifecycle.py
	$(VENV)/bin/python -m unittest tools/test_radio_physical_downlink_check.py
	$(VENV)/bin/python -m unittest tools/test_pulse_route_mame.py
	$(VENV)/bin/python -m unittest tools/test_gsm_sms_session_invariants.py

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

prepare-run-files:
	@mkdir -p "$(RUN_DIR)"
	@find "$(RUN_DIR)" -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' -delete
	@truncate -s 0 "$(MAME_DIR)/error.log"
	@if [ "$(PHONE)" = "noki3210" ]; then \
		mkdir -p "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)"; \
		if [ "$(PRESERVE_NVRAM)" != "1" ]; then \
			rm -f "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/sim_card"; \
		fi; \
		if [ "$(PRESERVE_NVRAM)" != "1" ] || [ ! -f "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/eeprom" ]; then \
			cp "$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)" \
				"$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/eeprom"; \
		fi; \
	elif { [ "$(PHONE)" = "noki3310" ] || [ "$(PHONE)" = "noki3330" ] || \
			[ "$(PHONE)" = "noki3410" ]; } && [ "$(PRESERVE_NVRAM)" != "1" ]; then \
		rm -f "$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/flash" \
			"$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/sim_card" \
			"$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/eeprom"; \
	fi

prepare-run-nvram: build prepare-run-files

run: prepare-run-nvram
	cd $(MAME_DIR) && env $(BOOT_ENV) $(RUN_ENV) NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		./mame $(MAME_ARGS) $(RUN_EXTRA_ARGS) -nvram_directory $(RUN_NVRAM_DIR) -seconds_to_run $(SECONDS)
	@$(MAKE) --no-print-directory frame RUN_DIR=$(RUN_DIR)

run-prebuilt: prepare-run-files
	cd $(MAME_DIR) && env $(BOOT_ENV) $(RUN_ENV) NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		./mame $(MAME_ARGS) $(RUN_EXTRA_ARGS) -nvram_directory $(RUN_NVRAM_DIR) -seconds_to_run $(SECONDS)
	@$(MAKE) --no-print-directory frame RUN_DIR=$(RUN_DIR)

run-captured: run
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log

run-prebuilt-captured: run-prebuilt
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log

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

verify-cobba-control:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_cobba_conformance SECONDS=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/cobba_conformance'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cobba_conformance/error.log
	$(PYTHON) tools/cobba_control_trace_check.py \
		$(RUN_DIR)_cobba_conformance/error.log

verify-dsp-bootstrap-3310:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=1 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/dsp_transport_trace_check.py $(RUN_DIR)/error.log \
		--completion-only --expected-bootstrap-exchanges 58
	@echo "OK — 3310 v6.39 completed its product-calibrated DSP bootstrap"

verify-3310-radio-boundary:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=7 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/dsp_radio_profile_trace_check.py $(RUN_DIR)/error.log \
		--profile nhm5-search --rom roms/noki3310/3310f639e.fls
	@echo "OK — 3310 v6.39 completed its evidenced serving-BCCH acquisition boundary"

verify-3330-radio-boundary: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR) SECONDS=12 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3330_boundary_trace_check.py $(RUN_DIR)/error.log

verify-3310-radio-registration:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=12 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py $(RUN_DIR)/error.log \
		--profile nhm5

verify-3330-radio-registration: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR) SECONDS=18 RUN_VERBOSE=1
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py $(RUN_DIR)/error.log \
		--profile nhm6

verify-3330-radio-registration-preserved:
	@$(MAKE) --no-print-directory verify-3330-radio-registration \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=18 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram)
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)_cold/error.log --profile nhm6 --preserved

verify-3330-radio-registration-state: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR) SECONDS=20 RUN_VERBOSE=1 \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=10.45 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_state_roundtrip_trace_check.py \
		$(RUN_DIR)/error.log --profile nhm6

verify-3330-radio-unsuitable-cells: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_barred SECONDS=18 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_cell_barred'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_barred/error.log
	$(PYTHON) tools/radio_3330_unsuitable_cell_trace_check.py \
		$(RUN_DIR)_barred/error.log --profile barred
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_rxlev SECONDS=18 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_cell_rxlev'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_rxlev/error.log
	$(PYTHON) tools/radio_3330_unsuitable_cell_trace_check.py \
		$(RUN_DIR)_rxlev/error.log --profile rxlev

verify-3410-radio-registration: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=30 RUN_VERBOSE=1 \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py $(RUN_DIR)/error.log \
		--profile nhm2

verify-3410-radio-registration-preserved:
	@$(MAKE) --no-print-directory verify-3410-radio-registration \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=30 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram) \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)_cold/error.log --profile nhm2 --preserved

verify-3410-radio-registration-state: normalize-3410
	@for boundary in pre assigned; do \
		if test "$$boundary" = pre; then at=10.070; replay=30; else at=10.092; replay=50; fi; \
		$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=30 RUN_VERBOSE=1 \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay $(NOKI3410_RADIO_INPUT)" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_registration_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log --profile nhm2 || exit; \
	done

verify-3410-radio-unsuitable-cells: normalize-3410
	@for profile in barred rxlev; do \
		fixture=radio_cell_$$profile; \
		$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
			RUN_DIR=$(RUN_DIR)_$$profile SECONDS=30 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/$$fixture" \
			RUN_ENV='$(NOKI3410_RADIO_INPUT)' || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$profile/error.log || exit; \
		$(PYTHON) tools/radio_3410_registration_negative_trace_check.py \
			$(RUN_DIR)_$$profile/error.log --profile $$profile || exit; \
	done
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_assignment SECONDS=30 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_assignment_mismatch' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_assignment/error.log
	$(PYTHON) tools/radio_3410_registration_negative_trace_check.py \
		$(RUN_DIR)_assignment/error.log --profile assignment

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
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 RUN_DIR=$(RUN_DIR)_return SECONDS=16 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,enter,wait700,enter,wait700,down,wait700,c,wait700,c,wait1800 NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
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
	@$(PYTHON) tools/check_model_frontier_summary.py $(RUN_DIR)/boot_summary.txt \
		--require-fiq0
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
	@$(PYTHON) tools/check_model_frontier_summary.py $(RUN_DIR)/boot_summary.txt \
		--require-fiq0
	@echo "OK — 3410 v5.46 compacted its virgin PMM and exposed the idle UI"

verify-3410-menu: normalize-3410
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR) \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR))/nvram SECONDS=22 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_disabled' \
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
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_disabled' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR)_menu -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z918_*' ! -name '*_ff918_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3410 menu frame produced in $(RUN_DIR)_menu"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3410_MESSAGES_SHA)
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e RUN_DIR=$(RUN_DIR)_return \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_return)/nvram SECONDS=24 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_disabled' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'
	@frame=$$(find $(RUN_DIR)_return -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		! -name '*_z918_*' ! -name '*_ff918_*' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no informative 3410 return frame produced in $(RUN_DIR)_return"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3410_IDLE_SHA)
	@grep -Fqx 'soft_resets=0' $(RUN_DIR)_menu/boot_summary.txt
	@grep -Fqx 'soft_resets=0' $(RUN_DIR)_return/boot_summary.txt
	@echo "OK — 3410 v5.46 opened Messages and returned to idle through physical keys"

verify-model-frontier-state: normalize-3330 normalize-3410
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_3330 SECONDS=8 \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=3'
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_3330/boot_summary.txt --require-state-roundtrip
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e \
		RUN_DIR=$(RUN_DIR)_3410 SECONDS=8 \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=3'
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_3410/boot_summary.txt --require-state-roundtrip
	@echo "OK — 3330/3410 DSP-frontier state survives isolated save/load replay"

verify-model-frontier-negative: normalize-3330 normalize-3410
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_3330 SECONDS=44 \
		RUN_EXTRA_ARGS='$(DSP_SERVICE_MISSING_ARGS)'
	@frame=$$(find $(RUN_DIR)_3330 -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no 3330 DSP-missing frame produced"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3330_DSP_MISSING_SHA)
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_3330/boot_summary.txt --reject-fiq0
	@$(MAKE) --no-print-directory run PHONE=noki3410 BIOS=546e \
		RUN_DIR=$(RUN_DIR)_3410 SECONDS=22 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_3410)/nvram \
		RUN_EXTRA_ARGS='$(DSP_SERVICE_MISSING_ARGS)'
	@frame=$$(find $(RUN_DIR)_3410 -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no 3410 DSP-missing frame produced"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" --sha256 $(ORACLE_3410_DSP_MISSING_SHA)
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_3410/boot_summary.txt --reject-fiq0
	@echo "OK — removing only the DSP service prevents both promoted frontiers"

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

.PHONY: verify-radio-reselection-same-lac verify-radio-reselection-different-lac \
	verify-radio-reselection-state verify-radio-loss-recovery \
	verify-radio-reselection-preserved \
	verify-radio-loss-recovery-state verify-radio-all-cell-loss \
	verify-radio-reselection-paging \
	verify-3310-radio-reselection-same-lac \
	verify-3310-radio-reselection-different-lac \
	verify-3310-radio-reselection-state \
	verify-3310-radio-reselection-preserved \
	verify-3310-radio-reselection-paging \
	verify-3330-radio-reselection-same-lac \
	verify-3330-radio-reselection-different-lac \
	verify-3330-radio-reselection-state \
	verify-3330-radio-reselection-preserved \
	verify-3330-radio-reselection-paging \
	verify-3410-radio-reselection-same-lac \
	verify-3410-radio-reselection-different-lac \
	verify-3410-radio-reselection-state \
	verify-3410-radio-reselection-preserved \
	verify-3410-radio-reselection-paging \
	verify-3410-radio-loss-recovery verify-3410-radio-loss-recovery-state \
	verify-3410-radio-all-cell-loss \
	verify-radio-reselection-unsuitable-neighbours

verify-radio-reselection-same-lac:
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_SAME_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nse8
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac

verify-radio-reselection-different-lac:
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR) SECONDS=70 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile different-lac

verify-radio-reselection-state:
	@for boundary in camped candidate location-update; do \
		if test "$$boundary" = camped; then at=18.00; replay=500; \
		elif test "$$boundary" = candidate; then at=23.20; replay=1000; \
		else at=24.20; replay=100; fi; \
		$(MAKE) --no-print-directory run PHONE=noki3210 \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=70 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_reselection_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log --profile different-lac || exit; \
	done

verify-radio-reselection-preserved:
	@$(MAKE) --no-print-directory verify-radio-reselection-different-lac \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=70 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_reselection_preserved_trace_check.py \
		$(RUN_DIR)_cold/error.log

verify-radio-loss-recovery:
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR) SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_LOSS_RECOVERY_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nse8
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile loss-recovery --radio-profile nse8

verify-radio-loss-recovery-state:
	@for boundary in degradation search; do \
		if test "$$boundary" = degradation; then at=20.95; replay=500; \
		else at=21.08; replay=150; fi; \
		$(MAKE) --no-print-directory run PHONE=noki3210 \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=45 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_RESELECTION_LOSS_RECOVERY_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_reselection_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log --profile loss-recovery \
			--radio-profile nse8 || exit; \
	done

verify-radio-all-cell-loss:
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_reselection_persistent_loss'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nse8
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile all-cell-loss --radio-profile nse8

verify-radio-reselection-unsuitable-neighbours: normalize-3410
	@for profile in barred rxlev malformed forbidden stale; do \
		$(MAKE) --no-print-directory run PHONE=noki3210 \
			RUN_DIR=$(RUN_DIR)_$$profile SECONDS=45 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/radio_reselection_neighbour_$$profile" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$profile/error.log || exit; \
		$(PYTHON) tools/radio_reselection_negative_trace_check.py \
			$(RUN_DIR)_$$profile/error.log --profile $$profile || exit; \
	done
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR)_unsupported_band SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_reselection_neighbour_unsupported_band'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_unsupported_band/error.log
	$(PYTHON) tools/radio_reselection_negative_trace_check.py \
		$(RUN_DIR)_unsupported_band/error.log --profile unsupported-band \
		--neighbour-arfcn 823
	@for profile in bsic access_class; do \
		check_profile=$$(echo $$profile | tr _ -); \
		$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
			RUN_DIR=$(RUN_DIR)_3410_$$profile SECONDS=50 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/radio_reselection_neighbour_$$profile" \
			RUN_ENV='$(NOKI3410_RADIO_INPUT)' || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_3410_$$profile/error.log || exit; \
		$(PYTHON) tools/radio_reselection_negative_trace_check.py \
			$(RUN_DIR)_3410_$$profile/error.log \
			--profile $$check_profile || exit; \
	done

verify-radio-reselection-paging:
	@$(MAKE) --no-print-directory run PHONE=noki3210 \
		RUN_DIR=$(RUN_DIR) SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac --paging-after-reselection
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3310-radio-reselection-same-lac:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_SAME_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nhm5
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac --radio-profile nhm5 \
		--serving-arfcn 88 --neighbour-arfcn 89

verify-3310-radio-reselection-different-lac:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=70 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile different-lac --radio-profile nhm5 \
		--serving-arfcn 88 --neighbour-arfcn 89

verify-3310-radio-reselection-state:
	@for boundary in candidate location-update; do \
		if test "$$boundary" = candidate; then at=17.25; replay=400; \
		else at=52.85; replay=300; fi; \
		$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=70 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_reselection_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log --profile different-lac \
			--radio-profile nhm5 --serving-arfcn 88 \
			--neighbour-arfcn 89 || exit; \
	done

verify-3310-radio-reselection-preserved:
	@$(MAKE) --no-print-directory verify-3310-radio-reselection-different-lac \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=70 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_reselection_preserved_trace_check.py \
		$(RUN_DIR)_cold/error.log --serving-arfcn 88 --neighbour-arfcn 89

verify-3310-radio-reselection-paging:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac --radio-profile nhm5 \
		--serving-arfcn 88 --neighbour-arfcn 89 \
		--paging-after-reselection
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3330-radio-reselection-same-lac: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_SAME_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nhm6
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac --radio-profile nhm6 \
		--serving-arfcn 823 --neighbour-arfcn 824

verify-3330-radio-reselection-different-lac: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile different-lac --radio-profile nhm6 \
		--serving-arfcn 823 --neighbour-arfcn 824

verify-3330-radio-reselection-state: normalize-3330
	@for boundary in candidate location-update; do \
		if test "$$boundary" = candidate; then at=19.10; replay=300; \
		else at=20.95; replay=250; fi; \
		$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=35 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_reselection_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log --profile different-lac \
			--radio-profile nhm6 --serving-arfcn 823 \
			--neighbour-arfcn 824 || exit; \
	done

verify-3330-radio-reselection-preserved: normalize-3330
	@$(MAKE) --no-print-directory verify-3330-radio-reselection-different-lac \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=35 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_reselection_preserved_trace_check.py \
		$(RUN_DIR)_cold/error.log --serving-arfcn 823 --neighbour-arfcn 824

verify-3330-radio-reselection-paging: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac --radio-profile nhm6 \
		--serving-arfcn 823 --neighbour-arfcn 824 \
		--paging-after-reselection
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3410-radio-reselection-same-lac: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_SAME_LAC_ARGS)' \
	RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nhm2
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac

verify-3410-radio-reselection-different-lac: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=90 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)' \
	RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile different-lac --radio-profile nhm2

verify-3410-radio-reselection-state: normalize-3410
	@for boundary in camped candidate location-update; do \
		if test "$$boundary" = camped; then at=11.50; replay=500; \
		elif test "$$boundary" = candidate; then at=19.90; replay=1000; \
		else at=54.62; replay=100; fi; \
		$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=90 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay $(NOKI3410_RADIO_INPUT)" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_reselection_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log --profile different-lac \
			--radio-profile nhm2 || exit; \
	done

verify-3410-radio-reselection-preserved: normalize-3410
	@$(MAKE) --no-print-directory verify-3410-radio-reselection-different-lac \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=90 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_DIFFERENT_LAC_ARGS)' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_reselection_preserved_trace_check.py \
		$(RUN_DIR)_cold/error.log --allow-pre-update-invalidation

verify-3410-radio-reselection-paging: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=50 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_PAGING_ARGS)' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile same-lac --radio-profile nhm2 \
		--paging-after-reselection
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3410-radio-loss-recovery: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_RESELECTION_LOSS_RECOVERY_ARGS)' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nhm2
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile loss-recovery --radio-profile nhm2

verify-3410-radio-loss-recovery-state: normalize-3410
	@for boundary in degradation sch; do \
		if test "$$boundary" = degradation; then at=16.70; replay=700; \
		else at=17.07; replay=150; fi; \
		$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=45 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_RESELECTION_LOSS_RECOVERY_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay $(NOKI3410_RADIO_INPUT)" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_reselection_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log --profile loss-recovery \
			--radio-profile nhm2 || exit; \
	done

verify-3410-radio-all-cell-loss: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_reselection_persistent_loss' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nhm2
	$(PYTHON) tools/radio_reselection_trace_check.py \
		$(RUN_DIR)/error.log --profile all-cell-loss --radio-profile nhm2

verify-radio-authentication-boundary:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=25 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_AUTHENTICATION_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_authentication_boundary_trace_check.py \
		$(RUN_DIR)/error.log

verify-3310-radio-authentication-boundary:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=22 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_AUTHENTICATION_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_authentication_boundary_trace_check.py \
		$(RUN_DIR)/error.log

verify-radio-paging:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=32 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3310-radio-paging:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=20 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3330-radio-paging: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR) SECONDS=28 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3330-radio-paging-preserved:
	@$(MAKE) --no-print-directory verify-3330-radio-paging \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=28 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)_cold/error.log --profile nhm6 --preserved
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)_cold/error.log

verify-3330-radio-paging-state: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_before SECONDS=30 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)' \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=11.80 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_before/error.log
	$(PYTHON) tools/radio_paging_state_roundtrip_trace_check.py \
		$(RUN_DIR)_before/error.log
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_assigned SECONDS=30 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)' \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=13.40 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=700'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_assigned/error.log
	$(PYTHON) tools/radio_paging_state_roundtrip_trace_check.py \
		$(RUN_DIR)_assigned/error.log

verify-3330-radio-paging-negatives: normalize-3330
	@for profile in wrong_group unmatched malformed; do \
		$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
			RUN_DIR=$(RUN_DIR)_$$profile SECONDS=24 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/radio_paging_$$profile"; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$profile/error.log; \
		check_profile=$$(echo $$profile | tr _ -); \
		$(PYTHON) tools/radio_paging_negative_trace_check.py \
			$(RUN_DIR)_$$profile/error.log --profile $$check_profile || exit 1; \
	done
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_unsuitable SECONDS=18 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_paging_unsuitable'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_unsuitable/error.log
	$(PYTHON) tools/radio_3330_unsuitable_cell_trace_check.py \
		$(RUN_DIR)_unsuitable/error.log --profile barred

verify-3410-radio-paging: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)/error.log --profile nhm2
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)/error.log

verify-3410-radio-paging-preserved:
	@$(MAKE) --no-print-directory verify-3410-radio-paging \
		RUN_DIR=$(RUN_DIR)_fresh
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=30 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_fresh/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_registration_trace_check.py \
		$(RUN_DIR)_cold/error.log --profile nhm2 --preserved
	$(PYTHON) tools/radio_paging_trace_check.py $(RUN_DIR)_cold/error.log

verify-3410-radio-paging-state: normalize-3410
	@for boundary in before assigned; do \
		if test "$$boundary" = before; then at=23.30; replay=500; else at=24.82; replay=100; fi; \
		$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
			RUN_DIR=$(RUN_DIR)_$$boundary SECONDS=35 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_PAGING_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay $(NOKI3410_RADIO_INPUT)" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_paging_state_roundtrip_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log || exit; \
	done

verify-3410-radio-paging-negatives: normalize-3410
	@for profile in wrong_group unmatched malformed; do \
		fixture=$$(echo $$profile | tr _ -); \
		$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
			RUN_DIR=$(RUN_DIR)_$$profile SECONDS=30 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/radio_paging_$$profile" \
			RUN_ENV='$(NOKI3410_RADIO_INPUT)' || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$profile/error.log || exit; \
		$(PYTHON) tools/radio_paging_negative_trace_check.py \
			$(RUN_DIR)_$$profile/error.log --profile $$fixture || exit; \
	done
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_unsuitable SECONDS=30 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_paging_unsuitable' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_unsuitable/error.log
	$(PYTHON) tools/radio_3410_registration_negative_trace_check.py \
		$(RUN_DIR)_unsuitable/error.log --profile barred

verify-3310-radio-incoming-call-boundary:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=16 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3310_incoming_call_boundary_check.py \
		$(RUN_DIR)/error.log

verify-3310-radio-incoming-call-ui:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=24 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=navi NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=2000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3310_incoming_call_boundary_check.py \
		$(RUN_DIR)/error.log --answered
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	test -n "$$frame" || { echo "no answered 3310 call frame produced in $(RUN_DIR)"; exit 1; }; \
	$(PYTHON) tools/check_lcd_frame.py "$$frame" \
		--sha256 $(ORACLE_3310_ANSWERED_UI_SHA)
	@echo "OK — NHM-5 completed Connect/Connect Ack and physical Answer changed the UI to End"

verify-3310-radio-incoming-call-lifecycle:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=28 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=navi,wait3000,navi NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=2000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3310_incoming_call_boundary_check.py \
		$(RUN_DIR)/error.log --ended
	$(PYTHON) tools/radio_3310_speech_control_trace_check.py \
		$(RUN_DIR)/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)
	@echo "OK — NHM-5 carried bidirectional GSM-FR media and returned call control to idle"

verify-3330-radio-incoming-call-lifecycle: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_provision SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_provision/boot_summary.txt --require-fiq0
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_call SECONDS=22 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_provision/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,c,wait500,c,waitalerting,enter,wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_3330_incoming_call_boundary_check.py \
		$(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)_call/error.log $(COBBA_GJP_PCM_CHECK_ARGS)
	@echo "OK — NHM-6 carried bidirectional GSM-FR media and returned call control to idle"

verify-3410-radio-incoming-call-lifecycle:
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_ANSWERED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end,waitalerting,send,wait3000,end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3410_incoming_call_lifecycle_check.py \
		$(RUN_DIR)/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)
	@echo "OK — NHM-2 carried internal GSM-FR media through physical Answer/End and returned to PCH"

verify-3410-radio-a5-1-incoming-call:
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_incoming_call_answered' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end,waitalerting,send,wait3000,end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3410_incoming_call_lifecycle_check.py \
		$(RUN_DIR)/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3330-radio-media-resilience: verify-3330-radio-incoming-call-lifecycle
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_degraded SECONDS=24 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_provision/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_DEGRADED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,c,wait500,c,waitalerting,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=13.0 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=2000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_degraded/error.log
	$(PYTHON) tools/radio_3330_incoming_call_boundary_check.py \
		$(RUN_DIR)_degraded/error.log
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)_degraded/error.log
	$(PYTHON) tools/radio_degraded_speech_trace_check.py \
		$(RUN_DIR)_degraded/error.log --data-clock 1000000 --frame-clock 8000 \
		--frame-clocks 125 --sync-clocks 1 --word-clocks 16
	$(PYTHON) tools/radio_facch_interruption_trace_check.py \
		$(RUN_DIR)_degraded/error.log
	$(PYTHON) tools/radio_sacch_coexistence_trace_check.py \
		$(RUN_DIR)_degraded/error.log
	@echo "OK — NHM-6 media survived FACCH, bidirectional BFIs and exact save-state replay"

verify-3310-radio-media-resilience:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=28 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_CALL_DEGRADED_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=navi NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=2000 NOKIA_DCT3_STATE_ROUNDTRIP_AT=19.0 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=2000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3310_incoming_call_boundary_check.py \
		$(RUN_DIR)/error.log --ended
	$(PYTHON) tools/radio_3310_speech_control_trace_check.py \
		$(RUN_DIR)/error.log
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)/error.log
	$(PYTHON) tools/radio_degraded_speech_trace_check.py \
		$(RUN_DIR)/error.log --data-clock 1000000 --frame-clock 8000 \
		--frame-clocks 125 --sync-clocks 1 --word-clocks 16
	$(PYTHON) tools/radio_facch_interruption_trace_check.py \
		$(RUN_DIR)/error.log
	$(PYTHON) tools/radio_sacch_coexistence_trace_check.py \
		$(RUN_DIR)/error.log
	@echo "OK — NHM-5 media survived FACCH, bidirectional BFIs and exact save-state replay while SACCH/TF slots remained reserved"

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
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
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
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-a5-1-incoming-call:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_incoming_call_answered' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-a5-1-state:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_incoming_call_answered' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=21.5 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=2000'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py \
		$(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-a5-1-sdcch-state:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=40 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_outgoing_call' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=24.16 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=50 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=5000'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_a5_state_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log

verify-radio-a5-1-outgoing-call:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_outgoing_call' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS),waitalerting,wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_call_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-outgoing-call-lifecycle:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_OUTGOING_CALL_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS),wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_call_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_speech_media_trace_check.py $(RUN_DIR)/error.log

verify-radio-outgoing-call-state:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_OUTGOING_CALL_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS),waitalerting NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=26.0 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=2000'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_call_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)/error.log

define verify_3210_outgoing_outcome
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=$(1) \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(2)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(3) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_call_outcome_trace_check.py \
		$(RUN_DIR)/error.log --outcome $(4)
endef

verify-radio-outgoing-call-busy:
	$(call verify_3210_outgoing_outcome,40,$(RADIO_OUTGOING_BUSY_ARGS),$(NOKI3210_OUTGOING_DIAL_KEYS),busy)

verify-radio-outgoing-call-no-answer:
	$(call verify_3210_outgoing_outcome,45,$(RADIO_OUTGOING_NO_ANSWER_ARGS),$(NOKI3210_OUTGOING_NO_ANSWER_KEYS),no-answer)

verify-radio-outgoing-call-no-answer-state:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_OUTGOING_NO_ANSWER_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS),waitalerting NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=26.0 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=2000'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_call_outcome_trace_check.py \
		$(RUN_DIR)/error.log --outcome no-answer --require-state-roundtrip

verify-radio-outgoing-call-service-reject:
	$(call verify_3210_outgoing_outcome,40,$(RADIO_OUTGOING_SERVICE_REJECT_ARGS),$(NOKI3210_OUTGOING_DIAL_KEYS),service-reject)

verify-radio-outgoing-call-delayed-decision-state:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_OUTGOING_DELAYED_BUSY_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=26.0 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000'; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_call_outcome_trace_check.py \
		$(RUN_DIR)/error.log --outcome busy --require-state-roundtrip \
		--require-deferred-decision

verify-radio-outgoing-call-host-adapter:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_ADAPTER_PORT) --cwd $(MAME_DIR) \
			$(HOST_CALL_ADAPTER_RUNNER_ARGS) -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			$(HOST_CALL_ADAPTER_THROTTLE) \
			-autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_ADAPTER_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_call_outcome_trace_check.py \
		$(RUN_DIR)/error.log --outcome busy; \
	$(PYTHON) tools/radio_outgoing_host_adapter_trace_check.py \
		$(RUN_DIR)/error.log; \
	if test -n "$(HOST_CALL_ADAPTER_EXTRA_CHECKER)"; then \
		$(PYTHON) $(HOST_CALL_ADAPTER_EXTRA_CHECKER) $(RUN_DIR)/error.log; \
	fi

verify-radio-outgoing-call-host-hostile:
	$(MAKE) --no-print-directory verify-radio-outgoing-call-host-adapter \
		JOBS=$(JOBS) RUN_DIR=$(RUN_DIR) \
		HOST_CALL_ADAPTER_RUNNER_ARGS=--hostile-pre-setup \
		HOST_CALL_ADAPTER_THROTTLE=-throttle \
		HOST_CALL_ADAPTER_EXTRA_CHECKER=tools/radio_outgoing_host_hostile_trace_check.py

verify-radio-outgoing-call-host-local-end:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS),wait5000,enter \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_local_end_gate.py \
			--port $(HOST_CALL_ADAPTER_PORT) --cwd $(MAME_DIR) -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-throttle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_ADAPTER_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 48; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_local_end_trace_check.py \
		$(RUN_DIR)/error.log

verify-radio-outgoing-call-host-termination:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_STATE_ROUNDTRIP_AT=24.6 \
		NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_TERMINATION_PORT) --cwd $(MAME_DIR) \
			--decision connect --terminate -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_TERMINATION_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_termination_trace_check.py \
		$(RUN_DIR)/error.log --require-state-roundtrip

verify-radio-outgoing-call-host-alerting-termination:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_ALERTING_TERMINATION_PORT) --cwd $(MAME_DIR) \
			--decision no_answer --terminate -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_ALERTING_TERMINATION_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_termination_trace_check.py \
		$(RUN_DIR)/error.log --phase alerting

verify-radio-outgoing-call-host-media:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_MEDIA_PORT) --cwd $(MAME_DIR) \
			--decision connect --media-frames 200 -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_MEDIA_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_media_trace_check.py \
		$(RUN_DIR)/error.log --frames 200

verify-radio-outgoing-call-host-reconnect:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_STATE_ROUNDTRIP_AT=28.0 \
		NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_reconnect_gate.py \
			--port $(HOST_CALL_RECONNECT_PORT) --cwd $(MAME_DIR) -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_RECONNECT_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_reconnect_trace_check.py \
		$(RUN_DIR)/error.log

verify-radio-outgoing-call-host-alerting-reconnect:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_reconnect_gate.py \
			--port $(HOST_CALL_ALERTING_TERMINATION_PORT) \
			--cwd $(MAME_DIR) --phase alerting -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_ALERTING_TERMINATION_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_reconnect_trace_check.py \
		$(RUN_DIR)/error.log --phase alerting

verify-radio-outgoing-call-host-media-restore:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_STATE_ROUNDTRIP_AT=28.0 \
		NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_media_restore_gate.py \
			--port $(HOST_CALL_RECONNECT_PORT) --cwd $(MAME_DIR) -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_RECONNECT_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 48; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_media_restore_trace_check.py \
		$(RUN_DIR)/error.log --frames 80

verify-radio-outgoing-call-host-release-restore:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS) \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_STATE_ROUNDTRIP_AT=0 \
		NOKIA_DCT3_STATE_ROUNDTRIP_WAIT_RELEASE=1 \
		NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_MEDIA_PORT) --cwd $(MAME_DIR) \
			--decision connect --media-frames 200 -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(HOST_CALL_CONFIG_ARGS) -http \
			-http_port $(HOST_CALL_MEDIA_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 48; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(HOST_RELEASE_SECURITY_CHECK); \
	$(PYTHON) tools/radio_outgoing_host_release_restore_trace_check.py \
		$(RUN_DIR)/error.log

verify-radio-a5-1-host-release-restore:
	@$(MAKE) --no-print-directory \
		verify-radio-outgoing-call-host-release-restore \
		RUN_DIR=$(RUN_DIR) JOBS=$(JOBS) \
		HOST_CALL_CONFIG_ARGS='-cfg_directory ../fixtures/radio_a5_1_host' \
		HOST_RELEASE_SECURITY_CHECK='$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log'

verify-radio-outgoing-call-host-two-calls:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS) \
		ERASED_IDENTITY_SECURITY_CODE=12345; \
	$(call prepare_host_run,$(RUN_DIR),noki3210,); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_OUTGOING_DIAL_KEYS),wait5000,c,wait500,c,wait1000,5,5,5,1,2,3,4,enter \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_two_calls_gate.py \
			--port $(HOST_CALL_TWO_CALLS_PORT) --cwd $(MAME_DIR) -- \
			./mame noki3210 -rompath roms -log -video none -sound none \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo \
			-nothrottle -autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(HOST_CALL_CONFIG_ARGS) -http \
			-http_port $(HOST_CALL_TWO_CALLS_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 55; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(HOST_TWO_CALL_SECURITY_CHECK); \
	$(PYTHON) tools/radio_outgoing_host_two_calls_trace_check.py \
		$(RUN_DIR)/error.log

verify-radio-a5-1-host-two-calls:
	@$(MAKE) --no-print-directory \
		verify-radio-outgoing-call-host-two-calls \
		RUN_DIR=$(RUN_DIR) JOBS=$(JOBS) \
		HOST_CALL_CONFIG_ARGS='-cfg_directory ../fixtures/radio_a5_1_host' \
		HOST_TWO_CALL_SECURITY_CHECK='$(PYTHON) tools/radio_a5_two_calls_trace_check.py $(RUN_DIR)/error.log'

verify-3410-radio-outgoing-call-host-termination:
	@set -e; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS); \
	$(call prepare_host_run,$(RUN_DIR),noki3410,546e); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=end,wait1000,5,5,5,1,2,3,4,send \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=120 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=240 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_3410_PORT) --cwd $(MAME_DIR) \
			--decision connect --terminate -- \
			./mame noki3410 -bios 546e -rompath roms -log \
			-video none -sound none -keyboardprovider none \
			-mouseprovider none -lightgunprovider none -joystickprovider none \
			-midiprovider none -skip_gameinfo -nothrottle \
			-autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_3410_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_termination_trace_check.py \
		$(RUN_DIR)/error.log

verify-3410-radio-outgoing-call-host-media:
	@set -e; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS); \
	$(call prepare_host_run,$(RUN_DIR),noki3410,546e); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=end,wait1000,5,5,5,1,2,3,4,send \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=120 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=240 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_3410_PORT) --cwd $(MAME_DIR) \
			--decision connect --media-frames 200 -- \
			./mame noki3410 -bios 546e -rompath roms -log \
			-video none -sound none -keyboardprovider none \
			-mouseprovider none -lightgunprovider none -joystickprovider none \
			-midiprovider none -skip_gameinfo -nothrottle \
			-autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_3410_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 48; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_media_trace_check.py \
		$(RUN_DIR)/error.log --frames 200

verify-3310-radio-outgoing-call-host-termination:
	@set -e; \
	$(MAKE) --no-print-directory build JOBS=$(JOBS); \
	$(call prepare_host_run,$(RUN_DIR),noki3310,639); \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=5,5,5,1,2,3,4,enter \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR))/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_3310_PORT) --cwd $(MAME_DIR) \
			--decision connect --terminate -- \
			./mame noki3310 -bios 639 -rompath roms -log \
			-video none -sound none -keyboardprovider none \
			-mouseprovider none -lightgunprovider none -joystickprovider none \
			-midiprovider none -skip_gameinfo -nothrottle \
			-autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_3310_PORT) \
			-nvram_directory $(abspath $(RUN_DIR))/nvram -seconds_to_run 45; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_outgoing_host_termination_trace_check.py \
		$(RUN_DIR)/error.log

verify-3330-radio-outgoing-call-host-termination: normalize-3330
	@set -e; \
	$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_provision SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'; \
	$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_provision/boot_summary.txt --require-fiq0; \
	$(MAKE) --no-print-directory prepare-run-files \
		PHONE=noki3330 BIOS=450e RUN_DIR="$(RUN_DIR)_call" \
		RUN_NVRAM_DIR="$(abspath $(RUN_DIR)_provision)/nvram" \
		PRESERVE_NVRAM=1; \
	env NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,c,wait500,c,wait500,5,5,5,1,2,3,4,enter \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 \
		NOKIA_DCT3_SNAPSHOT_DIR=$(abspath $(RUN_DIR)_call) \
		NOKIA_DCT3_BOOT_SUMMARY=$(abspath $(RUN_DIR)_call)/boot_summary.txt \
		$(PYTHON) tools/run_host_call_adapter_gate.py \
			--port $(HOST_CALL_3330_PORT) --cwd $(MAME_DIR) \
			--decision connect --terminate -- \
			./mame noki3330 -bios 450e -rompath roms -log \
			-video none -sound none -keyboardprovider none \
			-mouseprovider none -lightgunprovider none -joystickprovider none \
			-midiprovider none -skip_gameinfo -nothrottle \
			-autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			-verbose $(RADIO_OUTGOING_HOST_ADAPTER_ARGS) -http \
			-http_port $(HOST_CALL_3330_PORT) \
			-nvram_directory $(abspath $(RUN_DIR)_provision/nvram) \
			-seconds_to_run 32; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)_call/error.log; \
	$(PYTHON) tools/radio_outgoing_host_termination_trace_check.py \
		$(RUN_DIR)_call/error.log

verify-3310-radio-outgoing-call-lifecycle:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=42 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_OUTGOING_CALL_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=5,5,5,1,2,3,4,enter,waitalerting,wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_outgoing_call_trace_check.py $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3310-radio-a5-1-incoming-call:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=42 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_incoming_call_answered' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=navi,wait3000,navi NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_3310_incoming_call_boundary_check.py \
		$(RUN_DIR)/error.log --ended
	$(PYTHON) tools/radio_3310_speech_control_trace_check.py $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3310-radio-a5-1-outgoing-call:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=42 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_outgoing_call' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=5,5,5,1,2,3,4,enter,waitalerting,wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_outgoing_call_trace_check.py $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3330-radio-outgoing-call-lifecycle: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_provision SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_provision/boot_summary.txt --require-fiq0
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_call SECONDS=32 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_provision/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_OUTGOING_CALL_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,c,wait500,c,wait500,5,5,5,1,2,3,4,enter,waitalerting,wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_outgoing_call_trace_check.py \
		$(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)_call/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3330-radio-a5-1-incoming-call: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_provision SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_provision/boot_summary.txt --require-fiq0
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_call SECONDS=28 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_provision/nvram) \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_incoming_call_answered' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,c,wait500,c,wait500,waitalerting,enter,wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_3330_incoming_call_boundary_check.py \
		$(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)_call/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3330-radio-a5-1-outgoing-call: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_provision SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_provision/boot_summary.txt --require-fiq0
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_call SECONDS=32 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_provision/nvram) \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_outgoing_call' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,c,wait500,c,wait500,5,5,5,1,2,3,4,enter,waitalerting,wait5000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=70 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_outgoing_call_trace_check.py $(RUN_DIR)_call/error.log
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)_call/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3410-radio-outgoing-call-lifecycle:
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_OUTGOING_CALL_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end,wait1000,5,5,5,1,2,3,4,send,waitalerting,wait5000,end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=120 NOKIA_DCT3_POST_READY_KEY_GAP_MS=240'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_outgoing_call_trace_check.py \
		$(RUN_DIR)/error.log --release-complete optional
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

verify-3410-radio-a5-1-outgoing-call:
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_outgoing_call' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end,wait1000,5,5,5,1,2,3,4,send,waitalerting,wait5000,end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=120 NOKIA_DCT3_POST_READY_KEY_GAP_MS=240'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_outgoing_call_trace_check.py \
		$(RUN_DIR)/error.log --release-complete optional
	$(PYTHON) tools/radio_speech_media_trace_check.py \
		$(RUN_DIR)/error.log $(COBBA_GJP_PCM_CHECK_ARGS)

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
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
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
	replay_env='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=28.51 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=2000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=3000'; \
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
	pcm_missing_env='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280'; \
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
	degraded_env='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=28.51 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=2000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=3000'; \
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

verify-radio-a5-1-degraded:
	@set -e; \
	restore_default() { \
		$(MAKE) --no-print-directory eeprom-profile; \
		cp "roms/noki3210/$(EEPROM_BASENAME)" \
			"$(MAME_DIR)/roms/noki3210/$(EEPROM_BASENAME)"; \
	}; \
	trap restore_default EXIT; \
	degraded_env='NOKIA_DCT3_POST_READY_KEYS=$(NOKI3210_INCOMING_READY_KEYS),enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_STATE_ROUNDTRIP_AT=28.51 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=2000 NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=3000'; \
	$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_a5_1_incoming_call_degraded' \
		RUN_ENV="$$degraded_env"; \
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_a5_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_call_state_roundtrip_trace_check.py \
		$(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_answered_call_lifecycle_trace_check.py \
		$(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_degraded_speech_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_facch_interruption_trace_check.py $(RUN_DIR)/error.log; \
	$(PYTHON) tools/radio_sacch_coexistence_trace_check.py $(RUN_DIR)/error.log

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

verify-radio-outgoing-call-host-physical-media:
	HOST_MEDIA_PORT=$(HOST_CALL_PHYSICAL_MEDIA_PORT) \
		RUN_DIR=$(RUN_DIR) RUN_SECONDS=38 \
		POST_READY_KEYS='$(NOKI3210_OUTGOING_DIAL_KEYS)' \
		POST_READY_DELAY_MS=12000 POST_READY_DURATION_MS=220 \
		POST_READY_GAP_MS=280 \
		tools/run_physical_uplink_gate.sh

verify-3310-radio-physical-duplex:
	PHONE=noki3310 BIOS=639 ROM=roms/noki3310/3310f639e.fls \
		RUN_DIR=$(RUN_DIR) FIXTURE=fixtures/radio_incoming_call_answered \
		RUN_SECONDS=28 POST_READY_KEYS=navi,wait5000,navi \
		POST_READY_DELAY_MS=18000 POST_READY_DURATION_MS=200 \
		POST_READY_GAP_MS=200 \
		AUDIO_CONTROL_CHECKER=tools/radio_3310_speech_control_trace_check.py \
		PCM_CHECK_ARGS='--data-clock 1000000 --frame-clock 8000 --frame-clocks 125 --sync-clocks 1 --word-clocks 16' \
		tools/run_physical_uplink_gate.sh

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

verify-radio-sms-inbox: ERASED_IDENTITY_SECURITY_CODE=12345
verify-radio-sms-inbox: build
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_receive SECONDS=55 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_UNLOCK_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=20000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_inbox_trace_check.py \
		$(RUN_DIR)_receive/error.log $(RUN_DIR)_receive \
		$(RUN_DIR)_receive/nvram/noki3210/sim_card delivered
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_cold SECONDS=42 \
		RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_receive/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_COLD_READ_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=15000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_inbox_trace_check.py \
		$(RUN_DIR)_cold/error.log $(RUN_DIR)_cold \
		$(RUN_DIR)_receive/nvram/noki3210/sim_card cold
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_delete SECONDS=65 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_DELETE_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=20000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_inbox_trace_check.py \
		$(RUN_DIR)_delete/error.log $(RUN_DIR)_delete \
		$(RUN_DIR)_delete/nvram/noki3210/sim_card deleted

verify-radio-sms-inbox-state: ERASED_IDENTITY_SECURITY_CODE=12345
verify-radio-sms-inbox-state: build
	@set -e; \
	for boundary in storage before-rp after-rp notification viewing delete-confirmed deleted; do \
		case "$$boundary" in \
			storage) at=17.75; keys='' post_keys='' \
				outcome=delivered ;; \
			before-rp) at=18.20; keys='' post_keys='' \
				outcome=delivered ;; \
			after-rp) at=18.24; keys='' post_keys='' \
				outcome=delivered ;; \
			notification) at=30.0; keys='$(NSE8_SMS_UNLOCK_KEYS)' \
				post_keys='enter,wait1200,enter' \
				outcome=read ;; \
			viewing) at=40.0; keys='$(NSE8_SMS_READ_KEYS)' post_keys='' \
				outcome=read ;; \
			delete-confirmed) at=43.0; \
				keys='$(NSE8_SMS_DELETE_PROMPT_KEYS)' post_keys='enter' \
				outcome=deleted ;; \
			deleted) at=47.0; keys='$(NSE8_SMS_DELETE_KEYS)' post_keys='' \
				outcome=deleted ;; \
		esac; \
		out="$(RUN_DIR)_$$boundary"; \
		$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR="$$out" SECONDS=65 \
			RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
			RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)' \
			RUN_ENV="NOKIA_DCT3_POST_READY_KEYS=$$keys NOKIA_DCT3_POST_READY_KEY_DELAY_MS=20000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=500 NOKIA_DCT3_STATE_ROUNDTRIP_KEYS=$$post_keys NOKIA_DCT3_STATE_ROUNDTRIP_KEY_DELAY_MS=500" || exit; \
		$(PYTHON) tools/radio_sms_inbox_trace_check.py \
			"$$out/error.log" "$$out" \
			"$$out/nvram/noki3210/sim_card" "$$outcome-state" || exit; \
	done

verify-radio-sms-inbox-negatives: ERASED_IDENTITY_SECURITY_CODE=12345
verify-radio-sms-inbox-negatives: build
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_dismiss SECONDS=55 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_UNLOCK_KEYS),wait5000,c NOKIA_DCT3_POST_READY_KEY_DELAY_MS=20000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_inbox_trace_check.py \
		$(RUN_DIR)_dismiss/error.log $(RUN_DIR)_dismiss \
		$(RUN_DIR)_dismiss/nvram/noki3210/sim_card dismissed
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_cancel SECONDS=65 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_DELETE_PROMPT_KEYS),wait1200,c NOKIA_DCT3_POST_READY_KEY_DELAY_MS=20000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_inbox_trace_check.py \
		$(RUN_DIR)_cancel/error.log $(RUN_DIR)_cancel \
		$(RUN_DIR)_cancel/nvram/noki3210/sim_card cancelled
	@set -e; \
	for profile in bad_oa bad_dcs truncated bad_udl; do \
		out="$(RUN_DIR)_$$profile"; \
		$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR="$$out" SECONDS=40 \
			RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/radio_incoming_sms_$$profile" || exit; \
		$(PYTHON) tools/radio_sms_negative_trace_check.py \
			"$$out/error.log" "$$out/nvram/noki3210/sim_card" \
			malformed || exit; \
	done
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_duplicate SECONDS=45 \
		RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_incoming_sms_duplicate'
	$(PYTHON) tools/radio_sms_negative_trace_check.py \
		$(RUN_DIR)_duplicate/error.log \
		$(RUN_DIR)_duplicate/nvram/noki3210/sim_card duplicate
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_capacity SECONDS=90 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_incoming_sms_capacity' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_UNLOCK_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=60000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_negative_trace_check.py \
		$(RUN_DIR)_capacity/error.log \
		$(RUN_DIR)_capacity/nvram/noki3210/sim_card capacity \
		$(RUN_DIR)_capacity

verify-radio-sms-sequential: ERASED_IDENTITY_SECURITY_CODE=12345
verify-radio-sms-sequential: build
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_two SECONDS=45 \
		RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_incoming_sms_two'
	$(PYTHON) tools/radio_sms_sequence_trace_check.py \
		$(RUN_DIR)_two/error.log $(RUN_DIR)_two/nvram/noki3210/sim_card
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_two-state SECONDS=50 \
		RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_incoming_sms_two' \
		RUN_ENV='NOKIA_DCT3_STATE_ROUNDTRIP_AT=18.5 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=500'
	$(PYTHON) tools/radio_sms_sequence_trace_check.py \
		$(RUN_DIR)_two-state/error.log \
		$(RUN_DIR)_two-state/nvram/noki3210/sim_card state
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_two-notified SECONDS=55 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_incoming_sms_two' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_UNLOCK_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=25000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_two_message_ui_check.py \
		$(RUN_DIR)_two-notified/error.log $(RUN_DIR)_two-notified \
		$(RUN_DIR)_two-notified/nvram/noki3210/sim_card notified
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_two-read SECONDS=65 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_incoming_sms_two' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_UNLOCK_KEYS),wait5000,enter,wait1200,enter,wait1200,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=25000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_two_message_ui_check.py \
		$(RUN_DIR)_two-read/error.log $(RUN_DIR)_two-read \
		$(RUN_DIR)_two-read/nvram/noki3210/sim_card read-first
	@$(MAKE) --no-print-directory run-prebuilt-captured RUN_DIR=$(RUN_DIR)_two-delete SECONDS=70 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/radio_incoming_sms_two' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=$(NSE8_SMS_UNLOCK_KEYS),wait5000,enter,wait1200,enter,wait1200,enter,wait1200,enter,wait1200,enter,wait1200,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=25000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_two_message_ui_check.py \
		$(RUN_DIR)_two-delete/error.log $(RUN_DIR)_two-delete \
		$(RUN_DIR)_two-delete/nvram/noki3210/sim_card selective-delete

verify-3410-radio-sms-inbox: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_prepare SECONDS=35 \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	@$(MAKE) --no-print-directory run-prebuilt-captured $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_receive SECONDS=45 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_prepare/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)'
	$(PYTHON) tools/radio_sms_product_inbox_trace_check.py 3410 received \
		$(RUN_DIR)_receive/error.log $(RUN_DIR)_receive \
		$(RUN_DIR)_prepare/nvram/noki3410/sim_card
	@$(MAKE) --no-print-directory run-prebuilt-captured $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_read SECONDS=35 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_prepare/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait800,enter,wait800,down,wait800,enter,wait800,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=8000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=350 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_product_inbox_trace_check.py 3410 read \
		$(RUN_DIR)_read/error.log $(RUN_DIR)_read \
		$(RUN_DIR)_prepare/nvram/noki3410/sim_card
	@$(MAKE) --no-print-directory run-prebuilt-captured $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_delete SECONDS=40 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_prepare/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait800,enter,wait800,down,wait800,enter,wait800,enter,wait1000,enter,wait800,enter,wait800,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=8000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=350 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_product_inbox_trace_check.py 3410 deleted \
		$(RUN_DIR)_delete/error.log $(RUN_DIR)_delete \
		$(RUN_DIR)_prepare/nvram/noki3410/sim_card

verify-3310-radio-sms-inbox:
	@$(MAKE) --no-print-directory run-captured PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR)_receive SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)'
	$(PYTHON) tools/radio_sms_product_inbox_trace_check.py 3310 received \
		$(RUN_DIR)_receive/error.log $(RUN_DIR)_receive \
		$(RUN_DIR)_receive/nvram/noki3310_3/sim_card
	@$(MAKE) --no-print-directory run-prebuilt-captured PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR)_cold-read SECONDS=50 RUN_VERBOSE=1 \
		PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_receive/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,0,0,enter,wait1000,0,1,0,1,2,0,2,6,enter,wait5000,enter,wait1000,2,wait500,2,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=350 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_product_inbox_trace_check.py 3310 read \
		$(RUN_DIR)_cold-read/error.log $(RUN_DIR)_cold-read \
		$(RUN_DIR)_receive/nvram/noki3310_3/sim_card
	@$(MAKE) --no-print-directory run-prebuilt-captured PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR)_delete SECONDS=55 RUN_VERBOSE=1 \
		PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_receive/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,0,0,enter,wait1000,0,1,0,1,2,0,2,6,enter,wait5000,enter,wait1000,2,wait500,2,wait1000,enter,wait1000,enter,wait1000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=350 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	$(PYTHON) tools/radio_sms_product_inbox_trace_check.py 3310 deleted \
		$(RUN_DIR)_delete/error.log $(RUN_DIR)_delete \
		$(RUN_DIR)_receive/nvram/noki3310_3/sim_card

verify-3330-radio-sms-transport: normalize-3330
	@$(MAKE) --no-print-directory run-captured PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_provision SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'
	@$(MAKE) --no-print-directory run-prebuilt-captured PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_receive SECONDS=40 RUN_VERBOSE=1 \
		PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_provision/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMS_ARGS)'
	$(PYTHON) tools/radio_sms_product_inbox_trace_check.py 3330 transport \
		$(RUN_DIR)_receive/error.log $(RUN_DIR)_receive \
		$(RUN_DIR)_provision/nvram/noki3330_1/sim_card

verify-radio-incoming-smart-message:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=40 \
		RUN_VERBOSE=1 RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_incoming_smart_message_trace_check.py \
		$(RUN_DIR)/error.log $(RUN_DIR)/nvram/noki3210/sim_card

verify-radio-incoming-smart-message-state:
	@for boundary in first_close queued second_close; do \
		case "$$boundary" in \
			first_close) at=18.285; replay=500 ;; \
			queued) at=19.0; replay=500 ;; \
			second_close) at=22.18; replay=300 ;; \
		esac; \
		$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_$$boundary \
			SECONDS=40 RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=$$replay NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS=1000" || exit; \
		cp $(MAME_DIR)/error.log $(RUN_DIR)_$$boundary/error.log || exit; \
		$(PYTHON) tools/radio_incoming_smart_message_state_trace_check.py \
			$(RUN_DIR)_$$boundary/error.log \
			$(RUN_DIR)_$$boundary/nvram/noki3210/sim_card || exit; \
	done

verify-radio-smart-message-application:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=50 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	$(PYTHON) tools/radio_smart_message_application_trace_check.py \
		$(RUN_DIR)/error.log "$$frame" accepted

verify-radio-smart-message-application-state:
	@set -e; \
	for boundary in retained predispatch; do \
		case "$$boundary" in \
			retained) at=19.0 ;; \
			predispatch) at=21.75 ;; \
		esac; \
		out="$(RUN_DIR)_$$boundary"; \
		$(MAKE) --no-print-directory run RUN_DIR="$$out" SECONDS=40 \
			RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
			RUN_ENV="NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=500" || exit; \
		cp $(MAME_DIR)/error.log "$$out/error.log" || exit; \
		$(PYTHON) tools/radio_incoming_smart_message_state_trace_check.py \
			"$$out/error.log" "$$out/nvram/noki3210/sim_card" || exit; \
		$(PYTHON) tools/radio_smart_message_envelope_trace_check.py \
			"$$out/error.log" "$$out/nvram/noki3210/sim_card" valid || exit; \
	done
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_accepted SECONDS=50 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_AT=35 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=500'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_accepted/error.log
	@frame=$$(find $(RUN_DIR)_accepted -maxdepth 1 \
		-name 'nokia_dct3_lcdmirror_*.pgm' -printf '%T@ %p\n' | \
		sort -n | tail -1 | cut -d' ' -f2-); \
	$(PYTHON) tools/radio_smart_message_application_trace_check.py \
		$(RUN_DIR)_accepted/error.log "$$frame" accepted-state
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_rejected SECONDS=45 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/smart_message_invalid_rtpl' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_AT=30 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=500'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_rejected/error.log
	@frame=$$(find $(RUN_DIR)_rejected -maxdepth 1 \
		-name 'nokia_dct3_lcdmirror_*.pgm' -printf '%T@ %p\n' | \
		sort -n | tail -1 | cut -d' ' -f2-); \
	$(PYTHON) tools/radio_smart_message_application_trace_check.py \
		$(RUN_DIR)_rejected/error.log "$$frame" rejected-state

verify-radio-smart-message-invalid-rtpl:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR) SECONDS=45 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='-cfg_directory ../fixtures/smart_message_invalid_rtpl' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	$(PYTHON) tools/radio_smart_message_application_trace_check.py \
		$(RUN_DIR)/error.log "$$frame" rejected

verify-radio-smart-message-parser-quirks:
	@set -e; \
	for case in \
		duplicate:smart_message_duplicate \
		missing-terminator:smart_message_missing_terminator; do \
		name=$${case%%:*}; fixture=$${case#*:}; out="$(RUN_DIR)_$$name"; \
		$(MAKE) --no-print-directory run RUN_DIR="$$out" SECONDS=45 \
			RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/$$fixture" \
			RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000' || exit; \
		cp $(MAME_DIR)/error.log "$$out/error.log" || exit; \
		frame=$$(find "$$out" -maxdepth 1 \
			-name 'nokia_dct3_lcdmirror_*.pgm' -printf '%T@ %p\n' | \
			sort -n | tail -1 | cut -d' ' -f2-); \
		outcome=accepted-no-terminator; \
		test "$$name" != duplicate || outcome=accepted-duplicate; \
		$(PYTHON) tools/radio_smart_message_application_trace_check.py \
			"$$out/error.log" "$$frame" "$$outcome" || exit; \
	done

verify-radio-smart-message-envelopes:
	@set -e; \
	for case in \
		missing:smart_message_missing \
		bad-reference:smart_message_bad_reference \
		bad-total:smart_message_bad_total \
		wrong-port:smart_message_wrong_port \
		reversed:smart_message_reversed \
		duplicate:smart_message_duplicate \
		truncated-udh:smart_message_truncated_udh \
		stale-then-valid:smart_message_stale_then_valid; do \
		profile=$${case%%:*}; fixture=$${case#*:}; \
		out="$(RUN_DIR)_$$profile"; \
		$(MAKE) --no-print-directory run RUN_DIR="$$out" SECONDS=40 \
			RUN_VERBOSE=1 \
			RUN_EXTRA_ARGS="-cfg_directory ../fixtures/$$fixture" || exit; \
		cp $(MAME_DIR)/error.log "$$out/error.log" || exit; \
		$(PYTHON) tools/radio_smart_message_envelope_trace_check.py \
			"$$out/error.log" "$$out/nvram/noki3210/sim_card" \
			"$$profile" || exit; \
	done

verify-radio-smart-message-persistence:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_save SECONDS=52 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,down,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_save/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py \
		$(RUN_DIR)_save/error.log $(RUN_DIR)_save \
		$(RUN_DIR)_save/nvram/noki3210/eeprom \
		$(RUN_DIR)_save/nvram/noki3210/flash roms/3210f600a.fls saved
	$(PYTHON) tools/radio_smart_message_envelope_trace_check.py \
		$(RUN_DIR)_save/error.log \
		$(RUN_DIR)_save/nvram/noki3210/sim_card valid
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_cold SECONDS=42 \
		RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_save/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,9,wait500,2,wait1000,up,up,up,up,up,up,up,up NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=180 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py \
		$(RUN_DIR)_cold/error.log $(RUN_DIR)_cold \
		$(RUN_DIR)_save/nvram/noki3210/eeprom \
		$(RUN_DIR)_save/nvram/noki3210/flash roms/3210f600a.fls cold
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_discard SECONDS=50 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,down,down,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_discard/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py \
		$(RUN_DIR)_discard/error.log $(RUN_DIR)_discard \
		$(RUN_DIR)_discard/nvram/noki3210/eeprom \
		$(RUN_DIR)_discard/nvram/noki3210/flash roms/3210f600a.fls discarded
	$(PYTHON) tools/radio_smart_message_envelope_trace_check.py \
		$(RUN_DIR)_discard/error.log \
		$(RUN_DIR)_discard/nvram/noki3210/sim_card valid

verify-radio-smart-message-persistence-state:
	@set -e; \
	for boundary in before-save after-save; do \
		case "$$boundary" in \
			before-save) at=26.0 ;; \
			after-save) at=30.0 ;; \
		esac; \
		out="$(RUN_DIR)_$$boundary"; \
		$(MAKE) --no-print-directory run RUN_DIR="$$out" SECONDS=52 \
			RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
			RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
			RUN_ENV="NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,down,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000 NOKIA_DCT3_STATE_ROUNDTRIP_AT=$$at NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=500" || exit; \
		cp $(MAME_DIR)/error.log "$$out/error.log" || exit; \
		$(PYTHON) tools/radio_smart_message_persistence_trace_check.py \
			"$$out/error.log" "$$out" \
			"$$out/nvram/noki3210/eeprom" \
			"$$out/nvram/noki3210/flash" roms/3210f600a.fls \
			saved-state || exit; \
	done

verify-radio-smart-message-persistence-negatives:
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_cancel SECONDS=50 \
		RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,c NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cancel/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py \
		$(RUN_DIR)_cancel/error.log $(RUN_DIR)_cancel \
		$(RUN_DIR)_cancel/nvram/noki3210/eeprom \
		$(RUN_DIR)_cancel/nvram/noki3210/flash roms/3210f600a.fls cancelled
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_duplicate-first \
		SECONDS=52 RUN_VERBOSE=1 ERASED_IDENTITY_SECURITY_CODE=12345 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,down,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_duplicate-second \
		SECONDS=52 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_duplicate-first/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait9000,enter,wait1000,down,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_duplicate-second/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py \
		$(RUN_DIR)_duplicate-second/error.log \
		$(RUN_DIR)_duplicate-second \
		$(RUN_DIR)_duplicate-first/nvram/noki3210/eeprom \
		$(RUN_DIR)_duplicate-first/nvram/noki3210/flash \
		roms/3210f600a.fls saved

verify-3410-radio-smart-message-persistence: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_reference SECONDS=52 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_save SECONDS=52 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end,wait5000,enter,wait1200,down,wait1200,enter,wait3000,enter,wait3000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=240 NOKIA_DCT3_POST_READY_KEY_GAP_MS=350 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_save/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py nhm2 \
		$(RUN_DIR)_save/error.log $(RUN_DIR)_save \
		$(RUN_DIR)_save/nvram/noki3410/flash \
		$(RUN_DIR)_reference/nvram/noki3410/flash \
		$(RUN_DIR)_save/nvram/noki3410/eeprom \
		$(RUN_DIR)_reference/nvram/noki3410/eeprom \
		$(RUN_DIR)_save/nvram/noki3410/sim_card \
		$(RUN_DIR)_reference/nvram/noki3410/sim_card saved
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=55 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_save/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end,wait5000,enter,wait1000,3,wait1000,enter,wait1000,down,wait1000,enter,wait1000,enter,wait1000,up,up,up,up,up,up,up,up,up,up,up,up,up,up,up,up,up,up,up NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=350 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py nhm2 \
		$(RUN_DIR)_cold/error.log $(RUN_DIR)_cold \
		$(RUN_DIR)_save/nvram/noki3410/flash \
		$(RUN_DIR)_reference/nvram/noki3410/flash \
		$(RUN_DIR)_save/nvram/noki3410/eeprom \
		$(RUN_DIR)_reference/nvram/noki3410/eeprom \
		$(RUN_DIR)_save/nvram/noki3410/sim_card \
		$(RUN_DIR)_reference/nvram/noki3410/sim_card cold

verify-3310-radio-smart-message-persistence:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR)_reference SECONDS=58 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)'
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR)_save SECONDS=58 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,down,wait1000,enter,wait1000,enter,wait1000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=300 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_save/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py nhm5 \
		$(RUN_DIR)_save/error.log $(RUN_DIR)_save \
		$(RUN_DIR)_save/nvram/noki3310_3/flash \
		$(RUN_DIR)_reference/nvram/noki3310_3/flash \
		$(RUN_DIR)_save/nvram/noki3310_3/eeprom \
		$(RUN_DIR)_reference/nvram/noki3310_3/eeprom \
		$(RUN_DIR)_save/nvram/noki3310_3/sim_card \
		$(RUN_DIR)_reference/nvram/noki3310_3/sim_card saved
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR)_cold SECONDS=48 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_save/nvram) \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=1,2,0,0,enter,wait1000,0,1,0,1,2,0,2,6,enter,wait5000,enter,wait1000,5,wait1000,enter,wait1000,up,up,up,up,up,up,up,up NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=350 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_cold/error.log
	$(PYTHON) tools/radio_smart_message_persistence_trace_check.py nhm5 \
		$(RUN_DIR)_cold/error.log $(RUN_DIR)_cold \
		$(RUN_DIR)_save/nvram/noki3310_3/flash \
		$(RUN_DIR)_reference/nvram/noki3310_3/flash \
		$(RUN_DIR)_save/nvram/noki3310_3/eeprom \
		$(RUN_DIR)_reference/nvram/noki3310_3/eeprom \
		$(RUN_DIR)_save/nvram/noki3310_3/sim_card \
		$(RUN_DIR)_reference/nvram/noki3310_3/sim_card cold

verify-3410-radio-smart-message-application: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=end,wait5000,enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=16000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	$(PYTHON) tools/radio_smart_message_application_trace_check.py \
		$(RUN_DIR)/error.log "$$frame" accepted-3410

verify-3310-radio-smart-message-application:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=18000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	@frame=$$(find $(RUN_DIR) -maxdepth 1 -name 'nokia_dct3_lcdmirror_*.pgm' \
		-printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-); \
	$(PYTHON) tools/radio_smart_message_application_trace_check.py \
		$(RUN_DIR)/error.log "$$frame" accepted-3310

verify-3310-radio-incoming-smart-message:
	@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \
		RUN_DIR=$(RUN_DIR) SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_incoming_smart_message_trace_check.py \
		$(RUN_DIR)/error.log $(RUN_DIR)/nvram/noki3310_3/sim_card

verify-3330-radio-incoming-smart-message: normalize-3330
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_provision SECONDS=44 \
		RUN_ENV='$(NOKI3330_FIRST_BOOT_INPUT) NOKIA_DCT3_POST_READY_KEYS=$(NOKI3330_FIRST_BOOT_KEYS) NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=7000'
	@$(PYTHON) tools/check_model_frontier_summary.py \
		$(RUN_DIR)_provision/boot_summary.txt --require-fiq0
	@$(MAKE) --no-print-directory run PHONE=noki3330 BIOS=450e \
		RUN_DIR=$(RUN_DIR)_sms SECONDS=35 RUN_VERBOSE=1 PRESERVE_NVRAM=1 \
		RUN_NVRAM_DIR=$(abspath $(RUN_DIR)_provision/nvram) \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_sms/error.log
	$(PYTHON) tools/radio_incoming_smart_message_trace_check.py \
		$(RUN_DIR)_sms/error.log \
		$(RUN_DIR)_provision/nvram/noki3330_1/sim_card

verify-3410-radio-incoming-smart-message: normalize-3410
	@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \
		RUN_DIR=$(RUN_DIR) SECONDS=45 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS='$(RADIO_INCOMING_SMART_MESSAGE_ARGS)' \
		RUN_ENV='$(NOKI3410_RADIO_INPUT)'
	cp $(MAME_DIR)/error.log $(RUN_DIR)/error.log
	$(PYTHON) tools/radio_incoming_smart_message_trace_check.py \
		$(RUN_DIR)/error.log $(RUN_DIR)/nvram/noki3410/sim_card

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
		RUN_VERBOSE=1 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=power NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=250 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1500'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_power_short/error.log
	$(PYTHON) tools/power_lifecycle_check.py short \
		$(RUN_DIR)_power_short/boot_summary.txt \
		--log $(RUN_DIR)_power_short/error.log
	@$(MAKE) --no-print-directory run RUN_DIR=$(RUN_DIR)_power_long SECONDS=22 \
		RUN_VERBOSE=1 \
		RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=power NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=2000 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1500 NOKIA_DCT3_STATE_ROUNDTRIP_AT=10.0 NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS=1000'
	cp $(MAME_DIR)/error.log $(RUN_DIR)_power_long/error.log
	grep -Fqx 'state_roundtrip=pass' \
		$(RUN_DIR)_power_long/boot_summary.txt
	$(PYTHON) tools/power_lifecycle_check.py long \
		$(RUN_DIR)_power_long/boot_summary.txt \
		--log $(RUN_DIR)_power_long/error.log
	@echo "OK — physical power-key short/long firmware lifecycles reproduced"

verify-power-lifecycle-v501:
	@$(MAKE) --no-print-directory verify-power-lifecycle \
		PHONE=noki3210 BIOS=501 \
		ROM=roms/nokia_3210_nse-8_v05_01_full_hu.fls \
		EEPROM_BASENAME='3210 v501 eeprom.bin' \
		RUN_DIR=$(RUN_DIR)_v501 JOBS=$(JOBS)

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

# Run state is anything a gate regenerates: the per-gate output trees plus the
# files MAME writes beside them when it is started from the repository root.
# Both separators are covered; ad-hoc run directories have used either.
CLEAN_RUN_STATE := $(sort $(RUN_DIR) run run_* run-* \
	error.log progress_latest_frame.* nokia_dct3_lcdmirror_*.pgm \
	cfg nvram snap)

clean:
	rm -rf $(CLEAN_RUN_STATE)

# Removing the object tree forces a full MAME rebuild, so keep it out of the
# ordinary run-state clean.
clean-build:
	rm -rf $(MAME_DIR)/obj

mad2-static-census:
	@mkdir -p run_census
	$(VENV)/bin/python tools/mad2_static_census.py --check \
		--json run_census/mad2_static_access.json --markdown docs/mad2_static_access.md
