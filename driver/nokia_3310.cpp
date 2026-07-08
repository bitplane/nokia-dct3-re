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
	bool          m_startup_latch_complete_seen;
	bool          m_after_mad2_soft_reset;

	// Node-0x18 service-responder trampoline state (NOKI3210_MODEL_SVC_RESPONDER).
	unsigned      m_svcresp_state;      // 0 idle, 1 await-alloc, 2 await-post, 3 done
	uint32_t      m_svcresp_saved[16];  // R0..R14 + CPSR saved at the trigger point
	uint32_t      m_svcresp_msg;        // allocated message pointer
	// SIM ATR FIFO (NOKI3210_MODEL_SIM_ATR): register-level ATR delivery on SIM activation.
	uint8_t       m_sim_atr[40];
	uint8_t       m_sim_atr_len = 0;
	uint8_t       m_sim_atr_pos = 0;
	bool          m_sim_loop = false;    // c2 feeder: set once the file-read loop (0x27ee40) is reached
	uint8_t       m_sim_script_idx = 0;  // c2 feeder: scripted-command index
	uint8_t       m_sim_last_ins = 0;    // FS responder: INS of the last APDU the phone sent (0x2aec34)
	uint8_t       m_sim_last_cmd[16] = {0}; // FS responder: full bytes of the last command the phone sent
	uint8_t       m_sim_last_cmdlen = 0; // FS responder: length of m_sim_last_cmd
	uint8_t       m_sim_card_phase = 0;  // MODEL_SIM_CARD: 0=await ATR, 1=post-ATR (PPS/data)
	uint32_t      m_sim_card_recv = 0;   // MODEL_SIM_CARD: SIM-task recv count (for ATR delivery timing)
	bool          m_sim_card_pending = false; // MODEL_SIM_CARD: a command awaits a response (set at 0x2aec34)
	bool          m_mmi_idle_forced = false;  // EXPERIMENT_MMI_IDLE: idle flag forced once
	uint8_t       m_mode4_step = 0;      // EXPERIMENT_MODE4_EVENTS: 0=idle,1=sent3,2=sent-e,3=done
	bool          m_mode4_bflag = false; // EXPERIMENT_MODE4_EVENTS: the missing readiness flag pre-set once
	bool          m_mode4_74 = false;    // EXPERIMENT_MODE4_EVENTS: event 0x74 injected once
	uint8_t       m_sim_card_step = 0;   // MODEL_SIM_CARD: EF-read T=0 step (0=SELECT,1=GET_RESPONSE,2=READ,3=done)
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

	// TRACE_SIMBUF (opt-in): log reads of the code-3 SIM buffer 0x10deec (0x118 bytes) with PC + offset,
	// to pin which bytes the phone reads as SIM file data (beyond the command at +5).
	if (nokia_env_u32("NOKI3210_TRACE_SIMBUF", 0) != 0 &&
			((address >= 0x0010deec && address < 0x0010df04) || (address >= 0x0010dddc && address < 0x0010ddf4)))
	{
		static unsigned sb_log = 0;
		const char *buf = address >= 0x0010deec ? "deec" : "dddc";
		const offs_t base = address >= 0x0010deec ? 0x0010deec : 0x0010dddc;
		if (sb_log++ < 300)
			logerror("simbuf: %s+%02x [%06x]=%04x pc=%08x t=%.4f\n",
					buf, address - base, address, data, pc, machine().time().as_double());
	}

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

	// TRACE_NOSIM (opt-in): trace writers of the "no SIM" flag [0x111c64] (BE even addr -> high byte of
	// the word at 0x111c64). The read-complete handler 0x27ea88 posts status 0x1f (Insert SIM card) iff
	// this is !=0. Log every change with the writing PC to find what SETS it during the failed read --
	// its inverse is the success criterion (keep it 0 -> events 0xe8/0xea -> SIM ready).
	if (nokia_env_u32("NOKI3210_TRACE_NOSIM", 0) != 0 && address == 0x00111c64)
	{
		const uint16_t oldw = m_ram[offset];
		const uint16_t neww = (oldw & ~mem_mask) | (data & mem_mask);
		const uint8_t oldb = oldw >> 8, newb = neww >> 8;   // even addr -> high byte
		if (oldb != newb)
		{
			static unsigned ns = 0;
			if (ns++ < 60)
				logerror("nosim: [111c64] %02x->%02x pc=%08x lr=%08x t=%.4f\n", oldb, newb, pc,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
		}
	}
	// TRACE_MODEWR (opt-in): writes to the startup mode field 0x1123f0 (struct 0x1123ec+4). Logs the new
	// mode value + writing PC -- pins where mode 000c (terminal) is committed vs where mode 0007 (advance).
	if (nokia_env_u32("NOKI3210_TRACE_MODEWR", 0) != 0 && address == 0x001123f0)
	{
		const uint16_t neww = (m_ram[offset] & ~mem_mask) | (data & mem_mask);
		static uint16_t last = 0xffff;
		if (neww != last)
		{
			last = neww;
			static unsigned mw = 0;
			if (mw++ < 40) logerror("modewr: [1123f0]=%04x pc=%08x lr=%08x t=%.4f\n", neww, pc,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
		}
	}
	// TRACE_DISPCB (opt-in): writes to the display-manager control block 0x10e2ec. Halfword 0x10e2ec holds
	// +0,+1(=type gate byte, low byte); halfword 0x10e2ee holds +2(=state, high byte),+3. Reveals what sets
	// the type to 0x80 (active window) -- the gate that unlocks the window-diff/code-2 path.
	if (nokia_env_u32("NOKI3210_TRACE_DISPCB", 0) != 0 && (address == 0x0010e2ec || address == 0x0010e2ee))
	{
		const uint16_t neww = (m_ram[offset] & ~mem_mask) | (data & mem_mask);
		static unsigned dc = 0;
		if (dc++ < 40) logerror("dispcb: [%08x]=%04x (%s) pc=%08x t=%.4f\n", address, neww,
				address == 0x0010e2ec ? "type=low byte" : "state=high byte", pc, machine().time().as_double());
	}
	// TRACE_DREADY (opt-in): trace writers of the display-ready flag [0x11fee4] (even addr -> high byte of
	// word 0x11fee4). 0x2b12b4 returns "resource unavailable" whenever this is 0, gating all display-resource
	// acquisition (incl. the idle draw's 0x224c). Find if/when the display subsystem sets it.
	if (nokia_env_u32("NOKI3210_TRACE_DREADY", 0) != 0 && address == 0x0011fee4)
	{
		const uint16_t oldw = m_ram[offset];
		const uint16_t neww = (oldw & ~mem_mask) | (data & mem_mask);
		const uint8_t oldb = oldw >> 8, newb = neww >> 8;   // even addr -> high byte
		if (oldb != newb)
		{
			static unsigned dr = 0;
			if (dr++ < 40)
				logerror("dready: [11fee4] %02x->%02x pc=%08x t=%.4f\n", oldb, newb, pc, machine().time().as_double());
		}
	}
	// TRACE_IDLEFLAG (opt-in): trace writers of the MMI idle-draw flag [0x1116fd] = MMI-struct base
	// 0x1116f8 (r4 in loop 0x297fc4, literal 16f80011 swap16) + 5; odd addr -> low byte of word 0x1116fc.
	// The MMI draws display_idle (0x2a255c) at 0x298000 iff this is 1. (Prior 0x11f81b was a swap error.)
	if (nokia_env_u32("NOKI3210_TRACE_IDLEFLAG", 0) != 0 && address == 0x001116fc)
	{
		const uint16_t oldw = m_ram[offset];
		const uint16_t neww = (oldw & ~mem_mask) | (data & mem_mask);
		const uint8_t oldb = oldw & 0xff, newb = neww & 0xff;   // odd addr -> low byte
		if (oldb != newb)
		{
			static unsigned idf = 0;
			if (idf++ < 60)
				logerror("idleflag: [1116fd] %02x->%02x pc=%08x lr=%08x t=%.4f\n", oldb, newb, pc,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
		}
	}
	// TRACE_SVCBIT2 (opt-in): faithfulness check for MODEL_SVC_CHANNEL_DRAIN. [0x11fed1] bit2 (the
	// service-channel-busy bit the readiness gate 0x29bafc spins on) lives in the word at 0x11fed0.
	// Log every write with the resulting byte + PC to see who sets bit2 (our faked responder path vs
	// real boot code) and whether anything ever clears it.
	if (nokia_env_u32("NOKI3210_TRACE_SVCBIT2", 0) != 0 && address == 0x0011fed0)
	{
		const uint16_t oldw = m_ram[offset];
		const uint16_t neww = (oldw & ~mem_mask) | (data & mem_mask);
		const uint8_t oldb = oldw & 0xff, newb = neww & 0xff;   // BE: odd addr 0x11fed1 = low byte
		if (((oldb ^ newb) & 0x04) || nokia_env_u32("NOKI3210_TRACE_SVCBIT2", 0) == 2)
		{
			static unsigned b2 = 0;
			if (b2++ < 50)
				logerror("svcbit2: [11fed1] %02x->%02x (bit2 %u->%u) pc=%08x t=%.4f\n",
						oldb, newb, (oldb>>2)&1, (newb>>2)&1, pc, machine().time().as_double());
		}
	}

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
	// EXPERIMENT (opt-in, diagnostic): mode-000d advance-gate preview. The handler (0x270e22)
	// completes flag [0x112399] only when it RECEIVES standalone codes 0x14/0x16/0x15/0x17
	// (bits 0x01/0x02/0x04/0x08). Confirmed (ROM disasm + limp2_ecb trace): 0x15 is posted ONLY
	// via the delayed primitive 0x2697aa, whose waiter-delivery branch (0x2697f2) needs
	// (TCB.mask 0x100024 & ECB.flags 0x10023c+7) != 0 — but it is 0x100 & 0x01 = 0 here, and the
	// ECB waiter list is empty (waithead=ffffffff), so the post is wheel-only and only ever
	// reflects as the 0xd5 poll (never a raw 0x15). That waiter/subscription state is runtime
	// state a genuinely-provisioned boot establishes and our blank+faked boot does not — a WALL,
	// not a missing hardware model. This knob injects the codes the firmware never injects on
	// this path, purely to preview post-gate boot (renders 4235fa). NOT faithful; see
	// docs/ccont_subsystem.md ("the 000d wall").
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
	// raw-0x16 source probe (opt-in): at BOTH message-post primitives (0x26a204 / 0x26a354, r0=task,
	// r1=msgptr — both branch targets, so the fetch hook fires), scan the message for a 0x15/0x16 byte
	// in the first 12 bytes. Finds the producer of the raw sweep deliveries (the one-off raw 0x16),
	// which is the template for what would deliver a raw 0x15 and clear 000d. Logs target task + LR.
	if (nokia_env_u32("NOKI3210_TRACE_MSGSRC", 0) != 0 && pc == addr &&
			(addr == 0x0026a204 || addr == 0x0026a354))
	{
		const u32 task = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 msg  = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		if (msg >= 0x00100000 && msg < 0x00180000)
		{
			bool hit = false;
			for (int i = 0; i < 12; i++)
			{ const uint8_t b = debug_ram_byte(msg + i); if (b == 0x15 || b == 0x16) hit = true; }
			static unsigned mp = 0;
			if (hit && mp++ < 60)
				logerror("msgsrc: via=%s task=%02x hdr=%02x%02x %02x%02x %02x%02x %02x%02x %02x%02x lr=%08x mode=%04x t=%.5f\n",
						addr == 0x0026a204 ? "a204" : "a354", task,
						debug_ram_byte(msg+0), debug_ram_byte(msg+1), debug_ram_byte(msg+2), debug_ram_byte(msg+3),
						debug_ram_byte(msg+4), debug_ram_byte(msg+5), debug_ram_byte(msg+6), debug_ram_byte(msg+7),
						debug_ram_byte(msg+8), debug_ram_byte(msg+9),
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
						debug_ram_word(FW_STARTUP_MODE), machine().time().as_double());
		}
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
	// delivery-path probe (opt-in): catch which of 0x26a458's three return-value stores fires and
	// with what code. pathA (raw, buffer @fp+0xc) = str r0 @0x26a4ee; pathC (RECODE table 0x2d71a8)
	// = str r1 @0x26a658; pathB (raw, buffer @fp+0x14) = str r0 @0x26a5ec. Filtered to the sweep
	// range (raw 0x14-0x17 / recoded 0xd4-0xd7). Shows whether 0x15 ever takes a raw path and what
	// node state routed the one-off raw 0x16 — the 000d raw-vs-recode routing question.
	// recode-path probe (opt-in): 0x26a640 is the bne target for the fp+8 linked-list delivery (a
	// pipeline-resync point, so the instruction-fetch hook fires here — mid-line stores do NOT, as
	// m_maincpu->pc() lags the fetch). r0 = the delivered node, which is the ECB entry itself
	// (0x100140 + event*0xc); its class byte [+9] indexes the recode table 0x2d71a8 (small classes:
	// surfaced = 0xc0 + class). Runtime-confirms class 0x15 -> 0xd5, class 0x16 -> 0xd6 at mode 000d.
	if (nokia_env_u32("NOKI3210_TRACE_DELIV", 0) != 0 && pc == addr && addr == 0x0026a640)
	{
		const u32 node = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		static unsigned dv = 0;
		if (dv++ < 50 && node >= 0x00100000 && node < 0x00180000)
		{
			const uint8_t cls = debug_ram_byte(node + 9);
			logerror("recode: ecb=%06x class[+9]=%02x -> surfaced=%02x mode=%04x t=%.5f\n",
					node, cls, (cls < 0x40) ? (0xc0 + cls) : 0, debug_ram_word(FW_STARTUP_MODE),
					machine().time().as_double());
		}
	}
	// EXPERIMENT (opt-in, diagnostic): "march" the startup mode chain toward idle by injecting each
	// mode's advance event at the startup recv (0x26ff1a, r0 = the code the mode handler will check).
	// The real event-producers exist in-ROM but our stubbed subsystems (DSP/RF/battery-ready) don't
	// fire them; this fakes the ready-signal per mode. 000d is left to FORCE_000D_EVENTS (multi-event
	// flag). NOT faithful — a scaffold to see how far a "bullshit" boot reaches (a hollow idle).
	// diagnostic (opt-in): map each startup mode to its recv wait-loop by logging the caller LR at
	// the recv-wrapper entry 0x26ff14, once per (mode,lr). Reveals where each mode spins, so its exact
	// wait event can be read from the disassembly.
	if (nokia_env_u32("NOKI3210_TRACE_MODEWAIT", 0) != 0 && pc == addr && addr == 0x0026ff14)
	{
		const u16 mode = debug_ram_word(FW_STARTUP_MODE);
		const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		static uint32_t seen[64]; static unsigned n = 0;
		const uint32_t key = (u32(mode) << 24) ^ lr;
		bool f = false; for (unsigned i = 0; i < n; i++) if (seen[i] == key) { f = true; break; }
		if (!f && n < 64) { seen[n++] = key;
			logerror("modewait: mode=%04x recv-caller lr=%08x t=%.4f\n", mode, lr, machine().time().as_double()); }
	}
	if (nokia_env_u32("NOKI3210_EXPERIMENT_MARCH", 0) != 0 && pc == addr && addr == 0x0026ff1a)
	{
		u16 ev = 0;
		switch (debug_ram_word(FW_STARTUP_MODE))
		{
			case 0x0004: ev = 0x07; break;   // POST_SELFTEST      -> BATTERY_READY
			case 0x000b: ev = 0x07; break;   // POST_CHARGER       -> BATTERY_READY
			case 0x0005: ev = 0x06; break;   // READY_GATE
			case 0x0006: ev = 0x03; break;   // SERVICE_QUIESCE
			case 0x0007: ev = 0x74; break;   // BATTERY_READY_GATE -> subsystem-ready
			case 0x0009: ev = 0x0e; break;   // BATTERY_WAIT
			case 0x000c: break;              // 000c handled by EXPERIMENT_BOOTPATH (PC-specific, no derail)
		}
		if (ev != 0)
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, ev);
	}
	// EXPERIMENT (opt-in, diagnostic): override the startup *outcome* committed at 0x2b4dda. The whole
	// power-on-reason arbiter (boot vs charge vs low-battery-off) funnels here: 0x2b4dda stores r4=outcome
	// to [0x1150ff], and the supervisor 0x2a924c dispatches on it ({1,5,6}=retry). Sweeping the value lets
	// us see empirically what each outcome renders, before RE-ing the gates that steer to a given outcome.
	{
		const u32 fo = nokia_env_u32("NOKI3210_FORCE_OUTCOME", 0xffffffff);
		if (fo != 0xffffffff && pc == addr && addr == 0x002b4dda)
		{
			const u32 was = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, fo & 0xff);
			logerror("force_outcome: commit outcome=%u (was %u) t=%.4f\n",
					fo & 0xff, was, machine().time().as_double());
		}
	}
	// EXPERIMENT (opt-in, option C — hollow idle): the MMI main loop (0x297fc4) recv's messages, dispatches,
	// and when idle-flag [0x11f81b]==1 calls display_idle (0x2a255c). TRACE_MMI shows whether that task runs
	// in our stuck boot; FORCE_IDLE pins the flag at the recv (0x298008) so the idle redraw fires each loop.
	// EXPERIMENT_MODE4_EVENTS (opt-in): inject the raw handshake sequence 3 -> 0xe -> 7 that mode 4's
	// nested recvs need (0x271254 wants raw 3 -> 0x2711f6 wants 0xe -> 0x27120e wants 7 -> ev7-init 0x271266),
	// modelling the external producer our boot lacks. Started once after EXPERIMENT_MODE4_EVENTS_MS ms.
	// Does ev7-init then run its subsystem-init and advance the boot toward the app/idle?
	if (nokia_env_u32("NOKI3210_EXPERIMENT_MODE4_EVENTS", 0) != 0 && pc == addr)
	{
		if (addr == 0x00271254 && m_mode4_step == 0 &&
				machine().time().as_double() * 1000.0 >= nokia_env_u32("NOKI3210_EXPERIMENT_MODE4_EVENTS_MS", 1200))
		{
			const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			if (r4 >= 0x00100000 && r4 < 0x00180000) { debug_ram_byte_w(r4 + 2, 0); debug_ram_byte_w(r4 + 3, 3); }  // event 3
			m_mode4_step = 1;
			logerror("mode4: injected event 3 (-> 0x2711f6) t=%.4f\n", machine().time().as_double());
		}
		else if (addr == 0x002711fa && m_mode4_step == 1)   // nested recv return: force 0xe
		{
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x0e);
			m_mode4_step = 2;
			logerror("mode4: injected event 0xe (-> 0x27120e) t=%.4f\n", machine().time().as_double());
		}
		else if (addr == 0x00271212 && m_mode4_step == 2)   // nested recv return: force 7 -> ev7-init
		{
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x07);
			m_mode4_step = 3;
			logerror("mode4: injected event 7 (-> ev7-init 0x271266) t=%.4f\n", machine().time().as_double());
		}
		// The advance is event 4 -> 0x271354 -> (if [0x112398]!=1 and charger absent) -> mode-7 wait 0x271392.
		// But ev7-init's own self-posted event 6 arrives first: event 6 -> 0x271316 -> [0x112398]=1 ->
		// 0x2714fc UNCONDITIONAL terminal 000c. So suppress event 6 post-ev7-init (convert it to a harmless
		// tick 0xc3 at the recv-stores) so event 4 wins the race and advances with [0x112398]==0.
		// The accumulator advances via the completion check 0x271326 (reached on event 0xd): if ALL readiness
		// flags 0x112390-95 are set it falls through to the advance, else it loops. Combined lever: (a) at the
		// post-init recv 0x2712ba pre-set all readiness flags 0x112390-95=1; (b) at the accumulator recv-stores
		// convert the stray self-tick 0xc3 (which would hit the else->terminal) into event 0xd so the
		// completion check runs and, with all flags set, advances instead of terminaling.
		else if (addr == 0x002712ba && m_mode4_step == 3 && !m_mode4_bflag)
		{
			for (offs_t a = 0x00112390; a <= 0x00112395; a++) debug_ram_byte_w(a, 1);
			m_mode4_bflag = true;
			logerror("mode4: pre-set all readiness flags 0x112390-95=1 t=%.4f\n", machine().time().as_double());
		}
		else if ((addr == 0x002712be || addr == 0x002712ca) && m_mode4_step == 3 &&
				(m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff) == 0xc3)
		{
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x0d);   // tick -> completion-check trigger
			static unsigned s6 = 0;
			if (s6++ < 12) logerror("mode4: tick 0xc3 -> event 0xd (completion check) at %06x t=%.4f\n", addr, machine().time().as_double());
		}
		// Past the accumulator the boot spins at 0x27138e waiting for event 0x74 (mode-7 gate 0x271396).
		// Inject 0x74 once at the recv-store 0x271392 -- with the SIM accepted and mode 4 cracked, see how
		// far the now-far-more-coherent boot proceeds (outcome 3? idle? terminal?).
		else if (addr == 0x00271392 && m_mode4_step == 3 && !m_mode4_74)
		{
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x74);
			m_mode4_74 = true;
			logerror("mode4: injected event 0x74 (mode-7 gate) t=%.4f\n", machine().time().as_double());
		}
	}
	// TRACE_STARTUP4 (opt-in): dump the startup-readiness flag accumulator 0x112390-0x112398 at the mode-4
	// event loop 0x2712cc. Events 9/a/b/c/1c set flags [0x112390/92/93/94/95]; event 6 -> terminal 0x2714fc.
	// Shows which subsystem-readiness flags are set vs missing with the SIM accepted.
	if (nokia_env_u32("NOKI3210_TRACE_STARTUP4", 0) != 0 && pc == addr &&
			(addr == 0x00271364 || addr == 0x00271396 || addr == 0x00271422 || addr == 0x0027136c || addr == 0x0027138e))
	{
		static unsigned s7 = 0;
		if (s7++ < 25) logerror("startup4: %s t=%.4f\n",
				addr == 0x00271364 ? "ADVANCED past accumulator -> 0x271364 (charger check)" :
				addr == 0x00271422 ? "-> 0x271422 (charger PRESENT -> battery display)" :
				addr == 0x0027136c ? "-> 0x27136c (charger absent -> subsystem inits)" :
				addr == 0x0027138e ? "-> 0x27138e (post-init recv, before 0x74 wait)" :
				"reached mode-7 event-0x74 wait 0x271396", machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_STARTUP4", 0) != 0 && pc == addr &&
			(addr == 0x00271254 || addr == 0x00271266 || addr == 0x00271270 || addr == 0x00271292 ||
			 addr == 0x0027129a || addr == 0x002712aa || addr == 0x002712ba || addr == 0x002712cc))
	{
		static unsigned su = 0;
		if (su++ < 60)
		{
			if (addr == 0x00271254)
			{
				const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
				const u16 ev = (r4 >= 0x00100000 && r4 < 0x00180000) ?
						((u16(debug_ram_byte(r4 + 2)) << 8) | debug_ram_byte(r4 + 3)) : 0xffff;
				logerror("startup4: mode4 handler, event[r4+2]=%04x (needs 7 for ev7-init) t=%.4f\n",
						ev, machine().time().as_double());
			}
			else if (addr == 0x002712cc)
			{
				const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
				const u16 ev = (r4 >= 0x00100000 && r4 < 0x00180000) ?
						((u16(debug_ram_byte(r4 + 2)) << 8) | debug_ram_byte(r4 + 3)) : 0xffff;
				char f[24]; for (int i = 0; i <= 8; i++) std::snprintf(f + i*2, 3, "%02x", debug_ram_byte(0x00112390 + i));
				static std::string last;
				std::string cur = std::to_string(ev) + f;
				if (cur != last) { last = cur;
					logerror("startup4: accum event=%04x flags[112390..98]=%s t=%.4f\n", ev, f, machine().time().as_double()); }
			}
			else
			{
				const char *w =
					addr == 0x00271266 ? "ev7-init START" :
					addr == 0x00271270 ? "-> 0x2af058" :
					addr == 0x00271292 ? "-> 0x2a26d4" :
					addr == 0x0027129a ? "-> 0x2794d2" :
					addr == 0x002712aa ? "-> 0x2a102c (last init)" :
					addr == 0x002712ba ? "post-init recv" : "?";
				logerror("startup4: %s t=%.4f\n", w, machine().time().as_double());
			}
		}
	}
	// TRACE_MMIPROD (opt-in): match MMI messages to their producer by message POINTER. Log every send via
	// both 0x26a204 and 0x26a354 (r0=target, r1=msg ptr, [r1+5]=primary code) AND every MMI recv at 0x29800c
	// (r0=recv'd msg). The post whose msg ptr == an MMI-recv msg ptr is that message's exact producer; and
	// whether a code-2 (window-mgmt/wake) message is ever posted to the MMI reveals the missing producer.
	if (nokia_env_u32("NOKI3210_TRACE_MMIPROD", 0) != 0 && pc == addr && (addr == 0x0026a204 || addr == 0x0026a354))
	{
		const u32 tgt = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		if (msg >= 0x00100000 && msg < 0x00180000)
		{
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			static unsigned mp = 0;
			if (mp++ < 400) logerror("mmiprod: POST fn=%s tgt=%u code=%02x msg=%08x lr=%08x t=%.4f\n",
					addr == 0x0026a204 ? "204" : "354", tgt, debug_ram_byte(msg + 5), msg, lr, machine().time().as_double());
		}
	}
	// TRACE_DISPPROD (opt-in): find the display/window task's id and posters by matching message pointers.
	// Log every send 0x26a204/0x26a354 (r0=target, r1=msg, [msg+0]hw = the display task's code) and every
	// display recv at 0x23e64a. The post whose msg ptr == a display-recv ptr is that message's producer;
	// any post with code[+0] in 0x0b04+ (window-create) is the wake we're hunting.
	if (nokia_env_u32("NOKI3210_TRACE_DISPPROD", 0) != 0 && pc == addr && (addr == 0x0026a204 || addr == 0x0026a354))
	{
		const u32 tgt = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		if (msg >= 0x00100000 && msg < 0x00180000)
		{
			const u32 code0 = (u32(debug_ram_byte(msg)) << 8) | debug_ram_byte(msg + 1);
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			static unsigned dp = 0;
			if (dp++ < 400) logerror("dispprod: POST fn=%s tgt=%u code0=%04x msg=%08x lr=%08x t=%.4f\n",
					addr == 0x0026a204 ? "204" : "354", tgt, code0, msg, lr, machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_DISPPROD", 0) != 0 && pc == addr && addr == 0x0023e64a)
	{
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (msg >= 0x00100000 && msg < 0x00180000)
			logerror("dispprod: DISP-RECV msg=%08x code0=%04x t=%.4f\n", msg,
					(u32(debug_ram_byte(msg)) << 8) | debug_ram_byte(msg + 1), machine().time().as_double());
	}
	// TRACE_DISPSUB (opt-in): does the 0x240xxx-0x242xxx display/window-diff subsystem execute at all? Log
	// the first fetch into the region and count distinct entry addresses -- if it never runs, the whole
	// code-2-to-MMI producer layer is dead on our boot.
	if (nokia_env_u32("NOKI3210_TRACE_DISPSUB", 0) != 0 && pc == addr && addr >= 0x00240000 && addr < 0x00243000)
	{
		static unsigned ds = 0; static u32 firstpc = 0;
		if (firstpc == 0) { firstpc = addr; logerror("dispsub: FIRST entry pc=%08x t=%.4f\n", addr, machine().time().as_double()); }
		if (ds++ < 4000 && (ds % 500) == 0) logerror("dispsub: %u fetches, latest pc=%08x t=%.4f\n", ds, addr, machine().time().as_double());
		// At the state-dispatch entry 0x24047e dump the control block r4: +1=type, +2=state (the loop gate),
		// +8=ptr; and [sp+0x10] pointer P whose [P] is compared to the state.
		if (addr == 0x0024092e || addr == 0x00240e5e || addr == 0x00241aac)
			logerror("dispsub: REACHED code-2 post site %08x t=%.4f\n", addr, machine().time().as_double());
		if (addr == 0x0024047e)
		{
			const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			static unsigned de = 0;
			if (de++ < 8 && r4 >= 0x00100000 && r4 < 0x00180000)
				logerror("dispsub: @47e r4=%08x type[+1]=%02x state[+2]=%02x ptr[+8]=%08x t=%.4f\n",
						r4, debug_ram_byte(r4 + 1), debug_ram_byte(r4 + 2),
						(u32(debug_ram_byte(r4+8))<<24)|(u32(debug_ram_byte(r4+9))<<16)|(u32(debug_ram_byte(r4+10))<<8)|debug_ram_byte(r4+11),
						machine().time().as_double());
		}
	}
	// EXPERIMENT_FORCE_WINTYPE (opt-in, NOT faithful): the display manager's type stays 0 because the window
	// entry [0x10e461] is 0. Poke it to 0x80 (active window) at the display recv, so the next manager update
	// sees type 0x80 -- a cascade test: does the 0x240xxx dispatch then reach the code-2 post -> MMI window-SM?
	if (nokia_env_u32("NOKI3210_EXPERIMENT_FORCE_WINTYPE", 0) != 0 && pc == addr && addr == 0x0023e64a)
	{
		debug_ram_byte_w(0x0010e461, 0x80);
		static unsigned fw = 0;
		if (fw++ < 4) logerror("force_wintype: poked [0x10e461]=0x80 t=%.4f\n", machine().time().as_double());
	}
	// The display/window task 0x23e62c recv's at 0x23e646 (ret 0x23e64a, r0=msg, [msg]=code base 0x0b04).
	// Log the codes it gets -- reveals whether a window-activate message (setting type 0x80) ever arrives.
	if (nokia_env_u32("NOKI3210_TRACE_DISPSUB", 0) != 0 && pc == addr && addr == 0x0023e64a)
	{
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (msg >= 0x00100000 && msg < 0x00180000)
		{
			static unsigned dm = 0;
			if (dm++ < 40) logerror("dispsub: window-task recv code=%04x [+2]=%02x [+3]=%02x t=%.4f\n",
					(u32(debug_ram_byte(msg)) << 8) | debug_ram_byte(msg + 1), debug_ram_byte(msg + 2), debug_ram_byte(msg + 3),
					machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_MMIPROD", 0) != 0 && pc == addr && addr == 0x0029800c)
	{
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (msg >= 0x00100000 && msg < 0x00180000)
			logerror("mmiprod: MMI-RECV msg=%08x code=%02x t=%.4f\n", msg, debug_ram_byte(msg + 5), machine().time().as_double());
	}
	// TRACE_MMIWIN (opt-in): the MMI window state machine 0x297ed8 -- W = [0x1116f8+8] (window array cursor),
	// count = [W+4], top entry = 0x111724 + count*0x1c, its window-id byte [top+0]->deref and state [+0x19].
	// Shows the window stack over the boot: which screens are active and whether an idle window is ever pushed.
	if (nokia_env_u32("NOKI3210_TRACE_MMIWIN", 0) != 0 && pc == addr && addr == 0x00297ed8)
	{
		auto rw = [&](u32 a)->u32 { return (u32(debug_ram_byte(a))<<24)|(u32(debug_ram_byte(a+1))<<16)|
				(u32(debug_ram_byte(a+2))<<8)|debug_ram_byte(a+3); };
		const u32 W = rw(0x00111700);
		const u8 count = (W >= 0x00100000 && W < 0x00180000) ? debug_ram_byte(W + 4) : 0xff;
		const u32 top = 0x00111724 + u32(count) * 0x1c;
		const u32 wdesc = rw(top);   // [top+0] = window descriptor ptr
		const u8 wid = (wdesc >= 0x00100000 && wdesc < 0x00300000) ? debug_ram_byte(wdesc) : 0xff;
		static u32 last = 0xffffffff;
		const u32 key = (u32(count) << 16) | (wid << 8) | debug_ram_byte(top + 0x19);
		if (key != last)
		{
			last = key;
			static unsigned wc = 0;
			if (wc++ < 40) logerror("mmiwin: count=%u topdesc=%08x wid=%02x state[+19]=%02x t=%.4f\n",
					count, wdesc, wid, debug_ram_byte(top + 0x19), machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_MMI", 0) != 0 && pc == addr &&
		(addr == 0x00297fc4 || addr == 0x00298008 || addr == 0x002a255c))
	{
		static unsigned mc[3] = { 0, 0, 0 };
		const int i = (addr == 0x00297fc4) ? 0 : (addr == 0x00298008) ? 1 : 2;
		if (mc[i]++ < 4)
			logerror("mmi: %06x (#%u) idleflag[11f81b]=%u t=%.4f\n",
					addr, mc[i], debug_ram_byte(0x0011f81b), machine().time().as_double());
	}
	// TRACE_RENDER (opt-in, diagnostic): instruments the display-render task (task 5). 0x2af638 = its loop
	// top, 0x2af670 = display_state_main dispatch, 0x2af6ea = startup_status_message_post (idle posts here),
	// 0x2b1c9a = lcd_write (GENSIO). Shows whether task 5 runs and renders in the plain limp.
	if (nokia_env_u32("NOKI3210_TRACE_RENDER", 0) != 0 && pc == addr &&
		(addr == 0x002af638 || addr == 0x002af670 || addr == 0x002af6ea || addr == 0x002b1c9a))
	{
		static unsigned rc[4] = { 0, 0, 0, 0 };
		const int i = (addr == 0x002af638) ? 0 : (addr == 0x002af670) ? 1 : (addr == 0x002af6ea) ? 2 : 3;
		if (rc[i]++ < nokia_env_u32("NOKI3210_TRACE_RENDER_MAX", 10))
			logerror("render: %06x (#%u) r0=%04x t=%.4f\n", addr, rc[i],
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff, machine().time().as_double());
	}
	// TRACE_SWEEP15 (opt-in, diagnostic): the raw-0x15 producer (0x2af208, called at 0x2521cc) fires only
	// when the 11-byte battery-init checklist 0x112280[0..0xa] is ALL non-zero (loop at 0x2521b2). Log the
	// checklist each time it is evaluated, and flag the post site -- shows which sub-steps our boot misses.
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
	// TRACE_SIMSM (opt-in): map the SIM state machine 0x29ff2c (dispatch on state [r4+9]; data
	// [r4+0xa]/[r4+0xb]) and the 0xaf service sends (0x234634 r1=code, r2=subcode) + the SIM reset
	// entry 0x2a01b8. Shows the SIM protocol flow and where our boot is stuck.
	if (nokia_env_u32("NOKI3210_TRACE_SIMSM", 0) != 0 && pc == addr)
	{
		if (addr == 0x0029ff2c)
		{
			static unsigned e = 0;
			const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			if (e++ < 60 && r4 >= 0x00100000 && r4 < 0x00180000)
				logerror("simsm: enter state[+9]=%02x d[+a]=%02x d[+b]=%02x r4=%08x lr=%08x t=%.4f\n",
						debug_ram_byte(r4+9), debug_ram_byte(r4+0xa), debug_ram_byte(r4+0xb), r4,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
		}
		else if (addr == 0x00234634)
		{
			static unsigned s = 0;
			if ((m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff) == 0xaf && s++ < 60)
				logerror("simsm: send 0xaf subcode=%02x lr=%08x t=%.4f\n",
						m_maincpu->state_int(arm7_cpu_device::ARM7_R2) & 0xff,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
		}
		else if (addr == 0x002a01b8)
		{
			static unsigned r = 0;
			if (r++ < 20) logerror("simsm: SIM-reset-entry 0x2a01b8 t=%.4f\n", machine().time().as_double());
		}
		else if (addr == 0x0027df1c)   // SIM task dispatch: r8=[msg+4] code. Struct base literal dca80010
		{
			// VERIFY the real struct address: dca80010 halfword-swaps to 0x0010dca8 (earlier 0x10a8dc was a
			// byte-swap error). Log both so we can see which holds live SIM state.
			static unsigned d = 0;
			if (d++ < 60)
				logerror("simsm: msg[+4]=%02x  @dca8[+9,+7,+a]=%02x %02x %02x  @a8dc[+9,+7,+a]=%02x %02x %02x t=%.4f\n",
						m_maincpu->state_int(arm7_cpu_device::ARM7_R8) & 0xff,
						debug_ram_byte(0x0010dca8+9), debug_ram_byte(0x0010dca8+7), debug_ram_byte(0x0010dca8+0xa),
						debug_ram_byte(0x0010a8dc+9), debug_ram_byte(0x0010a8dc+7), debug_ram_byte(0x0010a8dc+0xa),
						machine().time().as_double());
		}
		else if (addr == 0x0027e024)   // reset-start
		{
			static unsigned rs = 0;
			if (rs++ < 20) logerror("simsm: RESET-START (0x27e024) t=%.4f\n", machine().time().as_double());
		}
	}
	// MODEL_SIM_RESPONDER (opt-in, step 4 increment (a) — skeleton, log only): intercept the SIM APDU
	// command the phone sends over the service-lower transport (0x2aec34 with msg code 0x2701 in r1;
	// r0=len, r2=data ptr to the raw APDU). Log each APDU decoded (CLA INS P1 P2 P3 ...). This is the
	// interception point where the T=0 response will be injected in later increments. For now it only
	// observes -- with no response the phone stalls after the first APDU (that is expected here).
	if ((nokia_env_u32("NOKI3210_MODEL_SIM_RESPONDER", 0) != 0 || nokia_env_u32("NOKI3210_MODEL_SIM_CARD", 0) != 0)
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
	// TRACE_SIMSEL (opt-in): trace the file-read loop's response dispatch — the caller-gate 0x27ee56 (code
	// stored), the data path 0x27ee94 (dumps sb + [sb..] + INS desc[+6]), the error 0x27ef02, the
	// completion 0x27ef34 — to see how the EF SELECT response is (mis)handled.
	if (nokia_env_u32("NOKI3210_TRACE_SIMSEL", 0) != 0 && pc == addr &&
			(addr == 0x0027ee94 || addr == 0x0027ef02 || addr == 0x0027ef34 || addr == 0x0027ef0a))
	{
		static unsigned ss = 0;
		if (ss++ < 30)
		{
			if (addr == 0x0027ef0a)
				logerror("simsel: HEADER-DONE 0x27ef0a INS=%02x r1=%02x t=%.4f\n",
						m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff, machine().time().as_double());
			else if (addr == 0x0027ee94)
			{
				const u32 sb = m_maincpu->state_int(arm7_cpu_device::ARM7_R9);
				char b[48]; int k = 0;
				if (sb >= 0x00100000 && sb < 0x00180000)
					for (unsigned i = 0; i < 6; i++) k += std::snprintf(b+k, sizeof(b)-k, "%02x ", debug_ram_byte(sb+i));
				else std::snprintf(b, sizeof(b), "<non-ram>");
				logerror("simsel: DATA-PATH 0x27ee94 sb=%08x [%s] t=%.4f\n", sb, b, machine().time().as_double());
			}
			else
				logerror("simsel: %s t=%.4f\n", addr == 0x0027ef02 ? "ERROR 0x27ef02" : "COMPLETION 0x27ef34",
						machine().time().as_double());
		}
	}
	// TRACE_SIMPPS (opt-in): trace the file-read phase entry 0x27ed3c and the PPS-compare path, to see the
	// ack code after the len-3 (PPS) send and whether the compare 0x27ed6a is reached / what it compares.
	if (nokia_env_u32("NOKI3210_TRACE_SIMPPS", 0) != 0 && pc == addr &&
			(addr == 0x0027ed3c || addr == 0x0027ed4a || addr == 0x0027ed5c || addr == 0x0027ed6a || addr == 0x0027ed6e))
	{
		static unsigned pp = 0;
		if (pp++ < 40)
		{
			const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			const u32 r4 = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
			const uint8_t ack = (r4 >= 0x00100000 && r4 < 0x00180000) ? debug_ram_byte(r4 + 5) : 0xee;
			if (addr == 0x0027ed3c)
				logerror("simpps: ENTRY 0x27ed3c [+10]=%02x t=%.4f\n",
						(r4 >= 0x00100000 && r4 < 0x00180000) ? debug_ram_byte(r4 + 0x10) : 0xee, machine().time().as_double());
			else if (addr == 0x0027ed4a)
				logerror("simpps: after len3 send, [+5]=%02x (need 7) t=%.4f\n", ack, machine().time().as_double());
			else if (addr == 0x0027ed5c)
				logerror("simpps: after recv, [+5]=%02x t=%.4f\n", ack, machine().time().as_double());
			else if (addr == 0x0027ed6e)
				logerror("simpps: cmp result r0=%d t=%.4f\n", int32_t(r0), machine().time().as_double());
		}
	}
	// TRACE_SIMPPS: hook the memcmp 0x2b58e8 itself (a call target, so the fetch hook fires) when called
	// from the PPS compare site (LR==0x27ed6e) — dumps the actual operands the firmware compares.
	if (nokia_env_u32("NOKI3210_TRACE_SIMPPS", 0) != 0 && pc == addr && addr == 0x002b58e8 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x0027ed6e)
	{
		const u32 c0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 c1 = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		const u32 c2 = m_maincpu->state_int(arm7_cpu_device::ARM7_R2);
		auto d = [&](u32 p)->std::string {
			if (p < 0x00100000 || p >= 0x00180000) return std::string("<non-ram>");
			char b[64]; int k = 0; for (u32 i = 0; i < c2 && i < 8; i++) k += std::snprintf(b+k, sizeof(b)-k, "%02x ", debug_ram_byte(p+i));
			return std::string(b);
		};
		static unsigned mc = 0;
		if (mc++ < 20)
			logerror("simpps: CMP(sb=%08x [%s], sent=%08x [%s], len=%u) t=%.4f\n",
					c0, d(c0).c_str(), c1, d(c1).c_str(), c2, machine().time().as_double());
	}
	// MODEL_SIM_RESPONDER increment (b): respond to the SIM command so the phone advances. The phone
	// recv's the response inside the command sender 0x27e98c (bl 0x26a458 at 0x27e9ca, return 0x27e9ce).
	// Trampoline that recv only (LR==0x27e9ce): return a synthetic response message (scratch RAM) with
	// [msg+4]=the response code (default 0xa4 = the T=0 procedure byte ACK for INS a4), so the phone
	// proceeds to send the SELECT data (the 2-byte file ID). Empirical: iterate SIM_RESP_CODE / fields.
	if (nokia_env_u32("NOKI3210_MODEL_SIM_RESPONDER", 0) != 0 && pc == addr && addr == 0x0026a458 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x0027e9ce)
	{
		constexpr u32 SCRATCH = 0x0017fe00;   // high RAM, response message (not pool-freed by 0x27e98c)
		for (offs_t i = 0; i < 0x20; i++) debug_ram_byte_w(SCRATCH + i, 0);
		debug_ram_byte_w(SCRATCH + 4, nokia_env_u32("NOKI3210_SIM_RESP_CODE", 0xa4) & 0xff);
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, SCRATCH);
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R12, 0x0027e9ce | 1);
		// c2 feeder: once we're answering SIM commands we're in the file-read phase -> arm the loop feed.
		if (nokia_env_u32("NOKI3210_MODEL_SIM_LOOP", 0) != 0) m_sim_loop = true;
		static unsigned rp = 0;
		if (rp++ < 40)
			logerror("sim_resp: injected response [+4]=%02x -> 0x27e9ce t=%.4f\n",
					nokia_env_u32("NOKI3210_SIM_RESP_CODE", 0xa4) & 0xff, machine().time().as_double());
		return uint16_t(0x4760);   // BX r12 -> return to 0x27e9ce with r0=response
	}
	// EXPERIMENT_MMI_IDLE (opt-in): at the MMI idle gate 0x297ffa (ldrb r0,[r4,#5]; cmp 1 -> display_idle
	// 0x2a255c), force the idle flag [0x1116fd] = 1 once (after EXPERIMENT_MMI_IDLE_MS ms so the SIM read is
	// done), to see whether display_idle draws a REAL idle screen now (SIM accepted, MMI alive) or black
	// (content pipeline not up, as the old plain-limp FORCE_IDLE_JUMP did).
	if (nokia_env_u32("NOKI3210_EXPERIMENT_MMI_IDLE", 0) != 0 && pc == addr && addr == 0x00297ffa
			&& !m_mmi_idle_forced
			&& machine().time().as_double() * 1000.0 >= nokia_env_u32("NOKI3210_EXPERIMENT_MMI_IDLE_MS", 1000))
	{
		debug_ram_byte_w(0x001116fd, 1);
		// EXPERIMENT_MMI_IDLE_DREADY: also force the display-ready flag [0x11fee4]=1 so the resource check
		// 0x2b12b4 gets past its "display not ready" early-out -- test whether the idle draw then clears the
		// resource-get 0x2b257e (render post 0x2af6ea would fire) or is gated further (the RAM bitmap).
		if (nokia_env_u32("NOKI3210_EXPERIMENT_MMI_IDLE_DREADY", 0) != 0)
			debug_ram_byte_w(0x0011fee4, 1);
		m_mmi_idle_forced = true;
		logerror("mmi_idle: forced [1116fd]=1 (dready=%u) at gate 0x297ffa t=%.4f\n",
				nokia_env_u32("NOKI3210_EXPERIMENT_MMI_IDLE_DREADY", 0), machine().time().as_double());
	}
	// EXPERIMENT_MMI_IDLE: trace display_idle (0x2a255c) entry, and 0x2b257e when called FROM display_idle
	// (LR==0x2a2566, the resource get for 0x224c). If 0x2b257e is entered but the render post 0x2af6ea
	// (via TRACE_RENDER) never follows, the idle draw hangs acquiring resource 0x224c.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_MMI_IDLE", 0) != 0 && pc == addr && addr == 0x002a255c)
	{
		static unsigned di = 0;
		// [0x11fee4] = display-ready flag gating resource availability (0x2b12b4 returns 0 if this is 0).
		// [0x2e5c2f + (0x224c&7)] and [0x1108ff + (0x224c>>3)] are the resource bitmaps for id 0x224c.
		const uint8_t dready = debug_ram_byte(0x0011fee4);
		if (di++ < 20) logerror("mmi_idle: display_idle EXECUTED; display-ready[11fee4]=%02x t=%.4f\n",
				dready, machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_EXPERIMENT_MMI_IDLE", 0) != 0 && pc == addr && addr == 0x002b257e &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x002a2566)
	{
		static unsigned dg = 0;
		if (dg++ < 20) logerror("mmi_idle: display_idle -> resource-get 0x2b257e(0x224c) entered t=%.4f\n", machine().time().as_double());
	}
	// EXPERIMENT_MMI_IDLE: log the sub-calls of the resource-get 0x2b257e (called with LR in its range) to
	// find where it hangs -- the last one logged before the run ends is the blocking sub-function.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_MMI_IDLE", 0) != 0 && pc == addr && m_mmi_idle_forced &&
			(addr == 0x002b12b4 || addr == 0x002b2560 || addr == 0x002b6680 || addr == 0x002b6638 || addr == 0x002b12dc))
	{
		const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		if (lr >= 0x002b2580 && lr < 0x002b26a0)
		{
			static unsigned sg = 0;
			if (sg++ < 30) logerror("mmi_idle: resget-subcall %06x (lr=%06x) t=%.4f\n", addr, lr, machine().time().as_double());
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
	// MODEL_SIM_LOOP (opt-in, c2 feeder): drive the file-read loop. Armed by the command recv-trampoline
	// above (file-read phase). At the SIM-task recv 0x27df0c (bl 0x26a458, return 0x27df10), once armed,
	// inject a synthetic code-0xb SIM message carrying file data: 0x27df64 copies its data (msg+5, len
	// [msg+2]) into the response buffer 0x10dddc, returns 0xb -> the continue path processes it and sends
	// the next command. Data from NOKI3210_SIM_LOOP_HEX (default a bare SW=9000).
	if (nokia_env_u32("NOKI3210_MODEL_SIM_LOOP", 0) != 0 && m_sim_loop && pc == addr && addr == 0x0026a458 &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x0027df10)
	{
		constexpr u32 SCRATCH = 0x0017fd00;
		uint8_t data[40]; unsigned n = 0;
		// data = the current scripted command (SIM_LOOP_SCRIPT, comma-separated), advancing per feed;
		// else SIM_LOOP_HEX; else SW=9000. The code-3 handler copies this to 0x10deec, the buffer the
		// file-read block reads its 5-byte APDU from (0x10deec+5).
		if (const char *scr = getenv("NOKI3210_SIM_LOOP_SCRIPT"))
		{
			unsigned ncmd = 1; for (const char *q = scr; *q; q++) if (*q == ',') ncmd++;
			const char *p = scr; for (unsigned k = 0, idx = m_sim_script_idx % ncmd; k < idx; k++) { while (*p && *p != ',') p++; if (*p) p++; }
			for (; p[0] && p[1] && p[0] != ',' && n < sizeof(data); p += 2)
				data[n++] = uint8_t(std::strtoul(std::string(p, 2).c_str(), nullptr, 16));
			m_sim_script_idx++;
		}
		else if (const char *hex = getenv("NOKI3210_SIM_LOOP_HEX"))
			for (; hex[0] && hex[1] && n < sizeof(data); hex += 2)
				data[n++] = uint8_t(std::strtoul(std::string(hex, 2).c_str(), nullptr, 16));
		// SIM_LOOP_ECHO (FS responder, pass 2): a PPS exchange (command starts with PPSS=0xFF) requires the
		// SIM to ECHO the request verbatim. When the last command the phone sent was a PPS, answer with its
		// exact bytes so the protocol negotiation completes instead of failing to a giveup.
		if (nokia_env_u32("NOKI3210_SIM_LOOP_ECHO", 0) != 0 && m_sim_last_cmdlen > 0 && m_sim_last_cmd[0] == 0xff)
		{
			n = 0;
			for (unsigned i = 0; i < m_sim_last_cmdlen && n < sizeof(data); i++) data[n++] = m_sim_last_cmd[i];
		}
		if (n == 0) { data[0] = 0x90; data[1] = 0x00; n = 2; }   // default: SW=9000
		// Cap the number of feeds (SIM_LOOP_MAX, default 24) so a wrong code can't spin forever: once the
		// cap is hit, fall through to the real (blocking) recv.
		static unsigned lf = 0;
		if (lf >= nokia_env_u32("NOKI3210_SIM_LOOP_MAX", 24)) { /* fall through to real recv */ }
		else {
			for (offs_t i = 0; i < 0x30; i++) debug_ram_byte_w(SCRATCH + i, 0);
			debug_ram_byte_w(SCRATCH + 2, uint8_t(n >> 8));
			debug_ram_byte_w(SCRATCH + 3, uint8_t(n));
			debug_ram_byte_w(SCRATCH + 4, nokia_env_u32("NOKI3210_SIM_LOOP_CODE", 0x0b) & 0xff);
			for (unsigned i = 0; i < n; i++) debug_ram_byte_w(SCRATCH + 5 + i, data[i]);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, SCRATCH);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R12, 0x0027df10 | 1);
			if (lf++ < 40) logerror("sim_loop: fed code-%02x msg (%u data bytes) -> 0x27df10 t=%.4f\n",
					nokia_env_u32("NOKI3210_SIM_LOOP_CODE", 0x0b) & 0xff, n, machine().time().as_double());
			return uint16_t(0x4760);   // BX r12 -> return to 0x27df10 with r0=message
		}
	}
	// MODEL_SIM_LOOP caller-gate: the code-3 message triggers the file-read at the trigger recvs, but the
	// file-read LOOP recv 0x27ee52 (return 0x27ee56) then gets that same code 3 and rejects it. Override
	// the returned code to 0xb at the loop return so the loop PROCESSES (0x27ee94 -> send next) instead of
	// rejecting. 0x27ee56 is `strb r0,[r4,#5]` (stores the code); set r0=0xb before it runs.
	if (nokia_env_u32("NOKI3210_MODEL_SIM_LOOP", 0) != 0 && m_sim_loop && pc == addr && addr == 0x0027ee56)
	{
		// 0xb = "process, keep reading"; once the script has been walked (idx >= SIM_LOOP_DONE) return
		// code 1 = completion (-> 0x27ef34 -> read done -> accept), to try to end the read and reach idle.
		const u32 done = nokia_env_u32("NOKI3210_SIM_LOOP_DONE", 0xffff);
		// completion code: 0xd routes to the SUCCESS path 0x27ee72 (events 0xe8/0xea, no no-SIM check);
		// 1 routes to 0x27ef34 -> 0x27f06e -> no-SIM handler 0x27ea88. Default 0xd (the success route).
		const u32 done_code = nokia_env_u32("NOKI3210_SIM_LOOP_DONE_CODE", 0x0d);
		const u32 code = (m_sim_script_idx >= done) ? done_code : 0x0b;
		m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, code);
		static unsigned ov = 0;
		if (ov++ < 40) logerror("sim_gate: loop recv 0x27ee56 -> code %u (script idx %u) t=%.4f\n",
				code, m_sim_script_idx, machine().time().as_double());
	}
	// TRACE_SIMPATH (opt-in): log which SIM state-machine decision points actually execute, to find the
	// REAL reject path empirically (static reading of this machine has been error-prone). r4=0x10dca8.
	if (nokia_env_u32("NOKI3210_TRACE_SIMPATH", 0) != 0 && pc == addr)
	{
		const char *w =
			addr == 0x0027e046 ? "parser_entry" : addr == 0x0027e556 ? "accept_decision" :
			addr == 0x0027e560 ? "e=2_continue" : addr == 0x0027e570 ? "e=3_ACCEPT" :
			addr == 0x0027ef9e ? "a=e" : addr == 0x0027efbc ? "reject_check_a==2" :
			addr == 0x0027efde ? "post_0x15_setup" : addr == 0x0027ebb2 ? "a=sp28(2)" :
			addr == 0x0027ec9c ? "process_resp" : addr == 0x0027f04c ? "proceed_a=sp28" :
			addr == 0x0027efee ? "giveup_bump" :
			addr == 0x0027ed3c ? "cxx=1_FILEREAD_LOOP" : addr == 0x0027ee72 ? "fileread_cmd_ready" :
			addr == 0x0027eda8 ? "cxx=0_path" : addr == 0x0027ebee ? "cxx=2/3_reset" :
			addr == 0x0027eb86 ? "TIMEOUT_code6_SP++" : addr == 0x0027ebbc ? "code5_handler" :
			addr == 0x0027f020 ? "else_dispatch_path" : nullptr;
		if (w)
		{
			static unsigned pt = 0;
			if (pt++ < 60)
				logerror("simpath: %-18s @%08x  [+9]=%02x [+a]=%02x [+e]=%02x [+10]=%02x t=%.4f\n", w, addr,
						debug_ram_byte(0x0010dca8+9), debug_ram_byte(0x0010dca8+0xa),
						debug_ram_byte(0x0010dca8+0xe), debug_ram_byte(0x0010dca8+0x10), machine().time().as_double());
		}
	}
	// MODEL_SIM_FILE (opt-in, c2 probe): feed a file response into the code-3 buffer 0x10deec (the
	// file-data channel from c1) at the code-5 handler entry 0x27ebbc, to see if a non-empty buffer
	// changes the reject (status 0x15). First cut: a plausible GSM 11.11 MF 3F00 SELECT-response block
	// at 0x10deec+5 (msg data ptr), header/[+4]=3 at 0x10deec. Empirical -- iterate format from result.
	if (nokia_env_u32("NOKI3210_MODEL_SIM_FILE", 0) != 0 && pc == addr && addr == 0x0027ebbc)
	{
		// GSM MF (3F00) GET-RESPONSE data: RFU, mem, file ID 3F00, type 01(MF), RFU, len, chars, #DF, #EF...
		static const uint8_t mf[] = { 0x00,0x00,0x00,0x00,0x3f,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
									  0x0a,0x00,0x05,0x0a,0x04,0x00,0x8a,0x8a,0x8a,0x8a,0x8a };
		for (offs_t i = 0; i < 5; i++) debug_ram_byte_w(0x0010deec + i, 0);
		debug_ram_byte_w(0x0010deec + 4, 3);                       // message code = 3 (code-3 data)
		debug_ram_byte_w(0x0010deec + 2, uint8_t(sizeof(mf)));     // length guess
		for (offs_t i = 0; i < sizeof(mf); i++) debug_ram_byte_w(0x0010deec + 5 + i, mf[i]);
		static unsigned ff = 0;
		if (ff++ < 8) logerror("sim_file: seeded 0x10deec with MF SELECT response (%u bytes) t=%.4f\n",
				unsigned(sizeof(mf)), machine().time().as_double());
	}
	// TRACE_SIMSTATUS (opt-in, c0): the SIM manager posts a SIM status to the MMI via 0x27e240
	// (r1 = status code: 0x15/0x16/...). Log each to find which code = "SIM card rejected" vs accept.
	if (nokia_env_u32("NOKI3210_TRACE_SIMSTATUS", 0) != 0 && pc == addr && addr == 0x0027e240)
	{
		static unsigned st = 0;
		if (st++ < 40)
			logerror("simstatus: post status=%02x lr=%08x t=%.4f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
	}
	// TRACE_SIMTX (opt-in, step 3): the SIM command sender 0x27e98c queues the outgoing SIM command
	// (APDU: SELECT/READ with the file ID) via the TxD-queue fn 0x2a02e6 (r0 = buffer; buffer[0]=len,
	// buffer[1..]=command bytes). Log the command to see which SIM files the phone requests.
	if (nokia_env_u32("NOKI3210_TRACE_SIMTX", 0) != 0 && pc == addr && addr == 0x002a02e6)
	{
		const u32 buf = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (buf >= 0x00100000 && buf < 0x00180000)
		{
			const uint8_t len = debug_ram_byte(buf);
			char b[128]; int n = 0;
			for (unsigned i = 1; i <= len && i <= 20; i++) n += std::snprintf(b + n, sizeof(b) - n, "%02x ", debug_ram_byte(buf + i));
			static unsigned tx = 0;
			if (tx++ < 40)
				logerror("simtx: cmd len=%u bytes: %s lr=%08x t=%.4f\n", len, b,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
		}
	}
	// EXPERIMENT (opt-in, Phase-1 SIM probe): force the SIM task dispatch (0x27df1c) to take the
	// code-0xc "SIM present" path once, after the SIM has done its reset attempts. Code 0xc sets the
	// SIM-ready flags ([0x10a8dd],[0x10a8e3],[0x113cff]) and signals startup-ready (0x279486). Tests
	// whether "SIM present" alone breaks the Insert-SIM-card retry loop and advances the boot.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_SIM_PRESENT", 0) != 0 && pc == addr && addr == 0x0027df1c)
	{
		static unsigned n = 0;
		n++;
		if (n == nokia_env_u32("NOKI3210_SIM_PRESENT_AFTER", 6))
		{
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R8, 0x0c);   // -> code-0xc handler 0x27df52
			logerror("sim_present: forced dispatch #%u code=0xc at t=%.4f\n", n, machine().time().as_double());
		}
	}
	// TRACE_SVCREADY (opt-in, diagnostic): the block-2 (app-task) resume gate at 0x2a9182 needs
	// 0x29bafc()==1 (reads service-ready [0x11fed1] bit7), phase [0x110c2c]==1, and [0x11239c]!=3.
	// Hook the startup-service fn entries + the gate to see which run and what the gate variables hold.
	if (nokia_env_u32("NOKI3210_TRACE_SVCREADY", 0) != 0 && pc == addr &&
		(addr == 0x00290b54 || addr == 0x00290e42 || addr == 0x00291034 ||
		 addr == 0x0029b700 || addr == 0x0029bafc || addr == 0x002a9182 ||
		 addr == 0x002a91ac || addr == 0x002a91f0 || addr == 0x002a9216 ||
		 addr == 0x0029bb06 || addr == 0x0029bb16 || addr == 0x0029bb1a || addr == 0x002a9186))
	{
		static unsigned svc = 0;
		if (svc++ < 80)
		{
			const char *nm = addr == 0x00290b54 ? "svc_table_init" : addr == 0x00290e42 ? "svc_buf_update"
						   : addr == 0x00291034 ? "svc_mode8_commit" : addr == 0x0029b700 ? "init_29b700"
						   : addr == 0x0029bafc ? "GATE_29bafc" : addr == 0x002a9182 ? "block2_gate_2a9182"
						   : addr == 0x002a91ac ? "BLOCK2_RESUMES!" : addr == 0x002a91f0 ? "gate_FAIL_2a91f0"
						   : addr == 0x002a9216 ? "func_exit_2a9216" : addr == 0x0029bb06 ? "29bafc_waitloop"
						   : addr == 0x0029bb16 ? "29bafc_ret1" : addr == 0x0029bb1a ? "29bafc_ret0_post19"
						   : "ret_2a9186";
			const u32 r7 = m_maincpu->state_int(arm7_cpu_device::ARM7_R7);
			const u32 r6r = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
			logerror("svcready: %-18s [11fed1]=%02x(b2=%u,b6=%u) phase[110c2c]=%02x [11239c]=%02x [11ff41]=%02x "
					"r7=%08x [r7]=%02x [r7+1]=%02x r6=%08x t=%.4f\n",
					nm, debug_ram_byte(0x0011fed1), (debug_ram_byte(0x0011fed1) >> 2) & 1, (debug_ram_byte(0x0011fed1) >> 6) & 1,
					debug_ram_byte(0x00110c2c), debug_ram_byte(0x0011239c), debug_ram_byte(0x0011ff41),
					r7, (r7 >= 0x00100000 && r7 < 0x00180000) ? debug_ram_byte(r7) : 0xff,
					(r7 >= 0x00100000 && r7 < 0x00180000) ? debug_ram_byte(r7 + 1) : 0xff,
					r6r, machine().time().as_double());
		}
	}
	// TRACE_RESUME (opt-in, diagnostic): task make-ready primitive 0x269c6e (r0=task index; TCB at
	// 0x1093bc+idx*0x10, state[+0xd] 5=dormant->2=ready). Shows which of the 23 registered tasks get
	// resumed vs stay dormant (reporter tasks are indices 10..22), and from where (LR = the boot phase).
	if (nokia_env_u32("NOKI3210_TRACE_RESUME", 0) != 0 && pc == addr && addr == 0x00269c6e)
	{
		static unsigned resc = 0;
		if (resc++ < 80)
			logerror("resume: task=%02u state[+d]=%u lr=%08x t=%.4f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
					debug_ram_byte(0x001093bc + (m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff) * 0x10 + 0xd),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SWEEP15", 0) != 0 && pc == addr &&
		(addr == 0x002520ec || addr == 0x002521cc))
	{
		static unsigned sc = 0;
		if (sc++ < 60)
		{
			char b[64]; int n = 0;
			for (offs_t k = 0; k < 0x0b; k++) n += std::snprintf(b + n, sizeof(b) - n, "%d", debug_ram_byte(0x00112280 + k) ? 1 : 0);
			logerror("sweep15: %s code=%02x lr=%08x checklist[0..a]=%s t=%.4f\n",
					addr == 0x002521cc ? "POST-0x15" : "mark",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1), b,
					machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_FORCE_IDLE", 0) != 0 && pc == addr && addr == 0x00298008)
		debug_ram_byte_w(0x0011f81b, 1);   // pin MMI idle-redraw flag so display_idle fires
	// EXPERIMENT (opt-in, option C — hollow idle, done RIGHT): FORCE_IDLE alone fails because the MMI
	// task is parked in the blocking recv at 0x298008 and never loops back to the flag-check at 0x297ffa.
	// TRACE_MMI proved the MMI runs and reaches 0x298008 a few times (t<=0.84) then blocks forever. So we
	// trampoline PAST the recv: on the Nth fetch of 0x298008 (after the initial display-init messages have
	// been processed), set the idle flag and BX to 0x297ffa so display_idle (0x2a255c) actually renders.
	// This drives the layer that draws screens (the MMI), not the startup machine (which never draws).
	if (nokia_env_u32("NOKI3210_FORCE_IDLE_JUMP", 0) != 0 && pc == addr && addr == 0x00298008)
	{
		static bool ij_done = false;
		static unsigned ij = 0;
		ij++;
		// Fire once. Two gating modes: FORCE_IDLE_JUMP_MS (fire on first recv at/after that sim-time —
		// use when combined with a mode-advance knob so the display controller is initialised first) or,
		// if unset, the Nth recv (bare limp, display never inits past t=0.84). Draw idle, then let later
		// fetches recv normally so the framebuffer holds the idle image.
		const u32 gate_ms = nokia_env_u32("NOKI3210_FORCE_IDLE_JUMP_MS", 0);
		const bool fire = gate_ms ? (!ij_done && machine().time().as_double() * 1000.0 >= gate_ms)
								  : (ij == nokia_env_u32("NOKI3210_FORCE_IDLE_JUMP_AFTER", 3));
		if (fire)
		{
			ij_done = true;
			debug_ram_byte_w(0x0011f81b, 1);                                        // idle-redraw flag
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R4, 0x0011f816);         // flag base (loaded at 0x297ff4, which we skip)
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R12, 0x00297ffa | 1);    // -> flag-check
			logerror("force_idle_jump: bypass recv #%u -> 0x297ffa t=%.4f\n",
					ij, machine().time().as_double());
			return uint16_t(0x4760);                                                // BX r12
		}
	}
	// EXPERIMENT (opt-in, option C): thread the enumerated normal-boot path to outcome 3, PC-specific so
	// side effects stay coherent (no blunt event injection). Requires MARCH (feeds mode 4 -> event 7, which
	// lands in the 000c post-charger accumulator at 0x2712c0). Chain: fill accumulator flags in RAM + feed
	// event 0xd -> flag-check -> 0x271354([0x112398]==0) -> 0x271364 boot decision; charger_present_check
	// (ADC ch5=0) -> normal boot -> wait 0x74 -> gate2 (0x2b2f90==0x80) + [0x11239d]==1 -> commit outcome 3.
	if (nokia_env_u32("NOKI3210_EXPERIMENT_BOOTPATH", 0) != 0 && pc == addr)
	{
		if (addr == 0x002712ca)          // 000c recv return: pre-fill flags, feed event 0xd
		{
			for (offs_t f = 0x00112390; f <= 0x00112395; f++) debug_ram_byte_w(f, 1);
			debug_ram_byte_w(0x00112398, 0);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x0d);
		}
		else if (addr == 0x0027138e)     // the 0x74 wait uses a BLOCKING recv (0x26ff14->0x26a458) on an
		{                                // empty ring. Trampoline PAST it: land at 0x271392 with r0=0x74 so
			debug_ram_byte_w(0x0011239d, 1);                       // gate3 ([0x11239d]==1) up front
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x74);
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R12, 0x00271392 | 1);
			logerror("bootpath: trampoline 0x74 at 0x27138e t=%.4f\n", machine().time().as_double());
			return uint16_t(0x4760);     // BX r12 -> 0x271392 (skips the blocking recv)
		}
		else if (addr == 0x002713a6)     // gate2: force 0x2b2f90 result to 0x80
			m_maincpu->set_state_int(arm7_cpu_device::ARM7_R0, 0x80);
		else if (addr == 0x00271364 || addr == 0x00271422 || addr == 0x00271392 ||
				 addr == 0x002713b2 || addr == 0x002714fe || addr == 0x002b4dda ||
				 addr == 0x002a924c || addr == 0x002a92fc || addr == 0x002a934c ||
				 addr == 0x002a933a || addr == 0x00298000 || addr == 0x002a255c)
		{
			static unsigned bc = 0;
			if (bc++ < 40)
				logerror("bootpath: hit %06x mode=%04x r0=%02x [1150ff]=%02x t=%.4f\n", addr,
						debug_ram_word(FW_STARTUP_MODE), m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
						debug_ram_byte(0x001150ff), machine().time().as_double());
		}
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

	// TRACE_SIM (opt-in): SIM UART/ISO-7816 traffic (regs 0x36-0x3f) to scope SIM emulation.
	if (nokia_env_u32("NOKI3210_TRACE_SIM", 0) != 0 && offset >= 0x36 && offset <= 0x3f)
	{
		static unsigned sr = 0;
		if (sr++ < 120)
			logerror("sim R %02x=%02x (%s) pc=%08x t=%.4f\n", offset, data, nokia_mad2_reg_desc(offset),
					m_maincpu->pc(), machine().time().as_double());
	}

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
			// MODEL_SIM_ATR (opt-in, faithful SIM build): serve the ATR from a register-level FIFO.
			if (nokia_env_u32("NOKI3210_MODEL_SIM_ATR", 0) != 0 && m_sim_atr_pos < m_sim_atr_len)
			{
				data = m_sim_atr[m_sim_atr_pos++];
				if (nokia_env_u32("NOKI3210_TRACE_SIM", 0) != 0)
					logerror("sim_atr: RxD byte %u/%u = %02x pc=%08x t=%.4f\n",
							m_sim_atr_pos, m_sim_atr_len, data, m_maincpu->pc(), machine().time().as_double());
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

	if (nokia_env_u32("NOKI3210_TRACE_SIM", 0) != 0 && offset >= 0x36 && offset <= 0x3f)
	{
		static unsigned sw = 0;
		if (sw++ < 120)
			logerror("sim W %02x=%02x (%s) pc=%08x t=%.4f\n", offset, data, nokia_mad2_reg_desc(offset),
					pc, machine().time().as_double());
	}

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
		if (nokia_env_u32("NOKI3210_TRACE_SIM", 0) != 0)
			logerror("sim_atr: ARMED %u-byte ATR on activate (0x39=%02x) pc=%08x t=%.4f\n",
					m_sim_atr_len, data, pc, machine().time().as_double());
		// The SIM RxD reception is interrupt-driven; raise the SIM IRQ so the ISR drains the ATR.
		// Line is configurable while we pin it down (DSP=4, CCONT=6 are taken); default 5.
		const u32 line = nokia_env_u32("NOKI3210_SIM_IRQ_LINE", 5);
		if (line < 8)
			assert_irq(int(line));
	}

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
