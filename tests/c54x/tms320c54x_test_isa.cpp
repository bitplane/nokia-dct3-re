// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "tms320c54x_test_domains.h"
#include "tms320c54x_test_memory_examples.h"
#include "tms320c54x_test_word_examples.h"
#include "tms320c54x_test_absolute_examples.h"
#include "tms320c54x_test_disassembly.h"
#include "tms320c54x_test_long_t_store.h"
#include "tms320c54x_test_mapped_mac.h"
#include "tms320c54x_test_dual.h"
#include "tms320c54x_test_normalization.h"
#include "tms320c54x_test_register_sequence.h"
#include "tms320c54x_test_address_mode.h"
#include "tms320c54x_test_stack_sequence.h"
#include "tms320c54x_test_register_pairs.h"
#include "tms320c54x_test_register_alu.h"
#include "tms320c54x_test_register_unary.h"
#include "tms320c54x_test_composition_limits.h"
#include <cstring>

namespace {

class c54x_isa_test_state : public driver_device
{
public:
	c54x_isa_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_cpu(*this, "cpu") { }
	void test(machine_config &config)
	{
		m_domain = !std::strcmp(config.options().bios(), "source-domain");
		m_mapped_mac = !std::strcmp(config.options().bios(), "mac-map-imm");
		m_dual_mode = c54x_test::dual_multiply_mode::select(config.options().bios());
		m_normalization_kind = c54x_test::select_normalization(config.options().bios());
		m_register_pair_kind = c54x_test::select_register_pair(config.options().bios());
		m_register_alu_kind = c54x_test::select_register_alu(config.options().bios());
		m_register_unary_kind = c54x_test::select_register_unary(config.options().bios());
		m_long_t_store_kind = c54x_test::select_long_t_store(config.options().bios());
		m_address_modes = !std::strcmp(config.options().bios(), "mode-latency");
		m_stack_sequences = !std::strcmp(config.options().bios(), "stack-sp");
		m_composition_limit = c54x_test::select_composition_limit(config.options().bios());
		m_extended_program = m_long_t_store_kind != c54x_test::long_t_store_kind::none || !std::strcmp(config.options().bios(), "isa-disasm") || m_domain || m_mapped_mac || bool(m_dual_mode) ||
			m_normalization_kind != c54x_test::normalization_kind::none || m_register_pair_kind != c54x_test::register_pair_kind::none || m_register_alu_kind != c54x_test::register_alu_kind::none || m_register_unary_kind != c54x_test::register_unary_kind::none || m_composition_limit != c54x_test::composition_limit::none;
		TMS320C54X(config, m_cpu, 13'000'000);
		m_cpu->set_extended_program(m_extended_program);
		m_cpu->set_addrmap(AS_PROGRAM, &c54x_isa_test_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &c54x_isa_test_state::ram_map);
		m_cpu->set_addrmap(AS_IO, &c54x_isa_test_state::io_map);
	}
protected:
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(c54x_isa_test_state::check), this);
		if (disassembly()) return;
		if (m_composition_limit != c54x_test::composition_limit::none)
			m_register_sequence = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
				c54x_test::composition_limit_count(m_composition_limit), [this](unsigned raw) { return c54x_test::composition_limit_case<tms320c54x_device>(raw, m_composition_limit); });
		else if (m_stack_sequences)
			m_register_sequence = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
				774, c54x_test::stack_sequence<tms320c54x_device>);
		else if (m_address_modes)
			m_register_sequence = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
				1072, c54x_test::address_mode_bank_case<tms320c54x_device>);
		else if (m_long_t_store_kind != c54x_test::long_t_store_kind::none)
			m_register_sequence = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
				c54x_test::long_t_store_count(m_long_t_store_kind),
				[this](unsigned key) { return c54x_test::long_t_store_sequence<tms320c54x_device>(key, m_long_t_store_kind); });
		else if (m_register_unary_kind != c54x_test::register_unary_kind::none)
			m_register_sequence = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
				c54x_test::register_unary_count(m_register_unary_kind),
				[this](unsigned key) { return c54x_test::register_unary_case<tms320c54x_device>(key, m_register_unary_kind); });
		else if (m_register_pair_kind != c54x_test::register_pair_kind::none || m_register_alu_kind != c54x_test::register_alu_kind::none)
			m_register_sequence = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
				m_register_pair_kind != c54x_test::register_pair_kind::none ? c54x_test::register_pair_count(m_register_pair_kind) : c54x_test::register_alu_count(m_register_alu_kind),
				[this](unsigned key) { return m_register_pair_kind != c54x_test::register_pair_kind::none ? c54x_test::register_pair_case<tms320c54x_device>(key, m_register_pair_kind) : c54x_test::register_alu_case<tms320c54x_device>(key, m_register_alu_kind); });
		else if (m_normalization_kind != c54x_test::normalization_kind::none)
			m_normalization = std::make_unique<c54x_test::exponent_normalization_fixture>(machine(), *m_cpu, *m_timer, m_normalization_kind);
		else if (m_dual_mode) m_dual = std::make_unique<c54x_test::dual_multiply_fixture_runner>(machine(), *m_cpu, *m_timer, m_dual_mode);
		else if (m_mapped_mac) m_mapped = std::make_unique<c54x_test::mapped_mac_fixture>(machine(), *m_cpu, *m_timer);
		else if (m_domain) m_domains = std::make_unique<c54x_test::source_domain_fixture>(machine(), *m_cpu, *m_timer);
		else
		{
			if (memory_reference())
			{
				for (auto const *bank : {
					&c54x_test::memory_examples(),
					&c54x_test::absolute_arithmetic_examples(),
					&c54x_test::long_offset_examples(),
					&c54x_test::memory_route_examples(),
					&c54x_test::memory_address_examples()})
					m_cases.insert(m_cases.end(), bank->begin(), bank->end());
			}
			else if (words()) m_cases = c54x_test::instruction_word_examples();
			else if (products()) throw emu_fatalerror("Legacy product bank retired: run atomic_product_literals");
			else if (near_flow())
				throw emu_fatalerror("Legacy near-flow bank retired: run the atomic_near_control section");
			else if (long_reference())
				throw emu_fatalerror("Legacy long bank retired: run the long_literals section");
			else if (parallel())
				throw emu_fatalerror("Legacy parallel bank retired: run atomic_parallel_literals");
			else
				throw emu_fatalerror("Legacy square bank retired: run the square_literals section");
			if (m_cases.empty() || !m_cases.front().reset_before)
				throw emu_fatalerror("C54x literal bank must start with an explicit CPU reset");
		}
	}
	void machine_reset() override
	{
		if (disassembly())
		{
			c54x_test::check_disassembly(m_cpu->get_disassembler(),
				[](bool valid, char const *contract) { if (!valid) throw emu_fatalerror("%s", contract); },
				[](char const *id, char const *format, auto... values) {
					osd_printf_info("C54x ISA case: PASS id=%s\n", id);
					osd_printf_info(format, values...);
				});
			throw emu_fatalerror(3, "TMS320C54x isolated disassembly contracts complete");
		}
		m_case = m_resets = 0;
		if (m_register_sequence) m_register_sequence->start();
		else if (m_normalization) m_normalization->start();
		else if (m_dual) m_dual->start();
		else if (m_mapped) m_mapped->start();
		else if (m_domains) m_domains->start();
		else start();
	}
private:
	[[noreturn]] void register_sequence_result() const
	{
		if (m_composition_limit != c54x_test::composition_limit::none)
		{
			osd_printf_info("C54x composition rejection coverage: PASS cases=%u restore=%u coverage_fnv64=%016llx\n",
				m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
			auto const contract = c54x_test::composition_limit_result(m_composition_limit);
			osd_printf_info("%s\n", contract.summary);
			throw emu_fatalerror(3, "%s", contract.completion);
		}
		if (m_stack_sequences)
		{
			osd_printf_info("TMS320C54x stack sequences: PASS cases=%u restore=%u frame_offsets=256 frame_snapshots=3 sp_cases=6 gap_nops=1 silicon_phase_claim=0 coverage_fnv64=%016llx\n",
				m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated stack sequences complete");
		}
		if (m_address_modes)
		{
			osd_printf_info("TMS320C54x addressing-mode banks: PASS cases=%u restore=%u basic=8 full_status=40 dp_forward=1024 producers=RSBX,SSBX,MVMD,STLM,STM,LD_DP gaps=2,3 arp_initial=0 finish=10,11 idle_required=0 silicon_phase_claim=0 coverage_fnv64=%016llx\n",
				m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated addressing-mode latency complete");
		}
		if (m_long_t_store_kind != c54x_test::long_t_store_kind::none)
		{
			osd_printf_info("TMS320C54x long T store coverage: PASS cases=%u restore=%u coverage_fnv64=%016llx\n",
				m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
			if (m_long_t_store_kind == c54x_test::long_t_store_kind::rejected)
			{
				osd_printf_info("TMS320C54x extended T/TRN exclusions: PASS cases=256 restore=256 forms=4 registers=8 compatibility=2 sources=2 pages=2 snapshots=3 checkpoint_policy=balanced reads=0 writes=0 pointer_updates=0 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated extended T/TRN exclusions complete");
			}
			osd_printf_info("TMS320C54x extended T/TRN stores: PASS cases=6144 restore=6144 forms=absolute,offset,premod,circular registers=8 compatibility=2 sources=T,TRN values=4 endpoints=RAM,peripheral pages=2 snapshots=3 destination=Read source=Execute literal_retained=1 pointer_updates=once destination_reads=0 writes=1 peripheral_extra_clock=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated extended T/TRN stores complete");
		}
		if (m_register_unary_kind != c54x_test::register_unary_kind::none)
		{
			osd_printf_info("%s\n", c54x_test::register_unary_result(m_register_unary_kind, m_register_sequence->count(),
				m_register_sequence->restores(), m_register_sequence->digest()).c_str());
			char const *name = m_register_unary_kind == c54x_test::register_unary_kind::logic ? "register logic" :
				m_register_unary_kind == c54x_test::register_unary_kind::shift ? "accumulator shifts" :
				m_register_unary_kind == c54x_test::register_unary_kind::absolute ? "ABS" : "NEG";
			throw emu_fatalerror(3, "TMS320C54x isolated %s complete", name);
		}
		if (m_register_alu_kind == c54x_test::register_alu_kind::minmax)
		{
			osd_printf_info("TMS320C54x staged MAX MIN: PASS cases=%u restore=%u selectors=4 value_pairs=256 status_states=8 pages=2 snapshots=4 checkpoint_policy=balanced_non_ties_all_ties operands=Execute signed_bits=40 equality=B carry_only=1 data_accesses=0 duplicate_fetches=0 producer_composition=unvalidated silicon_phase_claim=0 coverage_fnv64=%016llx\n", m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated MAX MIN complete");
		}
		if (m_register_alu_kind == c54x_test::register_alu_kind::square)
		{
			osd_printf_info("TMS320C54x staged accumulator SQUR: PASS cases=%u restore=%u destinations=2 a_patterns=16 b_patterns=2 flags=8 pages=2 snapshots=4 operand_bits=17 operand=Execute t_unchanged=1 sticky_overflow=1 data_accesses=0 producer_composition=unvalidated silicon_phase_claim=0 coverage_fnv64=%016llx\n", m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated accumulator SQUR complete");
		}
		if (m_register_pair_kind == c54x_test::register_pair_kind::saturation)
		{
			osd_printf_info("TMS320C54x staged SAT: PASS cases=%u restore=%u patterns=16 accumulators=2 initial_overflow=4 sxm_ovm=4 pages=2 snapshots=3 adjacent=2 silicon_phase_claim=0 coverage_fnv64=%016llx\n", m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated staged SAT complete");
		}
		osd_printf_info("TMS320C54x staged rotates: PASS cases=%u restore=%u forms=ROR_A,ROR_B,ROL_A,ROL_B,ROLTC_A,ROLTC_B patterns=8 c_tc=4 sxm_ovm=4 pages=2 snapshots=3 adjacent=2 guard_clear=1 status_preserved=1 silicon_phase_claim=0 coverage_fnv64=%016llx\n", m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
		throw emu_fatalerror(3, "TMS320C54x isolated staged rotates complete");
	}
	[[noreturn]] void normalization_result() const
	{
		using k = c54x_test::normalization_kind;
		unsigned const count = m_normalization->count(), restores = m_normalization->restores();
		auto const digest = (unsigned long long)m_normalization->digest();
		if (m_normalization_kind == k::aliases)
		{
			osd_printf_info("TMS320C54x staged NORM aliases: PASS cases=%u restore=%u selectors=4 ts_codes=32..47 shifts=-16..-1 patterns=8 status_states=8 pages=2 snapshots=2 checkpoint_policy=balanced data_accesses=0 silicon_phase_claim=0 coverage_fnv64=%016llx\n", count, restores, digest);
			throw emu_fatalerror(3, "TMS320C54x isolated NORM aliases complete");
		}
		if (m_normalization_kind == k::producer)
		{
			osd_printf_info("TMS320C54x staged EXP NORM: PASS cases=%u restore=%u selectors=4 patterns=16 gaps=0,1,2 sxm_ovm=4 pages=2 snapshots=4 checkpoint_policy=balanced ts=Read zero_gap=projection_only data_accesses=0 silicon_phase_claim=0 coverage_fnv64=%016llx\n", count, restores, digest);
			throw emu_fatalerror(3, "TMS320C54x isolated EXP NORM complete");
		}
		if (m_normalization_kind == k::exponent)
		{
			osd_printf_info("TMS320C54x staged EXP T: PASS cases=%u restore=%u accumulators=2 vectors=16 gaps=0,1,2 status_states=8 producers=none,LDU pages=2 snapshots=4 checkpoint_policy=balanced publish=Execute adjacent_T=old gap_T=new retained_consumer=1 flags_preserved=1 data_writes=0 norm_acceptance=0 silicon_phase_claim=0 coverage_fnv64=%016llx\n", count, restores, digest);
			throw emu_fatalerror(3, "TMS320C54x isolated EXP T complete");
		}
		osd_printf_info("TMS320C54x staged NORM: PASS cases=%u restore=%u selectors=4 shifts=-16..31 patterns=8 status_states=8 pages=2 snapshots=4 checkpoint_policy=balanced ts=Read result=Execute carry_preserved=1 data_accesses=0 silicon_phase_claim=0 coverage_fnv64=%016llx\n", count, restores, digest);
		throw emu_fatalerror(3, "TMS320C54x isolated NORM complete");
	}
	[[noreturn]] void dual_result() const
	{
		bool const alu = m_dual_mode.alu(), parallel = m_dual_mode.parallel(), family = m_dual_mode.family(), macsu = m_dual_mode.macsu(), edge = m_dual_mode.edge;
		if (family)
		{
			if (parallel)
			{
				if (edge) { osd_printf_info("TMS320C54x parallel MAC exclusions: PASS cases=%u restore=%u reads=0 writes=0 pointer_updates=0 silicon_trap_claim=0 coverage_fnv64=%016llx\n", m_dual->count(), m_dual->restores(), (unsigned long long)m_dual->digest()); throw emu_fatalerror(3, "TMS320C54x isolated parallel MAC exclusions complete"); }
				osd_printf_info("TMS320C54x parallel MAC: PASS cases=%u restore=%u address_cases=147456 arithmetic_cases=524288 checkpoint_policy=balanced operations=MAC,MACR,MAS,MASR encodings=4096 source_destination_pairs=4 shifts=32 reads=1 writes=1 retained=T,X,ASM old_source_store=1 carry=unvalidated physical_banks=unvalidated interrupt_repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_dual->count(), m_dual->restores(), (unsigned long long)m_dual->digest()); throw emu_fatalerror(3, "TMS320C54x isolated parallel MAC complete");
			}
			osd_printf_info("TMS320C54x dual MAC family: PASS cases=%u restore=%u address_cases=147456 arithmetic_cases=524288 checkpoint_policy=balanced operations=MAC,MACR,MAS,MASR encodings=4096 source_destination_pairs=4 snapshots=3 reads=2 t_publish=Read accumulator_sample=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_dual->count(), m_dual->restores(), (unsigned long long)m_dual->digest()); throw emu_fatalerror(3, "TMS320C54x isolated dual MAC family complete");
		}
		if (macsu)
		{
			osd_printf_info("TMS320C54x staged dual MACSU: PASS address_cases=18432 arithmetic_cases=65536 restore=%u encodings=512 layouts=3 bk_modes=2 value_pairs=64 accumulator_patterns=8 arithmetic_flags=32 pages=2 snapshots=3 checkpoint_policy=balanced reads=2 data_writes=0 x_unsigned_y_signed=1 x_alias_priority=1 t_publish=Read accumulator_sample=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_dual->restores(), (unsigned long long)m_dual->digest()); throw emu_fatalerror(0, "TMS320C54x isolated dual MACSU complete");
		}
		if (alu)
		{
			if (edge)
			{
				osd_printf_info("TMS320C54x staged dual ALU exclusions: PASS cases=%u restore=%u operations=ADD,SUB operands=X,Y sources=cpu_register,peripheral reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0 coverage_fnv64=%016llx\n", m_dual->count(), m_dual->restores(), (unsigned long long)m_dual->digest());
				throw emu_fatalerror(0, "TMS320C54x isolated dual ALU exclusions complete");
			}
			osd_printf_info("TMS320C54x staged dual ALU: PASS address_cases=36864 arithmetic_cases=16384 restore=%u operations=ADD,SUB encodings=1024 layouts=3 bk_modes=2 value_pairs=64 arithmetic_flags=32 pages=2 snapshots=3 checkpoint_policy=balanced reads=2 data_writes=0 x_alias_priority=1 t_unchanged=1 result_publish=Execute physical_banks=unvalidated repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_dual->restores(), (unsigned long long)m_dual->digest());
			throw emu_fatalerror(0, "TMS320C54x isolated dual ALU complete");
		}
		if (edge)
		{
			osd_printf_info("TMS320C54x staged dual MPY exclusions: PASS cases=%u restore=%u operands=X,Y sources=cpu_register,peripheral pages=2 snapshots=2 reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0 coverage_fnv64=%016llx\n", m_dual->count(), m_dual->restores(), (unsigned long long)m_dual->digest());
			throw emu_fatalerror(0, "TMS320C54x isolated dual MPY exclusions complete");
		}
		osd_printf_info("TMS320C54x staged dual MPY: PASS address_cases=18432 arithmetic_cases=8192 restore=%u encodings=512 layouts=3 bk_modes=2 value_pairs=64 arithmetic_flags=32 pages=2 snapshots=3 checkpoint_policy=balanced reads=2 data_writes=0 x_alias_priority=1 t_publish=Read result_publish=Execute physical_banks=unvalidated repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_dual->restores(), (unsigned long long)m_dual->digest());
		throw emu_fatalerror(0, "TMS320C54x isolated dual MPY complete");
	}
	bool parallel() const { return !std::strcmp(machine().options().bios(), "isa-parallel"); }
	bool scalars() const { return !std::strcmp(machine().options().bios(), "isa-scalars"); }
	bool long_reference() const { return !std::strcmp(machine().options().bios(), "isa-long"); }
	bool products() const { return !std::strcmp(machine().options().bios(), "isa-products"); }
	bool memory_reference() const { return !std::strcmp(machine().options().bios(), "isa-memory"); }
	bool near_flow() const { return !std::strcmp(machine().options().bios(), "isa-flow"); }
	bool words() const { return !std::strcmp(machine().options().bios(), "isa-words"); }
	bool disassembly() const { return !std::strcmp(machine().options().bios(), "isa-disasm"); }
	unsigned count() const
	{
		return m_cases.size();
	}
	c54x_test::instruction_case const &test_case() const
	{
		return m_cases.at(m_case);
	}
	void ram_map(address_map &map) { map(0, 0xffff).ram(); }
	void program_map(address_map &map)
	{
		map(0, 0xffff).ram();
		if (m_composition_limit == c54x_test::composition_limit::interrupt_pop) map(0x10000, 0x1ffff).ram();
		if (m_extended_program) map(0x20000, 0x2ffff).ram();
	}
	void io_map(address_map &map)
	{
		if (m_domain) map(0, 0xffff).ram();
		map(0x0124, 0x0124).w(FUNC(c54x_isa_test_state::port_w));
	}
	void port_w(u16)
	{
		if (!m_writes++) m_first = m_cpu->total_cycles();
		m_last = m_cpu->total_cycles();
		if (m_writes == 2) m_middle = m_last;
	}
	void start()
	{
		if (test_case().reset_before)
		{
			++m_resets;
			m_cpu->reset();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			for (auto const &setting : test_case().reset_initial)
				m_cpu->set_state_int(setting.index, setting.value);
			for (auto const &word : test_case().reset_data)
				m_cpu->space(AS_DATA).write_word(word.address, word.value);
		}
		m_writes = 0; m_first = m_middle = m_last = 0;
		c54x_test::prepare_case(*m_cpu, test_case());
		m_timer->adjust(attotime::from_usec(100));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		if (m_register_sequence) { if (m_register_sequence->step()) register_sequence_result(); return; }
		if (m_normalization) { if (m_normalization->step()) normalization_result(); return; }
		if (m_dual) { if (m_dual->step()) dual_result(); return; }
		if (m_mapped)
		{
			if (!m_mapped->step()) return;
			osd_printf_info("TMS320C54x mapped memory immediate MAC: PASS cases=%u restore=%u sources=AL,AH,AG,BL,BH,BG,T,TRN,AR0_AR7 forms=DP,plain,absolute,offset values=4 literals=8 flags=16 selectors=4 pages=2 snapshots=3 checkpoint_policy=balanced source_sample=Read accumulator_sample=Execute t_publication=Read pointer_literal_poison=1 reset_before_restore=1 physical_phase_claim=0 cpu_only=1 coverage_fnv64=%016llx\n", m_mapped->count(), m_mapped->restores(), (unsigned long long)m_mapped->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated mapped memory immediate MAC complete");
		}
		if (m_domains)
		{
			if (!m_domains->step()) return;
			osd_printf_info("TMS320C54x source-domain exclusions: PASS cases=%u restore=%u base_cases=30000 absolute_tc0_cases=448 short_control_cases=14208 mac_source_cases=1280 mac_short_checkpoint_cases=128 mapped_rejection_cases=8328 mmr_guard_cases=2304 repeat_transfer_guard_cases=4800 guard_checkpoints=balanced pmst_bit14=2 mac_frontend=staged mac_checkpoints=3,5 mac_modes=f8,98..9f base_instruction_forms=125 smem_classes=111 shifted_reads=10 mmr_sources=4 shifts=0,1,15,16,31 base_modes=98..9f,f9..ff cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_accesses=0 silicon_trap_claim=0 coverage_fnv64=%016llx\n",
				m_domains->count(), m_domains->restores(), (unsigned long long)m_domains->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated source-domain exclusions complete");
		}
		auto const &test = test_case();
		c54x_test::check_case(*m_cpu, test, m_writes, m_last - m_first,
			std::array<u64, 2>{m_middle - m_first, m_last - m_middle});
		osd_printf_info("C54x ISA case: PASS id=%s\n", test.id);
		if (++m_case < count()) { start(); return; }
		if (words())
		{
			osd_printf_info("TMS320C54x generic instruction words: PASS cases=%u reset_groups=%u nokia_devices=0 firmware=0\n", m_case, m_resets);
			throw emu_fatalerror(3, "TMS320C54x isolated generic instruction words complete");
		}
		if (near_flow())
		{
			osd_printf_info("TMS320C54x generic near flow: PASS cases=%u reset_groups=%u nokia_devices=0 firmware=0\n", m_case, m_resets);
			throw emu_fatalerror(3, "TMS320C54x isolated generic near flow complete");
		}
		if (memory_reference())
		{
			osd_printf_info("TMS320C54x generic memory examples: PASS cases=%u reset_groups=%u nokia_devices=0 firmware=0\n", m_case, m_resets);
			throw emu_fatalerror(3, "TMS320C54x isolated generic memory examples complete");
		}
		if (products())
		{
			osd_printf_info("TMS320C54x generic product sequence: PASS cases=%u reset_groups=%u nokia_devices=0 firmware=0\n", m_case, m_resets);
			throw emu_fatalerror(3, "TMS320C54x isolated generic product sequence complete");
		}
		if (long_reference())
		{
			osd_printf_info("TMS320C54x generic long arithmetic: PASS cases=%u reset_groups=%u nokia_devices=0 firmware=0\n", m_case, m_resets);
			throw emu_fatalerror(3, "TMS320C54x isolated generic long arithmetic complete");
		}
		if (scalars())
		{
			osd_printf_info("TMS320C54x generic scalars: PASS cases=%u reset_groups=%u nokia_devices=0 firmware=0\n", m_case, m_resets);
			throw emu_fatalerror(3, "TMS320C54x isolated generic scalars complete");
		}
		if (parallel())
		{
			osd_printf_info("TMS320C54x generic parallel: PASS cases=%u reset_groups=%u nokia_devices=0 firmware=0\n", m_case, m_resets);
			throw emu_fatalerror(3, "TMS320C54x isolated generic parallel complete");
		}
		osd_printf_info("TMS320C54x generic square: PASS cases=6 reset_each=1 nokia_devices=0 firmware=0\n");
		throw emu_fatalerror(3, "TMS320C54x isolated generic square complete");
	}
	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_timer = nullptr;
	unsigned m_case = 0, m_writes = 0, m_resets = 0;
	u64 m_first = 0, m_middle = 0, m_last = 0;
	bool m_domain = false;
	bool m_extended_program = false;
	bool m_mapped_mac = false;
	c54x_test::dual_multiply_mode m_dual_mode;
	c54x_test::normalization_kind m_normalization_kind = c54x_test::normalization_kind::none;
	c54x_test::register_pair_kind m_register_pair_kind = c54x_test::register_pair_kind::none;
	c54x_test::register_alu_kind m_register_alu_kind = c54x_test::register_alu_kind::none;
	c54x_test::register_unary_kind m_register_unary_kind = c54x_test::register_unary_kind::none;
	c54x_test::long_t_store_kind m_long_t_store_kind = c54x_test::long_t_store_kind::none;
	bool m_address_modes = false;
	bool m_stack_sequences = false;
	std::unique_ptr<c54x_test::register_sequence_fixture> m_register_sequence;
	c54x_test::composition_limit m_composition_limit = c54x_test::composition_limit::none;
	std::unique_ptr<c54x_test::exponent_normalization_fixture> m_normalization;
	std::unique_ptr<c54x_test::dual_multiply_fixture_runner> m_dual;
	std::unique_ptr<c54x_test::source_domain_fixture> m_domains;
	std::unique_ptr<c54x_test::mapped_mac_fixture> m_mapped;
	std::vector<c54x_test::instruction_case> m_cases;
};

ROM_START(tms54isa)
	ROM_SYSTEM_BIOS(0, "isa-square", "TI square arithmetic reference cases")
	ROM_SYSTEM_BIOS(1, "isa-parallel", "Parallel store arithmetic reference cases")
	ROM_SYSTEM_BIOS(2, "source-domain", "Implemented single-memory operand admission limits")
	ROM_SYSTEM_BIOS(3, "isa-scalars", "Scalar arithmetic boundaries with explicit stateful groups")
	ROM_SYSTEM_BIOS(4, "isa-long", "TI long-word arithmetic examples")
	ROM_SYSTEM_BIOS(5, "isa-products", "Literal multiply arithmetic and DELAY continuation sequence")
	ROM_SYSTEM_BIOS(6, "isa-flow", "Conditional delayed branch return and high-address stack examples")
	ROM_SYSTEM_BIOS(7, "mac-map-imm", "Read-retained mapped source with Execute-live immediate MAC")
	ROM_SYSTEM_BIOS(8, "isa-memory", "Ordered literal memory arithmetic multiply and T examples")
	ROM_SYSTEM_BIOS(9, "mpy-dual", "Dual-memory multiply addresses and arithmetic replay")
	ROM_SYSTEM_BIOS(10, "mpy-dual-edge", "Excluded dual-memory multiply low-page operands")
	ROM_SYSTEM_BIOS(11, "alu-dual", "Dual-memory ADD SUB retained arithmetic replay")
	ROM_SYSTEM_BIOS(12, "alu-dual-edge", "Excluded dual-memory ADD SUB low-page operands")
	ROM_SYSTEM_BIOS(13, "macsu-dual", "Dual signed unsigned multiply accumulate ownership")
	ROM_SYSTEM_BIOS(14, "mac-family", "Dual MAC MACR MAS MASR with independent source and destination")
	ROM_SYSTEM_BIOS(15, "stmac", "Parallel MAC stores with retained T X and ASM")
	ROM_SYSTEM_BIOS(16, "stmac-edge", "Parallel MAC mapped operand exclusions")
	ROM_SYSTEM_BIOS(17, "norm", "Retained normalization shift sampling and replay")
	ROM_SYSTEM_BIOS(18, "norm-edge", "Recovered negative normalization-count aliases")
	ROM_SYSTEM_BIOS(19, "exp-norm", "EXP producer and normalization T latency")
	ROM_SYSTEM_BIOS(20, "exp-t", "Staged exponent publication and T consumer latency")
	ROM_SYSTEM_BIOS(21, "sat", "Staged signed saturation and overflow publication")
	ROM_SYSTEM_BIOS(22, "rotate", "Staged accumulator rotates and carry forwarding")
	ROM_SYSTEM_BIOS(23, "minmax", "Signed accumulator MAX MIN Execute ownership and replay")
	ROM_SYSTEM_BIOS(24, "squr", "Accumulator square with signed 17-bit operand")
	ROM_SYSTEM_BIOS(25, "reg-logic", "Register logic and complement without memory access")
	ROM_SYSTEM_BIOS(26, "reg-shifts", "Arithmetic and logical accumulator shifts")
	ROM_SYSTEM_BIOS(27, "negate", "Accumulator negate with producer and replay")
	ROM_SYSTEM_BIOS(28, "absolute", "Accumulator absolute value with producer and replay")
	ROM_SYSTEM_BIOS(29, "isa-words", "Atomic instruction-word arithmetic and addressing examples")
	ROM_SYSTEM_BIOS(30, "isa-disasm", "Debugger syntax length page wrapping and stepping contracts")
	ROM_SYSTEM_BIOS(31, "st-t-long", "Long-address T/TRN stores and retained Execute source")
	ROM_SYSTEM_BIOS(32, "st-t-long-edge", "Rejected mapped destinations of long-address T/TRN stores")
	ROM_SYSTEM_BIOS(33, "mode-latency", "Status producer CPL DP CMPT consumer ordering")
	ROM_SYSTEM_BIOS(34, "stack-sp", "FRAME push and safely separated compiler SP consumers")
	ROM_SYSTEM_BIOS(35, "pop-irq-edge", "Interrupt-register POPM composition admission limits")
	ROM_SYSTEM_BIOS(36, "clock-mvdk-edge", "CLKMD move composition admission limits")
	ROM_SYSTEM_BIOS(37, "st-delay-edge", "Rejected stores crossing the control-delay boundary")
	ROM_SYSTEM_BIOS(38, "block-xc-edge", "Rejected XC spans crossing the block endpoint")
	ROM_SYSTEM_BIOS(39, "rptbd-edge", "Rejected delayed-block control and spill forms")
	ROM_REGION(1, "unused", ROMREGION_ERASE00)
ROM_END

} // anonymous namespace

SYST(2026, tms54isa, 0, 0, test, 0, c54x_isa_test_state, empty_init,
	"Test", "TMS320C54x CPU-only tests", MACHINE_NO_SOUND | MACHINE_NOT_WORKING)
