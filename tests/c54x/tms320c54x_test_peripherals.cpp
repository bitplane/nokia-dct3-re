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

} // namespace c54x_test
