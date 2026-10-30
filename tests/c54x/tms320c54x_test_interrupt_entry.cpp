// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {


void tms320c54x_test_state::start_vector_prefetch_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	// Retire predecessor observers before installing this family's own taps.
	m_vector_prefetch_tap.remove();
	m_vector_stack_tap.remove();
	m_entry.fetch_tap.remove();
	m_entry.stack_tap.remove();
	m_entry.reads = m_entry.acknowledgements = 0;
	m_entry.vector_case = index;
	u32 const address = index ? 4 : 0x40;
	program.write_word(address, 0xf020); // LD #1234,A: retain opcode, fetch extension normally.
	program.write_word(address + 1, 0x1234);
	program.write_word(address + 2, 0xf4e1);
	program.write_word(0x0800, 0xf4e1);
	data.write_word(0x0fff, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_ST1, index ? 0x0800 : 0}, {cpu_regs::STATE_IMR, 1}, {cpu_regs::STATE_IFR, 3},
		{cpu_regs::STATE_A, 0}, {STATE_GENPC, 0x0800}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 80000;
	m_entry.fetch_tap = program.install_read_tap(address, address, "vector_prefetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
				"SPRU131G INTR Access decrements SP before vector Fetch, but stack memory is not written yet");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (m_entry.vector_case ? 3 : 2),
				"Vector acknowledgement clears only the accepted maskable source");
			++m_entry.reads;
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
		});
	m_entry.stack_tap = data.install_write_tap(0x0fff, 0x0fff, "vector_stack_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled() || m_phase != 80000) return;
			expect(m_entry.reads == 1 && m_entry.acknowledgements == 1 && value == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff,
				"SPRU131G INTR Execute writes RTN after vector Fetch without decrementing SP again");
		});
	if (index) { m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); }
	m_check_timer->adjust(attotime::from_usec(100));
}

bool tms320c54x_test_state::check_vector_prefetch_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 80000 && m_phase <= 80003)
	{
		u32 const address = m_entry.vector_case ? 4 : 0x40;
		if (m_phase == 80003)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(STATE_GENPC) == 0x0801,
				"Reset discards the retained hardware vector opcode");
			m_entry.fetch_tap.remove();
			if (m_entry.vector_case == 2) { start_vector_prefetch_case(3); return true; }
			m_entry.stack_tap.remove();
			osd_printf_info("TMS320C54x vector prefetch: PASS sources=irq,nmi before_stack=1 sp_access=1 retained_opcode=1 extension_fetch=1 save_replay=2 single_ack=1 later_mask=1 reset=1 debugger_redirect=1 physical_latency_claim=0\n");
			start_xc_irq_case(0); return true;
		}
		if (m_phase == 80000)
		{
			expect(m_entry.reads == 1 && m_entry.acknowledgements == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff && data.read_word(0x0fff) == 0xbeef,
				"Suspension after vector Fetch leaves the later continuation write pending");
			expect(machine().scheduler().can_save(), "Vector prefetch checkpoint has no synchronized inputs");
			if (m_entry.vector_case >= 2)
			{
				if (m_entry.vector_case == 2) { m_cpu->reset(); m_cpu->set_state_int(STATE_GENPC, 0x0800); }
				else m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
				m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80003;
				m_check_timer->adjust(attotime::from_usec(100)); return true;
			}
			expect(capture_snapshot(machine(), m_entry.saved) == STATERR_NONE, "Save retained vector opcode");
			program.write_word(address, 0xf4e1); // Must not replace the already fetched LD.
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); // A later mask cannot revoke accepted entry.
			m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80001;
		}
		else
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				data.read_word(0x0fff) == 0x0800 &&
				m_entry.reads == 1 && m_entry.acknowledgements == 1,
				"Retained vector opcode executes once with its normally fetched extension");
			if (m_phase == 80001)
			{
				expect(restore_snapshot(machine(), m_entry.saved) == STATERR_NONE, "Restore retained vector opcode");
				program.write_word(address, 0xf4e1);
				m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
				m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80002;
			}
			else
			{
				m_entry.fetch_tap.remove();
				start_vector_prefetch_case(m_entry.vector_case + 1); return true;
			}
		}
		m_check_timer->adjust(attotime::from_usec(100)); return true;
	}
	return false;
}


void tms320c54x_test_state::acknowledge_entry_interrupt(int line)
{
	if (m_phase >= 80000 && m_phase <= 80003)
	{
		expect(line == (m_entry.vector_case ? INPUT_LINE_NMI : 0), "Vector acknowledgement retains accepted source identity");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
			"SPRU131G vector acknowledgement follows SP Access but precedes the continuation memory write");
		++m_entry.acknowledgements;
	}
}

} // namespace c54x_test
