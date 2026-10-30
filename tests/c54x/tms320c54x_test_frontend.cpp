// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {


void tms320c54x_test_state::start_empty_pipeline_case(bool nmi)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_frontend.nmi = nmi;
	m_frontend.expected_cycle = 1;
	m_frontend.vector_reads = m_frontend.acks = 0;
	u16 const vector = nmi ? 4 : 0x40;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(vector + i, 0xf495);
	program.write_word(vector, 0xe955);
	program.write_word(0x600, 0xe811);
	m_cpu->space(AS_DATA).write_word(0xfff, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST1, nmi ? 0x0800 : 0}, {cpu_regs::STATE_IMR, nmi ? 0 : 1},
		{cpu_regs::STATE_IFR, nmi ? 2 : 0}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0},
		{cpu_regs::STATE_B, 0}, {STATE_GENPC, 0x600},
	});
	m_cpu->set_pipeline_frontend(true);
	m_frontend.vector_tap = program.install_read_tap(vector, vector, "empty_pipeline_vector",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 4 && m_frontend.acks == 1,
				"Empty frontend vector Fetch follows IACK at cycle 4");
			++m_frontend.vector_reads;
		});
	m_phase = 251030;
	m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE);
	m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, CLEAR_LINE);
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_pipeline_frontend_results()
{
	if (m_phase == 251030)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_frontend.expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"Empty frontend advances one clock without fallback");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle < 8 ? 0 : 0x55),
			"Undecoded foreground is flushed; ISR retires at cycle 8");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle < 3 ? 0x1000 : 0xfff) &&
			m_cpu->space(AS_DATA).read_word(0xfff) == (cycle < 5 ? 0xbeef : 0x600),
			"Empty frontend stacks the unexecuted first instruction at cycle 5");
		if (cycle < 8)
		{
			++m_frontend.expected_cycle;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
			return true;
		}
		expect(m_frontend.vector_reads == 1 && m_frontend.acks == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == (m_frontend.nmi ? 2 : 0),
			"Empty IRQ/NMI acknowledges once and preserves unrelated pending sources");
		m_frontend.vector_tap.remove();
		m_cpu->set_pipeline_frontend(false);
		if (!m_frontend.nmi) { start_empty_pipeline_case(true); return true; }
		osd_printf_info("TMS320C54x staged empty frontend: PASS irq=1 nmi=1 return_pc=0600 vector_fetch=4 stack_write=5 isr_execute=8\n");
		return true;
	}
	return false;
}

void tms320c54x_test_state::acknowledge_frontend_interrupt(int line)
{
		if (m_phase == 251030)
		{
			expect(line == (m_frontend.nmi ? INPUT_LINE_NMI : 0) && m_cpu->pipeline_cycles() == 4,
				"Empty frontend acknowledges the selected IRQ/NMI at vector Fetch");
			++m_frontend.acks;
		}
}

} // namespace c54x_test
