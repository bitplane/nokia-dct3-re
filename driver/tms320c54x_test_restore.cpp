// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_far_save_case(bool returning)
{
	m_far_save_return = returning;
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	const unsigned origin = returning ? 0x030600 : 0x010600;
	unsigned next = origin;
	program.write_word(next++, returning ? 0xf6e4 : 0xfb82);
	if (!returning)
		program.write_word(next++, 0x0700);
	program.write_word(next++, 0x0082); // First delay word schedules a zero-time checkpoint.
	program.write_word(next++, 0xe905);
	program.write_word(next, 0xf4e1);
	program.write_word(0x020700, 0xe807);
	program.write_word(0x020701, 0xf4e4);
	program.write_word(0x000700, 0xe80f); // Wrong-page sentinels.
	program.write_word(0x000701, 0xf4e4);
	program.write_word(0x010700, 0xe80e);
	program.write_word(0x010701, 0xf4e4);
	if (returning)
	{
		program.write_word(0x010604, 0xe807);
		program.write_word(0x010605, 0xf4e1);
	}
	data.write_word(0x03fe, 1);
	data.write_word(0x03ff, 0x0604);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, returning ? 0x03fe : 0x0400);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0060);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(STATE_GENPC, origin);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 784;
	m_check_timer->adjust(attotime::from_usec(100));
}

tms320c54x_test_state::saved_repeat_fixture tms320c54x_test_state::saved_repeat_case(unsigned v)
{
	return {std::array<unsigned, 5>{0, 1, 2, 7, 56}[v % 5], std::array<unsigned, 4>{0, 1, 3, 63}[(v / 5) % 4], v / 640,
		bool((v / 20) % 2), bool((v / 40) % 2), bool((v / 320) % 2), (v / 80) % 2 ? 0x20000U : 0U, u16(0x3000 + (v / 160) % 2)};
}

u16 tms320c54x_test_state::saved_repeat_word(saved_repeat_fixture const &f, unsigned slot) const
{
	return slot == 1 ? 0x60a : slot == 2 ? m_saved_repeat_counts.return_pc : slot == 3 ? f.base >> 16 :
		slot == 4 ? 0xa608 : slot == 5 ? f.saved_st1() : slot == 6 ? f.saved_count() : slot == 7 ? 0x604 : f.end();
}

void tms320c54x_test_state::start_saved_repeat(unsigned variant)
{
	bool const stlm = !strcmp(machine().options().bios(), "repeat-stlm");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = saved_repeat_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); program.write_word(f.base + 0x900 + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, f.end());
	program.write_word(f.base + 0x604, 0xe589); program.write_word(f.base + 0x605, 0xe801);
	program.write_word(f.base + 0x606, 0xfb44); program.write_word(f.base + 0x607, 0x700);
	program.write_word(f.base + 0x60a, 0x6d96);
	program.write_word(f.base + f.end() + 1, 0x6d97); program.write_word(f.base + f.end() + 2, 0xf4e1);
	program.write_word(f.base + 0x701, 0x7695); program.write_word(f.base + 0x702, 0x1357);
	program.write_word(f.base + 0x71c, 0xfc00);
	program.write_word(f.base + f.vector(), 0x4a1e);
	program.write_word(f.base + f.vector() + 1, 0xf880 | (f.base >> 16)); program.write_word(f.base + f.vector() + 2, 0x900);
	std::vector<u16> handler{0x4a06, 0x4a07, 0x4a1a, 0x4a1b, 0x4a1c, 0xf495,
		0x771a, u16(f.inner), 0xf072, 0x90b, 0x7694, 0x55aa, 0xf495,
		0x8a1c, 0x8a1b, 0x8a1a, 0xf495, 0xf495, 0xf495, 0xf495, 0xf495, 0xf495,
		0x8a07, 0x8a06, u16(f.delayed ? 0xf6e5 : 0xf4e5)};
	if (stlm)
	{
		// The ROM4 frame ISR clears BRAF through an Execute-owned STLM
		// after saving the interrupted context, before using its own loop.
		handler.erase(handler.begin() + 5);
		handler.insert(handler.begin() + 5, {0x4807, 0xf030, 0x2000, 0xf040, 0x4900, 0x8807, 0xf495, 0xf495});
		handler[16] = 0x912;
		// This synthetic caller owns A=1; restore it before saved status.
		handler.insert(handler.begin() + 29, {0xf020, 0x0001});
	}
	for (unsigned i = 0; i < handler.size(); ++i) program.write_word(f.base + 0x900 + i, handler[i]);
	for (unsigned i = 0; i < 128; ++i) { data.write_word(0x5000 + i, 0x1357 ^ (i * 0x2345)); data.write_word(0x6000 + i, 0xdead); data.write_word(0x7000 + i, 0xdead); data.write_word(0x7100 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100); m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2345);
	for (unsigned ar = 2; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, std::array<u16, 6>{0x5000, 0x6000, 0x7100, 0x7000, 0x1600, 0x1700}[ar - 2]);
	m_saved_repeat_counts = {}; m_saved_repeat_checkpoint = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "saved_repeat_read", [this, f, stlm](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_saved_repeat_counts;
		if (at >= 0x5000 && at < 0x5080) { unsigned const i = c.outer_reads++; expect(i <= f.count && at == 0x5000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Restored outer loop reads each ordered source once"); return; }
		if (at >= f.sp - 8 && at < f.sp)
		{
			expect(word == saved_repeat_word(f, f.sp - at), "Return and context pops capture the firmware-published stack word"); ++c.stack_reads;
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xdead); return;
		}
		if (stlm && c.mapped_reads == 6) { expect(at == 7 && word == f.saved_st1(), "STLM prologue reads the still-active saved status before clearing BRAF"); ++c.mapped_reads; return; }
		std::array<u16, 6> const registers{0x1e, 6, 7, 0x1a, 0x1b, 0x1c};
		if (c.mapped_reads >= registers.size() || at != registers[c.mapped_reads] || word != saved_repeat_word(f, c.mapped_reads + 3))
			osd_printf_info("Saved-repeat register mismatch: case=%u read=%u at=%04x word=%04x expected=%04x\n", m_pipeline_peripheral_case, c.mapped_reads, unsigned(at), word, saved_repeat_word(f, c.mapped_reads + 3));
		expect(c.mapped_reads < registers.size() && at == registers[c.mapped_reads] && word == saved_repeat_word(f, c.mapped_reads + 3), "ISR saves XPC status and the interrupted repeat registers in order"); ++c.mapped_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "saved_repeat_write", [this, f, stlm](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_saved_repeat_counts;
		if (at >= 0x6000 && at < 0x6080) { unsigned const i = c.outer_writes++; expect(i <= f.count && at == 0x6000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Outer repeat resumes without skipped or duplicated copies"); return; }
		if (at >= 0x7000 && at < 0x7080)
		{
			unsigned const i = c.leaves++; expect(i <= f.count && at == 0x7000 + i && word == 0x1357, "CCD enters its outside-body leaf exactly once per outer iteration");
			if (!c.assertions) { ++c.assertions; m_cpu->set_input_line(f.nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE); } return;
		}
		if (at >= 0x7100 && at < 0x7180) { unsigned const i = c.inner_writes++; expect(i <= f.inner && at == 0x7100 + i && word == 0x55aa, "Callback repeat owns its independent count and ordered destinations"); if (!i) c.inner_cycle = m_cpu->pipeline_cycles(); return; }
		if (at >= f.sp - 8 && at < f.sp)
		{
			if (at == f.sp - 2) { expect(c.vectors == 1 && word >= 0x703 && word < 0x71c, "Hardware interrupts inside the CCD leaf, not after its return"); c.return_pc = word; }
			expect(word == saved_repeat_word(f, f.sp - at), "Calls hardware entry and software saves publish the balanced frame"); ++c.stack_writes; return;
		}
		if (stlm && !c.controls)
		{
			if (at != 7 || word != 0x4900) osd_printf_info("STLM suspension mismatch: case=%u at=%04x word=%04x pc=%06x\n", m_pipeline_peripheral_case, unsigned(at), word, unsigned(m_cpu->state_int(STATE_GENPC)));
			expect(at == 7 && word == 0x4900, "STLM publishes the computed status with BRAF clear once"); ++c.controls; return;
		}
		unsigned const control = c.controls - unsigned(stlm);
		std::array<u16, 6> const registers{0x1a, 0x1c, 0x1b, 0x1a, 7, 6};
		std::array<u16, 6> const words{u16(f.inner), f.end(), 0x604, f.saved_count(), f.saved_st1(), 0xa608};
		expect(control < registers.size() && at == registers[control] && word == words[control], "Inner setup then firmware pops restore REA RSA BRC before ST1"); ++c.controls;
		if (control == 3) c.restore_cycle = m_cpu->pipeline_cycles();
		if (control == 4) c.st1_cycle = m_cpu->pipeline_cycles();
	});
	m_vector_prefetch_tap = program.install_read_tap(f.base + f.vector(), f.base + f.vector(), "saved_repeat_vector", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_saved_repeat_counts; expect(c.assertions == 1 && !c.vectors && word == 0x4a1e, "One physical request reaches its real vector"); ++c.vectors; c.vector_cycle = m_cpu->pipeline_cycles(); m_cpu->set_input_line(f.nmi ? INPUT_LINE_NMI : 0, CLEAR_LINE);
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x703, f.base + 0x71b, "saved_repeat_resumed_leaf", [this, f, stlm](offs_t at, u16 &, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_saved_repeat_counts;
		if (c.controls == 6 + unsigned(stlm) && c.leaves == 1 && at == f.base + c.return_pc) { expect(!c.resumed, "FRETE fetches its retained outside-body continuation once"); ++c.resumed; c.resume_cycle = m_cpu->pipeline_cycles(); }
	});
	m_phase = 255800; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_far_save_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 786 || m_phase == 787)
	{
		const unsigned continuation = m_far_save_return ? 0x010606 : 0x010605;
		expect(m_cpu->state_int(STATE_GENPC) == continuation &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 7 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 5 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"uninterrupted and restored pending far transfer complete identically");
		if (m_phase == 786)
		{
			m_cpu->set_state_int(STATE_GENPC, 0x7f0900);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0500);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 99);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 88);
			data.write_word(0x03fe, 0x7f);
			data.write_word(0x03ff, 0xbad);
			m_saved_repeat.clear();
			m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE,
				"restore pending far-transfer save state");
			expect(m_cpu->state_int(STATE_GENPC) == (m_far_save_return ? 0x030602 : 0x010603) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
				data.read_word(0x03fe) == 1 && data.read_word(0x03ff) == 0x0604,
				"restore exact extended checkpoint registers and stack RAM");
			m_phase = 787;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (!m_far_save_return)
		{
			start_far_save_case(true);
			return true;
		}
		osd_printf_info("TMS320C54x extended save replay: PASS pending_call pending_return xpc stack delay\n");
		program.write_word(0x010800, 0xec02); // RPT #2
		program.write_word(0x010801, 0x7f90); // WRITA *AR0+
		program.write_word(0x010802, 0xec02);
		program.write_word(0x010803, 0x7e90); // READA *AR0+
		program.write_word(0x010804, 0xf4e1);
		data.write_word(0x0500, 0x1234);
		data.write_word(0x0501, 0x5678);
		data.write_word(0x0502, 0x9abc);
		for (unsigned address = 0x0503; address != 0x0506; ++address)
			data.write_word(address, 0);
		program.write_word(0x00fffe, 0xaaaa);
		program.write_word(0x00ffff, 0xbbbb);
		program.write_word(0x000000, 0xcccc);
		program.write_word(0x020000, 0xdddd);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0500);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x02fffe);
		m_cpu->set_state_int(STATE_GENPC, 0x010800);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 788;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 788)
	{
		expect_opcode(0x7f90,
			program.read_word(0x02fffe) == 0x1234 &&
			program.read_word(0x02ffff) == 0x5678 &&
			program.read_word(0x030000) == 0x9abc &&
			data.read_word(0x0503) == 0x1234 &&
			data.read_word(0x0504) == 0x5678 &&
			data.read_word(0x0505) == 0x9abc,
			"repeated extended WRITA/READA cross a page boundary without truncating PAR");
		expect(program.read_word(0x00fffe) == 0xaaaa &&
			program.read_word(0x00ffff) == 0xbbbb &&
			program.read_word(0x000000) == 0xcccc &&
			program.read_word(0x020000) == 0xdddd &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x02fffe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0506 &&
			m_cpu->state_int(STATE_GENPC) == 0x010805 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"extended repeated program transfer retains A, executing XPC and wrong-page sentinels");
		osd_printf_info("TMS320C54x extended program conformance: PASS\n");
		program.write_word(0x0600, 0xf495); // NOP in the common window.
		program.write_word(0x0601, 0x7f90); // WRITA *AR0+
		program.write_word(0x0602, 0x7e90); // READA *AR0+
		program.write_word(0x0603, 0xf882); // FB 02:8000 (unique upper window).
		program.write_word(0x0604, 0x8000);
		program.write_word(0x028000, 0xe909);
		program.write_word(0x028001, 0xf4e1);
		program.write_word(0x020600, 0xf4e1); // Wrong-page fetch sentinel.
		program.write_word(0x020007, 0xaaaa);
		data.write_word(0x0500, 0x1234);
		data.write_word(0x0501, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0500);
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0020);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x020007);
		m_cpu->set_state_int(STATE_GENPC, 0x020600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 60000;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 60000)
	{
		expect(program.read_word(7) == 0x1234 && program.read_word(0x020007) == 0xaaaa &&
			data.read_word(0x0501) == 0x1234 && m_cpu->state_int(tms320c54x_device::STATE_B) == 9 &&
			m_cpu->state_int(STATE_GENPC) == 0x028002 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"OVLY folds CPU fetch and accumulator transfers while preserving logical XPC and upper pages");
		osd_printf_info("TMS320C54x program overlay conformance: PASS\n");
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
		program.write_word(0x0600, 0xf4a0);
		program.write_word(0x0601, 0xf4e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1abc);
		m_cpu->set_state_int(STATE_GENPC, 0x0600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 60001;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase >= 60001 && m_phase <= 60008)
	{
		unsigned const arp = m_phase - 60001;
		expect_opcode(0xf4a0 | arp,
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x1abc | (arp << 13)) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"LD immediate ARP selects all eight pointers and preserves unrelated ST0 fields");
		if (arp != 7)
		{
			program.write_word(0x0600, 0xf4a0 | (arp + 1));
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1abc);
			m_cpu->set_state_int(STATE_GENPC, 0x0600);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x immediate ARP conformance: PASS variants=8\n");
		program.write_word(0x0600, 0x46f8);
		program.write_word(0x0601, 0x0500);
		program.write_word(0x0602, 0xf4e1);
		data.write_word(0x0500, 0xffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xfe00);
		m_cpu->set_state_int(STATE_GENPC, 0x0600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 60010;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase >= 60010 && m_phase <= 60012)
	{
		u16 const values[] = {0xffff, 0x0200, 0x0155};
		unsigned const index = m_phase - 60010;
		expect_opcode(0x46f8,
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xfe00 | (values[index] & 0x01ff)) &&
			m_cpu->state_int(STATE_GENPC) == 0x0603 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"LD absolute Smem,DP consumes its address extension and preserves unrelated ST0 fields");
		if (index != 2)
		{
			data.write_word(0x0500, values[index + 1]);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xfe00);
			m_cpu->set_state_int(STATE_GENPC, 0x0600);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x memory DP conformance: PASS variants=3\n");
		program.write_word(0x0600, 0x75d6);
		program.write_word(0x0601, 0x0124);
		program.write_word(0x0602, 0xfa47);
		program.write_word(0x0603, 0x060c);
		program.write_word(0x0604, 0xe801); // Changes A after the branch predicate is captured.
		program.write_word(0x0605, 0xe902);
		program.write_word(0x0606, 0xe903);
		program.write_word(0x0607, 0x75d6);
		program.write_word(0x0608, 0x0124);
		program.write_word(0x0609, 0xf4e1);
		program.write_word(0x060c, 0x75d6);
		program.write_word(0x060d, 0x0124);
		program.write_word(0x060e, 0xf4e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x8000000000ULL);
		m_cpu->set_state_int(STATE_GENPC, 0x0600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 60020;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_saved_repeat_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255800)
	{
		unsigned const stlm = !strcmp(machine().options().bios(), "repeat-stlm");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = saved_repeat_case(variant); auto const &c = m_saved_repeat_counts;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Software-saved callback repeat and architectural resumption remain admitted");
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const checkpoint = f.snapshot == 0 ? tick == 4 : f.snapshot == 1 ? c.vectors && cycle >= c.vector_cycle + 2 :
			f.snapshot == 2 ? c.inner_writes && cycle >= c.inner_cycle + 2 : f.snapshot == 3 ? c.controls >= 4 + stlm && cycle >= c.restore_cycle + 2 :
			f.snapshot == 4 ? c.controls >= 5 + stlm && cycle >= c.st1_cycle + 1 : c.resumed && cycle >= c.resume_cycle + 2;
		if (checkpoint && !m_pipeline_replay && m_saved_repeat_checkpoint == 0)
		{
			m_saved_repeat_checkpoint = tick; m_saved_repeat_snapshot = c; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save actual IRQ repeat state after queued input edges settle");
		}
		if (m_cpu->state_int(tms320c54x_device::STATE_IDLE) || tick == 5000)
		{
			unsigned const iterations = f.count + 1;
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || c.outer_writes != iterations)
				osd_printf_info("Saved-repeat mismatch: case=%u replay=%u pc=%06x brc=%04x rsa=%04x rea=%04x st1=%04x outer=%u inner=%u leaves=%u controls=%u stack_reads=%u stack_writes=%u\n",variant,unsigned(m_pipeline_replay),unsigned(m_cpu->state_int(STATE_GENPC)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_RSA)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_REA)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)),c.outer_writes,c.inner_writes,c.leaves,c.controls,c.stack_reads,c.stack_writes);
			expect(m_saved_repeat_checkpoint && c.outer_reads == iterations && c.outer_writes == iterations && c.leaves == iterations && c.inner_writes == f.inner + 1 &&
				c.assertions == 1 && c.vectors == 1 && c.mapped_reads == 6 + stlm && c.stack_reads == iterations + 7 && c.stack_writes == iterations + 7 && c.controls == 6 + stlm && c.resumed == 1,
				"One IRQ owns one saved context and inner loop; all outer calls and frames settle exactly");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end() + 3 && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.sp &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 1 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2345 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x100 &&
				!m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end() &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7101 + f.inner &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + (f.after_end ? 0 : iterations) && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1701,
				"Firmware-restored loop registers resume the leaf then the outer endpoint without a hidden loop stack");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_saved_repeat_checkpoint; m_saved_repeat_counts = m_saved_repeat_snapshot;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore software-saved repeat state without resubmitting physical input");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255810; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3839) { start_saved_repeat(variant + 1); return true; }
			if (stlm)
			{
				osd_printf_info("TMS320C54x STLM-suspended IRQ repeat: PASS cases=3840 restore=3840 outer_counts=0,1,2,7,56 inner_counts=0,1,3,63 sources=IRQ,NMI pages=2 stacks=2 returns=FRETE,FRETED snapshots=6 status_clear=Execute balanced_frames=1 physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated STLM-suspended IRQ repeat complete");
			}
			osd_printf_info("TMS320C54x software-saved IRQ repeat: PASS cases=3840 restore=3840 outer_counts=0,1,2,7,56 inner_counts=0,1,3,63 sources=IRQ,NMI pages=2 stacks=2 returns=FRETE,FRETED snapshots=6 leaf_return=inside,REA_plus_1 ordered_copies=1 balanced_frames=1 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated software-saved IRQ repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255810) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255800; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
