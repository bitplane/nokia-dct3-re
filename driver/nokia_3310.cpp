// license:BSD-3-Clause
// copyright-holders:Sandro Ronco
/*
    Driver for Nokia phones based on Texas Instrument MAD2WD1 (ARM7TDMI + DSP)

    Driver based on documentations found here:
        http://nokix.sourceforge.net/help/blacksphere/sub_050main.htm
        http://tudor.rdslink.ro/MADos/

*/

// if anybody has solid information to aid in the emulation of this (or other phones) please contribute.

#include "emu.h"

#include <optional>
#include <unordered_map>

#include "cpu/arm7/arm7.h"
#include "machine/intelfsh.h"
#include "video/pcd8544.h"

#include "debugger.h"
#include "emupal.h"
#include "screen.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

#define LOG_MAD2_REGISTER_ACCESS    (1U << 1)
#define LOG_CCONT_REGISTER_ACCESS   (1U << 2)

#define VERBOSE (0)
#include "logmacro.h"

namespace {

static unsigned nokia_env_u32(const char *name, unsigned fallback);

constexpr offs_t NOKIA_RAM_BASE = 0x100000;
constexpr offs_t NOKIA_RAM_END = 0x180000;
constexpr offs_t NOKIA_FLASH1_BASE = 0x00200000;
constexpr offs_t NOKIA_FLASH2_BASE = 0x00600000;
constexpr offs_t NOKIA_FLASH_END = 0x00a00000;
constexpr uint32_t NOKIA_FLASH_ENTRY = 0x200040;

enum mad2_reg : uint8_t
{
	MAD2_MCU_RESET_CTRL = 0x01,
	MAD2_WATCHDOG = 0x03,
	MAD2_TIMER1_COUNTER_MSB = 0x04,
	MAD2_TIMER1_COUNTER_LSB = 0x05,
	MAD2_TIMER1_COMPARE_MSB = 0x06,
	MAD2_TIMER1_COMPARE_LSB = 0x07,
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

enum ccont_reg : uint8_t
{
	CCONT_ADC_CTRL = 0x0,
	CCONT_ADC_LSB = 0x2,
	CCONT_ADC_MSB = 0x3,
	CCONT_WATCHDOG = 0x5,
	CCONT_IRQ_STATUS = 0x0e,
	CCONT_IRQ_MASK = 0x0f
};

enum ccont_adc_channel : uint8_t
{
	CCONT_ADC_ACCESSORY = 0,
	CCONT_ADC_RSSI = 1,
	CCONT_ADC_BATTERY_VOLTAGE = 2,
	CCONT_ADC_BATTERY_TYPE = 3,
	CCONT_ADC_BATTERY_TEMP = 4,
	CCONT_ADC_CHARGER_VOLTAGE = 5,
	CCONT_ADC_VCXO_TEMP = 6,
	CCONT_ADC_CHARGING_CURRENT = 7
};

// MAD2 interrupt and MBUS control bits observed during the 3210 boot path.
constexpr uint16_t MAD2_LINE_EXTENDED = 0x100;
constexpr uint8_t MAD2_IRQ_CTRL_FIQ_ENABLE = 0x01;
constexpr uint8_t MAD2_IRQ_CTRL_IRQ_ENABLE = 0x04;
constexpr uint8_t MAD2_IRQ_CTRL_EXT_IRQ_MASK = 0x40;
constexpr uint8_t MAD2_FIQ8_MASKED = 0x04;
constexpr uint8_t MAD2_MBUS_BUSY_MASK = 0x60;
constexpr uint8_t MAD2_MBUS_DONE_FLAGS = 0xc0;
constexpr uint8_t MAD2_MBUS_TX_READY = 0x10;
constexpr uint8_t MAD2_MBUS_RX_READY = 0x20;
constexpr uint8_t MAD2_MBUS_TX_ENABLE = 0x20;
constexpr uint8_t MAD2_MBUS_RX_ENABLE = 0x40;
constexpr uint16_t MAD2_FIQ_TIMER0_COMPARE = 0x04;
constexpr uint16_t MAD2_FIQ_MBUS_MASK = 0x0c;

// CCONT serial command/status bits + fixed wiring (hardware constants, not configurable).
constexpr uint8_t CCONT_BOOT_IRQ_DEFAULT = 0x08;  // IRQ status the CCONT raises at boot (pulse 0)
constexpr uint8_t CCONT_IRQ_LINE_NUM = 6;         // MAD2 IRQ line the CCONT asserts
constexpr uint8_t CCONT_CMD_READ = 0x04;
constexpr uint8_t CCONT_CMD_ADDR_SHIFT = 3;

// Firmware RAM locations used only by focused diagnostics and scoped boot shims.
constexpr offs_t FW_CURRENT_TASK_ID = 0x100002;
constexpr offs_t FW_SCHED_STATE = 0x100020;
constexpr offs_t FW_SCHED_RUNNING_TASK_ID = 0x100022;
constexpr offs_t FW_SCHED_DELAY_HEAD = 0x10004c;
constexpr offs_t FW_SCHED_POST_STATE_BASE = 0x1093bc;
constexpr offs_t FW_SCHED_POST_TASK3_STATE = FW_SCHED_POST_STATE_BASE + (3 * 0x10);
constexpr offs_t FW_SCHED_POST_TASK3_WAIT_STATE = FW_SCHED_POST_TASK3_STATE + 0x0d;
constexpr offs_t FW_SCHED_TASK_TABLE = 0x2e2878;
constexpr offs_t FW_TASK_CONTEXT_BASE = 0x101484;
constexpr offs_t FW_TASK1_QUEUE_BASE = FW_TASK_CONTEXT_BASE + 0x1c;
constexpr offs_t FW_TASK1_QUEUE_PUT = 0x1014b0;
constexpr offs_t FW_TASK3_QUEUE_BASE = FW_TASK_CONTEXT_BASE + (3 * 0x1c);
constexpr offs_t FW_TASK3_QUEUE_PUT = FW_TASK3_QUEUE_BASE + 0x10;
constexpr offs_t FW_TASK5_QUEUE_BASE = FW_TASK_CONTEXT_BASE + (5 * 0x1c);
constexpr offs_t FW_TASK5_STATUS_STATE = 0x110f14;
constexpr offs_t FW_TASK5_STATUS_SEQUENCE = 0x110f28;
constexpr offs_t FW_TASK7_QUEUE_BASE = 0x100e68;
constexpr offs_t FW_TASK14_TCB = 0x1094a8;
constexpr offs_t FW_TASK14_QUEUE_SUSPECT = 0x1014f8;
constexpr offs_t FW_STARTUP_SERVICE_BUFFER = 0x110c2c;
constexpr offs_t FW_STARTUP_STATUS_WORD = 0x112448;
// Service-ready / DSP-handshake chain (the CONTACT SERVICE root cause; see
// docs/service_bootstrap.md). The startup service-ready byte (0x110c2c) is set =1 by
// the setter 0x291068 iff the DSP-shared pending counter (DSP RAM byte 0xe4) == 0; the
// setter only runs when MAD2 IRQ line 4 (the DSP service-completion interrupt) fires.
// The extended-task resume (incl. task 0x14, batch 2) also gates on the startup phase
// byte (FW_STARTUP_STATUS_WORD+1 = 0x112449) being in {0,2}.
constexpr offs_t FW_STARTUP_SERVICE_READY = FW_STARTUP_SERVICE_BUFFER;  // ready byte, gate input
constexpr offs_t FW_STARTUP_SERVICE_STATUS = 0x110c2e;                  // service-startup status word (-> 0x8002)
constexpr offs_t FW_STARTUP_PHASE = 0x112449;                          // startup phase byte (batch-2/task14 gate)
constexpr unsigned DSP_SVC_PENDING_COUNTER_OFF = 0x0e4;                // DSP-shared RAM byte: lower-service pending count
constexpr int MAD2_IRQ_LINE_DSP_SERVICE = 4;                          // IRQ line 4 = DSP service-completion interrupt
constexpr offs_t FW_POWER_STATE = 0x1100d0;
constexpr offs_t FW_BATTERY_RECORD = 0x110434;
constexpr offs_t FW_BATTERY_LEVEL_PERCENT = 0x110434;
constexpr offs_t FW_BATTERY_STATE = 0x110436;
constexpr offs_t FW_BATTERY_CLASSIFIER_FLAGS = 0x110438;
constexpr offs_t FW_BATTERY_CLASSIFIER_PREV_FLAGS = 0x110439;
constexpr offs_t FW_BATTERY_AVERAGED_ADC = 0x11043a;
constexpr offs_t FW_BATTERY_INIT_MODE = 0x11043d;
constexpr offs_t FW_BATTERY_ADC_PHASE = 0x11043e;
constexpr offs_t FW_BATTERY_FAST_VBAT_READS = 0x110464;
constexpr offs_t FW_BATTERY_CLASSIFIER_LOW_THRESHOLDS = 0x11048a;
constexpr offs_t FW_BATTERY_CLASSIFIER_HIGH_THRESHOLDS = 0x110494;
constexpr offs_t FW_BATTERY_SOURCE_STATE = 0x111458;
constexpr offs_t FW_BATTERY_SOURCE_SELECTOR_TABLE = 0x11145a;
constexpr offs_t FW_BATTERY_SOURCE_TABLE = 0x111488;
constexpr offs_t FW_BATTERY_SOURCE_WEIGHT_TABLE = 0x111d5c;
constexpr offs_t FW_BATTERY_SOURCE_REGION_END = 0x111d7f;
constexpr offs_t FW_BATTERY_HW_MODE_LATCH = 0x11fe52;
constexpr offs_t FW_STARTUP_WAIT_STATUS = 0x112398;
constexpr offs_t FW_POST74_KEYPAD_FALLBACK_FLAG = 0x11239d;
constexpr offs_t FW_STARTUP_MODE4_FLAG_RADIO = 0x112390;
constexpr offs_t FW_STARTUP_MODE4_FLAG_INITIAL = 0x112391;
constexpr offs_t FW_STARTUP_MODE4_FLAG_DISPLAY = 0x112392;
constexpr offs_t FW_STARTUP_MODE4_FLAG_SERVICE = 0x112393;
constexpr offs_t FW_STARTUP_MODE4_FLAG_BATTERY = 0x112394;
constexpr offs_t FW_STARTUP_MODE4_FLAG_UI = 0x112395;
constexpr offs_t FW_STARTUP_DISPATCH_STATE = 0x1123ec;
constexpr offs_t FW_STARTUP_EVENT = 0x1123ee;
constexpr offs_t FW_STARTUP_MODE = 0x1123f0;
constexpr offs_t FW_STARTUP_READY_TIMER_BASE = 0x1122c4;
constexpr offs_t FW_STARTUP_READY_TIMER_STATE = 0x1122c8;
constexpr offs_t FW_STARTUP_READY_GATE_FLAG = 0x100024;
constexpr offs_t FW_POST74_EVENT_GATE = 0x112368;
constexpr offs_t FW_POST74_EVENT_GATE_READY = FW_POST74_EVENT_GATE + 4;
constexpr offs_t FW_POST74_EVENT_GATE_FLAGS = FW_POST74_EVENT_GATE + 6;
constexpr offs_t FW_SCHED_ACTIVE_DELAY_HEAD = 0x100048;
constexpr offs_t FW_STARTUP_READY_DELAY_RECORD = 0x10026c;
constexpr offs_t FW_STARTUP_READY_SCHED_RECORD_A = 0x1126a0;
constexpr offs_t FW_STARTUP_READY_SCHED_RECORD_B = 0x1126ac;
constexpr offs_t FW_STARTUP_EVENT14_LATCH = 0x112424;
constexpr offs_t FW_CCONT_CHARGER_EVENT = 0x1124c8;
constexpr offs_t FW_CCONT_CHARGER_EVENT_VALUE = FW_CCONT_CHARGER_EVENT;
constexpr offs_t FW_CCONT_CHARGER_EVENT_POST_VALUE = 0x1124ca;
constexpr offs_t FW_CCONT_CHARGER_EVENT_LAST = 0x1124cc;
constexpr offs_t FW_CCONT_CHARGER_EVENT_RETRY = 0x1124cd;
constexpr offs_t FW_CCONT_STATE = 0x11ff6c;
constexpr offs_t FW_TASK14_READY_FLAG = 0x111c93;
constexpr offs_t FW_TASK14_HELPER_MODE_FLAG = 0x10d1c0;
constexpr offs_t FW_TASK14_HELPER_READY_FLAG = 0x10dcae;
constexpr offs_t FW_TASK14_FINAL_READY_FLAG = 0x10dcb0;
constexpr offs_t FW_TASK14_STATE_BLOCK = 0x111eb4;

// Contact-service state reached during the startup watchdog path. The firmware
// uses this block to accumulate test/status results before normal UI startup.
constexpr offs_t FW_CONTACT_SERVICE_STATE = 0x11fecc;
constexpr offs_t FW_CONTACT_SERVICE_STATUS = 0x11fed0;
constexpr offs_t FW_CONTACT_SERVICE_RESULT = 0x11fed4;
constexpr offs_t FW_CONTACT_SERVICE_COUNTER = 0x11fed6;
constexpr offs_t FW_CONTACT_SERVICE_SUBSTATE = 0x11feda;
constexpr offs_t FW_CONTACT_SERVICE_ACK = 0x11fedb;
constexpr offs_t FW_CONTACT_SERVICE_REASON = 0x11ff50;
constexpr offs_t FW_RESOURCE_CHECK_STATUS_TABLE = 0x11fc60;
constexpr offs_t FW_RESOURCE_CHECK_STATUS_INDEX = 0x11ff5a;

// Service-channel/lower-service state used by the current startup transport model.
constexpr offs_t FW_SERVICE_CHANNEL_READY_FLAGS = 0x111794;
constexpr offs_t FW_SERVICE_CHANNEL_ENABLE_FLAGS = 0x11fee4;
constexpr offs_t FW_SERVICE_CHANNEL_MASK_BASE = 0x11ff08;
constexpr uint8_t FW_SERVICE_CHANNEL_READY_BOOT_BIT = 0x08;

// Contact-service remote read (the deepest mapped layer; see docs/service_bootstrap.md).
// The contact-service reads its command from PM logical address 0x5f00 via an async MBUS/PM
// request message. The request's dest node ([msg+1]) is sourced from the channel-enable flag
// FW_SERVICE_CHANNEL_ENABLE_FLAGS (0x11fee4) — which is 0 on a blank phone, so the read is
// dropped (no request sent). The response, when one arrives, is dispatched by command at
// 0x236dc6; command 0x05 completes the contact-service healthily. Request frame format:
//   00 [node] 00 00 00 0a 00 01 [addr_hi] [addr_lo] [seq][seq] [ctr] [count] [data..]
constexpr uint16_t PM_LOGICAL_CONTACT_COMMAND = 0x5f00;         // PM addr read for the command
constexpr uint8_t  CONTACT_SVC_RESPONSE_CMD_HEALTHY = 0x05;     // response command that completes

// EEPROM checksummed-block layout. Cross-validated between NokTool 1.8 (the
// Delphi service tool: sub_0046AAA8 = 16-bit additive byte-sum, stored
// big-endian at each block's end; TForm1.e2prom1Click validates the tune and
// security blocks) and the 3210 firmware's own contact-service block check
// (checksum routine 0x234588, compare at 0x234810). The blocks tile exactly:
// each is data[start .. cksum-1] with a big-endian 16-bit sum at [cksum, cksum+1],
// and the next block starts at cksum+2. See docs/eeprom_analysis.md.
constexpr uint16_t FW_EEPROM_TUNE_BLOCK_START     = 0x0000;  // tune/calibration
constexpr uint16_t FW_EEPROM_TUNE_BLOCK_CKSUM     = 0x003e;  // BE sum16 of [0x0000..0x003d]
constexpr uint16_t FW_EEPROM_SECURITY_BLOCK_START = 0x0040;  // security/IMEI/locks
constexpr uint16_t FW_EEPROM_SECURITY_BLOCK_CKSUM = 0x011e;  // BE sum16 of [0x0040..0x011d]
constexpr uint16_t FW_EEPROM_CONFIG_BLOCK_START   = 0x0120;  // contact-service config
constexpr uint16_t FW_EEPROM_CONFIG_BLOCK_CKSUM   = 0x0244;  // BE sum16(-corr) of [0x0120..0x0243]

// Startup modes named from the traced charger/battery progression.
constexpr uint16_t FW_STARTUP_MODE_CHARGER_WAIT = 0x000d;
constexpr uint16_t FW_STARTUP_MODE_POST_CHARGER = 0x000b;
constexpr uint16_t FW_STARTUP_MODE_POST_CHARGER_DONE = 0x000c;
constexpr uint16_t FW_STARTUP_MODE_BATTERY_WAIT = 0x0009;
constexpr uint16_t FW_STARTUP_MODE_POST_SELFTEST = 0x0004;
constexpr uint16_t FW_STARTUP_MODE_READY_GATE = 0x0005;
constexpr uint16_t FW_STARTUP_MODE_SERVICE_QUIESCE_GATE = 0x0006;
constexpr uint16_t FW_STARTUP_MODE_BATTERY_READY_GATE = 0x0007;

constexpr uint16_t FW_STARTUP_EVENT_CHARGER_PRESENT = 0x000e;
constexpr uint16_t FW_STARTUP_EVENT_BATTERY_PRESENT = 0x0003;
constexpr uint16_t FW_STARTUP_EVENT_BATTERY_READY = 0x0007;
constexpr uint16_t FW_STARTUP_EVENT_CCONT_BATTERY_COMPLETE = 0x0015;
constexpr uint16_t FW_STARTUP_EVENT_PHASE5_CONTINUE = 0x0003;

class noki3310_state : public driver_device
{
public:
	noki3310_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_flash(*this, "flash"),
		m_pcd8544(*this, "pcd8544"),
		m_keypad(*this, "COL.%u", 0),
		m_pwr(*this, "PWR")
	{ }

	void noki3330(machine_config &config);
	void noki3410(machine_config &config);
	void noki7110(machine_config &config);
	void noki6210(machine_config &config);
	void noki3310(machine_config &config);

	DECLARE_INPUT_CHANGED_MEMBER(key_irq);

private:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

	PCD8544_SCREEN_UPDATE(pcd8544_screen_update);

	uint8_t mad2_io_r(offs_t offset);
	void mad2_io_w(offs_t offset, uint8_t data);
	uint8_t mad2_dspif_r(offs_t offset);
	void mad2_dspif_w(offs_t offset, uint8_t data);
	uint8_t mad2_mcuif_r(offs_t offset);
	void mad2_mcuif_w(offs_t offset, uint8_t data);

	TIMER_CALLBACK_MEMBER(timer0);
	TIMER_CALLBACK_MEMBER(timer1);
	TIMER_CALLBACK_MEMBER(timer_watchdog);
	TIMER_CALLBACK_MEMBER(timer_fiq8);
	TIMER_CALLBACK_MEMBER(timer_mbus);
	TIMER_CALLBACK_MEMBER(timer_power_irq);
	TIMER_CALLBACK_MEMBER(timer_keypad);
	TIMER_CALLBACK_MEMBER(timer_mad2_soft_reset);
	TIMER_CALLBACK_MEMBER(timer_dsp_service);

	uint16_t ram_r(offs_t offset, uint16_t mem_mask = ~0);
	uint16_t ram_r_firmware_overrides(offs_t offset, uint16_t mem_mask);
	void ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	void ram_w_firmware_overrides(offs_t offset, uint16_t data, uint16_t mem_mask);
	uint16_t eeprom_r(offs_t offset, uint16_t mem_mask = ~0);
	void eeprom_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t dsp_ram_r(offs_t offset);
	void dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t flash_r(offs_t offset, uint16_t mem_mask = ~0);
	std::optional<uint16_t> flash_firmware_hooks(offs_t offset, u32 pc, u32 addr, uint16_t mem_mask);
	void flash_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint32_t rom2_mirror_r(offs_t offset, uint32_t mem_mask = ~0);
	void rom2_mirror_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	void noki3310_map(address_map &map) ATTR_COLD;

	void assert_fiq(int num);
	void assert_irq(int num);
	void ack_fiq(uint16_t mask);
	void ack_irq(uint16_t mask);
	void update_fiq_line();
	void update_irq_line();
	void ccont_update_irq_line();
	void ccont_set_irq_status(uint8_t status, const char *reason);
	uint8_t ccont_boot_status(unsigned pulse) const;
	bool timer0_compare_due() const;
	void update_timer0_compare();
	void schedule_mbus_fiq(int num);
	void signal_mbus_fiq(int num);
	void complete_mbus_transfer();
	uint8_t keypad_irq_state() const;
	uint8_t synthetic_keypad_state() const;
	bool synthetic_key_active(uint8_t &row, uint8_t &mask) const;
	uint16_t fw_word(offs_t address) const;
	uint8_t fw_byte(offs_t address) const;
	uint32_t fw_dword(offs_t address) const;
	void fw_word_w(offs_t address, uint16_t data);
	void fw_byte_w(offs_t address, uint8_t data);
	uint16_t debug_ram_word(offs_t address) const { return fw_word(address); }
	uint8_t debug_ram_byte(offs_t address) const { return fw_byte(address); }
	void debug_ram_word_w(offs_t address, uint16_t data) { fw_word_w(address, data); }
	void debug_ram_byte_w(offs_t address, uint8_t data) { fw_byte_w(address, data); }
	void trace_state31_event_source(uint32_t pc, uint32_t addr, offs_t offset);
	void nokia_ccont_w(uint8_t data);
	uint8_t nokia_ccont_r();
	void serial_eeprom_start();
	void serial_eeprom_write_bit(uint8_t bit);
	void serial_eeprom_accept_byte(uint8_t data);
	void serial_eeprom_clock_read_bit();
	uint8_t serial_eeprom_byte(uint16_t address) const;

	required_device<cpu_device> m_maincpu;
	required_device<intelfsh16_device> m_flash;
	required_device<pcd8544_device> m_pcd8544;
	required_ioport_array<5> m_keypad;
	required_ioport m_pwr;

	std::unique_ptr<uint16_t[]>   m_ram;
	std::unique_ptr<uint16_t[]>   m_dsp_ram;

	uint8_t       m_power_on;
	uint16_t      m_fiq_status;
	uint16_t      m_irq_status;
	uint16_t      m_timer1_counter;
	uint16_t      m_timer0_counter;
	uint8_t       m_timer0_divider;
	bool          m_timer0_compare_latched;
	uint8_t       m_keypad_irq_state;
	bool          m_startup_event15_posted;
	bool          m_startup_latch_complete_seen;
	bool          m_after_mad2_soft_reset;

	// Node-0x18 service-responder trampoline state (NOKI3210_MODEL_SVC_RESPONDER).
	unsigned      m_svcresp_state;      // 0 idle, 1 await-alloc, 2 await-post, 3 done
	uint32_t      m_svcresp_saved[16];  // R0..R14 + CPSR saved at the trigger point
	uint32_t      m_svcresp_msg;        // allocated message pointer
	uint8_t       m_battery_startup_event_step;
	uint8_t       m_battery_startup_event_step_mode9;
	uint8_t       m_mode4_startup_completion_step;
	uint8_t       m_post_charger_completion_step;
	bool          m_post_charger_sequence_entered;
	uint8_t       m_mode5_startup_event_step;
	bool          m_mode5_ccont_event_sent;
	bool          m_mode_d_startup_complete_forced;
	bool          m_mode_d_late_startup_complete_forced;
	uint32_t      m_power_irq_count;
	attotime      m_startup_latch_complete_time;

	emu_timer * m_timer0;
	emu_timer * m_timer1;
	emu_timer * m_timer_watchdog;
	emu_timer * m_timer_fiq8;
	emu_timer * m_timer_mbus;
	emu_timer * m_timer_power_irq;
	emu_timer * m_timer_keypad;
	emu_timer * m_timer_mad2_soft_reset;
	emu_timer * m_timer_dsp_service;

	// CCONT
	struct nokia_ccont
	{
		bool    dc;
		uint8_t   cmd;
		uint8_t   watchdog;
		uint8_t   regs[0x10];
		uint8_t   adc_request;
		uint8_t   adc_channel;
		uint16_t  adc_value;
		uint32_t  adc_log_count;
		// ADC source values (10-bit) the chip "measures", indexed by ccont_adc_channel.
		// Populated from the power scenario at reset; the measurement path samples these.
		// This is the ccont_device's "what the chip senses" model (replacing per-read knobs).
		uint16_t  adc_src[8];
		uint8_t   irq_line;
		uint8_t   boot_status;
		bool      irq_asserted;
	} m_ccont;

	struct nokia_serial_eeprom
	{
		uint8_t write_shift;
		uint8_t write_bits;
		uint16_t address;
		uint16_t address_temp;
		uint8_t address_stage;
		uint8_t read_byte;
		uint8_t read_bits;
		uint8_t read_latched_bit;
		bool read_mode;
	} m_serial_eeprom;

	uint8_t       m_mad2_regs[0x100];
};

static const char * nokia_mad2_reg_desc(uint8_t offset)
{
	switch(offset)
	{
	case 0x00:  return "[CTSI] DCT3 ASIC version Primary hardware version (r)";
	case 0x01:  return "[CTSI] MCU reset control register (rw)";
	case 0x02:  return "[CTSI] DSP reset control register (rw)";
	case 0x03:  return "[CTSI] ASIC watchdog write register (w)";
	case 0x04:  return "[CTSI] Sleep clock counter (MSB) (r)";
	case 0x05:  return "[CTSI] Sleep clock counter (LSB) (r)";
	case 0x06:  return "[CTSI] ? (sleep) clock destination (LSB) (r)";
	case 0x07:  return "[CTSI] ? (sleep) clock destination (MSB) (r)";
	case 0x08:  return "[CTSI] FIQ lines active (rw)";
	case 0x09:  return "[CTSI] IRQ lines active (rw)";
	case 0x0A:  return "[CTSI] FIQ lines mask (rw)";
	case 0x0B:  return "[CTSI] IRQ lines mask (rw)";
	case 0x0C:  return "[CTSI] Interrupt control register (rw)";
	case 0x0D:  return "[CTSI] Clock control register (rw)";
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
	case 0x1C:  return "[PUP] Buzzer clock divider (w)";
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

static const char * nokia_ccont_reg_desc(uint8_t offset)
{
	switch(offset)
	{
	case 0x0:   return "Control register (w)";
	case 0x1:   return "PWM (charger) (w)";
	case 0x2:   return "A/D read (LSB) (r)";
	case 0x3:   return "A/D read (MSB) (rw)";
	case 0x4:   return "?";
	case 0x5:   return "Watchdog (WDReg) (w)";
	case 0x6:   return "RTC enabled (w)";
	case 0x7:   return "RTC second (rw)";
	case 0x8:   return "RTC minute (r)";
	case 0x9:   return "RTC hour (r)";
	case 0xA:   return "RTC day (rw)";
	case 0xB:   return "RTC alarm minute (rw)";
	case 0xC:   return "RTC alarm hour (rw)";
	case 0xD:   return "RTC calibration value (rw)";
	case 0xE:   return "Interrupt lines (rw)";
	case 0xF:   return "Interrupt mask (rw)";
	default:    return "<Unknown>";
	}
}

static uint16_t nokia_adc_override(unsigned id, uint16_t fallback)
{
	char name[] = "NOKI3210_ADC0";
	name[12] = '0' + (id & 0x07);

	if (const char *value = std::getenv(name))
	{
		char *end = nullptr;
		const unsigned long parsed = std::strtoul(value, &end, 0);
		if (end != value)
			return parsed & 0x03ff;
	}

	if (const char *profile = std::getenv("NOKI3210_ADC_PROFILE"))
	{
		if (!std::strcmp(profile, "sane") || !std::strcmp(profile, "charged"))
		{
			switch(id & 0x07)
			{
				case 0: return 0x000; // Accessory Detect: none
				case 1: return 0x200; // RSSI: mid-scale
				case 2: return 0x2d0; // Battery voltage: plausible charged pack
				case 3: return 0x280; // Battery type
				case 4: return 0x200; // Battery temperature
				case 5: return std::strcmp(profile, "charged") ? 0x000 : 0x200; // Charger voltage
				case 6: return 0x200; // VCXO temperature
				case 7: return std::strcmp(profile, "charged") ? 0x000 : 0x120; // Charging current
			}
		}
	}

	return fallback;
}

static unsigned nokia_env_u32(const char *name, unsigned fallback)
{
	// Env vars don't change during a run, so memoise per name: flash_firmware_hooks
	// fires this ~50x per instruction fetch, and an uncached getenv() there is the
	// dominant cost of the whole emulation. Keyed by the literal pointer (every call
	// site passes a string literal). See docs/driver_vision.md (hot-path config smell).
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
	m_ram = std::make_unique<uint16_t[]>((NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1);
	m_dsp_ram = std::make_unique<uint16_t[]>(0x800);      // DSP shared RAM

	// allocate timers
	m_timer0 = timer_alloc(FUNC(noki3310_state::timer0), this);
	m_timer1 = timer_alloc(FUNC(noki3310_state::timer1), this);
	m_timer_watchdog = timer_alloc(FUNC(noki3310_state::timer_watchdog), this);
	m_timer_fiq8 = timer_alloc(FUNC(noki3310_state::timer_fiq8), this);
	m_timer_mbus = timer_alloc(FUNC(noki3310_state::timer_mbus), this);
	m_timer_power_irq = timer_alloc(FUNC(noki3310_state::timer_power_irq), this);
	m_timer_keypad = timer_alloc(FUNC(noki3310_state::timer_keypad), this);
	m_timer_mad2_soft_reset = timer_alloc(FUNC(noki3310_state::timer_mad2_soft_reset), this);
	m_timer_dsp_service = timer_alloc(FUNC(noki3310_state::timer_dsp_service), this);
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

void noki3310_state::fw_word_w(offs_t address, uint16_t data)
{
	if (address < NOKIA_RAM_BASE || address >= NOKIA_RAM_END)
		return;

	m_ram[(address - NOKIA_RAM_BASE) >> 1] = data;
}

void noki3310_state::fw_byte_w(offs_t address, uint8_t data)
{
	if (address < NOKIA_RAM_BASE || address >= NOKIA_RAM_END)
		return;

	uint16_t &word = m_ram[(address - NOKIA_RAM_BASE) >> 1];
	if (BIT(address, 0))
		word = (word & 0xff00) | data;
	else
		word = (word & 0x00ff) | (uint16_t(data) << 8);
}

void noki3310_state::trace_state31_event_source(uint32_t pc, uint32_t addr, offs_t offset)
{
	if (nokia_env_u32("NOKI3210_TRACE_SERVICE_TRANSPORT", 0) == 0 ||
			pc != addr ||
			!(addr == 0x002a6f1c || addr == 0x002a6f20 || addr == 0x002a6f82 ||
			  addr == 0x002a6fb2 || addr == 0x002a6fd0 || addr == 0x002a6ff8 ||
			  addr == 0x002a6ffc || addr == 0x002a7000 || addr == 0x002a7006 || addr == 0x002a701e ||
			  addr == 0x002a7048 || addr == 0x002a70ac || addr == 0x002a710e ||
			  addr == 0x002a7124))
		return;

	static unsigned state31_event_source_count = 0;
	if (state31_event_source_count++ >= 320)
		return;

}

void noki3310_state::machine_reset()
{
	std::fill_n(m_ram.get(), (NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1, 0);
	std::fill_n(m_dsp_ram.get(), 0x800, 0);

	// according to the boot rom disassembly here http://www.nokix.pasjagsm.pl/help/blacksphere/sub_100hardware/sub_arm/sub_bootrom.htm
	// flash entry point is at 0x200040, we can probably reassemble the above code, but for now this should be enough.
	m_maincpu->set_state_int(arm7_cpu_device::ARM7_R15, NOKIA_FLASH_ENTRY);

	memset(m_mad2_regs, 0, 0x100);
	m_mad2_regs[MAD2_MCU_RESET_CTRL] = 0x01;   // power-on flag
	m_mad2_regs[MAD2_IRQ_CTRL] = 0x0a;         // disable FIQ and IRQ
	m_mad2_regs[MAD2_WATCHDOG] = 0xff;         // disable MAD2 watchdog
	for (uint8_t &reg : m_ccont.regs)
		reg = 0;
	m_ccont.watchdog  = 0;      // disable CCONT watchdog
	m_ccont.dc  = false;
	m_ccont.adc_request = 0;
	m_ccont.adc_channel = 0;
	m_ccont.adc_value = 0;
	m_ccont.adc_log_count = 0;
	// Load the ADC source model from the power scenario. Per-channel defaults are the
	// chip's "battery present, no charger" rest state; nokia_adc_override applies the
	// NOKI3210_ADC_PROFILE / ADCn knobs on top, so values are identical to before (the
	// override is constant for a run). The scenario will become a typed object later.
	{
		static const uint16_t adc_default[8] =
				{ 0x000, 0x3ff, 0x3ff, 0x280, 0x200, 0x000, 0x200, 0x000 };
		for (unsigned id = 0; id < 8; id++)
			m_ccont.adc_src[id] = nokia_adc_override(id, adc_default[id]);
	}
	m_ccont.irq_line = CCONT_IRQ_LINE_NUM;            // fixed hardware wiring (was CCONT_IRQ_LINE knob)
	m_ccont.boot_status = CCONT_BOOT_IRQ_DEFAULT;     // fixed boot IRQ status (was CCONT_BOOT_STATUS knob)
	m_ccont.irq_asserted = false;
	m_serial_eeprom = {};

	m_fiq_status = 0;
	m_irq_status = 0;
	m_timer1_counter = 0;
	m_timer0_counter = 0;
	m_timer0_divider = 255;
	m_timer0_compare_latched = false;
	m_keypad_irq_state = 0xff;
	m_startup_event15_posted = false;
	m_startup_latch_complete_seen = false;
	m_after_mad2_soft_reset = false;
	m_svcresp_state = 0;
	m_svcresp_msg = 0;
	m_battery_startup_event_step = 0;
	m_battery_startup_event_step_mode9 = 0;
	m_mode4_startup_completion_step = 0;
	m_post_charger_completion_step = 0;
	m_post_charger_sequence_entered = false;
	m_mode5_startup_event_step = 0;
	m_mode5_ccont_event_sent = false;
	m_mode_d_startup_complete_forced = false;
	m_mode_d_late_startup_complete_forced = false;
	m_power_irq_count = 0;
	m_startup_latch_complete_time = attotime::never;

	const unsigned timer0_hz = nokia_env_u32("NOKI3210_TIMER0_HZ", 33055);
	const unsigned timer1_hz = nokia_env_u32("NOKI3210_TIMER1_HZ", 1057);
	const unsigned fiq8_hz = nokia_env_u32("NOKI3210_FIQ8_HZ", 1000);

	m_timer0->adjust(attotime::from_hz(timer0_hz), 0, attotime::from_hz(timer0_hz));    // programmable divider through port 0x0f
	m_timer1->adjust(attotime::from_hz(timer1_hz), 0, attotime::from_hz(timer1_hz));
	m_timer_watchdog->adjust(attotime::from_hz(1), 0, attotime::from_hz(1));
	m_timer_fiq8->adjust(attotime::from_hz(fiq8_hz), 0, attotime::from_hz(fiq8_hz));
	m_timer_mbus->adjust(attotime::never);
	m_timer_dsp_service->adjust(attotime::never);
	m_timer_power_irq->adjust(attotime::from_msec(nokia_env_u32("NOKI3210_POWER_IRQ_MS", 1000)));
	m_timer_keypad->adjust(attotime::from_hz(200), 0, attotime::from_hz(200));

	// simulate power-on input
	if (machine().system().name[4] == '8' || machine().system().name[4] == '5')
		m_power_on = ~0x10;
	else if (!std::strcmp(machine().system().name, "noki3210"))
		m_power_on = ~0x01;
	else
		m_power_on = ~0x04;
}

void noki3310_state::assert_fiq(int num)
{
	if (num < 8)
		m_fiq_status |= 1 << num;
	else
		m_fiq_status |= MAD2_LINE_EXTENDED;

	update_fiq_line();
}

void noki3310_state::update_fiq_line()
{
	bool active = false;

	if (m_mad2_regs[MAD2_IRQ_CTRL] & MAD2_IRQ_CTRL_FIQ_ENABLE)
	{
		active = (m_fiq_status & ~m_mad2_regs[MAD2_FIQ_MASK] & 0xff) != 0;

		if ((m_fiq_status & MAD2_LINE_EXTENDED) && !(m_mad2_regs[MAD2_FIQ8_CTRL] & MAD2_FIQ8_MASKED))
			active = true;
	}

	m_maincpu->set_input_line(1, active ? ASSERT_LINE : CLEAR_LINE);
}

void noki3310_state::assert_irq(int num)
{
	if (num < 8)
		m_irq_status |= 1 << num;
	else
		m_irq_status |= MAD2_LINE_EXTENDED;

	update_irq_line();
}

void noki3310_state::update_irq_line()
{
	bool active = false;

	if (m_mad2_regs[MAD2_IRQ_CTRL] & MAD2_IRQ_CTRL_IRQ_ENABLE)
	{
		active = (m_irq_status & ~m_mad2_regs[MAD2_IRQ_MASK] & 0xff) != 0;

		if ((m_irq_status & MAD2_LINE_EXTENDED) && !(m_mad2_regs[MAD2_IRQ_CTRL] & MAD2_IRQ_CTRL_EXT_IRQ_MASK))
			active = true;
	}

	m_maincpu->set_input_line(0, active ? ASSERT_LINE : CLEAR_LINE);
}

void noki3310_state::ccont_update_irq_line()
{
	const uint16_t irq_mask = (m_ccont.irq_line < 8) ? (uint16_t(1) << m_ccont.irq_line) : MAD2_LINE_EXTENDED;
	const bool active = (m_ccont.regs[CCONT_IRQ_STATUS] & ~m_ccont.regs[CCONT_IRQ_MASK]) != 0;

	if (active)
	{
		if (!m_ccont.irq_asserted)
		{
			m_irq_status |= irq_mask;
			m_ccont.irq_asserted = true;
		}
	}
	else if (m_ccont.irq_asserted)
	{
		m_irq_status &= ~irq_mask;
		m_ccont.irq_asserted = false;
	}

	update_irq_line();
}

void noki3310_state::ccont_set_irq_status(uint8_t status, const char *reason)
{
	if (status == 0)
		return;

	m_ccont.regs[CCONT_IRQ_STATUS] |= status;
	ccont_update_irq_line();
}

uint8_t noki3310_state::ccont_boot_status(unsigned pulse) const
{
	// The CCONT raises its boot IRQ (status 0x08) once, on the first pulse.
	return (pulse == 0) ? m_ccont.boot_status : 0;
}

uint8_t noki3310_state::keypad_irq_state() const
{
	uint8_t data = 0xff;

	for (int i = 0; i < 5; i++)
		data &= m_keypad[i]->read() | 0xe0;

	data &= m_pwr->read() | 0xe0;
	if (nokia_env_u32("NOKI3210_HOLD_POWER_KEY", 0) != 0)
		data &= 0xfe;
	data &= synthetic_keypad_state();
	return data;
}

bool noki3310_state::synthetic_key_active(uint8_t &row, uint8_t &mask) const
{
	const char *key = std::getenv("NOKI3210_POST_READY_KEY");
	if (key == nullptr || key[0] == '\0' || !m_startup_latch_complete_seen)
		return false;

	const unsigned delay_ms = nokia_env_u32("NOKI3210_POST_READY_KEY_DELAY_MS", 250);
	const unsigned duration_ms = nokia_env_u32("NOKI3210_POST_READY_KEY_DURATION_MS", 750);
	const unsigned period_ms = nokia_env_u32("NOKI3210_POST_READY_KEY_PERIOD_MS", 0);
	const attotime start = m_startup_latch_complete_time + attotime::from_msec(delay_ms);
	const attotime now = machine().time();
	if (now < start)
		return false;

	if (period_ms == 0)
	{
		const attotime end = start + attotime::from_msec(duration_ms);
		if (now >= end)
			return false;
	}
	else
	{
		const unsigned elapsed_ms = (now - start).as_double() * 1000.0;
		if ((elapsed_ms % period_ms) >= duration_ms)
			return false;
	}

	if (!std::strcmp(key, "enter"))
	{
		row = 4;
		mask = 0x08;
		return true;
	}
	if (!std::strcmp(key, "up"))
	{
		row = 0;
		mask = 0x02;
		return true;
	}
	if (!std::strcmp(key, "down"))
	{
		row = 1;
		mask = 0x02;
		return true;
	}
	if (!std::strcmp(key, "0"))
	{
		row = 0;
		mask = 0x04;
		return true;
	}
	if (!std::strcmp(key, "1"))
	{
		row = 1;
		mask = 0x10;
		return true;
	}
	if (!std::strcmp(key, "2"))
	{
		row = 1;
		mask = 0x08;
		return true;
	}
	if (!std::strcmp(key, "3"))
	{
		row = 4;
		mask = 0x02;
		return true;
	}
	if (!std::strcmp(key, "4"))
	{
		row = 2;
		mask = 0x10;
		return true;
	}
	if (!std::strcmp(key, "5"))
	{
		row = 2;
		mask = 0x08;
		return true;
	}
	if (!std::strcmp(key, "6"))
	{
		row = 2;
		mask = 0x04;
		return true;
	}
	if (!std::strcmp(key, "7"))
	{
		row = 3;
		mask = 0x10;
		return true;
	}
	if (!std::strcmp(key, "8"))
	{
		row = 3;
		mask = 0x08;
		return true;
	}
	if (!std::strcmp(key, "9"))
	{
		row = 3;
		mask = 0x04;
		return true;
	}
	if (!std::strcmp(key, "del") || !std::strcmp(key, "c"))
	{
		row = 0;
		mask = 0x10;
		return true;
	}
	if (!std::strcmp(key, "minus"))
	{
		row = 4;
		mask = 0x04;
		return true;
	}
	if (!std::strcmp(key, "star"))
	{
		row = 4;
		mask = 0x10;
		return true;
	}
	if (!std::strcmp(key, "power"))
	{
		row = 0xff;
		mask = 0x01;
		return true;
	}

	return false;
}

uint8_t noki3310_state::synthetic_keypad_state() const
{
	uint8_t row = 0xff;
	uint8_t mask = 0xff;
	if (!synthetic_key_active(row, mask))
		return 0xff;
	return uint8_t(~mask) | 0xe0;
}

void noki3310_state::signal_mbus_fiq(int num)
{
	if (num == 2 && (m_mad2_regs[MAD2_MBUS_CTRL] & MAD2_MBUS_TX_ENABLE))
		m_mad2_regs[MAD2_MBUS_STATUS] &= ~0x07;
	m_mad2_regs[MAD2_MBUS_STATUS] |= MAD2_MBUS_DONE_FLAGS;
	if (num == 2 && (m_mad2_regs[MAD2_MBUS_CTRL] & MAD2_MBUS_TX_ENABLE))
		m_mad2_regs[MAD2_MBUS_STATUS] |= MAD2_MBUS_TX_READY;

	if (num == 2 && !(m_mad2_regs[MAD2_MBUS_CTRL] & MAD2_MBUS_BUSY_MASK))
	{
		complete_mbus_transfer();
		return;
	}

	assert_fiq(num);
}

void noki3310_state::schedule_mbus_fiq(int num)
{
	m_timer_mbus->adjust(attotime::from_msec(5), num);
}

void noki3310_state::complete_mbus_transfer()
{
	m_mad2_regs[MAD2_MBUS_CTRL] &= ~MAD2_MBUS_BUSY_MASK;
	m_mad2_regs[MAD2_FIQ_MASK] |= 0x08;
	ack_fiq(MAD2_FIQ_MBUS_MASK);
}

void noki3310_state::ack_fiq(uint16_t mask)
{
	m_fiq_status &= ~mask;
	update_fiq_line();
}

void noki3310_state::ack_irq(uint16_t mask)
{
	m_irq_status &= ~mask;
	update_irq_line();
}

void noki3310_state::nokia_ccont_w(uint8_t data)
{
	if (m_ccont.dc == false)
	{
		LOGMASKED(LOG_CCONT_REGISTER_ACCESS, "CCONT command %s %x\n", data & CCONT_CMD_READ ? "R" : "W", data >> CCONT_CMD_ADDR_SHIFT);
		m_ccont.cmd  = data;
	}
	else
	{
		uint8_t addr = (m_ccont.cmd >> CCONT_CMD_ADDR_SHIFT) & 0x0f;

		switch(addr)
		{
			case CCONT_ADC_CTRL:
			{
					uint16_t ad_id = (data >> 4) & 0x07;
				// Sample the ADC source model (the conversion result). Today this is
				// instantaneous; the measurement state machine (next increment) will move
				// the result + completion IRQ onto a timer.
				uint16_t ad_value = m_ccont.adc_src[ad_id & 0x07];

				m_ccont.regs[addr] = data;
				m_ccont.regs[CCONT_ADC_LSB] = ad_value & 0xff;
				m_ccont.regs[CCONT_ADC_MSB] = ((ad_value >> 8) & 0x03);
				m_ccont.adc_request = data;
				m_ccont.adc_channel = ad_id;
				m_ccont.adc_value = ad_value;
				m_ccont.adc_log_count++;
				break;
			}
			case CCONT_WATCHDOG:
				if (data == 0x20)
					m_ccont.regs[addr] = data;
				else if (data == 0x31)
					m_ccont.watchdog = m_ccont.regs[addr];
				else if (data == 0x3f)
					m_ccont.watchdog = 0;
				else if (data == 0)
					printf("CCONT power-off\n");
				break;

			case CCONT_IRQ_STATUS:
			{
				m_ccont.regs[addr] &= ~data;
				ccont_update_irq_line();
				break;
			}

			default:
				m_ccont.regs[addr] = data;
				if (addr == CCONT_IRQ_MASK)
					ccont_update_irq_line();
				break;
		}

		LOGMASKED(LOG_CCONT_REGISTER_ACCESS, "CCONT W %02x = %02x %s\n", addr, data, nokia_ccont_reg_desc(addr));
	}

	m_ccont.dc = !m_ccont.dc;
}

uint8_t noki3310_state::nokia_ccont_r()
{
	uint8_t addr = (m_ccont.cmd >> CCONT_CMD_ADDR_SHIFT) & 0x0f;
	uint8_t data = m_ccont.regs[addr];

	// CCONT register-1 read probe (opt-in): the idx6 service-channel check tests a cached
	// CCONT value at index 1 & 0x90. Confirm whether the firmware actually serial-reads
	// hardware register 1, and what the emulation returns (0 currently — write-only PWM reg).
	if (nokia_env_u32("NOKI3210_TRACE_CCONT_READ", 0) != 0)
	{
		static unsigned cr_log = 0;
		if (cr_log++ < 4000)
			logerror("ccont_r reg=%x returns=%02x t=%.4f\n", addr, data, machine().time().as_double());
	}

	// MODEL (opt-in): CCONT register 0xe (the interrupt register) bit 0 is a persistent
	// present/status bit, NOT a serviced interrupt — the firmware's own CCONT IRQ dispatcher
	// (0x2b08c6) masks bits 0..2 off (`and #0xf8`) before handling. The service-channel scan
	// reads it *live* as "is the CCONT service present?" (idx6, via ccont_reg_read(0x9001) =>
	// CCONT cmd 0x74 => reg 0xe & 0x01). A functional CCONT reports it set on any phone (blank
	// or provisioned); the emulation otherwise never sets it, so idx6 wrongly reads the CCONT
	// as absent. Report it set (read-time only, so it does not perturb the IRQ-line latch).
	// See docs/service_bootstrap.md. (Open: confirm bit-0 semantics vs a CCONT register map.)
	if (addr == CCONT_IRQ_STATUS && nokia_env_u32("NOKI3210_MODEL_CCONT_PRESENT", 0) != 0)
		data |= 0x01;

	system_time systime;
	machine().current_datetime(systime);

	switch(addr)
	{
		case CCONT_ADC_MSB: data = 0xb0 | (m_ccont.regs[addr] & 0x03);  break;
		case 0x7:       data = systime.local_time.second;           break;
		case 0x8:       data = systime.local_time.minute;           break;
		case 0x9:       data = systime.local_time.hour;             break;
		case 0xa:       data = systime.local_time.mday;             break;
	}

	m_ccont.dc = !m_ccont.dc;

	LOGMASKED(LOG_CCONT_REGISTER_ACCESS, "CCONT R %02x = %02x %s\n", addr, data, nokia_ccont_reg_desc(addr));
	return data;
}

PCD8544_SCREEN_UPDATE(noki3310_state::pcd8544_screen_update)
{
	for (int r = 0; r < 6; r++)
		for (int x = 0; x < 84; x++)
		{
			uint8_t gfx = vram[r*84 + x];

			for (int y = 0; y < 8; y++)
			{
				int p = BIT(gfx, y);
				bitmap.pix(r*8 + y, x) = p ^ inv;
			}
		}
}

bool noki3310_state::timer0_compare_due() const
{
	const uint16_t compare = (uint16_t(m_mad2_regs[0x12]) << 8) | m_mad2_regs[0x13];
	if (compare == 0)
		return false;

	if (nokia_env_u32("NOKI3210_TIMER0_CATCHUP", 0) == 0)
		return m_timer0_counter == compare;

	return int16_t(m_timer0_counter - compare) >= 0;
}

void noki3310_state::update_timer0_compare()
{
	if (m_timer0_compare_latched || !timer0_compare_due())
		return;

	m_timer0_compare_latched = true;

	if (!(m_fiq_status & 0x04))
		assert_fiq(4);
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer0)
{
	if (m_mad2_regs[0x0f] != 0)
	{
		m_mad2_regs[0x0f]--;
		return;
	}

	m_mad2_regs[0x0f] = m_timer0_divider;

	m_timer0_counter++;
	update_timer0_compare();
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer1)
{
	m_timer1_counter++;

	if (m_timer1_counter == 0x8000)
	{
		assert_fiq(5);
		m_timer1_counter = 0;
	}
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_fiq8)
{
	if (m_mad2_regs[0x16] & 0x01)
		assert_fiq(8);
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_mbus)
{
	signal_mbus_fiq(param);
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_power_irq)
{
	const unsigned pulse = m_power_irq_count++;
	const bool assert_power_irq = nokia_env_u32("NOKI3210_POWER_IRQ_ASSERT", 1) != 0;
	if (assert_power_irq)
		assert_irq(0);

	const uint8_t ccont_status = ccont_boot_status(pulse);
	if (m_ccont.irq_line < 9 && ccont_status != 0)
	{
		ccont_set_irq_status(ccont_status, "boot");
	}
	else
	{
	}
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_keypad)
{
	const uint8_t state = keypad_irq_state();
	const uint8_t falling = m_keypad_irq_state & ~state & 0x1f;

	if (falling != 0)
	{
		assert_irq(6);
	}

	m_keypad_irq_state = state;

	// EXPERIMENT (opt-in): the lower-service / service_ready poll is driven by MAD2
	// IRQ line 4, which in real hardware is raised by the (un-emulated) MAD2 DSP on
	// work completion. Nothing in the driver ever asserts it, so the service_ready
	// setter 0x291068 never runs. Simulate the DSP completion interrupt by pulsing
	// IRQ 4 periodically (200 Hz here) once past early init, to test whether it lets
	// the service come up and clear CONTACT SERVICE. See docs/service_bootstrap.md.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_DSP_IRQ4", 0) != 0 &&
			machine().time().as_double() >= nokia_env_u32("NOKI3210_EXPERIMENT_DSP_IRQ4_AFTER_MS", 250) / 1000.0)
		assert_irq(MAD2_IRQ_LINE_DSP_SERVICE);  // DSP service-completion interrupt (experiment)
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_mad2_soft_reset)
{
	uint8_t reset_reg = uint8_t(param & 0xff) & ~0x04;
	const char *reset_reg_override = std::getenv("NOKI3210_MAD2_SOFT_RESET_REG");
	if (reset_reg_override != nullptr && reset_reg_override[0] != '\0')
		reset_reg = nokia_env_u32("NOKI3210_MAD2_SOFT_RESET_REG", reset_reg) & 0xff;
	m_after_mad2_soft_reset = true;

	m_maincpu->reset();
	m_maincpu->set_state_int(arm7_cpu_device::ARM7_R15, 0x200040);

	if (nokia_env_u32("NOKI3210_MAD2_SOFT_RESET_CLEAR_RAM", 0) != 0)
	{
		std::fill_n(m_ram.get(), 0x40000, 0);
		std::fill_n(m_dsp_ram.get(), 0x800, 0);
	}
	else if (nokia_env_u32("NOKI3210_MAD2_SOFT_RESET_CLEAR_STARTUP_STATE", 0) != 0)
	{
		std::fill_n(&m_ram[(0x112390 - 0x100000) >> 1], 0x80 >> 1, 0);
		std::fill_n(&m_ram[(0x11ff60 - 0x100000) >> 1], 0x40 >> 1, 0);
	}

	memset(m_mad2_regs, 0, 0x100);
	m_mad2_regs[0x01] = reset_reg;
	m_mad2_regs[0x0c] = 0x0a;
	m_mad2_regs[0x03] = 0xff;
	m_fiq_status = 0;
	m_irq_status = 0;
	m_timer1_counter = 0;
	m_timer0_counter = 0;
	m_timer0_divider = 255;
	m_timer0_compare_latched = false;
	m_keypad_irq_state = 0xff;
	m_power_irq_count = 0;
	m_timer_mbus->adjust(attotime::never);
	m_timer_dsp_service->adjust(attotime::never);
	m_timer_power_irq->adjust(attotime::from_msec(nokia_env_u32("NOKI3210_POWER_IRQ_MS", 1000)));
	update_fiq_line();
	update_irq_line();
}

TIMER_CALLBACK_MEMBER(noki3310_state::timer_watchdog)
{
	// CCONT watchdog
	if (m_ccont.watchdog != 0 && nokia_env_u32("NOKI3210_DISABLE_CCONT_WATCHDOG", 0) == 0)
	{
		m_ccont.watchdog--;

		if (m_ccont.watchdog == 0)
		{
			m_maincpu->reset();
			machine_reset();
		}
	}

	// MAD2 watchdog
	if (m_mad2_regs[0x03] != 0xff)
	{
		m_mad2_regs[0x03]--;
		if (m_mad2_regs[0x03] == 0)
		{
			m_maincpu->reset();
			machine_reset();
			m_mad2_regs[0x01] |= 0x02;  // Last reset was by watchdog
		}
	}
}

// Hardware RAM read entry point (registered in the address map). The real
// backing read plus all firmware-research forcing/traces live in the
// quarantined ram_r_firmware_overrides below.
uint16_t noki3310_state::ram_r(offs_t offset, uint16_t mem_mask)
{
	return ram_r_firmware_overrides(offset, mem_mask);
}

// ============================================================================
// Firmware-research RAM-read path: backing read + forcing shims (which can
// rewrite the returned value) + execution traces. NOT clean hardware
// behaviour; should shrink as shims become real models.
// ============================================================================
uint16_t noki3310_state::ram_r_firmware_overrides(offs_t offset, uint16_t mem_mask)
{
	uint16_t data = m_ram[offset];
	const offs_t address = 0x100000 + (offset << 1);
	const u32 pc = m_maincpu->pc();

	// ccont_reg_read (0x2afb44) table probe (opt-in): log the RAM the idx6 availability
	// check reads, to locate the "CCONT status" table it indexes (index 1 & 0x90) and what
	// populates it — the firmware never serial-reads hardware reg 1, so the source is RAM.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc >= 0x002afb44 && pc <= 0x002afbcc)
	{
		static unsigned t_log = 0;
		if (t_log++ < 4000)
			logerror("ccont_read_tbl: pc=%08x reads [%06x]=%02x t=%.4f\n",
					pc, address, debug_ram_byte(address), machine().time().as_double());
	}

	if (pc >= 0x002b1e80 && pc <= 0x002b1f22 && address >= 0x11fc80 && address <= 0x11fc90)
	{
		// Boot-research shim: force the firmware-selected display type while
		// the real board/NV source for this byte is still unidentified.
		const unsigned display_type = nokia_env_u32("NOKI3210_DISPLAY_TYPE", 0xff) & 0xff;
	if (display_type != 0xff && address == 0x11fc86 && mem_mask == 0x00ff)
		data = (data & 0xff00) | display_type;
	}

	// EXPERIMENT (opt-in, diagnostic only): force the FW_STARTUP_SERVICE_BUFFER gate
	// byte non-zero at the resume-sequence gate read (pc 0x2a9132) so the extended
	// task batch — including task 14 — gets resumed. Used to map what lies behind
	// task14_ready; NOT a real model. Mirrors the removed FORCE_STARTUP_SERVICE_READY.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_RESUME_TASK14", 0) != 0 &&
			pc == 0x002a9132 && address == 0x00110c2c)
		data |= 0x0101;

	// EXPERIMENT (opt-in): force the D9 watchdog ack/heartbeat byte non-zero. The
	// watchdog (0x237b2e) resets its counter whenever ack [0x11fedb] != 0, so forcing
	// it keeps the counter from reaching the CONTACT SERVICE timeout. ack is the low
	// byte of the word at 0x11feda. Tests whether the ack heartbeat is the last gate
	// once the DSP/IRQ-4 model has set service_ready + bit 6.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_FORCE_ACK", 0) != 0 && address == 0x0011feda &&
			pc == 0x00237b42)
		data |= 0x0001;

	// EXPERIMENT (opt-in): provision the service-channel enable flag at READ time. The firmware
	// only READS 0x11fee4 (never writes it), so a write-side force can't set it — force the read.
	// Used to test whether provisioning the enable flag (vs the responder trampoline) clears
	// CONTACT SERVICE / changes the post-CS 000d state. Result: it does NOT (see ccont_subsystem.md).
	{
		const unsigned prov_enable = nokia_env_u32("NOKI3210_EXPERIMENT_PROV_READ", 0);
		if (prov_enable != 0 && address == 0x0011fee4)
			data |= (prov_enable & mem_mask);
	}


	// EXPERIMENT (opt-in, diagnostic — like FORCE_ACK, not a model): the contact-service
	// bit-6 loop (0x23487e) clears service-present bit 6 unless every service-channel status
	// byte [0x11fc60+i] (i != 11) reads 0x00/0xfe/0xff. Two are dirty on a blank phone
	// ([0x11fc66]=0xfd idx6, [0x11fc72]=0x12 idx18 — service modules reporting "not OK").
	// Force them to read 0xff ("absent") to test whether a clean service-channel array is
	// the real gate that keeps bit 6 set and clears CONTACT SERVICE (the ack 0x11fedb is a
	// red herring — never written non-zero anywhere reachable).
	// idx6=0x11fc66, idx18=0x11fc72 are even addresses = the HIGH byte of their 16-bit word
	// (big-endian: ROM_REGION16_BE), so force bits 15..8, not 7..0.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_CLEAN_SVCCHAN", 0) != 0 &&
			(address == 0x0011fc66 || address == 0x0011fc72))
		data |= 0xff00;

	// Boot-research shim: startup check 5 currently expects this event-14
	// latch byte to be clear. Replace with the real producer.
	if (offset == ((FW_STARTUP_EVENT14_LATCH - NOKIA_RAM_BASE) >> 1))
		data &= 0xff00;

	return data & mem_mask;
}

// Hardware RAM write entry point (registered in the address map). The backing
// store plus all firmware-research forcing/traces live in the quarantined
// ram_w_firmware_overrides below.
void noki3310_state::ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	ram_w_firmware_overrides(offset, data, mem_mask);
}

// ============================================================================
// Firmware-research RAM-write path: forcing shims (which can rewrite the stored
// value) + execution traces, wrapping the real backing store (COMBINE_DATA).
// NOT clean hardware behaviour; should shrink as shims become real models.
// ============================================================================
void noki3310_state::ram_w_firmware_overrides(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	const offs_t address = 0x100000 + (offset << 1);
	const u32 pc = m_maincpu->pc();

	// FW_STARTUP_SERVICE_BUFFER (0x110c2c) write lifecycle (opt-in): the gate that
	// defers task 14's resume reads byte [0x110c2c]; log who writes it (or never).
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 &&
			(address == 0x00110c2c || address == 0x00110c2e))
	{
		static unsigned svc_log = 0;
		if (svc_log++ < 40)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("svcbuf_write: t=%.4f addr=%06x old=%04x new=%04x mask=%04x pc=%08x lr=%08x mode=%04x  r4=%08x r5=%08x r6=%08x r7=%08x [r6+4]=%04x\n",
					machine().time().as_double(), address, m_ram[offset], data, mem_mask, pc, lr,
					debug_ram_word(FW_STARTUP_MODE),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R5),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R7),
					[&]{ u32 r6 = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
					     return (r6 >= 0x100000 && r6 < 0x180000) ? debug_ram_word(r6 + 4) : 0xeeee; }());
		}
	}

	// CCONT status-shadow writer probe (opt-in): the idx6 availability check reads the CCONT
	// register shadow at [0x11238c] & 0x90 (currently 0). Log writers of the shadow block
	// 0x112380..0x11238f to find what populates it (and thus what real CCONT state idx6 needs).
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 &&
			address >= 0x00112380 && address <= 0x0011238f)
	{
		const u16 oldw = m_ram[offset];
		const u16 neww = (oldw & ~mem_mask) | (data & mem_mask);
		if (oldw != neww)
			logerror("ccont_shadow_write: [%06x] %04x->%04x pc=%08x lr=%08x t=%.4f\n",
					address, oldw, neww, pc, m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					machine().time().as_double());
	}

	// Service-channel status-array writer probe (opt-in): the contact-service bit-6 loop
	// (0x23487e) clears service-present bit 6 if any of the 24 status bytes [0x11fc60+i]
	// (i != 11) is not 0x00/0xfe/0xff. Two are dirty on a blank phone ([0x11fc66]=0xfd,
	// [0x11fc72]=0x12). Log every write to the array so the producer of each is identified.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 &&
			address >= 0x0011fc60 && address <= 0x0011fc77)
	{
		// big-endian array: even byte address = HIGH byte of its 16-bit word. Log the full
		// word transition + both resolved bytes so the writer of a dirty entry is visible
		// (e.g. byte 0x11fc66 = (new >> 8), byte 0x11fc67 = (new & 0xff)).
		const u16 oldw = m_ram[offset];
		const u16 neww = (oldw & ~mem_mask) | (data & mem_mask);
		if (oldw != neww)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("svcchan_write: t=%.4f word@%06x (idx%u hi=%02x idx%u lo=%02x) old=%04x new=%04x pc=%08x lr=%08x\n",
					machine().time().as_double(), address,
					unsigned(address - 0x0011fc60), (neww >> 8) & 0xff,
					unsigned(address - 0x0011fc60 + 1), neww & 0xff, oldw, neww, pc, lr);
		}
	}

	// Task-dispatch set probe (opt-in): the scheduler current-task byte is 0x100022;
	// log each distinct task id that ever runs, to see the full task structure and
	// confirm whether task 0x14 is ever dispatched.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && address == 0x00100022 &&
			(mem_mask & 0xff00) != 0)
	{
		static uint64_t disp_seen = 0;
		const uint8_t nt = uint8_t((data >> 8) & 0x00ff);
		if (nt < 64 && !(disp_seen & (uint64_t(1) << nt)))
		{
			disp_seen |= uint64_t(1) << nt;
			logerror("task_dispatched: t=%.4f task=0x%02x pc=%08x %s\n", machine().time().as_double(),
					nt, pc, nt == 0x14 ? "<-- TASK 14 RUNS" : "");
		}
	}

	// Task14 ready-flag lifecycle probe (opt-in): does anything ever set the flags
	// task14_ready_28ff14 needs? (0x111c93 READY, 0x10dcb0 FINAL_READY, 0x10d1c0
	// HELPER_MODE, 0x10dcae HELPER_READY).
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 &&
			(address == 0x00111c92 || address == 0x0010dcb0 ||
			 address == 0x0010d1c0 || address == 0x0010dcae))
	{
		static unsigned t14_log = 0;
		if (t14_log++ < 40)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("t14_write: t=%.4f addr=%06x old=%04x new=%04x mask=%04x pc=%08x lr=%08x mode=%04x\n",
					machine().time().as_double(), address, m_ram[offset], data, mem_mask, pc, lr,
					debug_ram_word(FW_STARTUP_MODE));
		}
	}

	// Lower-service transmit lifecycle probe (opt-in): log every write to the
	// busy/ready bytes the idle check reads, to see who sets them (the TX queue)
	// and whether anything ever clears them (the transmit completion).
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 &&
			(address == 0x00110d30 || address == 0x00110d34 ||
			 address == 0x0010f4a8 || address == 0x0010f4ac || address == 0x00111794))
	{
		static unsigned ls_log = 0;
		if (ls_log++ < 60)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			const char *nm =
					address == 0x00110d30 ? "queue_block " :
					address == 0x00110d34 ? "queue_block4" :
					address == 0x0010f4a8 ? "tx_flags_a  " :
					address == 0x0010f4ac ? "tx_busy_d   " : "ready_flags ";
			logerror("ls_write: t=%.4f %s[%06x] old=%04x new=%04x mask=%04x pc=%08x lr=%08x\n",
					machine().time().as_double(), nm, address, m_ram[offset], data, mem_mask, pc, lr);
		}
	}

	// Contact-service state lifecycle probe (opt-in): log every write into the
	// contact-service control block (0x11fecc..0x11fedb) and the reason byte, with
	// PC and old->new, to see who initializes/acks it (or never does).
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 &&
			((address >= 0x0011fecc && address <= 0x0011fedc) || address == FW_CONTACT_SERVICE_REASON ||
			 address == 0x0011fee4 || address == 0x0011ff12))
	{
		static unsigned cs_write_log = 0;
		if (cs_write_log++ < 80)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("cs_write: t=%.4f addr=%06x old=%04x new=%04x mask=%04x pc=%08x lr=%08x mode=%04x\n",
					machine().time().as_double(), address, m_ram[offset], data, mem_mask, pc, lr,
					debug_ram_word(FW_STARTUP_MODE));
		}
	}

	auto ram_byte = [this](offs_t addr) -> uint8_t
	{
		if (addr < 0x100000 || addr >= 0x180000)
			return 0xff;

		const uint16_t word = m_ram[(addr - 0x100000) >> 1];
		return (addr & 1) ? uint8_t(word & 0x00ff) : uint8_t(word >> 8);
	};
	auto ram_word = [this](offs_t addr) -> uint16_t
	{
		if (addr < 0x100000 || addr >= 0x180000)
			return 0xffff;
		return m_ram[(addr - 0x100000) >> 1];
	};
	auto mem_byte = [this, &ram_byte](offs_t addr) -> uint8_t
	{
		if (addr >= 0x100000 && addr < 0x180000)
			return ram_byte(addr);

		if ((addr >= 0x00200000 && addr < 0x00600000) || (addr >= 0x00600000 && addr < 0x00a00000))
		{
			memory_region *flash = memregion("flash");
			if (flash)
			{
				const offs_t base = (addr >= 0x00600000) ? 0x00600000 : 0x00200000;
				return flash->base()[(addr - base) % flash->bytes()];
			}
		}

		return 0xff;
	};
	static unsigned event_post_write_count = 0;
	if (pc >= 0x002697aa && pc <= 0x002698da && event_post_write_count++ < 1000)
	{
		const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
			const unsigned d9_timeout_delay = nokia_env_u32("NOKI3210_CONTACT_D9_TIMEOUT_DELAY", 0xffff) & 0xffff;
			if (d9_timeout_delay != 0xffff &&
					pc == 0x002697aa &&
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) == 0x00237b37 &&
					r0 == 0x19 &&
					r4 == 0xd9 &&
					mem_byte(sp + 0) == 0x00 &&
					mem_byte(sp + 1) == 0xd9)
			{
				m_maincpu->set_state_int(arm7_cpu_device::ARM7_R1, d9_timeout_delay);
			}
		if (pc == 0x002697aa && r0 == 0x15)
		{
			m_startup_event15_posted = true;
		}
	}

	const unsigned startup_event15_delay_clamp = nokia_env_u32("NOKI3210_STARTUP_EVENT15_DELAY_CLAMP", 0xffff) & 0xffff;
	if (startup_event15_delay_clamp != 0xffff &&
			pc >= 0x002697aa && pc <= 0x00269bd0 &&
			address == 0x100240)
	{
		data = (data & ~mem_mask) | (startup_event15_delay_clamp & mem_mask);
	}
		const char *battery_profile = std::getenv("NOKI3210_BATTERY_PROFILE");
	if (battery_profile != nullptr &&
				!std::strcmp(battery_profile, "charged") &&
				pc >= 0x00270c80 && pc <= 0x00271230 &&
				address == FW_STARTUP_EVENT &&
				mem_mask == 0xffff &&
				(ram_word(FW_STARTUP_MODE) == FW_STARTUP_MODE_CHARGER_WAIT ||
						ram_word(FW_STARTUP_MODE) == FW_STARTUP_MODE_BATTERY_WAIT ||
						ram_word(FW_STARTUP_MODE) == FW_STARTUP_MODE_POST_CHARGER))
		{
			uint16_t startup_mode = ram_word(FW_STARTUP_MODE);
			uint8_t startup_event_step = (startup_mode == FW_STARTUP_MODE_BATTERY_WAIT) ? m_battery_startup_event_step_mode9 : m_battery_startup_event_step;
			if (startup_mode == FW_STARTUP_MODE_POST_CHARGER &&
					startup_event_step < 3 &&
					ram_word(FW_CCONT_CHARGER_EVENT) != 0)
			{
				data = FW_STARTUP_EVENT_BATTERY_READY;
				m_battery_startup_event_step = 3;
			}
			else if (startup_mode == FW_STARTUP_MODE_POST_CHARGER &&
					startup_event_step >= 3 &&
					!m_post_charger_sequence_entered)
			{
				data = FW_STARTUP_EVENT_BATTERY_READY;
				m_post_charger_sequence_entered = true;
			}
			else if (startup_event_step < 3 && ram_word(FW_CCONT_CHARGER_EVENT) != 0)
			{
		static constexpr uint16_t charge_startup_events[] = {
			FW_STARTUP_EVENT_CHARGER_PRESENT,
			FW_STARTUP_EVENT_BATTERY_PRESENT,
			FW_STARTUP_EVENT_BATTERY_READY
			};
			data = charge_startup_events[startup_event_step];
			if (startup_mode == FW_STARTUP_MODE_BATTERY_WAIT)
				m_battery_startup_event_step_mode9 = startup_event_step + 1;
			else
				m_battery_startup_event_step++;
			}
		}
	// EXPERIMENT (opt-in, diagnostic): mode-000d advance-gate confirmation. The handler
	// (0x270e22) completes the flag byte [0x112399] only when events 0x14/0x15/0x16/0x17 all
	// arrive as FW_STARTUP_EVENT; 0x15/0x16 never get dequeued from the RTOS mailbox, so the
	// flag stalls at 0x08-0x09. Inject the missing events at the dispatch write (0x270e20) to
	// prove the gate is sufficient. NOT faithful — replace with a real CCONT measurement model.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_FORCE_000D_EVENTS", 0) != 0 &&
			pc == 0x00270e20 &&
			address == FW_STARTUP_EVENT &&
			mem_mask == 0xffff &&
			ram_word(FW_STARTUP_MODE) == FW_STARTUP_MODE_CHARGER_WAIT)
	{
		const uint8_t flag = debug_ram_byte(0x00112399);
		if ((flag & 0x02) == 0)        data = 0x16;   // bit 1
		else if ((flag & 0x04) == 0)   data = 0x15;   // bit 2
	}
	// EXPERIMENT (opt-in, diagnostic): scaffold-march. Generalises FORCE_000D_EVENTS to the whole
	// startup mode chain — at any dispatch write of FW_STARTUP_EVENT, inject the advancing event
	// for the current mode (table from the per-mode dispatch disasm). Marches the boot through the
	// charger/battery startup states to see how close idle is. NOT faithful — the real fix is a
	// CCONT measurement-event model that produces this stream. See docs/service_bootstrap.md.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_SCAFFOLD_MARCH", 0) != 0 &&
			address == FW_STARTUP_EVENT &&
			mem_mask == 0xffff &&
			pc >= 0x00270000 && pc <= 0x00271600)
	{
		// PC-specific nested sub-loop waits (take priority over the mode-level event)
		if (pc == 0x00271392)   // mode-0007 tail spins until event 0x74
		{
			data = 0x74;
		}
		else
		switch (ram_word(FW_STARTUP_MODE))
		{
		case 0x000d:   // flag accumulator: feed whichever of 0x14/0x16/0x15/0x17 is still missing
		{
			const uint8_t f = debug_ram_byte(0x00112399);
			if      ((f & 0x01) == 0) data = 0x14;
			else if ((f & 0x02) == 0) data = 0x16;
			else if ((f & 0x04) == 0) data = 0x15;
			else if ((f & 0x08) == 0) data = 0x17;
			break;
		}
		case 0x0004:   data = 0x07;  break;   // POST_SELFTEST       -> BATTERY_READY
		case 0x000b:   data = 0x07;  break;   // POST_CHARGER        -> BATTERY_READY
		case 0x0007:   data = 0x07;  break;   // BATTERY_READY_GATE  -> BATTERY_READY
		case 0x0005:   data = 0x06;  break;   // READY_GATE          -> event 6
		case 0x0006:   data = 0x03;  break;   // SERVICE_QUIESCE_GATE-> event 3
		case 0x0009:   data = 0x0e;  break;   // BATTERY_WAIT        -> CHARGER_PRESENT (try)
		case 0x000c:   data = 0x04;  break;   // (sub-states)        -> try 0x04
		default: break;
		}
	}
	if (nokia_env_u32("NOKI3210_CONTACT_DA_PRESERVE_READY_BIT", 0) != 0 &&
			address == FW_CONTACT_SERVICE_STATUS &&
			mem_mask == 0x00ff &&
			(pc == 0x00237b04 || pc == 0x00237b0c) &&
			(m_ram[offset] & 0x0040) != 0 &&
			(data & 0x0040) == 0)
	{
		data |= 0x0040;
	}
	const uint16_t old_word = m_ram[offset];

	COMBINE_DATA(&m_ram[offset]);

	if (startup_event15_delay_clamp != 0xffff &&
			pc >= 0x002697aa && pc <= 0x00269bd0 &&
			address == 0x100244 &&
			(data & mem_mask & 0xff00) != 0 &&
			ram_byte(0x100245) == 0x15 &&
			ram_word(0x100240) > startup_event15_delay_clamp)
	{
		m_ram[(0x100240 - NOKIA_RAM_BASE) >> 1] = startup_event15_delay_clamp;
	}

	if (!m_startup_latch_complete_seen && address == 0x112398 && ((m_ram[offset] & 0x00ff) == 0x000f))
	{
		m_startup_latch_complete_seen = true;
		m_startup_latch_complete_time = machine().time();
	}

	if (pc == 0x0026a3be &&
			mem_mask == 0xffff &&
			address >= 0x100000 && address < 0x180000)
	{

	}

	if (nokia_env_u32("NOKI3210_SUPPRESS_SIM_CONTEXT_EVENTS", 0) != 0 &&
			pc == 0x0026a3be &&
			mem_mask == 0xffff &&
			m_maincpu->state_int(arm7_cpu_device::ARM7_R1) == 0x00100c70 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R4) == 0x37 ||
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4) == 0x33) &&
			(data == 0x0037 || data == 0x0033))
	{
		// Boot-research shim: useful negative test for SIM-context noise, but
		// not a hardware model and not enabled by the default profile.
		const uint8_t put = ram_byte(0x1014b0);
		const uint8_t restored_put = (put == 0) ? 0x13 : (put - 1);
		m_ram[offset] = old_word;
		m_ram[(0x1014b0 - 0x100000) >> 1] =
				(m_ram[(0x1014b0 - 0x100000) >> 1] & 0x00ff) | (uint16_t(restored_put) << 8);
	}

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

TIMER_CALLBACK_MEMBER(noki3310_state::timer_dsp_service)
{
	// The modelled DSP processes the queued lower-service work: drain the pending counter
	// for real (so the service_ready gate at 0x291096 reads 0 honestly, with no read-time
	// hack) and raise the service interrupt (MAD2 IRQ line 4).
	//
	// Then keep ticking. The firmware resets service_ready at the top of every startup
	// phase (0x2a90d6) and only sets it back from inside the IRQ-4 service path, so it
	// needs the interrupt to recur within each phase window — which is exactly how a
	// continuously-running DSP behaves (a periodic per-frame service tick), not a single
	// completion. Re-arm at the service-tick rate to model that. This replaces the blind
	// wall-clock-gated EXPERIMENT_DSP_IRQ4 pulse, now causally anchored to the DSP being
	// given work and draining real DSP RAM. See docs/service_bootstrap.md.
	m_dsp_ram[DSP_SVC_PENDING_COUNTER_OFF >> 1] = 0;
	assert_irq(MAD2_IRQ_LINE_DSP_SERVICE);
	if (nokia_env_u32("NOKI3210_TRACE_DSP", 0) != 0)
		logerror("dsp_service: tick; drained [0e4]=0, asserted IRQ4  t=%.4f\n",
				machine().time().as_double());
	m_timer_dsp_service->adjust(attotime::from_msec(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_TICK_MS", 5)));
}

uint16_t noki3310_state::dsp_ram_r(offs_t offset)
{
	// DSP-handshake probe (opt-in): log distinct (byte-offset, pc) reads — placed
	// before the hack so it captures the polled status offsets too.
	if (nokia_env_u32("NOKI3210_TRACE_DSP", 0) != 0)
	{
		static uint32_t seen[512] = {}; static unsigned nseen = 0;
		const u32 pc = m_maincpu->pc();
		const uint32_t key = (pc << 9) ^ (offset & 0x1ff);
		bool found = false;
		for (unsigned i = 0; i < nseen; i++) if (seen[i] == key) { found = true; break; }
		if (!found && nseen < 512)
		{
			seen[nseen++] = key;
			logerror("dsprd: off=%03x val=%04x pc=%08x t=%.4f\n",
					(offset & 0x7ff) << 1, m_dsp_ram[offset & 0x7ff], pc, machine().time().as_double());
		}
	}

	// HACK: avoid hangs when ARM try to communicate with the DSP
	if (offset <= 0x004 >> 1)   return 0x01;
	if (offset == 0x0e0 >> 1)   return 0x00;
	if (offset == 0x0fe >> 1)   return 0x01;
	if (offset == 0x100 >> 1)   return 0x01;

	// EXPERIMENT (opt-in): the lower-service "pending work" counter at DSP-shared
	// RAM byte 0xe4 (word offset 0x72) is read by the service_ready setter 0x291068;
	// ready is set only when it is 0. In real hardware the DSP drains it on
	// completion. Simulate that drain so the setter (driven by the IRQ-4 pulse, see
	// timer_keypad) can set service_ready. See docs/service_bootstrap.md.
	if (offset == (DSP_SVC_PENDING_COUNTER_OFF >> 1) && nokia_env_u32("NOKI3210_EXPERIMENT_DSP_IRQ4", 0) != 0)
		return 0x00;

	return m_dsp_ram[offset & 0x7ff];
}

void noki3310_state::dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (nokia_env_u32("NOKI3210_TRACE_DSP", 0) != 0)
	{
		static uint32_t seen[256] = {}; static unsigned nseen = 0;
		const u32 pc = m_maincpu->pc();
		const uint32_t key = (pc << 8) ^ (offset & 0xff);
		bool found = false;
		for (unsigned i = 0; i < nseen; i++) if (seen[i] == key) { found = true; break; }
		if (!found && nseen < 256)
		{
			seen[nseen++] = key;
			logerror("dspwr: off=%03x data=%04x pc=%08x t=%.4f\n",
					(offset & 0x7ff) << 1, data, pc, machine().time().as_double());
		}
		// Targeted (un-deduped): the lower-service pending counter at byte 0xe4 is read
		// 0x0002 by the service_ready gate (0x291096) yet never appears in the deduped
		// dspwr stream (the 0xe00 program upload saturates it). Log every write here so
		// the DSP-handshake model can anchor to the real "work queued" event.
		if ((offset & 0x7ff) == (DSP_SVC_PENDING_COUNTER_OFF >> 1))
			logerror("dspwr-pending: off=0e4 data=%04x pc=%08x t=%.4f\n",
					data, m_maincpu->pc(), machine().time().as_double());
	}
	COMBINE_DATA(&m_dsp_ram[offset & 0x7ff]);

	// DSP service-completion model (opt-in): when the MCU queues lower-service work by
	// writing a non-zero count to the pending counter (byte 0xe4, pc 0x290c98), the real
	// MAD2 DSP processes it and signals completion by draining the counter and raising
	// IRQ line 4. Model that: schedule a completion after a short processing delay. This
	// is the faithful replacement for the EXPERIMENT_DSP_IRQ4 force (blind periodic pulse
	// + read-time fake-zero). See docs/service_bootstrap.md.
	if (nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE", 0) != 0 &&
			(offset & 0x7ff) == (DSP_SVC_PENDING_COUNTER_OFF >> 1) && data != 0)
		m_timer_dsp_service->adjust(attotime::from_msec(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_DELAY_MS", 5)));
}

// ============================================================================
// Firmware-research hooks for flash fetches: forcing shims + execution traces.
// This is NOT hardware behaviour. It returns an override fetch value, or
// nullopt to let the real flash read proceed. It should shrink toward empty as
// each shim is replaced by a real hardware/scheduler model.
// ============================================================================
std::optional<uint16_t> noki3310_state::flash_firmware_hooks(offs_t offset, u32 pc, u32 addr, uint16_t mem_mask)
{
	// ========================================================================
	// MODEL: node-0x18 service responder (opt-in, NOKI3210_MODEL_SVC_RESPONDER).
	// The contact-service completes when it receives a message {[3]=0x40,[8]=0x64,
	// [9]=0x05}; node 0x18 never answers, so we synthesise it by driving the
	// firmware's OWN primitives — alloc 0x26afe0(size) -> fill -> post 0x26a204(task,
	// msg) — trampolined from this instruction-fetch hook. We set PC reliably by
	// overriding the fetched opcode with "BX r12" (after setting r12); the firmware
	// function returns to a flash sentinel (LR=SENT|1) where the hook fires again.
	// Trigger at the contact-service loop top 0x237bc6 (a safe point, not inside the
	// scheduler). See docs/service_bootstrap.md.
	if (nokia_env_u32("NOKI3210_MODEL_SVC_RESPONDER", 0) != 0 && pc == addr)
	{
		constexpr u32 SENT = 0x003ff000;     // unused flash addr used as a Thumb return sentinel
		constexpr uint16_t BX_R12 = 0x4760;  // Thumb: BX r12
		if (m_svcresp_state == 3 && nokia_env_u32("NOKI3210_SVC_RESPONDER_PCTRACE", 0) != 0)
		{
			static unsigned pctr = 0;
			if (pctr < 60) { pctr++; logerror("svcresp_pc: %08x t=%.5f\n", addr, machine().time().as_double()); }
		}
		auto setr = [&](int r, u32 v){ m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0 + r, v); };
		auto getr = [&](int r){ return u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0 + r)); };

		if (m_svcresp_state == 0 && addr == 0x00237bc6 &&
				machine().time().as_double() >= nokia_env_u32("NOKI3210_SVC_RESPONDER_DELAY_MS", 450) / 1000.0)
		{
			for (int i = 0; i < 15; i++) m_svcresp_saved[i] = getr(i);
			m_svcresp_saved[15] = m_maincpu->state_int(arm7_cpu_device::ARM7_CPSR);
			if (nokia_env_u32("NOKI3210_MODEL_SVC_RESPONDER", 0) == 2)
			{
				// dry-run: save then immediately restore (tests the trampoline mechanism
				// in isolation — should be a no-op and boot to d8a9a7).
				for (int i = 0; i < 15; i++) setr(i, m_svcresp_saved[i]);
				m_maincpu->set_state_int(arm7_cpu_device::ARM7_CPSR, m_svcresp_saved[15]);
				setr(12, 0x00237bc6 | 1);
				m_svcresp_state = 3;
				logerror("svcresp: DRY-RUN save+restore at trigger t=%.4f\n", machine().time().as_double());
				return BX_R12;
			}
			setr(0, nokia_env_u32("NOKI3210_SVC_RESPONDER_MSGSZ", 0x14));   // alloc size
			setr(14, SENT | 1);                                            // LR -> sentinel
			setr(12, 0x0026afe0 | 1);                                      // r12 -> alloc
			m_svcresp_state = 1;
			logerror("svcresp: trigger task=%02x t=%.4f -> alloc(%#x)\n",
					debug_ram_byte(0x00100022), machine().time().as_double(),
					nokia_env_u32("NOKI3210_SVC_RESPONDER_MSGSZ", 0x14));
			return BX_R12;
		}
		if (m_svcresp_state == 1 && addr == SENT)
		{
			const u32 msg = getr(0);
			if (msg >= 0x00100000 && msg < 0x00180000)
			{
				for (int i = 0; i < 0x14; i++) debug_ram_byte_w(msg + i, 0);
				debug_ram_byte_w(msg + 3, nokia_env_u32("NOKI3210_SVC_RESPONDER_B3", 0x40));   // -> 0x237400 dispatch
				debug_ram_byte_w(msg + 8, nokia_env_u32("NOKI3210_SVC_RESPONDER_B8", 0x64));   // -> response handler 0x236dc4
				debug_ram_byte_w(msg + 9, nokia_env_u32("NOKI3210_SVC_RESPONDER_B9", 0x05));   // -> HEALTHY substate 5
				const uint8_t task = debug_ram_byte(0x00100022);
				setr(0, task);
				setr(1, msg);
				setr(14, SENT | 1);
				setr(12, 0x0026a204 | 1);          // r12 -> post_task_message
				m_svcresp_msg = msg;
				m_svcresp_state = 2;
				logerror("svcresp: alloc=%08x -> post(task=%02x msg{3=40,8=64,9=05})\n", msg, task);
				return BX_R12;
			}
			// alloc failed: restore and bail
			for (int i = 0; i < 15; i++) setr(i, m_svcresp_saved[i]);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_CPSR, m_svcresp_saved[15]);
			setr(12, 0x00237bc6 | 1);
			m_svcresp_state = 3;
			logerror("svcresp: alloc returned %08x (not RAM) — aborted\n", msg);
			return BX_R12;
		}
		if (m_svcresp_state == 2 && addr == SENT)
		{
			for (int i = 0; i < 15; i++) setr(i, m_svcresp_saved[i]);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_CPSR, m_svcresp_saved[15]);
			setr(12, 0x00237bc6 | 1);              // resume the contact-service loop
			m_svcresp_state = 3;
			logerror("svcresp: posted; resuming contact-service loop t=%.4f\n", machine().time().as_double());
			return BX_R12;
		}
	}

	// Scheduler message/event BUS wiretap (opt-in): one consistent line per inter-task
	// interaction, to reconstruct the whole startup state machine breadth-first.
	//   post_task_message 0x26a204 / 0x26a354 (r0=target task, r1=msg ptr; msg[0..1]=id, [2]=a0,[3]=a1)
	//   event_post 0x2697aa (r0=event id, r1=arg);  event2 0x2698e4 (r0=event id)
	//   resume 0x269c6e (r0=task);  recv 0x26a458 (current task is waiting)
	if (nokia_env_u32("NOKI3210_TRACE_BUS", 0) != 0 && pc == addr &&
			(addr == 0x0026a204 || addr == 0x0026a354 || addr == 0x002697aa ||
			 addr == 0x002698e4 || addr == 0x00269c6e || addr == 0x0026a458))
	{
		static unsigned bus_log = 0;
		if (bus_log++ < 3000)
		{
			const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			const u32 r1 = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			const uint8_t cur = debug_ram_byte(0x00100022);
			const uint16_t mode = debug_ram_word(FW_STARTUP_MODE);
			auto msg = [&](u32 m) {
				if (m < 0x100000 || m >= 0x180000) return;
				logerror("bus: t=%.4f mode=%04x cur=%02x POST->task=%02x id=%04x a0=%02x a1=%02x lr=%08x\n",
						machine().time().as_double(), mode, cur, r0 & 0xff,
						debug_ram_word(m), debug_ram_byte(m + 2), debug_ram_byte(m + 3), lr); };
			if (addr == 0x0026a204 || addr == 0x0026a354) msg(r1);
			else if (addr == 0x002697aa)
				logerror("bus: t=%.4f mode=%04x cur=%02x EVENT id=%02x arg=%04x lr=%08x\n",
						machine().time().as_double(), mode, cur, r0 & 0xff, r1 & 0xffff, lr);
			else if (addr == 0x002698e4)
				logerror("bus: t=%.4f mode=%04x cur=%02x EVENT2 id=%02x lr=%08x\n",
						machine().time().as_double(), mode, cur, r0 & 0xff, lr);
			else if (addr == 0x00269c6e)
				logerror("bus: t=%.4f mode=%04x cur=%02x RESUME task=%02x lr=%08x\n",
						machine().time().as_double(), mode, cur, r0 & 0xff, lr);
			else
				logerror("bus: t=%.4f mode=%04x cur=%02x RECV(wait) lr=%08x\n",
						machine().time().as_double(), mode, cur, lr);
		}
	}

	// EXPERIMENT (opt-in, diagnostic only): force task14_ready_28ff14 to "pass" by
	// setting R0=1 at the readiness-loop check (0x2a931e), to map what blocks the
	// boot once task 14 is treated as ready. NOT a real model.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_FORCE_TASK14_READY", 0) != 0 &&
			pc == addr && addr == 0x002a931e)
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 1);


	// Task-resume batch-2 gate probe (opt-in): task 14's resume is in the second,
	// conditionally-skipped batch of the startup resume sequence. Log the gate
	// decision (0x2a9186) and whether task 14's resume (0x2a91dc) is reached.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr &&
			(addr == 0x002a91ac || addr == 0x002a91f0 || addr == 0x002a9216 || addr == 0x002a91fe))
	{
		static unsigned rg_log = 0;
		if (rg_log++ < 12)
		{
			const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			const u32 r6 = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
			const u32 r7 = m_maincpu->state_int(arm7_cpu_device::ARM7_R7);
			auto rb = [&](u32 a) { return (a >= 0x100000 && a < 0x180000) ? debug_ram_byte(a) : 0xee; };
			const char *where = addr == 0x002a91ac ? "BATCH-2 (resumes task 0x14)" :
					addr == 0x002a91f0 ? "gate-SKIP (batch-1 only)" :
					addr == 0x002a91fe ? "MINIMAL path (early divert, task 14 NOT resumed)" :
					"SKIP-ALL";
			(void)r4; (void)r6;
			logerror("resume_gate: t=%.4f %s  [r7+1]=%02x [0x110c2c]=%02x  (divert if [r7+1]==5 or [110c2c]==0)  r7=%08x mode=%04x\n",
					machine().time().as_double(), where, rb(r7 + 1), rb(0x00110c2c), r7,
					debug_ram_word(FW_STARTUP_MODE));
		}
	}

	// Ready-list insert probe (opt-in): 0x2699be(list, tcb) links a task into the
	// scheduler ready list; r1=TCB, task id at [TCB+0xe]. Log the distinct set of
	// tasks ever made runnable — if 0x14 never appears, task 14 is never created/resumed.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x002699be)
	{
		static uint64_t rdy_seen = 0;
		const u32 tcb = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		if (tcb >= 0x100000 && tcb < 0x180000)
		{
			const uint8_t tid = debug_ram_byte(tcb + 0x0e);
			if (tid < 64 && !(rdy_seen & (uint64_t(1) << tid)))
			{
				rdy_seen |= uint64_t(1) << tid;
				const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
				logerror("ready_insert: t=%.4f task=0x%02x lr=%08x %s\n", machine().time().as_double(),
						tid, lr, tid == 0x14 ? "<-- TASK 14 MADE READY" : "");
			}
		}
	}

	// Task-14 body probe (opt-in): scheduler current-task id is at 0x100022; when the
	// recv loop (0x26a458) runs under task 0x14, log the caller (LR = task 14's body)
	// and how far along it is. If this never fires, task 14 never runs its body.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x0026a458)
	{
		// Record the set of distinct task ids that reach the recv loop (bit per id).
		static uint64_t tasks_seen = 0;
		const uint8_t cur = debug_ram_byte(0x00100022);
		if (cur < 64 && !(tasks_seen & (uint64_t(1) << cur)))
		{
			tasks_seen |= uint64_t(1) << cur;
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("task_recv_seen: t=%.4f task=0x%02x lr=%08x %s\n",
					machine().time().as_double(), cur, lr, cur == 0x14 ? "<-- TASK 14" : "");
		}
	}

	// Task-14 drive probe (opt-in): is the task-14 message ever posted (0x28ff38),
	// and is its trigger handler (0x275ff8) reached? Shows whether task 14 is driven.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr &&
			(addr == 0x0028ff38 || addr == 0x00275ffc || addr == 0x0028ff14))
	{
		static unsigned t14d_log = 0;
		if (t14d_log++ < 40)
		{
			const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			if (addr == 0x0028ff14)
			{
				// compare task 0x14 TCB (0x1016b4, never runs) vs task 0x02 TCB
				// (0x1014bc, runs) + scheduler state (0x100020), to see if task 14
				// is created-but-suspended or never created. TCB = 0x1c bytes.
				logerror("tcb14[1016b4]= %04x %04x %04x %04x %04x %04x %04x\n",
						debug_ram_word(0x1016b4), debug_ram_word(0x1016b8), debug_ram_word(0x1016bc),
						debug_ram_word(0x1016c0), debug_ram_word(0x1016c4), debug_ram_word(0x1016c8),
						debug_ram_word(0x1016cc));
				logerror("tcb02[1014bc]= %04x %04x %04x %04x %04x %04x %04x\n",
						debug_ram_word(0x1014bc), debug_ram_word(0x1014c0), debug_ram_word(0x1014c4),
						debug_ram_word(0x1014c8), debug_ram_word(0x1014cc), debug_ram_word(0x1014d0),
						debug_ram_word(0x1014d4));
				logerror("sched[100020]= %04x %04x %04x %04x  curtask=%02x\n",
						debug_ram_word(0x100020), debug_ram_word(0x100024), debug_ram_word(0x100028),
						debug_ram_word(0x10002c), debug_ram_byte(0x100022));
			}
			else
				logerror("t14_drive: t=%.4f %s r0=%08x mode=%04x\n", machine().time().as_double(),
						addr == 0x0028ff38 ? "POST-task14-msg(0x28ff38)" : "trigger-fn(0x275ffc)",
						r0, debug_ram_word(FW_STARTUP_MODE));
		}
	}

	// MBUS RX state-machine probe (opt-in): at the handler entry (0x2aae76) log the
	// state (0x10f4a8), the RX-byte countdown (0x10f4ae), and the RX data reg; and
	// flag whether service_transport_complete (0x2b052e) is ever actually called.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr &&
			(addr == 0x002aae76 || addr == 0x002b052e || addr == 0x002aaf44 ||
			 addr == 0x002b0554 || addr == 0x002b0590))
	{
		static unsigned rxsm_log = 0;
		if (rxsm_log++ < 120)
		{
			if (addr == 0x002aaf44)
				logerror("rx_sm: t=%.4f STATE-1 cmp: rx_byte[10f4ad]=%02x vs expected[111794]=%02x "
						"(G[10f4b4]=%02x)  %s\n", machine().time().as_double(),
						debug_ram_byte(0x0010f4ad), debug_ram_byte(0x00111794), debug_ram_byte(0x0010f4b4),
						debug_ram_byte(0x0010f4ad) == debug_ram_byte(0x00111794) ? "match->state2" : "MISMATCH->abort(state8)");
			else if (addr == 0x002b052e)
			{
				// Dump the inbound frame buffer (r0 = frame ptr) the router processes,
				// plus the phone address it compares against, to see routing inputs.
				const u32 fp = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
				if (fp >= 0x100000 && fp < 0x180000)
					logerror("rx_sm: t=%.4f *** transport_complete_2b052e *** frame=%08x  "
							"[%02x %02x %02x %02x %02x %02x %02x %02x]  ouraddr[111794]=%02x\n",
							machine().time().as_double(), fp,
							debug_ram_byte(fp+0), debug_ram_byte(fp+1), debug_ram_byte(fp+2), debug_ram_byte(fp+3),
							debug_ram_byte(fp+4), debug_ram_byte(fp+5), debug_ram_byte(fp+6), debug_ram_byte(fp+7),
							debug_ram_byte(0x00111794));
				else
					logerror("rx_sm: t=%.4f *** transport_complete_2b052e *** frame=%08x (non-RAM)\n",
							machine().time().as_double(), fp);
			}
			else if (addr == 0x002b0554 || addr == 0x002b0590)
				logerror("rx_sm: t=%.4f route_post_%s: 0x26aac0 returned r0=%08x (1=delivered to task7)\n",
						machine().time().as_double(), addr == 0x002b0554 ? "A" : "B",
						m_maincpu->state_int(arm7_cpu_device::ARM7_R0));
			else
				logerror("rx_sm: t=%.4f state[10f4a8]=%02x count[10f4ae]=%04x rxreg=%02x "
						"mbus_ctrl[18]=%02x mbus_stat[19]=%02x\n",
						machine().time().as_double(),
						debug_ram_byte(0x0010f4a8), debug_ram_word(0x0010f4ae),
						m_mad2_regs[0x1a], m_mad2_regs[0x18], m_mad2_regs[0x19]);
		}
	}

	// Service-startup dispatch probe (opt-in): at service_dispatch entry (0x290cf4) log
	// the command + args and the live ready/status bytes, to map the bit-field state
	// machine timeline and see what (if anything) reaches the completion that sets ready.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x00290cf4)
	{
		static unsigned disp_log = 0;
		if (disp_log++ < 120)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("svc_disp: t=%.4f cmd=%02x arg1=%02x arg2=%02x  ready[110c2c]=%02x status[110c2e]=%04x  lr=%08x mode=%04x\n",
					machine().time().as_double(),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R2) & 0xff,
					debug_ram_byte(0x00110c2c), debug_ram_word(0x00110c2e), lr,
					debug_ram_word(FW_STARTUP_MODE));
		}
	}

	// PM / service-response trace (opt-in). The contact-service reads remote data
	// (e.g. logical address 0x5f00) as MBUS request frames via 0x2b13a2
	// (r0=count, r1=address, r2=dest), and dispatches the async response by command
	// at 0x236dc6 (r0/r6 = command; 0x05 => healthy substate). See docs/service_bootstrap.md.
	// PM read-validity probe: 0x2b12b4 returns 0 (drop, no request sent) unless the
	// service-channel enable flags (0x11fee4) are set AND the address is "registered".
	// 0x2b13b0 is the return site (r0 = validity, r5 = address).
	if (nokia_env_u32("NOKI3210_TRACE_PM", 0) != 0 && pc == addr && addr == 0x002b13b0)
	{
		static unsigned pmv_log = 0;
		const u32 addr_req = m_maincpu->state_int(arm7_cpu_device::ARM7_R5) & 0xffff;
		if (addr_req == 0x5f00 && pmv_log++ < 8)
			logerror("pm_valid: t=%.4f addr=%04x valid=%u (0=dropped) enable_flags[11fee4]=%02x\n",
					machine().time().as_double(), addr_req,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0), debug_ram_byte(0x0011fee4));
	}

	// Request-message dump (opt-in): at the post site 0x2b0482 (r0 = message ptr), dump
	// the request frame for the 0x5f00 read ([msg+8/9] = address) so the response format
	// can be synthesised. Only fires when the read actually transmits (i.e. the channel is
	// enabled / the validity check passes) — on a blank phone the read is dropped.
	if (nokia_env_u32("NOKI3210_TRACE_PM", 0) != 0 && pc == addr && addr == 0x002b0482)
	{
		static unsigned req_log = 0;
		const u32 m = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (m >= 0x100000 && m < 0x180000 &&
				debug_ram_byte(m + 8) == 0x5f && debug_ram_byte(m + 9) == 0x00 && req_log++ < 4)
		{
			char buf[64]; int n = 0;
			for (int i = 0; i < 0x14; i++)
				n += snprintf(buf + n, sizeof(buf) - n, "%02x ", debug_ram_byte(m + i));
			logerror("pm_request: t=%.4f msg=%08x  [%s]\n", machine().time().as_double(), m, buf);
		}
	}

	if (nokia_env_u32("NOKI3210_TRACE_PM", 0) != 0 && pc == addr &&
			(addr == 0x002b13a2 || addr == 0x00236dc6))
	{
		static unsigned pm_log = 0;
		if (pm_log++ < 200)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			if (addr == 0x002b13a2)
				logerror("pm_read: t=%.4f addr=%04x count=%u dest=%02x lr=%08x\n",
						machine().time().as_double(),
						m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xffff,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R2) & 0xff, lr);
			else
				logerror("svc_response: t=%.4f command=%02x lr=%08x %s\n",
						machine().time().as_double(),
						m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff, lr,
						(m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff) == 0x05 ? "<-- HEALTHY" : "");
		}
	}

	// service_ready setter probe (opt-in): the setter at 0x29109e writes ready[0x110c2c]=1
	// iff the lower-service pending counter [0x100e4] (base 0x10000) is 0. Probe the
	// branch target 0x2910a0 (just after the decision) to see if this function runs and
	// whether ready got set; capture the gate halfword from r0 just-loaded paths.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x002910a0)
	{
		static unsigned setr_log = 0;
		if (setr_log++ < 12)
			logerror("svc_ready_setter: t=%.4f ran; ready[110c2c]=%02x  (set iff gate[100e4]==0)\n",
					machine().time().as_double(), debug_ram_byte(0x00110c2c));
	}

	// Contact-service EEPROM-checksum probe (opt-in): at the return target after the
	// EEPROM[0x244] read (0x2347fe), r4 = firmware-computed checksum of EEPROM[0x120..0x243]
	// and [sp+4] = the stored checksum it compares against (0x234810). Log both so the
	// exact value to write at EEPROM[0x244] is known.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x002347fe)
	{
		static unsigned cksum_log = 0;
		if (cksum_log++ < 8)
		{
			const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
			const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			logerror("cs_cksum: t=%.4f computed_r4=%04x stored[sp+4]=%02x%02x (word=%04x)  match=%d\n",
					machine().time().as_double(), r4 & 0xffff,
					debug_ram_byte(sp+5), debug_ram_byte(sp+4), debug_ram_word(sp+4),
					(r4 & 0xffff) == debug_ram_word(sp+4));
		}
	}

	// ccont_reg_read (0x2afb44) entry probe (opt-in): log arg r0 (packs reg-index<<8 | mask)
	// and caller lr, so the idx6 call (lr~0x295ec3) and its early vs late behaviour is visible.
	if (nokia_env_u32("NOKI3210_TRACE_CCONT_READ", 0) != 0 && pc == addr && addr == 0x002afb44)
	{
		static unsigned e_log = 0;
		if (e_log++ < 40)
			logerror("ccont_reg_read: arg=%04x lr=%08x t=%.4f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
	}

	// channel-open gate probe (opt-in): 0x2366e0 is the cmp after sum16(block,0x40) (0x2a41d0);
	// r0 = the checksum. If 0 -> enable arg = 0 (channel not opened). Then 0x2b140a is the
	// channel-open: r1 = the master-enable value written to 0x11fee4. Log both to see the gate.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x002366e0)
		logerror("chan_open_gate: sum16(block,0x40)=%04x t=%.4f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff, machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x002b140a)
		logerror("chan_open: enable(r1)=%02x r0=%02x r2=%02x lr=%08x t=%.4f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());

	// idx18 EEPROM-checksum probe (opt-in): 0x264c56 checks sum16(cache[0..0x11b]) == word[0x11c]
	// (sum16 = 0x2a41d0 at 0x264c74). At its return site 0x264c78, r0 = the computed sum and
	// r4 = the cache base. Log computed vs stored so the exact mismatch is known.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x00264c78)
	{
		static unsigned i18 = 0;
		if (i18++ < 6)
		{
			const u32 base = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			const u32 computed = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
			logerror("idx18_cksum: base=%08x computed=%04x stored[+0x11c]=%04x t=%.4f\n",
					base, computed,
					(base >= 0x100000 && base < 0x180000) ? debug_ram_word(base + 0x11c) : 0xeeee,
					machine().time().as_double());
		}
	}

	// limp probe (opt-in): the post-CONTACT-SERVICE loop grinds sum16 (0x2a41d0). Log its
	// caller + (ptr,count) to see which block it re-validates, and whether the ADC monitor
	// source walker (0x2a7230) is the loop.
	if (nokia_env_u32("NOKI3210_TRACE_LIMP", 0) != 0 && pc == addr && addr == 0x0021c4a0)
	{
		static unsigned l1 = 0;
		if (l1++ < 24)
			logerror("limp_loop: cksum_refresh caller lr=%08x mode=%04x t=%.5f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_word(0x001123f0), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_LIMP", 0) != 0 && pc == addr && addr == 0x002a7230)
	{
		static unsigned l2 = 0;
		if (l2++ < 8) logerror("limp_adcmon: 0x2a7230 reached t=%.4f\n", machine().time().as_double());
	}
	// charger-chain probes: does the detector run, and what event does the wait receive?
	if (nokia_env_u32("NOKI3210_TRACE_LIMP", 0) != 0 && pc == addr && addr == 0x002b084c)
	{
		static unsigned c1 = 0;
		if (c1++ < 6) logerror("limp_chgcheck: charger_present_check 0x2b084c runs t=%.4f\n", machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_LIMP", 0) != 0 && pc == addr && addr == 0x00271252)
	{
		static unsigned c2 = 0;
		if (c2++ < 12) logerror("limp_chgwait: recv event=%u (3=present,7=absent->go,0xe=followup) t=%.4f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff, machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_LIMP", 0) != 0 && pc == addr && addr == 0x00271266)
	{
		static unsigned c3 = 0;
		if (c3++ < 4) logerror("limp_chgadvance: event 7 -> post_charger_continue 0x271266 t=%.4f\n", machine().time().as_double());
	}

	// limp2 probes (opt-in): what drives the mode-000d startup task. 0x2697aa = post_startup_event
	// (r0=event id); the three call sites of charger-detect+post 0x2b09f2; and 0x2b09f2 itself with
	// the charger-event latch word [0x1124c8]. Shows whether ANY event reaches the task in 000d.
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr && addr == 0x002697aa)
	{
		static unsigned e1 = 0;
		if (e1++ < 800)
			logerror("limp2_evpost: ev=%u arg=%u mode=%04x latch=%04x lr=%08x t=%.5f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xffff,
					debug_ram_word(0x001123f0), debug_ram_word(0x001124c8),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					machine().time().as_double());
	}
	// task-message POST probe: at 0x26a204(r0=task, r1=msgptr) scan the message buffer for a
	// sweep-event id (0x14/0x16/0x17); log offset, value, target task, and caller lr — finds the
	// producers of the mailbox messages the 000d handler consumes.
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr && addr == 0x0026a204)
	{
		static unsigned pp = 0;
		const u32 task = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 msg  = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		if (task == 3 && msg >= 0x00100000 && msg < 0x00180000 && pp++ < 60)
			logerror("limp2_post: task=3 hdr[0..6]=%02x %02x %02x %02x %02x %02x %02x lr=%08x t=%.5f\n",
					debug_ram_byte(msg+0), debug_ram_byte(msg+1), debug_ram_byte(msg+2),
					debug_ram_byte(msg+3), debug_ram_byte(msg+4), debug_ram_byte(msg+5),
					debug_ram_byte(msg+6), m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					machine().time().as_double());
	}
	// startup-message dequeue probe: at 0x26ff1a (just after bl 0x26a458) log the raw message id
	// r0 the translator received — the channel the 000d handler actually reads (vs 0x2697aa events).
	// 0x70 channel-map response probes: does the 0x70/0x71 command handler (0x23670c) and/or the
	// channel-map-apply (0x2366c8) run, with what enable flags? Tests delivering a 0x70 response via
	// the responder (SVC_RESPONDER_B9=0x70) so the firmware sets 0x11fee4 itself.
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr &&
			(addr == 0x00236dc4 || addr == 0x00236e60 || addr == 0x002b140a))
	{
		static unsigned ch70 = 0;
		if (ch70++ < 24)
			logerror("svc70: pc=%08x (236dc4=resp,236e60=high-cmd,2b140a=config-writer) r0=%02x r6=%02x enable[11fee4]=%02x%02x t=%.5f\n",
					pc, m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R6) & 0xff,
					debug_ram_byte(0x0011fee4), debug_ram_byte(0x0011fee5), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr && addr == 0x0026ff1a)
	{
		static unsigned dq = 0;
		const u32 id = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
		if (dq++ < 200 && id != 0)
			logerror("limp2_deq: msgid=%02x mode=%04x t=%.5f\n",
					id, debug_ram_word(0x001123f0), machine().time().as_double());
		// provisioning-gate check (one-shot at 000d): dump the channel-enable flags (the
		// CONTACT-SERVICE provisioning state) and the per-event records at 0x100140+ev*0xc for the
		// sweep events, to compare delivering (0x14/0x17) vs not (0x15/0x16).
		static bool dumped = false;
		if (!dumped && debug_ram_word(0x001123f0) == 0x000d)
		{
			dumped = true;
			logerror("limp2_prov: chan_enable[11fee4]=%02x%02x mask[11ff08]=%02x%02x%02x%02x\n",
					debug_ram_byte(0x0011fee4), debug_ram_byte(0x0011fee5),
					debug_ram_byte(0x0011ff08), debug_ram_byte(0x0011ff09),
					debug_ram_byte(0x0011ff0a), debug_ram_byte(0x0011ff0b));
			for (uint8_t ev : { 0x14, 0x15, 0x16, 0x17 })
			{
				const offs_t r = 0x00100140 + ev * 0xc;
				logerror("limp2_prov: ev=%02x rec@%06x: +6=%02x +7=%02x +8=%02x +9=%02x +a=%02x\n",
						ev, r, debug_ram_byte(r+6), debug_ram_byte(r+7),
						debug_ram_byte(r+8), debug_ram_byte(r+9), debug_ram_byte(r+0xa));
			}
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr && addr == 0x002b09f2)
	{
		static unsigned e2 = 0;
		if (e2++ < 12)
			logerror("limp2_chgdetect: 0x2b09f2 entry mode=%04x latch=%04x lr=%08x t=%.5f\n",
					debug_ram_word(0x001123f0), debug_ram_word(0x001124c8),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr &&
			(addr == 0x00270d54 || addr == 0x00270e0e || addr == 0x0027102a))
	{
		static unsigned e3 = 0;
		if (e3++ < 12)
			logerror("limp2_chgpost_site: pc=%08x reached mode=%04x t=%.5f\n",
					pc, debug_ram_word(0x001123f0), machine().time().as_double());
	}
	// mode-000d advance gate: at the dispatch top (0x270e22) log the event the handler sees
	// plus the two gate bytes — flag accumulator [0x112399] (needs low nibble 0xf = all of
	// 0x14/0x15/0x16/0x17 seen) and FW_CCONT_STATE [0x11ff6c] (needs low nibble 6).
	// mode-trajectory tracker (opt-in): log FW_STARTUP_MODE whenever it changes, sampled at
	// the frequently-run cksum loop 0x21c4a0 — shows the full mode progression compactly.
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr && addr == 0x0021c4a0)
	{
		static uint16_t last_mode = 0xffff;
		const uint16_t m = debug_ram_word(0x001123f0);
		if (m != last_mode)
		{
			logerror("limp2_mode: %04x -> %04x  flag[112399]=%02x ccont_state[11ff6c]=%02x t=%.5f\n",
					last_mode, m, debug_ram_byte(0x00112399), debug_ram_byte(0x0011ff6c),
					machine().time().as_double());
			last_mode = m;
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_LIMP2", 0) != 0 && pc == addr && addr == 0x00270e24)
	{
		static unsigned e4 = 0;
		if (e4++ < 50)
			logerror("limp2_000dgate: ev=%02x flag[112399]=%02x ccont_state[11ff6c]=%02x t=%.5f\n",
					debug_ram_word(0x001123ee) & 0xffff,
					debug_ram_byte(0x00112399), debug_ram_byte(0x0011ff6c),
					machine().time().as_double());
	}

	// ccont_reg_read internal-path probe (opt-in): which branch the idx6 call (lr~0x295ec3)
	// takes — cache (0x2afb60), live serial read (0x2afb76), or the return normaliser (0x2afbca).
	if (nokia_env_u32("NOKI3210_TRACE_CCONT_READ", 0) != 0 && pc == addr &&
			(addr == 0x002afb60 || addr == 0x002afb76 || addr == 0x002afbca))
	{
		const u32 lr2 = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		if (lr2 >= 0x00295ec0 && lr2 <= 0x00295ec4)
			logerror("ccont_path: pc=%08x r4=%02x r5=%02x r6=%02x t=%.4f\n", pc,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R4) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R5) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R6) & 0xff, machine().time().as_double());
	}

	// idx6 CCONT-check result probe (opt-in): after the idx6 routine's availability call
	// (0x295ebe: bl 0x2afb44 = ccont_reg_read(0x9001) = index 0x10, mask 0x01), r0 at the
	// return 0x295ec2 is the masked value; non-zero => idx6 clean. Log what it actually reads.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x00295ec2)
	{
		static unsigned idx6_log = 0;
		if (idx6_log++ < 8)
			logerror("idx6_ccont_check: t=%.4f r0=%02x  (idx6 clean iff r0 != 0; reads CCONT reg 0xe bit0)\n",
					machine().time().as_double(), m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff);
	}

	// bit-6 service_ready check probe (opt-in): the contact-service init reads service_ready
	// via the getter 0x2a8fec; if r0 != 1 at the return (0x2347a8) it clears bit 6 at
	// 0x2347b2 — a clear path independent of the service-channel array loop. Log what the
	// init actually sees, to tell whether service_ready is 1 at this instant (t~0.46).
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x002347a8)
	{
		static unsigned srchk_log = 0;
		if (srchk_log++ < 8)
			logerror("bit6_svcready_check: t=%.4f r0=%u ready[110c2c]=%02x  (bit6 cleared at 0x2347b2 unless r0==1)\n",
					machine().time().as_double(), m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
					debug_ram_byte(0x00110c2c));
	}

	// bit-6 service-channel clear probe (opt-in): the loop 0x23487e..0x2348a2 clears the
	// service-present bit 6 (0x11fed0 &= 0xbf) if any of 24 service-channel status bytes
	// [sb+i] (i != 11) is not "clean/absent" (0x00/0xfe/0xff). Log which entry trips it —
	// the real condition behind CONTACT SERVICE once checksum + service_ready are satisfied.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x0023487e)
	{
		const u32 idx = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 entry = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		const u8 v = debug_ram_byte(entry);
		const bool dirty = (v != 0x00 && v != 0xfe && v != 0xff && idx != 0x0b);
		static unsigned bit6_log = 0;
		if (dirty && bit6_log++ < 24)
			logerror("bit6_clear: t=%.4f idx=%u entry=%08x val=%02x  -> clears service-present bit6\n",
					machine().time().as_double(), idx, entry, v);
	}

	// Lower-service idle-byte probe (opt-in): at the idle test (0x2ad1e0) log the
	// individual busy/ready bytes so the exact stuck transport state is visible.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr && addr == 0x002b03e0)
	{
		static unsigned idle_log = 0;
		if (idle_log++ < 8)
			logerror("cs_idle: t=%.4f queue_block[110d30]=%02x tx_busy_d[10f4ac]=%02x "
					"tx_flags_a[10f4a8]=%02x queue_block4[110d34]=%02x ready_flags[111794]=%02x\n",
					machine().time().as_double(),
					debug_ram_byte(0x00110d30), debug_ram_byte(0x0010f4ac),
					debug_ram_byte(0x0010f4a8), debug_ram_byte(0x00110d34),
					debug_ram_byte(0x00111794));
	}

	// Startup readiness-predicate probe (opt-in): at each cmp in the mode-0x000d
	// readiness loop (0x2a92fc), log the predicate's result r0 so the one that
	// fails (returns 0) — the real boot blocker the watchdog symptomatizes — is
	// identified. Pure trace.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr &&
			(addr == 0x002a930e || addr == 0x002a9316 || addr == 0x002a931e ||
			 addr == 0x002a9326 || addr == 0x002a932e || addr == 0x002a9336 ||
			 addr == 0x002b03e0 || addr == 0x002b03e8))
	{
		static unsigned pred_log = 0;
		if (pred_log++ < 96)
		{
			const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			const char *name =
					addr == 0x002a930e ? "service_context_ready_2b03d8" :
					addr == 0x002a9316 ? "display_init_ready_2a2680" :
					addr == 0x002a931e ? "task14_ready_28ff14" :
					addr == 0x002a9326 ? "pred_2a6566" :
					addr == 0x002a932e ? "pred_2a0ec4" :
					addr == 0x002a9336 ? "pred_279282" :
					addr == 0x002b03e0 ? "  └ service_lower_idle_check_2ad1c8" :
					                     "  └ service_event_queue_empty_283dce";
			logerror("cs_pred: t=%.4f %-34s r0=%u %s\n", machine().time().as_double(),
					name, r0, r0 == 0 ? "<-- FAIL" : "");
		}
	}

	// Contact-service input probe (opt-in): logs every command the contact-service
	// task dispatches (0x237994, r0=command) and every frame-dispatch (0x23670c,
	// r0=frame ptr, frame[8]=command byte). Shows the complete set of messages the
	// task actually receives, to find whether anything besides the self-reposted
	// 0xd9 watchdog ever drives it. Pure trace.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr &&
			(addr == 0x00237994 || addr == 0x0023670c))
	{
		static unsigned disp_log = 0;
		if (disp_log++ < 120)
		{
			const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			if (addr == 0x00237994)
				logerror("cs_disp: t=%.4f cmd_dispatch r0=%04x lr=%08x mode=%04x\n",
						machine().time().as_double(), r0 & 0xffff, lr, debug_ram_word(FW_STARTUP_MODE));
			else
			{
				const uint8_t fcmd = (r0 >= 0x100000 && r0 < 0x180000) ? debug_ram_byte(r0 + 8) : 0xff;
				logerror("cs_disp: t=%.4f frame_dispatch frame=%08x frame[8]=%02x lr=%08x\n",
						machine().time().as_double(), r0, fcmd, lr);
			}
		}
	}

	// Contact-service commit probe (opt-in). Logs the first hits of the terminal
	// handler (0x2b4dda) and the D9 watchdog timeout builder (0x236dc4) with the
	// reason code, caller (LR), and contact-service state bytes, so the upstream
	// decision that dooms the boot can be located. Pure trace, no state change.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 && pc == addr &&
			(addr == 0x002b4dda || addr == 0x00236dc4))
	{
		static unsigned commit_log = 0;
		if (commit_log++ < 48)
		{
			const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			const u32 r1 = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("contact_commit: t=%.4f site=%s r0=%u r1=%u lr=%08x mode=%04x event=%04x cs_state=%02x cs_status=%02x cs_substate=%02x cs_ack=%02x cs_reason=%02x\n",
					machine().time().as_double(),
					addr == 0x002b4dda ? "TERMINAL" : "D9TIMEOUT",
					r0, r1, lr,
					debug_ram_word(FW_STARTUP_MODE), debug_ram_word(FW_STARTUP_EVENT),
					debug_ram_byte(FW_CONTACT_SERVICE_STATE), debug_ram_byte(FW_CONTACT_SERVICE_STATUS),
					debug_ram_byte(FW_CONTACT_SERVICE_SUBSTATE), debug_ram_byte(FW_CONTACT_SERVICE_ACK),
					debug_ram_byte(FW_CONTACT_SERVICE_REASON));
		}
	}

	// VBAT pipeline probe: at the sample generator (0x27cc74) log the live float

	if (nokia_env_u32("NOKI3210_SKIP_SERVICE_E2_REARM", 0) != 0 &&
			addr == 0x002697aa &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff) == 0x00e2 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xffff) == 0x0282 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x0021e00e &&
			debug_ram_word(FW_STARTUP_MODE) == 0x0007 &&
			debug_ram_word(FW_STARTUP_EVENT) == 0x0074 &&
			debug_ram_word(FW_CCONT_STATE) == 0x0500)
	{
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 1);
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R15,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1));
	}

	if (nokia_env_u32("NOKI3210_TRACE_SERVICE_TRANSPORT", 0) != 0 &&
			pc == addr &&
			(addr == 0x002b12dc || addr == 0x002b132e || addr == 0x002b1382 ||
			 addr == 0x002b1388 || addr == 0x002b1392 ||
			 addr == 0x002b13a2 || addr == 0x002b13d4))
	{
		const u32 frame = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		const uint16_t channel = (uint16_t(debug_ram_byte(frame + 8)) << 8) | debug_ram_byte(frame + 9);
		const u32 forced_service72_status = nokia_env_u32("NOKI3210_SERVICE72_RESPONSE_STATUS", 0xffffffff);
		if (addr == 0x002b1388 && forced_service72_status <= 0xffff &&
				(channel == 0x7206 || channel == 0x7207))
		{
			debug_ram_byte_w(frame + 0x0a, uint8_t(forced_service72_status >> 8));
			debug_ram_byte_w(frame + 0x0b, uint8_t(forced_service72_status));
		}
	}
		trace_state31_event_source(pc, addr, offset);

	const unsigned ccont_event15_delay = nokia_env_u32("NOKI3210_CCONT_EVENT15_DELAY", 0xffffffff);
	if (ccont_event15_delay != 0xffffffff &&
			pc >= 0x002b08fc && pc <= 0x002b0a12 &&
			(addr == 0x002b0a40 || addr == 0x002b0a42))
	{
		// Boot-research shim: override the ROM literal used for the delayed
		// CCONT/battery event15 record. This should become real timer/IRQ state.
		const uint16_t data = (addr == 0x002b0a40) ? ((ccont_event15_delay >> 16) & 0xffff) : (ccont_event15_delay & 0xffff);
		return data & mem_mask;
	}

	return std::nullopt;
}

uint16_t noki3310_state::flash_r(offs_t offset, uint16_t mem_mask)
{
	const u32 pc = m_maincpu->pc();
	const u32 addr = 0x00200000 + (offset << 1);
	if (const std::optional<uint16_t> ov = flash_firmware_hooks(offset, pc, addr, mem_mask))
		return *ov;
	return m_flash->read(offset) & mem_mask;
}

void noki3310_state::flash_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	static unsigned flash_write_log_count = 0;
	const u32 pc = m_maincpu->pc();

	if (flash_write_log_count < 200 || pc == 0x0026a648 || pc == 0x0026a64a)
	{
		flash_write_log_count++;
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
	// The ROM mirrors are also used by the firmware as a one-byte trace/status port.
}

void noki3310_state::serial_eeprom_start()
{
	m_serial_eeprom.write_shift = 0;
	m_serial_eeprom.write_bits = 0;
}

void noki3310_state::serial_eeprom_write_bit(uint8_t bit)
{
	m_serial_eeprom.write_shift = (m_serial_eeprom.write_shift << 1) | (bit & 1);
	m_serial_eeprom.write_bits++;
	if (m_serial_eeprom.write_bits == 8)
	{
		serial_eeprom_accept_byte(m_serial_eeprom.write_shift);
		m_serial_eeprom.write_shift = 0;
		m_serial_eeprom.write_bits = 0;
	}
}

void noki3310_state::serial_eeprom_accept_byte(uint8_t data)
{
	if (m_serial_eeprom.address_stage == 0 && (data & 0xf0) == 0xa0)
	{
		m_serial_eeprom.read_mode = BIT(data, 0);
		m_serial_eeprom.read_bits = 0;
		if (!m_serial_eeprom.read_mode)
		{
			m_serial_eeprom.address_temp = 0;
			m_serial_eeprom.address_stage = 1;
		}
		return;
	}

	if (!m_serial_eeprom.read_mode)
	{
		if (m_serial_eeprom.address_stage == 1)
		{
			m_serial_eeprom.address_temp = uint16_t(data) << 8;
			m_serial_eeprom.address_stage = 2;
		}
		else
		{
			m_serial_eeprom.address = uint16_t(m_serial_eeprom.address_temp | data);
			m_serial_eeprom.address_stage = 0;
			m_serial_eeprom.read_bits = 0;
		}
	}
}

void noki3310_state::serial_eeprom_clock_read_bit()
{
	if (!m_serial_eeprom.read_mode)
		return;

	if (m_serial_eeprom.read_bits == 0)
	{
		m_serial_eeprom.read_byte = serial_eeprom_byte(m_serial_eeprom.address);
		// EEPROM field-map probe (opt-in): log every byte address the firmware reads
		// and the value returned, to document which EEPROM regions matter.
		if (nokia_env_u32("NOKI3210_TRACE_EEPROM", 0) != 0)
		{
			static unsigned ee_log = 0;
			if (ee_log++ < 4000)
				logerror("eeprd: addr=%04x val=%02x t=%.4f\n",
						m_serial_eeprom.address, m_serial_eeprom.read_byte, machine().time().as_double());
		}
	}

	m_serial_eeprom.read_latched_bit = BIT(m_serial_eeprom.read_byte, 7 - m_serial_eeprom.read_bits);
	if (m_serial_eeprom.read_latched_bit)
		m_mad2_regs[0x20] |= 0x01;
	else
		m_mad2_regs[0x20] &= ~0x01;
	m_serial_eeprom.read_bits++;
	if (m_serial_eeprom.read_bits == 8)
	{
		m_serial_eeprom.read_bits = 0;
		m_serial_eeprom.address++;
	}
}

uint8_t noki3310_state::serial_eeprom_byte(uint16_t address) const
{
	if (const char *profile = std::getenv("NOKI3210_EEPROM_PROFILE"))
	{
		if (!std::strcmp(profile, "selftest"))
		{
			// The bundled EEPROM file is erased. This overlay supplies the
			// small set of NV defaults needed to expose later boot gates.
			// Offsets are annotated with their checksummed block (see the
			// FW_EEPROM_*_BLOCK_* map above and docs/eeprom_analysis.md).
			switch (address)
			{
				// --- config block [0x0120..0x0243], checksum at FW_EEPROM_CONFIG_BLOCK_CKSUM ---
				case 0x0170: return 0x01;
				case 0x0171: return 0x00;
				// Stored checksum (big-endian) for the config block, read at 0x234810.
				// Firmware computes sum16(EEPROM[0x120..0x243]) minus a correction
				// (EEPROM[0x154]+[0x155]) = 0x1ee1 for this overlay; high byte = 0x1e,
				// low = 0xe1. (NokTool's tune/security blocks use a plain sum16; this
				// firmware block additionally subtracts the [0x154] word.)
				case FW_EEPROM_CONFIG_BLOCK_CKSUM:     return 0x1e;
				case FW_EEPROM_CONFIG_BLOCK_CKSUM + 1: return 0xe1;
				// --- tune+security region [0x00..0x11b] checksum, verified by idx18's
				// service-channel availability check (0x264c56: sum16(EEPROM[0..0x11b]) ==
				// 32-bit word[0x11c], big-endian). The erased region (all 0xff over 0x11c
				// bytes) sums to 0x1ae4; store that big-endian at 0x11c..0x11f (these four
				// bytes are outside the summed range, so they don't change the sum). A
				// real provisioned phone has a matching checksum here; a virgin EEPROM
				// leaves 0xffff (mismatch) — which is why idx18 reads the service absent.
				case 0x011c: return 0x00;
				case 0x011d: return 0x00;
				case 0x011e: return 0x1a;
				case 0x011f: return 0xe4;
				// --- blocks beyond config (RF/ADC profile records, [0x0394+], [0x048c+]) ---
				case 0x048c: return 0x0a;
				case 0x048d: return 0x00;
				case 0x048e: return 0x0a;
				case 0x048f: return 0x80;
				case 0x0394: return 0x0a;
				case 0x0395: return 0x00;
				case 0x0396: return 0x0a;
				case 0x0397: return 0x80;
				case 0x0398: return 0x09;
				case 0x0399: return 0x00;
				case 0x039a: return 0x00;
				case 0x039b: return 0x00;
			}

			// ADC monitor calibration/config records read by 0x2a7230. Erased
			// 0xff bytes turn into invalid selector and weight tables at
			// 0x11145a/0x111d3c/0x111d5c, causing the live source walker to
			// accumulate implausible scores during startup mode 7.
			if ((address >= 0x02e0 && address <= 0x02eb) ||
					(address >= 0x0310 && address <= 0x0313) ||
					(address >= 0x0330 && address <= 0x0337))
				return 0x00;
		}
	}

	memory_region *eeprom = memregion("eeprom");
	if (!eeprom || eeprom->bytes() == 0)
		return 0xff;

	return eeprom->base()[address % eeprom->bytes()];
}

uint8_t noki3310_state::mad2_io_r(offs_t offset)
{
	uint8_t data = m_mad2_regs[offset];

	// Hardware-atlas breadth-first trace (opt-in): one line per distinct MAD2 I/O register
	// the firmware reads, with its description + first PC. Builds docs/hardware_atlas.md.
	if (nokia_env_u32("NOKI3210_TRACE_MMIO", 0) != 0)
	{
		static uint16_t seen[512] = {}; static unsigned n = 0;
		const uint16_t key = uint16_t(offset) | 0x8000;
		bool f = false; for (unsigned i = 0; i < n; i++) if (seen[i] == key) { f = true; break; }
		if (!f && n < 512) { seen[n++] = key;
			logerror("mmio: R mad2[%02x] pc=%08x t=%.4f  %s\n", unsigned(offset), m_maincpu->pc(),
					machine().time().as_double(), nokia_mad2_reg_desc(offset)); }
	}

	switch(offset)
	{
		case 0x00:
			data = 0x40;    // ASIC version
			break;
		case 0x04:
			data = m_timer1_counter >> 8;
			break;
		case 0x05:
			data = m_timer1_counter;
			break;
		case 0x06:
			data = (m_timer1_counter + 0x40) >> 8;
			break;
		case 0x07:
			data = m_timer1_counter + 0x40;
			break;
		case 0x08:
			data = m_fiq_status & 0xff;
			break;
		case 0x09:
			data = m_irq_status & 0xff;
			break;
		case 0x0c:
			data = (data & (~0x20)) | ((m_irq_status >> 3) & 0x20);
			break;
		case 0x10:
			data = m_timer0_counter >> 8;
			break;
		case 0x11:
			data = m_timer0_counter;
			break;
		case 0x16:
			data = (data & (~0x02)) | ((m_fiq_status >> 7) & 0x02);
			break;
		case 0x18:
			data &= 0x7f;
			break;
		case 0x19:
			data |= 0xc0;
			break;
		case 0x2a:
			data = 0xff;
			for(int i=0; i<5; i++)
				if (!(m_mad2_regs[0x28] & (1 <<i)))
					data &= m_keypad[i]->read() | 0xe0;

			data &= m_pwr->read() | 0xe0;
			{
				uint8_t synth_row = 0xff;
				uint8_t synth_mask = 0xff;
				const bool synth_active = synthetic_key_active(synth_row, synth_mask);
				const bool synth_selected = synth_active && (synth_row == 0xff || !(m_mad2_regs[0x28] & (1 << synth_row)));
				if (synth_active && !synth_selected)
				{
				}
				if (synth_selected)
				{
					data &= uint8_t(~synth_mask) | 0xe0;
				}
			}

			if (m_power_on)
			{
				data &= m_power_on;
				m_power_on = 0;
			}
			if (nokia_env_u32("NOKI3210_HOLD_POWER_KEY", 0) != 0)
				data &= 0xfe;
			break;
		case 0x37:  // SIM UART RxD
			if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_RXD", 0xff) & 0xff;
			break;
		case 0x38:  // SIM UART interrupt identification
			if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_IIR", 0x01) & 0xff;
			break;
		case 0x3c:  // SIM UART RxD queue fill
			if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_RX_FILL", 0x00) & 0xff;
			break;
		case 0x3d:  // SIM RxD flags
			if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_RX_FLAGS", 0x00) & 0xff;
			break;
		case 0x3e:  // SIM TxD flags
			if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_TX_FLAGS", 0x20) & 0xff;
			break;
		case 0x3f:  // SIM UART TxD queue fill
			if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_TX_FILL", 0x00) & 0xff;
			break;
		case 0x6c:
			data = nokia_ccont_r();
			break;
		case 0x6d:
			data = 0x07;    // GENSIO ready
			break;
	}

	const u32 pc = m_maincpu->pc();
	if (offset == 0x20 && pc >= 0x002b0188 && pc <= 0x002b0238)
	{
		// The EEPROM acknowledges by pulling SDA low after a byte write.
		data &= 0xfe;
	}
	else if (offset == 0x20 && pc >= 0x002b024a && pc <= 0x002b0288)
	{
		data = (data & 0xfe) | (m_serial_eeprom.read_latched_bit & 1);
	}

	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x = %02x %s\n", offset, data, nokia_mad2_reg_desc(offset));
	return data;
}

void noki3310_state::mad2_io_w(offs_t offset, uint8_t data)
{
	uint8_t old_data = m_mad2_regs[offset];
	const u32 pc = m_maincpu->pc();
	m_mad2_regs[offset] = data;

	if (nokia_env_u32("NOKI3210_TRACE_MMIO", 0) != 0)
	{
		static uint16_t seen[512] = {}; static unsigned n = 0;
		const uint16_t key = uint16_t(offset);
		bool f = false; for (unsigned i = 0; i < n; i++) if (seen[i] == key) { f = true; break; }
		if (!f && n < 512) { seen[n++] = key;
			logerror("mmio: W mad2[%02x] pc=%08x t=%.4f  %s\n", unsigned(offset), pc,
					machine().time().as_double(), nokia_mad2_reg_desc(offset)); }
	}

	// MBUS transmit/control probe (opt-in): logs writes to the MAD2 MBUS
	// control/status/data registers so the outbound D9 service path can be
	// correlated against the contact-service watchdog window.
	if (nokia_env_u32("NOKI3210_TRACE_CONTACT_COMMIT", 0) != 0 &&
			(offset == 0x18 || offset == 0x19 || offset == 0x1a))
	{
		static unsigned mbusw_log = 0;
		if (mbusw_log++ < 80)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			logerror("mbus_w: t=%.4f reg=%02x data=%02x pc=%08x lr=%08x\n",
					machine().time().as_double(), offset, data, pc, lr);
		}
	}

	// EEPROM I2C activity probe (opt-in): log distinct PCs writing the MAD2 EEPROM
	// register (0x20), to see whether/where the firmware actually drives the bus.
	if (nokia_env_u32("NOKI3210_TRACE_EEPROM", 0) != 0 && offset == 0x20)
	{
		static uint32_t seen[64] = {}; static unsigned nseen = 0;
		bool found = false;
		for (unsigned i = 0; i < nseen; i++) if (seen[i] == pc) { found = true; break; }
		if (!found && nseen < 64)
		{
			seen[nseen++] = pc;
			logerror("eepi2c_pc: pc=%08x data=%02x t=%.4f sda=%d\n", pc, data, machine().time().as_double(),
					m_serial_eeprom.read_mode ? 1 : 0);
		}
	}

	if (offset == 0x20 && pc >= 0x002b0318 && pc <= 0x002b0340)
		serial_eeprom_start();
	else if (offset == 0x20 && pc >= 0x002b01ac && pc <= 0x002b01c8)
		serial_eeprom_write_bit(data & 1);
	else if (offset == 0x20 && pc == 0x002b028e)
		serial_eeprom_clock_read_bit();

	if (offset == 0x01 && (data & 0x04) != 0 && (old_data & 0x04) == 0 &&
			nokia_env_u32("NOKI3210_MAD2_SOFT_RESET", 0) != 0)
	{
		m_timer_mad2_soft_reset->adjust(attotime::zero, data);
		return;
	}

	switch(offset)
	{
		case 0x02:
			//printf("DSP %s\n", data & 1 ? "RUN" : "HOLD");
			//if (data & 0x01)  machine().debug_break();
			break;
		case 0x08:
			ack_fiq(data);
			break;
		case 0x09:
			ack_irq(data);
			break;
		case 0x0a:
			update_fiq_line();
			break;
		case 0x0b:
			update_irq_line();
			break;
		case 0x0c:
			ack_irq((data << 3) & 0x100);
			update_fiq_line();
			update_irq_line();
			break;
		case 0x0f:
			m_timer0_divider = data;
			break;
		case 0x12:
			m_timer0_compare_latched = false;
			break;
		case 0x13:
			m_timer0_compare_latched = false;
			if (nokia_env_u32("NOKI3210_TIMER0_CATCHUP", 0) != 0 ||
					m_timer0_counter == ((uint16_t(m_mad2_regs[0x12]) << 8) | m_mad2_regs[0x13]))
				update_timer0_compare();
			break;
		case 0x16:
			ack_fiq((data << 7) & 0x100);
			update_fiq_line();
			break;
		case 0x18:
			if (data & 0x20)
				schedule_mbus_fiq(2);
			else if ((old_data & 0x40) && !(data & 0x40))
				schedule_mbus_fiq(2);
			break;
		case 0x19:
			if ((data & 0x80) && !(old_data & 0x80))
				schedule_mbus_fiq(3);
			break;
		case 0x1a:
			// Byte written to the MBUS TX register; the controller raises the
			// TX-byte-sent FIQ. (No bus peer answers: the D0 lower-service reply
			// is superseded by MODEL_DSP_SERVICE and task 08 is never resumed.)
			schedule_mbus_fiq(2);
			break;
		case 0x2c:
			nokia_ccont_w(data);
			break;
		case 0x2e:
		case 0x6e:
		{
			static unsigned lcd_cmd_count = 0;
			static unsigned lcd_data_count = 0;
			static unsigned lcd_data_non_ff_count = 0;
			static unsigned lcd_mirror_dump_count = 0;
			static uint8_t lcd_mirror_vram[6 * 84] = { };
			static uint8_t lcd_mirror_mode = 0x04;
			static uint8_t lcd_mirror_control = 0x00;
			static uint8_t lcd_mirror_x = 0;
			static uint8_t lcd_mirror_y = 0;
			const bool lcd_data = !(offset & 0x40);
			const uint8_t old_lcd_mirror_x = lcd_mirror_x;
			const uint8_t old_lcd_mirror_y = lcd_mirror_y;
			if (lcd_data)
			{
				lcd_data_count++;
				if (data != 0xff)
					lcd_data_non_ff_count++;

				lcd_mirror_vram[lcd_mirror_y * 84 + lcd_mirror_x] = data;
				if (lcd_mirror_mode & 0x02)
				{
					lcd_mirror_y++;
					if (lcd_mirror_y > 5)
					{
						lcd_mirror_y = 0;
						lcd_mirror_x = (lcd_mirror_x + 1) % 84;
					}
				}
				else
				{
					lcd_mirror_x++;
					if (lcd_mirror_x > 83)
					{
						lcd_mirror_x = 0;
						lcd_mirror_y = (lcd_mirror_y + 1) % 6;
					}
				}
			}
			else
			{
				lcd_cmd_count++;
				if (lcd_mirror_mode & 0x01)
				{
					if (data & 0x20)
						lcd_mirror_mode = data & 0x07;
				}
				else
				{
					if (data & 0x80)
						lcd_mirror_x = (data & 0x7f) % 84;
					else if (data & 0x40)
						lcd_mirror_y = data & 0x07;
					else if (data & 0x20)
						lcd_mirror_mode = data & 0x07;
					else if (data & 0x08)
						lcd_mirror_control = ((data & 0x04) >> 1) | (data & 0x01);
				}
			}
			unsigned lcd_mirror_zero = 0;
			unsigned lcd_mirror_ff = 0;
			unsigned lcd_mirror_other = 0;
			for (uint8_t mirror_byte : lcd_mirror_vram)
			{
				if (mirror_byte == 0x00)
					lcd_mirror_zero++;
				else if (mirror_byte == 0xff)
					lcd_mirror_ff++;
				else
					lcd_mirror_other++;
			}
			if (lcd_data && old_lcd_mirror_x == 83 && old_lcd_mirror_y == 5 && lcd_mirror_x == 0 && lcd_mirror_y == 0)
			{
				lcd_mirror_dump_count++;
				const char *snapshot_dir = std::getenv("NOKI3210_SNAPSHOT_DIR");
				if (!snapshot_dir || !*snapshot_dir)
					snapshot_dir = ".";

				char filename[512];
				std::snprintf(filename, sizeof(filename), "%s/noki3210_lcdmirror_%04u_z%03u_ff%03u_o%03u.pgm",
						snapshot_dir,
						lcd_mirror_dump_count,
						lcd_mirror_zero,
						lcd_mirror_ff,
						lcd_mirror_other);

				if (FILE *file = std::fopen(filename, "wb"))
				{
					std::fprintf(file, "P5\n84 48\n255\n");
					for (unsigned y = 0; y < 48; y++)
					{
						const unsigned row = y >> 3;
						const unsigned bit = y & 7;
						for (unsigned x = 0; x < 84; x++)
						{
							unsigned on = BIT(lcd_mirror_vram[row * 84 + x], bit);
							if (lcd_mirror_control & 0x01)
								on ^= 1;
							const uint8_t pixel = on ? 0x00 : 0xff;
							std::fwrite(&pixel, 1, 1, file);
						}
					}
					std::fclose(file);
				}
			}
			m_pcd8544->dc_w(lcd_data ? ASSERT_LINE : CLEAR_LINE);
			for (int i=7; i>=0; i--)
			{
				m_pcd8544->sclk_w(CLEAR_LINE);
				m_pcd8544->sdin_w(BIT(data, i));
				m_pcd8544->sclk_w(ASSERT_LINE);
			}
			m_pcd8544->dc_w(ASSERT_LINE);
			break;
		}
	}

	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x %s\n", offset, data, nokia_mad2_reg_desc(offset));
}

uint8_t noki3310_state::mad2_dspif_r(offs_t offset)
{
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x DSPIF\n", offset);
	if (nokia_env_u32("NOKI3210_TRACE_MMIO", 0) != 0)
		logerror("mmio: R DSPIF[%x] pc=%08x t=%.4f  (STUB -> 0)\n", unsigned(offset), m_maincpu->pc(), machine().time().as_double());
	return 0;
}

void noki3310_state::mad2_dspif_w(offs_t offset, uint8_t data)
{
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x DSPIF\n", offset, data);
	if (nokia_env_u32("NOKI3210_TRACE_MMIO", 0) != 0)
		logerror("mmio: W DSPIF[%x]=%02x pc=%08x t=%.4f  (STUB)\n", unsigned(offset), data, m_maincpu->pc(), machine().time().as_double());
}

uint8_t noki3310_state::mad2_mcuif_r(offs_t offset)
{
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x MCUIF\n", offset);
	if (nokia_env_u32("NOKI3210_TRACE_MMIO", 0) != 0)
		logerror("mmio: R MCUIF[%x] pc=%08x t=%.4f  (STUB -> 0)\n", unsigned(offset), m_maincpu->pc(), machine().time().as_double());
	return 0;
}

void noki3310_state::mad2_mcuif_w(offs_t offset, uint8_t data)
{
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x MCUIF\n", offset, data);
	if (nokia_env_u32("NOKI3210_TRACE_MMIO", 0) != 0)
		logerror("mmio: W MCUIF[%x]=%02x pc=%08x t=%.4f  (STUB)\n", unsigned(offset), data, m_maincpu->pc(), machine().time().as_double());
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
	if (!newval)    // TODO: COL/ROW IRQ mask
		assert_irq(6);
}

static INPUT_PORTS_START( noki3310 )
	PORT_START("COL.0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_UP)       PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_0)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_DEL)      PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_DOWN)     PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_2)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_1)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_6)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_5)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_4)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_9)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_8)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_7)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("COL.4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_3)        PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_MINUS)    PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_ENTER)    PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)

	PORT_START("PWR")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_CODE(KEYCODE_SPACE)    PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)
	PORT_BIT( 0x1e, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

void noki3310_state::noki3310(machine_config &config)
{
	/* basic machine hardware */
	ARM7_BE(config, m_maincpu, 26000000 / 2);  // MAD2WD1 13 MHz, clock internally supplied to ARM core can be divided by 2, in sleep mode a 32768 Hz clock is used
	m_maincpu->set_addrmap(AS_PROGRAM, &noki3310_state::noki3310_map);

	/* video hardware */
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_LCD, rgb_t::white()));
	screen.set_refresh_hz(60);
	screen.set_vblank_time(ATTOSECONDS_IN_USEC(2500) /* not accurate */);
	screen.set_size(84, 48);
	screen.set_visarea(0, 84-1, 0, 48-1);
	screen.set_screen_update("pcd8544", FUNC(pcd8544_device::screen_update));
	screen.set_palette("palette");

	PALETTE(config, "palette", palette_device::MONOCHROME_INVERTED);

	PCD8544(config, m_pcd8544);
	m_pcd8544->set_screen_update_cb(FUNC(noki3310_state::pcd8544_screen_update));

	INTEL_TE28F160(config, "flash");
}

void noki3310_state::noki3330(machine_config &config)
{
	noki3310(config);

	INTEL_TE28F320(config.replace(), "flash");
}

void noki3310_state::noki3410(machine_config &config)
{
	noki3330(config);

	subdevice<screen_device>("screen")->set_size(96, 65);    // Philips OM6206
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
	ROMX_LOAD("3210f600a.fls", 0x000000, 0x200000, CRC(6a978478) SHA1(6bdec2ec76aca15bc12b621be4402e455562454b), ROM_BIOS(0))

	ROM_REGION16_BE(0x04000, "eeprom", 0 )
	ROM_LOAD("3210 virgin eeprom,24c128.bin", 0x00000, 0x04000, CRC(690b37d3) SHA1(547372f1044a3442aa52fcd2b3546540aba59344))
ROM_END

ROM_START( noki3310 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x200000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "607", "v6.07")  // C 17-06-2003
	ROM_SYSTEM_BIOS(1, "579", "v5.79")  // N 11-11-2002
	ROM_SYSTEM_BIOS(2, "513", "v5.13")  // C 11-01-2002
	ROMX_LOAD("3310_607_ppm_c.fls", 0x000000, 0x200000, CRC(5743f6ba) SHA1(0e80b5f1698909c9850be770c1289566582aa77a), ROM_BIOS(0))
	ROMX_LOAD("3310 nr1 v5.79.fls", 0x000000, 0x200000, CRC(26b4f0df) SHA1(649de05ed88205a080693b918cd1295ac691dff1), ROM_BIOS(1))
	ROMX_LOAD("3310 v. 5.13 c.fls", 0x000000, 0x1d0000, CRC(0f66d256) SHA1(04d8dabe2c454d6a1161f352d85c69c409895000), ROM_BIOS(2))
	ROM_LOAD("3310 virgin eeprom 003d0000.fls", 0x1d0000, 0x030000, CRC(8393b1f7) SHA1(ab6c05bfa54ecd7c2acbd172009ffe6c7f130cb8))

	// these 2 are apparently the 6.39 update firmware data
	ROM_REGION(0x0200000, "misc", 0 )
	ROM_LOAD( "nhm5ny06.390",   0x000000, 0x0131161, CRC(5dfb1af7) SHA1(3a8ad82dc239b0cd18be60f537c4d0e07881155d) )
	ROM_LOAD( "nhm5ny06.39i",   0x000000, 0x0090288, CRC(ec214ee4) SHA1(f5b3b9ceaa7280d5246dd70d5696f8f6983122fc) )
ROM_END

ROM_START( noki3330 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "450", "v4.50")  // C 12-10-2001
	ROMX_LOAD("3330f450c.fls", 0x000000, 0x350000, CRC(259313e7) SHA1(88bcc39d9358fd8a8562fe3a0280f0ce82f5897f), ROM_BIOS(0))
	ROM_LOAD("3330 virgin eeprom 005f0000.fls", 0x3f0000, 0x010000, CRC(23459c10) SHA1(68481effb39d90a1639e8f261009c66e97d3e668))
ROM_END

ROM_START( noki3410 )
	MAD2_INTERNAL_ROMS

	ROM_REGION16_BE(0x0400000, "flash", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "506", "v5.06")  // C 29-11-2002
	ROMX_LOAD("3410_5-06c.fls", 0x000000, 0x370000, CRC(1483e094) SHA1(ef26026297c779de7b01923a364ded822e720c38), ROM_BIOS(0))
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
SYST( 1999, noki3210, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3210", MACHINE_NO_SOUND )
SYST( 1999, noki7110, 0,      0,      noki7110, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 7110", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8210, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8850, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8850", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki3310, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3310", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6210, 0,      0,      noki6210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 6210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6250, 0,      0,      noki6210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 6250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8250, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8890, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8890", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2001, noki3330, 0,      0,      noki3330, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3330", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki3410, 0,      0,      noki3410, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3410", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki5210, 0,      0,      noki3330, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 5210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
