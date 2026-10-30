// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_memory_immediate_schedule.h"
#include "tms320c54x_test_circular_move_schedule.h"
#include "tms320c54x_test_parallel_schedule.h"

namespace c54x_test {

















tms320c54x_test_state::extended_ar_store_fixture tms320c54x_test_state::extended_ar_store_case(unsigned variant, bool consumer, bool shifted)
{
	extended_ar_store_fixture f{};
	f.shifted = shifted;
	unsigned const span = consumer ? 6144 : 24576;
	f.shift = shifted ? std::array<int, 5>{-16, -1, 0, 1, 15}[variant / span] : 0;
	unsigned v = variant % span;
	if (!consumer) { f.form = v % 4; v /= 4; }
	f.reg = v % 8; v /= 8;
	f.kind = v % 4; v /= 4;
	f.value = std::array<u64, 4>{0x123456789aULL, 0x0180001200ULL, 0xff7fff1200ULL, 0xfffffff495ULL}[v % 4]; v /= 4;
	if (shifted && consumer && f.value < 0x8000000000ULL) f.value |= 0x7f00000000ULL;
	f.flags = v % 4; v /= 4; // SST and SXM, not arithmetic OVM.
	bool const alias = !consumer && v % 2;
	if (consumer) f.gap = 2 + v % 2;
	v /= 2;
	f.base = v % 2 ? 0x20000 : 0;
	f.selected = alias ? f.reg : (f.reg + 3) % 8;
	u16 const target = 0x10 + f.reg;
	f.literal = f.form == 0 ? target : f.form == 1 ? u16(target - 0x40) : 1;
	f.initial_ar = f.form == 0 ? 0x4000 + f.selected : f.form == 1 ? 0x40 : f.form == 2 ? target - 1 : 0x10 + (f.reg + 7) % 8;
	f.next_ar = f.form < 2 ? f.initial_ar : target;
	u64 bits = f.value;
	for (int i = 0; i < f.shift; ++i) bits = (bits * 2) & 0xffffffffffULL;
	for (int i = 0; i < -f.shift; ++i) bits = (bits >> 1) | ((f.flags & 2) && (bits & 0x8000000000ULL) ? 0x8000000000ULL : 0);
	u32 stored = u32(bits);
	if (f.flags & 1)
	{
		if (f.flags & 2)
		{
			s64 const signed_value = s64(bits << 24) >> 24;
			stored = signed_value < -0x80000000LL ? 0x80000000U : signed_value > 0x7fffffffLL ? 0x7fffffffU : u32(signed_value);
		}
		else if (bits > 0xffffffffULL) stored = 0xffffffffU;
	}
	f.result = (f.kind & 2) ? stored >> 16 : stored;
	return f;
}

void tms320c54x_test_state::start_extended_ar_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const consumer = profile_is("st-ar-consumer") || profile_is("st-shift-ar-use");
	bool const shifted = profile_is("st-shift-ar") || profile_is("st-shift-ar-use");
	auto const f = extended_ar_store_case(variant, consumer, shifted);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	u16 const mode = f.form == 0 ? 0xf8 : f.form == 1 ? 0xe0 | f.selected : f.form == 2 ? 0xe8 | f.selected : 0xf0 | f.selected;
	program.write_word(f.base + 0x600, shifted ? 0x6f00 | mode : 0x8000 | (f.kind << 8) | mode);
	program.write_word(f.base + 0x601, f.literal);
	if (shifted) program.write_word(f.base + 0x602, (f.kind & 2 ? 0x0c60 : 0x0c80) | ((f.kind & 1) << 8) | (f.shift & 31));
	if (consumer) program.write_word(f.base + 0x602 + shifted + f.gap, 0x1280 | (((f.kind & 1) ^ 1) << 8) | f.reg);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, f.flags & 1}, {cpu_regs::STATE_ST0, 0xa608},
		{cpu_regs::STATE_ST1, 0x4800 | ((f.flags & 2) ? 0x100 : 0)},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 8);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x4000 + ar);
	if (f.form) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, f.initial_ar);
	if (consumer) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.reg, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, (f.kind & 1) ? 0xdead : consumer ? f.value : 0x11112222);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, (f.kind & 1) ? consumer ? f.value : 0x11112222 : 0xdead);
	if (consumer && f.result >= 0x60) data.write_word(f.result, 0xbbbb);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "extended_ar_store_reads", [this, f, consumer](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(consumer && !m_pipeline_operand_reads && at == f.result && word == (f.result ? 0xbbbb : 0) && m_cpu->pipeline_cycles() == 7 + f.shifted + f.gap,
			"Accumulator store has no destination read; legal-gap LDU consumes the published AR once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "extended_ar_store_publication", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(!m_pipeline_store_writes && at == 0x10 + f.reg && word == f.result && m_cpu->pipeline_cycles() == 7 + f.shifted,
			"Extended STL/STH publishes the Execute-live saturated accumulator word to retained AR destination once"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "extended_ar_store_literal", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(!m_pipeline_extensions_reads && word == f.literal, "Extended AR store retains one address or offset literal"); ++m_pipeline_extensions_reads; });
	m_phase = 254560; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


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

void tms320c54x_test_state::verify_paired_circular_domain()
{
	struct guard_case { u16 initial, bk; int step; bool legal; u16 next; };
	static constexpr guard_case cases[] = {
		{0x1007,16,-2923,false,0}, {0x4007,8192,-2923,true,0x549c},
		{0x1007,16,4103,false,0}, {0x1007,8192,4103,true,0x000e},
		{0x1007,8192,-4103,true,0}, {0x5007,16,32767,false,0},
		{0x5007,32768,32767,true,0x5006}, {0x5007,32768,-32768,true,0x5007},
		{0x0010,16,1,false,0}, {0x001f,31,-1,false,0},
		{0x000f,16,1,true,0}, {0,16,-1,true,15},
		{0x001e,31,1,true,0}, {0x1007,0,0,false,0},
	};
	for (auto const &f : cases)
	{
		expect(circular_fixture_domain(f.initial, f.step, f.bk) == f.legal,
			"Circular fixture guard distinguishes legal steps/indices from modulo-only stress inputs");
		if (f.legal) expect(circular_fixture_next(f.initial, f.step, f.bk) == f.next,
			"Circular fixture oracle covers signed wrap and the strict power-of-two base mask");
	}
	osd_printf_info("TMS320C54x paired circular fixture guard: PASS cases=14 nonzero_bk_domain=SPRU131G_5.5.3.4 cpu_execution=0\n");
}

tms320c54x_test_state::dst_extended_fixture tms320c54x_test_state::extended_dst_fixture(unsigned variant)
{
	if (variant >= 3072)
	{
		unsigned const v = (variant - 3072) % 3072;
		unsigned const reg = v % 8, source = (v / 8) % 2, mode = (v / 16) % 12, pattern = (v / 192) % 4;
		bool const compatibility = (v / 768) % 2, sst = (v / 1536) % 2;
		unsigned const selected = compatibility && !reg ? 3 : reg;
		u16 const initial = 0x1007 + (pattern & 1), index = selected ? 3 : initial;
		u16 const bk = !selected && (mode == 9 || mode == 11) ? 8192 : 16;
		auto reverse = [](u16 word) { u16 result = 0; for (unsigned i = 0; i < 16; ++i) result |= ((word >> i) & 1) << (15 - i); return result; };
		int const delta = mode == 1 || mode == 8 ? -2 : mode == 2 || mode == 3 || mode == 10 ? 2 : mode == 5 || mode == 9 ? -int(index) : int(index);
		u16 next = initial;
		if (mode == 4 || mode == 7) next = reverse(u16(reverse(initial) + (mode == 4 ? -int(reverse(index)) : int(reverse(index)))));
		else if (mode >= 8) next = circular_fixture_next(initial, delta, bk);
		else if (mode) next = initial + delta;
		u64 const value = std::array<u64, 4>{0x0012345678ULL, 0xff87654321ULL, 0x017fffffffULL, 0xfe80000000ULL}[pattern];
		s64 const signed_value = BIT(value, 39) ? s64(value) - (s64(1) << 40) : s64(value);
		u32 const stored = u32(sst ? std::clamp(signed_value, s64(-2147483648LL), s64(2147483647LL)) : signed_value);
		return {reg, selected, source, 4 + mode, initial, u16(mode == 3 ? next : initial), next, value, stored, initial, bk};
	}
	unsigned const reg = variant % 8, source = (variant / 8) % 2, kind = (variant / 16) % 4, pattern = (variant / 64) % 4;
	bool const compatibility = (variant / 256) % 2, sst = (variant / 512) % 2;
	unsigned const selected = compatibility && !reg ? 3 : reg;
	s16 const offset = std::array<s16, 4>{0, 1, -1, s16(0xf495)}[pattern];
	// Preserve the opcode-shaped offset with a legal negative wrap, not repeated BK=16 wrapping.
	u16 const initial = kind == 2 ? 0x4007 : 0x1007, bk = kind == 2 ? 8192 : 16;
	u16 const linear = u16(initial + offset);
	u16 const address = kind == 2 ? circular_fixture_next(initial, offset, bk) : linear;
	u64 const value = std::array<u64, 4>{0x0012345678ULL, 0xff87654321ULL, 0x017fffffffULL, 0xfe80000000ULL}[pattern];
	s64 const signed_value = BIT(value, 39) ? s64(value) - (s64(1) << 40) : s64(value);
	u32 const stored = u32(sst ? std::clamp(signed_value, s64(-2147483648LL), s64(2147483647LL)) : signed_value);
	return {reg, selected, source, kind, kind == 3 ? address : u16(offset), address,
		u16(kind == 1 || kind == 2 ? address : initial), value, stored, initial, bk};
}

void tms320c54x_test_state::start_pipeline_extended_dst(unsigned variant)
{
	if (!variant) verify_paired_circular_domain();
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = extended_dst_fixture(variant);
	bool const short_form = variant >= 3072;
	unsigned const flags = short_form ? (variant - 3072) % 3072 : variant;
	u32 const base = f.reg & 1 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x4e00 | (f.source << 8) | (short_form ? 0x80 + (f.kind - 4) * 8 + f.reg : f.kind == 3 ? 0xf8 : 0xe0 + f.kind * 8 + f.reg));
	if (!short_form) program.write_word(base + 0x601, f.literal);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, (flags / (short_form ? 1536 : 512)) % 2},
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 0x6608},
		{cpu_regs::STATE_ST1, 0x4900 | (((flags / (short_form ? 768 : 256)) % 2) << 5)},
		{cpu_regs::STATE_A, f.source ? 0xdead : f.value}, {cpu_regs::STATE_B, f.source ? f.value : 0xdead},
		{cpu_regs::STATE_AR0, 3}, {cpu_regs::STATE_AR0 + f.selected, f.initial}, {cpu_regs::STATE_BK, f.bk},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(f.address, 0xaaaa);
	m_cpu->space(AS_DATA).write_word(f.address ^ 1, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "extended_dst_no_operand_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "DST does not read either destination word"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x60, 0xffff, "extended_dst_pair",
		[this, f, short_form](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes < 2 && at == (f.address ^ m_pipeline_store_writes) &&
				word == (m_pipeline_store_writes ? u16(f.stored) : u16(f.stored >> 16)) && m_cpu->pipeline_cycles() == (short_form ? 6 : 7) + m_pipeline_store_writes,
				"Extended DST retains its original address and saturated pair across two owned write clocks"); ++m_pipeline_store_writes; });
	if (!short_form) m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "extended_dst_literal",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == f.literal && !m_pipeline_extensions_reads, "DST long offset/address is fetched once, including opcode-shaped literals"); ++m_pipeline_extensions_reads; });
	m_phase = 252550; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::mvkd_sp_fixture tms320c54x_test_state::mvkd_sp_case(unsigned variant)
{
	unsigned const form = variant % 8, read = form == 5 || form == 6 ? 7 : 6;
	bool const compiler = BIT(variant / 32, 0);
	u16 const destination = std::array<u16, 8>{u16(compiler ? 0x1002 : 0x402), 0x1400, 0x1400, 0x1400, 0x1401, 0x1500, 0x1403, 0x13}[form];
	return {form, read, std::array<unsigned, 3>{4, read, read + 1}[variant / 128],
		std::array<u16, 8>{0x7002, 0x7082, 0x7092, 0x708a, 0x709a, 0x70f8, 0x70e2, 0x7213}[form],
		u16(form == 5 ? destination : form == 6 ? 3 : 0x18), destination,
		u16(form == 2 || form == 4 ? 0x1401 : form == 3 ? 0x13ff : 0x1400),
		std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 8) % 4],
		BIT(variant / 64, 0) ? 0x20000U : 0, compiler};
}

void tms320c54x_test_state::start_mvkd_sp(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = mvkd_sp_case(variant); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode); program.write_word(f.base + 0x601, f.literal);
	if (f.form == 5 || f.form == 6) program.write_word(f.base + 0x602, 0x18);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x900 | (f.compiler ? 0x4000 : 0)},
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_AR2, 0x1400}, {cpu_regs::STATE_AR3, 0x1357},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	if (f.destination >= 0x60) m_cpu->space(AS_DATA).write_word(f.destination, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mvkd_sp_source",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x18 && word == f.value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read,
				"MVKD reads mapped SP in Read, independently of the earlier compiler address"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "mvkd_sp_destination",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.destination && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == f.read + (f.form != 7),
				"MVKD publishes the retained SP word once to its retained RAM or AR destination"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + (f.read == 7 ? 0x602 : 0x601), "mvkd_sp_literals",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const part = m_pipeline_extensions_reads++;
			expect(part < (f.read == 7 ? 2 : 1) && at == f.base + 0x601 + part && word == (part ? 0x18 : f.literal),
				"MVKD retains the original source and optional destination literal exactly once"); });
	m_phase = 293000; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::mvkd_fixture tms320c54x_test_state::data_copy_fixture(unsigned variant)
{
	unsigned const form = variant % 7, kind = (variant / 7) % 4;
	u16 const destination = std::array<u16, 7>{0x402, 0x1002, 0x1000, 0x1000, 0x1001, 0x1500, 0x11}[form];
	u16 const next_ar = form == 2 || form == 4 ? 0x1001 : form == 3 ? 0xfff : 0x1000;
	u16 const source = std::array<u16, 4>{0x11, 0x1400, destination, 0x12}[kind];
	u16 const value = kind == 0 ? next_ar : std::array<u16, 4>{0, 1, 0x8000, 0xf495}[(variant / 28) % 4];
	return {form, form == 5 ? 7U : 6U, source, destination, next_ar, value};
}

void tms320c54x_test_state::start_pipeline_data_copy(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = data_copy_fixture(variant);
	u32 const base = (variant / 112) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x7000 | std::array<u16, 7>{2, 2, 0x91, 0x89, 0x99, 0xf8, 0x11}[f.form]);
	program.write_word(base + 0x601, f.form == 5 ? f.destination : f.source);
	if (f.form == 5) program.write_word(base + 0x602, f.source);
	if (f.form == 6) program.write_word(base + 0x602 + ((variant / 112) % 2), 0xf4a9); // CMPR with zero/one gap (table 7-7).
	// A preceding shared fixture finishes in architectural IDLE.
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, f.form == 6 ? 0xa600 : 0xa608}, {cpu_regs::STATE_ST1, 0x900 | (f.form == 1 ? 0x4000 : 0)},
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_AR1, 0x1000}, {cpu_regs::STATE_AR0, f.value},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	if (f.destination >= 0x60) m_cpu->space(AS_DATA).write_word(f.destination, 0xaaaa);
	if (f.source != 0x11 || (variant / 7) % 4 != 0) m_cpu->space(AS_DATA).write_word(f.source, f.value);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mvkd_source",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.source && word == f.value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read_cycle,
				"MVKD samples its retained literal source once after destination-pointer modification"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "mvkd_destination",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.destination && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == f.read_cycle + (f.form != 6),
				"MVKD publishes the retained source once to its original destination"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + (f.form == 5 ? 0x602 : 0x601), "mvkd_literals",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < (f.form == 5 ? 2 : 1) && (at & 0xffff) == 0x601 + m_pipeline_extensions_reads &&
				word == (f.form == 5 && !m_pipeline_extensions_reads ? f.destination : f.source), "MVKD retains address/source literals in original order"); ++m_pipeline_extensions_reads; });
	m_phase = 252590; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::long_store_fixture tms320c54x_test_state::immediate_long_store_fixture(unsigned variant)
{
	unsigned const reg = variant % 8, kind = (variant / 8) % 3;
	bool const compatibility = (variant / 384) % 2;
	s16 const offset = std::array<s16, 4>{0, 1, -1, s16(0xf495)}[(variant / 24) % 4];
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xf495}[(variant / 96) % 4];
	u16 const initial = kind == 2 ? 0x4007 : 0x1007, bk = kind == 2 ? 8192 : 16;
	u16 const address = kind == 2 ? circular_fixture_next(initial, offset, bk) : u16(initial + offset);
	return {reg, compatibility && !reg ? 3U : reg, kind, u16(offset), value, address,
		u16(kind ? address : initial), u16(compatibility ? 0x6608 : 0x608), compatibility, initial, bk};
}

void tms320c54x_test_state::start_pipeline_long_immediate_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const peripheral = profile_is("st-long-periph");
	auto const f = peripheral ? peripheral_long_store_fixture(variant) : immediate_long_store_fixture(variant);
	u32 const base = (variant / 768) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x76e0 | (f.kind << 3) | f.reg);
	program.write_word(base + 0x601, f.offset); program.write_word(base + 0x602, f.value);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, f.st0}, {cpu_regs::STATE_ST1, 0x800 | (f.compatibility << 5)},
	});
	if (peripheral) for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5555);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR0 + f.selected, peripheral ? u16(f.address - f.offset) : f.initial},
		{cpu_regs::STATE_BK, f.bk}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(f.address, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "long_immediate_store_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Long immediate ST does not read its destination"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "long_immediate_store_publication",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (f.address >= 0x20 && f.address < 0x60 ? 9 : 8),
				"Long immediate ST owns its cached address and literal value with one Execute write"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x602, "long_immediate_store_literals",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && (at & 0xffff) == 0x601 + m_pipeline_extensions_reads &&
				word == (m_pipeline_extensions_reads ? f.value : f.offset), "Long immediate ST fetches offset then value exactly once"); ++m_pipeline_extensions_reads; });
	m_phase = 252650; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::push_mvmm_fixture tms320c54x_test_state::push_mvmm_case(unsigned v)
{
	push_mvmm_fixture f{};
	f.pushes=v%2+1;v/=2;f.mmr=std::array<unsigned,4>{0x12,6,7,0x18}[v%4];v/=4;
	f.gap=v%3;v/=3;f.destination=v%8;v/=8;f.sp=v%2?0:0x3000;v/=2;
	f.base=v%2?0x20000:0;v/=2;f.checkpoint=std::array<unsigned,3>{3,f.read(),f.read()+1}[v%3];
	return f;
}

void tms320c54x_test_state::start_push_mvmm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true);m_pipeline_peripheral_case=variant;
	auto const f=push_mvmm_case(variant);auto &program=m_cpu->space(AS_PROGRAM);
	for(unsigned i=0;i<16;++i) program.write_word(f.base+0x600+i,0xf495);
	for(unsigned i=0;i<f.pushes;++i) program.write_word(f.base+0x600+i,0x4a00|f.source(i));
	program.write_word(f.base+0x600+f.pushes+f.gap,0xe780|f.destination);
	program.write_word(f.base+0x601+f.pushes+f.gap,0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0);m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0xa020);m_cpu->set_state_int(tms320c54x_device::STATE_ST1,0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0);m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0);m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,f.sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,0x123456789aULL);m_cpu->set_state_int(tms320c54x_device::STATE_B,0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T,0x5aa5);m_cpu->set_state_int(tms320c54x_device::STATE_TRN,0x2468);
	for(unsigned ar=0;ar<8;++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+ar,0x5000+ar);
	m_pipeline_operand_reads=m_pipeline_store_writes=m_pipeline_register_writes=0;
	m_pipeline_operand_tap=m_cpu->space(AS_DATA).install_read_tap(0,0xffff,"push_mvmm_reads",[this,f](offs_t at,u16 &word,u16) {
		if(machine().side_effects_disabled()) return;
		auto const reads=f.reads();unsigned const i=m_pipeline_operand_reads++;
		expect(i<reads.size() && m_cpu->pipeline_cycles()==reads[i][0] && at==reads[i][1] && word==reads[i][2],"PSHM and MVMM own their ordered source samples before the internal SP bypass");
	});
	m_pipeline_write_tap=m_cpu->space(AS_DATA).install_write_tap(0,0xffff,"push_mvmm_writes",[this,f](offs_t at,u16 &word,u16) {
		if(machine().side_effects_disabled()) return;
		if(at==0x10+f.destination) { expect(!m_pipeline_register_writes++ && word==f.copied() && m_cpu->pipeline_cycles()==f.read()+1,"MVMM publishes the older pushes' SP once in Access");return; }
		unsigned const i=m_pipeline_store_writes++;
		expect(i<f.pushes && at==u16(f.sp-1-i) && word==f.value(i) && m_cpu->pipeline_cycles()==6+i,"PSHM retains its source and owns exactly one original stack write");
	});
	m_pipeline_replay=false;m_idle_fixture_tick=0;m_phase=push_mvmm_phase;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600);m_cpu->set_pipeline_frontend(true);m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}


void tms320c54x_test_state::start_pipeline_mapped_load_age(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const arithmetic = profile_is("addsub-ar-age");
	bool const shifted = profile_is("shift16-load-age");
	bool const mapped = arithmetic || shifted || profile_is("mapped-load-age");
	unsigned const fields = shifted ? variant / 4 : mapped ? variant / 8 : variant, form = shifted ? variant%4 : mapped ? (variant / 2) % 4 : 0;
	unsigned const ar = fields % 8, kind = (fields / 64) % 3, pointer = (ar + 2) % 8;
	unsigned const words = mapped && form >= 2 ? 2 : 1, read_cycle = 4 + words;
	bool const destination_b = (fields / 8) % 2, sxm = (fields / 192) % 2;
	u16 const value = std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[(fields / 16) % 4];
	u32 const base = (fields / 384) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	u16 const mode = form == 0 ? 0x10 + ar : form == 1 ? 0x80 + pointer : form == 2 ? 0xf8 : 0xe0 + pointer;
	program.write_word(base + 0x600, (arithmetic ? (variant%2 ? 0x0800 : 0) : shifted ? 0x4400 : mapped ? 0x1000 | (variant % 2 ? 0x200 : 0) : 0x4800) |
		(destination_b ? 0x100 : 0) | mode);
	if (words == 2) program.write_word(base + 0x601, form == 2 ? 0x10 + ar : 0);
	program.write_word(base + 0x600 + words, kind == 2 ? 0xe700 | (((ar + 1) % 8) << 4) | ar :
		(kind ? 0x6d88 : 0x6d90) | ar);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0x1c00}, {cpu_regs::STATE_ST1, 0x800 | (sxm ? 0x100 : 0)},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0x123456789aULL},
	});
	if(arithmetic) m_cpu->set_state_int(destination_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A,0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x3333);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, value);
	if (mapped && (form == 1 || form == 3)) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + pointer, 0x10 + ar);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "ldm_ar_age_read",
		[this, ar, kind, value, read_cycle](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			if (at == 0x10 + ar)
			{
				expect(word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read_cycle,
					"Older mapped load Read retains AR before a younger Access update"); ++m_pipeline_operand_reads;
			}
			else
			{
				expect(kind == 2 && at == 0x10 + (ar + 1) % 8 && word == 0x3333 &&
					!m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == read_cycle - 1, "Younger MVMM retains its own Decode source once");
				++m_pipeline_extensions_reads;
			}
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "ldm_ar_age_write",
		[this, ar, kind, read_cycle](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(kind == 2 && at == 0x10 + ar && word == 0x3333 && !m_pipeline_store_writes &&
				m_cpu->pipeline_cycles() == read_cycle, "Younger MVMM writes once without changing the older load sample");
			++m_pipeline_store_writes;
		});
	m_phase = 600010; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

void tms320c54x_test_state::start_pipeline_mvmm_producer(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const immediate = variant % 2;
	unsigned const gap = (variant / 2) % 4, read_cycle = (immediate ? 5 : 4) + gap;
	u16 const value = immediate || gap >= 2 ? 0x1200 : 0x1000;
	u32 const base = (variant / 8) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, immediate ? 0x7711 : 0x8811);
	if (immediate) program.write_word(base + 0x601, 0x1200);
	program.write_word(base + 0x600 + (immediate ? 2 : 1) + gap, 0xe715);
	program.write_word(base + 0x601 + (immediate ? 2 : 1) + gap, 0x1085);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 8},
		{cpu_regs::STATE_ST1, 0x800},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1300);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1200);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x1000, 0xaaaa); m_cpu->space(AS_DATA).write_word(0x1200, 0xbbbb);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mvmm_producer_visibility",
		[this, read_cycle, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x11) { expect(word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read_cycle,
				"MVMM Decode observes STM immediately and STLM only after its documented source gap"); ++m_pipeline_operand_reads; }
			else { expect(at == value && word == (value == 0x1000 ? 0xaaaa : 0xbbbb) && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == read_cycle + 3,
				"MVMM forwards its retained source to a following AR consumer"); ++m_pipeline_extensions_reads; } });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "mvmm_producer_writes",
		[this, immediate, read_cycle, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes < 2 && (at == 0x11 ? word == 0x1200 && m_cpu->pipeline_cycles() == (immediate ? 5 : 6) :
				at == 0x15 && word == value && m_cpu->pipeline_cycles() == read_cycle + 1), "Parent and MVMM publish at their own age-ordered clocks"); ++m_pipeline_store_writes; });
	m_phase = 252730; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_xmem_bit(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const x = variant % 16, bitcode = (variant / 16) % 16;
	bool const value = (variant / 256) % 2, initial_tc = (variant / 512) % 2;
	unsigned const gap = (variant / 1024) % 3;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0x9600 | (x << 4) | bitcode);
	program.write_word(0x601 + gap, 0xfd20); // XC 1,NTC samples in Access.
	program.write_word(0x602 + gap, 0xe855);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0x608 | (initial_tc ? 0x1000 : 0)}, {cpu_regs::STATE_ST1, 0x800},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0);
	for (unsigned ar = 2; ar <= 5; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x1000);
	m_cpu->space(AS_DATA).write_word(0x1000, value ? 1U << (15 - bitcode) : ~(1U << (15 - bitcode)));
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "xmem_bit_read",
		[this](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x1000 && m_cpu->pipeline_cycles() == 5 && !m_pipeline_operand_reads,
				"BIT reads its captured Xmem address exactly once in Read"); ++m_pipeline_operand_reads; });
	m_phase = 252770; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::long_mvkd_fixture tms320c54x_test_state::long_mvkd_case(unsigned variant)
{
	unsigned const v = variant % 6400, kind = (v / 200) % 4;
	auto const f = long_accumulator_store_fixture(v % 100 + ((v / 100) % 2) * 6400);
	u16 const source = std::array<u16, 4>{u16(0x10 + f.selected), 0x12, 0x1400, f.address}[kind];
	u16 const value = source == 0x10 + f.selected ? f.next_ar : std::array<u16, 4>{0, 1, 0x8000, 0xf495}[(v / 800) % 4];
	return {f, source, value, v / 3200 ? 0x20000U : 0, std::array<unsigned, 3>{4, 7, 8}[variant / 6400]};
}

void tms320c54x_test_state::start_pipeline_long_mvkd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = long_mvkd_case(variant); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x7000 | u8(f.address.opcode)); program.write_word(f.base + 0x601, f.address.literal); program.write_word(f.base + 0x602, f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.address.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.address.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.address.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x1007);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address.selected, f.address.initial);
	m_cpu->space(AS_DATA).write_word(f.address.address, 0xaaaa);
	if (f.source != 0x10 + f.address.selected) m_cpu->space(AS_DATA).write_word(f.source, f.value);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "long_mvkd_source",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.source && word == f.value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 7, "Long MVKD owns one source Read after destination DAGEN"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "long_mvkd_destination",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address.address && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 8, "Long MVKD writes its retained source to its retained destination once"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x602, "long_mvkd_extensions",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && at == f.base + 0x601 + m_pipeline_extensions_reads && word == (m_pipeline_extensions_reads ? f.source : f.address.literal), "Long MVKD fetches offset before source, exactly once"); ++m_pipeline_extensions_reads; });
	m_phase = 253490; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::imr_store_fixture tms320c54x_test_state::imr_store_case(unsigned variant)
{
	unsigned const v = variant % 1536, source = v % 16, pattern = (v / 16) % 4, kind = (v / 256) % 3;
	u16 const bit = u16(1U << source), old_mask = (v / 64) % 2 ? 0xffff : 0;
	u16 const mask = std::array<u16, 4>{0, 0xffff, bit, u16(~bit)}[pattern], st1 = (v / 128) % 2 ? 0x800 : 0;
	unsigned const ack = kind == 2 ? 7 : kind == 1 && !st1 && (old_mask || (mask & bit)) ? old_mask ? 7 : 10 : 0;
	return {old_mask, mask, st1, bit, source, kind, ack, std::array<unsigned, 3>{4, 6, 7}[variant / 1536], v / 768 ? 0x20000U : 0};
}

tms320c54x_test_state::imr_store_fixture tms320c54x_test_state::stlm_imr_case(unsigned variant)
{
	auto f = imr_store_case(variant % 4608);
	if (f.ack_cycle) --f.ack_cycle;
	f.checkpoint = std::array<unsigned, 3>{4, 5, 6}[(variant % 4608) / 1536];
	return f;
}

void tms320c54x_test_state::start_pipeline_imr_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const stlm = profile_is("stlm-imr");
	auto const f = stlm ? stlm_imr_case(variant) : imr_store_case(variant); auto &program = m_cpu->space(AS_PROGRAM);
	u32 const vector = f.base + (f.kind == 2 ? 4 : (16 + f.source) * 4);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(vector + i, 0xf495); }
	program.write_word(f.base + 0x600, 0x7700); program.write_word(f.base + 0x601, f.mask); program.write_word(vector, 0xe955);
	if (stlm) { program.write_word(f.base + 0x600, variant >= 4608 ? 0x8900 : 0x8800); program.write_word(f.base + 0x601, 0xf495); }
	for (unsigned line = 0; line < 16; ++line) m_cpu->set_input_line(line, CLEAR_LINE);
	m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.old_mask);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1804); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	if (stlm) { m_cpu->set_state_int(tms320c54x_device::STATE_A, variant >= 4608 ? 0x1234 : f.mask ^ 0xffff); m_cpu->set_state_int(tms320c54x_device::STATE_B, variant >= 4608 ? f.mask ^ 0xffff : 0); }
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0, "imr_store_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Immediate IMR write must not read its destination"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0, "imr_store_write",
		[this, f, stlm](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == f.mask && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (stlm ? 6 : 7), "IMR store publishes its owned operand once in Execute"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "imr_store_literal",
		[this, f, stlm](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || stlm) return; expect(word == f.mask && !m_pipeline_extensions_reads, "STM IMR fetches its literal once"); ++m_pipeline_extensions_reads; });
	m_phase = 253510; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::offset_load_case(unsigned variant)
{
	if (variant >= 12288)
	{
		unsigned const v = (variant - 12288) % 128, source = v % 8, destination = (v / 8) % 2, pattern = (v / 16) % 4;
		bool const sxm = (v / 64) % 2;
		u16 const memory = source == 7 ? 0x5007 : std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern];
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0);
		u64 const result = u64(sxm ? s64(s16(memory)) : memory) & 0xffffffffffULL;
		return {14, 7, 7, 6, u16(0x10e7 | (destination << 8)), u16(0xb009 + source), u16(0x10 + source), 0x5007,
			memory, 0x1804, st1, 0x1804, 0xdeadbeefULL, result};
	}
	unsigned const v = variant % 4096, form = v % 4, reg = (v / 4) % 8, destination = (v / 32) % 2;
	unsigned const pattern = (v / 64) % 8, flags = (v / 512) % 4, compatibility = (v / 2048) % 2;
	auto f = memory_arithmetic_case(14 + form + 18 * reg + 144 * destination + 576 * pattern + 4608 * flags + 18432 * compatibility);
	f.opcode |= 0x1000;
	f.result = u64(flags & 1 ? s64(s16(f.memory)) : f.memory) & 0xffffffffffULL;
	f.result_st0 = compatibility && reg && form != 3 ? (f.st0 & 0x1fff) | (reg << 13) : f.st0;
	return f;
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::unsigned_load_case(unsigned variant)
{
	if (variant >= 55296)
	{
		unsigned const v = (variant - 55296) % 768, source = v % 8, destination = (v / 8) % 2, pattern = (v / 16) % 4, flags = (v / 64) % 4, kind = (v / 256) % 3;
		u16 const memory = source == 7 ? 0x5007 : std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern];
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0), address = 0x10 + source;
		u16 const mode = kind == 0 ? address : kind == 1 ? 0xf8 : 0xe7;
		return {kind == 0 ? 0U : kind == 1 ? 17U : 14U, 7, 7, kind == 0 ? 5U : 6U,
			u16(0x1200 | (destination << 8) | mode), u16(kind == 2 ? 0xb009 + source : address), address, 0x5007,
			memory, 0x1800, st1, 0x1800, 0xdeadbeefULL, memory};
	}
	unsigned const v = variant % 18432, form = v % 18, reg = (v / 18) % 8, destination = (v / 144) % 2;
	unsigned const pattern = (v / 288) % 8, flags = (v / 2304) % 4, compatibility = (v / 9216) % 2;
	auto f = memory_arithmetic_case(form + 18 * reg + 144 * destination + 576 * pattern + 4608 * flags + 18432 * compatibility);
	f.opcode |= 0x1200;
	f.memory = std::array<u16, 8>{0, 1, 0x7fff, 0x8000, 0xffff, 0xf495, 0xff, 0xff00}[pattern];
	f.result = f.memory;
	f.result_st0 = compatibility && reg && form >= 2 && form != 17 ? (f.st0 & 0x1fff) | (reg << 13) : f.st0;
	return f;
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::staged_load_case(unsigned variant)
{
	if (variant >= 127872)
	{
		unsigned const v = (variant - 127872) % 36864;
		unsigned const form = v % 18, shift = (v / 18) % 32, pattern = (v / 576) % 4;
		unsigned const flags = (v / 2304) % 4, reg = (v / 9216) % 2 ? 6 : 1, destination = (v / 18432) % 2;
		auto f = unsigned_load_case(form + 18 * reg + 144 * destination + 288 * pattern + 2304 * flags);
		f.opcode = (f.opcode & 0x01ff) | 0x4400; // Fixture destination; actual opcode carries it in extension.
		++f.read_cycle;
		int const signed_shift = shift >= 16 ? int(shift) - 32 : int(shift);
		s64 const value = BIT(f.st1, 8) ? s16(f.memory) : f.memory;
		u64 const source = u64(value) & 0xffffffffffULL;
		f.result = signed_shift >= 0 ? (source << signed_shift) & 0xffffffffffULL :
			BIT(f.st1, 8) ? u64(value >> -signed_shift) & 0xffffffffffULL : source >> -signed_shift;
		return f;
	}
	return variant >= 70272 ? shifted_memory_load_case(variant - 70272) :
		variant >= 12672 ? unsigned_load_case(variant - 12672) : offset_load_case(variant);
}

void tms320c54x_test_state::start_pipeline_offset_load(unsigned variant)
{
	// Retain stable raw case IDs; omit short write-only MOD=3, not its
	// long-offset counterpart. PASS totals count only admitted cases.
	if (variant >= 12672 && staged_load_case(variant).form == 5) ++variant;
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = staged_load_case(variant);
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, f.opcode); if (f.form >= 14) program.write_word(base + 0x601, f.literal);
	bool const variable_shift = variant >= 127872;
	if (variable_shift)
	{
		program.write_word(base + 0x600, 0x6f00 | u8(f.opcode));
		program.write_word(base + 0x601 + (f.form >= 14), 0x0c40 | (f.opcode & 0x100) | (((variant - 127872) % 36864 / 18) % 32));
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, BIT(f.opcode, 8) ? 0x123456789aULL : f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, BIT(f.opcode, 8) ? f.source : 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(f.address, f.memory);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "offset_load_sample",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == f.memory && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read_cycle,
				"Memory load samples one retained RAM or AR word in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "offset_load_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Long-offset LD publishes only its accumulator"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601 + (variable_shift && f.form >= 14), "offset_load_literal",
		[this, f, variable_shift, variant, base](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled() || (!variable_shift && f.form < 14)) return;
			bool const operation = variable_shift && at == base + 0x601 + (f.form >= 14);
			u16 const expected = operation ? 0x0c40 | (f.opcode & 0x100) | (((variant - 127872) % 36864 / 18) % 32) : f.literal;
			expect(word == expected && m_pipeline_extensions_reads == unsigned(operation && f.form >= 14), "Memory load retains address then operation extension once");
			++m_pipeline_extensions_reads; });
	m_phase = 253050; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::dst_extended_fixture tms320c54x_test_state::indirect_dld_case(unsigned variant)
{
	unsigned const v = variant % 8192, form = v % 16, reg = (v / 16) % 8, destination = (v / 128) % 2;
	unsigned const pattern = (v / 256) % 4, flags = (v / 1024) % 8;
	bool const compatibility = flags & 2;
	auto f = extended_dst_fixture(form < 12 ? 3072 + reg + 8 * destination + 16 * form + 192 * pattern + 768 * compatibility :
		reg + 8 * destination + 16 * (form - 12) + 64 * pattern + 256 * compatibility);
	f.stored = std::array<u32, 4>{0x12345678, 0x87654321, 0x80000000, 0xffff0001}[pattern];
	f.value = f.stored;
	if ((flags & 1) && BIT(f.stored, 31)) f.value |= 0xff00000000ULL;
	return f;
}

void tms320c54x_test_state::start_pipeline_indirect_dld(unsigned variant)
{
	if (variant % 16 == 3) ++variant; // Short MOD=3 is write-only, including paired accesses.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = indirect_dld_case(variant);
	unsigned const v = variant % 8192, form = v % 16, flags = (v / 1024) % 8, read_cycle = form < 12 ? 5 : 6;
	u32 const base = f.reg & 1 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x5600 | (f.source << 8) | (form < 12 ? 0x80 + form * 8 + f.reg : form == 15 ? 0xf8 : 0xe0 + (form - 12) * 8 + f.reg));
	if (form >= 12) program.write_word(base + 0x601, f.literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 0x6608},
		{cpu_regs::STATE_ST1, 0x800 | ((flags & 1) << 8) | ((flags & 2) << 4) | ((flags & 4) << 5)},
		{cpu_regs::STATE_AR0, 3}, {cpu_regs::STATE_AR0 + f.selected, f.initial}, {cpu_regs::STATE_BK, f.bk},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1111222233ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x4444555566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(f.address, f.stored >> 16); m_cpu->space(AS_DATA).write_word(f.address ^ 1, f.stored);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "indirect_dld_pair",
		[this, f, read_cycle](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads < 2 && at == (f.address ^ m_pipeline_operand_reads) &&
				word == (m_pipeline_operand_reads ? u16(f.stored) : u16(f.stored >> 16)) && m_cpu->pipeline_cycles() == read_cycle,
				"Indirect DLD samples the retained high/low pair once in Read, preserving odd-address ordering"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "indirect_dld_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Indirect DLD publishes only its accumulator"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "indirect_dld_literal",
		[this, f, form](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || form < 12) return;
			expect(word == f.literal && !m_pipeline_extensions_reads, "Extended DLD fetches its retained offset/address once"); ++m_pipeline_extensions_reads; });
	m_phase = 253070; m_cpu->resume(SUSPEND_REASON_DISABLE);
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

tms320c54x_test_state::register_arithmetic_fixture tms320c54x_test_state::register_load_case(unsigned variant)
{
	bool const producer = variant >= 196608;
	unsigned const v = producer ? (variant - 196608) % 48 : variant % 65536;
	unsigned const selector = producer ? v % 4 : (v / 32) % 4;
	unsigned const pattern = producer ? (v / 4) % 4 : (v / 128) % 8;
	unsigned const flags = producer ? 5 : (v / 1024) % 16, gap = producer ? (v / 16) % 3 : 0;
	bool const asm_shift = !producer && (v / 16384) % 2;
	int const shift = producer ? 0 : int(v % 32) - 16;
	std::array<u64, 2> initial = producer ? std::array<u64, 2>{0x10, 0x20} : std::array<u64, 2>{
		std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x80aa001234ULL, 0xffffffff55ULL}[pattern],
		std::array<u64, 8>{1, 0xffffffff, 0xff80000000ULL, 0x7fffffff, 0xff7fffffffULL, 0x017fffffffULL, 0x123456789aULL, 0x80000000}[pattern]};
	auto before = initial;
	if (producer) before[selector >> 1] = u64(s64(s16(std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern]))) & 0xffffffffffULL;
	u64 const source = before[selector >> 1];
	u64 result = 0;
	for (int bit = 0; bit < 40; ++bit)
	{
		int const source_bit = bit - shift;
		if ((source_bit >= 0 && source_bit < 40 && BIT(source, source_bit)) ||
			(shift < 0 && source_bit >= 40 && (flags & 1) && BIT(source, 39))) result |= u64(1) << bit;
	}
	s64 const signed_source = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 const full = shift >= 0 ? signed_source * (s64(1) << shift) : 0;
	bool const overflow = shift >= 0 && (full < -2147483648LL || full > 2147483647LL);
	if (overflow && (flags & 2)) result = full < 0 ? 0xff80000000ULL : 0x7fffffff;
	u16 const st0 = 0x1004 | (flags & 4 ? 0x800 : 0) | (flags & 8 ? 0x600 : 0);
	u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (asm_shift ? shift & 31 : 0x13);
	u16 const result_st0 = st0 | (overflow ? (selector & 1 ? 0x200 : 0x400) : 0);
	auto after = before; after[selector & 1] = result;
	u16 const opcode = (asm_shift ? 0xf482 : 0xf440 | (shift & 31)) | ((selector >> 1) << 9) | ((selector & 1) << 8);
	u32 const base = (producer ? selector & 1 : (v / 32768) % 2) ? 0x20000 : 0;
	return {selector, gap, producer ? 8 + gap : 6, producer, opcode, st0, st1, result_st0, initial, before, after, base};
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::delay_copy_case(unsigned variant)
{
	unsigned v = variant;
	unsigned const form = v % 5; v /= 5; unsigned const reg = v % 8; v /= 8;
	unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 4; v /= 4;
	auto f = memory_arithmetic_case(form + 18 * reg + 576 * pattern + 4608 * flags + 18432 * (v % 2));
	f.opcode = 0x4d00 | u8(f.opcode);
	f.result_st0 = (v % 2 && reg && form >= 2) ? (f.st0 & 0x1fff) | (reg << 13) : f.st0;
	return f;
}

void tms320c54x_test_state::start_delay_copy(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const control = profile_is("delay-copy-ctrl");
	unsigned const kind = variant % 6, v = control ? (variant / 6 / 4) * 5 + std::array<unsigned, 4>{0, 2, 3, 4}[(variant / 6) % 4] : variant;
	auto const f = delay_copy_case(v); u32 const base = (v / 2560) % 2 ? 0x20000 : 0, target = control && kind == 4 ? base ^ 0x20000 : base;
	unsigned const read = control ? delay_multiply_fixture::operand_read(kind, 0) : 5;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 12; ++i) program.write_word(base + 0x600 + i, 0xf495);
	if (control)
	{
		program.write_word(base + 0x600, kind == 4 ? 0xfb80 | (target >> 16) : std::array<u16, 6>{0xf273, 0xfa45, 0xfa45, 0xf274, 0, 0xfe00}[kind]);
		if (kind != 5) program.write_word(base + 0x601, 0x700);
		program.write_word(base + (kind == 5 ? 0x601 : 0x602), f.opcode);
		program.write_word(base + 0x604, 0xf4e1); program.write_word(target + 0x700, 0xf4e1);
	}
	else { program.write_word(base + 0x600, f.opcode); program.write_word(base + 0x602, 0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, control && kind == 1 ? 0 : control && kind == 2 ? 1 : 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0xa55a);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x5007);
	data.write_word(f.address, f.memory); data.write_word(f.address + 1, 0xbeef);
	if (control) data.write_word(0x1000, 0x700);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_fast_stack_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_copy_read", [this, f, control, kind, read](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (control && at == 0x1000) { expect(kind == 5 && word == 0x700 && !m_fast_stack_reads++, "DELAY does not reread its RETD owner's frame"); return; } expect(at == f.address && word == f.memory && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == read, "DELAY owns one Read-stage source sample"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_copy_write", [this, f, control, kind, base, read](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (control && (at == 0xfff || at == 0xffe)) { unsigned const i = m_pipeline_register_writes++; expect((kind == 3 || kind == 4) && i < (kind == 4 ? 2 : 1) && at == 0xfff - i && word == (i ? base >> 16 : 0x604), "DELAY preserves the delayed call frame"); return; } expect(at == f.address + 1 && word == f.memory && !m_pipeline_store_writes++ && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() == read + 1, "DELAY writes its retained sample to the successor in Execute"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62174;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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


tms320c54x_test_state::extended_push_fixture tms320c54x_test_state::extended_push_case(unsigned v)
{
	extended_push_fixture f{}; f.mapped = v >= 96000;
	unsigned offset = 0;
	if (f.mapped)
	{
		v -= 96000; unsigned pair = v % 77; v /= 77; if (pair >= 41) ++pair;
		f.form = pair / 26;
		f.address = std::array<u16, 26>{0, 1, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e}[pair % 26];
		f.pattern = v % 4; v /= 4; f.compatibility = v % 2; v /= 2; f.base = v % 2 ? 0x20000 : 0; v /= 2;
		f.mode = std::array<u16, 3>{0xe3, 0xeb, 0xf8}[f.form]; f.bk = 9;
	}
	else
	{
		f.form = v % 25; v /= 25; offset = v % 4; v /= 4; f.pattern = v % 4; v /= 4;
		f.compatibility = v % 2; v /= 2; f.compiler = v % 2; v /= 2; f.bk = v % 2 ? 9 : 0; v /= 2;
		f.base = v % 2 ? 0x20000 : 0; v /= 2; f.irq = v % 5; v /= 5;
		f.mode = f.form == 24 ? 0xf8 : 0xe0 + f.form;
	}
	f.checkpoint = std::array<unsigned, 3>{3, 5, 7}[v];
	f.selected = f.mode == 0xf8 ? 8 : f.compatibility && !(f.mode & 7) ? 5 : f.mode & 7;
	f.st0 = 0xb608; f.final_st0 = f.compatibility && f.mode != 0xf8 && (f.mode & 7) ? (f.st0 & ~0xe000) | ((f.mode & 7) << 13) : f.st0;
	f.st1 = (f.compiler ? 0x4000 : 0) | (f.compatibility ? 0x20 : 0) | (f.irq ? 0 : 0x800) | (f.pattern & 1 ? 0x100 : 0) | (f.pattern & 2 ? 0x200 : 0) | 0x13;
	f.pmst = f.pattern & 1; f.value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[f.pattern]; f.a = 0x123456789aULL; f.b = 0xfedcba9876ULL; f.t = 0x5aa5; f.trn = 0x1234;
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = i == f.selected ? 0x5007 : 0x7000 + i;
	if (f.mapped)
	{
		if (f.address >= 8 && f.address <= 0xd)
		{
			unsigned const part = (f.address - 8) % 3, shift = part * 16; u64 const mask = (part == 2 ? 0xffULL : 0xffffULL) << shift;
			u64 &accumulator = f.address >= 0xb ? f.b : f.a; accumulator = (accumulator & ~mask) | ((u64(f.value) << shift) & mask);
			if (part == 2) f.value &= 0xff;
		}
		else if (f.address == 0xe) f.t = f.value;
		else if (f.address == 0xf) f.trn = f.value;
		else if (f.address >= 0x10 && f.address <= 0x17) f.initial[f.address - 0x10] = f.value;
		else if (f.address == 0x19) f.bk = f.value;
		else if (f.address == 0) f.value = 0;
		else if (f.address == 1) f.value = 0;
		else if (f.address == 6) f.value = f.final_st0;
		else if (f.address == 7) f.value = f.st1;
		else if (f.address == 0x1a) f.value = 13;
		else if (f.address == 0x1b) f.value = 0x1234;
		else if (f.address == 0x1c) f.value = 0x5678;
		else if (f.address == 0x1d) f.value = f.pmst;
		else if (f.address == 0x1e) f.value = f.base >> 16;
		f.literal = f.mode == 0xf8 ? f.address : u16(f.address - f.initial[f.selected]);
	}
	else if (f.mode == 0xf8) f.address = f.literal = std::array<u16, 4>{0x5000, 0x3000, 0x2fff, 0xffff}[offset];
	else
	{
		f.literal = std::array<u16, 4>{0xffff, 0, 4, 0x7fff}[offset];
		if (f.mode >= 0xf0 && f.bk && offset == 3) f.literal = 8; // Keep the circular step within BK.
		int const delta = s16(f.literal);
		f.address = f.mode >= 0xf0 && f.bk ? 0x5000 + ((7 + delta) % 9 + 9) % 9 : u16(0x5007 + delta);
	}
	f.final = f.initial; if (f.mode >= 0xe8 && f.mode < 0xf8) f.final[f.selected] = f.address;
	return f;
}

void tms320c54x_test_state::start_extended_push(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = extended_push_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x4b00 | f.mode); program.write_word(f.base + 0x601, f.literal); program.write_word(f.base + 0x602, 0xe955); program.write_word(f.base + 0x60f, 0xf4e1);
	u32 const vector = f.base + (f.irq == 2 || f.irq == 4 ? 4 : 0x48); program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, f.trn);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 13); m_cpu->set_state_int(tms320c54x_device::STATE_RSA, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_REA, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq == 1 || f.irq == 3 ? 4 : 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	if (!f.mapped) data.write_word(f.address, f.value);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_rptbd_counts = {}; m_rptbd_return = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "extended_push_literal", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == f.literal && !m_rptbd_counts.fetches++ && m_cpu->pipeline_cycles() == 3, "Extended push fetches its address word once"); });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "extended_push_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2ffe) { expect(f.irq && !m_rptbd_counts.stack_reads++ && word == m_rptbd_return, "Extended push interrupt reads its own continuation, not the pushed operand"); return; }
		if (at != f.address || word != f.value || m_rptbd_counts.reads || m_cpu->pipeline_cycles() != 6) osd_printf_info("Extended push source mismatch case=%u replay=%u at=%04x word=%04x expected=%04x/%04x cycle=%llu\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), unsigned(at), word, f.address, f.value, (unsigned long long)m_cpu->pipeline_cycles());
		expect(at == f.address && word == f.value && !m_rptbd_counts.reads++ && m_cpu->pipeline_cycles() == 6, "Extended push samples its retained source once in Read"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "extended_push_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(word == f.value && !m_rptbd_counts.writes++ && m_cpu->pipeline_cycles() == 7, "Extended push writes the captured source to its own retained stack slot once"); return; }
		expect(f.irq && at == 0x2ffe && !m_rptbd_counts.stack_writes++ && word >= 0x602 && word <= 0x60f && m_rptbd_counts.writes == 1, "Interrupt stack allocation follows the older extended push"); m_rptbd_return = word; });
	m_phase = 310560; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::extended_push_fixture tms320c54x_test_state::extended_pop_case(unsigned variant)
{
	// Reuse address/value domains, excluding IRQ dimensions (not yet accepted for extended POPD).
	return extended_push_case(variant % 6400 + (variant / 6400) * 32000);
}

void tms320c54x_test_state::start_extended_pop(unsigned variant)
{
	while (variant < 19199 && extended_pop_case(variant).mode >= 0xf0 && extended_pop_case(variant).mode < 0xf8 && !extended_pop_case(variant).bk) ++variant;
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = extended_pop_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x8b00 | f.mode); program.write_word(f.base + 0x601, f.literal);
	program.write_word(f.base + 0x602, 0xe955); program.write_word(f.base + 0x60f, 0xf4e1);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, f.trn); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	data.write_word(f.address, 0xaaaa); data.write_word(0x3000, f.value);
	m_rptbd_counts = {}; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "extended_pop_literal", [this, f](offs_t, u16 &word, u16) {
		if (!machine().side_effects_disabled()) expect(word == f.literal && !m_rptbd_counts.fetches++ && m_cpu->pipeline_cycles() == 3, "Extended POPD fetches its retained address word once"); });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "extended_pop_source", [this, f](offs_t at, u16 &word, u16) {
		if (!machine().side_effects_disabled()) expect(at == 0x3000 && word == f.value && !m_rptbd_counts.reads++ && m_cpu->pipeline_cycles() == 6, "Extended POPD reads one stack word, never its destination"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "extended_pop_destination", [this, f](offs_t at, u16 &word, u16) {
		if (!machine().side_effects_disabled()) expect(at == f.address && word == f.value && !m_rptbd_counts.writes++ && m_cpu->pipeline_cycles() == 7, "Extended POPD writes its retained destination once"); });
	m_phase = 310620; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_extended_pop_edge(unsigned variant)
{
	unsigned const seed = ((variant / 8) % 2 ? 400 : 0) + ((variant / 16) % 2 ? 3200 : 0);
	start_extended_pop(seed); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = extended_pop_case(seed); auto &program = m_cpu->space(AS_PROGRAM); unsigned const kind = variant % 8;
	m_pipeline_fetch_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5007);
	program.write_word(f.base + 0x600, kind < 3 ? 0x8beb : kind == 3 ? 0x8bf9 : 0xf495);
	program.write_word(f.base + 0x601, u16((kind == 0 ? 0x0e : kind == 1 ? 0x18 : 0x24) - 0x5007));
	if (kind == 4 || kind == 6) { program.write_word(f.base + 0x600, kind == 4 ? 0xf273 : 0xf072); program.write_word(f.base + 0x601, kind == 4 ? 0x610 : 0x604); program.write_word(f.base + 0x602, 0x8be3); program.write_word(f.base + 0x603, 4); }
	if (kind == 5 || kind == 7) { program.write_word(f.base + 0x600, kind == 5 ? 0xec00 : 0xfd30); program.write_word(f.base + 0x601, 0x8be3); program.write_word(f.base + 0x602, 4); }
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "extended_pop_edge_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "extended_pop_edge_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.writes; });
	m_phase = 310640; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

tms320c54x_test_state::stlm_pmst_fixture tms320c54x_test_state::stlm_pmst_case(unsigned v)
{
	stlm_pmst_fixture f{}; bool const interrupt = v >= 4096; if (interrupt) v -= 4096;
	f.pattern = v % 8; v /= 8; f.saturation = v % 2; v /= 2; f.sign = v % 2; v /= 2; f.source_b = v % 2; v /= 2;
	if (!interrupt) { f.compiler = v % 2; v /= 2; }
	f.old_overlay = v % 2; v /= 2; f.base = (v % 2 ? 3 : 2) << 16; v /= 2;
	if (interrupt) { f.irq = v % 17 + 1; v /= 17; f.late = v % 2; v /= 2; }
	else { f.low_entry = !(v % 2); v /= 2; }
	f.checkpoint = std::array<unsigned, 4>{3, 5, 6, 12}[v];
	f.entry = f.low_entry ? 0x600 : 0x9000;
	f.old_pmst = 0x200 | (f.old_overlay ? 0x20 : 0) | (f.saturation ? 1 : 0);
	f.st1 = (f.sign ? 0x100 : 0) | (f.compiler ? 0x4000 : 0) | (f.irq ? 0 : 0x800) | 0x13;
	f.source = std::array<u64, 8>{0, 0x20, 0x2021, 0xffaa, 0x01230021, 0x7f00000021ULL, 0xff00000021ULL, 0xffffffffffULL}[f.pattern];
	u64 stored = f.source;
	if (f.saturation)
	{
		if (!f.sign) stored = std::min(stored, u64(0xffffffffULL));
		else { s64 const signed_value = stored & 0x8000000000ULL ? s64(stored) - (s64(1) << 40) : s64(stored); stored = u64(std::clamp(signed_value, s64(-2147483648LL), s64(2147483647LL))); }
	}
	f.value = stored; return f;
}

u32 tms320c54x_test_state::pmst_fixture_physical(u32 base, u16 address, u16 pmst)
{
	return (pmst & 0x20) && address < 0x8000 ? address : base + address;
}

void tms320c54x_test_state::start_stlm_pmst(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = stlm_pmst_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(f.entry + i, 0xf495); program.write_word(f.base + f.entry + i, 0xf495); }
	for (u32 bank : {0U, f.base})
	{
		program.write_word(bank + f.entry, f.source_b ? 0x891d : 0x881d);
		program.write_word(bank + f.entry + 4, bank ? 0xe944 : 0xe911);
		program.write_word(bank + f.entry + 5, bank ? 0xe844 : 0xe811);
		program.write_word(bank + f.entry + 15, 0xf4e1);
	}
	u16 const offset = f.irq == 17 ? 4 : (15 + f.irq) * 4;
	for (u16 vector : {u16((f.old_pmst & 0xff80) | offset), u16((f.value & 0xff80) | offset)})
		for (u32 bank : {0U, f.base}) { for (unsigned i = 0; i < 12; ++i) program.write_word(bank + vector + i, 0xf495); program.write_word(bank + vector + 1, 0xf4eb); }
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.old_pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xb9ff); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.source_b ? 0x123456789aULL : 0x12121212); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.source_b ? 0x12121212 : 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq && f.irq != 17 ? u16(1U << (f.irq - 1)) : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 13); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 17);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x7000 + i);
	for (unsigned i = 0; i < 16; ++i) m_cpu->set_input_line(i, CLEAR_LINE);
	m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_rptbd_counts = {}; m_rptbd_return = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "stlm_pmst_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.irq && at == 0x2fff && !m_rptbd_counts.stack_reads++ && word == m_rptbd_return, "STLM PMST has no source-memory read; RETE reads one original continuation"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "stlm_pmst_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x1d) { expect(word == f.value && !m_rptbd_counts.writes++ && m_cpu->pipeline_cycles() == 6, "STLM PMST stores the Execute-live source using old SST/SXM exactly once"); return; }
		expect(f.irq && at == 0x2fff && !m_rptbd_counts.stack_writes++ && word > f.entry && word <= f.entry + 15, "PMST store leaves stack writes to its optional interrupt"); m_rptbd_return = word; });
	m_pipeline_fetch_tap = program.install_read_tap(0, 0x7fffff, "stlm_pmst_vectors", [this, f, offset](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled() || !f.irq) return;
		u16 const old_vector = (f.old_pmst & 0xff80) | offset, new_vector = (f.value & 0xff80) | offset;
		u32 const old_physical = pmst_fixture_physical(f.base, old_vector, f.old_pmst), new_physical = pmst_fixture_physical(f.base, new_vector, f.value);
		if (at != old_physical && at != new_physical) return;
		expect(at == (f.late ? new_physical : old_physical) && word == 0xf495 && !m_rptbd_counts.fetches++ && m_cpu->pipeline_cycles() == (f.late ? 12 : 6), "INTR vector Prefetch owns IPTR/OVLY before or after the STLM publication"); });
	m_phase = 310520; m_cpu->set_state_int(STATE_GENPC, f.base + f.entry); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::repeat_load_fixture tms320c54x_test_state::repeat_load_case(unsigned v)
{
	repeat_load_fixture f{}; unsigned snapshot = v % 3;
	f.arithmetic = profile_is("rpt-sfta");
	if (v >= 198656)
	{
		v -= 198656; f.block = true; f.count = std::array<unsigned, 4>{0, 1, 7, 255}[v % 4]; v /= 4;
		f.outer = v % 2 ? 2 : 0; v /= 2; f.selector = v % 4; v /= 4; f.shift = v % 2 ? 15 : -1; v /= 2;
		f.flags = v % 4; v /= 4; f.pattern = v % 2 ? 4 : 2; v /= 2; f.base = v % 2 ? 0x20000 : 0; v /= 2;
		snapshot = v % 2; f.asm_form = false;
	}
	else if (v < 32768)
	{
		f.selector = v % 4; v /= 4; f.shift = int(v % 32) - 16; v /= 32;
		f.flags = v % 4; v /= 4; f.pattern = v % 8; v /= 8;
		f.count = std::array<unsigned, 4>{0, 1, 7, 255}[v % 4]; v /= 4; f.asm_form = v % 2;
		f.base = (f.pattern + f.flags) % 2 ? 0x20000 : 0;
	}
	else
	{
		v -= 32768; f.setup = v % 6; v /= 6; f.count = std::array<unsigned, 4>{0, 1, 7, 255}[v % 4]; v /= 4;
		f.selector = v % 4; v /= 4; f.shift = std::array<int, 3>{-16, -1, 15}[v % 3]; v /= 3;
		f.flags = v % 4; v /= 4; f.pattern = v % 2 ? 4 : 2; v /= 2;
		f.base = v % 2 ? 0x20000 : 0; v /= 2; f.irq = v % 3; v /= 3; snapshot = v % 3; v /= 3; f.asm_form = v % 2;
	}
	bool const extended = f.setup == 1 || f.setup >= 4;
	f.body = f.base + (extended ? 0x602 : 0x601); f.first = f.setup == 0 ? 7 : f.setup == 1 ? 8 : f.setup <= 3 ? 9 : 10;
	if (f.block) { f.body = f.base + 0x603; f.first = 11; }
	f.checkpoint = snapshot == 0 ? 3 : snapshot == 1 ? f.first + f.count / 2 : f.first + f.count;
	f.word = (f.asm_form ? 0xf482 : 0xf440 | (f.shift & 31)) | ((f.selector & 2) << 8) | ((f.selector & 1) << 8);
	if (f.arithmetic) f.word = 0xf460 | (f.shift & 31) | ((f.selector & 3) << 8);
	f.st0 = 0xb808 | (f.pattern & 1 ? 0x600 : 0);
	if (f.arithmetic && !(f.pattern & 2)) f.st0 &= ~0x800;
	f.st1 = (f.setup == 3 ? 0x4000 : 0) | (f.irq ? 0 : 0x800) | (f.flags & 1 ? 0x100 : 0) | (f.flags & 2 ? 0x200 : 0) | (f.asm_form ? f.shift & 31 : 11);
	f.address = f.setup == 3 ? 0x3000 : f.setup == 5 ? 0x15 : 0x400;
	return f;
}

std::array<u64, 3> tms320c54x_test_state::repeat_load_initial(repeat_load_fixture const &f)
{
	u64 const source = std::array<u64, 8>{0, 1, 0x007fffffffULL, 0x0080000000ULL, 0xff80000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x8000000000ULL}[f.pattern];
	return {f.selector & 2 ? 0x123456789aULL : source, f.selector & 2 ? source : 0xfedcba9876ULL, f.st0};
}

std::array<u64, 3> tms320c54x_test_state::repeat_load_step(repeat_load_fixture const &f, std::array<u64, 3> state)
{
	u64 const source = state[(f.selector >> 1) & 1]; u64 value = source;
	if (f.arithmetic && f.shift)
		state[2] = (state[2] & ~0x800ULL) | ((source >> (f.shift < 0 ? -f.shift - 1 : 39 - f.shift)) & 1 ? 0x800 : 0);
	if (f.shift < 0)
		for (int i = 0; i < -f.shift; ++i) value = (value >> 1) | (f.flags & 1 && value & 0x8000000000ULL ? 0x8000000000ULL : 0);
	else
	{
		s64 const signed_source = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
		s64 const full = signed_source * (s64(1) << f.shift); value = u64(full) & 0xffffffffffULL;
		if ((!f.arithmetic || f.shift > 0) && (full < -2147483648LL || full > 2147483647LL))
		{
			state[2] |= f.selector & 1 ? 0x200 : 0x400;
			if (f.flags & 2) value = full < 0 ? 0xff80000000ULL : 0x007fffffffULL;
		}
	}
	state[f.selector & 1] = value; return state;
}

void tms320c54x_test_state::start_repeat_load(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = repeat_load_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.setup == 0 ? 0xec00 | f.count : f.setup == 1 ? 0xf070 : f.setup <= 3 ? 0x4700 : 0x47f8);
	if (f.setup == 1 || f.setup >= 4) program.write_word(f.base + 0x601, f.setup == 1 ? f.count : f.address);
	program.write_word(f.body, f.word); program.write_word(f.body + 1, 0x6d94); program.write_word(f.base + 0x60f, 0xf4e1);
	if (f.block) { program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, 0x607); program.write_word(f.base + 0x602, 0xec00 | f.count); program.write_word(f.body, 0xe8ff); program.write_word(f.base + 0x608, 0xf4e1); }
	u32 const vector = f.base + (f.irq == 2 ? 4 : 0x48); program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_RC, 0); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.block ? f.outer : 13); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 17);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq == 1 ? 4 : 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 4 ? 0x6000 : 0x7000 + i);
	m_repeat_load_expected = {repeat_load_initial(f)};
	for (unsigned i = 0; i < (f.count + 1) * (f.outer + 1); ++i) m_repeat_load_expected.push_back(repeat_load_step(f, m_repeat_load_expected.back()));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, m_repeat_load_expected[0][0]); m_cpu->set_state_int(tms320c54x_device::STATE_B, m_repeat_load_expected[0][1]);
	if (f.setup >= 2) data.write_word(f.address, f.count);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_observed_instructions.clear(); m_saved_observed_instructions.clear(); m_rptbd_counts = {}; m_rptbd_return = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.body, f.body, "repeat_load_body", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		// RPTB discards the sequential pre-redirect fetch. Give it a
		// distinct word to prove it never becomes an executed inner body.
		bool const discarded = f.block && !m_rptbd_counts.fetches;
		u16 const expected = discarded ? 0xe8ff : f.word;
		if (word != expected || m_rptbd_counts.fetches > f.outer + unsigned(f.block)) osd_printf_info("Repeated load fetch mismatch case=%u replay=%u at=%06x word=%04x expected=%04x fetches=%u outer=%u cycle=%llu\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), unsigned(at), word, expected, m_rptbd_counts.fetches, f.outer, (unsigned long long)m_cpu->pipeline_cycles());
		expect(word == expected && m_rptbd_counts.fetches <= f.outer + unsigned(f.block) && (!discarded || m_cpu->pipeline_cycles() == 5), "Repeated load distinguishes discarded block-setup prefetch from one accepted opcode per outer iteration"); ++m_rptbd_counts.fetches; });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeat_load_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && !m_rptbd_counts.stack_reads++ && word == m_rptbd_return, "Repeated load interrupt reads its original continuation once"); return; }
		expect(f.setup >= 2 && at == f.address && word == f.count && !m_rptbd_counts.reads++ && m_cpu->pipeline_cycles() == (f.setup >= 4 ? 6 : 5), "Repeated accumulator load reads only its one memory count"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeat_load_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.irq && at == 0x2fff && !m_rptbd_counts.stack_writes++ && word >= u16(f.body + 1) && word <= 0x60f, "Repeated accumulator load writes only the deferred interrupt frame"); m_rptbd_return = word; });
	m_phase = 310480; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::memory_immediate_fixture tms320c54x_test_state::memory_immediate_case(unsigned variant)
{
	variant = memory_immediate_key(variant);
	bool const mac = variant >= 98304;
	unsigned v = mac ? variant - 98304 : variant, form = v % (mac ? 8 : 12); v /= mac ? 8 : 12;
	// Keep the original form indices, but omit write-only short MOD=3.
	if (form >= 4) ++form;
	unsigned const p = v % 8; v /= 8; unsigned const q = v % 8; v /= 8;
	unsigned const flags = v % (mac ? 16 : 8); v /= mac ? 16 : 8;
	unsigned const selectors = v % (mac ? 4 : 2); v /= mac ? 4 : 2;
	bool const b = selectors & 1, bsrc = selectors & 2;
	u32 const base = v % 2 ? 0x20000 : 0; v /= 2;
	u16 const word = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0x007f, 0x1234, 0xf495}[p], literal = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0x012c, 0xf495, 0xff80}[q];
	u16 const mode = std::array<u16, 13>{4, 0x82, 0x92, 0x8a, 0x9a, 0xe2, 0xea, 0xf2, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8}[form];
	u16 const address = std::array<u16, 13>{0x1404, 0x140c, 0x140c, 0x140c, 0x140d, 0x140f, 0x140f, 0x1402, 0x140c, 8, 9, 0x0e, 0x13}[form];
	unsigned const access = mode >= 0xe0 ? 6 : 5, read = access + 1, words = mode >= 0xe0 ? 3 : 2;
	s64 product = s64(s16(word)) * s16(literal) * (flags & 1 ? 2 : 1);
	u16 const st0 = 0x1028 | (p & 1 ? 0x800 : 0) | (q & 1 ? 0x600 : 0), st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 4 ? 0x100 : 0) | (!form && base ? 0x4000 : 0);
	u64 a = 0x123456789aULL; if (address == 8) a = (a & ~0xffffULL) | word; if (address == 9) a = (a & ~0xffff0000ULL) | (u64(word) << 16);
	u64 bb = 0xfedcba9876ULL;
	if (mac)
	{
		std::array<u64, 8> const seeds{0, 1, 0x123456789aULL, 0xffffffffffULL, 0xff80000000ULL, 0x7fffffff, 0x0080000000ULL, 0xfedcba9876ULL};
		a = seeds[p]; bb = seeds[q];
		// SMUL clamps the fractional product before adding the signed 40-bit source.
		if ((flags & 11) == 11) product = std::clamp<s64>(product, -2147483648LL, 2147483647LL);
		u64 const source = bsrc ? bb : a;
		product += source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source);
	}
	bool const overflow = product < -2147483648LL || product > 2147483647LL;
	s64 const result = overflow && (flags & 2) ? std::clamp<s64>(product, -2147483648LL, 2147483647LL) : product;
	return {form, read, access, std::array<unsigned, 4>{3, access, read, read + 1}[v], words, mode, address, word, literal, u16(mode == 0xf8 ? address : 3), st0, st1, u16(st0 | (overflow ? b ? 0x200 : 0x400 : 0)), base, a, bb, u64(result) & 0xffffffffffULL, b, bsrc, mac, u16(mac && (flags & 8) ? 2 : 0)};
}


tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_memory_square_case(unsigned variant)
{
	auto f = delay_square_case(variant % 36 + (variant / 36) * 72);
	f.square = false; f.memory_square = true;
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::t_move_fixture tms320c54x_test_state::t_move_case(unsigned variant)
{
	unsigned const form = variant % 5, destination = (variant / 5) % 2, flags = (variant / 40) % 4, gap = (variant / 160) % 2;
	unsigned const read = 5 + unsigned(form >= 3), consume = read + (destination ? 2 : 3) + gap;
	return {form, destination, flags, gap, std::array<unsigned, 3>{3, read, consume}[variant / 640],
		std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 10) % 4],
		std::array<u16, 5>{0x16, 0x400, 0x5000, 0x5003, 0x5000}[form], (variant / 320) % 2 ? 0x20000U : 0};
}

void tms320c54x_test_state::start_t_register_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = t_move_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, std::array<u16, 5>{0x7316, 0x7100, 0x7182, 0x71e2, 0x71f8}[f.form]);
	if (f.form >= 3) program.write_word(f.base + 0x601, f.form == 3 ? 3 : f.source);
	program.write_word(f.base + 0x5ff + f.words(), 0x0e + f.destination);
	unsigned const consumer = 0x600 + f.words() + f.gap;
	program.write_word(f.base + consumer, f.destination ? 0x480f : 0xf066);
	if (!f.destination) program.write_word(f.base + consumer + 1, 0x8000);
	program.write_word(f.base + consumer + (f.destination ? 1 : 2), 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa008); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900 | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2468); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000);
	data.write_word(f.source, f.value); m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "t_move_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads++; expect((i == 0 && at == f.source && word == f.value && m_cpu->pipeline_cycles() == f.read()) || (i == 1 && f.destination && at == 0x0f && word == f.value && m_cpu->pipeline_cycles() == f.consume()), "T/TRN move and consumer each sample their owned source once"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "t_move_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x0e + f.destination && word == f.value && !m_pipeline_store_writes++ && m_cpu->pipeline_cycles() == f.read() + 1, "T/TRN move publishes once at Read after its retained source Access"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254520;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_store_fixture tms320c54x_test_state::delay_store_case(unsigned variant)
{
	unsigned const kind = variant % 5, operation = (variant / 5) % 4, form = (variant / 20) % 4;
	u16 const literal = std::array<u16, 6>{1, 0xffff, 0xf495, 0xfc43, 0xf7d9, 0xf4e2}[(variant / 80) % 6];
	u16 const offset = form == 3 && literal == 1 ? 0x5001 : literal;
	int const index = ((5 + s16(offset)) % 13 + 13) % 13;
	u16 const address = form == 3 ? offset : form == 2 ? 0x5000 + index : u16(0x5005 + s16(offset));
	u64 const a = kind == 1 ? 0 : kind == 2 ? 1 : 0x123456789aULL, b = 0xfedcba9876ULL;
	u64 const accumulator = operation & 1 ? b : a;
	unsigned const write = kind == 0 || kind == 3 ? 9 : 10;
	return {kind, operation, std::array<unsigned, 4>{0xe2, 0xea, 0xf2, 0xf8}[form],
		std::array<unsigned, 3>{3, kind == 4 ? 4U : 5U, write - 1}[variant / 960],
		offset, address, form == 1 || form == 2 ? address : u16(0x5005),
		u16(accumulator >> (operation & 2 ? 16 : 0)), (variant / 480) % 2 ? 0x20000U : 0, a, b};
}

void tms320c54x_test_state::start_delay_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = delay_store_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, std::array<u16, 5>{0xf273, 0xfa45, 0xfa45, 0xf274, 0xfe00}[f.kind]);
	if (f.kind != 4) program.write_word(f.base + 0x601, 0x700);
	program.write_word(f.base + 0x600 + f.delay(), 0x8000 | (f.operation << 8) | f.mode);
	program.write_word(f.base + 0x601 + f.delay(), f.literal);
	program.write_word(f.base + 0x602 + f.delay(), 0xf4e1); program.write_word(f.base + 0x700, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, 13); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5005); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	data.write_word(0x3000, 0x700); data.write_word(f.address, 0xdead);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_store_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.kind == 4 && at == 0x3000 && word == 0x700 && !m_pipeline_operand_reads++, "Only RETD reads one stack continuation; the delay store never reads its destination"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_store_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(f.kind == 3 && word == 0x604 && !m_pipeline_register_writes++, "CALLD owns one separate post-delay continuation frame"); return; } bool const valid = at == f.address && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == f.write_clock(); if (!valid) osd_printf_info("delay_store_write: case=%u replay=%u address=%04x expected=%04x word=%04x expected_word=%04x cycle=%llu expected_cycle=%u\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), unsigned(at), f.address, word, f.value, (unsigned long long)m_cpu->pipeline_cycles(), f.write_clock()); expect(valid, "The complete two-word delay store writes its retained address exactly once"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x700, "delay_store_program", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == f.base + 0x601 + f.delay()) { bool const valid = word == f.literal && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == f.literal_clock(); if (!valid) osd_printf_info("delay_store_literal: case=%u replay=%u address=%06x word=%04x expected=%04x reads=%u cycle=%llu expected_cycle=%u\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), unsigned(at), word, f.literal, m_pipeline_extensions_reads, (unsigned long long)m_cpu->pipeline_cycles(), f.literal_clock()); expect(valid, "Delay literal belongs to its store and is fetched once"); ++m_pipeline_extensions_reads; } if (at == f.base + 0x700) ++m_pipeline_drained_fetches; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254480;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}





tms320c54x_test_state::move_brc_fixture tms320c54x_test_state::move_brc_case(unsigned variant, bool control)
{
	move_brc_fixture f{};
	f.form = variant % 8;
	f.count = std::array<unsigned, 5>{0, 1, 2, 7, 127}[(variant / 8) % 5];
	f.gap = (variant / 40) % 2; f.delayed = (variant / 80) % 2;
	f.base = (variant / 160) % 2 ? 0x20000 : 0; f.snapshot = variant / 320;
	f.opcode = std::array<u16, 8>{0x7102, 0x7102, 0x7192, 0x71f8, 0x71e2, 0x7314, 0x7308, 0x7108}[f.form];
	f.source = std::array<u16, 8>{0x1202, 0x3002, 0x1400, 0x1400, 0x1403, 0x14, 8, 8}[f.form];
	f.offset = f.form == 3 ? 0x1400 : 3;
	f.st1 = 0x800 | (f.form == 1 ? 0x4000 : 0);
	f.read_tick = f.form == 3 || f.form == 4 ? 6 : 5;
	if (control)
	{
		unsigned v = variant;
		unsigned const form = std::array<unsigned, 5>{0, 2, 5, 6, 7}[v % 5]; v /= 5;
		unsigned const count = v % 5; v /= 5;
		unsigned const kind = v % 6; v /= 6;
		bool const delayed = v % 2; v /= 2;
		unsigned const page = v % 2; v /= 2;
		f = move_brc_case(form + count * 8 + unsigned(delayed) * 80 + page * 160 + v * 320);
		f.control_kind = kind; f.read_tick = delay_multiply_fixture::operand_read(kind, 1) - 1;
	}
	return f;
}

void tms320c54x_test_state::start_move_brc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const control = profile_is("move-brc-delay");
	auto const f = move_brc_case(variant, control);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	u32 const target_base = control && f.control_kind == 4 ? f.base ^ 0x20000 : f.base;
	for (unsigned i = 0; i < 32; ++i) program.write_word(target_base + 0x700 + i, 0xf495);
	unsigned cursor = control ? f.control_kind == 5 ? 0x601 : 0x602 : 0x600;
	if (control) { program.write_word(f.base + 0x600, f.control_kind == 4 ? 0xfb80 | (target_base >> 16) : std::array<u16, 6>{0xf273, 0xfa30, 0xfa30, 0xf274, 0, 0xfe00}[f.control_kind]); if (f.control_kind != 5) program.write_word(f.base + 0x601, 0x700); }
	program.write_word(f.base + cursor++, f.opcode);
	if (f.form == 3 || f.form == 4) program.write_word(f.base + cursor++, f.offset);
	program.write_word(f.base + cursor++, 0x1a);
	if (control) cursor = f.control_kind == 2 ? 0x604 : 0x700;
	cursor += f.gap;
	program.write_word(target_base + cursor++, f.delayed ? 0xf272 : 0xf072);
	u16 const end = cursor + 2 + (f.delayed ? 2 : 0);
	program.write_word(target_base + cursor++, end);
	if (f.delayed) cursor += 2;
	program.write_word(target_base + cursor++, 0x1096);
	program.write_word(target_base + cursor++, 0x8097);
	program.write_word(target_base + cursor, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (f.form == 7 ? 0xa000 : 0xa024) | (control && f.control_kind == 1 ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, f.count);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x5100);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count ^ 1);
	if (f.source >= 0x60) data.write_word(f.source, f.count);
	if (control) data.write_word(0x3000, 0x700);
	for (unsigned i = 0; i <= f.count; ++i) { data.write_word(0x5000 + i, 0x6000 + i); data.write_word(0x5100 + i, 0xdead); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = 0;
	m_fast_stack_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "move_brc_reads", [this, f, control](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (control && at == 0x3000) { expect(f.control_kind == 5 && word == 0x700 && !m_fast_stack_reads++, "BRC move has an independent RETD target read"); return; }
		if (at == f.source) { expect(!m_pipeline_extensions_reads++ && word == f.count && m_cpu->pipeline_cycles() == f.read_tick, "BRC move reads its source once in Access"); return; }
		unsigned const i = m_pipeline_operand_reads++;
		if (!(i <= f.count && at == 0x5000 + i && word == 0x6000 + i)) osd_printf_info("BRC move read failure: variant=%u address=%04x word=%04x ordinal=%u cycle=%llu\n", m_pipeline_peripheral_case, unsigned(at), word, i, (unsigned long long)m_cpu->pipeline_cycles());
		expect(i <= f.count && at == 0x5000 + i && word == 0x6000 + i, "Move-fed repeat reads exactly count plus one ordered words");
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "move_brc_writes", [this, f, control](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (control && (at == 0x2fff || at == 0x2ffe)) { unsigned const i = m_pipeline_drained_fetches++; expect((f.control_kind == 3 || f.control_kind == 4) && i < (f.control_kind == 4 ? 2 : 1) && at == 0x2fff - i && word == (i ? f.base >> 16 : 0x604), "BRC move does not alter its call frame"); return; }
		if (at == 0x1a) { expect(!m_pipeline_register_writes++ && word == f.count && m_cpu->pipeline_cycles() == f.read_tick, "BRC move publishes once before adjacent RPTB Decode"); return; }
		unsigned const i = m_pipeline_store_writes++;
		expect(i <= f.count && at == 0x5100 + i && word == 0x6000 + i, "Move-fed repeat writes exactly count plus one ordered words");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255620;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

tms320c54x_test_state::dst_bypass_fixture tms320c54x_test_state::dst_bypass_case(unsigned variant)
{
	unsigned v=variant, kind=v%3; v/=3; bool const b=v%2; v/=2;
	unsigned const pattern=v%4; v/=4; unsigned const mode=v%3; v/=3;
	unsigned const gap=v%2; v/=2; bool const same=v%2; v/=2;
	bool const sxm=v%2; v/=2; bool const sst=v%2; v/=2;
	u32 const base=v%2?0x20000:0; v/=2; unsigned const checkpoint=v%2?10:6;
	u64 const source=std::array<u64,4>{0x0012345678ULL,0xff87654321ULL,0x0080000000ULL,0xff00000000ULL}[pattern];
	s64 const signed_source=s64(source<<24)>>24;
	u32 const stored=!sst?u32(source):!sxm?u32(std::min<u64>(source,0xffffffffULL)):
		u32(std::clamp(signed_source,s64(-0x80000000LL),s64(0x7fffffffLL)));
	u32 const memory=same?stored:0x13579bdf;
	u16 const selected=kind==1?u16(memory):u16(memory>>16);
	u64 const loaded=kind==2?(sxm&&BIT(memory,31)?u64(memory)|0xff00000000ULL:memory):
		(sxm&&BIT(selected,15)?u64(selected)|0xffffff0000ULL:selected);
	return {b,sxm,sst,same,kind,mode,gap,checkpoint,base,stored,source,loaded};
}

void tms320c54x_test_state::start_dst_bypass(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_cpu->set_pipeline_frontend(false); m_pipeline_peripheral_case=variant;
	auto const f=dst_bypass_case(variant); auto &program=m_cpu->space(AS_PROGRAM); auto &data=m_cpu->space(AS_DATA);
	for (unsigned i=0;i<16;++i) program.write_word(f.base+0x700+i,0xf495);
	unsigned cursor=0;
	program.write_word(f.base+0x700+cursor++,0x4e00|(f.producer_b?0x100:0)|(f.mode==0?4:f.mode==1?0x81:0xf8));
	if (f.mode==2) program.write_word(f.base+0x700+cursor++,0x1800);
	if (f.gap) ++cursor;
	program.write_word(f.base+0x700+cursor++,(f.kind==2?0x5600:0x1000)|(f.producer_b?0:0x100)|0x82);
	program.write_word(f.base+0x700+cursor,0xf4e1);
	data.write_word(0x1800,0x2468); data.write_word(0x1801,0xace0); data.write_word(0x1900,0x1357); data.write_word(0x1901,0x9bdf);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,f.sst?1:0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,0x4800|(f.sxm?0x100:0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x17fc); m_cpu->set_state_int(tms320c54x_device::STATE_AR1,0x1800);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2,(f.same?0x1800:0x1900)+(f.kind==1?1:0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.producer_b?0x5555:f.source); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.producer_b?f.source:0x5555);
	m_pipeline_operand_reads=m_pipeline_store_writes=0;
	m_pipeline_operand_tap=data.install_read_tap(0,0xffff,"dst_bypass_read",[this,f](offs_t at,u16 &value,u16) {
		if (machine().side_effects_disabled()) return;
		u16 const first=(f.same?0x1800:0x1900)+(f.kind==1?1:0); u32 const memory=f.same?f.stored:0x13579bdf;
		expect(at==first || (f.kind==2 && at==(first^1)),"DST consumer reads only its selected word/pair");
		expect(value==(BIT(at,0)?u16(memory):u16(memory>>16)),"Non-bypassed DST consumer sees the stored or independent pair"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap=data.install_write_tap(0,0xffff,"dst_bypass_write",[this,f](offs_t at,u16 &value,u16) {
		if (machine().side_effects_disabled()) return;
		expect(m_pipeline_store_writes<2 && at==0x1800+m_pipeline_store_writes && value==(m_pipeline_store_writes?u16(f.stored):u16(f.stored>>16)),"DST preserves both ordered physical writes and saturated payload"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=399960;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x700); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}








tms320c54x_test_state::mvdd_fixture tms320c54x_test_state::mvdd_case(unsigned v, bool repeated)
{
	mvdd_fixture f{};
	f.x = (v >> 4) & 15; f.y = v & 15; f.repeated = repeated;
	unsigned const layout = repeated ? (v / 768) % 3 : (v / 1024) % 3;
	f.count = repeated ? std::array<unsigned, 3>{0, 1, 15}[(v / 256) % 3] : 0;
	f.linear = repeated ? (v / 2304) % 2 : (v / 3072) % 2;
	f.base = (repeated ? (v / 4608) % 2 : (v / 6144) % 2) ? 0x20000 : 0;
	f.irq = repeated ? (v / 9216) % 3 : 0;
	f.prefix = repeated ? 1 + (v / 27648) % 2 : 0;
	f.snapshot = repeated ? v / 55296 : v / 12288;
	f.input = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 256) % 4];
	f.st0 = 0xa602 | (v & 1 ? 0x1800 : 0);
	f.st1 = (v & 2 ? 0x4320 : 0) | (f.irq == 1 ? 0 : 0x800);
	// BK=16 requires a 32-word aligned base and an initial index below 16.
	for (unsigned i = 0; i < 4; ++i) f.initial[i] = layout == 2 ? 0x1048 + i : 0x100f + (layout == 0 ? i * 0x100 : 0);
	auto ar = f.initial;
	auto modify = [&f](u16 a, unsigned mode) -> u16 {
		if (mode == 1) return a - 1;
		if (mode == 2) return a + 1;
		if (mode == 3) return f.linear ? a + 3 : (a & 0xfff0) | ((a + 3) & 15);
		return a;
	};
	for (unsigned i = 0; i <= f.count; ++i)
	{
		f.source[i] = ar[f.x & 3]; f.destination[i] = ar[f.y & 3];
		ar[f.x & 3] = modify(ar[f.x & 3], f.x >> 2);
		if ((f.x & 3) != (f.y & 3)) ar[f.y & 3] = modify(ar[f.y & 3], f.y >> 2);
	}
	f.final = ar;
	std::array<u16, 1024> memory;
	for (unsigned i = 0; i < memory.size(); ++i) memory[i] = mvdd_fixture::seed(0x1000 + i);
	if (!repeated) memory[f.source[0] - 0x1000] = f.input;
	for (unsigned i = 0; i <= f.count; ++i)
	{
		// DARAM Read n precedes the older Write n-1 in the same clock.
		f.word[i] = memory[f.source[i] - 0x1000];
		if (i) memory[f.destination[i - 1] - 0x1000] = f.word[i - 1];
	}
	return f;
}

void tms320c54x_test_state::start_mvdd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_mvdd_fixture = mvdd_case(variant, profile_is("mvdd-repeat")); auto const &f = m_mvdd_fixture;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.prefix) program.write_word(f.base + 0x600, f.prefix == 2 ? 0xf070 : 0xec00 | f.count);
	if (f.prefix == 2) program.write_word(f.base + 0x601, f.count);
	program.write_word(f.base + 0x600 + f.prefix, 0xe500 | (f.x << 4) | f.y); program.write_word(f.base + 0x601 + f.prefix, 0xf4e1);
	if (f.irq) program.write_word(f.base + (f.irq == 2 ? 4 : 0x40), 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq == 1 ? 1 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead);
	for (unsigned i = 0; i < 4; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, f.initial[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.linear ? 0 : 16);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	for (unsigned i = 0; i <= f.count; ++i) { data.write_word(f.source[i], mvdd_fixture::seed(f.source[i])); data.write_word(f.destination[i], mvdd_fixture::seed(f.destination[i])); }
	if (!f.repeated) data.write_word(f.source[0], f.input);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_fast_stack_reads = m_pipeline_register_writes = m_vector_acks = m_port_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvdd_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && m_pipeline_store_writes == f.count + 1 && !m_fast_stack_reads && word == 0x601 + f.prefix, "RETE consumes only the post-MVDD interrupt frame"); ++m_fast_stack_reads; return; }
		unsigned const i = m_pipeline_operand_reads;
		expect(i <= f.count && at == f.source[i] && word == f.word[i] && m_cpu->pipeline_cycles() == f.first_write() - 1 + i, "MVDD reads its captured source before the preceding write, including overlap"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvdd_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && m_pipeline_store_writes == f.count + 1 && !m_pipeline_register_writes && word == 0x601 + f.prefix, "IRQ frame follows all MVDD destination writes"); ++m_pipeline_register_writes; return; }
		unsigned const i = m_pipeline_store_writes;
		expect(i <= f.count && at == f.destination[i] && word == f.word[i] && m_cpu->pipeline_cycles() == f.first_write() + i, "MVDD writes one retained word to its captured destination at Execute"); ++m_pipeline_store_writes;
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600 + f.prefix, "mvdd_fetch", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.base + 0x600 + f.prefix) { expect(!m_pipeline_extensions_reads && word == (0xe500 | (f.x << 4) | f.y), "MVDD owns one opcode fetch even when repeated"); ++m_pipeline_extensions_reads; }
		else if (f.prefix == 2 && at == f.base + 0x601) { expect(!m_port_reads && word == f.count, "Long RPT owns one retained count literal"); ++m_port_reads; }
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254140;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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




tms320c54x_test_state::indexed_mar_fixture tms320c54x_test_state::indexed_mar_case(unsigned v, bool reverse)
{
	indexed_mar_fixture f{};
	f.ar = v % 8; f.add = (v / 8) % 2; f.compatibility = (v / 16) % 2;
	f.selected = f.compatibility && !f.ar ? 4 : f.ar;
	f.index = std::array<u16, 8>{0, 1, 0xffff, 3, 0x8000, 0x7fff, 0x1234, 0xfffe}[(v / 32) % 8];
	if (reverse) f.index = std::array<u16, 8>{1, 2, 4, 8, 0x80, 0x100, 0x4000, 0x8000}[(v / 32) % 8];
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = (v / 256) % 2 ? 0xfffe - i : 0x1000 + i;
	f.initial[0] = f.index;
	f.result = f.add ? f.initial[f.selected] + f.index : f.initial[f.selected] - f.index;
	if (reverse)
	{
		// Independent bit-serial oracle, not the core's reverse/add/reverse.
		f.result = 0; int carry = 0;
		for (int bit = 15; bit >= 0; --bit)
		{
			int const sum = int(BIT(f.initial[f.selected], bit)) + (f.add ? int(BIT(f.index, bit)) : -int(BIT(f.index, bit))) + carry;
			f.result |= (sum & 1) << bit;
			carry = f.add ? int(sum >= 2) : sum < 0 ? -1 : 0;
		}
	}
	f.st0 = f.compatibility && f.ar ? 0x608 | (f.ar << 13) : 0x8608;
	f.base = (v / 512) % 2 ? 0x20000 : 0; f.snapshot = v / 1024;
	return f;
}

void tms320c54x_test_state::start_indexed_mar(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const reverse = profile_is("mar-reverse");
	auto const f = indexed_mar_case(variant, reverse); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (reverse ? f.add ? 0x6db8 : 0x6da0 : f.add ? 0x6db0 : 0x6da8) | f.ar); program.write_word(f.base + 0x601, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (f.compatibility ? 0x20 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "indexed_mar_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Indexed MAR never reads data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "indexed_mar_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Indexed MAR never writes data memory"); });
	m_pipeline_extensions_reads = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600, "indexed_mar_fetch", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "Indexed MAR opcode is fetched once"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254920;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

tms320c54x_test_state::repeated_stld_fixture tms320c54x_test_state::repeated_stld_case(unsigned v) const
{
	repeated_stld_fixture f{};
	f.arithmetic = profile_is("st-alu-rpt");
	unsigned const forms = f.arithmetic ? 8 : 6, modes = f.arithmetic ? 8 : 4;
	unsigned const layouts = f.arithmetic ? 5 : 4;
	f.form = v % forms; v /= forms; f.layout = v % layouts; v /= layouts;
	int const shift = std::array<int, 4>{0, -1, 7, -16}[v % 4]; v /= 4;
	unsigned const flags = v % modes; v /= modes;
	f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0) | (shift & 31); f.pmst = flags & 2 ? 1 : 0;
	f.iterations = std::array<unsigned, 4>{1, 2, 8, 256}[v % 4]; v /= 4;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	f.checkpoint = std::array<unsigned, 3>{5, 6 + f.iterations / 2, 6 + f.iterations}[v % 3];
	return f;
}

u16 tms320c54x_test_state::repeated_stld_address(repeated_stld_fixture const &f, unsigned i, bool y)
{
	if (y && f.layout != 3) return 0x1c00 + (f.layout ? i : 0);
	return 0x1800 + (f.layout == 4 ? 3 * int(i) : f.layout == 2 ? -int(i) : f.layout ? int(i) : 0);
}

u16 tms320c54x_test_state::repeated_stld_sample(unsigned i)
{ return 0x8001 ^ u16(i * 0x1111); }

u16 tms320c54x_test_state::repeated_stld_store(repeated_stld_fixture const &f, std::array<u64, 3> const &state)
{
	u64 value = state[f.arithmetic ? (f.form / 2) & 1 : f.form >= 4 ? f.form - 4 : f.form / 2];
	int const shift = int(f.st1 & 31) - (f.st1 & 16 ? 32 : 0);
	s64 const signed_value = value & 0x8000000000ULL ? s64(value) - 0x10000000000LL : s64(value);
	value = shift < 0 ? u64(f.st1 & 0x100 ? signed_value >> -shift : s64(value >> -shift)) & 0xffffffffffULL : (value << shift) & 0xffffffffffULL;
	if (f.pmst & 1)
	{
		if (f.st1 & 0x100) { s64 const number = value & 0x8000000000ULL ? s64(value) - 0x10000000000LL : s64(value); value = u64(std::clamp(number, s64(-2147483648LL), s64(2147483647LL))); }
		else value = std::min(value, u64(0xffffffff));
	}
	return value >> 16;
}

void tms320c54x_test_state::start_repeated_stld(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = repeated_stld_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	u16 const addressing = std::array<u16, 5>{0x01, 0xb9, 0x49, 0x84, 0xc9}[f.layout];
	u16 const opcode = (f.arithmetic ? 0xc000 | ((f.form / 4) << 10) | ((f.form & 1) << 8) | (((f.form / 2) & 1) << 9) : f.form < 4 ? 0xc800 | ((f.form & 1) << 8) | ((f.form / 2) << 9) : 0xe400 | ((f.form - 4) << 9)) | addressing;
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xec00 | (f.iterations - 1)); program.write_word(f.base + 0x601, opcode); program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	u16 const initial_status = f.arithmetic ? 0xa030 | (f.form & 1 ? 0x800 : 0) : 0xa630;
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, initial_status); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0180014321ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff12348765ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0);
	for (unsigned ar = 2; ar < 6; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x4800 + ar);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + (f.layout == 1 ? 5 : 2), 0x1800);
	if (f.layout != 3) m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1c00);
	for (unsigned i = 0; i < (f.layout ? f.iterations : 1); ++i) { data.write_word(repeated_stld_address(f, i, true), 0xdead); data.write_word(repeated_stld_address(f, i, false), repeated_stld_sample(i)); }
	m_repeat_load_expected = {{{0x0180014321ULL, 0xff12348765ULL, f.arithmetic ? u64(initial_status) : 0xbeefULL}}};
	for (unsigned i = 0; i < f.iterations; ++i)
	{
		auto state = m_repeat_load_expected.back();
		s64 const memory = s64(f.st1 & 0x100 ? s16(repeated_stld_sample(i)) : s32(repeated_stld_sample(i))) * 65536;
		if (f.arithmetic)
		{
			unsigned const destination = f.form & 1;
			u64 const old = state[!destination], operand = u64(memory) & 0xffffffffffULL;
			s64 const other = old & 0x8000000000ULL ? s64(old) - 0x10000000000LL : s64(old);
			s64 result = f.form >= 4 ? memory - other : other + memory;
			bool const carry = f.form >= 4 ? u32(operand) >= u32(old) : u64(u32(operand)) + u32(old) > 0xffffffffULL;
			state[2] = (state[2] & ~u64(0x800)) | (carry ? 0x800 : 0);
			if (result < -2147483648LL || result > 2147483647LL) { state[2] |= destination ? 0x200 : 0x400; if (f.st1 & 0x200) result = std::clamp(result, s64(-2147483648LL), s64(2147483647LL)); }
			state[destination] = u64(result) & 0xffffffffffULL;
		}
		else if (f.form < 4) state[f.form & 1] = u64(memory) & 0xffffffffffULL;
		else state[2] = repeated_stld_sample(i);
		m_repeat_load_expected.push_back(state);
	}
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeated_stld_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads++; expect(i < f.iterations && at == repeated_stld_address(f, i, false) && word == repeated_stld_sample(i) && m_cpu->pipeline_cycles() == 6 + i, "Repeated STLD captures one fresh X sample before its own Y publication"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, f.layout ? u16(~word) : repeated_stld_sample(i + 1)); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeated_stld_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes++; expect(i < f.iterations && at == repeated_stld_address(f, i, true) && word == repeated_stld_store(f, m_repeat_load_expected[i]) && m_cpu->pipeline_cycles() == 7 + i, "Repeated STLD stores the Execute-live old accumulator before load replacement"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "repeated_stld_opcode", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(!m_pipeline_extensions_reads++, "Repeated STLD retains one body opcode"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62136;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::finish_parallel_load_case(parallel_load_fixture &c)
{
	auto &f = c.memory;
	int const shift = int(f.st1 & 31) - (f.st1 & 16 ? 32 : 0);
	u64 const source = f.source ? c.b : c.a;
	s64 const signed_source = source & (1ULL << 39) ? s64(source) - (1LL << 40) : s64(source);
	u64 shifted = shift < 0 ? u64(f.st1 & 0x100 ? signed_source >> -shift : s64(source >> -shift)) & 0xffffffffffULL : (source << shift) & 0xffffffffffULL;
	if (f.pmst & 1)
	{
		if (f.st1 & 0x100) { s64 const number = shifted & (1ULL << 39) ? s64(shifted) - (1LL << 40) : s64(shifted); shifted = u64(std::clamp(number, s64(-2147483648LL), s64(2147483647LL))); }
		else shifted = std::min(shifted, u64(0xffffffff));
	}
	f.stored = shifted >> 16;
	u64 const loaded = u64(s64(f.st1 & 0x100 ? s16(f.xword) : s32(f.xword)) * 65536) & 0xffffffffffULL;
	c.result_a = c.form < 4 && !f.destination ? loaded : c.a;
	c.result_b = c.form < 4 && f.destination ? loaded : c.b;
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::parallel_load_case(unsigned variant)
{
	unsigned const extent = 67584, v = variant % extent;
	parallel_load_fixture c{}; c.form = variant / extent;
	auto &f = c.memory; f = dual_multiply_case(v % 18432, false);
	unsigned seed = v / 512;
	if (v >= 18432)
	{
		unsigned const n = v - 18432, flags = (n / 512) % 16;
		f.x = 8; f.y = 9; f.initial = {0x1000, 0x1100, 0x1200, 0x1300}; f.final = {0x1001, 0x1101, 0x1200, 0x1300};
		f.xaddress = 0x1000; f.yaddress = 0x1100;
		f.xword = std::array<u16, 8>{0, 1, 0x7fff, 0x8000, 0xffff, 0x1234, 0x5555, 0xaaaa}[n % 8];
		int const shift = std::array<int, 8>{-16, -15, -8, -1, 0, 1, 7, 15}[(n / 64) % 8];
		f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0) | (shift & 31);
		f.pmst = flags & 2 ? 1 : 0; f.st0 = 0xa002 | (flags & 8 ? 0x800 : 0);
		f.base = (n / 8192) % 2 ? 0x20000 : 0; f.snapshot = n / 16384; f.linear = true; seed = (n / 8) % 8;
	}
	else { f.st1 = (f.st1 & ~31) | (v & 31); f.pmst |= (v / 32) & 1; }
	f.source = c.form >= 4 ? c.form - 4 : c.form / 2; f.destination = c.form & 1;
	c.a = std::array<u64, 8>{0, 1, 0xffffffffffULL, 0x7fffffff, 0xff80000000ULL, 0x0180000000ULL, 0xfe7fffffffULL, 0x123456789aULL}[seed % 8];
	c.b = c.a ^ 0x5aa55aa55aULL;
	finish_parallel_load_case(c);
	return c;
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::mapped_parallel_load_case(unsigned v)
{
	parallel_load_fixture c{}; auto &f = c.memory;
	c.form = v % 6; v /= 6; f.xaddress = v % 32; v /= 32;
	unsigned const selectors = std::array<unsigned, 8>{0x01,0x0a,0x45,0xcd,0xf2,0x83,0xbc,0xef}[v % 8]; v /= 8;
	unsigned const pattern = v % 2; v /= 2; unsigned const flags = v % 8; v /= 8;
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v;
	f.x = selectors >> 4; f.y = selectors & 15; f.source = c.form >= 4 ? c.form - 4 : c.form / 2; f.destination = c.form & 1;
	f.initial = {0x401f,0x411f,0x421f,0x431f}; f.initial[f.x & 3] = f.xaddress; f.yaddress = f.initial[f.y & 3]; f.final = f.initial;
	auto modify = [](u16 at, unsigned mode) -> u16 { return at + (mode == 1 ? -1 : mode == 2 ? 1 : mode == 3 ? 3 : 0); };
	f.final[f.x & 3] = modify(f.initial[f.x & 3], f.x >> 2); f.final[f.y & 3] = modify(f.initial[f.y & 3], f.y >> 2); f.linear = true;
	f.st0 = 0xa002 | (pattern ? 0x800 : 0); f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0) | (pattern ? 31 : 0); f.pmst = flags & 2 ? 1 : 0;
	c.a = pattern ? 0xfe7fffffffULL : 0x123456789aULL; c.b = c.a ^ 0x5aa55aa55aULL; c.imr = pattern ? 0xa55a : 0; c.ifr = pattern ? 0x5aa5 : 0;
	u64 const initial_a = c.a ^ 0x1111111111ULL, initial_b = c.b ^ 0x1111111111ULL;
	if (f.xaddress == 0) f.xword = c.imr;
	else if (f.xaddress == 1) f.xword = c.ifr;
	else if (f.xaddress < 6 || f.xaddress == 0x1f) f.xword = pattern ? 0x7fff : 0x8001;
	else if (f.xaddress == 6) f.xword = f.st0;
	else if (f.xaddress == 7) f.xword = f.st1;
	else if (f.xaddress <= 0x0a) f.xword = initial_a >> ((f.xaddress - 8) * 16);
	else if (f.xaddress <= 0x0d) f.xword = initial_b >> ((f.xaddress - 11) * 16);
	else if (f.xaddress == 0x0e) f.xword = 0xbeef;
	else if (f.xaddress == 0x0f) f.xword = 0x7fff;
	else if (f.xaddress == 0x10) f.xword = 3;
	else if (f.xaddress == 0x11) f.xword = 0x1700;
	else if (f.xaddress <= 0x15) f.xword = f.final[f.xaddress - 0x12];
	else if (f.xaddress <= 0x17) f.xword = f.xaddress == 0x16 ? 0x1800 : 0x1900;
	else f.xword = std::array<u16, 7>{0x3000,0,7,0x4321,0x5678,f.pmst,u16(f.base >> 16)}[f.xaddress - 0x18];
	finish_parallel_load_case(c); return c;
}

void tms320c54x_test_state::start_parallel_load(unsigned variant)
{
	bool const arithmetic = profile_is("st-par-alu");
	bool const mapped = profile_is("stld-map");
	bool const multiply = profile_is("st-par-mpy");
	bool const bsp = profile_is("stld-bsp");
	bool const bus = profile_is("stld-periph"), peripheral = bsp || bus;
	bool const projected = !mapped && !peripheral;
	if (!variant && projected) m_parallel_coverage = {};
	unsigned const key = projected ? parallel_schedule::key(variant) : variant;
	auto const c = multiply ? parallel_multiply_case(key) : bus ? peripheral_parallel_load_case(variant) : bsp ? bsp_parallel_load_case(variant) : mapped ? mapped_parallel_load_case(variant) : arithmetic ? parallel_arithmetic_case(key) : parallel_load_case(key); auto const &f = c.memory;
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (multiply ? 0xcc00 | (f.destination << 8) : arithmetic ? 0xc000 | ((c.form / 4) << 10) | (f.destination << 8) : c.form >= 4 ? 0xe400 : 0xc800 | (f.destination << 8)) | (f.source << 9) | (f.x << 4) | f.y);
	program.write_word(f.base + 0x601, 0xf4e1);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, c.ifr); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, c.imr); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, c.a ^ 0x1111111111ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, c.b ^ 0x1111111111ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, c.t); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	for (unsigned i = 0; i < 4; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, f.initial[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.linear ? 0 : 16);
	if (mapped) { m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1700); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x1800); m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x1900); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x7fff); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 7); m_cpu->set_state_int(tms320c54x_device::STATE_RSA, 0x4321); m_cpu->set_state_int(tms320c54x_device::STATE_REA, 0x5678); }
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	if (f.xaddress >= 0x60 || (mapped && ((f.xaddress >= 2 && f.xaddress <= 5) || f.xaddress == 0x1f))) data.write_word(f.xaddress, f.xword);
	if (f.yaddress >= 0x60 && f.yaddress != f.xaddress) data.write_word(f.yaddress, 0xdead);
	if (bsp) { m_shifted_bsp.reset(); m_shifted_bsp.control_w(0x80); expect(m_shifted_bsp.frame_receive(f.xword), "Seed parallel BSP receive"); expect(!m_shifted_bsp.frame_receive(f.xword ^ 0xffff) && m_shifted_bsp.receive_overrun, "Seed unread BSP overrun"); }
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_shifted_bus_word = f.xword;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "parallel_load_source", [this, f, bsp, bus, peripheral](offs_t at, u16 &word, u16) {
		if (bsp && at == f.xaddress) { word = machine().side_effects_disabled() ? m_shifted_bsp.receive : m_shifted_bsp.receive_r(); }
		if (bus && at == f.xaddress) word = m_shifted_bus_word;
		if (machine().side_effects_disabled()) return;
		expect(!m_pipeline_operand_reads && at == f.xaddress && word == f.xword && m_cpu->pipeline_cycles() == (peripheral ? 6 : 5), "Parallel load retains one X sample before any aliased Y write"); ++m_pipeline_operand_reads;
		if (bus) m_shifted_bus_word ^= 0xffff;
		if (bsp) { expect(!m_shifted_bsp.receive_ready && !m_shifted_bsp.receive_overrun, "Parallel BSP read acknowledges ready and overrun once"); expect(m_shifted_bsp.frame_receive(f.xword ^ 0xffff), "Next BSP frame remains pending"); }
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "parallel_load_store", [this, f, peripheral](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at != f.yaddress || word != f.stored) osd_printf_info("STLD store mismatch: case=%u address=%04x expected=%04x value=%04x expected=%04x\n", m_pipeline_peripheral_case, unsigned(at), f.yaddress, word, f.stored);
		expect(!m_pipeline_store_writes && at == f.yaddress && word == f.stored && m_cpu->pipeline_cycles() == (peripheral ? 7 : 6), "Parallel store uses the Execute-live old accumulator with retained ASM before its load replacement"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62164;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

tms320c54x_test_state::xmem_load_fixture tms320c54x_test_state::xmem_load_case(unsigned v)
{
	unsigned const pattern = (v / 256) % 8;
	return {v % 16, (v / 16) % 16, (v / 2048) % 2, (v / 4096) % 8,
		std::array<u16, 8>{0, 1, 0x7fff, 0x8000, 0xffff, 0x1234, 0x5555, 0xaaaa}[pattern], (v / 32768) % 2 ? 0x20000U : 0, bool(pattern & 4)};
}

void tms320c54x_test_state::start_xmem_load(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = xmem_load_case(variant % 196608);
	bool const arithmetic = profile_is("xmem-alu");
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (arithmetic ? 0x9000 | (variant >= 196608 ? 0x200 : 0) : 0x9400) | (f.destination << 8) | (f.x << 4) | f.shift); program.write_word(f.base + 0x601, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0()); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1());
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x4000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead);
	if (arithmetic) m_cpu->set_state_int(f.destination ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, xmem_arithmetic_expected(variant).source);
	for (unsigned i = 2; i <= 5; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x100f + 0x100 * (i - 2));
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.linear ? 0 : 16);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); data.write_word(f.address(), f.value);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "xmem_load_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && at == f.address() && word == f.value && m_cpu->pipeline_cycles() == 5, "Xmem LD owns one captured Read regardless of later AR mutation"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "xmem_load_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Xmem LD writes no data memory"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600, "xmem_load_fetch", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "Xmem LD fetches one opcode"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254120;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u64 tms320c54x_test_state::xmem_store_source(unsigned variant)
{
	return std::array<u64, 8>{0, 1, 0x7fffffff, 0x80000000ULL, 0xff80000000ULL, 0xffffffffULL, 0x123456789aULL, 0xffffffffffULL}[(variant / 256) % 8];
}

u16 tms320c54x_test_state::xmem_store_expected(unsigned variant)
{
	auto const f = xmem_load_case(variant % 196608);
	u64 value = (xmem_store_source(variant) << f.shift) & 0xffffffffffULL;
	if ((variant / 393216) % 2)
	{
		if (f.flags & 1)
		{
			s64 const signed_value = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
			value = u32(std::clamp(signed_value, s64(-2147483648LL), s64(2147483647LL)));
		}
		else if (value > 0xffffffffULL) value = 0xffffffffULL;
	}
	return u16(value >> ((variant / 196608) % 2 ? 16 : 0));
}

void tms320c54x_test_state::start_xmem_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = xmem_load_case(variant % 196608);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x9800 | (((variant / 196608) % 2) << 9) | (f.destination << 8) | (f.x << 4) | f.shift);
	program.write_word(f.base + 0x601, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, (variant / 393216) % 2);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0()); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1());
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x4000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead);
	for (unsigned ar = 2; ar <= 5; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x100f + 0x100 * (ar - 2));
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.linear ? 0 : 16);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	data.write_word(f.address(), 0xbeef);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "xmem_store_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_operand_reads; expect(false, "Xmem store never reads its destination"); } });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "xmem_store_write", [this, f, variant](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_store_writes && at == f.address() && word == xmem_store_expected(variant) && m_cpu->pipeline_cycles() == 6, "Xmem store writes the Execute-owned source to the retained EAB exactly once"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600, "xmem_store_fetch", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "Xmem store fetches one opcode"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254840;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::repeat_push_fixture tms320c54x_test_state::repeat_push_case(unsigned v)
{
	return {std::array<unsigned, 4>{0, 1, 4, 11}[v % 4], (v / 4) % 4, (v / 32) % 3, bool((v / 96) % 2), (v / 16) % 2 ? 0x20000U : 0};
}

void tms320c54x_test_state::start_repeat_push(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = repeat_push_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	u32 const body = f.base + (f.long_form ? 0x602 : 0x601);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.long_form ? 0xf070 : 0xec00 | f.count);
	if (f.long_form) program.write_word(f.base + 0x601, f.count);
	program.write_word(body, f.kind == 1 || f.kind == 3 ? 0x4b97 : 0x4b8f); program.write_word(body + 1, 0xf4e1);
	if (f.interrupt)
	{
		u32 const vector = f.base + (f.interrupt == 2 ? 4 : 0x40);
		program.write_word(vector, 0x4a1e); program.write_word(vector + 1, 0xf495); program.write_word(vector + 2, 0xf4e5);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.interrupt == 1 ? 1 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5678); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x9abc);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	for (unsigned i = 0; i < 8; ++i) data.write_word(0x10 + i, f.first_source());
	for (unsigned i = 2; i <= 5; ++i) data.write_word(i, 0x9000 + i);
	if (f.kind < 2) for (unsigned i = 0; i <= f.count; ++i) data.write_word(f.source(i), f.value(i));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_fast_stack_reads = m_pipeline_register_writes = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeat_push_source",
		[this, f](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			if (at == 0x1e) { expect(f.interrupt && m_vector_acks == 1 && word == f.base >> 16, "ISR saves its current XPC after repeated pushes retire"); return; }
			if (at < 0x1000 - f.count - 1 && at >= 0x1000 - f.count - 3) { expect(f.interrupt && m_pipeline_store_writes == f.count + 1, "ISR return cannot precede the final repeated push"); ++m_fast_stack_reads; return; }
			unsigned const i = m_pipeline_operand_reads;
			if (i > f.count || at != f.source(i) || word != f.value(i) || m_cpu->pipeline_cycles() != f.first_write() - 1 + i)
				osd_printf_info("repeat_push_read: case=%u index=%u address=%04x word=%04x clock=%llu expected_address=%04x expected_word=%04x expected_clock=%u\n", m_pipeline_peripheral_case, i, at, word, (unsigned long long)m_cpu->pipeline_cycles(), f.source(i), i <= f.count ? f.value(i) : 0, f.first_write() - 1 + i);
			expect(i <= f.count && at == f.source(i) && word == f.value(i) && m_cpu->pipeline_cycles() == f.first_write() - 1 + i, "Each repeated PSHD owns a distinct Read before the preceding Execute"); ++m_pipeline_operand_reads;
			if (f.kind < 2) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xdead); }
		});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeat_push_stack",
		[this, f](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			if (at < 0x1000 - f.count - 1) { expect(f.interrupt && m_pipeline_store_writes == f.count + 1 && at >= 0x1000 - f.count - 3, "IRQ frame is below the completed repeated-push frame"); ++m_pipeline_register_writes; return; }
			unsigned const i = m_pipeline_store_writes;
			expect(i <= f.count && at == 0xfff - i && word == f.value(i) && m_cpu->pipeline_cycles() == f.first_write() + i, "Repeated PSHD publishes one retained stack word per Execute"); ++m_pipeline_store_writes;
		});
	m_pipeline_fetch_tap = program.install_read_tap(body, body, "repeat_push_body",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "Repeated PSHD fetches its opcode once"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254070;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_push_exclusion(bool self_ar)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = self_ar;
	auto &program = m_cpu->space(AS_PROGRAM); program.write_word(0x600, 0x4b97); program.write_word(0x601, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR7, self_ar ? 0x17 : 0x18);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "push_exclusion_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "push_exclusion_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_idle_fixture_tick = 0; m_phase = 254090; m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mapped_pshd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const source = std::array<u16, 12>{0, 1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}[variant % 12];
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 12) % 4];
	u16 const expected = source <= 1 ? 0 : source == 6 ? 0xa600 : source == 7 ? 0x800 : (source == 10 || source == 13) ? value & 0xff : value;
	u32 const base = (variant / 48) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x4b80); program.write_word(base + 0x601, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	for (unsigned i = 0; i < 8; ++i) data.write_word(0x10 + i, source);
	if (source >= 8) data.write_word(source, value);
	data.write_word(0xfff, 0xbeef);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mapped_pshd_source",
		[this, source, expected](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && at == source && word == expected && m_cpu->pipeline_cycles() == 5, "Mapped PSHD samples the actual register once in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mapped_pshd_stack",
		[this, expected](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_store_writes && at == 0xfff && word == expected && m_cpu->pipeline_cycles() == 6, "Mapped PSHD publishes its retained source once in Execute"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254050;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::standard_reada_brc() const
{ return profile_is("reada-brc16"); }

tms320c54x_test_state::reada_brc_fixture tms320c54x_test_state::reada_brc_case(unsigned v) const
{
	reada_brc_fixture f{};
	f.form = v % 6; v /= 6; f.reg = v % 8; v /= 8; unsigned const pattern = v % 4; v /= 4;
	f.logical = (v % 2 ? 0x20000 : 0) | 0x400; v /= 2; f.overlay = v % 2; v /= 2;
	f.gap = 1 + v % 3; v /= 3; f.loop = v % 2; v /= 2;
	f.checkpoint = std::array<unsigned, 4>{6, 9, 11, 16}[v];
	f.value = (f.loop ? std::array<u16, 4>{0, 1, 2, 7} : std::array<u16, 4>{0, 1, 0x8000, 0xffff})[pattern];
	f.physical = standard_reada_brc() || f.overlay ? f.logical & 0xffff : f.logical;
	f.mode = f.form == 0 ? 0x1a : f.form == 1 ? 0x0a : f.form == 2 ? 0xf8 : (f.form == 3 ? 0x80 : f.form == 4 ? 0x90 : 0x88) | f.reg;
	f.st1 = f.form == 1 ? 0x4800 : 0x800;
	f.body_reg = (f.reg + 1) % 8; f.words = f.form == 2 ? 2 : 1; f.read = 8 + f.words; f.write = f.read + 1;
	return f;
}

void tms320c54x_test_state::start_reada_brc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = reada_brc_case(variant); m_cpu->set_extended_program(!standard_reada_brc());
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 40; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0x7e00 | f.mode); if (f.form == 2) program.write_word(0x601, 0x1a);
	if (f.loop)
	{
		unsigned const setup = 0x600 + f.words + f.gap;
		program.write_word(setup, 0xf072); program.write_word(setup + 1, setup + 3);
		program.write_word(setup + 2, 0x7690 | f.body_reg); program.write_word(setup + 3, 0x2222); program.write_word(setup + 4, 0xf4e1);
	}
	else program.write_word(0x600 + f.words + 6, 0xf4e1);
	program.write_word(f.physical, f.value); program.write_word(0x480, f.value ^ 0xffff); program.write_word(0x30480, f.value ^ 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.overlay ? 0x20 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1200000000ULL | f.logical); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.form == 1 ? 0x10 : 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0x55aa);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == f.body_reg ? 0x1400 : f.form >= 3 && i == f.reg ? 0x1a : 0x1500 + i);
	m_pipeline_operand_reads = m_pipeline_register_writes = m_pipeline_store_writes = m_pipeline_extensions_reads = m_fast_stack_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(f.physical, f.physical, "reada_brc_source", [this, f](offs_t, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		expect(!m_pipeline_operand_reads && value == f.value && m_cpu->pipeline_cycles() == f.read, "READA BRC reads the retained physical program source once"); ++m_pipeline_operand_reads;
	});
	m_pipeline_fetch_tap = program.install_read_tap(0x601, 0x601, "reada_brc_extension", [this, f](offs_t, u16 &value, u16) {
		if (machine().side_effects_disabled() || f.form != 2) return;
		expect(!m_pipeline_extensions_reads && value == 0x1a, "Absolute READA BRC owns one retained destination extension"); ++m_pipeline_extensions_reads;
	});
	m_vector_prefetch_tap = data.install_read_tap(0, 0xffff, "reada_brc_no_data_reads", [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled()) { ++m_fast_stack_reads; expect(false, "READA BRC and its immediate-store loop perform no data reads"); }
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "reada_brc_writes", [this, f](offs_t address, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		if (address == 0x1a)
		{
			expect(!m_pipeline_register_writes && value == f.value && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() == f.write, "READA publishes BRC once in Execute using the retained word");
			++m_pipeline_register_writes; return;
		}
		unsigned const i = m_pipeline_store_writes++;
		expect(f.loop && i <= f.value && address == 0x1400 + i && value == 0x2222 && m_pipeline_register_writes == 1, "Spaced RPTB consumes the new READA count, not the stale BRC seed");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62066;
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mapped_read_a(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const destination = 8 + variant % 6;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 6) % 4];
	bool const absolute = (variant / 24) % 2;
	u32 const source = ((variant / 48) % 2 ? 0x20000 : 0) | 0x400;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0x7e00 | (absolute ? 0xf8 : destination));
	if (absolute) program.write_word(0x601, destination);
	program.write_word(source, value); program.write_word(0x30400, value ^ 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1200000000ULL | source); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(source, source, "mapped_read_a_source",
		[this, value, absolute](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && word == value && m_cpu->pipeline_cycles() == (absolute ? 10 : 9), "Mapped READA retains the full program source through held clocks"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(destination, destination, "mapped_read_a_write",
		[this, value, absolute](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_store_writes && word == value && m_cpu->pipeline_cycles() == (absolute ? 11 : 10), "Mapped READA publishes the retained word once in Execute"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(0x601, 0x601, "mapped_read_a_extension",
		[this, absolute, destination](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || !absolute) return; expect(!m_pipeline_extensions_reads && word == destination, "Mapped READA owns one absolute extension"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253610;
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::page_zero_stack_fixture tms320c54x_test_state::page_zero_stack_case(unsigned v, bool edge)
{
	if (edge) { unsigned const index = v % 192; return {(v / 192) % 2, v / 768, u16(index < 64 ? 0x20 + index : 0x40 + index), 0x1357, 0x3000, 0x4900, (v / 384) % 2 ? 0x20000U : 0U}; }
	u16 const address = 0x60 + v % 32;
	return {(v / 12288) % 6, (v / 4096) % 3, address,
		std::array<u16, 8>{0, 1, 0x7f, 0x80, 0xff, 0x7fff, 0x8000, 0xffff}[(v / 32) % 8],
		std::array<u16, 4>{0x3000, 0x3001, u16(address + ((v / 12288) % 6 == 5 ? 2 : 1)), u16(address + ((v / 12288) % 6 == 5 ? 3 : 2))}[(v / 256) % 4],
		u16((v / 1024) % 2 ? 0x4900 : 0x900), (v / 2048) % 2 ? 0x20000U : 0U};
}

void tms320c54x_test_state::start_page_zero_stack(unsigned variant)
{
	bool const edge = profile_is("stack-page0-edge"); auto const f = page_zero_stack_case(variant, edge);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (f.form == 1 ? 0x8a00 : 0x4a00) | f.address);
	unsigned const gap = f.form == 3 ? 1 : f.form == 4 ? 3 : 0;
	if (!edge && f.form >= 2) program.write_word(f.base + 0x601 + gap, f.form == 5 ? 0x4a60 | ((f.address + 1) & 31) : 0x8a00 | f.address);
	program.write_word(f.base + (f.form >= 2 ? 0x602 + gap : 0x601), 0xf4e1);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	if (!edge) { data.write_word(f.address, f.value); data.write_word(0x60 | ((f.address + 1) & 31), f.value ^ 0x5aa5); if (f.form == 1) data.write_word(f.sp, f.value); }
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "page_zero_stack_read", [this, f, gap, edge](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_operand_reads++;
		u16 const address = !i ? f.form == 1 ? f.sp : f.address : f.form == 5 ? 0x60 | ((f.address + 1) & 31) : f.sp - 1;
		u16 const value = i && f.form == 5 && f.sp - 1 != address ? f.value ^ 0x5aa5 : f.value;
		if (edge || at != address || word != value || m_cpu->pipeline_cycles() != 5 + (i ? 1 + gap : 0)) osd_printf_info("Page-zero stack read mismatch: case=%u form=%u replay=%u read=%u address=%04x expected=%04x word=%04x expected=%04x clock=%llu\n", m_pipeline_peripheral_case, f.form, unsigned(m_pipeline_replay), i, unsigned(at), address, word, value, (unsigned long long)m_cpu->pipeline_cycles());
		expect(!edge && i < (f.form >= 2 ? 2U : 1U) && at == address && word == value && m_cpu->pipeline_cycles() == 5 + (i ? 1 + gap : 0), "Page-zero stack instructions retain their source once in order");
		auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, word ^ 0xffff);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "page_zero_stack_write", [this, f, gap, edge](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_store_writes++;
		u16 const address = !i ? f.form == 1 ? f.address : f.sp - 1 : f.form == 5 ? f.sp - 2 : f.address;
		u16 const value = i && f.form == 5 && f.sp - 1 != (0x60 | ((f.address + 1) & 31)) ? f.value ^ 0x5aa5 : f.value;
		expect(!edge && i < (f.form >= 2 ? 2U : 1U) && at == address && word == value && m_cpu->pipeline_cycles() == 6 + (i ? 1 + gap : 0), "Page-zero stack instructions publish the retained word once at the encoded destination");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62162;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}







u16 tms320c54x_test_state::square_repeat_memory(poly_fixture const &f, unsigned ordinal)
{
	return f.form == 2 ? f.t ^ u16(ordinal * 0x1111) : ordinal & 1 ? u16(~f.t) : f.t;
}




void tms320c54x_test_state::start_t_store_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = variant & 8 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, variant & 1 ? 0xf830 : 0xf820); program.write_word(base + 0x601, 0x602);
	program.write_word(base + 0x602, variant & 4 ? 0x8df8 : 0x8cf8); program.write_word(base + 0x603, 0x1200); program.write_word(base + 0x604, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | (variant & 2 ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x2468); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x3579);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "t_store_branch_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "t_store_branch_write", [this, variant](offs_t at, u16 &value, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_store_writes && at == 0x1200 && value == (variant & 4 ? 0x3579 : 0x2468), "A nondelayed conditional branch reaches one ordinary T/TRN store on either outcome"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62136;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mvdp(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	unsigned const form = variant % 7, reg = (variant / 7) % 8;
	u32 const base = (variant / 56) % 2 ? 0x20000 : 0;
	bool const extended = form >= 3;
	u16 const source = std::array<u16, 7>{0x1203, 0x1203, 0x1230, 0x1230, 0x122f, 0x1231, 0x1201}[form];
	u8 const mode = form < 2 ? 3 : form == 2 ? 0x90 + reg : form == 3 ? 0xf8 : 0xe0 + (form - 4) * 8 + reg;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x7d00 | mode);
	// Absolute Smem precedes pmad; long-offset Smem follows pmad.
	program.write_word(base + 0x601, form == 3 ? source : 0x2e00);
	if (extended) program.write_word(base + 0x602, form == 3 ? 0x2e00 : form == 4 ? 0xffff : form == 5 ? 1 : 2);
	program.write_word(base + 0x602 + extended, 0xf4e1);
	program.write_word(base + 0x2e00, 0xbeef);
	m_cpu->space(AS_DATA).write_word(source, 0x2468);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa624}, {cpu_regs::STATE_ST1, 0x800 | (form == 1 ? 0x4000 : 0)},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_SP, 0x1200},
		{cpu_regs::STATE_BK, 16},
	});
	for (unsigned ar = 0; ar < 8; ++ar)
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == reg ? form == 6 ? 0x120f : 0x1230 : 0x3000 + ar);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mvdp_source",
		[this, source](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == source && word == 0x2468 && !m_pipeline_operand_reads, "MVDP reads the retained source exactly once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = program.install_write_tap(0, 0x7fffff, "mvdp_destination",
		[this, base](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == base + 0x2e00 && word == 0x2468 && !m_pipeline_store_writes, "MVDP writes once in its instruction page"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 259900;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::repeat_mvkd_fixture tms320c54x_test_state::repeat_mvkd_case(unsigned v)
{
	repeat_mvkd_fixture f{};
	f.form = v % 6; f.reg = (v / 6) % 8;
	f.count = std::array<unsigned, 4>{0, 1, 2, 7}[(v / 48) % 4];
	f.prefix = (v / 192) % 2 ? 2 : 1;
	f.destination = std::array<u16, 3>{0x2000, 0x1000, 0x1001}[(v / 384) % 3];
	f.base = (v / 1152) % 2 ? 0x20000 : 0; f.snapshot = v / 2304;
	f.initial_ar = f.destination - unsigned(f.form == 5);
	f.opcode = 0x7000 | (f.form < 2 ? f.form ? 5 : f.destination & 0x7f :
		(f.form == 2 ? 0x80 : f.form == 3 ? 0x90 : f.form == 4 ? 0x88 : 0x98) | f.reg);
	f.st0 = 0xa600 | (f.form ? 0 : f.destination >> 7); f.st1 = 0x800 | (f.form == 1 ? 0x4000 : 0);
	f.read = 6 + f.prefix;
	return f;
}

bool tms320c54x_test_state::repeat_mvkd_bypass(repeat_mvkd_fixture const &f, unsigned n)
{
	if (!n) return false;
	u16 const older_destination = f.destination + (f.form == 3 || f.form == 5 ? n - 1 : f.form == 4 ? -int(n - 1) : 0);
	return older_destination == 0x1000 + n;
}

u16 tms320c54x_test_state::repeat_mvkd_value(repeat_mvkd_fixture const &f, unsigned n)
{
	// SPRU131G 7.3.2 forwards any same-location DB/CB read from
	// the pending write bus. These layouts never overwrite a later,
	// nonadjacent source, so only the immediately older copy can feed it.
	while (repeat_mvkd_bypass(f, n)) --n;
	return 0x2468 + n;
}

void tms320c54x_test_state::start_repeat_mvkd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = repeat_mvkd_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.prefix == 2 ? 0xf070 : 0xec00 | f.count);
	if (f.prefix == 2) program.write_word(f.base + 0x601, f.count);
	program.write_word(f.base + 0x600 + f.prefix, f.opcode); program.write_word(f.base + 0x601 + f.prefix, 0x1000);
	program.write_word(f.base + 0x602 + f.prefix, 0xf4e1);
	for (unsigned i = 0; i <= f.count; ++i) data.write_word(0x1000 + i, 0x2468 + i);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.destination - 5);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x9876543210ULL);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == f.reg ? f.initial_ar : 0x3300 + i);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeat_mvkd_read", [this, f](offs_t at, u16 &value, u16) { if (machine().side_effects_disabled()) return; unsigned const n = at - 0x1000; ++m_pipeline_operand_reads; expect(n <= f.count && !repeat_mvkd_bypass(f, n) && value == 0x2468 + n && m_cpu->pipeline_cycles() == f.read + n, "Repeated MVKD reads each non-bypassed DAR source once"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeat_mvkd_write", [this, f](offs_t at, u16 &value, u16) { if (machine().side_effects_disabled()) return; unsigned const n = m_pipeline_store_writes++; u16 const destination = f.destination + (f.form == 3 || f.form == 5 ? n : f.form == 4 ? -int(n) : 0); bool const valid = n <= f.count && at == destination && value == repeat_mvkd_value(f, n) && m_cpu->pipeline_cycles() == f.read + 1 + n; if (!valid) osd_printf_info("repeat_mvkd_write: case=%u replay=%u n=%u at=%04x expected=%04x value=%04x cycle=%llu expected_cycle=%u\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), n, unsigned(at), destination, value, (unsigned long long)m_cpu->pipeline_cycles(), f.read + 1 + n); expect(valid, "Repeated MVKD writes its retained source once on each consecutive Execute clock"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600 + f.prefix, f.base + 0x601 + f.prefix, "repeat_mvkd_fetch", [this, f](offs_t at, u16 &value, u16) { if (machine().side_effects_disabled()) return; unsigned const n = m_pipeline_extensions_reads++; bool const valid = n < 2 && at == f.base + 0x600 + f.prefix + n && value == (n ? 0x1000 : f.opcode); if (!valid) osd_printf_info("repeat_mvkd_fetch: case=%u replay=%u n=%u at=%06x value=%04x cycle=%llu\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), n, unsigned(at), value, (unsigned long long)m_cpu->pipeline_cycles()); expect(valid, "Repeated MVKD fetches one opcode and one retained source literal"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62128;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_repeat_mvdp(unsigned variant)
{
	unsigned const form = variant % 5, reg = (variant / 5) % 8;
	unsigned const count = std::array<unsigned, 5>{0, 1, 2, 7, 255}[(variant / 40) % 5];
	bool const long_count = (variant / 400) % 2;
	u32 const base = (variant / 200) % 2 ? 0x20000 : 0;
	bool const mapped = (variant / 1600) % 2;
	u16 const destination = (variant / 800) % 2 ? 0xfffc : 0x2e00;
	start_mvdp((form == 1 ? 1 : 0) + reg * 7 + (base ? 56 : 0));
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
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
}

void tms320c54x_test_state::start_mvdp_pair(unsigned variant)
{
	start_mvdp(16 + (variant / 6) * 56);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	m_pipeline_peripheral_case = variant;
	u32 const base = variant / 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(base + 0x602, 0x7d92);
	program.write_word(base + 0x603, 0x2e01);
	program.write_word(base + 0x604, 0xf4e1);
	program.write_word(base + 0x2e01, 0xbeef);
	m_cpu->space(AS_DATA).write_word(0x1231, 0x1357);
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mvdp_pair_source",
		[this](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_operand_reads < 2 && at == 0x1230 + m_pipeline_operand_reads && word == (m_pipeline_operand_reads ? 0x1357 : 0x2468), "Adjacent MVDP sources are read once in order"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = program.install_write_tap(0, 0x7fffff, "mvdp_pair_destination",
		[this, base](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_store_writes < 2 && at == base + 0x2e00 + m_pipeline_store_writes && word == (m_pipeline_store_writes ? 0x1357 : 0x2468), "Adjacent MVDP destinations remain instruction-owned"); ++m_pipeline_store_writes; });
	m_phase = 259920;
}

void tms320c54x_test_state::start_write_a(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const form = variant % 7, reg = (variant / 7) % 8, pattern = (variant / 56) % 4;
	bool const mapped = (variant / 448) % 2, extended = form >= 3;
	u32 const base = (variant / 224) % 2 ? 0x20000 : 0;
	u32 const logical = std::array<u32, 4>{0x032e00, 0x03a000, 0x7fff00, 0xffffff}[pattern];
	u32 const physical = mapped && (logical & 0xffff) < 0x8000 ? logical & 0xffff : logical & 0x7fffff;
	u16 const source = std::array<u16, 7>{0x1203, 0x1203, 0x1230, 0x1230, 0x122f, 0x1231, 0x1201}[form];
	u16 const value = std::array<u16, 4>{0, 0xf495, 0xffff, 0x1234}[pattern];
	u8 const mode = form < 2 ? 3 : form == 2 ? 0x90 + reg : form == 3 ? 0xf8 : 0xe0 + (form - 4) * 8 + reg;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned page : {0U, 0x20000U})
	{
		for (unsigned i = 0; i < 16; ++i) program.write_word(page + 0x600 + i, 0xf495);
		program.write_word(page + 0x600, 0x7f00 | mode);
		if (extended) program.write_word(page + 0x601, form == 3 ? source : form == 4 ? 0xffff : form == 5 ? 1 : 2);
		program.write_word(page + 0x601 + extended, 0xf4e1);
	}
	program.write_word(physical, 0xbeef);
	m_cpu->space(AS_DATA).write_word(source, value);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, mapped ? 0x20 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa624); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (form == 1 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1200);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, 16);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == reg ? form == 6 ? 0x120f : 0x1230 : 0x3000 + ar);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1200000000ULL | logical); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123456789aULL);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "write_a_source",
		[this, source, value, extended](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == source && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 5 + extended, "WRITA samples its Access-owned source once in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = program.install_write_tap(0, 0x7fffff, "write_a_destination",
		[this, physical, value, extended](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == physical && word == value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 10 + extended, "WRITA writes one retained word to its mapped PAR after the held clocks"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(physical, physical, "write_a_no_destination_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "WRITA never reads the destination program word"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254800;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_return_load(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const b = variant & 1, zero = variant & 2;
	unsigned const gap = (variant >> 2) & 1;
	u32 const base = variant & 8 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x1292 | (b ? 0x100 : 0));
	program.write_word(base + 0x601 + gap, b ? 0xfc4d : 0xfc45);
	program.write_word(base + 0x602 + gap, 0xf4e1);
	program.write_word(base + 0x700, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x1200, zero ? 0 : 1);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x700);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_AR2, 0x1200},
		{cpu_regs::STATE_A, b ? 0x1234 : unsigned(zero)}, {cpu_regs::STATE_B, b ? unsigned(zero) : 0x2345},
	});
	m_pipeline_operand_reads = m_fast_stack_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "return_load_reads",
		[this, zero](offs_t at, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			if (at == 0x1200) { expect(!m_pipeline_operand_reads, "Return producer reads once"); ++m_pipeline_operand_reads; }
			else { expect(zero && at == 0x1000 && !m_fast_stack_reads, "Only zero producer owns one return stack read"); ++m_fast_stack_reads; }
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "return_load_writes",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Load and return never write data memory"); } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254700;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


tms320c54x_test_state::extended_banz_fixture tms320c54x_test_state::block_banz_case(unsigned v)
{
	return extended_banz_case((v / 4) % 3 + ((v / 12) % 8) * 24 + ((v / 96) % 2) * 384);
}

u16 tms320c54x_test_state::block_banz_resolved(unsigned v, unsigned iteration)
{
	auto const f = block_banz_case(v); u16 current = f.initial, resolved = current;
	for (unsigned i = 0; i <= iteration; ++i)
	{
		resolved = f.form == 2 ? circular_fixture_next(current, s16(f.offset), f.bk) : u16(current + s16(f.offset));
		if (f.form) current = resolved;
	}
	return resolved;
}

tms320c54x_test_state::push_pop_same_fixture tms320c54x_test_state::push_pop_same_case(unsigned v)
{
	u16 const source = 8 + v % 8;
	u16 value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 8) % 4];
	if (source == 0x0a || source == 0x0d) value &= 0xff;
	return {source, std::array<u16, 4>{0x0e, 0x13, 0x19, 0x18}[(v / 32) % 4], value, u16(0x3000 + (v / 128) % 2),
		(v / 256) % 2 ? 0x20000U : 0U, std::array<unsigned, 4>{4, 5, 6, 7}[v / 512]};
}

void tms320c54x_test_state::start_push_pop_same(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = push_pop_same_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x4a00 | f.source); program.write_word(f.base + 0x601, 0x8a00 | f.destination); program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900); m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	data.write_word(f.source, f.value); data.write_word(f.sp - 1, 0xbeef);
	m_pipeline_operand_reads = m_fast_stack_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "push_pop_same_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.source) { expect(!m_pipeline_operand_reads && word == f.value && m_cpu->pipeline_cycles() == 5, "PSHM captures the mapped source once before later mutation"); ++m_pipeline_operand_reads; return; }
		expect(at == f.sp - 1 && word == f.value && !m_fast_stack_reads && m_pipeline_store_writes == 1 && m_cpu->pipeline_cycles() == 6, "Adjacent POPM receives the pending PSHM word, not old RAM"); ++m_fast_stack_reads;
		auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xbeef); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "push_pop_same_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.sp - 1) { expect(!m_pipeline_store_writes && word == f.value && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() == 6, "PSHM publishes its retained word once in Execute"); ++m_pipeline_store_writes; return; }
		expect(at == f.destination && word == f.value && !m_pipeline_register_writes && m_fast_stack_reads == 1 && m_cpu->pipeline_cycles() == (f.destination == 0x0e ? 7 : 6), "POPM publishes the captured word at the destination's existing owned clock"); ++m_pipeline_register_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255500;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

void tms320c54x_test_state::start_mvmd_word_register(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const bk = profile_is("mvmd-bk"), producer = bk && variant >= 216;
	unsigned const v = bk ? variant % 216 : variant, sources = bk ? 1 : 2, prefix = producer ? 2 : 0;
	unsigned const source = bk ? 0x19 : 0x0e + v % sources, destination = (v / sources) % 9 < 8 ? 0x10 + (v / sources) % 9 : 0x19;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / (sources * 9)) % 4];
	u32 const base = (v / (sources * 36)) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	if (producer) program.write_word(base + 0x600, 0x8819);
	program.write_word(base + 0x600 + prefix, 0x7300 | source); program.write_word(base + 0x601 + prefix, destination); program.write_word(base + 0x602 + prefix, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_A, producer ? value : 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	for (unsigned r = 0; r < 8; ++r) data.write_word(0x10 + r, 0x1400 + r);
	data.write_word(0x19, bk && !producer ? value : 13); data.write_word(0x0e, source == 0x0e ? value : 0x1357); data.write_word(0x0f, source == 0x0f ? value : 0x2468);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvmd_t_read", [this, source, value, prefix](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == source && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 5 + prefix, "MVMD samples its register once at retained Access"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvmd_t_write", [this, destination, value, prefix, producer](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; bool const first = producer && !m_pipeline_store_writes; expect(at == (first ? 0x19 : destination) && word == value && m_pipeline_store_writes < 1 + unsigned(producer) && m_pipeline_operand_reads == unsigned(!first) && m_cpu->pipeline_cycles() == (first ? 6 : 5 + prefix), "STLM and MVMD publish once at their owned clocks"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254300;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

tms320c54x_test_state::accumulator_store_fixture tms320c54x_test_state::preincrement_store_case(unsigned variant)
{
	unsigned const reg = variant % 8, selector = (variant / 8) % 4, flags = (variant / 32) % 4, pattern = (variant / 128) % 4;
	bool const compatibility = (variant / 512) % 2;
	auto f = long_accumulator_store_fixture(reg + selector * 100 + flags * 400 + pattern * 1600 + (compatibility ? 6400 : 0));
	f.opcode = 0x8000 | (selector << 8) | 0x98 | reg;
	f.address = std::array<u16, 4>{0x1400, 0xfffe, 0xffff, 0}[pattern]; f.next_ar = f.address;
	return f;
}

tms320c54x_test_state::accumulator_store_fixture tms320c54x_test_state::preincrement_immediate_case(unsigned variant)
{
	auto f = preincrement_store_case(variant % 8 + ((variant / 8) % 4) * 32 +
		((variant / 32) % 4) * 128 + ((variant / 128) % 2) * 512);
	f.opcode = 0x7698 | f.reg;
	f.value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 32) % 4];
	return f;
}

void tms320c54x_test_state::start_preincrement_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const immediate = profile_is("preinc-st");
	auto const f = immediate ? preincrement_immediate_case(variant) : preincrement_store_case(variant); u32 const base = (immediate ? bool(variant & 256) : bool(variant & 1024)) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, f.opcode);
	if (immediate) program.write_word(base + 0x601, f.value);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.selector & 1 ? 0xabcdef1234ULL : f.source ^ 0xffff); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.selector & 1 ? f.source ^ 0xffff : 0xabcdef1234ULL);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, u16(f.address - 1));
	data.write_word(f.address, 0xaaaa);
	m_pipeline_store_writes = 0;
	if (immediate)
	{
		m_pipeline_extensions_reads = 0;
		m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "preincrement_immediate_literal", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_extensions_reads && word == f.value, "Preincrement immediate store retains its literal once"); ++m_pipeline_extensions_reads; });
	}
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "preincrement_store_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Preincrement accumulator store does not read its destination"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "preincrement_store_write",
		[this, f, immediate](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == f.address && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (immediate ? 7 : 6), "Preincrement store writes one retained destination and its operation-owned value"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253930;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u16 tms320c54x_test_state::ldu_status_value(unsigned variant)
{
	return std::array<u16, 8>{0, 0x200, 0x8000, 0x8200, 0x6900, 0x6b00, 0xfdff, 0xffff}[variant % 8] & ~u16(0x400);
}

tms320c54x_test_state::stlm_status_fixture tms320c54x_test_state::stlm_status_case(unsigned variant, bool immediate)
{
	unsigned const pattern = variant % 64;
	stlm_status_fixture f{(variant / 64) % 6, (variant / 768) % 4, 4 + (variant / 6144), bool((variant / 384) % 2),
		(variant / 3072) % 2 ? 0x20000U : 0, 0, 0, 0, 0};
	f.requested = f.family ? 0x800 | (pattern & 31) | (pattern & 32 ? 0x100 : 0) : u16(0xffff ^ (pattern * 0x421));
	f.before = ((f.requested ^ 0x611f) & 0x711f) | ((variant / 384) % 2 ? 0x800 : 0);
	f.value = 0x8012340000ULL | f.requested;
	if (!immediate && !f.family && (pattern & 32)) f.requested = f.before & 0x100 ? 0 : 0xffff;
	f.status = (f.requested & ~u16(0xc00)) | (f.before & f.requested & 0x800);
	return f;
}

void tms320c54x_test_state::start_stlm_status(unsigned variant)
{
	bool const immediate = profile_is("stm-st1");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = stlm_status_case(variant, immediate); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.source ? 0x8907 : 0x8807);
	if (f.family == 1 || f.family == 3)
	{
		program.write_word(f.base + 0x601 + f.gap, 0x1000 | (f.source ? 0x100 : 0) | (f.family == 3 ? 0xe0 : 0x80));
		if (f.family == 3) program.write_word(f.base + 0x602 + f.gap, 0);
	}
	if (f.family == 2 || f.family >= 4) program.write_word(f.base + 0x601 + f.gap, (f.family == 2 ? 0xf482 : f.family == 4 ? 0xf480 : 0xf481) | (f.source ? 0x100 : 0x200));
	if (immediate)
	{
		for (unsigned i = 31; i > 0; --i) program.write_word(f.base + 0x601 + i, program.read_word(f.base + 0x600 + i));
		program.write_word(f.base + 0x600, 0x7707); program.write_word(f.base + 0x601, f.requested);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x260e);
	if (!f.family) m_cpu->set_state_int(tms320c54x_device::STATE_PMST, (variant % 64) & 32 ? 1 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.before); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2000); m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1400);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.source ? 0x8012345678ULL : 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.source ? 0xdead : 0x8012345678ULL);
	data.write_word(0x1400, 0x8001);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "stlm_status_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect((f.family == 1 || f.family == 3) && at == 0x1400 && word == 0x8001 && !m_pipeline_operand_reads,
			"STLM ST1 never reads its destination and the following load reads RAM once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "stlm_status_write", [this, f, immediate](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(at == 7 && word == f.requested && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 6,
			"STLM ST1 publishes the Execute-live accumulator once independent of DP/CPL"); ++m_pipeline_store_writes; });
	m_pipeline_extensions_reads = 0;
	if (immediate) m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "stm_status_literal", [this, f](offs_t, u16 &word, u16) {
		if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads++ && word == f.requested, "STM ST1 retains one original literal"); }
	});
	m_phase = 62110; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

void tms320c54x_test_state::start_ldu_status(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const form = (variant / 8) % 3, read = form == 1 ? 6 : 5;
	bool const b = (variant / 24) % 2;
	u16 const value = ldu_status_value(variant);
	u32 const base = (variant / 48) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x1200 | (b ? 0x100 : 0) | (form == 1 ? 0xf8 : form == 2 ? 0x82 : 7));
	if (form == 1) program.write_word(base + 0x601, 7);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, value);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0); // CPL direct offset 7 still selects ST1.
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcdef1234ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123456789aULL);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 7);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "ldu_status_read", [this, value, read](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == 7 && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read, "LDU captures live mapped ST1 once at Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "ldu_status_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Mapped ST1 LDU does not write memory or registers"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "ldu_status_literal", [this, form](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || form != 1) return; expect(word == 7 && !m_pipeline_extensions_reads, "Absolute ST1 LDU captures one address literal"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254010;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mvmd_status_block(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const pop = profile_is("pop-brc");
	bool const stm = pop || profile_is("stm-brc");
	unsigned const count = stm ? std::array<unsigned, 5>{0, 1, 2, 7, 127}[variant % 5] : std::array<unsigned, 4>{0, 1, 2, 7}[variant % 4]; u32 const base = (stm ? (variant / 5) % 2 : bool(variant & 4)) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	static constexpr u16 words[] = {0x730b, 7, 0xf072, 0x608, 0x1386, 0xf678, 0xf3e8, 0xf1a0, 0x8196, 0xf4e1};
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, i < std::size(words) ? words[i] : 0xf495);
	if (stm) { program.write_word(base + 0x600, 0x771a); program.write_word(base + 0x601, count); }
	if (pop) { program.write_word(base + 0x600, 0x8a1a); program.write_word(base + 0x601, 0xf495); data.write_word(0x1000, count); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, stm ? 0x800 : 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x800); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, stm ? count ^ 1 : count);
	for (unsigned i = 0; i <= count; ++i) data.write_word(0x5000 + i, 0x1234 + i * 0x321);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	if (stm)
	{
		m_pipeline_extensions_reads = 0;
		m_operand_data_tap = data.install_write_tap(0x1a, 0x1a, "brc_publication", [this, count, pop](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_register_writes && word == count && m_cpu->pipeline_cycles() == (pop ? 6 : 5), "BRC publishes its captured count once before RPTB"); ++m_pipeline_register_writes; });
		m_pipeline_fetch_tap = data.install_read_tap(0x1a, 0x1a, "stm_brc_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "STM BRC does not read its destination"); });
		m_vector_prefetch_tap = (pop ? data : program).install_read_tap(pop ? 0x1000 : base + 0x601, pop ? 0x1000 : base + 0x601, "brc_source", [this, count](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_extensions_reads && word == count, "BRC producer captures one count through replay"); ++m_pipeline_extensions_reads; });
	}
	m_pipeline_operand_tap = data.install_read_tap(0x5000, 0x50ff, "mvmd_status_block_read",
		[this, count](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_operand_reads <= count && at == 0x5000 + m_pipeline_operand_reads && word == u16(0x1234 + m_pipeline_operand_reads * 0x321), "Following RPTB reads each original word once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x5000, 0x50ff, "mvmd_status_block_write",
		[this, count](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; u16 const source = 0x1234 + m_pipeline_store_writes * 0x321; expect(m_pipeline_store_writes <= count && at == 0x5000 + m_pipeline_store_writes && word == u16((source << 8) | (source >> 8)), "Following RPTB writes each byte-swapped word once"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253870;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mvmd_status_alu(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const fields = variant % 256;
	bool const sxm = fields & 1, shift = fields & 64;
	u16 const status = 0x800 | (sxm ? 0x100 : 0) | ((fields / 2) & 31);
	u32 const base = fields & 128 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x730b); program.write_word(base + 0x601, 7);
	program.write_word(base + 0x602, shift ? 0xf682 : 0x1080);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa200); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, status ^ 0x11f);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x55); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x8012340000ULL | status); m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1400);
	data.write_word(0x1400, 0x8001);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvmd_status_alu_reads",
		[this, status, shift](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0xb) { expect(!m_pipeline_operand_reads && word == status && m_cpu->pipeline_cycles() == 5, "Full status move samples BL once"); ++m_pipeline_operand_reads; } else { expect(!shift && at == 0x1400 && word == 0x8001 && !m_pipeline_register_writes && m_cpu->pipeline_cycles() == 7, "Adjacent SXM load samples its RAM operand once"); ++m_pipeline_register_writes; } });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvmd_status_alu_write",
		[this, status](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == 7 && word == status && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 6, "Full status publication precedes adjacent arithmetic"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253850;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

















bool tms320c54x_test_state::check_pipeline_immediate_store_results()
{
	if (m_phase == 251240)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->pipeline_cycles() == 6,
			"Unsupported peripheral arbitration stops at the first contending stages");
		expect(m_pipeline_operand_reads == 0 && m_pipeline_store_writes == 0,
			"Rejected arbitration performs no CPU bus access");
		// PRD debugger export reads the bus; exclude that observer access.
		m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
		expect(m_cpu->state_int(tms320c54x_device::STATE_PRD) == 0xaaaa &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x5678,
			"Rejected arbitration performs no bus access or atomic fallback");
		m_cpu->set_pipeline_frontend(false);
		osd_printf_info("TMS320C54x staged peripheral arbitration exclusion: PASS stopped_cycle=6 bus_accesses=0 atomic_fallback=0\n");
		start_pipeline_immediate_store();
		return true;
	}
	return false;
}






bool tms320c54x_test_state::check_repeat_pop_exclusion_results()
{
	if (m_phase == 252270)
	{
		bool const popd = profile_is("pmst-popd");
		bool const repeat = profile_is("pop-repeat");
		bool const iptr = profile_is("pmst-iptr");
		bool const boundary = profile_is("pmst-prefetch");
		bool const pop = popd || repeat || iptr || boundary || profile_is("pmst-pop");
		unsigned const variant = m_pipeline_peripheral_case;
		u16 const value = (repeat ? std::array<u16, 4>{0, 1, 0x8000, 0xffff} : iptr ? std::array<u16, 4>{0, 0x20, 0x2020, 0x3020} : std::array<u16, 4>{0, 0x20, 0x2020, 0xffaa})[variant % 4];
		u32 const base = (BIT(variant, 2) ? 3 : 2) << 16;
		unsigned const tick = ++m_idle_fixture_tick;
		// This CPU conformance fixture supplies a pending latch, not a queued pin edge.
		unsigned const checkpoint = (iptr ? std::array<unsigned, 4>{3, 5, 6, 11} : boundary ? std::array<unsigned, 4>{1, 2, 5, 6} : pop ? std::array<unsigned, 4>{4, 5, 6, 11} : std::array<unsigned, 4>{4, 6, 8, 11})[(variant % 64) / 16];
		if (boundary && tick == 1) m_cpu->set_state_int(tms320c54x_device::STATE_PMST, value);
		if (iptr && tick == (BIT(variant, 3) ? 7 : 4)) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, u16(1U << (variant / 64)));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged PMST does not enable unsupported immediate register forms");
		if (!pop && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x9001, 0xffff); }
		if (pop && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1000, value ^ 0xffff); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_pipeline_store_case = m_pipeline_store_writes; m_pipeline_branch_case = m_pipeline_operand_reads;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save PMST literal, publication and subsequent mapping");
		}
		if (tick == 20)
		{
			if (iptr && m_cpu->state_int(tms320c54x_device::STATE_B) != 0x55)
				osd_printf_info("PMST IPTR observation: case=%u replay=%u pmst=%04x b=%010llx sp=%04x pc=%06x imr=%04x ifr=%04x st1=%04x\n", variant, unsigned(m_pipeline_replay),
					u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
					u16(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(STATE_GENPC)),
					u16(m_cpu->state_int(tms320c54x_device::STATE_IMR)), u16(m_cpu->state_int(tms320c54x_device::STATE_IFR)), u16(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
			expect(m_pipeline_store_writes == 1 && m_pipeline_operand_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == (repeat ? 0 : value) &&
				(iptr ? m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 : m_cpu->state_int(tms320c54x_device::STATE_A) == (!repeat && BIT(value, 5) && !BIT(variant, 3) ? 0x11 : 0x44)) &&
				m_cpu->state_int(tms320c54x_device::STATE_XPC) == base >> 16 && m_cpu->state_int(tms320c54x_device::STATE_SP) == (pop && !iptr ? 0x1001 : 0x1000) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
				"PMST changes subsequent lower program mapping only, retains IPTR bits and does not alter source XPC or status");
			if (repeat)
			{
				unsigned const selected = variant / 64;
				expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == (selected == 0 ? value : 0x1234) &&
					m_cpu->state_int(tms320c54x_device::STATE_RSA) == (selected == 1 ? value : 0x5678) &&
					m_cpu->state_int(tms320c54x_device::STATE_REA) == (selected == 2 ? value : 0x9abc), "Repeat-control POPM preserves the other two registers");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_store_writes = m_pipeline_store_case; m_pipeline_operand_reads = m_pipeline_branch_case;
				return restore_pipeline_at(252280, "Restore staged PMST without duplicate literal fetch or write");
			}
			m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (repeat ? 191 : iptr ? 1023 : 63)) { start_pipeline_pmst(variant + 1); return true; }
			if (pop)
			{
				if (repeat)
				{
					start_repeat_pop_exclusion(); return true;
				}
				if (iptr)
				{
					osd_printf_info("TMS320C54x staged PMST IPTR: PASS cases=1024 restore=1024 values=4 source_pages=2 irq_bits=16 irq_timings=2 snapshots=4 vector_prefetch_owned=1 pending_latch_fixture=1 isr_return_claim=0 silicon_phase_claim=0\n");
					throw emu_fatalerror(3, "TMS320C54x isolated PMST IPTR complete");
				}
				if (boundary)
				{
					osd_printf_info("TMS320C54x staged PMST prefetch ownership: PASS cases=64 restore=64 values=4 source_pages=2 target_regions=2 snapshots=4 debugger_state_fixture=1 firmware_ordering_claim=0\n");
					throw emu_fatalerror(3, "TMS320C54x isolated PMST prefetch complete");
				}
				if (popd)
				{
					expect(m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1d, "Indirect POPD preserves its nonmodifying destination register");
					osd_printf_info("TMS320C54x staged POPD PMST: PASS cases=64 restore=64 values=4 source_pages=2 target_regions=2 stack_reads=1 writes=1 source_retained=1 active_repeat_claim=0 silicon_phase_claim=0\n");
					throw emu_fatalerror(3, "TMS320C54x isolated POPD PMST complete");
				}
				osd_printf_info("TMS320C54x staged POPM PMST: PASS cases=64 restore=64 values=4 source_pages=2 target_regions=2 stack_reads=1 writes=1 source_retained=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated POPM PMST complete");
			}
			osd_printf_info("TMS320C54x staged STM PMST: PASS cases=64 values=4 source_pages=2 target_regions=2 restore=64 duplicate_literal_fetches=0 duplicate_register_writes=0\n");
			if (profile_is("pmst-map")) throw emu_fatalerror(3, "TMS320C54x isolated PMST mapping complete");
			start_pipeline_mvpd(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252280)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252270;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}


bool tms320c54x_test_state::check_mvkd_sp_results()
{
	if (m_phase == 293000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = mvkd_sp_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "MVKD mapped SP is an admitted data source, not a compiler address");
		if (tick == f.read - 2)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
			if (f.read == 7) m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0xbeef);
		}
		if (tick == f.read - 1)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == f.next_ar, "MVKD changes its pointer only once during Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.value);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900 | (f.compiler ? 0 : 0x4000));
		}
		if (tick == f.read)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0xbeef);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2468);
		}
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped SP copy before Read, after Read or after publication");
		}
		if (tick == 20)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.destination) == f.value && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == (f.read == 7 ? 2 : 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0xbeef && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (f.compiler ? 0 : 0x4000)), "Mapped SP copy retains address, word and status across post-Read source replacement"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(293010, "Restore mapped SP copy without duplicate reads, updates or writes");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_mvkd_sp(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MVKD SP: PASS cases=384 restore=384 forms=DP,CPL,indirect,postincrement,postdecrement,preincrement,absolute,offset,MVDM values=4 cpl_states=2 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_retained=1 address_retained=1 silicon_phase_claim=0\n");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 293010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 293000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
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

bool tms320c54x_test_state::check_extended_ar_store_results()
{
	if (m_phase == 254560)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const consumer = profile_is("st-ar-consumer") || profile_is("st-shift-ar-use");
		bool const shifted = profile_is("st-shift-ar") || profile_is("st-shift-ar-use");
		auto const f = extended_ar_store_case(variant, consumer, shifted);
		unsigned const local = variant % (consumer ? 6144 : 24576);
		unsigned const checkpoint = consumer ? std::array<unsigned, 3>{4, 7 + unsigned(shifted), 7 + unsigned(shifted) + f.gap}[local / 2048] : std::array<unsigned, 3>{4, 6 + unsigned(shifted), 7 + unsigned(shifted)}[local / 8192];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Extended accumulator store owns its mapped AR destination");
		if (tick == 4)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
		}
		if (shifted && tick == 5)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0xdead);
		}
		if (tick == 6 + unsigned(shifted) && !consumer)
		{
			for (unsigned ar = 0; ar < 8; ++ar)
			{
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (f.form && ar == f.selected ? f.next_ar : 0x4000 + ar),
					"Extended store resolves its address and implicit pointer modification before explicit AR publication");
				m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5000 + ar);
			}
			m_cpu->set_state_int((f.kind & 1) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, f.value);
		}
		if (consumer && tick == 7 + unsigned(shifted) + f.gap && f.result >= 0x60)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.result, 0xcccc);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save extended AR store address, Execute write or legal-gap consumer");
		}
		if (tick == 24)
		{
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.reg ? f.result : (consumer ? 0x4000 : 0x5000) + ar),
					"Only the retained destination receives the explicit accumulator store");
			expect(m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(consumer) &&
				m_cpu->state_int((f.kind & 1) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == f.value &&
				m_cpu->state_int((f.kind & 1) ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (consumer ? (f.result ? 0xbbbb : 0) : 0xdead) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x4800 | ((f.flags & 2) ? 0x100 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x6000 && m_cpu->state_int(tms320c54x_device::STATE_BK) == 8,
				"Extended AR store preserves arithmetic state, source and legal-gap consumer across replay");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_operand_reads = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(254570, "Restore extended AR store without recapturing its address or duplicating its write");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < (consumer ? 6144U : 24576U) * (shifted ? 5 : 1)) { start_extended_ar_store(variant + 1); return true; }
			if (shifted)
			{
				if (consumer) osd_printf_info("TMS320C54x shifted AR store consumers: PASS cases=30720 restore=30720 destinations=8 selectors=4 patterns=4 sst_sxm=4 shifts=-16,-1,0,1,15 legal_gaps=2,3 pages=2 snapshots=3 publish_clock=8 consumer_reads=1 writes=1 retained_extensions=1 silicon_phase_claim=0\n");
				else osd_printf_info("TMS320C54x shifted AR stores: PASS cases=122880 restore=122880 forms=absolute,offset,preincrement,circular destinations=8 selectors=4 patterns=4 sst_sxm=4 shifts=-16,-1,0,1,15 aliases=self,other pages=2 snapshots=3 publish_clock=8 destination_reads=0 writes=1 retained_extensions=1 execute_live_source=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated shifted AR stores complete");
			}
			if (consumer)
			{
				osd_printf_info("TMS320C54x extended AR store consumer: PASS cases=6144 restore=6144 destinations=8 stores=STL_A,STL_B,STH_A,STH_B patterns=4 sst_sxm=4 legal_gaps=2,3 pages=2 snapshots=3 publish_clock=7 consumer_reads=1 ar_writes=1 literal_fetches=1 forbidden_gap_claim=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated extended AR consumer complete");
			}
			osd_printf_info("TMS320C54x extended AR stores: PASS cases=24576 restore=24576 forms=absolute,offset,preincrement,circular destinations=8 stores=STL_A,STL_B,STH_A,STH_B patterns=4 sst_sxm=4 aliases=self,other pages=2 snapshots=3 publish_clock=7 destination_reads=0 writes=1 literal_fetches=1 execute_live_source=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated extended AR stores complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254570)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254560;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252550)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = extended_dst_fixture(variant);
		bool const short_form = variant >= 3072;
		unsigned const checkpoint = short_form ? std::array<unsigned, 3>{3, 5, 6}[(variant - 3072) / 3072] : std::array<unsigned, 3>{4, 6, 7}[variant / 1024];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Extended DST owns its retained address and both writes");
		if (tick == 4 && !short_form)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((f.reg & 1 ? 0x20000 : 0) + 0x601, 0xdead);
		}
		if (tick == (short_form ? 4 : 5))
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == f.next_ar, "Long DST applies preincrement/circular modification once in Access");
			bool const compatibility = short_form ? ((variant - 3072) / 768) % 2 : (variant / 256) % 2;
			u16 const st0 = compatibility && f.reg && f.kind != 3 ? (0x6608 & ~0xe000) | (f.reg << 13) : 0x6608;
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0, "Long DST uses standard ARF or compatibility ARP ownership without DP/CPL addressing");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 32);
		}
		if (tick == (short_form ? 5 : 6)) m_cpu->set_state_int(f.source ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save extended DST after Decode, source capture or first write");
		}
		if (tick == 24)
		{
			{
				auto const disable = machine().disable_side_effects();
				u16 const opcode = 0x4e00 | (f.source << 8) | (short_form ? 0x80 + (f.kind - 4) * 8 + f.reg : f.kind == 3 ? 0xf8 : 0xe0 + f.kind * 8 + f.reg);
				expect_opcode(opcode, m_pipeline_store_writes == 2 && m_pipeline_extensions_reads == (short_form ? 0 : 1) &&
					m_cpu->space(AS_DATA).read_word(f.address) == u16(f.stored >> 16) && m_cpu->space(AS_DATA).read_word(f.address ^ 1) == u16(f.stored) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2000,
					"DST retains both source words and destination despite later accumulator/AR/literal changes");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(252560, "Restore DST without duplicate address updates or writes");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 12287) { start_pipeline_extended_dst(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged extended DST: PASS cases=3072 forms=offset,preincrement,circular,absolute registers=8 accumulators=2 offsets=4 compatibility=0,1 sst=0,1 restore=3072 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0\n");
			osd_printf_info("TMS320C54x staged short-indirect DST: PASS cases=9216 modes=12 registers=8 accumulators=2 parity=even,odd compatibility=0,1 sst=0,1 restore=9216 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0\n");
			start_pipeline_data_copy(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252560)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252550;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_data_copy_results()
{
	if (m_phase == 252590)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = data_copy_fixture(variant);
		u32 const base = (variant / 112) % 2 ? 0x20000 : 0;
		unsigned const checkpoint = f.read_cycle - 2 + std::array<unsigned, 3>{0, 2, 3}[variant / 224];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "MVKD has distinct retained source and destination ownership");
		if (tick == f.read_cycle - 2)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_cpu->state_int(tms320c54x_device::STATE_ST1) ^ 0x4000);
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead);
			if (f.form == 5) m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xbeef);
		}
		if (tick == f.read_cycle - 1)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & ~0x1ff) | 9);
		}
		if (tick == f.read_cycle)
		{
			auto const disable = machine().disable_side_effects();
			if (f.source >= 0x60) m_cpu->space(AS_DATA).write_word(f.source, 0xdead);
			if (f.form != 6) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_extensions_reads, m_pipeline_operand_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save MVKD after Decode, source read or write");
		}
		if (tick == 24)
		{
			{
				auto const disable = machine().disable_side_effects();
				bool const equal = (variant / 112) % 2 || (variant / 7) % 4 == 0 || (variant / 7) % 4 == 2;
				expect(m_pipeline_store_writes == 1 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == (f.form == 5 ? 2 : 1) &&
					m_cpu->space(AS_DATA).read_word(f.destination) == f.value && (f.form != 6 || BIT(m_cpu->state_int(tms320c54x_device::STATE_ST0), 12) == equal),
					"MVKD preserves its sample and allows a Category-I AR consumer after one intervening instruction");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_pipeline_operand_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(252600, "Restore MVKD without duplicate sampling or publication");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 671) { start_pipeline_data_copy(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MVKD: PASS cases=672 forms=DP,CPL,postincrement,postdecrement,preincrement,absolute,AR sources=AR1,RAM,alias,AR2 values=4 pages=2 category_i_gaps=0,1 restore=672 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0\n");
			start_pipeline_long_mvkd(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252600)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252590;
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
	if (m_phase == 600010)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const arithmetic = profile_is("addsub-ar-age");
		bool const shifted = profile_is("shift16-load-age");
		bool const mapped = arithmetic || shifted || profile_is("mapped-load-age");
		unsigned const fields = shifted ? variant / 4 : mapped ? variant / 8 : variant, form = shifted ? variant%4 : mapped ? (variant / 2) % 4 : 0;
		unsigned const ar = fields % 8, kind = (fields / 64) % 3;
		unsigned const checkpoint = 4 + unsigned(mapped && form >= 2) + fields / 768;
		bool const destination_b = (fields / 8) % 2, sxm = (fields / 192) % 2;
		u16 const value = std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[(fields / 16) % 4];
		u64 const extended = mapped && (arithmetic || shifted || !(variant % 2)) && sxm ? u64(s64(s16(value))) & 0xffffffffffULL : value;
		u64 const loaded = arithmetic && variant%2 ? (0-extended)&0xffffffffffULL : shifted ? (extended<<16)&0xffffffffffULL : extended;
		u16 const status = arithmetic ? 0x1400 | (variant%2 && !value ? 0x800 : 0) : 0x1c00;
		u16 const updated = kind == 2 ? 0x3333 : u16(value + (kind ? -1 : 1));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "LDM/younger DAGEN chain stays supported");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save LDM around its retained Read and Execute");
		}
		if (tick == 12)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == unsigned(kind == 2) &&
				m_pipeline_extensions_reads == unsigned(kind == 2) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == updated &&
				m_cpu->state_int(destination_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == loaded &&
				m_cpu->state_int(destination_b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x123456789aULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == status &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm ? 0x100 : 0)),
				"Mapped load extends the old AR once, preserves flags and retains the younger update through replay");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_check_timer->adjust(attotime::never);
				m_cpu->set_pipeline_frontend(true);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore LDM with one retained old-register sample");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 600011; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (shifted ? 9215 : mapped ? 18431 : 2303)) { start_pipeline_mapped_load_age(variant + 1); return true; }
			if(arithmetic) { osd_printf_info("TMS320C54x mapped ADD SUB age: PASS cases=18432 restore=18432 operations=ADD,SUB forms=DP,indirect,absolute,offset registers=8 accumulators=2 values=4 younger=MAR_inc,MAR_dec,MVMM sxm=2 pages=2 snapshots=3 zero_initial_destination=1 retained_old_AR=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated mapped ADD SUB age complete"); }
			if(shifted) { osd_printf_info("TMS320C54x shifted mapped load age: PASS cases=9216 restore=9216 forms=DP,indirect,absolute,offset registers=8 accumulators=2 values=4 younger=MAR_inc,MAR_dec,MVMM sxm=2 pages=2 snapshots=3 shift=16 retained_old_AR=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated shifted mapped load age complete"); }
			if (mapped)
			{
				osd_printf_info("TMS320C54x staged mapped load age: PASS cases=18432 restore=18432 loads=LD,LDU forms=DP,indirect,absolute,offset registers=8 accumulators=2 values=4 younger=MAR_inc,MAR_dec,MVMM sxm=2 pages=2 snapshots=3 physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated mapped load age complete");
			}
			osd_printf_info("TMS320C54x staged LDM AR age: PASS cases=2304 restore=2304 registers=8 accumulators=2 values=4 younger=MAR_inc,MAR_dec,MVMM sxm=2 pages=2 snapshots=3 read=5 execute=6 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated LDM AR age complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600011)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600010;
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
	if (m_phase == 252730)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, gap = (variant / 2) % 4;
		bool const immediate = variant % 2;
		unsigned const read_cycle = (immediate ? 5 : 4) + gap, checkpoint = read_cycle + std::array<unsigned, 3>{0, 1, 3}[variant / 16];
		u16 const value = immediate || gap >= 2 ? 0x1200 : 0x1000;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "MVMM producer chain executes through staged register ownership");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save MVMM source-forwarding chain after sample, publication or consumer read");
		}
		if (tick == 20)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 2 && m_pipeline_extensions_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_AR5) == value &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (value == 0x1000 ? 0xaaaa : 0xbbbb), "MVMM preserves source producer latency and a zero-gap destination consumer across replay");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0];
				m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(252740, "Restore MVMM producer forwarding without extra reads or writes");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 47) { start_pipeline_mvmm_producer(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MVMM producers: PASS cases=48 producers=STM,STLM gap_nops=0,1,2,3 pages=2 restore=48 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_repeat_nop(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252740)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252730;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_xmem_bit_results()
{
	if (m_phase == 252770)
	{
		unsigned const variant = m_pipeline_peripheral_case, x = variant % 16;
		bool const value = (variant / 256) % 2, initial_tc = (variant / 512) % 2;
		unsigned const gap = (variant / 1024) % 3, checkpoint = 3 + variant / 3072;
		unsigned const tick = ++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Packed Xmem BIT has staged ownership");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x608 | ((tick < 6 ? initial_tc : value) ? 0x1000 : 0)),
			"BIT publishes only TC in Execute, not Decode/Access/Read");
		for (unsigned ar = 2; ar <= 5; ++ar)
		{
			int const delta = ar == 2 + (x & 3) && tick >= 4 ? std::array<int, 4>{0, -1, 1, 3}[x >> 2] : 0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == unsigned(0x1000 + delta),
				"BIT modifies only its selected X pointer once in Access, including BK=0 linear percent mode");
		}
		if (tick == 5)
			m_cpu->space(AS_DATA).write_word(0x1000, 0xdead); // Operand must already be retained.
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save BIT at Decode, Access or retained Read");
		}
		if (tick == 16)
		{
			bool const execute = !(gap < 2 ? initial_tc : value);
			expect(m_pipeline_operand_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == (execute ? 0x55 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234,
				"XC observes old TC at zero/one gap and BIT's TC with the original two-NOP gap");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0];
				return restore_pipeline_at(252780, "Restore BIT without resampling or duplicate X update");
			}
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 9215) { start_pipeline_xmem_bit(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged BIT: PASS cases=9216 xmem_modes=16 bitcodes=16 values=2 initial_tc=2 xc_gaps=0,1,2 restore=9216 duplicate_reads=0\n");
			start_pipeline_port_read(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252780)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252770;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_staged_load_results()
{
	if (m_phase == 253050)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const unsigned_load = variant >= 12672;
		bool const variable_shift = variant >= 127872;
		unsigned const model_variant = variable_shift ? variant - 127872 : variant >= 70272 ? variant - 70272 : unsigned_load ? variant - 12672 : variant;
		auto const f = staged_load_case(variant);
		unsigned const checkpoint_index = variable_shift ? model_variant / 36864 : unsigned_load ? (model_variant < 55296 ? model_variant / 18432 : (model_variant - 55296) / 768) :
			(variant < 12288 ? variant / 4096 : (variant - 12288) / 128);
		unsigned const checkpoint = std::array<unsigned, 3>{variable_shift && f.form >= 14 ? 4U : 3U, f.read_cycle - 1, f.read_cycle}[checkpoint_index];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long-offset LD has staged operand ownership");
		bool const retired = tick >= f.read_cycle + 1;
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (BIT(f.opcode, 8) ? 0x123456789aULL : retired ? f.result : f.source) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (BIT(f.opcode, 8) ? retired ? f.result : f.source : 0x123456789aULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= f.read_cycle - 1 ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Memory load publishes only in Execute and changes only the Access-owned ARP before retirement");
		if (tick == 4 && (f.form >= 14 || variable_shift))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((f.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead);
			if (variable_shift && f.form >= 14) m_cpu->space(AS_PROGRAM).write_word((f.reg == 6 ? 0x20000 : 0) + 0x602, 0xdead);
		}
		if (tick == f.read_cycle - 1)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == f.next_ar,
				"Long-offset LD applies its selected linear/circular pointer modification once in Access");
			if (f.address != 0x17) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2222);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2222);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 32);
		}
		if (tick == f.read_cycle)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save long-offset LD during Decode, Access or retained Read");
		}
		if (tick == 18)
		{
			u16 const opcode = variable_shift ? 0x6f00 | u8(f.opcode) : f.opcode;
			expect_opcode(opcode, m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(f.form >= 14) + unsigned(variable_shift) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (BIT(f.opcode, 8) ? 0x123456789aULL : f.result) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (BIT(f.opcode, 8) ? f.result : 0x123456789aULL),
				"Long-offset LD uses its retained sample and SXM without arithmetic flag changes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(253060, "Restore long-offset LD without resampling or duplicate pointer modification");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant == 12671) osd_printf_info("TMS320C54x staged long-offset LD: PASS ram_cases=12288 mapped_ar_cases=384 forms=offset,preincrement,circular,absolute registers=8 accumulators=2 offsets=8 sxm_ovm=4 compatibility=2 restore=12672 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant == 12671 && profile_is("load-offset")) throw emu_fatalerror(0, "TMS320C54x isolated offset memory load complete");
			if (variant == 70271) osd_printf_info("TMS320C54x staged LDU: PASS ram_cases=52224 mapped_ar_cases=2304 address_forms=17 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=54528 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant == 127871) osd_printf_info("TMS320C54x staged memory LD shift16: PASS ram_cases=52224 mapped_ar_cases=2304 address_forms=17 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=54528 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant == 127871 && profile_is("ldu-shift16")) throw emu_fatalerror(0, "TMS320C54x isolated LDU and shift16 loads complete");
			if (variant < 238463) { start_pipeline_offset_load(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged variable memory LD: PASS cases=104448 address_forms=17 registers=2 accumulators=2 values=4 shifts=32 sxm_ovm=4 restore=104448 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (profile_is("load-variable")) throw emu_fatalerror(0, "TMS320C54x isolated variable memory loads complete");
			start_pipeline_indirect_dld(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253060)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253050;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253070)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, v = variant % 8192;
		auto const f = indirect_dld_case(variant);
		unsigned const form = v % 16, flags = (v / 1024) % 8, read_cycle = form < 12 ? 5 : 6;
		unsigned const checkpoint = std::array<unsigned, 3>{3, read_cycle - 1, read_cycle}[variant / 8192];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Indirect DLD owns its paired reads and retained address");
		if (form >= 12 && tick == 4)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((f.reg & 1 ? 0x20000 : 0) + 0x601, 0xdead);
		}
		if (tick == read_cycle - 1)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == f.next_ar,
				"DLD applies its selected two-word or indexed/circular pointer step once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2222);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 32);
		}
		if (tick == read_cycle)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(f.address, 0xdead); m_cpu->space(AS_DATA).write_word(f.address ^ 1, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save indirect DLD in Decode, Access or paired Read");
		}
		if (tick == 18)
		{
			u16 const st0 = (flags & 2) && f.reg && form != 15 ? (0x6608 & ~0xe000) | (f.reg << 13) : 0x6608;
			u16 const st1 = 0x800 | ((flags & 1) << 8) | ((flags & 2) << 4) | ((flags & 4) << 5);
			u16 const opcode = 0x5600 | (f.source << 8) | (form < 12 ? 0x80 + form * 8 + f.reg : form == 15 ? 0xf8 : 0xe0 + (form - 12) * 8 + f.reg);
			expect_opcode(opcode, m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == unsigned(form >= 12) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (f.source ? 0x1111222233ULL : f.value) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (f.source ? f.value : 0x4444555566ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2222,
				"DLD publishes its retained 32-bit pair with SXM guard extension in either C16 mode, preserving the other accumulator/status");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(253080, "Restore DLD without duplicate paired reads or pointer updates");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 24575) { start_pipeline_indirect_dld(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged indirect DLD: PASS cases=23040 forms=15 registers=8 accumulators=2 patterns=4 sxm=0,1 c16=0,1 compatibility=0,1 restore=23040 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (profile_is("indirect-dld")) throw emu_fatalerror(0, "TMS320C54x isolated indirect double loads complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253080)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253070;
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


bool tms320c54x_test_state::check_push_pop_same_results()
{
	if (m_phase == 255500)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = push_pop_same_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Adjacent stack pair stays within admitted forms");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_fast_stack_reads, &m_pipeline_store_writes, &m_pipeline_register_writes},
			"Save PSHM capture, pending write or retained POPM operand");
		if (tick == 16)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x603 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.destination == 0x18 ? f.value : f.sp) &&
				m_pipeline_operand_reads == 1 && m_fast_stack_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == 1 && m_cpu->space(AS_DATA).read_word(f.destination) == f.value, "Adjacent PSHM POPM preserves the forwarded word, stack ownership and flags across replay"); }
			if (replay_pipeline_at(255510, f.checkpoint,
				"Restore adjacent stack pair without rereading or duplicating either publication")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 2047) { start_push_pop_same(variant + 1); return true; }
			osd_printf_info("TMS320C54x adjacent PSHM POPM: PASS cases=2048 restore=2048 sources=8 destinations=T,AR3,BK,SP patterns=4 pages=2 stack_alignments=2 snapshots=4 source_reads=1 stack_reads=1 stack_writes=1 register_writes=1 source_mutation_after_read=1 stack_mutation_after_read=1 logical_bypass=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated adjacent PSHM POPM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255510) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255500; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_asm_store_results_part2()
{
	if (m_phase == 255560)
	{
		bool const unconditional = profile_is("rptb-bd");
		bool const tc = profile_is("rptb-tc-delay");
		bool const banz = profile_is("rptb-banz");
		unsigned const case_index = m_pipeline_peripheral_case, variant = unconditional || tc ? case_index * 6 : case_index, tick = ++m_idle_fixture_tick, count = std::array<unsigned, 4>{0, 1, 2, 7}[banz ? variant % 4 : (variant / 12) % 4], checkpoint = std::array<unsigned, 4>{7, 10, 12, 16}[banz ? variant / 192 : variant / 768];
		bool const b = !banz && variant % 12 >= 6, wide = !banz && (variant / 192) % 2;
		u32 const base = banz ? block_banz_case(variant).base : (variant / 384) % 2 ? 0x20000 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Complete delayed internal branch remains admitted within its repeat block");
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save delayed block branch predicate, delay work and speculative loop tags");
		if (tick == 512)
		{
			u16 const last = block_bcd_source(variant, count); u64 const value = tc ? (b ? 0x2345 : 0x1234) + unsigned(wide) * (count + 1) : u64(s64(s16(last)) + unsigned(wide)) & 0xffffffffffULL;
			u16 const st0 = tc ? 0xa608 | ((wide ? bool(last & 0x8000) : !b) ? 0x1000 : 0) : (0xa608 & ~0x800) | (wide && last == 0xffff ? 0x800 : 0);
			unsigned fallthroughs = 0;
			if (banz) { for (unsigned i = 0; i <= count; ++i) fallthroughs += block_banz_resolved(variant, i) == 0; expect(m_cpu->state_int(tms320c54x_device::STATE_AR0) == (block_banz_case(variant).form ? block_banz_resolved(variant, count) : block_banz_case(variant).initial), "BANZ applies each address modification once across block iterations"); }
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + 0x613 && m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 && m_pipeline_extensions_reads == count + 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5001 + count && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6001 + count && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400 + (wide || banz || tc ? 0 : count + 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1500 + (banz ? fallthroughs : wide ? 0 : count + 1) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1601 + count && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1701 &&
				m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == value && m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (b ? 0x1234 : 0x2345) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && !m_cpu->state_int(tms320c54x_device::STATE_BRC),
				"Internal branches retain decisions and complete each block iteration exactly once");
			if (replay_pipeline_at(255570, checkpoint,
				"Restore delayed block branch without duplicate arms, delays or source reads")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (case_index < (banz ? 767 : unconditional || tc ? 511 : 3071)) { start_block_bcd(case_index + 1); return true; }
			if (tc) { osd_printf_info("TMS320C54x delayed TC branches in block: PASS cases=512 restore=512 predicates=TC,NTC counts=0,1,2,7 patterns=4 delay_layouts=2 pages=2 snapshots=4 tc_producer=BITT ordered_sources=1 selected_writes=1 delay_effects_once=1 actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed TC branches in block complete"); }
			if (banz) { osd_printf_info("TMS320C54x long BANZ within block: PASS cases=768 restore=768 counts=0,1,2,7 forms=offset,preincrement,circular offset_pairs=8 pages=2 snapshots=4 ordered_sources=1 selected_writes=1 ar_updates_once=1 actual_iterations=1 compatibility_composition=unvalidated interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated long BANZ within block complete"); }
			if (unconditional) { osd_printf_info("TMS320C54x unconditional delayed block branches: PASS cases=512 restore=512 counts=0,1,2,7 patterns=4 accumulators=2 delay_layouts=2 pages=2 snapshots=4 ordered_sources=1 selected_writes=1 delay_words_once=1 actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated unconditional delayed block branches complete"); }
			osd_printf_info("TMS320C54x delayed internal block branches: PASS cases=3072 restore=3072 predicates=12 counts=0,1,2,7 patterns=4 delay_layouts=2 pages=2 snapshots=4 ordered_sources=1 selected_writes=1 delay_decision_retained=1 actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated delayed internal block branches complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255570) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255560; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
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
	if (m_phase == 310640)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant < 32 ? 2 : 10;
		expect(!m_rptbd_counts.reads && !m_rptbd_counts.writes && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5007 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xb608, "Excluded extended POPD leaves operands, stack, pointer and ARP unchanged");
		if (tick == checkpoint && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save extended POPD rejection before or after Decode/Access"); }
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Unaccepted extended POPD composition rejects explicitly");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; return restore_pipeline_at(310650, "Restore excluded extended POPD without operand effects"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 63) { start_extended_pop_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x extended POPD exclusions: PASS cases=64 restore=64 forms=mapped,SP,peripheral,unadmitted_mode,delay,repeat,block,XC pages=2 cmpt=2 snapshots=2 data_reads=0 data_writes=0 stack_changes=0 pointer_changes=0 silicon_trap_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated POPD edges complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310650) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310640; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310620)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = extended_pop_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.a &&
			(tick > 7 || m_cpu->state_int(tms320c54x_device::STATE_B) == f.b) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick < 5 ? f.st0 : f.final_st0), "Extended POPD preserves arithmetic state and applies only the address-owned ARP update");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (tick > 5 ? 0x6a00 + i : tick < 5 ? f.initial[i] : f.final[i]), "Extended POPD captures its destination before later pointer changes");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (tick < 6 ? 0x3000 : 0x3001), "Extended POPD consumes one stack word in Read");
		if (tick == 3) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == 5) for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x6a00 + i);
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x3000, 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save extended POPD across literal, address and publication ownership");
		}
		if (tick == 24)
		{
			expect(m_rptbd_counts.fetches == 1 && m_rptbd_counts.reads == 1 && m_rptbd_counts.writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x610 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst && m_cpu->state_int(tms320c54x_device::STATE_T) == f.t && m_cpu->state_int(tms320c54x_device::STATE_TRN) == f.trn && m_cpu->state_int(tms320c54x_device::STATE_BK) == f.bk, "Extended POPD reaches the actual two-word continuation with one transfer and unrelated state preserved");
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.address) == f.value, "Extended POPD retains its stack sample after backing-memory mutation"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts;
				return restore_pipeline_at(310630, "Restore extended POPD without refetching, retargeting or consuming another stack word");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 19199) { start_extended_pop(variant + 1); return true; }
			osd_printf_info("TMS320C54x extended POPD: PASS cases=16128 restore=16128 forms=25 values=4 cmpt=2 cpl=2 pages=2 snapshots=3 circular_bk=9 circular_steps_bounded=1 literal_fetches=1 stack_reads=1 destination_writes=1 pointer_updates=once source_retained=1 mapped_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated RAM POPD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310630) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310620; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310560)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = extended_push_case(variant);
		bool const success = !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick < 5 ? f.st0 : f.final_st0) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.a && (tick > 7 || m_cpu->state_int(tms320c54x_device::STATE_B) == f.b);
		if (!success) osd_printf_info("Extended push state mismatch case=%u replay=%u tick=%u illegal=%u st0=%04x expected=%04x sp=%04x\n", variant, unsigned(m_pipeline_replay), tick, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), tick < 5 ? f.st0 : f.final_st0, unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
		expect(success, "Extended push preserves accumulators/flags and publishes only its owned ARP update");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (tick < 5 ? f.initial[i] : f.final[i]), "Extended push modifies only its selected pointer once in Access");
		if (tick <= 5 || !f.irq) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (tick < 5 ? 0x3000 : 0x2fff), "Extended push allocates one stack word in Access");
		if (f.irq && tick == (f.irq <= 2 ? 2U : 8U)) { int const line = f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2; m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE); }
		if (tick == 3) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == 6 && !f.mapped) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_rptbd_saved_return = m_rptbd_return; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save extended push before address, after allocation or after write");
		}
		if (tick == 96)
		{
			expect(m_rptbd_counts.fetches == 1 && m_rptbd_counts.reads == 1 && m_rptbd_counts.writes == 1 && m_rptbd_counts.acks == unsigned(bool(f.irq)) && m_rptbd_counts.stack_reads == unsigned(bool(f.irq)) && m_rptbd_counts.stack_writes == unsigned(bool(f.irq)), "Extended push retains its literal/source and balances one optional interrupt frame");
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(0x2fff) == f.value, "Extended push retains the sampled source despite backing-memory mutation"); }
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x610 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2fff && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst && m_cpu->state_int(tms320c54x_device::STATE_T) == f.t && m_cpu->state_int(tms320c54x_device::STATE_TRN) == f.trn && m_cpu->state_int(tms320c54x_device::STATE_BK) == f.bk && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x1234 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x5678, "Extended push reaches its actual two-word continuation with unrelated state preserved");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts; m_rptbd_return = m_rptbd_saved_return;
				return restore_pipeline_at(310570, "Restore extended push without duplicate source, pointer or stack work");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 99695) { start_extended_push(variant + 1); return true; }
			if (profile_is("pshd-long-map")) { osd_printf_info("TMS320C54x extended PSHD mapped sources: PASS cases=3696 restore=3696 sources=26 forms=offset,preincrement,absolute self_modify_excluded=1 patterns=4 controls=safe pages=2 cmpt=2 snapshots=3 literal_fetches=1 source_reads=1 stack_writes=1 pointer_updates=once physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated extended PSHD mapped complete"); }
			osd_printf_info("TMS320C54x extended PSHD: PASS cases=99696 restore=99696 ram_cases=96000 mapped_cases=3696 forms=25 offsets=-1,0,4,32767 circular_offsets=-1,0,4,8 values=4 bk=0,9 cmpt=2 cpl=2 pages=2 sources=none,IRQ,NMI arrivals=early,late snapshots=3 literal_fetches=1 source_reads=1 stack_writes=1 pointer_updates=once source_retained=1 balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated extended PSHD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310570) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310560; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310520)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = stlm_pmst_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_PMST) == (tick < 6 ? f.old_pmst : f.value) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xb9ff, "STLM PMST owns one Execute publication and no status-flag change");
		if (f.irq && tick == (f.late ? 8U : 2U)) { int const line = f.irq == 17 ? INPUT_LINE_NMI : int(f.irq) - 1; m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE); }
		if (tick == 5) m_cpu->set_state_int(f.source_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, f.source);
		if (tick <= 6)
		{
			u64 const source = tick >= 5 ? f.source : 0x12121212;
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (f.source_b ? 0x123456789aULL : source) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.source_b ? source : 0xfedcba9876ULL), "STLM leaves both accumulators intact and consumes the Execute-live value");
		}
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_rptbd_saved_return = m_rptbd_return; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save STLM PMST before, at or after publication and vector selection");
		}
		if (tick == 96)
		{
			expect(m_rptbd_counts.writes == 1 && m_rptbd_counts.fetches == unsigned(bool(f.irq)) && m_rptbd_counts.acks == unsigned(bool(f.irq)) && m_rptbd_counts.stack_reads == unsigned(bool(f.irq)) && m_rptbd_counts.stack_writes == unsigned(bool(f.irq)), "STLM PMST and its optional vector/frame each own one side effect");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (f.low_entry && (f.value & 0x20) ? 0x11 : 0x44) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.low_entry && f.old_overlay ? 0x11 : 0x44) &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.entry + 16 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_BK) == 17, "Prefetched marker retains old mapping; subsequent Prefetch uses new OVLY without changing XPC or unrelated state");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x7000 + i, "Direct PMST store ignores DP/CPL and preserves all pointers");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts; m_rptbd_return = m_rptbd_saved_return;
				return restore_pipeline_at(310530, "Restore PMST source, mapping and interrupt state without duplicate store or vector");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 38911) { start_stlm_pmst(variant + 1); return true; }
			osd_printf_info("TMS320C54x STLM PMST: PASS cases=38912 restore=38912 mapping_cases=4096 interrupt_cases=34816 sources=A,B patterns=8 old_sst_sxm=4 cpl=0,1 pages=2 entry_regions=low,high old_overlay=0,1 interrupt_sources=IRQ0..15,NMI arrivals=early,late snapshots=4 writes=1 source_sample=Execute prefetched_mapping=retained next_mapping=new vector_mapping=Prefetch balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated STLM PMST complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310530) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310520; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310480)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = repeat_load_case(variant);
		unsigned completed = tick < f.first ? 0 : std::min(tick - f.first + 1, f.count + 1);
		if (f.block) { completed = 0; for (auto const &[address, word] : m_observed_instructions) if (address == f.body) ++completed; }
		unsigned const last = f.first + f.count, total = (f.count + 1) * (f.outer + 1);
		expect(completed <= total, "Outer and inner repeat counts cannot execute an extra load");
		auto const expected = m_repeat_load_expected[completed];
		unsigned const remaining = tick < f.first - 1 ? 0 : tick <= f.first ? f.count : tick <= last ? last - tick : 0;
		bool const success = !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[2] && (f.block || m_cpu->state_int(tms320c54x_device::STATE_RC) == remaining);
		if (!success) osd_printf_info("Repeated load mismatch case=%u tick=%u replay=%u setup=%u shift=%d selector=%u completed=%u expected=%010llx/%010llx/%04x/%04x actual=%010llx/%010llx/%04x/%04x illegal=%u\n", variant, tick, unsigned(m_pipeline_replay), f.setup, f.shift, f.selector, completed, (unsigned long long)expected[0], (unsigned long long)expected[1], unsigned(expected[2]), remaining, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_RC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
		expect(success, "Repeated load publishes each live-source shift and sticky destination overflow on its own clock");
		if (f.irq && tick == 2) { int const line = f.irq == 2 ? INPUT_LINE_NMI : 2; m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE); }
		if (f.block && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.body, f.word); }
		if (!f.block && tick == (f.setup == 1 || f.setup >= 4 ? 4U : 3U)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.body, 0xe8ff); }
		if (f.setup >= 2 && tick == (f.setup >= 4 ? 6U : 5U)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		if (tick < last) expect(!m_rptbd_counts.acks && !m_rptbd_counts.stack_writes, "Repeated load defers every interrupt until its final arithmetic publication");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_rptbd_saved_return = m_rptbd_return; m_saved_observed_instructions = m_observed_instructions;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeated load at Decode, middle or final arithmetic publication");
		}
		if (tick == (f.block ? total + 12 * (f.outer + 1) + 80 : last + 64))
		{
			unsigned iterations = 0, continuations = 0;
			for (auto const &[address, word] : m_observed_instructions) { if (address == f.body) { expect(word == f.word, "Repeated load retains its fetched opcode"); ++iterations; } if (address == f.body + 1) ++continuations; }
			expect(iterations == total && continuations == f.outer + 1 && m_rptbd_counts.fetches == f.outer + 1 + unsigned(f.block) && m_rptbd_counts.reads == unsigned(f.setup >= 2) && m_rptbd_counts.acks == unsigned(bool(f.irq)) && m_rptbd_counts.stack_writes == unsigned(bool(f.irq)) && m_rptbd_counts.stack_reads == unsigned(bool(f.irq)), "Repeated load owns count+1 operations and one fresh continuation per outer iteration, excluding discarded block prefetch");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + (f.block ? 0x609 : 0x610) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.block ? 0 : 13) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_BK) == 17, "Repeated load preserves status, stack and unrelated control state");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (i == 4 ? 0x6000 + f.outer + 1 : i == 5 && f.setup == 5 ? 0xdead : 0x7000 + i), "Repeated accumulator load does not modify address registers; each outer continuation runs once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts; m_rptbd_return = m_rptbd_saved_return; m_observed_instructions = m_saved_observed_instructions;
				return restore_pipeline_at(310490, "Restore repeat iteration, live accumulators and sticky flags without restarting");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.arithmetic ? 16383U : 200703U)) { start_repeat_load(variant + 1); return true; }
			if (f.arithmetic) { osd_printf_info("TMS320C54x repeated SFTA: PASS cases=16384 restore=16384 selectors=4 shifts=-16..15 patterns=8 sxm_ovm=4 counts=0,1,7,255 each_iteration=1 retained_opcode=1 carry_overflow=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated repeated SFTA complete"); }
			if (profile_is("rpt-ld-block")) { osd_printf_info("TMS320C54x repeated accumulator LD in block: PASS cases=2048 restore=2048 outer_counts=0,2 inner_counts=0,1,7,255 selectors=4 shifts=-1,15 sxm_ovm=4 patterns=2 pages=2 snapshots=2 each_iteration=1 inner_continuations=outer+1 outer_counter_preserved=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated repeated accumulator LD block complete"); }
			osd_printf_info("TMS320C54x repeated accumulator LD: PASS cases=200704 restore=200704 arithmetic_cases=32768 composition_cases=165888 block_cases=2048 selectors=4 shifts=-16..15 shift_sources=opcode,ASM sxm_ovm=4 counts=0,1,7,255 count_sources=short,long,DP,SP,absolute_RAM,absolute_AR pages=2 sources=none,IRQ,NMI snapshots=3 each_iteration=1 retained_opcode=1 retained_count=1 fresh_continuation=1 balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated repeated accumulator LD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310490) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310480; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
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
			start_dst_bypass(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase==399950) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=399940; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase==399960)
	{
		unsigned const tick=++m_idle_fixture_tick, variant=m_pipeline_peripheral_case; auto const f=dst_bypass_case(variant);
		if (tick==f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_store_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save DST forwarding dependency at selected checkpoint");
		}
		if (tick==48)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE),"DST dependency completes cleanly");
			expect(m_cpu->state_int(f.producer_b?tms320c54x_device::STATE_A:tms320c54x_device::STATE_B)==f.loaded && m_cpu->state_int(f.producer_b?tms320c54x_device::STATE_B:tms320c54x_device::STATE_A)==f.source,"DST consumers match independent word/pair and saturation oracle");
			unsigned const reads=f.kind==2?2:1;
			expect(m_pipeline_store_writes==2 && m_pipeline_operand_reads<=reads && ((f.same&&!f.gap)||m_pipeline_operand_reads==reads),"DST bypass suppresses only aliased pending reads, never real writes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay=true; m_idle_fixture_tick=f.checkpoint; m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_store_writes=m_recurring_irq_snapshot[1];
				return restore_pipeline_at(399970, "Restore DST dependency without duplicate reads or writes");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant<4607) { start_dst_bypass(variant+1); return true; }
			osd_printf_info("TMS320C54x DST read bypass: PASS cases=4608 restore=4608 accumulators=2 consumers=high,low,pair modes=3 patterns=4 gaps=0,1 aliases=2 sxm=2 sst=2 pages=2 save_points=2 writes_each=2 silicon_phase_claim=0\n");
			if (profile_is("mpy-mapped")) throw emu_fatalerror(0,"TMS320C54x isolated MPY mapped consumer complete");
			start_timer_boundary_probe(0,true); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase==399970) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=399960; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 253490)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = long_mvkd_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long MVKD has no atomic fallback");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0xbeef);
		}
		if (tick == 6) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.address.selected) == f.address.next_ar, "Long MVKD updates only the selected destination pointer once in Access");
		if (tick == 7)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address.selected, 0x2000);
		}
		capture_pipeline_at(f.checkpoint, {&m_pipeline_store_writes, &m_pipeline_operand_reads, &m_pipeline_extensions_reads},
			"Save partial extensions, retained source or completed write");
		if (tick == 16)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.address.address) == f.value && m_pipeline_store_writes == 1 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 2, "Long MVKD retains both literals and its source across later mutations"); }
			if (replay_pipeline_at(253500, f.checkpoint,
				"Restore long MVKD without duplicate reads, pointer updates or writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 19199) { start_pipeline_long_mvkd(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged long MVKD: PASS cases=19200 forms=25 offsets=4 sources=selected_AR,AR2,RAM,alias values=4 compatibility=2 pages=2 restore=19200 reads_each=1 writes_each=1 extensions_each=2 silicon_phase_claim=0\n");
			if (profile_is("long-mvkd")) throw emu_fatalerror(0, "TMS320C54x isolated long MVKD complete");
			start_pipeline_imr_store(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253500)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253490; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253510)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const stlm = profile_is("stlm-imr");
		auto const f = stlm ? stlm_imr_case(variant) : imr_store_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IMR) == (tick < (stlm ? 6 : 7) ? f.old_mask : f.mask) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1804 && m_pipeline_store_writes == unsigned(tick >= (stlm ? 6 : 7)), "IMR store leaves mask/status unchanged until its owned publication");
		if (tick == 2 && f.kind) m_cpu->set_input_line(f.kind == 2 ? INPUT_LINE_NMI : f.source, ASSERT_LINE);
		if (!stlm && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xbeef); }
		if (stlm && tick == 5) m_cpu->set_state_int(variant >= 4608 ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, f.mask);
		capture_pipeline_at(f.checkpoint, {&m_pipeline_store_writes, &m_pipeline_extensions_reads, &m_vector_acks},
			"Save retained mask literal, pending publication or accepted entry");
		if (tick == 20)
		{
			if (stlm) expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (variant >= 4608 ? 0x1234 : f.mask), "IMR publication preserves its A source or unrelated accumulator through ISR entry");
			expect(m_pipeline_extensions_reads == unsigned(!stlm) && m_vector_acks == unsigned(bool(f.ack_cycle)) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.ack_cycle ? 0x55 : stlm && variant >= 4608 ? f.mask : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (f.kind == 1 && !f.ack_cycle ? f.bit : 0), "Mask enable/clear, INTM and NMI preserve accepted entries or leave masked IRQ pending");
			if (replay_pipeline_at(253520, f.checkpoint,
				"Restore mask write and interrupt entry without duplicate literal, write or acknowledgement")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (stlm ? 9215 : 4607)) { start_pipeline_imr_store(variant + 1); return true; }
			// Later fixture groups must be able to generate a fresh NMI edge.
			for (unsigned line = 0; line < 16; ++line) m_cpu->set_input_line(line, CLEAR_LINE);
			m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
			if (stlm)
			{
				osd_printf_info("TMS320C54x staged STLM IMR: PASS cases=9216 restore=9216 accumulators=2 irq_bits=16 masks=4 initial_masks=2 intm=2 sources=none,IRQ,NMI pages=2 snapshots=3 source_execute_live=1 writes_each=1 destination_reads=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated STLM IMR complete");
			}
			osd_printf_info("TMS320C54x staged STM IMR: PASS cases=4608 restore=4608 irq_bits=16 masks=4 initial_masks=2 intm=2 sources=none,IRQ,NMI pages=2 literal_fetches=1 writes_each=1 destination_reads=0 silicon_phase_claim=0\n");
			if (profile_is("imr-store")) throw emu_fatalerror(0, "TMS320C54x isolated STM IMR complete");
			start_pipeline_long_pair(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253520)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253510; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253530)
	{
		expect(m_vector_acks == 1 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 && !m_bus_iack_enable,
			"Disabling the physical IACK output must not suppress logical interrupt acknowledgement");
		m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
		osd_printf_info("TMS320C54x C5416 bus registers: PASS encodings=65536 saved_restore=1 reset=1 partial_writes=1 clkout_system_independent=1 logical_ack_with_iackoff=1 bus_timing_acceptance=0 physical_mapping=unvalidated\n");
		throw emu_fatalerror(0, "TMS320C54x isolated C5416 bus registers complete");
	}
	return false;
}

bool tms320c54x_test_state::check_preincrement_immediate_results()
{
	if (m_phase == 253930)
	{
		bool const immediate = profile_is("preinc-st");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = immediate ? std::array<unsigned, 3>{4, 6, 7}[variant / 512] : std::array<unsigned, 3>{4, 5, 6}[variant / 2048]; auto const f = immediate ? preincrement_immediate_case(variant) : preincrement_store_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Short preincrement accumulator stores remain staged");
		if (immediate && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word((variant & 256 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (tick == (immediate ? 6 : 5))
		{
			for (unsigned ar = 0; ar < 8; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.selected ? f.address : u16(f.address - 1)), "Preincrement updates only the selected pointer before retaining the destination");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2222); m_cpu->set_state_int(f.selector & 1 ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, f.source);
		}
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_store_writes, immediate ? m_pipeline_extensions_reads : 0}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save preincrement around pointer, captured address and publication"); }
		if (tick == 18)
		{
			u16 const status = BIT(f.st1, 5) && f.reg ? (f.st0 & 0x1fff) | (f.reg << 13) : f.st0;
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.address) == f.value, "RAM or wrapped IMR destination contains the independently saturated word"); }
			expect(m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.selector & 1 ? 0xabcdef1234ULL : f.source) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.selector & 1 ? f.source : 0xabcdef1234ULL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == status && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst && m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2222, "Preincrement retains its write address without restoring a changed pointer or altering accumulator/status");
			if (immediate) expect(m_pipeline_extensions_reads == 1, "Immediate preincrement emits one literal-owned write despite accumulator and program changes");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; if (immediate) m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; return restore_pipeline_at(253940, "Restore preincrement without duplicate pointer modifications or writes"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (immediate)
			{
				m_pipeline_fetch_tap.remove();
				if (variant < 1535) { start_preincrement_store(variant + 1); return true; }
				osd_printf_info("TMS320C54x staged preincrement ST: PASS cases=1536 restore=1536 registers=8 literals=4 sst_sxm=4 compatibility=2 pages=2 snapshots=3 literal_fetches=1 reads=0 writes=1 pointer_updates=1 wrap_imr=1 source_literal_retained=1 single_repeat_acceptance=0 interrupt_composition=unvalidated silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated preincrement ST complete");
			}
			if (variant < 6143) { start_preincrement_store(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged preincrement store: PASS cases=6144 restore=6144 selectors=STL_A,STL_B,STH_A,STH_B registers=8 patterns=4 sst_sxm=4 compatibility=2 pages=2 snapshots=3 reads=0 writes=1 pointer_updates=1 wrap_imr=1 source_execute_live=1 interrupt_composition=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated preincrement store complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_repeat_pop_exclusion_results_part2()
{
	if (m_phase == 253940) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253930; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
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
	if (m_phase == 62136)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = repeated_stld_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeated STLD remains within the qualified RAM contract");
		unsigned const retired = tick < 7 ? 0 : std::min(tick - 6, f.iterations), sampled = tick < 6 ? 0 : std::min(tick - 5, f.iterations);
		auto const state = m_repeat_load_expected[retired];
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == state[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == state[1] && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.arithmetic || f.form < 4 || !sampled ? 0xbeef : repeated_stld_sample(sampled - 1)), "Repeated parallel operations maintain per-clock accumulator feedback and their T contract");
		if (f.arithmetic) expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == state[2], "Repeated parallel arithmetic publishes carry and sticky overflow once per retired copy");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeated STLD with pending samples, stores and pointer updates"); }
		if (tick == f.iterations + 16)
		{
			expect(m_pipeline_operand_reads == f.iterations && m_pipeline_store_writes == f.iterations && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x603 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (f.arithmetic ? state[2] : 0xa630) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "Repeated STLD drains exactly once before a fresh continuation");
			for (unsigned ar = 2; ar < 6; ++ar) { u16 expected = 0x4800 + ar; if (ar == (f.layout == 1 ? 5U : 2U)) expected = repeated_stld_address(f, f.iterations, false); else if (f.layout != 3 && ar == 3) expected = repeated_stld_address(f, f.iterations, true); expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == expected, "Repeated STLD applies each DAGEN update once with X priority on aliases"); }
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < (f.layout ? f.iterations : 1); ++i) expect(m_cpu->space(AS_DATA).read_word(repeated_stld_address(f, i, true)) == repeated_stld_store(f, m_repeat_load_expected[f.layout ? i : f.iterations - 1]), "Repeated STLD retains every published RAM result"); }
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(62137, "Restore repeated STLD after clearing live pipeline state"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.arithmetic ? 30719U : 9215U)) { start_repeated_stld(variant + 1); return true; }
			if (f.arithmetic) { osd_printf_info("TMS320C54x repeated parallel arithmetic: PASS cases=30720 restore=30720 forms=8 operations=ADD,SUB layouts=5 ar0_stride=3 shifts=4 sxm_sst_ovm=8 counts=0,1,7,255 pages=2 snapshots=3 fresh_sources=1 old_source_store=1 accumulator_feedback=1 carry_bit=32 sticky_overflow=1 t_preserved=1 alias_priority=X opcode_fetches=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated repeated parallel arithmetic complete"); }
			osd_printf_info("TMS320C54x repeated STLD: PASS cases=9216 restore=9216 forms=6 layouts=4 shifts=4 sxm_sat=4 counts=0,1,7,255 pages=2 snapshots=3 source_retained=1 accumulator_feedback=1 t_publish=Read alias_priority=X opcode_fetches=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated repeated STLD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62137) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62136; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_ldu_status_value_results()
{
	if (m_phase == 254010)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, form = (variant / 8) % 3, read = form == 1 ? 6 : 5;
		unsigned const checkpoint = std::array<unsigned, 3>{3, read - 1, read}[variant / 96];
		bool const b = (variant / 24) % 2; u16 const value = ldu_status_value(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped ST1 LDU remains staged");
		if (form == 1 && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(((variant / 48) % 2 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (form == 2 && tick == read - 1) for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x2222);
		if (tick == read) m_cpu->set_state_int(tms320c54x_device::STATE_ST1, value ^ 0xffff);
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads},
			"Save mapped status before and after retained Read");
		if (tick == 18)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(form == 1) && m_cpu->state_int(tms320c54x_device::STATE_A) == (b ? 0xabcdef1234ULL : value) && m_cpu->state_int(tms320c54x_device::STATE_B) == (b ? value : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((value ^ 0xffff) & ~u16(0x400)) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (form == 2 && (value & 0x20) ? 0x5600 : 0x1600) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0, "LDU retains unsigned ST1 and only the captured CMPT's documented ARP update");
			if (replay_pipeline_at(254020, checkpoint,
				"Restore ST1 LDU without rereading a changed register or literal")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 287) { start_ldu_status(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged LDU ST1: PASS cases=288 restore=288 forms=direct,absolute,indirect values=8 accumulators=2 pages=2 snapshots=3 register_reads=1 writes=0 source_retained=1 unsigned_full_word=1 producer_latency_claim=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated LDU ST1 complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254020) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254010; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mvmd_status_block_results()
{
	if (m_phase == 253870)
	{
		bool const pop = profile_is("pop-brc");
		bool const stm = pop || profile_is("stm-brc");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = stm ? std::array<unsigned, 5>{0, 1, 2, 7, 127}[variant % 5] : std::array<unsigned, 4>{0, 1, 2, 7}[variant % 4], checkpoint = stm ? std::array<unsigned, 3>{4, 5, 6}[variant / 10] : std::array<unsigned, 3>{5, 6, 12}[variant / 8];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Full status followed by RPTB stays staged");
		if (tick == 6) expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x8800, "Full status write does not erase the younger RPTB BRAF update");
		if (stm && !pop && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(((variant / 5) % 2 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (pop && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1000, count ^ 0xffff); }
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, stm ? m_pipeline_extensions_reads : 0}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save prefix write and younger block repeat"); }
		if (tick == (stm ? 32 + count * 5 : 96))
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x5001 + count && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800, "Following RPTB completes its count-plus-one body and clears only BRAF");
			if (stm) expect(m_pipeline_register_writes == 1 && m_pipeline_extensions_reads == 1, "Count publication and literal fetch occur once across the entire repeat");
			if (pop) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1001, "POPM BRC advances its source stack once across the repeat");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; if (stm) m_pipeline_extensions_reads = m_recurring_irq_snapshot[3]; return restore_pipeline_at(253880, "Restore prefix/block composition without duplicate words"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (stm)
			{
				m_operand_data_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
				if (variant < 29) { start_mvmd_status_block(variant + 1); return true; }
				if (pop)
				{
					osd_printf_info("TMS320C54x staged POPM BRC repeat: PASS cases=30 restore=30 counts=0,1,2,7,127 pages=2 snapshots=3 stack_reads=1 counter_reads=0 counter_writes=1 gap_nops=1 source_retained=1 count_plus_one=1 active_repeat_write_acceptance=0 silicon_phase_claim=0\n");
					throw emu_fatalerror(3, "TMS320C54x isolated POPM BRC repeat complete");
				}
				osd_printf_info("TMS320C54x staged STM BRC: PASS cases=30 restore=30 counts=0,1,2,7,127 pages=2 snapshots=3 literal_fetches=1 counter_reads=0 counter_writes=1 adjacent_rptb=1 latency=0 in_loop_acceptance=0 interrupt_composition=unvalidated silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated STM BRC complete");
			}
			if (variant < 23) { start_mvmd_status_block(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MVMD block: PASS cases=24 restore=24 counts=0,1,2,7 pages=2 snapshots=3 adjacent_rptb=1 braf_preserved=1 interrupt_composition=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated MVMD block complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253880) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253870; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delay_copy_results()
{
	if (m_phase == 62174)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const control = profile_is("delay-copy-ctrl");
		unsigned const kind = variant % 6, v = control ? (variant / 6 / 4) * 5 + std::array<unsigned, 4>{0, 2, 3, 4}[(variant / 6) % 4] : variant;
		auto const f = delay_copy_case(v); unsigned const read = control ? delay_multiply_fixture::operand_read(kind, 0) : 5;
		unsigned const checkpoint = std::array<unsigned, 3>{3, read - 1, read}[v / 5120];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Short DELAY remains admitted through retirement");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (control && kind == 1 ? 0 : control && kind == 2 ? 1 : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_TRN) == 0xa55a && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= read - 1 ? f.result_st0 : f.st0), "DELAY preserves accumulators T TRN and non-addressing flags");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (tick > read - 1 ? 0x6000 + i : tick == read - 1 && i == f.selected ? f.next_ar : 0x5007), "DELAY applies its pointer update once in Access");
		if (tick == read - 1) for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x6000 + i);
		if (tick == read) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_fast_stack_reads, &m_pipeline_register_writes},
			"Save DELAY around Access and retained Read");
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1, "DELAY completes with exactly one read and successor write");
			if (control) expect(m_fast_stack_reads == unsigned(kind == 5) && m_pipeline_register_writes == (kind == 4 ? 2U : unsigned(kind == 3)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (kind == 4 ? 0xffe : kind == 3 ? 0xfff : kind == 5 ? 0x1001 : 0x1000), "DELAY control owner balances its independent stack transactions");
			if (replay_pipeline_at(62175, checkpoint,
				"Restore DELAY without rereading or duplicate write")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (control ? 73727U : 15359U)) { start_delay_copy(variant + 1); return true; }
			if (control) { osd_printf_info("TMS320C54x control-delay DELAY: PASS cases=73728 restore=73728 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postdecrement,postincrement registers=8 patterns=8 sxm_ovm=4 compatibility=2 pages=2 snapshots=3 reads=1 writes=1 stack=owned physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated control-delay DELAY complete"); }
			osd_printf_info("TMS320C54x staged DELAY: PASS cases=15360 restore=15360 forms=DP,SP,indirect,postdecrement,postincrement registers=8 patterns=8 sxm_ovm=4 compatibility=2 pages=2 snapshots=3 reads=1 writes=1 source=Read destination=Execute physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated DELAY complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62175) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62174; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
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
	if (m_phase == 62110)
	{
		bool const immediate = profile_is("stm-st1");
		auto const f = stlm_status_case(m_pipeline_peripheral_case, immediate); unsigned const tick = ++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "STLM status publication and qualified consumers stay staged");
		if (tick == 4 || tick == 5) m_cpu->set_state_int(f.source ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, tick == 4 ? f.value ^ 0xffff : f.value);
		if (immediate && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, u16(~f.requested)); }
		if (tick == 6) expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status && m_xf_pin == bool(f.status & 0x2000), "STLM applies reserved-bit/INTM rules and publishes XF at Execute");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save retained STLM and consumer mode before/at publication");
		}
		if (tick == 18)
		{
			if (immediate) expect(m_pipeline_extensions_reads == 1, "STM captures the literal once across replay");
			u16 const mode = immediate || f.gap || f.family == 3 ? f.status : f.before;
			u64 expected = f.value;
			if (f.family == 1 || f.family == 3) expected = mode & 0x100 ? 0xffffff8001ULL : 0x8001;
			if (f.family == 2 || f.family >= 4)
			{
				u64 const source = 0x8012345678ULL; int const shift = int(mode & 31) - (mode & 16 ? 32 : 0);
				expected = shift >= 0 ? (source << shift) & 0xffffffffffULL : mode & 0x100 ? u64((s64(source) - (s64(1) << 40)) >> -shift) & 0xffffffffffULL : source >> -shift;
				if (f.family >= 4) expected = (f.family == 4 ? f.value + expected : f.value - expected) & 0xffffffffffULL;
			}
			if (m_cpu->state_int(f.source ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) != expected)
				osd_printf_info("STLM status mismatch: case=%u family=%u gap=%u before=%04x status=%04x expected=%010llx actual=%010llx\n", m_pipeline_peripheral_case, f.family, f.gap, f.before, f.status, (unsigned long long)expected, (unsigned long long)m_cpu->state_int(f.source ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A));
			expect(m_cpu->state_int(f.source ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == expected && m_cpu->state_int(f.source ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x8012345678ULL && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status && m_cpu->state_int(tms320c54x_device::STATE_PMST) == unsigned(!f.family && (m_pipeline_peripheral_case % 64) & 32) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2000 && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1400 && m_pipeline_store_writes == 1 && m_pipeline_operand_reads == unsigned(f.family == 1 || f.family == 3), "STLM retains one publication and consumers obey one-gap SXM/ASM or zero-gap long-offset latency");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_check_timer->adjust(attotime::never);
				m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				if (immediate) m_cpu->set_pipeline_frontend(true);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Replay STLM without duplicate publication or loss of sampled modes"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62111; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 18431) { start_stlm_status(m_pipeline_peripheral_case + 1); return true; }
			if (immediate)
			{
				expect(m_pipeline_extensions_reads == 1, "STM captures the literal once across replay");
				osd_printf_info("TMS320C54x staged STM ST1: PASS cases=18432 restore=18432 families=raw,SXM_LD,ASM_LD,long_SXM_LD,ASM_ADD,ASM_SUB gaps=0,1,2,3 pages=2 snapshots=3 literal_reads=1 literal_poison=1 status_writes=1 destination_reads=0 short_latency=0 long_latency=0 clear_policy=legacy_unverified reset_before_restore=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated STM ST1 complete");
			}
			osd_printf_info("TMS320C54x staged STLM ST1: PASS cases=18432 restore=18432 sources=A,B families=raw,SXM_LD,ASM_LD,long_SXM_LD,ASM_ADD,ASM_SUB gaps=0,1,2,3 pages=2 snapshots=3 execute_live=1 status_writes=1 destination_reads=0 short_latency=1 long_latency=0 clear_policy=legacy_unverified silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated STLM ST1 complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62111) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62110; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 253850)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, fields = variant % 256, checkpoint = std::array<unsigned, 3>{4, 6, 7}[variant / 256];
		bool const sxm = fields & 1, shift = fields & 64;
		u16 const status = 0x800 | (sxm ? 0x100 : 0) | ((fields / 2) & 31);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Full status and adjacent arithmetic remain staged");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, 0}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save full status publication and adjacent consumer"); }
		if (tick == 14)
		{
			u64 const source = 0x8012340000ULL | status;
			int const count = int(status & 31) - ((status & 16) ? 32 : 0);
			u64 const expected = !shift ? (sxm ? 0xffffff8001ULL : 0x8001) : count >= 0 ? (source << count) & 0xffffffffffULL : sxm ? u64((s64(source) - (s64(1) << 40)) >> -count) & 0xffffffffffULL : source >> -count;
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected && m_cpu->state_int(tms320c54x_device::STATE_B) == source && m_cpu->state_int(tms320c54x_device::STATE_ST1) == status && (m_cpu->state_int(tms320c54x_device::STATE_ST0) & ~0x400) == 0xa200 && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1400 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == unsigned(!shift), "Adjacent SXM and ASM consumers use the newly published fields with zero latency");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; return restore_pipeline_at(253860, "Restore status/consumer without duplicate accesses"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_mvmd_status_alu(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MVMD status ALU: PASS cases=768 restore=768 consumers=SXM_LD,ASM_LD sxm_states=2 asm_values=32 pages=2 snapshots=3 source_reads=1 status_writes=1 latency=0 interrupt_composition=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated MVMD status ALU complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253860) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253850; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}


bool tms320c54x_test_state::check_repeat_mvkd_results()
{
	if (m_phase == 62128)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = repeat_mvkd_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Short RAM-only repeated MVKD is admitted");
		if (tick == f.read - 1) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601 + f.prefix, 0xdead); }
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1000, 0xcafe); }
		unsigned const checkpoint = std::array<unsigned, 4>{f.read - 1, f.read, f.read + 1, f.read + f.count / 2}[f.snapshot];
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save repeated MVKD with owned literal, source and destination slots");
		if (tick == f.read + f.count + 16)
		{
			unsigned reads = 0;
			for (unsigned n = 0; n <= f.count; ++n) reads += !repeat_mvkd_bypass(f, n);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && !m_cpu->state_int(tms320c54x_device::STATE_RC) && m_pipeline_operand_reads == reads && m_pipeline_store_writes == f.count + 1 && m_pipeline_extensions_reads == 2 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x9876543210ULL, "Repeated MVKD drains RC, resumes IDLE and preserves unrelated state without duplicate transactions");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (i != f.reg ? 0x3300 + i : u16(f.initial_ar + (f.form == 3 || f.form == 5 ? f.count + 1 : f.form == 4 ? -int(f.count + 1) : 0))), "Repeated MVKD applies each short-address modification exactly once");
			if (replay_pipeline_at(62129, checkpoint,
				"Restore repeated MVKD without resampling or repeating writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 9215) { start_repeat_mvkd(variant + 1); return true; }
			osd_printf_info("TMS320C54x repeated MVKD: PASS cases=9216 restore=9216 forms=DP,SP,plain,postinc,postdec,preinc registers=8 counts=0,1,2,7 count_forms=2 layouts=distinct,equal,overlap pages=2 snapshots=4 source_increment=1 same_address_bypass=1 issue_interval=1 literal_fetches=once mapped_acceptance=0 interrupt_composition=unvalidated silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated repeated MVKD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62129) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62128; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 259940)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const form = variant % 5, reg = (variant / 5) % 8;
		unsigned const count = std::array<unsigned, 5>{0, 1, 2, 7, 255}[(variant / 40) % 5];
		unsigned const checkpoint = std::array<unsigned, 3>{5, 8, 12}[variant / 3200];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Legal short-source repeated MVDP is admitted");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, unsigned(m_first_port_cycle)};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeated MVDP before and during the stream");
		}
		if (tick == count + 64)
		{
			u16 const next = form == 3 ? 0x1230 + count + 1 : form == 4 ? 0x1230 - count - 1 : 0x1230;
			expect(m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + reg) == next, "Repeated MVDP retires count plus one transfers and resumes after pmad");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_first_port_cycle = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(259950, "Restore repeated MVDP without duplicate transfers");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 9599) { start_repeat_mvdp(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged repeated MVDP: PASS cases=9600 restore=9600 forms=5 registers=8 counts=0,1,2,7,255 count_forms=2 pages=2 destinations=ordinary,wrap ovly=2 snapshots=3 source_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeated MVDP complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 259950) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 259940; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 259920)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = std::array<unsigned, 6>{5, 6, 8, 9, 10, 12}[variant % 6];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Adjacent standalone MVDP remains admitted");
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save adjacent MVDP transfers");
		if (tick == 32)
		{
			expect(m_pipeline_operand_reads == 2 && m_pipeline_store_writes == 2 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1232, "Adjacent MVDP completes both transfers and address updates");
			if (replay_pipeline_at(259930, checkpoint,
				"Restore adjacent MVDP without duplicate transfers")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 11) { start_mvdp_pair(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged adjacent MVDP: PASS cases=12 restore=12 pages=2 snapshots=6 source_reads=2 program_writes=2\n");
			throw emu_fatalerror(0, "TMS320C54x isolated adjacent MVDP complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 259930) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 259920; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 259900)
	{
		unsigned const variant = m_pipeline_peripheral_case, form = variant % 7, reg = (variant / 7) % 8;
		unsigned const tick = ++m_idle_fixture_tick, extra = form >= 3;
		unsigned const checkpoint = std::array<unsigned, 3>{5, 6, 8}[variant / 112] + extra;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Legal standalone MVDP is admitted by the staged frontend");
		if (tick == 5 + extra)
		{
			u16 const next = form == 6 ? 0x1201 : form == 2 || form == 5 ? 0x1231 : 0x1230;
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + reg) == next, "MVDP modifies its address once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + reg, 0x4000);
		}
		if (tick == 6 + extra)
		{
			u16 const source = std::array<u16, 7>{0x1203, 0x1203, 0x1230, 0x1230, 0x122f, 0x1231, 0x1201}[form];
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(source, 0xdead);
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save MVDP before source and during the retained transfer");
		if (tick == 24)
		{
			{ auto const disable = machine().disable_side_effects();
				expect(m_cpu->space(AS_PROGRAM).read_word(((variant / 56) % 2 ? 0x20000 : 0) + 0x2e00) == 0x2468, "MVDP publishes data in executable program storage"); }
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + reg) == 0x4000, "MVDP retains its source without reapplying the address update");
			if (replay_pipeline_at(259910, checkpoint,
				"Restore MVDP without duplicate source or destination effects")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 335) { start_mvdp(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MVDP: PASS cases=336 restore=336 forms=7 registers=8 pages=2 snapshots=3 source_reads=1 program_writes=1 repeat=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated MVDP complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 259910) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 259900; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254800)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const form = variant % 7, reg = (variant / 7) % 8, pattern = (variant / 56) % 4;
		bool const mapped = (variant / 448) % 2, extended = form >= 3;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 5, 6, 9}[variant / 896] + extended;
		u16 const source = std::array<u16, 7>{0x1203, 0x1203, 0x1230, 0x1230, 0x122f, 0x1231, 0x1201}[form];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "WRITA retained transfer remains admitted");
		if (tick == 4 + extended)
		{
			u16 const next = form == 6 ? 0x1201 : form == 2 || form == 5 ? 0x1231 : 0x1230;
			if (m_cpu->state_int(tms320c54x_device::STATE_AR0 + reg) != next)
				osd_printf_info("WRITA address mismatch: case=%u tick=%u AR%u=%04x expected=%04x\n", variant, tick, reg, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0 + reg)), next);
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + reg) == next, "WRITA address modification happens once at Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + reg, 0x4000);
		}
		if (tick == 5 + extended) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(source, 0xdead); }
		if (tick == 6 + extended)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7f9000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, mapped ? 0 : 0x20);
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save WRITA before source, PAR capture and write");
		if (tick == 24)
		{
			{ auto const disable = machine().disable_side_effects();
				u32 const logical = std::array<u32, 4>{0x032e00, 0x03a000, 0x7fff00, 0xffffff}[pattern];
				u32 const physical = mapped && (logical & 0xffff) < 0x8000 ? logical & 0xffff : logical & 0x7fffff;
				expect(m_cpu->space(AS_PROGRAM).read_word(physical) == std::array<u16, 4>{0, 0xf495, 0xffff, 0x1234}[pattern], "WRITA publishes the retained word in program RAM, not merely a write tap");
			}
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7f9000 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa624 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (form == 1 ? 0x4000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + reg) == 0x4000, "WRITA retains its word and mapped destination without rewriting status, accumulators or pointers");
			if (replay_pipeline_at(254810, checkpoint,
				"Restore WRITA without duplicate source reads or program writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3583) { start_write_a(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged WRITA: PASS cases=3584 restore=3584 forms=7 registers=8 values=4 pages=2 mapping=2 snapshots=4 source_reads=1 program_writes=1 destination_reads=0 repeat=unvalidated mapped_source=excluded silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated WRITA complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254810) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254800; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254700)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const b = variant & 1, zero = variant & 2;
		unsigned const gap = (variant >> 2) & 1, checkpoint = 4 + (variant >> 4);
		u32 const base = variant & 8 ? 0x20000 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Load/return composition stays admitted");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_fast_stack_reads, m_pipeline_store_writes, 0};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save load/return around producer and predicate");
		}
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + (zero ? 0x701 : 0x603 + gap), "RC consumes the preceding load result, not the opposite old accumulator");
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(zero) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1201 && m_pipeline_operand_reads == 1 && m_fast_stack_reads == unsigned(zero) && !m_pipeline_store_writes, "Load/return owns exactly one producer and conditional stack read");
			expect(m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == unsigned(!zero) && m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (b ? 0x1234 : 0x2345), "Load/return preserves unselected accumulator");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_fast_stack_reads = m_recurring_irq_snapshot[1]; m_pipeline_store_writes = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(254710, "Restore load/return without duplicate accesses");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 63) { start_return_load(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged load return: PASS cases=64 restore=64 accumulators=2 values=2 gaps=2 pages=2 snapshots=4 producer_reads=1 conditional_stack_reads=1 data_writes=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated load return complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254710) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254700; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_t_store_branch_results()
{
	if (m_phase == 62136)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant & 16 ? 10 : 5;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Draining nondelayed BC bookkeeping is not a T/TRN delay-slot owner");
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save conditional branch and ordinary long T/TRN store");
		if (tick == 24)
		{
			expect(!m_pipeline_operand_reads && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (variant & 2 ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900, "TC/NTC taken and untaken paths preserve flags and publish exactly one store");
			if (replay_pipeline_at(62137, checkpoint,
				"Restore BC/store composition without duplicate writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 31) { start_t_store_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x BC T/TRN stores: PASS cases=32 restore=32 predicates=TC,NTC initial_tc=2 sources=T,TRN pages=2 snapshots=2 writes=1 reads=0 delayed_acceptance=0 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated BC T/TRN stores complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62137) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62136; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}


bool tms320c54x_test_state::check_mvkd_delay_results()
{
	if (m_phase == 253610)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const reg = variant % 6, shift = (reg % 3) * 16;
		u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 6) % 4];
		bool const absolute = (variant / 24) % 2;
		unsigned const checkpoint = std::array<unsigned, 3>{5, 8, 12}[variant / 96];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped READA has no atomic fallback");
		if (tick == 7) m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x3400030400ULL);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, 0};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped READA before/during program hold or after write");
		}
		if (tick == 15)
		{
			u64 expected_a = 0x3400030400ULL, expected_b = 0xfeabcd9876ULL;
			u64 &target = reg < 3 ? expected_a : expected_b;
			u64 const mask = u64(reg % 3 == 2 ? 0xff : 0xffff) << shift;
			target = (target & ~mask) | ((u64(value) << shift) & mask);
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected_a && m_cpu->state_int(tms320c54x_device::STATE_B) == expected_b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800, "Mapped READA preserves untouched accumulator words and status");
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == unsigned(absolute), "Mapped READA does not duplicate retained accesses");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(253620, "Replay held program source and mapped publication");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 287) { start_mapped_read_a(variant + 1); return true; }
			osd_printf_info("TMS320C54x mapped READA: PASS cases=288 restore=288 registers=AL,AH,AG,BL,BH,BG forms=direct,absolute source_pages=2 source_reads=1 writes=1 status_preserved=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated mapped READA complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253620) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253610; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254140)
	{
		unsigned const tick = ++m_idle_fixture_tick; auto const &f = m_mvdd_fixture;
		unsigned const checkpoint = std::array<unsigned, 4>{3 + f.prefix, f.first_write() - 2, f.first_write() - 1, f.first_write() + f.count}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "MVDD remains stage-owned without atomic fallback");
		if (!f.repeated && tick <= 4)
			for (unsigned i = 0; i < 4; ++i)
			{
				u16 const expected = tick == 4 ? f.final[i] : f.initial[i];
				if (m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) != expected) osd_printf_info("mvdd_ar: case=%u tick=%u reg=%u actual=%04x expected=%04x x=%x y=%x bk=%u\n", m_pipeline_peripheral_case, tick, i + 2, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i)), expected, f.x, f.y, f.linear ? 0 : 16);
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == expected, "MVDD captures both addresses before one X-priority AR update at Access");
			}
		if (!f.repeated && tick == 4) for (unsigned i = 0; i < 4; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, 0x2222 + i);
		if (!f.repeated && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source[0], 0xdead); }
		if (f.prefix == 2 && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xffff); }
		// A pin edge queued in this callback is not CPU-owned save state.
		// RPT is decoded at clock 3; latch the event before any checkpoint.
		if (f.irq && tick == 3) { if (f.irq == 1) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1); else { m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); } }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads, &m_fast_stack_reads, &m_pipeline_register_writes, &m_vector_acks, &m_port_reads},
			"Save MVDD addresses, pending Read/write, issue count and deferred IRQ");
		if (tick == f.first_write() + f.count + 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x602 + f.prefix && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "MVDD resumes the exact caller page after any deferred interrupt return");
			if (m_pipeline_operand_reads != f.count + 1 || m_pipeline_store_writes != f.count + 1 || m_pipeline_extensions_reads != 1 || m_fast_stack_reads != unsigned(bool(f.irq)) || m_pipeline_register_writes != unsigned(bool(f.irq)) || m_vector_acks != unsigned(bool(f.irq)) || m_port_reads != unsigned(f.prefix == 2))
				osd_printf_info("mvdd_counts: case=%u replay=%u count=%u prefix=%u irq=%u reads=%u writes=%u fetches=%u irq_reads=%u irq_writes=%u acks=%u literals=%u\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), f.count, f.prefix, f.irq, m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_fast_stack_reads, m_pipeline_register_writes, m_vector_acks, m_port_reads);
			expect(m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.count + 1 && m_pipeline_extensions_reads == 1 && m_fast_stack_reads == unsigned(bool(f.irq)) && m_pipeline_register_writes == unsigned(bool(f.irq)) && m_vector_acks == unsigned(bool(f.irq)) && m_port_reads == unsigned(f.prefix == 2), "MVDD owns exact, nonduplicated copy, count and interrupt transactions");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1 & ~(f.irq ? 0x800 : 0)), "MVDD preserves accumulators, T and status apart from RETE's INTM clearance");
			for (unsigned i = 0; i < 4; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == (f.repeated ? f.final[i] : 0x2222 + i), "MVDD applies each pointer modification once with Xmod priority for a shared AR");
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.destination[f.count]) == f.word[f.count], "MVDD's final destination contains the retained word"); }
			if (f.repeated) expect(m_cpu->state_int(tms320c54x_device::STATE_RC) == 0, "Repeated MVDD drains its architectural repeat count");
			if (replay_pipeline_at(254150, checkpoint,
				"Restore MVDD without duplicating a read, write, pointer update or issue")) return true;
			bool const repeated = f.repeated; unsigned const variant = m_pipeline_peripheral_case;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (repeated ? 221183 : 49151)) { start_mvdd(variant + 1); return true; }
			if (repeated)
			{
				osd_printf_info("TMS320C54x repeated MVDD: PASS cases=221184 restore=221184 encodings=256 counts=0,1,15 forms=short,long layouts=distinct,equal,overlap bk=0,16 pages=2 snapshots=4 sources=none,IRQ,NMI read_before_older_write=1 same_ar_xmod_priority=1 opcode_fetches=1 literal_fetches=once issue_interval=1 interrupt_return=1 mapped_acceptance=0 memory_count_acceptance=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated repeated MVDD complete");
			}
			osd_printf_info("TMS320C54x staged MVDD: PASS cases=49152 restore=49152 encodings=256 layouts=distinct,equal,overlap values=4 bk=0,16 pages=2 snapshots=4 address_access=4 source_read=5 destination_write=6 same_ar_xmod_priority=1 captured_addresses=1 captured_source=1 reads=1 writes=1 mapped_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged MVDD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254150) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254140; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
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
			osd_printf_info("TMS320C54x POPD edges: PASS cases=31 restore=31 exclusions=15 dagen_conflicts=16 conflict_read=6 conflict_write=7 same_and_different_registers=1 active_controls_rejected=4 memory_count_rejected=1 source_capture=1 destination_capture=1 silicon_phase_claim=0\n"); start_extended_pop_edge(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254230) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254220; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mvmd_word_register_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254300)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const bk = profile_is("mvmd-bk"), producer = bk && variant >= 216;
		unsigned const v = bk ? variant % 216 : variant, sources = bk ? 1 : 2, prefix = producer ? 2 : 0;
		unsigned const source = bk ? 0x19 : 0x0e + v % sources, destination = (v / sources) % 9 < 8 ? 0x10 + (v / sources) % 9 : 0x19, checkpoint = 4 + prefix + v / (sources * 72);
		u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / (sources * 9)) % 4];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Register-source MVMD remains staged");
		if (tick == 5 + prefix) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(source, 0xdead); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save register-source MVMD across retained move ownership");
		if (tick == 12)
		{
			{ auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA); expect(data.read_word(source) == 0xdead && (bk ? data.read_word(0x0e) == 0x1357 && data.read_word(0x0f) == 0x2468 : data.read_word(source ^ 1) == (source == 0x0e ? 0x2468 : 0x1357)), "MVMD preserves the other source register and does not reread the changed source"); for (unsigned r = 0; r < 8; ++r) expect(data.read_word(0x10 + r) == (destination == 0x10 + r ? value : 0x1400 + r), "MVMD changes only the selected AR"); expect(data.read_word(0x19) == (bk ? 0xdead : destination == 0x19 ? value : 13), "MVMD preserves the selected BK publication and post-read source change"); }
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 + unsigned(producer) && m_cpu->state_int(tms320c54x_device::STATE_A) == (producer ? value : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900, "MVMD preserves accumulators, stack and status with owned source and destination accesses");
			if (replay_pipeline_at(254310, checkpoint,
				"Restore register-source MVMD without duplicate publication")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 431) { start_mvmd_word_register(variant + 1); return true; }
			if (bk) { osd_printf_info("TMS320C54x staged MVMD BK: PASS cases=432 restore=432 contexts=isolated,STLM_NOP destinations=AR0_to_AR7,BK values=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_retained=1 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated MVMD BK complete"); }
			osd_printf_info("TMS320C54x staged MVMD T/TRN: PASS cases=432 restore=432 sources=2 destinations=AR0_to_AR7,BK values=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 producer_composition=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated MVMD T/TRN complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254310) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254300; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_push_mvmm_results()
{
	if(m_phase==push_mvmm_phase)
	{
		unsigned const variant=m_pipeline_peripheral_case,tick=++m_idle_fixture_tick;auto const f=push_mvmm_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"PSHM and MVMM SP remain admitted");
		unsigned const allocated=tick<5?0:std::min(tick-4,f.pushes);
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP)==u16(f.sp-allocated),"MVMM SP forwarding does not move or duplicate PSHM's Read allocation");
		if(tick==f.read()) { auto const disable=machine().disable_side_effects();m_cpu->space(AS_PROGRAM).write_word(f.base+0x600+f.pushes+f.gap,0xffff); }
		if(tick==f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_register_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save pending stack allocation and retained MVMM SP sample");
		}
		if(tick==24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads==f.pushes+1 && m_pipeline_store_writes==f.pushes && m_pipeline_register_writes==1,"PSHM/MVMM performs no duplicate source, stack or register transactions");
			for(unsigned ar=0;ar<8;++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0+ar)==(ar==f.destination?f.copied():0x5000+ar),"Only the selected MVMM destination changes");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0)==0xa020 && m_cpu->state_int(tms320c54x_device::STATE_ST1)==0x800 && m_cpu->state_int(tms320c54x_device::STATE_A)==0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B)==0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T)==0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_TRN)==0x2468,"PSHM/MVMM preserves arithmetic and unrelated registers");
			if(!m_pipeline_replay)
			{
				m_pipeline_replay=true;m_idle_fixture_tick=f.checkpoint;m_pipeline_operand_reads=m_recurring_irq_snapshot[0];m_pipeline_store_writes=m_recurring_irq_snapshot[1];m_pipeline_register_writes=m_recurring_irq_snapshot[2];
				m_cpu->set_pipeline_frontend(true);return restore_pipeline_at(push_mvmm_phase+1, "Restore PSHM/MVMM after clearing the future pipeline");
			}
			m_pipeline_operand_tap.remove();m_pipeline_write_tap.remove();m_cpu->set_pipeline_frontend(false);
			if(variant<2303) { start_push_mvmm(variant+1);return true; }
			osd_printf_info("TMS320C54x PSHM MVMM SP: PASS cases=2304 restore=2304 pushes=1,2 last_sources=AR2,ST0,ST1,SP gaps=0,1,2 destinations=8 stack_wrap=1 pages=2 snapshots=3 source=Decode publication=Access stack_allocation=Read stack_write=Execute opcode_poison=1 duplicate_transactions=0 physical_phase_claim=0\n");
			throw emu_fatalerror(3,"TMS320C54x isolated PSHM MVMM SP complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));return true;
	}
	if(m_phase==push_mvmm_phase+1) { m_cpu->resume(SUSPEND_REASON_DISABLE);m_phase=push_mvmm_phase;m_check_timer->enable(true);m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));return true; }
	return false;
}

bool tms320c54x_test_state::check_t_move_results()
{
	if (m_phase == 254520)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = t_move_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "T/TRN register destinations use retained move execution");
		if (tick == f.read()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); }
		if (tick == f.consume()) m_cpu->set_state_int(f.destination ? tms320c54x_device::STATE_TRN : tms320c54x_device::STATE_T, 0xdead);
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save T/TRN move source, publication and consumer sample");
		if (tick == 24)
		{
			s64 const product = s64(s16(f.value)) * -32768 * (f.flags & 1 ? 2 : 1);
			bool const overflow = !f.destination && product > 0x7fffffffLL;
			u64 const result = f.destination ? f.value : u64(overflow && (f.flags & 2) ? 0x7fffffffLL : product) & 0xffffffffffULL;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 + f.destination && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == result && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.destination ? 0xbeef : 0xdead) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == (f.destination ? 0xdead : 0x2468) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa008 | (overflow ? 0x400 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0)), "Moves give following MPY/LDM the new retained word with independent arithmetic and status results");
			if (replay_pipeline_at(254530, f.checkpoint,
				"Replay T/TRN publication and consumer without duplicate bus accesses")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1919) { start_t_register_move(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged T/TRN moves: PASS cases=1920 restore=1920 forms=MVMD_AR,MVDK_direct,MVDK_short,MVDK_offset,MVDK_absolute destinations=T,TRN values=4 frct_ovm=4 gaps=0,1 pages=2 snapshots=3 source_reads=1 destination_writes=1 consumers=MPY,LDM silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated T/TRN moves complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254530) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254520; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254480)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delay_store_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "A complete long store fits the two-word control delay");
		if (tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601 + f.delay(), 0x1234); }
		if (tick == f.write_clock() - 1) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == f.next_ar, "Delay-store address modification occurs once before publication"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x6000); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_pipeline_extensions_reads, &m_pipeline_drained_fetches},
			"Save control redirect and partially decoded or addressed delay store");
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + (f.kind == 2 ? 0x605 : 0x701) && m_pipeline_operand_reads == unsigned(f.kind == 4) && m_pipeline_store_writes == 1 && m_pipeline_register_writes == unsigned(f.kind == 3) && m_pipeline_extensions_reads == 1 && m_pipeline_drained_fetches == unsigned(f.kind != 2) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.kind == 3 ? 0x2fff : f.kind == 4 ? 0x3001 : 0x3000) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.a && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900, "Delay store retires once before the correct control continuation without extra frames or status changes");
			if (replay_pipeline_at(254490, f.checkpoint,
				"Replay delay store without repeated literals, writes or control transfers")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 2879) { start_delay_store(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged long delay store: PASS cases=2880 restore=2880 controls=BD,BCD_taken,BCD_untaken,CALLD,RETD stores=STL_A,STL_B,STH_A,STH_B forms=offset,preincrement,circular,absolute literals=6 pages=2 snapshots=3 literal_fetches=1 destination_reads=0 store_writes=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated long delay store complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254490) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254480; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_move_brc_results()
{
	if (m_phase == 255620)
	{
		bool const control = profile_is("move-brc-delay");
		auto const f = move_brc_case(m_pipeline_peripheral_case, control);
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = f.read_tick - 1 + f.snapshot;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Memory move BRC and following block repeat remain staged");
		if (tick == f.read_tick)
		{
			expect(m_pipeline_register_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == f.count, "Zero-latency BRC value is visible before younger block setup");
			auto const disable = machine().disable_side_effects();
			if (f.source >= 0x60) m_cpu->space(AS_DATA).write_word(f.source, 0xdead);
			else if (f.source == 8) m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xdead);
			else m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0xdead);
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_pipeline_extensions_reads, &m_fast_stack_reads, &m_pipeline_drained_fetches},
			"Save before during and after BRC move publication");
		if (tick == 80 + f.count * 4)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.count + 1 && m_pipeline_register_writes == 1 && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.form == 2 ? 0x1401 : 0x1400) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x5001 + f.count && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x5101 + f.count && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x6000 + f.count, "Move-fed RPTB and RPTBD finish without stale counts or duplicate effects");
			if (control) expect(m_fast_stack_reads == unsigned(f.control_kind == 5) && m_pipeline_drained_fetches == (f.control_kind == 4 ? 2U : unsigned(f.control_kind == 3)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.control_kind == 4 ? 0x2ffe : f.control_kind == 3 ? 0x2fff : f.control_kind == 5 ? 0x3001 : 0x3000), "BRC move preserves delayed call and return stack ownership");
			if (replay_pipeline_at(255630, checkpoint,
				"Restore retained BRC source and block continuation")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < (control ? 1799U : 959U)) { start_move_brc(m_pipeline_peripheral_case + 1); return true; }
			if (control) { osd_printf_info("TMS320C54x delayed memory move BRC: PASS cases=1800 restore=1800 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD source_forms=DP,postincrement,MVDK_AR,MVDK_AL,MVMD_AL counts=0,1,2,7,127 rptb_rptbd=2 pages=2 snapshots=3 source_reads=1 counter_writes=1 source_retained=1 count_plus_one=1 stack=owned active_repeat_write_acceptance=0 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed memory move BRC complete"); }
			osd_printf_info("TMS320C54x staged memory move BRC: PASS cases=960 restore=960 source_forms=8 counts=0,1,2,7,127 gaps=0,1 rptb_rptbd=2 pages=2 snapshots=3 source_reads=1 counter_reads=0 counter_writes=1 latency=0 source_retained=1 count_plus_one=1 active_repeat_write_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated memory move BRC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255630) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255620; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}





bool tms320c54x_test_state::check_indexed_mar_results()
{
	if (m_phase == 254920)
	{
		bool const reverse = profile_is("mar-reverse");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = indexed_mar_case(variant, reverse);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (f.compatibility ? 0x20 : 0)), "Indexed MAR preserves accumulators T and ST1");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= 4 ? f.st0 : 0x8608), "Indexed MAR publishes only compatibility ARP at Access");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (tick > 4 ? 0x2200 + i : tick == 4 && i == f.selected ? f.result : f.initial[i]), "Indexed MAR updates the selected register once at Access using original AR0");
		if (tick == 4)
			expect_opcode((reverse ? f.add ? 0x6db8 : 0x6da0 : f.add ? 0x6db0 : 0x6da8) | f.ar,
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == f.result,
				"Exact indexed MAR opcode publishes the independently calculated AR update");
		if (tick == 4) for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x2200 + i);
		unsigned const checkpoint = 3 + f.snapshot;
		capture_pipeline_at(checkpoint, {&m_pipeline_extensions_reads},
			"Save indexed MAR before or after Access update");
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_extensions_reads == 1, "Indexed MAR completes without extension fetch or duplicate opcode");
			if (replay_pipeline_at(254930, checkpoint,
				"Restore indexed MAR without repeating its auxiliary update")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3071) { start_indexed_mar(variant + 1); return true; }
			if (reverse)
			{
				osd_printf_info("TMS320C54x staged reverse-carry MAR: PASS cases=3072 restore=3072 directions=2 auxiliary_registers=8 compatibility_states=2 power_of_two_indices=8 starts=2 pages=2 snapshots=3 publication=Access data_accesses=0 alias_AR0=1 duplicate_updates=0 independent_bit_serial_oracle=1 producer_latency=unvalidated silicon_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated reverse-carry MAR complete");
			}
			osd_printf_info("TMS320C54x staged indexed MAR: PASS cases=3072 restore=3072 directions=2 auxiliary_registers=8 compatibility_states=2 indices=8 starts=2 pages=2 snapshots=3 publication=Access data_accesses=0 alias_AR0=1 duplicate_updates=0 producer_latency=unvalidated silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated indexed MAR complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254930) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254920; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mapped_parallel_load_results()
{
	if (m_phase == 62164)
	{
		bool const multiply = profile_is("st-par-mpy");
		bool const bsp = profile_is("stld-bsp");
		bool const bus = profile_is("stld-periph"), peripheral = bsp || bus;
		bool const arithmetic = profile_is("st-par-alu");
		bool const mapped = profile_is("stld-map");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const projected = !mapped && !peripheral;
		unsigned const key = projected ? parallel_schedule::key(variant) : variant;
		auto const c = multiply ? parallel_multiply_case(key) : bus ? peripheral_parallel_load_case(variant) : bsp ? bsp_parallel_load_case(variant) : mapped ? mapped_parallel_load_case(variant) : arithmetic ? parallel_arithmetic_case(key) : parallel_load_case(key); auto const &f = c.memory;
		unsigned const checkpoint = 4 + f.snapshot;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Parallel operation remains admitted");
		if (tick == unsigned(peripheral ? 6 : 5))
		{
			if (mapped) expect(m_cpu->state_int(tms320c54x_device::STATE_IMR) == c.imr && m_cpu->state_int(tms320c54x_device::STATE_IFR) == c.ifr, "Parallel mapped reads do not acknowledge interrupt state");
			for (unsigned i = 0; i < 4; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == f.final[i], "Parallel load applies X priority and each dual-address update once");
			if (!arithmetic && c.form >= 4) expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.xword, "Parallel LD Xmem,T publishes the retained input in Read");
			if (multiply) expect(m_cpu->state_int(tms320c54x_device::STATE_T) == c.t, "Parallel MPY retains T without publishing a replacement");
			m_cpu->set_state_int(tms320c54x_device::STATE_A, c.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, c.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x55aa);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1 ^ 31);
			for (unsigned i = 0; i < 4; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, 0x2200 + i);
			auto const disable = machine().disable_side_effects();
			if (!peripheral && (!mapped || (f.xaddress >= 2 && f.xaddress <= 5) || f.xaddress == 0x1f)) m_cpu->space(AS_DATA).write_word(f.xaddress, f.xword ^ 0xffff);
			if (mapped) { m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x0f0f); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xf0f0); }
		}
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save parallel load around address capture Read and Execute"); }
		if (tick == 12)
		{
			if (bus) expect(m_shifted_bus_word == u16(f.xword ^ 0xffff), "Parallel peripheral load does not resample its volatile source");
			if (bsp) expect(m_shifted_bsp.receive_ready && !m_shifted_bsp.receive_overrun && m_shifted_bsp.receive == u16(f.xword ^ 0xffff), "Parallel load leaves the next BSP frame unread after completion and restore");
			u64 const a = c.result_a, b = c.result_b;
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != a || m_cpu->state_int(tms320c54x_device::STATE_B) != b) osd_printf_info("STLD result mismatch: case=%u form=%u a=%010llx expected=%010llx b=%010llx expected=%010llx\n", variant, c.form, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)a, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)b);
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x55aa, "Parallel store/load separates Read-owned input and T from Execute-owned old accumulator and destination");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((arithmetic || multiply) ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1 ^ 31) && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Parallel operation preserves modes and publishes only its specified arithmetic status");
			for (unsigned i = 0; i < 4; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == (0x2200 + i), "No pointer update is repeated after the retained address is consumed");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Parallel store/load completes before IDLE");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; if (arithmetic || mapped || peripheral || multiply) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(62165, "Replay parallel store/load without duplicate samples publications or address effects"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (projected) m_parallel_coverage.add(key);
			if (variant + 1 < (multiply ? 139264U : bus ? 393216U : bsp ? 6144U : mapped ? 147456U : arithmetic ? 278528U : 208896U)) { start_parallel_load(variant + 1); return true; }
			if (multiply)
			{
				osd_printf_info("TMS320C54x parallel multiply: PASS cases=%u restore=%u forms=4 address_cases=73728 numeric_cases=65536 encodings=1024 asm_modes=32 pages=2 address_snapshots=3 numeric_snapshot_policy=balanced source_sample=Read t_sample=Read store_source=Execute asm_source=Read old_source_store=1 smul_product_clamp=0 pointer_updates=once reset_before_restore=1 carry_policy=preserve_unvalidated physical_phase_claim=0 coverage_fnv64=%016llx\n", m_parallel_coverage.count, m_parallel_coverage.count, (unsigned long long)m_parallel_coverage.value); throw emu_fatalerror(3, "TMS320C54x isolated parallel multiply complete");
			}
			if (bus) { osd_printf_info("TMS320C54x parallel peripheral load: PASS cases=393216 restore=393216 forms=6 addresses=0020..005f selector_pairs=8 patterns=2 sxm_sst_ovm=8 pages=2 snapshots=4 extra_clock=1 reads=1 writes=1 volatile_source=1 store_source=Execute t_publish=Read reset_before_restore=1 carry_policy=preserve_unvalidated endpoint_identity_claim=0 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated parallel peripheral load complete"); }
			if (bsp) { osd_printf_info("TMS320C54x parallel BSP load: PASS cases=6144 restore=6144 forms=6 selector_pairs=8 patterns=2 sxm_sst_ovm=8 pages=2 snapshots=4 extra_clock=1 reads=1 writes=1 ready_overrun_ack=1 next_frame_pending=1 store_source=Execute t_publish=Read reset_before_restore=1 carry_policy=preserve_unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated parallel BSP load complete"); }
			if (mapped) { osd_printf_info("TMS320C54x mapped parallel load: PASS cases=147456 restore=147456 forms=6 source_addresses=0000..001f destination=RAM selector_pairs=8 patterns=2 sxm_sst_ovm=8 asm=0,-1 pages=2 snapshots=3 source_sample=Read store_source=Execute reads=1 writes=1 pointer_updates=once t_publish=Read carry_policy=preserve_unvalidated reset_before_restore=1 repeat_peripheral_composition=unvalidated endpoint_identity_claim=0 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped parallel load complete"); }
			if (arithmetic)
			{
				osd_printf_info("TMS320C54x parallel arithmetic: PASS cases=%u restore=%u forms=8 address_cases=147456 numeric_cases=131072 pages=2 address_snapshots=3 numeric_snapshot_policy=balanced read_retained=1 asm_sample=Read accumulator_sample=Execute old_source_store=1 subtraction=memory_minus_other carry_bit=32 sticky_overflow=1 ovm=1 saturation=1 pointer_updates=once reset_before_restore=1 physical_phase_claim=0 coverage_fnv64=%016llx\n", m_parallel_coverage.count, m_parallel_coverage.count, (unsigned long long)m_parallel_coverage.value); throw emu_fatalerror(3, "TMS320C54x isolated parallel arithmetic complete");
			}
			osd_printf_info("TMS320C54x parallel store/load: PASS cases=%u restore=%u forms=6 encodings=1536 address_cases=110592 numeric_cases=98304 words=8 patterns=8 asm_modes=32 pages=2 address_snapshots=3 numeric_snapshot_policy=balanced read_retained=1 store_source=Execute asm_source=Read t_publication=Read alias_priority=X carry_policy=preserve_unvalidated physical_phase_claim=0 coverage_fnv64=%016llx\n", m_parallel_coverage.count, m_parallel_coverage.count, (unsigned long long)m_parallel_coverage.value); throw emu_fatalerror(3, "TMS320C54x isolated parallel store/load complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62165) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62164; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62162)
	{
		bool const edge = profile_is("stack-page0-edge");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = page_zero_stack_case(variant, edge);
		unsigned const checkpoint = edge ? f.snapshot ? 7 : 4 : std::array<unsigned, 3>{4, 6, 9}[f.snapshot];
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (edge && tick >= 3), "Page-zero stack admission excludes peripherals and indirect encodings before source access");
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save page-zero stack source destination and pending publication");
		if (tick == 16)
		{
			unsigned const transactions = edge ? 0 : f.form >= 2 ? 2 : 1;
			u16 const sp = edge ? f.sp : f.form == 0 ? f.sp - 1 : f.form == 1 ? f.sp + 1 : f.form == 5 ? f.sp - 2 : f.sp;
			expect(m_pipeline_operand_reads == transactions && m_pipeline_store_writes == transactions && m_cpu->state_int(tms320c54x_device::STATE_SP) == sp, "Every stack operation owns one source one destination and one SP update");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "Direct page-zero stack addressing ignores DP and CPL and preserves unrelated registers");
			if (!edge) expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "All ordered stack publications drain before IDLE");
			if (replay_pipeline_at(62163, checkpoint,
				"Replay stack slots without duplicate source reads writes or SP updates")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < (edge ? 1536U : 73728U)) { start_page_zero_stack(variant + 1); return true; }
			if (edge) { osd_printf_info("TMS320C54x page-zero stack exclusions: PASS cases=1536 restore=1536 forms=PSHM,POPM encodings=192 pages=2 snapshots=2 data_reads=0 data_writes=0 stack_allocation=0 silicon_trap_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated page-zero stack exclusions complete"); }
			osd_printf_info("TMS320C54x page-zero stack: PASS cases=73728 restore=73728 addresses=32 words=8 stacks=4 cpl=2 pages=2 snapshots=3 forms=push,pop,pair0,pair1,pair3,double_push source_retained=1 publications=once sp_updates=once status_preserved=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated page-zero stack complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62163) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62162; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
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

bool tms320c54x_test_state::check_xmem_load_results()
{
	if (m_phase == 254840)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = 3 + (variant % 196608) / 65536;
		auto const f = xmem_load_case(variant % 196608);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Compact Xmem store stays in the retained frontend");
		if (tick == 4 || tick == 5)
		{
			for (unsigned ar = 2; ar <= 5; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (tick == 5 && ar == 2 + (f.x & 3) ? f.next_ar() : 0x100f + 0x100 * (ar - 2)), "Xmem store publishes its one pointer update with the Read-owned EAB");
			if (tick == 5)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + 2 + (f.x & 3), 0x2222);
				m_cpu->set_state_int(f.destination ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, xmem_store_source(variant));
			}
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save Xmem store before address sampling and after retained EAB");
		if (tick == 12)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.address()) == xmem_store_expected(variant), "Xmem store publishes the shifted/saturated word in RAM"); }
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination ? 0x123456789aULL : xmem_store_source(variant)) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination ? xmem_store_source(variant) : 0xfeabcd9876ULL) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + 2 + (f.x & 3)) == 0x2222, "Xmem store preserves both accumulators and never reapplies its pointer update");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0() && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1() && m_cpu->state_int(tms320c54x_device::STATE_PMST) == (variant / 393216) % 2 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && !m_pipeline_operand_reads && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 1, "Xmem store preserves status/T with one fetch/write and no destination read");
			if (replay_pipeline_at(254850, checkpoint,
				"Restore Xmem store without duplicate writes, fetches or address updates")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 786431) { start_xmem_store(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged Xmem stores: PASS cases=786432 restore=786432 encodings=1024 operations=STL,STH xmem_modes=16 shifts=0..15 values=8 accumulators=2 status_states=16 pages=2 snapshots=3 eab=Read eb=Execute destination_reads=0 data_writes=1 sst_sxm=1 ovm_independent=1 bk_zero_and_wrap=1 mapped_acceptance=0 repeat_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated Xmem stores complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254850) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254840; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254120)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = 3 + (variant % 196608) / 65536; auto const f = xmem_load_case(variant % 196608);
		bool const arithmetic = profile_is("xmem-alu");
		auto const a = xmem_arithmetic_expected(variant);
		u64 const result = arithmetic ? a.result : f.result();
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Xmem LD stays in the staged frontend");
		if (tick == 5 || tick == 6) expect(m_cpu->state_int(f.destination ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (tick == 5 ? arithmetic ? a.source : (f.destination ? 0xfeabcd9876ULL : 0x123456789aULL) : result), "Xmem result publishes at Execute, not Read");
		if (tick == 4)
		{
			for (unsigned ar = 2; ar <= 5; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == 2 + (f.x & 3) ? f.next_ar() : 0x100f + 0x100 * (ar - 2)), "Xmem LD modifies only the selected pointer once at Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + 2 + (f.x & 3), 0x2222);
		}
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address(), 0xdead); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save Xmem LD at Decode, captured address or retained Read");
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination ? 0x123456789aULL : result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination ? result : 0xfeabcd9876ULL), "Xmem shifts its retained signed/unsigned source and preserves the other accumulator");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (arithmetic ? a.st0 : f.st0()) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1() && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_pipeline_operand_reads == 1 && !m_pipeline_store_writes && m_pipeline_extensions_reads == 1, "Xmem arithmetic updates carry/sticky overflow and preserves other status and T");
			if (replay_pipeline_at(254130, checkpoint,
				"Restore Xmem LD without rereading its source or repeating its pointer update")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (arithmetic ? 393215 : 196607)) { start_xmem_load(variant + 1); return true; }
			if (arithmetic)
			{
				osd_printf_info("TMS320C54x staged Xmem arithmetic: PASS cases=393216 restore=393216 encodings=1024 operations=ADD,SUB xmem_modes=16 shifts=0..15 values=8 accumulators=2 status_states=8 pages=2 snapshots=3 read=5 publish=6 pointer_access=4 captured_source=1 bk_zero_and_wrap=1 data_writes=0 mapped_acceptance=0 repeat_acceptance=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated Xmem arithmetic complete");
			}
			osd_printf_info("TMS320C54x staged Xmem LD: PASS cases=196608 restore=196608 encodings=512 xmem_modes=16 shifts=0..15 values=8 accumulators=2 status_states=8 pages=2 snapshots=3 source_read=5 result_publish=6 pointer_access=4 captured_source=1 bk_zero_and_wrap=1 reads=1 data_writes=0 mapped_acceptance=0 repeat_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated Xmem LD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254130) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254120; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_repeat_push_results()
{
	if (m_phase == 254070)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = repeat_push_case(variant);
		unsigned const checkpoint = std::array<unsigned, 4>{4, f.first_write() - 1, f.first_write(), f.first_write() + f.count}[variant / 192];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeated PSHD stays in the staged frontend");
		if (tick == 5 && f.interrupt) { if (f.interrupt == 2) m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); else m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads, &m_fast_stack_reads, &m_pipeline_register_writes, &m_vector_acks},
			"Save issued and in-flight repeated pushes and pending IRQ/NMI");
		if (tick == f.first_write() + f.count + 32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0xfff - f.count && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0, "Repeated PSHD completes and restores the interrupt frame above its caller-owned pushes");
			expect(m_cpu->state_int(STATE_GENPC) == f.base + (f.long_form ? 0x604 : 0x603), "Repeated PSHD resumes the exact caller continuation and program page");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR7) == u16(f.kind == 1 || f.kind == 3 ? f.first_source() + f.count + 1 : f.first_source() - f.count - 1), "Repeated PSHD modifies AR7 once per iteration");
			expect(m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.count + 1 && m_pipeline_extensions_reads == 1 && m_vector_acks == unsigned(bool(f.interrupt)) && m_fast_stack_reads == (f.interrupt ? 2 : 0) && m_pipeline_register_writes == (f.interrupt ? 2 : 0), "Repeated PSHD and interrupt return own exact, nonduplicated transactions");
			if (replay_pipeline_at(254080, checkpoint,
				"Restore repeated PSHD without reissuing completed or in-flight pushes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_repeat_push(variant + 1); return true; }
			start_push_exclusion(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254080) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254070; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254090)
	{
		if (++m_idle_fixture_tick == 8)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_AR7) == (m_pipeline_peripheral_case ? 0x17 : 0x18) && !m_pipeline_operand_reads && !m_pipeline_store_writes, "PSHD SP and modifying self-AR aliases fail before allocation, mutation or data access");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (!m_pipeline_peripheral_case) { start_push_exclusion(true); return true; }
			osd_printf_info("TMS320C54x repeated PSHD: PASS cases=768 restore=768 counts=0,1,4,11 forms=short,long sources=RAM_up,RAM_down,MMR_up,MMR_down pages=2 interrupts=none,IRQ,NMI snapshots=4 reads_each=1 writes_each=1 issue_interval=1 opcode_fetches=1 pending_irq_deferred=1 isr_return=1 excluded_aliases=2 memory_count_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeated PSHD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254050)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		u16 const source = std::array<u16, 12>{0, 1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}[variant % 12];
		u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 12) % 4];
		u16 const expected = source <= 1 ? 0 : source == 6 ? 0xa600 : source == 7 ? 0x800 : (source == 10 || source == 13) ? value & 0xff : value;
		unsigned const checkpoint = std::array<unsigned, 3>{4, 5, 6}[variant / 96];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped PSHD executes without atomic fallback");
		if (tick == 4) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0xfff, "PSHD allocates the retained stack address in Access");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			// Change control registers safely: INTM stays set and no enabled IFR is introduced.
			if (source == 1) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xffff);
			else if (source == 6) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa601);
			else if (source == 7) m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
			else m_cpu->space(AS_DATA).write_word(source, expected ^ 0xffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x2222);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, 0, 0};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped PSHD around Access, Read and Execute");
		}
		if (tick == 12)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(0xfff) == expected, "Mapped PSHD retains the register word after source and pointer mutation"); }
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0xfff && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1, "Mapped PSHD completes one allocation, read and write");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(254060, "Restore mapped PSHD without rereading its source or duplicating its push");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 287) { start_mapped_pshd(variant + 1); return true; }
			osd_printf_info("TMS320C54x mapped PSHD: PASS cases=288 restore=288 registers=12 patterns=4 pages=2 snapshots=3 address_access=4 source_read=5 stack_write=6 reads=1 writes=1 source_retained=1 repeat_coverage=0 dagen_source_coverage=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated mapped PSHD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254060) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254050; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62066)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = reada_brc_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Inactive BRC accepts the owned READA transfer");
		if (tick == 5 && f.form == 2) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(0x601, 0x1b); }
		if (tick == 7) m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x3400030480ULL);
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.physical, f.value ^ 0xffff); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_register_writes, &m_pipeline_store_writes, &m_pipeline_extensions_reads, &m_fast_stack_reads},
			"Save READA BRC through PAR capture, program hold, publication and following loop");
		if (tick == 96)
		{
			unsigned const writes = f.loop ? f.value + 1 : 0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_register_writes == 1 && m_pipeline_store_writes == writes &&
				m_pipeline_extensions_reads == unsigned(f.form == 2) && !m_fast_stack_reads && m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.loop ? 0 : f.value), "READA count publication and spaced loop have exact once-only effects");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x3400030480ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.form == 1 ? 0x10 : 0x3000), "READA BRC preserves accumulators, status and compiler-mode SP");
			for (unsigned i = 0; i < 8; ++i)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (i == f.body_reg ? 0x1400 + writes : f.form >= 3 && i == f.reg ? 0x1a + (f.form == 4 ? 1 : f.form == 5 ? -1 : 0) : 0x1500 + i), "READA applies only its owned pointer update and the subsequent loop's updates");
			if (replay_pipeline_at(62067, f.checkpoint,
				"Restore READA BRC without retargeting PAR, rereading poisoned source or duplicating count writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 18431) { start_reada_brc(variant + 1); return true; }
			osd_printf_info("TMS320C54x READA BRC: PASS cases=18432 restore=18432 forms=DP,SP,absolute,plain,postinc,postdec registers=8 standalone_values=0,1,8000,ffff loop_counts=0,1,2,7 gaps=1,2,3 source_pages=2 ovly=2 snapshots=4 address_bits=%u program_reads=1 brc_writes=1 data_reads=0 retained_par=1 retained_word=1 source_poison=1 silicon_phase_claim=0\n", standard_reada_brc() ? 16 : 23);
			throw emu_fatalerror(0, "TMS320C54x isolated READA BRC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62067) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62066; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}


} // namespace c54x_test
