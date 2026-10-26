// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

namespace {
struct source_domain_instruction
{
	u16 opcode;
	u16 operation;
};

// Independent instruction inventory: SPRU172C's single-data-memory read/RMW
// forms, shifted read extensions, and the four implemented MMR-source forms.
// Do not derive this bank from the CPU's admission predicates.
constexpr source_domain_instruction source_domains[] = {
	{ 0x0000, 0 }, { 0x0100, 0 }, { 0x0200, 0 }, { 0x0300, 0 },
	{ 0x0400, 0 }, { 0x0500, 0 }, { 0x0600, 0 }, { 0x0700, 0 },
	{ 0x0800, 0 }, { 0x0900, 0 }, { 0x0a00, 0 }, { 0x0b00, 0 },
	{ 0x0c00, 0 }, { 0x0d00, 0 }, { 0x0e00, 0 }, { 0x0f00, 0 },
	{ 0x1000, 0 }, { 0x1100, 0 }, { 0x1200, 0 }, { 0x1300, 0 },
	{ 0x1400, 0 }, { 0x1500, 0 }, { 0x1600, 0 }, { 0x1700, 0 },
	{ 0x1800, 0 }, { 0x1900, 0 }, { 0x1a00, 0 }, { 0x1b00, 0 },
	{ 0x1c00, 0 }, { 0x1d00, 0 }, { 0x1e00, 0 }, { 0x1f00, 0 },
	{ 0x2000, 0 }, { 0x2100, 0 }, { 0x2200, 0 }, { 0x2300, 0 },
	{ 0x2400, 0 }, { 0x2500, 0 }, { 0x2600, 0 }, { 0x2700, 0 },
	{ 0x2800, 0 }, { 0x2900, 0 }, { 0x2a00, 0 }, { 0x2b00, 0 },
	{ 0x2c00, 0 }, { 0x2d00, 0 }, { 0x2e00, 0 }, { 0x2f00, 0 },
	{ 0x3000, 0 }, { 0x3100, 0 }, { 0x3200, 0 }, { 0x3300, 0 },
	{ 0x3400, 0 }, { 0x3500, 0 }, { 0x3600, 0 }, { 0x3700, 0 },
	{ 0x3800, 0 }, { 0x3900, 0 }, { 0x3a00, 0 }, { 0x3b00, 0 },
	{ 0x3c00, 0 }, { 0x3d00, 0 }, { 0x3e00, 0 }, { 0x3f00, 0 },
	{ 0x4000, 0 }, { 0x4100, 0 }, { 0x4200, 0 }, { 0x4300, 0 },
	{ 0x4400, 0 }, { 0x4500, 0 }, { 0x4600, 0 }, { 0x4700, 0 },
	{ 0x4b00, 0 }, { 0x4c00, 0 }, { 0x4d00, 0 }, { 0x5000, 0 },
	{ 0x5100, 0 }, { 0x5200, 0 }, { 0x5300, 0 }, { 0x5400, 0 },
	{ 0x5500, 0 }, { 0x5600, 0 }, { 0x5700, 0 }, { 0x5800, 0 },
	{ 0x5900, 0 }, { 0x5a00, 0 }, { 0x5b00, 0 }, { 0x5c00, 0 },
	{ 0x5d00, 0 }, { 0x5e00, 0 }, { 0x5f00, 0 }, { 0x6000, 0 },
	{ 0x6100, 0 }, { 0x6200, 0 }, { 0x6300, 0 }, { 0x6400, 0 },
	{ 0x6500, 0 }, { 0x6600, 0 }, { 0x6700, 0 }, { 0x6800, 0 },
	{ 0x6900, 0 }, { 0x6a00, 0 }, { 0x6b00, 0 }, { 0x7100, 0 },
	{ 0x7500, 0 }, { 0x7800, 0 }, { 0x7900, 0 }, { 0x7a00, 0 },
	{ 0x7b00, 0 }, { 0x7d00, 0 }, { 0x7f00, 0 }, { 0x6f00, 0x0c40 },
	{ 0x6f00, 0x0d40 }, { 0x6f00, 0x0c00 }, { 0x6f00, 0x0d00 }, { 0x6f00, 0x0e00 },
	{ 0x6f00, 0x0f00 }, { 0x6f00, 0x0c20 }, { 0x6f00, 0x0d20 }, { 0x6f00, 0x0e20 },
	{ 0x6f00, 0x0f20 }, { 0x4800, 0 }, { 0x4900, 0 }, { 0x4a00, 0 },
	{ 0x7300, 0 },
};
static_assert(std::size(source_domains) == 125);
constexpr unsigned domain_cases = 125 * 15 * 16;
}

void tms320c54x_test_state::start_source_domains(unsigned variant)
{
	m_cpu->reset();
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	m_idle_fixture_tick = 0;
	m_pipeline_replay = false;
	unsigned const context = (variant / 15) % 16;
	auto const instruction = source_domains[variant / (15 * 16)];
	unsigned const mode_index = variant % 15;
	u16 const mode = mode_index < 8 ? 0x98 + mode_index : 0xf9 + mode_index - 8;
	u32 const base = context & 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(base + 0x600, instruction.opcode | mode);
	program.write_word(base + 0x601, instruction.operation && mode < 0xe0 ? instruction.operation : 0x1500);
	program.write_word(base + 0x602, instruction.operation && mode >= 0xe0 ? instruction.operation : 3);
	program.write_word(base + 0x603, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1804);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (context & 1 ? 0x20 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xabcd);
	m_cpu->set_state_int(tms320c54x_device::STATE_RC, 7);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1600);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	for (unsigned i = 0; i < 8; ++i)
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x1400 + i);
	m_rptbd_counts = {};
	m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "domain_data_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "domain_data_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.writes; });
	m_operand_program_tap = program.install_write_tap(0, 0x7fffff, "domain_program_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_extensions_reads; });
	m_operand_io_tap = m_cpu->space(AS_IO).install_readwrite_tap(0, 0xffff, "domain_io_access",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_register_writes; },
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_register_writes; });
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(!(context & 4));
	m_phase = 311200;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_source_domains()
{
	if (m_phase == 311210)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 311200;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase != 311200)
		return false;
	unsigned const variant = m_pipeline_peripheral_case;
	unsigned const context = (variant / 15) % 16;
	unsigned const tick = ++m_idle_fixture_tick, checkpoint = context & 8 ? 10 : 1;
	expect(!m_rptbd_counts.reads && !m_rptbd_counts.writes &&
			!m_pipeline_extensions_reads && !m_pipeline_register_writes,
			"Source-domain rejection precedes data/program/I-O effects");
	expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_RC) == 7 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1600 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1804 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (context & 1 ? 0x20 : 0)),
			"Rejected operand preserves accumulators, T, repeat/stack and status");
	for (unsigned i = 0; i < 8; ++i)
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x1400 + i,
				"Rejected source preserves every address register");
	if (tick == checkpoint && !m_pipeline_replay)
	{
		m_saved_pipeline.str({});
		m_saved_pipeline.clear();
		expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE,
				"Save source-domain exclusion before or after rejection");
	}
	if (tick == 20)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
				"Atomic and staged source-domain aliases are rejected");
		if (!m_pipeline_replay)
		{
			m_pipeline_replay = true;
			m_idle_fixture_tick = checkpoint;
			m_saved_pipeline.clear();
			m_saved_pipeline.seekg(0);
			m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE,
					"Restore source-domain exclusion without operand effects");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			m_phase = 311210;
			m_check_timer->enable(true);
			return true;
		}
		m_pipeline_operand_tap.remove();
		m_pipeline_write_tap.remove();
		m_operand_program_tap.remove();
		m_operand_io_tap.remove();
		m_cpu->set_pipeline_frontend(false);
		if (variant + 1 < domain_cases)
		{
			start_source_domains(variant + 1);
			return true;
		}
		osd_printf_info("TMS320C54x source-domain exclusions: PASS cases=30000 restore=30000 instruction_forms=125 smem_classes=111 shifted_reads=10 mmr_sources=4 modes=98..9f,f9..ff cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_accesses=0 silicon_trap_claim=0\n");
		throw emu_fatalerror(3, "TMS320C54x isolated source-domain exclusions complete");
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
	return true;
}

} // namespace c54x_test
