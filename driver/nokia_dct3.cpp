// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
/*
    Driver for Nokia phones based on the Texas Instruments MAD2 family
    (ARM7TDMI + DSP). The Nokia 3210 uses MAD2PR1; later products use other
    revisions, including MAD2WD1.

    Driver based on documentation found here:
        http://nokix.sourceforge.net/help/blacksphere/sub_050main.htm
        http://tudor.rdslink.ro/MADos/

*/

// if anybody has solid information to aid in the emulation of this (or other phones) please contribute.

#include "emu.h"
#include "emuopts.h"

#include <array>
#include <unordered_map>

#include "cpu/arm7/arm7.h"
#include "machine/i2cmem.h"
#include "machine/intelfsh.h"
#include "sound/beep.h"
#include "speaker.h"
#include "video/pcd8544.h"

#include "nokia_ccont.h"
#include "nokia_b3_flash.h"
#include "nokia_cobba.h"
#include "nokia_dsp_hle.h"
#include "nokia_dspif.h"
#include "nokia_external_service.h"
#include "nokia_gensio.h"
#include "nokia_gsm_network.h"
#include "nokia_gsm_session.h"
#include "nokia_gsm_voice_peer.h"
#include "nokia_lapdm_link.h"
#include "nokia_kbgpio.h"
#include "nokia_mad2.h"
#include "nokia_mad2_pcm.h"
#include "nokia_mbus.h"
#include "nokia_pup.h"
#include "nokia_radio_peer.h"
#include "nokia_sim_card.h"
#include "nokia_simi.h"

#include "emupal.h"
#include "screen.h"

#include <cstring>

#define LOG_CCONT_RTC               (1U << 3)
#define LOG_CCONT_WATCHDOG          (1U << 4)
#define LOG_DISPLAY                 (1U << 5)
#define LOG_DISPLAY_IO              (1U << 6)
#define LOG_DISPLAY_PROFILE         (1U << 7)
#define LOG_DSP_BOUNDARY            (1U << 8)
#define LOG_DSP_SHARED              (1U << 9)
#define LOG_GENSIO                  (1U << 10)
#define LOG_MAD2_CLOCKS             (1U << 12)
#define LOG_MAD2_INTERRUPTS         (1U << 13)
#define LOG_MAD2_LEDGER             (1U << 14)
#define LOG_MAD2_TIMERS             (1U << 15)
#define LOG_MBUS                    (1U << 16)

#define VERBOSE (LOG_CCONT_RTC | LOG_CCONT_WATCHDOG | LOG_DISPLAY | LOG_DISPLAY_IO | \
		LOG_DISPLAY_PROFILE | LOG_DSP_BOUNDARY | LOG_DSP_SHARED | LOG_GENSIO | \
		LOG_MAD2_CLOCKS | LOG_MAD2_INTERRUPTS | LOG_MAD2_LEDGER | \
		LOG_MAD2_TIMERS | LOG_MBUS)
#include "logmacro.h"

namespace {

struct nokia_ccont_board_profile
{
	std::array<u16, 8> channel_defaults;
	std::array<u8, 2> vbatt_channels;
	u8 vbatt_channel_count;
	u8 bsi_channel;
	u8 btemp_channel;
	u8 vchar_channel;
	u16 vchar_connected_raw = 0x03ff;
};

constexpr nokia_ccont_board_profile ADC_DEFAULT = {
	{ 0x000, 0x3ff, 0x3ff, 0x280, 0x200, 0x000, 0x200, 0x000 },
	{ 2, 0 }, 1, 3, 4, 5
};

// NSE-8 routes the early battery input to channel 0 and the monitored battery
// quantity to channel 1. Both paths consume voltage calibration and voltage
// thresholds; BSI remains the independent channel-3 input.
constexpr nokia_ccont_board_profile ADC_3210 = {
	{ 0x2c0, 0x2c0, 0x2d0, 0x280, 0x200, 0x000, 0x200, 0x000 },
	{ 0, 1 }, 2, 3, 4, 5
};
// Standard 3310 routing: channel 2 is VBATT, 3 is the BMC-3 pack's BSI
// resistor and 4 is battery temperature. This tuple clears the firmware's
// ordinary pack/self-test path; it is product input, not a state fixture.
constexpr nokia_ccont_board_profile ADC_STANDARD = {
	{ 0x000, 0x3ff, 0x220, 0x026, 0x200, 0x000, 0x200, 0x000 },
	{ 2, 0 }, 1, 3, 4, 5
};

struct nokia_product_config
{
	u8 power_on_column_mask = 0x04;
	bool boot_rom_bypass = true;
	bool simi_controller = false;
	bool synthetic_sim_card = false;
	bool dsp_service = false;
	bool external_service = false;
	bool radio_peer = false;
	nokia_radio_peer_device::wire_profile radio_wire =
			nokia_radio_peer_device::wire_profile::none;
	nokia_radio_peer_device::acquisition_profile radio_acquisition =
			nokia_radio_peer_device::acquisition_profile::none;
	bool keypad_five_rows = false;
	bool ccont_wddisx_grounded = false;
	unsigned dsp_bootstrap_exchanges = 64;
	bool dsp_bootstrap_ping_pong = false;
	bool dsp_code_block_request = false;
	bool dsp_parked_boot_status = false;
	u16 dsp_boot_status_response = 0;
	unsigned dsp_service_delay_us = 5'000;
	unsigned dsp_peer_poll_ms = 5;
	u8 dsp_parameter_command = 0xff;
	u16 dsp_speech_request_mask = 0;
	u16 dsp_speech_request_value = 0;
	nokia_mad2_pcm_device::bus_profile cobba_pcm;
	nokia_cobba_device::hle_voice_profile cobba_hle_voice;
	bool flash_b3_block_lock = false;
	u8 dsp_reset_running_status = 0;
	u8 dsp_release_mask = 0;
	u8 lcd_controller_width = 84;
	u8 lcd_controller_height = 48;
	u8 lcd_visible_width = 84;
	u8 lcd_visible_height = 48;
	u8 pup_eeprom_scl_bit = 3;
	nokia_ccont_board_profile ccont_board = ADC_DEFAULT;
};

constexpr nokia_product_config make_3210_config()
{
	nokia_product_config result;
	result.power_on_column_mask = 0x01;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.dsp_service = true;
	result.external_service = true;
	result.radio_peer = true;
	result.radio_wire = nokia_radio_peer_device::wire_profile::bitmap_search;
	result.radio_acquisition =
			nokia_radio_peer_device::acquisition_profile::nse8;
	result.dsp_service_delay_us = 4'000;
	result.dsp_peer_poll_ms = 4;
	// Paired NSE-8 firmware independently constructs/removes this field around
	// Answer while retaining the separate 0x0408 dedicated-channel field.
	result.dsp_parameter_command = 0x08;
	result.dsp_speech_request_mask = 0x0201;
	result.dsp_speech_request_value = 0x0201;
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

constexpr nokia_product_config make_3310_config()
{
	nokia_product_config result;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.dsp_service = true;
	result.external_service = true;
	result.radio_peer = true;
	result.radio_wire = nokia_radio_peer_device::wire_profile::candidate_list;
	result.radio_acquisition =
			nokia_radio_peer_device::acquisition_profile::nhm5;
	result.keypad_five_rows = true;
	result.dsp_bootstrap_exchanges = 58;
	result.dsp_service_delay_us = 4'000;
	result.dsp_peer_poll_ms = 4;
	// NHM-5 independently publishes command 0x08 value 0x060b immediately
	// after organic Answer, then 0x040a during physical-End teardown. Those
	// values satisfy and clear the same recovered speech-request field, but do
	// not establish a PCM bus for this product.
	result.dsp_parameter_command = 0x08;
	result.dsp_speech_request_mask = 0x0201;
	result.dsp_speech_request_value = 0x0201;
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
// PCB signal identity.
constexpr nokia_product_config make_3330_config()
{
	nokia_product_config result;
	result.simi_controller = true;
	result.synthetic_sim_card = true;
	result.dsp_service = true;
	result.external_service = true;
	result.keypad_five_rows = true;
	result.dsp_service_delay_us = 4'000;
	result.dsp_peer_poll_ms = 4;
	result.ccont_board = ADC_STANDARD;
	return result;
}

// NHM-2 releases the DSP through reset-control bit 2 and then polls MAD2's
// clock/ready status bit. The 0x53 readback is the observed running state; its
// readiness semantics live in MAD2, while the board wiring remains here.
constexpr nokia_product_config make_3410_config()
{
	nokia_product_config result = make_3330_config();
	result.power_on_column_mask = 0x02;
	result.dsp_bootstrap_ping_pong = true;
	result.dsp_code_block_request = true;
	result.dsp_parked_boot_status = true;
	result.dsp_service_delay_us = 50;
	result.flash_b3_block_lock = true;
	result.dsp_reset_running_status = 0x53;
	result.dsp_release_mask = 0x04;
	result.lcd_controller_width = 102;
	result.lcd_controller_height = 72;
	result.lcd_visible_width = 96;
	result.lcd_visible_height = 65;
	return result;
}

constexpr nokia_product_config make_6110_config()
{
	nokia_product_config result;
	result.power_on_column_mask = 0x01;
	result.boot_rom_bypass = false;
	result.keypad_five_rows = true;
	result.simi_controller = true;
	// NSE-3 v4.06 accepts direct/inverse convention ATRs, parses the T0/TDn
	// interface-byte chain and maps the lab card's TA1=0x05 to PPS ff 00 ff.
	// Compose that removable standards-shaped test card; its subscriber files
	// are fixture policy and do not claim Nokia 6110 product identity.
	result.synthetic_sim_card = true;
	// NSE-3 Chapter 3 documents COBBA-GJ deriving a 1 MHz PCMDClk and
	// 8 kHz PCMSClk, with a sign-extended 13-bit sample in a 16-bit word.
	// Firmware-facing DSP, external-service and radio peers deliberately
	// retain their disabled defaults until an identified NSE-3 ROM is traced.
	// v4.06 statically proves 64 alternating DSP transfer blocks, but that is
	// transfer geometry rather than evidence for the HLE peer's completion
	// counter. NSE-3 projects captured shared word 0x10000 as COBBA identity
	// B06 and a later path compares it against 0x0b06, so the generic HLE ready
	// value 1 is demonstrably incompatible. Keep the peer disabled pending the
	// full DSP-side publication semantics.
	// Its external firmware independently proves the shared type-0x1a/68
	// bitmap wire boundary. Record that separately from the still-unproved
	// NSE-3 acquisition policy; a disabled peer cannot synthesize traffic.
	result.radio_wire = nokia_radio_peer_device::wire_profile::bitmap_search;
	// NSE-3 independently publishes selector 8 as 0x8000 | value[11:0].
	// Decode that proven wire command, but leave the speech-request predicate
	// empty until an organic Answer/End transition identifies its semantics.
	result.dsp_parameter_command = 0x08;
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
	return result;
}

// Preserve the previous 64-exchange behavior for unvalidated products. This is
// an explicit compatibility calibration, not a recovered cross-DCT3 constant.
constexpr nokia_product_config make_conservative_config(u8 power_on_column_mask = 0x04)
{
	nokia_product_config result;
	result.power_on_column_mask = power_on_column_mask;
	return result;
}

constexpr nokia_product_config PRODUCT_3210 = make_3210_config();
constexpr nokia_product_config PRODUCT_3310 = make_3310_config();
constexpr nokia_product_config PRODUCT_3330 = make_3330_config();
constexpr nokia_product_config PRODUCT_3410 = make_3410_config();
constexpr nokia_product_config PRODUCT_6110 = make_6110_config();
constexpr nokia_product_config PRODUCT_DEFAULT = make_conservative_config();
constexpr nokia_product_config PRODUCT_5X10 = make_conservative_config(0x10);
constexpr nokia_product_config PRODUCT_8XXX = make_conservative_config(0x10);

constexpr offs_t NOKIA_RAM_BASE = 0x100000;
constexpr offs_t NOKIA_RAM_END = 0x180000;
constexpr offs_t NOKIA_FLASH1_BASE = 0x00200000;
constexpr offs_t NOKIA_3410_FLASH_STATUS_CSR = 0x003fff00;
constexpr uint32_t NOKIA_FLASH_ENTRY = 0x200040;
constexpr unsigned GENSIO_TRACE_LIMIT = 20'000;

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

constexpr offs_t DSP_TONE_OSCILLATOR_1 = 0x0ae;
constexpr offs_t DSP_TONE_OSCILLATOR_2 = 0x0b0;
constexpr offs_t DSP_TONE_AMPLITUDE = 0x0b6;

// CCONT serial command/status bits + fixed wiring (hardware constants, not configurable).
// PWRONX is latched as CCONT status bit 1 on a cold power-key boot. It is a
// reset cause sampled by firmware, not one of the upper interrupt sources.
constexpr uint8_t KEYPAD_IRQ_LINE_NUM = 0;        // MAD2 keypad/UIF interrupt
constexpr uint8_t CCONT_IRQ_LINE_NUM = 2;         // MAD2 IRQ line the CCONT asserts

enum hardware_config : u8
{
	HWCFG_CCONT_READY = 0x01,
	HWCFG_SIM_DEVICE = 0x02,
	HWCFG_DSP_SERVICE = 0x04,
	HWCFG_EXTERNAL_SERVICE = 0x08,
	HWCFG_RADIO_PEER = 0x10,
	HWCFG_PCM_LINK = 0x20
};

class nokia_dct3_state : public driver_device
{
public:
	nokia_dct3_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_b3_flash(*this, "b3_flash"),
		m_eeprom(*this, "eeprom"),
		m_ccont(*this, "ccont"),
		m_cobba(*this, "cobba"),
		m_gensio(*this, "gensio"),
		m_mad2(*this, "mad2"),
		m_mad2_pcm(*this, "mad2_pcm"),
		m_kbgpio(*this, "kbgpio"),
		m_mbus(*this, "mbus"),
		m_pup(*this, "pup"),
		m_dspif(*this, "dspif"),
		m_dsp_hle(*this, "dsp_hle"),
		m_external_service_peer(*this, "external_service_peer"),
		m_gsm_network(*this, "gsm_network"),
		m_gsm_session(*this, "gsm_session"),
		m_lapdm_link(*this, "lapdm_link"),
		m_radio_peer(*this, "radio_peer"),
		m_simi(*this, "simi"),
		m_sim_card(*this, "sim_card"),
		m_lcd(*this, "lcd"),
		m_buzzer(*this, "buzzer"),
		m_dsp_tone1(*this, "dsp_tone1"),
		m_dsp_tone2(*this, "dsp_tone2"),
		m_vibration(*this, "vibration"),
		m_hw_config(*this, "HWCFG"),
		m_diag_config(*this, "DIAGCFG"),
		m_network_config(*this, "NETCFG")
	{ }

	void noki3330(machine_config &config);
	void noki3410(machine_config &config);
	void noki6110(machine_config &config);
	void noki7110(machine_config &config);
	void noki6210(machine_config &config);
	void dct3_base(machine_config &config);
	void dct3_32mbit_flash_base(machine_config &config);
	void noki3310(machine_config &config);
	void noki3210(machine_config &config);
	void noki5210(machine_config &config);
	void noki8xxx(machine_config &config);

	DECLARE_INPUT_CHANGED_MEMBER(key_irq);
	DECLARE_INPUT_CHANGED_MEMBER(charger_irq);
	DECLARE_INPUT_CHANGED_MEMBER(mbus_rx_byte);

private:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;
	void post_load();
	void apply_product_config(nokia_product_config const &product);


	uint8_t mad2_io_r(offs_t offset);
	void mad2_io_w(offs_t offset, uint8_t data);
	uint8_t mad2_register_r(offs_t offset);
	uint8_t mad2_register_peek(offs_t offset);
	void mad2_register_w(offs_t offset, uint8_t data);
	void trace_mad2_read(offs_t offset, uint8_t data);
	void trace_mad2_write(offs_t offset, uint8_t data, uint8_t old_data);
	uint8_t mad2_dspif_r(offs_t offset);
	void mad2_dspif_w(offs_t offset, uint8_t data);
	uint8_t mad2_mcuif_r(offs_t offset);
	void mad2_mcuif_w(offs_t offset, uint8_t data);

	TIMER_CALLBACK_MEMBER(timer_watchdog);

	uint16_t ram_r(offs_t offset, uint16_t mem_mask = ~0);
	void ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t eeprom_r(offs_t offset, uint16_t mem_mask = ~0);
	void eeprom_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t dsp_ram_r(offs_t offset);
	void dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t flash_r(offs_t offset, uint16_t mem_mask = ~0);
	void flash_firmware_traces(u32 pc, u32 addr);
	void flash_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint32_t rom2_mirror_r(offs_t offset, uint32_t mem_mask = ~0);
	void rom2_mirror_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	void dct3_map(address_map &map) ATTR_COLD;
	void dct3_nse3_map(address_map &map) ATTR_COLD;

	void trace_interrupt_register(char operation, offs_t offset, uint8_t data);
	void mad2_fiq_w(int state);
	void mad2_irq_w(int state);
	void mad2_sleep_w(int state);
	void mad2_irq_ack_w(u16 mask);
	void kbgpio_irq_w(int state);
	void pup_buzzer_clock_w(u32 frequency);
	void pup_buzzer_enable_w(int state);
	void pup_vibrator_w(int state);
	void mad2_reset_w(int state);
	TIMER_CALLBACK_MEMBER(deferred_mad2_reset);
	void ccont_irq_w(int state);
	void ccont_power_w(int state);
	void reset_digital_baseband();
	void sim_irq_w(int state);
	void mbus_fiq2_w(int state);
	void mbus_fiq3_w(int state);
	void mbus_tx_w(u8 data);
	void dsp_fiq0_w(int state);
	void dsp_service_irq_w(int state);
	void dsp_tx_commit_w(int state);
	void dsp_service_pending_w(int state);
	void dsp_doorbell_w(int state);
	void update_dsp_tones();
	// Observation-only helpers implemented in nokia_dct3_trace.inc.
	uint16_t fw_word(offs_t address) const;
	uint8_t fw_byte(offs_t address) const;
	uint32_t fw_dword(offs_t address) const;
	void trace_dsp_audio_shadow_write(
			offs_t address, uint16_t old_data, uint16_t data);
	required_device<cpu_device> m_maincpu;
	required_device<nokia_b3_flash_device> m_b3_flash;
	required_device<i2cmem_device> m_eeprom;
	required_device<nokia_ccont_device> m_ccont;
	required_device<nokia_cobba_device> m_cobba;
	required_device<nokia_gensio_device> m_gensio;
	required_device<nokia_mad2_device> m_mad2;
	required_device<nokia_mad2_pcm_device> m_mad2_pcm;
	required_device<nokia_kbgpio_device> m_kbgpio;
	required_device<nokia_mbus_device> m_mbus;
	required_device<nokia_pup_device> m_pup;
	required_device<nokia_dspif_device> m_dspif;
	required_device<nokia_dsp_hle_device> m_dsp_hle;
	required_device<nokia_external_service_peer_device> m_external_service_peer;
	required_device<nokia_gsm_network_device> m_gsm_network;
	required_device<nokia_gsm_session_device> m_gsm_session;
	required_device<nokia_lapdm_link_device> m_lapdm_link;
	required_device<nokia_radio_peer_device> m_radio_peer;
	required_device<nokia_simi_device> m_simi;
	required_device<nokia_sim_card_device> m_sim_card;
	required_device<pcd8544_device> m_lcd;
	required_device<beep_device> m_buzzer;
	required_device<beep_device> m_dsp_tone1;
	required_device<beep_device> m_dsp_tone2;
	output_finder<> m_vibration;
	optional_ioport m_hw_config;
	optional_ioport m_diag_config;
	optional_ioport m_network_config;

	std::unique_ptr<uint16_t[]>   m_ram;

	nokia_product_config m_product = PRODUCT_DEFAULT;
	bool          m_ccont_irq_state;
	bool          m_baseband_powered = true;

	emu_timer * m_timer_watchdog;

	uint8_t       m_mad2_regs[0x100];
	bool          m_mad2_trace_read[0x100] = {false};
	bool          m_mad2_trace_write[0x100] = {false};
	bool          m_dspif_trace_read[4] = {false};
	bool          m_dspif_trace_write[4] = {false};
	std::unordered_map<uint64_t, uint16_t> m_dsp_shared_trace_reads;
	bool          m_mcuif_trace_read[4] = {false};
	bool          m_mcuif_trace_write[4] = {false};
	uint8_t       m_mcuif_regs[4] = {0};
	unsigned      m_gensio_trace_count = 0;
	unsigned      m_display_io_trace_count = 0;
	unsigned      m_mad2_timer_trace_count = 0;
	unsigned      m_mad2_interrupt_trace_count = 0;
	unsigned      m_mad2_clock_trace_count = 0;
	unsigned      m_mbus_trace_count = 0;

	bool m_trace_enabled = false;
};

static const char * nokia_mad2_reg_desc(uint8_t offset)
{
	switch(offset)
	{
	case 0x00:  return "[CTSI] DCT3 ASIC version Primary hardware version (r)";
	case 0x01:  return "[CTSI] MCU reset control register (rw)";
	case 0x02:  return "[CTSI] DSP reset control register (rw)";
	case 0x03:  return "[CTSI] ASIC watchdog write register (w)";
	case 0x04:  return "[CTSI] Timer 1 counter (MSB) (r)";
	case 0x05:  return "[CTSI] Timer 1 counter (LSB) (r)";
	case 0x06:  return "[CTSI] Timer 1 destination (MSB) (r)";
	case 0x07:  return "[CTSI] Timer 1 destination (LSB) (r)";
	case 0x08:  return "[CTSI] FIQ lines active (rw)";
	case 0x09:  return "[CTSI] IRQ lines active (rw)";
	case 0x0A:  return "[CTSI] FIQ lines mask (rw)";
	case 0x0B:  return "[CTSI] IRQ lines mask (rw)";
	case 0x0C:  return "[CTSI] Interrupt control register (rw)";
	case 0x0D:  return "[CTSI] Peripheral clock gates (bit 5 SIM clock) (rw)";
	case 0x0E:  return "[CTSI] Interrupt trigger register (r)";
	case 0x0F:  return "[CTSI] Programmable timer clock divider (rw)";
	case 0x10:  return "[CTSI] Programmable timer counter (MSB) (r)";
	case 0x11:  return "[CTSI] Programmable timer counter (LSB) (r)";
	case 0x12:  return "[CTSI] Programmable timer destination (MSB) (rw)";
	case 0x13:  return "[CTSI] Programmable timer destination (LSB) (rw)";
	case 0x15:  return "[PUP] PUP control (rw)";
	case 0x16:  return "[CTSI] FIQ 8 (timer?) interrupt control (rw)";
	case 0x18:  return "[MBUS] control (rw)";
	case 0x19:  return "[MBUS] status (rw)";
	case 0x1A:  return "[MBUS] RX/TX (rw)";
	case 0x1B:  return "[PUP] Vibrator (w)";
	case 0x1C:  return "[PUP] Buzzer clock divider high (w)";
	case 0x1D:  return "[PUP] Buzzer clock divider low (w)";
	case 0x1E:  return "[PUP] Buzzer volume (w)";
	case 0x20:  return "[PUP] McuGenIO signal lines (rw)";
	case 0x22:  return "[PUP] ? (?)";
	case 0x24:  return "[PUP] McuGenIO I/O direction (rw)";
	case 0x28:  return "[UIF/KBGPIO] Keyboard ROW signal lines (rw)";
	case 0x29:  return "[UIF/KBGPIO] Keyboard ROW ?? (rw)";
	case 0x2A:  return "[UIF/KBGPIO] Keyboard COL signal lines (rw)";
	case 0x2B:  return "[UIF/KBGPIO] Keyboard COL ?? (rw)";
	case 0x2C:  return "[UIF/GENSIO] CCont write (w)";
	case 0x2D:  return "[UIF/GENSIO] GENSIO start transaction (w)";
	case 0x2E:  return "[UIF/GENSIO] LCD data write (w)";
	case 0x31:  return "[UIF] CTRL I/O 1 signal lines (rw)";
	case 0x32:  return "[UIF] CTRL I/O 2 (rw)";
	case 0x33:  return "[UIF] CTRL I/O 3 (rw)";
	case 0x36:  return "[SIMI] SIM UART TxD (w)";
	case 0x37:  return "[SIMI] SIM UART RxD (r)";
	case 0x38:  return "[SIMI] SIM UART Interrupt Identification (r)";
	case 0x39:  return "[SIMI] SIM Control (rw)";
	case 0x3A:  return "[SIMI] SIM Clock Control (rw)";
	case 0x3B:  return "[SIMI] SIM UART TxD Low Water Mark (?)";
	case 0x3C:  return "[SIMI] SIM UART RxD queue fill (r)";
	case 0x3D:  return "[SIMI] SIM RxD flags (?)";
	case 0x3E:  return "[SIMI] SIM TxD flags (?)";
	case 0x3F:  return "[SIMI] SIM UART TxD queue fill (r)";
	case 0x68:  return "[UIF/KBGPIO] Keyboard ROW ?? 2 (rw)";
	case 0x69:  return "[UIF/KBGPIO] Keyboard ROW interrupt (rw)";
	case 0x6A:  return "[UIF/KBGPIO] Keyboard COL ?? 2 (rw)";
	case 0x6B:  return "[UIF/KBGPIO] Keyboard COL interrupt mask (rw)";
	case 0x6C:  return "[UIF/GENSIO] CCont read (r)";
	case 0x6D:  return "[UIF/GENSIO] GENSIO status (r)";
	case 0x6E:  return "[UIF/GENSIO] LCD command write (w)";
	case 0x6F:  return "[UIF/GENSIO] GENSIO ?? (3/SELECT1) (?)";
	case 0x70:  return "[UIF] CTRL I/O 0 I/O direction (1) (rw)";
	case 0x71:  return "[UIF] CTRL I/O 1 I/O direction (1) (rw)";
	case 0x72:  return "[UIF] CTRL I/O 2 I/O direction (1) (rw)";
	case 0x73:  return "[UIF] CTRL I/O 3 I/O direction (1) (rw)";
	case 0xA8:  return "[UIF/KBGPIO] Keyboard ROW I/O direction (rw)";
	case 0xA9:  return "[UIF/KBGPIO] Keyboard ROW ?? 3 (rw)";
	case 0xAA:  return "[UIF/KBGPIO] Keyboard COL I/O direction 0=in 1=out (rw)";
	case 0xAB:  return "[UIF/KBGPIO] Keyboard COL ?? 3 (rw)";
	case 0xAD:  return "[UIF/GENSIO] GENSIO ?? (1/SELECT2) (?)";
	case 0xAE:  return "[UIF/GENSIO] GENSIO ?? (2/SELECT2) (?)";
	case 0xAF:  return "[UIF/GENSIO] GENSIO ?? (3/SELECT2) (?)";
	case 0xB0:  return "[UIF] CTRL I/O 0 I/O direction (2) (rw)";
	case 0xB1:  return "[UIF] CTRL I/O 1 I/O direction (2) (rw)";
	case 0xB2:  return "[UIF] CTRL I/O 2 I/O direction (2) (rw)";
	case 0xB3:  return "[UIF] CTRL I/O 3 I/O direction (2) (rw)";
	case 0xED:  return "[UIF/GENSIO] GENSIO ?? (1/SELECT3) (?)";
	case 0xEE:  return "[UIF/GENSIO] GENSIO ?? (2/SELECT3) (?)";
	case 0xEF:  return "[UIF/GENSIO] GENSIO ?? (3/SELECT3) (?)";
	case 0xF0:  return "[UIF] CTRL I/O 0 input (r)";
	case 0xF1:  return "[UIF] CTRL I/O 1 input (r)";
	case 0xF2:  return "[UIF] CTRL I/O 2 input (r)";
	case 0xF3:  return "[UIF] CTRL I/O 3 input (r)";
	default:    return "<Unknown>";
	}
}

// ============================================================================
// Firmware-address traces are quarantined in nokia_dct3_trace.inc. Add no
// forced firmware results or messages. See docs/driver_structure.md.
// ============================================================================
void nokia_dct3_state::machine_start()
{
	// Passive research logging uses MAME's standard -verbose switch.  Product
	// composition and hardware timing are configured by machine_config.
	m_trace_enabled = machine().options().verbose();
	m_pup->set_trace(m_trace_enabled);
	m_ram = std::make_unique<uint16_t[]>((NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1);

	m_timer_watchdog = timer_alloc(FUNC(nokia_dct3_state::timer_watchdog), this);
	save_pointer(NAME(m_ram), (NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1);
	save_item(NAME(m_ccont_irq_state));
	save_item(NAME(m_baseband_powered));
	save_item(NAME(m_mad2_regs));
	save_item(NAME(m_mcuif_regs));
	machine().save().register_postload(save_prepost_delegate(FUNC(nokia_dct3_state::post_load), this));
}

void nokia_dct3_state::post_load()
{
	update_dsp_tones();
}

void nokia_dct3_state::apply_product_config(nokia_product_config const &product)
{
	m_product = product;
	m_dsp_hle->set_bootstrap_exchange_limit(product.dsp_bootstrap_exchanges);
	m_dsp_hle->set_bootstrap_ping_pong(product.dsp_bootstrap_ping_pong);
	m_dsp_hle->set_code_block_request(product.dsp_code_block_request);
	m_dsp_hle->set_parked_boot_status(product.dsp_parked_boot_status,
			product.dsp_boot_status_response);
	m_mad2->set_dsp_reset_running_status(product.dsp_reset_running_status);
	m_mad2->set_dsp_release_mask(product.dsp_release_mask);
	m_kbgpio->set_five_rows(product.keypad_five_rows);
	m_kbgpio->set_power_on_column_mask(product.power_on_column_mask);
	m_pup->set_eeprom_scl_bit(product.pup_eeprom_scl_bit);
	m_lcd->set_geometry(product.lcd_controller_width, product.lcd_controller_height,
			product.lcd_visible_width, product.lcd_visible_height);
	screen_device *const screen = subdevice<screen_device>("screen");
	screen->set_size(product.lcd_visible_width, product.lcd_visible_height);
	screen->set_visarea(0, product.lcd_visible_width - 1, 0, product.lcd_visible_height - 1);
	m_dsp_hle->set_service_enabled(product.dsp_service);
	m_dsp_hle->set_external_service_enabled(product.external_service);
	m_dsp_hle->set_service_delay_us(product.dsp_service_delay_us);
	m_dsp_hle->set_peer_poll_ms(product.dsp_peer_poll_ms);
	m_dsp_hle->set_parameter_command(product.dsp_parameter_command);
	m_dsp_hle->set_speech_request_policy(
			product.dsp_speech_request_mask,
			product.dsp_speech_request_value);
	m_mad2_pcm->set_bus_profile(product.cobba_pcm);
	m_cobba->set_pcm_sample_bits(product.cobba_pcm.sample_bits);
	// This explicitly configures the HLE's internal-handset path. A future
	// real DSP backend must instead select paths through COBBA's serial
	// control transport; MCU speech state must never mutate this fallback.
	m_cobba->set_hle_voice_profile(product.cobba_hle_voice);
	m_external_service_peer->set_enabled(product.external_service);
	m_radio_peer->set_enabled(product.radio_peer);
	m_radio_peer->set_wire_profile(product.radio_wire);
	m_radio_peer->set_acquisition_profile(product.radio_acquisition);
	m_b3_flash->set_enabled(product.flash_b3_block_lock);
}

void nokia_dct3_state::machine_reset()
{
	std::fill_n(m_ram.get(), (NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1, 0);

	// according to the boot rom disassembly here http://www.nokix.pasjagsm.pl/help/blacksphere/sub_100hardware/sub_arm/sub_bootrom.htm
	// flash entry point is at 0x200040, we can probably reassemble the above code, but for now this should be enough.
	// Existing executable profiles retain the historical boot-ROM bypass.
	// Products whose boot ROM has not been identified must start at the ARM
	// reset vector instead of inheriting a later firmware entry assumption.
	if (m_product.boot_rom_bypass)
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R15, NOKIA_FLASH_ENTRY);

	memset(m_mad2_regs, 0, 0x100);
	update_dsp_tones();
	std::fill(std::begin(m_mad2_trace_read), std::end(m_mad2_trace_read), false);
	std::fill(std::begin(m_mad2_trace_write), std::end(m_mad2_trace_write), false);
	std::fill(std::begin(m_dspif_trace_read), std::end(m_dspif_trace_read), false);
	std::fill(std::begin(m_dspif_trace_write), std::end(m_dspif_trace_write), false);
	m_dsp_shared_trace_reads.clear();
	std::fill(std::begin(m_mcuif_trace_read), std::end(m_mcuif_trace_read), false);
	std::fill(std::begin(m_mcuif_trace_write), std::end(m_mcuif_trace_write), false);
	std::fill(std::begin(m_mcuif_regs), std::end(m_mcuif_regs), 0);
	m_gensio_trace_count = 0;
	m_display_io_trace_count = 0;
	m_mad2_timer_trace_count = 0;
	m_mad2_interrupt_trace_count = 0;
	m_mad2_clock_trace_count = 0;
	m_mbus_trace_count = 0;
	const u8 hardware = m_hw_config.read_safe(0x3f);
	if (BIT(m_diag_config.read_safe(0x00), 0))
		machine().logerror("dspif_fixture: conformance=%02x\n", m_dspif->run_conformance_checks());
	if (BIT(m_diag_config.read_safe(0x00), 1))
		machine().logerror("cobba_fixture: control_conformance=%02x\n",
				m_cobba->run_control_conformance_checks());
	// Load the deterministic product-level selector tuple. Electrical signal
	// names and units remain deliberately unassigned where board evidence is absent.
	for (unsigned id = 0; id < 8; id++)
		m_ccont->set_adc_source(id, m_product.ccont_board.channel_defaults[id]);
	m_ccont->set_wddisx_grounded(m_product.ccont_wddisx_grounded);
	m_ccont->set_ready(BIT(hardware, 0));
	m_simi->set_enabled(m_product.simi_controller && BIT(hardware, 1));
	m_simi->set_card_present(m_product.synthetic_sim_card && BIT(hardware, 1));
	m_dsp_hle->set_service_enabled(m_product.dsp_service && BIT(hardware, 2));
	m_dsp_hle->set_external_service_enabled(m_product.external_service && BIT(hardware, 3));
	m_external_service_peer->set_enabled(m_product.external_service && BIT(hardware, 3));
	m_radio_peer->set_enabled(m_product.radio_peer && BIT(hardware, 4));
	m_mad2_pcm->set_enabled(BIT(hardware, 5));
	const u8 network = m_network_config.read_safe(0x00);
	m_radio_peer->set_page_after_registration(
			BIT(network, 0) || BIT(network, 1) || BIT(network, 2) ||
			BIT(network, 3));
	m_radio_peer->set_incoming_call_after_registration(BIT(network, 1));
	m_radio_peer->set_incoming_sms_after_registration(BIT(network, 2));
	m_radio_peer->set_incoming_smart_message_after_registration(BIT(network, 3));
	m_radio_peer->set_speech_loopback(BIT(network, 4));
	m_radio_peer->set_lab_voice_source(BIT(network, 5));
	m_radio_peer->set_downlink_tch_burst_error_profile(
			BIT(network, 6) ? 144 : 0, BIT(network, 6) ? 4 : 0);
	m_radio_peer->set_uplink_tch_burst_error_profile(
			BIT(network, 7) ? 144 : 0, BIT(network, 7) ? 4 : 0);
	m_sim_card->set_cphs_aoc(false);
	m_sim_card->set_cached_location(false);
	const u8 atr[] = { 0x3b, 0x10, 0x05 };
	m_sim_card->set_atr(atr, std::size(atr));
	m_eeprom->write_scl(1);
	m_ccont_irq_state = false;
	m_timer_watchdog->adjust(attotime::from_hz(1), 0, attotime::from_hz(1));
}

void nokia_dct3_state::mad2_fiq_w(int state)
{
	m_maincpu->set_input_line(1, state ? ASSERT_LINE : CLEAR_LINE);
}

void nokia_dct3_state::mad2_irq_w(int state)
{
	m_maincpu->set_input_line(0, state ? ASSERT_LINE : CLEAR_LINE);
}

void nokia_dct3_state::mad2_sleep_w(int state)
{
	if (state)
		m_maincpu->suspend(SUSPEND_REASON_HALT, true);
	else
		m_maincpu->resume(SUSPEND_REASON_HALT);
}

void nokia_dct3_state::mad2_reset_w(int state)
{
	if (state)
		machine().scheduler().synchronize(
				timer_expired_delegate(FUNC(nokia_dct3_state::deferred_mad2_reset), this));
}

TIMER_CALLBACK_MEMBER(nokia_dct3_state::deferred_mad2_reset)
{
	reset_digital_baseband();
	// Bit 2 records the MCU-initiated reset that brought the new boot up;
	// device_reset supplies the persistent power-reset bit 0.
	m_mad2->set_reset_cause(0x04);
}

void nokia_dct3_state::mad2_irq_ack_w(u16 mask)
{
	if (mask & (u16(1) << KEYPAD_IRQ_LINE_NUM))
		m_kbgpio->irq_acknowledge();
}

void nokia_dct3_state::kbgpio_irq_w(int state)
{
	const u16 before = m_mad2->irq_status();
	m_mad2->set_irq_line(KEYPAD_IRQ_LINE_NUM, state);
	const u16 after = m_mad2->irq_status();
	if (before != after && m_trace_enabled && m_mad2_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2_INTERRUPTS, "mad2_interrupt: event=levels domain=IRQ keypad=%u ccont=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				state, m_ccont_irq_state, before, after, machine().time().as_double());
}

void nokia_dct3_state::pup_vibrator_w(int state)
{
	m_vibration = state;
}

void nokia_dct3_state::pup_buzzer_clock_w(u32 frequency)
{
	m_buzzer->set_clock(frequency);
}

void nokia_dct3_state::pup_buzzer_enable_w(int state)
{
	m_buzzer->set_state(state);
}

void nokia_dct3_state::ccont_irq_w(int state)
{
	// MAD2 latches the rising CCONT indication. Its IRQ acknowledgement clears
	// that pending edge; CCONT retains the source in register 0x0e until the
	// deferred firmware service acknowledges it through GENSIO.
	if (state && !m_ccont_irq_state)
		m_mad2->assert_irq(CCONT_IRQ_LINE_NUM);
	if (m_trace_enabled && state != m_ccont_irq_state)
		LOGMASKED(LOG_CCONT_RTC, "ccont_route: state=%u irq_line=%u pending=%03x mask=%02x ctrl=%02x t=%.9f\n",
			state, CCONT_IRQ_LINE_NUM, m_mad2->irq_status(), m_mad2->reg(MAD2_IRQ_MASK),
			m_mad2->reg(MAD2_IRQ_CTRL), machine().time().as_double());
	m_ccont_irq_state = bool(state);
}

void nokia_dct3_state::ccont_power_w(int state)
{
	if (!state && m_baseband_powered)
	{
		m_baseband_powered = false;
		m_maincpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	}
	else if (state && !m_baseband_powered)
	{
		// CCONT controls the digital baseband rails. A charger-originated rising
		// edge restarts the complete MAD2 digital domain; CCONT itself retains
		// the reset-cause latch, while flash and EEPROM retain their contents.
		reset_digital_baseband();
		m_baseband_powered = true;
		m_maincpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
	}
}

void nokia_dct3_state::reset_digital_baseband()
{
	// These blocks share the switched digital-baseband domain. CCONT, flash and
	// EEPROM intentionally survive this reset and retain their state.
	m_maincpu->reset();
	m_mad2->reset();
	m_kbgpio->reset();
	m_kbgpio->clear_power_on_latch();
	m_pup->reset();
	m_gensio->reset();
	m_mbus->reset();
	m_dspif->reset();
	m_dsp_hle->reset();
	m_external_service_peer->reset();
	m_gsm_session->reset();
	m_lapdm_link->reset();
	m_radio_peer->reset();
	m_simi->reset();
	m_sim_card->reset();
	m_lcd->reset();
	// nokia_gsm_network_device contains immutable cell data; the session, link
	// and radio peers above own the reset-sensitive protocol phases.
	machine_reset();
}

void nokia_dct3_state::sim_irq_w(int state)
{
	if (state)
		m_mad2->assert_fiq(6);
}

void nokia_dct3_state::dsp_fiq0_w(int state)
{
	if (state)
		m_mad2->assert_fiq(0);
}

void nokia_dct3_state::dsp_service_irq_w(int state)
{
	if (state)
		m_mad2->assert_irq(4);
}

void nokia_dct3_state::dsp_tx_commit_w(int state)
{
	m_dsp_hle->tx_commit_w(state);
}

void nokia_dct3_state::dsp_service_pending_w(int state)
{
	m_dsp_hle->service_pending_w(state);
}

void nokia_dct3_state::dsp_doorbell_w(int state)
{
	m_dsp_hle->doorbell_w(state);
}

void nokia_dct3_state::mbus_fiq2_w(int state)
{
	if (state)
		m_mad2->assert_fiq(2);
}

void nokia_dct3_state::mbus_fiq3_w(int state)
{
	if (state)
		m_mad2->assert_fiq(3);
}

void nokia_dct3_state::mbus_tx_w(u8 data)
{
	if (m_trace_enabled && m_mbus_trace_count++ < 8192)
		LOGMASKED(LOG_MBUS, "mbus: event=TX data=%02x pc=%08x t=%.9f\n", data,
				m_maincpu->pc(), machine().time().as_double());
}

void nokia_dct3_state::trace_interrupt_register(char operation, offs_t offset, uint8_t data)
{
	if (m_trace_enabled &&
			m_mad2_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2_INTERRUPTS, "mad2_interrupt: event=reg_%c off=%02x data=%02x fiq=%03x irq=%03x fiqmask=%02x irqmask=%02x ctrl=%02x extctrl=%02x t=%.9f\n",
				operation, u32(offset), data, m_mad2->fiq_status(), m_mad2->irq_status(),
				m_mad2->reg(MAD2_FIQ_MASK), m_mad2->reg(MAD2_IRQ_MASK),
				m_mad2->reg(MAD2_IRQ_CTRL), m_mad2->reg(MAD2_FIQ8_CTRL),
				machine().time().as_double());
}

TIMER_CALLBACK_MEMBER(nokia_dct3_state::timer_watchdog)
{
	// CCONT watchdog
	if (m_ccont->watchdog_tick())
	{
		if (m_trace_enabled)
				LOGMASKED(LOG_CCONT_WATCHDOG, "ccont_watchdog_expired: t=%.6f\n", machine().time().as_double());
		// CCONT supervises the switched digital-baseband domain, so expiry has
		// the same reset extent as a CCONT-controlled rail restart. CCONT itself,
		// flash and EEPROM remain outside this domain and retain their state.
		reset_digital_baseband();
	}

	// MAD2 watchdog
	if (m_mad2->watchdog_tick())
	{
		// The ASIC watchdog resets the same digital baseband that an explicit
		// MCU reset-control request does; only the retained cause differs.
		reset_digital_baseband();
		m_mad2->set_reset_cause(0x02);
	}
}

// Hardware RAM read entry point (registered in the address map).
uint16_t nokia_dct3_state::ram_r(offs_t offset, uint16_t mem_mask)
{
	return m_ram[offset] & mem_mask;
}

// Hardware RAM write entry point registered in the address map.
void nokia_dct3_state::ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	const uint16_t old_data = m_ram[offset];
	COMBINE_DATA(&m_ram[offset]);
	const offs_t address = NOKIA_RAM_BASE + (offset << 1);
	if (m_trace_enabled && old_data != m_ram[offset])
		trace_dsp_audio_shadow_write(address, old_data, m_ram[offset]);
}


uint16_t nokia_dct3_state::eeprom_r(offs_t offset, uint16_t mem_mask)
{
	memory_region *eeprom = memregion("eeprom");
	uint16_t data = 0xffff;

	if (eeprom && offset < (eeprom->bytes() / 2))
		data = eeprom->as_u16(offset);

	return data & mem_mask;
}

void nokia_dct3_state::eeprom_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
}

uint16_t nokia_dct3_state::dsp_ram_r(offs_t offset)
{
	offset &= 0x7ff;
	const uint16_t data = m_dspif->shared_r(offset);
	const uint32_t pc = m_maincpu->pc();
	const uint64_t trace_key = (uint64_t(pc) << 11) | offset;
	const unsigned byte_offset = offset << 1;
	if (m_trace_enabled &&
			(byte_offset <= 0x004 || byte_offset == 0x0a6 ||
			 byte_offset == 0x0a8 || byte_offset == 0x0aa || byte_offset == 0x0e0 ||
			 byte_offset == 0x0e4 || byte_offset == 0x0fe || byte_offset == 0x100 ||
			 byte_offset == 0x1c8))
		LOGMASKED(LOG_DSP_SHARED, "dsp_shared_observe: off=%03x data=%04x pc=%08x t=%.6f\n",
				byte_offset, data, pc, machine().time().as_double());
	if (m_trace_enabled)
	{
		auto [trace_item, inserted] = m_dsp_shared_trace_reads.emplace(trace_key, data);
		if (inserted || trace_item->second != data)
		{
			trace_item->second = data;
				LOGMASKED(LOG_DSP_SHARED, "dsp_shared_read: off=%03x data=%04x pc=%08x t=%.6f\n",
					byte_offset, data, pc, machine().time().as_double());
		}
	}
	return data;
}

void nokia_dct3_state::dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	offset &= 0x7ff;
	const uint16_t old_data = m_dspif->shared_word(offset);
	m_dspif->shared_w(offset, data, mem_mask);
	const uint16_t new_data = m_dspif->shared_word(offset);
	const unsigned byte_offset = offset << 1;
	if (m_trace_enabled && old_data != new_data)
		LOGMASKED(LOG_DSP_SHARED,
				"dsp_shared_write: off=%03x old=%04x data=%04x pc=%08x t=%.6f\n",
				byte_offset, old_data, new_data, m_maincpu->pc(),
				machine().time().as_double());
	if (byte_offset == DSP_TONE_OSCILLATOR_1 || byte_offset == DSP_TONE_OSCILLATOR_2 || byte_offset == DSP_TONE_AMPLITUDE)
		update_dsp_tones();
}

#include "nokia_dct3_trace.inc"

uint16_t nokia_dct3_state::flash_r(offs_t offset, uint16_t mem_mask)
{
	const u32 pc = m_maincpu->pc();
	const u32 addr = NOKIA_FLASH1_BASE + (offset << 1);
	flash_firmware_traces(pc, addr);
	return m_b3_flash->read(offset, mem_mask);
}

void nokia_dct3_state::flash_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	m_b3_flash->write(offset, data, mem_mask);
}

uint32_t nokia_dct3_state::rom2_mirror_r(offs_t offset, uint32_t mem_mask)
{
	memory_region *flash = memregion("flash");
	if (!flash || flash->bytes() == 0)
		return 0xffffffff;

	const offs_t byte_addr = (offset << 2) % flash->bytes();
	const uint8_t *base = flash->base();
	const uint32_t b0 = base[(byte_addr + 0) % flash->bytes()];
	const uint32_t b1 = base[(byte_addr + 1) % flash->bytes()];
	const uint32_t b2 = base[(byte_addr + 2) % flash->bytes()];
	const uint32_t b3 = base[(byte_addr + 3) % flash->bytes()];
	return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

void nokia_dct3_state::rom2_mirror_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
}

uint8_t nokia_dct3_state::mad2_io_r(offs_t offset)
{
	const uint8_t data = mad2_register_r(offset);
	trace_mad2_read(offset, data);
	return data;
}

uint8_t nokia_dct3_state::mad2_register_r(offs_t offset)
{
	uint8_t data = nokia_pup_device::owns(offset) ? m_pup->read(offset) :
			(nokia_kbgpio_device::owns(offset) ? m_kbgpio->read(offset) :
			(offset <= MAD2_FIQ8_CTRL ? m_mad2->read(offset) :
			(offset >= MAD2_MBUS_CTRL && offset <= 0x1a ? m_mbus->read(offset - MAD2_MBUS_CTRL) :
			(nokia_gensio_device::owns(offset) ? m_gensio->read(offset) : m_mad2_regs[offset]))));

	switch(offset)
	{
		case 0x37:  // SIM UART RxD
			if (m_simi->enabled())
				data = m_simi->rxd_r();
			break;
		case 0x38:  // SIM UART interrupt identification
			if (m_simi->enabled())
				data = m_simi->iir_r();
			break;
		case 0x39:  // SIM control and live-interface status
			if (m_simi->enabled())
				data = m_simi->control_r();
			break;
		case 0x3c:  // SIM UART RxD queue fill
			if (m_simi->enabled())
				data = m_simi->rx_count_r();
			break;
		case 0x3f:  // SIM UART TxD queue fill
			if (m_simi->enabled())
				data = m_simi->tx_count_r();
			break;
	}

	return data;
}

void nokia_dct3_state::trace_mad2_read(offs_t offset, uint8_t data)
{
	if (m_trace_enabled &&
			nokia_gensio_device::owns(offset) &&
			m_gensio_trace_count++ < GENSIO_TRACE_LIMIT)
		LOGMASKED(LOG_GENSIO, "gensio: R off=%02x data=%02x pc=%08x t=%.9f\n", offset, data,
				m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled &&
			((offset >= 0x08 && offset <= 0x13) || offset == 0x0a) &&
			m_mad2_timer_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2_TIMERS, "mad2_timer: event=R off=%02x data=%02x pc=%08x t=%.9f\n",
				u32(offset), data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled &&
			(offset == MAD2_MCU_RESET_CTRL || offset == MAD2_WATCHDOG ||
			 (offset >= MAD2_TIMER1_COUNTER_MSB && offset <= MAD2_TIMER1_DESTINATION_LSB) ||
			 offset == MAD2_CLOCK_CTRL) && m_mad2_clock_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2_CLOCKS, "mad2_clock: event=R off=%02x data=%02x counter=%04x pc=%08x t=%.9f\n",
				u32(offset), data, m_mad2->timer1_counter(), m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled &&
			(offset == MAD2_MBUS_CTRL || offset == MAD2_MBUS_STATUS || offset == MAD2_MBUS_DATA ||
			 offset == MAD2_FIQ_STATUS || offset == MAD2_FIQ_MASK) && m_mbus_trace_count++ < 8192)
		LOGMASKED(LOG_MBUS, "mbus: event=R off=%02x data=%02x ctrl=%02x status=%02x fiq=%03x mask=%02x pc=%08x t=%.9f\n",
				u32(offset), data, m_mbus->control(), m_mbus->status(),
				m_mad2->fiq_status(), m_mad2->reg(MAD2_FIQ_MASK), m_maincpu->pc(), machine().time().as_double());
	if (offset == MAD2_FIQ_STATUS || offset == MAD2_IRQ_STATUS ||
			offset == MAD2_FIQ_MASK || offset == MAD2_IRQ_MASK ||
			offset == MAD2_IRQ_CTRL || offset == MAD2_FIQ8_CTRL)
		trace_interrupt_register('R', offset, data);

	if (m_trace_enabled && !m_mad2_trace_read[offset])
	{
		m_mad2_trace_read[offset] = true;
			LOGMASKED(LOG_MAD2_LEDGER, "mad2_ledger: R off=%02x data=%02x pc=%08x t=%.6f %s\n", offset, data,
				m_maincpu->pc(), machine().time().as_double(), nokia_mad2_reg_desc(offset));
	}
}

void nokia_dct3_state::mad2_io_w(offs_t offset, uint8_t data)
{
	const uint8_t old_data = mad2_register_peek(offset);
	mad2_register_w(offset, data);
	trace_mad2_write(offset, data, old_data);
}

uint8_t nokia_dct3_state::mad2_register_peek(offs_t offset)
{
	const bool core_register = offset <= MAD2_FIQ8_CTRL && !nokia_pup_device::owns(offset);
	const bool mbus_register = offset >= MAD2_MBUS_CTRL && offset <= MAD2_MBUS_DATA;
	const bool gensio_register = nokia_gensio_device::owns(offset);
	return nokia_pup_device::owns(offset) ? m_pup->peek(offset) :
			(nokia_kbgpio_device::owns(offset) ? m_kbgpio->peek(offset) : core_register ? m_mad2->read(offset) :
			(mbus_register ? (offset == MAD2_MBUS_CTRL ? m_mbus->control() :
				offset == MAD2_MBUS_STATUS ? m_mbus->status() : m_mbus->data()) : gensio_register ?
				m_gensio->peek(offset) :
				m_mad2_regs[offset]));
}

void nokia_dct3_state::mad2_register_w(offs_t offset, uint8_t data)
{
	const bool pup_register = nokia_pup_device::owns(offset);
	const bool kbgpio_register = nokia_kbgpio_device::owns(offset);
	const bool core_register = offset <= MAD2_FIQ8_CTRL && !pup_register;
	const bool mbus_register = offset >= MAD2_MBUS_CTRL && offset <= MAD2_MBUS_DATA;
	const bool gensio_register = nokia_gensio_device::owns(offset);
	if (pup_register)
		m_pup->write(offset, data);
	else if (kbgpio_register)
		m_kbgpio->write(offset, data);
	else if (core_register)
		m_mad2->write(offset, data);
	else if (mbus_register)
		m_mbus->write(offset - MAD2_MBUS_CTRL, data);
	else if (gensio_register)
		m_gensio->write(offset, data);
	else
		m_mad2_regs[offset] = data;

	if (offset == MAD2_SIM_TXD && m_simi->enabled())
		m_simi->txd_w(data);
	else if (offset == MAD2_SIM_IIR && m_simi->enabled())
		m_simi->iir_w(data);
	else if (offset == MAD2_SIM_CONTROL && m_simi->enabled())
		m_simi->control_w(data);
	else if (offset == MAD2_SIM_RX_FLAGS && m_simi->enabled())
		m_simi->rx_fifo_control_w(data);
	else if (offset == MAD2_SIM_TX_FLAGS && m_simi->enabled())
		m_simi->tx_fifo_control_w(data);
}

void nokia_dct3_state::trace_mad2_write(offs_t offset, uint8_t data, uint8_t old_data)
{
	const bool gensio_register = nokia_gensio_device::owns(offset);
	if (m_trace_enabled &&
			(offset >= 0x08 && offset <= 0x13) &&
			m_mad2_timer_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2_TIMERS, "mad2_timer: event=W off=%02x data=%02x old=%02x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled &&
			(offset == MAD2_MCU_RESET_CTRL || offset == MAD2_WATCHDOG ||
			 (offset >= MAD2_TIMER1_COUNTER_MSB && offset <= MAD2_TIMER1_DESTINATION_LSB) ||
			 offset == MAD2_CLOCK_CTRL) && m_mad2_clock_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2_CLOCKS, "mad2_clock: event=W off=%02x data=%02x old=%02x counter=%04x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_mad2->timer1_counter(), m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled &&
			(offset == MAD2_MBUS_CTRL || offset == MAD2_MBUS_STATUS || offset == MAD2_MBUS_DATA ||
			 offset == MAD2_FIQ_STATUS || offset == MAD2_FIQ_MASK) && m_mbus_trace_count++ < 8192)
		LOGMASKED(LOG_MBUS, "mbus: event=W off=%02x data=%02x old=%02x ctrl=%02x status=%02x fiq=%03x mask=%02x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_mbus->control(), m_mbus->status(),
				m_mad2->fiq_status(), m_mad2->reg(MAD2_FIQ_MASK), m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled &&
			gensio_register &&
			m_gensio_trace_count++ < GENSIO_TRACE_LIMIT)
		LOGMASKED(LOG_GENSIO, "gensio: W off=%02x data=%02x old=%02x pc=%08x t=%.9f\n", offset, data,
				old_data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled &&
			(offset == MAD2_GENSIO_CONTROL || offset == MAD2_LCD_DATA ||
			 offset == MAD2_LCD_COMMAND) &&
			m_display_io_trace_count++ < 4096)
		LOGMASKED(LOG_DISPLAY_IO, "display_io: off=%02x data=%02x old=%02x pc=%08x t=%.9f\n", offset,
				data, old_data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace_enabled && !m_mad2_trace_write[offset])
	{
		m_mad2_trace_write[offset] = true;
			LOGMASKED(LOG_MAD2_LEDGER, "mad2_ledger: W off=%02x data=%02x old=%02x pc=%08x t=%.6f %s\n", offset,
				data, old_data, m_maincpu->pc(), machine().time().as_double(), nokia_mad2_reg_desc(offset));
	}

	if (offset == MAD2_FIQ_STATUS || offset == MAD2_IRQ_STATUS ||
			offset == MAD2_FIQ_MASK || offset == MAD2_IRQ_MASK ||
			offset == MAD2_IRQ_CTRL || offset == MAD2_FIQ8_CTRL)
		trace_interrupt_register('W', offset, data);
	if (m_trace_enabled &&
			offset == MAD2_IRQ_STATUS && BIT(data, CCONT_IRQ_LINE_NUM))
		LOGMASKED(LOG_CCONT_RTC, "ccont_route: event=mad_ack data=%02x pc=%08x t=%.9f\n",
			data, m_maincpu->pc(), machine().time().as_double());
}

void nokia_dct3_state::update_dsp_tones()
{
	// The ROM-4 MCU programs the COBBA tone oscillators in quarter-Hz units.
	// A real DSP renders them through the codec; this HLE voice exposes the same
	// firmware-owned command while no DSP core or codec PCM backend is present.
	const u16 oscillator1 = m_dspif->shared_r(DSP_TONE_OSCILLATOR_1 >> 1);
	const u16 oscillator2 = m_dspif->shared_r(DSP_TONE_OSCILLATOR_2 >> 1);
	const u16 amplitude = m_dspif->shared_r(DSP_TONE_AMPLITUDE >> 1);
	const unsigned frequency1 = oscillator1 >> 2;
	const unsigned frequency2 = oscillator2 >> 2;
	if (frequency1 != 0)
		m_dsp_tone1->set_clock(frequency1);
	if (frequency2 != 0)
		m_dsp_tone2->set_clock(frequency2);
	m_dsp_tone1->set_state(amplitude != 0 && frequency1 != 0);
	m_dsp_tone2->set_state(amplitude != 0 && frequency2 != 0);
	if (m_trace_enabled)
		LOGMASKED(LOG_DSP_BOUNDARY, "dsp_tone: oscillator=%04x/%04x frequency=%u/%u amplitude=%04x active=%u/%u t=%.6f\n",
				oscillator1, oscillator2, frequency1, frequency2, amplitude,
				amplitude != 0 && frequency1 != 0, amplitude != 0 && frequency2 != 0,
				machine().time().as_double());
}

uint8_t nokia_dct3_state::mad2_dspif_r(offs_t offset)
{
	offset &= 3;
	const u8 data = m_dspif->dspif_r(offset);
	if (m_trace_enabled && !m_dspif_trace_read[offset])
	{
		m_dspif_trace_read[offset] = true;
			LOGMASKED(LOG_MAD2_LEDGER, "mad2_ledger: R bus=DSPIF off=%02x data=%02x pc=%08x t=%.6f DSP API control\n",
				u32(offset), data, m_maincpu->pc(), machine().time().as_double());
	}
	return data;
}

void nokia_dct3_state::mad2_dspif_w(offs_t offset, uint8_t data)
{
	offset &= 3;
	const u8 old_data = m_dspif->dspif_r(offset);
	if (m_trace_enabled && !m_dspif_trace_write[offset])
	{
		m_dspif_trace_write[offset] = true;
			LOGMASKED(LOG_MAD2_LEDGER, "mad2_ledger: W bus=DSPIF off=%02x data=%02x old=%02x pc=%08x t=%.6f DSP API control\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	}
	m_dspif->dspif_w(offset, data);
}

uint8_t nokia_dct3_state::mad2_mcuif_r(offs_t offset)
{
	offset &= 3;
	const u8 data = m_mcuif_regs[offset];
	if (m_trace_enabled && !m_mcuif_trace_read[offset])
	{
		m_mcuif_trace_read[offset] = true;
			LOGMASKED(LOG_MAD2_LEDGER, "mad2_ledger: R bus=MCUIF off=%02x data=%02x pc=%08x t=%.6f memory-window control\n",
				u32(offset), data, m_maincpu->pc(), machine().time().as_double());
	}
	return data;
}

void nokia_dct3_state::mad2_mcuif_w(offs_t offset, uint8_t data)
{
	offset &= 3;
	const u8 old_data = m_mcuif_regs[offset];
	m_mcuif_regs[offset] = data;
	if (m_trace_enabled && !m_mcuif_trace_write[offset])
	{
		m_mcuif_trace_write[offset] = true;
			LOGMASKED(LOG_MAD2_LEDGER, "mad2_ledger: W bus=MCUIF off=%02x data=%02x old=%02x pc=%08x t=%.6f memory-window control\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	}
}

void nokia_dct3_state::dct3_map(address_map &map)
{
	map.global_mask(0x00ffffff);
	map(0x00000000, 0x0000ffff).mirror(0x80000).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));                // boot ROM / RAM
	map(0x00010000, 0x00010fff).mirror(0x8f000).rw(FUNC(nokia_dct3_state::dsp_ram_r), FUNC(nokia_dct3_state::dsp_ram_w));        // DSP shared memory
	map(0x00020000, 0x000200ff).mirror(0x8ff00).rw(FUNC(nokia_dct3_state::mad2_io_r), FUNC(nokia_dct3_state::mad2_io_w));         // IO (Primary I/O range, configures peripherals)
	map(0x00030000, 0x00030003).mirror(0x8fffc).rw(FUNC(nokia_dct3_state::mad2_dspif_r), FUNC(nokia_dct3_state::mad2_dspif_w));   // DSPIF (API control register)
	map(0x00040000, 0x00040003).mirror(0x8fffc).rw(FUNC(nokia_dct3_state::mad2_mcuif_r), FUNC(nokia_dct3_state::mad2_mcuif_w));   // MCUIF (Secondary I/O range, configures memory ranges)
	map(0x00100000, 0x0017ffff).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));                                   // RAMSelX
	map(0x00200000, 0x005fffff).rw(FUNC(nokia_dct3_state::flash_r), FUNC(nokia_dct3_state::flash_w));     // ROM1SelX
	map(0x00600000, 0x009fffff).rw(FUNC(nokia_dct3_state::rom2_mirror_r), FUNC(nokia_dct3_state::rom2_mirror_w));   // ROM2SelX mirror/window
	map(0x00a00000, 0x00a03fff).rw(FUNC(nokia_dct3_state::eeprom_r), FUNC(nokia_dct3_state::eeprom_w));           // EEPROMSelX
	map(0x00a04000, 0x00dfffff).unmaprw();                                                                   // EEPROMSelX
	map(0x00e00000, 0x00ffffff).unmaprw();                                                                   // Reserved
}

void nokia_dct3_state::dct3_nse3_map(address_map &map)
{
	map.global_mask(0x00ffffff);
	map(0x00000000, 0x0000ffff).mirror(0x80000).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));                // boot ROM / RAM
	map(0x00010000, 0x00010fff).mirror(0x8f000).rw(FUNC(nokia_dct3_state::dsp_ram_r), FUNC(nokia_dct3_state::dsp_ram_w));        // DSP shared memory
	map(0x00020000, 0x000200ff).mirror(0x8ff00).rw(FUNC(nokia_dct3_state::mad2_io_r), FUNC(nokia_dct3_state::mad2_io_w));         // IO
	map(0x00030000, 0x00030003).mirror(0x8fffc).rw(FUNC(nokia_dct3_state::mad2_dspif_r), FUNC(nokia_dct3_state::mad2_dspif_w));   // DSPIF
	map(0x00040000, 0x00040003).mirror(0x8fffc).rw(FUNC(nokia_dct3_state::mad2_mcuif_r), FUNC(nokia_dct3_state::mad2_mcuif_w));   // MCUIF
	// NSE-3's parts list establishes the physical extents. Undocumented
	// ROM2/alias decode remains unmapped rather than borrowing a later board.
	map(0x00100000, 0x0010ffff).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));       // 64 KiB SRAM
	map(0x00110000, 0x001fffff).unmaprw();
	map(0x00200000, 0x002fffff).rw(FUNC(nokia_dct3_state::flash_r), FUNC(nokia_dct3_state::flash_w));   // 1 MiB TE28F800
	map(0x00300000, 0x009fffff).unmaprw();
	// The 24C64 is reached through PUP's documented serial signals. No NSE-3
	// evidence establishes a parallel EEPROMSelX alias.
	map(0x00a00000, 0x00ffffff).unmaprw();
}

INPUT_CHANGED_MEMBER( nokia_dct3_state::key_irq )
{
	m_kbgpio->input_changed();
}

INPUT_CHANGED_MEMBER( nokia_dct3_state::charger_irq )
{
	m_ccont->set_charger_input(m_product.ccont_board.vchar_channel, newval != 0,
		m_product.ccont_board.vchar_connected_raw);
}

INPUT_CHANGED_MEMBER( nokia_dct3_state::mbus_rx_byte )
{
	// Configuration loading initializes adjusters before the machine runs. Only
	// a live field change represents a byte arriving at the external MBUS pin.
	if (newval != oldval && machine().phase() == machine_phase::RUNNING)
	{
		const bool accepted = m_mbus->receive_byte(u8(newval));
		LOGMASKED(LOG_MBUS, "mbus_fixture: data=%02x accepted=%u t=%.9f\n",
				u8(newval), accepted, machine().time().as_double());
	}
}

static INPUT_PORTS_START( dct3_network_config )
	// External network-event fixtures may queue a bounded incoming service.
	// The default cell remains passive after registration.
	PORT_START("NETCFG")
	PORT_CONFNAME(0x01, 0x00, "Queue one incoming page after registration")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x01, DEF_STR(On))
	PORT_CONFNAME(0x02, 0x00, "Queue one incoming call after registration")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x02, DEF_STR(On))
	PORT_CONFNAME(0x04, 0x00, "Queue one incoming SMS after registration")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x04, DEF_STR(On))
	PORT_CONFNAME(0x08, 0x00, "Queue one incoming Smart Messaging ringtone")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x08, DEF_STR(On))
	PORT_CONFNAME(0x10, 0x00, "Laboratory network speech loopback")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x10, DEF_STR(On))
	PORT_CONFNAME(0x20, 0x00, "Laboratory remote 1 kHz voice source")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x20, DEF_STR(On))
	PORT_CONFNAME(0x40, 0x00, "Four-burst downlink TCH fade per six multiframes")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x40, DEF_STR(On))
	PORT_CONFNAME(0x80, 0x00, "Four-burst uplink TCH fade per six multiframes")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x80, DEF_STR(On))
INPUT_PORTS_END

static INPUT_PORTS_START( noki3210 )
	PORT_INCLUDE(dct3_network_config)

	// Nokia 3210 v5.01/v6.00 share this ROM-derived matrix. COL.n is the
	// firmware read bit and bits 1..4 are driven rows; power uses the special
	// all-rows scan. Names describe the handset controls, while PORT_CODE gives
	// practical default host bindings which remain user-remappable in MAME.
	PORT_START("COL.0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Navi / Left Softkey") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 1") PORT_CODE(KEYCODE_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 2") PORT_CODE(KEYCODE_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("C / Right Softkey") PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 0") PORT_CODE(KEYCODE_0) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 3") PORT_CODE(KEYCODE_3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 5") PORT_CODE(KEYCODE_5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad #") PORT_CODE(KEYCODE_MINUS) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 4") PORT_CODE(KEYCODE_4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 8") PORT_CODE(KEYCODE_8) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad *") PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 6") PORT_CODE(KEYCODE_6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 7") PORT_CODE(KEYCODE_7) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 9") PORT_CODE(KEYCODE_9) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Power") PORT_CODE(KEYCODE_SPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Charger connected") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::charger_irq), 0)

	PORT_START("MBUS_RX")
	PORT_BIT(0xff, 0xff, IPT_POSITIONAL) PORT_NAME("MBUS receive byte")
		PORT_POSITIONS(0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(1)
		PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::mbus_rx_byte), 0)

	// Standard MAME configuration inputs keep negative-composition tests out of
	// process-global environment state. Normal machine profiles enable every
	// component they own; external test cfg files may remove one explicitly.
	PORT_START("HWCFG")
	PORT_CONFNAME(HWCFG_CCONT_READY, HWCFG_CCONT_READY, "CCONT readiness")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(HWCFG_CCONT_READY, DEF_STR(On))
	PORT_CONFNAME(HWCFG_SIM_DEVICE, HWCFG_SIM_DEVICE, "SIM interface")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(HWCFG_SIM_DEVICE, DEF_STR(On))
	PORT_CONFNAME(HWCFG_DSP_SERVICE, HWCFG_DSP_SERVICE, "DSP service HLE")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(HWCFG_DSP_SERVICE, DEF_STR(On))
	PORT_CONFNAME(HWCFG_EXTERNAL_SERVICE, HWCFG_EXTERNAL_SERVICE, "External service HLE")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(HWCFG_EXTERNAL_SERVICE, DEF_STR(On))
	PORT_CONFNAME(HWCFG_RADIO_PEER, HWCFG_RADIO_PEER, "Radio peer HLE")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(HWCFG_RADIO_PEER, DEF_STR(On))
	PORT_CONFNAME(HWCFG_PCM_LINK, HWCFG_PCM_LINK, "MAD2/COBBA PCM link")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(HWCFG_PCM_LINK, DEF_STR(On))

	PORT_START("DIAGCFG")
	PORT_CONFNAME(0x01, 0x00, "Run DSPIF conformance check")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x01, DEF_STR(On))
	PORT_CONFNAME(0x02, 0x00, "Run COBBA control conformance check")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x02, DEF_STR(On))

INPUT_PORTS_END

static INPUT_PORTS_START( noki3310 )
	PORT_INCLUDE(dct3_network_config)

	// NHM-5 v6.39 keymap: raw key = row * 5 + column. Unlike the
	// four-active-row 3210 layout, the 3310 uses every row of the MAD2 5x5 scan.
	PORT_START("COL.0")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 3") PORT_CODE(KEYCODE_3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 0") PORT_CODE(KEYCODE_0) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 1") PORT_CODE(KEYCODE_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 6") PORT_CODE(KEYCODE_6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 9") PORT_CODE(KEYCODE_9) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad #") PORT_CODE(KEYCODE_MINUS) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 2") PORT_CODE(KEYCODE_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 5") PORT_CODE(KEYCODE_5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 8") PORT_CODE(KEYCODE_8) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Menu") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Names / C") PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 4") PORT_CODE(KEYCODE_4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 7") PORT_CODE(KEYCODE_7) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad *") PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Power") PORT_CODE(KEYCODE_SPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Charger connected") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::charger_irq), 0)
INPUT_PORTS_END

static INPUT_PORTS_START( noki6110 )
	// UE4 service-manual matrix. COL.n selects a documented column and each
	// bit is its row; the power key is exposed separately for MAD2's all-row
	// cold-start scan.
	PORT_START("COL.0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Flip") PORT_TOGGLE PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Volume Down") PORT_CODE(KEYCODE_PGDN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Left Softkey") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 1") PORT_CODE(KEYCODE_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 2") PORT_CODE(KEYCODE_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 3") PORT_CODE(KEYCODE_3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Send") PORT_CODE(KEYCODE_S) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 4") PORT_CODE(KEYCODE_4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 5") PORT_CODE(KEYCODE_5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 6") PORT_CODE(KEYCODE_6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("End / Mode") PORT_CODE(KEYCODE_E) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 7") PORT_CODE(KEYCODE_7) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 8") PORT_CODE(KEYCODE_8) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 9") PORT_CODE(KEYCODE_9) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Volume Up") PORT_CODE(KEYCODE_PGUP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Right Softkey") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad *") PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 0") PORT_CODE(KEYCODE_0) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad #") PORT_CODE(KEYCODE_MINUS) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Power") PORT_CODE(KEYCODE_SPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Charger connected") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::charger_irq), 0)
INPUT_PORTS_END

static INPUT_PORTS_START( noki3410 )
	// NHM-2 v5.46 keymap table at 0x4c5130, indexed as row * 5 + column
	// by the scanner at 0x3e496e.  The numeric block matches the 3310,
	// while the two softkeys, scroll keys and send/end keys occupy the
	// previously unused cells around it.
	PORT_START("COL.0")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Right Softkey") PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("End") PORT_CODE(KEYCODE_E) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 3") PORT_CODE(KEYCODE_3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 0") PORT_CODE(KEYCODE_0) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 1") PORT_CODE(KEYCODE_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 6") PORT_CODE(KEYCODE_6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 9") PORT_CODE(KEYCODE_9) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad #") PORT_CODE(KEYCODE_MINUS) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 2") PORT_CODE(KEYCODE_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 5") PORT_CODE(KEYCODE_5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 8") PORT_CODE(KEYCODE_8) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Left Softkey / Menu") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Send") PORT_CODE(KEYCODE_S) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 4") PORT_CODE(KEYCODE_4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 7") PORT_CODE(KEYCODE_7) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad *") PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Power") PORT_CODE(KEYCODE_SPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Charger connected") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::charger_irq), 0)
INPUT_PORTS_END

void nokia_dct3_state::dct3_base(machine_config &config)
{
	/* basic machine hardware */
	ARM7_BE(config, m_maincpu, 26000000 / 2);  // MAD2-family 13 MHz ARM clock; sleep uses the 32.768 kHz domain
	m_maincpu->set_addrmap(AS_PROGRAM, &nokia_dct3_state::dct3_map);

	/* video hardware */
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_LCD, rgb_t::white()));
	screen.set_refresh_hz(60);
	screen.set_vblank_time(ATTOSECONDS_IN_USEC(2500) /* not accurate */);
	screen.set_size(84, 48);
	screen.set_visarea(0, 84-1, 0, 48-1);
	screen.set_screen_update("lcd", FUNC(pcd8544_device::screen_update));
	screen.set_palette("palette");

	PALETTE(config, "palette", palette_device::MONOCHROME_INVERTED);

	PCD8544(config, m_lcd);

	SPEAKER(config, "mono").front_center();
	BEEP(config, m_buzzer).add_route(ALL_OUTPUTS, "mono", 0.25);
	BEEP(config, m_dsp_tone1).add_route(ALL_OUTPUTS, "mono", 0.12);
	BEEP(config, m_dsp_tone2).add_route(ALL_OUTPUTS, "mono", 0.12);
	NOKIA_COBBA(config, m_cobba);

	INTEL_TE28F160(config, "flash");
	NOKIA_B3_FLASH(config, m_b3_flash, 0);
	m_b3_flash->set_status_csr((NOKIA_3410_FLASH_STATUS_CSR - NOKIA_FLASH1_BASE) >> 1);
	I2C_24C128(config, m_eeprom);
	NOKIA_MAD2(config, m_mad2);
	NOKIA_MAD2_PCM(config, m_mad2_pcm);
	NOKIA_GSM_VOICE_PEER(config, "gsm_voice_peer");
	m_mad2->set_timer0_hz(33'055);
	m_mad2->set_timer1_hz(1'057);
	m_mad2->set_fiq8_hz(1'000);
	m_mad2->set_timer0_catchup(false);
	m_mad2->fiq_cb().set(FUNC(nokia_dct3_state::mad2_fiq_w));
	m_mad2->irq_cb().set(FUNC(nokia_dct3_state::mad2_irq_w));
	m_mad2->irq_ack_cb().set(FUNC(nokia_dct3_state::mad2_irq_ack_w));
	m_mad2->reset_cb().set(FUNC(nokia_dct3_state::mad2_reset_w));
	m_mad2->sleep_cb().set(FUNC(nokia_dct3_state::mad2_sleep_w));
	m_mad2->simi_clock_cb().set(m_simi, FUNC(nokia_simi_device::set_clock_enabled));
	NOKIA_KBGPIO(config, m_kbgpio);
	m_kbgpio->matrix_cb(0).set_ioport("COL.0");
	m_kbgpio->matrix_cb(1).set_ioport("COL.1");
	m_kbgpio->matrix_cb(2).set_ioport("COL.2");
	m_kbgpio->matrix_cb(3).set_ioport("COL.3");
	m_kbgpio->matrix_cb(4).set_ioport("COL.4");
	m_kbgpio->power_cb().set_ioport("PWR");
	m_kbgpio->irq_cb().set(FUNC(nokia_dct3_state::kbgpio_irq_w));
	NOKIA_MBUS(config, m_mbus);
	m_mbus->tx_cb().set(FUNC(nokia_dct3_state::mbus_tx_w));
	m_mbus->fiq2_cb().set(FUNC(nokia_dct3_state::mbus_fiq2_w));
	m_mbus->fiq3_cb().set(FUNC(nokia_dct3_state::mbus_fiq3_w));
	NOKIA_PUP(config, m_pup);
	m_pup->eeprom_sda_read_cb().set(m_eeprom, FUNC(i2cmem_device::read_sda));
	m_pup->eeprom_sda_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_sda));
	m_pup->eeprom_scl_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_scl));
	m_pup->buzzer_clock_cb().set(FUNC(nokia_dct3_state::pup_buzzer_clock_w));
	m_pup->buzzer_enable_cb().set(FUNC(nokia_dct3_state::pup_buzzer_enable_w));
	m_pup->vibrator_enable_cb().set(FUNC(nokia_dct3_state::pup_vibrator_w));
	NOKIA_CCONT(config, m_ccont);
	// The low status bit is persistent CCONT reset state, not an IRQ source.
	// Clearing it provides the explicit missing/unready-CCONT fault fixture.
	m_ccont->set_ready(true);
	m_ccont->irq_cb().set(FUNC(nokia_dct3_state::ccont_irq_w));
	m_ccont->power_cb().set(FUNC(nokia_dct3_state::ccont_power_w));
	NOKIA_GENSIO(config, m_gensio);
	m_gensio->ccont_read_cb().set(m_ccont, FUNC(nokia_ccont_device::serial_r));
	m_gensio->ccont_write_cb().set(m_ccont, FUNC(nokia_ccont_device::serial_w));
	m_gensio->ccont_select_cb().set(m_ccont, FUNC(nokia_ccont_device::select_w));
	m_gensio->lcd_dc_cb().set(m_lcd, FUNC(pcd8544_device::dc_w));
	m_gensio->lcd_sdin_cb().set(m_lcd, FUNC(pcd8544_device::sdin_w));
	m_gensio->lcd_sclk_cb().set(m_lcd, FUNC(pcd8544_device::sclk_w));
	NOKIA_DSPIF(config, m_dspif);
	NOKIA_DSP_HLE(config, m_dsp_hle);
	NOKIA_EXTERNAL_SERVICE_PEER(config, m_external_service_peer);
	NOKIA_GSM_NETWORK(config, m_gsm_network);
	NOKIA_GSM_SESSION(config, m_gsm_session);
	NOKIA_LAPDM_LINK(config, m_lapdm_link);
	NOKIA_RADIO_PEER(config, m_radio_peer);
	m_dspif->tx_commit_cb().set(FUNC(nokia_dct3_state::dsp_tx_commit_w));
	m_dspif->service_pending_cb().set(FUNC(nokia_dct3_state::dsp_service_pending_w));
	m_dspif->doorbell_cb().set(FUNC(nokia_dct3_state::dsp_doorbell_w));
	m_dspif->shared_002_write_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_002_write_w));
	m_dspif->shared_0fe_read_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_0fe_read_w));
	m_dspif->shared_0fe_write_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_0fe_write_w));
	m_dspif->shared_100_read_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_100_read_w));
	m_dspif->shared_100_write_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_100_write_w));
	m_dspif->fiq0_cb().set(FUNC(nokia_dct3_state::dsp_fiq0_w));
	m_dspif->service_irq_cb().set(FUNC(nokia_dct3_state::dsp_service_irq_w));
	NOKIA_SIMI(config, m_simi);
	NOKIA_SIM_CARD(config, m_sim_card);
	m_simi->irq_cb().set(FUNC(nokia_dct3_state::sim_irq_w));
	m_sim_card->response_cb().set(m_simi, FUNC(nokia_simi_device::card_rx_w));
}

void nokia_dct3_state::noki3310(machine_config &config)
{
	dct3_base(config);
	// NHM-5/UB 4 V09 board topology terminates the internal receiver on
	// COBBA EARP/EARN and the built-in microphone on MIC2P/MIC2N. These
	// neutral host routes express connectivity only; product analogue gains
	// remain unset until independently recovered.
	m_cobba->add_route(nokia_cobba_device::ear, "mono", 1.0);
	MICROPHONE(config, "microphone", 1).front_center()
			.add_route(0, m_cobba, 1.0, nokia_cobba_device::mic2);
	apply_product_config(PRODUCT_3310);
}

void nokia_dct3_state::dct3_32mbit_flash_base(machine_config &config)
{
	dct3_base(config);
	INTEL_TE28F320(config.replace(), "flash");
}

void nokia_dct3_state::noki3330(machine_config &config)
{
	dct3_32mbit_flash_base(config);
	apply_product_config(PRODUCT_3330);
}

void nokia_dct3_state::noki3210(machine_config &config)
{
	dct3_base(config);
	// NSE-8 board topology: the internal microphone is physically wired to
	// COBBA MIC2 and the receiver to its differential EAR output. Keep these
	// MAME sound routes out of the generic DCT3 base configuration.
	m_cobba->add_route(nokia_cobba_device::ear, "mono", 0.50);
	MICROPHONE(config, "microphone", 1).front_center()
			.add_route(0, m_cobba, 1.0, nokia_cobba_device::mic2);
	apply_product_config(PRODUCT_3210);

	// Both supported 3210 firmware revisions use this validated composition.
	// The 3310 has its own validated profile; other DCT3 products retain the
	// conservative base-device defaults until their contracts are exercised.
	// The paired ROMs prove that Timer 1 ticks eight times faster than Timer 0's
	// divided counter. Keep the measured inputs distinct until the exact CTSI
	// oscillator/divider tree is recovered; do not conflate both timer inputs.
	m_mad2->set_timer0_hz(33'055);
	m_mad2->set_timer1_hz(1'057);
	m_mad2->set_timer0_catchup(false);
}

void nokia_dct3_state::noki5210(machine_config &config)
{
	dct3_32mbit_flash_base(config);
	apply_product_config(PRODUCT_5X10);
}

void nokia_dct3_state::noki8xxx(machine_config &config)
{
	dct3_base(config);
	apply_product_config(PRODUCT_8XXX);
}

void nokia_dct3_state::noki3410(machine_config &config)
{
	dct3_32mbit_flash_base(config);
	ST_M28W320ECT(config.replace(), "flash");
	apply_product_config(PRODUCT_3410);
}

void nokia_dct3_state::noki6110(machine_config &config)
{
	dct3_base(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &nokia_dct3_state::dct3_nse3_map);
	INTEL_28F800B3T(config.replace(), "flash");
	I2C_24C64(config.replace(), m_eeprom);
	// NSE-3's internal differential receiver and microphone terminate at
	// COBBA EAR and MIC2. Neutral routes declare wiring, not unproved gain.
	m_cobba->add_route(nokia_cobba_device::ear, "mono", 1.0);
	MICROPHONE(config, "microphone", 1).front_center()
			.add_route(0, m_cobba, 1.0, nokia_cobba_device::mic2);
	apply_product_config(PRODUCT_6110);
}

void nokia_dct3_state::noki7110(machine_config &config)
{
	dct3_32mbit_flash_base(config);
	apply_product_config(PRODUCT_DEFAULT);

	subdevice<screen_device>("screen")->set_size(96, 65);    // Epson SED1565
}

void nokia_dct3_state::noki6210(machine_config &config)
{
	dct3_32mbit_flash_base(config);
	apply_product_config(PRODUCT_DEFAULT);

	subdevice<screen_device>("screen")->set_size(96, 60);
}

// MAD2 internal ROMS
#define MAD2_INTERNAL_ROMS \
	ROM_REGION16_BE(0x10000, "boot_rom", ROMREGION_ERASE00 )    \
	ROM_LOAD("boot_rom", 0x00000, 0x10000, CRC(deab7e4e) SHA1(472a55b0ba289b0f4e538bb4c8b826dede3a40bb)) \
																\
	ROM_REGION16_BE(0x20000, "dsp", ROMREGION_ERASE00 )         \
	ROM_LOAD("dsp_prom" , 0x00000, 0xc000, CRC(485d974c) SHA1(eac8c1e0dbb6e17b60b2e7ef6685880d3fd85521)) \
	ROM_LOAD("dsp_drom" , 0x0c000, 0x4000, CRC(690b37d3) SHA1(547372f1044a3442aa52fcd2b3546540aba59344)) \
	ROM_LOAD("dsp_pdrom", 0x10000, 0x1000, CRC(f154670a) SHA1(e0c66649d1434eca3435033a32634cb90cef0f31))

ROM_START( noki3210 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x200000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "600", "v6.00")  // A 03-10-2000
	ROM_SYSTEM_BIOS(1, "501", "v5.01")  // NSE-8 08-12-2000
	ROMX_LOAD("3210f600a.fls", 0x000000, 0x200000, CRC(6a978478) SHA1(6bdec2ec76aca15bc12b621be4402e455562454b), ROM_BIOS(0))
	ROMX_LOAD("3210f501.fls", 0x000000, 0x200000, CRC(e8d904a6) SHA1(8ad137c6aba9eb27bef067458d23016843f7fad5), ROM_BIOS(1))

	ROM_REGION(0x04000, "eeprom", ROMREGION_ERASEFF)
	ROMX_LOAD("3210 v600 eeprom.bin", 0x00000, 0x04000, CRC(e236395f) SHA1(14f207b6b6e04945d26049df404723830bc765e7), ROM_BIOS(0))
	ROMX_LOAD("3210 v501 eeprom.bin", 0x00000, 0x04000, CRC(82dc441c) SHA1(4cbc156da79d49610dd0018d3eaf8f8cbcbc05bf), ROM_BIOS(1))
ROM_END

ROM_START( noki6110 )
	// NSE-3 uses MAD2 ROM3 F711604. The later shared MAD2 dumps are not a
	// substitute, so keep every proprietary execution input explicitly absent.
	ROM_REGION16_BE(0x10000, "boot_rom", ROMREGION_ERASE00)
	ROM_LOAD("nse3_rom3_f711604_boot.bin", 0x00000, 0x10000, NO_DUMP)

	ROM_REGION16_BE(0x20000, "dsp", ROMREGION_ERASE00)
	ROM_LOAD("nse3_rom3_dsp_prom.bin", 0x00000, 0x0c000, NO_DUMP)
	ROM_LOAD("nse3_rom3_dsp_drom.bin", 0x0c000, 0x04000, NO_DUMP)
	ROM_LOAD("nse3_rom3_dsp_pdrom.bin", 0x10000, 0x01000, NO_DUMP)

	ROM_REGION16_BE(0x100000, "flash", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS(0, "406", "v4.06 PPM B (ROM3 candidate)")
	ROM_SYSTEM_BIOS(1, "548", "v5.48 PPM B")
	ROMX_LOAD("6110_nse3_v406_rom3_candidate.fls", 0x000000, 0x100000, CRC(78f6dce9) SHA1(5025a6ac3b4a13714211fde903f27f92cbb7c9b6), ROM_BIOS(0))
	ROMX_LOAD("6110_nse3_v548_rom3.fls", 0x000000, 0x100000, NO_DUMP, ROM_BIOS(1))

	ROM_REGION(0x02000, "eeprom", ROMREGION_ERASEFF)
	ROM_LOAD("6110_nse3_eeprom.bin", 0x00000, 0x02000, NO_DUMP)
ROM_END

ROM_START( noki3310 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x200000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "607", "v6.07")  // C 17-06-2003
	ROM_SYSTEM_BIOS(1, "579", "v5.79")  // N 11-11-2002
	ROM_SYSTEM_BIOS(2, "513", "v5.13")  // C 11-01-2002
	ROM_SYSTEM_BIOS(3, "639", "v6.39 local spike")
	ROMX_LOAD("3310_607_ppm_c.fls", 0x000000, 0x200000, CRC(5743f6ba) SHA1(0e80b5f1698909c9850be770c1289566582aa77a), ROM_BIOS(0))
	ROMX_LOAD("3310 nr1 v5.79.fls", 0x000000, 0x200000, CRC(26b4f0df) SHA1(649de05ed88205a080693b918cd1295ac691dff1), ROM_BIOS(1))
	ROMX_LOAD("3310 v. 5.13 c.fls", 0x000000, 0x1d0000, CRC(0f66d256) SHA1(04d8dabe2c454d6a1161f352d85c69c409895000), ROM_BIOS(2))
	ROMX_LOAD("3310f639e.fls", 0x000000, 0x200000, CRC(13430c77) SHA1(d5da65f417595200314eb0115bf46ca1fbf53128), ROM_BIOS(3))
	ROMX_LOAD("3310 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(8393b1f7) SHA1(ab6c05bfa54ecd7c2acbd172009ffe6c7f130cb8), ROM_BIOS(0))
	ROMX_LOAD("3310 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(8393b1f7) SHA1(ab6c05bfa54ecd7c2acbd172009ffe6c7f130cb8), ROM_BIOS(1))
	ROMX_LOAD("3310 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(8393b1f7) SHA1(ab6c05bfa54ecd7c2acbd172009ffe6c7f130cb8), ROM_BIOS(2))
	ROMX_LOAD("3310 v2 pmm.bin", 0x1d0000, 0x030000, CRC(1027bcbf) SHA1(6bfb76a2055617e16016bf1b86efa621859efef6), ROM_BIOS(3))

ROM_END

ROM_START( noki3330 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "450", "v4.50")  // C 12-10-2001
	ROM_SYSTEM_BIOS(1, "450e", "v4.50 PPM E")
	ROMX_LOAD("3330f450c.fls", 0x000000, 0x350000, CRC(259313e7) SHA1(88bcc39d9358fd8a8562fe3a0280f0ce82f5897f), ROM_BIOS(0))
	ROMX_LOAD("3330f450e.fls", 0x000000, 0x350000, CRC(9710f695) SHA1(7e88caa4963c57ebbd4d919023e38103ff8b528a), ROM_BIOS(1))
	ROM_LOAD("3330 virgin eeprom 005f0000.fls", 0x3f0000, 0x010000, CRC(23459c10) SHA1(68481effb39d90a1639e8f261009c66e97d3e668))
ROM_END

ROM_START( noki3410 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "546e", "v5.46 PPM E")
	ROM_SYSTEM_BIOS(1, "506", "v5.06")  // C 29-11-2002
	ROMX_LOAD("3410f546e.fls", 0x000000, 0x370000, CRC(f9f669cc) SHA1(e650b8a289b434f2c8260c68e44e70e84e41b4cc), ROM_BIOS(0))
	ROMX_LOAD("3410 virgin eeprom 005f0000.fls", 0x370000, 0x090000, CRC(c03a3b8b) SHA1(c1cb3a37efc11ea57b96969d2b01ca0f3b0f6bbe), ROM_BIOS(0))
	ROMX_LOAD("3410_5-06c.fls", 0x000000, 0x370000, CRC(1483e094) SHA1(ef26026297c779de7b01923a364ded822e720c38), ROM_BIOS(1))
ROM_END

ROM_START( noki5210 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "540", "v5.40")  // C 11-10-2003
	ROM_SYSTEM_BIOS(1, "525", "v5.25")  // C 26-02-2003
	ROM_SYSTEM_BIOS(2, "520", "v5.20")  // C 12-08-2002
	ROMX_LOAD("5210_5.40_ppm_c.fls", 0x000000, 0x380000, CRC(e37d5beb) SHA1(726f000780dd67750b7d2859687f846ce17a1bf7), ROM_BIOS(0))
	ROMX_LOAD("5210_5.25_ppm_c.fls", 0x000000, 0x380000, CRC(13bba458) SHA1(3b5244244743fba48f9061e158f95fc46b86446e), ROM_BIOS(1))
	ROMX_LOAD("5210_520_c.fls", 0x000000, 0x380000, CRC(38648cd3) SHA1(9210e15e6bd780f86c467bec33ef54d6393abe5a), ROM_BIOS(2))
ROM_END

ROM_START( noki6210 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "556", "v5.56")  // C 25-01-2002
	ROMX_LOAD("6210_556c.fls", 0x000000, 0x3a0000, CRC(203fb962) SHA1(3d9ea319503e78ec69b60d72cda23e461e118ea9), ROM_BIOS(0))
	ROM_LOAD("6210 virgin eeprom 005fa000.fls", 0x3fa000, 0x006000, CRC(3c6d3437) SHA1(b3a527ede1be87bd715fb3741a81eef5bd422efa))
ROM_END

ROM_START( noki6250 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "503", "v5.03")  // C 06-12-2001
	ROMX_LOAD("6250-503mcuppmc.fls", 0x000000, 0x3a0000, CRC(8dffb91b) SHA1(95607ce39c383bda75f1e6aeae67a214b787b0a1), ROM_BIOS(0))
	ROM_LOAD("6250 virgin eeprom 005fa000.fls", 0x3fa000, 0x006000, CRC(6087ce70) SHA1(57c29c8387caf864603d94a22bfb63ace427b7f9))
ROM_END

ROM_START( noki7110 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "501", "v5.01")  // C 08-12-2000
	ROMX_LOAD("7110f501_ppmc.fls", 0x000000, 0x390000, CRC(919ac753) SHA1(53af8324919f455ba8199d2c05f7a921cfb811d5), ROM_BIOS(0))
	ROM_LOAD("7110 virgin eeprom 005fa000.fls", 0x3fa000, 0x006000, CRC(78e7d8c1) SHA1(8b4dd782fc9d1306268ba63124ee463ac646912b))
ROM_END

ROM_START( noki8210 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x200000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "531", "v5.31")  // C 08-03-2002
	ROMX_LOAD("8210_5.31ppm_c.fls", 0x000000, 0x1d0000, CRC(927022b1) SHA1(c1a0fe95cedb89a92b19654208cc4855e1a4988e), ROM_BIOS(0))
	ROM_LOAD("8210 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(37fddeea) SHA1(1c01ad3948ff9919890498a84f31052369d93e1d))
ROM_END

ROM_START( noki8250 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x200000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "502", "v5.02")  // K 28-01-2002
	ROMX_LOAD("8250-502mcuppmk.fls", 0x000000, 0x1d0000, CRC(2c58e48b) SHA1(f26c98ffcfffbbd5714889e10cfa41c5f6dd2529), ROM_BIOS(0))
	ROM_LOAD("8250 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(7ca585e0) SHA1(a974fb5fddcd0438ac4aaf32b431f1453e8d923c))
ROM_END

ROM_START( noki8850 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x200000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "531", "v5.31")  // C 08-03-2002
	ROMX_LOAD("8850v531.fls", 0x000000, 0x1d0000, CRC(8864fcb3) SHA1(9f966787403b68a09530680ad911302403eb1521), ROM_BIOS(0))
	ROM_LOAD("8850 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(4823f27e) SHA1(b09455302d98fbedf35072c9ecfd7721a04924b0))
ROM_END

ROM_START( noki8890 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x200000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "1220", "v12.20")    // C 19-03-2001
	ROMX_LOAD("8890_12.20_ppmc.fls", 0x000000, 0x1d0000, CRC(77206f78) SHA1(a214a0d69760ecd8eeca0b9d82f95c94bdfe70ed), ROM_BIOS(0))
	ROM_LOAD("8890 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(1d8ef3b5) SHA1(cc0924cfd4c0ce796fca157c640fc3183c2b5f2c))
ROM_END

} // anonymous namespace

//    YEAR  NAME      PARENT  COMPAT  MACHINE   INPUT     CLASS           INIT        COMPANY  FULLNAME      FLAGS
SYST( 1999, noki3210, 0,      0,      noki3210, noki3210, nokia_dct3_state, empty_init, "Nokia", "Nokia 3210", 0 )
SYST( 1997, noki6110, 0,      0,      noki6110, noki6110, nokia_dct3_state, empty_init, "Nokia", "Nokia 6110 (NSE-3)", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki7110, 0,      0,      noki7110, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 7110", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8210, 0,      0,      noki8xxx, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 8210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8850, 0,      0,      noki8xxx, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 8850", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki3310, 0,      0,      noki3310, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 3310", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6210, 0,      0,      noki6210, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 6210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6250, 0,      0,      noki6210, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 6250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8250, 0,      0,      noki8xxx, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 8250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8890, 0,      0,      noki8xxx, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 8890", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2001, noki3330, 0,      0,      noki3330, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 3330", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki3410, 0,      0,      noki3410, noki3410, nokia_dct3_state, empty_init, "Nokia", "Nokia 3410", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki5210, 0,      0,      noki5210, noki3310, nokia_dct3_state, empty_init, "Nokia", "Nokia 5210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
