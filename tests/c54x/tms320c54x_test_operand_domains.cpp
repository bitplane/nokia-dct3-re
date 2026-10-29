// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_source_alias_case(unsigned variant)
{
	m_cpu->reset();
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_source_aliases.variant = variant;
	m_source_aliases.tick = 0;
	m_source_aliases.replay = false;
	unsigned const flags = variant / 7;
	auto const *profile = c54x_test::find_source_alias_profile(machine().options().bios());
	expect(profile && variant < profile->cases(), "Known source-alias profile and bounded case");
	u32 const base = flags & 4 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(base + 0x600, profile->opcode(variant));
	program.write_word(base + 0x601, 0x1500);
	program.write_word(base + 0x602, 3);
	program.write_word(base + 0x603, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, profile->st0(variant));
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ST1, 0x800 | (flags & 2 ? 0x20 : 0)}, {cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678},
		{cpu_regs::STATE_T, 0xabcd},
	});
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x1400 + i);
	m_source_aliases.observed = {};
	m_source_aliases.data_read_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mpy_alias_reads",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_source_aliases.observed.reads; });
	m_source_aliases.data_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "mpy_alias_writes",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_source_aliases.observed.writes; });
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(!(flags & 8));
	m_phase = 310700;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_source_alias_results()
{
	if (m_phase == 310700)
	{
		auto const *profile = c54x_test::find_source_alias_profile(machine().options().bios());
		expect(profile, "Source-alias continuation retains its profile");
		bool const long_memory = profile_is("long-abs-edge");
		bool const rmw = profile_is("rmw-abs-edge");
		bool const logic = profile_is("logic-abs-edge");
		bool const arithmetic = profile_is("arith-abs-edge");
		bool const load = profile_is("load-abs-edge");
		bool const memory_t = profile_is("mem-t-abs-edge");
		bool const mac = profile_is("mac-abs-edge");
		bool const delay = profile_is("delay-abs-edge");
		bool const pmac = profile_is("pmac-abs-edge");
		unsigned const variant = m_source_aliases.variant, flags = variant / 7;
		unsigned const tick = ++m_source_aliases.tick, checkpoint = flags & 16 ? 10 : 1;
		expect(!m_source_aliases.observed.reads && !m_source_aliases.observed.writes, "Unadmitted source absolute alias performs no data transaction");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == profile->st0(variant) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (flags & 2 ? 0x20 : 0)),
				"Unadmitted source absolute alias preserves accumulators, T and status");
		for (unsigned i = 0; i < 8; ++i)
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x1400 + i,
					"Unadmitted source alias preserves every AR");
		if (tick == checkpoint && !m_source_aliases.replay)
		{
			expect(capture_snapshot(machine(), m_source_aliases.saved) == STATERR_NONE,
					"Save source alias before or after rejection");
		}
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Both frontends reject unadmitted source absolute aliases");
			if (!m_source_aliases.replay)
			{
				m_source_aliases.replay = true;
				m_source_aliases.tick = checkpoint;
				expect(restore_snapshot(machine(), *m_check_timer, m_source_aliases.saved) == STATERR_NONE,
						"Restore source rejection without operand effects");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_phase = 310710;
				m_check_timer->enable(true);
				return true;
			}
			m_source_aliases.data_read_tap.remove();
			m_source_aliases.data_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < profile->cases())
			{
				start_source_alias_case(variant + 1);
				return true;
			}
			if (long_memory)
			{
				osd_printf_info("TMS320C54x long-memory absolute alias exclusions: PASS cases=1568 restore=1568 modes=f9..ff families=7 destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated long-memory absolute alias exclusions complete");
			}
			if (arithmetic)
			{
				osd_printf_info("TMS320C54x arithmetic absolute alias exclusions: PASS cases=2688 restore=2688 modes=f9..ff families=12 destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated arithmetic absolute alias exclusions complete");
			}
			if (logic)
			{
				osd_printf_info("TMS320C54x logic absolute alias exclusions: PASS cases=672 restore=672 modes=f9..ff instructions=AND,OR,XOR destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated logic absolute alias exclusions complete");
			}
			if (rmw)
			{
				osd_printf_info("TMS320C54x RMW absolute alias exclusions: PASS cases=896 restore=896 modes=f9..ff families=4 initial_tc=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated RMW absolute alias exclusions complete");
			}
			if (load)
			{
				osd_printf_info("TMS320C54x load absolute alias exclusions: PASS cases=1344 restore=1344 modes=f9..ff families=6 destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated load absolute alias exclusions complete");
			}
			if (memory_t)
			{
				osd_printf_info("TMS320C54x memory T multiply absolute alias exclusions: PASS cases=1792 restore=1792 modes=f9..ff families=8 destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated memory T multiply absolute alias exclusions complete");
			}
			if (pmac)
			{
				osd_printf_info("TMS320C54x program multiply absolute alias exclusions: PASS cases=448 restore=448 modes=f9..ff instructions=MACP,MACD destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated program multiply absolute alias exclusions complete");
			}
			if (delay)
			{
				osd_printf_info("TMS320C54x delay absolute alias exclusions: PASS cases=224 restore=224 modes=f9..ff instructions=LTD,DELAY cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated delay absolute alias exclusions complete");
			}
			if (mac)
			{
				osd_printf_info("TMS320C54x MAC absolute alias exclusions: PASS cases=448 restore=448 modes=f9..ff sources=2 destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated MAC absolute alias exclusions complete");
			}
			osd_printf_info("TMS320C54x MPY absolute alias exclusions: PASS cases=224 restore=224 modes=f9..ff destinations=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated MPY absolute alias exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 310710)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 310700;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	return false;
}

void tms320c54x_test_state::start_short_read_mod3_edge(unsigned variant)
{
	m_cpu->reset(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_short_sources.variant = variant; m_short_sources.tick = 0; m_short_sources.replay = false;
	u32 const base = (variant & 32) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	bool const rmw = variant >= 59136;
	bool const delay = variant >= 58624 && !rmw, pmac = variant >= 58112 && variant < 58624, transfer = variant >= 55296 && variant < 58112, paired = variant >= 51712 && variant < 55296, stack = variant >= 51200 && variant < 51712, multiply = variant >= 46848 && variant < 51200, arithmetic = variant >= 11008 && variant < 46848, shifted_arithmetic = arithmetic && variant >= 14080;
	bool const bitfield = variant >= 10496 && variant < 11008, load = variant >= 768 && variant < 2048, dld = variant >= 10240 && variant < 10496, shifted = variant >= 2048 && variant < 10240, subc = variant >= 256 && variant < 768, repeat = subc && variant >= 512;
	u16 const family = transfer ? std::array<u16, 11>{0x1800,0x1900,0x1a00,0x1b00,0x1c00,0x1d00,0x7100,0x7300,0x7500,0x7f00,0x7d00}[(variant - 55296) / 256] : paired ? std::array<u16, 7>{0x5000,0x5200,0x5400,0x5800,0x5a00,0x5c00,0x5e00}[(variant - 51712) / 512] : stack ? ((variant - 51200) / 256 ? 0x4a00 : 0x4800) : multiply ? std::array<u16, 17>{0x2000,0x2200,0x2400,0x2600,0x2800,0x2a00,0x2c00,0x2e00,0x3000,0x3200,0x3400,0x3600,0x3800,0x3a00,0x6400,0x6600,0x4600}[(variant - 46848) / 256] : shifted_arithmetic ? 0x6f00 : arithmetic ? std::array<u16, 12>{0x0000,0x0200,0x0400,0x0600,0x0800,0x0a00,0x0c00,0x0e00,0x3c00,0x3e00,0x4000,0x4200}[(variant - 11008) / 256] : bitfield ? 0x6000 : dld ? 0x5600 : shifted ? 0x6f00 : load ? std::array<u16, 5>{0x1000, 0x1200, 0x1400, 0x1600, 0x4400}[(variant - 768) / 256] : subc ? 0x1e00 : 0x6200;
	u16 const opcode = (rmw ? 0x6800 + ((variant - 59136) / 256) * 0x100 : delay ? ((variant - 58624) / 256 ? 0x4d00 : 0x4c00) : pmac ? ((variant - 58112) / 256 ? 0x7a00 : 0x7800) : family) | 0x98 | (variant & 7) | (!transfer && !delay && !rmw && (variant & 8) ? 0x100 : 0);
	program.write_word(base + 0x600, repeat ? 0xec03 : opcode);
	unsigned const operation = shifted_arithmetic ? (variant - 14080) / 8192 : 0;
	u16 const extension = 0x0c00 | ((operation & 1) << 9) | ((operation & 2) << 4) | ((variant & 8) ? 0x100 : 0) | (shifted_arithmetic ? ((variant - 14080) / 256) % 32 : 0);
	program.write_word(base + 0x601, shifted_arithmetic ? extension : shifted ? 0x0c40 | ((variant & 8) ? 0x100 : 0) | ((variant - 2048) / 256) : repeat ? opcode : subc ? 0xf4e1 : 3); program.write_word(base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	// CMPT is ST1 bit 5, not PMST bit 14.
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (bitfield && variant >= 10752) || ((transfer || pmac || delay || rmw) && (variant & 8)) ? 0x0804 : 0x1804); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (multiply ? 0x0815 : 0x800 | (paired && (variant & 256) ? 0x80 : 0)) | (variant & 16 ? 0x20 : 0));
	if (multiply) m_cpu->set_state_int(tms320c54x_device::STATE_RC, 7);
	if (stack) m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xabcd);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x1400 + i);
	m_short_sources.observed = {};
	m_short_sources.data_read_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "short_read_mod3_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_short_sources.observed.reads; });
	m_short_sources.data_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "short_read_mod3_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_short_sources.observed.writes; });
	if (transfer || pmac || delay || rmw)
	{
		m_short_sources.observed.program_writes = m_short_sources.observed.io_accesses = 0;
		m_short_sources.program_tap = program.install_write_tap(0, 0x7fffff, "short_read_mod3_program_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_short_sources.observed.program_writes; });
		m_short_sources.io_tap = m_cpu->space(AS_IO).install_write_tap(0, 0xffff, "short_read_mod3_io_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_short_sources.observed.io_accesses; });
	}
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(!(variant & 64));
	m_phase = 310600; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_short_source_results()
{
	if (m_phase == 310600)
	{
		unsigned const variant = m_short_sources.variant, tick = ++m_short_sources.tick, checkpoint = (variant & 128) ? 10 : 1;
		expect(!m_short_sources.observed.reads && !m_short_sources.observed.writes, "Write-only short MOD=3 source performs no data transaction");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xabcd && m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((variant >= 10752 && variant < 11008) || (variant >= 55296 && (variant & 8)) ? 0x0804 : 0x1804), "Rejected source preserves accumulators, T and arithmetic flags");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((variant >= 46848 && variant < 51200 ? 0x0815 : 0x800 | (variant >= 51712 && variant < 55296 && (variant & 256) ? 0x80 : 0)) | (variant & 16 ? 0x20 : 0)), "Rejected source preserves ASM, CMPT, C16 and the other ST1 fields");
		if (variant >= 55296) expect(!m_short_sources.observed.program_writes && !m_short_sources.observed.io_accesses, "Rejected transfer performs no program-memory or I/O write");
		if (variant >= 46848 && variant < 51200) expect(m_cpu->state_int(tms320c54x_device::STATE_RC) == 7, "Rejected source preserves the repeat counter");
		if (variant >= 51200 && variant < 51712) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000, "Rejected push preserves the stack pointer");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x1400 + i, "Rejected source does not preincrement any AR");
		if (tick == checkpoint && !m_short_sources.replay)
		{
			expect(capture_snapshot(machine(), m_short_sources.saved) == STATERR_NONE,
					"Save source before or after rejection");
		}
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Both frontends reject write-only short source addressing");
			if (!m_short_sources.replay)
			{
				m_short_sources.replay = true;
				m_short_sources.tick = checkpoint;
				expect(restore_snapshot(machine(), *m_check_timer, m_short_sources.saved) == STATERR_NONE,
						"Restore rejected source without operand effects");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_phase = 310610;
				m_check_timer->enable(true);
				return true;
			}
			m_short_sources.data_read_tap.remove();
			m_short_sources.data_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (variant >= 55296)
			{
				m_short_sources.program_tap.remove();
				m_short_sources.io_tap.remove();
			}
			if (variant < (variant >= 59136 ? 60159U : variant >= 58624 ? 59135U : variant >= 58112 ? 58623U : variant >= 55296 ? 58111U : variant >= 51712 ? 55295U : variant >= 51200 ? 51711U : variant >= 46848 ? 51199U : variant >= 11008 ? 46847U : variant >= 10496 ? 11007U : variant >= 768 ? 10495U : variant >= 256 ? 767U : 255U)) { start_short_read_mod3_edge(variant + 1); return true; }
			if (variant >= 59136)
			{
				osd_printf_info("TMS320C54x RMW MOD3 exclusions: PASS cases=1024 restore=1024 opcode_classes=4 arf=8 cmpt=2 initial_tc=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated XORM complete");
			}
			if (variant >= 58624)
			{
				osd_printf_info("TMS320C54x delay MOD3 exclusions: PASS cases=512 restore=512 opcode_classes=2 arf=8 cmpt=2 initial_tc=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated delay MOD3 exclusions complete");
			}
			if (variant >= 58112)
			{
				osd_printf_info("TMS320C54x program multiply MOD3 exclusions: PASS cases=512 restore=512 opcode_classes=4 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated program multiply MOD3 exclusions complete");
			}
			if (variant >= 55296)
			{
				osd_printf_info("TMS320C54x logic transfer MOD3 exclusions: PASS cases=2816 restore=2816 opcode_classes=11 arf=8 cmpt=2 initial_tc=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated short memory logic complete");
			}
			if (variant >= 51712)
			{
				osd_printf_info("TMS320C54x paired arithmetic MOD3 exclusions: PASS cases=3584 restore=3584 opcode_classes=14 arf=8 cmpt=2 c16=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, profile_is("drsub") ? "TMS320C54x isolated DRSUB complete" : "TMS320C54x isolated long-pair controls complete");
			}
			if (variant >= 51200)
			{
				osd_printf_info("TMS320C54x MMR push MOD3 exclusions: PASS cases=512 restore=512 opcode_classes=4 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 stack_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated MMR push MOD3 exclusions complete");
			}
			if (variant >= 46848)
			{
				osd_printf_info("TMS320C54x multiply load MOD3 exclusions: PASS cases=4352 restore=4352 opcode_classes=34 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 asm_changes=0 rc_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated multiply load MOD3 exclusions complete");
			}
			if (variant >= 11008)
			{
				osd_printf_info("TMS320C54x arithmetic MOD3 exclusions: PASS cases=35840 restore=35840 scalar_classes=24 variable_selectors=8 shifts=32 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 alu_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated arithmetic MOD3 exclusions complete");
			}
			if (variant >= 10496)
			{
				osd_printf_info("TMS320C54x bitfield MOD3 exclusions: PASS cases=512 restore=512 operations=CMPM,BITF initial_tc=2 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 tc_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated bitfield MOD3 exclusions complete");
			}
			if (variant >= 768)
			{
				osd_printf_info("TMS320C54x load MOD3 exclusions: PASS cases=9728 restore=9728 families=7 variable_shifts=32 arf=8 accumulators=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated load MOD3 exclusions complete");
			}
			if (variant >= 256)
			{
				osd_printf_info("TMS320C54x SUBC MOD3 exclusions: PASS cases=512 restore=512 arf=8 accumulators=2 cmpt=2 pages=2 frontends=2 snapshots=2 repeat=none,short data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated SUBC MOD3 exclusions complete");
			}
			osd_printf_info("TMS320C54x memory MPY MOD3 exclusions: PASS cases=256 restore=256 arf=8 accumulators=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated memory MPY MOD3 exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310610)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 310600;
		m_check_timer->enable(true);
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	return false;
}

} // namespace c54x_test
