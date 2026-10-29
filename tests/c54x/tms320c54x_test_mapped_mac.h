// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_MAPPED_MAC_H
#define MAME_NOKIA_TMS320C54X_TEST_MAPPED_MAC_H

#include "tms320c54x_test_case.h"
#include "tms320c54x_test_memory_immediate_schedule.h"
#include "tms320c54x_test_snapshot.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <algorithm>
#include <array>

namespace c54x_test {

// CPU-only mapped-source MAC retention contract. Host observations are rewound
// explicitly after guest restoration; no phase IDs or handset state participate.
class mapped_mac_fixture
{
public:
	mapped_mac_fixture(running_machine &machine, tms320c54x_device &cpu, emu_timer &timer)
		: m_machine(machine), m_cpu(cpu), m_timer(timer) { }
	void start() { m_variant = m_restores = 0; m_coverage = {}; begin(); }
	unsigned count() const { return m_coverage.count; }
	unsigned restores() const { return m_restores; }
	u64 digest() const { return m_coverage.value; }
	bool step()
	{
		if (m_resume) { m_resume = false; m_cpu.resume(SUSPEND_REASON_DISABLE); schedule(); return false; }
		auto const &f = m_case;
		unsigned const tick = ++m_tick;
		require(!state(r::STATE_ILLEGAL) && state(r::STATE_PMST) == f.pmst && state(r::STATE_ST1) == f.st1,
			"preserve execution modes without fallback");
		u64 const a = tick > f.read ? (f.bdst ? f.late_a() : f.result) : f.a;
		u64 const b = tick > f.read ? (f.bdst ? f.result : f.late_b()) : f.b;
		require(state(r::STATE_A) == a && state(r::STATE_B) == b && state(r::STATE_ST0) == (tick > f.read ? f.result_st0 : f.st0),
			"combine retained mapped word/literal with Execute-live accumulator and publish sticky overflow at Execute");
		require(state(r::STATE_T) == (tick > f.read ? 0x5a5a : tick == f.read || f.address == 0x0e ? f.word : 0x9abc),
			"publish T at Read without overwriting a younger value");
		if (tick == 3 || (f.words == 3 && tick == 4))
		{
			auto const disable = m_machine.disable_side_effects();
			m_cpu.space(AS_PROGRAM).write_word(f.base + 0x5fe + tick, 0xf495);
		}
		if (tick == f.access) m_cpu.set_state_int(r::STATE_AR0 + f.pointer, 0x7000 + f.pointer);
		if (tick == f.read)
		{
			set_registers(m_cpu, {{r::STATE_A, f.late_a()}, {r::STATE_B, f.late_b()}, {r::STATE_T, 0x5a5a}, {r::STATE_TRN, 0xdead}});
			for (unsigned i = 0; i < 8; ++i) m_cpu.set_state_int(r::STATE_AR0 + i, 0x7000 + i);
		}
		if (tick == f.checkpoint && !m_replay)
		{
			m_saved_counts = {m_reads, m_extensions};
			require(capture_snapshot(m_machine, m_saved) == STATERR_NONE, "save literal/Read/Execute ownership");
		}
		if (tick != 16) { schedule(); return false; }
		require(state(r::STATE_IDLE) && state(STATE_GENPC) == f.base + 0x601 + f.words && m_reads == 1 &&
			m_extensions == f.words - 1 && state(r::STATE_TRN) == 0xdead, "retain address/literal with one read and no bus write");
		for (unsigned i = 0; i < 8; ++i) require(state(r::STATE_AR0 + i) == 0x7000 + i, "never recompute a poisoned pointer");
		if (!m_replay)
		{
			m_replay = true; m_tick = f.checkpoint; m_reads = m_saved_counts[0]; m_extensions = m_saved_counts[1];
			m_cpu.set_pipeline_frontend(true);
			require(restore_snapshot(m_machine, m_timer, m_saved) == STATERR_NONE, "restore after clearing future pipeline state");
			++m_restores;
			m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_resume = true; m_timer.enable(true);
			return false;
		}
		clear_taps(); m_cpu.set_pipeline_frontend(false);
		m_coverage.add(mapped_mac_key(m_variant));
		if (++m_variant < mapped_mac_inputs) { begin(); return false; }
		require(count() == mapped_mac_inputs && restores() == mapped_mac_inputs, "complete ordered cases and actual restores");
		return true;
	}
private:
	using r = tms320c54x_device;
	struct input
	{
		unsigned form, pattern, pointer, words, access, read, checkpoint;
		u16 address, word, literal, mode, extension, st0, st1, pmst, result_st0;
		u32 base;
		bool bdst, bsrc;
		u64 a, b, result;
		u64 late_a() const { return std::array<u64, 4>{0, 1, 0x7fffffff, 0xff80000000ULL}[pattern]; }
		u64 late_b() const { return std::array<u64, 4>{0xffffffffffULL, 0x0080000000ULL, 0xff80000001ULL, 0x123456789aULL}[pattern]; }
	};
	static input select(unsigned ordinal)
	{
		unsigned v = mapped_mac_key(ordinal);
		input f{};
		f.address = 8 + v % 16; v /= 16; f.form = v % 4; v /= 4; f.pattern = v % 4; v /= 4;
		f.word = std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[f.pattern];
		f.literal = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0x012c, 0xf495, 0xf38b}[v % 8]; v /= 8;
		unsigned const flags = v % 16; v /= 16; unsigned const selectors = v % 4; v /= 4;
		f.bdst = selectors & 1; f.bsrc = selectors & 2; f.base = v % 2 ? 0x20000 : 0; v /= 2;
		f.pointer = f.address == 0x17 ? 6 : 7;
		f.mode = f.form == 0 ? f.address : f.form == 1 ? 0x80 + f.pointer : f.form == 2 ? 0xf8 : 0xe0 + f.pointer;
		f.words = f.form >= 2 ? 3 : 2; f.access = f.words == 3 ? 6 : 5; f.read = f.access + 1;
		f.extension = f.form == 2 ? f.address : 0;
		f.checkpoint = std::array<unsigned, 3>{3, f.read, f.read + 1}[v];
		f.st0 = 0x1000 | (f.pattern & 1 ? 0x800 : 0);
		f.st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 4 ? 0x100 : 0); f.pmst = flags & 8 ? 2 : 0;
		f.a = 0x123456789aULL; f.b = 0xfedcba9876ULL;
		if (f.address == 0x0a || f.address == 0x0d) f.word &= 0xff;
		if (f.address <= 0x0d)
		{
			u64 &acc = f.address >= 0x0b ? f.b : f.a;
			unsigned const shift = ((f.address - 8) % 3) * 16;
			u64 const mask = u64(shift == 32 ? 0xff : 0xffff) << shift;
			acc = (acc & ~mask) | (u64(f.word) << shift);
		}
		// Independent signed-integer reference, unchanged from the legacy
		// fixture: clamp SMUL product before the Execute-live addend.
		s64 product = s64(s16(f.word)) * s16(f.literal) * (flags & 1 ? 2 : 1);
		if ((flags & 11) == 11) product = std::clamp<s64>(product, -2147483648LL, 2147483647LL);
		u64 const source = f.bsrc ? f.late_b() : f.late_a();
		s64 const total = product + (source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source));
		bool const overflow = total < -2147483648LL || total > 2147483647LL;
		f.result = u64(overflow && (flags & 2) ? std::clamp<s64>(total, -2147483648LL, 2147483647LL) : total) & 0xffffffffffULL;
		f.result_st0 = f.st0 | (overflow ? f.bdst ? 0x200 : 0x400 : 0);
		return f;
	}
	u64 state(int index) const { return m_cpu.state_int(index); }
	void require(bool condition, const char *message) const
	{
		if (!condition) throw emu_fatalerror("C54x mapped MAC case=%u tick=%u replay=%u: %s", m_variant, m_tick, unsigned(m_replay), message);
	}
	void schedule() { m_timer.adjust(attotime::from_ticks(1, m_cpu.clock())); }
	void clear_taps() { m_read_tap.remove(); m_write_tap.remove(); m_fetch_tap.remove(); }
	void begin()
	{
		clear_taps(); m_cpu.reset(); m_cpu.suspend(SUSPEND_REASON_DISABLE, true);
		m_case = select(m_variant); auto const &f = m_case;
		auto &program = m_cpu.space(AS_PROGRAM); auto &data = m_cpu.space(AS_DATA);
		for (unsigned i = 0; i < 12; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
		program.write_word(f.base + 0x600, 0x6400 | (f.bsrc ? 0x200 : 0) | (f.bdst ? 0x100 : 0) | f.mode);
		program.write_word(f.base + 0x601, f.words == 3 ? f.extension : f.literal);
		if (f.words == 3) program.write_word(f.base + 0x602, f.literal);
		program.write_word(f.base + 0x600 + f.words, 0xf4e1);
		set_registers(m_cpu, {{r::STATE_IDLE, 0}, {r::STATE_ILLEGAL, 0}, {r::STATE_PMST, f.pmst}, {r::STATE_ST0, f.st0},
			{r::STATE_ST1, f.st1}, {r::STATE_TCR, 0x10}, {r::STATE_IMR, 0}, {r::STATE_IFR, 0},
			{r::STATE_A, f.a}, {r::STATE_B, f.b}, {r::STATE_T, 0x9abc}, {r::STATE_TRN, 0x7bad}});
		for (unsigned i = 0; i < 8; ++i) m_cpu.set_state_int(r::STATE_AR0 + i, 0x4000 + i);
		m_cpu.set_state_int(r::STATE_AR0 + f.pointer, f.address); data.write_word(f.address, f.word);
		m_reads = m_extensions = m_tick = 0; m_replay = m_resume = false;
		m_read_tap = data.install_read_tap(0, 0xffff, "mapped_mac_read", [this, f](offs_t at, u16 &word, u16) {
			if (m_machine.side_effects_disabled()) return;
			require(at == f.address && word == f.word && !m_reads++ && m_cpu.pipeline_cycles() == f.read, "sample original register field once at Read");
		});
		m_write_tap = data.install_write_tap(0, 0xffff, "mapped_mac_no_write", [this](offs_t, u16 &, u16) {
			if (!m_machine.side_effects_disabled()) require(false, "no data-bus write");
		});
		m_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x600 + f.words - 1, "mapped_mac_literals", [this, f](offs_t at, u16 &word, u16) {
			if (m_machine.side_effects_disabled()) return;
			unsigned const i = m_extensions++;
			require(i < f.words - 1 && at == f.base + 0x601 + i && word == (f.words == 3 && !i ? f.extension : f.literal), "retain extensions in order exactly once");
		});
		m_cpu.set_state_int(STATE_GENPC, f.base + 0x600); m_cpu.set_pipeline_frontend(true);
		m_cpu.resume(SUSPEND_REASON_DISABLE); schedule();
	}
	running_machine &m_machine;
	tms320c54x_device &m_cpu;
	emu_timer &m_timer;
	input m_case{};
	unsigned m_variant = 0, m_tick = 0, m_restores = 0, m_reads = 0, m_extensions = 0;
	bool m_replay = false, m_resume = false;
	std::array<unsigned, 2> m_saved_counts{};
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
	memory_passthrough_handler m_read_tap, m_write_tap, m_fetch_tap;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_MAPPED_MAC_H
