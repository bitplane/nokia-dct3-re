// license:BSD-3-Clause
// copyright-holders:Gaz

// Private ROM/vector inputs belong to firmware integration, not ISA fixtures.

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace {

using c54x_test::tms320c54x_test_state;

ROM_START(tms54rom4)
	ROM_SYSTEM_BIOS(0, "entry", "Transform-entry snapshot")
	ROM_SYSTEM_BIOS(1, "cold", "Cold reset from mask ROM and DROM")
	ROM_SYSTEM_BIOS(2, "crc-helper", "Isolated acquired ROM carry/XOR helper")
	ROM_SYSTEM_BIOS(3, "crc-packed", "Isolated acquired ROM packed CRC")
	ROM_SYSTEM_BIOS(4, "fr-packing", "Isolated acquired ROM FR parameter round trip")
	ROM_SYSTEM_BIOS(5, "fr-decoder", "Isolated acquired ROM FR decoder")
	ROM_SYSTEM_BIOS(6, "fr-encoder", "Isolated acquired ROM FR encoder")
	ROM_SYSTEM_BIOS(7, "efr-crc", "Isolated acquired ROM EFR CRC")
	ROM_SYSTEM_BIOS(8, "efr-packing", "Isolated acquired ROM EFR packing and error detection")
	ROM_SYSTEM_BIOS(9, "efr-decoder", "Isolated acquired ROM EFR speech decoder")
	ROM_SYSTEM_BIOS(10, "efr-encoder", "Isolated acquired ROM EFR speech encoder")
	ROM_SYSTEM_BIOS(11, "efr-dtx-decoder", "Isolated acquired ROM EFR silence and speech decoder")
	ROM_SYSTEM_BIOS(12, "efr-dtx-encoder", "Isolated acquired ROM EFR silence and speech encoder")
	ROM_SYSTEM_BIOS(13, "efr-erasure", "Isolated acquired ROM EFR active-speech erasure decoder")
	ROM_SYSTEM_BIOS(14, "efr-homing", "Isolated acquired ROM EFR decoder homing and speech restart")
	ROM_SYSTEM_BIOS(15, "efr-enc-homing", "Isolated acquired ROM EFR encoder homing and speech restart")
	ROM_SYSTEM_BIOS(16, "efr-loss-observe", "Characterize native EFR loss differences without conformance promotion")
	ROM_SYSTEM_BIOS(17, "efr-invalid-sid", "Isolated acquired ROM EFR invalid SID and bad SID decoder")
	ROM_SYSTEM_BIOS(18, "efr-home-errors", "Characterize original EFR homing/error reset and PCM differences")
	ROM_SYSTEM_BIOS(19, "cold-pipeline", "Cold mask ROM execution with the staged CPU frontend")
	ROM_SYSTEM_BIOS(20, "fr-decoder-pipe", "Isolated acquired ROM FR decoder on staged CPU")
	ROM_SYSTEM_BIOS(21, "fr-encoder-pipe", "Isolated acquired ROM FR encoder on staged CPU")
	ROM_SYSTEM_BIOS(22, "efr-encoder-pipe", "Isolated acquired ROM EFR encoder on staged CPU")
	ROM_SYSTEM_BIOS(23, "efr-decoder-pipe", "Isolated acquired ROM EFR decoder on staged CPU")
	ROM_SYSTEM_BIOS(24, "efr-dtx-enc-pipe", "Isolated acquired ROM EFR DTX encoder on staged CPU")
	ROM_SYSTEM_BIOS(25, "efr-dtx-dec-pipe", "Isolated acquired ROM EFR DTX decoder on staged CPU")
	ROM_SYSTEM_BIOS(26, "efr-ehome-pipe", "Isolated acquired ROM EFR encoder homing on staged CPU")
	ROM_SYSTEM_BIOS(27, "efr-homing-pipe", "Isolated acquired ROM EFR decoder homing on staged CPU")
	ROM_SYSTEM_BIOS(28, "efr-isid-pipe", "Isolated acquired ROM EFR invalid SID decoder on staged CPU")
	ROM_SYSTEM_BIOS(29, "efr-erasure-pipe", "Strict acquired ROM EFR erasure comparison on staged CPU")
	ROM_SYSTEM_BIOS(30, "efr-loss-pipe", "Characterize staged EFR loss differences without conformance promotion")
	ROM_SYSTEM_BIOS(31, "efr-herr-pipe", "Characterize staged EFR homing error reset and PCM differences")
	ROM_SYSTEM_BIOS(32, "rf-bitmap", "Original RF search bitmap decoder basis and replay")
	ROM_SYSTEM_BIOS(33, "rf-bitmap-pipe", "Original RF search bitmap decoder on staged CPU")
	ROM_SYSTEM_BIOS(34, "rf-pairs", "Original adjacent RF pair products and TC history policy")
	ROM_SYSTEM_BIOS(35, "rf-pairs-pipe", "Original adjacent RF pair products on staged CPU")
	ROM_SYSTEM_BIOS(36, "efr-lost-sid", "Original EFR lost-SID muting and speech resumption")
	ROM_SYSTEM_BIOS(37, "efr-lsid-pipe", "Original EFR lost-SID muting on staged CPU")
	ROM_SYSTEM_BIOS(38, "efr-lsid-observe", "Characterize complete EFR lost-SID recovery without conformance promotion")
	ROM_SYSTEM_BIOS(39, "efr-lso-pipe", "Characterize complete EFR lost-SID recovery on staged CPU")
	ROM_REGION16_LE(0x80000, "dspprg", 0)
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(38))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(39))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(36))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(37))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(34))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(35))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(32))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(33))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(31))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(29))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(30))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(28))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(24))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(25))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(26))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(27))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(23))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(22))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(21))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(20))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(19))
	ROMX_LOAD("transform_entry_prog.bin", 0, 0x80000,
			CRC(99757118) SHA1(0a1da67d21f4c333acd331271c3d9f08e896008f),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(0))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(1))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(2))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(3))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(4))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(5))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(6))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(7))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(8))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(9))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(10))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(11))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(12))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(13))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(14))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(15))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(16))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(17))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(18))
	ROM_REGION16_LE(0x20000, "dspdata", 0)
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(38))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(39))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(36))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(37))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(34))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(35))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(32))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(33))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(31))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(29))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(30))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(28))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(24))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(25))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(26))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(27))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(23))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(22))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(21))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(20))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(19))
	ROMX_LOAD("transform_entry_data.bin", 0, 0x20000,
			CRC(bef92101) SHA1(1c547eb7fd457d95cdf7956462e80a40dbadb46e),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(0))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(1))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(2))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(3))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(4))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(5))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(6))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(7))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(8))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(9))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(10))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(11))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(12))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(13))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(14))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(15))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(16))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(17))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(18))
	ROM_REGION(24206, "efr_homing_error_input", 0)
	ROMX_LOAD("homing_error.dec", 0, 24206, CRC(f04d25bf) SHA1(e232a2cbc161fbacb185758001677c1e462bf0ca), ROM_BIOS(18))
	ROMX_LOAD("homing_error.dec", 0, 24206, CRC(f04d25bf) SHA1(e232a2cbc161fbacb185758001677c1e462bf0ca), ROM_BIOS(31))
	ROM_REGION(15680, "efr_homing_error_pcm", 0)
	ROMX_LOAD("homing_error.out", 0, 15680, CRC(f1f822a8) SHA1(fbdf3d13761497e798baadc21b5dd8ac327ca593), ROM_BIOS(18))
	ROMX_LOAD("homing_error.out", 0, 15680, CRC(f1f822a8) SHA1(fbdf3d13761497e798baadc21b5dd8ac327ca593), ROM_BIOS(31))
	ROM_REGION(255892, "efr_lost_sid_input", 0)
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(38))
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(39))
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(36))
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(37))
	ROM_REGION(165760, "efr_lost_sid_pcm", 0)
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(38))
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(39))
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(36))
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(37))
	ROM_REGION(255892, "efr_invalid_sid_input", 0)
	ROMX_LOAD("invalid_sid.dec", 0, 255892, CRC(1f747e55) SHA1(a26dfa2a8406406114ffb47092d98c5fdd78e758), ROM_BIOS(17))
	ROMX_LOAD("invalid_sid.dec", 0, 255892, CRC(1f747e55) SHA1(a26dfa2a8406406114ffb47092d98c5fdd78e758), ROM_BIOS(28))
	ROM_REGION(165760, "efr_invalid_sid_pcm", 0)
	ROMX_LOAD("invalid_sid.out", 0, 165760, CRC(e9d2e42a) SHA1(ccd15ebb216d99c0ff7bd9a54f250f816d52b779), ROM_BIOS(17))
	ROMX_LOAD("invalid_sid.out", 0, 165760, CRC(e9d2e42a) SHA1(ccd15ebb216d99c0ff7bd9a54f250f816d52b779), ROM_BIOS(28))
	ROM_REGION(14720, "efr_homing_encoder_pcm", 0)
	ROMX_LOAD("homing_enc.inp", 0, 14720, CRC(b1c8352b) SHA1(2aef5f790d2fad7ee3acd529bfca4656b44d9165), ROM_BIOS(15))
	ROMX_LOAD("homing_enc.inp", 0, 14720, CRC(b1c8352b) SHA1(2aef5f790d2fad7ee3acd529bfca4656b44d9165), ROM_BIOS(26))
	ROM_REGION(22632, "efr_homing_encoder_serial", 0)
	ROMX_LOAD("homing_enc.cod", 0, 22632, CRC(7457992e) SHA1(10aee87b5da40ac216c96306fa7ad7616e2cceda), ROM_BIOS(15))
	ROMX_LOAD("homing_enc.cod", 0, 22632, CRC(7457992e) SHA1(10aee87b5da40ac216c96306fa7ad7616e2cceda), ROM_BIOS(26))
	ROM_REGION(22724, "efr_homing_input", 0)
	ROMX_LOAD("homing.dec", 0, 22724, CRC(7935ce74) SHA1(8d28790270a6897f04f9ac4bf19d8c5c0aeaf170), ROM_BIOS(14))
	ROMX_LOAD("homing.dec", 0, 22724, CRC(7935ce74) SHA1(8d28790270a6897f04f9ac4bf19d8c5c0aeaf170), ROM_BIOS(27))
	ROM_REGION(14720, "efr_homing_pcm", 0)
	ROMX_LOAD("homing.out", 0, 14720, CRC(45ad424d) SHA1(d006cdaa2cbfb6b65c1c7954ed22442491ef990a), ROM_BIOS(14))
	ROMX_LOAD("homing.out", 0, 14720, CRC(45ad424d) SHA1(d006cdaa2cbfb6b65c1c7954ed22442491ef990a), ROM_BIOS(27))
	ROM_REGION(80522, "efr_erasure_input", 0)
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(13))
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(16))
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(29))
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(30))
	ROM_REGION(52160, "efr_erasure_pcm", 0)
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(13))
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(16))
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(29))
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(30))
	ROM_REGION(165760, "efr_dtx_encoder_pcm", 0)
	ROMX_LOAD("dtx.inp", 0, 165760, CRC(4e0886dc) SHA1(d9b80f83f09556f430f03deb17c207b4aefe782e), ROM_BIOS(12))
	ROMX_LOAD("dtx.inp", 0, 165760, CRC(4e0886dc) SHA1(d9b80f83f09556f430f03deb17c207b4aefe782e), ROM_BIOS(24))
	ROM_REGION(254856, "efr_dtx_encoder_serial", 0)
	ROMX_LOAD("dtx.cod", 0, 254856, CRC(6b2a679f) SHA1(00c8972a74dd4e06843058d7a1899e0d5d957bdc), ROM_BIOS(12))
	ROMX_LOAD("dtx.cod", 0, 254856, CRC(6b2a679f) SHA1(00c8972a74dd4e06843058d7a1899e0d5d957bdc), ROM_BIOS(24))
	ROM_REGION(255892, "efr_dtx_input", 0)
	ROMX_LOAD("dtx.dec", 0, 255892, CRC(8bb21b82) SHA1(464b9f354aa4cd2828c3f80e812b18af170f7624), ROM_BIOS(11))
	ROMX_LOAD("dtx.dec", 0, 255892, CRC(8bb21b82) SHA1(464b9f354aa4cd2828c3f80e812b18af170f7624), ROM_BIOS(25))
	ROM_REGION(165760, "efr_dtx_pcm", 0)
	ROMX_LOAD("dtx.out", 0, 165760, CRC(a2bc82da) SHA1(44ea0a4b98623d3282f8bdafb0a068287a879f47), ROM_BIOS(11))
	ROMX_LOAD("dtx.out", 0, 165760, CRC(a2bc82da) SHA1(44ea0a4b98623d3282f8bdafb0a068287a879f47), ROM_BIOS(25))
	ROM_REGION(52352, "efr_encoder_pcm", 0)
	ROMX_LOAD("spch_dos.inp", 0, 52352, CRC(36fb2ba9) SHA1(3d4abd66d2d033de929175f1d84e970f78007628), ROM_BIOS(10))
	ROMX_LOAD("spch_dos.inp", 0, 52352, CRC(36fb2ba9) SHA1(3d4abd66d2d033de929175f1d84e970f78007628), ROM_BIOS(22))
	ROM_REGION(80196, "efr_encoder_serial", 0)
	ROMX_LOAD("spch_dos.cod", 0, 80196, CRC(f4d761a3) SHA1(49bd709881e15e018632567bedabf6ed3b94c479), ROM_BIOS(10))
	ROMX_LOAD("spch_dos.cod", 0, 80196, CRC(f4d761a3) SHA1(49bd709881e15e018632567bedabf6ed3b94c479), ROM_BIOS(22))
	ROM_REGION(80522, "efr_reference_input", 0)
	ROMX_LOAD("spch_dos.dec", 0, 80522, CRC(cdaa4997) SHA1(e93d62943336e4327e7bab5905dee73a58b7ce4b), ROM_BIOS(9))
	ROMX_LOAD("spch_dos.dec", 0, 80522, CRC(cdaa4997) SHA1(e93d62943336e4327e7bab5905dee73a58b7ce4b), ROM_BIOS(23))
	ROM_REGION(52160, "efr_reference_pcm", 0)
	ROMX_LOAD("spch_dos.out", 0, 52160, CRC(bbfe268c) SHA1(d738d872bbb4bcf84bc68031166a8981003eea94), ROM_BIOS(9))
	ROMX_LOAD("spch_dos.out", 0, 52160, CRC(bbfe268c) SHA1(d738d872bbb4bcf84bc68031166a8981003eea94), ROM_BIOS(23))
	ROM_REGION16_LE(0x20000, "dspdrom", 0)
	ROM_LOAD16_WORD_SWAP("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d))
ROM_END

} // anonymous namespace

SYST(2026, tms54rom4, 0, 0, rom4, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x ROM4 private execution fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
