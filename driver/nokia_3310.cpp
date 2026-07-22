// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
/*
    Driver for Nokia phones based on the Texas Instruments MAD2 family
    (ARM7TDMI + DSP). The Nokia 3210 uses MAD2PR1; later products use other
    revisions, including MAD2WD1.

    Driver based on documentations found here:
        http://nokix.sourceforge.net/help/blacksphere/sub_050main.htm
        http://tudor.rdslink.ro/MADos/

*/

// if anybody has solid information to aid in the emulation of this (or other phones) please contribute.

#include "emu.h"

#include <array>
#include <optional>
#include <unordered_map>

#include "cpu/arm7/arm7.h"
#include "machine/i2cmem.h"
#include "machine/intelfsh.h"
#include "sound/beep.h"
#include "speaker.h"
#include "video/pcd8544.h"

#include "nokia_ccont.h"
#include "nokia_dsp_hle.h"
#include "nokia_dspif.h"
#include "nokia_external_service.h"
#include "nokia_gensio.h"
#include "nokia_gsm_network.h"
#include "nokia_mad2.h"
#include "nokia_mbus.h"
#include "nokia_radio_peer.h"
#include "nokia_sim_card.h"
#include "nokia_simi.h"

#include "emupal.h"
#include "screen.h"

#include <cstdlib>
#include <cstring>
#include <string>

#define LOG_MAD2_REGISTER_ACCESS    (1U << 1)

#define VERBOSE (0)
#include "logmacro.h"

namespace {

static unsigned nokia_env_u32(const char *name, unsigned fallback);

struct nokia_product_config
{
	u8 power_on_column_mask;
	bool sim_device;
	bool dsp_service;
	bool external_service;
	bool radio_peer;
	bool keypad_five_rows;
	bool ccont_wddisx_grounded;
	unsigned dsp_bootstrap_exchanges;
	bool dsp_bootstrap_ping_pong;
	bool dsp_code_block_request;
	bool dsp_parked_boot_status;
	u16 dsp_boot_status_response;
	unsigned dsp_service_delay_us;
	bool flash_b3_block_lock;
	u8 dsp_reset_running_status;
	u8 dsp_release_mask;
	u8 lcd_controller_width;
	u8 lcd_controller_height;
	u8 lcd_visible_width;
	u8 lcd_visible_height;
	std::array<u16, 8> adc_defaults;
};

constexpr std::array<u16, 8> ADC_DEFAULT =
		{ 0x000, 0x3ff, 0x3ff, 0x280, 0x200, 0x000, 0x200, 0x000 };
constexpr std::array<u16, 8> ADC_3210 =
		{ 0x000, 0x200, 0x2d0, 0x280, 0x200, 0x000, 0x200, 0x000 };
// Standard 3310 routing: channel 2 is VBATT, 3 is the BMC-3 pack's BSI
// resistor and 4 is battery temperature. This tuple clears the firmware's
// ordinary pack/self-test path; it is product input, not a state fixture.
constexpr std::array<u16, 8> ADC_3310 =
		{ 0x000, 0x3ff, 0x220, 0x026, 0x200, 0x000, 0x200, 0x000 };

constexpr nokia_product_config PRODUCT_3210 =
		{ 0x01, true, true, true, true, false, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_3210 };
constexpr nokia_product_config PRODUCT_3310 =
		{ 0x04, true, true, true, false, true, false, 58, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_3310 };
// NHM-6 v4.50 completes 64 DSP bootstrap exchanges and shares the five-row
// keypad and standard VBATT/BSI/BTEMP channel routing with the 3310 family.
// Enabling the three request-driven peers plus this ADC tuple advances the
// virgin PMM from CONTACT SERVICE to its organic security-code editor.
constexpr nokia_product_config PRODUCT_3330 =
		{ 0x04, true, true, true, false, true, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_3310 };
// NHM-2 releases the DSP through reset-control bit 2 and then polls MAD2's
// clock/ready status bit. The 0x53 readback is the observed running state; its
// readiness semantics live in MAD2, while the board wiring remains here.
constexpr nokia_product_config PRODUCT_3410 =
		{ 0x02, true, true, true, false, true, false, 64, true, true, true, 0, 50, true, 0x53, 0x04, 102, 72, 96, 65, ADC_3310 };
// Preserve the previous 64-exchange behavior for unvalidated products. This is
// an explicit compatibility calibration, not a recovered cross-DCT3 constant.
constexpr nokia_product_config PRODUCT_DEFAULT =
		{ 0x04, false, false, false, false, false, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_DEFAULT };
constexpr nokia_product_config PRODUCT_5X10 =
		{ 0x10, false, false, false, false, false, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_DEFAULT };
constexpr nokia_product_config PRODUCT_8XXX =
		{ 0x10, false, false, false, false, false, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_DEFAULT };

constexpr offs_t NOKIA_RAM_BASE = 0x100000;
constexpr offs_t NOKIA_RAM_END = 0x180000;
constexpr offs_t NOKIA_FLASH1_BASE = 0x00200000;
constexpr offs_t NOKIA_3410_FLASH_STATUS_CSR = 0x003fff00;
constexpr offs_t NOKIA_FLASH_END = 0x00a00000;
constexpr uint32_t NOKIA_FLASH_ENTRY = 0x200040;

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
	MAD2_TIMER0_DIVIDER = 0x0f,
	MAD2_TIMER0_COUNTER_MSB = 0x10,
	MAD2_TIMER0_COUNTER_LSB = 0x11,
	MAD2_TIMER0_COMPARE_MSB = 0x12,
	MAD2_TIMER0_COMPARE_LSB = 0x13,
	MAD2_FIQ8_CTRL = 0x16,
	MAD2_MBUS_CTRL = 0x18,
	MAD2_MBUS_STATUS = 0x19,
	MAD2_CCONT_WRITE = 0x2c,
	MAD2_LCD_DATA = 0x2e,
	MAD2_KEYBOARD_ROWS = 0x28,
	MAD2_KEYBOARD_COLS = 0x2a,
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

constexpr uint16_t MAD2_FIQ_TIMER0_COMPARE = uint16_t(1) << 4;

// CCONT serial command/status bits + fixed wiring (hardware constants, not configurable).
// PWRONX is latched as CCONT status bit 1 on a cold power-key boot. It is a
// reset cause sampled by firmware, not one of the upper interrupt sources.
constexpr uint8_t KEYPAD_IRQ_LINE_NUM = 0;        // MAD2 keypad/UIF interrupt
constexpr uint8_t CCONT_IRQ_LINE_NUM = 2;         // MAD2 IRQ line the CCONT asserts

// Firmware RAM locations used only by observation-only research diagnostics.
constexpr offs_t FW_SCHED_RUNNING_TASK_ID = 0x100022;
// Service-ready / DSP-handshake chain; see docs/service_bootstrap.md. The
// startup service-ready byte (0x110c2c) is set =1 by
// the setter 0x291068 iff the DSP-shared pending counter (DSP RAM byte 0xe4) == 0; the
// setter only runs when MAD2 IRQ line 4 (the DSP service-completion interrupt) fires.
constexpr offs_t FW_STARTUP_EVENT = 0x1123ee;
constexpr offs_t FW_STARTUP_MODE = 0x1123f0;
constexpr offs_t FW_POST74_EVENT_GATE = 0x112368;
constexpr offs_t FW_POST74_EVENT_GATE_READY = FW_POST74_EVENT_GATE + 4;
constexpr offs_t FW_POST74_EVENT_GATE_FLAGS = FW_POST74_EVENT_GATE + 6;

// Service-session state reached during the startup watchdog path.
constexpr offs_t FW_SERVICE_SESSION_RESULT = 0x11fed4;
constexpr offs_t FW_SERVICE_SESSION_COUNTER = 0x11fed6;
constexpr offs_t FW_SERVICE_SESSION_SUBSTATE = 0x11feda;
constexpr offs_t FW_SERVICE_SESSION_ACK = 0x11fedb;

// Service-channel/lower-service state used by the current startup transport model.
constexpr offs_t FW_SERVICE_CHANNEL_ENABLE_FLAGS = 0x11fee4;

// Service-session remote read (see docs/service_bootstrap.md). The firmware
// reads its command from PM logical address 0x5f00 via an async MBUS/PM
// request message. The request's dest node ([msg+1]) is sourced from the channel-enable flag
// FW_SERVICE_CHANNEL_ENABLE_FLAGS (0x11fee4) — which is 0 on a blank phone, so the read is
// dropped (no request sent). The response, when one arrives, is dispatched by command at
// 0x236dc6; command 0x05 completes the service transaction. Request frame format:
//   00 [node] 00 00 00 0a 00 01 [addr_hi] [addr_lo] [seq][seq] [ctr] [count] [data..]
constexpr uint16_t PM_LOGICAL_SERVICE_COMMAND = 0x5f00;
constexpr uint8_t  SERVICE_RESPONSE_CMD_HEALTHY = 0x05;

class noki3310_state : public driver_device
{
public:
	noki3310_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_flash(*this, "flash"),
		m_eeprom(*this, "eeprom"),
		m_ccont(*this, "ccont"),
		m_gensio(*this, "gensio"),
		m_mad2(*this, "mad2"),
		m_mbus(*this, "mbus"),
		m_dspif(*this, "dspif"),
		m_dsp_hle(*this, "dsp_hle"),
		m_external_service_peer(*this, "external_service_peer"),
		m_gsm_network(*this, "gsm_network"),
		m_radio_peer(*this, "radio_peer"),
		m_simi(*this, "simi"),
		m_sim_card(*this, "sim_card"),
		m_lcd(*this, "lcd"),
		m_buzzer(*this, "buzzer"),
		m_dsp_tone1(*this, "dsp_tone1"),
		m_dsp_tone2(*this, "dsp_tone2"),
		m_vibration(*this, "vibration"),
		m_keypad(*this, "COL.%u", 0),
		m_pwr(*this, "PWR")
	{ }

	void noki3330(machine_config &config);
	void noki3410(machine_config &config);
	void noki7110(machine_config &config);
	void noki6210(machine_config &config);
	void noki3310(machine_config &config);
	void noki3210(machine_config &config);
	void noki5210(machine_config &config);
	void noki8xxx(machine_config &config);

	DECLARE_INPUT_CHANGED_MEMBER(key_irq);
	DECLARE_INPUT_CHANGED_MEMBER(charger_irq);

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
	void mad2_board_outputs_w(offs_t offset);
	void trace_mad2_read(offs_t offset, uint8_t data);
	void trace_mad2_write(offs_t offset, uint8_t data, uint8_t old_data);
	uint8_t mad2_dspif_r(offs_t offset);
	void mad2_dspif_w(offs_t offset, uint8_t data);
	uint8_t mad2_mcuif_r(offs_t offset);
	void mad2_mcuif_w(offs_t offset, uint8_t data);

	TIMER_CALLBACK_MEMBER(timer_watchdog);
	TIMER_CALLBACK_MEMBER(timer_mbus_rx_fixture);
	TIMER_CALLBACK_MEMBER(timer_flash_b3_erase);

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

	void noki3310_map(address_map &map) ATTR_COLD;

	void trace_interrupt_register(char operation, offs_t offset, uint8_t data);
	void mad2_fiq_w(int state);
	void mad2_irq_w(int state);
	void mad2_sleep_w(int state);
	void mad2_irq_ack_w(u16 mask);
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
	uint8_t keypad_columns_r(bool consume_power_on = false);
	void update_keypad_columns();
	void update_keypad_ccont_irqs();
	void update_buzzer();
	void update_vibrator();
	void update_dsp_tones();
	uint16_t fw_word(offs_t address) const;
	uint8_t fw_byte(offs_t address) const;
	uint32_t fw_dword(offs_t address) const;
	uint16_t debug_ram_word(offs_t address) const { return fw_word(address); }
	uint8_t debug_ram_byte(offs_t address) const { return fw_byte(address); }
	required_device<cpu_device> m_maincpu;
	required_device<intelfsh16_device> m_flash;
	required_device<i2c_24c128_device> m_eeprom;
	required_device<nokia_ccont_device> m_ccont;
	required_device<nokia_gensio_device> m_gensio;
	required_device<nokia_mad2_device> m_mad2;
	required_device<nokia_mbus_device> m_mbus;
	required_device<nokia_dspif_device> m_dspif;
	required_device<nokia_dsp_hle_device> m_dsp_hle;
	required_device<nokia_external_service_peer_device> m_external_service_peer;
	required_device<nokia_gsm_network_device> m_gsm_network;
	required_device<nokia_radio_peer_device> m_radio_peer;
	required_device<nokia_simi_device> m_simi;
	required_device<nokia_sim_card_device> m_sim_card;
	required_device<pcd8544_device> m_lcd;
	required_device<beep_device> m_buzzer;
	required_device<beep_device> m_dsp_tone1;
	required_device<beep_device> m_dsp_tone2;
	output_finder<> m_vibration;
	required_ioport_array<5> m_keypad;
	required_ioport m_pwr;

	std::unique_ptr<uint16_t[]>   m_ram;

	nokia_product_config m_product = PRODUCT_DEFAULT;
	uint8_t       m_power_on;
	uint8_t       m_keypad_columns;
	bool          m_keypad_irq_latched;
	bool          m_ccont_irq_state;
	bool          m_baseband_powered = true;
	bool          m_flash_b3_lock_command = false;
	bool          m_flash_b3_program_data = false;
	bool          m_flash_b3_erase_confirm = false;
	bool          m_flash_b3_erase_active = false;
	bool          m_flash_b3_erase_suspended = false;
	bool          m_flash_b3_status_override = false;
	u64           m_flash_b3_erase_remaining_us = 0;

	emu_timer * m_timer_watchdog;
	emu_timer * m_timer_mbus_rx_fixture;
	emu_timer * m_timer_flash_b3_erase;

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

	struct trace_config
	{
		bool buzzer = false;
		bool ccont_adc = false;
		bool ccont_rtc = false;
		bool ccont_watchdog = false;
		bool display = false;
		bool display_io = false;
		bool display_profile = false;
		bool dsp_boundary = false;
		bool dsp_shared_reads = false;
		bool dsp_shared_transitions = false;
		bool gensio = false;
		bool keypad = false;
		bool mad2_clocks = false;
		bool mad2_interrupts = false;
		bool mad2_ledger = false;
		bool mad2_timers = false;
		bool mbus = false;
		bool pup_outputs = false;
		bool sim_rx = false;
		unsigned gensio_limit = 20'000;

		bool firmware() const
		{
			return ccont_rtc || ccont_watchdog || display || display_profile || dsp_boundary;
		}
	} m_trace;
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
	case 0x16:  return "[PUP] FIQ 8 (timer?) interrupt control (rw)";
	case 0x18:  return "[PUP] MBUS control (rw)";
	case 0x19:  return "[PUP] MBUS status (rw)";
	case 0x1A:  return "[PUP] MBUS RX/TX (rw)";
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
// NOKIA_DCT3_* environment controls below are test-harness or diagnostic inputs.
// Normal machine composition, clocks, ADC tuples and peer enablement are fixed
// by nokia_product_config and never selected from the process environment.
//
// Firmware-address traces are quarantined in nokia_3310_trace.inc. Add no
// forced firmware results or messages. See docs/driver_structure.md.
// ============================================================================
static unsigned nokia_env_u32(const char *name, unsigned fallback)
{
	// Environment options are immutable for a run. Device configuration and
	// machine_start resolve them once; this cache also keeps reset-time scenario
	// reads cheap without putting process lookups back into emulation callbacks.
	static std::unordered_map<const char *, std::optional<unsigned>> cache;
	auto it = cache.find(name);
	if (it == cache.end())
	{
		std::optional<unsigned> resolved;
		if (const char *value = std::getenv(name))
		{
			char *end = nullptr;
			const unsigned long parsed = std::strtoul(value, &end, 0);
			if (end != value)
				resolved = unsigned(parsed);
		}
		it = cache.emplace(name, resolved).first;
	}

	return it->second.value_or(fallback);
}

void noki3310_state::machine_start()
{
	m_trace.buzzer = nokia_env_u32("NOKIA_DCT3_TRACE_BUZZER", 0) != 0;
	m_trace.ccont_adc = nokia_env_u32("NOKIA_DCT3_TRACE_CCONT_ADC", 0) != 0;
	m_trace.ccont_rtc = nokia_env_u32("NOKIA_DCT3_TRACE_CCONT_RTC", 0) != 0;
	m_trace.ccont_watchdog = nokia_env_u32("NOKIA_DCT3_TRACE_CCONT_WATCHDOG", 0) != 0;
	m_trace.display = nokia_env_u32("NOKIA_DCT3_TRACE_DISPLAY", 0) != 0;
	m_trace.display_io = nokia_env_u32("NOKIA_DCT3_TRACE_DISPLAY_IO", 0) != 0;
	m_trace.display_profile = nokia_env_u32("NOKIA_DCT3_TRACE_DISPLAY_PROFILE", 0) != 0;
	m_trace.dsp_boundary = nokia_env_u32("NOKIA_DCT3_TRACE_DSP_BOUNDARY", 0) != 0;
	m_trace.dsp_shared_reads = nokia_env_u32("NOKIA_DCT3_TRACE_DSP_SHARED_READS", 0) != 0;
	m_trace.dsp_shared_transitions = nokia_env_u32("NOKIA_DCT3_TRACE_DSP_SHARED_TRANSITIONS", 0) != 0;
	m_trace.gensio = nokia_env_u32("NOKIA_DCT3_TRACE_GENSIO", 0) != 0;
	m_trace.keypad = nokia_env_u32("NOKIA_DCT3_TRACE_KEYPAD", 0) != 0;
	m_trace.mad2_clocks = nokia_env_u32("NOKIA_DCT3_TRACE_MAD2_CLOCKS", 0) != 0;
	m_trace.mad2_interrupts = nokia_env_u32("NOKIA_DCT3_TRACE_MAD2_INTERRUPTS", 0) != 0;
	m_trace.mad2_ledger = nokia_env_u32("NOKIA_DCT3_TRACE_MAD2_LEDGER", 0) != 0;
	m_trace.mad2_timers = nokia_env_u32("NOKIA_DCT3_TRACE_MAD2_TIMERS", 0) != 0;
	m_trace.mbus = nokia_env_u32("NOKIA_DCT3_TRACE_MBUS", 0) != 0;
	m_trace.pup_outputs = nokia_env_u32("NOKIA_DCT3_TRACE_PUP_OUTPUTS", 0) != 0;
	m_trace.sim_rx = nokia_env_u32("NOKIA_DCT3_TRACE_SIM_RX", 0) != 0;
	m_trace.gensio_limit = nokia_env_u32("NOKIA_DCT3_TRACE_GENSIO_LIMIT", 20'000);
	m_mad2->set_timer_trace(m_trace.mad2_timers);
	m_mad2->set_interrupt_trace(m_trace.mad2_interrupts);
	m_mad2->set_clock_trace(m_trace.mad2_clocks);
	m_mbus->set_trace(m_trace.mbus);
	m_ccont->set_adc_trace(m_trace.ccont_adc);
	m_ccont->set_rtc_trace(m_trace.ccont_rtc);
	m_dspif->set_trace_enabled(m_trace.dsp_boundary);
	m_dsp_hle->set_trace_enabled(m_trace.dsp_boundary);
	m_radio_peer->set_trace_enabled(m_trace.dsp_boundary);
	m_external_service_peer->set_trace_enabled(m_trace.dsp_boundary);
	m_sim_card->set_trace(m_trace.sim_rx);
	m_ram = std::make_unique<uint16_t[]>((NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1);

	m_timer_watchdog = timer_alloc(FUNC(noki3310_state::timer_watchdog), this);
	m_timer_mbus_rx_fixture = timer_alloc(FUNC(noki3310_state::timer_mbus_rx_fixture), this);
	m_timer_flash_b3_erase = timer_alloc(FUNC(noki3310_state::timer_flash_b3_erase), this);
	save_pointer(NAME(m_ram), (NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1);
	save_item(NAME(m_power_on));
	save_item(NAME(m_keypad_columns));
	save_item(NAME(m_keypad_irq_latched));
	save_item(NAME(m_ccont_irq_state));
	save_item(NAME(m_baseband_powered));
	save_item(NAME(m_flash_b3_lock_command));
	save_item(NAME(m_flash_b3_program_data));
	save_item(NAME(m_flash_b3_erase_confirm));
	save_item(NAME(m_flash_b3_erase_active));
	save_item(NAME(m_flash_b3_erase_suspended));
	save_item(NAME(m_flash_b3_status_override));
	save_item(NAME(m_flash_b3_erase_remaining_us));
	save_item(NAME(m_mad2_regs));
	save_item(NAME(m_mcuif_regs));
	machine().save().register_postload(save_prepost_delegate(FUNC(noki3310_state::post_load), this));
}

void noki3310_state::post_load()
{
	update_keypad_ccont_irqs();
	update_buzzer();
	update_vibrator();
	update_dsp_tones();
}

void noki3310_state::apply_product_config(nokia_product_config const &product)
{
	m_product = product;
	m_dsp_hle->set_bootstrap_exchange_limit(product.dsp_bootstrap_exchanges);
	m_dsp_hle->set_bootstrap_ping_pong(product.dsp_bootstrap_ping_pong);
	m_dsp_hle->set_code_block_request(product.dsp_code_block_request);
	m_dsp_hle->set_parked_boot_status(product.dsp_parked_boot_status,
			product.dsp_boot_status_response);
	m_mad2->set_dsp_reset_running_status(product.dsp_reset_running_status);
	m_mad2->set_dsp_release_mask(product.dsp_release_mask);
	m_lcd->set_geometry(product.lcd_controller_width, product.lcd_controller_height,
			product.lcd_visible_width, product.lcd_visible_height);
	screen_device *const screen = subdevice<screen_device>("screen");
	screen->set_size(product.lcd_visible_width, product.lcd_visible_height);
	screen->set_visarea(0, product.lcd_visible_width - 1, 0, product.lcd_visible_height - 1);
	const bool external_service = product.external_service;
	m_dsp_hle->set_service_enabled(product.dsp_service);
	m_dsp_hle->set_external_service_enabled(product.external_service);
	const unsigned dsp_default_ms = product.external_service ? 4 : 5;
	unsigned service_delay_us = product.dsp_service_delay_us != 0 ?
			product.dsp_service_delay_us : dsp_default_ms * 1'000;
	m_dsp_hle->set_service_delay_us(service_delay_us);
	m_dsp_hle->set_peer_poll_ms(dsp_default_ms);
	m_external_service_peer->set_enabled(external_service);
	m_radio_peer->set_enabled(product.radio_peer);
}

uint16_t noki3310_state::fw_word(offs_t address) const
{
	if (address < NOKIA_RAM_BASE || address >= NOKIA_RAM_END)
		return 0xffff;

	return m_ram[(address - NOKIA_RAM_BASE) >> 1];
}

uint8_t noki3310_state::fw_byte(offs_t address) const
{
	const uint16_t word = fw_word(address);
	return BIT(address, 0) ? uint8_t(word & 0x00ff) : uint8_t(word >> 8);
}

uint32_t noki3310_state::fw_dword(offs_t address) const
{
	return (uint32_t(fw_word(address)) << 16) | uint32_t(fw_word(address + 2));
}

void noki3310_state::machine_reset()
{
	std::fill_n(m_ram.get(), (NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1, 0);

	// according to the boot rom disassembly here http://www.nokix.pasjagsm.pl/help/blacksphere/sub_100hardware/sub_arm/sub_bootrom.htm
	// flash entry point is at 0x200040, we can probably reassemble the above code, but for now this should be enough.
	m_maincpu->set_state_int(arm7_cpu_device::ARM7_R15, NOKIA_FLASH_ENTRY);

	memset(m_mad2_regs, 0, 0x100);
	update_buzzer();
	update_vibrator();
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
	m_flash_b3_lock_command = false;
	m_flash_b3_program_data = false;
	m_flash_b3_erase_confirm = false;
	m_flash_b3_erase_active = false;
	m_flash_b3_erase_suspended = false;
	m_flash_b3_status_override = false;
	m_flash_b3_erase_remaining_us = 0;
	m_timer_flash_b3_erase->adjust(attotime::never);
	// These overrides are retained strictly for negative/conformance fixtures.
	// Normal runs use the typed product configuration installed above.
	m_mad2->set_timer0_hz(nokia_env_u32("NOKIA_DCT3_TIMER0_HZ", 33'055));
	m_mad2->set_timer1_hz(nokia_env_u32("NOKIA_DCT3_TIMER1_HZ", 1'057));
	m_mad2->set_fiq8_hz(nokia_env_u32("NOKIA_DCT3_FIQ8_HZ", 1'000));
	m_mad2->set_timer0_catchup(nokia_env_u32("NOKIA_DCT3_TIMER0_CATCHUP", 0) != 0);
	if (nokia_env_u32("NOKIA_DCT3_DSPIF_CONFORMANCE", 0) != 0)
		logerror("dspif_fixture: conformance=%02x\n", m_dspif->run_conformance_checks());
	// Load the deterministic product-level selector tuple. Electrical signal
	// names and units remain deliberately unassigned where board evidence is absent.
	for (unsigned id = 0; id < 8; id++)
		m_ccont->set_adc_source(id, m_product.adc_defaults[id]);
	m_ccont->set_wddisx_grounded(m_product.ccont_wddisx_grounded);
	m_ccont->set_ready(nokia_env_u32("NOKIA_DCT3_CCONT_READY", 1) != 0);
	m_simi->set_enabled(m_product.sim_device &&
			nokia_env_u32("NOKIA_DCT3_MODEL_SIM_DEVICE", 1) != 0);
	m_dsp_hle->set_service_enabled(m_product.dsp_service &&
			nokia_env_u32("NOKIA_DCT3_MODEL_DSP_SERVICE", 1) != 0);
	m_dsp_hle->set_external_service_enabled(m_product.external_service &&
			nokia_env_u32("NOKIA_DCT3_MODEL_EXTERNAL_SERVICE_PEER", 1) != 0);
	m_external_service_peer->set_enabled(m_product.external_service &&
			nokia_env_u32("NOKIA_DCT3_MODEL_EXTERNAL_SERVICE_PEER", 1) != 0);
	m_radio_peer->set_enabled(m_product.radio_peer &&
			nokia_env_u32("NOKIA_DCT3_MODEL_RADIO_PEER", 1) != 0);
	m_sim_card->set_cphs_aoc(nokia_env_u32("NOKIA_DCT3_SIM_CPHS_AOC", 0) != 0);
	m_sim_card->set_cached_location(nokia_env_u32("NOKIA_DCT3_SIM_CACHED_LOCATION", 0) != 0);
	{
		u8 atr[40] = { 0x3b, 0x10, 0x05 };
		unsigned length = 3;
		if (const char *hex = std::getenv("NOKIA_DCT3_SIM_ATR_HEX"))
		{
			length = 0;
			for (const char *p = hex; p[0] && p[1] && length < std::size(atr); p += 2)
				atr[length++] = u8(std::strtoul(std::string(p, 2).c_str(), nullptr, 16));
		}
		m_sim_card->set_atr(atr, length);
	}
	m_eeprom->write_scl(1);
	m_eeprom->write_sda(1);

	m_keypad_columns = 0x1f;
	m_keypad_irq_latched = false;
	m_ccont_irq_state = false;
	m_timer_watchdog->adjust(attotime::from_hz(1), 0, attotime::from_hz(1));
	if (std::getenv("NOKIA_DCT3_MBUS_RX_FIXTURE"))
		m_timer_mbus_rx_fixture->adjust(attotime::from_msec(
				nokia_env_u32("NOKIA_DCT3_MBUS_RX_FIXTURE_AT_MS", 300)),
				nokia_env_u32("NOKIA_DCT3_MBUS_RX_FIXTURE", 0xff) & 0xff);
	else
		m_timer_mbus_rx_fixture->adjust(attotime::never);

	// Simulate the product-specific keypad column used by the power-on input.
	m_power_on = ~m_product.power_on_column_mask;
}

void noki3310_state::mad2_fiq_w(int state)
{
	m_maincpu->set_input_line(1, state ? ASSERT_LINE : CLEAR_LINE);
}

void noki3310_state::mad2_irq_w(int state)
{
	m_maincpu->set_input_line(0, state ? ASSERT_LINE : CLEAR_LINE);
}

void noki3310_state::mad2_sleep_w(int state)
{
	if (state)
		m_maincpu->suspend(SUSPEND_REASON_HALT, true);
	else
		m_maincpu->resume(SUSPEND_REASON_HALT);
}

void noki3310_state::mad2_reset_w(int state)
{
	if (state)
		machine().scheduler().synchronize(
				timer_expired_delegate(FUNC(noki3310_state::deferred_mad2_reset), this));
}

TIMER_CALLBACK_MEMBER(noki3310_state::deferred_mad2_reset)
{
	reset_digital_baseband();
	// Bit 2 records the MCU-initiated reset that brought the new boot up;
	// device_reset supplies the persistent power-reset bit 0.
	m_mad2->set_reset_cause(0x04);
}

void noki3310_state::mad2_irq_ack_w(u16 mask)
{
	if (mask & (u16(1) << KEYPAD_IRQ_LINE_NUM))
	{
		m_keypad_irq_latched = false;
		update_keypad_ccont_irqs();
	}
}

void noki3310_state::ccont_irq_w(int state)
{
	// MAD2 latches the rising CCONT indication. Its IRQ acknowledgement clears
	// that pending edge; CCONT retains the source in register 0x0e until the
	// deferred firmware service acknowledges it through GENSIO.
	if (state && !m_ccont_irq_state)
		m_mad2->assert_irq(CCONT_IRQ_LINE_NUM);
	if (m_trace.ccont_rtc && state != m_ccont_irq_state)
		logerror("ccont_route: state=%u irq_line=%u pending=%03x mask=%02x ctrl=%02x t=%.9f\n",
			state, CCONT_IRQ_LINE_NUM, m_mad2->irq_status(), m_mad2->reg(MAD2_IRQ_MASK),
			m_mad2->reg(MAD2_IRQ_CTRL), machine().time().as_double());
	m_ccont_irq_state = bool(state);
}

void noki3310_state::ccont_power_w(int state)
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

void noki3310_state::reset_digital_baseband()
{
	// These blocks share the switched digital-baseband domain. CCONT, flash and
	// EEPROM intentionally survive this reset and retain their state.
	m_maincpu->reset();
	m_mad2->reset();
	m_gensio->reset();
	m_mbus->reset();
	m_dspif->reset();
	m_dsp_hle->reset();
	m_external_service_peer->reset();
	m_simi->reset();
	m_sim_card->reset();
	m_lcd->reset();
	machine_reset();
	m_power_on = 0;
}

void noki3310_state::sim_irq_w(int state)
{
	if (state)
		m_mad2->assert_fiq(6);
}

void noki3310_state::dsp_fiq0_w(int state)
{
	if (state)
		m_mad2->assert_fiq(0);
}

void noki3310_state::dsp_service_irq_w(int state)
{
	if (state)
		m_mad2->assert_irq(4);
}

void noki3310_state::dsp_tx_commit_w(int state)
{
	if (state && m_trace.dsp_boundary)
	{
		nokia_dspif_device::packet packet;
		if (m_dspif->peek_tx_packet(packet) && packet.type == 0x0f)
			logerror("radio_type0f_commit: pc=%08x lr=%08x data0=%02x "
					"task16=%02x/%02x/%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
					u32(m_maincpu->pc()),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					packet.length != 0 ? packet.payload[0] : 0xff,
					fw_byte(0x0010faec), fw_byte(0x0010faef),
					fw_byte(0x0010faf4), fw_byte(0x0010faff),
					fw_byte(0x0010fb08), fw_byte(0x0010fb14),
					fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	m_dsp_hle->tx_commit_w(state);
}

void noki3310_state::dsp_service_pending_w(int state)
{
	m_dsp_hle->service_pending_w(state);
}

void noki3310_state::dsp_doorbell_w(int state)
{
	m_dsp_hle->doorbell_w(state);
}

uint8_t noki3310_state::keypad_columns_r(bool consume_power_on)
{
	uint8_t data = 0x1f;
	const uint8_t row_mask = m_product.keypad_five_rows ? 0x1f : 0x0f;
	const uint8_t rows_low = m_mad2_regs[0xa8] & ~m_mad2_regs[0x28] & row_mask;

	for (unsigned column = 0; column < 5; column++)
	{
		const uint8_t keys = m_keypad[column]->read();
		const unsigned row_count = m_product.keypad_five_rows ? 5 : 4;
		for (unsigned row = 0; row < row_count; row++)
		{
			const unsigned key_bit = m_product.keypad_five_rows ? row : row + 1;
			if (BIT(rows_low, row) && !BIT(keys, key_bit))
				data &= ~(uint8_t(1) << column);
		}
	}

	if (!BIT(m_pwr->read(), 0))
		data &= 0xfe;
	if (m_power_on)
	{
		data &= m_power_on;
		if (consume_power_on)
			m_power_on = 0;
	}
	return data | 0xe0;
}

void noki3310_state::update_keypad_ccont_irqs()
{
	const u16 before = m_mad2->irq_status();
	m_mad2->set_irq_line(KEYPAD_IRQ_LINE_NUM, m_keypad_irq_latched);
	const u16 after = m_mad2->irq_status();
	if (before != after &&
			m_trace.mad2_interrupts &&
			m_mad2_interrupt_trace_count++ < 4096)
		logerror("mad2_interrupt: event=levels domain=IRQ keypad=%u ccont=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				m_keypad_irq_latched, m_ccont_irq_state, before, after,
				machine().time().as_double());
}

void noki3310_state::update_keypad_columns()
{
	const uint8_t columns = keypad_columns_r();
	const uint8_t falling = m_keypad_columns & ~columns & ~m_mad2_regs[0x6b] & 0x1f;
	m_keypad_columns = columns & 0x1f;
	if (falling)
	{
		m_keypad_irq_latched = true;
		update_keypad_ccont_irqs();
	}
}

void noki3310_state::mbus_fiq2_w(int state)
{
	if (state)
		m_mad2->assert_fiq(2);
}

void noki3310_state::mbus_fiq3_w(int state)
{
	if (state)
		m_mad2->assert_fiq(3);
}

void noki3310_state::mbus_tx_w(u8 data)
{
	if (m_trace.mbus && m_mbus_trace_count++ < 8192)
		logerror("mbus: event=TX data=%02x pc=%08x t=%.9f\n", data,
				m_maincpu->pc(), machine().time().as_double());
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_mbus_rx_fixture)
{
	const bool accepted = m_mbus->receive_byte(u8(param));
	logerror("mbus_fixture: data=%02x accepted=%u t=%.9f\n", u8(param), accepted,
			machine().time().as_double());
}

void noki3310_state::trace_interrupt_register(char operation, offs_t offset, uint8_t data)
{
	if (m_trace.mad2_interrupts &&
			m_mad2_interrupt_trace_count++ < 4096)
		logerror("mad2_interrupt: event=reg_%c off=%02x data=%02x fiq=%03x irq=%03x fiqmask=%02x irqmask=%02x ctrl=%02x extctrl=%02x t=%.9f\n",
				operation, u32(offset), data, m_mad2->fiq_status(), m_mad2->irq_status(),
				m_mad2->reg(MAD2_FIQ_MASK), m_mad2->reg(MAD2_IRQ_MASK),
				m_mad2->reg(MAD2_IRQ_CTRL), m_mad2->reg(MAD2_FIQ8_CTRL),
				machine().time().as_double());
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_watchdog)
{
	// CCONT watchdog
	if (m_ccont->watchdog_tick())
	{
		if (m_trace.ccont_watchdog)
			logerror("ccont_watchdog_expired: t=%.6f\n", machine().time().as_double());
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

TIMER_CALLBACK_MEMBER(noki3310_state::timer_flash_b3_erase)
{
	m_flash_b3_erase_active = false;
	m_flash_b3_erase_suspended = false;
	m_flash_b3_erase_remaining_us = 0;
}

// Hardware RAM read entry point (registered in the address map).
uint16_t noki3310_state::ram_r(offs_t offset, uint16_t mem_mask)
{
	return m_ram[offset] & mem_mask;
}

// Hardware RAM write entry point registered in the address map.
void noki3310_state::ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_ram[offset]);
}


uint16_t noki3310_state::eeprom_r(offs_t offset, uint16_t mem_mask)
{
	memory_region *eeprom = memregion("eeprom");
	uint16_t data = 0xffff;

	if (eeprom && offset < (eeprom->bytes() / 2))
		data = eeprom->as_u16(offset);

	return data & mem_mask;
}

void noki3310_state::eeprom_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
}

uint16_t noki3310_state::dsp_ram_r(offs_t offset)
{
	offset &= 0x7ff;
	const uint16_t data = m_dspif->shared_r(offset);
	const uint32_t pc = m_maincpu->pc();
	const uint64_t trace_key = (uint64_t(pc) << 11) | offset;
	const unsigned byte_offset = offset << 1;
	if (m_trace.dsp_shared_transitions &&
			(byte_offset <= 0x004 || byte_offset == 0x0a6 || byte_offset == 0x0e0 ||
			 byte_offset == 0x0e4 || byte_offset == 0x0fe || byte_offset == 0x100 ||
			 byte_offset == 0x1c8))
		logerror("dsp_shared_observe: off=%03x data=%04x pc=%08x t=%.6f\n",
				byte_offset, data, pc, machine().time().as_double());
	if (m_trace.dsp_shared_reads)
	{
		auto [trace_item, inserted] = m_dsp_shared_trace_reads.emplace(trace_key, data);
		if (inserted || trace_item->second != data)
		{
			trace_item->second = data;
			logerror("dsp_shared_read: off=%03x data=%04x pc=%08x t=%.6f\n",
					byte_offset, data, pc, machine().time().as_double());
		}
	}
	return data;
}

void noki3310_state::dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	m_dspif->shared_w(offset, data, mem_mask);
	const unsigned byte_offset = (offset & 0x7ff) << 1;
	if (byte_offset == 0x0ae || byte_offset == 0x0b0 || byte_offset == 0x0b6)
		update_dsp_tones();
}

#include "nokia_3310_trace.inc"

uint16_t noki3310_state::flash_r(offs_t offset, uint16_t mem_mask)
{
	const u32 pc = m_maincpu->pc();
	const u32 addr = 0x00200000 + (offset << 1);
	flash_firmware_traces(pc, addr);
	// The 3410's B3 driver sends commands and polls status through a fixed
	// CSR while reading record data from other read-while-write partitions.
	// The generic flash core exposes one global command mode, so bypass that
	// mode for ordinary array reads while the externally timed erase is active.
	u16 value;
	if (m_product.flash_b3_block_lock && m_flash_b3_status_override &&
			(addr & ~u32(1)) != NOKIA_3410_FLASH_STATUS_CSR)
	{
		const u8 *const array = m_flash->base();
		value = (u16(array[offset * 2]) << 8) | array[offset * 2 + 1];
	}
	else
	{
		value = m_flash->read(offset);
	}
	if (m_product.flash_b3_block_lock && m_flash_b3_status_override &&
			(addr & ~u32(1)) == NOKIA_3410_FLASH_STATUS_CSR)
	{
		// The 3410 interleaves writes to another partition while a B3 erase is
		// suspended.  The generic flash core has a single global command state,
		// so preserve the independently observable erase status here.
		if (m_flash_b3_erase_suspended)
			return 0x00c0 & mem_mask; // ready + erase suspended
		return (m_flash_b3_erase_active ? 0x0000 : 0x0080) & mem_mask;
	}
	return value & mem_mask;
}

void noki3310_state::flash_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (m_product.flash_b3_block_lock)
	{
		if (m_flash_b3_program_data)
		{
			// The word following 40 is array data, even when its low byte is also
			// a command opcode (the observed 3410 PMM stream contains xx60).
			m_flash_b3_program_data = false;
		}
		else if (m_flash_b3_lock_command)
		{
			m_flash_b3_lock_command = false;
			// Intel B3 uses 60/01, 60/d0 and 60/2f for block lock,
			// unlock and lock-down.  The current firmware unlocks every block
			// before programming and relocks it afterwards; expose ready status
			// while the flash core remains responsible for the data operation.
			if ((data & 0xff) == 0x01 || (data & 0xff) == 0xd0 || (data & 0xff) == 0x2f)
			{
				m_flash->write(offset, 0x70);
				return;
			}
		}
		else if ((data & 0xff) == 0x60)
		{
			m_flash_b3_lock_command = true;
			return;
		}
		else if (m_flash_b3_erase_active && m_flash_b3_erase_suspended &&
				(data & 0xff) == 0xd0)
		{
			m_flash_b3_erase_suspended = false;
			m_flash_b3_status_override = true;
			m_timer_flash_b3_erase->adjust(attotime::from_usec(
					std::max<u64>(1, m_flash_b3_erase_remaining_us)));
			return;
		}
		else if (m_flash_b3_erase_active && (data & 0xff) == 0xb0)
		{
			m_flash_b3_erase_remaining_us = std::max<u64>(1,
					m_timer_flash_b3_erase->remaining().as_ticks(1'000'000));
			m_timer_flash_b3_erase->adjust(attotime::never);
			m_flash_b3_erase_suspended = true;
			m_flash_b3_status_override = true;
			return;
		}
		else if ((data & 0xff) == 0x20)
		{
			m_flash_b3_erase_confirm = true;
		}
		else if ((data & 0xff) == 0x40)
		{
			m_flash_b3_program_data = true;
		}
		else if (m_flash_b3_erase_confirm)
		{
			m_flash_b3_erase_confirm = false;
			if ((data & 0xff) == 0xd0)
			{
				m_flash_b3_erase_active = true;
				m_flash_b3_erase_suspended = false;
				m_flash_b3_status_override = true;
				// Approximate only: the firmware polls ready status, but no physical
				// M28W320ECT erase-duration measurement is available yet.
				m_flash_b3_erase_remaining_us = 1'000'000;
				m_timer_flash_b3_erase->adjust(attotime::from_seconds(1));
			}
		}
		if ((data & 0xff) == 0xff || (data & 0xff) == 0xf0)
			m_flash_b3_status_override = false;
	}
	m_flash->write(offset, data);
}

uint32_t noki3310_state::rom2_mirror_r(offs_t offset, uint32_t mem_mask)
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

void noki3310_state::rom2_mirror_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
}

uint8_t noki3310_state::mad2_io_r(offs_t offset)
{
	const uint8_t data = mad2_register_r(offset);
	trace_mad2_read(offset, data);
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x = %02x %s\n", offset, data, nokia_mad2_reg_desc(offset));
	return data;
}

uint8_t noki3310_state::mad2_register_r(offs_t offset)
{
	uint8_t data = offset <= MAD2_FIQ8_CTRL ? m_mad2->read(offset) :
			(offset >= MAD2_MBUS_CTRL && offset <= 0x1a ? m_mbus->read(offset - MAD2_MBUS_CTRL) :
			(nokia_gensio_device::owns(offset) ? m_gensio->read(offset) : m_mad2_regs[offset]));

	switch(offset)
	{
		case 0x2a:
			data = keypad_columns_r(true);
			if (m_trace.keypad)
				logerror("keypad: event=cols_r data=%02x rows=%02x dir=%02x irqc=%02x pc=%08x t=%.9f\n",
						data, m_mad2_regs[MAD2_KEYBOARD_ROWS], m_mad2_regs[0xa8],
						m_mad2_regs[0x6b], m_maincpu->pc(), machine().time().as_double());
			break;
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

	if (offset == 0x20)
	{
		const bool sda_output = BIT(m_mad2_regs[0x24], 0);
		const int sda = sda_output ? (BIT(data, 0) & m_eeprom->read_sda()) : m_eeprom->read_sda();
		data = (data & 0xfe) | sda;
	}
	return data;
}

void noki3310_state::trace_mad2_read(offs_t offset, uint8_t data)
{
	if (m_trace.sim_rx &&
			m_simi->enabled() &&
			(offset == 0x37 || offset == 0x38 || offset == 0x3c))
	{
		static unsigned sim_fifo_read_count = 0;
		if (sim_fifo_read_count++ < 64)
			logerror("sim_fifo_read: off=%02x data=%02x remaining=%u pc=%08x t=%.8f\n",
					offset, data, m_simi->rx_count_r(), m_maincpu->pc(), machine().time().as_double());
	}
	if (m_trace.gensio &&
			nokia_gensio_device::owns(offset) &&
			m_gensio_trace_count++ < m_trace.gensio_limit)
		logerror("gensio: R off=%02x data=%02x pc=%08x t=%.9f\n", offset, data,
				m_maincpu->pc(), machine().time().as_double());
	if (m_trace.mad2_timers &&
			((offset >= 0x08 && offset <= 0x13) || offset == 0x0a) &&
			m_mad2_timer_trace_count++ < 4096)
		logerror("mad2_timer: event=R off=%02x data=%02x pc=%08x t=%.9f\n",
				u32(offset), data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace.mad2_clocks &&
			(offset == MAD2_MCU_RESET_CTRL || offset == MAD2_WATCHDOG ||
			 (offset >= MAD2_TIMER1_COUNTER_MSB && offset <= MAD2_TIMER1_DESTINATION_LSB) ||
			 offset == 0x0d) && m_mad2_clock_trace_count++ < 4096)
		logerror("mad2_clock: event=R off=%02x data=%02x counter=%04x pc=%08x t=%.9f\n",
				u32(offset), data, m_mad2->timer1_counter(), m_maincpu->pc(), machine().time().as_double());
	if (m_trace.mbus &&
			(offset == MAD2_MBUS_CTRL || offset == MAD2_MBUS_STATUS || offset == 0x1a ||
			 offset == MAD2_FIQ_STATUS || offset == MAD2_FIQ_MASK) && m_mbus_trace_count++ < 8192)
		logerror("mbus: event=R off=%02x data=%02x ctrl=%02x status=%02x fiq=%03x mask=%02x pc=%08x t=%.9f\n",
				u32(offset), data, m_mbus->control(), m_mbus->status(),
				m_mad2->fiq_status(), m_mad2->reg(MAD2_FIQ_MASK), m_maincpu->pc(), machine().time().as_double());
	if (offset == MAD2_FIQ_STATUS || offset == MAD2_IRQ_STATUS ||
			offset == MAD2_FIQ_MASK || offset == MAD2_IRQ_MASK ||
			offset == MAD2_IRQ_CTRL || offset == MAD2_FIQ8_CTRL)
		trace_interrupt_register('R', offset, data);

	if (m_trace.mad2_ledger && !m_mad2_trace_read[offset])
	{
		m_mad2_trace_read[offset] = true;
		logerror("mad2_ledger: R off=%02x data=%02x pc=%08x t=%.6f %s\n", offset, data,
				m_maincpu->pc(), machine().time().as_double(), nokia_mad2_reg_desc(offset));
	}
}

void noki3310_state::mad2_io_w(offs_t offset, uint8_t data)
{
	const uint8_t old_data = mad2_register_peek(offset);
	mad2_register_w(offset, data);
	mad2_board_outputs_w(offset);
	trace_mad2_write(offset, data, old_data);
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x %s\n", offset, data, nokia_mad2_reg_desc(offset));
}

uint8_t noki3310_state::mad2_register_peek(offs_t offset)
{
	const bool core_register = offset <= MAD2_FIQ8_CTRL;
	const bool mbus_register = offset >= MAD2_MBUS_CTRL && offset <= 0x1a;
	const bool gensio_register = nokia_gensio_device::owns(offset);
	return core_register ? m_mad2->read(offset) :
			(mbus_register ? (offset == MAD2_MBUS_CTRL ? m_mbus->control() :
				offset == MAD2_MBUS_STATUS ? m_mbus->status() : m_mbus->data()) : gensio_register ?
				m_gensio->peek(offset) :
				m_mad2_regs[offset]);
}

void noki3310_state::mad2_register_w(offs_t offset, uint8_t data)
{
	const bool core_register = offset <= MAD2_FIQ8_CTRL;
	const bool mbus_register = offset >= MAD2_MBUS_CTRL && offset <= 0x1a;
	const bool gensio_register = nokia_gensio_device::owns(offset);
	if (core_register)
		m_mad2->write(offset, data);
	else if (mbus_register)
		m_mbus->write(offset - MAD2_MBUS_CTRL, data);
	else if (gensio_register)
		m_gensio->write(offset, data);
	else
		m_mad2_regs[offset] = data;

	if (offset == 0x0d)
		m_simi->set_clock_enabled(BIT(data, 5));

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

void noki3310_state::mad2_board_outputs_w(offs_t offset)
{
	if (m_trace.keypad &&
			(offset == MAD2_KEYBOARD_ROWS || offset == 0x6b || offset == 0xa8))
		logerror("keypad: event=reg_w off=%02x data=%02x rows=%02x dir=%02x irqc=%02x cols=%02x matrix3=%02x pc=%08x t=%.9f\n",
				u32(offset), m_mad2_regs[offset], m_mad2_regs[MAD2_KEYBOARD_ROWS],
				m_mad2_regs[0xa8], m_mad2_regs[0x6b], keypad_columns_r(false),
				m_keypad[3]->read(),
				m_maincpu->pc(), machine().time().as_double());
	if (offset == 0x15 || offset == 0x1c || offset == 0x1d || offset == 0x1e)
		update_buzzer();
	if (offset == 0x15 || offset == 0x1b)
		update_vibrator();

	if (offset == 0x20 || offset == 0x24)
	{
		const uint8_t signal = m_mad2_regs[0x20];
		const uint8_t direction = m_mad2_regs[0x24];
		m_eeprom->write_sda(BIT(direction, 0) ? BIT(signal, 0) : 1);
		m_eeprom->write_scl(BIT(signal, 3));
	}
	if (offset == MAD2_KEYBOARD_ROWS || offset == 0x6b || offset == 0xa8)
		update_keypad_columns();
}

void noki3310_state::trace_mad2_write(offs_t offset, uint8_t data, uint8_t old_data)
{
	const bool gensio_register = nokia_gensio_device::owns(offset);
	const bool pup_output_change =
		(offset == 0x15 && ((old_data ^ data) & 0x30) != 0) ||
		(offset >= 0x1b && offset <= 0x1e && old_data != data) ||
		(offset == 0x20 && ((old_data ^ data) & 0x40) != 0);
	if (m_trace.pup_outputs && pup_output_change)
		logerror("pup_output: off=%02x data=%02x old=%02x buzzer=%u vibra=%u backlight6=%u pc=%08x t=%.9f\n",
			u32(offset), data, old_data, BIT(m_mad2->reg(0x15), 5),
			BIT(m_mad2->reg(0x15), 4), BIT(m_mad2_regs[0x20], 6),
			m_maincpu->pc(), machine().time().as_double());
	if (m_trace.mad2_timers &&
			(offset >= 0x08 && offset <= 0x13) &&
			m_mad2_timer_trace_count++ < 4096)
		logerror("mad2_timer: event=W off=%02x data=%02x old=%02x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace.mad2_clocks &&
			(offset == MAD2_MCU_RESET_CTRL || offset == MAD2_WATCHDOG ||
			 (offset >= MAD2_TIMER1_COUNTER_MSB && offset <= MAD2_TIMER1_DESTINATION_LSB) ||
			 offset == 0x0d) && m_mad2_clock_trace_count++ < 4096)
		logerror("mad2_clock: event=W off=%02x data=%02x old=%02x counter=%04x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_mad2->timer1_counter(), m_maincpu->pc(), machine().time().as_double());
	if (m_trace.mbus &&
			(offset == MAD2_MBUS_CTRL || offset == MAD2_MBUS_STATUS || offset == 0x1a ||
			 offset == MAD2_FIQ_STATUS || offset == MAD2_FIQ_MASK) && m_mbus_trace_count++ < 8192)
		logerror("mbus: event=W off=%02x data=%02x old=%02x ctrl=%02x status=%02x fiq=%03x mask=%02x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_mbus->control(), m_mbus->status(),
				m_mad2->fiq_status(), m_mad2->reg(MAD2_FIQ_MASK), m_maincpu->pc(), machine().time().as_double());
	if (m_trace.sim_rx && offset == 0x36)
	{
		static unsigned sim_txd_count = 0;
		if (sim_txd_count++ < 128)
			logerror("sim_txd: data=%02x pc=%08x t=%.8f\n", data, m_maincpu->pc(),
					machine().time().as_double());
	}
	if (m_trace.sim_rx && offset == 0x39)
	{
		static unsigned sim_control_count = 0;
		if (sim_control_count++ < 128)
			logerror("sim_control_w: data=%02x old=%02x live=%02x pc=%08x t=%.8f\n", data,
					old_data, m_simi->control_r(), m_maincpu->pc(), machine().time().as_double());
	}
	if (m_trace.gensio &&
			gensio_register &&
			m_gensio_trace_count++ < m_trace.gensio_limit)
		logerror("gensio: W off=%02x data=%02x old=%02x pc=%08x t=%.9f\n", offset, data,
				old_data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace.display_io &&
			(offset == 0x2d || offset == 0x2e || offset == 0x6e) &&
			m_display_io_trace_count++ < 4096)
		logerror("display_io: off=%02x data=%02x old=%02x pc=%08x t=%.9f\n", offset,
				data, old_data, m_maincpu->pc(), machine().time().as_double());
	if (m_trace.mad2_ledger && !m_mad2_trace_write[offset])
	{
		m_mad2_trace_write[offset] = true;
		logerror("mad2_ledger: W off=%02x data=%02x old=%02x pc=%08x t=%.6f %s\n", offset,
				data, old_data, m_maincpu->pc(), machine().time().as_double(), nokia_mad2_reg_desc(offset));
	}

	if (offset == MAD2_FIQ_STATUS || offset == MAD2_IRQ_STATUS ||
			offset == MAD2_FIQ_MASK || offset == MAD2_IRQ_MASK ||
			offset == MAD2_IRQ_CTRL || offset == MAD2_FIQ8_CTRL)
		trace_interrupt_register('W', offset, data);
	if (m_trace.ccont_rtc &&
			offset == MAD2_IRQ_STATUS && BIT(data, CCONT_IRQ_LINE_NUM))
		logerror("ccont_route: event=mad_ack data=%02x pc=%08x t=%.9f\n",
			data, m_maincpu->pc(), machine().time().as_double());
}

void noki3310_state::update_buzzer()
{
	const u16 divider = (u16(m_mad2_regs[0x1c]) << 8) | m_mad2_regs[0x1d];
	if (divider != 0)
		m_buzzer->set_clock(13'000'000 / divider);
	const bool enabled = BIT(m_mad2->reg(0x15), 5) && divider != 0;
	m_buzzer->set_state(enabled);
	if (m_trace.buzzer)
		logerror("buzzer: enabled=%u divider=%u frequency=%u volume=%u t=%.6f\n",
				enabled, divider, divider ? 13'000'000 / divider : 0,
				m_mad2_regs[0x1e], machine().time().as_double());
}

void noki3310_state::update_vibrator()
{
	// PUP control bit 4 gates the output; register 0x1b carries the separate
	// frequency/mode setting. The 3210 used an optional vibra battery pack.
	const bool enabled = BIT(m_mad2->reg(0x15), 4);
	m_vibration = enabled;
	if (m_trace.pup_outputs)
		logerror("vibrator: enabled=%u control=%02x t=%.6f\n",
			enabled, m_mad2_regs[0x1b], machine().time().as_double());
}

void noki3310_state::update_dsp_tones()
{
	// The ROM-4 MCU programs the COBBA tone oscillators in quarter-Hz units.
	// A real DSP renders them through the codec; this HLE voice exposes the same
	// firmware-owned command while no DSP core or codec PCM backend is present.
	const u16 oscillator1 = m_dspif->shared_r(0x0ae >> 1);
	const u16 oscillator2 = m_dspif->shared_r(0x0b0 >> 1);
	const u16 amplitude = m_dspif->shared_r(0x0b6 >> 1);
	const unsigned frequency1 = oscillator1 >> 2;
	const unsigned frequency2 = oscillator2 >> 2;
	if (frequency1 != 0)
		m_dsp_tone1->set_clock(frequency1);
	if (frequency2 != 0)
		m_dsp_tone2->set_clock(frequency2);
	m_dsp_tone1->set_state(amplitude != 0 && frequency1 != 0);
	m_dsp_tone2->set_state(amplitude != 0 && frequency2 != 0);
	if (m_trace.dsp_boundary)
		logerror("dsp_tone: oscillator=%04x/%04x frequency=%u/%u amplitude=%04x active=%u/%u t=%.6f\n",
				oscillator1, oscillator2, frequency1, frequency2, amplitude,
				amplitude != 0 && frequency1 != 0, amplitude != 0 && frequency2 != 0,
				machine().time().as_double());
}

uint8_t noki3310_state::mad2_dspif_r(offs_t offset)
{
	offset &= 3;
	const u8 data = m_dspif->dspif_r(offset);
	if (m_trace.mad2_ledger && !m_dspif_trace_read[offset])
	{
		m_dspif_trace_read[offset] = true;
		logerror("mad2_ledger: R bus=DSPIF off=%02x data=%02x pc=%08x t=%.6f DSP API control\n",
				u32(offset), data, m_maincpu->pc(), machine().time().as_double());
	}
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x DSPIF\n", offset);
	return data;
}

void noki3310_state::mad2_dspif_w(offs_t offset, uint8_t data)
{
	offset &= 3;
	const u8 old_data = m_dspif->dspif_r(offset);
	if (m_trace.mad2_ledger && !m_dspif_trace_write[offset])
	{
		m_dspif_trace_write[offset] = true;
		logerror("mad2_ledger: W bus=DSPIF off=%02x data=%02x old=%02x pc=%08x t=%.6f DSP API control\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	}
	if (m_trace.dsp_boundary)
		logerror("dsp_boundary: DSPIF W off=%x data=%02x pc=%08x task=%02x t=%.6f\n",
				u32(offset), data, m_maincpu->pc() & ~u32(1), fw_byte(0x00100022),
				machine().time().as_double());
	m_dspif->dspif_w(offset, data);
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x DSPIF\n", offset, data);
}

uint8_t noki3310_state::mad2_mcuif_r(offs_t offset)
{
	offset &= 3;
	const u8 data = m_mcuif_regs[offset];
	if (m_trace.mad2_ledger && !m_mcuif_trace_read[offset])
	{
		m_mcuif_trace_read[offset] = true;
		logerror("mad2_ledger: R bus=MCUIF off=%02x data=%02x pc=%08x t=%.6f memory-window control\n",
				u32(offset), data, m_maincpu->pc(), machine().time().as_double());
	}
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x MCUIF\n", offset);
	return data;
}

void noki3310_state::mad2_mcuif_w(offs_t offset, uint8_t data)
{
	offset &= 3;
	const u8 old_data = m_mcuif_regs[offset];
	m_mcuif_regs[offset] = data;
	if (m_trace.mad2_ledger && !m_mcuif_trace_write[offset])
	{
		m_mcuif_trace_write[offset] = true;
		logerror("mad2_ledger: W bus=MCUIF off=%02x data=%02x old=%02x pc=%08x t=%.6f memory-window control\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	}
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x MCUIF\n", offset, data);
}

void noki3310_state::noki3310_map(address_map &map)
{
	map.global_mask(0x00ffffff);
	map(0x00000000, 0x0000ffff).mirror(0x80000).rw(FUNC(noki3310_state::ram_r), FUNC(noki3310_state::ram_w));                // boot ROM / RAM
	map(0x00010000, 0x00010fff).mirror(0x8f000).rw(FUNC(noki3310_state::dsp_ram_r), FUNC(noki3310_state::dsp_ram_w));        // DSP shared memory
	map(0x00020000, 0x000200ff).mirror(0x8ff00).rw(FUNC(noki3310_state::mad2_io_r), FUNC(noki3310_state::mad2_io_w));         // IO (Primary I/O range, configures peripherals)
	map(0x00030000, 0x00030003).mirror(0x8fffc).rw(FUNC(noki3310_state::mad2_dspif_r), FUNC(noki3310_state::mad2_dspif_w));   // DSPIF (API control register)
	map(0x00040000, 0x00040003).mirror(0x8fffc).rw(FUNC(noki3310_state::mad2_mcuif_r), FUNC(noki3310_state::mad2_mcuif_w));   // MCUIF (Secondary I/O range, configures memory ranges)
	map(0x00100000, 0x0017ffff).rw(FUNC(noki3310_state::ram_r), FUNC(noki3310_state::ram_w));                                   // RAMSelX
	map(0x00200000, 0x005fffff).rw(FUNC(noki3310_state::flash_r), FUNC(noki3310_state::flash_w));     // ROM1SelX
	map(0x00600000, 0x009fffff).rw(FUNC(noki3310_state::rom2_mirror_r), FUNC(noki3310_state::rom2_mirror_w));   // ROM2SelX mirror/window
	map(0x00a00000, 0x00a03fff).rw(FUNC(noki3310_state::eeprom_r), FUNC(noki3310_state::eeprom_w));           // EEPROMSelX
	map(0x00a04000, 0x00dfffff).unmaprw();                                                                   // EEPROMSelX
	map(0x00e00000, 0x00ffffff).unmaprw();                                                                   // Reserved
}

INPUT_CHANGED_MEMBER( noki3310_state::key_irq )
{
	if (m_trace.keypad)
		logerror("keypad: event=edge old=%u new=%u param=%u rows=%02x dir=%02x irqc=%02x cols=%02x matrix=%02x/%02x/%02x/%02x/%02x pc=%08x t=%.9f\n",
				oldval, newval, param, m_mad2_regs[MAD2_KEYBOARD_ROWS],
				m_mad2_regs[0xa8], m_mad2_regs[0x6b], keypad_columns_r(false),
				m_keypad[0]->read(), m_keypad[1]->read(), m_keypad[2]->read(),
				m_keypad[3]->read(), m_keypad[4]->read(),
				m_maincpu->pc(), machine().time().as_double());
	update_keypad_columns();
	// A physical matrix switch edge, including release, wakes the keypad ISR.
	// Row-drive writes also recompute columns but must not manufacture edges.
	m_keypad_irq_latched = true;
	update_keypad_ccont_irqs();
}

INPUT_CHANGED_MEMBER( noki3310_state::charger_irq )
{
	m_ccont->set_charger_input(newval != 0,
			nokia_env_u32("NOKIA_DCT3_CHARGER_ADC", 0x3ff));
}

static INPUT_PORTS_START( noki3210 )
	// Nokia 3210 v5.01/v6.00 share this ROM-derived matrix. COL.n is the
	// firmware read bit and bits 1..4 are driven rows; power uses the special
	// all-rows scan. Names describe the handset controls, while PORT_CODE gives
	// practical default host bindings which remain user-remappable in MAME.
	PORT_START("COL.0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Navi / Left Softkey") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 1") PORT_CODE(KEYCODE_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 2") PORT_CODE(KEYCODE_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("C / Right Softkey") PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 0") PORT_CODE(KEYCODE_0) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 3") PORT_CODE(KEYCODE_3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 5") PORT_CODE(KEYCODE_5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad #") PORT_CODE(KEYCODE_MINUS) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 4") PORT_CODE(KEYCODE_4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 8") PORT_CODE(KEYCODE_8) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad *") PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 6") PORT_CODE(KEYCODE_6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 7") PORT_CODE(KEYCODE_7) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 9") PORT_CODE(KEYCODE_9) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Power") PORT_CODE(KEYCODE_SPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Charger connected") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::charger_irq), 0)
INPUT_PORTS_END

static INPUT_PORTS_START( noki3310 )
	// NHM-5 v6.39 keymap: raw key = row * 5 + column. Unlike the
	// four-active-row 3210 layout, the 3310 uses every row of the MAD2 5x5 scan.
	PORT_START("COL.0")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 3") PORT_CODE(KEYCODE_3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 0") PORT_CODE(KEYCODE_0) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 1") PORT_CODE(KEYCODE_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 6") PORT_CODE(KEYCODE_6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 9") PORT_CODE(KEYCODE_9) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad #") PORT_CODE(KEYCODE_MINUS) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 2") PORT_CODE(KEYCODE_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 5") PORT_CODE(KEYCODE_5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 8") PORT_CODE(KEYCODE_8) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Menu") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Names / C") PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 4") PORT_CODE(KEYCODE_4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 7") PORT_CODE(KEYCODE_7) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad *") PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Power") PORT_CODE(KEYCODE_SPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Charger connected") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::charger_irq), 0)
INPUT_PORTS_END

static INPUT_PORTS_START( noki3410 )
	// NHM-2 v5.46 keymap table at 0x4c5130, indexed as row * 5 + column
	// by the scanner at 0x3e496e.  The numeric block matches the 3310,
	// while the two softkeys, scroll keys and send/end keys occupy the
	// previously unused cells around it.
	PORT_START("COL.0")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Right Softkey") PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("End") PORT_CODE(KEYCODE_E) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 3") PORT_CODE(KEYCODE_3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 0") PORT_CODE(KEYCODE_0) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 1") PORT_CODE(KEYCODE_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 6") PORT_CODE(KEYCODE_6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 9") PORT_CODE(KEYCODE_9) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad #") PORT_CODE(KEYCODE_MINUS) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 2") PORT_CODE(KEYCODE_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 5") PORT_CODE(KEYCODE_5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 8") PORT_CODE(KEYCODE_8) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Scroll Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Left Softkey / Menu") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Send") PORT_CODE(KEYCODE_S) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 4") PORT_CODE(KEYCODE_4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad 7") PORT_CODE(KEYCODE_7) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Keypad *") PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Power") PORT_CODE(KEYCODE_SPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Charger connected") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::charger_irq), 0)
INPUT_PORTS_END

void noki3310_state::noki3310(machine_config &config)
{
	/* basic machine hardware */
	ARM7_BE(config, m_maincpu, 26000000 / 2);  // MAD2-family 13 MHz ARM clock; sleep uses the 32.768 kHz domain
	m_maincpu->set_addrmap(AS_PROGRAM, &noki3310_state::noki3310_map);

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

	INTEL_TE28F160(config, "flash");
	I2C_24C128(config, m_eeprom);
	NOKIA_MAD2(config, m_mad2);
	m_mad2->set_timer0_hz(33'055);
	m_mad2->set_timer1_hz(1'057);
	m_mad2->set_fiq8_hz(1'000);
	m_mad2->set_timer0_catchup(false);
	m_mad2->fiq_cb().set(FUNC(noki3310_state::mad2_fiq_w));
	m_mad2->irq_cb().set(FUNC(noki3310_state::mad2_irq_w));
	m_mad2->irq_ack_cb().set(FUNC(noki3310_state::mad2_irq_ack_w));
	m_mad2->reset_cb().set(FUNC(noki3310_state::mad2_reset_w));
	m_mad2->sleep_cb().set(FUNC(noki3310_state::mad2_sleep_w));
	NOKIA_MBUS(config, m_mbus);
	m_mbus->tx_cb().set(FUNC(noki3310_state::mbus_tx_w));
	m_mbus->fiq2_cb().set(FUNC(noki3310_state::mbus_fiq2_w));
	m_mbus->fiq3_cb().set(FUNC(noki3310_state::mbus_fiq3_w));
	NOKIA_CCONT(config, m_ccont);
	// The low status bit is persistent CCONT reset state, not an IRQ source.
	// Clearing it provides the explicit missing/unready-CCONT fault fixture.
	m_ccont->set_ready(true);
	m_ccont->irq_cb().set(FUNC(noki3310_state::ccont_irq_w));
	m_ccont->power_cb().set(FUNC(noki3310_state::ccont_power_w));
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
	NOKIA_RADIO_PEER(config, m_radio_peer);
	m_dspif->tx_commit_cb().set(FUNC(noki3310_state::dsp_tx_commit_w));
	m_dspif->service_pending_cb().set(FUNC(noki3310_state::dsp_service_pending_w));
	m_dspif->doorbell_cb().set(FUNC(noki3310_state::dsp_doorbell_w));
	m_dspif->shared_002_write_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_002_write_w));
	m_dspif->shared_0fe_read_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_0fe_read_w));
	m_dspif->shared_0fe_write_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_0fe_write_w));
	m_dspif->shared_100_read_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_100_read_w));
	m_dspif->shared_100_write_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::shared_100_write_w));
	m_dspif->fiq0_cb().set(FUNC(noki3310_state::dsp_fiq0_w));
	m_dspif->service_irq_cb().set(FUNC(noki3310_state::dsp_service_irq_w));
	NOKIA_SIMI(config, m_simi);
	NOKIA_SIM_CARD(config, m_sim_card);
	m_simi->irq_cb().set(FUNC(noki3310_state::sim_irq_w));
	m_sim_card->response_cb().set(m_simi, FUNC(nokia_simi_device::card_rx_w));
	apply_product_config(PRODUCT_3310);
}

void noki3310_state::noki3330(machine_config &config)
{
	noki3310(config);
	apply_product_config(PRODUCT_3330);

	INTEL_TE28F320(config.replace(), "flash");
}

void noki3310_state::noki3210(machine_config &config)
{
	noki3310(config);
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

void noki3310_state::noki5210(machine_config &config)
{
	noki3330(config);
	apply_product_config(PRODUCT_5X10);
}

void noki3310_state::noki8xxx(machine_config &config)
{
	noki3310(config);
	apply_product_config(PRODUCT_8XXX);
}

void noki3310_state::noki3410(machine_config &config)
{
	noki3330(config);
	apply_product_config(PRODUCT_3410);

	ST_M28W320ECT(config.replace(), "flash");
}

void noki3310_state::noki7110(machine_config &config)
{
	noki3330(config);

	subdevice<screen_device>("screen")->set_size(96, 65);    // Epson SED1565
}

void noki3310_state::noki6210(machine_config &config)
{
	noki3330(config);

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
SYST( 1999, noki3210, 0,      0,      noki3210, noki3210, noki3310_state, empty_init, "Nokia", "Nokia 3210", 0 )
SYST( 1999, noki7110, 0,      0,      noki7110, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 7110", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8210, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8850, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8850", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki3310, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3310", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6210, 0,      0,      noki6210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 6210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6250, 0,      0,      noki6210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 6250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8250, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8890, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8890", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2001, noki3330, 0,      0,      noki3330, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3330", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki3410, 0,      0,      noki3410, noki3410, noki3310_state, empty_init, "Nokia", "Nokia 3410", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki5210, 0,      0,      noki5210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 5210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
