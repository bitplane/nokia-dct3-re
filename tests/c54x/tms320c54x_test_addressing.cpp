// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_circular_move_schedule.h"

namespace c54x_test {

















unsigned tms320c54x_test_state::circular_fixture_mask(u16 bk)
{
	unsigned mask = 1;
	while (mask < bk) mask = (mask << 1) | 1;
	return mask;
}

bool tms320c54x_test_state::circular_fixture_domain(u16 initial, int step, u16 bk)
{
	return bk && (initial & circular_fixture_mask(bk)) < bk && step >= -int(bk) && step <= int(bk);
}

u16 tms320c54x_test_state::circular_fixture_next(u16 initial, int step, u16 bk)
{
	// SPRU131G 5.5.3.4: modulo is an oracle only inside the documented nonzero-BK domain.
	if (!circular_fixture_domain(initial, step, bk))
		throw emu_fatalerror(1, "Circular fixture outside documented domain: AR=%04x step=%d BK=%u", initial, step, bk);
	unsigned const mask = circular_fixture_mask(bk);
	int const index = (int(initial & mask) + step) % int(bk);
	return (initial & ~mask) | (index < 0 ? index + bk : index);
}

void tms320c54x_test_state::start_pipeline_store_mvmm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const accumulator = profile_is("st-acc-mvmm");
	unsigned const count = 1 + variant % 2, source = (variant / 2) % 8, destination = (variant / 16) % 8;
	unsigned const width = accumulator ? 1 : 2, form=(variant/1536)%4;
	u16 const stored=std::array<u16,4>{0,1,0x8000,0xffff}[variant/6144];
	unsigned const gap = (variant / 128) % 2, read_cycle = 3 + width * count + gap;
	u32 const base = (variant / 256) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	for (unsigned i = 0; i < count; ++i)
	{
		program.write_word(base + 0x600 + width * i, (accumulator ? 0x8090|(form<<8) : 0x7690) | source);
		if (!accumulator) program.write_word(base + 0x601 + 2 * i, i ? 0 : 0x3532);
	}
	program.write_word(base + 0x600 + width * count + gap, 0xe700 | (source << 4) | destination);
	program.write_word(base + 0x601 + width * count + gap, 0x1080 | destination);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 0},
		{cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_A, u64(stored)<<(form&2 ? 16 : 0)},
		{cpu_regs::STATE_B, u64(stored)<<(form&2 ? 16 : 0)},
	});
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x1400);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + source, 0x1200);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 3; ++i) m_cpu->space(AS_DATA).write_word(0x1200 + i, 0x55aa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "store_mvmm_read",
		[this, source, count, read_cycle, accumulator, gap](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			if (at == 0x10 + source)
			{
				expect((!accumulator || gap) && word == 0x1200 + count && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read_cycle,
					"MVMM reads the committed AR only when no internal DAGEN bypass applies"); ++m_pipeline_operand_reads;
			}
			else
			{
				expect(at == 0x1200 + count && word == 0x55aa && !m_pipeline_extensions_reads,
					"Following load uses the retained MVMM destination"); ++m_pipeline_extensions_reads;
			}
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "store_mvmm_write",
		[this, destination, count, read_cycle, accumulator, stored](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			if (at == 0x10 + destination)
				expect(word == 0x1200 + count && m_cpu->pipeline_cycles() == read_cycle + 1,
					"MVMM publishes its retained sample in Access");
			else
				expect(at >= 0x1200 && at < 0x1200 + count && word == (accumulator ? stored : at == 0x1200 ? 0x3532 : 0) &&
					m_cpu->pipeline_cycles() == (accumulator ? 6 + (at-0x1200) : 7 + 2 * (at - 0x1200)), "Stores retain original addresses and publish once in Execute");
			++m_pipeline_store_writes;
		});
	m_phase = 600001; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}









tms320c54x_test_state::extended_banz_fixture tms320c54x_test_state::extended_banz_case(unsigned variant)
{
	unsigned const v = variant % 768, form = v % 3, reg = (v / 3) % 8, pattern = (v / 24) % 8;
	bool const compatibility = (v / 192) % 2;
	// Circular inputs must begin inside the queue and admit the signed step.
	// Keep linear 16-bit wrap cases separate from legal circular wrap cases.
	u16 const initial = form == 2 ? std::array<u16, 8>{0, 1, 0x7fff, 0, 1, 0x0234, 7, 0}[pattern] :
		std::array<u16, 8>{0, 1, 0xffff, 0, 1, 0x1234, 7, 0x8000}[pattern];
	s16 const offset = std::array<s16, 8>{0, -1, 1, 1, 0, s16(0xf495), 17, s16(0x8000)}[pattern];
	u16 const bk = form == 2 ? 0x8000 : 16;
	u16 const resolved = form == 2 ? circular_fixture_next(initial, offset, bk) : u16(initial + offset);
	return {form, reg, compatibility && !reg ? 3U : reg, initial, u16(offset), resolved, form ? resolved : initial,
		(v / 384) % 2 ? 0x20000U : 0U, bk};
}




tms320c54x_test_state::memory_repeat_fixture tms320c54x_test_state::memory_repeat_case(unsigned variant)
{
	if (variant >= 38400)
	{
		unsigned const v = (variant - 38400) % 9600, pattern = (v / 10) % 4;
		bool const long_form = v % 2; unsigned const form = std::array<unsigned, 5>{0, 1, 2, 3, 5}[(v / 1920) % 5];
		u32 const base = (v / 120) % 2 ? 0x20000 : 0;
		return {long_form, false, std::array<unsigned, 5>{0, 1, 15, 32, 255}[(v / 2) % 5], (v / 40) % 3, (v / 240) % 8, form, pattern, base,
			base + (long_form ? 0x602 : 0x601), u16(0x7600 | std::array<u16, 6>{2, 2, 0x91, 0x81, 0x99, 0x89}[form]),
			std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern], 0};
	}
	unsigned const v = variant % 9600;
	bool const long_form = v % 2, destination = (v / 10) % 2;
	unsigned const count = std::array<unsigned, 5>{0, 1, 15, 32, 255}[(v / 2) % 5], pattern = (v / 20) % 4;
	unsigned const interrupt = (v / 80) % 3, flags = (v / 480) % 4;
	// Short MOD=3 is write-only; preserve the other five source forms.
	unsigned const mode_form = std::array<unsigned, 5>{0, 1, 2, 3, 5}[v / 1920];
	u32 const base = (v / 240) % 2 ? 0x20000 : 0;
	u16 const mode = std::array<u16, 6>{2, 2, 0x91, 0x81, 0x99, 0x89}[mode_form];
	return {long_form, destination, count, interrupt, flags, mode_form, pattern, base,
		base + (long_form ? 0x602 : 0x601), u16(0x1e00 | (destination << 8) | mode),
		std::array<u16, 4>{7, 1, 0x1234, 0xffff}[pattern],
		std::array<u64, 4>{0x41, 0x7fff, 0x12345678, 0x1fffffff}[pattern]};
}

void tms320c54x_test_state::start_pipeline_memory_repeat(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = memory_repeat_case(variant); unsigned const last = (f.long_form ? 8 : 7) + f.count;
	bool const store = variant >= 38400;
	unsigned const final_write = (f.long_form ? 9 : 8) + 2 * f.count;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.long_form ? 0xf070 : 0xec00 | f.count);
	if (f.long_form) program.write_word(f.base + 0x601, f.count);
	program.write_word(f.body, f.opcode);
	if (store) program.write_word(f.body + 1, f.divisor);
	program.write_word(f.body + (store ? 3 : 2), 0xf4e1);
	if (f.interrupt) { u32 const vector = f.base + (f.interrupt == 2 ? 4 : 0x40); program.write_word(vector, 0x4a1e); program.write_word(vector + 1, 0xf495); program.write_word(vector + 2, 0xf4e5); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, store && (f.flags & 4) ? 1 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1804);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (f.interrupt == 1 ? 0 : 0x800) | (f.flags & 1 ? 0x100 : 0) | (f.flags & 2 ? 0x200 : 0) | (f.form == 1 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, f.form == 4 ? 0x1ff : f.form == 5 ? 0x400 : 0x200);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.destination ? 0x1234 : f.dividend); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.destination ? f.dividend : 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.interrupt == 1 ? 1 : 0);
	for (unsigned at = 0x100; at <= 0x400; ++at) m_cpu->space(AS_DATA).write_word(at, f.divisor);
	m_cpu->space(AS_DATA).write_word(0x1002, f.divisor);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = m_vector_acks = m_pipeline_store_case = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x100, 0x1002, "repeat_subc_reads",
		[this, f, store, final_write](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at >= 0xffe && at <= 0xfff && f.interrupt) { expect(m_cpu->pipeline_cycles() > (store ? final_write : (f.long_form ? 8U : 7U) + f.count), "Interrupt stack reads follow the final repeated operation"); return; }
			if (store) { expect(false, "Repeated immediate stores do not read their destination"); return; }
			u16 const address = f.form == 0 ? 0x202 : f.form == 1 ? 0x1002 : f.form == 5 ? 0x400 - m_pipeline_operand_reads : f.form == 2 || f.form == 4 ? 0x200 + m_pipeline_operand_reads : 0x200;
			unsigned const first = f.long_form ? 8 : 7;
			expect(at == address && word == f.divisor && m_pipeline_operand_reads <= f.count && m_cpu->pipeline_cycles() == (!m_pipeline_operand_reads ? first - 1 : first + m_pipeline_operand_reads), "Repeated SUBC reads each divisor through the iteration-owned address"); ++m_pipeline_operand_reads; });
	m_vector_prefetch_tap = program.install_read_tap(f.body, f.body, "repeat_subc_body",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == f.opcode && !m_pipeline_extensions_reads, "Repeated SUBC fetches its opcode once"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "repeat_subc_stack_only",
		[this, f, last, store, final_write](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (store && !(at >= 0xffe && at <= 0xfff))
			{
				u16 const address = f.form == 0 ? 0x202 : f.form == 1 ? 0x1002 : f.form == 5 ? 0x400 - m_pipeline_operand_reads : f.form == 2 ? 0x200 + m_pipeline_operand_reads : 0x200;
				expect(at == address && word == f.divisor && m_pipeline_operand_reads <= f.count && m_cpu->pipeline_cycles() == (f.long_form ? 9U : 8U) + 2 * m_pipeline_operand_reads, "Repeated ST writes the retained literal once at each two-clock destination"); ++m_pipeline_operand_reads; return;
			}
			expect(f.interrupt && m_cpu->pipeline_cycles() > (store ? final_write : last) && at >= 0xffe && at <= 0xfff && m_pipeline_store_writes < 2, "Deferred interrupt owns only post-repeat stack writes"); ++m_pipeline_store_writes; });
	if (store)
		m_pipeline_fetch_tap = program.install_read_tap(f.body + 1, f.body + 1, "repeat_store_literal",
			[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == f.divisor && !m_pipeline_store_case, "Repeated immediate store retains its literal once"); ++m_pipeline_store_case; });
	m_phase = 253230; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}










tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_memory_square_case(unsigned variant)
{
	auto f = delay_square_case(variant % 36 + (variant / 36) * 72);
	f.square = false; f.memory_square = true;
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}













void tms320c54x_test_state::start_long_mapped_original(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_cpu->set_pipeline_frontend(false); m_pipeline_peripheral_case = variant;
	unsigned v = variant;
	u16 const length = std::array<u16,8>{12,0,6,7,8,13,21,143}[v % 8]; v /= 8;
	u16 const stale = std::array<u16,4>{0xff84,0,7,0xffff}[v % 4]; v /= 4;
	bool const sxm = !(v % 2); v /= 2;
	u32 const base = v % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	// Unchanged AA88 03:bdb6..bdc2, including the dependent double load.
	std::array<u16,13> const source{0x1106,0x08f8,0x000b,0xf843,0xbdd2,0xf640,0x10f8,0x0008,0x4e0a,0x560a,0x7002,0x0011,0x4e00};
	for (unsigned i=0; i<source.size(); ++i) program.write_word(base+0xbdb6+i,source[i]);
	program.write_word(base+0xbdc3,0xf4e1); program.write_word(base+0xbdd2,0xf4e1);
	data.write_word(0x1406,7); data.write_word(0x140a,variant?0x7777:0); data.write_word(0x140b,variant?0x8888:0x88);
	data.write_word(0x1400,0xaaaa); data.write_word(0x1401,0xbbbb); data.write_word(0x1402,0xcccc);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,0x4800 | (sxm ? 0x100 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_A,length); m_cpu->set_state_int(tms320c54x_device::STATE_B,stale);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1,3);
	m_pipeline_operand_reads=m_pipeline_extensions_reads=m_pipeline_register_writes=m_pipeline_store_writes=m_pipeline_drained_fetches=0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"long_mapped_original_read",[this](offs_t at,u16 &value,u16) {
		if (machine().side_effects_disabled()) return;
		if (at==0x1406) { expect(value==7 && !m_pipeline_extensions_reads++,"Original escape reads its threshold once"); return; }
		if (at==0x11) { expect(value==3 && m_pipeline_extensions_reads==1,"Original escape copies its width once"); ++m_pipeline_extensions_reads; return; }
		if (at==0x140a || at==0x140b) { ++m_pipeline_drained_fetches; osd_printf_info("long_mapped_original_backing_read: address=%04x value=%04x\n",unsigned(at),value); return; }
		expect(at==8 || at==0x0b,"Original escape reads only mapped AL/BL after its threshold");
		if (at==8) ++m_pipeline_register_writes; else ++m_pipeline_operand_reads;
		if (value!=7) { ++m_pipeline_drained_fetches; osd_printf_info("long_mapped_original_read: case=%u address=%04x actual=%04x expected=0007\n",m_pipeline_peripheral_case,unsigned(at),value); }
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"long_mapped_original_write",[this](offs_t at,u16 &,u16) {
		if (machine().side_effects_disabled()) return;
		expect(at==0x140a || at==0x140b || (at>=0x1400 && at<=0x1402),"Original escape publishes only its local value and outgoing arguments"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=399940;
	m_cpu->set_state_int(STATE_GENPC,base+0xbdb6); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

tms320c54x_test_state::popd_fixture tms320c54x_test_state::popd_case(unsigned v, bool repeated)
{
	popd_fixture f{}; f.repeated = repeated;
	unsigned const spec = v % 17, form = repeated ? (spec < 15 ? spec / 3 : spec - 10) : (v / 84) % 3;
	unsigned const pattern = repeated ? (v / 17) % 4 : (v / 21) % 4;
	f.prefix = repeated ? 1 + (v / 68) % 2 : 0;
	f.cmpt = repeated ? (v / 136) % 2 : (v / 252) % 2;
	f.base = (repeated ? (v / 272) % 2 : (v / 504) % 2) ? 0x20000 : 0;
	f.irq = repeated ? (v / 544) % 3 : 0;
	f.count = !repeated ? 0 : spec < 15 ? std::array<unsigned, 3>{0, 1, 15}[spec % 3] : spec == 15 ? 4 : 11;
	f.pointer = !repeated ? (form ? popd_fixture::reg(v % 21) : 0x7777) : std::array<u16, 7>{0x4000, 0x400f, 0x4020, 0x3001, 0x2fff, 0x1d, 0x16}[form];
	f.mode = !repeated ? (form ? form == 1 ? 0x97 : 0x8f : popd_fixture::reg(v % 21)) : form == 2 ? 0x20 : form == 1 || form >= 5 ? 0x8f : 0x97;
	f.step = BIT(f.mode, 7) ? (f.mode & 0x78) == 0x08 ? -1 : 1 : 0;
	f.st0 = 0xa600 | (repeated && form == 2 ? 0x80 : 0);
	f.st1 = 0x100 | (f.cmpt ? 0x20 : 0) | (f.irq == 1 ? 0 : 0x800);
	for (unsigned i = 0; i <= f.count; ++i)
	{
		f.destination[i] = !repeated ? popd_fixture::reg(v % 21) : f.pointer + i * f.step;
		f.word[i] = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern] ^ (i * 0x111);
	}
	unsigned const checkpoint = repeated ? v / 1632 : v / 1008;
	f.snapshot = std::array<unsigned, 4>{3 + f.prefix, 4 + f.prefix, f.read_clock() + (f.count + 1) / 2, f.read_clock() + f.count + 1}[checkpoint];
	return f;
}

void tms320c54x_test_state::start_popd_mapped(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_popd_fixture = popd_case(variant, profile_is("popd-repeat")); auto &f = m_popd_fixture;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	// PMST restoration may select the common lower program window.
	for (u32 const base : {0U, 0x20000U})
	{
		for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
		if (f.prefix) program.write_word(base + 0x600, f.prefix == 2 ? 0xf070 : 0xec00 | f.count);
		if (f.prefix == 2) program.write_word(base + 0x601, f.count);
		program.write_word(base + 0x600 + f.prefix, f.opcode()); program.write_word(base + 0x601 + f.prefix, 0xf4e1);
		u16 const pmst = f.repeated && f.destination[0] == 0x1d ? f.word[0] : 0;
		program.write_word(base + (pmst & 0xff80) + (f.irq == 2 ? 4 : 0x40), 0xf4eb);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	for (unsigned i = 0; i < f.initial.size(); ++i) data.write_word(popd_fixture::reg(i), 0x2200 + i);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_AR7, f.pointer);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq == 1 ? 1 : 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	for (unsigned i = 0; i <= f.count; ++i) if (f.destination[i] >= 0x60) data.write_word(f.destination[i], 0xa55a);
	for (unsigned i = 0; i <= f.count; ++i) data.write_word(0x3000 + i, f.word[i]);
	for (unsigned i = 0; i < f.initial.size(); ++i) f.initial[i] = data.read_word(popd_fixture::reg(i));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_port_reads = m_fast_stack_reads = m_pipeline_register_writes = m_vector_acks = m_popd_write_mask = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "popd_mapped_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.irq && m_pipeline_operand_reads == f.count + 1 && at == 0x3000 + f.count) { expect(!m_fast_stack_reads && word == 0x601 + f.prefix, "RETE consumes only the completed POPD repeat's interrupt frame"); ++m_fast_stack_reads; return; }
		unsigned const i = m_pipeline_operand_reads;
		if (i > f.count || at != 0x3000 + i || word != f.word[i] || m_cpu->pipeline_cycles() != f.read_clock(i)) osd_printf_info("popd_read: case=%u replay=%u i=%u at=%04x word=%04x cycle=%llu\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), i, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles());
		expect(i <= f.count && at == 0x3000 + i && word == f.word[i] && m_cpu->pipeline_cycles() == f.read_clock(i), "POPD consumes each stack word once before the preceding DARAM write"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "popd_mapped_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.irq && m_pipeline_store_writes == f.count + 1 && at == 0x3000 + f.count) { expect(!m_pipeline_register_writes && word == 0x601 + f.prefix, "IRQ reserves one frame only after every POPD publication"); ++m_pipeline_register_writes; return; }
		for (unsigned i = 0; i <= f.count; ++i)
			if (!(m_popd_write_mask & (1U << i)) && at == f.destination[i] && word == f.word[i] && m_cpu->pipeline_cycles() == f.write_clock(i)) { m_popd_write_mask |= 1U << i; ++m_pipeline_store_writes; return; }
		osd_printf_info("popd_write: case=%u replay=%u at=%04x word=%04x cycle=%llu mask=%x\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles(), m_popd_write_mask);
		expect(false, "POPD publishes a retained word once at its destination's Read/Execute stage");
	});
	m_pipeline_fetch_tap = program.install_read_tap(0, 0x7fffff, "popd_mapped_fetch", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.base + 0x600 + f.prefix || at == 0x600 + f.prefix) { expect(!m_pipeline_extensions_reads && word == f.opcode(), "POPD fetches one body opcode even during repeat or PMST mapping changes"); ++m_pipeline_extensions_reads; }
		else if (f.prefix == 2 && (at == f.base + 0x601 || at == 0x601)) { expect(!m_port_reads && word == f.count, "Long POPD repeat retains one count literal"); ++m_port_reads; }
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254200;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_popd_edges(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const conflict = variant >= 15, active = variant >= 8 && variant < 12;
	unsigned const c = conflict ? variant - 15 : 0;
	u32 const base = conflict && c >= 8 ? 0x20000 : 0;
	u16 const destination = conflict ? std::array<u16, 4>{0x10, 0x12, 0x17, 0x19}[c % 4] : variant < 8 ? std::array<u16, 8>{0, 1, 6, 7, 0x18, 0x1e, 0x24, 0x58}[variant] : active ? 0x1a + variant - 8 : 0x4000;
	u16 const writer = (c / 4) % 2 ? destination : destination == 0x12 ? 0x11 : 0x12;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	if (conflict) { program.write_word(base + 0x600, 0x8000 | writer); program.write_word(base + 0x601, 0x8b97); program.write_word(base + 0x602, 0xf4e1); }
	else if (active) { program.write_word(0x600, 0xf072); program.write_word(0x601, 0x603); program.write_word(0x602, 0x8b97); }
	else if (variant == 12) { program.write_word(0x600, 0x47f8); program.write_word(0x601, 0x100); program.write_word(0x602, 0x8b97); data.write_word(0x100, 1); }
	else program.write_word(0x600, variant == 14 ? 0x8ba0 : 0x8b97);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	if (conflict) data.write_word(destination, 0x2222);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, variant == 13 ? 0xe : 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_AR7, destination); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1357);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 1); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); data.write_word(0x3000, 0x2468);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_popd_write_mask = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "popd_edge_read", [this, conflict, variant](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (!conflict && variant == 12 && at == 0x100) return;
		expect(conflict && at == 0x3000 && word == 0x2468 && m_cpu->pipeline_cycles() == 6 && !m_pipeline_operand_reads, "POPD rejects unsupported contracts before stack consumption and retains the conflict source once"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "popd_edge_write", [this, conflict, destination, writer](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const bit = m_cpu->pipeline_cycles() == 6 && at == writer && word == 0x1357 ? 1 : m_cpu->pipeline_cycles() == 7 && at == destination && word == 0x2468 ? 2 : 0;
		expect(conflict && bit && !(m_popd_write_mask & bit), "POPD defers its Read-owned DAGEN write behind the older store's bank access"); m_popd_write_mask |= bit; ++m_pipeline_store_writes;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254220;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}



tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_load_case(unsigned v, bool shifted)
{
	delay_load_fixture f{};
	if (shifted)
	{
		f.kind = v % 6; f.form = (v / 6) % 3; f.destination = (v / 18) % 2; f.flags = (v / 4608) % 4;
		unsigned const raw = (v / 36) % 32; int const shift = raw < 16 ? int(raw) : int(raw) - 32;
		f.literal = 0x0c40 | (f.destination << 8) | raw; f.address = 0x1002;
		f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 1152) % 4];
		f.st0 = f.final_st0 = 0xa620; f.st1 = 0x800 | (f.flags & 1 ? 0x100 : 0) | (f.flags & 2 ? 0x200 : 0);
		f.caller = (v / 18432) % 2 ? 0x20000 : 0; f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
		f.a = f.kind == 1 ? 0 : f.kind == 2 ? 1 : 0x123456789aULL; f.b = 0xfedcba9876ULL;
		s64 const input = f.flags & 1 ? s64(s16(f.memory)) : s64(f.memory);
		s64 const divisor = s64(1) << (shift < 0 ? -shift : 0);
		f.result = u64(shift >= 0 ? input * (s64(1) << shift) : input / divisor - (input < 0 && input % divisor != 0)) & 0xffffffffffULL;
		for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x5100 + i;
		f.initial[3] = 0x1002; f.final = f.initial; if (f.form == 2) ++f.final[3];
		f.read = delay_multiply_fixture::operand_read(f.kind, 1); f.checkpoint = std::array<unsigned, 3>{f.literal_clock(), f.read - 1, f.read}[v / 36864];
		return f;
	}
	f.kind = v % 6; f.form = (v / 6) % 4; f.ar = (v / 24) % 8; f.destination = (v / 384) % 2; f.flags = (v / 3072) % 4; f.compatibility = (v / 12288) % 2;
	f.selected = f.compatibility && !f.ar ? 5 : f.ar; bool const negative = (v / 192) % 2;
	f.literal = f.form == 3 ? negative ? 0x5004 : 0x5006 : negative ? 0xffff : 1; f.address = negative ? 0x5004 : 0x5006;
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 768) % 4];
	f.st0 = 0xa608; f.final_st0 = f.compatibility && f.ar && f.form != 3 ? (f.st0 & 0x1fff) | (f.ar << 13) : f.st0;
	f.st1 = 0x800 | (f.flags & 1 ? 0x100 : 0) | (f.flags & 2 ? 0x200 : 0) | (f.compatibility ? 0x20 : 0);
	f.caller = (v / 24576) % 2 ? 0x20000 : 0; f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
	f.a = f.kind == 1 ? 0 : f.kind == 2 ? 1 : 0x123456789aULL; f.b = 0xfedcba9876ULL;
	f.result = u64(f.flags & 1 ? s64(s16(f.memory)) : s64(f.memory)) & 0xffffffffffULL;
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x5100 + i;
	f.initial[f.selected] = 0x5005; f.final = f.initial;
	if (f.form == 1 || f.form == 2) f.final[f.selected] = f.address;
	f.read = delay_multiply_fixture::operand_read(f.kind, 1); f.checkpoint = std::array<unsigned, 3>{f.literal_clock(), f.read - 1, f.read}[v / 49152];
	return f;
}

void tms320c54x_test_state::start_delay_load(unsigned variant)
{
	bool const shifted = !strncmp(machine().options().bios(), "shift-delay", 11);
	bool const shift_alu = profile_is("shift-alu-delay");
	bool const add = profile_is("add-delay");
	bool const dld = profile_is("dld-delay");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = shift_alu ? delay_shift_alu_case(variant) : dld ? delay_dld_case(variant) : add ? delay_add_case(variant) : delay_load_case(variant, shifted);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.caller + 0x600 + i, 0xf495);
	program.write_word(f.caller + 0x600, f.kind == 4 ? 0xfb80 | (f.target >> 16) : std::array<u16, 6>{0xf273, 0xfa45, 0xfa45, 0xf274, 0, 0xfe00}[f.kind]);
	if (f.kind != 5) program.write_word(f.caller + 0x601, 0x700);
	u16 const mode = f.form == 3 ? 0xf8 : (0xe0 + f.form * 8) | f.ar;
	program.write_word(f.caller + 0x600 + f.delay(), add ? 0x00f8 | (f.destination << 8) : shifted || shift_alu ? 0x6f00 | (f.form == 0 ? 2 : f.form == 1 ? 0x83 : 0x93) : (dld ? 0x5600 : 0x1000) | (f.destination << 8) | mode); program.write_word(f.caller + 0x601 + f.delay(), f.literal);
	for (u32 at : {f.caller + 0x604, f.target + 0x700}) { program.write_word(at, 0x880f | (f.destination << 8)); program.write_word(at + 1, 0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2468); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 13);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	data.write_word(0x3000, 0x700); data.write_word(f.address, f.memory);
	if (dld) data.write_word(f.address ^ 1, f.memory_low);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_load_read", [this, f, dld](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x3000) { expect(f.kind == 5 && word == 0x700 && !m_fast_stack_reads, "RETD load delay has one independent return-frame read"); ++m_fast_stack_reads; return; } expect(m_pipeline_operand_reads < (dld ? 2U : 1U) && at == (f.address ^ m_pipeline_operand_reads) && word == (m_pipeline_operand_reads ? f.memory_low : f.memory) && m_cpu->pipeline_cycles() == f.read, "Delayed load retains its ordered RAM operand once at Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_load_write", [this, f, dld](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff || at == 0x2ffe) { unsigned const i = m_pipeline_register_writes; expect((f.kind == 3 || f.kind == 4) && i < (f.kind == 4 ? 2 : 1) && at == 0x2fff - i && word == (i ? f.caller >> 16 : 0x604), "Delayed load does not change its owning call frame"); ++m_pipeline_register_writes; return; } expect(at == 0x0f && word == u16(f.result) && !m_pipeline_store_writes && m_pipeline_operand_reads == (dld ? 2U : 1U) && m_cpu->pipeline_cycles() >= f.read + 2, "Target observes the completed delay load once"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.caller + 0x601 + f.delay(), f.caller + 0x601 + f.delay(), "delay_load_literal", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == f.literal && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == f.literal_clock(), "Delayed LD owns one literal without decoding it as an opcode"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255260;
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}





tms320c54x_test_state::indexed_store_fixture tms320c54x_test_state::circular_load_case(unsigned v)
{
	indexed_store_fixture f{};
	unsigned const operation = v % 4, index = (v / 512) % 4, flags = (v / 8192) % 8;
	f.ar = (v / 4) % 8; f.compatibility = (v / 32) % 2; f.selector = (v / 64) % 2;
	f.selected = f.compatibility && !f.ar ? 4 : f.ar;
	f.bk = std::array<u16, 4>{0, 1, 7, 16}[(v / 128) % 4];
	if (!f.selected && f.bk) f.bk = 0x2000;
	f.mode = 0xc0 | (operation << 3) | f.ar;
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x1000 + i * 0x100;
	// SPRU131G 5.5.3.4 requires |step| <= BK for a nonzero block.
	// A self-indexed RAM pointer therefore needs a correspondingly large BK.
	f.initial[0] = std::array<u16, 4>{0, 1, 0xffff, f.bk ? u16(-f.bk) : u16(0x8000)}[index];
	unsigned const position = !f.bk || !index ? 0 : index == 1 ? f.bk - 1 : index == 2 ? f.bk / 2 : f.bk > 1 ? 1 : 0;
	f.address = index == 3 && (v / 128) % 4 == 1 ? 0x20 : 0x1000 + f.selected * 0x100 + (f.selected ? position : 0);
	f.initial[f.selected] = f.address;
	f.excluded = f.address < 0x60;
	s32 const step = operation & 1 ? s32(s16(f.initial[0])) : 1;
	s32 const delta = operation < 2 ? -step : step;
	if (!f.bk) f.final_ar = f.address + delta;
	else
	{
		// Independent modulo oracle for the selected block sizes; the CPU
		// instead derives an address mask and performs iterative wrapping.
		u16 const mask = f.bk == 1 ? 1 : f.bk == 7 ? 7 : f.bk == 16 ? 31 : 0x3fff;
		s32 const remainder = (s32(f.address & mask) + delta) % f.bk;
		f.final_ar = (f.address & ~mask) | u16(remainder < 0 ? remainder + f.bk : remainder);
	}
	f.word = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 2048) % 4];
	f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (f.compatibility ? 0x20 : 0);
	f.pmst = (flags >> 2) & 1;
	f.st0 = !f.excluded && f.compatibility && f.ar ? 0x608 | (f.ar << 13) : 0x8608;
	f.value = u64(f.st1 & 0x100 ? s64(s16(f.word)) : s64(f.word)) & 0xffffffffffULL;
	f.base = (v / 65536) % 2 ? 0x20000 : 0; f.snapshot = v / 131072;
	return f;
}






tms320c54x_test_state::mapped_dual_fixture tms320c54x_test_state::mapped_dual_case(unsigned variant, bool edge, bool interrupt_registers, bool low_bus)
{
	mapped_dual_fixture f{}; unsigned v = variant;
	unsigned const addresses = edge ? 74 : interrupt_registers ? 2 : low_bus ? 4 : 16;
	unsigned const address = v % addresses; v /= addresses;
	u16 const mapped = edge ? (address < 2 ? address + 6 : 0x18 + address - 2) : interrupt_registers ? address : low_bus ? address + 2 : 8 + address;
	unsigned const lanes = v % 3; v /= 3;
	unsigned const selectors = edge ? 0x89 : std::array<unsigned, 8>{0x01,0x0a,0x89,0x45,0xcd,0x88,0x4c,0xf2}[v % 8];
	if (!edge) v /= 8;
	bool const subtract = v % 2; v /= 2; f.destination = v % 2; v /= 2;
	unsigned const flags = edge ? 0 : v % 4; if (!edge) v /= 4;
	f.bk = !edge && v % 2 ? 32 : 0; if (!edge) v /= 2;
	unsigned const pattern = edge ? 0 : v % 2; if (!edge) v /= 2;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	f.checkpoint = edge ? 3 + v % 2 : std::array<unsigned, 3>{3,5,6}[v % 3];
	unsigned const x = selectors >> 4, y = selectors & 15, xar = 2 + (x & 3), yar = 2 + (y & 3);
	f.initial = {3,0x1700,0x101f,0x111f,0x121f,0x131f,0x1800,0x1900};
	if (lanes != 1) f.initial[xar] = mapped;
	if (lanes != 0) f.initial[yar] = lanes == 2 && xar != yar ? (edge ? (mapped == 0x5f ? 6 : mapped + 1) : interrupt_registers ? (address + 1) % 2 : low_bus ? 2 + (address + 1) % 4 : 8 + (address + 5) % 16) : mapped;
	f.xaddress = f.initial[xar]; f.yaddress = f.initial[yar]; f.final = f.initial;
	auto modify = [&f](u16 pointer, unsigned mode) -> u16 {
		if (mode == 1) return pointer - 1;
		if (mode == 2) return pointer + 1;
		if (mode == 3) return f.bk ? (pointer & 0xffc0) | ((pointer + 3) & 31) : pointer + 3;
		return pointer;
	};
	if (!edge) { f.final[xar] = modify(f.initial[xar], x >> 2); if (xar != yar) f.final[yar] = modify(f.initial[yar], y >> 2); }
	f.a = pattern ? 0x7fffff8001ULL : 0x8055557fffULL;
	f.b = pattern ? 0x8055557fffULL : 0x7fffff8001ULL;
	f.st0 = 0xa002 | (pattern ? 0x800 : 0); f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0);
	if (interrupt_registers) { f.imr = pattern ? 0xa55a : 0; f.ifr = pattern ? 0x5aa5 : 0; }
	auto sample = [&f,pattern](u16 at) -> u16 {
		if (at >= 0x60) return pattern ? 0x7fff : 0x8001;
		if (at < 2) return at ? f.ifr : f.imr;
		if (at < 6) return pattern ? 0xf495 : 0x8001;
		if (at >= 0x10 && at <= 0x17) return f.final[at - 0x10];
		if (at >= 8 && at <= 10) return f.a >> ((at - 8) * 16);
		if (at >= 11 && at <= 13) return f.b >> ((at - 11) * 16);
		return at == 14 ? 0x8001 : 0x7fff;
	};
	f.xword = sample(f.xaddress); f.yword = sample(f.yaddress);
	s64 const left = s64(flags & 1 ? s16(f.xword) : s32(f.xword)) * 65536;
	s64 const right = s64(flags & 1 ? s16(f.yword) : s32(f.yword)) * 65536;
	s64 const result = subtract ? left - right : left + right;
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	f.result = u64(flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	f.result_st0 = (f.st0 & ~0x800) | ((subtract ? f.xword >= f.yword : u32(f.xword) + f.yword > 65535) ? 0x800 : 0) | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	f.opcode = 0xa000 | (subtract ? 0x200 : 0) | (f.destination << 8) | selectors;
	return f;
}

void tms320c54x_test_state::start_mapped_dual(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const edge = profile_is("alu-map-edge");
	bool const interrupt_registers = profile_is("alu-map-irq");
	bool const low_bus = profile_is("alu-low-bus");
	auto const f = mapped_dual_case(variant, edge, interrupt_registers, low_bus); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode); program.write_word(f.base + 0x601, 0xf4e1);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, f.ifr); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.imr);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x8001); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x7fff);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	if (f.xaddress >= 0x60 || (low_bus && f.xaddress >= 2 && f.xaddress <= 5)) data.write_word(f.xaddress, f.xword);
	if (f.yaddress >= 0x60 || (low_bus && f.yaddress >= 2 && f.yaddress <= 5)) data.write_word(f.yaddress, f.yword);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mapped_dual_reads", [this,f,edge](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(!edge && m_pipeline_operand_reads < 2 && at == (m_pipeline_operand_reads ? f.yaddress : f.xaddress) && word == (m_pipeline_operand_reads ? f.yword : f.xword) && m_cpu->pipeline_cycles() == 5, "Mapped dual ALU samples both captured addresses once at Read"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mapped_dual_no_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62170;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::circular_move_fixture tms320c54x_test_state::circular_move_case(unsigned variant)
{
	circular_move_fixture c{}; auto &f = c.memory;
	unsigned kind = 0, page = 0, snapshot = 0;
	static constexpr std::array<u16, 8> words{0, 1, 0x8000, 0xffff, 0x1eff, 0x00d0, 0xabcd, 0xf495};
	if (variant < 129024)
	{
		unsigned const g = variant % 1024; kind = (variant / 1024) % 21; page = (variant / 21504) % 2; snapshot = variant / 43008;
		unsigned const encoded = g % 4 + ((g / 4) % 8) * 4 + ((g / 32) % 2) * 32 + ((g / 64) % 4) * 128 + (g / 256) * 512 + ((g / 8 + kind) % 4) * 2048 + ((g ^ kind) % 8) * 8192;
		f = circular_load_case(encoded);
	}
	else
	{
		unsigned n = variant - 129024;
		unsigned mode = 0x12, compatibility = 0, source = 0x1001, index = 0, position = 0;
		bool mapped = false;
		f = circular_load_case(2 + 2 * 4);
		if (n < 4032)
		{
			kind = n % 21; f.word = words[(n / 21) % 8];
			unsigned const flags = (n / 168) % 4;
			f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0); f.pmst = flags >> 1;
			page = (n / 672) % 2; snapshot = n / 1344;
		}
		else if (n < 100800)
		{
			n -= 4032; mapped = true; source = 8 + n % 18; mode = (n / 18) % 32; compatibility = (n / 576) % 2;
			kind = (n / 1152) % 21; page = (n / 24192) % 2; snapshot = n / 48384;
		}
		else
		{
			n -= 100800; mode = n % 32; compatibility = (n / 32) % 2; kind = (n / 64) % 21;
			index = (n / 1344) % 4; position = (n / 5376) % 2;
			page = (n / 10752) % 2; snapshot = n / 21504;
			f.word = words[(mode + kind + index) % 8];
		}
		f.mode = 0xc0 | mode; f.ar = mode & 7; f.compatibility = compatibility; f.selected = compatibility && !f.ar ? 4 : f.ar;
		for (unsigned ar = 0; ar < 8; ++ar) f.initial[ar] = 0x1000 + 0x100 * ar;
		f.bk = mapped ? 64 : 82;
		if (!f.selected) f.bk = 0x2000;
		f.initial[0] = std::array<u16, 4>{1, 0xffff, f.bk, u16(-f.bk)}[index];
		f.address = mapped ? source : 0x1000 + 0x100 * f.selected + (position ? 81 : 1);
		f.initial[f.selected] = f.address;
		unsigned const operation = mode >> 3;
		s32 const step = operation & 1 ? s32(s16(f.initial[0])) : 1;
		s32 const delta = operation < 2 ? -step : step;
		unsigned mask = 1; while (mask <= f.bk) mask <<= 1; --mask;
		s32 const remainder = (s32(f.address & mask) + delta) % f.bk;
		f.final_ar = (f.address & ~mask) | u16(remainder < 0 ? remainder + f.bk : remainder);
		f.st1 = (f.st1 & ~u16(0x20)) | (compatibility ? 0x20 : 0); f.excluded = false;
		if (mapped)
		{
			if (source < 14) { unsigned const part = (source - 8) % 3; f.word = u16((source < 11 ? c.a : c.b) >> (16 * part)) & (part == 2 ? 0xff : 0xffff); }
			else if (source == 14) f.word = c.t; else if (source == 15) f.word = c.trn;
			else if (source < 24) f.word = f.initial[source - 16]; else f.word = source == 24 ? c.sp : f.bk;
		}
	}
	f.base = page ? 0x20000 : 0; f.snapshot = snapshot;
	c.destination = kind < 10 ? 0x10 + kind : kind < 18 ? kind - 2 : kind == 18 ? 0x1a : kind == 19 ? 0x1800 : f.address;
	c.word = f.word; c.excluded = f.excluded; c.bk = f.bk;
	c.final = f.initial; if (!c.excluded) c.final[f.selected] = f.final_ar;
	f.st0 = !c.excluded && f.compatibility && f.ar ? 0x608 | (f.ar << 13) : 0x8608;
	c.write_clock = c.destination >= 0x10 && c.destination <= 0x1a ? 5 : 6;
	if (!c.excluded)
	{
		unsigned const d = c.destination;
		if (d >= 8 && d <= 13) { unsigned const part = (d - 8) % 3; u64 &acc = d < 11 ? c.a : c.b; u64 const mask = part == 2 ? 0xff : 0xffff; acc = (acc & ~(mask << (16 * part))) | ((u64(c.word) & mask) << (16 * part)); }
		else if (d == 14) c.t = c.word; else if (d == 15) c.trn = c.word;
		else if (d < 24) c.final[d - 16] = c.word; else if (d == 24) c.sp = c.word; else if (d == 25) c.bk = c.word; else if (d == 26) c.brc = c.word;
	}
	return c;
}

tms320c54x_test_state::circular_move_fixture tms320c54x_test_state::indexed_move_case(unsigned variant)
{
	auto c = circular_move_case(variant); auto &f = c.memory;
	bool const add = f.mode & 0x10;
	f.mode = (add ? 0xb0 : 0xa8) | f.ar;
	f.final_ar = add ? f.address + f.initial[0] : f.address - f.initial[0];
	c.final = f.initial;
	if (!c.excluded)
	{
		c.final[f.selected] = f.final_ar;
		if (c.destination >= 0x10 && c.destination <= 0x17) c.final[c.destination - 0x10] = c.word;
	}
	return c;
}

void tms320c54x_test_state::start_circular_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	if (!variant) { m_circular_move_coverage = {}; m_circular_move_excluded = 0; }
	bool const indexed = profile_is("mvdk-index");
	unsigned const key = circular_move_schedule::key(variant);
	auto const c = indexed ? indexed_move_case(key) : circular_move_case(key); auto const &f = c.memory;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x7100 | f.mode); program.write_word(f.base + 0x601, c.destination); program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x1357);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, f.initial[ar]);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2200); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0);
	if (f.address >= 0x60) data.write_word(f.address, c.word);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "circular_mvdk_sample", [this, c](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!c.excluded && at == c.memory.address && word == c.word && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 5, "Circular MVDK samples its old source once before pointer/destination alias updates"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "circular_mvdk_publication", [this, c](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!c.excluded && at == c.destination && word == c.word && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == c.write_clock, "Circular MVDK publishes once at its destination-owned write clock"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "circular_mvdk_literal", [this, c](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_extensions_reads && word == c.destination, "Circular MVDK retains its destination literal once"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62146;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::repeated_mvdk_fixture tms320c54x_test_state::repeated_mvdk_case(unsigned variant, bool indexed)
{
	repeated_mvdk_fixture f{};
	unsigned const encoding = variant % (indexed ? 16 : 184);
	unsigned const mode = indexed ? 0xa8 + encoding : encoding < 152 ? encoding : encoding + 40;
	unsigned n = variant / (indexed ? 16 : 184);
	f.count = std::array<unsigned, 4>{0, 1, 5, 15}[n % 4]; n /= 4;
	unsigned const placement = n % 6; n /= 6;
	bool const compatibility = n % 2; n /= 2;
	bool const cpl = n % 2; n /= 2;
	f.base = n % 2 ? 0x20000 : 0; n /= 2; f.snapshot = n % 3;
	f.opcode = 0x7100 | mode; f.st0 = 0x8600 | 0x20;
	f.st1 = 0x800 | (compatibility ? 0x20 : 0) | (cpl ? 0x4000 : 0);
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x1040;
	f.initial[0] = 1; f.bk = mode >= 0xc0 ? 82 : 0;
	unsigned const encoded_ar = mode & 7, selected = compatibility && !encoded_ar ? 4 : encoded_ar;
	if (mode & 0x80) f.initial[selected] = mode >= 0xc0 ? 0x1051 : 0x1040;
	if ((mode & 0x80) && !selected && f.bk) f.bk = 0x2000;
	f.final = f.initial;
	u16 const first_source = mode & 0x80 ? f.initial[selected] : 0x1000 + mode;
	f.destination = placement == 0 ? 0x1800 : u16(first_source + int(placement) - 3);
	if (mode & 0x80) if (compatibility && encoded_ar) f.st0 = (f.st0 & 0x1fff) | (encoded_ar << 13);
	std::map<u16, u16> committed;
	for (unsigned i = 0; i <= f.count; ++i)
	{
		u16 const source = mode & 0x80 ? f.final[selected] : first_source;
		f.sources[i] = source;
		f.values[i] = committed.count(source) ? committed[source] : repeated_mvdk_fixture::seed(source);
		// Access(n) precedes Read(n-1)'s RAM publication in the same clock.
		if (i) committed[u16(f.destination + i - 1)] = f.values[i - 1];
		if (mode & 0x80)
		{
			unsigned const operation = (mode >> 3) & 15;
			s32 const step = operation & 1 ? s32(s16(f.final[0])) : 1;
			s32 delta = indexed ? operation == 5 ? -s32(s16(f.final[0])) : s32(s16(f.final[0])) : operation == 1 ? -1 : operation == 2 ? 1 : operation >= 8 ? (operation < 10 ? -step : step) : 0;
			if (operation >= 8 && f.bk)
			{
				unsigned mask = 1; while (mask <= f.bk) mask <<= 1; --mask;
				s32 const remainder = (s32(source & mask) + delta) % f.bk;
				f.final[selected] = (source & ~mask) | u16(remainder < 0 ? remainder + f.bk : remainder);
			}
			else f.final[selected] = u16(source + delta);
		}
		f.excluded |= source < 0x60;
	}
	return f;
}

tms320c54x_test_state::rptbd_t_load_fixture tms320c54x_test_state::rptbd_t_load_case(unsigned v) const
{
	rptbd_t_load_fixture f{};
	f.ordinary = profile_is("ctrl-t-load");
	if (f.ordinary) { f.control = v % 6; v /= 6; }
	f.count = std::array<unsigned, 3>{0, 1, 7}[v % 3];
	f.value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 3) % 4];
	f.form = (v / 12) % 4; f.slot = (v / 48) % 2;
	f.status = 0x800 | (((v / 96) % 4) << 8) | (((v / 384) % 2) << 14);
	f.accumulator = (v / 768) % 2;
	f.base = (v / 1536) % 2 ? 0x20000 : 0; f.snapshot = v / 3072;
	f.mode = std::array<u16, 4>{0x12, 0x84, 0x94, 0x8c}[f.form];
	f.source = f.form ? 0x2800 : f.status & 0x4000 ? 0x1e12 : 0x1412;
	f.branch = profile_is("banzd-t-load");
	if (f.branch) { f.branch_count = f.count; f.count = 0; }
	if (f.ordinary) { f.late_t = v % 3; f.count = 0; if (!f.form && (f.status & 0x4000)) f.source = f.final_sp() + 0x12; }
	return f;
}

void tms320c54x_test_state::start_rptbd_t_load(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = rptbd_t_load_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, 0x605);
	program.write_word(f.base + 0x602 + f.slot, 0x3000 | f.mode);
	program.write_word(f.base + 0x604, 0x2083 | (f.accumulator << 8));
	program.write_word(f.base + 0x605, 0x8092 | (f.accumulator << 8)); program.write_word(f.base + 0x606, 0xf4e1);
	if (f.branch)
	{
		program.write_word(f.base + 0x600, 0x6e8f); program.write_word(f.base + 0x601, f.target());
		program.write_word(f.base + f.target(), 0x2083 | (f.accumulator << 8));
		program.write_word(f.base + f.target() + 1, 0x8092 | (f.accumulator << 8)); program.write_word(f.base + f.target() + 2, 0xf4e1);
	}
	if (f.ordinary)
	{
		for (unsigned i = 0; i < 8; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
		program.write_word(f.base + 0x600, f.control == 4 ? 0xfb80 | (f.target_base() >> 16) : std::array<u16, 6>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00}[f.control]);
		if (f.control != 5) program.write_word(f.base + 0x601, f.target());
		program.write_word(f.base + (f.control == 5 ? 0x601 : 0x602) + f.slot, 0x3000 | f.mode);
		for (u32 at : {f.base + 0x604, f.target_base() + f.target()}) { program.write_word(at, 0x2083 | (f.accumulator << 8)); program.write_word(at + 1, 0x8092 | (f.accumulator << 8)); program.write_word(at + 2, 0xf4e1); }
		if (f.control == 5) data.write_word(0x1e00, f.target());
	}
	data.write_word(f.source, f.value); data.write_word(0x3000, 3);
	for (unsigned i = 0; i <= f.count + 2; ++i) data.write_word(0x3fff + i, 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x2628); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.status);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123456789aULL);
	if (f.ordinary && (f.control == 1 || f.control == 2)) m_cpu->set_state_int(tms320c54x_device::STATE_A, f.control == 1 ? 0 : 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1e00); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x3300 + i);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x4000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x2800);
	if (f.branch) { m_cpu->set_state_int(tms320c54x_device::STATE_AR7, f.branch_count); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 13); }
	if (f.ordinary) m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 13);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_fast_stack_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "rptbd_delay_t_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.ordinary && at == 0x1e00) { expect(f.control == 5 && !m_fast_stack_reads++ && word == f.target(), "RETD owns one original continuation read before the T load"); return; }
		if (at == f.source) { expect(!m_pipeline_operand_reads && word == f.value && m_cpu->pipeline_cycles() == f.read_clock(), "RPTBD delay T load samples its operand once at Read"); ++m_pipeline_operand_reads; }
		else { expect(at == 0x3000 && word == 3 && m_pipeline_extensions_reads <= f.count, "RPTBD body reads only its own multiply operand"); ++m_pipeline_extensions_reads; }
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "rptbd_delay_t_product", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.ordinary && (at == 0x1dff || at == 0x1dfe)) { unsigned const i = m_pipeline_register_writes++; expect((f.control == 3 || f.control == 4) && i < unsigned(f.control == 4 ? 2 : 1) && at == 0x1dff - i && word == (i ? f.base >> 16 : 0x604), "CALLD/FCALLD write only their owned continuation frames"); return; }
		expect(m_pipeline_store_writes <= f.count && at == 0x4000 + m_pipeline_store_writes && word == f.product(), "First and later RPTBD multiplies consume the delay-owned T value"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 600030;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::repeated_mvmd_fixture tms320c54x_test_state::repeated_mvmd_case(unsigned v, bool zero)
{
	repeated_mvmd_fixture f{};
	f.source = v % 2 ? 0x0b : 0x08;
	f.count = std::array<unsigned, 4>{0, 1, 39, 255}[(v / 2) % 4];
	f.seed = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 8) % 4];
	f.clear_accumulator = zero ? (v / 128) % 2 : 2;
	f.value = f.clear_accumulator == unsigned(f.source == 0x0b) ? 0 : f.seed;
	f.status = 0x800 | (((v / 32) % 4) << 8);
	f.long_header = zero || (v / 128) % 2;
	f.base = (v / 256) % 2 ? 0x20000 : 0;
	f.snapshot = v / 512;
	f.destination = f.base ? 0x2400 : 0x122e;
	f.literal = f.base + (f.long_header ? 0x603 : 0x602);
	f.read_clock = f.long_header ? 7 : 6;
	return f;
}

void tms320c54x_test_state::start_repeated_mvmd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = repeated_mvmd_case(variant, profile_is("mvmd-rptz"));
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	unsigned pc = f.base + 0x600;
	program.write_word(pc++, f.clear_accumulator < 2 ? 0xf071 | (f.clear_accumulator << 8) : f.long_header ? 0xf070 : 0xec00 | f.count);
	if (f.long_header) program.write_word(pc++, f.count);
	program.write_word(pc++, 0x7300 | f.source); program.write_word(pc++, f.destination); program.write_word(pc, 0xf4e1);
	for (unsigned i = 0; i <= f.count + 2; ++i) data.write_word(f.destination - 1 + i, 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8620); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.status);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.source == 8 ? f.initial_accumulator() : 0xfeabcd9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.source == 0xb ? f.initial_accumulator() : 0xfeabcd9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1e00); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x3300 + i);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeated_mvmd_source", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_operand_reads++;
		if (i > f.count || at != f.source || word != f.sample(i) || m_cpu->pipeline_cycles() != f.read_clock + i)
			osd_printf_info("MVMD read mismatch: case=%u index=%u count=%u address=%04x expected_address=%04x word=%04x expected_word=%04x cycle=%llu expected_cycle=%u\n",
				m_pipeline_peripheral_case, i, f.count, unsigned(at), f.source, word, f.sample(i), (unsigned long long)m_cpu->pipeline_cycles(), f.read_clock + i);
		expect(i <= f.count && at == f.source && word == f.sample(i) && m_cpu->pipeline_cycles() == f.read_clock + i, "Repeated MVMD samples the fixed MMR each iteration");
		if (i == f.count / 2) m_cpu->set_state_int(f.source == 8 ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B, f.selected_after_clear() ^ 0xffffULL);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeated_mvmd_destination", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_store_writes++;
		expect(i <= f.count && at == f.destination + i && word == f.sample(i) && m_cpu->pipeline_cycles() == f.read_clock + 1 + i, "Repeated MVMD advances only EAR and writes its retained sample once");
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.literal, f.literal, "repeated_mvmd_literal", [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "Repeated MVMD fetches one original destination literal"); ++m_pipeline_extensions_reads; }
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 600020;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_repeated_mvdk(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	bool const indexed = profile_is("mvdk-index-rpt");
	auto f = repeated_mvdk_case(variant, indexed);
	// Self-indexed AR0 can enter mapped memory; that composition is not admitted.
	while (f.excluded) f = repeated_mvdk_case(++variant, indexed);
	m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xec00 | f.count); program.write_word(f.base + 0x601, f.opcode);
	program.write_word(f.base + 0x602, f.destination); program.write_word(f.base + 0x603, 0xf4e1);
	for (unsigned i = 0; i <= f.count; ++i) { data.write_word(f.sources[i], repeated_mvdk_fixture::seed(f.sources[i])); data.write_word(u16(f.destination + i), repeated_mvdk_fixture::seed(u16(f.destination + i))); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8620); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeated_mvdk_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads++; expect(i <= f.count && at == f.sources[i] && word == f.values[i] && m_cpu->pipeline_cycles() == 6 + i, "Repeated MVDK owns each Access sample and source update"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeated_mvdk_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes++; expect(i <= f.count && at == u16(f.destination + i) && word == f.values[i] && m_cpu->pipeline_cycles() == 7 + i, "Repeated MVDK increments EAR and publishes each retained sample once"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x602, f.base + 0x602, "repeated_mvdk_literal", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "Repeated MVDK fetches its destination only once"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62148;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::memory_copy_repeat_fixture tms320c54x_test_state::memory_copy_repeat_case(unsigned v, bool wrap, bool chain)
{
	memory_copy_repeat_fixture f{};
	if (wrap)
	{
		unsigned n = v; unsigned const form = n % 2 ? 18 : 0; n /= 2;
		unsigned const count = n % 4; n /= 4; unsigned const body = n % 3; n /= 3;
		unsigned const page = n % 2; n /= 2;
		v = form + count * 24 + body * 96 + page * 576 + n * 3456;
	}
	if (chain)
	{
		unsigned n = v; unsigned const form = n % 2 ? 18 : 0; n /= 2;
		unsigned const count = n % 4; n /= 4; unsigned const body = n % 3; n /= 3;
		unsigned const flags = n % 2; n /= 2; unsigned const page = n % 2; n /= 2;
		v = form + count * 24 + body * 96 + flags * 288 + page * 576 + n * 3456;
	}
	f.chain = chain;
	f.form = v % 24; v /= 24; f.count = std::array<unsigned, 4>{0, 1, 7, 31}[v % 4]; v /= 4;
	f.body_kind = v % 3; v /= 3; unsigned const flags = v % 2; v /= 2;
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.irq = v % 3; v /= 3;
	f.snapshot = v % 4; v /= 4; f.pattern = v;
	f.absolute = f.form <= 18;
	f.address = f.form < 18 ? f.form + 8 : f.form == 18 || f.form == 21 ? 0x4000 : f.form == 19 ? 0x4001 : f.form == 20 ? 0x3001 : 8;
	f.mode = f.absolute ? 0xf8 : f.form == 19 || f.form == 20 ? 1 : f.form == 22 ? 8 : 0x91;
	f.st0 = 0xa600 | (f.form == 19 ? 0x80 : f.form == 22 ? 0 : 8);
	f.st1 = (flags ? 0x320 : 0) | (f.irq ? 0 : 0x800) | (f.form == 20 ? 0x4000 : 0);
	f.source_ar = f.address == 0x12 || f.address == 0x13 ? 4 : 2; f.destination_ar = f.source_ar + 1;
	f.sentinel_ar = f.address == 0x17 ? 6 : 7;
	f.opcode = !f.body_kind ? 0xe500 | ((f.source_ar + 6) << 4) | (f.destination_ar + 6) :
		f.body_kind == 1 ? 0x7090 | f.destination_ar : 0x7190 | f.source_ar;
	f.origin = wrap ? 0xfffe : 0x600;
	f.body = f.base | u16(f.origin + (f.absolute ? 2 : 1));
	return f;
}

void tms320c54x_test_state::start_memory_copy_repeat(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const wrap = profile_is("rpt-mem-wrap"), chain = profile_is("rpt-mem-chain"); auto const f = memory_copy_repeat_case(variant, wrap, chain); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base | u16(f.origin + i), 0xf495);
	program.write_word(f.base | f.origin, 0x4700 | f.mode); if (f.absolute) program.write_word(f.base | u16(f.origin + 1), f.address);
	program.write_word(f.body, f.opcode); if (f.body_kind) program.write_word(f.body + 1, f.body_kind == 1 ? 0x5000 : 0x6000);
	u32 const after = f.body + f.body_words();
	if (chain) { program.write_word(after, 0x47f8); program.write_word(after + 1, 0x4002); program.write_word(after + 2, 0xf495); data.write_word(0x4002, 3); }
	program.write_word(after + (chain ? 3 : 0), 0x6d90 | f.sentinel_ar); program.write_word(after + (chain ? 15 : 12), 0xf4e1);
	if (f.irq) { unsigned const vector = f.irq == 2 ? 4 : 0x48; program.write_word(f.base | vector, 0xf495); program.write_word((f.base | vector) + 1, 0xf4eb); }
	for (unsigned i = 0; i <= f.count; ++i) { data.write_word(0x5000 + i, f.payload(i)); data.write_word(0x6000 + i, 0); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq == 1 ? 4 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_RC, 0); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 13);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x1357);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 82);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x7200 + i);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.source_ar, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.destination_ar, 0x6000);
	if (f.form == 21 || f.form == 23) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, f.address);
	set_repeat_count_source(f.address, f.count);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_store_case = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = 0;
	m_observed_instructions.clear();
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "memory_copy_repeat_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.address) { expect(!m_pipeline_extensions_reads++ && word == f.count && m_cpu->pipeline_cycles() == f.read_clock(), "RPT Smem captures its count once in Read"); return; }
		if (f.chain && at == 0x4002) { expect(m_pipeline_extensions_reads++ == 1 && word == 3, "Following NOP repeat captures its own count once"); return; }
		if (at == 0x2fff) { expect(f.irq && !m_fast_stack_reads++ && m_pipeline_register_writes == 1, "Deferred repeat interrupt reads its single committed frame"); return; }
		unsigned const i = m_pipeline_operand_reads++; expect(i <= f.count && at == 0x5000 + i && word == f.payload(i), "Memory count drives ordered source reads without using its address as the count");
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "memory_copy_repeat_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && !m_pipeline_register_writes++ && m_pipeline_store_writes == f.count + 1, "Interrupt frame follows the final repeated copy"); return; }
		unsigned const i = m_pipeline_store_writes++; expect(i <= f.count && at == 0x6000 + i && word == f.payload(i), "Memory count publishes every retained copy once");
		if (!i && f.irq) m_cpu->set_input_line(f.irq == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.body, f.body + (chain ? f.body_words() + 2 : 0), "memory_copy_repeat_fetch", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == f.body) expect(!m_pipeline_store_case++ && word == f.opcode, "Memory-count repeat fetches its body only once"); else if (f.chain && at == f.body + f.body_words() + 2) expect(m_pipeline_store_case++ == 1 && word == 0xf495, "Following NOP repeat fetches its distinct body once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62154;
	m_cpu->set_state_int(STATE_GENPC, f.base | f.origin); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}












void tms320c54x_test_state::start_repeat_mvdp(unsigned variant)
{
	unsigned const form = variant % 5, reg = (variant / 5) % 8;
	unsigned const count = std::array<unsigned, 5>{0, 1, 2, 7, 255}[(variant / 40) % 5];
	bool const long_count = (variant / 400) % 2;
	u32 const base = (variant / 200) % 2 ? 0x20000 : 0;
	bool const mapped = (variant / 1600) % 2;
	u16 const destination = (variant / 800) % 2 ? 0xfffc : 0x2e00;
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa624}, {cpu_regs::STATE_ST1, 0x800 | (form == 1 ? 0x4000 : 0)},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_TCR, 0x10},
		{cpu_regs::STATE_SP, 0x1200}, {cpu_regs::STATE_BK, 16},
	});
	for (unsigned ar = 0; ar < 8; ++ar)
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == reg ? 0x1230 : 0x3000 + ar);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const prefix = long_count ? 2 : 1;
	u8 const mode = form < 2 ? 3 : (form == 2 ? 0x80 : form == 3 ? 0x90 : 0x88) + reg;
	// OVLY maps the executing code too, not just the copy destination.
	auto emit = [&program, mapped](u32 at, u16 word) { program.write_word(mapped && u16(at) < 0x8000 ? u16(at) : at, word); };
	emit(base + 0x600, long_count ? 0xf070 : 0xec00 | count);
	if (long_count) emit(base + 0x601, count);
	emit(base + 0x600 + prefix, 0x7d00 | mode);
	emit(base + 0x601 + prefix, destination);
	emit(base + 0x602 + prefix, 0xf4e1);
	for (unsigned i = 0; i <= count; ++i)
	{
		u16 const source = form < 2 ? 0x1203 : form == 3 ? 0x1230 + i : form == 4 ? 0x1230 - i : 0x1230;
		m_cpu->space(AS_DATA).write_word(source, 0x2468 + (form >= 3 ? i : 0));
		u32 const logical = base | u16(destination + i);
		program.write_word(mapped && u16(logical) < 0x8000 ? u16(logical) : logical, 0xbeef);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, mapped ? 0x20 : 0);
	m_first_port_cycle = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "repeat_mvdp_source",
		[this, form, count](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			unsigned const i = m_pipeline_operand_reads;
			u16 const source = form < 2 ? 0x1203 : form == 3 ? 0x1230 + i : form == 4 ? 0x1230 - i : 0x1230;
			if (i > count || at != source || word != 0x2468 + (form >= 3 ? i : 0))
				osd_printf_info("MVDP repeat source mismatch: case=%u iteration=%u at=%04x expected=%04x word=%04x expected_word=%04x cycle=%llu\n", m_pipeline_peripheral_case, i, unsigned(at), source, word, unsigned(0x2468 + (form >= 3 ? i : 0)), (unsigned long long)m_cpu->pipeline_cycles());
			expect(i <= count && at == source && word == 0x2468 + (form >= 3 ? i : 0), "Repeated MVDP samples each iteration's source in order");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = program.install_write_tap(0, 0x7fffff, "repeat_mvdp_destination",
		[this, form, count, base, destination, mapped](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			unsigned const i = m_pipeline_store_writes;
			u32 const logical = base | u16(destination + i);
			u32 const physical = mapped && u16(logical) < 0x8000 ? u16(logical) : logical;
			if (!i) m_first_port_cycle = m_cpu->pipeline_cycles();
			expect(i <= count && at == physical && word == 0x2468 + (form >= 3 ? i : 0) && m_cpu->pipeline_cycles() == m_first_port_cycle + i, "Repeated MVDP wraps pmad and retires one word per accelerated clock");
			++m_pipeline_store_writes;
		});
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_phase = 259940;
	m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}





bool tms320c54x_test_state::mvdd_source_owned(u16 address)
{
	return address <= 1 || (address >= 6 && address <= 0x1e) || (address >= 0x24 && address <= 0x26) || address == 0x58;
}

u64 tms320c54x_test_state::mvdd_source_a(unsigned pattern)
{
	return std::array<u64, 4>{0, 0x123456789aULL, 0xff80000001ULL, 0xffffffffffULL}[pattern];
}

u64 tms320c54x_test_state::mvdd_source_b(unsigned pattern)
{
	return std::array<u64, 4>{0xffffffffffULL, 0xabcdef0123ULL, 0x007fffffffULL, 0}[pattern];
}

u16 tms320c54x_test_state::mvdd_source_word(u16 address, unsigned pattern, u32 base, u16 ar2, u16 ar3)
{
	if (address == 0) return 0x1234;
	if (address == 1) return 0;
	if (address == 6) return 0xa608;
	if (address == 7) return 0x900;
	if (address >= 8 && address <= 0xa) return mvdd_source_a(pattern) >> ((address - 8) * 16);
	if (address >= 0xb && address <= 0xd) return mvdd_source_b(pattern) >> ((address - 0xb) * 16);
	if (address == 0xe) return 0x7000 | pattern;
	if (address == 0xf) return 0xa500 | pattern;
	if (address >= 0x10 && address <= 0x17)
		return address == 0x10 ? 3 : address == 0x12 ? ar2 : address == 0x13 ? ar3 : 0x4000 + address - 0x10;
	if (address == 0x18) return 0x6000;
	if (address == 0x1e) return base >> 16;
	if (address >= 0x19 && address <= 0x1d) return 0;
	if (address == 0x24) return 0x1234;
	if (address == 0x25) return 0x5678;
	if (address == 0x26) return 0x10;
	if (address == 0x58) return 0;
	return std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern];
}

tms320c54x_test_state::mvdd_source_fixture tms320c54x_test_state::mvdd_source_case(unsigned variant, bool repeated)
{
	mvdd_source_fixture f{}; f.repeated = repeated;
	unsigned v = variant;
	if (repeated)
	{
		unsigned const layout = v % 5; v /= 5;
		f.source = std::array<unsigned, 5>{0, 0x1f, 0x5f, 0x12, 0x33}[layout];
		f.count = layout == 0 ? 128 : 4; f.fixed = layout == 4;
		f.xmode = f.fixed ? 0 : 2; f.ymode = 2;
	}
	else { f.source = v % 96; v /= 96; f.count = 1; }
	f.pattern = v % 4; v /= 4;
	if (repeated) { f.prefix = 1 + v % 2; v /= 2; }
	else { f.xmode = v % 4; v /= 4; f.ymode = v % 4; v /= 4; }
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v;
	return f;
}

void tms320c54x_test_state::start_mvdd_source(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const repeated = profile_is("mvdd-source-rpt");
	auto const f = mvdd_source_case(variant, repeated);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.prefix) program.write_word(f.base + 0x600, f.prefix == 1 ? 0xec00 | (f.count - 1) : 0xf070);
	if (f.prefix == 2) program.write_word(f.base + 0x601, f.count - 1);
	program.write_word(f.base + 0x600 + f.prefix, 0xe500 | ((1 | (f.xmode << 2)) << 4) | (f.ymode << 2));
	program.write_word(f.base + 0x601 + f.prefix, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, mvdd_source_a(f.pattern)); m_cpu->set_state_int(tms320c54x_device::STATE_B, mvdd_source_b(f.pattern));
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x7000 | f.pattern); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0xa500 | f.pattern);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0); m_cpu->set_state_int(tms320c54x_device::STATE_RSA, 0); m_cpu->set_state_int(tms320c54x_device::STATE_REA, 0);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == 0 ? 3 : ar == 2 ? f.destination : ar == 3 ? f.source : 0x4000 + ar);
	for (unsigned address = 0; address < 128; ++address)
		if (!mvdd_source_owned(address)) data.write_word(address, mvdd_source_word(address, f.pattern, f.base, 0, 0));
	for (unsigned i = 0; i < f.count; ++i) data.write_word(f.destination + i, 0xaaaa);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvdd_mapped_source", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_operand_reads;
		if (i >= f.count || at != f.address(i) || word != f.word(i) || m_cpu->pipeline_cycles() != f.read_clock(i))
			osd_printf_info("Mapped MVDD Read mismatch: case=%u index=%u address=%04x expected_address=%04x word=%04x expected_word=%04x cycle=%llu expected_cycle=%u ar2=%04x ar3=%04x\n", m_pipeline_peripheral_case, i, unsigned(at), f.address(i), word, f.word(i), (unsigned long long)m_cpu->pipeline_cycles(), f.read_clock(i), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR3)));
		expect(i < f.count && at == f.address(i) && word == f.word(i) && m_cpu->pipeline_cycles() == f.read_clock(i), "MVDD reads each mapped source once at its paid Read clock"); ++m_pipeline_operand_reads;
		if (at >= 0x20 && at < 0x60 && !mvdd_source_owned(at))
		{
			// Synthetic read-clear endpoint, not a DA150 register identity.
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0);
		}
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvdd_mapped_copy", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_store_writes;
		if (i >= f.count || at != 0x1000 + i || word != f.word(i) || m_cpu->pipeline_cycles() != f.write_clock(i))
			osd_printf_info("Mapped MVDD Write mismatch: case=%u index=%u address=%04x expected_address=%04x word=%04x expected_word=%04x cycle=%llu expected_cycle=%u\n", m_pipeline_peripheral_case, i, unsigned(at), 0x1000 + i, word, f.word(i), (unsigned long long)m_cpu->pipeline_cycles(), f.write_clock(i));
		expect(i < f.count && at == 0x1000 + i && word == f.word(i) && m_cpu->pipeline_cycles() == f.write_clock(i), "MVDD writes retained mapped data once after all owned waits"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600 + f.prefix, f.base + 0x600 + f.prefix, "mvdd_mapped_opcode", [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "Repeated mapped MVDD fetches one retained opcode"); ++m_pipeline_extensions_reads; } });
	m_phase = 254600; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


tms320c54x_test_state::mvdk_mapped_fixture tms320c54x_test_state::mvdk_mapped_case(unsigned v, bool accumulator_destination, bool clock)
{
	mvdk_mapped_fixture f{};
	if (clock)
	{
		f.source = 0x1420; f.destination = 0x58; f.form = v % 3;
		f.value = std::array<u16,4>{0,0x3146,0x8006,0xffff}[(v / 3) % 4];
		f.base = (v / 12) % 2 ? 0x20000 : 0;
		f.access = f.form == 2 ? 6 : 5;
		f.checkpoint = f.access + std::array<int,3>{-1,1,3}[v / 24];
		return f;
	}
	unsigned const sources = accumulator_destination ? 19 : 18, destinations = accumulator_destination ? 6 : 11;
	f.source = v % sources == 18 ? 0x62 : 8 + v % sources; f.form = (v / sources) % 3;
	unsigned const destination = (v / (sources * 3)) % destinations, patterns = sources * 3 * destinations;
	f.destination = accumulator_destination ? 8 + destination : destination < 8 ? 0x10 + destination : destination < 10 ? 0x0e + destination - 8 : 0x6000;
	f.value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / patterns) % 4];
	if (f.source == 0x0a || f.source == 0x0d) f.value &= 0xff;
	if (f.form == 1 && f.source == 0x16) f.value = f.source;
	f.base = (v / (patterns * 4)) % 2 ? 0x20000 : 0; f.access = f.form == 2 ? 6 : 5;
	f.checkpoint = f.access - 1 + v / (patterns * 8);
	return f;
}

void tms320c54x_test_state::start_mvdk_mapped(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const clock = profile_is("clock-mvdk");
	auto const f = mvdk_mapped_case(variant, profile_is("mvdk-acc"), clock); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.form == 0 ? 0x7100 | (f.source & 0x7f) : f.form == 1 ? 0x7196 : 0x71f8);
	program.write_word(f.base + 0x601, f.form == 2 ? f.source : f.destination);
	if (f.form == 2) program.write_word(f.base + 0x602, f.destination);
	program.write_word(f.base + (f.form == 2 ? 0x603 : 0x602), 0xf4e1);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, clock ? 0xa628 : 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	for (unsigned r = 0; r < 8; ++r) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + r, 0x1400 + r);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 13);
	data.write_word(f.source, f.value); if (f.form == 1) m_cpu->set_state_int(tms320c54x_device::STATE_AR6, f.source);
	if (clock) { data.write_word(0x58,0); data.write_word(0x58,0); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvdk_mapped_source",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			u32 instruction; u16 opcode;
			u16 const expected = f.form == 0 ? 0x7100 | (f.source & 0x7f) : f.form == 1 ? 0x7196 : 0x71f8;
			expect(m_cpu->observe_staged_data_read(at,instruction,opcode) && instruction == f.base+0x600 && opcode == expected,
				"MVDK source observer owns the actual Address-stage instruction, including mapped aliases");
			expect(!m_cpu->observe_staged_data_read(at^1,instruction,opcode), "Read ownership rejects another address");
			{ auto const disable = machine().disable_side_effects();
				expect(!m_cpu->observe_staged_data_read(at,instruction,opcode), "Debugger scope cannot claim the enclosing operand read"); }
			expect(at == f.source && word == f.value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.access, "MVDK samples one mapped register in Access before alias publication"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvdk_mapped_destination",
		[this, f, clock](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.destination && word == f.value && !m_pipeline_store_writes && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() == f.access + (clock ? 2 : unsigned(f.destination < 0x10 || f.destination >= 0x60)), "MVDK publishes one retained word at its destination-owned clock"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + (f.form == 2 ? 0x602 : 0x601), "mvdk_mapped_literals",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const i = m_pipeline_extensions_reads++;
			expect(at == f.base + 0x601 + i && i < (f.form == 2 ? 2U : 1U) && word == (f.form == 2 && !i ? f.source : f.destination), "MVDK fetches source/address literals once and in order"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255360;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::mvdk_producer_fixture tms320c54x_test_state::mvdk_producer_case(unsigned v, bool accumulator_destination)
{
	unsigned const group = accumulator_destination ? v / 5184 : 0; v %= 5184;
	mvdk_producer_fixture f{};
	f.source = 8 + v % 6; f.form = (v / 6) % 3; f.producer = (v / 18) % 3;
	f.prefix = 1 + (v / 54) % 2; f.destination = (v / 108) % 2 ? 0x6000 : 0x13;
	if (accumulator_destination) f.destination = 8 + 2 * group + (v / 108) % 2;
	f.value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 216) % 4];
	f.base = (v / 864) % 2 ? 0x20000 : 0; f.access = 5 + f.prefix + unsigned(f.form == 2);
	f.checkpoint = f.access - 1 + v / 1728;
	unsigned const part = (f.source - 8) % 3;
	u64 const initial = f.source >= 0x0b ? 0xfedcba9876ULL : 0x123456789aULL;
	// The adjacent short-form LD case is an Access-owned projection, not
	// a guaranteed result for firmware violating the manual's latency rule.
	// A long source-address word or NOP supplies the required separation.
	f.expected = f.producer == 2 ? (f.access == 6 ? u16(initial >> (16 * part)) : part ? 0 : u8(f.value)) : part == 2 ? u8(f.value) : f.value;
	return f;
}

void tms320c54x_test_state::start_mvdk_producer(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = mvdk_producer_case(variant, profile_is("mvdk-acc-prod")); bool const b = f.source >= 0x0b;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 20; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.producer == 0 ? (b ? 0x8000 : 0x8100) | f.source : f.producer == 1 ? 0x8a00 | f.source : (b ? 0xe900 : 0xe800) | u8(f.value));
	program.write_word(f.base + 0x600 + f.prefix, f.form == 0 ? 0x7100 | f.source : f.form == 1 ? 0x7186 : 0x71f8);
	program.write_word(f.base + 0x601 + f.prefix, f.form == 2 ? f.source : f.destination);
	if (f.form == 2) program.write_word(f.base + 0x602 + f.prefix, f.destination);
	program.write_word(f.base + 0x600 + f.prefix + (f.form == 2 ? 3 : 2), 0xf4e1);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.producer == 0 && b ? f.value : 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.producer == 0 && !b ? f.value : 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1403); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, f.source);
	data.write_word(0x3000, f.value);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvdk_producer_source",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x3000) { expect(f.producer == 1 && !m_fast_stack_reads && word == f.value && m_cpu->pipeline_cycles() == 5, "POPM captures one producer stack word"); ++m_fast_stack_reads; return; }
			expect(at == f.source && word == f.expected && !m_pipeline_operand_reads && m_pipeline_register_writes == unsigned(f.producer != 2) && m_cpu->pipeline_cycles() == f.access, "MVDK distinguishes direct ALU latency from mapped producer no-conflict ordering"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvdk_producer_writes",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (f.producer != 2 && !m_pipeline_register_writes && at == f.source) { expect(word == f.value && m_cpu->pipeline_cycles() == 6, "Mapped accumulator producer publishes once at Execute"); ++m_pipeline_register_writes; return; }
			expect(at == f.destination && word == f.expected && !m_pipeline_store_writes && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() == f.access + unsigned(f.destination < 0x10 || f.destination >= 0x60), "MVDK publishes its retained mapped word once without an extra producer delay"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601 + f.prefix, f.base + f.prefix + (f.form == 2 ? 0x602 : 0x601), "mvdk_producer_literals",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_extensions_reads++;
			expect(i < (f.form == 2 ? 2U : 1U) && at == f.base + 0x601 + f.prefix + i && word == (f.form == 2 && !i ? f.source : f.destination), "Producer-composed MVDK retains ordered literals"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255380;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}







void tms320c54x_test_state::check_st1_memory_rules()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); auto &data = m_cpu->space(AS_DATA);
	for (unsigned masked = 0; masked < 2; ++masked)
		for (u16 mask : {u16(0xffff), u16(0x00ff), u16(0xff00), u16(0x0800)})
			for (unsigned word = 0; word < 0x10000; ++word)
			{
				u16 const before = 0x6300 | (masked ? 0x800 : 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, before | 0x400);
				expect(data.read_word(7) == before && m_cpu->state_int(tms320c54x_device::STATE_ST1) == before, "Reserved ST1 bit 10 cannot survive debugger import");
				u16 const merged = (before & ~mask) | (word & mask);
				u16 const expected = (merged & ~u16(0xc00)) | (before & merged & 0x800);
				data.write_word(7, word, mask);
				expect(data.read_word(7) == expected && m_cpu->state_int(tms320c54x_device::STATE_ST1) == expected && m_xf_pin == bool(expected & 0x2000), "Full and partial memory writes cannot set INTM and reserved bit 10 reads zero");
			}
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x6fff);
	expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save architectural INTM independently of the memory-write restriction");
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE && data.read_word(7) == 0x6bff && m_xf_pin, "Restore INTM and XF without treating load-state as a firmware memory write");
	osd_printf_info("TMS320C54x ST1 memory rules: PASS cases=524288 word_values=65536 old_intm=2 masks=ffff,00ff,ff00,0800 memory_cannot_set_intm=1 reserved_bit10=0 debugger_seed=1 restore=1 xf_restored=1 clear_policy=legacy_unverified instruction_phase_claim=0\n");
	throw emu_fatalerror(0, "TMS320C54x isolated ST1 memory rules complete");
}




void tms320c54x_test_state::stack_mmr_w(offs_t offset, u16 value)
{
	m_stack_mmr[offset] = value;
}

tms320c54x_test_state::staged_conditional_store_fixture tms320c54x_test_state::staged_conditional_store_case(unsigned v) const
{
	staged_conditional_store_fixture f{}; f.edge = profile_is("cond-store-edge");
	bool const mapped = profile_is("cond-store-ar");
	unsigned const target = mapped ? v % 8 : 0; if (mapped) v /= 8;
	f.form = v % 4; v /= 4;
	unsigned flags = 0; int shift = 0;
	if (f.edge) { f.mode = v % 16; v /= 16; f.condition = f.mode % 12; f.address = std::array<u16, 4>{8, 0x1a, 0x28, 0x5f}[v % 4]; v /= 4; f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v % 2; }
	else
	{
		f.condition = v % 12; v /= 12; f.pattern = v % 6; v /= 6; f.mode = v % 16; v /= 16;
		if (mapped) { shift = f.pattern & 1 ? -1 : 0; flags = f.pattern % 4; }
		else { shift = std::array<int, 4>{-16, -1, 0, 15}[v % 4]; v /= 4; flags = v % 4; v /= 4; }
		f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v % 3;
		f.address = mapped ? 0x10 + target : 0x160f + 0x100 * (f.mode & 3);
	}
	static constexpr std::array<u64, 6> values{0, 1, 0xffffffffffULL, 0x7fffffff, 0xff80000000ULL, 0x0100000000ULL};
	f.a = values[f.pattern]; f.b = values[(f.pattern + 3) % 6]; if (f.condition >= 6) std::swap(f.a, f.b);
	f.t = 0x1234 ^ (f.pattern * 0x2222); f.brc = 0xabcd ^ (f.pattern * 0x1111);
	f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (shift & 31); f.pmst = flags & 2 ? 1 : 0;
	f.memory = 0xa55a ^ (f.pattern * 0x1111); f.bk = !mapped && (f.pattern & 1) ? 16 : 0;
	unsigned const modification = f.mode >> 2;
	s16 const step = mapped && !target ? s16(f.memory) : 3;
	f.final_ar = modification == 1 ? f.address - 1 : modification == 2 ? f.address + 1 : modification == 3 ? f.bk ? (f.address & 0xfff0) | ((f.address + 3) & 15) : f.address + step : f.address;
	if (mapped && target == 2 + (f.mode & 3)) f.memory = f.final_ar;
	u64 const tested = f.condition >= 6 ? f.b : f.a; bool const negative = tested & 0x8000000000ULL, zero = !tested;
	bool const take = std::array<bool, 6>{!negative, negative, !zero, zero, !negative && !zero, negative || zero}[f.condition % 6];
	f.output = f.memory;
	if (take)
	{
		if (f.form < 2) f.output = f.form ? f.brc : f.t;
		else { u64 const source = f.form == 3 ? f.b : f.a; s64 const signed_source = source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source); u64 const value = shift < 0 ? u64(f.st1 & 0x100 ? signed_source >> -shift : s64(source >> -shift)) & 0xffffffffffULL : (source << shift) & 0xffffffffffULL; f.output = value >> 16; }
	}
	return f;
}

void tms320c54x_test_state::start_staged_conditional_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const mapped = profile_is("cond-store-ar");
	auto const f = staged_conditional_store_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x9c00 | (f.form << 8) | (f.mode << 4) | (f.condition < 6 ? f.condition + 2 : f.condition + 4)); program.write_word(f.base + 0x601, 0xf4e1);
	if (!f.edge && !mapped) data.write_word(f.address, f.memory);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa630); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a ^ 0x1111111111ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ^ 0x1111111111ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t ^ 0xffff); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.brc ^ 0xffff); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	for (unsigned ar = 2; ar < 6; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x2200 + ar);
	if (mapped) { for (unsigned ar : {1U, 6U, 7U}) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x2200 + ar); m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address - 0x10, f.memory); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "conditional_store_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!f.edge && !m_pipeline_operand_reads++ && at == f.address && word == f.memory && m_cpu->pipeline_cycles() == 5, "Conditional store reads the captured RAM address even when its condition is false"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "conditional_store_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!f.edge && !m_pipeline_store_writes++ && at == f.address && word == f.output && m_cpu->pipeline_cycles() == 6, "Conditional store publishes Execute-live source or retained false-condition readback once"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600, "conditional_store_opcode", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(!m_pipeline_extensions_reads++, "Conditional store fetches one opcode"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 620200;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


























bool tms320c54x_test_state::check_mvdd_source_results()
{
	if (m_phase == 254600)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const repeated = profile_is("mvdd-source-rpt");
		auto const f = mvdd_source_case(variant, repeated);
		unsigned first_peripheral = 0;
		if (f.source == 0) first_peripheral = 32;
		else if (f.source == 0x1f) first_peripheral = 1;
		unsigned const checkpoint = repeated ? std::array<unsigned, 5>{4, f.read_clock(first_peripheral) - 1, f.read_clock(first_peripheral), f.read_clock(f.count - 1), f.write_clock(f.count - 1)}[f.snapshot] : std::array<unsigned, 4>{3, 4, 5, 6}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped-source MVDD retains its addresses through all access waits");
		if (!repeated && tick == 4)
		{
			auto modify = [](u16 value, unsigned mode) -> u16 { return mode == 0 ? value : mode == 1 ? value - 1 : mode == 2 ? value + 1 : value + 3; };
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == modify(0x1000, f.ymode) && m_cpu->state_int(tms320c54x_device::STATE_AR3) == modify(f.source, f.xmode), "MVDD updates its two selected pointers once before mapped Read");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2222); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x3333);
		}
		if (!repeated && tick == f.read_clock(0))
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x4444); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5555);
			if (f.source == 0xe) m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped MVDD before/after a read-clear access or peripheral wait");
		}
		if (tick == f.write_clock(f.count - 1) + 12)
		{
			expect(m_pipeline_operand_reads == f.count && m_pipeline_store_writes == f.count && m_pipeline_extensions_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == (repeated ? 0x1000 + f.count : 0x4444) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == (repeated ? f.source + (f.fixed ? 0 : f.count) : 0x5555),
				"Mapped MVDD completes each read/write once with no repeated wait, late retarget or stale register alias");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(254610, "Restore mapped MVDD without repeating source side effects or destination writes");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (repeated ? 399 : 49151)) { start_mvdd_source(variant + 1); return true; }
			if (repeated)
			{
				osd_printf_info("TMS320C54x mapped MVDD repeat: PASS cases=400 restore=400 layouts=zero_page,enter_peripheral,leave_peripheral,AR_alias,read_clear lengths=128,4 patterns=4 prefixes=short,long pages=2 snapshots=5 reads=11520 writes=11520 source_opcode_fetches=1 peripheral_waits=once register_values=Read_owned endpoint_identities=unvalidated silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated mapped MVDD repeat complete");
			}
			osd_printf_info("TMS320C54x mapped MVDD sources: PASS cases=49152 restore=49152 source_addresses=96 patterns=4 x_modifiers=4 y_modifiers=4 pages=2 snapshots=4 source_reads=1 guest_writes=1 source_opcode_fetches=1 peripheral_extra_clock=1 ram_extra_clock=0 register_aliases=Read_owned read_clear=retained endpoint_identities=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated mapped MVDD sources complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254610)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254600;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rptbd_t_load_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 600030)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = rptbd_t_load_case(variant);
		unsigned const checkpoint = f.snapshot == 0 ? 5 : f.snapshot == 1 ? f.read_clock() : 12;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "One-word T load is admitted in either RPTBD delay position");
		if (f.ordinary && tick == f.read_clock()) { if (m_cpu->state_int(tms320c54x_device::STATE_T) != f.value) osd_printf_info("Delay T publication mismatch: case=%u control=%u slot=%u form=%u cycle=%u reads=%u t=%04x expected=%04x\n", variant, f.control, f.slot, f.form, tick, m_pipeline_operand_reads, unsigned(m_cpu->state_int(tms320c54x_device::STATE_T)), f.value); expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.value, "Delay T load publishes its original sample in Read"); if (f.late_t) m_cpu->set_state_int(tms320c54x_device::STATE_T, f.loaded_t()); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_recurring_irq_snapshot[3] = m_fast_stack_reads; m_recurring_irq_snapshot[4] = m_pipeline_register_writes;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RPTBD before delay Read, with retained T or in its body");
		}
		if (tick == f.read_clock() + 1)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(f.source, u16(~f.value));
			m_cpu->space(AS_PROGRAM).write_word(f.base + (f.ordinary && f.control == 5 ? 0x601 : 0x602) + f.slot, 0x3000);
		}
		if (f.branch && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0x620); }
		if (f.ordinary && f.control != 5 && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0x620); }
		if (tick == 80)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == f.count + 1 && m_pipeline_extensions_reads == f.count + 1,
				"Delay T load executes once and every count-plus-one body consumes it");
			if (f.branch) expect(m_cpu->state_int(STATE_GENPC) == f.base + (f.branch_count ? f.target() + 3 : 0x607), "BANZD retains the selected target or fallthrough despite target-literal poisoning");
			if (f.ordinary) expect(m_cpu->state_int(STATE_GENPC) == (f.control == 2 ? f.base + 0x607 : f.target_base() + f.target() + 3) && m_fast_stack_reads == unsigned(f.control == 5) && m_pipeline_register_writes == unsigned(f.control == 4 ? 2 : f.control == 3 ? 1 : 0), "Delay T load preserves its selected continuation and owned call/return frame transactions");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.loaded_t() && m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.branch || f.ordinary ? 13 : 0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.final_sp(),
				"RPTBD preserves loaded T, status and stack while draining BRC");
			expect(m_cpu->state_int(f.accumulator ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (f.ordinary && f.accumulator && (f.control == 1 || f.control == 2) ? f.control == 1 ? 0ULL : 1ULL : 0x123456789aULL),
				"The unselected accumulator remains untouched");
			for (unsigned i = 0; i < 8; ++i)
			{
				u16 const expected = f.branch && i == 7 ? u16(f.branch_count - 1) : i == 2 ? 0x4001 + f.count : i == 3 ? 0x3000 : i == 4 ? 0x2800 + (f.form == 2 ? 1 : f.form == 3 ? -1 : 0) : 0x3300 + i;
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == expected, "Delay DAGEN changes once while the body advances its own destination");
			}
			{
				auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
				expect(data.read_word(0x3fff) == 0xdead && data.read_word(0x4001 + f.count) == 0xdead, "RPTBD product buffer guards remain intact");
				for (unsigned i = 0; i <= f.count; ++i) expect(data.read_word(0x4000 + i) == f.product(), "Every product uses the original retained T sample");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_pipeline_register_writes = m_recurring_irq_snapshot[4];
				m_check_timer->adjust(attotime::never);
				if (f.ordinary) m_cpu->set_pipeline_frontend(true);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore delay-owned T without repeating the delay or losing the first product");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 600031; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.ordinary ? 55295U : 9215U)) { start_rptbd_t_load(variant + 1); return true; }
			if (f.ordinary) { osd_printf_info("TMS320C54x control delay T load: PASS cases=55296 restore=55296 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD patterns=4 forms=direct,plain,postinc,postdec cpl=2 slots=2 flags=4 accumulators=2 pages=2 snapshots=3 t_cohorts=original,complement,5aa5 delay_reads=1 t_publish=Read newer_t_preserved=1 selected_path_multiply=1 source_opcode_target_poison=1 owner_stack_only=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated control delay T load complete"); }
			if (f.branch) { osd_printf_info("TMS320C54x BANZD delay T load: PASS cases=9216 restore=9216 counters=0,1,7 patterns=4 forms=direct,plain,postinc,postdec cpl=2 slots=2 flags=4 accumulators=2 pages=2 snapshots=3 delay_reads=1 selected_path_multiply=1 source_opcode_target_poison=1 brc_preserved=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated BANZD delay T load complete"); }
			osd_printf_info("TMS320C54x RPTBD delay T load: PASS cases=9216 restore=9216 counts=0,1,7 patterns=4 forms=direct,plain,postinc,postdec cpl=2 slots=2 flags=4 accumulators=2 pages=2 snapshots=3 delay_reads=1 first_body_multiply=1 source_and_opcode_poison=1 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated RPTBD delay T load complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600031)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600030; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600020)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = repeated_mvmd_case(variant, profile_is("mvmd-rptz"));
		unsigned const checkpoint = f.snapshot == 0 ? 4 : f.snapshot == 1 ? f.read_clock : f.read_clock + f.count / 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeated MVMD AL/BL to ordinary RAM is admitted");
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save repeated MVMD setup, retained source or active repeat");
		if (tick == f.read_clock - 1) m_cpu->space(AS_PROGRAM).write_word(f.literal, 0x5000);
		if (tick == f.count + 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.count + 1 && m_pipeline_extensions_reads == 1,
				"Repeated MVMD executes count plus one fixed-source copies with one literal fetch");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x8620 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1e00 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef,
				"Repeated MVMD preserves status, stack and T");
			if (m_cpu->state_int(f.source == 8 ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) != (f.selected_after_clear() ^ 0xffffULL) ||
				m_cpu->state_int(f.source == 8 ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) != f.other_after_clear())
				osd_printf_info("MVMD accumulator mismatch: case=%u replay=%u a=%010llx b=%010llx expected_selected=%010llx\n", variant, unsigned(m_pipeline_replay),
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)(f.selected_after_clear() ^ 0xffffULL));
			expect(m_cpu->state_int(f.source == 8 ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (f.selected_after_clear() ^ 0xffffULL) &&
				m_cpu->state_int(f.source == 8 ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == f.other_after_clear(),
				"Only repeat clear and the fixture's deliberate AL/BL change affect accumulator state");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x3300 + i, "MVMD does not turn its fixed MMR into a DAGEN source");
			{
				auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
				expect(data.read_word(f.destination - 1) == 0xdead && data.read_word(f.destination + f.count + 1) == 0xdead, "Repeated MVMD respects destination guards");
				for (unsigned i = 0; i <= f.count; ++i) expect(data.read_word(f.destination + i) == f.sample(i), "Repeated MVMD publishes every old and changed source word in order");
			}
			if (replay_pipeline_at(600021, checkpoint,
				"Restore MVMD without replaying source changes or writes twice")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_repeated_mvmd(variant + 1); return true; }
			if (f.clear_accumulator < 2)
			{
				osd_printf_info("TMS320C54x RPTZ MVMD: PASS cases=1536 restore=1536 sources=AL,BL clear=A,B counts=0,1,39,255 patterns=4 flags=4 pages=2 snapshots=3 first_read_after_clear=1 opposite_source_unchanged=1 mid_repeat_source_change=1 literal_fetches=1 physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated RPTZ MVMD complete");
			}
			osd_printf_info("TMS320C54x repeated MVMD: PASS cases=1536 restore=1536 sources=AL,BL counts=0,1,39,255 patterns=4 flags=4 headers=short,long pages=2 snapshots=3 fixed_source_reads=each destination=EAR_increment literal_fetches=1 mid_repeat_source_change=1 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated repeated MVMD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600021)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600020; m_check_timer->enable(true);
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600001)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const accumulator = profile_is("st-acc-mvmm");
		unsigned const count = 1 + variant % 2, source = (variant / 2) % 8, destination = (variant / 16) % 8;
		unsigned const gap=(variant/128)%2, read_cycle = 3 + (accumulator ? 1 : 2) * count + gap;
		unsigned const checkpoint = read_cycle - 1 + (variant / 512)%3;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Store/MVMM chain remains supported");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick;
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save store/MVMM chain around Decode and Access");
		}
		if (tick == 20)
		{
			expect(m_pipeline_operand_reads == unsigned(!accumulator || gap) && m_pipeline_store_writes == count + 1 && m_pipeline_extensions_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + source) == 0x1200 + count &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + destination) == 0x1200 + count &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x55aa, "Store/MVMM composition preserves each pointer and bus access across replay");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(600002, "Restore retained store/MVMM chain");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (accumulator ? 24575 : 1535)) { start_pipeline_store_mvmm(variant + 1); return true; }
			if(accumulator) { osd_printf_info("TMS320C54x accumulator store MVMM: PASS cases=24576 restore=24576 forms=STL_A,STL_B,STH_A,STH_B values=4 stores=1,2 source_registers=8 destination_registers=8 gap_nops=0,1 pages=2 snapshots=3 pending_dagen_bypass=1 original_write_addresses=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated accumulator store MVMM complete"); }
			osd_printf_info("TMS320C54x staged store MVMM: PASS cases=1536 restore=1536 stores=1,2 source_registers=8 destination_registers=8 gap_nops=0,1 pages=2 snapshots=3 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated store MVMM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600002)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600001;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}


bool tms320c54x_test_state::check_memory_repeat_results()
{
	if (m_phase == 253230)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = memory_repeat_case(variant); unsigned const first = f.long_form ? 8 : 7, last = first + f.count;
		if (variant >= 38400)
		{
			unsigned const first_write = first + 1, last_write = first_write + 2 * f.count;
			unsigned const writes = tick < first_write ? 0 : std::min((tick - first_write) / 2 + 1, f.count + 1);
			// The qualified postincrement form updates in first-body Access;
			// other forms and later retained repeats keep their own schedule.
			unsigned const first_address = first_write - (f.form == 2 ? 2 : 1);
			unsigned const addresses = tick < first_address ? 0 : tick < first_write - 1 ? 1 : std::min((tick - first_write + 1) / 2 + 1, f.count + 1);
			unsigned const remaining = tick < first_write - 2 ? 0 : f.count - (writes ? writes - 1 : 0);
			u16 const ar = f.form == 2 ? 0x200 + addresses : f.form == 5 ? 0x400 - addresses : 0x200;
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_RC) == remaining && m_pipeline_operand_reads == writes &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == ar && m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1804,
				"Repeated ST separates address/pointer ownership from the following literal-write/count clock");
			if (f.interrupt && tick == 2) { int const line = f.interrupt == 2 ? INPUT_LINE_NMI : 0; m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE); }
			if (tick < last_write) expect(!m_vector_acks && !m_pipeline_store_writes, "Repeated store defers IRQ/NMI until its final write");
			if (tick == (f.long_form ? 4 : 3)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.body, 0xe811); }
			if (tick == (f.long_form ? 6 : 5)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.body + 1, 0xdead); }
			unsigned const checkpoint = std::array<unsigned, 4>{3, first_write - 1, first_write + f.count, last_write}[(variant - 38400) / 9600];
			if (tick == checkpoint && !m_pipeline_replay)
			{
				m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_store_writes, m_vector_acks, m_pipeline_store_case};
				expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeated store before/after address or data publication");
			}
			if (tick == last_write + 30)
			{
				expect(m_pipeline_operand_reads == f.count + 1 && m_pipeline_extensions_reads == 1 && m_pipeline_store_case == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_vector_acks == unsigned(bool(f.interrupt)) && m_pipeline_store_writes == (f.interrupt ? 2 : 0) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == ((f.flags & 4) ? 1 : 0),
					"Repeated ST finishes count+1 two-clock writes with one retained opcode/literal and deferred interrupt return");
				if (!m_pipeline_replay)
				{
					m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_pipeline_store_writes = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3]; m_pipeline_store_case = m_recurring_irq_snapshot[4];
					return restore_pipeline_at(253240, "Restore retained literal/pending address/count without duplicate store or pointer update");
				}
				m_pipeline_operand_tap.remove(); m_vector_prefetch_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
				if (variant < 76799) { start_pipeline_memory_repeat(variant + 1); return true; }
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				osd_printf_info("TMS320C54x staged repeat immediate ST: PASS cases=38400 forms=5 rpt=short,long counts=0,1,15,32,255 values=4 sst_sxm_ovm=8 sources=none,IRQ,NMI pages=2 restore=38400 count_plus_one=1 literal_fetches=1 body_fetches=1 destination_reads=0 cycles_each=2 silicon_phase_claim=0\n");
				if (profile_is("rpt-subc-st")) throw emu_fatalerror(0, "TMS320C54x isolated repeated SUBC and ST complete");
			}
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		unsigned const completed = tick < first ? 0 : std::min(tick - first + 1, f.count + 1);
		auto const expected = repeat_subc_result(f, completed);
		unsigned const remaining = tick < first - 1 ? 0 : tick <= first ? f.count : tick <= last ? last - tick : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_RC) == remaining &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination ? 0x1234 : expected[0]) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination ? expected[0] : 0x1234) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[1], "Repeat SUBC preserves per-iteration arithmetic and exact counter ownership");
		unsigned const reads = tick < first - 1 ? 0 : tick < first ? 1 : completed;
		expect(m_pipeline_operand_reads == reads, "Repeated SUBC samples one divisor per completed iteration");
		if (f.interrupt && tick == 2) { int const line = f.interrupt == 2 ? INPUT_LINE_NMI : 0; m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE); }
		if (tick < last) expect(!m_vector_acks && !m_pipeline_store_writes, "Repeat SUBC defers interrupt entry until the final quotient step");
		if (tick == (f.long_form ? 4 : 3)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.body, 0xe811); }
		unsigned const checkpoint = std::array<unsigned, 4>{3, first, first + f.count / 2, last}[variant / 9600];
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_store_writes, m_vector_acks};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeated division at Decode, first, middle or final iteration");
		}
		if (tick == last + 30)
		{
			u16 const ar = f.form == 4 || f.form == 2 ? 0x200 + f.count + (f.form == 2) : f.form == 5 ? 0x400 - f.count - 1 : 0x200;
			expect(m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_vector_acks == unsigned(bool(f.interrupt)) && m_pipeline_store_writes == (f.interrupt ? 2 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == ar, "Repeated SUBC finishes through its saved continuation with only deferred interrupt stack writes");
			if (!f.pattern && f.count == 15) expect(expected[0] == 0x20009, "TI SUBC division example produces remainder 2 and quotient 9 for 65/7");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_pipeline_store_writes = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3];
				return restore_pipeline_at(253240, "Restore repeat body, division counter, address and pending IRQ without restarting");
			}
			m_pipeline_operand_tap.remove(); m_vector_prefetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 38399) { start_pipeline_memory_repeat(variant + 1); return true; }
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			osd_printf_info("TMS320C54x staged repeat SUBC: PASS cases=38400 forms=5 rpt=short,long counts=0,1,15,32,255 sources=none,IRQ,NMI pages=2 restore=38400 count_plus_one=1 divisor_reads_each_iteration=1 duplicate_body_fetches=0 silicon_phase_claim=0\n");
			start_pipeline_memory_repeat(38400); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253240)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253230; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}



bool tms320c54x_test_state::check_mvdk_producer_results()
{
	if (m_phase == 255380)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const accumulator_destination = profile_is("mvdk-acc-prod");
		auto const f = mvdk_producer_case(variant, accumulator_destination);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Producer-composed MVDK stays within supported forms");
		if (tick == 3 + f.prefix) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601 + f.prefix, 0xdead); }
		if (tick == 4 + f.prefix && f.form == 2) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602 + f.prefix, 0xdead); }
		if (tick == f.access) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads, &m_pipeline_extensions_reads},
			"Save MVDK producer publication and retained source ownership");
		if (tick == 24)
		{
			{ auto const disable = machine().disable_side_effects();
				expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x600 + f.prefix + (f.form == 2 ? 4 : 3) &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 + unsigned(f.producer == 1) &&
					m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == unsigned(f.producer != 2) && m_fast_stack_reads == unsigned(f.producer == 1) &&
					m_pipeline_extensions_reads == (f.form == 2 ? 2U : 1U) && m_cpu->space(AS_DATA).read_word(f.destination) == (f.expected & (f.destination == 0xa || f.destination == 0xd ? 0xff : 0xffff)), "Producer-composed MVDK preserves the captured word and balances all transactions"); }
			if (replay_pipeline_at(255390, f.checkpoint,
				"Restore mapped producer without repeating or resampling the move")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (accumulator_destination ? 15551U : 5183U)) { start_mvdk_producer(variant + 1); return true; }
			if (accumulator_destination)
			{
				osd_printf_info("TMS320C54x MVDK accumulator producers: PASS cases=15552 restore=15552 sources=AL,AH,AG,BL,BH,BG destinations=AL,AH,AG,BL,BH,BG forms=3 producers=STL,POPM,LD gaps=0,1 patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 mapped_no_conflict=1 direct_ld_gap0=projection_only guard_width=8 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated MVDK accumulator producers complete");
			}
			osd_printf_info("TMS320C54x mapped MVDK producers: PASS cases=5184 restore=5184 sources=AL,AH,AG,BL,BH,BG forms=3 producers=STL,POPM,LD gaps=0,1 destinations=AR3,RAM patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 direct_alu_latency=retained direct_ld_gap0=projection_only mapped_no_conflict=1 physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated mapped MVDK producers complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255390) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255380; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mvdk_mapped_results()
{
	if (m_phase == 255360)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const accumulator_destination = profile_is("mvdk-acc");
		bool const clock = profile_is("clock-mvdk");
		auto const f = mvdk_mapped_case(variant, accumulator_destination, clock);
		u32 observed_instruction; u16 observed_opcode;
		expect(!m_cpu->observe_staged_data_read(f.source,observed_instruction,observed_opcode),
			"MVDK read ownership is transient outside the actual bus callback, including after restore");
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped MVDK remains in supported staged forms");
		if (tick == 3) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == 4 && f.form == 2) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0xdead); }
		if (tick == f.access && f.source != f.destination) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save mapped MVDK before Access, after capture or after publication");
		if (tick == 20)
		{
			{ auto const disable = machine().disable_side_effects();
				expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + (f.form == 2 ? 0x604 : 0x603) &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == (clock ? 0xa628 : 0xa600) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
					m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == (f.form == 2 ? 2U : 1U) && m_cpu->space(AS_DATA).read_word(f.destination) == (clock ? (f.value & ~u16(1)) | BIT(f.value,1) : (f.value & (f.destination == 0xa || f.destination == 0xd ? 0xff : 0xffff))),
					"Mapped MVDK retains the sampled word, flags and continuation across replay"); }
			if (accumulator_destination)
			{
				u64 a = 0x123456789aULL, b = 0xfedcba9876ULL;
				auto const field = [&a, &b](unsigned address, u16 word) {
					if (address < 8 || address > 0xd) return;
					unsigned const part = (address - 8) % 3, shift = part * 16;
					u64 const mask = (part == 2 ? 0xffULL : 0xffffULL) << shift;
					u64 &value = address >= 0xb ? b : a; value = (value & ~mask) | ((u64(word) << shift) & mask);
				};
				field(f.source, f.value); if (f.source != f.destination) field(f.source, 0xdead); field(f.destination, f.value);
				expect(m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b, "Mapped accumulator move preserves every other field and truncates only guard destinations");
			}
			if (replay_pipeline_at(255370, f.checkpoint,
				"Restore mapped MVDK without rereading source or repeating writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (clock ? 71U : accumulator_destination ? 8207U : 14255U)) { start_mvdk_mapped(variant + 1); return true; }
			if (clock)
			{
				osd_printf_info("TMS320C54x staged CLKMD MVDK: PASS cases=72 restore=72 forms=direct,indirect,absolute patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 peripheral_extra_clock=1 retained_source=1 retained_literals=1 read_owner=Address observer_scope=bus_only debug_excluded=1 pll_reference=unconfigured physical_clock_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated staged CLKMD MVDK complete");
			}
			if (accumulator_destination)
			{
				osd_printf_info("TMS320C54x MVDK accumulator destinations: PASS cases=8208 restore=8208 sources=18_mapped,1_RAM destinations=AL,AH,AG,BL,BH,BG forms=direct,indirect,absolute patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_sample=Access write=next_clock retained_literals=1 aliases=1 guard_width=8 other_fields_preserved=1 read_owner=Address observer_scope=bus_only debug_excluded=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated MVDK accumulator destinations complete");
			}
			osd_printf_info("TMS320C54x mapped MVDK: PASS cases=14256 restore=14256 sources=18 forms=direct,indirect,absolute destinations=11 patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_sample=Access mapped_aliases=1 producer_composition=unvalidated read_owner=Address observer_scope=bus_only debug_excluded=1 physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated mapped MVDK complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255370) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255360; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_long_mapped_original_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 399940)
	{
		unsigned const tick=++m_idle_fixture_tick, variant=m_pipeline_peripheral_case;
		u16 const length=std::array<u16,8>{12,0,6,7,8,13,21,143}[variant%8]; bool const emits=length>=7;
		if (tick==6 && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_extensions_reads,m_pipeline_register_writes,m_pipeline_store_writes,m_pipeline_drained_fetches};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save original long-mapped escape prefix");
		}
		if (tick==48)
		{
			u16 high,low,width,local_high,local_low;
			{ auto const disable=machine().disable_side_effects(); auto &data=m_cpu->space(AS_DATA); high=data.read_word(0x1400); low=data.read_word(0x1401); width=data.read_word(0x1402); local_high=data.read_word(0x140a); local_low=data.read_word(0x140b); }
			if (m_pipeline_drained_fetches) osd_printf_info("long_mapped_original_result: case=%u low=%04x expected=%04x\n",variant,low,emits?7:0x8888);
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE),"Original escape prefix completes cleanly");
			expect(!m_pipeline_drained_fetches && m_pipeline_operand_reads==1 && m_pipeline_extensions_reads==unsigned(emits?2:1) && m_pipeline_register_writes==unsigned(emits) && m_pipeline_store_writes==unsigned(emits?5:0),"Original escape forwards its pending double store to the same-address load");
			expect(high==(emits?0:0xaaaa) && low==(emits?7:0xbbbb) && width==(emits?3:0xcccc) && local_high==(emits?0:0x7777) && local_low==(emits?7:0x8888) && m_cpu->state_int(tms320c54x_device::STATE_B)==7,"Original escape emits the threshold and preserves real local writes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay=true; m_idle_fixture_tick=6;
				m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads=m_recurring_irq_snapshot[1]; m_pipeline_register_writes=m_recurring_irq_snapshot[2]; m_pipeline_store_writes=m_recurring_irq_snapshot[3]; m_pipeline_drained_fetches=m_recurring_irq_snapshot[4];
				return restore_pipeline_at(399950, "Restore original long-mapped escape prefix");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant<127) { start_long_mapped_original(variant+1); return true; }
			osd_printf_info("TMS320C54x original escape store/load: PASS cases=128 restore=128 lengths=8 stale_values=4 sxm=2 pages=2 mapped_reads=BL,AL writes=0,5 backing_reload_reads=0 silicon_phase_claim=0\n");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase==399950) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=399940; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}



bool tms320c54x_test_state::check_staged_conditional_store_results()
{
	if (m_phase == 620200)
	{
		bool const mapped = profile_is("cond-store-ar");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = staged_conditional_store_case(variant);
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (f.edge && tick >= 4), "Conditional store rejects mapped RAM before pointer or bus effects");
		if (tick == 3) { m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + 2 + (f.mode & 3), f.address); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x600, 0xdead); }
		for (unsigned ar = mapped ? 0 : 2; ar < unsigned(mapped ? 8 : 6); ++ar)
		{
			u16 expected = !f.edge && tick > 4 ? 0x3300 + ar : ar != 2 + (f.mode & 3) || tick < 3 ? 0x2200 + ar : !f.edge && tick == 4 ? f.final_ar : f.address;
			if (mapped)
			{
				bool const target = ar == f.address - 0x10, pointer = ar == 2 + (f.mode & 3);
				expected = target ? f.memory : ar ? 0x2200 + ar : 3;
				if (pointer && tick >= 3) expected = tick == 3 ? f.address : tick == 4 ? f.final_ar : target ? f.memory : 0x3300 + ar;
				else if (ar >= 2 && ar < 6 && !target && tick >= 5) expected = 0x3300 + ar;
				if (target && tick >= 6) expected = f.output;
			}
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == expected, "Conditional store captures one address and modifies it once regardless of condition");
		}
		if (tick == 4 && !f.edge) for (unsigned ar = 2; ar < 6; ++ar) if (!mapped || ar != f.address - 0x10) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x3300 + ar);
		if (tick == 5 && !f.edge)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.brc); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1 ^ 31);
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, f.memory ^ 0xffff);
		}
		unsigned const checkpoint = (f.edge ? 3 : 4) + f.snapshot;
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save conditional store before/after address, readback and publication"); }
		if (tick == 16)
		{
			expect(m_pipeline_operand_reads == unsigned(!f.edge) && m_pipeline_store_writes == unsigned(!f.edge) && m_pipeline_extensions_reads == 1 && bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == !f.edge && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa630 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.edge ? f.st1 : f.st1 ^ 31) && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.edge ? f.a ^ 0x1111111111ULL : f.a) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.edge ? f.b ^ 0x1111111111ULL : f.b) && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.edge ? f.t ^ 0xffff : f.t) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.edge ? f.brc ^ 0xffff : f.brc) && m_cpu->state_int(tms320c54x_device::STATE_BK) == f.bk && m_cpu->state_int(tms320c54x_device::STATE_AR0) == (mapped && f.address == 0x10 ? f.output : 3), "Conditional store preserves source registers and flags with one read/write or no rejected effects");
			if (!f.edge) { auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.address) == f.output && m_cpu->state_int(STATE_GENPC) == f.base + 0x602, "Conditional store writes retained false sample or selected source before a fresh continuation"); }
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(620201, "Restore conditional store after clearing live pipeline state"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < (mapped ? 221184U : f.edge ? 1024U : 442368U)) { start_staged_conditional_store(variant + 1); return true; }
			if (mapped) { osd_printf_info("TMS320C54x conditional AR stores: PASS cases=221184 restore=221184 forms=4 predicates=12 patterns=6 xmem=16 destinations=AR0..AR7 asm=0,-1 flags=pattern_coupled bk=0 pages=2 snapshots=3 false_read_writeback=1 address_sample=Access read_after_pointer_update=1 predicate_source_sample=Execute pointer_target_alias=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated conditional AR stores complete"); }
			if (f.edge) { osd_printf_info("TMS320C54x conditional store exclusions: PASS cases=1024 restore=1024 forms=4 xmem=16 addresses=4 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 reset_before_restore=1 silicon_trap_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated conditional store exclusions complete"); }
			osd_printf_info("TMS320C54x staged conditional stores: PASS cases=442368 restore=442368 forms=STRCD,SRCCD,SACCDA,SACCDB predicates=12 patterns=6 xmem=16 shifts=4 flags=4 pages=2 snapshots=3 false_read_writeback=1 address_sample=Access asm_sample=Read predicate_source_sample=Execute saturation_ignored=1 read_retained=1 pointer_updates=once reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated staged conditional stores complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 620201) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 620200; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}




bool tms320c54x_test_state::check_mapped_dual_results()
{
	if (m_phase == 62170)
	{
		bool const edge = profile_is("alu-map-edge");
		bool const interrupt_registers = profile_is("alu-map-irq");
		bool const low_bus = profile_is("alu-low-bus");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = mapped_dual_case(variant, edge, interrupt_registers, low_bus);
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (edge && tick >= 4), "Dual ALU rejects unowned sources before Read and pointer updates");
		if (!edge && tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			expect(m_cpu->state_int(tms320c54x_device::STATE_IMR) == f.imr && m_cpu->state_int(tms320c54x_device::STATE_IFR) == f.ifr, "Dual source reads do not acknowledge IFR or change IMR");
			for (u16 at : {f.xaddress, f.yaddress}) if (at >= 2) m_cpu->space(AS_DATA).write_word(at, 0x5a5a);
			if (interrupt_registers)
			{
				// Change both sources without making the terminal IDLE wake
				// on IMR & IFR, independently of INTM's vector-entry mask.
				m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x0f0f);
				m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xf0f0);
			}
		}
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped dual operands before/after Read and Execute");
		}
		if (tick == 12)
		{
			u64 a = f.a, b = f.b; auto final = f.final;
			if (!edge)
				for (u16 at : {f.xaddress, f.yaddress})
				{
					if (at >= 8 && at <= 13)
					{
						unsigned const lane = at <= 10 ? at - 8 : at - 11, shift = lane * 16;
						u64 &acc = at <= 10 ? a : b; u64 const mask = lane == 2 ? 0xff : 0xffff;
						acc = (acc & ~(mask << shift)) | ((0x5a5a & mask) << shift);
					}
					if (at >= 0x10 && at <= 0x17) final[at - 0x10] = 0x5a5a;
				}
			if (!edge) (f.destination ? b : a) = f.result;
			bool const valid = m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (edge ? f.st0 : f.result_st0) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_BK) == f.bk && m_pipeline_operand_reads == (edge ? 0U : 2U) &&
				!m_pipeline_store_writes && bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == !edge;
			if (!valid) osd_printf_info("mapped_dual_result: case=%u replay=%u a=%010llx/%010llx b=%010llx/%010llx st0=%04llx/%04x st1=%04llx/%04x sp=%04llx bk=%04llx read=%u write=%u idle=%llu\n", variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)a, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)b, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST0), edge ? f.st0 : f.result_st0, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), f.st1, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BK), m_pipeline_operand_reads, m_pipeline_store_writes, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE));
			expect(valid, "Mapped dual arithmetic uses retained signed/unsigned words with independent carry/overflow and no data write");
			for (unsigned i = 0; i < 8; ++i)
			{
				if (m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) != final[i])
					osd_printf_info("Mapped dual pointer mismatch case=%u edge=%u replay=%u ar=%u expected=%04x actual=%04x\n", variant, unsigned(edge), unsigned(m_pipeline_replay), i, final[i], unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i)));
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == final[i], "Mapped dual address updates are not repeated after Read or restoration");
			}
			for (unsigned i = 0; i < 2; ++i)
				expect(m_cpu->state_int(i ? tms320c54x_device::STATE_TRN : tms320c54x_device::STATE_T) ==
					(!edge && (f.xaddress == 14 + i || f.yaddress == 14 + i) ? 0x5a5a : i ? 0x7fff : 0x8001), "Dual arithmetic preserves post-Read T/TRN values");
			if (interrupt_registers) expect(m_cpu->state_int(tms320c54x_device::STATE_IMR) == 0x0f0f && m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0xf0f0, "Execute and restoration do not resample or acknowledge interrupt sources");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(true);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore mapped dual operands after clearing the pipeline");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62171; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < (edge ? 3552U : interrupt_registers ? 18432U : low_bus ? 36864U : 147456U)) { start_mapped_dual(variant + 1); return true; }
			if (edge)
			{
				osd_printf_info("TMS320C54x mapped dual ALU exclusions: PASS cases=3552 restore=3552 addresses=74 lanes=3 operations=ADD,SUB accumulators=2 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 reset_before_restore=1 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated mapped dual ALU exclusions complete");
			}
			if (interrupt_registers) { osd_printf_info("TMS320C54x interrupt-register dual ALU: PASS cases=18432 restore=18432 registers=IMR,IFR lanes=3 selector_pairs=8 operations=ADD,SUB accumulators=2 sxm_ovm=4 bk_modes=2 patterns=2 pages=2 snapshots=3 interrupt_masked=1 reads_do_not_ack=1 source_sample=Read post_read_poison=1 reads=2 writes=0 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated interrupt-register dual ALU complete"); }
			if (low_bus) { osd_printf_info("TMS320C54x low-bus dual ALU: PASS cases=36864 restore=36864 addresses=0002..0005 lanes=3 selector_pairs=8 operations=ADD,SUB accumulators=2 sxm_ovm=4 bk_modes=2 patterns=2 pages=2 snapshots=3 source_sample=Read address_update=Access post_read_poison=1 reads=2 writes=0 reset_before_restore=1 endpoint_identity_claim=0 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated low-bus dual ALU complete"); }
			osd_printf_info("TMS320C54x mapped dual ALU: PASS cases=147456 restore=147456 mapped_sources=16 lanes=3 selector_pairs=8 operations=ADD,SUB accumulators=2 sxm_ovm=4 bk_modes=2 patterns=2 pages=2 snapshots=3 source_sample=Read address_update=Access post_read_poison=1 reads=2 writes=0 reset_before_restore=1 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated mapped dual ALU complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62171) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62170; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}






bool tms320c54x_test_state::check_popd_mapped_results()
{
	if (m_phase == 254200)
	{
		auto const &f = m_popd_fixture; unsigned const tick = ++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped and repeated POPD stays in the retained frontend");
		if (tick == 3 && f.irq) { if (f.irq == 1) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1); else { m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); } }
		if (tick == 3 && f.prefix == 2) { auto const disable = machine().disable_side_effects(); for (u32 const base : {0U, 0x20000U}) m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xffff); }
		unsigned const reads = std::min(f.count + 1, tick >= f.read_clock() ? tick - f.read_clock() + 1 : 0);
		expect(m_pipeline_operand_reads == reads && m_popd_write_mask == f.write_mask(tick) && m_pipeline_store_writes == f.write_count(tick), "POPD retains exact stack and destination transactions across replay");
		if (!f.repeated && tick == 4 && BIT(f.mode, 7)) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR7) == u16(f.pointer + f.step), "POPD modifies its destination pointer once at Access"); m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x2222); }
		if (tick >= f.read_clock() && tick <= f.read_clock(f.count)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x3000 + tick - f.read_clock(), 0xdead); }
		if (tick <= f.read_clock() + f.count + 1 || tick == 96)
		{
			auto const disable = machine().disable_side_effects();
			unsigned const accessed = std::min(f.count + 1, tick >= 4 + f.prefix ? tick - (4 + f.prefix) + 1 : 0);
			for (unsigned r = 0; r < f.initial.size(); ++r)
			{
				u16 const address = popd_fixture::reg(r); u16 expected = f.initial[r];
				if (address == 0x17 && BIT(f.mode, 7)) expected = !f.repeated && tick >= 4 ? 0x2222 : u16(f.pointer + f.step * accessed);
				for (unsigned i = 0; i <= f.count; ++i) if (address == f.destination[i] && tick >= f.write_clock(i)) expected = (address == 0xa || address == 0xd) ? f.word[i] & 0xff : f.word[i];
				u16 const actual = m_cpu->space(AS_DATA).read_word(address);
				if (actual != expected) osd_printf_info("popd_register: case=%u replay=%u tick=%u address=%04x actual=%04x expected=%04x\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), tick, address, actual, expected);
				expect(actual == expected, "POPD changes only its retained destination at the documented DAGEN/data publication stage");
			}
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 + reads && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (f.cmpt && BIT(f.mode, 7) && accessed ? (f.st0 & 0x1fff) | 0xe000 : f.st0), "POPD increments SP once per Read and retains only the legitimate ARP update");
		}
		if (tick == f.snapshot && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save POPD ownership, count, partial register restoration and deferred interrupt"); }
		if (tick == 96)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x602 + f.prefix && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1 & ~(f.irq ? 0x800 : 0)) && m_pipeline_extensions_reads == 1 && m_port_reads == unsigned(f.prefix == 2) && m_vector_acks == unsigned(bool(f.irq)) && m_fast_stack_reads == unsigned(bool(f.irq)) && m_pipeline_register_writes == unsigned(bool(f.irq)), "POPD completes on the exact caller page with balanced IRQ frame and preserved status");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.snapshot; m_pipeline_operand_reads = f.snapshot >= f.read_clock() ? std::min(f.count + 1, f.snapshot - f.read_clock() + 1) : 0;
				m_popd_write_mask = f.write_mask(f.snapshot); m_pipeline_store_writes = f.write_count(f.snapshot); m_pipeline_extensions_reads = 1; m_port_reads = unsigned(f.prefix == 2); m_fast_stack_reads = m_pipeline_register_writes = m_vector_acks = 0;
				return restore_pipeline_at(254210, "Restore POPD without duplicating a stack read, mapped write, modifier or interrupt");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < (f.repeated ? 6527 : 4031)) { start_popd_mapped(m_pipeline_peripheral_case + 1); return true; }
			if (f.repeated) { osd_printf_info("TMS320C54x repeated POPD: PASS cases=6528 restore=6528 layouts=RAM,fixed,forward_alias,back_alias,PMST_to_BK,AR6_to_BL forms=short,long counts=0,1,15,4,11 patterns=4 cmpt=2 pages=2 snapshots=4 sources=none,IRQ,NMI reads_each=1 writes_each=1 body_fetches=1 literal_fetches=once read_before_older_write=1 interrupt_return=1 active_block_acceptance=0 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated repeated POPD complete"); }
			osd_printf_info("TMS320C54x mapped POPD: PASS cases=4032 restore=4032 registers=21 forms=direct,postincrement,postdecrement patterns=4 cmpt=2 pages=2 snapshots=4 reads=1 writes=1 captured_destination=1 captured_source=1 dagen_write=Read data_write=Execute active_block_acceptance=0 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated mapped POPD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254210) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254200; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254220)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, c = variant >= 15 ? variant - 15 : 0;
		bool const conflict = variant >= 15, active = variant >= 8 && variant < 12;
		// RPTB flushes the speculative body and refetches it before Access.
		unsigned const fail = active ? 8 : variant == 12 || variant == 13 ? 5 : variant == 14 ? 3 : 4, checkpoint = conflict ? 6 : fail - 1;
		u16 const destination = conflict ? std::array<u16, 4>{0x10, 0x12, 0x17, 0x19}[c % 4] : variant < 8 ? std::array<u16, 8>{0, 1, 6, 7, 0x18, 0x1e, 0x24, 0x58}[variant] : active ? 0x1a + variant - 8 : 0x4000;
		u16 const writer = (c / 4) % 2 ? destination : destination == 0x12 ? 0x11 : 0x12;
		if (bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) != (!conflict && tick >= fail)) osd_printf_info("popd_edge_stop: case=%u replay=%u tick=%u cycle=%llu illegal=%u expected_at=%u\n", variant, unsigned(m_pipeline_replay), tick, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), fail);
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (!conflict && tick >= fail), "POPD fails closed at Decode/Access/Read before consuming an unsupported transaction");
		expect(m_pipeline_operand_reads == unsigned(conflict && tick >= 6) && m_pipeline_store_writes == (conflict ? unsigned(tick >= 6) + unsigned(tick >= 7) : 0), "POPD edge replay neither rereads the stack nor duplicates an old/new bank write");
		if (conflict && tick == 5) { m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x3333); }
		if (conflict && tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x3000, 0xdead); }
		if (tick == checkpoint && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save rejected or deferred POPD before its decisive stage"); }
		if (tick == (conflict ? 12 : fail))
		{
			if (conflict)
			{
				auto const disable = machine().disable_side_effects();
				expect(m_cpu->space(AS_DATA).read_word(destination) == 0x2468 && (writer == destination || m_cpu->space(AS_DATA).read_word(writer) == 0x1357) && m_cpu->state_int(tms320c54x_device::STATE_AR7) == (destination == 0x17 ? 0x2468 : 0x3333) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3001 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == (c >= 8 ? 0x20603 : 0x603), "Deferred POPD retains its source/destination and reaches the exact caller continuation");
			}
			else expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (variant == 13 ? 0xe : 0x3000) && m_cpu->state_int(tms320c54x_device::STATE_AR7) == (variant == 13 ? 0x4001 : destination), "Rejected POPD never consumes SP or applies a forbidden destination modifier");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = conflict ? 1 : 0; m_pipeline_store_writes = conflict ? 1 : 0; m_popd_write_mask = conflict ? 1 : 0;
				return restore_pipeline_at(254230, "Restore POPD rejection/deferred-write ownership without repeating effects");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 30) { start_popd_edges(variant + 1); return true; }
			osd_printf_info("TMS320C54x POPD edges: PASS cases=31 restore=31 exclusions=15 dagen_conflicts=16 conflict_read=6 conflict_write=7 same_and_different_registers=1 active_controls_rejected=4 memory_count_rejected=1 source_capture=1 destination_capture=1 silicon_phase_claim=0\n"); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254230) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254220; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}











bool tms320c54x_test_state::check_memory_copy_repeat_results()
{
	if (m_phase == 62154)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; bool const wrap = profile_is("rpt-mem-wrap"), chain = profile_is("rpt-mem-chain"); auto const f = memory_copy_repeat_case(variant, wrap, chain);
		unsigned const checkpoint = std::array<unsigned, 4>{3, f.read_clock(), f.read_clock() + 2, f.read_clock() + 6}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Memory-sourced count composes with pipelined data copies");
		if (tick == f.read_clock()) set_repeat_count_source(f.address, f.address == 0x18 ? 0x3000 : 0xa55a);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_pipeline_store_case, m_pipeline_register_writes, m_fast_stack_reads, m_vector_acks}; m_saved_observed_instructions = m_observed_instructions; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeat before and after count capture or body issue");
		}
		if (tick == 120)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.body + f.body_words() + (chain ? 16 : 13) && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1fff) == (f.st0 & 0x1fff), "Memory repeat finishes its captured count and post-loop sentinel with balanced interrupt state");
			expect(m_pipeline_extensions_reads == (chain ? 2 : 1) && m_pipeline_store_case == (chain ? 2 : 1) && m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.count + 1 && m_pipeline_register_writes == unsigned(bool(f.irq)) && m_fast_stack_reads == unsigned(bool(f.irq)) && m_vector_acks == unsigned(bool(f.irq)), "Memory repeat neither re-fetches its body nor re-samples its mutated count");
			if (chain) expect(std::count(m_observed_instructions.begin(), m_observed_instructions.end(), std::pair<offs_t, u16>{f.body + f.body_words() + 2, 0xf495}) == 4, "Following NOP repeat retires its own count plus one without a stale copy opcode");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.source_ar) == 0x5000 + (f.body_kind == 1 ? 0 : f.count + 1) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.destination_ar) == 0x6000 + (f.body_kind == 2 ? 0 : f.count + 1) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.sentinel_ar) == 0x7201 + f.sentinel_ar, "Memory count drives exactly count plus one auxiliary updates and one continuation");
			if (f.form == 21 || f.form == 23) expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == f.address + 1, "Indirect repeat count updates its address only once");
			{ auto const disable = machine().disable_side_effects(); u16 const expected = f.address == 0x18 ? 0x3000 : f.address == 10 || f.address == 13 ? 0x5a : 0xa55a; expect(m_cpu->space(AS_DATA).read_word(f.address) == expected, "Count capture never republishes its old source value"); for (unsigned i = 0; i <= f.count; ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == f.payload(i), "Every repeated destination preserves the raw source word"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_pipeline_store_case = m_recurring_irq_snapshot[3]; m_pipeline_register_writes = m_recurring_irq_snapshot[4]; m_fast_stack_reads = m_recurring_irq_snapshot[5]; m_vector_acks = m_recurring_irq_snapshot[6]; m_observed_instructions = m_saved_observed_instructions; return restore_pipeline_at(62155, "Restore memory count and pending copies without duplicate transactions");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (wrap ? 191U : chain ? 383U : 27647U)) { start_memory_copy_repeat(variant + 1); return true; }
			if (chain) { osd_printf_info("TMS320C54x memory-count chaining: PASS cases=384 restore=384 count_sources=AL,RAM bodies=MVDD,MVKD,MVDK then=RPT_NOP first_counts=0,1,7,31 second_count=3 flag_states=2 pages=2 snapshots=4 stale_body=excluded physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated memory-count chaining complete"); }
			if (wrap) { osd_printf_info("TMS320C54x memory-count wrap: PASS cases=192 restore=192 count_sources=AL,RAM counts=0,1,7,31 bodies=MVDD,MVKD,MVDK pages=2 snapshots=4 header=fffe,ffff body=0000 count_reads=once physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated memory-count wrap complete"); }
			osd_printf_info("TMS320C54x memory-count copies: PASS cases=27648 restore=27648 count_sources=24 owned_mapped_sources=18 counts=0,1,7,31 bodies=MVDD,MVKD,MVDK flag_states=2 pages=2 sources=none,IRQ,NMI snapshots=4 patterns=2 count_reads=once mutation_after_read=1 body_fetches=once physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated memory-count copies complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62155) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62154; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_repeated_mvdk_results()
{
	if (m_phase == 62148)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const indexed = profile_is("mvdk-index-rpt");
		auto const f = repeated_mvdk_case(variant, indexed); unsigned const checkpoint = 5 + f.snapshot;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeated MVDK remains within admitted compositions");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeated MVDK before and during its retained copies");
		}
		if (tick == 32)
		{
			expect(bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) && m_pipeline_operand_reads == (f.count + 1) && m_pipeline_store_writes == (f.count + 1) && m_pipeline_extensions_reads == 1, "Repeated MVDK copies count plus one words");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_BK) == f.bk && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef, "Repeated MVDK preserves unselected registers and status");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == f.final[i], "Repeated MVDK updates only its selected pointer once per copy");
			for (unsigned i = 0; i <= f.count; ++i) { auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(u16(f.destination + i)) == f.values[i], "Repeated MVDK publishes consecutive destination words"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; if (indexed) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(62149, "Restore repeated MVDK without duplicate accesses");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (indexed ? 9215U : 105983U)) { start_repeated_mvdk(variant + 1); return true; }
			if (indexed)
			{
				osd_printf_info("TMS320C54x repeated indexed MVDK: PASS cases=8928 restore=8928 source_geometries_skipped=288 encodings=16 counts=4 placements=6 cmpt=2 cpl=2 pages=2 snapshots=3 ordered_samples=1 destination_literal_once=1 mapped_access=excluded reset_before_restore=1 physical_phase_claim=0 interrupt_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated repeated indexed MVDK complete");
			}
			osd_printf_info("TMS320C54x repeated MVDK: PASS cases=105696 restore=105696 source_geometries_skipped=288 encodings=184 counts=4 placements=6 compatibility_states=2 cpl_states=2 pages=2 snapshots=3 mapped_access=excluded physical_phase_claim=0 interrupt_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated repeated MVDK complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62149) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62148; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62146)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const indexed = profile_is("mvdk-index");
		unsigned const key = circular_move_schedule::key(variant);
		auto const c = indexed ? indexed_move_case(key) : circular_move_case(key); auto const &f = c.memory;
		unsigned const checkpoint = 4 + f.snapshot;
		// Literal destinations are checked in Decode; resolved source addresses in Access.
		bool const unsupported_destination = c.destination < 0x60 && (c.destination < 8 || c.destination > 0x1a);
		unsigned const rejection_tick = unsupported_destination ? 4 : 5;
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (c.excluded && tick >= rejection_tick), "Circular MVDK excludes unsupported sources/destinations before side effects");
		if (tick == 5 && !c.excluded)
		{
			for (unsigned ar = 0; ar < 8; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == c.final[ar], "Circular MVDK samples before source update and aliasing DAGEN publication");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2200);
			if (f.address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, c.word ^ 0xffff); }
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save circular MVDK around sample and destination publication");
		}
		if (tick == 12)
		{
			bool const transfers = bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == !c.excluded && m_pipeline_operand_reads == unsigned(!c.excluded) && m_pipeline_store_writes == unsigned(!c.excluded) && m_pipeline_extensions_reads == 1;
			if (!c.excluded) expect_opcode(0x7100 | f.mode, transfers, "Circular MVDK owns one sample, literal and publication");
			else expect(transfers, "Excluded circular MVDK performs no transfer");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == c.a && m_cpu->state_int(tms320c54x_device::STATE_B) == c.b && m_cpu->state_int(tms320c54x_device::STATE_T) == c.t && m_cpu->state_int(tms320c54x_device::STATE_TRN) == c.trn && m_cpu->state_int(tms320c54x_device::STATE_SP) == c.sp && m_cpu->state_int(tms320c54x_device::STATE_BK) == c.bk && m_cpu->state_int(tms320c54x_device::STATE_BRC) == c.brc && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "Circular MVDK preserves unselected fields and raw values independently of ALU modes");
			for (unsigned ar = 0; ar < 8; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (!c.excluded && ar == f.selected ? 0x2200 : c.final[ar]), "Circular MVDK does not re-read or reapply pointer/destination updates");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; if (indexed) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(62147, "Restore circular MVDK without duplicate reads or writes");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_circular_move_coverage.add(key);
			m_circular_move_excluded += c.excluded;
			if (variant + 1 < circular_move_schedule::cases) { start_circular_move(variant + 1); return true; }
			if (indexed)
			{
				osd_printf_info("TMS320C54x indexed MVDK: PASS cases=%u restore=%u excluded=%u encodings=16 destination_kinds=21 mapped_sources=18 indices=1,-1,BK,-BK snapshots=balanced raw_words=1 alias_order=sample,modify,publish reset_before_restore=1 physical_phase_claim=0 interrupt_repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_circular_move_coverage.count, m_circular_move_coverage.count, m_circular_move_excluded, (unsigned long long)m_circular_move_coverage.value); throw emu_fatalerror(3, "TMS320C54x isolated indexed MVDK complete");
			}
			for (u16 mode = 0xc0; mode <= 0xdf; ++mode) expect(m_asserted_opcodes.test(0x7100 | mode), "Every circular MVDK encoding has an executed transfer assertion");
			osd_printf_info("TMS320C54x circular MVDK: PASS cases=%u restore=%u excluded=%u encodings=32 asserted_encodings=32 destination_kinds=21 mapped_sources=18 native_bk=82 snapshots=balanced raw_words=1 alias_order=sample,modify,publish physical_phase_claim=0 interrupt_repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_circular_move_coverage.count, m_circular_move_coverage.count, m_circular_move_excluded, (unsigned long long)m_circular_move_coverage.value); throw emu_fatalerror(3, "TMS320C54x isolated circular MVDK complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62147) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62146; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}




} // namespace c54x_test
