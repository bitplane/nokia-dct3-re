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
	return false;
}











bool tms320c54x_test_state::check_circular_move_results()
{
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
