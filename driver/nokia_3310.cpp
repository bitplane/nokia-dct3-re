// license:BSD-3-Clause
// copyright-holders:Sandro Ronco
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
#include "nokia_mad2.h"
#include "nokia_mbus.h"
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
	bool boot_devices;
	bool sane_adc_defaults;
	bool ccont_wddisx_grounded;
};

constexpr nokia_product_config PRODUCT_3210 = { 0x01, true, true, false };
constexpr nokia_product_config PRODUCT_DEFAULT = { 0x04, false, false, false };
constexpr nokia_product_config PRODUCT_5X10 = { 0x10, false, false, false };
constexpr nokia_product_config PRODUCT_8XXX = { 0x10, false, false, false };

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

constexpr uint16_t MAD2_FIQ_TIMER0_COMPARE = uint16_t(1) << 4;

// CCONT serial command/status bits + fixed wiring (hardware constants, not configurable).
// PWRONX is latched as CCONT status bit 1 on a cold power-key boot. It is a
// reset cause sampled by firmware, not one of the upper interrupt sources.
constexpr uint8_t KEYPAD_IRQ_LINE_NUM = 0;        // MAD2 keypad/UIF interrupt
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
// Service-ready / DSP-handshake chain; see docs/service_bootstrap.md. The
// startup service-ready byte (0x110c2c) is set =1 by
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
constexpr offs_t FW_STARTUP_PHASE_EXIT_SELECTOR = 0x112398;
constexpr offs_t FW_POST74_KEYPAD_SCAN_ARMED = 0x11239d;
constexpr offs_t FW_STARTUP_PHASE_FLAG_CODE09 = 0x112390;
constexpr offs_t FW_STARTUP_PHASE_FLAG_CODE0D = 0x112391;
constexpr offs_t FW_STARTUP_PHASE_FLAG_CODE0C = 0x112392;
constexpr offs_t FW_STARTUP_PHASE_FLAG_CODE0B = 0x112393;
constexpr offs_t FW_STARTUP_PHASE_FLAG_CODE0A = 0x112394;
constexpr offs_t FW_STARTUP_PHASE_FLAG_CODE1C = 0x112395;
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
constexpr offs_t FW_CCONT_STATE = 0x11ff6c;

// Service-session state reached during the startup watchdog path.
constexpr offs_t FW_SERVICE_SESSION_STATE = 0x11fecc;
constexpr offs_t FW_SERVICE_SESSION_STATUS = 0x11fed0;
constexpr offs_t FW_SERVICE_SESSION_RESULT = 0x11fed4;
constexpr offs_t FW_SERVICE_SESSION_COUNTER = 0x11fed6;
constexpr offs_t FW_SERVICE_SESSION_SUBSTATE = 0x11feda;
constexpr offs_t FW_SERVICE_SESSION_ACK = 0x11fedb;
constexpr offs_t FW_SERVICE_SESSION_REASON = 0x11ff50;
constexpr offs_t FW_RESOURCE_CHECK_STATUS_TABLE = 0x11fc60;
constexpr offs_t FW_RESOURCE_CHECK_STATUS_INDEX = 0x11ff5a;

// Service-channel/lower-service state used by the current startup transport model.
constexpr offs_t FW_SERVICE_CHANNEL_READY_FLAGS = 0x111794;
constexpr offs_t FW_SERVICE_CHANNEL_ENABLE_FLAGS = 0x11fee4;
constexpr offs_t FW_SERVICE_CHANNEL_MASK_BASE = 0x11ff08;
constexpr uint8_t FW_SERVICE_CHANNEL_READY_BOOT_BIT = 0x08;

// Service-session remote read (see docs/service_bootstrap.md). The firmware
// reads its command from PM logical address 0x5f00 via an async MBUS/PM
// request message. The request's dest node ([msg+1]) is sourced from the channel-enable flag
// FW_SERVICE_CHANNEL_ENABLE_FLAGS (0x11fee4) — which is 0 on a blank phone, so the read is
// dropped (no request sent). The response, when one arrives, is dispatched by command at
// 0x236dc6; command 0x05 completes the service transaction. Request frame format:
//   00 [node] 00 00 00 0a 00 01 [addr_hi] [addr_lo] [seq][seq] [ctr] [count] [data..]
constexpr uint16_t PM_LOGICAL_SERVICE_COMMAND = 0x5f00;
constexpr uint8_t  SERVICE_RESPONSE_CMD_HEALTHY = 0x05;

// Checksums validated in the 3210 v6.00 firmware. Routine 0x264c56 reads
// 0x0000..0x011f, sums 0x11e bytes, and compares the result with the 32-bit
// big-endian word at 0x011c. The config block has a separate check at 0x234810.
// Generic service tools describe finer tune/security sub-blocks, but those are
// not firmware-validated contracts for this ROM. See docs/eeprom_analysis.md.
constexpr uint16_t FW_EEPROM_TUNE_SECURITY_START  = 0x0000;
constexpr uint16_t FW_EEPROM_TUNE_SECURITY_CKSUM  = 0x011c;
constexpr uint16_t FW_EEPROM_CONFIG_BLOCK_START   = 0x0120;  // service-session config
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
		m_gensio(*this, "gensio"),
		m_mad2(*this, "mad2"),
		m_mbus(*this, "mbus"),
		m_dspif(*this, "dspif"),
		m_dsp_hle(*this, "dsp_hle"),
		m_external_service_peer(*this, "external_service_peer"),
		m_simi(*this, "simi"),
		m_sim_card(*this, "sim_card"),
		m_pcd8544(*this, "pcd8544"),
		m_buzzer(*this, "buzzer"),
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

	PCD8544_SCREEN_UPDATE(pcd8544_screen_update);

	uint8_t mad2_io_r(offs_t offset);
	void mad2_io_w(offs_t offset, uint8_t data);
	uint8_t mad2_dspif_r(offs_t offset);
	void mad2_dspif_w(offs_t offset, uint8_t data);
	uint8_t mad2_mcuif_r(offs_t offset);
	void mad2_mcuif_w(offs_t offset, uint8_t data);

	TIMER_CALLBACK_MEMBER(timer_watchdog);
	TIMER_CALLBACK_MEMBER(timer_mbus_rx_fixture);

	uint16_t ram_r(offs_t offset, uint16_t mem_mask = ~0);
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

	void trace_interrupt_register(char operation, offs_t offset, uint8_t data);
	void mad2_fiq_w(int state);
	void mad2_irq_w(int state);
	void mad2_irq_ack_w(u16 mask);
	void ccont_irq_w(int state);
	void ccont_power_w(int state);
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
	required_device<nokia_simi_device> m_simi;
	required_device<nokia_sim_card_device> m_sim_card;
	required_device<pcd8544_device> m_pcd8544;
	required_device<beep_device> m_buzzer;
	required_ioport_array<5> m_keypad;
	required_ioport m_pwr;

	std::unique_ptr<uint16_t[]>   m_ram;

	nokia_product_config m_product = PRODUCT_DEFAULT;
	uint8_t       m_power_on;
	uint8_t       m_keypad_columns;
	bool          m_keypad_irq_latched;
	bool          m_ccont_irq_state;

	emu_timer * m_timer_watchdog;
	emu_timer * m_timer_mbus_rx_fixture;

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
	case 0x06:  return "[CTSI] Timer 1 destination (LSB) (r)";
	case 0x07:  return "[CTSI] Timer 1 destination (MSB) (r)";
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

static uint16_t nokia_adc_override(unsigned id, uint16_t fallback, bool sane_default)
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

	const char *profile = std::getenv("NOKI3210_ADC_PROFILE");
	if (profile || sane_default)
	{
		const bool charged = profile && !std::strcmp(profile, "charged");
		if (!profile || !std::strcmp(profile, "sane") || charged)
		{
			// Stable raw-selector fixtures. Electrical signal names and units are
			// deliberately not assigned until a 3210 board-level source confirms them.
			switch(id & 0x07)
			{
				case 0: return 0x000;
				case 1: return 0x200;
				case 2: return 0x2d0;
				case 3: return 0x280;
				case 4: return 0x200;
				case 5: return charged ? 0x200 : 0x000;
				case 6: return 0x200;
				case 7: return charged ? 0x120 : 0x000;
			}
		}
	}

	return fallback;
}

// ============================================================================
// NOKI3210_* environment knobs — the driver reads every runtime option from the
// environment (pass overrides through `make run RUN_ENV='...'`). Three kinds:
//
//   1. HARDWARE/PRODUCT CONFIG — selects a scenario, not firmware behaviour:
//      power/ADC (ADC_PROFILE, CCONT_READY, CCONT_WDDISX_GROUNDED), clocks
//      (TIMER0_HZ/TIMER1_HZ/TIMER0_CATCHUP,
//      FIQ8_HZ), and the SIM UART/card fixture. The 3210's documented timer-0
//      clock is fixed in its product configuration; remaining environment
//      values are research overrides for unfinished profiles or scenarios.
//
//   2. DEVICE-BOUNDARY MODELS — behavior behind an ordinary hardware interface.
//      The 3210 enables the validated composition by default; MODEL_* can still
//      disable individual peers for negative tests. MODEL_SIM_DEVICE owns
//      SIMI/FIQ6. MODEL_DSP_SERVICE enables the DSP
//      HLE; MODEL_EXTERNAL_SERVICE_PEER enables the separate service peer
//      behind the DSP transport. Their wider contracts remain incomplete.
//   3. DIAGNOSTIC TAPS (TRACE_*) — opt-in, log-only, no state change. A curated few:
//      TRACE_SERVICE_COMMAND (class-0x40 service command stream),
//      TRACE_TASKS (app-task liveness + inter-task message edges).
//      TRACE_SIM_RX covers the register/FIQ/APDU path and SIM reply milestones;
//      TRACE_DSP_BOUNDARY and TRACE_GSM_SERVICE cover the current peer boundary;
//      TRACE_MAD2_TIMERS records the timer-0/FIQ register lifecycle.
//      TRACE_MAD2_INTERRUPTS records pending/mask/ack and CPU-line routing.
//      Research-force policy: docs/evidence_regime.md.
//
// Firmware-address traces are quarantined in nokia_3310_trace.inc. Add no
// forced firmware results or messages. See docs/driver_structure.md.
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

	m_timer_watchdog = timer_alloc(FUNC(noki3310_state::timer_watchdog), this);
	m_timer_mbus_rx_fixture = timer_alloc(FUNC(noki3310_state::timer_mbus_rx_fixture), this);
	save_pointer(NAME(m_ram), (NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1);
	save_item(NAME(m_power_on));
	save_item(NAME(m_keypad_columns));
	save_item(NAME(m_keypad_irq_latched));
	save_item(NAME(m_ccont_irq_state));
	save_item(NAME(m_mad2_regs));
	save_item(NAME(m_mcuif_regs));
	machine().save().register_postload(save_prepost_delegate(FUNC(noki3310_state::post_load), this));
}

void noki3310_state::post_load()
{
	update_keypad_ccont_irqs();
	update_buzzer();
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

void noki3310_state::machine_reset()
{
	std::fill_n(m_ram.get(), (NOKIA_RAM_END - NOKIA_RAM_BASE) >> 1, 0);

	// according to the boot rom disassembly here http://www.nokix.pasjagsm.pl/help/blacksphere/sub_100hardware/sub_arm/sub_bootrom.htm
	// flash entry point is at 0x200040, we can probably reassemble the above code, but for now this should be enough.
	m_maincpu->set_state_int(arm7_cpu_device::ARM7_R15, NOKIA_FLASH_ENTRY);

	memset(m_mad2_regs, 0, 0x100);
	update_buzzer();
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
	if (nokia_env_u32("NOKI3210_DSPIF_CONFORMANCE", 0) != 0)
		logerror("dspif_fixture: conformance=%02x\n", m_dspif->run_conformance_checks());
	// Load deterministic raw selector inputs. The firmware-observable selector
	// contract is known; physical 3210 net names and analog units are not.
	{
		static const uint16_t adc_default[8] =
				{ 0x000, 0x3ff, 0x3ff, 0x280, 0x200, 0x000, 0x200, 0x000 };
		for (unsigned id = 0; id < 8; id++)
			m_ccont->set_adc_source(id, nokia_adc_override(id, adc_default[id], m_product.sane_adc_defaults));
	}
	m_ccont->set_wddisx_grounded(nokia_env_u32("NOKI3210_CCONT_WDDISX_GROUNDED",
			m_product.ccont_wddisx_grounded) != 0);
	m_simi->set_enabled(nokia_env_u32("NOKI3210_MODEL_SIM_DEVICE", m_product.boot_devices) != 0);
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

	m_keypad_columns = 0x1f;
	m_keypad_irq_latched = false;
	m_ccont_irq_state = false;
	m_timer_watchdog->adjust(attotime::from_hz(1), 0, attotime::from_hz(1));
	if (std::getenv("NOKI3210_MBUS_RX_FIXTURE"))
		m_timer_mbus_rx_fixture->adjust(attotime::from_msec(
				nokia_env_u32("NOKI3210_MBUS_RX_FIXTURE_AT_MS", 300)),
				nokia_env_u32("NOKI3210_MBUS_RX_FIXTURE", 0xff) & 0xff);
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
	m_ccont_irq_state = bool(state);
}

void noki3310_state::ccont_power_w(int state)
{
	if (!state)
		m_maincpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
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
	const uint8_t rows_low = m_mad2_regs[0xa8] & ~m_mad2_regs[0x28] & 0x0f;

	for (unsigned column = 0; column < 5; column++)
	{
		const uint8_t keys = m_keypad[column]->read();
		for (unsigned row = 0; row < 4; row++)
			if (BIT(rows_low, row) && !BIT(keys, row + 1))
				data &= ~(uint8_t(1) << column);
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
			nokia_env_u32("NOKI3210_TRACE_MAD2_INTERRUPTS", 0) != 0 &&
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
	if (nokia_env_u32("NOKI3210_TRACE_MBUS", 0) != 0 && m_mbus_trace_count++ < 8192)
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
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_INTERRUPTS", 0) != 0 &&
			m_mad2_interrupt_trace_count++ < 4096)
		logerror("mad2_interrupt: event=reg_%c off=%02x data=%02x fiq=%03x irq=%03x fiqmask=%02x irqmask=%02x ctrl=%02x extctrl=%02x t=%.9f\n",
				operation, u32(offset), data, m_mad2->fiq_status(), m_mad2->irq_status(),
				m_mad2->reg(MAD2_FIQ_MASK), m_mad2->reg(MAD2_IRQ_MASK),
				m_mad2->reg(MAD2_IRQ_CTRL), m_mad2->reg(MAD2_FIQ8_CTRL),
				machine().time().as_double());
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

TIMER_CALLBACK_MEMBER(noki3310_state::timer_watchdog)
{
	// CCONT watchdog
	if (m_ccont->watchdog_tick())
	{
		if (nokia_env_u32("NOKI3210_TRACE_CCONT_WATCHDOG", 0) != 0)
			logerror("ccont_watchdog_expired: t=%.6f\n", machine().time().as_double());
		m_maincpu->reset();
		m_mad2->reset();
		machine_reset();
	}

	// MAD2 watchdog
	if (m_mad2->watchdog_tick())
	{
		m_maincpu->reset();
		m_mad2->reset();
		machine_reset();
		m_mad2->set_reset_cause(0x02);
	}
}

// Hardware RAM read entry point (registered in the address map).
uint16_t noki3310_state::ram_r(offs_t offset, uint16_t mem_mask)
{
	return m_ram[offset] & mem_mask;
}

// Hardware RAM write entry point (registered in the address map). The backing
// store plus firmware-research traces live in nokia_3310_trace.inc.
void noki3310_state::ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	ram_w_firmware_traces(offset, data, mem_mask);
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
	if (nokia_env_u32("NOKI3210_TRACE_DSP_SHARED_TRANSITIONS", 0) != 0 &&
			(byte_offset <= 0x004 || byte_offset == 0x0a6 || byte_offset == 0x0e0 ||
			 byte_offset == 0x0e4 || byte_offset == 0x0fe || byte_offset == 0x100 ||
			 byte_offset == 0x1c8))
		logerror("dsp_shared_observe: off=%03x data=%04x pc=%08x t=%.6f\n",
				byte_offset, data, pc, machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_SHARED_READS", 0) != 0)
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
}

#include "nokia_3310_trace.inc"

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
	uint8_t data = offset <= MAD2_FIQ8_CTRL ? m_mad2->read(offset) :
			(offset >= MAD2_MBUS_CTRL && offset <= 0x1a ? m_mbus->read(offset - MAD2_MBUS_CTRL) :
			(nokia_gensio_device::owns(offset) ? m_gensio->read(offset) : m_mad2_regs[offset]));

	switch(offset)
	{
		case 0x2a:
			data = keypad_columns_r(true);
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
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 &&
			m_simi->enabled() &&
			(offset == 0x37 || offset == 0x38 || offset == 0x3c))
	{
		static unsigned sim_fifo_read_count = 0;
		if (sim_fifo_read_count++ < 64)
			logerror("sim_fifo_read: off=%02x data=%02x remaining=%u pc=%08x t=%.8f\n",
					offset, data, m_simi->rx_count_r(), m_maincpu->pc(), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_GENSIO", 0) != 0 &&
			nokia_gensio_device::owns(offset) &&
			m_gensio_trace_count++ < 20000)
		logerror("gensio: R off=%02x data=%02x pc=%08x t=%.9f\n", offset, data,
				m_maincpu->pc(), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_TIMERS", 0) != 0 &&
			((offset >= 0x08 && offset <= 0x13) || offset == 0x0a) &&
			m_mad2_timer_trace_count++ < 4096)
		logerror("mad2_timer: event=R off=%02x data=%02x pc=%08x t=%.9f\n",
				u32(offset), data, m_maincpu->pc(), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_CLOCKS", 0) != 0 &&
			(offset == MAD2_MCU_RESET_CTRL || offset == MAD2_WATCHDOG ||
			 (offset >= MAD2_TIMER1_COUNTER_MSB && offset <= MAD2_TIMER1_COMPARE_LSB) ||
			 offset == 0x0d) && m_mad2_clock_trace_count++ < 4096)
		logerror("mad2_clock: event=R off=%02x data=%02x counter=%04x pc=%08x t=%.9f\n",
				u32(offset), data, m_mad2->timer1_counter(), m_maincpu->pc(), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_MBUS", 0) != 0 &&
			(offset == MAD2_MBUS_CTRL || offset == MAD2_MBUS_STATUS || offset == 0x1a ||
			 offset == MAD2_FIQ_STATUS || offset == MAD2_FIQ_MASK) && m_mbus_trace_count++ < 8192)
		logerror("mbus: event=R off=%02x data=%02x ctrl=%02x status=%02x fiq=%03x mask=%02x pc=%08x t=%.9f\n",
				u32(offset), data, m_mbus->control(), m_mbus->status(),
				m_mad2->fiq_status(), m_mad2->reg(MAD2_FIQ_MASK), m_maincpu->pc(), machine().time().as_double());
	if (offset == MAD2_FIQ_STATUS || offset == MAD2_IRQ_STATUS ||
			offset == MAD2_FIQ_MASK || offset == MAD2_IRQ_MASK ||
			offset == MAD2_IRQ_CTRL || offset == MAD2_FIQ8_CTRL)
		trace_interrupt_register('R', offset, data);

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
	const bool core_register = offset <= MAD2_FIQ8_CTRL;
	const bool mbus_register = offset >= MAD2_MBUS_CTRL && offset <= 0x1a;
	const bool gensio_register = nokia_gensio_device::owns(offset);
	uint8_t old_data = core_register ? m_mad2->read(offset) :
			(mbus_register ? (offset == MAD2_MBUS_CTRL ? m_mbus->control() :
				offset == MAD2_MBUS_STATUS ? m_mbus->status() : m_mbus->data()) : gensio_register ?
				m_gensio->peek(offset) :
				m_mad2_regs[offset]);
	if (core_register)
		m_mad2->write(offset, data);
	else if (mbus_register)
		m_mbus->write(offset - MAD2_MBUS_CTRL, data);
	else if (gensio_register)
		m_gensio->write(offset, data);
	else
		m_mad2_regs[offset] = data;
	if (offset == 0x15 || offset == 0x1c || offset == 0x1d || offset == 0x1e)
		update_buzzer();
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_TIMERS", 0) != 0 &&
			(offset >= 0x08 && offset <= 0x13) &&
			m_mad2_timer_trace_count++ < 4096)
		logerror("mad2_timer: event=W off=%02x data=%02x old=%02x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_CLOCKS", 0) != 0 &&
			(offset == MAD2_MCU_RESET_CTRL || offset == MAD2_WATCHDOG ||
			 (offset >= MAD2_TIMER1_COUNTER_MSB && offset <= MAD2_TIMER1_COMPARE_LSB) ||
			 offset == 0x0d) && m_mad2_clock_trace_count++ < 4096)
		logerror("mad2_clock: event=W off=%02x data=%02x old=%02x counter=%04x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_mad2->timer1_counter(), m_maincpu->pc(), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_MBUS", 0) != 0 &&
			(offset == MAD2_MBUS_CTRL || offset == MAD2_MBUS_STATUS || offset == 0x1a ||
			 offset == MAD2_FIQ_STATUS || offset == MAD2_FIQ_MASK) && m_mbus_trace_count++ < 8192)
		logerror("mbus: event=W off=%02x data=%02x old=%02x ctrl=%02x status=%02x fiq=%03x mask=%02x pc=%08x t=%.9f\n",
				u32(offset), data, old_data, m_mbus->control(), m_mbus->status(),
				m_mad2->fiq_status(), m_mad2->reg(MAD2_FIQ_MASK), m_maincpu->pc(), machine().time().as_double());
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
					old_data, m_simi->control_r(), m_maincpu->pc(), machine().time().as_double());
	}
	if (offset == 0x36 && m_simi->enabled())
		m_simi->txd_w(data);
	else if (offset == 0x38 && m_simi->enabled())
		m_simi->iir_w(data);
	else if (offset == 0x39 && m_simi->enabled())
		m_simi->control_w(data);
	else if (offset == 0x3d && m_simi->enabled())
		m_simi->rx_fifo_control_w(data);
	else if (offset == 0x3e && m_simi->enabled())
		m_simi->tx_fifo_control_w(data);
	if (nokia_env_u32("NOKI3210_TRACE_GENSIO", 0) != 0 &&
			gensio_register &&
			m_gensio_trace_count++ < 20000)
		logerror("gensio: W off=%02x data=%02x old=%02x pc=%08x t=%.9f\n", offset, data,
				old_data, m_maincpu->pc(), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DISPLAY_IO", 0) != 0 &&
			(offset == 0x2d || offset == 0x2e || offset == 0x6e) &&
			m_display_io_trace_count++ < 4096)
		logerror("display_io: off=%02x data=%02x old=%02x pc=%08x t=%.9f\n", offset,
				data, old_data, m_maincpu->pc(), machine().time().as_double());
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
	if (offset == 0x28 || offset == 0x6b || offset == 0xa8)
		update_keypad_columns();

	if (offset == MAD2_FIQ_STATUS || offset == MAD2_IRQ_STATUS ||
			offset == MAD2_FIQ_MASK || offset == MAD2_IRQ_MASK ||
			offset == MAD2_IRQ_CTRL || offset == MAD2_FIQ8_CTRL)
		trace_interrupt_register('W', offset, data);

	LOGMASKED(LOG_MAD2_REGISTER_ACCESS, "MAD2 W %02x = %02x %s\n", offset, data, nokia_mad2_reg_desc(offset));
}

void noki3310_state::update_buzzer()
{
	const u16 divider = (u16(m_mad2_regs[0x1c]) << 8) | m_mad2_regs[0x1d];
	if (divider != 0)
		m_buzzer->set_clock(13'000'000 / divider);
	const bool enabled = BIT(m_mad2->reg(0x15), 5) && divider != 0;
	m_buzzer->set_state(enabled);
	if (nokia_env_u32("NOKI3210_TRACE_BUZZER", 0) != 0)
		logerror("buzzer: enabled=%u divider=%u frequency=%u volume=%u t=%.6f\n",
				enabled, divider, divider ? 13'000'000 / divider : 0,
				m_mad2_regs[0x1e], machine().time().as_double());
}

uint8_t noki3310_state::mad2_dspif_r(offs_t offset)
{
	offset &= 3;
	const u8 data = m_dspif->dspif_r(offset);
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_LEDGER", 0) != 0 && !m_dspif_trace_read[offset])
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
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_LEDGER", 0) != 0 && !m_dspif_trace_write[offset])
	{
		m_dspif_trace_write[offset] = true;
		logerror("mad2_ledger: W bus=DSPIF off=%02x data=%02x old=%02x pc=%08x t=%.6f DSP API control\n",
				u32(offset), data, old_data, m_maincpu->pc(), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
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
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_LEDGER", 0) != 0 && !m_mcuif_trace_read[offset])
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
	if (nokia_env_u32("NOKI3210_TRACE_MAD2_LEDGER", 0) != 0 && !m_mcuif_trace_write[offset])
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
	update_keypad_columns();
	// A physical matrix switch edge, including release, wakes the keypad ISR.
	// Row-drive writes also recompute columns but must not manufacture edges.
	m_keypad_irq_latched = true;
	update_keypad_ccont_irqs();
}

INPUT_CHANGED_MEMBER( noki3310_state::charger_irq )
{
	// CCONT status bit 3 is the firmware-established charger event. The source
	// is latched on connection and remains set until firmware acknowledges it.
	if (newval)
		m_ccont->latch_irq_sources(0x08);
}

static INPUT_PORTS_START( noki3310 )
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

	SPEAKER(config, "mono").front_center();
	BEEP(config, m_buzzer).add_route(ALL_OUTPUTS, "mono", 0.25);

	INTEL_TE28F160(config, "flash");
	I2C_24C128(config, m_eeprom);
	NOKIA_MAD2(config, m_mad2);
	m_mad2->set_timer0_hz(nokia_env_u32("NOKI3210_TIMER0_HZ", 33055));
	m_mad2->set_timer1_hz(nokia_env_u32("NOKI3210_TIMER1_HZ", 1057));
	m_mad2->set_fiq8_hz(nokia_env_u32("NOKI3210_FIQ8_HZ", 1000));
	m_mad2->set_timer0_catchup(nokia_env_u32("NOKI3210_TIMER0_CATCHUP", 0) != 0);
	m_mad2->set_timer_trace(nokia_env_u32("NOKI3210_TRACE_MAD2_TIMERS", 0) != 0);
	m_mad2->set_interrupt_trace(nokia_env_u32("NOKI3210_TRACE_MAD2_INTERRUPTS", 0) != 0);
	m_mad2->fiq_cb().set(FUNC(noki3310_state::mad2_fiq_w));
	m_mad2->irq_cb().set(FUNC(noki3310_state::mad2_irq_w));
	m_mad2->irq_ack_cb().set(FUNC(noki3310_state::mad2_irq_ack_w));
	NOKIA_MBUS(config, m_mbus);
	m_mbus->set_trace(nokia_env_u32("NOKI3210_TRACE_MBUS", 0) != 0);
	m_mbus->tx_cb().set(FUNC(noki3310_state::mbus_tx_w));
	m_mbus->fiq2_cb().set(FUNC(noki3310_state::mbus_fiq2_w));
	m_mbus->fiq3_cb().set(FUNC(noki3310_state::mbus_fiq3_w));
	NOKIA_CCONT(config, m_ccont);
	// The low status bit is persistent CCONT reset state, not an IRQ source.
	// Clearing it provides the explicit missing/unready-CCONT fault fixture.
	m_ccont->set_ready(nokia_env_u32("NOKI3210_CCONT_READY", 1) != 0);
	m_ccont->irq_cb().set(FUNC(noki3310_state::ccont_irq_w));
	m_ccont->power_cb().set(FUNC(noki3310_state::ccont_power_w));
	NOKIA_GENSIO(config, m_gensio);
	m_gensio->ccont_read_cb().set(m_ccont, FUNC(nokia_ccont_device::serial_r));
	m_gensio->ccont_write_cb().set(m_ccont, FUNC(nokia_ccont_device::serial_w));
	m_gensio->ccont_select_cb().set(m_ccont, FUNC(nokia_ccont_device::select_w));
	m_gensio->lcd_dc_cb().set(m_pcd8544, FUNC(pcd8544_device::dc_w));
	m_gensio->lcd_sdin_cb().set(m_pcd8544, FUNC(pcd8544_device::sdin_w));
	m_gensio->lcd_sclk_cb().set(m_pcd8544, FUNC(pcd8544_device::sclk_w));
	NOKIA_DSPIF(config, m_dspif);
	NOKIA_DSP_HLE(config, m_dsp_hle);
	NOKIA_EXTERNAL_SERVICE_PEER(config, m_external_service_peer);
	const bool external_service_model = nokia_env_u32("NOKI3210_MODEL_EXTERNAL_SERVICE_PEER", 0) != 0;
	const unsigned dsp_default_ms = external_service_model ? 4 : 5;
	const bool dsp_trace = nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0;
	m_dspif->set_trace_enabled(dsp_trace);
	m_dspif->tx_commit_cb().set(FUNC(noki3310_state::dsp_tx_commit_w));
	m_dspif->service_pending_cb().set(FUNC(noki3310_state::dsp_service_pending_w));
	m_dspif->doorbell_cb().set(FUNC(noki3310_state::dsp_doorbell_w));
	m_dspif->bootstrap_fe_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::bootstrap_fe_w));
	m_dspif->bootstrap_100_cb().set(m_dsp_hle, FUNC(nokia_dsp_hle_device::bootstrap_100_w));
	m_dspif->fiq0_cb().set(FUNC(noki3310_state::dsp_fiq0_w));
	m_dspif->service_irq_cb().set(FUNC(noki3310_state::dsp_service_irq_w));
	m_dsp_hle->set_service_enabled(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE", 0) != 0);
	m_dsp_hle->set_external_service_enabled(external_service_model);
	m_dsp_hle->set_service_delay_ms(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_DELAY_MS", dsp_default_ms));
	m_dsp_hle->set_peer_poll_ms(nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_TICK_MS", dsp_default_ms));
	m_dsp_hle->set_trace_enabled(dsp_trace);
	m_external_service_peer->set_enabled(external_service_model);
	m_external_service_peer->set_trace_enabled(dsp_trace);
	NOKIA_SIMI(config, m_simi);
	NOKIA_SIM_CARD(config, m_sim_card);
	m_simi->irq_cb().set(FUNC(noki3310_state::sim_irq_w));
	m_sim_card->response_cb().set(m_simi, FUNC(nokia_simi_device::card_rx_w));
}

void noki3310_state::noki3330(machine_config &config)
{
	noki3310(config);

	INTEL_TE28F320(config.replace(), "flash");
}

void noki3310_state::noki3210(machine_config &config)
{
	noki3310(config);
	m_product = PRODUCT_3210;

	// Both supported 3210 firmware revisions use this validated composition.
	// Other DCT3 products retain the conservative base-device defaults until
	// their corresponding hardware and peer contracts have been exercised.
	// Both MAD2 timers use the 33,055 Hz CTSI source recovered for this product.
	// Timer 1 raises FIQ5 on 16-bit overflow; Timer 0 applies its programmed divider.
	m_mad2->set_timer0_hz(nokia_env_u32("NOKI3210_TIMER0_HZ", 33'055));
	m_mad2->set_timer1_hz(33'055);
	m_mad2->set_timer0_catchup(false);

	const bool external_service_model =
			nokia_env_u32("NOKI3210_MODEL_EXTERNAL_SERVICE_PEER", 1) != 0;
	const bool dsp_service_model = nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE", 1) != 0;
	const unsigned dsp_default_ms = external_service_model ? 4 : 5;
	m_dsp_hle->set_service_enabled(dsp_service_model);
	m_dsp_hle->set_external_service_enabled(external_service_model);
	m_dsp_hle->set_service_delay_ms(
			nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_DELAY_MS", dsp_default_ms));
	m_dsp_hle->set_peer_poll_ms(
			nokia_env_u32("NOKI3210_MODEL_DSP_SERVICE_TICK_MS", dsp_default_ms));
	m_external_service_peer->set_enabled(external_service_model);
}

void noki3310_state::noki5210(machine_config &config)
{
	noki3330(config);
	m_product = PRODUCT_5X10;
}

void noki3310_state::noki8xxx(machine_config &config)
{
	noki3310(config);
	m_product = PRODUCT_8XXX;
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
	ROM_SYSTEM_BIOS(1, "501", "v5.01")  // NSE-8 08-12-2000
	ROMX_LOAD("3210f600a.fls", 0x000000, 0x200000, CRC(6a978478) SHA1(6bdec2ec76aca15bc12b621be4402e455562454b), ROM_BIOS(0))
	ROMX_LOAD("3210f501.fls", 0x000000, 0x200000, CRC(e8d904a6) SHA1(8ad137c6aba9eb27bef067458d23016843f7fad5), ROM_BIOS(1))

	ROM_REGION(0x04000, "eeprom", ROMREGION_ERASEFF)
	ROMX_LOAD("3210 v600 eeprom.bin", 0x00000, 0x04000, CRC(f0153eaa) SHA1(ed1075357d3ee11cb1e532dc4b8ebd69b6c255ec), ROM_BIOS(0))
	ROMX_LOAD("3210 v501 eeprom.bin", 0x00000, 0x04000, CRC(6be8d3c6) SHA1(15cfad891555bb138a03d2398dfb0bdabaeab813), ROM_BIOS(1))
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
SYST( 1999, noki3210, 0,      0,      noki3210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3210", 0 )
SYST( 1999, noki7110, 0,      0,      noki7110, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 7110", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8210, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 1999, noki8850, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8850", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki3310, 0,      0,      noki3310, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3310", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6210, 0,      0,      noki6210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 6210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki6250, 0,      0,      noki6210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 6250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8250, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8250", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2000, noki8890, 0,      0,      noki8xxx, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 8890", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2001, noki3330, 0,      0,      noki3330, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3330", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki3410, 0,      0,      noki3410, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 3410", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
SYST( 2002, noki5210, 0,      0,      noki5210, noki3310, noki3310_state, empty_init, "Nokia", "Nokia 5210", MACHINE_NO_SOUND | MACHINE_NOT_WORKING )
