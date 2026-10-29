// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "tms320c54x_test_square.h"
#include "tms320c54x_test_parallel.h"
#include "tms320c54x_test_domains.h"
#include "tms320c54x_test_scalars.h"
#include "tms320c54x_test_long_reference.h"
#include "tms320c54x_test_product_examples.h"
#include "tms320c54x_test_memory_examples.h"
#include "tms320c54x_test_near_flow.h"
#include "tms320c54x_test_mapped_mac.h"
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
		TMS320C54X(config, m_cpu, 13'000'000);
		m_cpu->set_extended_program(m_domain || m_mapped_mac);
		m_cpu->set_addrmap(AS_PROGRAM, &c54x_isa_test_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &c54x_isa_test_state::ram_map);
		m_cpu->set_addrmap(AS_IO, &c54x_isa_test_state::io_map);
	}
protected:
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(c54x_isa_test_state::check), this);
		if (m_mapped_mac) m_mapped = std::make_unique<c54x_test::mapped_mac_fixture>(machine(), *m_cpu, *m_timer);
		else if (m_domain) m_domains = std::make_unique<c54x_test::source_domain_fixture>(machine(), *m_cpu, *m_timer);
		else
		{
			if (memory_reference()) m_cases = c54x_test::memory_examples();
			else if (products()) m_cases = c54x_test::product_examples();
			else if (near_flow())
				m_cases.assign(c54x_test::near_flow_cases().begin(), c54x_test::near_flow_cases().end());
			else if (long_reference())
				m_cases.assign(c54x_test::long_reference_cases().begin(), c54x_test::long_reference_cases().end());
			else if (scalars())
				for (auto const &row : c54x_test::scalar_vectors()) m_cases.push_back(row.test);
			else if (parallel())
				for (unsigned i = 0; i < c54x_test::parallel_vectors().size(); ++i) m_cases.push_back(c54x_test::parallel_case(i));
			else
				for (unsigned i = 0; i < c54x_test::square_vectors.size(); ++i) m_cases.push_back(c54x_test::square_case(i));
			m_reset_before.assign(m_cases.size(), !(products() || memory_reference()));
			m_reset_before[0] = true;
			if (scalars())
				for (unsigned i = 0; i < m_cases.size(); ++i) m_reset_before[i] = c54x_test::scalar_vectors()[i].reset;
		}
	}
	void machine_reset() override
	{
		m_case = m_resets = 0;
		if (m_mapped) m_mapped->start();
		else if (m_domains) m_domains->start();
		else start();
	}
private:
	bool parallel() const { return !std::strcmp(machine().options().bios(), "isa-parallel"); }
	bool scalars() const { return !std::strcmp(machine().options().bios(), "isa-scalars"); }
	bool long_reference() const { return !std::strcmp(machine().options().bios(), "isa-long"); }
	bool products() const { return !std::strcmp(machine().options().bios(), "isa-products"); }
	bool memory_reference() const { return !std::strcmp(machine().options().bios(), "isa-memory"); }
	bool near_flow() const { return !std::strcmp(machine().options().bios(), "isa-flow"); }
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
		if (m_domain || m_mapped_mac) map(0x20000, 0x2ffff).ram();
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
	}
	void start()
	{
		if (m_reset_before.at(m_case))
		{
			++m_resets;
			m_cpu->reset();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			// The sequence uses circular PORTW timing markers, not firmware I/O.
			if (products() || memory_reference())
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, memory_reference() ? 0x0a01 : 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
				// The memory group enters from the checked C16 long-word row
				// in the legacy sequence. Seed that mode only at its standalone reset.
				if (memory_reference()) m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0180);
			}
		}
		m_writes = 0; m_first = m_last = 0;
		c54x_test::prepare_case(*m_cpu, test_case());
		m_timer->adjust(attotime::from_usec(100));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		if (m_mapped)
		{
			if (!m_mapped->step()) return;
			osd_printf_info("TMS320C54x mapped memory immediate MAC: PASS cases=%u restore=%u sources=AL,AH,AG,BL,BH,BG,T,TRN,AR0_AR7 forms=DP,plain,absolute,offset values=4 literals=8 flags=16 selectors=4 pages=2 snapshots=3 checkpoint_policy=balanced source_sample=Read accumulator_sample=Execute t_publication=Read pointer_literal_poison=1 reset_before_restore=1 physical_phase_claim=0 cpu_only=1 coverage_fnv64=%016llx\n", m_mapped->count(), m_mapped->restores(), (unsigned long long)m_mapped->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated mapped memory immediate MAC complete");
		}
		if (m_domains)
		{
			if (!m_domains->step()) return;
			osd_printf_info("TMS320C54x source-domain exclusions: PASS cases=%u restore=%u base_cases=30000 absolute_tc0_cases=448 short_control_cases=14208 mac_source_cases=1280 mac_short_checkpoint_cases=128 mac_frontend=staged mac_checkpoints=3,5 mac_modes=f8,98..9f base_instruction_forms=125 smem_classes=111 shifted_reads=10 mmr_sources=4 shifts=0,1,15,16,31 base_modes=98..9f,f9..ff cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_accesses=0 silicon_trap_claim=0 coverage_fnv64=%016llx\n",
				m_domains->count(), m_domains->restores(), (unsigned long long)m_domains->digest());
			throw emu_fatalerror(3, "TMS320C54x isolated source-domain exclusions complete");
		}
		auto const &test = test_case();
		c54x_test::check_case(*m_cpu, test, m_writes, m_last - m_first);
		osd_printf_info("C54x ISA case: PASS id=%s\n", test.id);
		if (++m_case < count()) { start(); return; }
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
			osd_printf_info("TMS320C54x generic parallel: PASS cases=10 reset_each=1 nokia_devices=0 firmware=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated generic parallel complete");
		}
		osd_printf_info("TMS320C54x generic square: PASS cases=6 reset_each=1 nokia_devices=0 firmware=0\n");
		throw emu_fatalerror(3, "TMS320C54x isolated generic square complete");
	}
	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_timer = nullptr;
	unsigned m_case = 0, m_writes = 0, m_resets = 0;
	u64 m_first = 0, m_last = 0;
	bool m_domain = false;
	bool m_mapped_mac = false;
	std::unique_ptr<c54x_test::source_domain_fixture> m_domains;
	std::unique_ptr<c54x_test::mapped_mac_fixture> m_mapped;
	std::vector<c54x_test::instruction_case> m_cases;
	std::vector<bool> m_reset_before;
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
	ROM_REGION(1, "unused", ROMREGION_ERASE00)
ROM_END

} // anonymous namespace

SYST(2026, tms54isa, 0, 0, test, 0, c54x_isa_test_state, empty_init,
	"Test", "TMS320C54x CPU-only tests", MACHINE_NO_SOUND | MACHINE_NOT_WORKING)
