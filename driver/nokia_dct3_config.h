// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_DCT3_CONFIG_H
#define MAME_NOKIA_NOKIA_DCT3_CONFIG_H

#include <array>
#include <optional>
#include "nokia_cobba.h"
#include "nokia_dsp_hle.h"
#include "nokia_external_service.h"
#include "nokia_gensio.h"
#include "nokia_kbgpio.h"
#include "nokia_mad2.h"
#include "nokia_mad2_pcm.h"
#include "nokia_radio_peer.h"

namespace nokia_dct3 {

struct nokia_ccont_board_profile
{
	std::array<u16, 8> channel_defaults;
	u8 charger_voltage_channel;
	u16 charger_connected_raw = 0x03ff;
};

inline constexpr nokia_ccont_board_profile ADC_DEFAULT = {
	{ 0x000, 0x3ff, 0x3ff, 0x280, 0x200, 0x000, 0x200, 0x000 },
	5, 0x03ff
};

// NSE-6's schematic connects BVOLT directly to CCONT VBAT. The original
// manual gives a nominal 3.6 V pack; CCONT's documented typical VBAT scale
// is 6.5 mV/bit (NSE-8/9 Table 30), hence round(3600 / 6.5) = 554.
// Nominal silicon-transfer hypothesis, not measured NSE-6 calibration.
// All other channels retain research defaults; no donor NV is supplied.
inline constexpr nokia_ccont_board_profile ADC_NSE6 = {
	{ 0x000, 0x3ff, 554, 0x280, 0x200, 0x000, 0x200, 0x000 },
	5, 0x03ff
};

// NPM-5 MA4 A3-6: BSI is pulled to 2.8 V VBB through 150 kohm;
// General Information specifies BLB-2 (68 kohm). CCONT BSI conversion
// uses 1.5 V VREF, not VBB: 1023 * 2.8 * 68 / (1.5 * 218) ~= 595.
// BTEMP's 100 kohm VREF pull-up and 47 kohm/25 C NTC give 327.
// Nominal divider inputs, not measured NPM-5 calibration. Other channels
// retain research defaults until their physical transfer is recovered.
inline constexpr nokia_ccont_board_profile ADC_NPM5 = {
	{ 0x000, 0x3ff, 0x3ff, 595, 327, 0x000, 0x200, 0x000 },
	5, 0x03ff
};

// NSE-8 repurposes the conventional DCT3 ADC pins. The service manual gives
// typical uncalibrated transfer points for Vdc_out, Vchout, BTEMP and VCHAR.
// Selectors 0/1 retain the firmware-proven safe battery samples. Selector 0 is
// the documented RSSI/Vb input; selector 1 is a second firmware battery path
// whose physical mux remains unresolved.
inline constexpr u16 NSE8_VDC_OUT_3V3 = 508; // 3.3 V at 6.5 mV/bit
inline constexpr u16 NSE8_VCHOUT_2V7 = 575;  // manual's uncalibrated 2.7 V point
inline constexpr u16 NSE8_BTEMP_25C = 327;   // 47 kohm NTC at 25 C
inline constexpr u16 NSE8_VCHAR_8V4 = 521;   // manual's uncalibrated 8.4 V point
inline constexpr nokia_ccont_board_profile ADC_3210 = {
	{ 0x2c0, 0x2c0, NSE8_VDC_OUT_3V3, NSE8_VCHOUT_2V7,
		NSE8_BTEMP_25C, 0x000, 0x200, 0x000 },
	5, NSE8_VCHAR_8V4
};
// Standard 3310 routing: channel 2 is VBATT, 3 is the BMC-3 pack's BSI
// resistor and 4 is battery temperature. This tuple clears the firmware's
// ordinary pack/self-test path; it is product input, not a state fixture.
inline constexpr nokia_ccont_board_profile ADC_STANDARD = {
	{ 0x000, 0x3ff, 0x220, 0x026, 0x200, 0x000, 0x200, 0x000 },
	5, 0x03ff
};

// NSE-1 uses the standard CCONT channel placement with a BMC-3-class pack.
// These nominal raw inputs are calibration values, not synthesized firmware
// state; the firmware retains all pack, temperature and voltage decisions.
inline constexpr nokia_ccont_board_profile ADC_5110 = {
	{ 0x000, 0x3ff, 0x2c0, 0x150, 0x140, 0x000, 0x200, 0x000 },
	5, 0x03ff
};

// NSM-5 uses the standard channel placement with a BLB-2 Li-ion pack.  Its
// recovered recognition windows are BSI 0x13e..0x172 and BTEMP 0x133..0x157;
// use their nominal midpoints and an ordinary charged-pack VBATT sample.
inline constexpr nokia_ccont_board_profile ADC_5210 = {
	{ 0x000, 0x3ff, 0x2c0, 0x150, 0x140, 0x000, 0x200, 0x000 },
	5, 0x03ff
};

// NAM-2 retains the historical raw BSI fixture 0x0b6. The service material's
// 0.5 V test point does not establish this value: 2.8 V is VBB/pull-up supply,
// not a recovered NAM-2 ADC reference. Do not call this factory calibration.
// Full-scale selector 2 is rejected into the firmware's BOOT UP CHARGE / WAIT
// CHARGER VOLTAGE lifecycle. Use the ordinary charged-pack sample shared by
// the independently grounded DCT3 profiles until NAM-2 scaling is recovered.
inline constexpr nokia_ccont_board_profile ADC_2100 = {
	{ 0x000, 0x3ff, 0x2c0, 0x0b6, 0x200, 0x000, 0x200, 0x000 },
	5, 0x03ff
};

struct display_geometry_contract
{
	u8 controller_width = 84;
	u8 controller_height = 48;
	u8 visible_width = 84;
	u8 visible_height = 48;
	bool x_mirror = false;

	constexpr bool valid() const
	{
		return controller_width != 0 && controller_height != 0 &&
				visible_width != 0 && visible_height != 0 &&
				visible_width <= controller_width &&
				visible_height <= controller_height;
	}
};

struct nokia_product_config
{
	struct bootstrap_bios_override
	{
		u8 bios = 0xff;
		nokia_dsp_hle_device::bootstrap_contract contract;
	};

	nokia_kbgpio_device::wiring_contract keypad_wiring;
	nokia_gensio_device::wiring_contract gensio_wiring;
	bool boot_rom_hle = true;
	bool simi_controller = false;
	bool synthetic_sim_card = false;
	bool dsp_service = false;
	bool external_service_transport = false;
	nokia_external_service_peer_device::application_contract external_service;
	nokia_dsp_hle_device::service_control_contract dsp_service_control;
	nokia_radio_peer_device::protocol_contract radio;
	gsm::subscriber::profile subscriber = gsm::subscriber::laboratory;
	bool ccont_wddisx_grounded = false;
	nokia_dsp_hle_device::bootstrap_contract dsp_bootstrap;
	std::optional<bootstrap_bios_override> dsp_bootstrap_override;
	unsigned dsp_service_delay_us = 5'000;
	unsigned dsp_peer_poll_ms = 5;
	nokia_dsp_hle_device::speech_control_contract dsp_speech_control;
	nokia_dsp_hle_device::tone_control_contract dsp_tone_control;
	nokia_mad2_pcm_device::bus_profile cobba_pcm;
	nokia_cobba_device::hle_voice_profile cobba_hle_voice;
	nokia_mad2_device::dsp_reset_wiring_contract dsp_reset_wiring;
	display_geometry_contract display;
	u8 pup_eeprom_scl_bit = 3;
	bool mbus_timer_enabled = false;
	bool mbus_terminal = false;
	bool mad2_clock_stop = true;
	bool mad2_sleep_wake_irq3 = false;
	u32 flash_persistent_start = 0;
	nokia_ccont_board_profile ccont_board = ADC_DEFAULT;
};

inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NSE8 = {
	nokia_radio_peer_device::acquisition_strategy::bitmap_multistage,
	0x08, 0x00, 0x00, 0, false, 0, false, true,
	nokia_radio_peer_device::neighbour_arfcn_encoding::direct_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::low_six_bits
};

inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NHM5 = {
	nokia_radio_peer_device::acquisition_strategy::candidate_window,
	0x14, 0x01, 0x01, 1'000, true, 0, true, false,
	nokia_radio_peer_device::neighbour_arfcn_encoding::direct_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::direct
};

inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NHM6 = {
	nokia_radio_peer_device::acquisition_strategy::candidate_window,
	0x14, 0x01, 0x01, 0, true, 0, true, true,
	nokia_radio_peer_device::neighbour_arfcn_encoding::topology_low_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::none
};

// NSM-5 v5.40 independently emits the ROM6 0x56/160 candidate-window
// request (its virgin PMM starts with ARFCN 86). Keep the contract product-
// owned while subsequent channel-confirmation and idle behavior are observed.
inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NSM5 = {
	nokia_radio_peer_device::acquisition_strategy::candidate_window,
	0x14, 0x01, 0x01, 0, true, 0x57, true, false,
	nokia_radio_peer_device::neighbour_arfcn_encoding::direct_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::direct
};

inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NHM2 = {
	nokia_radio_peer_device::acquisition_strategy::autonomous_band_scan,
	// NHM-2 v5.46 organically publishes 0x14 in byte 15 of its TCH/F
	// release CHANNEL_CONFIGURE transaction, matching the independently
	// observed NHM-5/NHM-6 parameter without implying a bit meaning.
	// NHM-2 v5.46 organically publishes 0x57 after a serving-cell
	// DOWNLINK_SIGNALLING_FAIL and channel reconfiguration. Independent ROM6
	// analysis identifies it as the request for a serving-cell SCH observation.
	0x14, 0x01, 0x01, 0, false, 0x57, false, false,
	nokia_radio_peer_device::neighbour_arfcn_encoding::direct_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::none
};

// NSM-2 v5.31 publishes 55:03050000. Its own ring dispatcher 307346
// routes 8b to task 12; 2df316 correlates 89 body bit 0 with the pending
// channel context. Physical End publishes release parameter 14. Keep
// unobserved neighbour/handover contracts unset.
inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NSM2 = {
	nokia_radio_peer_device::acquisition_strategy::candidate_window,
	0x14, 0x01, 0, 0, false, 0, false, false,
	nokia_radio_peer_device::neighbour_arfcn_encoding::direct_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::none, true
};

// NSB-6 v12.20 independently publishes 56/160. Own dispatcher 30168e
// maps 8b to task 12 and 89 body bit 0 to its pending channel context.
// Physical End publishes release parameter 14. Neighbour and handover
// contracts remain unobserved and unset.
inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NSB6 = {
	nokia_radio_peer_device::acquisition_strategy::candidate_window,
	0x14, 0x01, 0, 0, false, 0, false, false,
	nokia_radio_peer_device::neighbour_arfcn_encoding::direct_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::none, true, true
};

// NSM-3 v5.31 emits 56/160. Own RX table 306fd4 maps 8b to
// 2df484 -> task 12; 2df22e checks 89 body bit 0 against its pending
// context. Physical End publishes traffic release parameter 14.
// Own constructor 2b304a..2b3052 serializes the full carrier into payload
// bytes 6/7. Neighbour lifecycle and handover remain unvalidated.
inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NSM3 = {
	nokia_radio_peer_device::acquisition_strategy::candidate_window,
	0x14, 0x01, 0, 0, false, 0, false, false,
	nokia_radio_peer_device::neighbour_arfcn_encoding::big_endian_word,
	nokia_radio_peer_device::neighbour_bsic_encoding::none, true, true
};

// NPE-3 v5.56 emits 56/160. Own RX table 4f6078 routes 8b to
// 45835c -> task 14 (not NSM-3's task 12); 458106 correlates 89
// body bit 0 with pending context byte 2. Physical End emits release 14.
inline constexpr nokia_radio_peer_device::protocol_contract RADIO_NPE3 = {
	nokia_radio_peer_device::acquisition_strategy::candidate_window,
	0x14, 0x01, 0, 0, false, 0, false, false,
	nokia_radio_peer_device::neighbour_arfcn_encoding::direct_octet,
	nokia_radio_peer_device::neighbour_bsic_encoding::none, true
};

inline constexpr nokia_dsp_hle_device::service_control_contract
		DSP_SERVICE_CONTROL_COMPACT = {
	{ 0x0d, 0x00 }, 2
};

inline constexpr nokia_dsp_hle_device::service_control_contract
		DSP_SERVICE_CONTROL_FRAMED = {
	{ 0x00, 0x04, 0x01, 0x00, 0x0d, 0x00 }, 6
};

inline constexpr nokia_dsp_hle_device::speech_control_contract
		make_dct3_speech_control_contract()
{
	return {
		0x08,
		nokia_dsp_hle_device::speech_request_predicate { 0x0201, 0x0201 }
	};
}

// These named contracts currently have equal values but retain independent
// evidence and product-owned selection. Equality does not establish shared
// firmware semantics or analogue topology.
inline constexpr nokia_dsp_hle_device::speech_control_contract
		DSP_SPEECH_CONTROL_NSE8 = make_dct3_speech_control_contract();

inline constexpr nokia_dsp_hle_device::speech_control_contract
		DSP_SPEECH_CONTROL_NHM5 = make_dct3_speech_control_contract();

inline constexpr nokia_dsp_hle_device::speech_control_contract
		DSP_SPEECH_CONTROL_NHM6 = make_dct3_speech_control_contract();

inline constexpr nokia_dsp_hle_device::speech_control_contract
		DSP_SPEECH_CONTROL_NHM2 = make_dct3_speech_control_contract();

inline constexpr nokia_dsp_hle_device::speech_control_contract
		DSP_SPEECH_CONTROL_NSM5 = make_dct3_speech_control_contract();

// NSE-8 and NHM-5 independently publish this ROM-family tone mailbox. The
// oscillator words are quarter-Hz values; interpreting them belongs to the
// DSP HLE rather than the shared-memory transport or handset driver. Other
// ROM4 profiles retain the formerly global mapping as an explicit compatibility
// projection until a product-local trace confirms or replaces it.
inline constexpr nokia_dsp_hle_device::tone_control_contract DSP_TONE_ROM4 = {
	0x0ae, 0x0b0, 0x0b6, 4
};

// NSE-3 proves selector 8's wire decoder, but no organic call has identified a
// speech-request predicate. Keep command decoding without enabling speech.
inline constexpr nokia_dsp_hle_device::speech_control_contract
		DSP_SPEECH_CONTROL_NSE3_COMMAND = {
	0x08, std::nullopt
};

inline constexpr display_geometry_contract DISPLAY_3410 = {
	102, 72, 96, 65
};
inline constexpr display_geometry_contract DISPLAY_2100 = {
	96, 72, 96, 65, true
};
// NAM-1 v5.11 independently writes nine banks of 96 bytes and renders its
// text in reverse segment order under the non-mirrored controller view.
inline constexpr display_geometry_contract DISPLAY_3610 = {
	96, 72, 96, 65, true
};
inline constexpr display_geometry_contract DISPLAY_5210 = {
	84, 48, 84, 48, true
};
// NPE-3 clears eight 96-byte banks at 0x4e656c; Nokia's board sheet
// labels the fitted GD45 module 96x60. Remaining controller identity is open.
inline constexpr display_geometry_contract DISPLAY_6210 = {
	96, 64, 96, 60
};
// NHM-3 independently writes eight 96-byte banks; Nokia's user manual
// specifies a 96x60 visible display. Controller identification remains open.
inline constexpr display_geometry_contract DISPLAY_6250 = {
	96, 64, 96, 60
};
static_assert(display_geometry_contract{}.valid());
static_assert(DISPLAY_3410.valid());
static_assert(DISPLAY_6210.valid());
static_assert(DISPLAY_6250.valid());

inline constexpr nokia_mad2_device::dsp_reset_wiring_contract
		DSP_RESET_WIRING_3410 = {
	0x53, 0x04
};
static_assert(nokia_mad2_device::dsp_reset_wiring_contract{}.valid());
static_assert(DSP_RESET_WIRING_3410.valid());

inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSE8 = { 4, 0x01 };
// ROM4's active special-key table maps raw 0x84 (column 4) to power 0x0d.
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSE1 = {
	5, 0x10, 0x31, 0x30, 0x33, 0x2f
};
// NSK-1 scanner 2933c4 uses these five-row GPIO registers. Own 2951be
// compares raw power key 84; table 2adbc8 maps that column to logical 0d.
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSK1 = {
	5, 0x10, 0x31, 0x30, 0x33, 0x2f
};
// NSM-1 v5.23 scanner 0x2c084c and special table 0x2e0b34.
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSM1 = {
	5, 0x10, 0x31, 0x30, 0x33, 0x2f
};
// NSE-6 v6.02 IRQ handler 0x2de292 tests the five column causes at
// 0x20034 before starting ordinary key handling; IRQ0 alone is insufficient.
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSE6 = {
	5, 0x10, 0x31, 0x30, 0x33, 0x2f, 0, 0x34
};
inline constexpr nokia_gensio_device::wiring_contract GENSIO_NSE1 = {
	0x2a, 0x28, 0x2b, 0x2d, 0x29, 0x2c, 0x07, true
};
inline constexpr nokia_gensio_device::wiring_contract GENSIO_NSM5 = {
	0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true
};
// NAM-1 v5.11 independently writes control 0x22 at 0x2d, transfers CCONT
// bytes through 0x2c and polls 0x6d after each byte. Since bit 2 remains
// clear, receive-ready must follow the byte write rather than that bit.
inline constexpr nokia_gensio_device::wiring_contract GENSIO_NAM1 = {
	0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true
};
// NSM-3 v5.31 selects with 0x22 and polls 0x6d after its command byte at
// 0x302d32. Bit 2 is clear: receive-ready follows the byte, not that bit.
inline constexpr nokia_gensio_device::wiring_contract GENSIO_NSM3 = {
	0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true
};
// NPE-3 v5.56 0x4ec7b0 selects 0x22, writes 0x2c, then polls
// status bit 2 at 0x4ec7bc; control bit 2 is not a receive trigger.
inline constexpr nokia_gensio_device::wiring_contract GENSIO_NPE3 = {
	0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true
};
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NHM5 = { 5, 0x04 };
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NHM6 = { 5, 0x04 };
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NHM2 = { 5, 0x02 };
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSM5 = { 5, 0x10 };
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSE3 = { 5, 0x01 };
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NAM2 = { 5, 0x04 };
// NPE-3 v5.56 scanner 0x4f84c8 drives five bits; special map 0x2869d4
// decodes bit 4 as Power (0x0d). Normal map 0x2869b8 matches NSM-5.
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NPE3 = { 5, 0x10 };
// NSE-5 v5.01 special table 0x28dcf4 maps column bit 1 to Power.
inline constexpr nokia_kbgpio_device::wiring_contract KEYPAD_NSE5 = { 5, 0x02 };
static_assert(KEYPAD_NSE8.valid());
static_assert(KEYPAD_NSE1.valid());
static_assert(KEYPAD_NSM1.valid());
static_assert(KEYPAD_NHM5.valid());
static_assert(KEYPAD_NHM6.valid());
static_assert(KEYPAD_NHM2.valid());
static_assert(KEYPAD_NSM5.valid());
static_assert(KEYPAD_NSE3.valid());
static_assert(KEYPAD_NAM2.valid());
static_assert(KEYPAD_NPE3.valid());
static_assert(KEYPAD_NSE5.valid());

// These values currently match but remain separate evidence: NSE-8 and NHM-5
// have independent organic startup and call gates. A new product must supply
// its own complete contract rather than inheriting either handset.
inline constexpr nokia_external_service_peer_device::application_contract
		EXTERNAL_SERVICE_NSE8 = {
	36, 0x01, 0x42,
	0x5f >> 3, 0x01, 0x62 >> 3, 0x20, 0x43
};

inline constexpr nokia_external_service_peer_device::application_contract
		EXTERNAL_SERVICE_NHM5 = {
	36, 0x01, 0x42,
	0x5f >> 3, 0x01, 0x62 >> 3, 0x20, 0x43
};

inline constexpr nokia_external_service_peer_device::application_contract
		EXTERNAL_SERVICE_NHM6 = {
	36, 0x01, 0x42,
	0x5f >> 3, 0x01, 0x62 >> 3, 0x20, 0x43
};

// NHM-2 v5.46 independently emits the same discovery-close and application
// registration grammar before its radio lifecycle. Keep the contract
// product-owned even though its recovered field values coincide with the
// independently gated NHM-5 and NHM-6 applications.
inline constexpr nokia_external_service_peer_device::application_contract
		EXTERNAL_SERVICE_NHM2 = {
	36, 0x01, 0x41,
	0x5f >> 3, 0x01, 0x62 >> 3, 0x20, 0x42
};

// NSM-5 independently closes its D0 discovery exchange with sequence 0x41,
// accepts registration sequence 0x42 and channel-map sequence 0x43, then
// organically exercises channel 0x5f. Channel 0x62 is accepted but has not
// yet been exercised independently, so keep the complete map product-owned.
inline constexpr nokia_external_service_peer_device::application_contract
		EXTERNAL_SERVICE_NSM5 = {
	36, 0x01, 0x42,
	0x5f >> 3, 0x01, 0x62 >> 3, 0x20, 0x43
};

// NAM-2 v5.84 independently completes the compact DSP service-control
// transaction before accepting this class-0x40 application sequence. Keep
// the profile separate even where its channel resources coincide with older
// products: acceptance and subsequent lifecycle behavior are product-local.
inline constexpr nokia_external_service_peer_device::application_contract
		EXTERNAL_SERVICE_NAM2 = {
	36, 0x01, 0x42,
	0x5f >> 3, 0x01, 0x62 >> 3, 0x20, 0x43
};

// NAM-1 independently acknowledges discovery sequence 0x41. Its application
// candidate therefore continues the same service grammar at sequence 0x42;
// acceptance and the channel resources must be established by NAM-1 itself.
inline constexpr nokia_external_service_peer_device::application_contract
		EXTERNAL_SERVICE_NAM1 = {
	36, 0x01, 0x42,
	0x5f >> 3, 0x01, 0x62 >> 3, 0x20, 0x43
};

inline constexpr nokia_dsp_hle_device::bootstrap_contract BOOTSTRAP_READY_64 = {
	nokia_dsp_hle_device::bootstrap_exchange_strategy::zero_acknowledge,
	64,
	{{ { 0x000, 0x0001 }, { 0x002, 0x0001 }, { 0x004, 0x0001 } }},
	3, std::nullopt, std::nullopt, 0
};

inline constexpr nokia_dsp_hle_device::bootstrap_contract BOOTSTRAP_READY_58 = {
	nokia_dsp_hle_device::bootstrap_exchange_strategy::zero_acknowledge,
	58,
	{{ { 0x000, 0x0001 }, { 0x002, 0x0001 }, { 0x004, 0x0001 } }},
	3, std::nullopt, std::nullopt, 0
};

inline constexpr nokia_dsp_hle_device::bootstrap_contract BOOTSTRAP_PING_PONG = {
	nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
	0, {}, 0, std::nullopt,
	nokia_dsp_hle_device::bootstrap_parked_contract {
		0x002, 0xffff, 0x0000
	},
	0x0001
};

// NAM-2 v5.21 performs 992 alternating zero-valued handoffs on each shared
// cell before reading the three DSP-owned startup verdicts.
inline constexpr nokia_dsp_hle_device::bootstrap_contract
		BOOTSTRAP_PING_PONG_READY_992 = {
	nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
	992,
	{{ { 0x000, 0x0001 }, { 0x002, 0x0001 }, { 0x004, 0x0001 } }},
	3, std::nullopt, std::nullopt, 0
};

inline constexpr nokia_dsp_hle_device::bootstrap_contract
		BOOTSTRAP_FLASH_VERIFICATION_PARTIAL = {
	nokia_dsp_hle_device::bootstrap_exchange_strategy::zero_acknowledge,
	64,
	{{ { 0x000, 0x0b06 } }},
	1, std::nullopt, std::nullopt, 0
};

inline constexpr nokia_dsp_hle_device::bootstrap_contract
		BOOTSTRAP_FLASH_VERIFICATION_ROM3 = {
	nokia_dsp_hle_device::bootstrap_exchange_strategy::zero_acknowledge,
	64,
	{{ { 0x000, 0x0b06 } }},
	1,
	nokia_dsp_hle_device::bootstrap_pair_contract {
		0x004, 0x006, 0xffff, 0x0003
	},
	std::nullopt, 0
};

inline constexpr nokia_product_config make_3210_config()
{
	nokia_product_config result;
	result.keypad_wiring = KEYPAD_NSE8;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.dsp_service = true;
	result.external_service_transport = true;
	result.external_service = EXTERNAL_SERVICE_NSE8;
	result.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	result.dsp_bootstrap = BOOTSTRAP_READY_64;
	result.radio = RADIO_NSE8;
	result.dsp_service_delay_us = 4'000;
	result.dsp_peer_poll_ms = 4;
	// Paired NSE-8 firmware independently constructs/removes this field around
	// Answer while retaining the separate 0x0408 dedicated-channel field.
	result.dsp_speech_control = DSP_SPEECH_CONTROL_NSE8;
	result.dsp_tone_control = DSP_TONE_ROM4;
	result.cobba_pcm.data_clock = 520'000;
	result.cobba_pcm.frame_clock = 8'000;
	// DCT3 MAD2/COBBA-GJ PCM timing diagrams show a 16-bit serial word
	// containing a sign-extended 13-bit linear converter sample.
	result.cobba_pcm.sample_bits = 13;
	// Best-evidenced Nokia/COBBA-family format: a one-clock active-high frame
	// pulse followed by a 16-bit, MSB-first word transferred on falling
	// PCMDClk edges. At NSE-8's 520/8 kHz rates, the remaining 48 of 65 clocks
	// are inactive. Keep this product-configured pending a direct NSE-8 trace.
	result.cobba_pcm.sync_clocks = 1;
	result.cobba_pcm.word_clocks = 16;
	result.cobba_pcm.msb_first = true;
	result.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	// HLE fallback corresponding to NSE-8's internal MIC2/EAR board path.
	// Physical sound routes are installed only by noki3210(machine_config).
	result.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	result.cobba_hle_voice.output = nokia_cobba_device::ear;
	// NSE-8/9 system-module Tables 33/34: the internal voice path uses
	// COBBA MIC2 at +18 dB and EAR at -10 dB.
	result.cobba_hle_voice.microphone_gain_db = 18.0F;
	result.cobba_hle_voice.output_gain_db = -10.0F;
	result.ccont_board = ADC_3210;
	return result;
}

inline constexpr nokia_product_config make_3310_config()
{
	nokia_product_config result;
	// Project Blacksphere's 331x interrupt table identifies IRQ3 as wake.
	result.mad2_sleep_wake_irq3 = true;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.dsp_service = true;
	result.external_service_transport = true;
	result.external_service = EXTERNAL_SERVICE_NHM5;
	result.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	result.radio = RADIO_NHM5;
	result.keypad_wiring = KEYPAD_NHM5;
	result.dsp_bootstrap = BOOTSTRAP_READY_58;
	result.dsp_service_delay_us = 4'000;
	result.dsp_peer_poll_ms = 4;
	// NHM-5 independently publishes command 0x08 value 0x060b immediately
	// after organic Answer, then 0x040a during physical-End teardown. Those
	// values satisfy and clear the same recovered speech-request field, but do
	// not establish a PCM bus for this product.
	result.dsp_speech_control = DSP_SPEECH_CONTROL_NHM5;
	result.dsp_tone_control = DSP_TONE_ROM4;
	// Nokia's NHM-5/UB 4 V09 COBBA schematic (version 2.0, 04.05.2001)
	// independently wires the built-in differential microphone pads through
	// L402 to MIC2P/MIC2N and the receiver to EARP/EARN. Record that topology
	// without borrowing NSE-8's gains.
	result.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	result.cobba_hle_voice.output = nokia_cobba_device::ear;
	// NHM-5NX System Module issue 1 09/00, pages 27-28: COBBA-GJP
	// divides RFIClk 13 MHz by 13 for a 1.000 MHz PCMDClk, then by 125
	// for the 8.0 kHz PCMSClk. Its timing chart shows a one-clock sync
	// pulse and a 16-bit, MSB-first word containing a sign-extended
	// 13-bit linear sample; data changes on rising PCMDClk edges.
	result.cobba_pcm.data_clock = 1'000'000;
	result.cobba_pcm.frame_clock = 8'000;
	result.cobba_pcm.sample_bits = 13;
	result.cobba_pcm.sync_clocks = 1;
	result.cobba_pcm.word_clocks = 16;
	result.cobba_pcm.msb_first = true;
	result.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	result.ccont_board = ADC_STANDARD;
	return result;
}

// NHM-6 v4.50 completes 64 DSP bootstrap exchanges and shares the five-row
// keypad. The 3310 analog tuple is retained as a calibrated compatibility
// profile: it advances the virgin PMM organically, but does not prove NHM-6
// PCB signal identity. Its lower external-service transport remains available,
// but the application contract remains empty: the independent NHM-6 gates
// do not establish NHM-5's delay, registration body or channel bitmap.
inline constexpr nokia_product_config make_3330_config()
{
	nokia_product_config result;
	result.mad2_sleep_wake_irq3 = true;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.dsp_service = true;
	result.external_service_transport = true;
	result.external_service = EXTERNAL_SERVICE_NHM6;
	// NHM-6 independently emits type 0x70 payload 0d00 during its boot
	// self-test and consumes the compact type 0x74 completion.
	result.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	result.radio = RADIO_NHM6;
	result.dsp_bootstrap = BOOTSTRAP_READY_64;
	result.keypad_wiring = KEYPAD_NHM6;
	result.dsp_service_delay_us = 4'000;
	result.dsp_peer_poll_ms = 4;
	// NHM-6 independently publishes command 0x08 value 0x060b immediately
	// after organic physical Answer and 0x040a during physical-End teardown.
	// This establishes speech control independently of NHM-5.
	result.dsp_speech_control = DSP_SPEECH_CONTROL_NHM6;
	result.dsp_tone_control = DSP_TONE_ROM4;
	// Nokia's combined NHM-2/5/6 service material identifies the common
	// COBBA-GJP N100 audio boundary. COBBA-GJP documentation fixes its codec
	// SIO at RFIClk/13 = 1 MHz and PCMSClk at /125 = 8 kHz, with the same
	// one-clock sync and sign-extended 13-in-16 serial word. Keep this as an
	// independent NHM-6 profile; it does not import NHM-5 analogue routing.
	result.cobba_pcm.data_clock = 1'000'000;
	result.cobba_pcm.frame_clock = 8'000;
	result.cobba_pcm.sample_bits = 13;
	result.cobba_pcm.sync_clocks = 1;
	result.cobba_pcm.word_clocks = 16;
	result.cobba_pcm.msb_first = true;
	result.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	// The combined NHM-2/5/6 repair guide assigns NHM-6 the common N100
	// microphone path through L402/C120 and receiver path through R119/R120.
	// The matching COBBA schematic terminates them on MIC2P/MIC2N and
	// EARP/EARN.  Keep gains neutral pending product-specific measurements.
	result.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	result.cobba_hle_voice.output = nokia_cobba_device::ear;
	result.ccont_board = ADC_STANDARD;
	// NHM-6's supplied PMM image is loaded at flash offset 0x3f0000,
	// CPU address 0x5f0000. Firmware-created objects may use earlier erased
	// sectors, but they are not part of the static PMM catalogue boundary.
	// This only enables attribution; the flash device still owns all storage.
	result.flash_persistent_start = 0x005f0000;
	return result;
}

// NHM-2 releases the DSP through reset-control bit 2 and then polls MAD2's
// clock/ready status bit. The 0x53 readback is the observed running state; its
// readiness semantics live in MAD2, while the board wiring remains here.
inline constexpr nokia_product_config make_3410_config()
{
	nokia_product_config result;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.dsp_service = true;
	result.external_service_transport = true;
	// NHM-2 independently emits the compact 70/0d00 request during organic
	// boot and requires its 74/0d00 completion before exposing idle.
	result.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	// Its organic 05/d0 discovery publications establish a separate
	// application entrance; do not inherit another product's builder.
	result.external_service = EXTERNAL_SERVICE_NHM2;
	result.radio = RADIO_NHM2;
	result.keypad_wiring = KEYPAD_NHM2;
	result.dsp_bootstrap = BOOTSTRAP_PING_PONG;
	result.dsp_service_delay_us = 50;
	result.dsp_peer_poll_ms = 4;
	// NHM-2 independently publishes command 0x08 value 0x060b immediately
	// after physical Send answers the call and 0x040a during physical-End
	// teardown. This establishes the speech-request field without inheriting
	// an NHM-5/NHM-6 firmware contract.
	result.dsp_speech_control = DSP_SPEECH_CONTROL_NHM2;
	result.dsp_tone_control = DSP_TONE_ROM4;
	// Nokia's combined NHM-2/5/6 repair material identifies COBBA-GJP N100.
	// Its codec SIO derives 1 MHz PCMDClk from 13 MHz / 13 and 8 kHz PCMSClk
	// by /125, with a one-clock sync and sign-extended 13-in-16 serial word.
	// The fitted NHM-2 analogue microphone and receiver routes remain unknown.
	result.cobba_pcm.data_clock = 1'000'000;
	result.cobba_pcm.frame_clock = 8'000;
	result.cobba_pcm.sample_bits = 13;
	result.cobba_pcm.sync_clocks = 1;
	result.cobba_pcm.word_clocks = 16;
	result.cobba_pcm.msb_first = true;
	result.cobba_pcm.data_edge =
			nokia_mad2_pcm_device::clock_edge::falling;
	// Nokia's combined NHM-2/5/6 level-3/4 repair guide assigns all three
	// products the same N100 COBBA microphone path through L402/C120 and the
	// receiver path through R119/R120.  The matching NHM-5 schematic names
	// those common endpoints MIC2P/MIC2N and EARP/EARN.  Record connectivity
	// only; no NHM-2 analogue gain is inferred.
	result.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	result.cobba_hle_voice.output = nokia_cobba_device::ear;
	result.dsp_reset_wiring = DSP_RESET_WIRING_3410;
	result.display = DISPLAY_3410;
	result.ccont_board = ADC_STANDARD;
	return result;
}

inline constexpr nokia_product_config make_6110_config()
{
	nokia_product_config result;
	result.keypad_wiring = KEYPAD_NSE3;
	result.boot_rom_hle = false;
	result.simi_controller = true;
	// NSE-3 v4.06 accepts direct/inverse convention ATRs, parses the T0/TDn
	// interface-byte chain and maps the lab card's TA1=0x05 to PPS ff 00 ff.
	// Compose that removable standards-shaped test card; its subscriber files
	// are fixture policy and do not claim Nokia 6110 product identity.
	result.synthetic_sim_card = true;
	// NSE-3 Chapter 3 documents COBBA-GJ deriving a 1 MHz PCMDClk and
	// 8 kHz PCMSClk, with a sign-extended 13-bit sample in a 16-bit word.
	// Firmware-facing DSP, external-service and radio peers deliberately
	// retain their disabled defaults. The external image establishes generic
	// 0x1e/0x1c service framing and d0 discovery through DSP reports 8d/8e,
	// plus the class-40 command 70/71 channel-map acknowledgement grammar and
	// its generic 64-byte map-consumer geometry, but not the NSE-8 5f/62 map
	// contents. It also proves a product-specific command-64 status body. Its
	// dispatcher and internal event-d3 publication conditions are bounded
	// too, without assigning meanings to their runtime control argument. It
	// does not establish the DSP-owned startup delay, advertised map contents
	// or exchange ordering,
	// so those proven receive-side pieces do not justify enabling the peer.
	// The external image also bounds task 9's 011c..0120 event dispatch and
	// family-specific counter policy, but neither the event timer units nor
	// the missing DSP's trigger policy.
	// All three recovered NSE-3 images statically prove 64 alternating sparse
	// flash-verification blocks. Own that exact transport count here rather
	// than inheriting the conservative product default. A separate ROM4 HLE
	// can acknowledge all 64 blocks and still leaves the exact v4.06 image
	// waiting for a non-zero final publication at 0x10002. NSE-3 later
	// compares captured shared word 0x10000 against
	// 0x0b06, so the generic HLE ready words of 1 are demonstrably
	// incompatible. Both v5.48 variants require the same value despite a real
	// handset reporting fitted COBBA B07, so this is typed only as a firmware
	// result, not a physical-silicon identity. Publish that evidenced first
	// result while deliberately leaving the unknown verdict untouched (reset
	// zero in v4.06; MCU-parked 0xffff in v5.48), and keep the peer disabled
	// pending DSP evidence.
	result.dsp_bootstrap = BOOTSTRAP_FLASH_VERIFICATION_PARTIAL;
	result.dsp_bootstrap_override = nokia_product_config::bootstrap_bios_override {
		2, BOOTSTRAP_FLASH_VERIFICATION_ROM3
	};
	// The external MCU image correlates its 70 0d request with a framed
	// type-74 completion through controller bit 2 and timer 14. NSE-8's exact
	// decoder supplies the missing four-byte frame transformation. Type this
	// proven request-derived boundary independently; it remains dormant while
	// the unresolved NSE-3 DSP bootstrap keeps the DSP service disabled.
	result.dsp_service_control = DSP_SERVICE_CONTROL_FRAMED;
	// Its external firmware independently proves the shared type-0x1a/68
	// bitmap wire boundary. Record that separately from the still-unproved
	// NSE-3 acquisition policy; a disabled peer cannot synthesize traffic.
	// NSE-3 independently publishes selector 8 as 0x8000 | value[11:0].
	// Decode that proven wire command, but leave the speech-request predicate
	// empty until an organic Answer/End transition identifies its semantics.
	result.dsp_speech_control = DSP_SPEECH_CONTROL_NSE3_COMMAND;
	result.dsp_tone_control = DSP_TONE_ROM4;
	result.cobba_pcm.data_clock = 1'000'000;
	result.cobba_pcm.frame_clock = 8'000;
	result.cobba_pcm.sample_bits = 13;
	result.cobba_pcm.sync_clocks = 1;
	result.cobba_pcm.word_clocks = 16;
	result.cobba_pcm.msb_first = true;
	result.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	result.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	result.cobba_hle_voice.output = nokia_cobba_device::ear;
	// NSE-3 v4.06's bit-banged 24C64 routines drive GenIO signal bit 2
	// as SCL; SDA is signal/direction bit 0.
	result.pup_eeprom_scl_bit = 2;
	result.mbus_timer_enabled = true;
	return result;
}

inline constexpr nokia_product_config make_5110_config()
{
	nokia_product_config result;
	result.keypad_wiring = KEYPAD_NSE1;
	result.gensio_wiring = GENSIO_NSE1;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	// NSE-1 bit-bangs its external 24C16 through PUP GenIO: signal
	// bit 0 is SDA, signal bit 2 is SCL, and direction bit 0 releases SDA.
	result.pup_eeprom_scl_bit = 2;
	// Coherent ROM4 idle writes 0x2c/0x0c, not the bit-1 clock-stop request.
	// The separate teardown setter has no validated wake contract yet.
	result.mad2_clock_stop = false;
	result.ccont_board = ADC_5110;
	result.boot_rom_hle = true;
	// NSE-1 System Module 03/98, p. 3-32: COBBA divides 13 MHz by 13,
	// then by 125, supplying 1 MHz data and 8 kHz PCM frame clocks. The
	// depicted word is 16-bit, sign-extended 13-bit PCM. Sync width/edge
	// stay unset: this does not enable the unattached native serial path.
	result.cobba_pcm.data_clock = 1'000'000;
	result.cobba_pcm.frame_clock = 8'000;
	result.cobba_pcm.sample_bits = 13;
	result.cobba_pcm.word_clocks = 16;
	return result;
}

// Preserve the previous 64-exchange behavior for unvalidated products. This is
// an explicit compatibility calibration, not a recovered cross-DCT3 constant.
inline constexpr nokia_product_config make_conservative_config(
		nokia_kbgpio_device::wiring_contract keypad_wiring = {})
{
	nokia_product_config result;
	result.keypad_wiring = keypad_wiring;
	result.dsp_bootstrap = BOOTSTRAP_READY_64;
	return result;
}

inline constexpr nokia_product_config make_5210_config()
{
	nokia_product_config result = make_conservative_config(KEYPAD_NSM5);
	// NSM-5 selects CCONT with control 0x22 (bit 2 clear), writes a command,
	// and then polls receive-ready. The reply therefore cannot be conditional
	// on the NSE-8 control-bit convention.
	result.gensio_wiring = GENSIO_NSM5;
	// NSM-5 organically issues DSP command 4 with pending value 2 after upload.
	// Complete that observed transport request without supplying any
	// higher-level service payload.
	result.dsp_service = true;
	// NSM-5 then sends a checksum-valid D0/01 discovery frame. Enable its
	// request-derived transport reply and independently accepted application
	// sequences; neither path fabricates a firmware-side completion.
	result.external_service_transport = true;
	result.external_service = EXTERNAL_SERVICE_NSM5;
	// Its next organic request is type 0x70 payload 0d00. Complete that exact
	// control transaction with the compact type-0x74 echo; no other type-0x70
	// payload is accepted by this contract.
	result.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	// After accepting that completion NSM-5 organically programs the standard
	// SIMI register block. Expose the controller and an ordinary removable GSM
	// card through that physical boundary; card contents remain external input.
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.radio = RADIO_NSM5;
	// Own 3de7a0 table routes RX 8b through 37019c to task 12;
	// 2e336c -> 2bd782 consumes forty four-byte ARFCN/RSSI records.
	// Late SIM readiness organically emits 57:03050000. Answer that
	// request at the DSP boundary, without selecting firmware state.
	result.radio.background_band_measurements = true;
	// Physical Answer and End independently publish wire values 0x860b and
	// 0x840a. Decode that evidenced control field independently of the PCM bus.
	result.dsp_speech_control = DSP_SPEECH_CONTROL_NSM5;
	// NSM-5 System Module issue 1 02/2002, page 24: COBBA-GJP divides
	// RFIClk 13 MHz by 13 for a 1.000 MHz PCMDClk and by 125 again for an
	// 8.0 kHz PCMSClk. The timing chart specifies a sign-extended 13-bit
	// linear sample in a 16-bit, MSB-first serial word.
	result.cobba_pcm.data_clock = 1'000'000;
	result.cobba_pcm.frame_clock = 8'000;
	result.cobba_pcm.sample_bits = 13;
	result.cobba_pcm.sync_clocks = 1;
	result.cobba_pcm.word_clocks = 16;
	result.cobba_pcm.msb_first = true;
	result.cobba_pcm.data_edge =
			nokia_mad2_pcm_device::clock_edge::falling;
	// NSM-5 troubleshooting issue 1 02/2002 traces the internal microphone
	// through R268/C274/C263/C278/C262 to MIC2P/MIC2N, and the receiver through
	// C292/C291/L272/L271 to EARP/EARN. Gains remain neutral until recovered.
	result.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	result.cobba_hle_voice.output = nokia_cobba_device::ear;
	result.ccont_board = ADC_5210;
	result.display = DISPLAY_5210;
	return result;
}

inline constexpr nokia_product_config make_2100_config()
{
	nokia_product_config result = make_conservative_config();
	// NAM-2 v5.84 drives five distinct row bits during its organic matrix scan;
	// the cold-start input is observed on column bit 2.
	result.keypad_wiring = KEYPAD_NAM2;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	// NAM-2 v5.84 organically writes control 0x22 at offset 0x2d, transfers
	// the CCONT byte through 0x2c and polls receive-ready at 0x6d. This is an
	// independent observation of the register placement used by NSM-5; it
	// does not imply shared keypad, display or peer contracts.
	result.gensio_wiring = GENSIO_NSM5;
	// After its 64-exchange upload, NAM-2 repeatedly rings DSP command 4 with
	// service-pending value 2. Acknowledge that observed transport transaction;
	// no application payload or sibling-handset service contract is implied.
	result.dsp_service = true;
	// The complete v5.21 image seeds shared words 0x0fe and 0x100 with one
	// and alternates ownership between them.  That is the observed ping-pong
	// bootstrap, distinct from v5.84's bounded 64-exchange zero-ack dialogue.
	// system_bios() uses ROM's one-based BIOS flag, so the second declaration
	// is selected as 2 here.
	result.dsp_bootstrap_override = nokia_product_config::bootstrap_bios_override {
		2, BOOTSTRAP_PING_PONG_READY_992
	};
	// Its type-05 D0/01 frame supplies the complete discovery reply correlation.
	// Enable request-derived transport handling and the independently retained
	// NAM-2 application contract.
	result.external_service_transport = true;
	result.external_service = EXTERNAL_SERVICE_NAM2;
	result.mbus_timer_enabled = true;
	result.mbus_terminal = true;
	// NAM-2 organically follows discovery with type-70 payload 0d00. The
	// protocol completion is the compact type-74 echo of that exact body.
	result.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	// NAM-2 addresses banks 0..8 and transfers exactly 96 bytes per ordinary
	// bank. Primary board material specifies a 96x65 display; the recovered
	// frame establishes reversed segment order rather than a software rotation.
	result.display = DISPLAY_2100;
	result.ccont_board = ADC_2100;
	return result;
}

inline constexpr nokia_product_config make_3610_config()
{
	nokia_product_config result = make_conservative_config();
	result.gensio_wiring = GENSIO_NAM1;
	result.display = DISPLAY_3610;
	// NAM-1 completes its 64-exchange bootstrap, then raises command-4
	// doorbells with service-pending value 2. Enable only the transport-level
	// IRQ4 completion for that observed request; no packet/application peer is
	// implied by this contract.
	result.dsp_service = true;
	// The first organic packet is a checksum-valid D0/01 discovery frame.
	// Enable only the generic request-derived discovery reply; unsolicited
	// registration remains disabled until NAM-1 supplies its own contract.
	result.external_service_transport = true;
	// NAM-1 independently emits the exact compact 0d00 service-control
	// request. Complete only that observed transaction.
	result.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	result.external_service = EXTERNAL_SERVICE_NAM1;
	// The timer makes NAM-1 independently emit the checksum-valid physical
	// startup frame 1f ff 00 d0 00 01 01 01 31. Its request-derived terminal
	// exchange is therefore enabled independently of NAM-2.
	result.mbus_timer_enabled = true;
	result.mbus_terminal = true;
	// NAM-1 uses the BLB-2 Li-ion pack family on the standard CCONT battery
	// inputs. Reuse the independently recovered NSM-5 BLB-2 nominal tuple;
	// this supplies physical board inputs while firmware retains recognition,
	// temperature and voltage decisions.
	result.ccont_board = ADC_5210;
	// The ROM contains the complete later-MAD2 SIMI register driver and reads
	// live control/status before its transaction task activates the interface.
	// Compose the physical controller and the ordinary removable lab card;
	// firmware remains responsible for clocking, activation and all APDUs.
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	// NAM-1's MCU/PPM image ends at 0x54ffff. Public flash maps place its
	// flash-backed product state in the remaining 0x550000..0x5fffff range.
	// Keep this as an observation boundary until a matching PMM is recovered.
	result.flash_persistent_start = 0x00550000;
	return result;
}

inline constexpr nokia_product_config PRODUCT_3210 = make_3210_config();
inline constexpr nokia_product_config PRODUCT_3310 = make_3310_config();
inline constexpr nokia_product_config PRODUCT_3330 = make_3330_config();
inline constexpr nokia_product_config PRODUCT_3410 = make_3410_config();
inline constexpr nokia_product_config PRODUCT_5110 = make_5110_config();
inline constexpr nokia_product_config PRODUCT_6110 = make_6110_config();
inline constexpr nokia_product_config PRODUCT_5210 = make_5210_config();
inline constexpr nokia_product_config PRODUCT_2100 = make_2100_config();
inline constexpr nokia_product_config PRODUCT_3610 = make_3610_config();
inline constexpr nokia_product_config make_3350_config()
{
	nokia_product_config result;
	// Own scanner 3a3568 drives five rows; special table 45bdb0
	// selects column bit 1 for Power, independently of NHM-2.
	result.keypad_wiring = { 5, 0x02 };
	// Own ROM has a nine-bank, 96-byte LCD clear candidate. Show its full
	// storage as a diagnostic view; panel selection/orientation is unproved.
	result.display = { 96, 72, 96, 72, false };
	// No DSP bootstrap completion, service backend or donor PMM is supplied.
	return result;
}
inline constexpr nokia_product_config PRODUCT_3350 = make_3350_config();
inline constexpr nokia_product_config make_8855_config()
{
	nokia_product_config result;
	// NSM-4 v5.18 scanner 3c0e68 and special table 45dff0.
	result.keypad_wiring = { 5, 0x10 };
	// Own CCONT 3ca4f4 and LCD 3cda6a/3cdad6 instruction contracts.
	result.gensio_wiring = { 0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true };
	result.simi_controller = true; // Own 385874 initialization; empty socket.
	// Own clear loop writes 84 bytes across six banks; physical panel unproved.
	result.display = { 84, 48, 84, 48, false };
	return result; // No DSP completion, service/radio peers or donor storage.
}
inline constexpr nokia_product_config PRODUCT_8855 = make_8855_config();
inline constexpr nokia_product_config make_5190_config()
{
	nokia_product_config result;
	// Own 29cde8 initializes 38/39 and FIFO controls 3d/3e; 29ce5e
	// drains RX at 37 while polling 3c. The socket is empty, not absent.
	result.simi_controller = true;
	// Own scanner 2af050 and maps 2d2984/2d29a0 establish this layout.
	result.keypad_wiring = { 5, 0x10, 0x31, 0x30, 0x33, 0x2f };
	// Own 2ad4b0 selects 22 at 28, sends through 2a, polls 29 bit 2
	// and receives at 2d. LCD 2ae0ac shares the serial selection block.
	result.gensio_wiring = { 0x2a, 0x28, 0x2b, 0x2d, 0x29, 0x2c, 0x07, true };
	// Own EEPROM byte transfer 2ac704: SDA bit 0, SCL bit 2.
	result.pup_eeprom_scl_bit = 2;
	return result; // No DSP acknowledgement, inserted card or application peer.
}
inline constexpr nokia_product_config PRODUCT_5190 = make_5190_config();
inline constexpr nokia_product_config make_6190_config()
{
	nokia_product_config result;
	// NSB-3's own scanner 2b2a2c and special key table 2d6a14.
	result.keypad_wiring = { 5, 0x10, 0x31, 0x30, 0x33, 0x2f };
	// Own 2b129e CCONT and 2b19e4/2b1a4a LCD accessor contracts.
	result.gensio_wiring = { 0x2a, 0x28, 0x2b, 0x2d, 0x29, 0x2c, 0x07, true };
	result.pup_eeprom_scl_bit = 2; // Own SDA/SCL transfer at 2b057e.
	result.simi_controller = true; // Own 2a19c0; empty socket.
	return result; // No DSP acknowledgement, application peer or inserted card.
}
inline constexpr nokia_product_config PRODUCT_6190 = make_6190_config();
inline constexpr nokia_product_config make_5110i_config()
{
	nokia_product_config result;
	// Own scanner 29806c and decoder/maps 299010/2b382c/2b3848.
	result.keypad_wiring = { 5, 0x10, 0x31, 0x30, 0x33, 0x2f };
	// Own 296704 selects 22 at 28, writes 2a, polls 29 bit 2, reads 2d.
	// LCD accessors 296e46/296eac independently use 2c/2b and ready bit 0.
	result.gensio_wiring = { 0x2a, 0x28, 0x2b, 0x2d, 0x29, 0x2c, 0x07, true };
	result.pup_eeprom_scl_bit = 2; // Own SDA/SCL byte transfer at 295746.
	result.simi_controller = true; // Own initialization at 286520; empty socket.
	return result; // No DSP completion, inserted card or service/radio peer.
}
inline constexpr nokia_product_config PRODUCT_5110I = make_5110i_config();
inline constexpr nokia_product_config PRODUCT_DEFAULT = make_conservative_config();
inline constexpr nokia_product_config make_8xxx_config()
{
	nokia_product_config result = make_conservative_config({ 4, 0x10 });
	// Independently recovered command/read loops: NSM-3D v5.02 0x2feb1c,
	// NSM-2 v5.31 0x3030ac and NSB-6 v12.20 0x2fd0d8 select 0x22,
	// write 0x2c and poll status bit 2 at 0x6d before reading 0x6c.
	result.gensio_wiring = { 0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true };
	return result;
}
inline constexpr nokia_product_config PRODUCT_8XXX = make_8xxx_config();

inline constexpr nokia_product_config make_8290_config()
{
	nokia_product_config result;
	// Own 308240 scan covers rows 0..4; tables 343d7c/343d98
	// use the five-row NSM-5 logical layout and special Power column 4.
	result.keypad_wiring = { 5, 0x10 };
	// NSB-7 v5.22 305796 selects 22; 3057b4 writes 2c, then
	// 3057b8 polls 6d bit 2 and 3057c0 reads 6c. No DSP verdict.
	result.gensio_wiring = { 0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true };
	return result;
}
inline constexpr nokia_product_config PRODUCT_8290 = make_8290_config();

inline constexpr nokia_product_config make_8290_upload_config()
{
	nokia_product_config result = PRODUCT_8290;
	result.dsp_reset_wiring = { 0x10, 0x01 };
	// Own compiled identity-6 branch, not a fitted resident mask.
	result.dsp_bootstrap = {
		nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
		0, {}, 0, std::nullopt,
		nokia_dsp_hle_device::bootstrap_parked_contract { 0x004, 0xffff, 6 }, 0
	};
	return result;
}
inline constexpr nokia_product_config PRODUCT_8290_UPLOAD = make_8290_upload_config();

inline constexpr nokia_product_config make_8850_config()
{
	nokia_product_config result = make_8xxx_config();
	// Own scan 3015c0 drives pins 1..4; IRQ0 handler 301714 reads
	// pending column bits at 2b before posting scan event 41.
	result.keypad_wiring.row_pin_shift = 1;
	result.keypad_wiring.column_irq_status = 0x2b;
	// NSM-2's system-module manual identifies the ordinary BLB-2 pack.
	// Use the existing BLB-2 nominal board inputs rather than conservative
	// full-scale placeholders. Raw transfer values remain calibrated, not
	// measured NSM-2 electrical units; firmware owns all pack decisions.
	result.ccont_board = ADC_5210;
	// NSM-2 v5.31 independently accepts silicon identity 5 or 6 at
	// 0x2cae26, then alternates upload ownership until its final-result poll.
	// Select ROM6 HLE; acknowledge transfers without inventing that verdict.
	result.dsp_bootstrap = {
		nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
		0, {}, 0, std::nullopt,
		nokia_dsp_hle_device::bootstrap_parked_contract { 0x004, 0xffff, 6 },
		0
	};
	return result;
}
inline constexpr nokia_product_config PRODUCT_8850 = make_8850_config();

inline constexpr nokia_product_config make_8210_config()
{
	nokia_product_config result = make_conservative_config({ 4, 0x10 });
	result.gensio_wiring = GENSIO_NSM3;
	// NSM-3 System Module issue 1 12/1999, page 24 independently defines
	// COBBA-GJP's 1 MHz/8 kHz serial bus and sign-extended 13-in-16 word.
	// This does not select a speech request, analogue route or codec gain.
	result.cobba_pcm.data_clock = 1'000'000;
	result.cobba_pcm.frame_clock = 8'000;
	result.cobba_pcm.sample_bits = 13;
	result.cobba_pcm.sync_clocks = 1;
	result.cobba_pcm.word_clocks = 16;
	result.cobba_pcm.msb_first = true;
	result.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	// The stock package selects this same MCU for ROM5 and ROM6. Model the
	// latter: 0x2cad46 accepts 6 in the parked silicon-identity cell before
	// alternating ownership handoffs. Final verification stays unmodelled.
	result.dsp_bootstrap = {
		nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
		0, {}, 0, std::nullopt,
		nokia_dsp_hle_device::bootstrap_parked_contract { 0x004, 0xffff, 6 },
		0
	};
	return result;
}
inline constexpr nokia_product_config PRODUCT_8210 = make_8210_config();

inline constexpr nokia_product_config make_6210_config()
{
	nokia_product_config result;
	result.keypad_wiring = KEYPAD_NPE3;
	result.display = DISPLAY_6210;
	// NPE-3 HEADDET has an open-microphone pull-up (schematic sheets 2/3).
	// Own selector-0 reader 46b3d6 feeds accessory state 173a97:
	// zero selects state 10/Headset; nominal high leaves state 0f/unattached.
	// This is an open-input nominal value, not measured ADC calibration.
	result.ccont_board.channel_defaults[0] = 0x3ff;
	// NPE-3 v5.56 0x4dc0e4 sets release bit 2 at CTSI+2, then
	// 0x4dc0fa tests bit 4 via LSRS #5/carry. No bootstrap reply is assumed.
	// Verifier start 426c36..426c3e separately sets CTSI+2 bit 0.
	result.dsp_reset_wiring = { 0x10, 0x04, 0x01 };
	result.gensio_wiring = GENSIO_NPE3;
	// Own 5046fc..50470e requires MBUSTIM, but enabling it exposes an
	// unresolved post-wake input regression. npe3mbus retains that research
	// path separately until the complete call/sleep contract is qualified.
	// 0x426c58 alternates the two shared-buffer ownership words. Keep
	// transfer acknowledgements separate from the unresolved final verdict.
	result.dsp_bootstrap = {
		nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
		0, {}, 0, std::nullopt, std::nullopt, 0
	};
	return result;
}
inline constexpr nokia_product_config PRODUCT_6210 = make_6210_config();

inline constexpr nokia_product_config make_6250_config()
{
	nokia_product_config result = make_conservative_config();
	// NHM-3 SM4v15 sheets A-2/A-6 connect HEADDET to CCONT EAD and
	// pull it toward VBB through R213/R212 (47k/2k2). Own reader 4ae33e
	// selects input 0; zero falsely enters accessory state 10/Headset.
	// Use a nominal open-input high, not a measured voltage/ADC calibration.
	result.ccont_board.channel_defaults[0] = 0x3ff;
	// NHM-3 source 7 maps to selector 2 (0x288fa0). Its acquired PMM
	// calibration and 1500/232 scale convert raw 0x230 to about 3.60 V;
	// full scale exceeds the analog initialization's 1.8..5.5 V window.
	// This is a nominal board input, not a measured ADC transfer curve.
	result.ccont_board.channel_defaults[2] = 0x230;
	// Own five-row matrix at 0x288f7c; separate power table 0x288f98
	// maps only column 4 to key 0x0d (the other columns are 0x5a).
	result.keypad_wiring = { 5, 0x10 };
	result.display = DISPLAY_6250;
	// 6250 v5.03 sets CTSI+2 bit 2 at 0x4e7dc4 and polls bit 4
	// at 0x4e7dca. Its reset path clears bit 2 and waits for bit 4 low.
	result.dsp_reset_wiring = { 0x10, 0x04, 0x01 };
	// Its CCONT read at 0x4f91c6 selects 0x22, writes 0x2c and polls
	// 0x6d bit 2. Selection bit 2 is clear: the byte starts receive-ready.
	result.gensio_wiring = { 0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x03, true };
	// Keep transport ownership acknowledgements, not the inherited 64-pair
	// compatibility verdict. The 6250's final DSP publication is unproved.
	result.dsp_bootstrap = {
		nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
		0, {}, 0, std::nullopt, std::nullopt, 0
	};
	return result;
}
inline constexpr nokia_product_config PRODUCT_6250 = make_6250_config();

inline constexpr nokia_product_config make_7110_config()
{
	nokia_product_config result;
	// Scanner 0x474004 and normal/special tables at 0x28dcd8/0x28dcf4.
	result.keypad_wiring = KEYPAD_NSE5;
	result.display = { 132, 65, 96, 65, false };
	// NSE-5 v5.01 0x432eae uploads 227 full sparse-flash blocks and
	// one terminal block before 0x432f96 waits for a DSP-owned verdict.
	// Ownership acknowledgements do not establish the final publication.
	result.dsp_bootstrap = {
		nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
		0, {}, 0, std::nullopt, std::nullopt, 0
	};
	return result;
}
inline constexpr nokia_product_config PRODUCT_7110 = make_7110_config();

inline constexpr offs_t NOKIA_RAM_BASE = 0x100000;
inline constexpr offs_t NOKIA_RAM_END = 0x180000;
inline constexpr offs_t NOKIA_FLASH1_BASE = 0x00200000;
inline constexpr uint32_t NOKIA_FLASH_ENTRY = 0x200040;
inline constexpr uint32_t NOKIA_BOOT_HLE_BRANCH =
		0xea000000U | (((NOKIA_FLASH_ENTRY - 8U) >> 2) & 0x00ffffffU);
inline constexpr unsigned GENSIO_TRACE_LIMIT = 20'000;

enum mad2_reg : uint8_t
{
	MAD2_MCU_RESET_CTRL = 0x01,
	MAD2_WATCHDOG = 0x03,
	MAD2_TIMER1_COUNTER_MSB = 0x04,
	MAD2_TIMER1_COUNTER_LSB = 0x05,
	MAD2_TIMER1_DESTINATION_MSB = 0x06,
	MAD2_TIMER1_DESTINATION_LSB = 0x07,
	MAD2_FIQ_STATUS = 0x08,
	MAD2_IRQ_STATUS = 0x09,
	MAD2_FIQ_MASK = 0x0a,
	MAD2_IRQ_MASK = 0x0b,
	MAD2_IRQ_CTRL = 0x0c,
	MAD2_CLOCK_CTRL = 0x0d,
	MAD2_TIMER0_DIVIDER = 0x0f,
	MAD2_TIMER0_COUNTER_MSB = 0x10,
	MAD2_TIMER0_COUNTER_LSB = 0x11,
	MAD2_TIMER0_COMPARE_MSB = 0x12,
	MAD2_TIMER0_COMPARE_LSB = 0x13,
	MAD2_FIQ8_CTRL = 0x16,
	MAD2_MBUS_CTRL = 0x18,
	MAD2_MBUS_STATUS = 0x19,
	MAD2_MBUS_DATA = 0x1a,
	MAD2_CCONT_WRITE = 0x2c,
	MAD2_GENSIO_CONTROL = 0x2d,
	MAD2_LCD_DATA = 0x2e,
	MAD2_CCONT_READ = 0x6c,
	MAD2_LCD_COMMAND = 0x6e,
	MAD2_SIM_TXD = 0x36,
	MAD2_SIM_RXD = 0x37,
	MAD2_SIM_IIR = 0x38,
	MAD2_SIM_CONTROL = 0x39,
	MAD2_SIM_CLOCK = 0x3a,
	MAD2_SIM_RX_FILL = 0x3c,
	MAD2_SIM_RX_FLAGS = 0x3d,
	MAD2_SIM_TX_FLAGS = 0x3e,
	MAD2_SIM_TX_FILL = 0x3f
};

// CCONT serial command/status bits + fixed wiring (hardware constants, not configurable).
// PWRONX is latched as CCONT status bit 1 on a cold power-key boot. It is a
// reset cause sampled by firmware, not one of the upper interrupt sources.
inline constexpr uint8_t KEYPAD_IRQ_LINE_NUM = 0;        // MAD2 keypad/UIF interrupt
inline constexpr uint8_t CCONT_IRQ_LINE_NUM = 2;         // MAD2 IRQ line the CCONT asserts

enum hardware_config : u8
{
	HWCFG_CCONT_READY = 0x01,
	HWCFG_SIM_DEVICE = 0x02,
	HWCFG_DSP_SERVICE = 0x04,
	HWCFG_EXTERNAL_SERVICE = 0x08,
	HWCFG_RADIO_PEER = 0x10,
	HWCFG_PCM_LINK = 0x20
};

} // namespace nokia_dct3

#endif // MAME_NOKIA_NOKIA_DCT3_CONFIG_H
