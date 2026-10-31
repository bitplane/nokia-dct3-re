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













void tms320c54x_test_state::stack_mmr_w(offs_t offset, u16 value)
{
	m_stack_mmr[offset] = value;
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
