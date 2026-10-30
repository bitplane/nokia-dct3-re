// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "tms320c54x_test_word_examples.h"
#include "tms320c54x_test_disassembly.h"
#include "tms320c54x_test_dual.h"
#include <cstring>

namespace {

class c54x_isa_test_state : public driver_device
{
public:
	c54x_isa_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_cpu(*this, "cpu") { }
	void test(machine_config &config)
	{
		m_dual_mode = c54x_test::dual_multiply_mode::select(config.options().bios());
		m_extended_program = !std::strcmp(config.options().bios(), "isa-disasm") || bool(m_dual_mode);
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
		if (m_dual_mode) m_dual = std::make_unique<c54x_test::dual_multiply_fixture_runner>(machine(), *m_cpu, *m_timer, m_dual_mode);
		else
		{
			if (words()) m_cases = c54x_test::instruction_word_examples();
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
		if (m_dual) m_dual->start();
		else start();
	}
private:
	[[noreturn]] void dual_result() const
	{
		if (m_dual_mode.parallel())
		{
			osd_printf_info("TMS320C54x parallel MAC: PASS cases=%u restore=%u address_cases=147456 arithmetic_cases=524288 checkpoint_policy=balanced operations=MAC,MACR,MAS,MASR encodings=4096 source_destination_pairs=4 shifts=32 reads=1 writes=1 retained=T,X,ASM old_source_store=1 carry=unvalidated physical_banks=unvalidated interrupt_repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_dual->count(), m_dual->restores(), (unsigned long long)m_dual->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated parallel MAC complete");
		}
		osd_printf_info("TMS320C54x dual MAC family: PASS cases=%u restore=%u address_cases=147456 arithmetic_cases=524288 checkpoint_policy=balanced operations=MAC,MACR,MAS,MASR encodings=4096 source_destination_pairs=4 snapshots=3 reads=2 t_publish=Read accumulator_sample=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated coverage_fnv64=%016llx\n", m_dual->count(), m_dual->restores(), (unsigned long long)m_dual->digest());
		throw emu_fatalerror(3, "TMS320C54x isolated dual MAC family complete");
	}
	bool parallel() const { return !std::strcmp(machine().options().bios(), "isa-parallel"); }
	bool scalars() const { return !std::strcmp(machine().options().bios(), "isa-scalars"); }
	bool long_reference() const { return !std::strcmp(machine().options().bios(), "isa-long"); }
	bool products() const { return !std::strcmp(machine().options().bios(), "isa-products"); }
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
		if (m_extended_program) map(0x20000, 0x2ffff).ram();
	}
	void io_map(address_map &map)
	{
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
		if (m_dual) { if (m_dual->step()) dual_result(); return; }
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
	bool m_extended_program = false;
	c54x_test::dual_multiply_mode m_dual_mode;
	std::unique_ptr<c54x_test::dual_multiply_fixture_runner> m_dual;
	std::vector<c54x_test::instruction_case> m_cases;
};

ROM_START(tms54isa)
	ROM_SYSTEM_BIOS(0, "isa-square", "TI square arithmetic reference cases")
	ROM_SYSTEM_BIOS(1, "isa-parallel", "Parallel store arithmetic reference cases")
	ROM_SYSTEM_BIOS(3, "isa-scalars", "Scalar arithmetic boundaries with explicit stateful groups")
	ROM_SYSTEM_BIOS(4, "isa-long", "TI long-word arithmetic examples")
	ROM_SYSTEM_BIOS(5, "isa-products", "Literal multiply arithmetic and DELAY continuation sequence")
	ROM_SYSTEM_BIOS(6, "isa-flow", "Conditional delayed branch return and high-address stack examples")
	ROM_SYSTEM_BIOS(14, "mac-family", "Dual MAC MACR MAS MASR with independent source and destination")
	ROM_SYSTEM_BIOS(15, "stmac", "Parallel MAC stores with retained T X and ASM")
	ROM_SYSTEM_BIOS(29, "isa-words", "Atomic instruction-word arithmetic and addressing examples")
	ROM_SYSTEM_BIOS(30, "isa-disasm", "Debugger syntax length page wrapping and stepping contracts")
	ROM_REGION(1, "unused", ROMREGION_ERASE00)
ROM_END

} // anonymous namespace

SYST(2026, tms54isa, 0, 0, test, 0, c54x_isa_test_state, empty_init,
	"Test", "TMS320C54x CPU-only tests", MACHINE_NO_SOUND | MACHINE_NOT_WORKING)
