// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_pipeline_frontend_case(bool memory, bool memory_b)
{
	m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
	m_vector_prefetch_tap.remove(); m_vector_stack_tap.remove();
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_frontend.memory = memory;
	m_frontend.memory_b = memory_b;
	m_frontend.operand_reads = 0;
	m_frontend.vector_tap.remove(); m_frontend.stack_tap.remove();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i)
	{
		program.write_word(0x600 + i, 0xf495);
		program.write_word(0x40 + i, 0xf495);
	}
	program.write_word(0x600, 0xe811); // Decoded before INTR, retires at cycle 6.
	if (memory) program.write_word(0x600, memory_b ? 0x1120 : 0x1020);
	program.write_word(0x601, 0xe822); // Fetched, not decoded; must not execute.
	program.write_word(0x602, 0xe833); // Prefetch already in flight; Fetch must drain.
	program.write_word(0x40, 0xe955); // One-word first ISR instruction, Execute at 10.
	data.write_word(0xfff, 0xbeef);
	data.write_word(0x420, 0x1234);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ST0, 8}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_IMR, 1},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0},
		{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {STATE_GENPC, 0x600}, {cpu_regs::STATE_IDLE, 0},
	});
	m_cpu->set_pipeline_frontend(true);
	m_frontend.expected_cycle = 1;
	m_frontend.replay = false;
	m_frontend.vector_reads = m_frontend.acks = 0;
	m_frontend.drained_fetches = 0;
	if (memory)
		m_frontend.operand_tap = data.install_read_tap(0x420, 0x420, "pipeline_operand",
			[this](offs_t, u16 &, u16) {
				if (machine().side_effects_disabled()) return;
				expect(m_cpu->pipeline_cycles() == 5, "DP-relative operand is read in Read, not Execute");
				++m_frontend.operand_reads;
			});
	m_frontend.fetch_tap = program.install_read_tap(0x602, 0x602, "pipeline_flush_fetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 4, "Already-prefetched flush word reaches Fetch at cycle 4");
			++m_frontend.drained_fetches;
		});
	m_frontend.vector_tap = program.install_read_tap(0x40, 0x40, "pipeline_vector",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 6 && m_frontend.acks == 1,
				"Vector Fetch occurs at cycle 6 once, after IACK");
			++m_frontend.vector_reads;
		});
	m_frontend.stack_tap = data.install_write_tap(0xfff, 0xfff, "pipeline_return_pc",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 && value == 0x601,
				"INTR Execute writes the first not-decoded continuation at cycle 7");
		});
	m_phase = 251000;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

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
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251000)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		osd_printf_info("TMS320C54x staged tick: cycle=%llu expected=%llu replay=%u pc=%06x a=%llx b=%llx sp=%04x ifr=%04x illegal=%u\n",
			static_cast<unsigned long long>(cycle), static_cast<unsigned long long>(m_frontend.expected_cycle), m_frontend.replay,
			unsigned(m_cpu->state_int(STATE_GENPC)), static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)),
			static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
		expect(cycle == m_frontend.expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"Staged frontend advances exactly one clock per observation without unsupported forms");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle < 6 || m_frontend.memory_b ? 0 : m_frontend.memory ? 0x1234 : 0x11) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 10 ? 0x55 : cycle >= 6 && m_frontend.memory_b ? 0x1234 : 0),
			"Decoded foreground retires at 6; first ISR retires at 10; flushed immediate loads never execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle < 5 ? 0x1000 : 0xfff) &&
			m_cpu->space(AS_DATA).read_word(0xfff) == (cycle < 7 ? 0xbeef : 0x601),
			"INTR Access and Execute own separate cycles 5 and 7");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		u64 const checkpoint = m_frontend.memory ? 5 : 4;
		if (cycle == checkpoint && !m_frontend.replay)
		{
			if (m_frontend.memory)
			{
				expect(m_frontend.operand_reads == 1, "Operand bus read completed once before checkpoint");
				m_cpu->space(AS_DATA).write_word(0x420, 0x5678);
			}
			expect(machine().scheduler().can_save(), "Decoded foreground and INTR checkpoint is saveable");
			m_frontend.saved.str({}); m_frontend.saved.clear();
			expect(machine().save().write_stream(m_frontend.saved) == STATERR_NONE,
				"Save decoded foreground and pending INTR before Access");
		}
		if (cycle == 10)
		{
			expect(m_frontend.vector_reads == 1 && m_frontend.acks == 1 && m_frontend.drained_fetches == 1,
				"Each leg drains its prefetched word and fetches/acknowledges one vector");
			if (!m_frontend.replay)
			{
				m_frontend.replay = true;
				m_frontend.vector_reads = m_frontend.acks = 0;
				// The flush word was already fetched at the saved cycle-4 boundary.
				m_frontend.saved.clear(); m_frontend.saved.seekg(0);
				expect(machine().save().read_stream(m_frontend.saved) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint,
					"Restore pending INTR and the decoded foreground slot");
				// Loading inside a live callback rewinds device local times while
				// the callback timestamp is still the old one. Let the scheduler
				// settle with this bench CPU suspended before single-clock replay.
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_phase = 251010;
				m_frontend.expected_cycle = checkpoint + 1;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			if (m_frontend.memory)
			{
				expect(m_frontend.operand_reads == 1, "Retirement and restored retirement reuse the captured operand without another bus read");
				if (m_frontend.memory_b)
					osd_printf_info("TMS320C54x staged direct load: PASS cases=2 read=5 execute=6 retained_operand=1 restore_after_read=1 bus_reads_per_case=1\n");
			}
			else
				osd_printf_info("TMS320C54x staged frontend: PASS foreground_execute=6 vector_fetch=6 stack_write=7 isr_execute=10 pending_restore=1 coverage=register_one_word native_enabled=0\n");
			m_frontend.fetch_tap.remove(); m_frontend.vector_tap.remove(); m_frontend.stack_tap.remove();
			m_frontend.operand_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (!m_frontend.memory) { start_pipeline_frontend_case(true); return true; }
			if (!m_frontend.memory_b) { start_pipeline_frontend_case(true, true); return true; }
			m_cpu->space(AS_PROGRAM).write_word(0x600, 0x77e1);
			set_registers(*m_cpu, {
				{STATE_GENPC, 0x600}, {cpu_regs::STATE_ST1, 0x0800}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_AR1, 0},
			});
			m_cpu->set_pipeline_frontend(true);
			m_phase = 251020;
			m_check_timer->adjust(attotime::from_ticks(3, m_cpu->clock()));
			return true;
		}
		++m_frontend.expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251010)
	{
		expect(m_cpu->pipeline_cycles() == (m_frontend.memory ? 5 : 4), "Scheduler settlement does not retire restored instructions");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251000;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251020)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0,
			"Unsupported staged multiword form stops at Decode without atomic fallback");
		m_cpu->set_pipeline_frontend(false);
		osd_printf_info("TMS320C54x staged frontend exclusions: PASS unsupported_stops=1 atomic_fallback=0\n");
		start_empty_pipeline_case(false);
		return true;
	}
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
		start_multiword_pipeline_case();
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
		if (m_phase == 251000)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 6 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
				"Staged vector IACK at cycle 6 clears its source and sets INTM");
			++m_frontend.acks;
		}
}

} // namespace c54x_test
