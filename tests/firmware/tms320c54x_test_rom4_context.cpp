// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

// Copied Nokia ROM4 programs: firmware integration, not generic ISA conformance.
void tms320c54x_test_state::start_pipeline_mu4_prologue(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	u32 const base = BIT(variant, 0) ? 0x20000 : 0;
	// Original R060 aa22 skip-worker path through RET at 02:3da5.
	static constexpr u16 prefix[] = {0xeeff, 0x4a06, 0x4a07, 0xf6be, 0xf6b9,
		0x4a08, 0x4a09, 0x4a0a, 0x4a0b, 0x4a0c, 0x4a0d, 0xea00, 0x577c, 0xf300, 0x0001, 0x4f7c, 0x487e,
		0xf030, 0xc004, 0xf050, 0xc004, 0xfa44, 0x3d9a, 0x467f, 0xf495};
	static constexpr u16 tail[] = {0x1036, 0xf7be, 0x8a0d, 0x8a0c, 0x8a0b,
		0x8006, 0x8a0a, 0x8a09, 0x8a08, 0x8a07, 0x8a06, 0xfc00};
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x3d73 + i, i < std::size(prefix) ? prefix[i] : 0xf495);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x3d9a + i, i < std::size(tail) ? tail[i] : 0xf495);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x4044 + i, 0xf495);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0x061b}, {cpu_regs::STATE_ST1, 0x4a00}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0x2233445566ULL}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0},
	});
	for (unsigned i = 0xff7; i <= 0xfff; ++i) m_cpu->space(AS_DATA).write_word(i, 0xaaaa);
	m_cpu->space(AS_DATA).write_word(0x7c, 0x1357);
	m_cpu->space(AS_DATA).write_word(0x7d, 0x2468);
	m_cpu->space(AS_DATA).write_word(0x7e, 0x8001);
	m_cpu->space(AS_DATA).write_word(0x7f, 0xe012);
	m_cpu->space(AS_DATA).write_word(0x936, 0x4044);
	m_cpu->set_state_int(STATE_GENPC, base + 0x3d73);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_store_writes = 0;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xff7, 0xfff, "staged_mu4_context_save",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			static constexpr u16 expected[] = {0x061b, 0x4a00, 0x789a, 0x3456, 0x12, 0x5566, 0x3344, 0x22, 0x4044};
			expect(m_pipeline_store_writes < 9 && address == (m_pipeline_store_writes < 8 ? 0xffe - m_pipeline_store_writes : 0xfff) &&
				value == expected[m_pipeline_store_writes], "Original MU4 prologue saves status and accumulator words in stack order");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xfff, 0xfff, "staged_mu4_reserved_return",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x4044 && m_cpu->pipeline_cycles() == 46, "Original RET reads the reserved slot after the older POPM ST0 increment");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x4044, base + 0x4044, "staged_mu4_reserved_continuation",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 48, "Original RET reaches the reserved continuation in its current program page");
			++m_pipeline_extensions_reads;
		});
	m_phase = 251470; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_original_worker_roundtrip(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const masked = BIT(variant, 0); u16 const saved_status = masked ? 0x0814 : 0x0014;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 96; ++i) program.write_word(0x23d90 + i, 0xf495);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x20040 + i, 0xf495);
	std::array<u16, 4> const call{0xf274, 0x3dbc, 0x770e, 0x0006};
	std::array<u16, 9> const entry{0xfd00, 0x4a07, 0xfd00, 0xf7bb, 0x7192, 0x0010, 0x1092, 0xf844, 0x3ddb};
	std::array<u16, 7> const tail{0x8a08, 0xf030, 0x0800, 0xf495, 0xfe00, 0xfd45, 0xf6bb};
	for (unsigned i = 0; i < call.size(); ++i) program.write_word(0x23d92 + i, call[i]);
	for (unsigned i = 0; i < entry.size(); ++i) program.write_word(0x23dbc + i, entry[i]);
	for (unsigned i = 0; i < tail.size(); ++i) program.write_word(0x23ddb + i, tail[i]);
	program.write_word(0x23de2, 0xffff); program.write_word(0x23d96, 0xe955);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0x0608}, {cpu_regs::STATE_ST1, saved_status}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, 0x2233445566ULL}, {cpu_regs::STATE_B, 0x1122334455ULL}, {cpu_regs::STATE_T, 0xcafe},
		{cpu_regs::STATE_AR0, 0x5678}, {cpu_regs::STATE_AR2, 0x510}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 1},
	});
	m_cpu->space(AS_DATA).write_word(0x510, 0xbeef); m_cpu->space(AS_DATA).write_word(0x511, 1);
	m_cpu->set_state_int(STATE_GENPC, 0x23d92); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = m_pipeline_drained_fetches = m_vector_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "worker_roundtrip_stack_reads",
		[this, saved_status](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == (address == 0xffe ? saved_status : 0x3d96) && m_cpu->pipeline_cycles() == (address == 0xffe ? 21 : 25), "Original worker reads its own saved ST1 and CALLD return once"); ++m_pipeline_operand_reads;
		});
	m_vector_stack_tap = m_cpu->space(AS_DATA).install_write_tap(0xffe, 0xfff, "worker_roundtrip_stack_writes",
		[this, saved_status, masked](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			u64 const clock = m_cpu->pipeline_cycles();
			expect((clock == 6 && address == 0xfff && word == 0x3d96) ||
				(clock == 11 && address == 0xffe && word == saved_status) ||
				(!masked && clock == 35 && address == 0xfff), "CALLD, worker status push and later IRQ own separate stack writes"); ++m_pipeline_store_writes;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_read_tap(0x510, 0x511, "worker_roundtrip_sources",
		[this](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == (address == 0x510 ? 0xbeef : 1) && m_cpu->pipeline_cycles() == (address == 0x510 ? 13 : 15), "Original worker samples its two adjacent source words once"); ++m_pipeline_extensions_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x23d96, 0x23de2, "worker_roundtrip_route",
		[this](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			if (address != 0x23dbc && address != 0x23ddb && address != 0x23d96 && address != 0x23de2) return;
			expect(address != 0x23de2 && m_cpu->pipeline_cycles() == (address == 0x23dbc ? 6 : address == 0x23ddb ? 18 : 27), "Original worker follows CALLD, skip-body BC and RETD once without a third delay word"); ++m_pipeline_drained_fetches;
		});
	m_vector_prefetch_tap = program.install_read_tap(0x20040, 0x20040, "worker_roundtrip_irq",
		[this, masked](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!masked && m_cpu->pipeline_cycles() == 34 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55, "Original worker restores interrupt delivery after returning to its caller"); ++m_vector_reads;
		});
	m_phase = 251830; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_original_context_call(unsigned variant, bool full_body)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = BIT(variant, 0) ? 0x20000 : 0;
	u16 const saved_status = 0x4200 | (BIT(variant, 1) ? 0x800 : 0) | (full_body && BIT(variant, 2) ? 0x100 : 0);
	std::array<u16, 51> const main{0xeeff, 0x4a06, 0x4a07, 0xf6be, 0xf6b9,
		0x4a08, 0x4a09, 0x4a0a, 0x4a0b, 0x4a0c, 0x4a0d, 0xea00, 0x577c, 0xf300, 0x0001, 0x4f7c, 0x487e,
		0xf030, 0xc004, 0xf050, 0xc004, 0xfa44, 0x3d9a, 0x467f, 0xf495,
		0x4a10, 0x4a12, 0x4a13, 0x4a0e, 0x7131, 0x0012, 0xf274, 0x3dbc, 0x770e, 0x0006,
		0x8a0e, 0x8a13, 0x8a12, 0x8a10, 0x1036, 0xf7be, 0x8a0d, 0x8a0c, 0x8a0b,
		0x8006, 0x8a0a, 0x8a09, 0x8a08, 0x8a07, 0x8a06, 0xfc00};
	std::array<u16, 9> const entry{0xfd00, 0x4a07, 0xfd00, 0xf7bb, 0x7192, 0x0010, 0x1092, 0xf844, 0x3ddb};
	std::array<u16, 7> const tail{0x8a08, 0xf030, 0x0800, 0xf495, 0xfe00, 0xfd45, 0xf6bb};
	std::array<u16, 22> const body{0x6b82, 2, 0x4b92, 0x7182, 0x0013, 0x8b93, 0x8393, 0x8193, 0xf4ab, 0x8c93,
		0x108a, 0x6d8a, 0xfd30, 0x8082, 0x6dea, 3, 0xf000, 4, 0x1892, 0x1a8a, 0x6d8a, 0x8082};
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 144; ++i) program.write_word(base + 0x3d73 + i, 0xf495);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x4044 + i, 0xf495);
	for (unsigned i = 0; i < main.size(); ++i) program.write_word(base + 0x3d73 + i, main[i]);
	for (unsigned i = 0; i < entry.size(); ++i) program.write_word(base + 0x3dbc + i, entry[i]);
	if (full_body) for (unsigned i = 0; i < body.size(); ++i) program.write_word(base + 0x3dc5 + i, body[i]);
	for (unsigned i = 0; i < tail.size(); ++i) program.write_word(base + 0x3ddb + i, tail[i]);
	program.write_word(base + 0x3de2, 0xffff);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0x061b}, {cpu_regs::STATE_ST1, saved_status}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0x2233445566ULL}, {cpu_regs::STATE_AR0, 0x1357},
		{cpu_regs::STATE_AR2, 0x2468}, {cpu_regs::STATE_AR3, 0x369c}, {cpu_regs::STATE_T, 0x8888},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0xff0; i <= 0x1000; ++i) data.write_word(i, 0xaaaa);
	data.write_word(0x7c, 0x1357); data.write_word(0x7d, 0x2468);
	data.write_word(0x7e, 0xc004); data.write_word(0x7f, 0xe012);
	data.write_word(0x931, 0x510); data.write_word(0x936, 0x4044);
	data.write_word(0x510, full_body ? BIT(variant, 3) ? 0x472 : 0x474 : 0xbeef); data.write_word(0x511, full_body ? 0 : 1);
	if (full_body)
	{
		data.write_word(0x512, 0xff0f); data.write_word(0x513, 0x470);
		data.write_word(0x514, 0xff0f); data.write_word(0x515, 0x00f0);
		for (unsigned i = 0; i < 4; ++i) data.write_word(0x470 + i, 0x7777);
	}
	m_cpu->set_state_int(STATE_GENPC, base + 0x3d73); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_drained_fetches = m_vector_reads = 0;
	m_pipeline_write_tap = data.install_write_tap(full_body ? 0xff0 : 0xff1, 0xfff, "original_context_call_stack",
		[this, saved_status, full_body](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 16> const expected{0x061b, saved_status, 0x789a, 0x3456, 0x12, 0x5566, 0x3344, 0x22,
				0x1357, 0x2468, 0x369c, 0x8888, 0x3d96, u16(saved_status & 0x900), full_body ? u16(0xff11) : u16(0x4044), 0x4044};
			unsigned const final = full_body ? 15 : 14;
			expect(m_pipeline_store_writes <= final && address == (m_pipeline_store_writes < final ? 0xffe - m_pipeline_store_writes : 0xfff) &&
				word == expected[m_pipeline_store_writes], "Original context call route owns all nested saves and reserved return write"); ++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = data.install_read_tap(0xfff, 0xfff, "original_context_call_return",
		[this](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x4044, "Original call route reads the reserved return after restoring context"); ++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x4044, base + 0x4044, "original_context_call_continuation",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			++m_pipeline_extensions_reads;
		});
	if (full_body)
	{
		bool const matched = BIT(variant, 3);
		m_vector_stack_tap = data.install_write_tap(0x470, 0x515, "full_context_body_writes",
			[this, matched](offs_t address, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				std::array<u16, 7> const addresses{0x512, 0x470, 0x471, 0x472, 0x473, 0x511, 0x513};
				std::array<u16, 7> const values{0xff11, 0xff11, 0x1357, 0x2469, 6, 0x470, 0x4f4};
				unsigned const index = m_pipeline_drained_fetches + unsigned(!matched && m_pipeline_drained_fetches >= 5);
				expect(index < addresses.size() && address == addresses[index] && value == values[index],
					"Outer context body publishes its counter-derived B words and retained source/store effects once");
				++m_pipeline_drained_fetches;
			});
		m_vector_prefetch_tap = data.install_read_tap(0x510, 0x515, "full_context_body_reads",
			[this, matched](offs_t address, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				std::array<u16, 7> const addresses{0x510, 0x511, 0x512, 0x513, 0x513, 0x514, 0x515};
				std::array<u16, 7> const values{u16(matched ? 0x472 : 0x474), 0, 0xff0f, 0x470, 0x470, 0xff0f, 0x00f0};
				expect(m_vector_reads < addresses.size() && address == addresses[m_vector_reads] && value == values[m_vector_reads],
					"Outer context body consumes each source in original order without duplicate replay reads");
				++m_vector_reads;
			});
	}
	m_phase = full_body ? 252070 : 251850; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mu4_timer_context(unsigned variant)
{
	bool const sxm = BIT(variant, 0), matched = BIT(variant, 1);
	unsigned const page = BIT(variant, 2) ? 3 : 0;
	start_pipeline_original_context_call(1 | (sxm ? 4 : 0) | (matched ? 8 : 0), true);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
	m_vector_stack_tap.remove(); m_vector_prefetch_tap.remove();
	auto &program = m_cpu->space(AS_PROGRAM);
	// Original uploaded lower-32K code, with the observed warm OVLY mapping.
	for (unsigned i = 0; i < 144; ++i) program.write_word(0x3d73 + i, program.read_word(0x23d73 + i));
	std::array<u16, 4> const vector{0x4a1e, 0xf495, 0xf882, 0x3d73};
	for (unsigned i = 0; i < vector.size(); ++i) program.write_word(0x204c + i, vector[i]);
	program.write_word(0x4044, 0xf4e5); // Original FRETE, not a fixture continuation.
	u32 const foreground = (page << 16) | 0x9000;
	for (unsigned i = 0; i < 512; ++i) program.write_word(foreground + i, 0xf495);
	program.write_word(foreground + 0x80, 0xe966);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PMST, 0x2020}, {cpu_regs::STATE_ST1, 0x4200 | (sxm ? 0x100 : 0)}, {cpu_regs::STATE_A, 0x77},
		{cpu_regs::STATE_IMR, 8}, {cpu_regs::STATE_IFR, 0},
	});
	m_cpu->set_state_int(STATE_GENPC, foreground); m_cpu->set_pipeline_frontend(true);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
	m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 31);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_vector_acks = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_drained_fetches = m_vector_reads = m_pipeline_operand_reads = m_fast_stack_reads = 0;
	m_timer_probe_return_pc = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfe0, 0xfff, "mu4_timer_original_stack",
		[this, page, sxm](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 18> const values{m_timer_probe_return_pc, u16(page), 0x061b, u16(0x4a00 | (sxm ? 0x100 : 0)),
				0x77, 0, 0, 0x5566, 0x3344, 0x22, 0x1357, 0x2468, 0x369c, 0x8888,
				0x3d96, u16(0x800 | (sxm ? 0x100 : 0)), 0xff11, 0x4044};
			unsigned const index = m_pipeline_store_writes;
			unsigned const destination = index < 2 ? 0xfff - index : index < 17 ? 0xffe - index : 0xffd;
			if (index >= values.size() || address != destination || value != values[index])
				osd_printf_info("MU4 timer stack mismatch: index=%u cycle=%llu address=%04x value=%04x return=%04x\n",
					index, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(address), value, m_timer_probe_return_pc);
			expect(index < values.size() && address == destination && value == values[index],
				"Hardware timer, original XPC vector and handler own a balanced ordered context stack");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "mu4_timer_frete_reads",
		[this, page](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_fast_stack_reads < 2 && address == 0xffe + m_fast_stack_reads && value == (m_fast_stack_reads ? m_timer_probe_return_pc : page),
				"Original FRETE consumes saved XPC then the hardware return PC once");
			++m_fast_stack_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x204c, 0x4044, "mu4_timer_original_route",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x204c) { expect(value == 0x4a1e, "Timer vector uses original PSHM XPC"); ++m_pipeline_extensions_reads; }
			if (address == 0x3d73) { expect(value == 0xeeff, "Timer enters the original context handler via FB"); ++m_pipeline_drained_fetches; }
			if (address == 0x4044) { expect(value == 0xf4e5, "Original handler returns through FRETE"); ++m_vector_reads; }
		});
	m_vector_prefetch_tap = program.install_read_tap(foreground + 0x80, foreground + 0x80, "mu4_timer_resumed_foreground",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0xe966 && m_fast_stack_reads == 2 && m_vector_reads == 1, "Foreground marker is fetched after original far return");
			++m_pipeline_operand_reads;
		});
	m_phase = 252110; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mu4_recurring_timer(unsigned variant)
{
	bool const short_period = BIT(variant, 3);
	start_pipeline_mu4_timer_context(variant & 7);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
	unsigned const page = BIT(variant, 2) ? 3 : 0;
	u32 const foreground = (page << 16) | 0x9000;
	auto &program = m_cpu->space(AS_PROGRAM);
	std::array<u16, 5> const loop{0x6bf8, 0x0700, 1, 0xf073, 0x9000};
	for (unsigned i = 0; i < loop.size(); ++i) program.write_word(foreground + i, loop[i]);
	program.write_word(foreground + 0x80, 0xf495);
	m_cpu->space(AS_DATA).write_word(0x700, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, short_period ? 1 : 0xff);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
	m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 31);
	m_pipeline_replay = false; m_irq_raised = false; m_recurring_irq_end_cycle = 0;
	m_recurring_irq_first_return_writes = ~0U;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfe0, 0xfff, "recurring_mu4_stack",
		[this, page](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0xfff) expect(value == m_timer_probe_return_pc, "Each timer entry stacks its actual foreground continuation");
			if (address == 0xffe) expect(value == page, "Each original vector saves the actual foreground page");
			if (address == 0xffd) expect(value == 0x4044, "Each original handler publishes its FRETE return into the reserved slot");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "recurring_mu4_return",
		[this, page](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			bool const pc = BIT(m_fast_stack_reads, 0);
			expect(address == (pc ? 0xfff : 0xffe) && value == (pc ? m_timer_probe_return_pc : page),
				"Every FRETE consumes its current saved page and PC in order");
			++m_fast_stack_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x204c, 0x4044, "recurring_mu4_route",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x204c) { expect(value == 0x4a1e, "Recurring timer uses original vector"); ++m_pipeline_extensions_reads; }
			if (address == 0x3d73) { expect(value == 0xeeff, "Recurring timer uses original context handler"); ++m_pipeline_drained_fetches; }
			if (address == 0x4044) { expect(value == 0xf4e5, "Recurring timer uses original far return"); ++m_vector_reads; }
		});
	m_vector_prefetch_tap = m_cpu->space(AS_DATA).install_write_tap(0x700, 0x700, "recurring_mu4_foreground_retirement",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == m_pipeline_operand_reads + 1, "Foreground counter changes only by an actually retired ADDM");
			++m_pipeline_operand_reads;
		});
	m_phase = 252130; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::observe_context_irq(int line)
{
	if (m_phase == 252110 || m_phase == 252130)
	{
		expect(line == 3, "Original MU4 timer fixture acknowledges only the hardware timer source");
		++m_vector_acks; m_timer_probe_return_pc = m_cpu->state_int(tms320c54x_device::STATE_RTN);
	}
}

bool tms320c54x_test_state::check_rom4_context_results()
{
	if (!profile_is("rom4-context")) return false;
	expect(!(m_cpu->state_int(cpu_regs::STATE_ST0) & 0xe000) && !(m_cpu->state_int(cpu_regs::STATE_ST1) & 0x20),
		"ROM4 integration uses standard mode with ARP zero at every observation");
	if (m_phase == 251470)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		u64 const checkpoint = std::array<unsigned, 3>{6, 13, 39}[m_pipeline_peripheral_case / 2];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original MU4 prologue advances without atomic fallback");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save in-flight original MU4 context prologue");
		}
		if (cycle == 56)
		{
			{
				auto const disable = machine().disable_side_effects();
			expect(m_pipeline_store_writes == 9 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x061b && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x4a00 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL &&
				m_cpu->space(AS_DATA).read_word(0xfff) == 0x4044 &&
				m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469,
				"Original skip-worker path restores context and writes the reserved return slot using the pre-POPM SP sample");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_store_writes = checkpoint == 6 ? 0 : checkpoint == 13 ? 5 : 8;
				m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore overlapping original prologue sources and stack writes");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251480;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			record_context_case(0, m_pipeline_peripheral_case);
			if (m_pipeline_peripheral_case < 5) { start_pipeline_mu4_prologue(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged original MU4 context: PASS words=37 cases=6 context_saves=8 context_restored=1 returned=4044 restore=6 full_handler_claim=0\n");
			start_pipeline_original_worker_roundtrip(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251480)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251470;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251830)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		bool const masked = BIT(variant, 0); u16 const saved_status = masked ? 0x0814 : 0x0014;
		u64 const checkpoint = std::array<unsigned, 8>{4, 6, 11, 17, 21, 25, 29, 30}[variant / 2];
		auto const reads_at = [](u64 clock) { return unsigned(clock >= 21) + unsigned(clock >= 25); };
		auto const sources_at = [](u64 clock) { return unsigned(clock >= 13) + unsigned(clock >= 15); };
		auto const fetches_at = [](u64 clock) { return unsigned(clock >= 6) + unsigned(clock >= 18) + unsigned(clock >= 27); };
		auto const writes_at = [masked](u64 clock) { return unsigned(clock >= 6) + unsigned(clock >= 11) + unsigned(!masked && clock >= 35); };
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == reads_at(cycle) && m_pipeline_extensions_reads == sources_at(cycle) &&
			m_pipeline_drained_fetches == fetches_at(cycle) && m_pipeline_store_writes == writes_at(cycle) && m_vector_reads == unsigned(!masked && cycle >= 34) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (!masked && cycle >= 33 ? 0xfff : cycle >= 24 ? 0x1000 : cycle >= 21 ? 0xfff : cycle >= 10 ? 0xffe : cycle >= 4 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (!masked && cycle >= 30 && cycle < 34 ? saved_status : cycle >= 13 ? saved_status | 0x800 : saved_status) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 24 ? (masked ? 0x800 : 0) : cycle >= 22 ? saved_status : cycle >= 16 ? 1 : 0x2233445566ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 31 ? 0x55 : 0x1122334455ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (cycle >= 7 ? 6 : 0xcafe) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == (cycle >= 13 ? 0xbeef : 0x5678) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (cycle >= 14 ? 0x512 : cycle >= 13 ? 0x511 : 0x510),
			"Original skip-body worker call composes parameter, context, source, branch and return ownership");
		if (cycle == 26) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
		if (cycle == 13) m_cpu->space(AS_DATA).write_word(0x510, 0);
		if (cycle == 15) m_cpu->space(AS_DATA).write_word(0x511, 0);
		if (cycle == 21 || cycle == 25)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(cycle == 21 ? 0xffe : 0xfff, 0xdead);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save original worker roundtrip at eight ownership boundaries");
		}
		if (cycle == 40)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = reads_at(checkpoint); m_pipeline_extensions_reads = sources_at(checkpoint);
				m_pipeline_drained_fetches = fetches_at(checkpoint); m_pipeline_store_writes = writes_at(checkpoint); m_vector_reads = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore original worker roundtrip without duplicate stack or source ownership");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251840;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_stack_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			record_context_case(1, variant);
			if (variant < 15) { start_pipeline_original_worker_roundtrip(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original worker roundtrip: PASS cases=16 original_words=20 route=skip_body saved_intm_states=2 restore=16 duplicate_stack_reads=0 duplicate_source_reads=0\n");
			start_pipeline_original_context_call(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251840)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251830;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251850)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		u16 const saved_status = BIT(variant, 1) ? 0x4a00 : 0x4200;
		u64 const checkpoint = std::array<unsigned, 10>{6, 13, 31, 40, 51, 58, 64, 74, 79, 85}[variant / 4];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original outer context call route advances without atomic fallback");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_pipeline_register_writes = m_pipeline_store_writes;
			m_pipeline_branch_case = m_pipeline_operand_reads; m_pipeline_stm_case = m_pipeline_extensions_reads;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save coherent outer handler at ten nested ownership boundaries");
		}
		if (cycle == 100)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_store_writes == 15 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x061b &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == saved_status && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 && m_cpu->space(AS_DATA).read_word(0xfff) == 0x4044 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469,
					"Original outer handler calls the skip-body worker and restores all saved context with one counter increment");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = m_pipeline_register_writes;
				m_pipeline_operand_reads = m_pipeline_branch_case; m_pipeline_extensions_reads = m_pipeline_stm_case;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore complete context call route without duplicated saves or return fetch");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251860;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			record_context_case(2, variant);
			if (variant < 39) { start_pipeline_original_context_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original context call route: PASS cases=40 original_words=67 pages=2 saved_intm_states=2 restore=40 context_restored=1 counter_increments=1 full_isr_claim=0\n");
			start_pipeline_original_context_call(0, true); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251860)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251850;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252070)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		u16 const saved_status = 0x4200 | (BIT(variant, 1) ? 0x800 : 0) | (BIT(variant, 2) ? 0x100 : 0);
		unsigned const checkpoint = std::array<unsigned, 8>{6, 13, 31, 51, 65, 75, 90, 104}[variant / 16];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"Complete outer-context body route advances without replacing original instructions");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_pipeline_register_writes = m_pipeline_store_writes; m_pipeline_branch_case = m_pipeline_operand_reads;
			m_pipeline_stm_case = m_pipeline_extensions_reads; m_pipeline_store_case = m_pipeline_drained_fetches;
			m_pipeline_indirect_case = m_vector_reads;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save original outer handler with active body at eight ownership boundaries");
		}
		if (cycle == 130)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_store_writes == 16 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 &&
					m_pipeline_drained_fetches == (BIT(variant, 3) ? 7 : 6) && m_vector_reads == 7 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x061b &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == saved_status && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 && m_cpu->space(AS_DATA).read_word(0xfff) == 0x4044 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469 &&
					m_cpu->space(AS_DATA).read_word(0x511) == (BIT(variant, 3) ? 0x470 : 0) &&
					m_cpu->space(AS_DATA).read_word(0x513) == 0x4f4,
					"Original active-body context route restores all saved registers and keeps its exact memory effects");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = m_pipeline_register_writes; m_pipeline_operand_reads = m_pipeline_branch_case;
				m_pipeline_extensions_reads = m_pipeline_stm_case; m_pipeline_drained_fetches = m_pipeline_store_case; m_vector_reads = m_pipeline_indirect_case;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore complete outer-context body route without repeating its work");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252080;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
			m_vector_stack_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			record_context_case(3, variant);
			if (variant < 127) { start_pipeline_original_context_call(variant + 1, true); return true; }
			osd_printf_info("TMS320C54x staged complete MU4 context body: PASS cases=128 original_words=89 pages=2 saved_intm_states=2 sxm_states=2 cmpr_outcomes=2 restore=128 context_restored=1 counter_increments=1 full_isr_claim=0\n");
			start_pipeline_mu4_timer_context(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252080)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252070;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252110)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const sxm = BIT(variant, 0), matched = BIT(variant, 1);
		unsigned const page = BIT(variant, 2) ? 3 : 0;
		unsigned const checkpoint = std::array<unsigned, 6>{28, 36, 55, 85, 130, 200}[variant / 8];
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Timer-driven original context executes without a synthetic handler jump");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_pipeline_store_case = m_pipeline_store_writes; m_pipeline_long_case = m_pipeline_extensions_reads;
			m_pipeline_rmw_case = m_pipeline_drained_fetches; m_pipeline_return_case = m_vector_reads;
			m_pipeline_indirect_case = m_pipeline_operand_reads; m_pipeline_register_writes = m_vector_acks; m_pipeline_branch_case = m_fast_stack_reads;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save genuine timer entry, original context and far-return foreground settlement");
		}
		if (cycle == 400)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_vector_acks == 1 && m_pipeline_extensions_reads == 1 && m_pipeline_drained_fetches == 1 && m_vector_reads == 1 &&
					m_pipeline_store_writes == 18 && m_fast_stack_reads == 2 && m_pipeline_operand_reads == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_XPC) == page &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x061b && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x4200 | (sxm ? 0x100 : 0)) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x77 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x66 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469 &&
					m_cpu->space(AS_DATA).read_word(0x511) == (matched ? 0x470 : 0) && m_cpu->space(AS_DATA).read_word(0x513) == 0x4f4,
					"Original timer ISR restores hardware return page/PC and foreground retirement with one worker execution");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = m_pipeline_store_case; m_pipeline_extensions_reads = m_pipeline_long_case;
				m_pipeline_drained_fetches = m_pipeline_rmw_case; m_vector_reads = m_pipeline_return_case; m_pipeline_operand_reads = m_pipeline_indirect_case;
				m_vector_acks = m_pipeline_register_writes; m_fast_stack_reads = m_pipeline_branch_case;
				if (!m_vector_acks) m_timer_probe_return_pc = 0;
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore original timer ISR without duplicate vector, context or return ownership");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252120;
				m_check_timer->enable(true); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_pipeline_frontend(false);
			record_context_case(4, variant);
			if (variant < 47) { start_pipeline_mu4_timer_context(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original MU4 timer ISR: PASS cases=48 original_words=94 foreground_pages=2 sxm_states=2 cmpr_outcomes=2 restore=48 timer_vector=1 original_frete=1 foreground_retired=1\n");
			start_pipeline_mu4_recurring_timer(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252120)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252110;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252130)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const sxm = BIT(variant, 0), matched = BIT(variant, 1), short_period = BIT(variant, 3);
		unsigned const page = BIT(variant, 2) ? 3 : 0;
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const point = variant / 16;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && cycle < 6000,
			"Recurring original timer context remains bounded without reseeding worker state");
		if (m_vector_acks == 1 && m_fast_stack_reads == 2 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
			!BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 11) && m_recurring_irq_first_return_writes == ~0U)
			m_recurring_irq_first_return_writes = m_pipeline_operand_reads;
		bool const save_point = point == 0 ? cycle == 28 : point == 1 ? m_vector_acks == 1 :
			point == 2 ? m_fast_stack_reads == 2 : point == 3 ? m_vector_acks == 3 : point == 4 ? m_fast_stack_reads == 6 : m_vector_acks == 6;
		if (save_point && !m_irq_raised && !m_pipeline_replay)
		{
			m_irq_raised = true; m_recurring_irq_checkpoint_cycle = cycle;
			m_recurring_irq_snapshot = {m_vector_acks, m_pipeline_extensions_reads, m_pipeline_drained_fetches, m_vector_reads,
				m_fast_stack_reads, m_pipeline_store_writes, m_pipeline_operand_reads, m_timer_probe_return_pc, m_recurring_irq_first_return_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save recurring timer and evolving original worker at a real interrupt boundary");
		}
		bool const returned = m_vector_acks == 8 && m_vector_reads == 8 && m_fast_stack_reads == 16 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && !BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 11) &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == page;
		if (returned)
		{
			{
				auto const disable = machine().disable_side_effects();
				unsigned const bodies = matched ? 1 : 8;
				u16 pointer = 0x470; for (unsigned i = 0; i < bodies; ++i) pointer = ((pointer + 4) & 0xff0f) | 0x00f0;
				expect(m_irq_raised && m_pipeline_extensions_reads == 8 && m_pipeline_drained_fetches == 8 &&
					m_pipeline_store_writes == 8 * 17 + bodies && m_recurring_irq_first_return_writes != ~0U &&
					(short_period || m_pipeline_operand_reads > m_recurring_irq_first_return_writes) &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x061b && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x4200 | (sxm ? 0x100 : 0)) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x77 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2470 &&
					m_cpu->space(AS_DATA).read_word(0x512) == 0xff0f + bodies * 2 && m_cpu->space(AS_DATA).read_word(0x513) == pointer &&
					m_cpu->space(AS_DATA).read_word(0x511) == (matched ? 0x470 : 0) && m_cpu->space(AS_DATA).read_word(0x700) == m_pipeline_operand_reads,
					"Recurring timer preserves caller context while original control/mask state evolves without forcing");
			}
			osd_printf_info("MU4 recurring timer observation: case=%u replay=%u prd=%u returns=8 cycle=%llu foreground_writes=%u first_return_writes=%u pending=%04x\n",
				variant, unsigned(m_pipeline_replay), short_period ? 1 : 255, (unsigned long long)cycle, m_pipeline_operand_reads, m_recurring_irq_first_return_writes,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
			if (!m_pipeline_replay)
			{
				m_recurring_irq_end_cycle = cycle; m_recurring_irq_end_writes = m_pipeline_operand_reads; m_pipeline_replay = true;
				m_vector_acks = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_pipeline_drained_fetches = m_recurring_irq_snapshot[2]; m_vector_reads = m_recurring_irq_snapshot[3];
				m_fast_stack_reads = m_recurring_irq_snapshot[4]; m_pipeline_store_writes = m_recurring_irq_snapshot[5];
				m_pipeline_operand_reads = m_recurring_irq_snapshot[6]; m_timer_probe_return_pc = m_recurring_irq_snapshot[7];
				m_recurring_irq_first_return_writes = m_recurring_irq_snapshot[8];
				// Mark the executing check timer modified before restoring it.
				// Resume at its restored deadline, not stale callback time.
				m_check_timer->adjust(attotime::never);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore recurring interrupts without duplicate timer, stack or foreground effects");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = m_recurring_irq_checkpoint_cycle + 1; m_phase = 252140;
				m_check_timer->enable(true); return true;
			}
			expect(cycle == m_recurring_irq_end_cycle && m_pipeline_operand_reads == m_recurring_irq_end_writes,
				"Recurring timer replay reproduces the exact eighth-return cycle and foreground effects");
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_pipeline_frontend(false);
			record_context_case(5, variant);
			if (variant < 95) { start_pipeline_mu4_recurring_timer(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged recurring MU4 timer: PASS cases=96 returns_per_leg=8 periods=2 foreground_pages=2 sxm_states=2 control_outcomes=2 restore=96 exact_return_cycle=1 firmware_reseeding=0\n");
			osd_printf_info("Nokia ROM4 context integration: PASS cases=%u restore=%u banks=6 silicon_phase_claim=0 coverage_fnv64=%016llx\n",
				m_context_coverage.count, m_context_coverage.count, (unsigned long long)m_context_coverage.value);
			throw emu_fatalerror(3, "Nokia ROM4 context integration complete");
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252140)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252130;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	throw emu_fatalerror("Unowned ROM4 context phase %u", m_phase);
}

} // namespace c54x_test
