// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DUAL_H
#define MAME_NOKIA_TMS320C54X_TEST_DUAL_H

#include "tms320c54x_test_dual_input.h"
#include "tms320c54x_test_dual_schedule.h"
#include "tms320c54x_test_snapshot.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <string_view>

namespace c54x_test {

struct dual_multiply_mode
{
	enum class operation { none, multiply, arithmetic, unsigned_mac, mac, parallel_mac };
	operation op = operation::none;
	bool edge = false;
	explicit operator bool() const { return op != operation::none; }
	bool alu() const { return op == operation::arithmetic; }
	bool parallel() const { return op == operation::parallel_mac; }
	bool family() const { return op == operation::mac || parallel(); }
	bool macsu() const { return family() || op == operation::unsigned_mac; }
	unsigned count() const { return family() && !edge ? dual_mac_schedule::cases : (edge ? 16U : macsu() ? 215040U : 43008U) * (family() ? 8U : alu() ? 2U : 1U); }
	static dual_multiply_mode select(std::string_view name)
	{
		bool const edge = name.size() >= 5 && name.substr(name.size() - 5) == "-edge";
		if (edge) name.remove_suffix(5);
		operation const op = name == "mpy-dual" ? operation::multiply : name == "alu-dual" ? operation::arithmetic :
			name == "macsu-dual" ? operation::unsigned_mac : name == "mac-family" ? operation::mac :
			name == "stmac" ? operation::parallel_mac : operation::none;
		return {op, edge};
	}
};

// Synthetic CPU-only RAM fixture. Original per-clock mutations and assertions
// remain explicit; the host continuation survives guest snapshot restoration.
class dual_multiply_fixture_runner
{
public:
	dual_multiply_fixture_runner(running_machine &machine, tms320c54x_device &cpu, emu_timer &timer, dual_multiply_mode mode)
		: m_machine(machine), m_cpu(cpu), m_timer(timer), m_mode(mode) { }
	void start() { m_ordinal = m_restores = 0; m_coverage = {}; begin(); }
	bool step()
	{
		if (m_resume) { m_resume = false; m_cpu.resume(SUSPEND_REASON_DISABLE); schedule(); return false; }
		bool const alu = m_mode.alu(), parallel = m_mode.parallel(), family = m_mode.family(), macsu = m_mode.macsu(), edge = m_mode.edge;
		auto const &f = m_input;
		unsigned const variant = m_raw, tick = ++m_tick;
		unsigned const feedback = parallel ? f.destination : f.source;
		unsigned const checkpoint = 3 + f.snapshot;
		require(bool(state(r::STATE_ILLEGAL)) == (edge && tick >= 4), "Dual MPY admits RAM pairs and rejects low-page operands before effects");
		u64 const before = dual_macsu_seed(family ? variant % (edge ? 16U : 215040U) : variant, edge) ^ (!edge && tick <= 5 ? 0xffffffffffULL : 0);
		require(state(r::STATE_A) == (!edge && tick >= 6 && !f.destination ? f.result : macsu && !feedback ? before : 0x123456789aULL) && state(r::STATE_B) == (!edge && tick >= 6 && f.destination ? f.result : macsu && feedback ? before : 0xfeabcd9876ULL) && state(r::STATE_ST0) == (!edge && tick >= 6 ? f.result_st0 : f.st0), "Dual multiply publishes its Execute-owned result and sticky destination overflow");
		if (tick == 4)
		{
			for (unsigned i = 0; i < 4; ++i) { require(state(r::STATE_AR2 + i) == f.final[i], "Dual MPY captures both addresses before one X-priority alias update"); if (!edge) m_cpu.set_state_int(r::STATE_AR2 + i, 0x2200 + i); }
			if (!edge) m_cpu.set_state_int(r::STATE_BK, 32);
		}
		if (!edge && tick == 5)
		{
			require(state(r::STATE_T) == (parallel ? f.yword : alu ? 0xabcd : f.xword), "Dual memory Read publishes T only for dual-read multiply");
			m_cpu.set_state_int(r::STATE_T, 0xdead);
			if (macsu) m_cpu.set_state_int(feedback ? r::STATE_B : r::STATE_A, dual_macsu_seed(family ? variant % (edge ? 16U : 215040U) : variant, edge));
			if (parallel) m_cpu.set_state_int(r::STATE_ST1, f.st1 ^ 31);
			auto const disable = m_machine.disable_side_effects();
			m_cpu.space(AS_DATA).write_word(f.xaddress, 0xdead); m_cpu.space(AS_DATA).write_word(f.yaddress, 0xbeef);
		}
		if (tick == checkpoint && !m_replay)
		{
			m_saved_counts = {m_reads, m_fetches, m_writes}; require(capture_snapshot(m_machine, m_saved) == STATERR_NONE, "Save dual MPY around address, paired Read and T publication");
		}
		if (tick == 12)
		{
			require(m_reads == (edge ? 0U : parallel ? 1U : 2U) && m_writes == unsigned(parallel && !edge) && m_fetches == 1 && bool(state(r::STATE_IDLE)) == !edge && state(r::STATE_T) == (edge ? parallel ? f.yword : 0xabcd : 0xdead) && state(r::STATE_ST1) == (parallel && !edge ? f.st1 ^ 31 : f.st1) && state(r::STATE_PMST) == f.pmst, "Dual multiply retains operands and ASM without duplicate publications");
			for (unsigned i = 0; i < 4; ++i) require(state(r::STATE_AR2 + i) == (edge ? f.initial[i] : 0x2200 + i), "Dual MPY never reapplies its address modification");
			if (!m_replay)
			{
				m_replay = true; m_tick = checkpoint; m_reads = m_saved_counts[0]; m_fetches = m_saved_counts[1]; m_writes = m_saved_counts[2];
				require(restore_snapshot(m_machine, m_timer, m_saved) == STATERR_NONE, "Restore dual MPY without resampling data, refetching or duplicate pointer updates");
				++m_restores;
				m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_resume = true; m_timer.enable(true); return false;
			}
			m_read_tap.remove(); m_write_tap.remove(); m_fetch_tap.remove(); m_cpu.set_pipeline_frontend(false);
			if (++m_ordinal < m_mode.count()) { begin(); return false; }
			return true;
		}
		schedule(); return false;
	}
	unsigned count() const { return m_ordinal; }
	unsigned restores() const { return m_restores; }
	u64 digest() const { return m_coverage.value; }
private:
	using r = tms320c54x_device;
	u64 state(int index) const { return m_cpu.state_int(index); }
	void require(bool condition, const char *message) const
	{
		if (!condition) throw emu_fatalerror("C54x dual case=%u raw=%u tick=%u replay=%u: %s", m_ordinal, m_raw, m_tick, unsigned(m_replay), message);
	}
	void schedule() { m_timer.adjust(attotime::from_ticks(1, m_cpu.clock())); }
	void begin()
	{
		m_cpu.suspend(SUSPEND_REASON_DISABLE, true);
		bool const alu = m_mode.alu(), parallel = m_mode.parallel(), family = m_mode.family(), macsu = m_mode.macsu(), edge = m_mode.edge;
		m_raw = family && !edge ? dual_mac_schedule::key(m_ordinal) : m_ordinal;
		m_input = dual_multiply_case(m_raw, edge, alu, macsu, family, family, parallel);
		auto const &f = m_input;
		if (family) m_coverage.add(m_raw);
		unsigned const feedback = parallel ? f.destination : f.source;
		auto &program = m_cpu.space(AS_PROGRAM); auto &data = m_cpu.space(AS_DATA);
		for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
		program.write_word(f.base + 0x600, (alu ? 0xa000 | (m_ordinal >= (edge ? 16U : 43008U) ? 0x200 : 0) : family ? (parallel ? 0xd000 : 0xb000) | (f.operation << 10) : macsu ? 0xa600 : 0xa400) | (family ? f.source << 9 : 0) | (f.destination << 8) | (f.x << 4) | f.y); program.write_word(f.base + 0x601, 0xf4e1);
		if (f.xaddress >= 0x60) data.write_word(f.xaddress, f.xword);
		if (f.yaddress >= 0x60) data.write_word(f.yaddress, f.yword);
		m_cpu.set_state_int(r::STATE_IDLE, 0); m_cpu.set_state_int(r::STATE_ILLEGAL, 0);
		m_cpu.set_state_int(r::STATE_PMST, f.pmst); m_cpu.set_state_int(r::STATE_ST0, f.st0); m_cpu.set_state_int(r::STATE_ST1, f.st1);
		m_cpu.set_state_int(r::STATE_TCR, 0x10); m_cpu.set_state_int(r::STATE_IFR, 0); m_cpu.set_state_int(r::STATE_IMR, 0);
		m_cpu.set_state_int(r::STATE_A, 0x123456789aULL); m_cpu.set_state_int(r::STATE_B, 0xfeabcd9876ULL); m_cpu.set_state_int(r::STATE_T, 0xabcd);
		if (parallel) m_cpu.set_state_int(r::STATE_T, f.yword);
		if (macsu) m_cpu.set_state_int(feedback ? r::STATE_B : r::STATE_A, dual_macsu_seed(family ? m_raw % (edge ? 16U : 215040U) : m_raw, edge) ^ (edge ? 0 : 0xffffffffffULL));
		for (unsigned i = 0; i < 4; ++i) m_cpu.set_state_int(r::STATE_AR2 + i, f.initial[i]);
		m_cpu.set_state_int(r::STATE_AR0, 3); m_cpu.set_state_int(r::STATE_BK, f.linear ? 0 : 16);
		m_cpu.set_input_line(0, CLEAR_LINE); m_cpu.set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
		m_reads = m_fetches = m_writes = 0;
		m_read_tap = data.install_read_tap(0, 0xffff, "dual_multiply_reads", [this, f, edge, parallel](offs_t at, u16 &word, u16) { if (m_machine.side_effects_disabled()) return; require(!edge && m_reads < (parallel ? 1U : 2U) && at == (m_reads ? f.yaddress : f.xaddress) && word == (m_reads ? f.yword : f.xword) && m_cpu.pipeline_cycles() == 5, "Dual multiply retains its captured logical operands in Read"); ++m_reads; });
		m_write_tap = data.install_write_tap(0, 0xffff, "dual_multiply_write", [this, f, edge, parallel](offs_t at, u16 &word, u16) { if (m_machine.side_effects_disabled()) return; require(parallel && !edge && !m_writes && at == f.yaddress && word == f.stored && m_cpu.pipeline_cycles() == 6, "Parallel MAC writes the old shifted source once at Execute"); ++m_writes; });
		m_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600, "dual_multiply_fetch", [this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) { require(!m_fetches, "Dual MPY opcode is fetched once"); ++m_fetches; } });
		m_replay = m_resume = false; m_tick = 0;
		m_cpu.set_state_int(STATE_GENPC, f.base + 0x600); m_cpu.set_pipeline_frontend(true); m_cpu.resume(SUSPEND_REASON_DISABLE);
		m_timer.adjust(attotime::from_ticks(1, m_cpu.clock()));
	}
	running_machine &m_machine;
	tms320c54x_device &m_cpu;
	emu_timer &m_timer;
	dual_multiply_mode m_mode;
	dual_multiply_fixture m_input{};
	unsigned m_ordinal = 0, m_raw = 0, m_tick = 0, m_restores = 0, m_reads = 0, m_fetches = 0, m_writes = 0;
	bool m_replay = false, m_resume = false;
	std::array<unsigned, 3> m_saved_counts{};
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
	memory_passthrough_handler m_read_tap, m_write_tap, m_fetch_tap;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DUAL_H
