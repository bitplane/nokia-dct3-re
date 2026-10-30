// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::check_mmr_bus()
{
	auto &data = m_cpu->space(AS_DATA);
	u16 const ar = m_cpu->state_int(tms320c54x_device::STATE_AR3);
	u16 const flags = m_cpu->state_int(tms320c54x_device::STATE_IFR);
	u16 const gap = data.read_word(3);
	unsigned observed = 0;
	auto tap = data.install_write_tap(0x13, 0x13, "mmr_bus_check",
		[&observed](offs_t, u16 &, u16) { ++observed; });
	data.write_word(0x13, 0x1234);
	expect(m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 && data.read_word(0x13) == 0x1234,
		"Mapped AR3 and CPU state share one register");
	data.write_word(0x13, 0xab00, 0xff00); data.write_word(0x13, 0x00cd, 0x00ff);
	expect(data.read_word(0x13) == 0xabcd && observed == 3, "MMR partial writes preserve unselected bits and reach passive taps");
	tap.remove();
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xbeef);
	data.write_word(1, 0x00ef, 0x00ff);
	expect(data.read_word(1) == 0xbe00, "IFR byte write clears only selected low flags");
	data.write_word(1, 0x0e00, 0xff00);
	expect(data.read_word(1) == 0xb000, "IFR byte write clears only selected high flags");
	data.write_word(3, 0x5a5a);
	expect(data.read_word(3) == 0x5a5a, "Reserved CPU register gap stays in the board map");
	data.write_word(3, gap);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, ar);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, flags);
	osd_printf_info("TMS320C54x MMR bus: PASS unified_state=1 partial_write=1 ifr_clear=1 passive_tap=1 reserved_gap=1\n");
}



bool tms320c54x_test_state::standard_bus_io() const
{ return profile_is("std-io") || profile_is("std-io-atomic"); }

bool tms320c54x_test_state::atomic_bus_io() const
{ return profile_is("bus-io-atomic") || profile_is("std-io-atomic"); }

unsigned tms320c54x_test_state::bus_io_form(unsigned variant) const
{ return atomic_bus_io() ? variant / 576 : 0; }

u16 tms320c54x_test_state::bus_io_address(unsigned variant) const
{ unsigned const form = bus_io_form(variant); return form == 3 ? 0x30 : form == 4 ? 0x11 : 0x1400; }

u64 tms320c54x_test_state::bus_io_clock() const
{ return atomic_bus_io() ? m_cpu->total_cycles() - m_bus_start_cycles + 1 : m_cpu->pipeline_cycles(); }

tms320c54x_test_state::bus_io_fixture tms320c54x_test_state::bus_io_case(unsigned variant) const
{
	unsigned const v = variant % 192, waits = (v % 8) * (1 + (v / 8) % 2);
	bool const hold = (v / 16) % 2, write = (v / 32) % 2;
	unsigned const form = bus_io_form(variant);
	unsigned const first = atomic_bus_io() ? 2 + unsigned(form >= 1 && form <= 3) + unsigned(form == 3) : write ? 7 : 6, complete = first + waits + (hold && waits >= 2 ? 3 : 0);
	// Input pulses are scheduler-synchronized; save after their delivery settles.
	return {waits, first, complete, std::array<unsigned, 3>{first - 1, first + 1, complete + 1}[(variant % 576) / 192], v / 64, write, hold,
		u16(write && form == 4 ? 0x11 : write || !waits ? 0x1234 : 0xabcd)};
}

void tms320c54x_test_state::start_bus_io_test(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = bus_io_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(0x600 + i, 0xf495); program.write_word(i, 0xf495); }
	unsigned const form = bus_io_form(variant);
	bool const extended = form >= 1 && form <= 3;
	program.write_word(0x600, (f.write ? 0x7500 : 0x7400) | (form == 1 ? 0xe1 : form == 2 || form == 3 ? 0xf8 : form == 4 ? 0x91 : 0x81));
	program.write_word(0x601, form == 2 || form == 3 ? bus_io_address(variant) : 0x125);
	if (extended) program.write_word(0x602, form == 1 ? 1 : 0x125);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1804); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, form == 1 ? 0x13ff : form == 4 ? 0x11 : 0x1400);
	data.write_word(0x1400, 0x1234); m_cpu->space(AS_IO).write_word(0x125, 0x1234);
	if (form == 3) data.write_word(0x30, 0x1234);
	data.write_word(0x28, (variant % 8) << 12); data.write_word(0x2b, (variant / 8) % 2);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0xffff); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
	m_bus_ready = !f.hold; m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(!atomic_bus_io());
	m_bus_start_cycles = m_cpu->total_cycles();
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_IO).install_read_tap(0x125, 0x125, "bus_io_read",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!f.write && !m_pipeline_operand_reads && word == f.value && bus_io_clock() == f.complete, "External PORTR samples once after software/READY waits"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_IO).install_write_tap(0x125, 0x125, "bus_io_write",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.write && !m_pipeline_store_writes && word == f.value && bus_io_clock() == f.complete, "External PORTW preserves one retained source through waits"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(0x601, extended ? 0x602 : 0x601, "bus_io_literal",
		[this, extended](offs_t address, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(m_pipeline_extensions_reads < 1 + unsigned(extended) && address == 0x601 + m_pipeline_extensions_reads, "External I/O extensions retain their order across stalls"); ++m_pipeline_extensions_reads; } });
	m_phase = 253550; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_standard_swwsr_test()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &data = m_cpu->space(AS_DATA);
	expect(data.read_word(0x28) == 0x7fff, "Original-family SWWSR resets to seven waits in each field");
	for (unsigned value = 0; value < 0x10000; ++value)
	{
		data.write_word(0x29, value); data.write_word(0x2b, value);
		data.write_word(0x28, value);
		expect(data.read_word(0x28) == (value & 0x7fff) && m_cpu->external_io_wait_states() == ((value >> 12) & 7), "Original SWWSR masks reserved bit 15 and has no multiplier");
		expect(data.read_word(0x29) == value && data.read_word(0x2b) == value && m_cpu->state_int(tms320c54x_device::STATE_BSCR) == 0 && m_cpu->state_int(tms320c54x_device::STATE_SWCR) == 0, "Original SWWSR option does not acquire C5416 register ownership");
	}
	data.write_word(0x28, 0x7fff); data.write_word(0x28, 0, 0x8000);
	expect(data.read_word(0x28) == 0x7fff, "Partial reserved-bit write cannot clear wait fields");
	data.write_word(0x28, 0xabcd, 0x00ff);
	expect(data.read_word(0x28) == 0x7fcd, "Partial original SWWSR write preserves other fields");
	data.write_word(0x28, 0x2000);
	expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save original-family SWWSR");
	data.write_word(0x28, 0);
	expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE && data.read_word(0x28) == 0x2000 && m_cpu->external_io_wait_states() == 2, "Restore the firmware's two-I/O-wait setting");
	m_cpu->reset(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	expect(data.read_word(0x28) == 0x7fff, "Reset restores the original SWWSR contract");
	osd_printf_info("TMS320C54x standard SWWSR: PASS encodings=65536 reserved_bit15=0 multiplier=0 separate_BSCR_SWCR=1 partial_writes=1 restore=1 reset=1 program_data_wait_claim=0 physical_mapping=unvalidated\n");
	throw emu_fatalerror(0, "TMS320C54x isolated standard SWWSR complete");
}

void tms320c54x_test_state::start_bus_register_test()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &data = m_cpu->space(AS_DATA);
	expect(data.read_word(0x28) == 0x7fff && data.read_word(0x29) == 0xf000 && data.read_word(0x2b) == 0 &&
		m_bus_divisor == 4 && !m_bus_iack_enable, "C5416-compatible bus controls reset from SPRS095P tables 3-3/3-4/3-5");
	u32 const system_clock = m_cpu->clock();
	for (unsigned value = 0; value < 0x10000; ++value)
	{
		data.write_word(0x28, value); data.write_word(0x29, value); data.write_word(0x2b, value);
		expect(data.read_word(0x28) == value && data.read_word(0x29) == (value & 0xf006) && data.read_word(0x2b) == (value & 1), "Bus-control reserved fields ignore writes");
		expect(m_cpu->external_io_wait_states() == ((value >> 12) & 7) * (1 + (value & 1)) &&
			m_cpu->clkout_divisor() == 1 + ((value >> 13) & 3) && m_bus_divisor == m_cpu->clkout_divisor() &&
			m_bus_iack_enable == !(value & 0x1000) && m_cpu->iack_output_enabled() == m_bus_iack_enable && m_cpu->clock() == system_clock,
			"Register decode publishes independent CLKOUT/IACK configuration without changing the system clock");
	}
	data.write_word(0x28, 0x7fff); data.write_word(0x29, 0x8006); data.write_word(0x2b, 0);
	expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save CPU-owned bus controls");
	data.write_word(0x28, 0); data.write_word(0x29, 0xffff); data.write_word(0x2b, 1);
	expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore bus controls and derived outputs");
	expect(data.read_word(0x28) == 0x7fff && data.read_word(0x29) == 0x8006 && !data.read_word(0x2b) && m_bus_divisor == 1 && m_bus_iack_enable && m_cpu->external_io_wait_states() == 7,
		"Saved bus state re-drives exact configuration outputs");
	data.write_word(0x29, 0, 0x8000); data.write_word(0x29, 0xffff, 0x0ff9);
	data.write_word(0x2b, 0xffff, 0xfffe); data.write_word(0x28, 0xabcd, 0x00ff);
	expect(data.read_word(0x29) == 6 && data.read_word(0x2b) == 0 && data.read_word(0x28) == 0x7fcd, "Partial writes merge only writable selected fields");
	m_cpu->reset(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	expect(data.read_word(0x28) == 0x7fff && data.read_word(0x29) == 0xf000 && !data.read_word(0x2b) && m_bus_divisor == 4 && !m_bus_iack_enable,
		"Device reset restores control state and outputs, independently of saved values");
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(0x600 + i, 0xf495); program.write_word(0x40 + i, 0xf495); }
	program.write_word(0x40, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(false); m_cpu->set_input_line(0, ASSERT_LINE);
	m_vector_acks = 0; m_phase = 253530; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_usec(5));
}











u16 tms320c54x_test_state::test_port_r()
{
	const u64 cycle = m_cpu->total_cycles();
	if (m_port_reads++ == 0)
		m_first_port_cycle = cycle;
	else
		m_last_port_cycle = cycle;
	return 0xabcd;
}

void tms320c54x_test_state::test_port_w(u16 value)
{
	const u64 cycle = m_cpu->total_cycles();
	if (m_port_writes++ == 0)
	{
		m_first_port_cycle = cycle;
		m_first_port_value = value;
	}
	else
	{
		if (m_port_writes == 2)
		{
			m_middle_port_value = value;
			m_middle_port_cycle = cycle;
		}
		m_last_port_cycle = cycle;
		m_last_port_value = value;
	}
}

bool tms320c54x_test_state::check_external_io_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253550)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = bus_io_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged external wait path has no atomic fallback");
		if (tick == (atomic_bus_io() ? 1 : 6) && f.write) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1400, 0xdead); }
		if (tick == f.first)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x28, 0); m_cpu->space(AS_DATA).write_word(0x2b, 0);
			m_cpu->space(AS_IO).write_word(0x125, 0xabcd); m_cpu->space(AS_PROGRAM).write_word(0x601, 0xbeef);
			// A zero-wait aliased read has already published AR1; do not erase its result.
			if (bus_io_form(variant) != 4 || f.waits) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1500);
			if (bus_io_form(variant) >= 1 && bus_io_form(variant) <= 3) m_cpu->space(AS_PROGRAM).write_word(0x602, 0xdead);
			if (f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		}
		if (f.hold && f.waits >= 2 && tick == f.complete - 1) m_bus_ready = true;
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads, &m_vector_acks},
			"Save external transfer before/during waits or after its side effect");
		if (tick == f.complete + 12)
		{
			unsigned const form = bus_io_form(variant), literals = 1 + unsigned(form >= 1 && form <= 3);
			u16 const expected_ram = f.value + unsigned(form == 4 && !f.write);
			{ auto const disable = machine().disable_side_effects();
				if ((!f.write && m_cpu->space(AS_DATA).read_word(bus_io_address(variant)) != expected_ram) || m_pipeline_operand_reads != unsigned(!f.write) || m_pipeline_store_writes != unsigned(f.write) || m_pipeline_extensions_reads != literals || m_vector_acks != unsigned(bool(f.interrupt)))
					osd_printf_info("Bus I/O failure: case=%u replay=%u waits=%u write=%u hold=%u irq=%u ram=%04x expected=%04x reads=%u writes=%u literals=%u acks=%u pc=%06x\n", variant, unsigned(m_pipeline_replay), f.waits, unsigned(f.write), unsigned(f.hold), f.interrupt, m_cpu->space(AS_DATA).read_word(0x1400), f.value, m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_vector_acks, unsigned(m_cpu->state_int(STATE_GENPC))); }
			{ auto const disable = machine().disable_side_effects(); expect((f.write || m_cpu->space(AS_DATA).read_word(bus_io_address(variant)) == expected_ram) && m_pipeline_operand_reads == unsigned(!f.write) && m_pipeline_store_writes == unsigned(f.write) && m_pipeline_extensions_reads == literals && m_vector_acks == unsigned(bool(f.interrupt)), "Waited transfer keeps address/data ownership, MMR alias ordering and deferred interrupt delivery"); }
			unsigned const elapsed = 0xffff - unsigned(m_cpu->state_int(tms320c54x_device::STATE_TIM));
			expect(elapsed >= tick - 1 && elapsed <= tick + 1, "Timer clocks continue during external bus suspension");
			if (replay_pipeline_at(253560, f.checkpoint,
				"Restore retained wait/READY state without duplicate I/O")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			unsigned next = variant + 1;
			// Original SWWSR has no multiplier: omit, rather than relabel,
			// the existing fixture's multiplier-enabled raw case IDs.
			if (standard_bus_io() && (next & 8)) next += 8;
			if (next < (atomic_bus_io() ? 2880 : 576)) { start_bus_io_test(next); return true; }
			m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
			if (standard_bus_io())
			{
				osd_printf_info("TMS320C54x standard SWWSR I/O: PASS frontend=%s cases=%u restore=%u software_waits=0..7 ready_threshold=2 sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 program_data_wait_claim=0 physical_mapping=unvalidated\n", atomic_bus_io() ? "atomic" : "staged", atomic_bus_io() ? 1440 : 288, atomic_bus_io() ? 1440 : 288);
				throw emu_fatalerror(0, "TMS320C54x isolated standard SWWSR I/O complete");
			}
			if (atomic_bus_io())
			{
				osd_printf_info("TMS320C54x atomic external I/O waits: PASS cases=2880 restore=2880 forms=short,offset,absolute,peripheral,AR_alias software_waits=0..14 ready_threshold=2 directions=read,write sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 physical_mapping=unvalidated\n");
				throw emu_fatalerror(0, "TMS320C54x isolated atomic external I/O complete");
			}
			osd_printf_info("TMS320C54x staged external I/O waits: PASS cases=576 restore=576 software_waits=0..14 ready_threshold=2 directions=read,write sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 atomic_acceptance=separate physical_mapping=unvalidated\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged external I/O complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253560) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253550; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
