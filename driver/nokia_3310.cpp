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
#include "machine/i2cmem.h"
#include "machine/intelfsh.h"
#include "video/pcd8544.h"

#include "nokia_ccont.h"
#include "nokia_dsp_peer.h"
#include "nokia_sim_card.h"

#include "debugger.h"
#include "emupal.h"
#include "screen.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

#define LOG_MAD2_REGISTER_ACCESS    (1U << 1)

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
// PWRONX is latched as CCONT status bit 1 on a cold power-key boot. It is a
// reset cause sampled by firmware, not one of the upper interrupt sources.
constexpr uint8_t CCONT_BOOT_IRQ_DEFAULT = 0x02;
constexpr uint8_t CCONT_IRQ_LINE_NUM = 6;         // MAD2 IRQ line the CCONT asserts

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
constexpr offs_t FW_CCONT_STATE = 0x11ff6c;

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

// Checksums validated in the 3210 v6.00 firmware. Routine 0x264c56 reads
// 0x0000..0x011f, sums 0x11e bytes, and compares the result with the 32-bit
// big-endian word at 0x011c. The config block has a separate check at 0x234810.
// Generic service tools describe finer tune/security sub-blocks, but those are
// not firmware-validated contracts for this ROM. See docs/eeprom_analysis.md.
constexpr uint16_t FW_EEPROM_TUNE_SECURITY_START  = 0x0000;
constexpr uint16_t FW_EEPROM_TUNE_SECURITY_CKSUM  = 0x011c;
constexpr uint16_t FW_EEPROM_CONFIG_BLOCK_START   = 0x0120;  // contact-service config
constexpr uint16_t FW_EEPROM_CONFIG_BLOCK_CKSUM   = 0x0244;  // BE sum16(-corr) of [0x0120..0x0243]

// Startup modes named from the traced boot progression.
constexpr uint16_t FW_STARTUP_MODE_POST_SELFTEST = 0x0004;
constexpr uint16_t FW_STARTUP_MODE_READY_GATE = 0x0005;
constexpr uint16_t FW_STARTUP_MODE_SERVICE_QUIESCE_GATE = 0x0006;
constexpr uint16_t FW_STARTUP_MODE_BATTERY_READY_GATE = 0x0007;

constexpr uint16_t FW_STARTUP_EVENT_CCONT_BATTERY_COMPLETE = 0x0015;
constexpr uint16_t FW_STARTUP_EVENT_PHASE5_CONTINUE = 0x0003;

class noki3310_state : public driver_device
{
public:
	noki3310_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_flash(*this, "flash"),
		m_eeprom(*this, "eeprom"),
		m_ccont(*this, "ccont"),
		m_dsp_peer(*this, "dsp_peer"),
		m_sim_card(*this, "sim_card"),
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

	uint16_t ram_r(offs_t offset, uint16_t mem_mask = ~0);
	uint16_t ram_r_firmware_overrides(offs_t offset, uint16_t mem_mask);
	void ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	void ram_w_firmware_traces(offs_t offset, uint16_t data, uint16_t mem_mask);
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

	void assert_fiq(int num);
	void assert_irq(int num);
	void ack_fiq(uint16_t mask);
	void ack_irq(uint16_t mask);
	void update_fiq_line();
	void update_irq_line();
	void ccont_irq_w(int state);
	void ccont_power_w(int state);
	void sim_irq_w(int state);
	bool timer0_compare_due() const;
	void update_timer0_compare();
	void schedule_mbus_fiq(int num);
	void signal_mbus_fiq(int num);
	void complete_mbus_transfer();
	void dsp_fiq0_w(int state);
	void dsp_service_irq_w(int state);
	uint8_t keypad_irq_state() const;
	uint16_t fw_word(offs_t address) const;
	uint8_t fw_byte(offs_t address) const;
	uint32_t fw_dword(offs_t address) const;
	void fw_word_w(offs_t address, uint16_t data);
	void fw_byte_w(offs_t address, uint8_t data);
	uint16_t debug_ram_word(offs_t address) const { return fw_word(address); }
	uint8_t debug_ram_byte(offs_t address) const { return fw_byte(address); }
	void debug_ram_word_w(offs_t address, uint16_t data) { fw_word_w(address, data); }
	void debug_ram_byte_w(offs_t address, uint8_t data) { fw_byte_w(address, data); }
	required_device<cpu_device> m_maincpu;
	required_device<intelfsh16_device> m_flash;
	required_device<i2c_24c128_device> m_eeprom;
	required_device<nokia_ccont_device> m_ccont;
	required_device<nokia_dsp_peer_device> m_dsp_peer;
	required_device<nokia_sim_card_device> m_sim_card;
	required_device<pcd8544_device> m_pcd8544;
	required_ioport_array<5> m_keypad;
	required_ioport m_pwr;

	std::unique_ptr<uint16_t[]>   m_ram;

	uint8_t       m_power_on;
	uint16_t      m_fiq_status;
	uint16_t      m_irq_status;
	uint16_t      m_timer1_counter;
	uint16_t      m_timer0_counter;
	uint8_t       m_timer0_divider;
	bool          m_timer0_compare_latched;
	uint8_t       m_keypad_irq_state;

	uint32_t      m_power_irq_count;

	emu_timer * m_timer0;
	emu_timer * m_timer1;
	emu_timer * m_timer_watchdog;
	emu_timer * m_timer_fiq8;
	emu_timer * m_timer_mbus;
	emu_timer * m_timer_power_irq;
	emu_timer * m_timer_keypad;

	uint8_t       m_mad2_regs[0x100];
	bool          m_mad2_trace_read[0x100] = {false};
	bool          m_mad2_trace_write[0x100] = {false};
	unsigned      m_gensio_trace_count = 0;
	uint8_t       m_gensio_status = 0x03;
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
// environment (overridable on the `make run` command line). Four kinds:
//
//   1. HARDWARE CONFIG — selects a hardware *scenario*, not firmware behaviour:
//      display variant (DISPLAY_TYPE), power/ADC (ADC_PROFILE, POWER_IRQ_*,
//      DISABLE_CCONT_WATCHDOG),
//      clocks (TIMER0_HZ/TIMER1_HZ/TIMER0_CATCHUP, FIQ8_HZ), NV (EEPROM_PROFILE),
	//      SIM UART/card fixture. The default boot (none set)
//      reproduces the CONTACT SERVICE oracle frame byte-for-byte.
//
//   2. DEVICE-BOUNDARY MODELS — opt-in behavior behind an ordinary hardware
//      interface. MODEL_SIM_DEVICE owns SIMI/FIQ6. MODEL_CCONT_PRESENT selects
//      the extracted CCONT device scenario. MODEL_DSP_SERVICE and
//      MODEL_DSP_CONTACT_PEER model DSP-owned counters, ring consumption, and
//      request-derived replies; their wider semantic contract remains incomplete.
//   3. DIAGNOSTIC TAPS (TRACE_*) — opt-in, log-only, no state change. A curated few:
//      TRACE_CSCMD (contact-service command stream), TRACE_HANDOFF (task-1 master
//      sequencer mode + startup checklist; the post-SIM interactive handoff),
//      TRACE_TASKS (app-task liveness + inter-task message edges).
//      TRACE_SIM_RX covers the register/FIQ/APDU path and SIM reply milestones;
//      TRACE_DSP_BOUNDARY and TRACE_GSM_SERVICE cover the current peer boundary.
//      Research-force policy: docs/evidence_regime.md.
//
// Traces are quarantined in flash_firmware_traces /
// ram_w_firmware_traces. Add no forced firmware results or messages. See
// docs/driver_structure.md.
// ============================================================================
static unsigned nokia_env_u32(const char *name, unsigned fallback)
{
	// Env vars don't change during a run, so memoise per name: flash_firmware_traces
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

	// allocate timers
	m_timer0 = timer_alloc(FUNC(noki3310_state::timer0), this);
	m_timer1 = timer_alloc(FUNC(noki3310_state::timer1), this);
	m_timer_watchdog = timer_alloc(FUNC(noki3310_state::timer_watchdog), this);
	m_timer_fiq8 = timer_alloc(FUNC(noki3310_state::timer_fiq8), this);
	m_timer_mbus = timer_alloc(FUNC(noki3310_state::timer_mbus), this);
	m_timer_power_irq = timer_alloc(FUNC(noki3310_state::timer_power_irq), this);
	m_timer_keypad = timer_alloc(FUNC(noki3310_state::timer_keypad), this);
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
	return uint32_t(fw_word(address)) | (uint32_t(fw_word(address + 2)) << 16);
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

	// according to the boot rom disassembly here http://www.nokix.pasjagsm.pl/help/blacksphere/sub_100hardware/sub_arm/sub_bootrom.htm
	// flash entry point is at 0x200040, we can probably reassemble the above code, but for now this should be enough.
	m_maincpu->set_state_int(arm7_cpu_device::ARM7_R15, NOKIA_FLASH_ENTRY);

	memset(m_mad2_regs, 0, 0x100);
	std::fill(std::begin(m_mad2_trace_read), std::end(m_mad2_trace_read), false);
	std::fill(std::begin(m_mad2_trace_write), std::end(m_mad2_trace_write), false);
	m_gensio_trace_count = 0;
	m_gensio_status = 0x03;
	m_mad2_regs[MAD2_MCU_RESET_CTRL] = 0x01;   // power-on flag
	m_mad2_regs[MAD2_IRQ_CTRL] = 0x0a;         // disable FIQ and IRQ
	m_mad2_regs[MAD2_WATCHDOG] = 0xff;         // disable MAD2 watchdog
	// Load the ADC source model from the power scenario. Per-channel defaults are the
	// chip's "battery present, no charger" rest state; nokia_adc_override applies the
	// NOKI3210_ADC_PROFILE / ADCn knobs on top, so values are identical to before (the
	// override is constant for a run). The scenario will become a typed object later.
	{
		static const uint16_t adc_default[8] =
				{ 0x000, 0x3ff, 0x3ff, 0x280, 0x200, 0x000, 0x200, 0x000 };
		for (unsigned id = 0; id < 8; id++)
			m_ccont->set_adc_source(id, nokia_adc_override(id, adc_default[id]));
	}
	m_ccont->set_boot_status(CCONT_BOOT_IRQ_DEFAULT);
	m_ccont->set_present(nokia_env_u32("NOKI3210_MODEL_CCONT_PRESENT", 0) != 0);
	m_sim_card->set_enabled(nokia_env_u32("NOKI3210_MODEL_SIM_DEVICE", 0) != 0);
	m_sim_card->set_cphs_aoc(nokia_env_u32("NOKI3210_SIM_CPHS_AOC", 0) != 0);
	{
		u8 atr[40] = { 0x3b, 0x10, 0x05 };
		unsigned length = 3;
		if (const char *hex = std::getenv("NOKI3210_SIM_ATR_HEX"))
		{
			length = 0;
			for (const char *p = hex; p[0] && p[1] && length < std::size(atr); p += 2)
				atr[length++] = u8(std::strtoul(std::string(p, 2).c_str(), nullptr, 16));
		}
		m_sim_card->set_atr(atr, length);
	}
	m_eeprom->write_scl(1);
	m_eeprom->write_sda(1);

	m_fiq_status = 0;
	m_irq_status = 0;
	m_timer1_counter = 0;
	m_timer0_counter = 0;
	m_timer0_divider = 255;
	m_timer0_compare_latched = false;
	m_keypad_irq_state = 0xff;
	m_power_irq_count = 0;

	const unsigned timer0_hz = nokia_env_u32("NOKI3210_TIMER0_HZ", 33055);
	const unsigned timer1_hz = nokia_env_u32("NOKI3210_TIMER1_HZ", 1057);
	const unsigned fiq8_hz = nokia_env_u32("NOKI3210_FIQ8_HZ", 1000);

	m_timer0->adjust(attotime::from_hz(timer0_hz), 0, attotime::from_hz(timer0_hz));    // programmable divider through port 0x0f
	m_timer1->adjust(attotime::from_hz(timer1_hz), 0, attotime::from_hz(timer1_hz));
	m_timer_watchdog->adjust(attotime::from_hz(1), 0, attotime::from_hz(1));
	m_timer_fiq8->adjust(attotime::from_hz(fiq8_hz), 0, attotime::from_hz(fiq8_hz));
	m_timer_mbus->adjust(attotime::never);
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

void noki3310_state::ccont_irq_w(int state)
{
	const uint16_t irq_mask = uint16_t(1) << CCONT_IRQ_LINE_NUM;
	if (state)
		m_irq_status |= irq_mask;
	else
		m_irq_status &= ~irq_mask;
	update_irq_line();
}

void noki3310_state::ccont_power_w(int state)
{
	if (!state)
		m_maincpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
}

void noki3310_state::sim_irq_w(int state)
{
	if (state)
		assert_fiq(6);
}

void noki3310_state::dsp_fiq0_w(int state)
{
	if (state)
		assert_fiq(0);
}

void noki3310_state::dsp_service_irq_w(int state)
{
	if (state)
		assert_irq(4);
}

uint8_t noki3310_state::keypad_irq_state() const
{
	uint8_t data = 0xff;

	for (int i = 0; i < 5; i++)
		data &= m_keypad[i]->read() | 0xe0;

	data &= m_pwr->read() | 0xe0;
	if (nokia_env_u32("NOKI3210_HOLD_POWER_KEY", 0) != 0)
		data &= 0xfe;
	return data;
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

	m_ccont->raise_boot_irq(pulse);
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

TIMER_CALLBACK_MEMBER(noki3310_state::timer_watchdog)
{
	// CCONT watchdog
	if (nokia_env_u32("NOKI3210_DISABLE_CCONT_WATCHDOG", 0) == 0 && m_ccont->watchdog_tick())
	{
		m_maincpu->reset();
		machine_reset();
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
// backing read plus firmware-research shortcuts/traces live in the
// quarantined ram_r_firmware_overrides below.
uint16_t noki3310_state::ram_r(offs_t offset, uint16_t mem_mask)
{
	return ram_r_firmware_overrides(offset, mem_mask);
}

// ============================================================================
// Firmware-research RAM-read path: backing read + two explicit shortcuts which
// can rewrite the returned value + execution traces. NOT clean hardware
// behaviour; should shrink as their real hardware/NV owners are established.
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
// store plus firmware-research traces live in the quarantined
// ram_w_firmware_traces below.
void noki3310_state::ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	ram_w_firmware_traces(offset, data, mem_mask);
}

// ============================================================================
// Firmware-research RAM-write path: execution traces around the real backing
// store (COMBINE_DATA). NOT hardware behaviour; should shrink as investigations
// close.
// ============================================================================
void noki3310_state::ram_w_firmware_traces(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	const offs_t address = 0x100000 + (offset << 1);
	const u32 pc = m_maincpu->pc();
	if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && address == FW_CONTACT_SERVICE_STATUS)
		logerror("csstatus_write: pc=%08x data=%04x mask=%04x old=%04x task=%02x "
				"svc_ready=%02x dsp=%04x/%04x/%04x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], fw_byte(FW_SCHED_RUNNING_TASK_ID),
				fw_byte(FW_STARTUP_SERVICE_READY), m_dsp_peer->shared_r(0x0da >> 1),
				m_dsp_peer->shared_r(0x0e2 >> 1), m_dsp_peer->shared_r(0x0e4 >> 1),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 &&
			(address == 0x0011fedc || address == 0x0011fede))
		logerror("csaddress_write: pc=%08x address=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), u32(address), data, mem_mask,
				m_ram[offset], fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && address == 0x0010dcb6)
		logerror("sim_contract: notify-latch write pc=%08x data=%04x mask=%04x old=%04x task=%02x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], fw_byte(0x00100022),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 &&
			(address == 0x00110e2e || address == 0x0011fd04))
		logerror("code7_activity_write: pc=%08x address=%08x data=%04x mask=%04x old=%04x "
				"task=%02x mode=%04x t=%.6f\n",
				pc & ~u32(1), u32(address), data, mem_mask, m_ram[offset],
				fw_byte(0x00100022), fw_word(FW_STARTUP_MODE), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && address == 0x0011fcd4)
		logerror("code7_gsm_state_write: pc=%08x data=%04x mask=%04x old=%04x "
				"state=%02x selector=%02x input=%04x index=%02x new=%02x task=%02x mode=%04x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], fw_byte(0x0011fcd5),
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R6)) & 0xff,
				fw_word(0x00112086),
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)) & 0xff,
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xff,
				fw_byte(0x00100022), fw_word(FW_STARTUP_MODE), machine().time().as_double());
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
	return m_dsp_peer->shared_r(offset);
}

void noki3310_state::dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	m_dsp_peer->shared_w(offset, data, mem_mask);
}
// ============================================================================
// Firmware-research traces for flash fetches. This is diagnostic observation,
// not hardware behaviour, and never changes the fetched instruction.
// ============================================================================
void noki3310_state::flash_firmware_traces(u32 pc, u32 addr)
{
	if (pc == addr && addr == 0x00290cf4 && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("dsp_boundary: service-command command=%08x arg=%08x payload=%08x caller=%08x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x00100022), machine().time().as_double());
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(addr == 0x002a2074 || addr == 0x0028d710 || addr == 0x0028d7fe || addr == 0x0028d886))
	{
		static unsigned l1_config_count = 0;
		if (l1_config_count++ < 128)
			logerror("dsp_boundary: l1-config pc=%08x caller=%08x task=%02x "
					"flags=%02x current=%02x active=%02x target=%02x applied=%04x requested=%04x "
					"r0=%04x r2=%04x t=%.6f\n",
					addr, m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					fw_byte(0x00100022), fw_byte(0x00110b3b), fw_byte(0x00112813),
					fw_byte(0x00112816), fw_byte(0x00112817), fw_word(0x0011281d),
					fw_word(0x0011281f),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)) & 0xffff,
					machine().time().as_double());
	}
	if (pc == addr && (addr == 0x002a1c88 || addr == 0x002a1ea6) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("dsp_boundary: l1-state-input pc=%08x r0=%08x r1=%08x caller=%08x task=%02x t=%.6f\n",
				addr, m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x00100022), machine().time().as_double());
	if (pc == addr && addr == 0x00291028 && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("dsp_boundary: service-update r0=%08x r1=%08x r2=%08x r3=%08x caller=%08x "
				"a8=%04x ac=%04x ae=%04x b6=%04x bc=%04x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R3),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				m_dsp_peer->shared_r(0x0a8 >> 1), m_dsp_peer->shared_r(0x0ac >> 1),
				m_dsp_peer->shared_r(0x0ae >> 1), m_dsp_peer->shared_r(0x0b6 >> 1),
				m_dsp_peer->shared_r(0x0bc >> 1), fw_byte(0x00100022),
				machine().time().as_double());
	// TRACE_TASKS (opt-in): app-task liveness tap. Log the first time each RTOS task reaches
	// the universal recv 0x26a458 (i.e. is scheduled and runs its message loop). Under the deep profile
	// stack this shows which of the app tasks 10-17 (resumed by mode-0xc's 0x2795e6) actually come
	// alive. Task id = [0x100022]. docs/interactive_handoff.md app-task sweep.
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x0026a458)
	{
		static u32 seen = 0;
		const u32 tid = debug_ram_byte(0x00100022);
		if (tid < 31 && !(seen & (1u << tid)))
		{
			seen |= (1u << tid);
			logerror("task_alive: task %2u first recv caller=%08x t=%.4f\n", tid,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		}
	}
	// Task 1 advances from both startup modes 4 and 7 on report code 7. Trace
	// branch targets around every organic reporter caller and the real getter
	// entry; hooks at the former mid-function 0x26ff14 seam are not reliable.
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr &&
			(addr == 0x0021e3f8 || addr == 0x0021f830 || addr == 0x00255c30 ||
			 addr == 0x0027b370 || addr == 0x0026ff14 || addr == 0x0026ff1a))
	{
		static unsigned code7_owner_count = 0;
		static unsigned task1_getter_count = 0;
		const bool task1_getter = addr == 0x0026ff14 || addr == 0x0026ff1a;
		unsigned &count = task1_getter ? task1_getter_count : code7_owner_count;
		if (count++ < (task1_getter ? 32U : 128U))
			logerror("code7_owner: pc=%08x r0=%08x r1=%08x mode=%04x task=%02x "
					"sim-enable=%02x no-sim=%02x caller=%08x t=%.6f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					debug_ram_word(FW_STARTUP_MODE), debug_ram_byte(0x00100022),
					debug_ram_byte(0x00111c79), debug_ram_byte(0x00111c64),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		if (addr == 0x0026ff1a &&
				(u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff) == 0x00ca)
			logerror("code7_eventca_recv: task=%02x event=%04x caller=%08x mode=%04x t=%.6f\n",
					debug_ram_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					debug_ram_word(FW_STARTUP_MODE), machine().time().as_double());
	}
	// Keypad IRQ6 posts raw event 0x72 to task 1. In startup mode 4, task 1
	// deliberately accepts only report code 7; this trace distinguishes that
	// lifecycle gate from failed interrupt, queue, or matrix hardware.
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr &&
			(addr == 0x002b5da0 || addr == 0x002b5dba || addr == 0x00271256 ||
			 addr == 0x002701b0 || addr == 0x00271266 || addr == 0x002b2f90))
	{
		static unsigned keypad_handoff_count = 0;
		const uint16_t event = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)) & 0xffff;
		const bool startup_dispatch = addr == 0x00271256 || addr == 0x002701b0;
		if ((!startup_dispatch || event == 0x0072 || event == 0x0007) && keypad_handoff_count++ < 128)
			logerror("keypad_handoff: pc=%08x r0=%08x r1=%08x event=%04x mode=%04x task=%02x "
					"irq=%02x mask=%02x rows=%02x caller=%08x t=%.6f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					(addr == 0x002b5da0 || addr == 0x002b2f90) ?
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff : event,
					debug_ram_word(FW_STARTUP_MODE), debug_ram_byte(0x00100022),
					m_irq_status, m_mad2_regs[MAD2_IRQ_MASK], m_mad2_regs[0x28],
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
	}
	// Keep the finite boot-readiness owner surface observable without tracing
	// the full callback engine.  The listed callbacks are every direct 0x05e1
	// publisher recovered statically, plus callback 0x5d which reports code 7.
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr &&
			(addr == 0x002ac652 || addr == 0x002ac65e))
	{
		const u32 record = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R5));
		const u8 selector = (record >= NOKIA_RAM_BASE && record + 3 < NOKIA_RAM_END) ?
			debug_ram_byte(record + 3) : 0xff;
		const bool code7_owner = selector == 0x0f || selector == 0x2f || selector == 0x31 ||
			selector == 0x32 || selector == 0x34 || selector == 0x47 ||
			selector == 0x4e || selector == 0x51 || selector == 0x55 || selector == 0x59 ||
			selector == 0x5d || selector == 0x6d;
		if (code7_owner)
		{
			static unsigned code7_callback_count = 0;
			if (code7_callback_count++ < 512)
				logerror("code7_callback: pc=%08x phase=%s selector=%02x status=%04x return=%04x "
						"state=%02x gate31=%02x/%02x task=%02x mode=%04x caller=%08x t=%.6f\n",
						addr, addr == 0x002ac652 ? "call" : "return",
						selector,
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R6)) & 0xffff,
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
						debug_ram_byte(0x0011fc80 + selector), debug_ram_byte(0x00111e72),
						debug_ram_byte(0x00110f1f), debug_ram_byte(0x00100022),
						debug_ram_word(FW_STARTUP_MODE),
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						machine().time().as_double());
		}
	}
	// Task 19's power manager receives scalar events through 0x21c646, so they
	// do not appear in the pointer-message post trace. Observe the receive result
	// and the normalized event at the two real return targets.
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr &&
			(addr == 0x0021c64c || addr == 0x00220a08) &&
			debug_ram_byte(0x00100022) == 19)
	{
		static unsigned power_event_count = 0;
		const u32 state_base = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R5));
		const u16 state = addr == 0x00220a08 && state_base >= NOKIA_RAM_BASE &&
				state_base + 0x39 < NOKIA_RAM_END ? debug_ram_word(state_base + 0x38) : 0xffff;
		if (power_event_count++ < 128)
			logerror("power_event: pc=%08x raw=%08x event=%04x state=%04x state_base=%08x caller=%08x t=%.6f\n",
				addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
				state, state_base,
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
				machine().time().as_double());
	}
	// Bound the remaining ordinary code-7 candidates at their real firmware
	// seams. These taps observe control flow and state only; they do not alter
	// registration, controller, callback, or task state.
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr)
	{
		static unsigned registration_count = 0;
		static unsigned callback47_count = 0;
		static unsigned controller_count = 0;
		static unsigned controller_activity_count = 0;
		static unsigned controller_selector_count = 0;
		static unsigned publication_count = 0;
		static unsigned init_family_count = 0;
		const u16 task5_status = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff;
		if ((addr == 0x00253e20 || addr == 0x00254590 || addr == 0x00255030) &&
				(task5_status == 0x13fd || task5_status == 0x13fe) && init_family_count++ < 32)
			logerror("code7_init_family: pc=%08x status=%04x source=%08x activity=%02x/%02x/%02x/%02x "
					"guard=%02x task=%02x mode=%04x caller=%08x t=%.6f\n",
					addr, task5_status,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					fw_byte(0x00110e2c), fw_byte(0x00110e2d),
					fw_byte(0x00110e2e), fw_byte(0x00110e2f), fw_byte(0x0011fd04),
					fw_byte(0x00100022), fw_word(FW_STARTUP_MODE),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		const u16 registration_status = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff;
		const bool relevant_handler_status = addr == 0x002638e4 &&
				(registration_status == 0x0395 || registration_status == 0x0396 ||
				 registration_status == 0x05dc || registration_status == 0x05eb ||
				 registration_status == 0x06c5 || registration_status == 0x0795 ||
				 registration_status == 0x08ac);
		if ((addr == 0x00263154 || addr == 0x002632ae || addr == 0x002632b2 ||
				addr == 0x002632be || addr == 0x002638e4 || addr == 0x00263b8e ||
				addr == 0x00263bd4 || addr == 0x00263d30 || addr == 0x00263e54 ||
				addr == 0x00263e58 || addr == 0x00263e64) &&
				(addr != 0x002638e4 || relevant_handler_status) && registration_count++ < 256)
			logerror("code7_registration: pc=%08x status=%04x service=%02x arg1=%08x arg2=%08x "
					"active=%02x transient=%02x/%02x resident=%02x/%02x caller=%08x task=%02x t=%.6f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xff,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)),
					fw_byte(0x00111931), fw_byte(0x0010b2b4), fw_byte(0x0010b2b5),
					fw_byte(0x0010b2dc), fw_byte(0x0010b2dd),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					fw_byte(0x00100022), machine().time().as_double());
		const u16 callback47_input = u32(m_maincpu->state_int(addr == 0x0028f484 ?
				arm7_cpu_device::ARM7_R0 : arm7_cpu_device::ARM7_R4)) & 0xffff;
		if ((addr == 0x002ae62c || addr == 0x002ae63e || addr == 0x002ae642 || addr == 0x002ae6aa) &&
				callback47_count++ < 64)
		{
			char identity[33] = {};
			const u32 stack = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R13));
			for (unsigned i = 0; i < 16; i++)
				std::snprintf(identity + i * 2, sizeof(identity) - i * 2, "%02x", fw_byte(stack + i));
			logerror("code7_identity_check: pc=%08x r0=%08x r1=%08x stored=%02x/%02x type=%02x/%02x "
					"result=%08x identity=%s t=%.6f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					fw_byte(0x00112466), fw_byte(0x00112467),
					fw_byte(0x0011fcb4), fw_byte(0x0011fcb5),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R5)),
					identity, machine().time().as_double());
		}
		const bool relevant_callback47_entry = addr == 0x0028f484 &&
				(callback47_input == 0x05dc || callback47_input == 0x0578 ||
				 callback47_input == 0x1440);
		if ((relevant_callback47_entry || addr == 0x0028f4e4 || addr == 0x0028f4ea) &&
				callback47_count++ < 64)
			logerror("code7_callback47: pc=%08x input=%04x query=%08x query_result=%04x "
					"task=%02x caller=%08x t=%.6f\n",
					addr, callback47_input,
					fw_dword(0x00111988),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
					debug_ram_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		if ((addr == 0x00255d26 || addr == 0x0027f150 ||
				addr == 0x0027f15e || addr == 0x0027f162 || addr == 0x0027f16e || addr == 0x0027f1c0 ||
				addr == 0x0027f1c4 || addr == 0x0027f1dc || addr == 0x0027f1e8 || addr == 0x0027f1f4 ||
				addr == 0x0027f220 || addr == 0x0027f22e || addr == 0x0027f236 ||
				addr == 0x0027f23e || addr == 0x0027f242 || addr == 0x002878e4 || addr == 0x00287950 ||
				addr == 0x00287958 || addr == 0x00287962 || addr == 0x0028796a ||
				addr == 0x0028c2be) && controller_count++ < 256)
			logerror("code7_controller: pc=%08x r0=%08x r1=%08x r2=%08x r4=%08x r5=%08x "
					"state=%02x/%02x/%02x "
					"init=%02x/%02x work=%02x selector=%02x netcfg=%08x mode=%04x "
					"slot=%02x/%02x/%02x/%02x/%08x caller=%08x task=%02x t=%.6f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R4)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R5)),
					fw_byte(0x0011fcd5), fw_byte(0x0011fcba), fw_byte(0x0011fd14),
					fw_byte(0x0011fd03), fw_byte(0x0011fd04), fw_byte(0x00110e2d),
					fw_byte(0x0011fcfa), fw_dword(0x0010d128),
					fw_word(FW_STARTUP_MODE),
					fw_byte(0x00110e4c), fw_byte(0x00110e4d), fw_byte(0x00110e4e),
					fw_byte(0x00110e4f), fw_dword(0x00110e54),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					fw_byte(0x00100022), machine().time().as_double());
		if ((addr == 0x0027f698 || addr == 0x0027f6d6 || addr == 0x0027f712 ||
				addr == 0x0027f718 || addr == 0x0027f76c || addr == 0x0027f7ec ||
				addr == 0x0027f7f6) && controller_activity_count++ < 128)
			logerror("code7_controller_activity: pc=%08x input=%04x source=%02x guard=%02x "
					"activity=%02x/%02x/%02x/%02x selector=%02x task=%02x caller=%08x t=%.6f\n",
					addr, u32(m_maincpu->state_int(addr == 0x0027f698 ?
						arm7_cpu_device::ARM7_R0 : arm7_cpu_device::ARM7_R4)) & 0xffff,
					fw_byte(0x00110e2f), fw_byte(0x0011fd04),
					fw_byte(0x00110e2c), fw_byte(0x00110e2d),
					fw_byte(0x00110e2e), fw_byte(0x00110e2f),
					fw_byte(0x0011fd03), fw_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		if ((addr == 0x0020157e || addr == 0x00201870 || addr == 0x0026f5a4 ||
				addr == 0x0026f5d8 || addr == 0x0026f5e0 || addr == 0x0026f5fa ||
				addr == 0x00287250 || addr == 0x0028725a || addr == 0x00287266 ||
				addr == 0x00287284 || addr == 0x0028728e || addr == 0x0029bb98 ||
				addr == 0x0029bbb4 || addr == 0x0029bbc2 || addr == 0x002b4764) &&
				controller_selector_count++ < 128)
		{
			const u32 selector_record = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R4));
			const bool selector_record_valid = selector_record >= NOKIA_RAM_BASE &&
				selector_record + 3 < NOKIA_RAM_END;
			logerror("code7_selector0: pc=%08x r0=%08x r4=%08x cphs_flags=%08x "
					"cphs_data=%02x/%02x/%02x/%02x feature=%02x/%02x/%02x/%02x "
					"record=%02x/%02x/%02x/%02x sst_flags=%04x task=%02x caller=%08x t=%.6f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R4)),
					fw_dword(0x0010d128), fw_byte(0x0010d24c), fw_byte(0x0010d24d),
					fw_byte(0x0010d24e), fw_byte(0x0010d24f),
					fw_byte(0x001124e8), fw_byte(0x001124e9),
					fw_byte(0x001124ea), fw_byte(0x001124eb),
					selector_record_valid ? fw_byte(selector_record + 0) : 0xff,
					selector_record_valid ? fw_byte(selector_record + 1) : 0xff,
					selector_record_valid ? fw_byte(selector_record + 2) : 0xff,
					selector_record_valid ? fw_byte(selector_record + 3) : 0xff,
					fw_word(0x0010d126),
					fw_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		}
		if (addr == 0x002695f4)
		{
			const u16 status = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff;
			if (status == 0x00ca && publication_count++ < 256)
				logerror("code7_publication: status=%04x packed=%04x source=%08x caller=%08x "
						"task=%02x mode=%04x ecb=%08x/%08x/%08x t=%.6f\n",
						status, status, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						fw_byte(0x00100022), fw_word(FW_STARTUP_MODE),
						fw_dword(0x00100ab8), fw_dword(0x00100abc), fw_dword(0x00100ac0),
						machine().time().as_double());
		}
		if (addr == 0x002af798)
		{
			const u16 status = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0x1fff;
			if ((status == 0x00ca || status == 0x0348 || status == 0x0395 ||
					status == 0x05e1 || status == 0x05e7 ||
					status == 0x05dc || status == 0x05eb || status == 0x06c5 ||
					status == 0x0795) && publication_count++ < 256)
				logerror("code7_publication: status=%04x packed=%04x source=%08x caller=%08x "
						"task=%02x mode=%04x t=%.6f\n",
						status, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						fw_byte(0x00100022), fw_word(FW_STARTUP_MODE), machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a03b4)
	{
		static unsigned sim_rx_count = 0;
		if (sim_rx_count++ < 64)
			logerror("sim_rx_isr: irq=%04x fiq=%04x byte=%02x state=%02x remaining=%04x received=%04x t=%.8f\n",
					m_irq_status, m_fiq_status, debug_ram_byte(0x001106c4),
					debug_ram_byte(0x001106d4), debug_ram_word(0x001106d6),
					debug_ram_word(0x001106d8), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002028a4)
	{
		static bool sim_presence_seen = false;
		if (!sim_presence_seen)
		{
			sim_presence_seen = true;
			logerror("sim_presence_monitor: mode=%04x enable=%02x no-sim=%02x selected-df=%04x "
					"ready=%02x task=%02x caller=%08x t=%.8f\n",
					debug_ram_word(FW_STARTUP_MODE), debug_ram_byte(0x00111c79),
					debug_ram_byte(0x00111c64), debug_ram_word(0x00111ca4),
					debug_ram_byte(0x0010dcaf), debug_ram_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x002028f4 || addr == 0x0020290a))
	{
		static unsigned sim_presence_result_count = 0;
		if (sim_presence_result_count++ < 4)
			logerror("sim_presence_result: %s current-df=%04x t=%.8f\n",
					addr == 0x0020290a ? "steady" : "changed",
					debug_ram_word(0x00111ca4), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a04c8)
	{
		static unsigned sim_irq_count = 0;
		if (sim_irq_count++ < 32)
			logerror("sim_irq_dispatch: irq=%04x fiq=%04x fifo=%02x control=%02x t=%.8f\n",
					m_irq_status, m_fiq_status, m_sim_card->rx_count_r(),
					debug_ram_byte(0x001106c3), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x0027e240 || addr == 0x002902ac || addr == 0x00293f30 ||
			 addr == 0x00207234 || addr == 0x0020733c || addr == 0x0027e046 ||
			 addr == 0x0027e98c || addr == 0x002a0218 || addr == 0x002a0060 ||
			 addr == 0x002a02e6 || addr == 0x002a0268 || addr == 0x002a0336))
	{
		static unsigned sim_contract_count = 0;
		if (sim_contract_count++ < 256)
			logerror("sim_contract: pc=%08x r0=%08x r1=%08x r2=%08x task=%02x enable=%02x t=%.8f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)),
					debug_ram_byte(0x00100022), debug_ram_byte(0x00111c79),
					machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027ee94)
	{
		static unsigned sim_procedure_count = 0;
		const u32 r9 = m_maincpu->state_int(arm7_cpu_device::ARM7_R9);
		const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
		const u32 command = fw_dword(sp + 0x30);
		const u32 reply = fw_dword(sp + 0x34);
		if (sim_procedure_count++ < 128)
			logerror("sim_procedure: r9=%08x proc=%02x command=%08x ins=%02x command_len=%04x reply=%08x reply_len=%04x t=%.8f\n",
					r9, debug_ram_byte(r9), command, debug_ram_byte(command + 6),
					debug_ram_word(command + 2), reply, debug_ram_word(reply + 2),
					machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027df10)
	{
		static unsigned sim_recv_count = 0;
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (sim_recv_count++ < 256 && msg >= NOKIA_RAM_BASE && msg + 8 < NOKIA_RAM_END)
			logerror("sim_recv: msg=%08x code=%02x len=%04x bytes=%02x %02x %02x task=%02x lr=%08x sp=%08x t=%.8f\n",
					msg, debug_ram_byte(msg + 4), debug_ram_word(msg + 2),
					debug_ram_byte(msg + 5), debug_ram_byte(msg + 6), debug_ram_byte(msg + 7),
					debug_ram_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R13)), machine().time().as_double());
		else if (sim_recv_count <= 256)
			logerror("sim_recv_static: msg=%08x task=%02x t=%.8f\n", msg,
					debug_ram_byte(0x00100022), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027defc)
	{
		static unsigned sim_recv_entry_count = 0;
		if (sim_recv_entry_count++ < 256)
			logerror("sim_recv_entry: lr=%08x r0=%08x r1=%08x sp=%08x task=%02x t=%.8f\n",
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R13)),
					debug_ram_byte(0x00100022), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027ebc0)
	{
		const u32 manager = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		logerror("sim_atr_result: result=%02x atr_len=%04x state=%02x manager=%08x fields=%02x/%02x/%02x/%02x/%02x t=%.8f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xff,
				debug_ram_word(0x0010dddc), debug_ram_byte(0x001106c0), manager,
				debug_ram_byte(manager + 0xa), debug_ram_byte(manager + 0xb),
				debug_ram_byte(manager + 0xc), debug_ram_byte(manager + 0xd),
				debug_ram_byte(manager + 0x10), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a04b6)
	{
		const u32 msg = (u32(debug_ram_word(0x001106cc)) << 16) |
				debug_ram_word(0x001106ce);
		logerror("sim_rx_complete: msg=%08x code=%02x len=%04x mode=%02x remaining=%04x received=%04x t=%.8f\n",
				msg, debug_ram_byte(msg + 4), debug_ram_word(msg + 2),
				debug_ram_byte(0x001106d4), debug_ram_word(0x001106d6),
				debug_ram_word(0x001106d8), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a04be)
		logerror("sim_rx_post_result: r0=%08x task=%02x t=%.8f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
				debug_ram_byte(0x00100022), machine().time().as_double());
	// TRACE_TASKS post-flow: inter-task messages during the handoff/stall. r1 is a message pointer;
	// decode its status instead of treating the allocation address as a protocol code.
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && (addr == 0x0026a354 || addr == 0x0026a204))
	{
		const u32 from = debug_ram_byte(0x00100022);
		const u32 to = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		const bool ram_msg = msg >= NOKIA_RAM_BASE && msg + 32 <= NOKIA_RAM_END;
		const bool flash_msg = msg >= 0x00200000 && msg + 32 <= 0x00400000;
		const bool readable_msg = ram_msg || flash_msg;
		auto message_byte = [&](u32 address) {
			return ram_msg ? fw_byte(address) : m_maincpu->space(AS_PROGRAM).read_byte(address);
		};
		const u16 status = ram_msg ? fw_word(msg) :
				(flash_msg ? m_maincpu->space(AS_PROGRAM).read_word(msg) : 0xffff);
		if (to == 19 && readable_msg)
		{
			static unsigned task19_posts = 0;
			if (task19_posts++ < 128)
				logerror("task19_post: from=%02x status=%04x via=%08x msg=%08x "
						"bytes=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x "
						"caller=%08x t=%.6f\n",
						from, status, addr, msg,
						message_byte(msg), message_byte(msg + 1), message_byte(msg + 2), message_byte(msg + 3),
						message_byte(msg + 4), message_byte(msg + 5), message_byte(msg + 6), message_byte(msg + 7),
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						machine().time().as_double());
		}
		static u32 seen[64]; static unsigned n = 0; static u32 total = 0;
		total++;
		const u32 kv = (from << 8) | to;   // dedup by communication EDGE (codes are per-message seq ids)
		bool dup = false; for (unsigned i = 0; i < n; i++) if (seen[i] == kv) { dup = true; break; }
		if (!dup && n < 64)
		{
			seen[n++] = kv;
			logerror("msgedge: t%u -> t%u (first status=%04x msg=%08x) t=%.4f\n",
					from, to, status, msg, machine().time().as_double());
		}
		if ((from == 3 || from == 5 || from == 10 || from == 15 || from == 16 || from == 17 || from == 20 || from == 21 ||
				to == 3 || to == 5 || to == 10 || to == 15 || to == 16 || to == 17 || to == 20 || to == 21) && readable_msg)
		{
			static unsigned provider_posts = 0;
			static unsigned task17_posts = 0;
			static unsigned task5_posts = 0;
			const bool task17_edge = from == 17 || to == 17;
			const bool task5_edge = from == 5 || to == 5;
			const bool log_post = task17_edge ? task17_posts++ < 256 :
				(task5_edge ? task5_posts++ < 256 : provider_posts++ < 128);
			if (log_post)
				logerror("sim_owner_post: t%u -> t%u status=%04x msg=%08x caller=%08x bytes="
						"%02x %02x %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x t=%.4f\n",
						from, to, status, msg,
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						message_byte(msg + 0), message_byte(msg + 1), message_byte(msg + 2), message_byte(msg + 3),
						message_byte(msg + 4), message_byte(msg + 5), message_byte(msg + 6), message_byte(msg + 7),
						message_byte(msg + 8), message_byte(msg + 9), message_byte(msg + 10), message_byte(msg + 11),
						message_byte(msg + 12), message_byte(msg + 13), message_byte(msg + 14), message_byte(msg + 15),
						machine().time().as_double());
		}
		if ((total % 2000) == 0)
			logerror("msgrate: %u posts by t=%.4f (protocol still active)\n", total, machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x00221d74)
		logerror("sim_owner_event: task17 build status=%04x caller=%08x state=%02x/%02x/%02x/%02x t=%.4f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
				fw_byte(0x0010fca8), fw_byte(0x0010fca9), fw_byte(0x0010fcaa), fw_byte(0x0010fcba),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x00225c9c)
		logerror("sim_owner_phase_rx: status=%04x latch=%02x phase=%02x/%02x t=%.4f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
				fw_byte(0x0010fcb4), fw_byte(0x0010fca9), fw_byte(0x0010fcba),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x00251c2a)
	{
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		logerror("sim_owner_043c: caller=%08x msg=%08x bytes=%02x %02x %02x %02x %02x %02x %02x %02x t=%.4f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1), msg,
				fw_byte(msg), fw_byte(msg + 1), fw_byte(msg + 2), fw_byte(msg + 3),
				fw_byte(msg + 4), fw_byte(msg + 5), fw_byte(msg + 6), fw_byte(msg + 7),
				machine().time().as_double());
	}
	// TRACE_CSCMD (opt-in, fetch side): log both sides of the contact-service transport.
	// 0x234634 constructs an MCU-to-peer frame and 0x234684 queues it; the dispatcher below
	// consumes peer-to-MCU frames. Keeping direction explicit prevents acknowledgements from
	// being mistaken for organic producers of inbound commands with the same numeric id.
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x00234634)
		{
			static unsigned csconstruct_count = 0;
			if (csconstruct_count++ < 256)
				logerror("csconstruct: command=%02x payload=%02x output=%08x caller=%08x task=%02x t=%.4f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R2) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x00234826)
		{
			const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
			logerror("cseeprom_fail: computed=%04x stored=%04x guard=%04x sp=%08x task=%02x t=%.6f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R4) & 0xffff,
					fw_word(sp + 4), fw_word(sp + 6), sp, fw_byte(FW_SCHED_RUNNING_TASK_ID),
					machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x00234684)
		{
			static unsigned cssend_count = 0;
			const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			if (cssend_count++ < 256 && msg >= NOKIA_RAM_BASE && msg + 9 < NOKIA_RAM_END)
				logerror("cssend: command=%02x class=%02x destination=%02x source=%02x route=%02x/%02x/%02x "
						"length=%04x msg=%08x caller=%08x task=%02x t=%.4f\n",
						fw_byte(msg + 8), fw_byte(msg + 3), fw_byte(msg), fw_byte(msg + 1),
						fw_byte(msg + 2), fw_byte(msg + 6), fw_byte(msg + 7), fw_word(msg + 4), msg,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
						debug_ram_byte(0x00100022), machine().time().as_double());
		}
		// The contact-service command dispatcher 0x237400 reads the
		// command byte [msg+8] into r4 at 0x23741a and binary-searches to a handler. cmd 0x70/0x71 route to
		// the channel-map handler 0x23670c: 0x70 = resource ENABLE (-> 0x2b140a, config blob = msg+9),
		// 0x71 = resource DISABLE. Log every command byte the contact-service processes -- confirms whether
		// the resource-enable command 0x70 is ever delivered on our boot, and inventories the command set.
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x0023741a)
		{
			const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
			constexpr u32 task2_tcb = 0x00101484 + 2 * 0x1c;
			static unsigned cc = 0;
			if (cc++ < 80 && msg >= 0x00100000 && msg < 0x00180000)
				logerror("cscmd: command=%02x class=%02x destination=%02x source=%02x route=%02x/%02x/%02x "
						"length=%04x payload0=%02x "
						"msg=%08x svcready=%02x qlink=%08x qa=%08x:%02x/%02x qb=%08x:%02x/%02x "
						"caller=%08x task=%02x t=%.4f\n",
						debug_ram_byte(msg + 8), debug_ram_byte(msg + 3), debug_ram_byte(msg), debug_ram_byte(msg + 1),
						debug_ram_byte(msg + 2), debug_ram_byte(msg + 6), debug_ram_byte(msg + 7),
						debug_ram_word(msg + 4), debug_ram_byte(msg + 9), msg, debug_ram_byte(0x0011fed1),
						fw_dword(task2_tcb + 8), fw_dword(task2_tcb + 0xc),
						fw_byte(task2_tcb + 0x10), fw_byte(task2_tcb + 0x11),
						fw_dword(task2_tcb + 0x14), fw_byte(task2_tcb + 0x18), fw_byte(task2_tcb + 0x19),
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
						debug_ram_byte(0x00100022), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x002a5468)
		{
			const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			static unsigned csroute_count = 0;
			if (csroute_count++ < 256 && msg >= NOKIA_RAM_BASE && msg + 9 < NOKIA_RAM_END)
				logerror("csroute: msg=%08x address=%02x/%02x route=%02x/%02x/%02x class=%02x command=%02x "
						"local=%02x caller=%08x task=%02x t=%.6f\n",
					msg, fw_byte(msg), fw_byte(msg + 1), fw_byte(msg + 2), fw_byte(msg + 6),
					fw_byte(msg + 7), fw_byte(msg + 3), fw_byte(msg + 8), fw_byte(0x00111794),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					fw_byte(0x00100022), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x00237c8e)
		{
			static unsigned csfree_count = 0;
			if (csfree_count++ < 256)
				logerror("csfree: msg=%08x command=%02x caller=%08x task=%02x t=%.6f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
					fw_byte(m_maincpu->state_int(arm7_cpu_device::ARM7_R4) + 8),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), machine().time().as_double());
		}
		// Contact-session result handling and its watchdog share 0x236dc4.  Record
		// which side actually drives repeated command-0x64 reports, together with
		// the compact state block consumed by both paths.
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x00236dc4)
		{
			static unsigned csresult_count = 0;
			if (csresult_count++ < 256)
				logerror("csresult: result=%02x caller=%08x task=%02x "
						"present=%02x flags=%02x stored=%02x counter=%02x state=%02x ack=%02x enable=%02x t=%.6f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), debug_ram_byte(FW_CONTACT_SERVICE_STATUS),
					debug_ram_byte(FW_CONTACT_SERVICE_STATUS + 1),
					debug_ram_byte(FW_CONTACT_SERVICE_RESULT),
					debug_ram_byte(FW_CONTACT_SERVICE_COUNTER),
					debug_ram_byte(FW_CONTACT_SERVICE_SUBSTATE),
					debug_ram_byte(FW_CONTACT_SERVICE_ACK),
					debug_ram_byte(FW_SERVICE_CHANNEL_ENABLE_FLAGS), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_CSCMD", 0) != 0 && pc == addr && addr == 0x00237b80)
		{
			static unsigned cswatch_count = 0;
			if (cswatch_count++ < 256)
				logerror("cswatch: present=%02x flags=%02x stored=%02x counter=%02x state=%02x ack=%02x enable=%02x "
						"caller=%08x task=%02x t=%.6f\n",
					debug_ram_byte(FW_CONTACT_SERVICE_STATUS),
					debug_ram_byte(FW_CONTACT_SERVICE_STATUS + 1),
					debug_ram_byte(FW_CONTACT_SERVICE_RESULT),
					debug_ram_byte(FW_CONTACT_SERVICE_COUNTER),
					debug_ram_byte(FW_CONTACT_SERVICE_SUBSTATE),
					debug_ram_byte(FW_CONTACT_SERVICE_ACK),
					debug_ram_byte(FW_SERVICE_CHANNEL_ENABLE_FLAGS),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), machine().time().as_double());
		}
		// TRACE_HANDOFF (opt-in): the post-SIM interactive/idle handoff (docs/interactive_handoff.md).
		// Curated task-1 mode, mailbox, and interactive-init seams:
		//  (a) task-1 dispatcher 0x270c8e -- mode [0x1123f0], mode-0d checklist [0x112399], CCONT [0x11ff6c];
		//  (b) mode-0 interactive-init burst 0x270d1c / display_idle 0x298000 -- did they run? (not yet);
		//  (c) 0x270184 -- every task-1 mode transition + caller lr;
		//  (d) 0x26a204/0x26a354 -- inventory of codes posted to task-1's mailbox;
		//  (e) 0x27d654 -- VBAT voltage-confirmation gate byte [0x110436] trajectory.
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
	// Service transport request boundary. The firmware calls 0x2b13d4 to report
	// the channel-empty/resource state; the peer completes it asynchronously and
	// exposes service-present through its callback. No firmware state is changed
	// from this execution hook.
	if (nokia_env_u32("NOKI3210_TRACE_HANDOFF", 0) != 0 && pc == addr && addr == 0x002b13d4)
	{
		const u32 report = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff;
		if ((report & 0xff00) == 0x7d00)
		{
			static unsigned power_report_count = 0;
			if (power_report_count++ < 128)
				logerror("power_service_report: report=%04x task=%02x caller=%08x t=%.6f\n",
						report, fw_byte(0x00100022),
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						machine().time().as_double());
		}
	}
	// Observe the lower GSM receive path without participating in it.  Task 15 receives event 0x07dd
	// at 0x20a026; its payload pointer at message+8 is parsed by 0x209978 and converted to an
	// internal result before task 15 emits any 0x09e* status toward task 14.
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x0027e3cc || addr == 0x0027e8fe || addr == 0x002085ce ||
			 addr == 0x00203d2c || addr == 0x002938b0 || addr == 0x00293522 ||
			 addr == 0x002726e0))
		logerror("sim_contract: registration-object pc=%08x r0=%08x r1=%08x r2=%08x r4=%08x "
				"notify=%02x task=%02x caller=%08x t=%.6f\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4), fw_byte(0x0010dcb7),
				fw_byte(0x00100022),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x002038ec || addr == 0x00203906 || addr == 0x0020390c ||
			 addr == 0x00203936 || addr == 0x00203948 || addr == 0x0027def4))
		logerror("sim_contract: toolkit-gate pc=%08x phase=%02x selected=%04x latch=%02x "
				"r0=%08x caller=%08x task=%02x t=%.6f\n", addr,
				fw_byte(0x00111c78), fw_word(0x00111ca6), fw_byte(0x0010dcb7),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x00100022), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_GSM_SERVICE", 0) != 0 && pc == addr && addr == 0x002618e8)
		logerror("gsm_lower: service5 callback status=%04x state=%02x index=%02x queue=%02x/%02x/%02x/%02x flags=%08x caller=%08x r4=%08x r5=%08x r6=%08x r7=%08x t=%.4f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
				fw_byte(0x0011fcc3),
				fw_byte(0x0011fcce),
				fw_byte(0x00111930), fw_byte(0x00111931), fw_byte(0x00111938), fw_byte(0x00111939),
				m_maincpu->space(AS_PROGRAM).read_dword(0x002db864),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4), m_maincpu->state_int(arm7_cpu_device::ARM7_R5),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6), m_maincpu->state_int(arm7_cpu_device::ARM7_R7),
				machine().time().as_double());
	// Observe generic service-framework registration and dispatch. This is diagnostic only;
	// service 5 is the organic callback path relevant to the argumentless 0x05e8 result.
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_GSM_SERVICE", 0) != 0)
	{
		static unsigned trace_count = 0;
		auto reg = [&](int n){ return u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0 + n)); };
		auto dump = [&](u32 p, unsigned n, char *out, size_t cap) {
			size_t used = 0;
			if (p < NOKIA_RAM_BASE || p + n > NOKIA_RAM_END)
			{
				std::snprintf(out, cap, "<%08x>", p);
				return;
			}
			for (unsigned i = 0; i < n && used + 4 < cap; i++)
				used += std::snprintf(out + used, cap - used, "%02x%s", fw_byte(p + i), i + 1 == n ? "" : " ");
		};
		if (addr == 0x002438e8 && reg(2) > 0xff)
			logerror("gsm_service: callback-resource id=%04x channel=%02x slot=%02x arg=%08x t=%.4f\n",
					reg(2) & 0xffff, reg(0) & 0xff, reg(1) & 0xff, reg(3),
					machine().time().as_double());
		if (addr == 0x002ae8ba)
			logerror("gsm_service: registered-callback event=%04x source=%08x t=%.4f\n",
					reg(0) & 0xffff, reg(1), machine().time().as_double());
		if (addr == 0x00262bc6 || addr == 0x00262dfe || addr == 0x00262e0c)
			logerror("gsm_service: callback-gate pc=%08x r0=%08x r1=%08x r2=%08x r4=%08x r5=%08x t=%.4f\n",
					addr, reg(0), reg(1), reg(2), reg(4), reg(5), machine().time().as_double());
		if (addr == 0x002aed5c || addr == 0x002aefca)
			logerror("gsm_service: task5-lookup pc=%08x current=%04x packed=%04x family=%02x result=%08x t=%.4f\n",
					addr, fw_word(0x00112086), fw_word(0x00112088),
					fw_byte(0x0011fc80 + (fw_word(0x00112086) & 0xff)), reg(0),
					machine().time().as_double());
		if (addr == 0x002632fc && trace_count++ < 512 &&
				reg(2) >= NOKIA_RAM_BASE && reg(2) + 0x1c <= NOKIA_RAM_END)
		{
			const u8 service = reg(0) & 0xff;
			const u32 descriptor = reg(2);
			const u32 data = (uint32_t(fw_word(descriptor)) << 16) | fw_word(descriptor + 2);
			const u16 callback = fw_word(descriptor + 0x12);
			char bytes[128] = {};
			char data_bytes[160] = {};
			char source_bytes[96] = {};
			const u32 source = (uint32_t(fw_word(descriptor + 0x0c)) << 16) | fw_word(descriptor + 0x0e);
			dump(descriptor, 0x1c, bytes, sizeof(bytes));
			dump(data, 32, data_bytes, sizeof(data_bytes));
			dump(source, 16, source_bytes, sizeof(source_bytes));
			logerror("gsm_service: register service=%02x ordinal=%u descriptor=%08x data=%08x raw=%08x source=%08x event=%04x callback=%04x caller=%08x bytes=[%s] data_bytes=[%s] source_bytes=[%s] t=%.4f\n",
					service, reg(1) & 0xff, descriptor, data, fw_dword(descriptor),
					source, fw_word(descriptor + 0x10), callback, reg(14) & ~u32(1),
					bytes, data_bytes, source_bytes, machine().time().as_double());
		}
		if (addr == 0x00263d30 && trace_count++ < 512)
			logerror("gsm_service: resident descriptor=%08x enable=%08x mask=%08x caller=%08x t=%.4f\n",
					reg(0), reg(1), reg(2), reg(14) & ~u32(1), machine().time().as_double());
		else if (addr == 0x00263154 && ((reg(0) & 0xff) == 0x0a || (reg(0) & 0xff) == 0x0b ||
				(reg(0) & 0xff) == 0x1e))
		{
			const u8 service = reg(0) & 0xff;
			const bool enabling = (reg(1) & 0xff) != 0;
			char registry[160] = {};
			dump(0x0010b2fc, 48, registry, sizeof(registry));
			logerror("gsm_service: service=%02x enabled=%u t=%.4f\n",
					service, enabling,
					machine().time().as_double());
			logerror("gsm_service: registry=[%s]\n", registry);
		}
		else if (addr == 0x002633d0 && ((reg(0) & 0xff) == 0x0a || (reg(0) & 0xff) == 0x0b ||
				(reg(0) & 0xff) == 0x1e))
		{
			logerror("gsm_service: declare service=%02x class=%02x opcode=%02x flags=%02x t=%.4f\n",
					reg(0) & 0xff, reg(1) & 0xff, reg(2) & 0xff, reg(3) & 0xff, machine().time().as_double());
		}
		else if (addr == 0x002629d0 && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: trigger token=%02x mode=%02x t=%.4f\n",
					reg(0) & 0xff, reg(1) & 0xff,
					machine().time().as_double());
		}
		else if (addr == 0x002624b8 && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: callback-dispatch service=%02x t=%.4f\n",
					reg(0) & 0xff, machine().time().as_double());
		}
		else if (addr == 0x0026265c && trace_count < 256)
		{
			trace_count++;
			char registry[160] = {};
			dump(0x0010b2dc, 48, registry, sizeof(registry));
			logerror("gsm_service: pending-scan caller=%08x active-slot=%02x current-service=%02x "
					"registry=[%s] t=%.4f\n",
					reg(14) & ~u32(1), fw_byte(0x00111931), fw_byte(0x00111f0f),
					registry, machine().time().as_double());
		}
		else if (addr == 0x00262544 && trace_count < 160)
		{
			trace_count++;
			char resident[128] = {};
			dump(reg(0), 0x1c, resident, sizeof(resident));
			logerror("gsm_service: resident-match object=%08x selector=%02x bytes=[%s] t=%.4f\n",
					reg(0), reg(1) & 0xff, resident, machine().time().as_double());
		}
		else if ((addr == 0x0026265c || addr == 0x0026309c || addr == 0x002625ac) && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: framework-step pc=%08x r0=%08x r1=%08x t=%.4f\n",
					addr, reg(0), reg(1), machine().time().as_double());
		}
		else if ((addr == 0x002438e8 || addr == 0x0024383c || addr == 0x0024387a ||
				 addr == 0x00243550 || addr == 0x00296f4e) && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: resource-step pc=%08x r0=%08x r1=%08x r2=%08x r3=%08x t=%.4f\n",
					addr, reg(0), reg(1), reg(2), reg(3), machine().time().as_double());
		}
	}
	// Observe organic resource-manager requests independently of the provisional GSM-service
	// responder. Callback 0x2f uses this API after its natural constructor, before any service-5
	// or service-30 transaction exists.
}

uint16_t noki3310_state::flash_r(offs_t offset, uint16_t mem_mask)
{
	const u32 pc = m_maincpu->pc();
	const u32 addr = 0x00200000 + (offset << 1);
	flash_firmware_traces(pc, addr);
	return m_flash->read(offset) & mem_mask;
}

void noki3310_state::flash_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
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
			if (m_power_on)
			{
				data &= m_power_on;
				m_power_on = 0;
			}
			if (nokia_env_u32("NOKI3210_HOLD_POWER_KEY", 0) != 0)
				data &= 0xfe;
			break;
		case 0x37:  // SIM UART RxD
			if (m_sim_card->enabled())
				data = m_sim_card->rxd_r();
			break;
		case 0x38:  // SIM UART interrupt identification
			if (m_sim_card->enabled())
				data = m_sim_card->iir_r();
			break;
		case 0x39:  // SIM control and live-interface status
			if (m_sim_card->enabled())
				data = m_sim_card->control_r();
			break;
		case 0x3c:  // SIM UART RxD queue fill
			if (m_sim_card->enabled())
				data = m_sim_card->rx_count_r();
			break;
		case 0x6c:
			data = m_ccont->serial_r();
			m_gensio_status &= ~0x04;
			break;
		case 0x6d:
			data = m_gensio_status;
			break;
	}

	if (offset == 0x20)
	{
		const bool sda_output = BIT(m_mad2_regs[0x24], 0);
		const int sda = sda_output ? (BIT(data, 0) & m_eeprom->read_sda()) : m_eeprom->read_sda();
		data = (data & 0xfe) | sda;
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 &&
			nokia_env_u32("NOKI3210_MODEL_SIM_DEVICE", 0) != 0 &&
			(offset == 0x37 || offset == 0x38 || offset == 0x3c))
	{
		static unsigned sim_fifo_read_count = 0;
		if (sim_fifo_read_count++ < 64)
			logerror("sim_fifo_read: off=%02x data=%02x remaining=%u pc=%08x t=%.8f\n",
					offset, data, m_sim_card->rx_count_r(), m_maincpu->pc(), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_GENSIO", 0) != 0 &&
			(offset == 0x2c || offset == 0x2d || offset == 0x6c || offset == 0x6d) &&
			m_gensio_trace_count++ < 20000)
		logerror("gensio: R off=%02x data=%02x pc=%08x t=%.9f\n", offset, data,
				m_maincpu->pc(), machine().time().as_double());

	if (nokia_env_u32("NOKI3210_TRACE_MAD2_LEDGER", 0) != 0 && !m_mad2_trace_read[offset])
	{
		m_mad2_trace_read[offset] = true;
		logerror("mad2_ledger: R off=%02x data=%02x pc=%08x t=%.6f %s\n", offset, data,
				m_maincpu->pc(), machine().time().as_double(), nokia_mad2_reg_desc(offset));
	}
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 R %02x = %02x %s\n", offset, data, nokia_mad2_reg_desc(offset));
	return data;
}

void noki3310_state::mad2_io_w(offs_t offset, uint8_t data)
{
	uint8_t old_data = m_mad2_regs[offset];
	m_mad2_regs[offset] = data;
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && offset == 0x36)
	{
		static unsigned sim_txd_count = 0;
		if (sim_txd_count++ < 128)
			logerror("sim_txd: data=%02x pc=%08x t=%.8f\n", data, m_maincpu->pc(),
					machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && offset == 0x39)
	{
		static unsigned sim_control_count = 0;
		if (sim_control_count++ < 128)
			logerror("sim_control_w: data=%02x old=%02x live=%02x pc=%08x t=%.8f\n", data,
					old_data, m_sim_card->control_r(), m_maincpu->pc(), machine().time().as_double());
	}
	if (offset == 0x36 && m_sim_card->enabled())
		m_sim_card->txd_w(data);
	else if (offset == 0x38 && m_sim_card->enabled())
		m_sim_card->iir_w(data);
	else if (offset == 0x39 && m_sim_card->enabled())
		m_sim_card->control_w(data);
	else if (offset == 0x3d && m_sim_card->enabled())
		m_sim_card->rx_ack_w(data);
	if (offset == 0x2d)
	{
		// Selecting a GENSIO endpoint leaves the controller idle and its
		// transmit path available. Read-data-ready is transaction-local.
		m_gensio_status = 0x03;
	}
	else if (offset == 0x2c && (m_mad2_regs[0x2d] & 0x04))
	{
		// CCONT transfers complete synchronously for now; the firmware polls
		// status bit 2 before consuming a register byte from 0x6c.
		m_gensio_status |= 0x04;
	}
	if (nokia_env_u32("NOKI3210_TRACE_GENSIO", 0) != 0 &&
			(offset == 0x2c || offset == 0x2d || offset == 0x6c || offset == 0x6d) &&
			m_gensio_trace_count++ < 20000)
		logerror("gensio: W off=%02x data=%02x old=%02x pc=%08x t=%.9f\n", offset, data,
				old_data, m_maincpu->pc(), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_LEDGER", 0) != 0 && !m_mad2_trace_write[offset])
	{
		m_mad2_trace_write[offset] = true;
		logerror("mad2_ledger: W off=%02x data=%02x old=%02x pc=%08x t=%.6f %s\n", offset,
				data, old_data, m_maincpu->pc(), machine().time().as_double(), nokia_mad2_reg_desc(offset));
	}

	if (offset == 0x20 || offset == 0x24)
	{
		const uint8_t signal = m_mad2_regs[0x20];
		const uint8_t direction = m_mad2_regs[0x24];
		m_eeprom->write_sda(BIT(direction, 0) ? BIT(signal, 0) : 1);
		m_eeprom->write_scl(BIT(signal, 3));
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
			// TX-byte-sent FIQ. No bus peer is currently attached.
			schedule_mbus_fiq(2);
			break;
		case 0x2c:
			m_ccont->serial_w(data);
			break;
		case 0x2e:
		case 0x6e:
		{
			const bool lcd_data = !(offset & 0x40);
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
	return 0;
}

void noki3310_state::mad2_dspif_w(offs_t offset, uint8_t data)
{
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("dsp_boundary: DSPIF W off=%x data=%02x pc=%08x task=%02x t=%.6f\n",
				u32(offset), data, m_maincpu->pc() & ~u32(1), fw_byte(0x00100022),
				machine().time().as_double());
	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x DSPIF\n", offset, data);
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
	I2C_24C128(config, m_eeprom);
	NOKIA_CCONT(config, m_ccont);
	m_ccont->irq_cb().set(FUNC(noki3310_state::ccont_irq_w));
	m_ccont->power_cb().set(FUNC(noki3310_state::ccont_power_w));
	NOKIA_DSP_PEER(config, m_dsp_peer);
	const bool dsp_contact = nokia_env_u32("NOKI3210_MODEL_DSP_CONTACT_PEER", 0) != 0;
	const unsigned dsp_default_ms = dsp_contact ? 4 : 5;
	m_dsp_peer->set_service_enabled(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE", 0) != 0);
	m_dsp_peer->set_contact_enabled(dsp_contact);
	m_dsp_peer->set_service_delay_ms(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_DELAY_MS", dsp_default_ms));
	m_dsp_peer->set_service_tick_ms(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_TICK_MS", dsp_default_ms));
	m_dsp_peer->set_trace_enabled(nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0);
	m_dsp_peer->fiq0_cb().set(FUNC(noki3310_state::dsp_fiq0_w));
	m_dsp_peer->service_irq_cb().set(FUNC(noki3310_state::dsp_service_irq_w));
	NOKIA_SIM_CARD(config, m_sim_card);
	m_sim_card->irq_cb().set(FUNC(noki3310_state::sim_irq_w));
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

	ROM_REGION(0x04000, "eeprom", ROMREGION_ERASEFF)
	ROM_LOAD("3210 selftest eeprom.bin", 0x00000, 0x04000, CRC(7f7fd703) SHA1(3402e47e133dc74c7fa03863fee44a171f15100e))
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
	ROM_SYSTEM_BIOS(1, "450e", "v4.50 PPM E")
	ROMX_LOAD("3330f450c.fls", 0x000000, 0x350000, CRC(259313e7) SHA1(88bcc39d9358fd8a8562fe3a0280f0ce82f5897f), ROM_BIOS(0))
	ROMX_LOAD("3330f450e.fls", 0x000000, 0x350000, CRC(9710f695) SHA1(7e88caa4963c57ebbd4d919023e38103ff8b528a), ROM_BIOS(1))
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
