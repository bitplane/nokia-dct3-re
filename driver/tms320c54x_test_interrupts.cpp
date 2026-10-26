// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_interrupt_entry_spacing_case(unsigned index)
{
	m_cpu->reset();
	m_vector_prefetch_tap.remove();
	m_vector_stack_tap.remove();
	m_vector_reads = m_entry_stack_writes = 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	program.write_word(index ? 4 : 0x40, 0xf4e1);
	data.write_word(0x0fff, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_vector_prefetch_tap = program.install_read_tap(index ? 4 : 0x40, index ? 4 : 0x40,
		"entry_fetch_cycle", [this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			++m_vector_reads;
			m_entry_fetch_cycle = m_cpu->total_cycles();
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
				"Entry Fetch follows SP Access and precedes the stack write");
		});
	m_vector_stack_tap = data.install_write_tap(0x0fff, 0x0fff,
		"entry_write_cycle", [this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			++m_entry_stack_writes;
			m_entry_stack_cycle = m_cpu->total_cycles();
			expect(value == 0x0800 && m_vector_reads == 1,
				"Entry Execute stacks the accepted continuation after one vector read");
		});
	m_phase = 180000 + index;
	if (index) m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
	else m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_interrupt_acceptance_checkpoint()
{
	m_cpu->reset();
	m_entry_ack_source = -1;
	m_cpu->space(AS_PROGRAM).write_word(0x40, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 3);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_phase = 190000;
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_entry_stage_restore_case(unsigned index)
{
	m_cpu->reset();
	m_vector_prefetch_tap.remove(); m_vector_stack_tap.remove();
	m_vector_reads = m_vector_acks = m_entry_stack_writes = 0;
	m_stage_restore_case = index;
	bool const nmi = index >= 5;
	u16 const vector = nmi ? 4 : 0x40;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(vector, 0xf020); // LD #1234,A, then IDLE.
	program.write_word(vector + 1, 0x1234);
	program.write_word(vector + 2, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, nmi ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 3);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_vector_prefetch_tap = program.install_read_tap(vector, vector, "entry_stage_restore_fetch",
		[this](offs_t, u16 &, u16) {
			if (!machine().side_effects_disabled()) ++m_vector_reads;
		});
	m_vector_stack_tap = m_cpu->space(AS_DATA).install_write_tap(0x0fff, 0x0fff,
		"entry_stage_restore_stack", [this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			++m_entry_stack_writes;
			expect(value == 0x0800 && m_vector_reads == 1 && m_vector_acks == 1,
				"Restored entry stacks the original continuation after one Fetch/IACK");
		});
	m_phase = 450000;
	if (nmi) m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
	m_check_timer->adjust(attotime::from_ticks(index % 5 + 1, m_cpu->clock()));
}

void tms320c54x_test_state::start_xc_irq_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	bool const two = index >= 2, accepted = !(index & 1);
	program.write_word(0x0800, two ? 0xff4d : 0xfd4d); // XC n,BEQ
	program.write_word(0x0801, 0xe801); // LD #1,A: conditionally executed.
	program.write_word(0x0802, 0xe809); // LD #9,A: outside XC 1, inside protection.
	program.write_word(0x0803, 0xf4e1); // IDLE1
	program.write_word(0x0048, 0x0083); // ISR reads *AR3 to observe entry A.
	program.write_word(0x0049, 0xf4eb); // RETE
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, accepted ? 0 : 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0061);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 4);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
	m_irq_accumulator = 0xdead;
	m_xc_irq_fetches = 0;
	m_xc_irq_tap.remove();
	m_xc_irq_tap = program.install_read_tap(0x0800, 0x0800, "xc_pending_irq",
		[this](offs_t, u16 &, u16) {
			if (!machine().side_effects_disabled() && !m_xc_irq_fetches++)
				m_cpu->set_input_line(2, ASSERT_LINE);
		});
	m_phase = 81000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_pipeline_far_irq(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	u32 const base = BIT(variant, 0) ? 0x10000 : 0;
	u32 const target = (base ^ 0x10000) + 0x700;
	bool const delayed = BIT(variant, 1);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) {
		program.write_word(base + 0x600 + i, 0xf495);
		program.write_word(target + i, 0xf495);
		program.write_word(0x44 + i, i ? 0xf495 : 0xe955);
		program.write_word(0x10044 + i, i ? 0xf495 : 0xe955);
	}
	program.write_word(base + 0x600, (delayed ? 0xf6e4 : 0xf4e4) | (BIT(variant, 2) ? 0 : 1));
	program.write_word(target, 0xe811);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
	m_cpu->space(AS_DATA).write_word(0x1000, target >> 16);
	m_cpu->space(AS_DATA).write_word(0x1001, 0x700);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	auto vector_read = [this, target](offs_t address, u16 &, u16) {
		if (machine().side_effects_disabled()) return;
		expect(address == (target & 0x10000) + 0x44, "Pending far-return IRQ fetches its vector in the restored page");
		++m_pipeline_operand_reads;
	};
	m_pipeline_operand_tap = program.install_read_tap(0x44, 0x44, "staged_far_irq_low", vector_read);
	m_pipeline_fetch_tap = program.install_read_tap(0x10044, 0x10044, "staged_far_irq_high", vector_read);
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x1001, 0x1001, "staged_far_irq_stack",
		[this, delayed](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == (delayed ? 0x702 : 0x700), "Pending far-return IRQ retains the first undecoded target continuation");
			++m_pipeline_store_writes;
		});
	m_phase = 251370; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::irq_pop_fixture tms320c54x_test_state::irq_pop_case(unsigned variant)
{
	return {u16(variant % 2),std::array<u16,4>{0,1,0x5a5a,0xffff}[(variant / 2) % 4],
		std::array<u16,4>{0,4,0xa5a5,0xffff}[(variant / 8) % 4],0x1357,
		(variant / 32) % 2 ? 0x10000U : 0U,4 + variant / 64};
}

void tms320c54x_test_state::start_interrupt_pop_edge(unsigned variant)
{
	unsigned const destination = variant % 2, context = (variant / 2) % 3;
	u32 const base = (variant / 6) % 2 ? 0x10000 : 0;
	start_pipeline_mmr_pop(4 + destination + (base ? 32 : 0));
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i=0;i<16;++i) program.write_word(base+0x600+i,0xf495);
	unsigned pc=base+0x600;
	program.write_word(pc++,context ? 0xf273 : 0xec01);
	if (context) program.write_word(pc++,0x680);
	if (context==2) program.write_word(pc++,0xf495);
	program.write_word(pc,0x8a00|destination);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	m_pipeline_operand_reads=m_pipeline_store_writes=0;
	m_pipeline_operand_tap=m_cpu->space(AS_DATA).install_read_tap(0,0xffff,"irq_pop_edge_reads",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap=m_cpu->space(AS_DATA).install_write_tap(0,0xffff,"irq_pop_edge_writes",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=62080;
	m_cpu->set_state_int(STATE_GENPC,base+0x600); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_xc_irq(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(0x600 + i, 0xf495); program.write_word(0x40 + i, 0xf495); }
	program.write_word(0x600, BIT(variant, 0) ? 0xff00 : 0xfd00);
	program.write_word(0x601, 0x8000); program.write_word(0x602, 0x8001);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa60e);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = m_pipeline_operand_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x700, 0x701, "xc_protected_stores",
		[this](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x1234 && m_cpu->pipeline_cycles() == 7 + address - 0x700, "Both XC-protected stores retire before interrupt entry");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = program.install_read_tap(0x40, 0x40, "xc_vector_fetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes == 2 && m_cpu->pipeline_cycles() > 8, "XC 1 and XC 2 protect two full words before vector Fetch");
			++m_pipeline_operand_reads;
		});
	m_phase = 251710; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_stack_irq_order(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const returning = !strcmp(machine().options().bios(), "stack-irq-return");
	unsigned const form = variant % 4;
	bool const nmi = (variant / 4) % 2;
	u32 const base = (variant / 8) % 2 ? 0x20000 : 0;
	u32 const vector = base + (nmi ? 4 : 0x40);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(vector + i, 0xf495); }
	program.write_word(base + 0x600, std::array<u16, 4>{0x4a08, 0x4b62, 0x8a0e, 0x8a18}[form]);
	program.write_word(vector, 0xe955);
	if (returning) { program.write_word(vector + 1, 0xf4eb); program.write_word(base + 0x601, form < 2 ? 0x8a0e : 0xf4e1); program.write_word(base + 0x602, 0xf4e1); }
	for (unsigned line = 0; line < 16; ++line) m_cpu->set_input_line(line, CLEAR_LINE);
	m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
	data.write_word(0x62, 0x1234); data.write_word(0x1000, form == 3 ? 0x1100 : 0x1234);
	data.write_word(0xffe, 0xdead); data.write_word(0xfff, 0xbeef); data.write_word(0x10ff, 0xdead);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_stack_irq_return_reads = m_stack_irq_cleanup_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "stack_irq_owned_read", [this, form, returning](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (returning && m_pipeline_operand_reads && at == (form < 2 ? 0xffe : form == 2 ? 0x1000 : 0x10ff)) { expect(word == 0x601 && !m_stack_irq_return_reads && m_vector_acks == 1, "RETE reads the hardware-owned return PC once"); ++m_stack_irq_return_reads; return; }
		if (returning && form < 2 && at == 0xfff) { expect(word == 0x1234 && m_stack_irq_return_reads == 1 && !m_stack_irq_cleanup_reads, "Returned caller pops its own preserved push word"); ++m_stack_irq_cleanup_reads; return; }
		if (at != (form == 0 ? 8 : form == 1 ? 0x62 : 0x1000) || word != (form == 3 ? 0x1100 : 0x1234) || m_pipeline_operand_reads || m_cpu->pipeline_cycles() != 5)
			osd_printf_info("Stack IRQ read observation: case=%u replay=%u address=%04x word=%04x clock=%llu reads=%u\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles(), m_pipeline_operand_reads);
		expect(at == (form == 0 ? 8 : form == 1 ? 0x62 : 0x1000) && word == (form == 3 ? 0x1100 : 0x1234) && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 5, "Foreground stack source is read before interrupt SP reservation"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "stack_irq_owned_write", [this, form, returning](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == (form < 2 ? 0xffe : form == 2 ? 0x1000 : 0x10ff)) { expect(word == 0x601 && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 7, "Interrupt return PC owns the slot after the older stack operation"); ++m_pipeline_store_writes; }
		else if (form < 2 && at == 0xfff) { expect(word == 0x1234 && !m_pipeline_register_writes && m_cpu->pipeline_cycles() == 6, "Older stack push retains its pre-interrupt destination"); ++m_pipeline_register_writes; }
		else if (form >= 2 && at == (form == 2 ? 0xe : 0x18)) { expect(word == (form == 2 ? 0x1234 : 0x1100) && !m_pipeline_extensions_reads, "Older POPM publishes its captured source once"); ++m_pipeline_extensions_reads; }
		else if (returning && form < 2 && at == 0xe) { expect(word == 0x1234 && !m_pipeline_extensions_reads && m_stack_irq_cleanup_reads == 1, "Caller cleanup publishes the original word after RETE"); ++m_pipeline_extensions_reads; }
		else expect(false, "Stack/interrupt composition must not write an unowned slot");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253980;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

int tms320c54x_test_state::rptbd_irq_line(unsigned v)
{ return ((v - 512) / 32) % 2 ? INPUT_LINE_NMI : 2; }

bool tms320c54x_test_state::rptbd_late_irq(unsigned v)
{ return ((v - 512) / 64) % 2; }

void tms320c54x_test_state::start_block_xc_irq(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const endpoint = !strcmp(machine().options().bios(), "block-xc-end-irq");
	if (endpoint) variant = variant % 4 + 24 * ((variant / 4) % 2) + 48 * ((variant / 8) % 2) + 96 * (variant / 16);
	unsigned const count = std::array<unsigned, 4>{3, 0, 1, 7}[variant % 4], gap = (variant / 8) % 3;
	bool const nmi = (variant / 24) % 2;
	u32 const base = (variant / 48) % 2 ? 0x20000 : 0;
	u16 const end = endpoint ? 0x603 : 0x604 + gap, vector = nmi ? 4 : 0x40;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf072); program.write_word(base + 0x601, end);
	program.write_word(base + 0x602, (variant / 4) % 2 ? 0xff00 : 0xfd00);
	program.write_word(base + 0x603, 0x8c93); program.write_word(base + 0x604, 0x8d94);
	program.write_word(base + end + 9, 0xf4e1); program.write_word(base + vector, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x5aa5);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5100); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x5200); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, count);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_memory_case = false;
	m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = m_vector_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_xc_irq_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Short XC loops and vector IDLE have no data reads"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_xc_irq_writes", [this, count, end, nmi, endpoint](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(!m_pipeline_extensions_reads++ && word > end && word <= end + 9, "Protected repeat stacks one post-loop continuation"); return; }
		if (at >= 0x5100 && at < 0x5200)
		{
			expect(m_pipeline_store_writes <= count && at == 0x5100 + m_pipeline_store_writes++ && word == 0xbeef, "Every first protected store executes once");
			if (!m_pipeline_memory_case) { m_pipeline_memory_case = true; m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE); }
			return;
		}
		expect(m_pipeline_register_writes < (endpoint ? 1U : count + 1) && at == 0x5200 + m_pipeline_register_writes++ && word == 0x5aa5, "Every second protected store executes once");
	});
	m_vector_prefetch_tap = program.install_read_tap(base + vector, base + vector, "block_xc_irq_vector", [this, count, nmi, endpoint](offs_t, u16 &, u16) {
		if (machine().side_effects_disabled()) return;
		if (m_pipeline_store_writes != count + 1 || m_pipeline_register_writes != (endpoint ? 1U : count + 1)) osd_printf_info("Short XC protection failure: variant=%u first=%u second=%u expected=%u cycle=%llu\n", m_pipeline_peripheral_case, m_pipeline_store_writes, m_pipeline_register_writes, count + 1, (unsigned long long)m_cpu->pipeline_cycles());
		expect(m_pipeline_memory_case && !m_vector_reads++ && m_pipeline_store_writes == count + 1 && m_pipeline_register_writes == (endpoint ? 1U : count + 1), "Pending IRQ/NMI waits for all overlapping short-loop XC spans");
		m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, CLEAR_LINE);
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255660;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_software_intr(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = variant & 64 ? 0x20000 : 0;
	unsigned const vector = variant % 32;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf7c0 | vector);
	program.write_word(base + 0x601, 0x890f);
	program.write_word(base + 0x602, 0xf4e1);
	program.write_word(base + 0x100 + vector * 4, 0x880e);
	program.write_word(base + 0x101 + vector * 4, 0xfc00);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x100);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, variant & 32 ? 0x900 : 0x100);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x1111);
	m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2222);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "software_intr_read", [this](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x2fff && word == 0x601 && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == 9, "Software INTR return reads exactly its own continuation at Read"); });
	// SPRU172C RET[D]: nondelayed RET takes five cycles, so the continuation executes at 15.
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "software_intr_write", [this](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes++; bool const valid = i < 3 && at == std::array<unsigned, 3>{0x2fff, 0x0e, 0x0f}[i] && word == std::array<u16, 3>{0x601, 0x789a, 0x9876}[i] && m_cpu->pipeline_cycles() == std::array<unsigned, 3>{6, 9, 15}[i]; if (!valid) osd_printf_info("software_intr_write: case=%u replay=%u index=%u address=%04x word=%04x cycle=%llu\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), i, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles()); expect(valid, "Software INTR publishes one frame, handler result and return result at their Execute clocks"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254400;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_vector_prefetch_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	m_vector_prefetch_tap.remove();
	m_vector_stack_tap.remove();
	m_vector_reads = m_vector_acks = 0;
	m_vector_case = index;
	u32 const address = index ? 4 : 0x40;
	program.write_word(address, 0xf020); // LD #1234,A: retain opcode, fetch extension normally.
	program.write_word(address + 1, 0x1234);
	program.write_word(address + 2, 0xf4e1);
	program.write_word(0x0800, 0xf4e1);
	data.write_word(0x0fff, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, index ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 3);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 80000;
	m_vector_prefetch_tap = program.install_read_tap(address, address, "vector_prefetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
				"SPRU131G INTR Access decrements SP before vector Fetch, but stack memory is not written yet");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (m_vector_case ? 3 : 2),
				"Vector acknowledgement clears only the accepted maskable source");
			++m_vector_reads;
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
		});
	m_vector_stack_tap = data.install_write_tap(0x0fff, 0x0fff, "vector_stack_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled() || m_phase != 80000) return;
			expect(m_vector_reads == 1 && m_vector_acks == 1 && value == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff,
				"SPRU131G INTR Execute writes RTN after vector Fetch without decrementing SP again");
		});
	if (index) { m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); }
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_intm_protection_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	program.write_word(0x0109d0, 0xf6bb); // RSBX ST1, INTM
	if (index == 0)
		program.write_word(0x0109d1, 0xe807); // One-word LD #7,A
	else if (index == 1)
	{
		program.write_word(0x0109d1, 0x76f8); // Three-word ST #7,*(0501)
		program.write_word(0x0109d2, 0x0501);
		program.write_word(0x0109d3, 7);
	}
	else
	{
		program.write_word(0x0109d1, 0xfc00); // RET must precede the pending interrupt.
		data.write_word(0x1000, 0x09e0);
	}
	program.write_word(0x010040, 0xf4e1); // Observe interrupt entry without returning.
	data.write_word(0x0501, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0109d0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 6164 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_software_interrupt_case(unsigned index, bool trap)
{
	unsigned const vector = index & 31;
	u16 const base = (index & 1) ? 0x4000 : 0x2000;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0109d0, 0x75d6); program.write_word(0x0109d1, 0x0124);
	program.write_word(0x0109d2, (trap ? 0xf4c0 : 0xf7c0) | vector);
	program.write_word(0x10000 | (base + vector * 4), 0x75d6);
	program.write_word(0x10000 | (base + vector * 4 + 1), 0x0124);
	program.write_word(0x10000 | (base + vector * 4 + 2), 0xf4e1);
	m_cpu->set_state_int(STATE_GENPC, 0x0109d0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, base);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (index & 32) ? 0x0980 : 0x0180);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x1100);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_port_writes = 0;
	m_phase = (trap ? 92000 : 2701) + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_poll_interrupt(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	u32 const base = (variant / 64) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); program.write_word(base + i, 0xf495); }
	std::array<u16, 9> const body{0x10f8, 0x1002, 0x0881, 0xfa45, 0x0602, 0x10f8, 0x1002, 0xe955, 0xf4e1};
	for (unsigned i = 0; i < body.size(); ++i) program.write_word(base + 0x600 + i, body[i]);
	for (unsigned vector : {4U, 0x40U}) { program.write_word(base + vector, 0xf273); program.write_word(base + vector + 1, 0x700); program.write_word(base + vector + 2, 0xf495); program.write_word(base + vector + 3, 0xf495); }
	program.write_word(base + 0x700, 0x80f8); program.write_word(base + 0x701, 0x1004);
	program.write_word(base + 0x702, 0x76f8); program.write_word(base + 0x703, 0x1002); program.write_word(base + 0x704, 2); program.write_word(base + 0x705, 0xf4eb);
	data.write_word(0x1002, 1); data.write_word(0x1003, 1); data.write_word(0x1004, 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1003);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	m_pipeline_operand_reads = m_pipeline_store_writes = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x1002, 0x1003, "poll_interrupt_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x1002, 0x1002, "poll_interrupt_release", [this](offs_t, u16 &word, u16) { if (!machine().side_effects_disabled()) { expect(word == 2 && !m_pipeline_store_writes, "Only the interrupt handler releases the polling loop once"); ++m_pipeline_store_writes; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 256100;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_ifr_pair(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const b = variant & 1;
	u16 const flags = std::array<u16, 4>{0, 8, 0x438, 0xffff}[(variant / 2) % 4] | (variant >= 48 && variant < 72 ? 0x1000 : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, b ? 0x4901 : 0x4801);
	program.write_word(0x601, b ? 0x8901 : 0x8801);
	program.write_word(4, 0xf4eb); program.write_word(0x70, 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, variant >= 48 ? 0 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, flags); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x23456789);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(12, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(1, 1, "ifr_pair_read",
		[this, flags](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == flags && !m_pipeline_operand_reads, "LDM reads pending IFR exactly once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(1, 1, "ifr_pair_ack",
		[this, flags](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == flags && !m_pipeline_store_writes, "STLM acknowledges the adjacent LDM result exactly once"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253730;
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_repeat_mvdp_irq(unsigned variant)
{
	unsigned const index = variant % 4, count_index = std::array<unsigned, 4>{0, 1, 3, 4}[index];
	unsigned const count = std::array<unsigned, 4>{0, 1, 7, 255}[index];
	bool const nmi = (variant / 16) % 2, long_count = (variant / 8) % 2;
	u32 const base = (variant / 4) % 2 ? 0x20000 : 0;
	start_repeat_mvdp(13 + count_index * 40 + (base ? 200 : 0) + (long_count ? 400 : 0));
	m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove();
	// Interrupt return legitimately reads the stack; isolate copy-source
	// observations from those independent data transactions.
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x1230, 0x132f, "repeat_mvdp_irq_source",
		[this, count](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads <= count && at == 0x1230 + m_pipeline_operand_reads && word == 0x2468 + m_pipeline_operand_reads, "Interrupted repeated MVDP reads each source once");
			++m_pipeline_operand_reads;
		});
	{ auto const disable = machine().disable_side_effects();
		u32 const vector = nmi ? 4 : base + 0x40;
		auto &program = m_cpu->space(AS_PROGRAM);
		program.write_word(vector, 0x4a1e); // PSHM RTN
		program.write_word(vector + 1, 0xe955); // LD #55,B
		program.write_word(vector + 2, 0xf4e5); // RETE
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, nmi ? 0x800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, nmi ? 0 : 1);
	m_vector_acks = 0; m_phase = 259960;
}

void tms320c54x_test_state::start_ldu_imr(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const form = (variant / 16) % 4, read = form == 3 ? 7 : form == 1 ? 6 : 5;
	bool const sxm = variant & 4, b = variant & 8;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[variant % 4];
	u32 const base = variant & 64 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	unsigned const prefix = form == 3 ? 2 : 0;
	if (prefix) { program.write_word(base + 0x600, 0x7182); program.write_word(base + 0x601, 0x12); }
	program.write_word(base + 0x600 + prefix, 0x1200 | (b ? 0x100 : 0) | (form == 1 ? 0xf8 : form >= 2 ? 0x82 : 0));
	if (form == 1) program.write_word(base + 0x601, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, sxm ? 0x900 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, value); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcdef1234ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, prefix ? 0x1400 : 0);
	data.write_word(0x1400, 0);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "ldu_imr_read",
		[this, value, form, read](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0) { expect(!m_pipeline_operand_reads && word == value && m_cpu->pipeline_cycles() == read, "LDU captures the actual IMR value once at its Read clock"); ++m_pipeline_operand_reads; } else { expect(form == 3 && at == 0x1400 && word == 0 && !m_pipeline_register_writes && m_cpu->pipeline_cycles() == 5, "MVDK captures the zero next pointer once from RAM"); ++m_pipeline_register_writes; } });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "ldu_imr_pointer",
		[this, form](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(form == 3 && at == 0x12 && word == 0 && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 5, "MVDK publishes the captured pointer before adjacent LDU address generation"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "ldu_imr_extension",
		[this, form](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || (form != 1 && form != 3)) return; expect(!m_pipeline_extensions_reads && word == (form == 3 ? 0x12 : 0), "Mapped unsigned load composition retains its one extension once"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253910;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mvmd_status_irq(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const stlm = !strcmp(machine().options().bios(), "stlm-st1-irq");
	bool const two_word = variant >= 288;
	bool const masked = variant & 2, enabled = variant & 4, nmi = variant & 8;
	u32 const base = variant & 16 ? 0x20000 : 0, vector = base + (nmi ? 4 : 0x40);
	u16 const status = masked ? 0x800 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	// A masked pending source may wake IDLE without being serviced. Keep
	// the bounded continuation valid rather than running into erased words.
	for (unsigned i = 0; i < 64; ++i) program.write_word(base + 0x600 + i, i >= 4 ? 0xf4e1 : 0xf495);
	program.write_word(base + 0x600, 0x730b); program.write_word(base + 0x601, 7);
	if (stlm) { program.write_word(base + 0x600, 0x8907); program.write_word(base + 0x601, 0xf495); }
	program.write_word(base + 0x602, two_word ? 0x770e : 0x880e); program.write_word(base + 0x603, two_word ? 0x55 : 0xf495);
	program.write_word(base + 0x604, 0xf4e1); program.write_word(base + 0x620, 0xf4e1);
	program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x2000); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, variant & 1 ? 0x800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, enabled ? 1 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x55); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234560000ULL | status); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_vector_acks = m_fast_stack_reads = m_pipeline_extensions_reads = 0; m_timer_probe_return_pc = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvmd_status_irq_reads",
		[this, status](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0xb) { expect(!m_pipeline_operand_reads && word == status && m_cpu->pipeline_cycles() == 5, "Status source read survives pending IRQ/NMI drain"); ++m_pipeline_operand_reads; } else { expect(at == 0xfff && !m_fast_stack_reads && m_vector_acks == 1 && word == m_timer_probe_return_pc, "RETE pops only the owned interrupt continuation"); ++m_fast_stack_reads; } });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvmd_status_irq_writes",
		[this, status](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 7) { expect(!m_pipeline_store_writes && word == status && m_cpu->pipeline_cycles() == 6, "Full status publishes once despite pending interrupts"); ++m_pipeline_store_writes; } else if (at == 0xe) { expect(!m_pipeline_register_writes && word == 0x55, "Foreground marker executes exactly once across interrupt return"); ++m_pipeline_register_writes; } else { expect(at == 0xfff && !m_pipeline_extensions_reads && m_vector_acks == 1 && word == m_timer_probe_return_pc, "IRQ/NMI stacks one owned continuation"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253890;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u16 tms320c54x_test_state::repeat_irq_r(offs_t offset)
{
	if (m_phase == 6517 || m_phase == 6518 || m_phase == 251210 || m_phase == 62162 || m_phase == 62163)
		return m_stack_mmr[offset];
	if (m_phase == 784)
	{
		// Abort this timeslice after the current instruction retires. Saving
		// inside the operand callback would capture a partial instruction.
		m_phase = 785;
		m_check_timer->adjust(attotime::zero);
		return 1;
	}
	if (offset)
	{
		m_irq_operand_cycle = m_cpu->total_cycles();
		m_irq_accumulator = m_cpu->state_int(tms320c54x_device::STATE_A);
		m_port_writes_at_irq = m_port_writes;
		return 0;
	}
	m_last_operand_cycle = m_cpu->total_cycles();
	if (++m_repeat_reads == 1)
	{
		m_first_operand_cycle = m_last_operand_cycle;
	}
	if (m_repeat_reads == m_irq_trigger_read)
		m_cpu->set_input_line(2, ASSERT_LINE);
	return 1;
}

bool tms320c54x_test_state::check_software_interrupt_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 61000 && m_phase < 61064)
	{
		unsigned const index = m_phase - 61000;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(STATE_GENPC) == 0x0109d4 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0180 &&
			data.read_word(0x0fff) == 0xdead,
			"SPRU131G software INTR publishes RTN for RETF independently of the stack word");
		if (index < 63) { start_software_interrupt_case(index + 1); return true; }
		osd_printf_info("TMS320C54x software interrupt conformance: PASS variants=64\n");
		osd_printf_info("TMS320C54x software interrupt fast return: PASS variants=64\n");
		start_software_interrupt_case(0, true);
		return true;
	}
	if (m_phase >= 92000 && m_phase < 92064)
	{
		unsigned const index = m_phase - 92000;
		unsigned const vector = index & 31;
		u16 const base = (index & 1) ? 0x4000 : 0x2000;
		expect_opcode(0xf4c0 | vector,
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(STATE_GENPC) == (0x10000 | (base + vector * 4 + 3)) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			data.read_word(0x0fff) == 0x09d3 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((index & 32) ? 0x0980 : 0x0180) &&
			m_cpu->state_int(tms320c54x_device::STATE_IMR) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0xffff &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"TRAP selects IPTR:K, pushes next PC, preserves INTM/IFR and costs three cycles");
		if (index < 63) { start_software_interrupt_case(index + 1, true); return true; }
		osd_printf_info("TMS320C54x TRAP conformance: PASS variants=64\n");
		start_trn_store_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_intm_protection_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 6164 && m_phase <= 6166)
	{
		unsigned const index = m_phase - 6164;
		u16 const expected_sp = index == 2 ? 0x1000 : 0x0fff;
		u16 const expected_pc = index == 0 ? 0x09d2 : index == 1 ? 0x09d4 : 0x09e0;
		expect(m_cpu->state_int(STATE_GENPC) == 0x010041 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == expected_sp &&
			data.read_word(expected_sp) == expected_pc &&
			(index != 0 || m_cpu->state_int(tms320c54x_device::STATE_A) == 7) &&
			(index != 1 || data.read_word(0x0501) == 7),
			"SPRU131G INTM change protects the complete following instruction, including RET");
		if (index < 2) { start_intm_protection_case(index + 1); return true; }
		osd_printf_info("TMS320C54x INTM protection: PASS one_word=1 three_words=1 return=1\n");
		start_idle_wake_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_vector_prefetch_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 79700 && m_phase <= 79705)
	{
		if (m_phase == 79700)
		{
			expect(data.read_word(0x58) == 0x77ff, "PLL locks before software-managed idle transition");
			data.write_word(0x58, 0x77fc);
			m_phase = 79701; m_check_timer->adjust(attotime::from_usec(1)); return true;
		}
		if (m_phase == 79701)
		{
			expect(data.read_word(0x58) == 0x77fc, "DIV source with PLL power held before IDLE3");
			auto &program = m_cpu->space(AS_PROGRAM);
			program.write_word(0x0b80, 0xf5e1); program.write_word(0x0b81, 0xf4e1);
			m_cpu->set_state_int(STATE_GENPC, 0x0b80);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->resume(SUSPEND_REASON_DISABLE);
			m_phase = 79702; m_check_timer->adjust(attotime::from_usec(10)); return true;
		}
		if (m_phase == 79702)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == 0x0b81,
				"original IDLE3 instruction stops before continuation");
			m_cpu->set_input_line(0, ASSERT_LINE);
			m_phase = 79703; m_check_timer->adjust(attotime::from_usec(10)); return true;
		}
		if (m_phase == 79703)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == 0x0b82,
				"enabled IRQ wakes IDLE3 to masked-interrupt continuation in DIV mode");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			m_cpu->set_input_line(0, CLEAR_LINE);
			data.write_word(0x58, 0x77ff);
			m_phase = 79704; m_check_timer->adjust(attotime::from_usec(1)); return true;
		}
		if (m_phase == 79704)
		{
			expect(data.read_word(0x58) == 0x77fe && m_cpu->clock() == 5'997'600,
				"IDLE3 invalidates retained PLL lock: software re-entry cannot skip the lock count");
			m_phase = 79705; m_check_timer->adjust(attotime::from_usec(340)); return true;
		}
		expect(data.read_word(0x58) == 0x77ff && m_cpu->clock() == 95'961'600,
			"software-managed PLL re-entry completes its new lock delay after IDLE3");
		m_cpu->set_follow_clkmd(false); m_cpu->set_pll_reference_clock(0);
		m_cpu->set_unscaled_clock(13'000'000);
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		osd_printf_info("TMS320C54x IDLE3 PLL relock: PASS div_wake=1 power_held=1 lock_invalidated=1 software_reentry=1 automatic_pll_wake_claim=0\n");
		start_vector_prefetch_case(0); return true;
	}
	if (m_phase >= 80000 && m_phase <= 80003)
	{
		u32 const address = m_vector_case ? 4 : 0x40;
		if (m_phase == 80003)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(STATE_GENPC) == 0x0801,
				"Reset discards the retained hardware vector opcode");
			m_vector_prefetch_tap.remove();
			if (m_vector_case == 2) { start_vector_prefetch_case(3); return true; }
			m_vector_stack_tap.remove();
			osd_printf_info("TMS320C54x vector prefetch: PASS sources=irq,nmi before_stack=1 sp_access=1 retained_opcode=1 extension_fetch=1 save_replay=2 single_ack=1 later_mask=1 reset=1 debugger_redirect=1 physical_latency_claim=0\n");
			start_xc_irq_case(0); return true;
		}
		if (m_phase == 80000)
		{
			expect(m_vector_reads == 1 && m_vector_acks == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff && data.read_word(0x0fff) == 0xbeef,
				"Suspension after vector Fetch leaves the later continuation write pending");
			expect(machine().scheduler().can_save(), "Vector prefetch checkpoint has no synchronized inputs");
			if (m_vector_case >= 2)
			{
				if (m_vector_case == 2) { m_cpu->reset(); m_cpu->set_state_int(STATE_GENPC, 0x0800); }
				else m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
				m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80003;
				m_check_timer->adjust(attotime::from_usec(100)); return true;
			}
			m_saved_vector.str(std::string()); m_saved_vector.clear();
			expect(machine().save().write_stream(m_saved_vector) == STATERR_NONE, "Save retained vector opcode");
			program.write_word(address, 0xf4e1); // Must not replace the already fetched LD.
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); // A later mask cannot revoke accepted entry.
			m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80001;
		}
		else
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				data.read_word(0x0fff) == 0x0800 &&
				m_vector_reads == 1 && m_vector_acks == 1,
				"Retained vector opcode executes once with its normally fetched extension");
			if (m_phase == 80001)
			{
				m_saved_vector.clear(); m_saved_vector.seekg(0);
				expect(machine().save().read_stream(m_saved_vector) == STATERR_NONE, "Restore retained vector opcode");
				program.write_word(address, 0xf4e1);
				m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
				m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80002;
			}
			else
			{
				m_vector_prefetch_tap.remove();
				start_vector_prefetch_case(m_vector_case + 1); return true;
			}
		}
		m_check_timer->adjust(attotime::from_usec(100)); return true;
	}
	if (m_phase >= 81000 && m_phase < 81004)
	{
		unsigned const index = m_phase - 81000;
		osd_printf_info("C54x XC IRQ case=%u entry_a=%llx continuation=%04x\n", index,
			(unsigned long long)m_irq_accumulator, data.read_word(0x0fff));
		expect(m_xc_irq_fetches == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC pending-IRQ fixture terminates normally");
		expect(m_irq_accumulator == (index == 3 ? 0 : 9) && data.read_word(0x0fff) == 0x0803,
			"XC protects two following words independently of conditional span and acceptance");
		m_cpu->set_input_line(2, CLEAR_LINE);
		m_xc_irq_tap.remove();
		if (index < 3) { start_xc_irq_case(index + 1); return true; }
		osd_printf_info("TMS320C54x XC interrupt protection: PASS cases=4 spans=1,2 accepted,rejected=1 physical_pipeline_claim=0\n");
		start_braf_control_case(0); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_interrupt_entry_spacing_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 180000 && m_phase < 180002)
	{
		expect(m_vector_reads == 1 && m_entry_stack_writes == 1 &&
			m_entry_stack_cycle == m_entry_fetch_cycle + 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"IRQ/NMI entry stack write is one CPU cycle after vector Fetch");
		m_vector_prefetch_tap.remove();
		m_vector_stack_tap.remove();
		if (m_phase == 180000) { start_interrupt_entry_spacing_case(1); return true; }
		osd_printf_info("TMS320C54x interrupt entry stages: PASS sources=irq,nmi fetch_to_stack_cycles=1 pipeline_drain_claim=0\n");
		start_interrupt_acceptance_checkpoint(); return true;
	}
	if (m_phase >= 190000 && m_phase <= 190003)
	{
		if (m_phase == 190000)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 3 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
				m_entry_ack_source == -1 && data.read_word(0x0fff) == 0xbeef,
				"Accepted maskable interrupt retains IFR and INTM before IACK, with no early SP/stack write");
			m_phase = 190003;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (m_phase == 190003)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 3 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
				m_entry_ack_source == -1 && data.read_word(0x0fff) == 0xbeef,
				"INTR Decode captures continuation before Access decrements SP");
			m_saved_vector.str(std::string()); m_saved_vector.clear();
			expect(machine().save().write_stream(m_saved_vector) == STATERR_NONE,
				"Save accepted interrupt before Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
			m_phase = 190001;
			m_check_timer->adjust(attotime::from_usec(100)); return true;
		}
		expect(m_entry_ack_source == 0 && m_cpu->state_int(tms320c54x_device::STATE_IFR) == 2 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			data.read_word(0x0fff) == 0x0800 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"Later IMR changes do not reselect the accepted source; vector Fetch clears only its IFR bit");
		if (m_phase == 190001)
		{
			m_saved_vector.clear(); m_saved_vector.seekg(0);
			expect(machine().save().read_stream(m_saved_vector) == STATERR_NONE,
				"Restore accepted interrupt before Access");
			expect(!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 3,
				"Restored accepted maskable interrupt retains pre-IACK INTM and IFR");
			m_entry_ack_source = -1;
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
			m_phase = 190002;
			m_check_timer->adjust(attotime::from_usec(100)); return true;
		}
		osd_printf_info("TMS320C54x accepted interrupt checkpoint: PASS ifr_until_fetch=1 intm_at_iack=1 decode_before_access=1 later_imr=1 source_latched=1 save_replay=1 pipeline_drain_claim=0\n");
		start_entry_stage_restore_case(0); return true;
	}
	if (m_phase >= 450000 && m_phase <= 450002)
	{
		unsigned const stage = m_stage_restore_case % 5;
		bool const nmi = m_stage_restore_case >= 5;
		u16 const vector = nmi ? 4 : 0x40;
		if (m_phase == 450000)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (stage >= 2 ? 0x0fff : 0x1000) &&
				data.read_word(0x0fff) == (stage >= 4 ? 0x0800 : 0xbeef) &&
				m_vector_reads == unsigned(stage >= 3) && m_vector_acks == unsigned(stage >= 3) &&
				m_entry_stack_writes == unsigned(stage >= 4) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (nmi || stage < 3 ? 3 : 2),
				"Checkpoint separates acceptance, Decode, Access, Fetch/IACK and stack Execute");
			m_saved_vector.str(std::string()); m_saved_vector.clear();
			expect(machine().save().write_stream(m_saved_vector) == STATERR_NONE,
				"Save each IRQ/NMI entry boundary");
		}
		else
		{
			expect(m_vector_reads == 1 && m_vector_acks == 1 && m_entry_stack_writes == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				data.read_word(0x0fff) == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == vector + 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (nmi ? 3 : 2) &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"Reference and restored entry finish once with retained opcode and original source");
			if (m_phase == 450002)
			{
				m_vector_prefetch_tap.remove(); m_vector_stack_tap.remove();
				if (m_stage_restore_case < 9) { start_entry_stage_restore_case(m_stage_restore_case + 1); return true; }
				osd_printf_info("TMS320C54x entry stage restoration: PASS cases=10 sources=irq,nmi boundaries=5 one_fetch=1 one_ack=1 one_stack_write=1 retained_opcode=1 pipeline_drain_claim=0\n");
				start_rounding_boundary_case(0); return true;
			}
			m_saved_vector.clear(); m_saved_vector.seekg(0);
			expect(machine().save().read_stream(m_saved_vector) == STATERR_NONE,
				"Restore each IRQ/NMI entry boundary");
			m_vector_reads = m_vector_acks = unsigned(stage >= 3);
			m_entry_stack_writes = unsigned(stage >= 4);
		}
		// Accepted sources survive later masking. After Fetch, changing memory
		// must not replace the already retained first vector opcode.
		m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
		if (stage >= 3) m_cpu->space(AS_PROGRAM).write_word(vector, 0xf4e1);
		m_phase = m_phase == 450000 ? 450001 : 450002;
		m_check_timer->adjust(attotime::from_usec(100)); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_far_irq_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251370)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const delayed = BIT(m_pipeline_peripheral_case, 1);
		bool const masked = BIT(m_pipeline_peripheral_case, 2);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Pending cross-page far IRQ advances without unsupported forms");
		if (cycle == 4) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 2);
		if (cycle == 5 && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save pending IRQ with retained cross-page far return");
		}
		if (cycle == 20)
		{
			expect(m_pipeline_operand_reads == unsigned(!masked) && m_pipeline_store_writes == unsigned(!masked) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == (masked ? 0x1002 : 0x1001) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (masked || delayed ? 0x11 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (masked ? 0 : 0x55) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (masked ? 2 : 0),
				"Pending far return drains decoded target work, enters once and preserves the continuation stack");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore pending cross-page far-return IRQ");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = 6; m_phase = 251380;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 7) { start_pipeline_far_irq(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged cross-page far IRQ: PASS cases=8 masked=4 restored_vector_page=1 continuations=0700,0702 restore=8 duplicate_vectors=0 silicon_phase_claim=0\n");
			start_pipeline_intm(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251380)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251370;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_interrupt_pop_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62080)
	{
		unsigned const variant=m_pipeline_peripheral_case,tick=++m_idle_fixture_tick,checkpoint=2+variant/12;
		if (tick==checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline)==STATERR_NONE,"Save excluded interrupt POPM before stack effects");
		}
		if (tick==12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_operand_reads && !m_pipeline_store_writes &&
				m_cpu->state_int(tms320c54x_device::STATE_SP)==0x1000 && m_cpu->state_int(tms320c54x_device::STATE_IMR)==0x1357 &&
				!m_cpu->state_int(tms320c54x_device::STATE_IFR),"Excluded interrupt POPM has no stack, mask or acknowledgement effects");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay=true; m_idle_fixture_tick=checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline)==STATERR_NONE,"Restore excluded interrupt POPM");
				m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_phase=62081; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant<23) { start_interrupt_pop_edge(variant+1); return true; }
			osd_printf_info("TMS320C54x interrupt POPM exclusions: PASS cases=24 restore=24 destinations=IMR,IFR contexts=RPT,BD_first,BD_last pages=2 snapshots=2 reads=0 writes=0 stack_changes=0 mask_changes=0 acknowledgements=0 hardware_trap_claim=0\n");
			throw emu_fatalerror(0,"TMS320C54x isolated interrupt POPM exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase==62081) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=62080; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_pipeline_xc_irq_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251710)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		u64 const checkpoint = 5 + 3 * (m_pipeline_peripheral_case / 2);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC interrupt fixture advances without unsupported instructions");
		if (cycle == 3) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
		if (cycle <= 8) expect(m_pipeline_operand_reads == 0, "Pending interrupt cannot fetch its vector inside XC word protection");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save pending interrupt in XC protection window");
		}
		if (cycle == 24)
		{
			expect(m_pipeline_store_writes == 2 && m_pipeline_operand_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x4ff,
				"XC releases pending IRQ after both stores with one vector fetch and one interrupt stack allocation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = checkpoint >= 8 ? 2 : 0; m_pipeline_operand_reads = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore XC interrupt guard and pending source");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251720;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 3) { start_pipeline_xc_irq(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged unconditional XC IRQ protection: PASS cases=4 forms=XC1,XC2 restore=4 protected_words=2 vector_fetches=1\n");
			start_pipeline_xc_arithmetic(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251720)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251710;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_stack_irq_order_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253980)
	{
		bool const returning = !strcmp(machine().options().bios(), "stack-irq-return");
		unsigned const variant = m_pipeline_peripheral_case, form = variant % 4, tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = returning ? std::array<unsigned, 5>{4, 5, 6, 10, 14}[variant / 16] : std::array<unsigned, 3>{4, 5, 6}[variant / 16];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Stack/interrupt composition stays staged");
		if (tick == 2) { if ((variant / 4) % 2) m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); else m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1); }
		if (tick == 5) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (form < 2 ? 0xffe : form == 2 ? 0x1000 : 0x10ff), "Interrupt Access reserves SP after the older stack update in the same clock");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_pipeline_extensions_reads, m_vector_acks}; m_stack_irq_read_snapshot = {m_stack_irq_return_reads, m_stack_irq_cleanup_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save foreground stack and interrupt reservation boundary"); }
		if (tick == 20)
		{
			expect(m_vector_acks == 1 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == unsigned(form < 2) && m_pipeline_extensions_reads == unsigned(returning || form >= 2) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == (returning ? form < 2 ? 0x1000 : form == 2 ? 0x1001 : 0x1100 : form < 2 ? 0xffe : form == 2 ? 0x1000 : 0x10ff) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x601,
				"Interrupt source and older stack operation preserve both owned effects");
			if (returning) expect(m_stack_irq_return_reads == 1 && m_stack_irq_cleanup_reads == unsigned(form < 2) && m_cpu->state_int(tms320c54x_device::STATE_T) == (form < 3 ? 0x1234 : 0) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && !(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x800), "RETE resumes the caller and restores its pre-interrupt stack lifecycle");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[3]; m_vector_acks = m_recurring_irq_snapshot[4]; m_stack_irq_return_reads = m_stack_irq_read_snapshot[0]; m_stack_irq_cleanup_reads = m_stack_irq_read_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore stack/interrupt ordering without duplicate bus effects"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253990; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (returning ? 79 : 47)) { start_stack_irq_order(variant + 1); return true; }
			if (returning) { osd_printf_info("TMS320C54x staged stack IRQ return: PASS cases=80 restore=80 forms=PSHM,PSHD,POPM_T,POPM_SP sources=IRQ,NMI pages=2 snapshots=5 acknowledgements=1 return_reads=1 caller_cleanup=push_forms resumed_idle=1 intm_cleared=1 silicon_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated stack IRQ return complete"); }
			osd_printf_info("TMS320C54x staged stack IRQ ordering: PASS cases=48 restore=48 forms=PSHM,PSHD,POPM_T,POPM_SP sources=IRQ,NMI pages=2 snapshots=3 stack_read=5 interrupt_reserve=5 foreground_write=6 interrupt_write=7 acknowledgements=1 older_first=1 isr_return_claim=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated stack IRQ ordering complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253990) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253980; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_ldu_imr_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253910)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, form = (variant / 16) % 4, read = form == 3 ? 7 : form == 1 ? 6 : 5, checkpoint = std::array<unsigned, 3>{3, read - 1, read}[variant / 128];
		bool const sxm = variant & 4, b = variant & 8; u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[variant % 4];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped IMR unsigned load remains staged");
		if (tick == 4 && (form == 1 || form == 3)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word((variant & 64 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (tick == read - 1 && form >= 2) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0, "Adjacent LDU consumes the published zero pointer before later changes"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2222); }
		if (tick == read) m_cpu->set_state_int(tms320c54x_device::STATE_IMR, value ^ 0xffff);
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_pipeline_register_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save mapped load around pointer and IMR sampling"); }
		if (tick == 18)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == unsigned(form == 3) && m_pipeline_register_writes == unsigned(form == 3) && m_pipeline_extensions_reads == unsigned(form == 1 || form == 3) && m_cpu->state_int(tms320c54x_device::STATE_A) == (b ? 0xabcdef1234ULL : value) && m_cpu->state_int(tms320c54x_device::STATE_B) == (b ? value : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_IMR) == (value ^ 0xffff) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (sxm ? 0x900 : 0x800) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (form >= 2 ? 0x2222 : 0), "LDU retains actual IMR without sign extension, resampling or extra pointer writes");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_pipeline_register_writes = m_recurring_irq_snapshot[3]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore mapped load without rereading changed IMR or pointer"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253920; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_ldu_imr(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged LDU IMR: PASS cases=384 restore=384 forms=direct,absolute,indirect,MVDK values=4 sxm_states=2 accumulators=2 pages=2 snapshots=3 imr_reads=1 producer_reads=1 producer_writes=1 source_retained=1 interrupt_composition=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated LDU IMR complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253920) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253910; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 253890)
	{
		bool const stlm = !strcmp(machine().options().bios(), "stlm-st1-irq");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, arrival = std::array<unsigned, 3>{4, 6, 8}[(variant / 32) % 3], checkpoint = std::array<unsigned, 3>{5, 7, 12}[(variant % 288) / 96];
		bool const old_masked = variant & 1, masked = variant & 2, enabled = variant & 4, nmi = variant & 8;
		// The two-word follower has an incomplete Decode at clock five;
		// the one-word follower permits selection before clock-six INTM.
		bool const effective_masked = old_masked && masked;
		bool const accepted = nmi || (enabled && !effective_masked);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Status/IRQ composition remains staged");
		if (tick == arrival) m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		if (tick == arrival + 1) m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, CLEAR_LINE);
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_acks, m_fast_stack_reads, m_pipeline_extensions_reads, m_timer_probe_return_pc}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save status mask change and pending/accepted interrupt"); }
		if (tick == 96)
		{
			if (m_vector_acks != unsigned(accepted) || m_pipeline_register_writes != 1 || m_fast_stack_reads != unsigned(accepted) || m_pipeline_extensions_reads != unsigned(accepted))
				osd_printf_info("MVMD status IRQ mismatch: case=%u replay=%u accepted=%u source=%u status=%u marker=%u ack=%u stack_read=%u stack_write=%u pc=%06x sp=%04x st1=%04x ifr=%04x t=%04x return=%04x\n", variant, unsigned(m_pipeline_replay), unsigned(accepted), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_acks, m_fast_stack_reads, m_pipeline_extensions_reads, unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_T)), m_timer_probe_return_pc);
			expect(m_pipeline_operand_reads == unsigned(!stlm) && m_pipeline_store_writes == 1 && m_pipeline_register_writes == 1 && m_vector_acks == unsigned(accepted) && m_fast_stack_reads == unsigned(accepted) && m_pipeline_extensions_reads == unsigned(accepted) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_B) == (0x1234560000ULL | (masked ? 0x800 : 0)), "Full status mask transitions preserve pending selection, one frame and the foreground continuation");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == (effective_masked && !accepted ? 0x800 : 0) && (m_cpu->state_int(tms320c54x_device::STATE_IFR) & 1) == unsigned(!nmi && !accepted), "Memory writes cannot set INTM; masked IRQ remains pending and RETE clears INTM after accepted service");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3]; m_fast_stack_reads = m_recurring_irq_snapshot[4]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[5]; m_timer_probe_return_pc = m_recurring_irq_snapshot[6]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore mask publication and IRQ/NMI without duplicate service"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253900; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 575) { start_mvmd_status_irq(variant + 1); return true; }
			if (stlm)
			{
				osd_printf_info("TMS320C54x staged STLM ST1 IRQ: PASS cases=576 restore=576 sources=IRQ,NMI accumulator=B gap_nops=1 intm_requests=4 memory_cannot_set_intm=1 clear_policy=legacy_unverified imr_states=2 arrivals=4,6,8 pages=2 snapshots=3 followers=STLM,STM source_reads=0 status_writes=1 accepted_frame=1 masked_frame=0 foreground_once=1 rsbx_guard_inherited=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated STLM ST1 IRQ complete");
			}
			osd_printf_info("TMS320C54x staged MVMD status IRQ: PASS cases=576 restore=576 sources=IRQ,NMI intm_requests=4 memory_cannot_set_intm=1 clear_policy=legacy_unverified imr_states=2 arrivals=4,6,8 pages=2 snapshots=3 followers=STLM,STM source_reads=1 status_writes=1 accepted_frame=1 masked_frame=0 foreground_once=1 rsbx_guard_inherited=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated MVMD status IRQ complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253900) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253890; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_repeat_mvdp_irq_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 259960)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const count = std::array<unsigned, 4>{0, 1, 7, 255}[variant % 4];
		unsigned const arrival = std::array<unsigned, 3>{3, 6, count >= 7 ? 12U : 8U}[(variant / 32) % 3];
		// set_input_line queues a scheduler synchronization. Snapshot after
		// that edge has reached the CPU, not inside its enqueue callback.
		unsigned const checkpoint = std::array<unsigned, 3>{4, 9, 14}[variant / 96];
		bool const nmi = (variant / 16) % 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Interrupted repeated MVDP remains admitted");
		if (tick == arrival)
		{
			int const line = nmi ? INPUT_LINE_NMI : 0;
			m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_pipeline_store_writes <= count) expect(!m_vector_acks, "Repeated MVDP does not acknowledge during an unfinished copy");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, unsigned(m_first_port_cycle), m_vector_acks};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save repeated MVDP with pending IRQ or NMI");
		}
		if (tick == count + 100)
		{
			if (m_pipeline_operand_reads != count + 1 || m_pipeline_store_writes != count + 1 || m_vector_acks != 1 || !m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_AR2) != 0x1230 + count + 1 || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || m_cpu->state_int(tms320c54x_device::STATE_B) != 0x55)
				osd_printf_info("MVDP interrupt mismatch: case=%u replay=%u reads=%u writes=%u acks=%u idle=%u AR2=%04x SP=%04x B=%llx PC=%06x IFR=%04x\n", variant, unsigned(m_pipeline_replay), m_pipeline_operand_reads, m_pipeline_store_writes, m_vector_acks, unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
			expect(m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 && m_vector_acks == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1230 + count + 1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55, "Repeated MVDP retires completely before one balanced ISR and continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_first_port_cycle = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore repeated MVDP and its original pending interrupt");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 259970; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 287) { start_repeat_mvdp_irq(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged repeated MVDP interrupts: PASS cases=288 restore=288 sources=IRQ,NMI counts=0,1,7,255 count_forms=2 pages=2 arrivals=3 snapshots=3 deferred_until_final_write=1 balanced_stack=1 duplicate_transfers=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeated MVDP interrupts complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 259970) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 259960; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_ifr_pair_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253730)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = std::array<unsigned, 3>{4, 6, 8}[(variant % 24) / 8];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "IFR pair remains admitted");
		if (tick == 5 && variant >= 24 && variant < 48)
		{
			expect(m_pipeline_operand_reads == 1 && !m_pipeline_store_writes, "Late IRQ arrives strictly between IFR Read and acknowledgement");
			m_cpu->set_input_line(12, ASSERT_LINE); m_cpu->set_input_line(12, CLEAR_LINE);
		}
		if (tick == 5 && variant >= 48)
		{
			expect(m_pipeline_operand_reads == 1 && !m_pipeline_store_writes, "IRQ/NMI selection composes with an in-flight IFR acknowledgement");
			if (variant < 72) m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x1000);
			else { m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); }
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_vector_acks};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save IFR pair around Read and acknowledgement");
		}
		if (tick == (variant >= 48 ? 40 : 12))
		{
			u16 const flags = std::array<u16, 4>{0, 8, 0x438, 0xffff}[(variant / 2) % 4];
			u16 const remaining = variant >= 24 && variant < 48 ? (0x1000 & ~flags) : 0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == remaining && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1, "Read-back/write-back clears sampled IFR bits and retains a newly pending IRQ");
			expect(m_cpu->state_int((variant & 1) ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == ((variant & 1) ? 0x12345678 : 0x23456789), "IFR acknowledgement preserves the unselected accumulator");
			expect(m_vector_acks == unsigned(variant >= 48) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000, "Accepted IRQ/NMI completes exactly one entry and RETE restores the stack");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_vector_acks = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay IFR acknowledgement ownership"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253740; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 95) { start_ifr_pair(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged IFR pair: PASS cases=96 restore=96 accumulators=2 patterns=4 pending_only=1 late_irq=1 accepted_irq=1 nmi=1 interrupt_return=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated IFR pair complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253740) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253730; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_xc_irq_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255660)
	{
		bool const endpoint = !strcmp(machine().options().bios(), "block-xc-end-irq");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = std::array<unsigned, 4>{3, 0, 1, 7}[variant % 4], checkpoint = std::array<unsigned, 4>{3, 7, 10, 14}[variant / (endpoint ? 16 : 96)];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Short in-block XC stays staged");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_register_writes, m_pipeline_extensions_reads, m_vector_reads, unsigned(m_pipeline_memory_case)}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save overlapping XC instances and pending interrupt"); }
		if (tick == 120)
		{
			bool const complete = m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_vector_reads == 1 && m_pipeline_extensions_reads == 1 && m_pipeline_store_writes == count + 1 && m_pipeline_register_writes == (endpoint ? 1U : count + 1) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2fff && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5101 + count && m_cpu->state_int(tms320c54x_device::STATE_AR4) == (endpoint ? 0x5201U : 0x5201 + count);
			if (!complete) osd_printf_info("Short XC final failure: variant=%u idle=%u vector=%u stack=%u first=%u second=%u brc=%04x sp=%04x ar3=%04x ar4=%04x pc=%06x\n", variant, unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)), m_vector_reads, m_pipeline_extensions_reads, m_pipeline_store_writes, m_pipeline_register_writes, unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR3)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR4)), unsigned(m_cpu->state_int(STATE_GENPC)));
			if (!complete) osd_printf_info("Short XC pending state: requested=%u ifr=%04x imr=%04x st1=%04x cycles=%llu\n", unsigned(m_pipeline_memory_case), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), (unsigned long long)m_cpu->pipeline_cycles());
			expect(complete, "Pending interrupt enters once after the complete short XC repeat");
			// IFR and the NMI latch are restored by the CPU. Resubmitting an
			// input here would queue a new edge before scheduler settlement.
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_register_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_vector_reads = m_recurring_irq_snapshot[3]; m_pipeline_memory_case = m_recurring_irq_snapshot[4]; if (endpoint) m_cpu->set_pipeline_frontend(true); m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore XC overlap without losing the pending physical source"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255670; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (endpoint ? 63U : 383U)) { start_block_xc_irq(variant + 1); return true; }
			if (endpoint) { osd_printf_info("TMS320C54x endpoint XC protection: PASS cases=64 restore=64 counts=0,1,3,7 loop_words=2 sources=IRQ,NMI pages=2 snapshots=4 wrapped_protection=1 final_fallthrough=1 all_stores_before_vector=1 vector_fetches=1 stack_writes=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated endpoint XC protection complete"); }
			osd_printf_info("TMS320C54x short block XC protection: PASS cases=384 restore=384 counts=0,1,3,7 forms=XC1,XC2 loop_words=3,4,5 sources=IRQ,NMI pages=2 snapshots=4 all_stores_before_vector=1 vector_fetches=1 stack_writes=1 physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated short block XC protection complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255670) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255660; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_software_intr_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254400)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant & 128 ? 5 : 3;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Software INTR executes without atomic fallback");
		u32 const base = variant & 64 ? 0x20000 : 0;
		if (tick == 3) { expect(m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x601, "Software INTR captures RTN at Decode"); m_cpu->space(AS_PROGRAM).write_word(base + 0x600, 0xf495); }
		if (tick == 5) m_cpu->space(AS_PROGRAM).write_word(base + 0x100 + (variant % 32) * 4, 0xf495);
		if (tick == 9) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2fff, 0xdead); }
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save retained software interrupt entry"); }
		if (tick == 32)
		{
			unsigned const vector = variant % 32;
			expect(m_cpu->state_int(STATE_GENPC) == base + 0x603 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == 0x100 && m_cpu->state_int(tms320c54x_device::STATE_IMR) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL, "Software INTR preserves program page, configuration and arithmetic state");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 3 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x789a && m_cpu->state_int(tms320c54x_device::STATE_TRN) == 0x9876 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_IFR) == (vector < 16 ? 0xffff : 0xffff ^ (1U << (vector - 16))), "Software INTR masks interrupts, clears only its IFR bit and returns to the preserved page");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay software interrupt without duplicate frame or handler writes"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254410; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 255) { start_software_intr(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged software INTR: PASS cases=256 restore=256 vectors=32 masks=2 pages=2 snapshots=2 frame_writes=1 return_reads=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated software INTR complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254410) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254400; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_poll_interrupt_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 256100)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		// set_input_line queues a scheduler event; snapshot only after it
		// settles, not in the host callback that first asserts the pin.
		unsigned const arrival = 16 + variant % 32, checkpoint = arrival - 1 + std::array<unsigned, 3>{0, 2, 6}[variant / 128];
		u32 const base = (variant / 64) % 2 ? 0x20000 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Polling loop and interrupt handler use supported instructions");
		if (tick == arrival) m_cpu->set_input_line((variant / 32) % 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_vector_acks, 0}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save polling loop before during or after interrupt arrival"); }
		if (tick == 160)
		{
			bool const passed = m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + 0x609 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1003 && m_pipeline_store_writes == 1 && m_vector_acks == 1;
			if (!passed) osd_printf_info("poll_interrupt_state: case=%u replay=%u pc=%llx idle=%llu sp=%llx st1=%llx ifr=%llx reads=%u writes=%u acks=%u\n", variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(STATE_GENPC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IFR), m_pipeline_operand_reads, m_pipeline_store_writes, m_vector_acks);
			expect(passed, "IRQ NMI must release a short delayed-branch polling loop and return with a balanced frame");
			expect(m_cpu->space(AS_DATA).read_word(0x1004) == 1, "The decoded delay load completes before the handler observes the foreground accumulator");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_vector_acks = m_recurring_irq_snapshot[2]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore polling and interrupt ownership without duplicate handler writes"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 256110; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_poll_interrupt(variant + 1); return true; }
			osd_printf_info("TMS320C54x polling loop interrupts: PASS cases=384 restore=384 sources=IRQ,NMI arrivals=32 pages=2 snapshots=3 release=handler balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated polling loop interrupts complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 256110) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 256100; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
