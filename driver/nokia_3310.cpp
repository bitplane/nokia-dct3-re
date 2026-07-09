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
	bool          m_startup_latch_complete_seen;
	bool          m_after_mad2_soft_reset;

	// Node-0x18 service-responder trampoline state (NOKI3210_MODEL_SVC_RESPONDER).
	unsigned      m_svcresp_state;      // 0 idle, 1 await-alloc, 2 await-post, 3 done
	uint32_t      m_svcresp_saved[16];  // R0..R14 + CPSR saved at the trigger point
	uint32_t      m_svcresp_msg;        // allocated message pointer
	// Resource-enable trampoline state (NOKI3210_MODEL_RES_ENABLE): delivers contact-service
	// command 0x70 (channel-map / resource enable) with a config blob so the firmware's own
	// 0x2b140a registers the resource-availability bitmap [0x11ff08] + enable [0x11fee4].
	unsigned      m_resen_state = 0;    // 0 idle, 1 await-alloc, 2 await-post, 3 done
	uint32_t      m_resen_saved[16];    // R0..R14 + CPSR saved at the trigger point
	uint32_t      m_resen_msg = 0;      // allocated message pointer
	// MODEL_STARTUP_REPORTS: inject the subsystem-ready reports (code 7 + the mode-4 6-message
	// checklist codes) into task-1's mailbox via the firmware's own post 0x26a354, as the real
	// subsystems would. State machine driving one 0x26a354(1,code) call per report code.
	unsigned      m_reports_state = 0;  // 0 idle, 1 armed, 2 posting, 3 done
	unsigned      m_reports_idx = 0;    // index into the report-code list
	uint32_t      m_reports_saved[16];  // R0..R14 + CPSR saved at the trigger point
	// SIM ATR FIFO (NOKI3210_MODEL_SIM_ATR): register-level ATR delivery on SIM activation.
	uint8_t       m_sim_atr[40];
	uint8_t       m_sim_atr_len = 0;
	uint8_t       m_sim_atr_pos = 0;
	uint8_t       m_sim_last_ins = 0;    // FS responder: INS of the last APDU the phone sent (0x2aec34)
	uint8_t       m_sim_last_cmd[16] = {0}; // FS responder: full bytes of the last command the phone sent
	uint8_t       m_sim_last_cmdlen = 0; // FS responder: length of m_sim_last_cmd
	uint8_t       m_sim_card_phase = 0;  // MODEL_SIM_CARD: 0=await ATR, 1=post-ATR (PPS/data)
	uint32_t      m_sim_card_recv = 0;   // MODEL_SIM_CARD: SIM-task recv count (for ATR delivery timing)
	bool          m_sim_card_pending = false; // MODEL_SIM_CARD: a command awaits a response (set at 0x2aec34)
	uint8_t       m_sim_card_step = 0;   // MODEL_SIM_CARD: EF-read T=0 step (0=SELECT,1=GET_RESPONSE,2=READ,3=done)
	uint8_t       m_battery_startup_event_step;
	uint8_t       m_battery_startup_event_step_mode9;
	bool          m_post_charger_sequence_entered;
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

// ============================================================================
// NOKI3210_* environment knobs — the driver reads every runtime option from the
// environment (overridable on the `make run` command line). Three kinds:
//
//   1. HARDWARE CONFIG — selects a hardware *scenario*, not firmware behaviour:
//      display variant (DISPLAY_TYPE), power/ADC/battery (ADC_PROFILE,
//      BATTERY_PROFILE, POWER_IRQ_*, CCONT_EVENT15_DELAY, DISABLE_CCONT_WATCHDOG),
//      clocks (TIMER0_HZ/TIMER1_HZ/TIMER0_CATCHUP, FIQ8_HZ), NV (EEPROM_PROFILE),
//      SIM UART (SIM_PROFILE), reset (MAD2_SOFT_RESET*). The default boot (none set)
//      reproduces the CONTACT SERVICE oracle frame byte-for-byte.
//
//   2. FAITHFUL BOOT MODELS (MODEL_*) — opt-in, each reproduces one real subsystem's
//      handshake that a blank/peerless boot can't perform, driving the firmware's OWN
//      code (no result-forcing); all oracle-preserving. The stack that reaches the
//      "Insert SIM card" home screen: MODEL_DSP_SERVICE, MODEL_CCONT_PRESENT,
//      MODEL_SVC_RESPONDER, MODEL_SVC_CHANNEL_DRAIN, MODEL_SIM_CARD (+ MODEL_SIM_ATR,
//      the register-level ATR variant), and MODEL_RES_ENABLE (display-resource
//      registration groundwork for operator-idle). See docs/service_bootstrap.md,
//      docs/boot_to_insert_sim.md, docs/sim_emulator_scope.md.
//
//   3. DIAGNOSTIC TAPS (TRACE_*) — opt-in, log-only, no state change. A curated few:
//      TRACE_CCONT_READ (power/ADC), TRACE_LIMP/TRACE_LIMP2 (the startup limp),
//      TRACE_CSCMD (contact-service command stream), TRACE_RESAVAIL (display-resource
//      availability), TRACE_DSPDRV (entries into the GSM-L1/audio DSP driver layer),
//      TRACE_DSPIO (MCU<->DSP shared-RAM + DSPIF access map; docs/dsp_interface.md) --
//      the network/DSP frontier (docs/network_scouting.md), TRACE_HANDOFF (task-1 master
//      sequencer mode + startup checklist; the post-SIM interactive handoff,
//      docs/interactive_handoff.md). The RE forcing shims and
//      one-off traces have been retired (docs/removed_forcing_knobs.md).
//
// The forcing/model logic is quarantined in flash_firmware_hooks / ram_*_firmware_*
// (banner'd "NOT hardware behaviour"); see docs/driver_structure.md.
// ============================================================================
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
	m_startup_latch_complete_seen = false;
	m_after_mad2_soft_reset = false;
	m_svcresp_state = 0;
	m_svcresp_msg = 0;
	m_resen_state = 0;
	m_resen_msg = 0;
	m_reports_state = 0;
	m_reports_idx = 0;
	m_battery_startup_event_step = 0;
	m_battery_startup_event_step_mode9 = 0;
	m_post_charger_sequence_entered = false;
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

	if (pc >= 0x002b1e80 && pc <= 0x002b1f22 && address >= 0x11fc80 && address <= 0x11fc90)
	{
		// Boot-research shim: force the firmware-selected display type while
		// the real board/NV source for this byte is still unidentified.
		const unsigned display_type = nokia_env_u32("NOKI3210_DISPLAY_TYPE", 0xff) & 0xff;
	if (display_type != 0xff && address == 0x11fc86 && mem_mask == 0x00ff)
		data = (data & 0xff00) | display_type;
	}

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

	auto ram_word = [this](offs_t addr) -> uint16_t
	{
		if (addr < 0x100000 || addr >= 0x180000)
			return 0xffff;
		return m_ram[(addr - 0x100000) >> 1];
	};
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
	if (nokia_env_u32("NOKI3210_CONTACT_DA_PRESERVE_READY_BIT", 0) != 0 &&
			address == FW_CONTACT_SERVICE_STATUS &&
			mem_mask == 0x00ff &&
			(pc == 0x00237b04 || pc == 0x00237b0c) &&
			(m_ram[offset] & 0x0040) != 0 &&
			(data & 0x0040) == 0)
	{
		data |= 0x0040;
	}

	COMBINE_DATA(&m_ram[offset]);

	if (!m_startup_latch_complete_seen && address == 0x112398 && ((m_ram[offset] & 0x00ff) == 0x000f))
	{
		m_startup_latch_complete_seen = true;
		m_startup_latch_complete_time = machine().time();
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
	m_timer_dsp_service->adjust(attotime::from_msec(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_TICK_MS", 5)));
}

// TRACE_DSPIO (opt-in): map the MCU<->DSP interface. Logs first-touch of each distinct
// DSP shared-RAM (0x10000) offset and DSPIF (0x30000) register the firmware accesses, with
// direction, value, and the accessing PC. Byte offsets are shown (halfword*2). See docs/dsp_interface.md.
static bool dsp_io_first_touch(char kindc, unsigned byte_off, char dir, u32 pc)
{
	// Dedup per distinct (kind, dir, offset, PC) so every call site is shown once.
	static std::unordered_map<uint64_t, bool> seen;
	const uint64_t key = (uint64_t(unsigned(kindc)) << 56) | (uint64_t(unsigned(dir)) << 48)
			| (uint64_t(byte_off & 0xffff) << 32) | pc;
	static unsigned n = 0;
	if (seen[key] || n >= 600) return false;
	seen[key] = true; n++;
	return true;
}

uint16_t noki3310_state::dsp_ram_r(offs_t offset)
{
	if (nokia_env_u32("NOKI3210_TRACE_DSPIO", 0) && dsp_io_first_touch('s', offset << 1, 'R', m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1)))
		logerror("dspio: shram R [%03x] = %04x  pc=%08x\n", offset << 1, m_dsp_ram[offset & 0x7ff],
				m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1));

	// HACK: avoid hangs when ARM try to communicate with the DSP
	if (offset <= 0x004 >> 1)   return 0x01;
	if (offset == 0x0e0 >> 1)   return 0x00;
	if (offset == 0x0fe >> 1)   return 0x01;
	if (offset == 0x100 >> 1)   return 0x01;

	return m_dsp_ram[offset & 0x7ff];
}

void noki3310_state::dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_dsp_ram[offset & 0x7ff]);

	if (nokia_env_u32("NOKI3210_TRACE_DSPIO", 0) && dsp_io_first_touch('s', offset << 1, 'W', m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1)))
		logerror("dspio: shram W [%03x] = %04x  pc=%08x\n", offset << 1, m_dsp_ram[offset & 0x7ff],
				m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1));

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
				machine().time().as_double() >= nokia_env_u32("NOKI3210_SVC_RESPONDER_DELAY_MS", 450) / 1000.0 &&
				// When RES_ENABLE is on, let it deliver cmd 0x70 FIRST (the enable command must be
				// processed before the 0x64 completion, which ends the contact-service command loop).
				(nokia_env_u32("NOKI3210_MODEL_RES_ENABLE", 0) == 0 || m_resen_state == 3))
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

	// MODEL: resource-enable / channel-map command 0x70 (opt-in, NOKI3210_MODEL_RES_ENABLE).
	// The display/idle draw acquires resources through 0x2b12b4, which needs the enable flag
	// [0x11fee4] set AND the per-class bit in the availability bitmap [0x11ff08]. Both are
	// registered only by resource_system_init 0x2b140a, which fires from the contact-service
	// command handler 0x23670c on command byte [msg+8]==0x70 (config blob = msg+9, 0x40 bytes;
	// enable value read from [0x11fedd]). On our faked session only cmd 0x64 is delivered, so
	// 0x70 never arrives (TRACE_CSCMD). This model synthesises it the same faithful way as
	// MODEL_SVC_RESPONDER: drive the firmware's own alloc 0x26afe0 -> fill -> post 0x26a204,
	// seeding the enable-param struct [0x11fedd/de/df] the handler reads (never written on our
	// boot; on a real phone a prior channel-setup command sets it). Delivered once, after the
	// 0x64 completion. The config blob defaults to all-0xff = "all resource classes available".
	if (nokia_env_u32("NOKI3210_MODEL_RES_ENABLE", 0) != 0 && pc == addr)
	{
		constexpr u32 SENT2 = 0x003ff100;    // distinct flash return sentinel (SVC_RESPONDER uses 0x3ff000)
		constexpr uint16_t BX_R12 = 0x4760;
		auto setr = [&](int r, u32 v){ m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0 + r, v); };
		auto getr = [&](int r){ return u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0 + r)); };
		const u32 msgsz = nokia_env_u32("NOKI3210_RES_ENABLE_MSGSZ", 0x50);   // >= 9 + 0x40 config blob

		// Seed the enable value [0x11fedd] right at the enable-handler entry 0x2366d4, immediately
		// before it reads it (0x2366ec) and passes it as the enable arg to 0x2b140a. A post-time seed
		// does not survive to here (the enable-param struct is only-read on our boot; on a real phone
		// a prior channel-setup command sets it). Any nonzero value -> [0x11fee4] enable set.
		if (addr == 0x002366d4)
			debug_ram_byte_w(0x0011fedd, uint8_t(nokia_env_u32("NOKI3210_RES_ENABLE_VAL", 1)));

		if (m_resen_state == 0 && addr == 0x00237bc6 &&
				machine().time().as_double() >= nokia_env_u32("NOKI3210_RES_ENABLE_MS", 440) / 1000.0)
		{
			for (int i = 0; i < 15; i++) m_resen_saved[i] = getr(i);
			m_resen_saved[15] = m_maincpu->state_int(arm7_cpu_device::ARM7_CPSR);
			setr(0, msgsz);                    // alloc size
			setr(14, SENT2 | 1);               // LR -> sentinel
			setr(12, 0x0026afe0 | 1);          // r12 -> alloc
			m_resen_state = 1;
			logerror("resen: trigger task=%02x t=%.4f -> alloc(%#x)\n",
					debug_ram_byte(0x00100022), machine().time().as_double(), msgsz);
			return BX_R12;
		}
		if (m_resen_state == 1 && addr == SENT2)
		{
			const u32 msg = getr(0);
			if (msg >= 0x00100000 && msg < 0x00180000)
			{
				for (u32 i = 0; i < msgsz; i++) debug_ram_byte_w(msg + i, 0);
				debug_ram_byte_w(msg + 3, 0x40);   // -> 0x237400 dispatch route
				debug_ram_byte_w(msg + 5, 0x50);   // [msg+5] must be > 0x42 (enable gate at 0x2366cc)
				debug_ram_byte_w(msg + 8, 0x70);   // -> channel-map handler 0x23670c, ENABLE branch
				// config blob msg+9 .. msg+0x49 (0x40 bytes): first 0x20 -> bitmap [0x11ff08],
				// second 0x20 -> [0x11fee8]. bit(class&7) of blob[class>>3] marks class available.
				// DEFAULT is SPARSE: register only the idle-draw resource class 0x22 (bit2 of byte4).
				// Enabling ALL classes (RES_ENABLE_FILL=0xff) is unfaithful and blanks the display --
				// the firmware acquires resource-classes with no real provider. A faithful blob is the
				// real 0x40 provisioned bytes (not obtainable here); this sparse default is the minimal
				// stab that keeps the display alive while satisfying the idle-draw availability check.
				const unsigned fillenv = nokia_env_u32("NOKI3210_RES_ENABLE_FILL", 0x100);   // 0x100 = sparse
				if (fillenv <= 0xff)
					for (u32 i = 0; i < 0x40; i++) debug_ram_byte_w(msg + 9 + i, uint8_t(fillenv));
				else
				{
					// SPARSE (safe default): register only the display resource classes that have ROM
					// resource definitions in table 0x2e0a50 -- 0x4c (idle window 0x4c22), 0x4f, 0x50,
					// 0x52, 0x56. These are ROM-backed, so marking them available is safe (display stays
					// alive). The availability bit for class C is the PERMUTED mask romtable[C&7] @0x2e2f5c
					// = {0x40,0x80,0x10,0x20,0x04,0x08,0x01,0x02}; blob[C>>3] |= romtable[C&7].
					// 0x4c->rt[4]=0x04, 0x4f->rt[7]=0x02 => byte9=0x06; 0x50->0x40,0x52->0x10,0x56->0x01
					// => byteA=0x51. NB: registering the OTHER ~13 idle-content classes the draw queries
					// (0x22/0x25/0x26/0x27/0x2a/0x2b/0x30/0x31/0x3a/0x3c/0x3d/0x44/0x4a/0x5c/0x5d/0x5e/0x78)
					// has NO ROM backing -> the render then fails (blank); see docs/sim_emulator_scope.md.
					debug_ram_byte_w(msg + 9 + 9, 0x06);   // byte9: classes 0x4c (idle window) + 0x4f
					debug_ram_byte_w(msg + 9 + 0xa, 0x51); // byteA: classes 0x50 + 0x52 + 0x56
				}
				// Seed the enable-param struct the handler reads (only-read, never-written on our
				// boot): [0x11fedd]=enable value (any nonzero -> [0x11fee4]); [de]/[df] secondary.
				debug_ram_byte_w(0x0011fedd, uint8_t(nokia_env_u32("NOKI3210_RES_ENABLE_VAL", 1)));
				debug_ram_byte_w(0x0011fede, 0);
				debug_ram_byte_w(0x0011fedf, 0);
				// The command dispatcher 0x237400 gates all non-0x64 commands on service-ready
				// [0x11fed1] bit0 (0x237426: skips to 0x237894 if clear). On our boot it is 0 (0xcc),
				// so cmd 0x70 would be dropped. Seed bit0 (models the service session reaching ready,
				// analogous to MODEL_SVC_CHANNEL_DRAIN seeding bit2).
				debug_ram_byte_w(0x0011fed1, debug_ram_byte(0x0011fed1) | 0x01);
				const uint8_t task = debug_ram_byte(0x00100022);
				setr(0, task);
				setr(1, msg);
				setr(14, SENT2 | 1);
				setr(12, 0x0026a204 | 1);          // r12 -> post_task_message
				m_resen_msg = msg;
				m_resen_state = 2;
				logerror("resen: alloc=%08x -> post(task=%02x cmd=70 blob=%s)\n", msg, task,
					fillenv <= 0xff ? "fill" : "sparse-class0x22");
				return BX_R12;
			}
			for (int i = 0; i < 15; i++) setr(i, m_resen_saved[i]);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_CPSR, m_resen_saved[15]);
			setr(12, 0x00237bc6 | 1);
			m_resen_state = 3;
			logerror("resen: alloc returned %08x (not RAM) — aborted\n", msg);
			return BX_R12;
		}
		if (m_resen_state == 2 && addr == SENT2)
		{
			for (int i = 0; i < 15; i++) setr(i, m_resen_saved[i]);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_CPSR, m_resen_saved[15]);
			setr(12, 0x00237bc6 | 1);
			m_resen_state = 3;
			logerror("resen: posted cmd 0x70; resuming contact-service loop t=%.4f\n", machine().time().as_double());
			return BX_R12;
		}
	}

	// MODEL_STARTUP_REPORTS (opt-in): emulate the subsystem-ready reports that drive the interactive
	// handoff past the mode-0d limp -- code 7 (the mode-4/7 init-burst trigger) and the mode-4 6-message
	// ready checklist codes 9/a/b/c/d/1c. On a real boot these are posted to task-1's mailbox by the
	// 0x2af0xx reporter stubs when the battery/MMI/display subsystems reach their ready states; our
	// reconstructed boot never reaches those states (docs/interactive_handoff.md #33). This model injects
	// them faithfully via the firmware's own post 0x26a354(mailbox=1, code) -- one call per report code,
	// chained through a sentinel trampoline (same pattern as MODEL_SVC_RESPONDER / MODEL_RES_ENABLE).
	// Triggered once at task-1's message getter 0x26ff14, after mode-0d completes.
	if (nokia_env_u32("NOKI3210_MODEL_STARTUP_REPORTS", 0) != 0 && pc == addr)
	{
		constexpr u32 SENT = 0x003ff200;     // distinct flash return sentinel
		constexpr uint16_t BX_R12 = 0x4760;
		static const uint8_t CODES[] = { 7, 9, 0xa, 0xb, 0xc, 0xd, 0x1c, 0x74 };
		constexpr unsigned NC = sizeof(CODES);
		auto setr = [&](int r, u32 v){ m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0 + r, v); };
		auto getr = [&](int r){ return u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0 + r)); };
		const double trig = nokia_env_u32("NOKI3210_STARTUP_REPORTS_MS", 950) / 1000.0;

		if (m_reports_state == 0 && addr == 0x0026ff14 &&
				machine().time().as_double() >= trig && debug_ram_byte(0x00100022) == 1)
		{
			for (int i = 0; i < 15; i++) m_reports_saved[i] = getr(i);
			m_reports_saved[15] = m_maincpu->state_int(arm7_cpu_device::ARM7_CPSR);
			setr(0, 1);                         // r0 = mailbox (task 1)
			setr(1, CODES[0]);                  // r1 = first report code
			setr(14, SENT | 1);                 // LR -> sentinel
			setr(12, 0x0026a354 | 1);           // r12 -> post-to-mailbox
			m_reports_idx = 0;
			m_reports_state = 2;
			logerror("reports: inject start t=%.4f (7,9,a,b,c,d,1c -> task1 mailbox)\n", machine().time().as_double());
			return BX_R12;
		}
		if (m_reports_state == 2 && addr == SENT)
		{
			m_reports_idx++;
			if (m_reports_idx < NC)
			{
				setr(0, 1);
				setr(1, CODES[m_reports_idx]);
				setr(14, SENT | 1);
				setr(12, 0x0026a354 | 1);
				return BX_R12;
			}
			for (int i = 0; i < 15; i++) setr(i, m_reports_saved[i]);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_CPSR, m_reports_saved[15]);
			setr(12, 0x0026ff14 | 1);           // resume: re-run task-1 getmsg (now finds the posted codes)
			m_reports_state = 3;
			logerror("reports: injected %u codes; resuming getmsg t=%.4f\n", NC, machine().time().as_double());
			return BX_R12;
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
		// ECB waiter-branch state at delayed-post time for the four 000d sweep events.
		// The delayed primitive 0x2697aa only delivers a raw code to the startup task if the
		// task is a registered WAITER (ECB +0 head non-null) and (TCB.mask 0x100024 & ECB.flags
		// +7) passes the 0x2697f2 bit test. Dump waithead/count/flags/state/mask to see whether
		// 0x15/0x16 can EVER reach the mailbox on this (blank+faked) boot, vs the wheel-only 0xd5.
		const u32 ev = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
		if (ev >= 0x14 && ev <= 0x17)
		{
			static unsigned ec = 0;
			if (ec++ < 40)
			{
				const offs_t r = 0x00100140 + ev * 0xc;
				const u32 waithead = debug_ram_word(r + 0) | (u32(debug_ram_word(r + 2)) << 16);
				const u32 tcbmask  = debug_ram_word(0x00100024) | (u32(debug_ram_word(0x00100026)) << 16);
				const uint8_t flags = debug_ram_byte(r + 7);
				logerror("limp2_ecb: ev=%02x waithead[+0]=%08x cnt[+4]=%04x flags[+7]=%02x state[+8]=%02x tcbmask=%08x mask&flags=%02x mode=%04x t=%.5f\n",
						ev, waithead, debug_ram_word(r + 4), flags, debug_ram_byte(r + 8),
						tcbmask, tcbmask & flags,
						debug_ram_word(0x001123f0), machine().time().as_double());
			}
		}
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
		// TRACE_RESAVAIL (opt-in): the availability predicate 0x2b12b4 -- log the resource id (r0),
		// enable [0x11fee4], the class, the bitmap byte it reads, the permuted mask, and the verdict.
		// Pins exactly why acquisition of the idle window 0x4c22 (class 0x4c) fails.
		if (nokia_env_u32("NOKI3210_TRACE_RESAVAIL", 0) != 0 && pc == addr && addr == 0x002b12b4)
		{
			const u32 id = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
			const unsigned cls = (id >> 8) & 0xff;
			const uint8_t byte = debug_ram_byte(0x0011ff08 + (cls >> 3));
			static const uint8_t rt[8] = {0x40,0x80,0x10,0x20,0x04,0x08,0x01,0x02};
			static unsigned ra = 0;
			if (debug_ram_byte(0x0011fee4) != 0 && ra++ < 80) logerror("resavail: id=%04x cls=%02x en[11fee4]=%02x byte[11ff%02x]=%02x mask=%02x -> %s t=%.4f\n",
					id, cls, debug_ram_byte(0x0011fee4), 0x08 + (cls>>3), byte, rt[cls&7],
					(byte & rt[cls&7]) ? "AVAIL" : "UNAVAIL", machine().time().as_double());
		}
		// TRACE_DSPDRV (opt-in, scouting): log distinct branch/call targets entering the GSM-L1 / audio
		// DSP driver layer 0x2b6000-0x2c8000 -- does the post-SIM boot reach the RF/network driver at all?
		if (nokia_env_u32("NOKI3210_TRACE_DSPDRV", 0) != 0 && pc == addr && addr >= 0x002b6000 && addr < 0x002c8000)
		{
			static std::unordered_map<u32, bool> seen;
			static unsigned dd = 0;
			if (!seen[addr] && dd++ < 120) { seen[addr] = true;
				logerror("dspdrv: enter %08x lr=%08x t=%.4f\n", addr,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double()); }
		}
		// TRACE_CSCMD (opt-in, fetch side): the contact-service command dispatcher 0x237400 reads the
		// command byte [msg+8] into r4 at 0x23741a and binary-searches to a handler. cmd 0x70/0x71 route to
		// the channel-map handler 0x23670c: 0x70 = resource ENABLE (-> 0x2b140a, config blob = msg+9),
		// 0x71 = resource DISABLE. Log every command byte the contact-service processes -- confirms whether
		// the resource-enable command 0x70 is ever delivered on our boot, and inventories the command set.
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x0023741a)
		{
			const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
			static unsigned cc = 0;
			if (cc++ < 80 && msg >= 0x00100000 && msg < 0x00180000)
				logerror("cscmd: [msg+8]=%02x [msg+5]=%02x svcready[11fed1]=%02x t=%.4f\n", debug_ram_byte(msg + 8),
						debug_ram_byte(msg + 5), debug_ram_byte(0x0011fed1), machine().time().as_double());
		}
		// TRACE_HANDOFF (opt-in): the post-SIM interactive/idle handoff. Seams (docs/interactive_handoff.md):
		//  (a) task-1 dispatcher 0x270c8e -- mode [0x1123f0], mode-0d checklist [0x112399], CCONT [0x11ff6c];
		//  (b) mode-0 interactive-init burst 0x270d1c -- did it run? (no: task 1 never enters mode 0);
		//  (c) display-manager idle-repaint call 0x298000 -- did display_idle fire? (no);
		//  (d) mode-0x04 handler entry 0x271254 -- the msgcodes task 1 gets in mode 4 (d5/75/33/c3, never
		//      the 7 that would trigger the init burst); 0x270184 -- every mode transition + caller lr;
		//      0x26a204/0x26a354 -- inventory of codes posted to task-1 mailbox (code 7 never appears).
		if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr)
		{
			if (addr == 0x00270c8e)
			{
				static unsigned h1 = 0; static u32 h1_last = 0xffffffff;
				const u32 key = (u32(debug_ram_word(0x001123f0)) << 16) | (u32(debug_ram_byte(0x00112399)) << 8)
						| debug_ram_byte(0x0011ff6c);
				if (key != h1_last && h1++ < 60)
				{
					h1_last = key;
					logerror("handoff: t1_dispatch mode=%04x chk[112399]=%02x ccont[11ff6c]=%02x sub[11239c]=%02x t=%.4f\n",
							debug_ram_word(0x001123f0), debug_ram_byte(0x00112399), debug_ram_byte(0x0011ff6c),
							debug_ram_byte(0x0011239c), machine().time().as_double());
				}
			}
			else if (addr == 0x00270d1c)
			{
				static unsigned h2 = 0;
				if (h2++ < 8)
					logerror("handoff: mode0 INTERACTIVE-INIT burst 0x270d1c ENTERED t=%.4f\n",
							machine().time().as_double());
			}
			else if (addr == 0x00298000)
			{
				static unsigned h3 = 0;
				if (h3++ < 8)
					logerror("handoff: display_idle FIRED (idle repaint) [1116fd]=%02x t=%.4f\n",
							debug_ram_byte(0x001116fd), machine().time().as_double());
			}
			else if (addr == 0x00271254)
			{
				static unsigned he = 0;
				if (he++ < 8) logerror("handoff4: mode-0x04 handler ENTERED (0x271254) msgcode=%04x t=%.4f\n",
						debug_ram_word(0x001123ee), machine().time().as_double());
			}
			// Inventory: every RTOS post to task 1 (0x26a204/0x26a354, r0=taskid, r1=msgptr; code=[msg+0]),
			// deduped by (code, caller). Does anything ever post code 7 (the mode-0x04 burst trigger)?
			else if (addr == 0x0026a204 || addr == 0x0026a354)
			{
				const u32 task = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
				if (task == 1)
				{
					const u32 code = m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xffff;
					const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
					static u32 seen[96]; static unsigned nseen = 0;
					const u32 kv = (code << 12) ^ lr;
					bool dup = false; for (unsigned i = 0; i < nseen; i++) if (seen[i] == kv) { dup = true; break; }
					if (!dup && nseen < 96)
					{
						seen[nseen++] = kv;
						logerror("t1post: code=%04x via=%08x lr=%08x t=%.4f\n", code, addr, lr, machine().time().as_double());
					}
				}
			}
			// mode-0d exit gate 0x270ec6 (log the two gate operands) + advance-taken 0x270ee6.
			else if (addr == 0x00270ec6)
			{
				static unsigned hgk = 0; static u32 hgk_last = 0xffffffff;
				const u32 chk = debug_ram_byte(0x00112399) & 0xf;
				const u32 cc = debug_ram_byte(0x0011ff6c) & 0xf;
				const u32 kv = (chk << 4) | cc;
				if (kv != hgk_last && hgk++ < 24)
				{
					hgk_last = kv;
					logerror("gate0d: 0x270ec6 chk&f=%x ccont&f=%x (need chk=f && cc=6) t=%.4f\n",
							chk, cc, machine().time().as_double());
				}
			}
			else if (addr == 0x00270ee6)
			{
				static unsigned hga = 0;
				if (hga++ < 4) logerror("gate0d: *** ADVANCE TAKEN 0x270ee6 t=%.4f\n", machine().time().as_double());
			}
			// VBAT voltage-confirmation gate byte [0x110436] (= battery struct [0x110434+2]) trajectory:
			// the interactive advance is blocked while it is 1 or 2 (0x2a6942 returns 0). NOTE this is the
			// battery/VBAT subsystem (0x21exxx, struct 0x110434), NOT the SIM -- confirmed via debug strings
			// ("BATTERY VOLTAGE CHECK", "Initialise VBAT filter"). Log on change.
			else if (addr == 0x0027d654)
			{
				static u32 last = 0xffffffff;
				const u32 v = debug_ram_byte(0x00110436);
				if (v != last)
				{
					last = v;
					logerror("vbatgate: [0x110436]=%02x t=%.4f\n", v, machine().time().as_double());
				}
			}
			// mode-write epilogue 0x270184 (strh r0,[r4,#4]): every task-1 mode transition + its caller lr.
			else if (addr == 0x00270184)
			{
				static u32 seen[48]; static unsigned nseen = 0;
				const u32 nm = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
				const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
				const u32 cur = debug_ram_word(0x001123f0) & 0xffff;
				const u32 kv = (cur << 20) ^ (nm << 12) ^ (lr & 0xfff);
				bool dup = false; for (unsigned i = 0; i < nseen; i++) if (seen[i] == kv) { dup = true; break; }
				if (!dup && nseen < 48)
				{
					seen[nseen++] = kv;
					logerror("t1mode: %04x -> %04x via lr=%08x t=%.4f\n", cur, nm, lr, machine().time().as_double());
				}
			}
		}
	// EXPERIMENT_VBAT_GATE_PASS (opt-in, diagnostic): the mode-0d proper advance (0x270eee, which arms the
	// mode-0x04 init-burst trigger 'code 7') is blocked because the VBAT voltage-confirmation byte [0x110436]
	// (battery struct 0x110434 field +2) == 1, making gate helper 0x2a6942 return 0. It is NOT a SIM state --
	// 0x21exxx/0x27dxxx here is the battery/VBAT voltage monitor (strings "BATTERY VOLTAGE CHECK",
	// "Initialise VBAT filter"); [0x110436] is a 15-tap moving-average classification that structurally
	// stays 1 on our boot and is independent of the battery ADC value. Force the gate to pass by overriding
	// 0x2a6942's read (at 0x2a6948, after bl 0x27d654) 1->0 -> 0x2a6942 returns 2 (nonzero). Tests whether
	// the advance then fires and cascades (arm code 7 -> burst -> interactive). Diagnostic only.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_VBAT_GATE_PASS", 0) != 0 && pc == addr && addr == 0x002a6948)
	{
		const u32 v = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		if (v == 1 || v == 2)
		{
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0);
			static unsigned e = 0;
			if (e++ < 4) logerror("vbat_gate_pass: forced 0x2a6942 read %u -> 0 t=%.4f\n", v, machine().time().as_double());
		}
	}
	// MODEL_VBAT_CONFIRM (opt-in): the VBAT voltage-confirmation gate. The classifier 0x27cbec writes a
	// state to [0x110436] used by 0x2a6942 (mode-0d fork AND mode-0xc exit, which needs state 3). Our
	// ROM-default reading (raw 0x200 -> sample ~2453) keeps it at 1. NOKI3210_VBAT_RAW overrides the raw
	// read at the sample generator (0x27cc80, post-bl 0x2b1bb2) so the classifier converges to a confirming
	// value -- faithful in that it drives the firmware's own classifier via the reading, not the gate.
	if (nokia_env_u32("NOKI3210_VBAT_RAW", 0xffffffff) != 0xffffffff && pc == addr && addr == 0x0027cc80)
	{
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, nokia_env_u32("NOKI3210_VBAT_RAW", 0x200) & 0xffff);
	}
	// code-7 emitter watch (opt-in): the single code-7 reporter 0x2af19e (post code 7 to task 1). If this
	// never fires, code 7 is never posted -- confirmed on our boot (all 4 callers sit behind subsystem
	// state machines that never reach the posting state; docs/interactive_handoff.md #33).
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr && addr == 0x002af19e)
	{
		static unsigned e = 0;
		if (e++ < 6) logerror("code7_post: code 7 -> task 1 lr=%08x t=%.4f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
	}
	// EXPERIMENT_FORCE_CODE7 (opt-in, diagnostic): the mode-0d advance's getmsg at 0x270f46 (bl 0x26ff14)
	// expects message code 7 to run the init burst; it never arrives. Force the getmsg return r0:=7 at the
	// post-bl point 0x270f4a (a reliably-hooked address, unlike mid-linear code MAME prefetches) so the
	// burst runs -- to MAP what gate comes next in the interactive chain (-> mode 0 interactive-init
	// 0x270d1c -> idle). Diagnostic chain-mapping lever, used with EXPERIMENT_VBAT_GATE_PASS.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_FORCE_CODE7", 0) != 0 && pc == addr && addr == 0x00270f4a)
	{
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 7);
		static unsigned e = 0;
		if (e++ < 8) logerror("force_code7: getmsg r0:=7 at 0x270f4a t=%.4f\n", machine().time().as_double());
	}
	// MODEL: service-channel drain (opt-in, NOKI3210_MODEL_SVC_CHANNEL_DRAIN). The service-ready gate
	// 0x29bafc requests channel-empty (0x2b13d4 -> msg 0x2a62) then busy-waits at 0x29bb06 for the
	// service-channel-busy bit [0x11fed1] bit2 (0x04) to clear -- which a real service peer does by
	// draining the channel. On our faked boot nothing drains it (bit2 stuck set), so the startup
	// supervisor spins forever and never resumes the application tasks. Model the drain: when the gate
	// is entered, clear bit2 (as the real transport would once the channel is empty). This is the
	// analogue of MODEL_SVC_RESPONDER for the readiness handshake. If the causal chain is right this
	// cascades: block2 resumes app tasks -> their init fills the 0x112280 checklist -> event 0x15 ->
	// 000d advances.
	if (nokia_env_u32("NOKI3210_MODEL_SVC_CHANNEL_DRAIN", 0) != 0 && pc == addr && addr == 0x0029bafc)
	{
		const uint8_t s = debug_ram_byte(0x0011fed1);
		if (s & 0x04)
		{
			debug_ram_byte_w(0x0011fed1, s & ~0x04);
			logerror("svc_drain: cleared [11fed1] bit2 (%02x->%02x) at gate t=%.4f\n",
					s, s & ~0x04, machine().time().as_double());
		}
	}
	// MODEL_SIM_CARD command capture: intercept the SIM APDU command the phone sends over the
	// service-lower transport (0x2aec34 with msg code 0x2701 in r1; r0=len, r2=data ptr to the raw
	// APDU). Remember the command (INS + full bytes) so the SIM_CARD responder can echo the T=0
	// procedure byte and mark that exactly one response is now pending.
	if (nokia_env_u32("NOKI3210_MODEL_SIM_CARD", 0) != 0
			&& pc == addr && addr == 0x002aec34 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xffff) == 0x2701)
	{
		const u32 len = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 buf = m_maincpu->state_int(arm7_cpu_device::ARM7_R2);
		if (buf >= 0x00100000 && buf < 0x00180000)
		{
			char hex[128]; int n = 0;
			for (u32 i = 0; i < len && i < 32; i++) n += std::snprintf(hex + n, sizeof(hex) - n, "%02x ", debug_ram_byte(buf + i));
			const uint8_t ins = len >= 2 ? debug_ram_byte(buf + 1) : 0;
			if (len >= 2) m_sim_last_ins = ins;   // FS responder: remember for the T=0 procedure-byte echo
			m_sim_last_cmdlen = uint8_t(std::min<u32>(len, sizeof(m_sim_last_cmd)));  // FS responder: full command
			for (unsigned i = 0; i < m_sim_last_cmdlen; i++) m_sim_last_cmd[i] = debug_ram_byte(buf + i);
			m_sim_card_pending = true;   // MODEL_SIM_CARD: this command now awaits exactly one response
			const char *name = ins == 0xa4 ? "SELECT" : ins == 0xc0 ? "GET_RESPONSE" : ins == 0xb0 ? "READ_BINARY"
							 : ins == 0xb2 ? "READ_RECORD" : ins == 0x20 ? "VERIFY_CHV" : ins == 0xf2 ? "STATUS" : "?";
			static unsigned ap = 0;
			if (ap++ < 60)
				logerror("sim_apdu: %-12s len=%u [ %s] t=%.4f\n", name, len, hex, machine().time().as_double());
		}
	}
	// MODEL_SIM_CARD (opt-in): the FAITHFUL ATR delivery. Instead of forcing the manager's recv return to
	// code 5 (EXPERIMENT_SIM_CODE5) and separately poking the ATR into 0x10dddc (MODEL_SIM_ATR_MSG), deliver
	// a genuine code-5 "response received" SIM-task message CARRYING the ATR bytes -- exactly how a real
	// SIM's ATR would arrive. The real dispatch 0x27df64 then copies the ATR (msg+5, len msg+2) into
	// 0x10dddc ([+0]=len, [+2..]=bytes) and returns 5, which the manager dispatches (0x27eb7c) to the code-5
	// handler 0x27ebbc -> parser 0x27e046. One message does the whole ATR handshake, no forcing.
	// Delivered once, on the SIM_CARD_ATR_AFTER'th SIM-task recv (default 2, modelling "reset -> ATR ready").
	if (nokia_env_u32("NOKI3210_MODEL_SIM_CARD", 0) != 0 && pc == addr && addr == 0x0026a458 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x0027df10)
	{
		m_sim_card_recv++;
		constexpr u32 SCRATCH = 0x0017fc00;
		uint8_t data[40]; unsigned n = 0; uint8_t code = 0;
		if (m_sim_card_phase == 0 && m_sim_card_recv >= nokia_env_u32("NOKI3210_SIM_CARD_ATR_AFTER", 2))
		{
			// Phase 0 -> ATR: a code-5 message carrying the ATR bytes (SIM_ATR_HEX).
			if (const char *hex = std::getenv("NOKI3210_SIM_ATR_HEX"))
				for (const char *p = hex; p[0] && p[1] && n < sizeof(data); p += 2)
					data[n++] = uint8_t(std::strtoul(std::string(p, 2).c_str(), nullptr, 16));
			if (n == 0) { data[0] = 0x3b; data[1] = 0x10; data[2] = 0x05; n = 3; }
			code = 5;
			m_sim_card_phase = 1;
		}
		else if (m_sim_card_phase == 1 && m_sim_card_pending)
		{
			// Phase 1 -> exactly one response per command (code 9): a PPS request (PPSS=0xFF) is echoed
			// verbatim; otherwise a bare SW=9000. (EF file content comes in a later increment.)
			if (m_sim_last_cmdlen > 0 && m_sim_last_cmd[0] == 0xff)
			{
				// PPS request: echoed verbatim (compared by memcmp at 0x27ed66, not the data path).
				for (unsigned i = 0; i < m_sim_last_cmdlen && n < sizeof(data); i++) data[n++] = m_sim_last_cmd[i];
			}
			else
			{
				// File-read command: the data path 0x27ee94 reads the response's FIRST byte (0x10dddc+2) as
				// the T=0 procedure byte (must equal the INS => "send all remaining"). After it comes the
				// payload + SW=9000. GET RESPONSE -> a GSM 11.11 EF FCP block; READ -> synthetic EF content.
				data[n++] = m_sim_last_ins;   // procedure byte = INS echo
				if (m_sim_last_ins == 0xc0)   // GET RESPONSE -> EF file-control-parameters (TS 51.011 9.2.1)
				{
					const u32 fid = nokia_env_u32("NOKI3210_SIM_CARD_EF", 0x6f07);
					const uint8_t fcp[15] = {
						0x00, 0x00,               // RFU
						0x00, 0x09,               // file size (9 bytes)
						uint8_t(fid >> 8), uint8_t(fid), // file id
						0x04,                     // type: EF
						0x00,                     // RFU
						0x00, 0x00, 0x00,         // access conditions: READ = ALWAYS (no PIN)
						0x01,                     // file status: not invalidated
						0x02,                     // length of following data
						0x00,                     // structure: transparent
						0x00 };                   // record length (n/a)
					for (unsigned i = 0; i < 15; i++) data[n++] = fcp[i];
					data[n++] = 0x90; data[n++] = 0x00;
				}
				else if (m_sim_last_ins == 0xb0 || m_sim_last_ins == 0xb2)  // READ -> synthetic EF content
				{
					const uint8_t body[8] = { 0x08, 0x09, 0x10, 0x10, 0x32, 0x54, 0x76, 0x98 };
					for (unsigned i = 0; i < 8; i++) data[n++] = body[i];
					data[n++] = 0x90; data[n++] = 0x00;
				}
			}
			code = 9;
			m_sim_card_pending = false;   // responded; wait for the next command
		}
		// Phase 2 (step 2d): SCRIPT the EF-read T=0 sequence as one code-3 request per step
		// (SELECT -> GET_RESPONSE -> READ BINARY). 0x27df9e copies the message (0x118 bytes) to descriptor
		// 0x10deec ([+2]=len 5, [+4]=code 3, [+5..9]=5-byte APDU, [+0xa..b]=file id for SELECT); the manager
		// issues the command and phase-1 answers it. Posted when no command is pending -- i.e. the manager is
		// looping at 0x27efb0 waiting for the next request. SIM_CARD_EF = file id (e.g. 6f07).
		else if (m_sim_card_phase == 1 && !m_sim_card_pending && m_sim_card_step < 4
				&& nokia_env_u32("NOKI3210_SIM_CARD_EF", 0) != 0)
		{
			const u32 fid = nokia_env_u32("NOKI3210_SIM_CARD_EF", 0x6f07);
			for (offs_t i = 0; i < 0x118; i++) debug_ram_byte_w(SCRATCH + i, 0);
			if (m_sim_card_step < 3)
			{
				uint8_t apdu[5]; unsigned fidlen = 0;
				static const char *sn[3] = { "SELECT", "GET_RESPONSE", "READ_BINARY" };
				if (m_sim_card_step == 0)      { apdu[0]=0xa0; apdu[1]=0xa4; apdu[2]=0; apdu[3]=0; apdu[4]=0x02; fidlen=2; }
				else if (m_sim_card_step == 1) { apdu[0]=0xa0; apdu[1]=0xc0; apdu[2]=0; apdu[3]=0; apdu[4]=0x0f; }
				else                           { apdu[0]=0xa0; apdu[1]=0xb0; apdu[2]=0; apdu[3]=0; apdu[4]=0x08; }
				debug_ram_byte_w(SCRATCH + 2, 0); debug_ram_byte_w(SCRATCH + 3, 5);   // descriptor len = 5
				debug_ram_byte_w(SCRATCH + 4, 3);                                     // code 3 = file-read request
				for (unsigned i = 0; i < 5; i++) debug_ram_byte_w(SCRATCH + 5 + i, apdu[i]);
				if (fidlen) { debug_ram_byte_w(SCRATCH + 0xa, uint8_t(fid >> 8)); debug_ram_byte_w(SCRATCH + 0xb, uint8_t(fid)); }
				logerror("sim_card: EF step %u -> code-3 %s (file %04x) t=%.4f\n",
						m_sim_card_step, sn[m_sim_card_step], fid, machine().time().as_double());
			}
			else
			{
				// Step 3: post a code-1 completion so the read-dispatch 0x27ede0 reaches the completion
				// handler 0x27ef34 (read done) instead of looping for more requests.
				debug_ram_byte_w(SCRATCH + 4, 1);   // code 1 = read complete
				// The read-complete decision 0x27ea88 posts no-SIM (0x1f) iff the no-SIM flag [0x111c64] is
				// set. It is set once at early init (no card present then) and never cleared, so a completed
				// read is always judged invalid. Model a validly-detected SIM by clearing it now (the
				// faithful analogue of "valid SIM found") -- gated so we can confirm it survives (a global
				// early force crashed the boot; clearing it late, post-detected, should be safe).
				if (nokia_env_u32("NOKI3210_SIM_CARD_CLEAR_NOSIM", 0) != 0)
					debug_ram_byte_w(0x00111c64, 0);
				logerror("sim_card: EF step 3 -> code-1 completion (nosim-clear=%u) t=%.4f\n",
						nokia_env_u32("NOKI3210_SIM_CARD_CLEAR_NOSIM", 0), machine().time().as_double());
			}
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, SCRATCH);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R12, 0x0027df10 | 1);
			m_sim_card_step++;
			return uint16_t(0x4760);
		}
		if (code != 0)
		{
			for (offs_t i = 0; i < 0x30; i++) debug_ram_byte_w(SCRATCH + i, 0);
			debug_ram_byte_w(SCRATCH + 2, uint8_t(n >> 8));
			debug_ram_byte_w(SCRATCH + 3, uint8_t(n));
			debug_ram_byte_w(SCRATCH + 4, code);
			for (unsigned i = 0; i < n; i++) debug_ram_byte_w(SCRATCH + 5 + i, data[i]);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, SCRATCH);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R12, 0x0027df10 | 1);
			static unsigned sc = 0;
			if (sc++ < 40) logerror("sim_card: phase->%u fed code-%u (%u bytes, [0]=%02x) recv #%u t=%.4f\n",
					m_sim_card_phase, code, n, data[0], m_sim_card_recv, machine().time().as_double());
			return uint16_t(0x4760);   // BX r12 -> return to 0x27df10 with r0=message
		}
	}
	// MODEL_SIM_CARD: the command ACK. The phone recv's the command result inside 0x27e98c (recv at
	// 0x27e9ca, ret 0x27e9ce); return a message with [msg+4]=7 so the send is accepted (procedure ACK).
	if (nokia_env_u32("NOKI3210_MODEL_SIM_CARD", 0) != 0 && pc == addr && addr == 0x0026a458 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x0027e9ce)
	{
		constexpr u32 SCRATCH = 0x0017fb00;
		for (offs_t i = 0; i < 0x20; i++) debug_ram_byte_w(SCRATCH + i, 0);
		debug_ram_byte_w(SCRATCH + 4, 7);
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, SCRATCH);
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R12, 0x0027e9ce | 1);
		return uint16_t(0x4760);
	}
	// MODEL_SIM_CARD: file-read-loop caller-gate. The loop recv 0x27ee52 (ret 0x27ee56 = strb r0,[r4,#5])
	// gets our fed code (9); force it to 0xb so the loop takes the DATA path 0x27ee94 (procedure-byte /
	// send-data / read) instead of the error branch. Only the file-read loop returns via 0x27ee56 -- the
	// PPS phase (0x27ed3c) uses a different recv, so its code-9 handling is untouched.
	if (nokia_env_u32("NOKI3210_MODEL_SIM_CARD", 0) != 0 && pc == addr && addr == 0x0027ee56)
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x0b);
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

	const unsigned ccont_event15_delay = nokia_env_u32("NOKI3210_CCONT_EVENT15_DELAY", 0xffffffff);
	if (ccont_event15_delay != 0xffffffff &&
			pc >= 0x002b08fc && pc <= 0x002b0a12 &&
			(addr == 0x002b0a40 || addr == 0x002b0a42))
	{
		// Boot-research shim: override the ROM delay literal (0x20a1=8353 ticks) for the delayed
		// event-15 post at 0x2b0a12. Shrinking it changes the 0xd5/wheel timing (hence deep-boot
		// frame-set sensitivity), but even at 1 tick 0x15 never delivers a raw code to the startup
		// task — the delayed post is wheel-only here (waiter branch gated off; see the 000d wall
		// note above and docs/ccont_subsystem.md). Diagnostic, not a hardware model.
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
			// MODEL_SIM_ATR (opt-in, faithful SIM build): serve the ATR from a register-level FIFO.
			if (nokia_env_u32("NOKI3210_MODEL_SIM_ATR", 0) != 0 && m_sim_atr_pos < m_sim_atr_len)
			{
				data = m_sim_atr[m_sim_atr_pos++];
			}
			else if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_RXD", 0xff) & 0xff;
			break;
		case 0x38:  // SIM UART interrupt identification
			if (nokia_env_u32("NOKI3210_MODEL_SIM_ATR", 0) != 0 && m_sim_atr_pos < m_sim_atr_len)
				data = nokia_env_u32("NOKI3210_SIM_ATR_IIR", 0x0a) & 0xff;   // RxD-data cause (iterate)
			else if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_IIR", 0x01) & 0xff;
			break;
		case 0x3c:  // SIM UART RxD queue fill
			if (nokia_env_u32("NOKI3210_MODEL_SIM_ATR", 0) != 0)
				data = uint8_t(m_sim_atr_len - m_sim_atr_pos);
			else if (std::getenv("NOKI3210_SIM_PROFILE"))
				data = nokia_env_u32("NOKI3210_SIM_RX_FILL", 0x00) & 0xff;
			break;
		case 0x3d:  // SIM RxD flags
			if (nokia_env_u32("NOKI3210_MODEL_SIM_ATR", 0) != 0)
				data = (m_sim_atr_pos < m_sim_atr_len) ? (nokia_env_u32("NOKI3210_SIM_ATR_RXFLAGS", 0x80) & 0xff) : 0x00;
			else if (std::getenv("NOKI3210_SIM_PROFILE"))
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

	// MODEL_SIM_ATR (opt-in, register-level ATR probe — NOT the faithful reception path).
	// CORRECTION: the MCU does NOT read the SIM RxD/RX_FILL/RX_FLAGS/IIR registers for reception — the
	// only genuine SIM-UART (base 0x20000) access in the whole firmware is the reset config + the single
	// flush at 0x2a01bc (earlier "register-based reception" scan hits were struct offsets that happen to
	// match SIM reg numbers). SIM byte reception is DSP/hardware-assisted, surfaced to the MCU as 0xaf
	// service messages into the data state machine 0x29ff2c. This model still arms an ATR on activate
	// (SIM_CONTROL 0x39 bit 0x80; reset seq ends 0x32->0x33->0xb3) and serves it on the registers, but
	// the firmware only ever reads the first byte via the flush — kept as a register probe, not a fix.
	// Default ATR is a minimal T=0 GSM convention (overridable via NOKI3210_SIM_ATR_HEX, space-free hex).
	if (nokia_env_u32("NOKI3210_MODEL_SIM_ATR", 0) != 0 && offset == 0x39 && (data & 0x80) && !(old_data & 0x80))
	{
		static const uint8_t default_atr[] = { 0x3b, 0x94, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00 };
		m_sim_atr_len = 0;
		if (const char *hex = std::getenv("NOKI3210_SIM_ATR_HEX"))
			for (const char *p = hex; p[0] && p[1] && m_sim_atr_len < sizeof(m_sim_atr); p += 2)
				m_sim_atr[m_sim_atr_len++] = uint8_t(std::strtoul(std::string(p, 2).c_str(), nullptr, 16));
		if (m_sim_atr_len == 0)
			for (uint8_t b : default_atr) m_sim_atr[m_sim_atr_len++] = b;
		m_sim_atr_pos = 0;
		// The SIM RxD reception is interrupt-driven; raise the SIM IRQ so the ISR drains the ATR.
		// Line is configurable while we pin it down (DSP=4, CCONT=6 are taken); default 5.
		const u32 line = nokia_env_u32("NOKI3210_SIM_IRQ_LINE", 5);
		if (line < 8)
			assert_irq(int(line));
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
	if (nokia_env_u32("NOKI3210_TRACE_DSPIO", 0) && dsp_io_first_touch('d', offset, 'R', m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1)))
		logerror("dspio: dspif R [%03x]        pc=%08x\n", offset, m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1));
	return 0;
}

void noki3310_state::mad2_dspif_w(offs_t offset, uint8_t data)
{
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x DSPIF\n", offset, data);
	if (nokia_env_u32("NOKI3210_TRACE_DSPIO", 0) && dsp_io_first_touch('d', offset, 'W', m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1)))
		logerror("dspio: dspif W [%03x] = %02x   pc=%08x\n", offset, data, m_maincpu->state_int(arm7_cpu_device::ARM7_PC) & ~u32(1));
}

uint8_t noki3310_state::mad2_mcuif_r(offs_t offset)
{
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x MCUIF\n", offset);
	return 0;
}

void noki3310_state::mad2_mcuif_w(offs_t offset, uint8_t data)
{
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
