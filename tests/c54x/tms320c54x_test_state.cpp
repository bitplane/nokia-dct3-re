// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::xf_w(int state)
{
	m_xf_pin = bool(state);
}


void tms320c54x_test_state::machine_start()
{
	save_item(NAME(m_stack_mmr));
	m_check_timer = timer_alloc(FUNC(tms320c54x_test_state::check_results), this);
	save_item(NAME(m_shifted_bus_word));
	save_item(NAME(m_shifted_bsp.control));
	save_item(NAME(m_shifted_bsp.receive));
	save_item(NAME(m_shifted_bsp.receive_ready));
	save_item(NAME(m_shifted_bsp.receive_overrun));
}

void tms320c54x_test_state::machine_reset()
{
	using self = tms320c54x_test_state;
	m_result_owner = nullptr;
	m_pipeline_counter_count = 0;
	m_context_coverage = {};
	if (start_profile(*this, machine().options().bios(), {
		{ {"rom4-context"}, [](self &test) { test.m_result_owner = &self::check_rom4_context_results; test.start_pipeline_mu4_prologue(); } },
		{ {"pop-irq"}, [](self &test) { test.start_pipeline_mmr_pop(); } },
		{ {"mvpd-extended"}, [](self &test) { test.start_extended_mvpd(); } },
		{ {"mvpd-controls"}, [](self &test) { test.start_pipeline_mvpd(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"add-bk", "add-absolute", "add-sp"}, [](self &test) { test.start_pipeline_absolute_add(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"store-observe"}, [](self &test) { test.start_pipeline_store_case(); } },
		{ {"push-observe"}, [](self &test) { test.start_pipeline_xpc_push(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"indirect-load"}, [](self &test) { test.start_pipeline_indirect_case(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"st-dagen"}, [](self &test) { test.start_constant_dagen(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"popd-ram"}, [](self &test) { test.start_pipeline_data_pop(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"stm-ind", "stm-ind-edge"}, [](self &test) { test.start_indirect_stm(); } },
		{ {"stm-low-ram"}, [](self &test) { test.start_indirect_stm(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
	}))
		return;
	if (absolute_memory_dp()) { start_pipeline_memory_dp(); return; }
	if (start_profile(*this, machine().options().bios(), {
	}))
		return;
	if (!strcmp(machine().system().name, "tms54test") &&
			profile_is("timer"))
	{
		throw emu_fatalerror("Legacy timer entry retired: run stopped_timer_reload, timer_prescaler and timer_irq_mask");
	}
	if (!strcmp(machine().system().name, "tms54rom4"))
	{
		auto &data = m_cpu->space(AS_DATA);
		u16 const *const initial = &memregion("dspdata")->as_u16();
		for (unsigned address = 0; address != 0x10000; ++address)
			// A memory snapshot seeds RAM, not live CPU/peripheral registers.
			if (data.get_write_ptr(address)) data.write_word(address, initial[address]);
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(rom4_codec_is("efr-dtx-decoder") || rom4_codec_is("efr-erasure") || rom4_codec_is("efr-loss-observe") ||
			 rom4_codec_is("efr-homing") || rom4_codec_is("efr-home-errors") || rom4_codec_is("efr-invalid-sid") || rom4_codec_is("efr-lost-sid") || rom4_codec_is("efr-lsid-observe")))
	{
		enable_rom4_codec_pipeline(rom4_codec_is("efr-lost-sid") || rom4_codec_is("efr-lsid-observe") ? "EFR lost SID decoder" : rom4_codec_is("efr-dtx-decoder") ? "EFR DTX decoder" : rom4_codec_is("efr-invalid-sid") ? "EFR invalid SID decoder" : rom4_codec_is("efr-erasure") || rom4_codec_is("efr-loss-observe") ? "EFR erasure decoder" : "EFR homing decoder");
		start_rom4_efr_decoder(0, true);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(rom4_codec_is("efr-encoder") || rom4_codec_is("efr-dtx-encoder") || rom4_codec_is("efr-enc-homing")))
	{
		enable_rom4_codec_pipeline(rom4_codec_is("efr-dtx-encoder") ? "EFR DTX encoder" : rom4_codec_is("efr-enc-homing") ? "EFR homing encoder" : "EFR encoder");
		start_rom4_efr_encoder(0, true);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("efr-decoder") || profile_is("efr-decoder-pipe")))
	{
		start_rom4_delayed_repeat(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("efr-packing"))
	{
		start_rom4_efr_packing(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("efr-crc"))
	{
		start_rom4_efr_crc(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("fr-encoder") || profile_is("fr-encoder-pipe")))
	{
		if (profile_is("fr-encoder-pipe"))
		{
			m_cpu->set_pipeline_frontend(true);
			osd_printf_info("ROM4 FR encoder frontend: staged handset_audio_claim=0\n");
		}
		start_rom4_fr_encoder(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("fr-decoder") || profile_is("fr-decoder-pipe")))
	{
		if (profile_is("fr-decoder-pipe"))
		{
			m_cpu->set_pipeline_frontend(true);
			osd_printf_info("ROM4 FR decoder frontend: staged handset_audio_claim=0\n");
		}
		start_rom4_fr_decoder(0, true);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("fr-packing"))
	{
		start_rom4_fr_packing(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("crc-packed"))
	{
		start_rom4_crc_packed(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("crc-helper"))
	{
		start_rom4_crc_helper(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") && rom4_codec_is("rf-pairs"))
	{
		enable_rom4_codec_pipeline("RF pairs");
		start_rom4_pairs(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") && rom4_codec_is("rf-bitmap"))
	{
		enable_rom4_codec_pipeline("RF bitmap");
		start_rom4_bitmap(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("cold") || profile_is("cold-pipeline")))
	{
		bool const staged = profile_is("cold-pipeline");
		m_phase = staged ? rom4_staged_cold_phase : 4;
		if (staged) m_cpu->set_pipeline_frontend(true);
		m_rom4_checks = 0;
		m_check_timer->adjust(attotime::from_usec(100));
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4"))
	{
		m_rom4_checks = 0;
		auto &data = m_cpu->space(AS_DATA);
		u16 const *const drom = &memregion("dspdrom")->as_u16();
		for (unsigned address = 0xb000; address != 0xf000; ++address)
			data.write_word(address, drom[address]);
		// The sparse entry snapshot predates the firmware-provided challenge.
		// Supply the factory-profile record encoded for COBBA 00160010 while
		// retaining the deterministic peripheral-free entry state.
		static constexpr u16 challenge[] = {
			0xd6fb, 0x4394, 0xe437, 0xda16, 0x9668, 0x964f, 0x5cd4,
			0x32fe, 0x5be2, 0xdba6, 0x9643, 0x82d7, 0x0000, 0x0000
		};
		for (unsigned i = 0; i != std::size(challenge); ++i)
			data.write_word(0x0825 + i, challenge[i]);
		m_phase = 2;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x4b73}, {cpu_regs::STATE_SP, 0x1ec3}, {cpu_regs::STATE_ST0, 0x281f},
			{cpu_regs::STATE_ST1, 0x4101}, {cpu_regs::STATE_PMST, 0xffac}, {cpu_regs::STATE_BK, 0x0052},
			{cpu_regs::STATE_T, 0x000e}, {cpu_regs::STATE_A, 0x0000004b73}, {cpu_regs::STATE_B, 0xfffffffffe},
		});
		static constexpr u16 ar[] = {
			0x0001, 0xb0bc, 0x0825, 0x06e3,
			0x001a, 0x12ca, 0x06e3, 0x0000
		};
		for (unsigned i = 0; i != std::size(ar); ++i)
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, ar[i]);
		m_check_timer->adjust(attotime::from_usec(100));
		return;
	}

	throw emu_fatalerror("Legacy generic phase machine retired: select an independent C54x section");
}

void tms320c54x_test_state::program_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
	map(0x010000, 0x03ffff).ram();
	map(0x7f0000, 0x7fffff).ram();
}

void tms320c54x_test_state::data_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
	map(0x0060, 0x0061).rw(FUNC(tms320c54x_test_state::repeat_irq_r), FUNC(tms320c54x_test_state::stack_mmr_w));
}

void tms320c54x_test_state::plain_data_map(address_map &map)
{
	map(0, 0xffff).ram();
}

void tms320c54x_test_state::io_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
	map(0x0123, 0x0123).r(FUNC(tms320c54x_test_state::test_port_r));
	map(0x0124, 0x0124).w(FUNC(tms320c54x_test_state::test_port_w));
}

void tms320c54x_test_state::rom4_program_map(address_map &map)
{
	map(0x0000, 0xffff).rom().region("dspprg", 0);
	map(0x0f00, 0x0f00).r(FUNC(tms320c54x_test_state::rom4_loader_entry_r));
}

void tms320c54x_test_state::rom4_data_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
}

void tms320c54x_test_state::expect(bool condition, const char *message)
{
	if (!condition)
	{
		unsigned case_index = m_pipeline_peripheral_case, tick = m_idle_fixture_tick;
		bool replay = m_pipeline_replay;
		auto progress = [&](auto const &fixture) {
			case_index = fixture.variant;
			tick = fixture.tick;
			replay = fixture.replay;
		};
		switch (m_phase)
		{
		case 255320: case 255330: case 255331:
			progress(m_rptbd); break;
		}
		osd_printf_info("c54x_failure: phase=%u case=%u tick=%u replay=%u pc=%06x rc=%04x ar1=%04x a=%010llx b=%010llx st0=%04x illegal=%u reads=%u writes=%u\n",
			m_phase, case_index, tick, unsigned(replay), unsigned(m_cpu->state_int(STATE_GENPC)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_RC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
			(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)),
			m_pipeline_operand_reads, m_pipeline_store_writes);
		throw emu_fatalerror("TMS320C54x core conformance: %s", message);
	}
}

void tms320c54x_test_state::expect_opcode(u16 opcode, bool condition, const char *message)
{
	expect(condition, message);
	// Deduplicate only the diagnostic record, never the assertion itself.
	if (!m_asserted_opcodes.test(opcode))
	{
		m_asserted_opcodes.set(opcode);
		logerror("[opassert] op=%04x\n", opcode);
	}
}

void tms320c54x_test_state::test(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	if (!strcmp(config.options().bios(), "rpt-smem") || !strcmp(config.options().bios(), "rpt-ld") || !strcmp(config.options().bios(), "rpt-sfta") || !strcmp(config.options().bios(), "rpt-ld-block"))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) { m_observed_instructions.emplace_back(address, opcode); });
	m_cpu->set_extended_program(true);
	m_cpu->set_irq_acknowledge_callback([this](device_t &, int line) {
		observe_context_irq(line);
		if (m_phase == 251090)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 6, "Indirect load drains while vector IACK occurs");
			++m_vector_acks;
		}
		if (m_phase == 251070)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 6,
				"Direct store retires during vector Fetch/IACK");
			++m_vector_acks;
		}
		return 0;
	}, "vector_acknowledge");
	m_cpu->xf_out_cb().set(FUNC(tms320c54x_test_state::xf_w));
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::program_map);
	// The low-page copy must see RAM at 0060, not the legacy IRQ probe.
	else if (!strcmp(config.options().bios(), "stm-ind") || !strcmp(config.options().bios(), "stm-ind-edge") || !strcmp(config.options().bios(), "stm-low-ram") || !strcmp(config.options().bios(), "bitt-ctrl")) m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::plain_data_map);
	else m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::data_map);
	m_cpu->set_addrmap(AS_IO, &tms320c54x_test_state::io_map);
}

void tms320c54x_test_state::rom4(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	bool const trace_encoder = !strcmp(config.options().bios(),"fr-encoder") || !strcmp(config.options().bios(),"fr-encoder-pipe");
	if (config.options().verbose() && (trace_encoder || !strcmp(config.options().bios(), "fr-decoder") || !strcmp(config.options().bios(), "fr-decoder-pipe")))
		m_cpu->instruction_observer_cb().set([this,trace_encoder](offs_t address, u16 opcode) {
			if (m_phase != (trace_encoder ? 74002U : 72017U) || m_rom4_codec_replay) return;
			if (++m_rom4_fr_trace_count <= 200000) trace_rom4_fr_state("execute", address, opcode);
		});
	if (config.options().verbose() && (!strcmp(config.options().bios(), "efr-encoder") || !strcmp(config.options().bios(), "efr-encoder-pipe")))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) {
			if (m_phase == 304000 && ++m_rom4_fr_trace_count <= 8192)
				trace_rom4_fr_state("efr_reset_execute", address, opcode);
			else if (m_phase == 304001 && !m_rom4_codec_replay && ++m_rom4_fr_trace_count <= 200000)
				trace_rom4_fr_state("efr_frame_execute", address, opcode);
		});
	if (!strcmp(config.options().bios(), "efr-loss-observe") || !strcmp(config.options().bios(), "efr-loss-pipe"))
		m_cpu->instruction_observer_cb().set([this](offs_t address,u16 opcode) {
			observe_rom4_efr_state(address,opcode);
			if (machine().options().verbose() && m_phase == 302009 && !m_rom4_codec_replay && ++m_rom4_fr_trace_count <= 200000)
				trace_rom4_fr_state("efr_loss_execute",address,opcode);
		});
	if (!strcmp(config.options().bios(), "efr-home-errors") || !strcmp(config.options().bios(), "efr-herr-pipe"))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) {
			// Original recovery may edit an input parameter before the full DHF
			// comparison. Observe it; never repair the parameter or reset flag.
			if (!m_rom4_codec_replay && (m_phase == 302002 || m_phase == 302025 || m_phase == 302048))
			{
				auto &data = m_cpu->space(AS_DATA);
				if (address == 0xc8dc || address == 0xc930)
					osd_printf_info("ROM4 homing recovery parameter: frame=%u pc=%04x actual=%04x table=%04x\n",
						m_phase - 302001, unsigned(address), data.read_word(0x1371), data.read_word(0xeb59));
				if (address == 0x28dc && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1371 && data.read_word(0x1371) == 0x000b)
				{
					expect(opcode == 0x9e0f, "Original recovery store is SACCD A, *AR2, BLEQ");
					osd_printf_info("ROM4 homing recovery store: frame=%u a=%010llx b=%010llx parameter=%04x\n",
						m_phase - 302001, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
						(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), data.read_word(0x1371));
				}
			}
			if (address == 0xc941 && !m_rom4_codec_replay && m_phase >= 302001 && m_phase <= 302049)
			{
				expect(opcode == 0x4a07, "Homing reset observation is the original routine entry");
				++m_rom4_homing_resets;
			}
		});
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::rom4_program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::rom4_data_map);
}

} // namespace c54x_test
