// license:BSD-3-Clause
// copyright-holders:Gaz

// Isolated original MU4 routine execution, not a DA150 or handset boot profile.
#include "emu.h"
#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "machine/nandflash.h"
#include "tms320c54x_dma.h"
#include "tms320c54x_mcbsp.h"
#include "tlv320aic23.h"
#include "speaker.h"
#include <array>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <map>

namespace {
class mu4_storage_test_state : public driver_device
{
public:
	mu4_storage_test_state(machine_config const &config, device_type type, char const *tag)
				: driver_device(config, type, tag), m_cpu(*this, "cpu"), m_dma(*this, "dma"), m_mcbsp(*this, "mcbsp1"), m_mcbsp0(*this, "mcbsp0"), m_mcbsp2(*this, "mcbsp2"), m_program_ram(*this, "program_ram"), m_dma_sink(*this, "dma_sink"), m_nand(*this, "nand"), m_cinit(*this, "cinit"),
		  m_initdisk(*this, "initdisk"), m_disk_cinit(*this, "disk_cinit"), m_disk_helpers(*this, "disk_helpers"),
		  m_disk_vectors(*this, "disk_vectors"), m_segment(*this, "segment") { }
	void test(machine_config &config)
	{
		// Initial configured rate is replaced by the evidenced compatibility reset.
		TMS320C54X(config, m_cpu, 13'000'000);
		// Sheet 4: R101 pulls CLKMD1/2/3 high. SPRS095P table 3-8,
		// matching C5416 PGE pin/MMIO layout: 111 -> 0000, Y101 / 2.
		// This is a compatible reset contract, not DA150 die identification.
		m_cpu->set_pll_reference_clock(11'995'200);
		m_cpu->set_follow_clkmd(true);
		m_cpu->set_reset_clkmd(0);
		// Only the fail-closed original-firmware diagnostic opts into bus waits.
		// Program/data bank ownership and physical READY wiring remain unvalidated.
		if (!strcmp(config.options().bios(), "bootrecstage") || !strcmp(config.options().bios(), "bootrecmeta") || !strcmp(config.options().bios(), "bootrecac") || !strcmp(config.options().bios(), "bootopenstage") || !strcmp(config.options().bios(), "bootplaystage") || !strcmp(config.options().bios(), "bootplaystop")) m_cpu->set_c5416_bus_profile(true);
		m_cpu->system_clock_cb().set([this](u32 hz) {
			m_dma->set_unscaled_clock(hz);
			m_mcbsp->set_unscaled_clock(hz);
			m_mcbsp0->set_unscaled_clock(hz);
			m_mcbsp2->set_unscaled_clock(hz);
			logerror("mu4_system_clock_selected: cpu=%u dma=%u mcbsp0=%u mcbsp1=%u mcbsp2=%u codec=%u t=%.9f\n",
				hz, m_dma->clock(), m_mcbsp0->clock(), m_mcbsp->clock(), m_mcbsp2->clock(),
				subdevice<tlv320aic23_device>("codec")->clock(), machine().time().as_double());
		});
		m_cpu->set_extended_program(true);
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) {
			if (recorder_ac_profile() && m_recorder_overlay_checked) observe_recorder_ingress(address,opcode);
			if (recorder_ac_profile() && m_recorder_overlay_checked) observe_recording_transform(address,opcode);
			if (measurement_profile() && (address == 0x3b021 || address == 0x3b02a || address == 0x3b034 || address == 0x3b038))
			{
				auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
				unsigned const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
				unsigned const step = address == 0x3b021 ? 0 : address == 0x3b02a ? 1 : address == 0x3b034 ? 2 : 3;
				if (m_measurement_blocks != 6 || m_measurement_branch_mask != (1U << step) - 1 ||
					opcode != (step == 0 ? 0x760b : step == 3 ? 0x4818 : 0xf847))
					fatalerror("MU4 measurement response branch order or instruction identity changed");
				if (step == 1 || step == 2)
				{
					u64 const expected = (u64(10'000'000) - m_measurement_expected[step - 1]) & 0xffffffffffULL;
					if ((m_cpu->state_int(tms320c54x_device::STATE_A) & 0xffffffffffULL) != expected)
						fatalerror("MU4 measurement response comparison disagrees with checked sample energies");
				}
				if (step == 3 && data.read_word((sp + 11) & 0xffff) != expected_measurement_result())
					fatalerror("MU4 measurement response selection disagrees with decoded threshold contract");
				m_measurement_branch_mask |= 1U << step;
				logerror("mu4_measurement_result_branch: address=%06x opcode=%04x a=%010llx left=%04x,%04x right=%04x,%04x result=%04x blocks=%u t=%.9f identity=Execute_entry\n",
					unsigned(address), opcode, static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
					data.read_word(0xb64a), data.read_word(0xb64b), data.read_word(0xb64c), data.read_word(0xb64d),
					data.read_word((sp + 11) & 0xffff), m_measurement_blocks, machine().time().as_double());
			}
			if (playback_profile())
			{
				// Execute reports logical XPC; the existing bus tap sees OVLY's physical 006d62.
				if (address == 0x0e41 || address == 0x26d62 || address == 0x2080)
				{
					unsigned const which = address == 0x26d62 ? 1 : address == 0x2080 ? 2 : 0;
					if (m_playback_execute_entries[which]++ < 16)
					{
						logerror("mu4_playback_execute_entry: address=%06x opcode=%04x t=%.9f sp=%04x pmst=%04x\n", unsigned(address), opcode, machine().time().as_double(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
						auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
						unsigned const p = m_playback_history_next;
						logerror("mu4_playback_entry_transfer: target=%06x index=%u p0=%06x/%04x p1=%06x/%04x p2=%06x/%04x p3=%06x/%04x a=%010llx lifecycle=%04x mode=%04x continuation=%04x identity=Execute_entry\n", unsigned(address), m_playback_execute_entries[which] - 1, m_playback_history_pc[p], m_playback_history_word[p], m_playback_history_pc[(p + 1) & 3], m_playback_history_word[(p + 1) & 3], m_playback_history_pc[(p + 2) & 3], m_playback_history_word[(p + 2) & 3], m_playback_history_pc[(p + 3) & 3], m_playback_history_word[(p + 3) & 3], static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL, data.read_word(0x373e), data.read_word(0x3750), data.read_word(0x3762));
					}
				}
				if (m_playback_overlay_checked)
				{
					++m_playback_execute_steps;
					if (m_playback_execute_entries[1] == 1 && address >= 0x2936d && address <= 0x2940d)
						observe_decoder_gate(address,opcode);
					if (m_playback_execute_entries[1] == 1 &&
						((address >= 0x2044a && address <= 0x205f3) ||
						 (address >= 0x27a99 && address <= 0x27a9f) ||
						 (address >= 0x28feb && address <= 0x28fee) ||
						 (address >= 0x29332 && address <= 0x293ed)))
						observe_context_calls(address,opcode);
					if (m_playback_execute_entries[1] == 1 && address >= 0x20553 && address <= 0x20586)
						observe_exchange_branch(address,opcode);
					// Whole-session counters must not widen the eight-call arithmetic snapshot budget.
					bool const detailed = m_playback_execute_entries[1] == 1 && m_playback_processing[0] <= 8;
					if (detailed && (address == 0x24a7e || address == 0x24af3))
						observe_playback_tail(address,opcode);
					if (detailed &&
						((address >= 0x24a60 && address <= 0x24a64) || address == 0x24af8 || address == 0x24b13 ||
						 address == 0x24b3e || address == 0x24b59 || address == 0x24955 || address == 0x2496e))
						observe_playback_window_callback(address,opcode);
					if (detailed && (address == 0x24a46 || address == 0x24a48))
						observe_playback_scale(address,opcode);
					if (detailed && (address == 0x24a70 || address == 0x24a72))
						observe_playback_window(address,opcode);
					if (detailed &&
						(address == 0x2491e || address == 0x2494e || address == 0x24950))
						observe_playback_rotation(address,opcode,false);
					if (detailed &&
						(address == 0x2490f || address == 0x24934 || address == 0x24914 || address == 0x24939))
						observe_playback_rotation(address,opcode,true);
					if (detailed &&
						(address == 0x24919 || address == 0x24944 || address == 0x2491e || address == 0x24946 ||
						 address == 0x2400a || address == 0x2401e || address == 0x24036 || address == 0x24051 ||
						 address == 0x2406c || address == 0x2408b || address == 0x240a6 || address == 0x240be))
						observe_playback_transform(address,opcode);
					if (m_playback_execute_entries[1] == 1 &&
						((address >= 0x23de5 && address <= 0x23e2c) || address == 0x248b1))
						observe_playback_processing(address,opcode);
					if (m_playback_execute_entries[1] == 1 && opcode == 0x62e6 &&
						(address == 0x2433a || address == 0x24340 || address == 0x24346))
					{
						unsigned const site = (address - 0x2433a) / 6;
						if (m_playback_bit_multiply_samples[site]++ < 8)
						{
							u16 source = 0, operand = 0, immediate = 0;
							bool const valid = m_cpu->observe_execute_memory_multiply(address, opcode, source, operand, immediate);
							logerror("mu4_playback_bit_multiply: address=%06x ordinal=%u retained=%u source=%04x operand=%04x immediate=%04x st1=%04x t=%.9f identity=Execute_entry observer_writes=0 retirement=unclaimed\n",
								unsigned(address), m_playback_bit_multiply_samples[site], unsigned(valid), source, operand, immediate,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), machine().time().as_double());
						}
					}
					if (m_playback_execute_entries[1] == 1 && !m_playback_optional_started && address == 0x2532e && opcode == 0xe801)
					{
						m_playback_optional_started = m_playback_optional_active = true;
						logerror("mu4_playback_optional_window: start=02532e t=%.9f identity=Execute_entry observer_writes=0\n", machine().time().as_double());
					}
					if (m_playback_optional_active &&
						((address == 0x25359 && opcode == 0xf845) || (address == 0x24538 && opcode == 0xeefe)))
					{
						m_playback_optional_active = false;
						logerror("mu4_playback_optional_terminal: end=%06x samples=%u capped=%u t=%.9f identity=Execute_entry observer_writes=0 audio_acceptance=0\n",
							unsigned(address), m_playback_bit_source_samples[3], m_playback_bit_source_samples[3] == 128, machine().time().as_double());
					}
					if (m_playback_execute_entries[1] == 1 &&
						((address == 0x2510c && opcode == 0xed00) || (address == 0x25142 && opcode == 0xf4e4)))
						observe_playback_bits(address);
					if (m_playback_execute_entries[1] == 1 && address == 0x24538 && opcode == 0xeefe && ++m_playback_restore_samples <= 8)
					{
						unsigned const p = m_playback_history_next;
						logerror("mu4_playback_restore_history: ordinal=%u address=024538 opcode=eefe p0=%06x/%04x p1=%06x/%04x p2=%06x/%04x p3=%06x/%04x t=%.9f identity=Execute_entry selection=entry_address_context receiver_ownership=unvalidated observer_writes=0\n",
							m_playback_restore_samples, m_playback_history_pc[p], m_playback_history_word[p],
							m_playback_history_pc[(p + 1) & 3], m_playback_history_word[(p + 1) & 3],
							m_playback_history_pc[(p + 2) & 3], m_playback_history_word[(p + 2) & 3],
							m_playback_history_pc[(p + 3) & 3], m_playback_history_word[(p + 3) & 3], machine().time().as_double());
						if (m_playback_restore_samples == 1)
							report_playback_path("mu4_playback_restore_path", "before_first_restore_wrapper");
					}
					if (m_playback_execute_entries[1] == 1 &&
						((address >= 0x23e64 && address <= 0x23ef7) || (address >= 0x293da && address <= 0x293ed)))
					{
						std::array<u32, 6> const sites{0x23e64, 0x23e84, 0x23e99, 0x23ef7, 0x293da, 0x293ed};
						std::array<u16, 6> const words{0x4a11, 0xf020, 0x62f8, 0xf4e4, 0x10f8, 0xf982};
						auto const found = std::find(sites.begin(), sites.end(), address);
						if (found != sites.end())
						{
							unsigned const site = found - sites.begin();
							if (opcode != words[site]) ++m_playback_method_mismatch;
							else if (++m_playback_method_entries[site] <= 8)
								logerror("mu4_playback_method_entry: site=%u address=%06x opcode=%04x ordinal=%u t=%.9f identity=Execute_entry observer_writes=0\n",
									site, unsigned(address), opcode, m_playback_method_entries[site], machine().time().as_double());
						}
					}
					if (m_playback_execute_entries[1] == 1 && address == 0x201b3 && opcode == 0xf7b8 && ++m_playback_notify_samples <= 8)
					{
						unsigned const p = m_playback_history_next;
						logerror("mu4_playback_notify_history: ordinal=%u address=0201b3 opcode=f7b8 p0=%06x/%04x p1=%06x/%04x p2=%06x/%04x p3=%06x/%04x t=%.9f identity=Execute_entry selection=entry_address_context receiver_ownership=unvalidated observer_writes=0\n",
							m_playback_notify_samples, m_playback_history_pc[p], m_playback_history_word[p],
							m_playback_history_pc[(p + 1) & 3], m_playback_history_word[(p + 1) & 3],
							m_playback_history_pc[(p + 2) & 3], m_playback_history_word[(p + 2) & 3],
							m_playback_history_pc[(p + 3) & 3], m_playback_history_word[(p + 3) & 3], machine().time().as_double());
						logerror("mu4_playback_notify_context: ordinal=%u ar1=%04x ar2=%04x a=%010llx b=%010llx t=%.9f context_only=1 observer_writes=0\n",
							m_playback_notify_samples, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)) & 0xffffffffffULL, machine().time().as_double());
					}
					if (m_playback_execute_entries[1] == 1 && address == 0x20478 && opcode == 0x10e4 &&
						m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xd7b0 && ++m_playback_callback_samples <= 8)
					{
						unsigned const p = m_playback_history_next;
						logerror("mu4_playback_callback_history: ordinal=%u address=020478 opcode=10e4 ar4=d7b0 p0=%06x/%04x p1=%06x/%04x p2=%06x/%04x p3=%06x/%04x t=%.9f identity=Execute_entry selection=live_ar4_context receiver_ownership=unvalidated observer_writes=0\n",
							m_playback_callback_samples, m_playback_history_pc[p], m_playback_history_word[p],
							m_playback_history_pc[(p + 1) & 3], m_playback_history_word[(p + 1) & 3],
							m_playback_history_pc[(p + 2) & 3], m_playback_history_word[(p + 2) & 3],
							m_playback_history_pc[(p + 3) & 3], m_playback_history_word[(p + 3) & 3], machine().time().as_double());
						if (m_playback_callback_samples == 1)
							report_playback_path("mu4_playback_path", "before_first_d7b0_callback");
					}
					if (m_playback_execute_entries[1] == 1 && address == 0x20419 && opcode == 0xf6be)
					{
						unsigned const p = m_playback_history_next;
						for (unsigned i = 0; i < 4; ++i)
						{
							m_playback_activation_pc[i] = m_playback_history_pc[(p + i) & 3];
							m_playback_activation_word[i] = m_playback_history_word[(p + i) & 3];
						}
						m_playback_activation_time = machine().time().as_double();
					}
					if (m_playback_execute_entries[1] == 1 && address == 0x202d7 && opcode == 0x487e &&
						m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x8044 && ++m_playback_enqueue_samples <= 8)
					{
						unsigned const p = m_playback_history_next;
						logerror("mu4_playback_activation_history: ordinal=%u address=020419 opcode=f6be paired_ar2=8044 p0=%06x/%04x p1=%06x/%04x p2=%06x/%04x p3=%06x/%04x t=%.9f identity=Execute_entry selection=paired_enqueue_ar2_context receiver_ownership=unvalidated observer_writes=0\n",
							m_playback_enqueue_samples, m_playback_activation_pc[0], m_playback_activation_word[0],
							m_playback_activation_pc[1], m_playback_activation_word[1],
							m_playback_activation_pc[2], m_playback_activation_word[2],
							m_playback_activation_pc[3], m_playback_activation_word[3], m_playback_activation_time);
						logerror("mu4_playback_enqueue_history: ordinal=%u address=0202d7 opcode=487e ar2=8044 p0=%06x/%04x p1=%06x/%04x p2=%06x/%04x p3=%06x/%04x t=%.9f identity=Execute_entry selection=live_ar2_context receiver_ownership=unvalidated observer_writes=0\n",
							m_playback_enqueue_samples, m_playback_history_pc[p], m_playback_history_word[p],
							m_playback_history_pc[(p + 1) & 3], m_playback_history_word[(p + 1) & 3],
							m_playback_history_pc[(p + 2) & 3], m_playback_history_word[(p + 2) & 3],
							m_playback_history_pc[(p + 3) & 3], m_playback_history_word[(p + 3) & 3], machine().time().as_double());
					}
					if (m_playback_execute_entries[1] == 1 && address == 0x20478 && opcode == 0x10e4 &&
						m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x804a && ++m_playback_switch_samples <= 8)
					{
						unsigned const p = m_playback_history_next;
						logerror("mu4_playback_switch_history: ordinal=%u address=020478 opcode=10e4 ar4=804a p0=%06x/%04x p1=%06x/%04x p2=%06x/%04x p3=%06x/%04x t=%.9f identity=Execute_entry selection=live_ar4_context receiver_ownership=unvalidated observer_writes=0\n",
							m_playback_switch_samples, m_playback_history_pc[p], m_playback_history_word[p],
							m_playback_history_pc[(p + 1) & 3], m_playback_history_word[(p + 1) & 3],
							m_playback_history_pc[(p + 2) & 3], m_playback_history_word[(p + 2) & 3],
							m_playback_history_pc[(p + 3) & 3], m_playback_history_word[(p + 3) & 3], machine().time().as_double());
					}
					// AA99 identity matters: AA22 has different code at these addresses.
					if (address == 0x2a2c6 && opcode == 0x771a) ++m_playback_filter[0];
					if (address == 0x2a2cc && opcode == 0xe900) ++m_playback_filter[1];
					if (address == 0x2a2ea && opcode == 0xf4e4) ++m_playback_filter[2];
					if (m_playback_execute_entries[1] == 1 && address >= PLAYBACK_RANGE_FIRST && address < PLAYBACK_RANGE_FIRST + PLAYBACK_RANGE_WORDS)
					{
						unsigned const index = address - PLAYBACK_RANGE_FIRST;
						if (!m_playback_range_counts[index]++) m_playback_range_opcodes[index] = opcode;
						else if (m_playback_range_opcodes[index] != opcode) ++m_playback_range_changes;
					}
					if (m_playback_execute_entries[1] == 1 && ((address >= 0x205ac && address <= 0x205d0) || (address >= 0x2a10c && address <= 0x2a2c6)))
						observe_playback_parent(address, opcode);
					if (m_playback_execute_entries[1] == 1)
					{
						m_playback_path_pc[m_playback_path_next] = address;
						m_playback_path_word[m_playback_path_next] = opcode;
						m_playback_path_next = (m_playback_path_next + 1) & 127;
						if (m_playback_path_count < 128) ++m_playback_path_count;
					}
				}
				// Retain executed bytes: current overlay memory may already differ.
				m_playback_history_pc[m_playback_history_next] = address;
				m_playback_history_word[m_playback_history_next] = opcode;
				m_playback_history_next = (m_playback_history_next + 1) & 3;
			}
			if (cold_catalog_profile() && !m_playback_decoder_loads)
			{
				if (address == 0x3941d) ++m_cold_catalog_scans;
				if (address == 0x39fe9) ++m_cold_metadata_reads;
				if (address == 0x395af) observe_cold_title(opcode);
				if (playback_profile())
				{
					if (address == 0x3b3b0) ++m_playback_requests;
					if (address == 0x2a1ed) ++m_playback_inspections;
					if (address == 0x3a9c3) ++m_playback_decoder_loads;
				}
			}
			if (playback_profile() && m_playback_decoder_loads && !m_playback_overlay_checked && (address & 0xffff) == 0x2000)
			{
				verify_media_overlay(true);
				logerror("mu4_playback_audio_window: start=decoder_execute_entry t=%.9f sample_source=converted_sample_bench music_decode=0\n", machine().time().as_double());
			}
			if (address != 0x73f8 || !m_recorder_loader_calls) return;
			++m_recorder_startup_entries;
			logerror("mu4_native_recorder_startup: pc=%06x opcode=%04x pmst=%04x identity=execute_entry\n",
				unsigned(address), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
		});
		m_cpu->set_addrmap(AS_PROGRAM, &mu4_storage_test_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &mu4_storage_test_state::data_map);
		m_cpu->set_addrmap(AS_IO, &mu4_storage_test_state::io_map);
		m_cpu->bio_in_cb().set(FUNC(mu4_storage_test_state::bio_r));
		TMS320C54X_DMA(config, m_dma, 13'000'000);
		m_dma->set_cpu(m_cpu);
		// NPM-5 MU4 schematic sheet 4: Y101, DA150 X1/X2, 11.9952 MHz.
		m_dma->set_input_clock(11'995'200);
		m_cpu->pll_clock_stop_cb().set(m_dma, FUNC(tms320c54x_dma_device::pll_clock_stop_w));
		m_dma->set_per_channel_reload(true);
		m_dma->completion_cb().set(FUNC(mu4_storage_test_state::dma_complete));
		TMS320C54X_MCBSP(config, m_mcbsp, 13'000'000);
		m_mcbsp->tx_word_cb().set(FUNC(mu4_storage_test_state::serial_tx));
		m_mcbsp->tx_bit_cb().set(FUNC(mu4_storage_test_state::serial_tx_bit));
		m_mcbsp->tx_irq_cb().set(FUNC(mu4_storage_test_state::serial_tx_irq));
		m_mcbsp->tx_event_cb().set([this](int state) { m_dma->sync_w(6, state); });
		TMS320C54X_MCBSP(config, m_mcbsp0, 13'000'000);
		TMS320C54X_MCBSP(config, m_mcbsp2, 13'000'000);
		m_cpu->peripheral_clock_stop_cb().set([this](int state) {
			m_mcbsp->internal_clock_stop_w(state);
			m_mcbsp0->internal_clock_stop_w(state);
			m_mcbsp2->internal_clock_stop_w(state);
		});
		m_mcbsp2->tx_irq_cb().set([this](int state) {
			if (m_phase == 30 && system_bios() >= 9) { if (state) ++m_command_tx_irqs; m_cpu->set_input_line(7, state); }
		});
		m_mcbsp2->tx_bit_cb().set([this](int state) {
			m_command_wire_byte = (m_command_wire_byte << 1) | (state ? 1 : 0);
			if (++m_command_wire_bit_count == 8) { m_command_wire_decoded.push_back(m_command_wire_byte); m_command_wire_bit_count = m_command_wire_byte = 0; }
		});
		m_mcbsp2->tx_word_cb().set(FUNC(mu4_storage_test_state::command_output));
		m_mcbsp2->rx_event_cb().set([this](int state) { m_command_rx_ready = bool(state); });
		m_mcbsp2->rx_irq_cb().set([this](int state) {
			if (m_phase == 30 && system_bios() >= 11) { if (state) ++m_command_rx_irqs; m_cpu->set_input_line(6, state); }
		});
		m_mcbsp0->tx_word_cb().set(FUNC(mu4_storage_test_state::stream_tx));
		m_mcbsp0->tx_observer_cb().set(FUNC(mu4_storage_test_state::observe_playback_tx));
		m_mcbsp0->tx_bit_cb().set([this](int state) {
			if ((m_phase >= 51 && m_phase <= 53) || m_phase == 66 || m_phase == 67 || (m_phase >= 70 && m_phase <= 73) || m_phase == 77 || m_phase == 78 || (m_phase >= 99 && m_phase <= 118)) m_external_bits.push_back(state);
			subdevice<tlv320aic23_device>("codec")->din_w(state);
		});
		m_mcbsp0->tx_event_cb().set([this](int state) { m_dma->sync_w(2, state); });
		m_mcbsp0->tx_irq_cb().set([this](int state) { if (state) ++m_external_tx_irqs; m_cpu->set_input_line(5, state); });
		m_mcbsp0->rx_event_cb().set([this](int state) { m_dma->sync_w(1, state); });
		m_mcbsp0->rx_irq_cb().set([this](int state) { if (state) ++m_rx_irqs; m_cpu->set_input_line(4, state); });
		m_mcbsp0->rx_observer_cb().set(FUNC(mu4_storage_test_state::observe_recorder_rx));
		TLV320AIC23(config, "codec", 11'995'200);
		SPEAKER(config, "headphone_left").front_left();
		SPEAKER(config, "headphone_right").front_right();
		subdevice<tlv320aic23_device>("codec")->add_route(0, "headphone_left", 1.0);
		subdevice<tlv320aic23_device>("codec")->add_route(1, "headphone_right", 1.0);
		subdevice<tlv320aic23_device>("codec")->bclk_cb().set([this](int state) {
			if (playback_profile() && m_playback_overlay_checked && state) ++m_playback_audio_clocks;
			if (m_phase >= 54 && m_phase <= 57) { m_codec_edges.push_back(state); if (state) ++m_codec_clocks; }
			m_mcbsp0->tx_clock_w(state); m_mcbsp0->rx_clock_w(state);
		});
		subdevice<tlv320aic23_device>("codec")->frame_cb().set([this](int state) {
			if (playback_profile() && machine().time().as_double() >= 0.817 && machine().time().as_double() <= 0.824)
			{
				++m_playback_startup_frames;
				logerror("mu4_startup_frame: state=%u ns=%llu observer_writes=0\n",unsigned(bool(state)),static_cast<unsigned long long>(machine().time().as_ticks(1'000'000'000)));
			}
			if (playback_profile() && m_playback_overlay_checked && state) ++m_playback_audio_frames;
			if (m_phase >= 54 && m_phase <= 57) { m_codec_edges.push_back(2 + state); if (state) m_codec_frames.push_back(m_codec_clocks); }
			m_mcbsp0->tx_frame_w(state); m_mcbsp0->rx_frame_w(state);
		});
		// Explicit converted-sample bench input, not a modeled analog ADC.
		subdevice<tlv320aic23_device>("codec")->converted_adc_cb().set(FUNC(mu4_storage_test_state::converted_adc));
		subdevice<tlv320aic23_device>("codec")->dout_cb().set([this](int state) { m_mcbsp0->rx_data_w(state); });
		subdevice<tlv320aic23_device>("codec")->din_word_cb().set([this](offs_t channel, u16 value) {
			if (playback_profile() && m_playback_overlay_checked)
			{
				capture_playback_slot(channel);
				observe_playback_audio(value, true);
				double const time = machine().time().as_double();
				if (channel == 1 && m_playback_dac_capture.is_open() && ++m_playback_dac_capture_frames <= PLAYBACK_DAC_FRAME_CAP)
				{
					auto const &codec = *subdevice<tlv320aic23_device>("codec");
					u64 const ns = machine().time().as_ticks(1'000'000'000);
					char bytes[32]{};
					for (unsigned i=0;i<8;++i) bytes[i] = char(ns >> (8*i));
					for (unsigned side=0;side<2;++side)
					{
						u16 const code = u16(codec.dac_sample(side));
						bytes[8+2*side] = char(code); bytes[9+2*side] = char(code >> 8);
						bytes[12+side] = char(codec.headphone_volume(side));
						double const level = codec.headphone_dac_level(side);
						u64 bits;
						static_assert(sizeof(bits) == sizeof(level) && std::numeric_limits<double>::is_iec559);
						std::memcpy(&bits,&level,sizeof(bits));
						for (unsigned i=0;i<8;++i) bytes[16+8*side+i] = char(bits >> (8*i));
					}
					m_playback_dac_capture.write(bytes,sizeof(bytes));
					for (char byte : bytes) m_playback_dac_capture_hash = (m_playback_dac_capture_hash ^ u8(byte))*16777619U;
				}
				if (channel == 1 && time >= 1.06 && time <= 1.10)
				{
					++m_playback_dac_frames;
					if (m_playback_dac_frames <= 4096)
					{
						auto const &codec = *subdevice<tlv320aic23_device>("codec");
						logerror("mu4_dac_frame: ordinal=%u ns=%llu left_code=%04x right_code=%04x left_volume=%02x right_volume=%02x left_level=%.12f right_level=%.12f observer_writes=0 analog_filter=0 audio_acceptance=0\n",
							m_playback_dac_frames,static_cast<unsigned long long>(machine().time().as_ticks(1'000'000'000)),u16(codec.dac_sample(0)),u16(codec.dac_sample(1)),codec.headphone_volume(0),codec.headphone_volume(1),codec.headphone_dac_level(0),codec.headphone_dac_level(1));
					}
				}
			}
			if (m_phase == 30 && system_bios() == 20 && m_stream_words)
			{
				if (!m_continuous_tx_count) fatalerror("MU4 codec DIN word has no observed transmitter word");
				if (channel != (m_continuous_din_words & 1) || value != m_continuous_tx[m_continuous_tx_head])
					fatalerror("MU4 continuous codec DIN mismatch word=%u channel=%u actual=%04x expected=%04x",
						m_continuous_din_words, unsigned(channel), value, m_continuous_tx[m_continuous_tx_head]);
				m_continuous_tx_head = (m_continuous_tx_head + 1) % m_continuous_tx.size();
				--m_continuous_tx_count;
				++m_continuous_din_words;
				if (value) ++m_continuous_nonzero_words;
			}
			if (m_phase >= 54 && m_phase <= 57)
			{
				if (channel != (m_codec_din_words.size() & 1)) fatalerror("MU4 codec DIN channel ordering mismatch");
				m_codec_din_words.push_back(value);
			}
			if (m_phase == 30 && m_native_din_words < m_native_tx_words.size())
			{
				if (channel != (m_native_din_words & 1) || value != m_native_tx_words[m_native_din_words])
					fatalerror("MU4 native codec DIN does not match transmitted word %u", m_native_din_words);
				++m_native_din_words;
				finish_stream_if_ready();
			}
		});
		SAMSUNG_K9K1208U0A(config, m_nand);
		m_nand->rnb_wr_callback().set(FUNC(mu4_storage_test_state::ready_w));
	}
	DECLARE_INPUT_CHANGED_MEMBER(mcu_dsp_interrupt);
	DECLARE_INPUT_CHANGED_MEMBER(mcu_status_request);
private:
	std::array<u32, 4> m_ac_generated{}, m_ac_received{};
	u32 m_ac_other = 0, m_ac_other_owners = 0;
	std::array<u64,64> m_ac_read_owners{};
	std::map<u32,u64> m_ac_unowned_contexts;
	u64 m_ac_read_total = 0, m_ac_context_dropped = 0;
	std::array<u64,64> m_ac_rx_fresh{}, m_ac_rx_stale{};
	u64 m_ac_rx_publications = 0, m_ac_rx_discarded = 0, m_ac_rx_resets = 0;
	u64 m_ac_rx_bad = 0, m_ac_rx_unknown_stale = 0;
	bool m_ac_rx_pending = false, m_ac_rx_known = false;
	u16 m_ac_rx_word = 0;
	std::array<bool,6> m_ac_dma_pending{};
	std::array<u16,6> m_ac_dma_word{};
	std::array<u64,18> m_ac_dma_writes{};
	std::array<u32,18> m_ac_dma_first{}, m_ac_dma_last{};
	u64 m_ac_dma_bad = 0;
	std::array<u16,512> m_ac_buffer_word{};
	std::array<bool,512> m_ac_buffer_valid{}, m_ac_buffer_read{};
	std::array<u64,2> m_ac_buffer_writes{}, m_ac_buffer_consumed{}, m_ac_buffer_unread{}, m_ac_buffer_unknown{};
	std::array<u64,128> m_ac_buffer_readers{};
	std::map<u32,u64> m_ac_buffer_contexts;
	u64 m_ac_buffer_bad = 0, m_ac_buffer_dropped = 0;
	std::map<u64,u64> m_ac_buffer_instructions;
	u64 m_ac_buffer_identity_missing = 0, m_ac_buffer_identity_dropped = 0;
	struct recorder_copy_read { u32 instruction; u16 opcode, source, value; bool known; };
	std::vector<recorder_copy_read> m_ac_copy_pending;
	std::map<u64,std::array<u64,6>> m_ac_copy_destinations;
	std::array<u64,2> m_ac_copy_reads{};
	u64 m_ac_copy_writes = 0, m_ac_copy_bad = 0, m_ac_copy_dropped = 0;
	struct recorder_workspace_word { u16 value = 0; bool valid = false, read = false; };
	std::array<recorder_workspace_word,4096> m_ac_workspace{};
	std::array<u64,128> m_ac_workspace_readers{};
	std::map<u64,u64> m_ac_workspace_instructions;
	u64 m_ac_workspace_writes = 0, m_ac_workspace_alias_writes = 0, m_ac_workspace_origins = 0;
	u64 m_ac_workspace_epochs_read = 0, m_ac_workspace_retired = 0, m_ac_workspace_known = 0, m_ac_workspace_unknown = 0;
	u64 m_ac_workspace_bad = 0, m_ac_workspace_dropped = 0, m_ac_workspace_missing = 0;
	bool m_ac_daram_read_alias = false, m_ac_daram_write_alias = false;
	std::array<std::vector<std::pair<u16,u16>>,2> m_ac_ingress_reads;
	struct recorder_ingress_store { bool valid = false, known = false; unsigned lane = 0; u16 source = 0, destination = 0, value = 0, opcode = 0; u32 instruction = 0; };
	recorder_ingress_store m_ac_ingress_store;
	std::array<u64,2> m_ac_ingress_loads{}, m_ac_ingress_stores{}, m_ac_ingress_known{};
	std::array<u32,2> m_ac_ingress_frame_stores{}, m_ac_ingress_frame_known{};
	std::array<u16,2> m_ac_ingress_source_base{}, m_ac_ingress_destination_base{};
	u32 m_ac_ingress_frames = 0, m_ac_ingress_entries = 0;
	u64 m_ac_ingress_bad = 0, m_ac_ingress_dropped = 0;
	bool m_ac_ingress_call = false;
	struct recorder_consumer_counts
	{
		u64 origins = 0, known = 0, epochs = 0, retired = 0, unread = 0, unknown = 0, writes = 0, alias_writes = 0, missing = 0;
		void add(recorder_consumer_counts const &other)
		{
			origins += other.origins; known += other.known; epochs += other.epochs; retired += other.retired;
			unread += other.unread; unknown += other.unknown; writes += other.writes; alias_writes += other.alias_writes; missing += other.missing;
		}
	};
	std::array<recorder_workspace_word,2048> m_ac_consumer_words{};
	recorder_consumer_counts m_ac_consumer_current, m_ac_consumer_total;
	std::array<u64,128> m_ac_consumer_readers{};
	std::map<u64,u64> m_ac_consumer_instructions;
	u32 m_ac_consumer_entries = 0, m_ac_consumer_returns = 0;
	u64 m_ac_consumer_bad = 0, m_ac_consumer_dropped = 0;
	u16 m_ac_consumer_base = 0;
	bool m_ac_consumer_active = false;
	std::ofstream m_ac_pcm_capture;
	u64 m_ac_pcm_words = 0, m_ac_pcm_bad_order = 0, m_ac_pcm_dropped = 0;
	u32 m_ac_pcm_hash = 2166136261U;
	struct recording_transform_context
	{
		bool active = false, window = false;
		u64 ordinal = 0;
		u32 caller = 0, cursor = 0, position = 0;
		u16 sp = 0, source = 0;
		std::array<u16,2> pointers{}, expected{}, inputs{}, outputs{};
		std::array<u16,82> state{};
		std::array<u16,6> arguments{};
		std::array<u16,32> plain{};
		unsigned reads = 0, writes = 0, state_other_writes = 0, source_writes = 0;
		double start = 0;
	};
	recording_transform_context m_record_transform;
	struct recording_transform_store { bool valid = false; unsigned side = 0; u32 instruction = 0; u16 address = 0, value = 0; };
	recording_transform_store m_record_transform_store;
	std::map<u32,u64> m_record_transform_callers;
	u64 m_record_transform_entries = 0, m_record_transform_returns = 0, m_record_transform_unadmitted = 0;
	u64 m_record_transform_bad = 0, m_record_transform_dropped = 0;
	void observe_recording_transform(offs_t address,u16 opcode)
	{
		if (m_record_transform_store.valid) { ++m_record_transform_bad; m_record_transform_store.valid = false; }
		auto &c = m_record_transform;
		if (c.active && address == c.caller)
		{
			++m_record_transform_returns;
			if (c.reads != 3 || c.writes != 3) ++m_record_transform_bad;
			if (c.ordinal <= 8192)
			{
				auto words = [](auto const &values) {
					std::string text;
					for (u16 word : values) { if (!text.empty()) text += ','; text += util::string_format("%04x",word); }
					return text;
				};
				std::string plain = "none";
				if (c.window) { plain.clear(); for (u16 byte : c.plain) plain += util::string_format("%02x",byte); }
				logerror("mu4_record_full_block: ordinal=%llu caller=%06x sp=%04x pointers=%04x,%04x input=%04x,%04x output=%04x,%04x state=%s reads=%u writes=%u state_other_writes=%u source_writes=%u domain=%s source=%04x position=%08x cursor=%08x arguments=%s plain=%s start=%.9f end=%.9f identity=Execute_entry_and_bus observer_writes=0\n",
					static_cast<unsigned long long>(c.ordinal),c.caller,c.sp,c.pointers[0],c.pointers[1],c.inputs[0],c.inputs[1],c.outputs[0],c.outputs[1],words(c.state).c_str(),c.reads,c.writes,c.state_other_writes,c.source_writes,c.window ? "aligned32" : "other",c.source,c.position,c.cursor,words(c.arguments).c_str(),plain.c_str(),c.start,machine().time().as_double());
			}
			else ++m_record_transform_dropped;
			c.active = false;
		}
		if (address == 0x3f0fd && opcode == 0x4a11)
		{
			if (c.active) ++m_record_transform_bad;
			c = {}; c.ordinal = ++m_record_transform_entries;
			// Younger FRAME can have changed live SP; PSHM retains its own slot.
			u16 pushed = 0, saved = 0;
			if (!m_cpu->observe_execute_memory_store(address,opcode,pushed,saved)) { ++m_record_transform_unadmitted; return; }
			c.sp = u16(pushed+1);
			auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
			if (c.sp < 0x80 || c.sp > 0xfffc) { ++m_record_transform_unadmitted; return; }
			c.caller = (u32(data.read_word(c.sp))<<16) | data.read_word(c.sp+1);
			if (!m_record_transform_callers.count(c.caller) && m_record_transform_callers.size() >= 128) ++m_record_transform_dropped;
			else ++m_record_transform_callers[c.caller];
			c.pointers = {u16(m_cpu->state_int(tms320c54x_device::STATE_A)),data.read_word(c.sp+2)};
			if (c.caller > 0x7fffff || c.pointers[0] < 0x80 || c.pointers[1] < 0x80)
			{
				++m_record_transform_unadmitted;
				logerror("mu4_record_full_unadmitted: ordinal=%llu caller=%06x pointers=%04x,%04x observer_writes=0\n",static_cast<unsigned long long>(c.ordinal),c.caller,c.pointers[0],c.pointers[1]);
				return;
			}
			for (unsigned side=0;side<2;++side) c.expected[side] = data.read_word(c.pointers[side]);
			for (unsigned index=0;index<82;++index) c.state[index] = data.read_word(0xfc6c+index);
			if (c.caller == 0x3ee95)
			{
				u16 const outer = c.sp+2;
				for (unsigned index=0;index<6;++index) c.arguments[index] = data.read_word(u16(outer+0x16+index));
				c.source = data.read_word(u16(outer+2));
				c.position = (u32(data.read_word(u16(outer+4)))<<16) | data.read_word(u16(outer+5));
				u16 const cursor = c.arguments[2];
				c.window = c.pointers[0] == u16(outer+0x10) && c.pointers[1] == u16(outer+0x11) &&
					c.arguments[0] == 0 && c.arguments[1] == 32 && c.arguments[3] == 0 && c.arguments[4] == 28 && c.arguments[5] == 4 &&
					c.position == 0 && c.source >= 0x80 && c.source <= 0xffe0 && cursor >= 0x80 && cursor < 0xffff;
				if (c.window)
				{
					c.cursor = (u32(data.read_word(cursor))<<16) | data.read_word(cursor+1);
					for (unsigned index=0;index<32;++index) { c.plain[index] = data.read_word(c.source+index); if (c.plain[index] > 255) c.window = false; }
				}
			}
			c.active = true; c.start = machine().time().as_double();
		}
		else if (c.active && (address == 0x3f45e || address == 0x3f462))
		{
			unsigned const side = address == 0x3f45e;
			u16 destination = 0, value = 0;
			if (opcode != 0x8081 || !m_cpu->observe_execute_memory_store(address,opcode,destination,value) || destination != c.pointers[side]) ++m_record_transform_bad;
			m_record_transform_store = {true,side,u32(address),destination,value};
		}
	}
	void observe_transform_read(offs_t address,u16 value)
	{
		auto &c = m_record_transform;
		if (!c.active) return;
		u32 instruction = 0; u16 opcode = 0;
		if (!m_cpu->observe_staged_data_read(address,instruction,opcode) || (instruction != 0x3f102 && instruction != 0x3f106)) return;
		unsigned const side = instruction == 0x3f106;
		if (opcode != 0x1081 || address != c.pointers[side] || value != c.expected[side] || BIT(c.reads,side)) ++m_record_transform_bad;
		c.inputs[side] = value; c.reads |= 1U<<side;
	}
	void observe_transform_write(offs_t address,u16 value,u16 mask)
	{
		auto &c = m_record_transform;
		if (!c.active) return;
		bool const final = m_record_transform_store.valid;
		if (address >= 0xfc6c && address <= 0xfcbd && !final) ++c.state_other_writes;
		if (c.window && address >= c.source && address < c.source+32) ++c.source_writes;
		if (!final) return;
		auto const store = m_record_transform_store; m_record_transform_store.valid = false;
		u16 destination = 0, observed = 0;
		if (mask != 0xffff || address != store.address || value != store.value || BIT(c.writes,store.side) ||
			!m_cpu->observe_execute_memory_store(store.instruction,0x8081,destination,observed) || destination != address || observed != value) ++m_record_transform_bad;
		c.outputs[store.side] = value; c.writes |= 1U<<store.side;
	}
	void finish_recorder_consumer()
	{
		if (!m_ac_consumer_active) { ++m_ac_consumer_bad; return; }
		for (auto const &word : m_ac_consumer_words) if (word.valid && !word.read) ++m_ac_consumer_current.unread;
		auto const &c = m_ac_consumer_current;
		++m_ac_consumer_returns;
		if (m_ac_consumer_returns <= 512)
			logerror("mu4_recorder_consumer_frame: ordinal=%u base=%04x origins=%llu known_reads=%llu read_epochs=%llu retired_unread=%llu unread_return=%llu unknown_reads=%llu writes=%llu alias_writes=%llu missing_data_identity=%llu observer_writes=0\n",
				m_ac_consumer_returns,m_ac_consumer_base,static_cast<unsigned long long>(c.origins),static_cast<unsigned long long>(c.known),static_cast<unsigned long long>(c.epochs),static_cast<unsigned long long>(c.retired),static_cast<unsigned long long>(c.unread),static_cast<unsigned long long>(c.unknown),static_cast<unsigned long long>(c.writes),static_cast<unsigned long long>(c.alias_writes),static_cast<unsigned long long>(c.missing));
		else ++m_ac_consumer_dropped;
		m_ac_consumer_total.add(c); m_ac_consumer_active = false; m_ac_consumer_words.fill({});
	}
	void observe_recorder_ingress(offs_t address,u16 opcode)
	{
		if (m_ac_ingress_store.valid) { ++m_ac_ingress_bad; m_ac_ingress_store.valid = false; }
		if (m_ac_ingress_call)
		{
			if (address != 0x39852 || opcode != 0x4a11 || u16(m_cpu->state_int(tms320c54x_device::STATE_A)) != m_ac_ingress_destination_base[0]) ++m_ac_ingress_bad;
			else
			{
				++m_ac_ingress_entries;
				if (m_ac_consumer_active) ++m_ac_consumer_bad;
				m_ac_consumer_active = true; ++m_ac_consumer_entries;
				m_ac_consumer_base = m_ac_ingress_destination_base[0]; m_ac_consumer_current = {};
				for (auto const &word : m_ac_consumer_words) if (word.valid) ++m_ac_consumer_current.origins;
			}
			m_ac_ingress_call = false;
		}
		if (address == 0x39825 || address == 0x3982d)
		{
			unsigned const lane = address == 0x3982d;
			u16 destination = 0, value = 0;
			if (opcode != (lane ? 0x80e2 : 0x8082) || !m_cpu->observe_execute_memory_store(address,opcode,destination,value)) { ++m_ac_ingress_bad; return; }
			bool const known = !m_ac_ingress_reads[lane].empty();
			u16 source = 0;
			if (known)
			{
				source = m_ac_ingress_reads[lane].front().first;
				if (value != m_ac_ingress_reads[lane].front().second) ++m_ac_ingress_bad;
				m_ac_ingress_reads[lane].erase(m_ac_ingress_reads[lane].begin());
			}
			m_ac_ingress_store = {true,known,lane,source,destination,value,opcode,u32(address)};
		}
		else if (address == 0x3983b)
		{
			if (opcode != 0xf983) ++m_ac_ingress_bad;
			++m_ac_ingress_frames; m_ac_ingress_call = true;
			if (m_ac_ingress_frames <= 512)
				logerror("mu4_recorder_ingress_frame: ordinal=%u stores=%u,%u known=%u,%u source_base=%04x,%04x destination_base=%04x,%04x argument=%04x identity=Execute_entry observer_writes=0\n",
					m_ac_ingress_frames,m_ac_ingress_frame_stores[0],m_ac_ingress_frame_stores[1],m_ac_ingress_frame_known[0],m_ac_ingress_frame_known[1],m_ac_ingress_source_base[0],m_ac_ingress_source_base[1],m_ac_ingress_destination_base[0],m_ac_ingress_destination_base[1],u16(m_cpu->state_int(tms320c54x_device::STATE_A)));
			else ++m_ac_ingress_dropped;
			m_ac_ingress_frame_stores.fill(0); m_ac_ingress_frame_known.fill(0);
		}
		else if (address == 0x3983d)
		{
			if (opcode != 0xf7b8) ++m_ac_consumer_bad;
			finish_recorder_consumer();
		}
	}
	void observe_recorder_ingress_write(offs_t address,u16 value,u16 mask)
	{
		if (!m_ac_ingress_store.valid) return;
		auto const store = m_ac_ingress_store; m_ac_ingress_store.valid = false;
		u16 destination = 0, observed = 0;
		if (mask != 0xffff || address != store.destination || value != store.value ||
			!m_cpu->observe_execute_memory_store(store.instruction,store.opcode,destination,observed) || destination != address || observed != value) ++m_ac_ingress_bad;
		unsigned const lane = store.lane, index = m_ac_ingress_frame_stores[lane]++;
		++m_ac_ingress_stores[lane];
		if (store.known)
		{
			if (m_ac_consumer_active || index >= 1024) ++m_ac_consumer_bad;
			else m_ac_consumer_words[2*index+lane] = {value,true,false};
			if (!m_ac_ingress_frame_known[lane])
			{
				m_ac_ingress_source_base[lane] = 0x7000+((store.source-0x7000-index)&0xfff);
				m_ac_ingress_destination_base[lane] = u16(address-2*index);
			}
			if (store.source != 0x7000+((m_ac_ingress_source_base[lane]-0x7000+index)&0xfff) || address != u16(m_ac_ingress_destination_base[lane]+2*index)) ++m_ac_ingress_bad;
			++m_ac_ingress_known[lane]; ++m_ac_ingress_frame_known[lane];
		}
	}
	void install_recorder_buffer_observers()
	{
		m_cpu->space(AS_DATA).install_read_tap(0,0xffff,"mu4_recorder_consumer_read",
			[this](offs_t address,u16 &value,u16) {
				if (machine().side_effects_disabled()) return;
				observe_transform_read(address,value);
				if (!m_ac_consumer_active) return;
				u16 const offset = u16(address-m_ac_consumer_base);
				if (offset >= 2048) return;
				auto &word = m_ac_consumer_words[offset]; auto &c = m_ac_consumer_current;
				if (!word.valid) { ++c.unknown; return; }
				++c.known;
				if (word.value != value) ++m_ac_consumer_bad;
				if (!word.read) { word.read = true; ++c.epochs; }
				u8 owners = 0;
				for (unsigned channel=0;channel<6;++channel)
					if (m_dma->source_read_matches(channel,m_ac_daram_read_alias ? AS_PROGRAM : AS_DATA,address)) owners |= 1U << channel;
				++m_ac_consumer_readers[unsigned(m_ac_daram_read_alias)*64+owners];
				if (!owners && !m_ac_daram_read_alias)
				{
					u32 instruction = 0; u16 opcode = 0;
					if (!m_cpu->observe_staged_data_read(address,instruction,opcode)) { ++c.missing; return; }
					u64 const key = (u64(instruction)<<16) | opcode;
					auto found = m_ac_consumer_instructions.find(key);
					if (found != m_ac_consumer_instructions.end()) ++found->second;
					else if (m_ac_consumer_instructions.size() < 256) m_ac_consumer_instructions.emplace(key,1);
					else ++m_ac_consumer_dropped;
					if (offset != c.known-1 || instruction != 0x3d9c8+(offset&1) || opcode != 0xe5b8+(offset&1)) ++m_ac_pcm_bad_order;
					if (m_ac_pcm_words >= 512U*2048) ++m_ac_pcm_dropped;
					else
					{
						char const bytes[]{char(value),char(value>>8)};
						m_ac_pcm_capture.write(bytes,sizeof(bytes));
						for (char byte : bytes) m_ac_pcm_hash = (m_ac_pcm_hash^u8(byte))*16777619U;
						++m_ac_pcm_words;
					}
				}
			});
		m_cpu->space(AS_DATA).install_read_tap(0x7000,0x7fff,"mu4_recorder_workspace_read",
			[this](offs_t address,u16 &value,u16) {
				if (machine().side_effects_disabled()) return;
				auto &word = m_ac_workspace[address-0x7000];
				if (!word.valid) { ++m_ac_workspace_unknown; return; }
				++m_ac_workspace_known;
				if (word.value != value) ++m_ac_workspace_bad;
				if (!word.read) { word.read = true; ++m_ac_workspace_epochs_read; }
				u8 owners = 0;
				for (unsigned channel=0;channel<6;++channel)
					if (m_dma->source_read_matches(channel,m_ac_daram_read_alias ? AS_PROGRAM : AS_DATA,address)) owners |= 1U << channel;
				++m_ac_workspace_readers[unsigned(m_ac_daram_read_alias)*64+owners];
				if (!owners && !m_ac_daram_read_alias)
				{
					u32 instruction = 0; u16 opcode = 0;
					if (!m_cpu->observe_staged_data_read(address,instruction,opcode)) { ++m_ac_workspace_missing; return; }
					if (instruction == 0x39823 || instruction == 0x3982b)
					{
						unsigned const lane = instruction == 0x3982b;
						if (opcode != (lane ? 0x10e3 : 0x10eb)) ++m_ac_ingress_bad;
						++m_ac_ingress_loads[lane];
						if (m_ac_ingress_reads[lane].size() >= 4) ++m_ac_ingress_dropped;
						else m_ac_ingress_reads[lane].emplace_back(address,value);
					}
					u64 const key = (u64(instruction)<<16) | opcode;
					auto found = m_ac_workspace_instructions.find(key);
					if (found != m_ac_workspace_instructions.end()) ++found->second;
					else if (m_ac_workspace_instructions.size() < 256) m_ac_workspace_instructions.emplace(key,1);
					else ++m_ac_workspace_dropped;
				}
			});
		m_cpu->space(AS_DATA).install_write_tap(0,0xffff,"mu4_recorder_copy_destination",
			[this](offs_t address,u16 &value,u16 mask) {
				if (machine().side_effects_disabled()) return;
				observe_transform_write(address,value,mask);
				if (m_ac_consumer_active)
				{
					u16 const offset = u16(address-m_ac_consumer_base);
					if (offset < 2048)
					{
						auto &word = m_ac_consumer_words[offset];
						if (word.valid && !word.read) ++m_ac_consumer_current.retired;
						word = {}; ++m_ac_consumer_current.writes;
						m_ac_consumer_current.alias_writes += unsigned(m_ac_daram_write_alias);
					}
				}
				observe_recorder_ingress_write(address,value,mask);
				if (address >= 0x7000 && address <= 0x7fff)
				{
					auto &word = m_ac_workspace[address-0x7000];
					if (word.valid && !word.read) ++m_ac_workspace_retired;
					word = {};
					++m_ac_workspace_writes; m_ac_workspace_alias_writes += unsigned(m_ac_daram_write_alias);
				}
				u32 instruction = 0; u16 opcode = 0, source = 0;
				if (!m_cpu->observe_staged_mvdd_write(address,instruction,opcode,source) || source < 0x1800 || source > 0x19ff) return;
				auto found = std::find_if(m_ac_copy_pending.begin(),m_ac_copy_pending.end(),
					[instruction,opcode,source](auto const &read) { return read.instruction == instruction && read.opcode == opcode && read.source == source; });
				if (found == m_ac_copy_pending.end()) { ++m_ac_copy_bad; return; }
				if (mask != 0xffff || value != found->value) ++m_ac_copy_bad;
				if (address >= 0x7000 && address <= 0x7fff && found->known && mask == 0xffff && value == found->value)
				{
					m_ac_workspace[address-0x7000] = {value,true,false};
					++m_ac_workspace_origins;
				}
				u64 const key = (u64((source-0x1800)/256)<<48) | (u64(instruction)<<16) | opcode;
				if (!m_ac_copy_destinations.count(key) && m_ac_copy_destinations.size() >= 256) ++m_ac_copy_dropped;
				else
				{
					auto &row = m_ac_copy_destinations[key];
					if (!row[0] && !row[1]) { row[2] = row[3] = source; row[4] = row[5] = address; }
					else { row[2] = std::min(row[2],u64(source)); row[3] = std::max(row[3],u64(source)); row[4] = std::min(row[4],u64(address)); row[5] = std::max(row[5],u64(address)); }
					++row[unsigned(found->known)];
				}
				++m_ac_copy_writes; m_ac_copy_pending.erase(found);
			});
		m_cpu->space(AS_DATA).install_write_tap(0x1800,0x19ff,"mu4_recorder_buffer_write",
			[this](offs_t address,u16 &value,u16 mask) {
				if (machine().side_effects_disabled()) return;
				unsigned const slot = address-0x1800, bank = slot/256;
				if (m_ac_buffer_valid[slot] && !m_ac_buffer_read[slot]) ++m_ac_buffer_unread[bank];
				unsigned owner = 0;
				for (unsigned channel=0;channel<6;++channel)
					if (m_dma->destination_write_matches(channel,AS_DATA,address) && m_dma->source_matches(channel,AS_DATA,0x21)) ++owner;
				if (owner > 1 || (owner && mask != 0xffff)) ++m_ac_buffer_bad;
				m_ac_buffer_valid[slot] = owner == 1 && mask == 0xffff;
				m_ac_buffer_read[slot] = false; m_ac_buffer_word[slot] = value;
				if (m_ac_buffer_valid[slot]) ++m_ac_buffer_writes[bank];
			});
		m_cpu->space(AS_DATA).install_read_tap(0x1800,0x19ff,"mu4_recorder_buffer_read",
			[this](offs_t address,u16 &value,u16) {
				if (machine().side_effects_disabled()) return;
				unsigned const slot = address-0x1800, bank = slot/256;
				u32 copy_instruction = 0; u16 copy_opcode = 0;
				if (m_cpu->observe_staged_data_read(address,copy_instruction,copy_opcode) && (copy_opcode&0xff00) == 0xe500)
				{
					++m_ac_copy_reads[unsigned(m_ac_buffer_valid[slot])];
					if (m_ac_copy_pending.size() >= 64) ++m_ac_copy_dropped;
					else m_ac_copy_pending.push_back({copy_instruction,copy_opcode,u16(address),value,m_ac_buffer_valid[slot]});
				}
				if (!m_ac_buffer_valid[slot]) { ++m_ac_buffer_unknown[bank]; return; }
				if (value != m_ac_buffer_word[slot]) ++m_ac_buffer_bad;
				if (!m_ac_buffer_read[slot]) { ++m_ac_buffer_consumed[bank]; m_ac_buffer_read[slot] = true; }
				u8 owners = 0;
				for (unsigned channel=0;channel<6;++channel)
					if (m_dma->source_read_matches(channel,AS_DATA,address)) owners |= 1U << channel;
				++m_ac_buffer_readers[bank*64+owners];
				if (!owners)
				{
					u32 instruction = 0; u16 opcode = 0;
					if (m_cpu->observe_staged_data_read(address,instruction,opcode))
					{
						u64 const identity = (u64(bank)<<48) | (u64(instruction)<<16) | opcode;
						auto found = m_ac_buffer_instructions.find(identity);
						if (found != m_ac_buffer_instructions.end()) ++found->second;
						else if (m_ac_buffer_instructions.size() < 256) m_ac_buffer_instructions.emplace(identity,1);
						else ++m_ac_buffer_identity_dropped;
					}
					else ++m_ac_buffer_identity_missing;
					u32 const key = (bank<<24) | m_cpu->state_int(STATE_GENPC);
					auto found = m_ac_buffer_contexts.find(key);
					if (found != m_ac_buffer_contexts.end()) ++found->second;
					else if (m_ac_buffer_contexts.size() < 256) m_ac_buffer_contexts.emplace(key,1);
					else ++m_ac_buffer_dropped;
				}
			});
	}
	void observe_recorder_rx(offs_t event,u16 word)
	{
		if (!recorder_ac_profile()) return;
		if (event == tms320c54x_mcbsp_device::RX_PUBLISH)
		{
			if (m_ac_rx_pending) ++m_ac_rx_bad;
			++m_ac_rx_publications;
			m_ac_rx_pending = m_ac_rx_known = true; m_ac_rx_word = word;
		}
		else if (event == tms320c54x_mcbsp_device::RX_RESET)
		{
			++m_ac_rx_resets;
			if (m_ac_rx_pending) ++m_ac_rx_discarded;
			m_ac_rx_pending = false; m_ac_rx_known = true; m_ac_rx_word = word;
		}
		else if (event == tms320c54x_mcbsp_device::RX_READ_FRESH || event == tms320c54x_mcbsp_device::RX_READ_STALE)
		{
			u8 owners = 0;
			for (unsigned channel=0;channel<6;++channel)
				if (m_dma->source_read_matches(channel,AS_DATA,0x21)) owners |= 1U << channel;
			bool const fresh = event == tms320c54x_mcbsp_device::RX_READ_FRESH;
			if (m_ac_rx_pending != fresh || (m_ac_rx_known && m_ac_rx_word != word)) ++m_ac_rx_bad;
			if (!fresh && !m_ac_rx_known) ++m_ac_rx_unknown_stale;
			++(fresh ? m_ac_rx_fresh : m_ac_rx_stale)[owners];
			if (fresh) m_ac_rx_pending = false;
		}
	}
	u16 converted_adc(offs_t channel)
	{
		if (!recorder_ac_profile()) return channel ? 0x5678 : 0x1234;
		unsigned const slot = unsigned(channel) * 2, period = channel ? 32 : 64;
		unsigned const phase = (m_ac_generated[slot] + m_ac_generated[slot + 1]) % period;
		bool const negative = phase >= period / 2;
		++m_ac_generated[slot + unsigned(negative)];
		int const amplitude = channel ? 4096 : 8192;
		return u16(negative ? -amplitude : amplitude);
	}
	required_device<tms320c54x_device> m_cpu;
	required_device<tms320c54x_dma_device> m_dma;
	required_device<tms320c54x_mcbsp_device> m_mcbsp;
	required_device<tms320c54x_mcbsp_device> m_mcbsp0;
	required_device<tms320c54x_mcbsp_device> m_mcbsp2;
	std::vector<u16> m_serial_tx_words;
	std::vector<int> m_serial_tx_bits;
	unsigned m_serial_saved_bits = 0;
	unsigned m_serial_tx_irqs = 0;
	std::vector<u16> m_dma_output;
	unsigned m_dma_completions = 0;
	unsigned m_rx_dma_completions = 0, m_rx_irqs = 0;
	void dma_complete(u8 channel)
	{
		if (channel != 2 && channel != 3) fatalerror("MU4 unexpected DMA completion channel");
		if (channel == 3) ++m_dma_completions;
		else ++m_rx_dma_completions;
		unsigned const selection = (m_dma->read(0) >> 6) & 3;
		if (selection == 1 || selection == 2)
		{
			unsigned const line = channel == 2 ? 10 : 11;
			m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		finish_stream_if_ready();
	}
	unsigned m_stream_words = 0, m_stream_config_writes = 0;
	unsigned m_native_din_words = 0;
	std::array<u16, 4> m_continuous_tx{};
	unsigned m_continuous_tx_head = 0, m_continuous_tx_count = 0;
	unsigned m_continuous_din_words = 0, m_continuous_nonzero_words = 0;
	std::array<u16, 4> m_source_words{};
	unsigned m_source_head = 0, m_source_count = 0, m_source_reads = 0;
	std::array<u16, 256> m_verified_tone{};
	std::array<bool, 2> m_verified_tone_valid{};
	std::array<unsigned, 2> m_verified_tone_reads{};
	unsigned m_tone_dma_words = 0;
	std::array<bool, 256> m_tone_cleared{};
	unsigned m_tone_discarded_words = 0;
	unsigned m_native_dma_rx_vectors = 0, m_native_dma_tx_vectors = 0;
	unsigned m_native_dma_tx_handler = 0;
	unsigned m_native_stream_pending_sets = 0, m_native_stream_pending_clears = 0, m_native_stream_pending_reads = 0;
	unsigned m_native_dispatch_traces = 0, m_native_descriptor_traces = 0;
	unsigned m_native_stream_consumer_entries = 0;
	unsigned m_native_selection_traces[17] = {};
	unsigned m_native_selection_writes = 0;
	unsigned m_native_startup_fetches[4] = {};
	unsigned m_native_main_call_traces = 0;
	unsigned m_native_config_call_traces = 0;
	unsigned m_native_tail_data_reads = 0;
	unsigned m_native_lookup_operand_traces = 0;
	unsigned m_native_cache_traces[6] = {};
	unsigned m_native_startup_subcall_traces = 0;
	unsigned m_native_startup_return_traces = 0;
	unsigned m_native_consumer_state_traces = 0;
	unsigned m_native_consumer_paths[6] = {};
	unsigned m_native_consumer_mode_writes = 0;
	unsigned m_native_command_paths[6] = {};
	unsigned m_measurement_blocks = 0;
	u8 m_measurement_branch_mask = 0;
	std::array<u32, 2> m_measurement_expected{};
	std::array<unsigned, 2> m_measurement_samples{};
	unsigned m_measurement_factor = 1;
	std::array<u16, 128> m_tone_expected{};
	std::array<u16, 2> m_tone_phase{};
	u16 m_tone_buffer = 0;
	unsigned m_tone_blocks = 0, m_tone_phase_wraps = 0;
	unsigned m_native_command_cursor = 0, m_native_command_waits = 0;
	unsigned m_command_tx_irqs = 0, m_command_wire_bits = 0;
	unsigned m_recorder_response_begin = 3, m_recorder_responses = 0;
	unsigned m_recorder_dispatches = 0, m_recorder_loader_calls = 0;
	bool m_recorder_overlay_checked = false;
	unsigned m_recorder_startup_entries = 0;
	unsigned m_recorder_control_entries = 0, m_recorder_control_acks = 0;
	unsigned m_recorder_metadata_result = 0;
	unsigned m_cold_catalog_scans = 0, m_cold_metadata_reads = 0;
	unsigned m_cold_notifications = 0, m_cold_response_begin = 14;
	unsigned m_cold_title_publications = 0;
	bool m_cold_title_verified = false;
	unsigned m_playback_requests = 0, m_playback_inspections = 0, m_playback_decoder_loads = 0;
	bool m_playback_status_replied = false;
	bool m_manual_status_requested = false;
	bool m_playback_overlay_checked = false;
	std::array<u32, 3> m_playback_execute_entries{};
	std::array<u32, 2> m_playback_audio_words{}, m_playback_audio_nonzero{};
	std::array<u32, 2> m_playback_audio_hash{2166136261U, 2166136261U};
	// Diagnostic files are external: same-process loads preserve the reference
	// sidecars and rewind to saved positions; cross-process capture is unsupported.
	std::array<std::ofstream, 3> m_playback_word_capture;
	std::array<u64, 9> m_playback_capture_positions{};
	unsigned m_playback_capture_loads = 0; // Artifact names, not emulated state.
	std::ofstream m_playback_slot_capture;
	std::ofstream m_playback_tx_time_capture;
	u32 m_playback_tx_time_hash = 2166136261U;
	std::ofstream m_playback_dxr_capture;
	u32 m_playback_dxr_hash = 2166136261U, m_playback_dxr_writes = 0;
	std::ofstream m_playback_tx_event_capture;
	u32 m_playback_tx_event_hash = 2166136261U, m_playback_tx_events = 0;
	static constexpr u32 PLAYBACK_TX_EVENT_CAP = 8'000'000;
	std::ofstream m_playback_decoded_capture;
	bool m_playback_capture_finished = false;
	u32 m_playback_tx_lifecycle_events = 0;
	u32 m_playback_tx_startup_events = 0, m_playback_startup_frames = 0;
	u32 m_playback_dac_frames = 0;
	std::ofstream m_playback_dac_capture;
	u32 m_playback_dac_capture_frames = 0, m_playback_dac_capture_hash = 2166136261U;
	static constexpr u32 PLAYBACK_DAC_FRAME_CAP = 2'000'000;
	u32 m_playback_decoded_hash = 2166136261U, m_playback_decoded_words = 0;
	u32 m_decoder_gate_events = 0, m_decoder_gate_bad = 0;
	u32 m_context_count_reads = 0;
	u32 m_context_pointer_reads = 0;
	u32 m_context_call_events = 0, m_context_call_bad = 0;
	u32 m_exchange_status_reads = 0, m_exchange_branch_events = 0, m_exchange_branch_bad = 0;
	u32 m_playback_slot_hash = 2166136261U;
	u32 m_playback_dma_hash = 2166136261U;
	std::array<double, 2> m_playback_audio_last_nonzero{};
	std::array<u32, 3> m_playback_filter{};
	u64 m_playback_execute_steps = 0;
	u32 m_playback_audio_clocks = 0, m_playback_audio_frames = 0;
	u32 m_playback_dma_reads = 0, m_playback_dma_nonzero = 0;
	// Bounded diagnostic provenance, not emulated hardware or save-state data.
	struct playback_buffer_write { double time = -1; u32 pc = 0; u16 word = 0, mask = 0; };
	std::array<playback_buffer_write, 0x380> m_playback_buffer_writes{};
	std::array<u32, 10> m_playback_parent{};
	u32 m_playback_parent_mismatch = 0;
	u32 m_playback_switch_samples = 0;
	u32 m_playback_enqueue_samples = 0;
	u32 m_playback_callback_samples = 0;
	u32 m_playback_notify_samples = 0;
	std::array<u32, 128> m_playback_path_pc{};
	std::array<u16, 128> m_playback_path_word{};
	u8 m_playback_path_next = 0;
	u16 m_playback_path_count = 0;
	std::array<u32, 6> m_playback_method_entries{};
	u32 m_playback_method_mismatch = 0;
	std::array<u32,4> m_playback_processing{}; // calls, returns, successes, observation mismatches
	u16 m_playback_fft_base = 0, m_playback_fft_parameter = 0, m_playback_post_base = 0;
	u16 m_playback_pre_base = 0, m_playback_pre_parameter = 0;
	u16 m_playback_window_base = 0, m_playback_window_parameter = 0;
	u16 m_playback_scale_base = 0, m_playback_scale_parameter = 0;
	u16 m_playback_callback_base = 0, m_playback_callback_history = 0, m_playback_callback_mode = 0;
	u32 m_playback_callback_target = 0;
	u8 m_playback_callback_site = 0;
	u16 m_playback_tail_base = 0, m_playback_tail_history = 0, m_playback_tail_saved = 0;
	u32 m_playback_tail_ordinal = 0;
	u32 m_playback_fft_invocation = 0;
	u16 m_playback_processing_sp = 0, m_playback_processing_destination = 0;
	std::array<u16,4> m_playback_conversion{}; // sample, channel, argument, caller SP
	u64 m_playback_conversion_raw = 0, m_playback_conversion_shifted = 0;
	bool m_playback_processing_pending = false, m_playback_conversion_active = false;
	bool m_playback_conversion_pending = false, m_playback_conversion_shift_seen = false;
	u32 m_playback_restore_samples = 0;
	std::array<u32, 2> m_playback_bit_samples{};
	std::array<u16, 4> m_playback_bit_source_samples{};
	u16 m_playback_bit_unusual_samples = 0;
	std::array<u16, 3> m_playback_bit_multiply_samples{};
	bool m_playback_optional_started = false, m_playback_optional_active = false;
	u8 m_playback_bit_group = 0;
	bool m_playback_bit_selected = false;
	u32 m_playback_bit_caller = 0;
	std::array<u32, 4> m_playback_activation_pc{};
	std::array<u16, 4> m_playback_activation_word{};
	double m_playback_activation_time = 0;
	static constexpr u32 PLAYBACK_RANGE_FIRST = 0x200a0;
	static constexpr unsigned PLAYBACK_RANGE_WORDS = 0x0cd2 - 0x00a0;
	std::array<u32, PLAYBACK_RANGE_WORDS> m_playback_range_counts{};
	std::array<u16, PLAYBACK_RANGE_WORDS> m_playback_range_opcodes{};
	u32 m_playback_range_changes = 0;
	u16 m_playback_dma_min = 0xffff, m_playback_dma_max = 0;
	double m_playback_dma_last_nonzero = 0;
	std::array<u32, 4> m_playback_history_pc{};
	std::array<u16, 4> m_playback_history_word{};
	u8 m_playback_history_next = 0;
	unsigned m_recorder_application_calls = 0, m_recorder_application_messages = 0;
	unsigned m_recorder_processing_waits = 0, m_recorder_processing_returns = 0;
	unsigned m_recorder_nand_controls = 0, m_recorder_nand_data = 0;
	u8 m_recorder_last_nand_command = 0;
	unsigned m_command_ack_cursor = 0, m_command_wire_bit_count = 0;
	bool m_command_wire_clock = false;
	bool m_command_ack_pending = false;
	u8 m_command_ack_token = 0, m_command_wire_byte = 0;
	u16 m_command_tx_register = 0;
	std::vector<u16> m_command_tx_words, m_command_wire_decoded;
	// Bounded diagnostic history, not a hardware FIFO. Late resident checkpoints
	// include multiple complete replies and can exceed a single 64-word frame.
	std::array<u16, 4096> m_saved_command_tx{}, m_saved_command_decoded{};
	unsigned m_saved_command_tx_count = 0, m_saved_command_decoded_count = 0;
	bool m_command_rx_ready = false, m_command_rx_busy = false, m_command_rx_clock = true;
	u8 m_command_rx_byte = 0;
	unsigned m_command_rx_phase = 0, m_command_rx_irqs = 0;
	// The replay controller/reference deliberately survive restoration of the DUT.
	unsigned m_native_replay_leg = 0;
	// Test supervisor, not DUT state: survives the requested machine reset.
	unsigned m_native_reset_leg = 0;
	unsigned m_native_reset_settings_mask = 0;
	unsigned m_native_bootstrap_entries = 0, m_native_bootstrap_resident_entries = 0;
	unsigned m_native_bootstrap_paths[10] = {};
	unsigned m_native_descriptor_reads = 0;
	unsigned m_native_descriptor_writes = 0;
	bool m_native_bootstrap_memory_checked = false;
	bool m_native_boundary_window = false;
	unsigned m_native_boundary_events = 0;
	unsigned m_native_boundary_dma_events = 0;
	bool m_native_runtime_observers_installed = false;
	unsigned m_native_runtime_reads[9] = {}, m_native_runtime_writes[9] = {};
	std::stringstream m_native_checkpoint, m_native_reference;
	unsigned m_native_settings_call_traces = 0;
	unsigned m_native_metadata_call_traces = 0;
	unsigned m_native_allocation_call_traces = 0;
	unsigned m_native_cache_update_traces = 0;
	unsigned m_native_cache_mode_traces[6] = {};
	bool m_native_settings_buffer_active = false;
	unsigned m_native_buffer_calls[20] = {}, m_native_buffer_returns[20] = {};
	static constexpr offs_t buffer_calls[] = {
		0x3b940, 0x3b95b, 0x3b973, 0x3b99b, 0x3b9c4, 0x3b9d3,
		0x3b9f2, 0x3b9fb, 0x3ba17, 0x3ba6a, 0x3ba78, 0x3ba7a,
		0x3ba84, 0x3ba88, 0x3ba8a, 0x3ba95, 0x3baa7, 0x3bab5,
		0x3babb, 0x3bacb
	};
	bool m_native_worker_window_started = false;
	attotime m_native_worker_window_start;
	std::vector<u16> m_native_tx_words;
	std::vector<u16> m_codec_din_words, m_codec_saved_din;
	unsigned m_codec_rx_snapshot_count = 0;
	unsigned native_stream_target() const { return system_bios() >= 3 ? 1024 : 128; }
	unsigned m_staged_copy_reads = 0, m_staged_copy_writes = 0;
	u16 m_staged_copy_word = 0;
	void staged_unmodeled_port_w(u16 word)
	{
		// Diagnostic sink only: no peripheral state or response is invented.
		logerror("mu4_staged_unmapped_port_write: port=0080 value=%04x write_context_pc=%06x device=unmodeled\n", word, unsigned(m_cpu->pc()));
	}
	unsigned native_worker_tail_ms() const { return system_bios() >= 7 ? 20000 : system_bios() == 6 ? 5000 : system_bios() == 5 ? 1000 : 100; }
	void trace_native_iterator(char const *point)
	{
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		logerror("mu4_native_iterator: point=%s source142c=%04x,%04x position145c=%04x,%04x advance145e=%04x reads=%u strobes=%u\n",
			point, data.read_word(0x142c), data.read_word(0x142d), data.read_word(0x145c),
			data.read_word(0x145d), data.read_word(0x145e), m_data_reads, unsigned(m_writes.size()));
		if (m_writes.size() >= 8)
			logerror("mu4_native_iterator_bus: point=%s suffix=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x\n", point,
				m_writes[m_writes.size()-8], m_writes[m_writes.size()-7], m_writes[m_writes.size()-6], m_writes[m_writes.size()-5],
				m_writes[m_writes.size()-4], m_writes[m_writes.size()-3], m_writes[m_writes.size()-2], m_writes[m_writes.size()-1]);
	}
	void finish_stream_if_ready()
	{
		if (original_bootstrap_profile()) return; // Fixed-window bootstrap observation, not a routine gate.
		if (m_phase == 30 && system_bios() >= 2 && m_stream_words >= native_stream_target() &&
			m_native_din_words >= native_stream_target() && m_dma_completions >= native_stream_target() / 128 &&
			m_rx_dma_completions >= native_stream_target() / 128)
		{
			if (system_bios() >= 4)
			{
				if (!m_native_worker_window_started)
				{
					m_native_worker_window_started = true;
					m_native_worker_window_start = machine().time();
					m_native_tail_data_reads = m_data_reads;
					trace_native_iterator("start");
					m_check->adjust(attotime::from_msec(native_worker_tail_ms()));
				}
			}
			else m_check->adjust(attotime::zero);
		}
	}
	unsigned m_codec_clocks = 0;
	std::vector<unsigned> m_codec_frames;
	std::vector<int> m_codec_edges, m_codec_saved_edges;
	attotime m_codec_snapshot_time;
	std::vector<u16> m_external_words;
	std::vector<int> m_external_bits;
	unsigned m_external_tx_irqs = 0;
	bool m_external_checkpoint_pending = false;
	void external_reg_w(u16 index, u16 value) { m_mcbsp0->control_w(0, index); m_mcbsp0->control_w(1, value); }
	u16 receive_status() { m_mcbsp0->control_w(0, 0); return m_mcbsp0->control_r(1); }
	void receive_setup(u16 rcr1, u16 rcr2, u16 pcr = 0, u16 spcr1 = 1)
	{
		external_reg_w(0, 0); external_reg_w(2, rcr1); external_reg_w(3, rcr2); external_reg_w(14, pcr);
		m_mcbsp0->rx_clock_w(!BIT(pcr, 0)); m_mcbsp0->rx_frame_w(BIT(pcr, 2));
		external_reg_w(0, spcr1);
	}
	void receive_bit(int bit, bool rising = false)
	{
		m_mcbsp0->rx_data_w(bit); m_mcbsp0->rx_clock_w(rising); m_mcbsp0->rx_clock_w(!rising);
	}
	void receive_word(u16 value, unsigned bits, unsigned delay = 0, bool inverted = false)
	{
		m_mcbsp0->rx_frame_w(!inverted);
		for (unsigned i = 0; i < delay; ++i) receive_bit(0, inverted);
		if (delay) m_mcbsp0->rx_frame_w(inverted);
		for (unsigned i = bits; i-- > 0; ) { receive_bit(BIT(value, i), inverted); m_mcbsp0->rx_frame_w(inverted); }
		receive_bit(0, inverted); // RBR -> DRR at the next sampling edge.
	}
	void external_clocks(unsigned count, bool inverted = true)
	{
		while (count--) { m_mcbsp0->tx_clock_w(!inverted); m_mcbsp0->tx_clock_w(inverted); }
	}
	void check_receive_packing(unsigned index)
	{
		unsigned const i = index / 6;
		unsigned const delay = (index / 2) % 3;
		unsigned const ignore = index % 2;
		static constexpr unsigned widths[] = {8, 12, 16};
		unsigned const width = widths[i];
		u16 const mask = (1U << width) - 1;
		static constexpr u16 words[] = {0xa55a, 0x5aa5, 0x1357};
			{
				for (unsigned gap = 0; gap < 2; ++gap)
				{
					receive_setup(i << 5, delay | (ignore ? 4 : 0));
					unsigned const irqs = m_rx_irqs;
					unsigned const period = width + gap;
					std::vector<u16> received;
					for (unsigned cycle = 0; cycle <= delay + 3 * period; ++cycle)
					{
						m_mcbsp0->rx_frame_w(cycle < 3 * period && cycle % period == 0);
						unsigned const bit = cycle >= delay ? cycle - delay : 3 * period;
						receive_bit(bit < 3 * period && bit % period < width ? BIT(words[bit / period], width - 1 - bit % period) : 0);
						if (receive_status() & 2) received.push_back(m_mcbsp0->data_r(1));
						if (receive_status() & 0xc) fatalerror("McBSP packed receive created a false sync error or overrun");
					}
					if (received != std::vector<u16>({u16(words[0] & mask), u16(words[1] & mask), u16(words[2] & mask)}) ||
						m_rx_irqs != irqs + 3)
						fatalerror("McBSP packed receive must preserve each complete word and ready edge");
				}
				// One clock earlier than the minimum legal frame period.
				receive_setup(i << 5, delay | (ignore ? 4 : 0), 0, 0x31);
				unsigned const error_irqs = m_rx_irqs;
				m_mcbsp0->rx_frame_w(1);
				for (unsigned cycle = 0; cycle < width - 1; ++cycle)
				{
					receive_bit(cycle >= delay ? BIT(words[0], width - 1 - (cycle - delay)) : 0);
					m_mcbsp0->rx_frame_w(0);
				}
				m_mcbsp0->rx_frame_w(1);
				if (ignore)
					for (unsigned bit = delay + 1; bit-- > 0; )
					{
						receive_bit(BIT(words[0], bit)); m_mcbsp0->rx_frame_w(0);
					}
				else
				{
					for (unsigned cycle = 0; cycle < delay; ++cycle) { receive_bit(0); m_mcbsp0->rx_frame_w(0); }
					for (unsigned bit = width; bit-- > 0; ) { receive_bit(BIT(words[1], bit)); m_mcbsp0->rx_frame_w(0); }
				}
				receive_bit(0);
				if ((receive_status() & 0xf) != (ignore ? 3 : 11) ||
					m_mcbsp0->data_r(1) != ((ignore ? words[0] : words[1]) & mask) || m_rx_irqs != error_irqs + !ignore)
					fatalerror("McBSP near-boundary premature frame must recover or ignore, not pack normally");
			}
		if (index == 17) logerror("mu4_mcbsp_receive_packing: PASS normal_cases=36 premature_cases=18 widths=8,12,16 delays=0,1,2 gaps=0,1 ignore=0,1 exact_words=1 ready_edges=1 no_false_error=1\n");
	}
	void check_receive_sync(unsigned i)
	{
		unsigned const widths[] = {8, 12, 16};
		unsigned const width = widths[i];
		u16 const mask = (1U << width) - 1;
		for (unsigned ignore = 0; ignore < 2; ++ignore)
		{
			receive_setup(i << 5, ignore ? 4 : 0, 0, 0x31);
			unsigned const irqs = m_rx_irqs;
			receive_word(0x17, width); // Keep an earlier complete word in DRR.
			m_mcbsp0->rx_frame_w(1);
			for (unsigned bit = width; bit-- > width - 4; )
			{
				receive_bit(BIT(0xa55a, bit)); m_mcbsp0->rx_frame_w(0);
			}
			m_mcbsp0->rx_frame_w(1); // A new sync well before the final word bit.
			if (ignore)
				for (unsigned bit = width - 4; bit-- > 0; )
				{
					receive_bit(BIT(0xa55a, bit)); m_mcbsp0->rx_frame_w(0);
				}
			else
				for (unsigned bit = width; bit-- > 0; )
				{
					receive_bit(BIT(0x5aa5, bit)); m_mcbsp0->rx_frame_w(0);
				}
			receive_bit(0);
			if ((receive_status() & 0xf) != (ignore ? 3 : 11) || m_rx_irqs != irqs + !ignore ||
				m_mcbsp0->data_r(1) != 0x17)
				fatalerror("McBSP receive sync must preserve DRR and report only non-ignored errors through RINTM=3");
			receive_bit(0);
			if (m_mcbsp0->data_r(1) != ((ignore ? 0xa55a : 0x5aa5) & mask) ||
				(receive_status() & 8) != (ignore ? 0 : 8) || m_rx_irqs != irqs + !ignore)
				fatalerror("McBSP receive sync replacement/ignore or sticky error contract mismatch");
			external_reg_w(0, 0x31); // Software clears RSYNCERR without resetting RX.
			if (receive_status() & 8) fatalerror("McBSP receive sync error did not clear on software zero");
			external_reg_w(0, 0x39); // It is R/W: software can also assert it.
			if (!(receive_status() & 8) || m_rx_irqs != irqs + !ignore + 1)
				fatalerror("McBSP receive sync software assertion did not drive RINT");
			external_reg_w(0, 0);
			if (receive_status() & 8) fatalerror("McBSP receive reset retained RSYNCERR");
		}
		if (i == 2) logerror("mu4_mcbsp_receive_sync: PASS cases=6 widths=8,12,16 replacement=1 ignore=1 drr_retained=1 sticky=1 software_clear=1 software_set=1 reset=1 error_irq=1\n");
	}
	void check_transmit_packing(unsigned index)
	{
		static constexpr unsigned widths[] = {8, 12, 16};
		static constexpr u16 words[] = {0xa55a, 0x5aa5, 0x1357};
		unsigned const i = index / 6, delay = (index / 2) % 3, ignore = index % 2;
		unsigned const width = widths[i];
		u16 const mask = (1U << width) - 1;
		for (unsigned gap = 0; gap < 2; ++gap)
		{
			external_reg_w(1, 0); external_reg_w(4, i << 5);
			external_reg_w(5, delay | (ignore ? 4 : 0)); external_reg_w(14, 0);
			m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0);
			m_external_words.clear(); m_external_bits.clear(); external_reg_w(1, 1);
			unsigned const irqs = m_external_tx_irqs, period = width + gap;
			m_mcbsp0->data_w(3, words[0]);
			unsigned queued = 1;
			for (unsigned cycle = 0; cycle < 3 * period + delay; ++cycle)
			{
				m_mcbsp0->tx_frame_w(cycle < 3 * period && cycle % period == 0);
				external_clocks(1, false);
				unsigned expected_bits = 0;
				for (unsigned word = 0; word < 3; ++word)
				{
					unsigned const first = word * period + (delay ? delay - 1 : 0);
					if (cycle >= first) expected_bits += std::min(width, cycle - first + 1);
				}
				if (m_external_bits.size() != expected_bits) fatalerror("McBSP transmit packing shifted on the wrong external clock edge");
				m_mcbsp0->control_w(0, 1);
				if (m_mcbsp0->control_r(1) & 8) fatalerror("McBSP legal transmit packing created XSYNCERR");
				if (queued < 3 && (m_mcbsp0->control_r(1) & 2)) m_mcbsp0->data_w(3, words[queued++]);
			}
			if (m_external_words != std::vector<u16>({u16(words[0] & mask), u16(words[1] & mask), u16(words[2] & mask)}) ||
				m_external_bits.size() != 3 * width || m_external_tx_irqs != irqs + 3)
				fatalerror("McBSP transmit packing lost a complete word or ready edge");
			for (unsigned bit = 0; bit < m_external_bits.size(); ++bit)
				if (m_external_bits[bit] != BIT(words[bit / width], width - 1 - bit % width))
					fatalerror("McBSP transmit packing changed physical bit order");
		}
		if (index == 17) logerror("mu4_mcbsp_transmit_packing: PASS cases=36 widths=8,12,16 delays=0,1,2 gaps=0,1 ignore=0,1 exact_words=1 physical_bits=1 ready_edges=1 no_false_error=1\n");
	}
	void check_transmit_sync(unsigned i)
	{
		static constexpr unsigned widths[] = {8, 12, 16};
		unsigned const width = widths[i];
		u16 const mask = (1U << width) - 1;
		for (unsigned ignore = 0; ignore < 2; ++ignore)
		{
			external_reg_w(1, 0); external_reg_w(4, 0x100 | (i << 5));
			external_reg_w(5, 1 | (ignore ? 4 : 0)); external_reg_w(14, 0);
			m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0);
			m_external_words.clear(); m_external_bits.clear(); external_reg_w(1, 1);
			unsigned const irqs = m_external_tx_irqs;
			m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1);
			external_clocks(4, false);
			m_mcbsp0->data_w(3, 0x5aa5);
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1);
			m_mcbsp0->control_w(0, 1);
			if ((m_mcbsp0->control_r(1) & 0xa) != (ignore ? 0 : 8) || m_external_tx_irqs != irqs + 1)
				fatalerror("McBSP transmit sync changed DXR readiness or error status incorrectly");
			external_clocks(2 * width - (ignore ? 4 : 0), false);
			if (m_external_words != std::vector<u16>({u16(0xa55a & mask), u16(0x5aa5 & mask)}) ||
				m_external_bits.size() != 2 * width + (ignore ? 0 : 4) || m_external_tx_irqs != irqs + 2)
				fatalerror("McBSP transmit sync must restart XSR and preserve queued DXR without extra ready IRQs");
			for (unsigned bit = 0; bit < m_external_bits.size(); ++bit)
			{
				unsigned const stream_bit = !ignore && bit >= 4 ? bit - 4 : bit;
				if (m_external_bits[bit] != BIT(stream_bit < width ? 0xa55a : 0x5aa5, width - 1 - stream_bit % width))
					fatalerror("McBSP transmit sync physical restart bit order mismatch");
			}
			external_reg_w(1, 1);
			if (m_mcbsp0->control_r(1) & 8) fatalerror("McBSP transmit sync software clear failed");
			unsigned const error_irqs = m_external_tx_irqs;
			external_reg_w(1, 0x39);
			if (!(m_mcbsp0->control_r(1) & 8) || m_external_tx_irqs != error_irqs + 1)
				fatalerror("McBSP transmit sync software set must drive XINTM=3");
			external_reg_w(1, 0x39);
			if (m_external_tx_irqs != error_irqs + 1) fatalerror("McBSP held transmit sync IRQ duplicated");
			external_reg_w(1, 0x31);
			external_reg_w(1, 0x39);
			if (m_external_tx_irqs != error_irqs + 2) fatalerror("McBSP transmit sync clear did not rearm error IRQ");
			external_reg_w(1, 0);
			if (m_mcbsp0->control_r(1) & 8) fatalerror("McBSP transmit reset retained sync error");
		}
		if (i == 2) logerror("mu4_mcbsp_transmit_sync: PASS cases=6 widths=8,12,16 xsr_restart=1 dxr_retained=1 ignore=1 physical_bits=1 ready_edges=1 sticky=1 software_clear=1 software_set=1 reset=1 error_irq=1\n");
	}
	void check_external_overwrite(unsigned i)
	{
		static constexpr unsigned widths[] = {8, 12, 16};
		unsigned const width = widths[i];
		u16 const mask = (1U << width) - 1;
		external_reg_w(1, 0); external_reg_w(4, 0x100 | (i << 5));
		external_reg_w(5, 1); external_reg_w(14, 0);
		m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0);
		m_external_words.clear(); m_external_bits.clear(); external_reg_w(1, 1);
		unsigned const irqs = m_external_tx_irqs;
		m_mcbsp0->data_w(3, 0x1234); m_mcbsp0->data_w(3, 0xa55a);
		m_mcbsp0->control_w(0, 1);
		if ((m_mcbsp0->control_r(1) & 2) || m_external_tx_irqs != irqs)
			fatalerror("McBSP buffered overwrite fabricated XRDY or a transmit interrupt");
		m_mcbsp0->tx_frame_w(1); external_clocks(1, false);
		// Overwrite the next DXR while the first word remains in XSR.
		m_mcbsp0->data_w(3, 0xabcd); m_mcbsp0->data_w(3, 0x5aa5);
		external_clocks(2 * width - 1, false);
		if (m_external_words != std::vector<u16>({u16(0xa55a & mask), u16(0x5aa5 & mask)}) ||
			m_external_bits.size() != 2 * width || m_external_tx_irqs != irqs + 2)
			fatalerror("McBSP overwrite must replace only DXR, preserving XSR and its physical bit stream");
		for (unsigned bit = 0; bit < 2 * width; ++bit)
			if (m_external_bits[bit] != BIT(bit < width ? 0xa55a : 0x5aa5, width - 1 - bit % width))
				fatalerror("McBSP overwritten transmit bit order mismatch");
		if (i == 2) logerror("mu4_mcbsp_overwrite: PASS widths=8,12,16 dxr_replaced=1 xsr_retained=1 ready_edges=1 physical_bits=1\n");
	}
	void check_external_underrun(unsigned i)
	{
		static constexpr unsigned widths[] = {8, 12, 16};
		if (i < std::size(widths))
		{
			external_reg_w(1, 0); external_reg_w(4, i << 5); external_reg_w(5, 1); external_reg_w(14, 0);
			m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0);
			m_external_words.clear(); m_external_bits.clear(); external_reg_w(1, 1);
			unsigned const empty_irq = m_external_tx_irqs;
			m_mcbsp0->tx_frame_w(1); external_clocks(widths[i], false);
			m_mcbsp0->control_w(0, 1);
			if (m_external_words != std::vector<u16>({0}) || (m_mcbsp0->control_r(1) & 7) != 3 || m_external_tx_irqs != empty_irq)
				fatalerror("MU4 McBSP empty frame must transmit zeros without fabricating ready edges");
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1);
			if ((m_mcbsp0->control_r(1) & 7) != 5) fatalerror("MU4 McBSP fresh transfer must deactivate XEMPTY");
			external_clocks(widths[i], false);
			unsigned const repeat_irq = m_external_tx_irqs;
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1);
			if ((m_mcbsp0->control_r(1) & 7) != 3) fatalerror("MU4 McBSP repeated DXR must retain empty status while shifting");
			external_clocks(widths[i], false);
			u16 const mask = (1U << widths[i]) - 1;
			if (m_external_words != std::vector<u16>({0, u16(0xa55a & mask), u16(0xa55a & mask)}) || m_external_tx_irqs != repeat_irq)
				fatalerror("MU4 McBSP underrun did not repeat old DXR without new ready events");
			for (unsigned bit = 0; bit < widths[i]; ++bit)
				if (m_external_bits[widths[i] * 2 + bit] != BIT(0xa55a, widths[i] - 1 - bit))
					fatalerror("MU4 McBSP underrun repeat bit order mismatch");
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->data_w(3, 0x1234);
			if ((m_mcbsp0->control_r(1) & 7) != 1) fatalerror("MU4 McBSP refill must not clear underflow before XSR transfer");
			m_mcbsp0->tx_frame_w(1); external_clocks(widths[i], false);
			if (m_external_words.back() != (0x1234 & mask) || m_external_tx_irqs != repeat_irq + 1)
				fatalerror("MU4 McBSP underrun recovery mismatch");
			return;
		}
		external_reg_w(1, 0); external_reg_w(4, 0x140);
		m_mcbsp0->tx_frame_w(0); external_reg_w(1, 1);
		m_external_words.clear(); m_external_bits.clear();
		m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1); external_clocks(32, false);
		if (m_external_words != std::vector<u16>({0xa55a}) || m_external_bits.size() != 16 || (m_mcbsp0->control_r(1) & 7) != 3)
			fatalerror("MU4 McBSP mid-frame underflow must stop shifting until the next frame");
		m_mcbsp0->data_w(3, 0x1234); external_clocks(16, false);
		if (m_external_words.size() != 1) fatalerror("MU4 McBSP underrun refill resumed without frame sync");
		m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1); external_clocks(1, false);
		m_mcbsp0->data_w(3, 0xabcd); external_clocks(31, false);
		if (m_external_words != std::vector<u16>({0xa55a, 0x1234, 0xabcd})) fatalerror("MU4 McBSP mid-frame underflow recovery mismatch");
		// The following save fixture repeats one complete 16-bit word.
		external_reg_w(4, 0x40);
		m_mcbsp0->data_w(3, 0x1234); m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1); external_clocks(16, false);
		logerror("mu4_mcbsp_underrun: PASS widths=8,12,16 empty_zero=1 repeat_dxr=1 xempty=1 ready_edges=1 recovery=1 midframe_wait=1\n");
	}
	void check_receive_overrun()
	{
		receive_setup(0, 0);
		unsigned const irqs = m_rx_irqs;
		receive_word(0x11, 8); receive_word(0x22, 8);
		if ((receive_status() & 7) != 3) fatalerror("MU4 McBSP RFULL asserted before three unread words");
		receive_word(0x33, 8); receive_word(0x44, 8);
		if ((receive_status() & 7) != 7 || m_rx_irqs != irqs + 1) fatalerror("MU4 McBSP overrun flag/ready edge mismatch");
		{ auto const disable = machine().disable_side_effects(); if (m_mcbsp0->data_r(1) != 0x11) fatalerror("MU4 McBSP overrun overwrote unread DRR"); }
		if ((receive_status() & 7) != 7 || m_mcbsp0->data_r(1) != 0x11 || (receive_status() & 7) != 1)
			fatalerror("MU4 McBSP overrun peek/clear mismatch");
		receive_bit(0);
		if (m_mcbsp0->data_r(1) != 0x22) fatalerror("MU4 McBSP overrun discarded buffered RBR");
		for (unsigned bit = 0; bit < 16; ++bit) receive_bit(0);
		if (receive_status() & 6) fatalerror("MU4 McBSP overrun recovered without a new frame");
		receive_word(0x55, 8);
		if (m_mcbsp0->data_r(1) != 0x55 || m_rx_irqs != irqs + 3) fatalerror("MU4 McBSP overrun frame recovery mismatch");
		receive_word(0x66, 8); receive_word(0x77, 8); receive_word(0x88, 8); external_reg_w(0, 0);
		if (receive_status() & 6) fatalerror("MU4 McBSP receiver reset did not clear RFULL/RRDY");
		logerror("mu4_mcbsp_overrun: PASS three_word_threshold=1 drr_retained=1 rbr_retained=1 rsr_loss=1 peek=1 read_clear=1 frame_recovery=1 reset=1\n");
	}
	void check_headphone_level(unsigned channel, double expected)
	{
		double const actual = subdevice<tlv320aic23_device>("codec")->headphone_dac_level(channel);
		if (!std::isfinite(actual) || std::abs(actual-expected) > 1e-9)
			fatalerror("MU4 ideal headphone DAC level mismatch channel=%u actual=%.12f expected=%.12f",channel,actual,expected);
	}
	static constexpr std::array<char const *,3> PLAYBACK_CAPTURE_FILES{
		"mu4_playback_tx.le16", "mu4_playback_din.le16", "mu4_playback_dma.le16"};
	void capture_playback_word(unsigned boundary, u16 value)
	{
		char const bytes[2]{char(value & 0xff), char(value >> 8)};
		m_playback_word_capture[boundary].write(bytes, sizeof(bytes));
	}
	void capture_playback_slot(unsigned slot)
	{
		if (slot > 1) fatalerror("MU4 playback observation received an invalid codec slot");
		// Keep actual callback framing/time; stream-index parity is not a slot oracle.
		u64 const ns = machine().time().as_ticks(1'000'000'000);
		char bytes[9];
		for (unsigned i=0;i<8;++i) bytes[i] = char(ns >> (8*i));
		bytes[8] = char(slot);
		m_playback_slot_capture.write(bytes,sizeof(bytes));
		for (char byte : bytes) m_playback_slot_hash = (m_playback_slot_hash ^ u8(byte)) * 16777619U;
	}
	void observe_playback_tx(offs_t event, u16 word)
	{
		double const time = machine().time().as_double();
		bool const startup = time >= 0.817 && time <= 0.824;
		if (!playback_profile()) return;
		static constexpr std::array<char const *,9> names{"reset","ignore","accept","overwrite","load_fresh","load_repeat","complete","restart","wait_frame"};
		if (event >= names.size()) fatalerror("MU4 invalid McBSP diagnostic transition");
		u8 owner = 0;
		if (event == tms320c54x_mcbsp_device::TX_ACCEPT || event == tms320c54x_mcbsp_device::TX_OVERWRITE || event == tms320c54x_mcbsp_device::TX_IGNORE)
			for (unsigned channel=0;channel<6;++channel)
				if (m_dma->destination_write_matches(channel, AS_DATA, 0x23)) owner = channel+1;
		if (m_playback_overlay_checked && m_playback_tx_event_capture.is_open())
		{
			if (++m_playback_tx_events <= PLAYBACK_TX_EVENT_CAP)
			{
				u64 const ns = machine().time().as_ticks(1'000'000'000);
				u32 const source = owner == 4 ? m_playback_dma_reads : 0;
				char bytes[16];
				for (unsigned i=0;i<8;++i) bytes[i] = char(ns >> (8*i));
				bytes[8] = char(word); bytes[9] = char(word >> 8);
				bytes[10] = char(event); bytes[11] = char(owner);
				for (unsigned i=0;i<4;++i) bytes[12+i] = char(source >> (8*i));
				m_playback_tx_event_capture.write(bytes,sizeof(bytes));
				for (char byte : bytes) m_playback_tx_event_hash = (m_playback_tx_event_hash ^ u8(byte))*16777619U;
			}
		}
		if (!startup && (time < 1.0 || time > 1.25)) return;
		u32 &counter = startup ? m_playback_tx_startup_events : m_playback_tx_lifecycle_events;
		++counter;
		if (counter > (startup ? 4096U : 100000U)) return;
		if (startup)
			logerror("mu4_tx_startup: ordinal=%u kind=%s word=%04x ns=%llu dma_owner=%u dma_read=%u pcr=%04x spcr2=%04x xcr1=%04x xcr2=%04x observer_writes=0 audio_acceptance=0\n",
				counter,names[event],word,static_cast<unsigned long long>(machine().time().as_ticks(1'000'000'000)),owner,owner == 4 ? m_playback_dma_reads : 0,
				m_mcbsp0->inspect_register(14),m_mcbsp0->inspect_register(1),m_mcbsp0->inspect_register(4),m_mcbsp0->inspect_register(5));
		else
			logerror("mu4_tx_lifecycle: ordinal=%u kind=%s word=%04x ns=%llu dma_owner=%u dma_read=%u observer_writes=0 audio_acceptance=0\n",
				counter,names[event],word,static_cast<unsigned long long>(machine().time().as_ticks(1'000'000'000)),owner,owner == 4 ? m_playback_dma_reads : 0);
	}
	void finish_playback_capture()
	{
		if (m_playback_capture_finished) return;
		char const *const restore_scope = m_playback_capture_loads ? "same_process_rewind" : "unsupported";
		std::array<u32,3> const counts{m_playback_audio_words[0],m_playback_audio_words[1],m_playback_dma_reads};
		std::array<u32,3> const hashes{m_playback_audio_hash[0],m_playback_audio_hash[1],m_playback_dma_hash};
		for (unsigned i=0;i<3;++i)
		{
			auto &stream=m_playback_word_capture[i]; stream.flush(); auto const bytes=stream.tellp(); stream.close();
			if (!stream || bytes != std::streamoff(counts[i])*2) fatalerror("MU4 playback observation capture is incomplete");
			logerror("mu4_playback_word_capture: boundary=%s words=%u bytes=%u hash=%08x encoding=le16 file=%s save_restore=%s observer_writes=0 audio_acceptance=0\n",
				std::array<char const *,3>{"mcbsp_tx","codec_din","dma_source"}[i],counts[i],counts[i]*2,hashes[i],PLAYBACK_CAPTURE_FILES[i],restore_scope);
		}
		m_playback_slot_capture.flush(); auto const slot_bytes=m_playback_slot_capture.tellp(); m_playback_slot_capture.close();
		if (!m_playback_slot_capture || slot_bytes != std::streamoff(counts[1])*9) fatalerror("MU4 playback slot observation is incomplete");
		logerror("mu4_playback_slot_capture: records=%u bytes=%u fnv1a=%08x encoding=le64ns_u8slot file=mu4_playback_din.slots save_restore=%s observer_writes=0 audio_acceptance=0\n",
			counts[1],counts[1]*9,m_playback_slot_hash,restore_scope);
		m_playback_tx_time_capture.flush(); auto const tx_bytes=m_playback_tx_time_capture.tellp(); m_playback_tx_time_capture.close();
		if (!m_playback_tx_time_capture || tx_bytes != std::streamoff(counts[0])*8) fatalerror("MU4 playback TX time observation is incomplete");
		logerror("mu4_playback_tx_time_capture: records=%u bytes=%u fnv1a=%08x encoding=le64ns file=mu4_playback_tx.times save_restore=%s observer_writes=0 audio_acceptance=0\n",
			counts[0],counts[0]*8,m_playback_tx_time_hash,restore_scope);
		m_playback_dxr_capture.flush(); auto const dxr_bytes=m_playback_dxr_capture.tellp(); m_playback_dxr_capture.close();
		if (!m_playback_dxr_capture || dxr_bytes != std::streamoff(m_playback_dxr_writes)*16) fatalerror("MU4 playback DXR observation is incomplete");
		logerror("mu4_playback_dxr_capture: records=%u bytes=%u fnv1a=%08x encoding=le64ns_u32ordinal_u16word_u8owner_u8reserved file=mu4_playback_dxr.writes save_restore=%s observer_writes=0 audio_acceptance=0\n",
			m_playback_dxr_writes,m_playback_dxr_writes*16,m_playback_dxr_hash,restore_scope);
		m_playback_tx_event_capture.flush(); auto const event_bytes=m_playback_tx_event_capture.tellp(); m_playback_tx_event_capture.close();
		u32 const captured_events = std::min(m_playback_tx_events,PLAYBACK_TX_EVENT_CAP);
		if (!m_playback_tx_event_capture || event_bytes != std::streamoff(captured_events)*16) fatalerror("MU4 playback TX event observation is incomplete");
		logerror("mu4_tx_event_capture: records=%u bytes=%u fnv1a=%08x capped=%u encoding=le64ns_u16word_u8event_u8owner_u32source file=mu4_playback_tx.events save_restore=%s observer_writes=0 audio_acceptance=0\n",
			captured_events,captured_events*16,m_playback_tx_event_hash,unsigned(m_playback_tx_events>PLAYBACK_TX_EVENT_CAP),restore_scope);
		m_playback_dac_capture.flush(); auto const dac_bytes=m_playback_dac_capture.tellp(); m_playback_dac_capture.close();
		u32 const captured_dac = std::min(m_playback_dac_capture_frames,PLAYBACK_DAC_FRAME_CAP);
		if (!m_playback_dac_capture || dac_bytes != std::streamoff(captured_dac)*32) fatalerror("MU4 full DAC observation is incomplete");
		logerror("mu4_dac_capture: frames=%u bytes=%u fnv1a=%08x capped=%u encoding=le64ns_i16left_i16right_u8lvol_u8rvol_u16reserved_f64left_f64right file=mu4_headphone.frames save_restore=%s observer_writes=0 analog_filter=0 audio_acceptance=0\n",
			captured_dac,captured_dac*32,m_playback_dac_capture_hash,unsigned(m_playback_dac_capture_frames>PLAYBACK_DAC_FRAME_CAP),restore_scope);
		m_playback_decoded_capture.flush(); auto const decoded_bytes=m_playback_decoded_capture.tellp(); m_playback_decoded_capture.close();
		if (!m_playback_decoded_capture || decoded_bytes != std::streamoff(m_playback_decoded_words)*2) fatalerror("MU4 decoded-return observation is incomplete");
		logerror("mu4_decoded_return_capture: calls=%u returns=%u blocks=%u words=%u bytes=%u fnv1a=%08x bad=%u pending=%u capped=%u encoding=le16 file=mu4_decoded_returns.le16 retirement=unclaimed save_restore=%s observer_writes=0 audio_acceptance=0\n",
			m_playback_processing[0],m_playback_processing[1],m_playback_processing[2],m_playback_decoded_words,m_playback_decoded_words*2,m_playback_decoded_hash,m_playback_processing[3],
			unsigned(m_playback_processing_pending || m_playback_conversion_active || m_playback_conversion_pending),unsigned(m_playback_processing[0]>=512),restore_scope);
		m_playback_capture_finished = true;
		logerror("mu4_playback_capture_terminal: closed=1 illegal=%u t=%.9f observer_writes=0 audio_acceptance=0\n",
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), machine().time().as_double());
		logerror("mu4_tx_lifecycle_terminal: events=%u first=1.000000000 last=1.250000000 capped=%u t=%.9f observer_writes=0 audio_acceptance=0\n",
			m_playback_tx_lifecycle_events,unsigned(m_playback_tx_lifecycle_events>100000),machine().time().as_double());
		logerror("mu4_tx_startup_terminal: events=%u frames=%u first=0.817000000 last=0.824000000 capped=%u t=%.9f observer_writes=0 audio_acceptance=0\n",
			m_playback_tx_startup_events,m_playback_startup_frames,unsigned(m_playback_tx_startup_events>4096),machine().time().as_double());
		logerror("mu4_dac_terminal: frames=%u first=1.060000000 last=1.100000000 capped=%u t=%.9f observer_writes=0 analog_filter=0 audio_acceptance=0\n",
			m_playback_dac_frames,unsigned(m_playback_dac_frames>4096),machine().time().as_double());
	}
	void observe_playback_audio(u16 value, bool din)
	{
		unsigned const which = din ? 1 : 0;
		capture_playback_word(which,value);
		if (!din)
		{
			// Timestamp the existing completed-word callback, not a guest read.
			u64 const ns = machine().time().as_ticks(1'000'000'000);
			char bytes[8];
			for (unsigned i=0;i<8;++i) bytes[i] = char(ns >> (8*i));
			m_playback_tx_time_capture.write(bytes,sizeof(bytes));
			for (char byte : bytes) m_playback_tx_time_hash = (m_playback_tx_time_hash ^ u8(byte)) * 16777619U;
		}
		if (m_playback_audio_words[which]++ < 4)
			logerror("mu4_playback_audio_word: boundary=%s word=%04x t=%.9f pc=%06x\n", din ? "codec_din" : "mcbsp_tx", value, machine().time().as_double(), unsigned(m_cpu->state_int(STATE_GENPC)));
		if (value)
		{
			m_playback_audio_last_nonzero[which] = machine().time().as_double();
			if (m_playback_audio_nonzero[which]++ < 4)
				logerror("mu4_playback_audio_nonzero: boundary=%s word=%04x t=%.9f\n", din ? "codec_din" : "mcbsp_tx", value, m_playback_audio_last_nonzero[which]);
		}
		m_playback_audio_hash[which] = (m_playback_audio_hash[which] ^ (value >> 8)) * 16777619U;
		m_playback_audio_hash[which] = (m_playback_audio_hash[which] ^ (value & 0xff)) * 16777619U;
	}
	void observe_playback_parent(offs_t address, u16 opcode)
	{
		std::array<u32, 10> const sites{0x2a10c, 0x2a139, 0x2a22e, 0x2a152, 0x2a138, 0x2a2c6, 0x205d0, 0x205ac, 0x205b3, 0x205ae};
		std::array<u16, 10> const words{0x4a11, 0x6fe1, 0x11e1, 0x6101, 0xf4e4, 0x771a, 0x7711, 0x6c81, 0x71e7, 0x76f8};
		auto const found = std::find(sites.begin(), sites.end(), address);
		if (found == sites.end()) return;
		unsigned const site = found - sites.begin();
		if (opcode != words[site]) { ++m_playback_parent_mismatch; return; }
		if (++m_playback_parent[site] > 2) return;
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		auto const ram_words = [&data](u32 pointer) {
			std::string result;
			if (pointer < 0x80 || pointer > 0xfff0) return std::string("unobserved");
			for (unsigned i = 0; i < 16; ++i) result += util::string_format("%s%04x", i ? "," : "", data.read_word(pointer + i));
			return result;
		};
		u32 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP), ar1 = m_cpu->state_int(tms320c54x_device::STATE_AR1);
		logerror("mu4_playback_parent_context: site=%u ordinal=%u address=%06x opcode=%04x sp=%04x ar1=%04x ar6=%04x ar7=%04x a=%010llx b=%010llx stack=%s frame=%s t=%.9f identity=Execute_entry context_only=1 observer_writes=0\n",
			site, m_playback_parent[site], unsigned(address), opcode, sp, ar1,
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR7)),
			static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)),
			static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)),
			ram_words(sp).c_str(), ram_words(ar1).c_str(), machine().time().as_double());
		if (site == 7)
		{
			u16 const receiver = data.read_word(0xaa5a);
			logerror("mu4_playback_selector_context: ordinal=%u receiver=%04x words=%s ar1=%04x a=%010llx t=%.9f identity=Execute_entry context_only=1 observer_writes=0\n",
				m_playback_parent[site], receiver, ram_words(receiver).c_str(), ar1,
				static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)), machine().time().as_double());
		}
	}
	void write_playback_snapshot(char const *prefix, char const *marker, u32 invocation, unsigned stage,
		offs_t address, u16 opcode, u16 base, unsigned words, u16 parameter)
	{
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		std::string const name = util::string_format("%s_%03u_stage%u.be16",prefix,invocation,stage);
		std::ofstream file(name,std::ios::binary | std::ios::trunc);
		u32 hash = 2166136261U;
		for (unsigned i=0;i<words;++i)
		{
			u16 const value = data.read_word(base+i);
			char const bytes[2]{char(value>>8),char(value)};
			file.write(bytes,2);
			hash = ((hash ^ u8(value>>8))*16777619U ^ u8(value))*16777619U;
		}
		file.close();
		if (!file) fatalerror("MU4 playback observation file incomplete");
		logerror("%s_snapshot: invocation=%u call=%u stage=%u address=%06x opcode=%04x base=%04x words=%u parameter=%04x hash=%08x file=%s t=%.9f identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0\n",
			marker,invocation,m_playback_processing[0],stage,unsigned(address),opcode,base,words,
			parameter,hash,name.c_str(),machine().time().as_double());
	}
	void observe_playback_tail(offs_t address, u16 opcode)
	{
		if (!m_playback_processing_pending) return;
		bool const entry = address == 0x24a7e;
		if (entry)
		{
			auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
			u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
			m_playback_tail_base = data.read_word(u16(sp+4));
			m_playback_tail_history = data.read_word(u16(sp+8));
			m_playback_tail_saved = data.read_word(u16(sp+9));
			++m_playback_tail_ordinal;
			if (m_playback_tail_base < 0x60 || unsigned(m_playback_tail_base)+2048 > 0x10000 ||
				m_playback_tail_history < 0x60 || unsigned(m_playback_tail_history)+512 > 0x10000 ||
				m_playback_tail_saved < 0x60 || unsigned(m_playback_tail_saved)+256 > 0x10000)
				fatalerror("MU4 final-tail observation geometry changed");
			logerror("mu4_tail_arguments: tail=%u invocation=%u call=%u mode=%04x base=%04x history=%04x saved=%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0\n",
				m_playback_tail_ordinal,m_playback_fft_invocation,m_playback_processing[0],data.read_word(u16(sp+7)),
				m_playback_tail_base,m_playback_tail_history,m_playback_tail_saved,machine().time().as_double());
		}
		if (!m_playback_tail_base) return;
		unsigned const stage = entry ? 0 : 3;
		write_playback_snapshot("mu4_tail","mu4_tail",m_playback_tail_ordinal,stage,address,opcode,m_playback_tail_base,2048,1024);
		write_playback_snapshot("mu4_tail","mu4_tail",m_playback_tail_ordinal,stage+1,address,opcode,m_playback_tail_history,512,1024);
		write_playback_snapshot("mu4_tail","mu4_tail",m_playback_tail_ordinal,stage+2,address,opcode,m_playback_tail_saved,256,1024);
		if (!entry) m_playback_tail_base = 0;
	}
	void observe_playback_window_callback(offs_t address, u16 opcode)
	{
		if (!m_playback_processing_pending) return;
		char const *phase = nullptr;
		if (address == 0x24a60 || address == 0x24a63)
		{
			auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
			u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
			m_playback_callback_site = address == 0x24a60 ? 1 : 2;
			m_playback_callback_target = m_cpu->state_int(tms320c54x_device::STATE_B) & 0x7fffff;
			m_playback_callback_mode = data.read_word(u16(sp+9));
			m_playback_callback_base = m_cpu->state_int(tms320c54x_device::STATE_A);
			m_playback_callback_history = data.read_word(u16(sp+2));
			if (m_playback_callback_site == 2)
			{
				if (m_playback_callback_base < 0x60 || m_playback_callback_base > 0xfffe)
					fatalerror("MU4 coefficient-selector observation geometry changed");
				logerror("mu4_coefficient_selection: invocation=%u call=%u target=%06x base=%04x flag0=%04x flag1=%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0\n",
					m_playback_fft_invocation,m_playback_processing[0],m_playback_callback_target,m_playback_callback_base,
					data.read_word(m_playback_callback_base),data.read_word(m_playback_callback_base+1),machine().time().as_double());
			}
			phase = "call";
		}
		else if (!m_playback_callback_site) return;
		else if (address == 0x24a61 || address == 0x24a64) phase = "return";
		else if (address == m_playback_callback_target) phase = "entry";
		if (!phase) return;
		logerror("mu4_window_callback: invocation=%u call=%u site=%u phase=%s address=%06x opcode=%04x target=%06x mode=%04x base=%04x history=%04x parameter=%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0\n",
			m_playback_fft_invocation,m_playback_processing[0],unsigned(m_playback_callback_site-1),phase,
			unsigned(address),opcode,m_playback_callback_target,m_playback_callback_mode,m_playback_callback_base,
			m_playback_callback_history,m_playback_fft_parameter,machine().time().as_double());
		if (m_playback_callback_site == 1 && address != m_playback_callback_target)
		{
			unsigned const history_words = m_playback_callback_target == 0x24b3e ? 64 : 512;
			if (m_playback_callback_base < 0x60 || m_playback_callback_history < 0x60 ||
				unsigned(m_playback_callback_base)+2*m_playback_fft_parameter > 0x10000 ||
				unsigned(m_playback_callback_history)+history_words > 0x10000)
				fatalerror("MU4 window callback observation geometry changed");
			unsigned const stage = address == 0x24a60 ? 0 : 2;
			write_playback_snapshot("mu4_history","mu4_history",m_playback_fft_invocation,stage,address,opcode,m_playback_callback_base,2*m_playback_fft_parameter,m_playback_fft_parameter);
			write_playback_snapshot("mu4_history","mu4_history",m_playback_fft_invocation,stage+1,address,opcode,m_playback_callback_history,history_words,m_playback_fft_parameter);
		}
		if (m_playback_callback_site == 2 && address != m_playback_callback_target)
			write_playback_snapshot("mu4_coefficients","mu4_coefficients",m_playback_fft_invocation,address == 0x24a63 ? 0 : 1,
				address,opcode,0xf000,2048,m_playback_fft_parameter);
		if (address == 0x24a61 || address == 0x24a64) m_playback_callback_site = 0;
	}
	void observe_playback_scale(offs_t address, u16 opcode)
	{
		if (!m_playback_processing_pending) return;
		if (address == 0x24a46)
		{
			auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
			u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
			m_playback_scale_parameter = data.read_word(u16(sp+2));
			m_playback_scale_base = m_cpu->state_int(tms320c54x_device::STATE_A);
			if ((m_playback_scale_parameter != 0x80 && m_playback_scale_parameter != 0x400) ||
				m_playback_scale_base < 0x60 || unsigned(m_playback_scale_base)+2*m_playback_scale_parameter > 0x10000)
				fatalerror("MU4 scale observation geometry changed");
			logerror("mu4_scale_arguments: invocation=%u call=%u parameter=%04x first=%04x second=%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0\n",
				m_playback_fft_invocation,m_playback_processing[0],m_playback_scale_parameter,
				data.read_word(u16(sp+3)),data.read_word(u16(sp+4)),machine().time().as_double());
			write_playback_snapshot("mu4_scale","mu4_scale",m_playback_fft_invocation,0,address,opcode,m_playback_scale_base,2*m_playback_scale_parameter,m_playback_scale_parameter);
		}
		else if (m_playback_scale_base)
		{
			write_playback_snapshot("mu4_scale","mu4_scale",m_playback_fft_invocation,1,address,opcode,m_playback_scale_base,2*m_playback_scale_parameter,m_playback_scale_parameter);
			m_playback_scale_base = 0;
		}
	}
	void observe_playback_window(offs_t address, u16 opcode)
	{
		if (!m_playback_processing_pending) return;
		if (address == 0x24a70)
		{
			auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
			u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
			u16 const parameter = 2*data.read_word(u16(sp+2));
			u16 const coefficients = data.read_word(u16(sp+3));
			m_playback_window_base = m_cpu->state_int(tms320c54x_device::STATE_A);
			m_playback_window_parameter = parameter;
			if ((parameter != 0x80 && parameter != 0x400) || m_playback_window_base < 0x60 || coefficients < 0x60 ||
				unsigned(m_playback_window_base)+2*parameter > 0x10000 || unsigned(coefficients)+2*parameter > 0x10000)
				fatalerror("MU4 window observation geometry changed");
			write_playback_snapshot("mu4_window","mu4_window",m_playback_fft_invocation,0,address,opcode,m_playback_window_base,2*parameter,parameter);
			write_playback_snapshot("mu4_window","mu4_window",m_playback_fft_invocation,1,address,opcode,coefficients,2*parameter,parameter);
		}
		else if (m_playback_window_base)
		{
			write_playback_snapshot("mu4_window","mu4_window",m_playback_fft_invocation,2,address,opcode,m_playback_window_base,2*m_playback_window_parameter,m_playback_window_parameter);
			m_playback_window_base = 0;
		}
	}
	void observe_playback_transform(offs_t address, u16 opcode)
	{
		if (!m_playback_processing_pending) return;
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		bool const entry = address == 0x24919 || address == 0x24944;
		if (entry)
		{
			u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
			// FCALL has allocated its two return words; caller SP(0) is the vector pointer.
			m_playback_fft_parameter = m_cpu->state_int(tms320c54x_device::STATE_A);
			m_playback_fft_base = data.read_word(u16(sp+2));
			++m_playback_fft_invocation;
			logerror("mu4_fft_invocation: call=%u address=%06x opcode=%04x parameter=%04x base=%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0\n",
				m_playback_processing[0],unsigned(address),opcode,m_playback_fft_parameter,
				m_playback_fft_base,machine().time().as_double());
			if ((m_playback_fft_parameter != 0x80 && m_playback_fft_parameter != 0x400) ||
				m_playback_fft_base < 0x60 || unsigned(m_playback_fft_base)+2*m_playback_fft_parameter > 0x10000)
				fatalerror("MU4 transform observation geometry changed");
		}
		if (!m_playback_fft_base) return;
		auto const snapshot = [&](unsigned stage, u16 base, unsigned words) {
			write_playback_snapshot("mu4_fft","mu4_fft_vector",m_playback_fft_invocation,stage,address,opcode,base,words,m_playback_fft_parameter);
		};
		if (entry)
		{
			snapshot(0,m_playback_fft_base,2*m_playback_fft_parameter);
			snapshot(10,0xf400,1024);
			snapshot(11,0x117a,2);
		}
		else if (address == 0x2491e || address == 0x24946)
		{
			snapshot(9,m_playback_fft_base,2*m_playback_fft_parameter);
			m_playback_fft_base = 0;
		}
		else
		{
			std::array<u32,8> const sites{0x2400a,0x2401e,0x24036,0x24051,0x2406c,0x2408b,0x240a6,0x240be};
			auto const found = std::find(sites.begin(),sites.end(),address);
			if (found == sites.end()) return;
			snapshot(1+unsigned(found-sites.begin()),m_playback_fft_base,2*m_playback_fft_parameter);
		}
	}
	void observe_playback_rotation(offs_t address, u16 opcode, bool pre)
	{
		if (!m_playback_processing_pending) return;
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		u16 &vector_base = pre ? m_playback_pre_base : m_playback_post_base;
		u16 &parameter = pre ? m_playback_pre_parameter : m_playback_fft_parameter;
		u32 const invocation = m_playback_fft_invocation + unsigned(pre);
		char const *prefix = pre ? "mu4_pre" : "mu4_post";
		bool const entry = pre ? (address == 0x2490f || address == 0x24934) : address != 0x24950;
		if (entry)
		{
			u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
			if (pre) parameter = m_cpu->state_int(tms320c54x_device::STATE_A);
			vector_base = data.read_word(u16(sp+2));
			if ((parameter != 0x80 && parameter != 0x400) ||
				u16(m_cpu->state_int(tms320c54x_device::STATE_A)) != parameter ||
				vector_base < 0x60 || unsigned(vector_base)+2*parameter > 0x10000)
				fatalerror("MU4 rotation observation geometry changed");
		}
		if (!vector_base) return;
		auto const snapshot = [&](unsigned stage, u16 base, unsigned words) {
			write_playback_snapshot(prefix,prefix,invocation,stage,address,opcode,base,words,parameter);
		};
		if (entry)
		{
			snapshot(0,vector_base,2*parameter);
			snapshot(1,parameter == 0x80 ? 0xf200 : 0xf000,2*parameter);
		}
		else
		{
			snapshot(2,vector_base,2*parameter);
			vector_base = 0;
		}
	}
	void observe_decoder_gate(offs_t address, u16 opcode)
	{
		double const time = machine().time().as_double();
		if (time < 9.64 || time > 9.86) return;
		std::array<u32,17> const sites{0x2936d,0x293a2,0x293a8,0x293aa,0x293ad,0x293ae,
			0x293b0,0x293b5,0x293b7,0x293cd,0x293cf,0x293df,0x293e1,0x293f1,0x293f4,0x293fe,0x2940b};
		std::array<u16,17> const words{0x10f8,0x10f8,0xf982,0x8812,0xf5aa,0xf830,
			0x7211,0xf845,0x10f8,0xf982,0x8811,0x6c81,0x76f8,0x6ce1,0x10f8,0x10f8,0x10f8};
		auto const found = std::find(sites.begin(),sites.end(),address);
		if (found == sites.end()) return;
		if (opcode != words[found-sites.begin()]) ++m_decoder_gate_bad;
		if (++m_decoder_gate_events > 512) return;
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		u16 const object = data.read_word(0x15e9), context = m_cpu->state_int(tms320c54x_device::STATE_AR6);
		std::string input = "unobserved", activity = "unobserved";
		if (object >= 0x60 && object <= 0xfffe) input = util::string_format("%04x,%04x",data.read_word(object),data.read_word(object+1));
		if (context >= 0x60 && context <= 0xffe9) activity = util::string_format("%04x",data.read_word(context+0x16));
		logerror("mu4_decoder_gate: ordinal=%u address=%06x opcode=%04x call=%u a=%010llx ar0=%04x ar1=%04x ar2=%04x tc=%u object=%04x input=%s threshold_flag=%04x activity=%04x context=%04x context_activity=%s t=%.9f identity=Execute_entry operands=context_only observer_writes=0\n",
			m_decoder_gate_events,unsigned(address),opcode,m_playback_processing[0],
			(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A)&0xffffffffffULL,
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),unsigned(BIT(m_cpu->state_int(tms320c54x_device::STATE_ST0),12)),
			object,input.c_str(),data.read_word(0x1310),data.read_word(0x15d9),context,activity.c_str(),time);
	}
	void observe_context_calls(offs_t address, u16 opcode)
	{
		std::array<u32,23> const sites{0x2044a,0x20454,0x20461,0x2046b,
			0x20453,0x20460,0x2046a,0x20477,
			0x204a6,0x204bc,0x20508,0x20581,0x2059c,0x205f3,
			0x27a99,0x27a9c,0x27a9f,0x28feb,0x28fee,0x29332,0x2935a,0x293c1,0x293ed};
		auto const found = std::find(sites.begin(),sites.end(),address);
		if (found == sites.end()) return;
		double const time = machine().time().as_double();
		if (time < 9.64 || time > 9.86) return;
		unsigned const index = found-sites.begin();
		char const *kind = index < 4 ? "entry" : index < 8 ? "return" : "call";
		if (opcode != (index < 4 ? 0xf6be : index < 8 ? 0xf4e4 : 0xf982)) ++m_context_call_bad;
		if (++m_context_call_events > 1024) return;
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		logerror("mu4_context_call: ordinal=%u address=%06x opcode=%04x kind=%s a=%010llx ar1=%04x aa55=%04x aa56=%04x aaa2=%04x aaa3=%04x t=%.9f identity=Execute_entry operands=context_only observer_writes=0\n",
				m_context_call_events,unsigned(address),opcode,kind,
			(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A)&0xffffffffffULL,
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),data.read_word(0xaa55),data.read_word(0xaa56),
			data.read_word(0xaaa2),data.read_word(0xaaa3),time);
	}
	void observe_exchange_branch(offs_t address, u16 opcode)
	{
		if (address != 0x20553 && address != 0x20555 && address != 0x20580 && address != 0x20586) return;
		double const time = machine().time().as_double();
		if (time < 9.64 || time > 9.86) return;
		if (opcode != (address == 0x20553 ? 0xf845 : address == 0x20580 ? 0x4811 : 0x10f8)) ++m_exchange_branch_bad;
		if (++m_exchange_branch_events > 512) return;
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		u16 const receiver = data.read_word(0xaa5a), context = m_cpu->state_int(tms320c54x_device::STATE_AR1);
		bool const valid = receiver >= 0x60 && receiver <= 0xfff2 && context >= 0x60 && context <= 0xffe9;
		if (!valid) ++m_exchange_branch_bad;
		logerror("mu4_exchange_branch: ordinal=%u address=%06x opcode=%04x receiver=%04x table=%04x field9=%04x field13=%04x context=%04x count22=%04x flag1310=%04x t=%.9f identity=Execute_entry operands=context_only observer_writes=0\n",
			m_exchange_branch_events,unsigned(address),opcode,receiver,valid ? data.read_word(receiver) : 0xffff,
			valid ? data.read_word(receiver+9) : 0xffff,valid ? data.read_word(receiver+13) : 0xffff,
			context,valid ? data.read_word(context+22) : 0xffff,data.read_word(0x1310),time);
	}
	void observe_playback_processing(offs_t address, u16 opcode)
	{
		if (address != 0x23de5 && address != 0x23de7 && address != 0x23e1b && address != 0x23e1d &&
			address != 0x23e21 && address != 0x23e23 && address != 0x23e2c && address != 0x248b1) return;
		if (m_playback_processing[0] >= 512 && !m_playback_processing_pending && !m_playback_conversion_active) return;
		auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
		u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
		u64 const a = m_cpu->state_int(tms320c54x_device::STATE_A) & 0xffffffffffULL;
		if (address == 0x23de5)
		{
			if (m_playback_processing[0] >= 512) return;
			if (opcode != 0xf982 || m_playback_processing_pending || m_playback_conversion_active) ++m_playback_processing[3];
			// FCALL reserves its two stack words before this Execute entry.
			++m_playback_processing[0]; m_playback_processing_pending = true; m_playback_processing_sp = sp+2;
			u16 const base = data.read_word(0xb332), index = data.read_word(0xb331);
			u16 const start = base + index - 8;
			std::string window = "unobserved";
			if (start >= 0x60 && start <= 0xfff4)
			{
				window.clear();
				for (unsigned i=0;i<12;++i) window += util::string_format("%04x",data.read_word(start+i));
			}
			if (m_playback_processing[0] <= 8) logerror("mu4_processing_entry: call=%u sp=%04x caller_sp=%04x a=%010llx base=%04x index=%04x remaining=%04x reservoir=%04x%04x window_start=%04x window=%s st1=%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0\n",
				m_playback_processing[0],sp,m_playback_processing_sp,(unsigned long long)a,base,index,data.read_word(0xa854),data.read_word(0xa858),data.read_word(0xa859),start,window.c_str(),unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)),machine().time().as_double());
			logerror("mu4_decoder_call: call=%u phase=entry base=%04x index=%04x remaining=%04x reservoir=%04x%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0\n",
				m_playback_processing[0],base,index,data.read_word(0xa854),data.read_word(0xa858),data.read_word(0xa859),machine().time().as_double());
		}
		else if (address == 0x23de7 && m_playback_processing_pending)
		{
			if (opcode != 0x800f || sp != m_playback_processing_sp) ++m_playback_processing[3];
			++m_playback_processing[1]; m_playback_processing_pending = false;
			m_playback_conversion_active = u16(a) == 0;
			m_playback_conversion_pending = false; m_playback_conversion = {};
			if (m_playback_conversion_active) ++m_playback_processing[2];
			m_playback_processing_destination = data.read_word(sp+12);
			if (m_playback_processing[0] <= 8) logerror("mu4_processing_return: call=%u result=%010llx sp=%04x input0=%04x input1=%04x destination=%04x index=%04x remaining=%04x t=%.9f identity=Execute_entry observer_writes=0\n",
				m_playback_processing[0],(unsigned long long)a,sp,data.read_word(sp+6),data.read_word(sp+5),m_playback_processing_destination,data.read_word(0xb331),data.read_word(0xa854),machine().time().as_double());
			logerror("mu4_decoder_call: call=%u phase=return result=%010llx index=%04x remaining=%04x t=%.9f identity=Execute_entry context_only=1 observer_writes=0\n",
				m_playback_processing[0],(unsigned long long)a,data.read_word(0xb331),data.read_word(0xa854),machine().time().as_double());
		}
		else if ((address == 0x23e1b || address == 0x23e21) && m_playback_conversion_active)
		{
			u16 const channel = address == 0x23e21;
			if (opcode != 0xf982 || m_playback_conversion_pending || channel != m_playback_conversion[1]) ++m_playback_processing[3];
			m_playback_conversion_pending = true; m_playback_conversion_shift_seen = false;
			m_playback_conversion[2] = data.read_word(u16(sp+2)); m_playback_conversion[3] = sp+2;
			m_playback_conversion_raw = a;
		}
		else if (address == 0x248b1 && m_playback_conversion_pending)
		{
			if (opcode != 0xf7b8 || m_playback_conversion_shift_seen) ++m_playback_processing[3];
			m_playback_conversion_shifted = a; m_playback_conversion_shift_seen = true;
		}
		else if ((address == 0x23e1d || address == 0x23e23) && m_playback_conversion_active)
		{
			u16 const channel = address == 0x23e23;
			if (opcode != (channel ? 0x80e1 : 0x8099) || !m_playback_conversion_pending || !m_playback_conversion_shift_seen ||
				sp != m_playback_conversion[3] || channel != m_playback_conversion[1]) ++m_playback_processing[3];
			if (m_playback_processing[2] <= 8)
				logerror("mu4_conversion_value: call=%u frame=%u sample=%u channel=%u raw=%010llx shifted=%010llx result=%010llx argument=%u destination=%04x t=%.9f identity=Execute_entry observer_writes=0 retirement=unclaimed\n",
					m_playback_processing[0],m_playback_processing[2]-1,m_playback_conversion[0],channel,(unsigned long long)m_playback_conversion_raw,
					(unsigned long long)m_playback_conversion_shifted,(unsigned long long)a,m_playback_conversion[2],
					u16(m_playback_processing_destination+m_playback_conversion[0]+channel*1024),machine().time().as_double());
			u16 const word = u16(a);
			char const bytes[2]{char(word),char(word >> 8)};
			m_playback_decoded_capture.write(bytes,sizeof(bytes));
			for (char byte : bytes) m_playback_decoded_hash = (m_playback_decoded_hash ^ u8(byte)) * 16777619U;
			++m_playback_decoded_words;
			m_playback_conversion_pending = false; m_playback_conversion[1] = 1-channel;
			if (channel) ++m_playback_conversion[0];
		}
		else if (address == 0x23e2c && m_playback_conversion_active)
		{
			if (opcode != 0x10e6 || m_playback_conversion_pending || m_playback_conversion[0] != 1024) ++m_playback_processing[3];
			if (m_playback_processing[0] <= 8) logerror("mu4_conversion_block: call=%u frame=%u samples=%u destination=%04x bad=%u t=%.9f identity=Execute_entry observer_writes=0 audio_acceptance=0\n",
				m_playback_processing[0],m_playback_processing[2]-1,m_playback_conversion[0],m_playback_processing_destination,m_playback_processing[3],machine().time().as_double());
			logerror("mu4_decoder_block: call=%u frame=%u words=%u bad=%u t=%.9f identity=Execute_entry retirement=unclaimed observer_writes=0\n",
				m_playback_processing[0],m_playback_processing[2]-1,m_playback_decoded_words,m_playback_processing[3],machine().time().as_double());
			m_playback_conversion_active = false;
		}
	}
	void observe_playback_bits(offs_t address)
	{
		unsigned const boundary = address == 0x2510c ? 0 : 1;
		if (!boundary)
		{
			m_playback_bit_caller = m_playback_history_pc[(m_playback_history_next + 3) & 3];
			unsigned const source = m_playback_optional_active ? 3 : m_playback_bit_caller == 0x25354 ? 1 : m_playback_bit_caller == 0x25e2d ? 2 : 0;
			m_playback_bit_group = source;
			m_playback_bit_selected = m_playback_bit_source_samples[source] < (source == 3 ? 128 : 8);
			if (m_playback_bit_selected) ++m_playback_bit_source_samples[source];
			// Retain bounded anomalous entry contexts even after ordinary samples fill.
			// These are live-register observations, not retained operand ownership.
			auto const disable = machine().disable_side_effects();
			u16 const raw_a = m_cpu->state_int(tms320c54x_device::STATE_A);
			u16 const remaining = m_cpu->space(AS_DATA).read_word(0xa854);
			if ((raw_a > 16 || remaining > 32) && m_playback_bit_unusual_samples < 8)
			{
				++m_playback_bit_unusual_samples;
				m_playback_bit_selected = true;
				logerror("mu4_playback_bit_unusual: ordinal=%u raw_a=%04x remaining=%04x sp=%04x st1=%04x caller=%06x t=%.9f identity=Execute_entry context_only=1 observer_writes=0\n",
					m_playback_bit_unusual_samples, raw_a, remaining,
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), m_playback_bit_caller, machine().time().as_double());
			}
		}
		if (!m_playback_bit_selected) return;
		++m_playback_bit_samples[boundary];
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		u16 const index = data.read_word(0xb331), base = data.read_word(0xb332);
		u16 const pointer = base + index;
		std::string const next = pointer >= 0x80 ? util::string_format("%04x", data.read_word(pointer)) : "unobserved";
		logerror("mu4_playback_bit_context: boundary=%s ordinal=%u address=%06x a=%010llx remaining=%04x reservoir=%04x%04x index=%04x base=%04x next=%s caller=%06x group=%u t=%.9f identity=Execute_entry context_only=1 observer_writes=0\n",
			boundary ? "return" : "entry", m_playback_bit_samples[boundary], unsigned(address),
			static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
			data.read_word(0xa854), data.read_word(0xa858), data.read_word(0xa859), index, base, next.c_str(), m_playback_bit_caller, m_playback_bit_group, machine().time().as_double());
	}
	void report_playback_path(char const *prefix, char const *selection)
	{
		unsigned const first = (m_playback_path_next + 128 - m_playback_path_count) & 127;
		for (unsigned i = 0; i < m_playback_path_count; ++i)
			logerror("%s_entry: index=%u address=%06x opcode=%04x identity=Execute_entry observer_writes=0\n",
				prefix, i, m_playback_path_pc[(first + i) & 127], m_playback_path_word[(first + i) & 127]);
		logerror("%s_terminal: entries=%u t=%.9f selection=%s identity=Execute_entry observer_writes=0 audio_acceptance=0\n",
			prefix, m_playback_path_count, machine().time().as_double(), selection);
	}
	void report_playback_parent()
	{
		// A bounded observer can stop before the twenty-second acceptance timer.
		// Close its evidence without pretending that the full playback gate ran.
		finish_playback_capture();
		logerror("mu4_decoder_gate_terminal: events=%u bad=%u capped=%u first=9.640000000 last=9.860000000 t=%.9f identity=Execute_entry operands=context_only observer_writes=0\n",
			m_decoder_gate_events,m_decoder_gate_bad,unsigned(m_decoder_gate_events>512),machine().time().as_double());
		logerror("mu4_context_count_terminal: reads=%u capped=%u address=d7ac first=9.640000000 last=9.860000000 t=%.9f identity=Access_bus observer_writes=0\n",
			m_context_count_reads,unsigned(m_context_count_reads>256),machine().time().as_double());
		logerror("mu4_context_pointer_terminal: reads=%u capped=%u first=9.640000000 last=9.860000000 t=%.9f identity=Access_bus observer_writes=0\n",
			m_context_pointer_reads,unsigned(m_context_pointer_reads>256),machine().time().as_double());
		logerror("mu4_context_call_terminal: events=%u bad=%u capped=%u first=9.640000000 last=9.860000000 t=%.9f identity=Execute_entry operands=context_only observer_writes=0\n",
			m_context_call_events,m_context_call_bad,unsigned(m_context_call_events>1024),machine().time().as_double());
		logerror("mu4_exchange_terminal: reads=%u events=%u bad=%u capped=%u first=9.640000000 last=9.860000000 t=%.9f identity=Access_bus_and_Execute_entry observer_writes=0\n",
			m_exchange_status_reads,m_exchange_branch_events,m_exchange_branch_bad,
			unsigned(m_exchange_status_reads>256 || m_exchange_branch_events>512),machine().time().as_double());
		logerror("mu4_playback_method_terminal: entry=%u early=%u body=%u returned=%u result=%u publish=%u opcode_mismatch=%u selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0\n",
			m_playback_method_entries[0], m_playback_method_entries[1], m_playback_method_entries[2],
			m_playback_method_entries[3], m_playback_method_entries[4], m_playback_method_entries[5], m_playback_method_mismatch);
		unsigned unique = 0; u64 entries = 0;
		for (unsigned i = 0; i < PLAYBACK_RANGE_WORDS; ++i)
			if (m_playback_range_counts[i])
			{
				++unique; entries += m_playback_range_counts[i];
				logerror("mu4_playback_range_entry: address=%06x opcode=%04x entries=%u selection=first_verified_aa99_session identity=Execute_entry observer_writes=0\n",
					PLAYBACK_RANGE_FIRST + i, m_playback_range_opcodes[i], m_playback_range_counts[i]);
			}
		logerror("mu4_playback_range_terminal: first=0200a0 last=020cd1 unique=%u entries=%llu opcode_changes=%u selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0\n",
			unique, static_cast<unsigned long long>(entries), m_playback_range_changes);
		logerror("mu4_playback_parent_terminal: wrapper=%u internal=%u body=%u optional=%u returned=%u loop=%u dispatch=%u select=%u target=%u fallthrough=%u opcode_mismatch=%u first_session_complete=%u overlay_verified=%u illegal=%u t=%.9f selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0\n",
			m_playback_parent[0], m_playback_parent[1], m_playback_parent[2], m_playback_parent[3], m_playback_parent[4], m_playback_parent[5],
			m_playback_parent[6], m_playback_parent[7], m_playback_parent[8], m_playback_parent[9],
			m_playback_parent_mismatch, unsigned(m_playback_execute_entries[1] > 1), unsigned(m_playback_overlay_checked),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), machine().time().as_double());
	}
	void stream_tx(u16 value)
	{
		++m_stream_words;
		if (playback_profile() && m_playback_overlay_checked) observe_playback_audio(value, false);
		// TX reports the last launched bit before DIN samples it on the next rising edge.
		// Idle-line DIN words before this observable transmission are not stream evidence.
		if (m_phase == 30 && m_native_tx_words.size() < native_stream_target()) m_native_tx_words.push_back(value);
		if (m_phase == 30 && system_bios() == 20)
		{
			if (!m_source_count || value != m_source_words[m_source_head])
				fatalerror("MU4 transmitter/source mismatch word=%u source_reads=%u pending=%u actual=%04x expected=%04x pc=%06x",
					m_stream_words, m_source_reads, m_source_count, value, m_source_words[m_source_head], unsigned(m_cpu->pc()));
			m_source_head = (m_source_head + 1) % m_source_words.size();
			--m_source_count;
			if (m_continuous_tx_count == m_continuous_tx.size()) fatalerror("MU4 codec comparison queue overflow");
			m_continuous_tx[(m_continuous_tx_head + m_continuous_tx_count) % m_continuous_tx.size()] = value;
			++m_continuous_tx_count;
		}
		if ((m_phase >= 51 && m_phase <= 53) || m_phase == 66 || m_phase == 67 || (m_phase >= 70 && m_phase <= 73) || m_phase == 77 || m_phase == 78 || (m_phase >= 99 && m_phase <= 118)) m_external_words.push_back(value);
		if (m_phase == 30 && m_stream_words <= 8) logerror("mu4_native_stream: word=%04x\n", value);
		finish_stream_if_ready();
	}
	void dma_output_w(u16 value) { m_dma_sink[0] = value; if (m_phase >= 38 && m_phase <= 50) m_dma_output.push_back(value); }
	void dma_reg_w(u16 index, u16 value) { m_dma->write(1, index); m_dma->write(3, value); }
	u16 dma_reg_r(u16 index) { m_dma->write(1, index); return m_dma->read(3); }
	// VC5410A IFR bit 11 is XINT1; its priority rank 14 is not a bit index.
	void serial_tx_irq(int value)
	{
		if (value) ++m_serial_tx_irqs;
		if (!(m_dma->read(0) & 0xc0)) m_cpu->set_input_line(11, value);
	}
	void serial_tx_bit(int value) { if (m_phase >= 31 && m_phase <= 37) m_serial_tx_bits.push_back(value); }
	void mcbsp_reg_w(u16 index, u16 value) { m_mcbsp->control_w(0, index); m_mcbsp->control_w(1, value); }
	u16 mcbsp_status() { m_mcbsp->control_w(0, 1); return m_mcbsp->control_r(1); }
	void serial_tx(u16 value)
	{
		if (m_phase == 30 && system_bios() >= 2)
		{
			auto &codec = *subdevice<tlv320aic23_device>("codec");
			codec.control_word_w(value);
			if (playback_profile())
				logerror("mu4_codec_control: word=%04x ns=%llu power=%03x analog=%03x digital=%03x format=%03x rate=%03x active=%03x hp_left=%03x hp_right=%03x observer_writes=0 audio_acceptance=0\n",
					value,static_cast<unsigned long long>(machine().time().as_ticks(1'000'000'000)),codec.reg(6),codec.reg(4),codec.reg(5),codec.reg(7),codec.reg(8),codec.reg(9),codec.reg(2),codec.reg(3));
		}
		m_serial_tx_words.push_back(value);
		if (m_phase == 30 && m_serial_tx_words.size() <= 16) logerror("mu4_native_tx: word=%04x\n", value);
		// End the serial-setup fixture at its observed complete control stream,
		// before the separate streaming-profile acceptance.
		if (m_phase == 30 && system_bios() < 2 && m_serial_tx_words.size() == 6) m_check->adjust(attotime::zero);
	}
	required_shared_ptr<u16> m_program_ram;
	required_shared_ptr<u16> m_dma_sink;
	required_device<samsung_k9k1208u0a_device> m_nand;
	required_region_ptr<u16> m_cinit;
	required_region_ptr<u16> m_initdisk, m_disk_cinit, m_disk_helpers, m_disk_vectors;
	required_region_ptr<u8> m_segment;
	unsigned m_segment_cursor = 0, m_segment_waits = 0;
	unsigned m_file_source = 0, m_file_size = 0, m_file_cursor = 0, m_files_checked = 0;
	u16 m_file_marker = 0;
	std::vector<u16> m_verified_files;
	unsigned m_read_words = 0;
	std::stringstream m_saved_dma;
	unsigned m_dma_observed_reads = 0;
	unsigned m_dma_observed_writes = 0;
	u16 m_serial_index[2] = {}, m_serial_regs[2][32] = {}, m_serial_word = 0;
	bool m_serial_ready = false;
	unsigned m_serial_reads = 0;
	unsigned m_serial_empty_reads = 0;
	u8 m_direction = 0, m_gpio = 0;
	bool m_ready = true;
	unsigned m_bio_reads = 0, m_busy_reads = 0, m_data_reads = 0;
	attotime m_read_started, m_first_data;
	std::vector<u16> m_writes;
	emu_timer *m_check = nullptr;
	emu_timer *m_command = nullptr;
	emu_timer *m_command_clock = nullptr;
	emu_timer *m_command_receive_clock = nullptr;
	emu_timer *m_native_replay = nullptr;
	unsigned m_phase = 54;
	unsigned m_native_entry_reads = 0, m_native_far_reads = 0;
	unsigned m_native_mcbsp_trace = 0, m_native_mcbsp_polls = 0;
	unsigned m_native_adjacent_reads = 0;
	unsigned m_native_dma_writes = 0;
	u16 m_native_mcbsp_index = 0, m_native_mcbsp_status = 0;
	unsigned m_page = 0;
	static u16 pattern(unsigned index) { return u16(0x1234 + index * 37); }
	void program_map(address_map &map)
	{
		// This is a routine-test address space, not the unknown DA150 silicon map.
		map(0, 0x7fffff).ram().share("program_ram");
		map(0x0200, 0x0f32).rom().region("entry", 0);
		map(0x2080, 0x20ad).rom().region("trampolines", 0);
		map(0x20ae, 0x3679).rom().region("library", 0);
	}
	void data_map(address_map &map)
	{
		map(0, 0xffff).ram();
		map(0x0020, 0x0023).rw(m_mcbsp0, FUNC(tms320c54x_mcbsp_device::data_r), FUNC(tms320c54x_mcbsp_device::data_w));
		map(0x0031, 0x0031).r(FUNC(mu4_storage_test_state::serial_r));
		map(0x0033, 0x0033).rw(FUNC(mu4_storage_test_state::command_tx_r), FUNC(mu4_storage_test_state::command_tx_w));
		map(0x0034, 0x0035).rw(FUNC(mu4_storage_test_state::serial_config_r), FUNC(mu4_storage_test_state::serial_config_w));
		map(0x0038, 0x0039).rw(FUNC(mu4_storage_test_state::serial_config0_r), FUNC(mu4_storage_test_state::serial_config0_w));
		map(0x003c, 0x003d).rw(FUNC(mu4_storage_test_state::gpio_r), FUNC(mu4_storage_test_state::gpio_w));
		map(0x0040, 0x0043).rw(m_mcbsp, FUNC(tms320c54x_mcbsp_device::data_r), FUNC(tms320c54x_mcbsp_device::data_w));
		map(0x0048, 0x0049).rw(m_mcbsp, FUNC(tms320c54x_mcbsp_device::control_r), FUNC(tms320c54x_mcbsp_device::control_w));
		map(0x0054, 0x0057).r(m_dma, FUNC(tms320c54x_dma_device::read)).w(FUNC(mu4_storage_test_state::dma_w));
		map(0x6200, 0x6200).ram().w(FUNC(mu4_storage_test_state::dma_output_w)).share("dma_sink");
	}
	void dma_w(offs_t offset, u16 value)
	{
		if (m_phase == 30 && m_native_far_reads && m_native_dma_writes++ < 128)
			logerror("mu4_native_dma: offset=%u index=%04x value=%04x pc=%06x\n",
				unsigned(offset), m_dma->read(1), value, unsigned(m_cpu->pc()));
		m_dma->write(offset, value);
	}
	// Loader and legacy profiles use word ingress; the resident pins profile uses McBSP2.
	u16 serial_r()
	{
		if (m_phase == 30 && system_bios() >= 11)
		{
			if (!machine().side_effects_disabled())
			{
				if (m_command_rx_ready)
					++m_serial_reads;
				else if (m_serial_empty_reads++ < 16)
					// DRR retains its latched value without RRDY; startup may read it to flush.
					logerror("mu4_native_empty_drr: pc=%06x count=%u\n", unsigned(m_cpu->pc()), m_serial_empty_reads);
			}
			return m_mcbsp2->data_r(1);
		}
		if (!m_serial_ready) fatalerror("MU4 serial read without delivered word");
		m_serial_ready = false;
		m_serial_regs[0][0] &= ~u16(2);
		++m_serial_reads;
		return m_serial_word;
	}
	u16 serial_config_r(offs_t offset)
	{
		if (m_phase == 30 && system_bios() >= 11) return m_mcbsp2->control_r(offset);
		if (offset && m_phase == 30 && system_bios() >= 9 && m_serial_index[0] == 1) return m_mcbsp2->control_r(1);
		return offset ? m_serial_regs[0][m_serial_index[0]] : m_serial_index[0];
	}
	void serial_config_w(offs_t offset, u16 value)
	{
		if (offset) m_serial_regs[0][m_serial_index[0]] = value;
		else m_serial_index[0] = value & 31;
		if (m_phase == 30 && system_bios() >= 9) m_mcbsp2->control_w(offset, value);
	}
	u16 command_tx_r() { return m_command_tx_register; } // Bench register history; not a recovered DXR read contract.
	void command_tx_w(u16 value)
	{
		m_command_tx_register = value;
		if (m_phase != 30 || system_bios() < 9) return;
		m_mcbsp2->data_w(3, value);
		if (!m_command_clock->enabled() || m_command_clock->remaining() == attotime::never)
			m_command_clock->adjust(attotime::from_usec(5));
	}
	void command_output(u16 value)
	{
		if (m_command_wire_decoded.size() != m_command_tx_words.size() + 1 || m_command_wire_decoded.back() != value)
			fatalerror("MU4 serial pin decoder disagrees with McBSP transmit word");
		m_command_tx_words.push_back(value);
		if (media_command_profile())
		{
			if (m_command_tx_words.size() == 3)
			{
				logerror("mu4_native_recorder_request_ack: words=%04x,%04x,%04x\n",
					m_command_tx_words[0], m_command_tx_words[1], m_command_tx_words[2]);
				if (m_command_tx_words != std::vector<u16>({0x7f, 1, 0x55}))
					fatalerror("MU4 recorder request acknowledgement differs from native framing");
			}
			unsigned const begin = m_recorder_response_begin;
			if (m_command_tx_words.size() < begin + 2) return;
			if ((recorder_control_profile() || playback_stop_profile()) && m_command_tx_words[begin] == 0x7f)
			{
				if (m_command_tx_words.size() < begin + 3) return;
				if (m_command_tx_words[begin + 1] != 2 + m_recorder_control_acks || m_command_tx_words[begin + 2] != 0x55)
					fatalerror("MU4 recorder subsequent acknowledgement differs from request sequence");
				++m_recorder_control_acks;
				m_recorder_response_begin += 3;
				logerror("mu4_native_recorder_control_ack: token=%02x pin_output=1\n", m_command_tx_words[begin + 1]);
				return;
			}
			unsigned const end = begin + m_command_tx_words[begin + 1] + 6;
			if (end > begin + 64) fatalerror("MU4 recorder response exceeds observation bound");
			if (m_command_tx_words.size() != end) return;
			u8 checksum = 0;
			for (unsigned i = begin; i < end - 2; ++i) checksum ^= m_command_tx_words[i];
			if (m_command_tx_words[begin] != 0x1e || m_command_tx_words[begin + 2] != 0xaa ||
				m_command_tx_words[begin + 3] > 1 || m_command_tx_words[end - 2] != checksum || m_command_tx_words[end - 1] != 0x55)
			{
				for (unsigned i = begin; i < end; ++i)
					logerror("mu4_native_recorder_invalid_response: index=%u value=%04x\n", i - begin, m_command_tx_words[i]);
				fatalerror("MU4 recorder response framing/checksum mismatch");
			}
			if (m_command_ack_pending && m_command_ack_cursor != 3)
				fatalerror("MU4 recorder response overlaps an unfinished peer acknowledgement");
			if (recorder_save_profile() && m_command_tx_words[begin + 4] == 0x5e && end - begin == 11 &&
				m_command_tx_words[begin + 6] == 7)
			{
				m_recorder_metadata_result = m_command_tx_words[begin + 5] == 1 && m_command_tx_words[begin + 7] == 12 ? 1 : 2;
				logerror("mu4_native_recorder_metadata_reply: status=%02x parameter=%02x reason=%02x success=%u\n",
					m_command_tx_words[begin + 5], m_command_tx_words[begin + 6], m_command_tx_words[begin + 7],
					m_recorder_metadata_result == 1);
			}
			logerror("mu4_native_recorder_response: index=%u selector=%02x words=%u token=%02x class=%u\n",
				m_recorder_responses++, m_command_tx_words[begin + 4], end - begin, m_command_tx_words[end - 3], m_command_tx_words[begin + 3]);
			// This fixture's DSP TX sequence is 3; request 2 has its own ACK.
			if (playback_stop_profile() && m_command_tx_words[begin + 4] == 0x71 &&
				m_command_tx_words[end - 3] == 3 && m_command_tx_words[begin + 3] == 1)
				m_playback_status_replied = true;
			for (unsigned i = begin; i < end; ++i)
				logerror("mu4_native_recorder_response_word: index=%u value=%04x\n", i - begin, m_command_tx_words[i]);
			m_recorder_response_begin = end;
			m_command_ack_token = m_command_tx_words[end - 3];
			m_command_ack_cursor = 0;
			m_command_ack_pending = true;
			m_command->adjust(attotime::from_msec(1));
			return;
		}
		if (measurement_profile())
		{
			if (m_command_tx_words.size() != 12) return;
			u8 checksum = 0;
			for (unsigned i = 3; i < 10; ++i) checksum ^= m_command_tx_words[i];
			if (m_command_tx_words[3] != 0x1e || m_command_tx_words[4] != 3 || m_command_tx_words[5] != 0xaa ||
				m_command_tx_words[6] != 1 || m_command_tx_words[7] != 0x68 || m_command_tx_words[10] != checksum || m_command_tx_words[11] != 0x55)
				fatalerror("MU4 measurement response framing differs from original contract");
			m_command_ack_token = m_command_tx_words[9];
			m_command_ack_pending = true;
			m_command->adjust(attotime::from_msec(1));
			return;
		}
		if (cold_saved_profile() && m_command_tx_words.size() > 14)
		{
			unsigned const begin = m_cold_response_begin;
			if (m_command_tx_words.size() < begin + 2) return;
			unsigned const end = begin + m_command_tx_words[begin + 1] + 6;
			if (end != begin + 16) fatalerror("MU4 cold notification length differs from observed class-0 selector-02 contract");
			if (m_command_tx_words.size() < end) return;
			u8 checksum = 0;
			for (unsigned i = begin; i < end - 2; ++i) checksum ^= m_command_tx_words[i];
			if (m_command_tx_words[begin] != 0x1e || m_command_tx_words[begin + 2] != 0xaa ||
				m_command_tx_words[begin + 3] || m_command_tx_words[begin + 4] != 2 ||
				m_command_tx_words[end - 2] != checksum || m_command_tx_words[end - 1] != 0x55 ||
				m_command_tx_words[end - 3] != ((m_cold_notifications + 1) & 7))
				fatalerror("MU4 cold notification framing, checksum or next sequence differs");
			if (m_command_ack_pending && m_command_ack_cursor != 3) fatalerror("MU4 cold notification overlaps an unfinished acknowledgement");
			m_cold_response_begin = end; ++m_cold_notifications;
			m_command_ack_token = m_command_tx_words[end - 3];
			m_command_ack_cursor = 0; m_command_ack_pending = true;
			logerror("mu4_native_cold_notification: class=0 selector=02 words=16 token=%02x pin_decode=1\n", m_command_ack_token);
			m_command->adjust(attotime::from_msec(1));
			return;
		}
		if (system_bios() < 10 || m_command_tx_words.size() != 14) return;
		static const std::vector<u16> expected = {0x7f, 1, 0x55, 0x1e, 5, 0xaa, 1, 0x71, 1, 0, 0, 0x80, 0x40, 0x55};
		if (m_command_tx_words != expected) fatalerror("MU4 original status response differs from decoded contract");
		m_command_ack_token = m_command_tx_words[11];
		m_command_ack_pending = true;
		m_command->adjust(attotime::from_msec(1));
	}
	TIMER_CALLBACK_MEMBER(command_clock)
	{
		m_command_wire_clock = !m_command_wire_clock;
		if (m_command_wire_clock && !m_command_wire_bits && BIT(m_gpio, 4))
		{
			m_mcbsp2->tx_frame_w(1);
			m_command_wire_bits = 8;
		}
		m_mcbsp2->tx_clock_w(m_command_wire_clock);
		if (m_command_wire_clock && m_command_wire_bits)
		{
			m_mcbsp2->tx_frame_w(0);
			--m_command_wire_bits;
		}
		// Explicit 100 kbit/s bench source, not an asserted MU4 board clock.
		if (m_command_wire_bits || BIT(m_gpio, 4) || m_command_wire_clock)
			m_command_clock->adjust(attotime::from_usec(5));
	}
	u16 serial_config0_r(offs_t offset) { return m_mcbsp0->control_r(offset); }
	void serial_config0_w(offs_t offset, u16 value)
	{
		if (m_phase == 30 && m_native_far_reads && m_stream_config_writes++ < 64)
			logerror("mu4_native_stream_config: offset=%u index=%04x value=%04x pc=%06x\n",
				unsigned(offset), m_mcbsp0->control_r(0), value, unsigned(m_cpu->pc()));
		m_mcbsp0->control_w(offset, value);
	}
	void deliver_serial(u8 value)
	{
		if (m_serial_ready || !(m_serial_regs[0][0] & 1)) fatalerror("MU4 serial fixture not receive-ready");
		m_serial_word = value;
		m_serial_ready = true;
		m_serial_regs[0][0] |= 2;
		m_cpu->set_input_line(6, ASSERT_LINE);
		m_cpu->set_input_line(6, CLEAR_LINE);
	}
	void command_receive(u8 value)
	{
		if (system_bios() < 11) { deliver_serial(value); return; }
		m_command_rx_byte = value;
		m_command_rx_busy = true;
		m_command_rx_phase = 0;
		m_command_rx_clock = true;
		m_mcbsp2->rx_data_w(0); m_mcbsp2->rx_frame_w(1); m_mcbsp2->rx_clock_w(1);
		m_command_receive_clock->adjust(attotime::from_usec(5));
	}
	TIMER_CALLBACK_MEMBER(command_receive_clock)
	{
		m_command_rx_clock = !m_command_rx_clock;
		if (!m_command_rx_clock && m_command_rx_phase >= 1 && m_command_rx_phase <= 8)
			m_mcbsp2->rx_data_w(BIT(m_command_rx_byte, 8 - m_command_rx_phase));
		if (m_command_rx_phase) m_mcbsp2->rx_frame_w(0);
		m_mcbsp2->rx_clock_w(m_command_rx_clock);
		if (!m_command_rx_clock) ++m_command_rx_phase;
		if ((system_bios() == 12 || system_bios() == 19) && !m_native_replay_leg && m_native_command_cursor == 1 && m_command_rx_phase == 4 && !m_command_rx_clock)
			// Let synchronized input events settle without advancing to the next bit.
			m_native_replay->adjust(attotime::from_nsec(1));
		if ((system_bios() == 14 || system_bios() == 18) && !m_native_reset_leg && m_native_command_cursor == 1 && m_command_rx_phase == 4 && !m_command_rx_clock)
			m_native_replay->adjust(attotime::from_nsec(1));
		// One delay cycle, eight data cycles, then RBR-to-DRR publication.
		if (m_command_rx_phase == 10 && m_command_rx_clock) m_command_rx_busy = false;
		else m_command_receive_clock->adjust(attotime::from_usec(5));
	}
	TIMER_CALLBACK_MEMBER(native_replay_checkpoint)
	{
		if (system_bios() == 14 || system_bios() == 18)
		{
			if (m_phase != 30 || m_native_reset_leg || !m_command_rx_busy || m_command_rx_phase != 4 || m_command_rx_clock ||
				m_command_rx_irqs || !m_command_tx_words.empty() || !machine().scheduler().can_save())
				fatalerror("MU4 reset fixture is not at the settled first-byte boundary");
			m_native_replay->adjust(attotime::never);
			m_native_reset_leg = 1;
			// Retain NAND. The full-startup profile replays the original upload
			// and reset-vector software; the legacy diagnostic uses routine ABIs.
			m_files_checked = 0;
			m_verified_files.clear();
			m_phase = system_bios() == 18 ? 74 : 19;
			logerror("mu4_native_reset_request: mid_rx_byte=1 data_bits=3 rx_irqs=0 tx_words=0\n");
			machine().schedule_soft_reset();
			return;
		}
		if (m_native_replay_leg == 2)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 native restore has pending synchronized inputs");
			// Mark this callback modified before restoration. The restored timer
			// state then survives callback cleanup without rearming the DUT timer.
			m_native_replay->adjust(attotime::never);
			m_native_checkpoint.clear(); m_native_checkpoint.seekg(0);
			if (machine().save().read_stream(m_native_checkpoint) != STATERR_NONE)
				fatalerror("MU4 native checkpoint restore failed");
			m_native_replay_leg = 3;
			logerror("mu4_native_replay_restore: mid_byte=1 original_end_timer=1\n");
			return;
		}
		if (m_phase != 30 || m_native_replay_leg || !m_command_rx_busy || m_command_rx_phase != 4 || m_command_rx_clock || !machine().scheduler().can_save())
			fatalerror("MU4 native replay checkpoint is not a settled mid-byte boundary");
		m_native_replay->adjust(attotime::never);
		m_native_checkpoint.str(""); m_native_checkpoint.clear();
		if (machine().save().write_stream(m_native_checkpoint) != STATERR_NONE)
			fatalerror("MU4 native mid-byte checkpoint failed");
		m_native_replay_leg = 1;
		logerror("mu4_native_replay_checkpoint: rx_phase=4 data_bits=3 rx_irqs=%u request_cursor=%u\n", m_command_rx_irqs, m_native_command_cursor);
	}
	void observe_native_tone(offs_t address)
	{
		if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->pc() != address + 1) return;
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		if (address == 0x2876a)
		{
			m_tone_buffer = data.read_word(0xbb86) ? 0xbaf9 : 0xba79;
			for (unsigned channel = 0; channel < 2; ++channel)
			{
				u16 phase = data.read_word(0xbb79 + channel);
				s32 const step = s16(data.read_word(0xbb7b + channel));
				s32 const amplitude = s16(data.read_word(0xbb7d + channel));
				for (unsigned i = 0; i < 64; ++i)
				{
					// Independent phase folding and signed fixed-point scaling, not CPU results.
					s32 const sum = s16(phase) + step;
					if (sum > 32767 || sum < -32768) ++m_tone_phase_wraps;
					phase = u16(sum);
					s32 const distance = sum - 0x4000;
					s32 const folded = s16(u16(distance < 0 ? -distance : distance));
					s32 const index = folded >= 0 ? folded / 128 : -((-folded + 127) / 128);
					u16 const table = data.read_word(0x17fd + (index < 0 ? -index : index));
					s64 const product = s64(s16(table)) * amplitude * 2;
					if (product < -0x80000000LL || product > 0x7fffffffLL)
						fatalerror("MU4 tone fixture exceeds independently modeled nonsaturating scale range");
					s64 const scaled = product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
					m_tone_expected[channel * 64 + i] = u16(scaled);
				}
				m_tone_phase[channel] = phase;
			}
			return;
		}
		for (unsigned i = 0; i < m_tone_expected.size(); ++i)
			if (data.read_word(m_tone_buffer + i) != m_tone_expected[i])
				fatalerror("MU4 native tone mismatch block=%u sample=%u actual=%04x expected=%04x", m_tone_blocks,
					i, data.read_word(m_tone_buffer + i), m_tone_expected[i]);
		for (unsigned channel = 0; channel < 2; ++channel)
			if (data.read_word(0xbb79 + channel) != m_tone_phase[channel])
				fatalerror("MU4 native tone phase disagrees with independent block model");
		if (system_bios() == 20)
		{
			unsigned const bank = m_tone_buffer == 0xbaf9;
			if (m_verified_tone_valid[bank] && m_verified_tone_reads[bank] != 128)
				fatalerror("MU4 tone bank overwritten before complete verified DMA consumption");
			std::copy(m_tone_expected.begin(), m_tone_expected.end(), m_verified_tone.begin() + bank * 128);
			m_verified_tone_valid[bank] = true;
			m_verified_tone_reads[bank] = 0;
			std::fill(m_tone_cleared.begin() + bank * 128, m_tone_cleared.begin() + (bank + 1) * 128, false);
		}
		++m_tone_blocks;
	}
	bool finish_native_replay()
	{
		if (system_bios() != 12 && system_bios() != 19) return false;
		if (!machine().scheduler().can_save() || !m_native_replay_leg)
			fatalerror("MU4 native replay has no restorable checkpoint");
		if (m_native_replay_leg == 1)
		{
			m_native_reference.str(""); m_native_reference.clear();
			if (machine().save().write_stream(m_native_reference) != STATERR_NONE)
				fatalerror("MU4 native first-leg snapshot failed");
			m_native_replay_leg = 2;
			m_native_replay->adjust(attotime::from_nsec(1));
			return true;
		}
		std::stringstream replay;
		if (m_native_replay_leg != 3) fatalerror("MU4 native replay comparison precedes restoration");
		if (machine().save().write_stream(replay) != STATERR_NONE)
			fatalerror("MU4 native second-leg snapshot failed");
		auto const expected = m_native_reference.str(), actual = replay.str();
		if (expected.size() != actual.size()) fatalerror("MU4 native replay snapshot size changed");
		// MAME's uncompressed stream has a 32-byte version/system/signature header.
		if (actual.size() < 32 || !std::equal(expected.begin(), expected.begin() + 32, actual.begin()))
			fatalerror("MU4 native replay snapshot header changed");
		size_t position = 32;
		unsigned differences = 0, compared = 0, frontend = 0;
		for (int index = 0; index < machine().save().registration_count(); ++index)
		{
			void *base; u32 size, count, blocks, stride;
			char const *name = machine().save().indexed_item(index, base, size, count, blocks, stride);
			size_t const length = size_t(size) * count * blocks;
			if (position + length > actual.size()) fatalerror("MU4 native replay registry extent exceeds snapshot");
			// lua_engine::on_machine_postload resets its frontend resume timer.
			// No Lua script runs here; that timer is not emulated hardware state.
			if (std::string_view(name).starts_with("timer/lua_engine::resume/")) ++frontend;
			else
			{
				++compared;
				auto const mismatch = std::mismatch(expected.begin() + position, expected.begin() + position + length, actual.begin() + position);
				if (mismatch.first != expected.begin() + position + length)
				{
					++differences;
					logerror("mu4_native_replay_mismatch: item=%s item_byte=%u expected=%02x actual=%02x\n", name,
						unsigned(mismatch.first - expected.begin() - position), u8(*mismatch.first), u8(*mismatch.second));
				}
			}
			position += length;
		}
		if (position != actual.size() || differences) fatalerror("MU4 native replay differs in %u registered emulation state items", differences);
		logerror("mu4_native_replay: PASS mid_rx_byte=1 complete_legs=2 emulation_state_equal=1 compared_items=%u frontend_timer_items=%u rx_words=11 tx_words=14 processing=0\n", compared, frontend);
		return false;
	}
	TIMER_CALLBACK_MEMBER(command_input)
	{
		if (m_phase != 30 || system_bios() < 8) return;
		// Original parser framing and selector 0x49: read-only software status query.
		static constexpr u8 status[] = {0x1e, 2, 0xaa, 1, 0x49, 1, 0xff, 0x55};
		// 03:b3b0/02:9d8c/02:9d1d: no shuffle, first track, reply cookie zero.
		static constexpr u8 playback[] = {0x1e, 6, 0xaa, 1, 0x30, 2, 0, 1, 0, 1, 0x81, 0x55};
		// Original AA99 selector30/parameter04 -> controller input03.
		// Status49 separates matching selectors at the duplicate-command guard.
		static constexpr u8 playback_control[] = {0x1e, 3, 0xaa, 1, 0x30, 4, 3, 0x81, 0x55};
		static constexpr u8 measurement[] = {0x1e, 2, 0xaa, 1, 0x40, 1, 0xf6, 0x55};
		// Payload class, selector and parameter are distinct from sequence token 1.
		static constexpr u8 recorder[] = {0x1e, 3, 0xaa, 1, 0x36, 5, 1, 0x84, 0x55};
		// AA88 selector 0x36/parameter 4 selects the original control transition.
		// Token 2 distinguishes it from the resident recorder-start request.
		static constexpr u8 recorder_control[] = {0x1e, 3, 0xaa, 1, 0x36, 4, 2, 0x86, 0x55};
		// AA88 0x49 constructs a status reply without recorder-state writes.
		static constexpr u8 recorder_poll[] = {0x1e, 2, 0xaa, 1, 0x49, 2, 0xfc, 0x55};
		static constexpr u8 recorder_polled_control[] = {0x1e, 3, 0xaa, 1, 0x36, 4, 3, 0x87, 0x55};
		static constexpr u8 recorder_resident_poll[] = {0x1e, 2, 0xaa, 1, 0x49, 4, 0xfa, 0x55};
		// Preserve the empty-field rejection control. The positive request
		// supplies filename, TPE1 and TIT2 strings; the ID3 builder requires
		// both metadata lengths to be positive (02:853d..8543).
		static constexpr u8 recorder_metadata_empty[] = {0x1e, 10, 0xaa, 1, 0x36, 7, 4, 'T', 'E', 'S', 'T', 0, 0, 5, 0x99, 0x55};
		static constexpr u8 recorder_metadata_full[] = {0x1e, 18, 0xaa, 1, 0x36, 7, 4, 'T', 'E', 'S', 'T', 4, 'T', 'E', 'S', 'T', 4, 'M', 'A', 'M', 'E', 5, 0x93, 0x55};
		u8 const *const recorder_metadata = full_metadata_profile() ? recorder_metadata_full : recorder_metadata_empty;
		unsigned const metadata_size = full_metadata_profile() ? std::size(recorder_metadata_full) : std::size(recorder_metadata_empty);
		constexpr auto valid_packet = [](auto const &bytes)
		{
			u8 checksum = 0;
			for (unsigned i = 0; i < std::size(bytes) - 2; ++i) checksum ^= bytes[i];
			return bytes[0] == 0x1e && bytes[2] == 0xaa && bytes[1] + 6 == std::size(bytes) &&
				bytes[std::size(bytes) - 2] == checksum && bytes[std::size(bytes) - 1] == 0x55;
		};
		static_assert(valid_packet(status) && valid_packet(playback) && valid_packet(playback_control) && valid_packet(measurement) && valid_packet(recorder) && valid_packet(recorder_control) &&
			valid_packet(recorder_poll) && valid_packet(recorder_polled_control) &&
			valid_packet(recorder_resident_poll) && valid_packet(recorder_metadata_empty) && valid_packet(recorder_metadata_full));
		if (metadata_size != recorder_metadata_size()) fatalerror("MU4 metadata fixture length differs from acceptance contract");
		u8 const *packet = playback_profile() ? playback : recorder_profile() ? recorder : measurement_profile() ? measurement : status;
		unsigned const first_size = playback_profile() ? std::size(playback) : recorder_profile() ? std::size(recorder) : std::size(status);
		unsigned const packet_size = first_size + (recorder_control_profile() ? std::size(recorder_control) : 0) +
			(recorder_poll_profile() ? std::size(recorder_poll) : 0) +
			(recorder_save_profile() ? std::size(recorder_resident_poll) + metadata_size : 0) +
			(playback_stop_profile() ? std::size(recorder_poll) + std::size(playback_control) +
				(m_manual_status_requested ? std::size(recorder_resident_poll) : 0) : 0);
		unsigned packet_start = 0;
		bool const ack = m_native_command_cursor >= first_size && m_command_ack_pending && m_command_ack_cursor != 3;
		if (playback_stop_profile() && m_native_command_cursor >= first_size && !ack)
		{
			if (m_native_command_cursor == packet_size) return;
			// Peer scheduling waits for observed native progress, not a calibrated
			// delay or a firmware-state write. This is an explicit user control.
			if (m_recorder_responses < 2 || !m_playback_processing[2])
			{ m_command->adjust(attotime::from_msec(1)); return; }
			packet_start = first_size;
			packet = recorder_poll;
			if (m_native_command_cursor >= first_size + std::size(recorder_poll))
			{
				if (!m_playback_status_replied)
				{ m_command->adjust(attotime::from_msec(1)); return; }
				packet_start += std::size(recorder_poll);
				packet = playback_control;
				if (m_native_command_cursor >= packet_start + std::size(playback_control))
				{
					packet_start += std::size(playback_control);
					packet = recorder_resident_poll;
				}
			}
		}
		else if (recorder_control_profile() && m_native_command_cursor >= first_size && !ack)
		{
			if (m_native_command_cursor == packet_size) return;
			if (m_recorder_responses < 2 || machine().time() < attotime::from_seconds(10))
			{
				m_command->adjust(attotime::from_msec(1));
				return;
			}
			packet_start = first_size;
			packet = recorder_control;
			if (recorder_poll_profile())
			{
				if (m_native_command_cursor < first_size + std::size(recorder_poll)) packet = recorder_poll;
				else
				{
					if (m_recorder_responses < 3) { m_command->adjust(attotime::from_msec(1)); return; }
					packet_start += std::size(recorder_poll);
					packet = recorder_polled_control;
					if (recorder_save_profile() && m_native_command_cursor >= packet_start + std::size(recorder_polled_control))
					{
						if (m_native_bootstrap_resident_entries < 2 || machine().time() < attotime::from_seconds(15))
						{ m_command->adjust(attotime::from_msec(1)); return; }
						packet_start += std::size(recorder_polled_control);
						packet = recorder_resident_poll;
						if (m_native_command_cursor >= packet_start + std::size(recorder_resident_poll))
						{
							if (m_recorder_responses < 4) { m_command->adjust(attotime::from_msec(1)); return; }
							packet_start += std::size(recorder_resident_poll);
							packet = recorder_metadata;
						}
					}
				}
			}
		}
		else if (m_native_command_cursor == packet_size && !ack) return;
		// Replay starts after the separately checked streaming prefix, so both
		// legs share the same scheduled worker-window endpoint.
		if (system_bios() >= 12 && !original_bootstrap_profile() && !m_native_worker_window_started) { m_command->adjust(attotime::from_msec(1)); return; }
		if (m_native_command_paths[5] && !m_serial_ready && !m_command_rx_busy && !m_command_rx_ready && (m_serial_regs[0][0] & 1))
		{
			if (ack)
			{
				u8 const reply[] = {0x7f, m_command_ack_token, 0x55};
				command_receive(reply[m_command_ack_cursor++]);
				logerror("mu4_native_command_ack_input: cursor=%u total=3\n", m_command_ack_cursor);
			}
			else
			{
				unsigned const index = m_native_command_cursor - packet_start;
				if (!index && playback_stop_profile())
					logerror("mu4_playback_control_input: phase=%s decoded_calls=%u replies=%u t=%.9f firmware_state_forcing=0 audio_acceptance=0\n",
						packet == playback_control ? "control30_04" : packet == recorder_poll ? "status49" :
						packet == recorder_resident_poll ? "resident_status49" : "play30_02",
						m_playback_processing[2], m_recorder_responses, machine().time().as_double());
				if (!index && recorder_save_profile() && packet == recorder_metadata)
					serialize_recorder_media("mu4_recorder_before_metadata.nand");
				command_receive(packet[index]);
				++m_native_command_cursor;
				logerror("mu4_native_command_input: cursor=%u total=%u\n", m_native_command_cursor, packet_size);
			}
		}
		else if (++m_native_command_waits > 20000) fatalerror("MU4 native serial command ingress timeout");
		m_command->adjust(attotime::from_msec(1));
	}
	void io_map(address_map &map)
	{
		map(0x4000, 0x7fff).rw(FUNC(mu4_storage_test_state::nand_r), FUNC(mu4_storage_test_state::nand_w));
	}
	u16 gpio_r(offs_t offset) { return offset ? m_gpio : m_direction; }
	void gpio_w(offs_t offset, u16 value)
	{
		if (offset) m_gpio = value; else m_direction = value;
		if (BIT(m_direction, 2)) m_nand->ce_w(BIT(m_gpio, 2));
	}
	void ready_w(int state)
	{
		m_ready = bool(state);
		if (!state) m_read_started = machine().time();
	}
	int bio_r()
	{
		++m_bio_reads;
		if (!m_ready) ++m_busy_reads;
		return m_ready;
	}
	void nand_w(offs_t offset, u16 value)
	{
		// ADD_H is held enabled by this fixture; U202's lower 14 address bits alias.
		if ((m_direction & 7) != 7) fatalerror("MU4 storage: strobe before GPIO outputs enabled");
		m_writes.push_back((u16(m_gpio & 7) << 8) | (value & 0xff));
		if (staged_original_profile() && m_writes.size() <= 16)
			logerror("mu4_staged_nand_strobe: offset=%04x byte=%02x hd012=%x direction=%x ready=%u observation_only=1\n",
				unsigned(offset), value & 0xff, m_gpio & 7, m_direction & 7, m_ready);
		if (BIT(m_gpio, 2)) return;
		if (recorder_profile() && m_recorder_loader_calls)
		{
			bool const control = BIT(m_gpio, 0) || BIT(m_gpio, 1);
			if (BIT(m_gpio, 0)) m_recorder_last_nand_command = value;
			unsigned &count = control ? m_recorder_nand_controls : m_recorder_nand_data;
			++count;
			if ((!control && count <= 16) || (BIT(m_gpio, 0) && value != 0 && value != 0x50 && value != 0x30 && count <= 4096))
				logerror("mu4_native_recorder_nand_write: kind=%s value=%02x last_command=%02x pc=%06x time_us=%lld\n",
					BIT(m_gpio, 0) ? "command" : BIT(m_gpio, 1) ? "address" : "data", value & 0xff,
					m_recorder_last_nand_command, unsigned(m_cpu->pc()), static_cast<long long>(machine().time().as_ticks(1000000)));
		}
		if (BIT(m_gpio, 0)) m_nand->command_w(value);
		else if (BIT(m_gpio, 1)) m_nand->address_w(value);
		else m_nand->data_w(value);
	}
	u16 nand_r()
	{
		if (!m_ready && !machine().side_effects_disabled())
			fatalerror("MU4 storage fixture read data while NAND busy");
		if (!m_data_reads) m_first_data = machine().time();
		++m_data_reads;
		u16 const value = m_nand->data_r();
		// Capture one bounded page from the existing read, never a second read.
		if (staged_original_profile() && m_data_reads <= 512)
			logerror("mu4_staged_nand_read: index=%u byte=%02x hd012=%x direction=%x ready=%u observation_only=1\n",
				m_data_reads, value & 0xff, m_gpio & 7, m_direction & 7, m_ready);
		return value;
	}
	bool recorder_ac_profile() const { return system_bios() == 30; }
	bool staged_recorder_profile() const { return system_bios() == 26 || system_bios() == 27 || recorder_ac_profile(); }
	bool cold_saved_profile() const { return system_bios() == 28; }
	bool playback_stop_profile() const { return system_bios() == 31; }
	bool playback_profile() const { return system_bios() == 29 || playback_stop_profile(); }
	bool cold_catalog_profile() const { return cold_saved_profile() || playback_profile(); }
	bool staged_original_profile() const { return staged_recorder_profile() || cold_catalog_profile(); }
	bool full_metadata_profile() const { return system_bios() == 27 || recorder_ac_profile(); }
	unsigned recorder_metadata_size() const { return full_metadata_profile() ? 24 : 16; }
	bool original_bootstrap_profile() const { return system_bios() >= 16 && system_bios() <= 31; }
	bool recorder_profile() const { return system_bios() == 21 || system_bios() == 22 || recorder_control_profile(); }
	bool media_command_profile() const { return recorder_profile() || playback_profile(); }
	bool shared_daram_profile() const { return system_bios() == 22 || recorder_control_profile() || playback_profile(); }
	bool recorder_control_profile() const { return system_bios() == 23 || recorder_poll_profile(); }
	bool recorder_poll_profile() const { return system_bios() == 24 || recorder_save_profile(); }
	bool recorder_save_profile() const { return system_bios() == 25 || staged_recorder_profile(); }
	u16 shared_daram_r(offs_t offset)
	{
		// Forward once through data space; tag the program origin for observation.
		bool const previous = m_ac_daram_read_alias;
		m_ac_daram_read_alias = true;
		u16 const value = m_cpu->space(AS_DATA).read_word(0x2000 + offset);
		m_ac_daram_read_alias = previous;
		return value;
	}
	void shared_daram_w(offs_t offset, u16 value)
	{
		bool const previous = m_ac_daram_write_alias;
		m_ac_daram_write_alias = true;
		m_cpu->space(AS_DATA).write_word(0x2000 + offset, value);
		m_ac_daram_write_alias = previous;
	}
	void observe_bootstrap_resident_memory()
	{
		auto const disable = machine().disable_side_effects();
		auto word = [this](unsigned offset) -> u16
		{
			if (offset + 1 >= m_segment.length()) fatalerror("MU4 resident source bounds");
			return (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
		};
		unsigned segment = 0;
		while (word(segment) != 0xaa22) segment += 10 + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
		unsigned cursor = segment + 6, total = 0, data_words = 0, register_words = 0;
		unsigned const end = cursor + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
		std::map<offs_t, u16> expected;
		std::map<offs_t, u16> expected_data;
		while (word(cursor))
		{
			unsigned const count = word(cursor);
			u32 const destination = (u32(word(cursor + 2)) << 16) | word(cursor + 4);
			cursor += 6;
			if (cursor + count * 2 > end) fatalerror("MU4 resident section bounds");
			if (destination > 0xffff)
			{
				// Original loader's common-window canonicalization; this checks
				// the configured mapping, not physical DA150 RAM-bank identity.
				u32 const address = (destination & 0x8000) ? destination : destination & 0xffff;
				for (unsigned i = 0; i < count; ++i)
				{
					offs_t const target = (address & 0x7f0000) | u16(address + i);
					u16 const value = word(cursor + i * 2);
					expected[target] = value;
					// Preserve record order when a later program write aliases a
					// previously initialized data word in the configured common RAM.
					if (shared_daram_profile() && target >= 0x2000 && target < 0x8000 && expected_data.count(target))
						expected_data[target] = value;
				}
			}
			else
			{
				data_words += count;
				for (unsigned i = 0; i < count; ++i)
				{
					offs_t const target = u16(destination + i);
					if (target < 0x80) { ++register_words; continue; }
					expected_data[target] = word(cursor + i * 2);
				}
			}
			cursor += count * 2; total += count;
		}
		if (cursor + 2 != end || total != 51921) fatalerror("MU4 resident source coverage mismatch");
		unsigned mismatches = 0;
		for (auto const &[address, value] : expected)
		{
			u16 const actual = m_cpu->space(AS_PROGRAM).read_word(address);
			if (actual != value)
			{
				if (mismatches < 8) logerror("mu4_original_resident_program_mismatch: address=%06x actual=%04x expected=%04x\n", unsigned(address), actual, value);
				++mismatches;
			}
		}
		m_native_bootstrap_memory_checked = true;
		logerror("mu4_original_resident_program: %s checked_words=%u mismatch_words=%u excluded_data_words=%u source_words=%u before_resident_execution=1 physical_mapping=unvalidated full_boot=0\n",
			mismatches ? "MISMATCH" : "PASS", unsigned(expected.size()), mismatches, data_words, total);
		unsigned data_mismatches = 0;
		for (auto const &[address, value] : expected_data)
		{
			u16 const actual = m_cpu->space(AS_DATA).read_word(address);
			if (actual != value)
			{
				if (data_mismatches < 8) logerror("mu4_original_resident_data_mismatch: address=%04x actual=%04x expected=%04x\n", unsigned(address), actual, value);
				++data_mismatches;
			}
		}
		logerror("mu4_original_resident_data: %s checked_words=%u mismatch_words=%u excluded_register_words=%u source_data_words=%u before_resident_execution=1 physical_mapping=unvalidated full_boot=0\n",
			data_mismatches ? "MISMATCH" : "PASS", unsigned(expected_data.size()), data_mismatches, register_words, data_words);
	}
	void verify_media_overlay(bool playback = false)
	{
		auto const disable = machine().disable_side_effects();
		auto word = [this](unsigned offset) -> u16
		{
			if (offset + 1 >= m_segment.length()) fatalerror("MU4 recorder source bounds");
			return (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
		};
		unsigned segment = 0;
		u16 const marker = playback ? 0xaa99 : 0xaa88;
		while (word(segment) != marker)
			segment += 10 + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
		unsigned cursor = segment + 6, records = 0, total = 0, registers = 0;
		unsigned const end = cursor + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
		std::map<std::pair<bool, offs_t>, u16> expected;
		while (word(cursor))
		{
			unsigned const count = word(cursor);
			u32 const destination = (u32(word(cursor + 2)) << 16) | word(cursor + 4);
			cursor += 6;
			if (cursor + count * 2 > end) fatalerror("MU4 recorder section bounds");
			bool const program = destination > 0xffff;
			// Original loader 2f00..2f0e / 2f60..2f70: the common window aliases page zero.
			u32 const address = (destination & 0x8000) ? destination : destination & 0xffff;
			for (unsigned i = 0; i < count; ++i)
			{
				offs_t const target = (program ? address & 0x7f0000 : 0) | u16(address + i);
				// Peripheral registers may change while later DMA records are loading.
				if (!program && target < 0x80) { ++registers; continue; }
				bool const shared = shared_daram_profile() && program && target >= 0x2000 && target < 0x8000;
				expected[{program && !shared, target}] = word(cursor + i * 2);
			}
			cursor += count * 2;
			total += count;
			++records;
		}
		if (cursor + 2 != end || total != (playback ? 49843 : 73766)) fatalerror("MU4 media loader coverage mismatch marker=%04x words=%u", marker, total);
		for (auto const &[location, value] : expected)
		{
			auto const &[program, address] = location;
			u16 const actual = m_cpu->space(program ? AS_PROGRAM : AS_DATA).read_word(address);
			if (actual != value)
				fatalerror("MU4 recorder DMA destination mismatch space=%s address=%06x actual=%04x expected=%04x",
					program ? "program" : "data", unsigned(address), actual, value);
		}
		if (playback) m_playback_overlay_checked = true;
		else m_recorder_overlay_checked = true;
		logerror("%s: PASS marker=%04x records=%u words=%u checked_ram_words=%u excluded_register_words=%u before_execution=1\n",
			playback ? "mu4_native_playback_overlay" : "mu4_native_recorder_overlay", marker, records, total, unsigned(expected.size()), registers);
	}
	bool measurement_profile() const { return system_bios() == 13 || system_bios() == 20; }
	void observe_cold_title(u16 opcode)
	{
		// The packing loop ends at 03:95ac. C682 is subsequently reused
		// by later storage work, so its endpoint value is not an invariant.
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		std::array<u16, 5> const title{'M', 'A', 'M', 'E', 0};
		std::array<u16, 3> const packed{0x4d41, 0x4d45, 0};
		bool valid = opcode == 0x7600;
		for (unsigned i = 0; i < title.size(); ++i) valid &= data.read_word(0xb6c9 + i) == title[i];
		for (unsigned i = 0; i < packed.size(); ++i) valid &= data.read_word(0xc682 + i) == packed[i];
		++m_cold_title_publications; m_cold_title_verified = valid;
		logerror("mu4_native_cold_title: publications=%u verified=%u packed=%04x,%04x,%04x identity=execute_entry\n",
			m_cold_title_publications, unsigned(valid), data.read_word(0xc682), data.read_word(0xc683), data.read_word(0xc684));
	}
	void verify_native_status()
	{
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		unsigned const rx_words = 11 + (cold_saved_profile() ? 3 * m_cold_notifications : 0);
		unsigned const tx_words = 14 + (cold_saved_profile() ? 16 * m_cold_notifications : 0);
		if (m_command_ack_cursor != 3 || m_command_tx_words.size() != tx_words || m_command_wire_bit_count ||
			data.read_word(0x1657) != rx_words % 80 || data.read_word(0x1658) != rx_words % 80 || data.read_word(0x1659) != tx_words % 80 ||
			data.read_word(0x165a) != tx_words % 80 || data.read_word(0x165e) || data.read_word(0x165d) || data.read_word(0xbb80))
			fatalerror("MU4 framed status transaction did not settle ack=%u tx_words=%zu bits=%u rx=%04x/%04x tx=%04x/%04x queued=%04x pending=%04x mode=%04x",
				m_command_ack_cursor, m_command_tx_words.size(), m_command_wire_bit_count,
				data.read_word(0x1657), data.read_word(0x1658), data.read_word(0x1659), data.read_word(0x165a),
				data.read_word(0x165e), data.read_word(0x165d), data.read_word(0xbb80));
		logerror("mu4_native_status_transaction: PASS rx_words=%u tx_words=%u pin_decode=1 ack=1 retries=0 queue_empty=1 mode=0 full_boot=0\n", rx_words, tx_words);
		if (system_bios() >= 11)
		{
			if (m_command_rx_irqs != rx_words || m_command_rx_busy || m_command_rx_ready)
				fatalerror("MU4 pin receive did not drain %u device-owned RRDY interrupts", rx_words);
			logerror("mu4_native_receive_pins: PASS bytes=%u rx_irqs=%u frame=1 data=1 clock=1 drr_drained=1\n", rx_words, m_command_rx_irqs);
		}
	}
	u16 expected_measurement_result() const
	{
		// AA22 03:b021..b037: right-channel low energy overrides the left result.
		return m_measurement_expected[1] < 10'000'000 ? 0x20 : m_measurement_expected[0] < 10'000'000 ? 0x1f : 1;
	}
	void verify_native_measurement()
	{
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		logerror("mu4_native_measurement_observe: mode=%04x complete=%04x blocks=%04x,%04x left=%04x,%04x right=%04x,%04x tx_words=%u full_boot=0\n",
			data.read_word(0xbb80), data.read_word(0xb64e), data.read_word(0xb650), data.read_word(0xb651),
			data.read_word(0xb652), data.read_word(0xb653), data.read_word(0xb65e), data.read_word(0xb65f), unsigned(m_command_tx_words.size()));
		logerror("mu4_native_measurement_response: result=%04x ack=%u rx_irqs=%u wire_bits=%u busy=%u ready=%u rx_head=%04x rx_tail=%04x tx_head=%04x tx_tail=%04x pending=%04x queued=%04x\n",
			m_command_tx_words.size() > 8 ? m_command_tx_words[8] : 0xffff, m_command_ack_cursor, m_command_rx_irqs,
			m_command_wire_bit_count, unsigned(m_command_rx_busy), unsigned(m_command_rx_ready),
			data.read_word(0x1657), data.read_word(0x1658), data.read_word(0x1659), data.read_word(0x165a), data.read_word(0x165d), data.read_word(0x165e));
		if (m_measurement_blocks != 6 || m_measurement_branch_mask != 15 || m_command_ack_cursor != 3 || m_command_tx_words.size() != 12 ||
			m_command_tx_words[8] != expected_measurement_result() || m_command_wire_bit_count || m_command_rx_irqs != 11 ||
			m_command_rx_busy || m_command_rx_ready || data.read_word(0x1657) != 11 || data.read_word(0x1658) != 11 ||
			data.read_word(0x1659) != 12 || data.read_word(0x165a) != 12 ||
			data.read_word(0xbb80) || data.read_word(0x165d) || data.read_word(0x165e))
			fatalerror("MU4 original measurement command did not complete six blocks and settle its acknowledged response");
		logerror("mu4_native_measurement: PASS blocks=6 stereo=1 independent_arithmetic=1 pin_request=1 ack=1 tx_words=12 mode=0 music_decode=0 full_boot=0\n");
		logerror("mu4_native_measurement_result: PASS result=%02x threshold=10000000 executed_comparisons=2 independent_energy=1 right_precedence=1\n", expected_measurement_result());
		if (m_tone_blocks < 6 || !m_tone_phase_wraps)
			fatalerror("MU4 native tone observation did not cover repeated blocks and signed phase wraparound");
		logerror("mu4_native_tone: PASS blocks=%u samples=%u stereo=1 independent_table_scale=1 phase_wraps=%u music_decode=0 analog_audio=0\n",
			m_tone_blocks, m_tone_blocks * 128, m_tone_phase_wraps);
	}
	void install_measurement_observers()
	{
		for (offs_t address : {0x2876a, 0x287ad})
			m_cpu->space(AS_PROGRAM).install_read_tap(address, address, "mu4_native_tone",
				[this](offs_t address, u16 &, u16) { observe_native_tone(address); });
		m_cpu->space(AS_DATA).install_read_tap(0x1900, 0x19ff, "mu4_measurement_input",
			[this](offs_t, u16 &value, u16)
			{
				if (m_phase != 30 || machine().side_effects_disabled()) return;
				unsigned const pc = m_cpu->pc();
				if (pc != 0x287fa && pc != 0x2880b) return;
				unsigned const channel = pc == 0x2880b;
				s32 const sample = s16(value);
				u32 const scaled = unsigned(sample < 0 ? -sample : sample) / 16;
				u64 const sum = m_measurement_expected[channel] + u64(scaled) * scaled * m_measurement_factor;
				if (sum > 0x7fffffff || ++m_measurement_samples[channel] > 64)
					fatalerror("MU4 live measurement inputs exceed independently modeled block range");
				m_measurement_expected[channel] = sum;
			});
		m_cpu->space(AS_PROGRAM).install_read_tap(0x287ea, 0x28816, "mu4_measurement_result",
			[this](offs_t address, u16 &, u16)
			{
				if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->pc() != address + 1) return;
				if (address != 0x287ea && address != 0x28816) return;
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				if (address == 0x287ea)
				{
					// Observe live input reads: DMA can update the buffer during execution.
					m_measurement_factor = BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 6) ? 2 : 1;
					m_measurement_samples.fill(0);
					for (unsigned channel = 0; channel < 2; ++channel)
					{
						u16 const aggregate = 0xb64a + 2 * channel;
						m_measurement_expected[channel] = (u32(data.read_word(aggregate)) << 16) | data.read_word(aggregate + 1);
					}
					return;
				}
				u16 const left = m_cpu->state_int(tms320c54x_device::STATE_AR4), right = m_cpu->state_int(tms320c54x_device::STATE_AR5);
				if (m_measurement_samples[0] != 64 || m_measurement_samples[1] != 64 ||
					((u32(data.read_word(left)) << 16) | data.read_word(left + 1)) != m_measurement_expected[0] ||
					((u32(data.read_word(right)) << 16) | data.read_word(right + 1)) != m_measurement_expected[1])
					fatalerror("MU4 native sample-energy mismatch block=%u samples=%u,%u actual=%08x,%08x expected=%08x,%08x factor=%u", m_measurement_blocks,
						m_measurement_samples[0], m_measurement_samples[1], (u32(data.read_word(left)) << 16) | data.read_word(left + 1),
						(u32(data.read_word(right)) << 16) | data.read_word(right + 1), m_measurement_expected[0], m_measurement_expected[1], m_measurement_factor);
				logerror("mu4_native_measurement_block: index=%u left=%04x,%04x right=%04x,%04x st1=%04x\n",
					m_measurement_blocks++, data.read_word(left), data.read_word(left + 1), data.read_word(right), data.read_word(right + 1),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
			});
	}
	virtual void machine_start() override
	{
		m_cpu->space(AS_DATA).install_read_tap(0x6000, 0x6001, "mu4_dma_read_ownership",
			[this](offs_t address, u16 &, u16) {
				if (m_phase < 25 || m_phase > 29 || machine().side_effects_disabled()) return;
				if (!m_dma->source_read_matches(0, AS_DATA, address)) return;
				if (m_dma->destination_write_matches(0, AS_DATA, 0x6100) ||
					m_dma->destination_write_matches(0, AS_PROGRAM, 0x2ffff))
					fatalerror("MU4 DMA source read also claimed destination-write ownership");
				if (m_dma->source_read_matches(1, AS_DATA, address) ||
					m_dma->source_read_matches(6, AS_DATA, address) ||
					m_dma->source_read_matches(0, AS_PROGRAM, address) ||
					m_dma->source_read_matches(0, AS_DATA, address ^ 1))
					fatalerror("MU4 actual DMA source read matched another channel/space/address");
				++m_dma_observed_reads;
			});
		auto observe_dma_write = [this](int space, offs_t address) {
			if (m_phase < 25 || m_phase > 29 || machine().side_effects_disabled()) return;
			if (!m_dma->destination_write_matches(0, space, address)) return;
			if (m_dma->destination_write_matches(1, space, address) ||
				m_dma->destination_write_matches(6, space, address) ||
				m_dma->destination_write_matches(0, space == AS_DATA ? AS_PROGRAM : AS_DATA, address) ||
				m_dma->destination_write_matches(0, space, address ^ 1) ||
				m_dma->source_read_matches(0, AS_DATA, 0x6000) || m_dma->source_read_matches(0, AS_DATA, 0x6001))
				fatalerror("MU4 actual DMA destination write matched another owner or source read");
			++m_dma_observed_writes;
		};
		m_cpu->space(AS_DATA).install_write_tap(0x6100, 0x6100, "mu4_dma_data_write_ownership",
			[observe_dma_write](offs_t address, u16 &, u16) { observe_dma_write(AS_DATA, address); });
		m_cpu->space(AS_PROGRAM).install_write_tap(0x20000, 0x2ffff, "mu4_dma_program_write_ownership",
			[observe_dma_write](offs_t address, u16 &, u16) { observe_dma_write(AS_PROGRAM, address); });
		logerror("mu4_clock_reset_profile: pins=111 clkmd=0000 divider=2 family=C5416_compatible exact_da150_identity=0\n");
		if (original_bootstrap_profile())
		{
			m_phase = 74;
			m_cpu->space(AS_PROGRAM).install_ram(0, 0xffff, &m_program_ram[0]);
			if (shared_daram_profile())
			{
				// Firmware-derived executable data vectors/startup, not a verified DA150 RAM extent.
				m_cpu->space(AS_PROGRAM).install_readwrite_handler(0x2000, 0x7fff,
					read16sm_delegate(*this, FUNC(mu4_storage_test_state::shared_daram_r)),
					write16sm_delegate(*this, FUNC(mu4_storage_test_state::shared_daram_w)));
				logerror("mu4_shared_daram_probe: first=2000 last=7fff program_data_shared=1 physical_extent=unvalidated\n");
			}
			if (measurement_profile()) install_measurement_observers();
			if (playback_profile()) machine().add_notifier(MACHINE_NOTIFY_EXIT, machine_notify_delegate(&mu4_storage_test_state::report_playback_parent, this));
			if (recorder_ac_profile())
			{
				install_recorder_buffer_observers();
				// Pair bus transactions by active DMA channel, never by sample similarity.
				int const spaces[] = {AS_PROGRAM,AS_DATA,AS_IO};
				for (unsigned index=0;index<3;++index)
					m_cpu->space(spaces[index]).install_write_tap(0,index ? 0xffff : 0x7fffff,"mu4_recorder_dma_destination",
						[this,index,space=spaces[index]](offs_t address,u16 &value,u16 mask) {
							if (machine().side_effects_disabled()) return;
							for (unsigned channel=0;channel<6;++channel)
							{
								if (!m_ac_dma_pending[channel] || !m_dma->destination_write_matches(channel,space,address)) continue;
								if (mask != 0xffff || value != m_ac_dma_word[channel]) ++m_ac_dma_bad;
								unsigned const slot = channel*3+index;
								if (!m_ac_dma_writes[slot]) m_ac_dma_first[slot] = m_ac_dma_last[slot] = address;
								else { m_ac_dma_first[slot] = std::min(m_ac_dma_first[slot],u32(address)); m_ac_dma_last[slot] = std::max(m_ac_dma_last[slot],u32(address)); }
								++m_ac_dma_writes[slot]; m_ac_dma_pending[channel] = false;
							}
						});
				logerror("mu4_recorder_read_scope: address=0021 context_cap=128 dma_identity=active_source_read context_pc=not_instruction_owner side_effects_disabled=excluded observer_writes=0\n");
				m_cpu->space(AS_DATA).install_read_tap(0x21, 0x21, "mu4_recorder_ac_receive",
					[this](offs_t address, u16 &value, u16) {
						if (machine().side_effects_disabled()) return;
						u8 owners = 0;
						for (unsigned channel=0;channel<6;++channel)
							if (m_dma->source_read_matches(channel,AS_DATA,address)) owners |= 1U << channel;
						++m_ac_read_total; ++m_ac_read_owners[owners];
						for (unsigned channel=0;channel<6;++channel)
							if (BIT(owners,channel))
							{
								if (m_ac_dma_pending[channel]) ++m_ac_dma_bad;
								m_ac_dma_pending[channel] = true; m_ac_dma_word[channel] = value;
							}
						if (!owners)
						{
							u32 const pc = m_cpu->state_int(STATE_GENPC);
							auto found = m_ac_unowned_contexts.find(pc);
							if (found != m_ac_unowned_contexts.end()) ++found->second;
							else if (m_ac_unowned_contexts.size() < 128) m_ac_unowned_contexts.emplace(pc,1);
							else ++m_ac_context_dropped;
						}
						if (!m_dma->source_read_matches(2, AS_DATA, address)) { ++m_ac_other_owners; return; }
						std::array<u16, 4> const alphabet{0x2000, 0xe000, 0x1000, 0xf000};
						auto const found = std::find(alphabet.begin(), alphabet.end(), value);
						if (found == alphabet.end()) ++m_ac_other;
						else ++m_ac_received[found - alphabet.begin()];
					});
			}
			if (playback_profile())
			{
				// Own initialized D796 context, word22. This observes CPU operand
				// reads, not write ownership, instruction retirement or queue capacity.
				m_cpu->space(AS_DATA).install_read_tap(0xd7ac,0xd7ac,"mu4_context_count_read",
					[this](offs_t address,u16 &value,u16 mask) {
						if (!m_playback_overlay_checked || m_playback_execute_entries[1]!=1 || machine().side_effects_disabled()) return;
						double const time=machine().time().as_double();
						if (time<9.64 || time>9.86) return;
						u32 instruction; u16 opcode;
						if (!m_cpu->observe_staged_data_read(address,instruction,opcode)) return;
						if (++m_context_count_reads>256) return;
						logerror("mu4_context_count_read: ordinal=%u address=%04x instruction=%06x opcode=%04x word=%04x mask=%04x t=%.9f identity=Access_bus observer_writes=0\n",
							m_context_count_reads,unsigned(address),instruction,opcode,value,mask,time);
					});
				m_cpu->space(AS_DATA).install_write_tap(0x23, 0x23, "mu4_playback_dxr_owner",
					[this](offs_t address, u16 &value, u16 mask) {
						if (!m_playback_overlay_checked || machine().side_effects_disabled()) return;
						if (mask != 0xffff) fatalerror("MU4 playback DXR observer requires full-word writes");
						u8 owner = 0;
						for (unsigned channel=0;channel<6;++channel)
							if (m_dma->destination_write_matches(channel, AS_DATA, address)) owner = channel+1;
						// Owner 0 means no active DMA write, not an inferred CPU instruction.
						u64 const ns = machine().time().as_ticks(1'000'000'000);
						u32 const ordinal = owner == 4 ? m_playback_dma_reads : 0;
						char bytes[16]{};
						for (unsigned i=0;i<8;++i) bytes[i] = char(ns >> (8*i));
						for (unsigned i=0;i<4;++i) bytes[8+i] = char(ordinal >> (8*i));
						bytes[12] = char(value); bytes[13] = char(value >> 8); bytes[14] = char(owner);
						m_playback_dxr_capture.write(bytes,sizeof(bytes));
						for (char byte : bytes) m_playback_dxr_hash = (m_playback_dxr_hash ^ u8(byte)) * 16777619U;
						++m_playback_dxr_writes;
					});
				m_cpu->space(AS_DATA).install_write_tap(0x1000, 0x137f, "mu4_playback_buffer_writer",
					[this](offs_t address, u16 &value, u16 mask) {
						double const time = machine().time().as_double();
						if (machine().side_effects_disabled() || time < 1.0 || time > 1.25) return;
						m_playback_buffer_writes[address - 0x1000] = {time, m_cpu->pc(), value, mask};
					});
				m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mu4_playback_dma_source",
					[this](offs_t address, u16 &value, u16 mask) {
						if (!m_playback_overlay_checked || machine().side_effects_disabled()) return;
						if (m_playback_execute_entries[1] == 1 && machine().time() >= attotime::from_msec(9640) && machine().time() <= attotime::from_msec(9860))
						{
							u32 instruction; u16 opcode;
							if (m_cpu->observe_staged_data_read(address,instruction,opcode))
							{
								if (instruction == 0x201c7 && ++m_context_pointer_reads <= 256)
									logerror("mu4_context_pointer_read: ordinal=%u address=%04x instruction=%06x opcode=%04x word=%04x mask=%04x t=%.9f identity=Access_bus observer_writes=0\n",
										m_context_pointer_reads,unsigned(address),instruction,opcode,value,mask,machine().time().as_double());
								if (instruction == 0x2a310 && ++m_exchange_status_reads <= 256)
									logerror("mu4_exchange_status_read: ordinal=%u address=%04x instruction=%06x opcode=%04x word=%04x mask=%04x t=%.9f identity=Access_bus observer_writes=0\n",
										m_exchange_status_reads,unsigned(address),instruction,opcode,value,mask,machine().time().as_double());
							}
						}
						if (!m_dma->source_read_matches(3, AS_DATA, address)) return;
						++m_playback_dma_reads;
						double const time = machine().time().as_double();
						if (time >= 1.0 && time <= 1.25 && address >= 0x1000 && address <= 0x137f)
						{
							auto const &writer = m_playback_buffer_writes[address - 0x1000];
							logerror("mu4_playback_dma_epoch: read=%u address=%04x word=%04x t=%.9f last_write_t=%.9f last_write_pc=%06x last_write_word=%04x last_write_mask=%04x channel=3 ownership=active_source_read observer_writes=0 audio_acceptance=0\n",
								m_playback_dma_reads, unsigned(address), value, time, writer.time, writer.pc, writer.word, writer.mask);
						}
						capture_playback_word(2,value);
						m_playback_dma_hash = (m_playback_dma_hash ^ (value >> 8)) * 16777619U;
						m_playback_dma_hash = (m_playback_dma_hash ^ (value & 0xff)) * 16777619U;
						m_playback_dma_min = std::min(m_playback_dma_min, u16(address));
						m_playback_dma_max = std::max(m_playback_dma_max, u16(address));
						if (m_playback_dma_reads <= 8)
						{
							u16 const index = m_dma->read(1);
							logerror("mu4_playback_dma_config: read=%u source=%04x destination=%04x elements=%04x frames=%04x mode=%04x reload_source=%04x reload_destination=%04x reload_elements=%04x reload_frames=%04x indexes=%04x,%04x,%04x,%04x t=%.9f channel=3 boundary=source_read index_preserved=1\n",
								m_playback_dma_reads, m_dma->inspect_register(0x0f), m_dma->inspect_register(0x10), m_dma->inspect_register(0x11), m_dma->inspect_register(0x12), m_dma->inspect_register(0x13),
								m_dma->inspect_register(0x32), m_dma->inspect_register(0x33), m_dma->inspect_register(0x34), m_dma->inspect_register(0x35),
								m_dma->inspect_register(0x20), m_dma->inspect_register(0x21), m_dma->inspect_register(0x22), m_dma->inspect_register(0x23), machine().time().as_double());
							if (m_dma->read(1) != index) fatalerror("MU4 DMA inspection changed its index latch");
						}
						if (m_playback_dma_reads <= 4)
							logerror("mu4_playback_dma_word: kind=prefix ordinal=%u read=%u address=%04x word=%04x t=%.9f channel=3 space=data ownership=active_source_read\n",
								m_playback_dma_reads, m_playback_dma_reads, unsigned(address), value, machine().time().as_double());
						if (!value) return;
						m_playback_dma_last_nonzero = machine().time().as_double();
						if (++m_playback_dma_nonzero <= 4)
							logerror("mu4_playback_dma_word: kind=nonzero ordinal=%u read=%u address=%04x word=%04x t=%.9f channel=3 space=data ownership=active_source_read\n",
								m_playback_dma_nonzero, m_playback_dma_reads, unsigned(address), value, m_playback_dma_last_nonzero);
					});
			}
			if (staged_original_profile())
			{
				m_cpu->space(AS_IO).install_write_handler(0x80, 0x80,
					write16smo_delegate(*this, FUNC(mu4_storage_test_state::staged_unmodeled_port_w)));
				// Observation only: quantify the original reset copy, not just
				// reaching the later unsupported opcode. No source-hole values assumed.
				m_cpu->space(AS_PROGRAM).install_read_tap(0x60, 0xff80, "staged_bootstrap_copy_source",
					[this](offs_t at, u16 &word, u16) {
						if (machine().side_effects_disabled() || m_staged_copy_reads == 65313 || (!m_staged_copy_reads && at != 0x60)) return;
						if (at != 0x60 + m_staged_copy_reads) fatalerror("Staged original bootstrap copy source order");
						++m_staged_copy_reads; m_staged_copy_word = word;
					});
				m_cpu->space(AS_DATA).install_write_tap(0x60, 0xff80, "staged_bootstrap_copy_destination",
					[this](offs_t at, u16 &word, u16) {
						if (machine().side_effects_disabled() || !m_staged_copy_reads || m_staged_copy_writes == 65313) return;
						if (at != 0x60 + m_staged_copy_writes || m_staged_copy_reads != m_staged_copy_writes + 1 || word != m_staged_copy_word)
							fatalerror("Staged original bootstrap copy destination/value order");
						if (++m_staged_copy_writes == 65313)
							logerror("mu4_staged_bootstrap_copy: PASS program_reads=65313 data_writes=65313 first=0060 last=ff80 native_startup_complete=0\n");
					});
			}
			if (recorder_profile())
			{
				if (recorder_control_profile())
				{
					m_cpu->space(AS_PROGRAM).install_read_tap(0x284cd, 0x284db, "mu4_recorder_application",
						[this](offs_t address, u16 &, u16)
						{
							if (machine().side_effects_disabled() || !m_recorder_overlay_checked || m_cpu->pc() != address + 1) return;
							if (address == 0x284cd) ++m_recorder_application_calls;
							if (address != 0x284db) return;
							if (m_recorder_application_messages++ >= 12) return;
							auto const disable = machine().disable_side_effects();
							auto &data = m_cpu->space(AS_DATA);
							logerror("mu4_native_recorder_application_message: length=%04x class=%04x selector=%04x parameter=%04x token=%04x previous_class=%04x previous_selector=%04x time_ms=%lld\n",
								data.read_word(0x45f), data.read_word(0x460), data.read_word(0x461), data.read_word(0x462), data.read_word(0x463),
								data.read_word(0x3766), data.read_word(0x3767),
								static_cast<long long>(machine().time().as_ticks(1000)));
						});
					m_cpu->space(AS_PROGRAM).install_read_tap(0x3943a, 0x394a2, "mu4_recorder_processing",
						[this](offs_t address, u16 &, u16)
						{
							if (machine().side_effects_disabled() || !m_recorder_overlay_checked || m_cpu->pc() != address + 1) return;
							if (address == 0x394a2) ++m_recorder_processing_returns;
							else if (address == 0x3943a || address == 0x39499) ++m_recorder_processing_waits;
						});
					m_cpu->space(AS_PROGRAM).install_read_tap(0x285dc, 0x285dc, "mu4_recorder_control",
						[this](offs_t address, u16 &, u16)
						{
							if (!machine().side_effects_disabled() && m_recorder_overlay_checked && m_cpu->pc() == address + 1)
							{
								++m_recorder_control_entries;
								logerror("mu4_native_recorder_control_entry: pc=0285dc pin_request=1\n");
							}
						});
				}
				m_cpu->space(AS_PROGRAM).install_read_tap(0x2000, 0x2000, "mu4_recorder_overlay_entry",
					[this](offs_t address, u16 &, u16)
					{
						if (!machine().side_effects_disabled() && m_cpu->pc() == address + 1 &&
							m_recorder_loader_calls && !m_recorder_overlay_checked)
							verify_media_overlay();
					});
				m_cpu->space(AS_PROGRAM).install_read_tap(0x29e90, 0x29fa8, "mu4_recorder_loader",
					[this](offs_t address, u16 &, u16)
					{
						// These symbols belong to aa22; aa88 reuses their addresses for other code.
						if (machine().side_effects_disabled() || m_cpu->pc() != address + 1 || m_recorder_overlay_checked) return;
						if (address == 0x29e90) ++m_recorder_dispatches;
						else if (address == 0x29fa8) ++m_recorder_loader_calls;
						else return;
						auto const disable = machine().disable_side_effects();
						auto &data = m_cpu->space(AS_DATA);
						logerror("mu4_native_recorder_path: address=%06x lifecycle=%04x capacity=%04x mode=%04x\n",
							unsigned(address), data.read_word(0x373e), data.read_word(0x374e), data.read_word(0x3750));
					});
			}
			m_cpu->space(AS_PROGRAM).install_read_tap(0x0e41, 0x0e41, "mu4_original_bootstrap_entry",
				[this](offs_t address, u16 &, u16) { if (!machine().side_effects_disabled() && m_cpu->pc() == address + 1) ++m_native_bootstrap_entries; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d62, 0x6d62, "mu4_original_bootstrap_resident",
				[this](offs_t address, u16 &, u16) {
					if (machine().side_effects_disabled() || (m_cpu->pc() & 0xffff) != address + 1) return;
					++m_native_bootstrap_resident_entries;
					if ((recorder_save_profile() || cold_catalog_profile()) && !m_native_bootstrap_memory_checked) observe_bootstrap_resident_memory();
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x090f, 0x3538, "mu4_original_bootstrap_calls",
				[this](offs_t address, u16 &, u16)
				{
					if (machine().side_effects_disabled() || m_cpu->pc() != address + 1) return;
					static constexpr offs_t points[] = {0x090f, 0x0945, 0x094e, 0x0951, 0x095e, 0x096c, 0x0997, 0x09b3, 0x09b5, 0x3538};
					auto const found = std::find(std::begin(points), std::end(points), address);
					if (found == std::end(points)) return;
					unsigned &count = m_native_bootstrap_paths[found - std::begin(points)];
					if (count++ < 4)
					{
						logerror("mu4_original_bootstrap_path: address=%04x a=%010llx nand_reads=%u\n", unsigned(address),
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL, m_data_reads);
						if (address == 0x3538)
						{
							auto const disable = machine().disable_side_effects();
							auto &data = m_cpu->space(AS_DATA);
							unsigned const index = u16(m_cpu->state_int(tms320c54x_device::STATE_A));
							// Fetch-time A is a snapshot, not the loader's consumed argument.
							// Original aa55 loader 2eda multiplies the index by 44 decimal.
							unsigned const descriptor = 0x36b0 + index * 0x2c;
							logerror("mu4_original_bootstrap_loader: name=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x ext=%04x,%04x,%04x length=%04x,%04x vector=%04x,%04x index=%u\n",
								data.read_word(descriptor), data.read_word(descriptor + 1), data.read_word(descriptor + 2), data.read_word(descriptor + 3),
								data.read_word(descriptor + 4), data.read_word(descriptor + 5), data.read_word(descriptor + 6), data.read_word(descriptor + 7),
								data.read_word(descriptor + 9), data.read_word(descriptor + 10), data.read_word(descriptor + 11), data.read_word(descriptor + 18), data.read_word(descriptor + 19),
								m_cpu->space(AS_PROGRAM).read_word(0x2000), m_cpu->space(AS_PROGRAM).read_word(0x2001), index);
						}
					}
				});
			m_cpu->space(AS_DATA).install_read_tap(0x36b0, 0x4cdf, "mu4_original_descriptor_reads",
				[this](offs_t address, u16 &value, u16)
				{
					if (machine().side_effects_disabled() || m_native_descriptor_reads >= 32 ||
						m_cpu->pc() < 0x2ed4 || m_cpu->pc() > 0x2fa8) return;
					// Record real data-bus reads, not fields inferred from a prefetched PC.
					++m_native_descriptor_reads;
					logerror("mu4_original_descriptor_read: address=%04x value=%04x pc=%06x ar1=%04x t=%04x\n",
						unsigned(address), value, unsigned(m_cpu->pc()),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_T)));
				});
			m_cpu->space(AS_DATA).install_write_tap(0x36b0, 0x36db, "mu4_original_descriptor_writes",
				[this](offs_t address, u16 &value, u16)
				{
					if (machine().side_effects_disabled() || !m_data_reads || m_native_descriptor_writes >= 64) return;
					++m_native_descriptor_writes;
					logerror("mu4_original_descriptor_write: address=%04x value=%04x pc=%06x nand_reads=%u\n",
						unsigned(address), value, unsigned(m_cpu->pc()), m_data_reads);
				});
			m_cpu->space(AS_DATA).install_write_tap(0x374d, 0x374d, "mu4_original_bootstrap_gate_init",
				[this](offs_t, u16 &value, u16) { if (!machine().side_effects_disabled()) logerror("mu4_original_bootstrap_gate_init: value=%04x pc=%06x\n", value, unsigned(m_cpu->pc())); });
			if (recorder_save_profile())
			{
				m_cpu->space(AS_DATA).install_write_tap(0x36c5, 0x36c5, "mu4_file_boundary_cursor",
					[this](offs_t, u16 &value, u16) {
						if (machine().side_effects_disabled() || m_native_boundary_events >= 128) return;
						if (value >= 0x1fe0 && value <= 0x2010) m_native_boundary_window = true;
						if (!m_native_boundary_window) return;
						++m_native_boundary_events;
						auto const disable = machine().disable_side_effects();
						logerror("mu4_file_boundary_cursor: value=%04x context_pc=%06x word36c4=%04x word36c6=%04x\n",
							value, unsigned(m_cpu->pc()), m_cpu->space(AS_DATA).read_word(0x36c4), m_cpu->space(AS_DATA).read_word(0x36c6));
					});
				m_cpu->space(AS_DATA).install_write_tap(0x3c21, 0x3c30, "mu4_file_boundary_buffer",
					[this](offs_t address, u16 &value, u16) {
						if (machine().side_effects_disabled() || !m_native_boundary_window || m_native_boundary_events >= 128) return;
						++m_native_boundary_events;
						logerror("mu4_file_boundary_buffer: address=%04x value=%04x context_pc=%06x\n", unsigned(address), value, unsigned(m_cpu->pc()));
					});
				m_cpu->space(AS_PROGRAM).install_read_tap(0x32ee, 0x32ee, "mu4_file_boundary_swap",
					[this](offs_t address, u16 &, u16) {
						if (machine().side_effects_disabled() || m_cpu->pc() != address + 1 || !m_native_boundary_window || m_native_boundary_events >= 128) return;
						++m_native_boundary_events;
						logerror("mu4_file_boundary_swap_fetch: a=%010llx b=%010llx ar6=%04x brc=%04x fetch_snapshot=1\n",
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)) & 0xffffffffffULL,
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)));
					});
				m_cpu->space(AS_PROGRAM).install_write_tap(0x028e80, 0x028e8f, "mu4_file_boundary_dma",
					[this](offs_t address, u16 &value, u16) {
						if (machine().side_effects_disabled() || m_native_boundary_dma_events >= 32) return;
						++m_native_boundary_dma_events;
						u16 const source = 0x3c21 + (address - 0x028e80);
						logerror("mu4_file_boundary_dma: destination=%06x value=%04x source=%04x channel0_source_match=%u\n",
							unsigned(address), value, source, unsigned(m_dma->source_matches(0, AS_DATA, source)));
						if (address == 0x028e8f) m_native_boundary_window = false;
					});
				m_cpu->space(AS_DATA).install_read_tap(0x36c4, 0x36c6, "mu4_file_boundary_cluster_reads",
					[this](offs_t address, u16 &value, u16) {
						if (machine().side_effects_disabled() || !m_native_boundary_window || m_native_boundary_events >= 128 ||
							m_cpu->pc() < 0x336c || m_cpu->pc() > 0x3380) return;
						++m_native_boundary_events;
						logerror("mu4_file_boundary_cluster_read: address=%04x value=%04x context_pc=%06x cycle=%llu\n",
							unsigned(address), value, unsigned(m_cpu->pc()), static_cast<unsigned long long>(m_cpu->pipeline_cycles()));
					});
				m_cpu->space(AS_PROGRAM).install_read_tap(0x336c, 0x33c0, "mu4_file_boundary_cluster_path",
					[this](offs_t address, u16 &, u16) {
						if (machine().side_effects_disabled() || !m_native_boundary_window || m_native_boundary_events >= 128 || m_cpu->pc() != address + 1 ||
							(address != 0x336c && address != 0x3375 && address != 0x3379 && address != 0x33bf && address != 0x33c0)) return;
						++m_native_boundary_events;
						logerror("mu4_file_boundary_cluster_fetch: address=%04x a=%010llx ar1=%04x cycle=%llu fetch_snapshot=1\n", unsigned(address),
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)), static_cast<unsigned long long>(m_cpu->pipeline_cycles()));
					});
				m_cpu->space(AS_PROGRAM).install_read_tap(0x343a, 0x3517, "mu4_file_boundary_fat_path",
					[this](offs_t address, u16 &, u16) {
						if (machine().side_effects_disabled() || !m_native_boundary_window || m_native_boundary_events >= 128 || m_cpu->pc() != address + 1) return;
						switch (address)
						{
						case 0x343a: case 0x344e: case 0x3458: case 0x347b: case 0x347c: case 0x347d: case 0x3482: case 0x348c:
						case 0x34a2: case 0x34c6: case 0x34dc: case 0x34e1: case 0x34ea: case 0x34f4: case 0x3512:
							break;
						default: return;
						}
						++m_native_boundary_events;
						logerror("mu4_file_boundary_fat_fetch: address=%04x a=%010llx b=%010llx ar1=%04x ar2=%04x ar6=%04x cycle=%llu fetch_snapshot=1\n", unsigned(address),
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)) & 0xffffffffffULL,
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), static_cast<unsigned long long>(m_cpu->pipeline_cycles()));
					});
				m_cpu->space(AS_DATA).install_read_tap(0x0008, 0x0008, "mu4_file_boundary_fat_al",
					[this](offs_t, u16 &value, u16) {
						if (machine().side_effects_disabled() || !m_native_boundary_window || m_native_boundary_events >= 128 ||
							m_cpu->pc() < 0x3478 || m_cpu->pc() > 0x3482) return;
						++m_native_boundary_events;
						logerror("mu4_file_boundary_fat_al_read: value=%04x context_pc=%06x cycle=%llu\n", value,
							unsigned(m_cpu->pc()), static_cast<unsigned long long>(m_cpu->pipeline_cycles()));
					});
			}
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3acf2, 0x3acf2, "mu4_original_bootstrap_dispatch",
				[this](offs_t address, u16 &, u16) { if (!machine().side_effects_disabled() && m_cpu->pc() == address + 1) ++m_native_command_paths[5]; });
		}
		if (system_bios() == 15)
		{
			// Fresh-process routine bench: mount externally retained NAND, never
			// run the erased-media formatter or inject a resident state snapshot.
			m_phase = 19;
			logerror("mu4_native_retained_start: fresh_process=1 original_mount=1 erase=0 runtime_snapshot=0\n");
		}
		m_check = timer_alloc(FUNC(mu4_storage_test_state::check), this);
		m_command = timer_alloc(FUNC(mu4_storage_test_state::command_input), this);
		m_command_clock = timer_alloc(FUNC(mu4_storage_test_state::command_clock), this);
		m_command_receive_clock = timer_alloc(FUNC(mu4_storage_test_state::command_receive_clock), this);
		m_native_replay = timer_alloc(FUNC(mu4_storage_test_state::native_replay_checkpoint), this);
		if (system_bios() >= 11)
		{
			// Save the external peer alongside McBSP2. Growing observation vectors
			// need bounded backing storage; save_item(vector) fixes its initial size.
			save_item(NAME(m_direction)); save_item(NAME(m_gpio)); save_item(NAME(m_ready));
			save_item(NAME(m_ac_generated)); save_item(NAME(m_ac_received));
			save_item(NAME(m_ac_other)); save_item(NAME(m_ac_other_owners));
			save_item(NAME(m_serial_index)); save_item(NAME(m_serial_regs));
			save_item(NAME(m_serial_word)); save_item(NAME(m_serial_ready)); save_item(NAME(m_serial_reads));
			save_item(NAME(m_serial_empty_reads));
			save_item(NAME(m_native_command_cursor)); save_item(NAME(m_native_command_waits));
			save_item(NAME(m_cold_catalog_scans)); save_item(NAME(m_cold_metadata_reads));
			save_item(NAME(m_cold_notifications)); save_item(NAME(m_cold_response_begin));
			save_item(NAME(m_cold_title_publications)); save_item(NAME(m_cold_title_verified));
			save_item(NAME(m_playback_requests)); save_item(NAME(m_playback_inspections)); save_item(NAME(m_playback_decoder_loads));
			save_item(NAME(m_playback_overlay_checked));
			save_item(NAME(m_playback_status_replied));
			save_item(NAME(m_manual_status_requested));
			save_item(NAME(m_playback_capture_positions));
			save_item(NAME(m_playback_tx_time_hash));
			save_item(NAME(m_playback_dxr_hash)); save_item(NAME(m_playback_dxr_writes));
			save_item(NAME(m_playback_tx_event_hash)); save_item(NAME(m_playback_tx_events));
			save_item(NAME(m_playback_decoded_hash)); save_item(NAME(m_playback_decoded_words));
			save_item(NAME(m_playback_dac_capture_frames)); save_item(NAME(m_playback_dac_capture_hash));
			save_item(NAME(m_playback_tx_lifecycle_events)); save_item(NAME(m_playback_tx_startup_events));
			save_item(NAME(m_playback_startup_frames));
			save_item(NAME(m_playback_execute_entries)); save_item(NAME(m_playback_execute_steps)); save_item(NAME(m_playback_filter));
			save_item(NAME(m_playback_audio_words)); save_item(NAME(m_playback_audio_nonzero)); save_item(NAME(m_playback_audio_hash));
			save_item(NAME(m_playback_audio_last_nonzero));
			save_item(NAME(m_playback_audio_clocks)); save_item(NAME(m_playback_audio_frames));
			save_item(NAME(m_playback_dma_reads)); save_item(NAME(m_playback_dma_nonzero)); save_item(NAME(m_playback_dma_hash));
			save_item(NAME(m_playback_slot_hash));
			save_item(NAME(m_playback_parent)); save_item(NAME(m_playback_parent_mismatch));
			save_item(NAME(m_playback_switch_samples));
			save_item(NAME(m_playback_enqueue_samples));
			save_item(NAME(m_playback_callback_samples));
			save_item(NAME(m_playback_notify_samples));
			save_item(NAME(m_playback_path_pc)); save_item(NAME(m_playback_path_word));
			save_item(NAME(m_playback_path_next)); save_item(NAME(m_playback_path_count));
			save_item(NAME(m_playback_method_entries)); save_item(NAME(m_playback_method_mismatch));
			save_item(NAME(m_playback_processing)); save_item(NAME(m_playback_processing_sp)); save_item(NAME(m_playback_processing_destination));
			save_item(NAME(m_playback_fft_base)); save_item(NAME(m_playback_fft_parameter)); save_item(NAME(m_playback_fft_invocation));
			save_item(NAME(m_playback_post_base));
			save_item(NAME(m_playback_pre_base)); save_item(NAME(m_playback_pre_parameter));
			save_item(NAME(m_playback_window_base));
			save_item(NAME(m_playback_window_parameter));
			save_item(NAME(m_playback_scale_base)); save_item(NAME(m_playback_scale_parameter));
			save_item(NAME(m_playback_callback_base)); save_item(NAME(m_playback_callback_history)); save_item(NAME(m_playback_callback_mode));
			save_item(NAME(m_playback_callback_target)); save_item(NAME(m_playback_callback_site));
			save_item(NAME(m_playback_tail_base)); save_item(NAME(m_playback_tail_history)); save_item(NAME(m_playback_tail_saved)); save_item(NAME(m_playback_tail_ordinal));
			save_item(NAME(m_playback_conversion)); save_item(NAME(m_playback_conversion_raw)); save_item(NAME(m_playback_conversion_shifted));
			save_item(NAME(m_playback_processing_pending)); save_item(NAME(m_playback_conversion_active));
			save_item(NAME(m_playback_conversion_pending)); save_item(NAME(m_playback_conversion_shift_seen));
			save_item(NAME(m_playback_restore_samples));
			save_item(NAME(m_playback_bit_samples));
			save_item(NAME(m_playback_bit_source_samples));
			save_item(NAME(m_playback_bit_unusual_samples));
			save_item(NAME(m_playback_bit_multiply_samples));
			save_item(NAME(m_playback_optional_started)); save_item(NAME(m_playback_optional_active)); save_item(NAME(m_playback_bit_group));
			save_item(NAME(m_playback_bit_selected)); save_item(NAME(m_playback_bit_caller));
			save_item(NAME(m_playback_activation_pc)); save_item(NAME(m_playback_activation_word)); save_item(NAME(m_playback_activation_time));
			save_item(NAME(m_playback_range_counts)); save_item(NAME(m_playback_range_opcodes)); save_item(NAME(m_playback_range_changes));
			save_item(NAME(m_playback_dma_min)); save_item(NAME(m_playback_dma_max)); save_item(NAME(m_playback_dma_last_nonzero));
			save_item(NAME(m_playback_history_pc)); save_item(NAME(m_playback_history_word)); save_item(NAME(m_playback_history_next));
			save_item(NAME(m_command_tx_irqs)); save_item(NAME(m_command_wire_bits));
			if (media_command_profile())
			{
				save_item(NAME(m_recorder_response_begin)); save_item(NAME(m_recorder_responses));
				save_item(NAME(m_recorder_control_acks));
			}
			if (recorder_profile())
			{
				save_item(NAME(m_recorder_dispatches)); save_item(NAME(m_recorder_loader_calls));
				save_item(NAME(m_recorder_overlay_checked));
				save_item(NAME(m_recorder_startup_entries));
				save_item(NAME(m_recorder_control_entries));
				save_item(NAME(m_recorder_metadata_result));
				save_item(NAME(m_recorder_application_calls)); save_item(NAME(m_recorder_application_messages));
				save_item(NAME(m_recorder_processing_waits)); save_item(NAME(m_recorder_processing_returns));
				save_item(NAME(m_recorder_nand_controls)); save_item(NAME(m_recorder_nand_data));
				save_item(NAME(m_recorder_last_nand_command));
			}
			save_item(NAME(m_command_ack_cursor)); save_item(NAME(m_command_wire_bit_count));
			save_item(NAME(m_command_wire_clock)); save_item(NAME(m_command_ack_pending));
			save_item(NAME(m_command_ack_token)); save_item(NAME(m_command_wire_byte));
			save_item(NAME(m_command_tx_register));
			save_item(NAME(m_command_rx_ready)); save_item(NAME(m_command_rx_busy));
			save_item(NAME(m_command_rx_clock)); save_item(NAME(m_command_rx_byte));
			save_item(NAME(m_command_rx_phase)); save_item(NAME(m_command_rx_irqs));
			save_item(NAME(m_saved_command_tx)); save_item(NAME(m_saved_command_decoded));
			save_item(NAME(m_saved_command_tx_count)); save_item(NAME(m_saved_command_decoded_count));
			machine().save().register_presave(save_prepost_delegate(FUNC(mu4_storage_test_state::save_command_peer), this));
			machine().save().register_postload(save_prepost_delegate(FUNC(mu4_storage_test_state::restore_command_peer), this));
		}
		if (system_bios() == 20)
		{
			save_item(NAME(m_stream_words));
			save_item(NAME(m_continuous_tx)); save_item(NAME(m_continuous_tx_head));
			save_item(NAME(m_continuous_tx_count)); save_item(NAME(m_continuous_din_words));
			save_item(NAME(m_continuous_nonzero_words));
			save_item(NAME(m_source_words)); save_item(NAME(m_source_head)); save_item(NAME(m_source_count)); save_item(NAME(m_source_reads));
			save_item(NAME(m_verified_tone)); save_item(NAME(m_verified_tone_valid));
			save_item(NAME(m_verified_tone_reads)); save_item(NAME(m_tone_dma_words));
			save_item(NAME(m_tone_cleared)); save_item(NAME(m_tone_discarded_words));
			m_cpu->space(AS_DATA).install_write_tap(0xba79, 0xbb78, "mu4_native_tone_cleanup",
				[this](offs_t address, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					unsigned const pc = m_cpu->pc(), bank = address >= 0xbaf9;
					if (pc != (bank ? 0x3b05a : 0x3b058)) return;
					unsigned const offset = address - (bank ? 0xbaf9 : 0xba79), index = bank * 128 + offset;
					if (value || !m_verified_tone_valid[bank] || m_tone_cleared[index])
						fatalerror("MU4 original tone cleanup store contract changed");
					unsigned const position = 2 * (offset % 64) + (offset >= 64);
					if (position >= m_verified_tone_reads[bank]) ++m_tone_discarded_words;
					m_tone_cleared[index] = true;
					m_verified_tone[index] = 0;
				});
			m_cpu->space(AS_DATA).install_read_tap(0x80, 0xffff, "mu4_native_dma_source",
				[this](offs_t address, u16 &value, u16)
				{
					// Actual channel-3 source reads, not merely an enabled matching source.
					if (m_phase != 30 || machine().side_effects_disabled() ||
						!m_dma->source_read_matches(3, AS_DATA, address)) return;
					bool const tone = address >= 0xba79 && address <= 0xbb78;
					if (m_source_count == m_source_words.size()) fatalerror("MU4 DMA source comparison queue overflow");
					m_source_words[(m_source_head + m_source_count) % m_source_words.size()] = value;
					++m_source_count; ++m_source_reads;
					if (!tone) return;
					unsigned const bank = address >= 0xbaf9;
					unsigned const offset = address - (bank ? 0xbaf9 : 0xba79);
					unsigned const count = m_verified_tone_reads[bank], position = count % 128;
					unsigned const expected_offset = position / 2 + ((position & 1) ? 64 : 0);
					if (!m_verified_tone_valid[bank] || (count >= 128 && !m_tone_cleared[bank * 128 + offset]) || offset != expected_offset ||
						value != m_verified_tone[bank * 128 + offset])
						fatalerror("MU4 tone DMA source mismatch bank=%u count=%u offset=%u actual=%04x expected=%04x mode=%04x blocks=%u pc=%06x",
							bank, count, offset, value, m_verified_tone[bank * 128 + offset], m_cpu->space(AS_DATA).read_word(0xbb80), m_tone_blocks, unsigned(m_cpu->pc()));
					++m_verified_tone_reads[bank];
					if (!m_tone_cleared[bank * 128 + offset]) ++m_tone_dma_words;
				});
		}
	}
	void save_command_peer()
	{
		if (m_command_tx_words.size() > m_saved_command_tx.size() || m_command_wire_decoded.size() > m_saved_command_decoded.size())
			fatalerror("MU4 command peer snapshot capture capacity exceeded: tx=%u decoded=%u capacity=%u",
				unsigned(m_command_tx_words.size()), unsigned(m_command_wire_decoded.size()), unsigned(m_saved_command_tx.size()));
		m_saved_command_tx_count = m_command_tx_words.size();
		m_saved_command_decoded_count = m_command_wire_decoded.size();
		std::copy(m_command_tx_words.begin(), m_command_tx_words.end(), m_saved_command_tx.begin());
		std::copy(m_command_wire_decoded.begin(), m_command_wire_decoded.end(), m_saved_command_decoded.begin());
		if (playback_profile())
		{
			auto const captures = playback_capture_streams();
			for (unsigned i = 0; i < captures.size(); ++i)
			{
				auto &stream = *captures[i].first;
				stream.flush();
				auto const offset = stream.tellp();
				if (!stream || offset < 0) fatalerror("MU4 playback checkpoint has no open diagnostic capture");
				m_playback_capture_positions[i] = u64(offset);
			}
		}
	}
	void restore_command_peer()
	{
		if (m_saved_command_tx_count > m_saved_command_tx.size() || m_saved_command_decoded_count > m_saved_command_decoded.size())
			fatalerror("MU4 command peer snapshot has invalid capture count");
		m_command_tx_words.assign(m_saved_command_tx.begin(), m_saved_command_tx.begin() + m_saved_command_tx_count);
		m_command_wire_decoded.assign(m_saved_command_decoded.begin(), m_saved_command_decoded.begin() + m_saved_command_decoded_count);
		if (playback_profile())
		{
			++m_playback_capture_loads;
			auto const captures = playback_capture_streams();
			for (unsigned i = 0; i < captures.size(); ++i)
			{
				auto &stream = *captures[i].first;
				stream.flush();
				auto const offset = stream.tellp();
				if (!stream || offset < 0 || u64(offset) < m_playback_capture_positions[i])
					fatalerror("MU4 playback diagnostic rewind requires the same open capture process");
				stream.close();
				std::string const reference = std::string(captures[i].second) + ".before-load-" + std::to_string(m_playback_capture_loads);
				std::error_code error;
				std::filesystem::copy_file(captures[i].second, reference, std::filesystem::copy_options::none, error);
				if (error) fatalerror("MU4 playback reference capture could not be preserved: %s", error.message());
				std::filesystem::resize_file(captures[i].second, m_playback_capture_positions[i], error);
				if (error) fatalerror("MU4 playback capture could not be rewound: %s", error.message());
				stream.open(captures[i].second, std::ios::binary | std::ios::app);
				if (!stream) fatalerror("MU4 playback capture could not be reopened");
				logerror("mu4_playback_capture_rewind: index=%u bytes=%llu reference=%s same_process=1 firmware_state_forcing=0 observer_writes=0 audio_acceptance=0\n",
					i, static_cast<unsigned long long>(m_playback_capture_positions[i]), reference);
			}
		}
	}
	std::array<std::pair<std::ofstream *, char const *>, 9> playback_capture_streams()
	{
		return {{{&m_playback_word_capture[0], PLAYBACK_CAPTURE_FILES[0]},
			{&m_playback_word_capture[1], PLAYBACK_CAPTURE_FILES[1]},
			{&m_playback_word_capture[2], PLAYBACK_CAPTURE_FILES[2]},
			{&m_playback_slot_capture, "mu4_playback_din.slots"},
			{&m_playback_tx_time_capture, "mu4_playback_tx.times"},
			{&m_playback_dxr_capture, "mu4_playback_dxr.writes"},
			{&m_playback_tx_event_capture, "mu4_playback_tx.events"},
			{&m_playback_decoded_capture, "mu4_decoded_returns.le16"},
			{&m_playback_dac_capture, "mu4_headphone.frames"}}};
	}
	void capture_native_ram(char const *stage)
	{
		if (system_bios() != 14 && system_bios() != 15) return;
		// Observation only: omit CPU/MMIO registers and encode words explicitly.
		auto const disable = machine().disable_side_effects();
		std::string const name = util::string_format("mu4_ram_%s_leg%u.bin", stage, m_native_reset_leg);
		std::ofstream output(name, std::ios::binary | std::ios::trunc);
		auto &data = m_cpu->space(AS_DATA);
		for (unsigned address = 0x80; address < 0x10000; ++address)
		{
			u16 const value = data.read_word(address);
			output.put(value & 0xff);
			output.put(value >> 8);
		}
		output.close();
		if (!output) fatalerror("MU4 RAM observation could not be written");
		logerror("mu4_native_ram_observation: stage=%s leg=%u first=0080 words=65408 encoding=le16 file=%s\n", stage, m_native_reset_leg, name.c_str());
	}
	void serialize_recorder_media(char const *name)
	{
		// Serialize storage only; do not issue NAND commands or save the source NVRAM.
		util::core_file::ptr file;
		if (util::core_file::open(name, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE, file) ||
			!m_nand->nvram_write(*file))
			fatalerror("MU4 recorder media observation could not be written");
		logerror("mu4_native_recorder_media_capture: file=%s observation_only=1\n", name);
	}
	void capture_recorder_media()
	{
		if (!recorder_poll_profile()) return;
		serialize_recorder_media("mu4_recorder_endpoint.nand");
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		for (unsigned i = 0; i < 16; ++i)
			logerror("mu4_native_recorder_file_context: address=%04x value=%04x\n", 0xbf1c + i, data.read_word(0xbf1c + i));
	}
	virtual void machine_reset() override
	{
		if (((system_bios() == 14 && m_phase == 19) || (system_bios() == 18 && m_phase == 74)) && m_native_reset_leg == 1)
		{
			if (m_mcbsp2->control_r(1) || m_mcbsp2->control_r(0))
				fatalerror("MU4 serial controller retained control state across reset");
			m_native_reset_leg = 2;
			logerror("mu4_native_reset_controller: control_cleared=1 loader_restart=1\n");
		}
		m_direction = m_gpio = 0;
		m_ready = true;
		m_bio_reads = m_busy_reads = m_data_reads = 0;
		m_writes.clear();
		m_serial_tx_words.clear(); m_serial_tx_bits.clear();
		m_serial_tx_irqs = 0;
		m_dma_output.clear(); m_dma_completions = 0;
		m_rx_dma_completions = m_rx_irqs = 0;
		m_ac_generated = {}; m_ac_received = {}; m_ac_other = m_ac_other_owners = 0;
		m_ac_read_owners.fill(0); m_ac_unowned_contexts.clear();
		m_ac_dma_pending.fill(false); m_ac_dma_word.fill(0); m_ac_dma_writes.fill(0);
		m_ac_dma_first.fill(0); m_ac_dma_last.fill(0); m_ac_dma_bad = 0;
		m_ac_buffer_word.fill(0); m_ac_buffer_valid.fill(false); m_ac_buffer_read.fill(false);
		m_ac_buffer_writes.fill(0); m_ac_buffer_consumed.fill(0); m_ac_buffer_unread.fill(0); m_ac_buffer_unknown.fill(0);
		m_ac_buffer_readers.fill(0); m_ac_buffer_contexts.clear(); m_ac_buffer_bad = m_ac_buffer_dropped = 0;
		m_ac_buffer_instructions.clear(); m_ac_buffer_identity_missing = m_ac_buffer_identity_dropped = 0;
		m_ac_copy_pending.clear(); m_ac_copy_destinations.clear(); m_ac_copy_reads.fill(0);
		m_ac_copy_writes = m_ac_copy_bad = m_ac_copy_dropped = 0;
		m_ac_workspace.fill({}); m_ac_workspace_readers.fill(0); m_ac_workspace_instructions.clear();
		m_ac_workspace_writes = m_ac_workspace_alias_writes = m_ac_workspace_origins = 0;
		m_ac_workspace_epochs_read = m_ac_workspace_retired = m_ac_workspace_known = m_ac_workspace_unknown = 0;
		m_ac_workspace_bad = m_ac_workspace_dropped = m_ac_workspace_missing = 0;
		m_ac_daram_read_alias = m_ac_daram_write_alias = false;
		for (auto &reads : m_ac_ingress_reads) reads.clear();
		m_ac_ingress_store = {}; m_ac_ingress_loads.fill(0); m_ac_ingress_stores.fill(0); m_ac_ingress_known.fill(0);
		m_ac_ingress_frame_stores.fill(0); m_ac_ingress_frame_known.fill(0); m_ac_ingress_source_base.fill(0); m_ac_ingress_destination_base.fill(0);
		m_ac_ingress_frames = m_ac_ingress_entries = 0; m_ac_ingress_bad = m_ac_ingress_dropped = 0; m_ac_ingress_call = false;
		m_ac_consumer_words.fill({}); m_ac_consumer_current = {}; m_ac_consumer_total = {};
		m_ac_consumer_readers.fill(0); m_ac_consumer_instructions.clear(); m_ac_consumer_entries = m_ac_consumer_returns = 0;
		m_ac_consumer_bad = m_ac_consumer_dropped = 0; m_ac_consumer_base = 0; m_ac_consumer_active = false;
		m_record_transform = {}; m_record_transform_store = {}; m_record_transform_callers.clear();
		m_record_transform_entries = m_record_transform_returns = m_record_transform_unadmitted = 0;
		m_record_transform_bad = m_record_transform_dropped = 0;
		if (m_ac_pcm_capture.is_open()) m_ac_pcm_capture.close();
		m_ac_pcm_capture.clear(); m_ac_pcm_words = m_ac_pcm_bad_order = m_ac_pcm_dropped = 0; m_ac_pcm_hash = 2166136261U;
		if (recorder_ac_profile())
		{
			m_ac_pcm_capture.open("mu4_recorder_input.pcm",std::ios::binary|std::ios::trunc);
			if (!m_ac_pcm_capture) fatalerror("MU4 recorder input capture could not be opened");
		}
		m_ac_read_total = m_ac_context_dropped = 0;
		m_ac_rx_fresh.fill(0); m_ac_rx_stale.fill(0);
		m_ac_rx_publications = m_ac_rx_discarded = m_ac_rx_resets = 0;
		m_ac_rx_bad = m_ac_rx_unknown_stale = 0;
		m_ac_rx_pending = m_ac_rx_known = false; m_ac_rx_word = 0;
		m_stream_words = m_stream_config_writes = 0;
		m_native_din_words = 0;
		m_continuous_tx.fill(0);
		m_continuous_tx_head = m_continuous_tx_count = m_continuous_din_words = m_continuous_nonzero_words = 0;
		m_source_words.fill(0); m_source_head = m_source_count = m_source_reads = 0;
		m_verified_tone.fill(0); m_verified_tone_valid.fill(false); m_verified_tone_reads.fill(0); m_tone_dma_words = 0;
		m_tone_cleared.fill(false); m_tone_discarded_words = 0;
		m_native_dma_rx_vectors = m_native_dma_tx_vectors = 0;
		m_native_dma_tx_handler = 0;
		m_native_stream_pending_sets = m_native_stream_pending_clears = m_native_stream_pending_reads = 0;
		m_native_dispatch_traces = m_native_descriptor_traces = 0;
		m_native_stream_consumer_entries = 0;
		std::fill(std::begin(m_native_selection_traces), std::end(m_native_selection_traces), 0);
		m_native_selection_writes = 0;
		std::fill(std::begin(m_native_startup_fetches), std::end(m_native_startup_fetches), 0);
		m_native_main_call_traces = 0;
		m_native_config_call_traces = 0;
		m_native_tail_data_reads = 0;
		m_native_lookup_operand_traces = 0;
		std::fill(std::begin(m_native_cache_traces), std::end(m_native_cache_traces), 0);
		m_native_startup_subcall_traces = 0;
		m_native_startup_return_traces = 0;
		m_native_consumer_state_traces = m_native_consumer_mode_writes = 0;
		std::fill(std::begin(m_native_consumer_paths), std::end(m_native_consumer_paths), 0);
		std::fill(std::begin(m_native_command_paths), std::end(m_native_command_paths), 0);
		m_native_command_cursor = m_native_command_waits = 0;
		m_measurement_blocks = 0;
		m_measurement_branch_mask = 0;
		m_measurement_expected.fill(0);
		m_measurement_samples.fill(0);
		m_measurement_factor = 1;
		m_tone_expected.fill(0); m_tone_phase.fill(0);
		m_tone_buffer = 0;
		m_tone_blocks = m_tone_phase_wraps = 0;
		m_command->adjust(attotime::never);
		m_command_clock->adjust(attotime::never);
		m_command_receive_clock->adjust(attotime::never);
		m_native_replay->adjust(attotime::never);
		m_native_replay_leg = 0;
		m_command_rx_ready = m_command_rx_busy = false; m_command_rx_clock = true;
		m_command_rx_byte = 0; m_command_rx_phase = m_command_rx_irqs = 0;
		m_command_tx_register = m_command_tx_irqs = m_command_wire_bits = 0;
		m_recorder_response_begin = 3; m_recorder_responses = 0;
		m_recorder_dispatches = m_recorder_loader_calls = 0;
		m_recorder_overlay_checked = false;
		m_recorder_startup_entries = 0;
		m_recorder_control_entries = m_recorder_control_acks = 0;
		m_recorder_metadata_result = 0;
		m_cold_catalog_scans = m_cold_metadata_reads = 0;
		m_cold_notifications = 0; m_cold_response_begin = 14;
		m_cold_title_publications = 0; m_cold_title_verified = false;
		m_playback_requests = m_playback_inspections = m_playback_decoder_loads = 0;
		m_playback_status_replied = false;
		m_manual_status_requested = false;
		m_playback_overlay_checked = false;
		m_playback_execute_entries = {}; m_playback_execute_steps = 0; m_playback_filter = {};
		m_playback_audio_words = {}; m_playback_audio_nonzero = {}; m_playback_audio_hash = {2166136261U, 2166136261U};
		m_playback_audio_last_nonzero = {};
		m_playback_audio_clocks = m_playback_audio_frames = 0;
		m_playback_dma_reads = m_playback_dma_nonzero = 0;
		m_playback_capture_finished = false;
		m_playback_tx_lifecycle_events = 0;
		m_playback_tx_startup_events = m_playback_startup_frames = 0;
		m_playback_dac_frames = 0;
		m_playback_dac_capture_frames = 0; m_playback_dac_capture_hash = 2166136261U;
		m_playback_buffer_writes.fill(playback_buffer_write{});
		m_playback_dma_hash = 2166136261U;
		m_playback_slot_hash = 2166136261U;
		m_playback_tx_time_hash = 2166136261U;
		m_playback_dxr_hash = 2166136261U; m_playback_dxr_writes = 0;
		m_playback_tx_event_hash = 2166136261U; m_playback_tx_events = 0;
		m_playback_decoded_hash = 2166136261U; m_playback_decoded_words = 0;
		m_decoder_gate_events = m_decoder_gate_bad = 0;
		m_context_count_reads = 0;
		m_context_pointer_reads = 0;
		m_context_call_events = m_context_call_bad = 0;
		m_exchange_status_reads = m_exchange_branch_events = m_exchange_branch_bad = 0;
		for (unsigned i=0;i<3;++i)
		{
			auto &stream=m_playback_word_capture[i];
			if (stream.is_open()) stream.close();
			stream.clear();
			if (!playback_profile()) continue;
			stream.open(PLAYBACK_CAPTURE_FILES[i],std::ios::binary|std::ios::trunc);
			if (!stream) fatalerror("MU4 playback observation capture could not be opened");
		}
		if (m_playback_slot_capture.is_open()) m_playback_slot_capture.close();
		m_playback_slot_capture.clear();
		if (m_playback_tx_time_capture.is_open()) m_playback_tx_time_capture.close();
		m_playback_tx_time_capture.clear();
		if (m_playback_dxr_capture.is_open()) m_playback_dxr_capture.close();
		m_playback_dxr_capture.clear();
		if (m_playback_tx_event_capture.is_open()) m_playback_tx_event_capture.close();
		m_playback_tx_event_capture.clear();
		if (m_playback_decoded_capture.is_open()) m_playback_decoded_capture.close();
		m_playback_decoded_capture.clear();
		if (m_playback_dac_capture.is_open()) m_playback_dac_capture.close();
		m_playback_dac_capture.clear();
		if (playback_profile())
		{
			m_playback_slot_capture.open("mu4_playback_din.slots",std::ios::binary|std::ios::trunc);
			if (!m_playback_slot_capture) fatalerror("MU4 playback slot observation could not be opened");
			m_playback_tx_time_capture.open("mu4_playback_tx.times",std::ios::binary|std::ios::trunc);
			if (!m_playback_tx_time_capture) fatalerror("MU4 playback TX time observation could not be opened");
			m_playback_dxr_capture.open("mu4_playback_dxr.writes",std::ios::binary|std::ios::trunc);
			if (!m_playback_dxr_capture) fatalerror("MU4 playback DXR observation could not be opened");
			m_playback_tx_event_capture.open("mu4_playback_tx.events",std::ios::binary|std::ios::trunc);
			if (!m_playback_tx_event_capture) fatalerror("MU4 playback TX event observation could not be opened");
			m_playback_decoded_capture.open("mu4_decoded_returns.le16",std::ios::binary|std::ios::trunc);
			if (!m_playback_decoded_capture) fatalerror("MU4 decoded-return observation could not be opened");
			m_playback_dac_capture.open("mu4_headphone.frames",std::ios::binary|std::ios::trunc);
			if (!m_playback_dac_capture) fatalerror("MU4 full DAC observation could not be opened");
		}
		m_playback_parent = {}; m_playback_parent_mismatch = 0;
		m_playback_switch_samples = 0;
		m_playback_enqueue_samples = 0;
		m_playback_callback_samples = 0;
		m_playback_notify_samples = 0;
		m_playback_path_pc = {}; m_playback_path_word = {}; m_playback_path_next = 0; m_playback_path_count = 0;
		m_playback_method_entries = {}; m_playback_method_mismatch = 0;
		m_playback_processing = {}; m_playback_processing_sp = m_playback_processing_destination = 0;
		m_playback_fft_base = m_playback_fft_parameter = 0; m_playback_fft_invocation = 0;
		m_playback_post_base = 0;
		m_playback_pre_base = m_playback_pre_parameter = 0;
		m_playback_window_base = m_playback_window_parameter = 0;
		m_playback_scale_base = m_playback_scale_parameter = 0;
		m_playback_callback_base = m_playback_callback_history = m_playback_callback_mode = 0;
		m_playback_callback_target = m_playback_callback_site = 0;
		m_playback_tail_base = m_playback_tail_history = m_playback_tail_saved = 0; m_playback_tail_ordinal = 0;
		m_playback_conversion = {}; m_playback_conversion_raw = m_playback_conversion_shifted = 0;
		m_playback_processing_pending = m_playback_conversion_active = false;
		m_playback_conversion_pending = m_playback_conversion_shift_seen = false;
		m_playback_restore_samples = 0;
		m_playback_bit_samples = {};
		m_playback_bit_source_samples = {};
		m_playback_bit_unusual_samples = 0;
		m_playback_bit_multiply_samples = {};
		m_playback_optional_started = m_playback_optional_active = false; m_playback_bit_group = 0;
		m_playback_bit_selected = false; m_playback_bit_caller = 0;
		m_playback_activation_pc = {}; m_playback_activation_word = {}; m_playback_activation_time = 0;
		m_playback_range_counts = {}; m_playback_range_opcodes = {}; m_playback_range_changes = 0;
		m_playback_dma_min = 0xffff; m_playback_dma_max = 0; m_playback_dma_last_nonzero = 0;
		m_playback_history_pc = {}; m_playback_history_word = {}; m_playback_history_next = 0;
		m_recorder_application_calls = m_recorder_application_messages = 0;
		m_recorder_processing_waits = m_recorder_processing_returns = 0;
		m_serial_empty_reads = 0;
		m_recorder_nand_controls = m_recorder_nand_data = 0;
		m_recorder_last_nand_command = 0;
		m_command_wire_clock = false; m_command_tx_words.clear();
		m_command_ack_cursor = m_command_wire_bit_count = 0;
		m_command_ack_pending = false; m_command_ack_token = m_command_wire_byte = 0; m_command_wire_decoded.clear();
		m_native_settings_call_traces = 0;
		m_native_metadata_call_traces = 0;
		m_native_allocation_call_traces = 0;
		m_native_cache_update_traces = 0;
		std::fill(std::begin(m_native_cache_mode_traces), std::end(m_native_cache_mode_traces), 0);
		m_native_settings_buffer_active = false;
		std::fill(std::begin(m_native_runtime_reads), std::end(m_native_runtime_reads), 0);
		std::fill(std::begin(m_native_runtime_writes), std::end(m_native_runtime_writes), 0);
		std::fill(std::begin(m_native_buffer_calls), std::end(m_native_buffer_calls), 0);
		std::fill(std::begin(m_native_buffer_returns), std::end(m_native_buffer_returns), 0);
		m_native_worker_window_started = false;
		m_native_tx_words.clear();
		m_external_words.clear(); m_external_bits.clear();
		auto &program = m_cpu->space(AS_PROGRAM);
		// Synthetic wrapper initializes the routine ABI, then calls untouched code.
		u16 const wrapper[] = {
			0x7718, 0x1200, // STM #1200,SP.
			0xf7be,         // SSBX CPL: stack-relative direct operands.
			0x76f8, 0x003c, 7, 0x76f8, 0x003d, 7,
			0xf6b8, 0xf020, u16(m_phase ? 0xffff : 0), // Unsigned logical row; 3035 adds one.
			0xf980, 0x3035, 0xf980, 0x306c, 0xf4e1
		};
		for (unsigned i = 0; i < std::size(wrapper); ++i) program.write_word(0x1800 + i, wrapper[i]);
		if (m_phase == 54)
		{
			program.write_word(0x1800, 0xf4e1);
			auto &codec = *subdevice<tlv320aic23_device>("codec");
			codec.control_word_w(0x0412);
			if (codec.reg(2) != 0x12 || codec.reg(3) != 0x79) fatalerror("MU4 headphone independent volume update mismatch");
			codec.control_word_w(0x0580);
			if (codec.reg(2) != 0x180 || codec.reg(3)) fatalerror("MU4 headphone simultaneous mute mismatch");
			codec.control_word_w(0x0401); codec.control_word_w(0x0790);
			if (codec.reg(2) != 0x10 || codec.reg(3) != 0x190) fatalerror("MU4 headphone simultaneous volume copied independent control fields");
			codec.control_word_w(0x0479); codec.control_word_w(0x0679);
			logerror("mu4_codec_headphone_controls: PASS independent=1 simultaneous=1 mute=1 independent_fields_preserved=1 analog_conversion=0\n");
			for (u16 word : { 0x0c10, 0x0818, 0x0a01, 0x0e53, 0x1023 }) codec.control_word_w(word);
			m_codec_edges.clear(); m_codec_frames.clear(); m_codec_clocks = 0;
			m_codec_din_words.clear();
			// Codec reset owns idle-low BCLK/FS. Do not override those pins with
			// the inactive levels used by the independent receive waveform fixture.
			external_reg_w(0, 0); external_reg_w(2, 0x140); external_reg_w(3, 0x44);
			external_reg_w(14, 0x0e); external_reg_w(0, 1);
			external_reg_w(4, 0x140); external_reg_w(5, 0x44); external_reg_w(1, 1);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000, 0x1357); data.write_word(0x6001, 0x9bdf);
			data.write_word(0x6400, 0xffff); data.write_word(0x6401, 0xffff);
			dma_reg_w(0xf, 0x6000); dma_reg_w(0x10, 0x23); dma_reg_w(0x11, 1); dma_reg_w(0x12, 0x2000); dma_reg_w(0x13, 0xc541);
			dma_reg_w(0x32, 0x6000); dma_reg_w(0x33, 0x23); dma_reg_w(0x34, 1); dma_reg_w(0x35, 0x2000);
			dma_reg_w(0xa, 0x21); dma_reg_w(0xb, 0x6400); dma_reg_w(0xc, 1); dma_reg_w(0xd, 0x1000); dma_reg_w(0xe, 0xc055);
			dma_reg_w(0x2e, 0x21); dma_reg_w(0x2f, 0x6400); dma_reg_w(0x30, 1); dma_reg_w(0x31, 0x1000);
			dma_reg_w(0x20, 1); dma_reg_w(0x22, 0xffff);
			m_dma->write(0, 0x4c);
		}
		if (m_phase == 58 || m_phase == 59)
		{
			program.write_word(0x1800, 0xf4e1);
			receive_setup(0x0140, 4);
			if (m_phase == 59)
			{
				m_mcbsp0->rx_frame_w(1);
				for (int bit = 15; bit >= 9; --bit) { receive_bit(BIT(0xa55a, bit)); m_mcbsp0->rx_frame_w(0); }
			}
		}
		if (m_phase == 31)
		{
			program.write_word(0x1800, 0xf4e1);
			mcbsp_reg_w(4, 0x0040); mcbsp_reg_w(5, 0);
			mcbsp_reg_w(6, 12); mcbsp_reg_w(7, 0x2000);
			mcbsp_reg_w(14, 0); // External clock absent: a queued word must remain busy.
			mcbsp_reg_w(1, 0x0041);
			if ((mcbsp_status() & 7) != 3) fatalerror("MU4 McBSP reset-release readiness mismatch");
			m_mcbsp->data_w(3, 0xa55a);
			if (mcbsp_status() & 2) fatalerror("MU4 McBSP DXR write did not clear readiness");
		}
		if (m_phase == 38 || m_phase == 44)
		{
			program.write_word(0x1800, 0xf4e1);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000, 0x1234); data.write_word(0x6040, 0x5678);
			data.write_word(0x6001, 0xabcd); data.write_word(0x6041, 0x9abc);
			dma_reg_w(0x0f, 0x6000); dma_reg_w(0x10, 0x6200); dma_reg_w(0x11, 1);
			dma_reg_w(0x12, 0x2001); dma_reg_w(0x13, m_phase == 44 ? 0x6541 : 0xc541);
			dma_reg_w(0x20, 0x40); dma_reg_w(0x22, 0xffc1);
			dma_reg_w(0x32, 0x6000); dma_reg_w(0x33, 0x6200);
			dma_reg_w(0x34, 1); dma_reg_w(0x35, 0x2001);
			m_dma->write(0, m_phase == 44 ? 0x48 : 8);
			if (m_phase == 44)
			{
				mcbsp_reg_w(1, 1); // XINT1 must be suppressed while INTOSEL selects DMA3.
			}
		}
		if (m_phase == 49)
		{
			program.write_word(0x1800, 0xf4e1);
			m_cpu->space(AS_DATA).write_word(0x6000, 0x1234);
			m_cpu->space(AS_DATA).write_word(0x6001, 0xabcd);
			dma_reg_w(0xf, 0x6000); dma_reg_w(0x10, 0x6200);
			dma_reg_w(0x11, 0); dma_reg_w(0x12, 0x2000); dma_reg_w(0x13, 0x4141);
			m_dma->sync_w(2, ASSERT_LINE);
			m_dma->write(0, 8);
			m_dma->sync_w(2, ASSERT_LINE); // Held level is not a second event.
		}
		if (m_phase == 51)
		{
			program.write_word(0x1800, 0xf4e1);
			external_reg_w(4, 0x0140); external_reg_w(5, 0x0044);
			external_reg_w(14, 0x000e); external_reg_w(1, 1);
			m_mcbsp0->tx_frame_w(1); m_mcbsp0->tx_clock_w(1);
			m_mcbsp0->data_w(3, 0xa55a);
		}
		if (m_phase >= 25 && m_phase <= 29)
		{
			m_dma_observed_reads = 0;
			m_dma_observed_writes = 0;
			program.write_word(0x1809, 0xf4e1);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000, 0x1234); data.write_word(0x6001, 0xabcd);
			program.write_word(0x2ffff, 0xffff); program.write_word(0x20000, 0xffff);
			data.write_word(0x6100, 0xffff);
			data.write_word(0x55, 0);
			data.write_word(0x56, 0x6000);
			data.write_word(0x56, m_phase == 25 ? 0xffff : 0x6100);
			data.write_word(0x56, m_phase == 25 ? 1 : 0);
			data.write_word(0x56, 0);
			data.write_word(0x56, m_phase == 25 ? 0x0144 : 0x0145);
			if (data.read_word(0x55) != 5) fatalerror("MU4 DMA autoincrement mismatch");
			data.write_word(0x55, 0x1e); data.write_word(0x56, 0); data.write_word(0x56, 0xff82);
			data.write_word(0x55, 0x1f);
			if (data.read_word(0x57) != 2 || data.read_word(0x55) != 0x1f) fatalerror("MU4 DMA page/nonincrement mismatch");
			if (m_dma->source_matches(0, AS_DATA, 0x6000)) fatalerror("MU4 disabled DMA source inspection matched");
			if (m_dma->inspect_register(0) != 0x6000 || m_dma->inspect_register(0x1f) != 2 ||
				m_dma->inspect_register(0x28) || m_dma->inspect_register(0x3e) || m_dma->inspect_register(0x10000) ||
				data.read_word(0x55) != 0x1f)
				fatalerror("MU4 indexed DMA inspection changed index or accepted an out-of-range register");
			data.write_word(0x54, 1);
			if (!m_dma->source_matches(0, AS_DATA, 0x6000) || m_dma->source_matches(0, AS_DATA, 0x6001) ||
				m_dma->source_matches(0, AS_PROGRAM, 0x6000) || m_dma->source_matches(6, AS_DATA, 0x6000) ||
				data.read_word(0x55) != 0x1f)
				fatalerror("MU4 DMA source inspection changed index or matched wrong channel/address/space");
			if (m_dma->source_read_matches(0, AS_DATA, 0x6000) || data.read_word(0x6000) != 0x1234 || m_dma_observed_reads)
				fatalerror("MU4 enabled DMA source falsely attributed an ordinary read");
			if (m_dma->destination_write_matches(0, AS_DATA, 0x6100) || m_dma_observed_writes)
				fatalerror("MU4 DMA destination falsely attributed ordinary fixture writes");
			if (!(data.read_word(0x54) & 1) || data.read_word(0x6100) != 0xffff || program.read_word(0x2ffff) != 0xffff)
				fatalerror("MU4 DMA completed synchronously");
			if (m_phase == 27) data.write_word(0x54, 0);
		}
		if (m_phase == 4)
		{
			// Flush is called with NAND already selected by the storage reader.
			program.write_word(0x1808, 3);
			// Routine ABI fixture: dirty cache contents and its physical row base.
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x3b12, 0); data.write_word(0x3b13, 64);
			data.write_word(0x3b14, 1);
			for (unsigned i = 0; i < 8192; ++i) data.write_word(0x4000 + i, pattern(i));
			program.write_word(0x1809, 0xf980); program.write_word(0x180a, 0x0725);
			program.write_word(0x180b, 0xf4e1);
		}
		if (m_phase == 6 || m_phase == 19 || m_phase == 21 || m_phase == 24)
		{
			if (m_phase == 19 || m_phase == 21 || m_phase == 24)
			{
				program.install_rom(0x20ae, 0x3679, reinterpret_cast<u16 *>(memregion("library")->base()));
				// Remove InitDisk-only ROM mappings before the consumer DMA writes these addresses.
				program.install_ram(0x367a, 0x4abe, &m_program_ram[0x367a]);
			}
			// Original startup's mount ABI: filesystem context and partition-aware flag.
			u16 const mount[] = {0x7600, 1, 0xf020, 0x3aea, 0xf980, 0x308e, 0xf4e1};
			for (unsigned i = 0; i < std::size(mount); ++i) program.write_word(0x1809 + i, mount[i]);
			if (m_phase == 21 || m_phase == 24)
			{
				// Same directory ABI as startup 0960..096b; no descriptor/file-state injection.
				u16 const directory[] = {0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x09c6,
					0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x09fa, 0xf4e1};
				for (unsigned i = 0; i < std::size(directory); ++i) program.write_word(0x180f + i, directory[i]);
				if (m_phase == 24)
				{
					// Loader-safe stack matches original 3538; isolate 2ed4 before its program transfer.
					u16 const load[] = {0x7718, 0x3aea, 0xe800, 0xf980, 0x2ed4, 0xf4e1};
					for (unsigned i = 0; i < std::size(load); ++i) program.write_word(0x181b + i, load[i]);
				}
			}
		}
		if (m_phase == 9)
		{
			u16 const format[] = {0xf020, 0x3aea, 0xf980, 0x051d, 0xf4e1};
			for (unsigned i = 0; i < std::size(format); ++i) program.write_word(0x1809 + i, format[i]);
		}
		if (m_phase == 22)
		{
			unsigned cursor = 0x1809;
			if (!m_file_cursor)
			{
				u16 const seek[] = {0x7600, 0, 0x7601, 0, 0x7602, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x33ea};
				for (u16 word : seek) program.write_word(cursor++, word);
			}
			m_read_words = std::min(256U, (m_file_size - m_file_cursor) / 2);
			u16 const read[] = {0x7600, 0x6000, 0x7601, u16(m_read_words), 0x7602, 0x3aea,
				0xf020, 0x36b0, 0xf980, 0x32d3, 0xf4e1};
			for (u16 word : read) program.write_word(cursor++, word);
		}
		if (m_phase == 23)
		{
			u16 const next[] = {0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x0a0b, 0xf4e1};
			for (unsigned i = 0; i < std::size(next); ++i) program.write_word(0x1809 + i, next[i]);
		}
		if (m_phase == 7 || m_phase == 19 || m_phase == 21 || m_phase == 24)
		{
			// Original C globals are routine inputs, not an invented disk image.
			auto &data = m_cpu->space(AS_DATA);
			unsigned cursor = 0;
			while (cursor < m_cinit.length())
			{
				unsigned const count = m_cinit[cursor++];
				if (!count) break;
				if (cursor + count + 1 > m_cinit.length()) fatalerror("MU4 fixture truncated cinit");
				unsigned const destination = m_cinit[cursor++];
				if (destination + count > 0x10000) fatalerror("MU4 fixture wrapping cinit");
				for (unsigned i = 0; i < count; ++i) data.write_word(destination + i, m_cinit[cursor++]);
			}
			if (cursor != m_cinit.length()) fatalerror("MU4 fixture incomplete cinit");
			if (m_phase == 7)
			{
				u16 const recovery[] = {0xf020, 0x3aea, 0xf980, 0x0880, 0xf4e1};
				for (unsigned i = 0; i < std::size(recovery); ++i) program.write_word(0x1809 + i, recovery[i]);
			}
		}
		if (m_phase == 10 || m_phase == 12 || m_phase == 13 || m_phase == 14 || m_phase == 18)
		{
			// Replace the routine library with unchanged InitDisk code, not a flat overlay image.
			program.install_rom(0x256d, 0x4aa8, &m_initdisk[0]);
			program.install_rom(0x4aa9, 0x4abe, &m_disk_helpers[0]);
			program.install_rom(0x0080, 0x00f7, &m_disk_vectors[0]);
			auto &data = m_cpu->space(AS_DATA);
			if (m_phase == 18)
			{
				// Independent factory-erased input, not media left by earlier routine tests.
				m_nand->nvram_reset();
				for (unsigned address = 0x80; address < 0x10000; ++address) data.write_word(address, 0);
			}
			unsigned cursor = 0;
			while (cursor < m_disk_cinit.length())
			{
				unsigned const count = m_disk_cinit[cursor++];
				if (!count) break;
				if (cursor + count + 1 > m_disk_cinit.length()) fatalerror("MU4 InitDisk truncated cinit");
				unsigned const destination = m_disk_cinit[cursor++];
				if (destination + count > 0x10000) fatalerror("MU4 InitDisk wrapping cinit");
				for (unsigned i = 0; i < count; ++i) data.write_word(destination + i, m_disk_cinit[cursor++]);
			}
			if (cursor != m_disk_cinit.length()) fatalerror("MU4 InitDisk incomplete cinit");
			// Original ABI: block zero in A, original NAND geometry context on the stack.
			u16 const marker[] = {0xf074, 0x377a, 0x7600, 0x0585, 0xf020, 0, 0xf074, 0x2889, 0xf4e1};
			for (unsigned i = 0; i < std::size(marker); ++i) program.write_word(0x1809 + i, marker[i]);
			if (m_phase == 13)
			{
				u16 const scan[] = {0xf074, 0x377a, 0x7600, 0x2000, 0x7601, 0x2100,
					0xf020, 0x068c, 0xf074, 0x36b9, 0xf4e1};
				for (unsigned i = 0; i < std::size(scan); ++i) program.write_word(0x1809 + i, scan[i]);
			}
			if (m_phase == 14)
			{
				u16 const receive[] = {0xf074, 0x346a, 0x771d, 0x00e8, 0x7700, 0x0040,
					0xf6bb, 0xf4e1, 0xf073, 0x1810};
				for (unsigned i = 0; i < std::size(receive); ++i) program.write_word(0x1809 + i, receive[i]);
			}
			if (m_phase == 18)
			{
				u16 const initialize[] = {0xf074, 0x346a, 0x771d, 0x00e8,
					0xf020, 0x057e, 0xf074, 0x30de, 0xf4e1};
				for (unsigned i = 0; i < std::size(initialize); ++i) program.write_word(0x1809 + i, initialize[i]);
			}
		}
		if (m_phase == 17)
		{
			u16 const consume[] = {0xf074, 0x34c0, 0xf4e1};
			for (unsigned i = 0; i < std::size(consume); ++i) program.write_word(0x1809 + i, consume[i]);
		}
		// Disable the unrelated core timer so it cannot wake the completion IDLE.
		program.write_word(0x17fe, 0x7726); program.write_word(0x17ff, 0x0010);
		program.write_word(0xff80, 0xf073); program.write_word(0xff81, 0x17fe);
		// These isolated routine fixtures bypass the original clock initializer.
		// Their instruction-budget deadlines were established at 13 MHz; retain
		// that budget at the selected reset clock without changing NAND latency.
		unsigned const budget_ms = m_phase == 24 ? 8000 : m_phase == 18 ? 60000 : m_phase == 13 ? 8000 :
			(m_phase == 7 || m_phase == 14) ? 500 : m_phase == 4 ? 250 : m_phase >= 6 ? 50 : 1;
		m_check->adjust(attotime::from_ticks(m_phase >= 25 ? 13 : u64(budget_ms) * 13'000, m_cpu->clock()));
		if (m_phase == 74)
		{
			// Original serial-boot entry handoff, not mask-ROM or DA150 reset wiring.
			// Let the original startup execute its C table and choose its NAND file.
			auto word = [this](unsigned offset) -> u16
			{
				if (offset + 1 >= m_segment.length()) fatalerror("MU4 bootstrap upload bounds");
				return (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
			};
			if (word(0) != 0xaa55 || word(6) != 0x08aa || word(16) || word(18) != 0x0e41)
				fatalerror("MU4 original bootstrap header changed");
			unsigned const end = 6 + (unsigned(word(2)) << 16) + word(4);
			unsigned cursor = 20, records = 0, uploaded = 0;
			while (word(cursor))
			{
				unsigned const count = word(cursor);
				u32 const destination = (u32(word(cursor + 2)) << 16) | word(cursor + 4);
				cursor += 6;
				if (destination + count > 0x10000 || cursor + count * 2 > end)
					fatalerror("MU4 original bootstrap upload range changed");
				for (unsigned i = 0; i < count; ++i) program.write_word(destination + i, word(cursor + i * 2));
				cursor += count * 2;
				uploaded += count;
				++records;
			}
			if (cursor + 2 != end || records != 7 || uploaded != 10760)
				fatalerror("MU4 original bootstrap upload coverage changed records=%u words=%u", records, uploaded);
			logerror("mu4_original_bootstrap_upload: records=%u words=%u program_only=1 data_alias=unvalidated\n", records, uploaded);
			if (staged_original_profile())
			{
				// Fail-closed coverage diagnostic from the original serial entry.
				// No atomic fallback and no firmware-PC-conditioned switch.
				m_cpu->set_pipeline_frontend(true);
				logerror("mu4_staged_original: enabled=1 entry=000e41 atomic_fallback=0 acceptance=0\n");
			}
			m_phase = 30;
			if (system_bios() >= 17) m_command->adjust(attotime::from_msec(1));
			m_check->adjust(attotime::from_seconds(20));
			logerror("mu4_original_bootstrap_start: reset_vector=00ff80 serial_entry=000e41 retained_nand=1 routine_wrapper=0 mask_rom=0\n");
		}
	}
	void start_media_read()
	{
		m_nand->command_w(0);
		m_nand->address_w(0); m_nand->address_w(64 + m_page);
		m_nand->address_w(0); m_nand->address_w(0);
		m_check->adjust(attotime::from_usec(11));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		if (m_phase >= 119 && m_phase <= 121)
		{
			auto &codec = *subdevice<tlv320aic23_device>("codec");
			if (m_phase == 119 || m_phase == 120)
			{
				if (u16(codec.dac_sample(0)) != 0xeca9 || u16(codec.dac_sample(1)) != 0x6421 || codec.headphone_volume(0) || codec.headphone_volume(1))
					fatalerror("MU4 headphone zero-cross sample/mute mismatch");
				check_headphone_level(0,0); check_headphone_level(1,0);
				if (m_phase == 119)
				{
					m_saved_dma.clear(); m_saved_dma.seekg(0);
					if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 pending headphone gain restore failed");
					if (u16(codec.dac_sample(0)) != 0x1357 || codec.headphone_volume(0) != 0x73 || codec.headphone_volume(1))
						fatalerror("MU4 pending headphone gain/sample state was not restored");
					m_phase = 120;
					m_check->adjust(m_codec_snapshot_time + attotime::from_usec(50) - machine().time()); return;
				}
				codec.control_word_w(0x0e73); codec.control_word_w(0x0579);
				m_phase = 121; m_check->adjust(attotime::from_usec(50)); return;
			}
			if (u16(codec.dac_sample(0)) != 0x6421 || u16(codec.dac_sample(1)) != 0xeca9)
				fatalerror("MU4 codec DAC LRSWAP mismatch");
			check_headphone_level(0,double(s16(0x6421))/32768.0);
			check_headphone_level(1,double(s16(0xeca9))/32768.0);
			codec.control_word_w(0x1200); codec.control_word_w(0x1e00);
			if (codec.reg(7) != 1 || codec.reg(9)) fatalerror("MU4 codec reset defaults mismatch");
			check_headphone_level(0,0); check_headphone_level(1,0);
			logerror("mu4_codec_dac: PASS stereo_latch=1 power=1 digital_mute=1 selected_route=1 nominal_gain=1 zero_cross_pending=1 pending_restore=1 lrswap=1 ideal_headphone_only=1 analog_filter=0\n");
			logerror("mu4_codec_clock: PASS inactive=1 controls=1 bclk_mclk=1 frame_divider=272 pending_restore=1 reset=1\n");
			logerror("mu4_codec_duplex: PASS post_clock_pins=1 din_stereo=1 dout_converted_fixture=1 dma=2,3 mid_word_restore=1\n");
			m_phase = 58; machine().schedule_soft_reset(); return;
		}
		if (original_bootstrap_profile())
		{
			if (playback_profile()) finish_playback_capture();
			if (recorder_ac_profile())
			{
				logerror("mu4_recorder_ac_input: generated=%u,%u,%u,%u received=%u,%u,%u,%u other=%u other_owner_reads=%u shape=square periods=64,32 amplitudes=8192,4096 ownership=actual_channel2_source_read analog_adc=0 decoded_audio=0\n",
					m_ac_generated[0], m_ac_generated[1], m_ac_generated[2], m_ac_generated[3],
					m_ac_received[0], m_ac_received[1], m_ac_received[2], m_ac_received[3], m_ac_other, m_ac_other_owners);
				for (unsigned mask=0;mask<m_ac_read_owners.size();++mask)
					if (m_ac_read_owners[mask]) logerror("mu4_recorder_read_owner: dma_mask=%02x reads=%llu observer_writes=0\n",mask,static_cast<unsigned long long>(m_ac_read_owners[mask]));
				for (auto const &[pc,count] : m_ac_unowned_contexts)
					logerror("mu4_recorder_read_context: context_pc=%06x reads=%llu observer_writes=0\n",pc,static_cast<unsigned long long>(count));
				for (unsigned slot=0;slot<18;++slot)
					if (m_ac_dma_writes[slot]) logerror("mu4_recorder_destination: channel=%u space=%u writes=%llu minimum=%06x maximum=%06x observer_writes=0\n",
						slot/3,slot%3,static_cast<unsigned long long>(m_ac_dma_writes[slot]),m_ac_dma_first[slot],m_ac_dma_last[slot]);
				logerror("mu4_recorder_destination_terminal: bad=%llu pending=%u save_restore=unsupported observer_writes=0 encoder_consumption=0\n",
					static_cast<unsigned long long>(m_ac_dma_bad),unsigned(std::count(m_ac_dma_pending.begin(),m_ac_dma_pending.end(),true)));
				for (unsigned bank=0;bank<2;++bank)
				{
					unsigned pending = 0;
					for (unsigned slot=bank*256;slot<(bank+1)*256;++slot)
						if (m_ac_buffer_valid[slot] && !m_ac_buffer_read[slot]) ++pending;
					logerror("mu4_recorder_buffer: bank=%u writes=%llu read_epochs=%llu retired_unread=%llu pending_unread=%u unknown_reads=%llu observer_writes=0\n",
						bank,static_cast<unsigned long long>(m_ac_buffer_writes[bank]),static_cast<unsigned long long>(m_ac_buffer_consumed[bank]),
						static_cast<unsigned long long>(m_ac_buffer_unread[bank]),pending,static_cast<unsigned long long>(m_ac_buffer_unknown[bank]));
					for (unsigned mask=0;mask<64;++mask)
						if (m_ac_buffer_readers[bank*64+mask]) logerror("mu4_recorder_buffer_reader: bank=%u dma_mask=%02x reads=%llu observer_writes=0\n",
							bank,mask,static_cast<unsigned long long>(m_ac_buffer_readers[bank*64+mask]));
				}
				for (auto const &[key,count] : m_ac_buffer_contexts)
					logerror("mu4_recorder_buffer_context: bank=%u context_pc=%06x reads=%llu observer_writes=0\n",key>>24,key&0xffffff,static_cast<unsigned long long>(count));
				logerror("mu4_recorder_buffer_terminal: bad=%llu dropped=%llu context_cap=256 context_pc=not_instruction_owner save_restore=unsupported observer_writes=0 encoder_consumption=0\n",
					static_cast<unsigned long long>(m_ac_buffer_bad),static_cast<unsigned long long>(m_ac_buffer_dropped));
				for (auto const &[key,count] : m_ac_buffer_instructions)
					logerror("mu4_recorder_buffer_instruction: bank=%u instruction=%06x opcode=%04x reads=%llu observer_writes=0\n",
						unsigned(key>>48),unsigned((key>>16)&0x7fffff),unsigned(key&0xffff),static_cast<unsigned long long>(count));
				logerror("mu4_recorder_buffer_identity_terminal: missing=%llu dropped=%llu cap=256 identity=staged_operand_bus_read save_restore=unsupported observer_writes=0 retirement=0\n",
					static_cast<unsigned long long>(m_ac_buffer_identity_missing),static_cast<unsigned long long>(m_ac_buffer_identity_dropped));
				for (auto const &[key,row] : m_ac_copy_destinations)
					logerror("mu4_recorder_copy_destination: bank=%u instruction=%06x opcode=%04x known_writes=%llu unknown_writes=%llu source_min=%04x source_max=%04x destination_min=%04x destination_max=%04x observer_writes=0\n",
						unsigned(key>>48),unsigned((key>>16)&0x7fffff),unsigned(key&0xffff),static_cast<unsigned long long>(row[1]),static_cast<unsigned long long>(row[0]),unsigned(row[2]),unsigned(row[3]),unsigned(row[4]),unsigned(row[5]));
				logerror("mu4_recorder_copy_terminal: known_reads=%llu unknown_reads=%llu writes=%llu pending=%u bad=%llu dropped=%llu pending_cap=64 site_cap=256 identity=staged_read_write save_restore=unsupported observer_writes=0 encoder_consumption=0\n",
					static_cast<unsigned long long>(m_ac_copy_reads[1]),static_cast<unsigned long long>(m_ac_copy_reads[0]),static_cast<unsigned long long>(m_ac_copy_writes),unsigned(m_ac_copy_pending.size()),static_cast<unsigned long long>(m_ac_copy_bad),static_cast<unsigned long long>(m_ac_copy_dropped));
				for (unsigned slot=0;slot<128;++slot)
					if (m_ac_workspace_readers[slot]) logerror("mu4_recorder_workspace_reader: program_alias=%u dma_mask=%02x reads=%llu observer_writes=0\n",slot/64,slot%64,static_cast<unsigned long long>(m_ac_workspace_readers[slot]));
				for (auto const &[key,count] : m_ac_workspace_instructions)
					logerror("mu4_recorder_workspace_instruction: instruction=%06x opcode=%04x reads=%llu observer_writes=0\n",unsigned(key>>16),unsigned(key&0xffff),static_cast<unsigned long long>(count));
				unsigned workspace_pending = 0;
				for (auto const &word : m_ac_workspace) if (word.valid && !word.read) ++workspace_pending;
				logerror("mu4_recorder_workspace_terminal: origin_writes=%llu total_writes=%llu alias_writes=%llu read_epochs=%llu retired_unread=%llu pending_unread=%u known_reads=%llu unknown_reads=%llu bad=%llu dropped=%llu missing_data_identity=%llu cap=256 save_restore=unsupported observer_writes=0 encoder_consumption=0\n",
					static_cast<unsigned long long>(m_ac_workspace_origins),static_cast<unsigned long long>(m_ac_workspace_writes),static_cast<unsigned long long>(m_ac_workspace_alias_writes),static_cast<unsigned long long>(m_ac_workspace_epochs_read),static_cast<unsigned long long>(m_ac_workspace_retired),workspace_pending,
					static_cast<unsigned long long>(m_ac_workspace_known),static_cast<unsigned long long>(m_ac_workspace_unknown),static_cast<unsigned long long>(m_ac_workspace_bad),static_cast<unsigned long long>(m_ac_workspace_dropped),static_cast<unsigned long long>(m_ac_workspace_missing));
				logerror("mu4_recorder_ingress_terminal: frames=%u entries=%u loads=%llu,%llu stores=%llu,%llu known=%llu,%llu pending_reads=%u pending_write=%u pending_call=%u partial=%u,%u bad=%llu dropped=%llu frame_cap=512 queue_cap=4 save_restore=unsupported observer_writes=0 encoder_consumption=0\n",
					m_ac_ingress_frames,m_ac_ingress_entries,static_cast<unsigned long long>(m_ac_ingress_loads[0]),static_cast<unsigned long long>(m_ac_ingress_loads[1]),static_cast<unsigned long long>(m_ac_ingress_stores[0]),static_cast<unsigned long long>(m_ac_ingress_stores[1]),static_cast<unsigned long long>(m_ac_ingress_known[0]),static_cast<unsigned long long>(m_ac_ingress_known[1]),unsigned(m_ac_ingress_reads[0].size()+m_ac_ingress_reads[1].size()),unsigned(m_ac_ingress_store.valid),unsigned(m_ac_ingress_call),m_ac_ingress_frame_stores[0],m_ac_ingress_frame_stores[1],static_cast<unsigned long long>(m_ac_ingress_bad),static_cast<unsigned long long>(m_ac_ingress_dropped));
				for (unsigned slot=0;slot<128;++slot)
					if (m_ac_consumer_readers[slot]) logerror("mu4_recorder_consumer_reader: program_alias=%u dma_mask=%02x reads=%llu observer_writes=0\n",slot/64,slot%64,static_cast<unsigned long long>(m_ac_consumer_readers[slot]));
				for (auto const &[key,count] : m_ac_consumer_instructions)
					logerror("mu4_recorder_consumer_instruction: instruction=%06x opcode=%04x reads=%llu observer_writes=0\n",unsigned(key>>16),unsigned(key&0xffff),static_cast<unsigned long long>(count));
				auto const &consumer = m_ac_consumer_total;
				logerror("mu4_recorder_consumer_terminal: entries=%u returns=%u origins=%llu known_reads=%llu read_epochs=%llu retired_unread=%llu unread_return=%llu unknown_reads=%llu writes=%llu alias_writes=%llu missing_data_identity=%llu bad=%llu dropped=%llu active=%u cap=256 frame_cap=512 save_restore=unsupported observer_writes=0 encoder_consumption=0\n",
					m_ac_consumer_entries,m_ac_consumer_returns,static_cast<unsigned long long>(consumer.origins),static_cast<unsigned long long>(consumer.known),static_cast<unsigned long long>(consumer.epochs),static_cast<unsigned long long>(consumer.retired),static_cast<unsigned long long>(consumer.unread),static_cast<unsigned long long>(consumer.unknown),static_cast<unsigned long long>(consumer.writes),static_cast<unsigned long long>(consumer.alias_writes),static_cast<unsigned long long>(consumer.missing),static_cast<unsigned long long>(m_ac_consumer_bad),static_cast<unsigned long long>(m_ac_consumer_dropped),unsigned(m_ac_consumer_active));
				m_ac_pcm_capture.flush();
				if (!m_ac_pcm_capture) fatalerror("MU4 recorder input capture write failed");
				m_ac_pcm_capture.close();
				logerror("mu4_recorder_input_pcm: file=mu4_recorder_input.pcm words=%llu fnv1a=%08x bad_order=%llu dropped=%llu cap=1048576 channels=2 format=s16le_interleaved source=staged_operand_read save_restore=unsupported observer_writes=0 audio_acceptance=0\n",
					static_cast<unsigned long long>(m_ac_pcm_words),m_ac_pcm_hash,static_cast<unsigned long long>(m_ac_pcm_bad_order),static_cast<unsigned long long>(m_ac_pcm_dropped));
				for (auto const &[caller,count] : m_record_transform_callers)
					logerror("mu4_record_full_caller: caller=%06x calls=%llu observer_writes=0\n",caller,static_cast<unsigned long long>(count));
				logerror("mu4_record_full_terminal: entries=%llu returns=%llu unadmitted=%llu bad=%llu dropped=%llu active=%u pending_write=%u cap=8192 caller_cap=128 stop=%.9f save_restore=unsupported observer_writes=0 audio_acceptance=0\n",
					static_cast<unsigned long long>(m_record_transform_entries),static_cast<unsigned long long>(m_record_transform_returns),static_cast<unsigned long long>(m_record_transform_unadmitted),static_cast<unsigned long long>(m_record_transform_bad),static_cast<unsigned long long>(m_record_transform_dropped),unsigned(m_record_transform.active),unsigned(m_record_transform_store.valid),machine().time().as_double());
				logerror("mu4_recorder_read_terminal: reads=%llu contexts=%u dropped=%llu context_cap=128 t=%.9f save_restore=unsupported observer_writes=0\n",
					static_cast<unsigned long long>(m_ac_read_total),unsigned(m_ac_unowned_contexts.size()),static_cast<unsigned long long>(m_ac_context_dropped),machine().time().as_double());
				for (unsigned mask=0;mask<64;++mask)
					if (m_ac_rx_fresh[mask] || m_ac_rx_stale[mask]) logerror("mu4_recorder_rx_owner: dma_mask=%02x fresh=%llu stale=%llu observer_writes=0\n",
						mask,static_cast<unsigned long long>(m_ac_rx_fresh[mask]),static_cast<unsigned long long>(m_ac_rx_stale[mask]));
				logerror("mu4_recorder_rx_terminal: publications=%llu discarded=%llu pending=%u resets=%llu bad=%llu unknown_stale=%llu save_restore=unsupported observer_writes=0 encoder_consumption=0\n",
					static_cast<unsigned long long>(m_ac_rx_publications),static_cast<unsigned long long>(m_ac_rx_discarded),unsigned(m_ac_rx_pending),
					static_cast<unsigned long long>(m_ac_rx_resets),static_cast<unsigned long long>(m_ac_rx_bad),static_cast<unsigned long long>(m_ac_rx_unknown_stale));
			}
			if (playback_profile())
			{
				logerror("mu4_native_playback_execution: bootstrap_fetch=%u bootstrap_execute=%u resident_fetch=%u resident_execute=%u filter_calls=%u filter_iterations=%u filter_returns=%u instruction_entries=%llu loader_execute=%u observer=Execute_entry retirement_claim=0\n", m_native_bootstrap_entries, m_playback_execute_entries[0], m_native_bootstrap_resident_entries, m_playback_execute_entries[1], m_playback_filter[0], m_playback_filter[1], m_playback_filter[2], static_cast<unsigned long long>(m_playback_execute_steps), m_playback_execute_entries[2]);
				auto const &codec = *subdevice<tlv320aic23_device>("codec");
				logerror("mu4_native_playback_audio: tx_words=%u din_words=%u tx_nonzero=%u din_nonzero=%u tx_hash=%08x din_hash=%08x clocks=%u frames=%u power=%04x format=%04x rate=%04x active=%04x tx_last_nonzero=%.9f din_last_nonzero=%.9f window=decoder_entry stream_alignment=unvalidated music_decode=0 analog_audio=0\n", m_playback_audio_words[0], m_playback_audio_words[1], m_playback_audio_nonzero[0], m_playback_audio_nonzero[1], m_playback_audio_hash[0], m_playback_audio_hash[1], m_playback_audio_clocks, m_playback_audio_frames, codec.reg(6), codec.reg(7), codec.reg(8), codec.reg(9), m_playback_audio_last_nonzero[0], m_playback_audio_last_nonzero[1]);
				logerror("mu4_playback_dma_source: reads=%u nonzero=%u minimum=%04x maximum=%04x last_nonzero=%.9f channel=3 space=data ownership=active_source_read window=decoder_entry tx_pairing=unvalidated music_decode=0\n",
					m_playback_dma_reads, m_playback_dma_nonzero, m_playback_dma_min, m_playback_dma_max, m_playback_dma_last_nonzero);
				logerror("mu4_native_playback_observe: requests=%u inspections=%u decoder_loads=%u replies=%u input_cursor=%u title_verified=%u overlay_verified=%u pc=%06x illegal=%u audio_acceptance=0\n",
					m_playback_requests, m_playback_inspections, m_playback_decoder_loads, m_recorder_responses, m_native_command_cursor,
					unsigned(m_cold_title_verified), unsigned(m_playback_overlay_checked), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
				if (playback_stop_profile())
					logerror("mu4_playback_control_terminal: status_reply=%u input_cursor=%u decoded_calls=%u idle=%u pc=%06x t=%.9f firmware_state_forcing=0 audio_acceptance=0\n",
						unsigned(m_playback_status_replied), m_native_command_cursor, m_playback_processing[2],
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)), unsigned(m_cpu->state_int(STATE_GENPC)), machine().time().as_double());
			}
			if (!m_native_bootstrap_entries || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
			{
				if (staged_original_profile())
					logerror("mu4_staged_stop: pc=%06x nand_reads=%u nand_strobes=%zu ar0=%04x ar1=%04x ar2=%04x ready=%u acceptance=0\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), m_data_reads, m_writes.size(),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), m_ready);
				// CPU coverage failures must retain the media produced before the stop.
				if (recorder_profile()) capture_recorder_media();
				fatalerror("MU4 original bootstrap entry did not execute cleanly");
			}
			auto const disable = machine().disable_side_effects();
			logerror("mu4_original_bootstrap_frontier: entry_count=%u resident_count=%u pc=%06x st1=%04x imr=%04x ifr=%04x flag374d=%04x nand_reads=%u stream_words=%u tail_ms=20000 board_boot=0 music_decode=0\n",
				m_native_bootstrap_entries, m_native_bootstrap_resident_entries, unsigned(m_cpu->state_int(STATE_GENPC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), m_cpu->space(AS_DATA).read_word(0x374d), m_data_reads, m_stream_words);
			auto &data = m_cpu->space(AS_DATA);
			logerror("mu4_original_bootstrap_media_context: lifecycle=%04x capacity=%04x mode=%04x continuation=%04x tracks=%04x firmware_state_forcing=0\n",
				data.read_word(0x373e), data.read_word(0x374e), data.read_word(0x3750),
				data.read_word(0x3762), data.read_word(0x3768));
			if (recorder_profile())
			{
				unsigned const programs = std::count(m_writes.begin(), m_writes.end(), u16(0x0180));
				unsigned const confirms = std::count(m_writes.begin(), m_writes.end(), u16(0x0110));
				logerror("mu4_native_recorder_storage: program_setups=%u program_confirms=%u data_writes=%u recorded_file_verified=0\n",
					programs, confirms, m_recorder_nand_data);
				if (recorder_control_profile())
				{
					logerror("mu4_native_recorder_control_receive: cursor=%u rx_ready=%u rx_busy=%u ring_head=%04x ring_tail=%04x parser=%04x\n",
						m_native_command_cursor, m_command_rx_ready, m_command_rx_busy,
						data.read_word(0xf9de), data.read_word(0xf9df), data.read_word(0xf9e8));
				}
				logerror("mu4_native_recorder_observe: dispatches=%u loader_calls=%u responses=%u rx_words=%u tx_words=%u ack_pending=%u music_recording=0\n",
					m_recorder_dispatches, m_recorder_loader_calls, m_recorder_responses, m_command_rx_irqs,
					unsigned(m_command_tx_words.size()), m_command_ack_pending && m_command_ack_cursor != 3);
				// Preserve postmortem media/context even when a later assertion fails.
				// Serialization issues no NAND command and does not save source NVRAM.
				capture_recorder_media();
				if ((!recorder_control_profile() && m_native_command_cursor != 9) || m_recorder_dispatches != 1 || m_recorder_loader_calls != 1)
					fatalerror("MU4 original recorder command did not reach the native overlay loader");
				if (!m_recorder_overlay_checked) fatalerror("MU4 recorder overlay destinations were not verified");
				if (shared_daram_profile() && m_recorder_startup_entries != 1)
					fatalerror("MU4 shared RAM startup observation differs count=%u expected=1 ownership=execute_entry", m_recorder_startup_entries);
				if (shared_daram_profile() && !recorder_control_profile() && (m_recorder_responses != 2 || m_command_rx_irqs != 15 ||
					m_command_tx_words.size() != 30 || (m_command_ack_pending && m_command_ack_cursor != 3)))
					fatalerror("MU4 recorder startup did not complete both observed native reply handshakes");
				if (recorder_control_profile())
				{
					logerror("mu4_native_recorder_control_observe: entries=%u acks=%u responses=%u rx_words=%u tx_words=%u recorded_file_verified=0\n",
						m_recorder_control_entries, m_recorder_control_acks, m_recorder_responses, m_command_rx_irqs, unsigned(m_command_tx_words.size()));
					logerror("mu4_native_recorder_processing: application_calls=%u application_messages=%u processing_waits=%u processing_returns=%u completion=%04x\n",
						m_recorder_application_calls, m_recorder_application_messages, m_recorder_processing_waits, m_recorder_processing_returns,
						data.read_word(0xfd85));
					// Transport receipt is not proof of application dispatch or recording completion.
					if (recorder_save_profile())
					{
						auto &program = m_cpu->space(AS_PROGRAM);
						logerror("mu4_native_recorder_warm_vector: low=%04x,%04x,%04x,%04x page2=%04x,%04x,%04x,%04x\n",
							program.read_word(0x204c), program.read_word(0x204d), program.read_word(0x204e), program.read_word(0x204f),
							program.read_word(0x2204c), program.read_word(0x2204d), program.read_word(0x2204e), program.read_word(0x2204f));
						logerror("mu4_native_recorder_warm_handler: low=%04x,%04x,%04x,%04x page2=%04x,%04x,%04x,%04x pmst=%04x\n",
							program.read_word(0x3d73), program.read_word(0x3d74), program.read_word(0x3d75), program.read_word(0x3d76),
							program.read_word(0x23d73), program.read_word(0x23d74), program.read_word(0x23d75), program.read_word(0x23d76),
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
						logerror("mu4_native_recorder_warm_timer: tim=%04x prd=%04x tcr=%04x\n",
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_TIM)),
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_PRD)),
							unsigned(m_cpu->state_int(tms320c54x_device::STATE_TCR)));
						// Initial start, recorder poll/control and resident poll total 34 bytes.
						if (m_native_command_cursor != 34 + recorder_metadata_size() || m_command_rx_ready || m_command_rx_busy ||
							m_recorder_control_acks != 4 || !m_recorder_metadata_result)
							fatalerror("MU4 recorder metadata transaction did not complete natively");
						if (full_metadata_profile() && m_recorder_metadata_result != 1)
							fatalerror("MU4 nonempty metadata request did not receive native success");
						logerror("mu4_native_recorder_metadata: PASS native_reply=1 success=%u playback=0 firmware_state_forcing=0\n",
							m_recorder_metadata_result == 1);
					}
					else if (recorder_poll_profile())
					{
						if (m_native_command_cursor != 26 || m_command_rx_ready || m_command_rx_busy ||
							m_recorder_control_acks != 2 || m_recorder_responses != 3 || m_recorder_control_entries != 1 ||
							m_command_rx_irqs != 35 || m_command_tx_words.size() != 47 ||
							data.read_word(0xf9de) != 26 || data.read_word(0xf9df) != 26 ||
							data.read_word(0x373e) != 2 || data.read_word(0x3750) != 0 || data.read_word(0x3762) != 10)
							fatalerror("MU4 recorder polled control did not reach the original handler");
						logerror("mu4_native_recorder_polled_control: PASS status_poll=1 control_handler=1 recording_complete=0 firmware_state_forcing=0\n");
					}
					else if (m_native_command_cursor != 18 || m_command_rx_ready || m_command_rx_busy ||
						m_command_rx_irqs != 24 || m_recorder_responses != 2 || m_command_tx_words.size() != 33 ||
						m_recorder_control_acks != 1 || data.read_word(0xf9de) != 15 || data.read_word(0xf9df) != 15)
						fatalerror("MU4 recorder control packet did not complete native receive and acknowledgement");
					if (!recorder_poll_profile())
						logerror("mu4_native_recorder_control_receive: PASS packet_received=1 native_ack=1 recording_complete=0 firmware_state_forcing=0\n");
				}
				logerror("mu4_native_recorder_dispatch: PASS pin_input=1 original_loader=1 firmware_state_forcing=0 music_recording=0\n");
			}
			else if (measurement_profile()) verify_native_measurement();
			else if (system_bios() >= 17 && !playback_profile()) verify_native_status();
			if (playback_profile()) fatalerror("MU4 playback coverage diagnostic: audio completion is not validated");
			if (cold_saved_profile())
			{
				bool valid = m_cold_catalog_scans == 1 && m_cold_metadata_reads == 1 && m_cold_notifications &&
					m_cold_title_publications == 1 && m_cold_title_verified &&
					data.read_word(0x3768) == 1 && data.read_word(0x373e) == 2 && !data.read_word(0x3750);
				if (!valid) fatalerror("MU4 cold saved-track catalogue differs scans=%u metadata_reads=%u tracks=%u", m_cold_catalog_scans, m_cold_metadata_reads, data.read_word(0x3768));
				logerror("mu4_native_recorder_reopen: PASS tracks=1 title=MAME packed_title=1 original_scanner=1 original_id3_reader=1 pin_status=1 playback=0 firmware_state_forcing=0\n");
			}
			if (system_bios() == 20)
			{
				logerror("mu4_native_codec_continuous_observe: tx_words=%u din_words=%u pending=%u nonzero=%u\n",
					m_stream_words, m_continuous_din_words, m_continuous_tx_count, m_continuous_nonzero_words);
				if (m_continuous_din_words <= 1024 || m_continuous_tx_count > 1 ||
					m_continuous_din_words + m_continuous_tx_count != m_stream_words || !m_continuous_nonzero_words)
					fatalerror("MU4 processing did not produce continuously verified nonzero codec output");
				logerror("mu4_native_codec_continuous: PASS complete_window=1 digital_din=1 nonzero=1 music_decode=0 analog_audio=0\n");
				logerror("mu4_native_tone_dma_observe: source_reads=%u pending=%u tone_words=%u generated_blocks=%u discarded=%u\n",
					m_source_reads, m_source_count, m_tone_dma_words, m_tone_blocks, m_tone_discarded_words);
				if (m_source_reads != m_stream_words + m_source_count || m_source_count > 2 ||
					!m_tone_dma_words || m_tone_dma_words + m_tone_discarded_words != m_tone_blocks * 128)
					fatalerror("MU4 generated tone samples are neither verified through DMA nor cancelled by original cleanup");
				logerror("mu4_native_tone_dma: PASS independent_samples=1 stereo_order=1 dma_to_tx=1 tx_to_din=1 music_decode=0 analog_audio=0\n");
			}
			if (finish_native_replay()) return;
			if (system_bios() == 18)
			{
				if (m_native_reset_leg != 2 || m_native_bootstrap_entries != 2 || m_native_bootstrap_resident_entries != 2)
					fatalerror("MU4 full startup did not repeat after the mid-byte reset");
				logerror("mu4_original_bootstrap_reset: PASS mid_rx_byte=1 controller_cleared=1 original_startup_legs=2 retained_nand=1 rx_words=11 tx_words=14 firmware_state_forcing=0 board_reset=0\n");
			}
			machine().schedule_exit();
			return;
		}
		if (m_phase >= 58 && m_phase <= 65)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_phase == 58)
			{
				for (unsigned i = 0; i < 4; ++i) receive_bit(1);
				if (receive_status() & 2) fatalerror("MU4 receive shifted without a frame");
				m_mcbsp0->rx_frame_w(1);
				for (int bit = 15; bit >= 0; --bit)
				{
					if (bit == 7) m_mcbsp0->rx_frame_w(1); // RFIG must ignore this early frame.
					receive_bit(BIT(0xa55a, bit)); m_mcbsp0->rx_frame_w(0);
				}
				if (receive_status() & 2) fatalerror("MU4 receive ready before RBR handoff edge");
				for (int bit = 15; bit >= 0; --bit) receive_bit(BIT(0x5aa5, bit));
				if (!(receive_status() & 2)) fatalerror("MU4 receive first word never ready");
				{ auto const disable = machine().disable_side_effects(); if (m_mcbsp0->data_r(1) != 0xa55a) fatalerror("MU4 receive peek mismatch"); }
				if (!(receive_status() & 2) || m_mcbsp0->data_r(1) != 0xa55a || (receive_status() & 2)) fatalerror("MU4 DRR read readiness mismatch");
				receive_bit(0);
				if (!(receive_status() & 2) || m_mcbsp0->data_r(1) != 0x5aa5 || m_rx_irqs != 2) fatalerror("MU4 receive buffered second word mismatch");
				m_phase = 59; machine().schedule_soft_reset(); return;
			}
			if (m_phase == 59)
			{
				if (!machine().scheduler().can_save() || (receive_status() & 2)) fatalerror("MU4 receive partial save is not quiescent");
				m_saved_dma.str({}); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 receive save failed");
				++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 61)
			{
				if (!machine().scheduler().can_save()) fatalerror("MU4 receive restore has synchronized inputs pending");
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE || (receive_status() & 2)) fatalerror("MU4 receive restore mismatch");
			}
			if (m_phase == 60 || m_phase == 61)
			{
				for (int bit = 8; bit >= 0; --bit) receive_bit(BIT(0xa55a, bit));
				receive_bit(0);
				if (!(receive_status() & 2) || m_mcbsp0->data_r(1) != 0xa55a) fatalerror("MU4 receive pending bit replay mismatch");
				++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 62)
			{
				if (m_rx_irqs != 2 || !(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x10)) fatalerror("MU4 receive interrupt replay mismatch");
				receive_setup(0, 5); receive_word(0xfa, 8, 1);
				if (m_mcbsp0->data_r(1) != 0xfa) fatalerror("MU4 8-bit receive delay mismatch");
				receive_setup(0x20, 6, 5, 0x2001); receive_word(0xabc, 12, 2, true);
				if (m_mcbsp0->data_r(0) != 0xffff || m_mcbsp0->data_r(1) != 0xfabc) fatalerror("MU4 signed receive/polarity mismatch");
				receive_setup(0x20, 4, 0, 0x4001); receive_word(0xabc, 12);
				if (m_mcbsp0->data_r(0) || m_mcbsp0->data_r(1) != 0xabc0) fatalerror("MU4 left-justified receive mismatch");
				m_mcbsp0->rx_frame_w(1); receive_bit(1); external_reg_w(0, 0);
				for (unsigned i = 0; i < 32; ++i) receive_bit(1);
				if (receive_status() & 2) fatalerror("MU4 receive reset did not cancel pending word");
				logerror("mu4_mcbsp_receive: PASS edge_handoff=1 buffering=1 widths=8,12,16 delays=0,1,2 polarity=1 justification=1 peek=1 pending_restore=1 irq=1 reset=1\n");
				check_receive_overrun();
				receive_setup(0x140, 0x44, 0x000e);
				data.write_word(0x6300, 0xffff); data.write_word(0x6340, 0xffff);
				dma_reg_w(0xa, 0x21); dma_reg_w(0xb, 0x6300); dma_reg_w(0xc, 1);
				dma_reg_w(0xd, 0x1000); dma_reg_w(0xe, 0xc055);
				dma_reg_w(0x20, 0x40); dma_reg_w(0x22, 0xffc1);
				dma_reg_w(0x2e, 0x21); dma_reg_w(0x2f, 0x6300); dma_reg_w(0x30, 1); dma_reg_w(0x31, 0x1000);
				m_dma->write(0, 0x44);
				m_phase = 63; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 63)
			{
				if (data.read_word(0x6300) != 0xffff || m_rx_dma_completions) fatalerror("MU4 receive DMA advanced without REVT0");
				m_mcbsp0->rx_frame_w(0);
				for (int bit = 15; bit >= 0; --bit) { receive_bit(BIT(0x1234, bit)); m_mcbsp0->rx_frame_w(1); }
				receive_bit(BIT(0x5678, 15));
				m_phase = 64; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 64)
			{
				if (data.read_word(0x6300) != 0x1234 || data.read_word(0x6340) != 0xffff || (receive_status() & 2) || m_rx_dma_completions)
					fatalerror("MU4 receive DMA first-word/readiness mismatch");
				for (int bit = 14; bit >= 0; --bit) receive_bit(BIT(0x5678, bit));
				receive_bit(0);
				m_phase = 65; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (data.read_word(0x6300) != 0x1234 || data.read_word(0x6340) != 0x5678 || m_rx_dma_completions != 1 ||
				!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x400) || (receive_status() & 2) ||
				dma_reg_r(0xb) != 0x6300 || dma_reg_r(0xc) != 1 || !(m_dma->read(0) & 4))
				fatalerror("MU4 receive DMA completion/sort/reload/IRQ mismatch");
			m_dma->write(0, 0);
			logerror("mu4_mcbsp_receive_dma: PASS event=REVT0 channel=2 deferred=1 sorted=1 auto_reload=1 irq=10\n");
			m_phase = 31; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 54 && m_phase <= 57)
		{
			auto &codec = *subdevice<tlv320aic23_device>("codec");
			if (m_phase == 54)
			{
				if (!m_codec_edges.empty() || codec.reg(7) != 0x53 || codec.reg(8) != 0x23)
					fatalerror("MU4 codec inactive/control mismatch");
				codec.control_word_w(0x1201);
				m_phase = 55; m_check->adjust(attotime::from_usec(92)); return;
			}
			if (m_phase == 55)
			{
				if (m_codec_clocks != 1104 || m_codec_frames.size() != 5)
					fatalerror("MU4 codec clock rate mismatch clocks=%u frames=%u", m_codec_clocks, unsigned(m_codec_frames.size()));
				for (unsigned i = 1; i < m_codec_frames.size(); ++i)
					if (m_codec_frames[i] - m_codec_frames[i - 1] != 272) fatalerror("MU4 codec frame divider mismatch");
				if (m_codec_din_words != std::vector<u16>({0x1357,0x9bdf,0x1357,0x9bdf,0x1357,0x9bdf,0x1357,0x9bdf}) ||
					m_rx_dma_completions != 4 || !machine().scheduler().can_save()) fatalerror("MU4 codec duplex/checkpoint mismatch");
				m_codec_rx_snapshot_count = m_rx_dma_completions;
				m_saved_dma.str({}); m_saved_dma.clear();
				m_codec_snapshot_time = machine().time();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 codec save failed");
				m_codec_edges.clear(); m_phase = 56; m_check->adjust(attotime::from_usec(10)); return;
			}
			if (m_phase == 56)
			{
				m_codec_saved_edges = m_codec_edges;
				m_codec_saved_din.assign(m_codec_din_words.begin() + 8, m_codec_din_words.end());
				if (m_codec_saved_din != std::vector<u16>({0x1357, 0x9bdf}) || m_rx_dma_completions != m_codec_rx_snapshot_count + 1 ||
					m_cpu->space(AS_DATA).read_word(0x6400) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x6401) != 0x5678 || !machine().scheduler().can_save())
					fatalerror("MU4 codec stereo/converted-sample delivery mismatch");
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 codec restore failed");
				// Loading inside a timer callback restores scheduler base time, but
				// adjust() still uses this callback's old timestamp. Target the saved
				// observation deadline, not ten microseconds after the old callback.
				m_codec_edges.clear(); m_codec_din_words.clear(); m_codec_rx_snapshot_count = m_rx_dma_completions; m_phase = 57;
				m_check->adjust(m_codec_snapshot_time + attotime::from_usec(10) - machine().time()); return;
			}
			if (m_codec_edges != m_codec_saved_edges)
				fatalerror("MU4 codec pending clock replay mismatch expected=%u actual=%u first=%d/%d", unsigned(m_codec_saved_edges.size()), unsigned(m_codec_edges.size()), m_codec_saved_edges.empty() ? -1 : m_codec_saved_edges.front(), m_codec_edges.empty() ? -1 : m_codec_edges.front());
			if (m_codec_din_words != m_codec_saved_din || m_rx_dma_completions != m_codec_rx_snapshot_count + 1 ||
				m_cpu->space(AS_DATA).read_word(0x6400) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x6401) != 0x5678)
				fatalerror("MU4 codec mid-word duplex replay mismatch");
			if (u16(codec.dac_sample(0)) != 0x1357 || u16(codec.dac_sample(1)) != 0x9bdf)
				fatalerror("MU4 codec complete stereo DAC latch mismatch");
			check_headphone_level(0,0); check_headphone_level(1,0);
			codec.control_word_w(0x0810); codec.control_word_w(0x0c07);
			check_headphone_level(0,double(s16(0x1357))/32768.0);
			check_headphone_level(1,double(s16(0x9bdf))/32768.0);
			codec.control_word_w(0x0a09);
			check_headphone_level(0,0); check_headphone_level(1,0);
			codec.control_word_w(0x0a01); codec.control_word_w(0x0c0f);
			check_headphone_level(0,0); check_headphone_level(1,0);
			codec.control_word_w(0x0c07); codec.control_word_w(0x0800);
			check_headphone_level(0,0); check_headphone_level(1,0);
			codec.control_word_w(0x0810); codec.control_word_w(0x0573);
			check_headphone_level(0,double(s16(0x1357))/32768.0*0.5011872336272722);
			codec.control_word_w(0x0580);
			if (codec.headphone_volume(0) != 0x73 || codec.headphone_volume(1)) fatalerror("MU4 headphone pending zero-cross update mismatch");
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000,0xeca9); data.write_word(0x6001,0x6421);
			if (!machine().scheduler().can_save()) fatalerror("MU4 pending headphone gain checkpoint is not stable");
			m_saved_dma.str({}); m_saved_dma.clear(); m_codec_snapshot_time = machine().time();
			if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 pending headphone gain save failed");
			m_phase = 119; m_check->adjust(attotime::from_usec(50)); return;
		}
		if (m_phase >= 31 && m_phase <= 37)
		{
			if (m_phase == 31)
			{
				if (!m_serial_tx_words.empty() || (mcbsp_status() & 6)) fatalerror("MU4 McBSP advanced without clock");
				mcbsp_reg_w(14, 0x0200);
				// Preserve the conformance observation windows in serial system-clock
				// ticks, independent of the board's selected reset/PLL rate.
				m_phase = 32; m_check->adjust(attotime::from_ticks(39, m_mcbsp->clock())); return;
			}
			if (m_phase == 32)
			{
				if ((mcbsp_status() & 7) != 7 || !m_serial_tx_words.empty() || m_serial_tx_bits.size() != 1)
					fatalerror("MU4 McBSP buffer-to-shift timing mismatch status=%04x words=%u bits=%u", mcbsp_status(), unsigned(m_serial_tx_words.size()), unsigned(m_serial_tx_bits.size()));
				m_mcbsp->data_w(3, 0x5aa5);
				if (mcbsp_status() & 2) fatalerror("MU4 McBSP second buffer not busy");
				m_serial_saved_bits = m_serial_tx_bits.size();
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 McBSP pending save failed");
				m_phase = 33; m_check->adjust(attotime::from_ticks(910, m_mcbsp->clock())); return;
			}
			if (m_phase == 33 || m_phase == 34)
			{
				if (m_serial_tx_words != std::vector<u16>({0xa55a, 0x5aa5}) || (mcbsp_status() & 7) != 3)
					fatalerror("MU4 McBSP serialized word/completion mismatch");
				unsigned const first = m_phase == 33 ? 0 : m_serial_saved_bits;
				if (m_serial_tx_bits.size() != 32 - first) fatalerror("MU4 McBSP serialized bit count mismatch");
				for (unsigned bit = first; bit < 32; ++bit)
					if (m_serial_tx_bits[bit - first] != BIT(bit < 16 ? 0xa55a : 0x5aa5, 15 - (bit & 15)))
						fatalerror("MU4 McBSP serialized bit order mismatch");
				if (m_phase == 33)
				{
					m_saved_dma.clear(); m_saved_dma.seekg(0);
					if (machine().save().read_stream(m_saved_dma) != STATERR_NONE || (mcbsp_status() & 7) != 5)
						fatalerror("MU4 McBSP pending restore mismatch");
					m_serial_tx_words.clear(); m_serial_tx_bits.clear();
					m_phase = 34; m_check->adjust(attotime::from_ticks(910, m_mcbsp->clock())); return;
				}
				m_mcbsp->data_w(3, 0x1234); mcbsp_reg_w(1, 0);
				m_phase = 35; m_check->adjust(attotime::from_ticks(520, m_mcbsp->clock())); return;
			}
			if (m_phase == 35)
			{
				if (mcbsp_status() & 7 || m_serial_tx_words.size() != 2 || m_serial_tx_irqs != 4) fatalerror("MU4 McBSP reset/interrupt count mismatch");
				m_serial_tx_words.clear(); m_serial_tx_bits.clear();
				mcbsp_reg_w(4, 0); mcbsp_reg_w(1, 0x0041); m_mcbsp->data_w(3, 0xa55a);
				m_phase = 36; m_check->adjust(attotime::from_ticks(520, m_mcbsp->clock())); return;
			}
			unsigned const width = m_phase == 36 ? 8 : 12;
			u16 const expected = m_phase == 36 ? 0x005a : 0x055a;
			if (m_serial_tx_words != std::vector<u16>({expected}) || m_serial_tx_bits.size() != width || (mcbsp_status() & 7) != 3)
				fatalerror("MU4 McBSP short-word completion mismatch");
			for (unsigned bit = 0; bit < width; ++bit)
				if (m_serial_tx_bits[bit] != BIT(expected, width - bit - 1)) fatalerror("MU4 McBSP short-word bit order mismatch");
			if (m_phase == 36)
			{
				m_serial_tx_words.clear(); m_serial_tx_bits.clear();
				mcbsp_reg_w(1, 0); mcbsp_reg_w(4, 0x0020); mcbsp_reg_w(1, 0x0041); m_mcbsp->data_w(3, 0xa55a);
				m_phase = 37; m_check->adjust(attotime::from_ticks(520, m_mcbsp->clock())); return;
			}
			if (m_serial_tx_irqs != 8) fatalerror("MU4 McBSP short-word interrupt mismatch");
			m_saved_dma.str(std::string());
			logerror("mu4_mcbsp_conformance: PASS reset_ready=1 busy=1 external_stall=1 bit_order=1 pending_restore=1 cancel=1 widths=8,12,16 tx_irqs=8\n");
			m_phase = 38; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 38 && m_phase <= 43)
		{
			if (m_phase == 38)
			{
				if (!m_dma_output.empty() || m_dma->read(0) != 8) fatalerror("MU4 synchronized DMA advanced without event");
				m_dma->sync_event(3);
				if (!m_dma_output.empty()) fatalerror("MU4 mismatched DMA event advanced transfer");
				m_dma->sync_event(2);
				if (!m_dma_output.empty()) fatalerror("MU4 synchronized DMA completed synchronously");
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 synchronized DMA save failed");
			}
			else if (m_phase <= 42)
			{
				unsigned const words = m_phase - 38;
				std::vector<u16> const expected = {0x1234, 0x5678, 0xabcd, 0x9abc};
				if (m_dma_output != std::vector<u16>(expected.begin(), expected.begin() + words))
					fatalerror("MU4 synchronized DMA sorting mismatch step=%u", words);
				if (m_dma_completions != (words == 4 ? 1U : 0U)) fatalerror("MU4 DMA block interrupt ordering mismatch");
				if (words == 4)
				{
					if (m_dma->read(0) != 8 || dma_reg_r(0xf) != 0x6000 || dma_reg_r(0x10) != 0x6200 ||
						dma_reg_r(0x11) != 1 || dma_reg_r(0x12) != 0x2001) fatalerror("MU4 DMA auto-reload mismatch");
					m_saved_dma.clear(); m_saved_dma.seekg(0);
					if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 synchronized DMA restore failed");
					m_dma_output.clear(); m_dma_completions = 0;
				}
				else m_dma->sync_event(2);
			}
			else
			{
				if (m_dma_output != std::vector<u16>({0x1234}) || m_dma_completions || dma_reg_r(0xf) != 0x6040)
					fatalerror("MU4 synchronized DMA pending replay mismatch");
				m_dma->write(0, 0); m_saved_dma.str(std::string());
				m_phase = 44; machine().schedule_soft_reset(); return;
			}
			++m_phase; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase >= 44 && m_phase <= 48)
		{
			unsigned const words = m_phase - 44;
			std::vector<u16> const expected = {0x1234, 0x5678, 0xabcd, 0x9abc};
			if (m_dma_output != std::vector<u16>(expected.begin(), expected.begin() + words) || m_dma_completions != words / 2)
				fatalerror("MU4 DMA frame interrupt/transfer ordering mismatch step=%u", words);
			if (bool(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x0800) != (words >= 2))
				fatalerror("MU4 DMA/McBSP interrupt mux mismatch step=%u", words);
			if (words == 1)
			{
				// Pause mid-frame without rewriting DMCTR; its original shadow must survive.
				u16 const control = m_dma->read(0);
				m_dma->write(0, control & ~u16(8));
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 paused-frame DMA save failed");
				dma_reg_w(0x11, 7); // Disturb both the live count and its shadow before restore.
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 paused-frame DMA restore failed");
				if (m_dma->inspect_register(0x11) || (m_dma->read(0) & 8))
					fatalerror("MU4 paused-frame DMA restore lost its decremented counter or disabled state");
				m_dma->write(0, control);
			}
			if (words == 2 && m_dma->inspect_register(0x11) != 1)
				fatalerror("MU4 DMA re-enable replaced original frame count with its decremented counter");
			if (words == 4)
			{
				if (m_dma->read(0) & 8) fatalerror("MU4 non-reloading DMA remains enabled");
				logerror("mu4_dma_sync_conformance: PASS event_gate=1 sorting=1 multiframe=1 auto_reload=1 block_interrupt=1 frame_interrupt=1 no_reload=1 pending_restore=1\n");
				logerror("mu4_dma_frame_pause: PASS mid_frame=1 counter_shadow=1 sorting=1 frame_interrupts=2 paused_restore=1\n");
				m_phase = 49; machine().schedule_soft_reset(); return;
			}
			m_dma->sync_event(2); ++m_phase; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 49 || m_phase == 50)
		{
			std::vector<u16> const expected = m_phase == 49 ? std::vector<u16>({0x1234}) : std::vector<u16>({0x1234, 0xabcd});
			if (m_dma_output != expected || m_dma_completions != m_phase - 48 || (m_dma->read(0) & 8))
				fatalerror("MU4 DMA held synchronization level mismatch");
			if (m_phase == 49)
			{
				m_dma->write(0, 8); ++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			m_dma->sync_w(2, CLEAR_LINE);
			logerror("mu4_dma_sync_level: PASS ready_before_enable=1 held_level=1 reenable=1\n");
			m_phase = 51; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 51 && m_phase <= 53)
		{
			// Pin transitions queue synchronized CPU inputs. Save/load must happen
			// after those anonymous timers drain: postload discards them, while the
			// input queue itself is not serialized by MAME.
			if (m_external_checkpoint_pending)
			{
				if (!machine().scheduler().can_save()) fatalerror("MU4 external checkpoint has pending synchronized inputs");
				if (!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x20))
					fatalerror("MU4 external checkpoint has not latched XINT0");
				m_external_checkpoint_pending = false;
				if (m_phase == 51)
				{
					m_saved_dma.str(std::string()); m_saved_dma.clear();
					if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 external McBSP save failed");
				}
				else
				{
					m_saved_dma.clear(); m_saved_dma.seekg(0);
					if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 external McBSP restore failed");
					if (!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x20))
						fatalerror("MU4 external checkpoint did not restore XINT0");
					m_external_words.clear(); m_external_bits.clear();
				}
				++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 51)
			{
				external_clocks(4);
				if (!m_external_bits.empty()) fatalerror("MU4 external McBSP shifted without a frame");
				m_mcbsp0->tx_frame_w(0);
				if (m_external_bits != std::vector<int>({1})) fatalerror("MU4 zero-delay frame first bit not asynchronous");
				m_mcbsp0->control_w(0, 1);
				if ((m_mcbsp0->control_r(1) & 7) != 5) fatalerror("MU4 external XRDY asserted before opposite edge");
				external_clocks(1); // Same edge as the asynchronous first bit cannot shift twice.
				if (m_external_bits.size() != 1) fatalerror("MU4 external McBSP double-shifted first bit");
				if ((m_mcbsp0->control_r(1) & 7) != 7) fatalerror("MU4 external XRDY missing after opposite edge");
				m_mcbsp0->data_w(3, 0x5aa5);
				m_mcbsp0->tx_frame_w(1); m_mcbsp0->tx_frame_w(0); // XFIG ignores this early frame.
				external_clocks(6);
				if (m_external_bits.size() != 7 || !m_external_words.empty()) fatalerror("MU4 external frame progress mismatch");
				m_external_checkpoint_pending = true;
				m_check->adjust(attotime::from_usec(1)); return;
			}
			else
			{
				external_clocks(9 + 16);
				unsigned const first = m_phase == 52 ? 0 : 7;
				if (m_external_words != std::vector<u16>({0xa55a, 0x5aa5}) || m_external_bits.size() != 32 - first)
					fatalerror("MU4 external framed word/save replay mismatch");
				for (unsigned bit = first; bit < 32; ++bit)
					if (m_external_bits[bit - first] != BIT(bit < 16 ? 0xa55a : 0x5aa5, 15 - (bit & 15)))
						fatalerror("MU4 external framed bit order mismatch");
				external_clocks(8);
				if (m_external_words.size() != 2) fatalerror("MU4 external McBSP advanced beyond frame length");
				if (m_phase == 52)
				{
					m_external_checkpoint_pending = true;
					m_check->adjust(attotime::from_usec(1)); return;
				}
				else
				{
					m_mcbsp0->data_w(3, 0x1234); m_mcbsp0->tx_frame_w(1); m_mcbsp0->tx_frame_w(0);
					external_reg_w(1, 0); external_clocks(32);
					if (m_external_words.size() != 2) fatalerror("MU4 external reset failed to cancel frame");
					m_external_words.clear(); m_external_bits.clear();
					external_reg_w(14, 0); external_reg_w(4, 0); external_reg_w(5, 1);
					m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0); external_reg_w(1, 1);
					m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1);
					if (!m_external_bits.empty()) fatalerror("MU4 delayed frame shifted before clock");
					external_clocks(8, false);
					if (m_external_words != std::vector<u16>({0x005a}) || m_external_bits.size() != 8)
						fatalerror("MU4 external polarity/data delay mismatch");
					m_external_words.clear(); m_external_bits.clear();
					external_reg_w(1, 0); external_reg_w(4, 0x20); external_reg_w(5, 2);
					m_mcbsp0->tx_frame_w(0); external_reg_w(1, 1);
					m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1);
					external_clocks(1, false);
					if (!m_external_bits.empty()) fatalerror("MU4 two-bit delay emitted on first clock");
					external_clocks(12, false);
					if (m_external_words != std::vector<u16>({0x055a}) || m_external_bits.size() != 12)
						fatalerror("MU4 external two-bit delay mismatch");
					logerror("mu4_mcbsp_external: PASS frame_gate=1 stereo=1 bit_order=1 ignore=1 asynchronous=1 delay=1 polarity=1 cancel=1 pending_restore=1 synchronized_inputs=1\n");
					m_phase = 70; m_check->adjust(attotime::from_usec(1)); return;
				}
			}
			++m_phase; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase >= 70 && m_phase <= 73)
		{
			// Each physical-input variant yields so synchronized IRQ events can drain.
			if (m_phase < 73)
			{
				check_receive_sync(m_phase - 70);
				check_transmit_sync(m_phase - 70);
				check_external_overwrite(m_phase - 70);
			}
			check_external_underrun(m_phase - 70);
			if (m_phase < 73) { ++m_phase; m_check->adjust(attotime::from_usec(1)); return; }
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1); external_clocks(7, false);
			m_phase = 66; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 66 || m_phase == 67)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 underrun checkpoint has pending synchronized inputs");
			if (m_phase == 66)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 underrun save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 underrun restore failed");
			}
			m_mcbsp0->control_w(0, 1);
			if ((m_mcbsp0->control_r(1) & 7) != 3) fatalerror("MU4 underrun save/restore lost XEMPTY or XRDY");
			m_external_words.clear(); m_external_bits.clear(); external_clocks(9, false);
			if (m_external_words != std::vector<u16>({0x1234}) || m_external_bits.size() != 9)
				fatalerror("MU4 underrun save/restore shifted word mismatch");
			for (unsigned bit = 0; bit < 9; ++bit)
				if (m_external_bits[bit] != BIT(0x1234, 8 - bit)) fatalerror("MU4 underrun save/restore bit mismatch");
			if (m_phase == 66) { m_phase = 67; m_check->adjust(attotime::from_usec(1)); return; }
			logerror("mu4_mcbsp_underrun_restore: PASS partial_word=1 xempty=1 ready=1 bits=9\n");
			receive_setup(0, 0); receive_word(0x11, 8); receive_word(0x22, 8); receive_word(0x33, 8);
			m_phase = 68; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 68 || m_phase == 69)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 overrun checkpoint has pending synchronized inputs");
			if (m_phase == 68)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 overrun save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 overrun restore failed");
			}
			if ((receive_status() & 7) != 7 || m_mcbsp0->data_r(1) != 0x11 || (receive_status() & 7) != 1)
				fatalerror("MU4 overrun save/restore lost unread DRR or RFULL");
			receive_bit(0);
			if (m_mcbsp0->data_r(1) != 0x22 || (receive_status() & 6)) fatalerror("MU4 overrun save/restore lost buffered RBR");
			if (m_phase == 68) { m_phase = 69; m_check->adjust(attotime::from_usec(1)); return; }
			logerror("mu4_mcbsp_overrun_restore: PASS rfull=1 drr=1 rbr=1 read_clear=1\n");
			receive_setup(0, 0, 0, 0x31);
			m_mcbsp0->rx_frame_w(1);
			for (unsigned bit = 8; bit-- > 4; ) { receive_bit(BIT(0xab, bit)); m_mcbsp0->rx_frame_w(0); }
			m_mcbsp0->rx_frame_w(1);
			for (unsigned bit = 8; bit-- > 4; ) { receive_bit(BIT(0x5a, bit)); m_mcbsp0->rx_frame_w(0); }
			m_phase = 75; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 75 || m_phase == 76)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 receive sync checkpoint has pending synchronized inputs");
			if (m_phase == 75)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 receive sync save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 receive sync restore failed");
			}
			unsigned const irqs = m_rx_irqs;
			external_reg_w(0, 0x39); // Rewriting an already asserted error must not make a new edge.
			if ((receive_status() & 0xf) != 9 || m_rx_irqs != irqs)
				fatalerror("MU4 receive sync restoration lost sticky error or held IRQ state");
			for (unsigned bit = 4; bit-- > 0; ) receive_bit(BIT(0x5a, bit));
			receive_bit(0);
			if ((receive_status() & 0xf) != 11 || m_mcbsp0->data_r(1) != 0x5a || m_rx_irqs != irqs)
				fatalerror("MU4 receive sync partial replacement word did not replay exactly");
			external_reg_w(0, 0x31);
			if (m_phase == 75)
			{
				receive_word(0xa5, 8); external_reg_w(0, 0); // Overwrite serial state before restoring.
				m_phase = 76; m_check->adjust(attotime::from_usec(1)); return;
			}
			external_reg_w(0, 0x39);
			if (m_rx_irqs != irqs + 1) fatalerror("MU4 receive sync IRQ did not reassert after clear");
			external_reg_w(0, 0);
			if (receive_status() & 8) fatalerror("MU4 receive sync reset retained an error");
			logerror("mu4_mcbsp_receive_sync_restore: PASS partial_word=1 sticky_error=1 irq_level=1 no_duplicate_irq=1 payload_replay=1 reset=1\n");
			external_reg_w(1, 0); external_reg_w(4, 0x100); external_reg_w(5, 1); external_reg_w(14, 0);
			m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0); external_reg_w(1, 0x31);
			m_mcbsp0->data_w(3, 0x5a); m_mcbsp0->tx_frame_w(1); external_clocks(4, false);
			m_mcbsp0->data_w(3, 0xa5);
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1); external_clocks(4, false);
			m_phase = 77; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 77 || m_phase == 78)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 transmit sync checkpoint has pending synchronized inputs");
			if (m_phase == 77)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 transmit sync save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 transmit sync restore failed");
			}
			m_external_words.clear(); m_external_bits.clear();
			unsigned const irqs = m_external_tx_irqs;
			external_reg_w(1, 0x39);
			if ((m_mcbsp0->control_r(1) & 0xf) != 13 || m_external_tx_irqs != irqs)
				fatalerror("MU4 transmit sync restoration lost sticky error, buffered DXR or held IRQ state");
			external_clocks(12, false);
			if (m_external_words != std::vector<u16>({0x5a, 0xa5}) || m_external_bits.size() != 12 || m_external_tx_irqs != irqs)
				fatalerror("MU4 transmit sync partial XSR and queued DXR did not replay");
			for (unsigned bit = 0; bit < 12; ++bit)
				if (m_external_bits[bit] != BIT(bit < 4 ? 0x5a : 0xa5, bit < 4 ? 3 - bit : 11 - bit))
					fatalerror("MU4 transmit sync restored physical bits mismatch");
			external_reg_w(1, 0x31);
			if (m_phase == 77)
			{
				external_reg_w(1, 0); m_phase = 78; m_check->adjust(attotime::from_usec(1)); return;
			}
			external_reg_w(1, 0x39);
			if (m_external_tx_irqs != irqs + 1) fatalerror("MU4 transmit sync restored IRQ did not reassert after clear");
			external_reg_w(1, 0);
			logerror("mu4_mcbsp_transmit_sync_restore: PASS partial_xsr=1 queued_dxr=1 sticky_error=1 irq_level=1 no_duplicate_irq=1 physical_bits=1\n");
			receive_setup(0, 2);
			m_mcbsp0->rx_frame_w(1); receive_bit(0); m_mcbsp0->rx_frame_w(0); receive_bit(0);
			for (unsigned bit = 8; bit-- > 2; ) receive_bit(BIT(0x5a, bit));
			m_mcbsp0->rx_frame_w(1); receive_bit(BIT(0x5a, 1)); m_mcbsp0->rx_frame_w(0);
			m_phase = 79; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 79 || m_phase == 80)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 packed receive checkpoint has pending synchronized inputs");
			if (m_phase == 79)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 packed receive save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 packed receive restore failed");
			}
			unsigned const irqs = m_rx_irqs;
			receive_bit(BIT(0x5a, 0)); receive_bit(BIT(0xa5, 7));
			if ((receive_status() & 0xf) != 3 || m_mcbsp0->data_r(1) != 0x5a)
				fatalerror("MU4 packed receive restoration lost the old word tail");
			for (unsigned bit = 7; bit-- > 0; ) receive_bit(BIT(0xa5, bit));
			receive_bit(0);
			if ((receive_status() & 0xf) != 3 || m_mcbsp0->data_r(1) != 0xa5 || m_rx_irqs != irqs + 2)
				fatalerror("MU4 packed receive restoration lost the pending frame or its delay");
			external_reg_w(0, 0);
			if (m_phase == 79) { m_phase = 80; m_check->adjust(attotime::from_usec(1)); return; }
			logerror("mu4_mcbsp_receive_packing_restore: PASS old_tail=1 pending_frame=1 exact_words=1 ready_edges=1 no_false_error=1\n");
			m_saved_dma.str(std::string()); m_phase = 81; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase >= 81 && m_phase <= 98)
		{
			// Yield between pin cases so real CPU interrupt synchronization can drain.
			check_receive_packing(m_phase - 81);
			if (m_phase < 98) { ++m_phase; m_check->adjust(attotime::from_usec(1)); return; }
			m_phase = 99; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase >= 99 && m_phase <= 116)
		{
			check_transmit_packing(m_phase - 99);
			if (m_phase < 116) { ++m_phase; m_check->adjust(attotime::from_usec(1)); return; }
			external_reg_w(1, 0); external_reg_w(4, 0); external_reg_w(5, 2);
			m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0); external_reg_w(1, 1);
			m_mcbsp0->data_w(3, 0x5a); m_mcbsp0->tx_frame_w(1); external_clocks(8, false);
			m_mcbsp0->data_w(3, 0xa5); m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1);
			m_phase = 117; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 117 || m_phase == 118)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 packed transmit checkpoint has pending synchronized inputs");
			if (m_phase == 117)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 packed transmit save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 packed transmit restore failed");
			}
			m_external_words.clear(); m_external_bits.clear();
			unsigned const irqs = m_external_tx_irqs;
			external_clocks(9, false); m_mcbsp0->control_w(0, 1);
			if (m_external_words != std::vector<u16>({0x5a, 0xa5}) || m_external_bits.size() != 9 ||
				(m_mcbsp0->control_r(1) & 8) || m_external_tx_irqs != irqs + 1)
				fatalerror("MU4 packed transmit replay lost XSR tail, queued DXR or next frame");
			for (unsigned bit = 0; bit < 9; ++bit)
				if (m_external_bits[bit] != (bit ? BIT(0xa5, 8 - bit) : BIT(0x5a, 0)))
					fatalerror("MU4 packed transmit replay changed physical bits");
			external_reg_w(1, 0);
			if (m_phase == 117) { m_phase = 118; m_check->adjust(attotime::from_usec(1)); return; }
			logerror("mu4_mcbsp_transmit_packing_restore: PASS old_tail=1 queued_dxr=1 pending_frame=1 physical_bits=1 ready_edges=1 no_false_error=1\n");
			m_saved_dma.str(std::string());
			m_phase = 25; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 25 && m_phase <= 29)
		{
			auto &data = m_cpu->space(AS_DATA);
			auto &program = m_cpu->space(AS_PROGRAM);
			if (m_phase == 28)
			{
				if ((data.read_word(0x54) & 1) || data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA pre-restore transfer failed");
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE || !(data.read_word(0x54) & 1) || data.read_word(0x6100) != 0xffff)
					fatalerror("MU4 DMA pending save restore mismatch");
				data.write_word(0x55, 0x32);
				if (data.read_word(0x57) != 0x1032) fatalerror("MU4 DMA reload-bank save restore mismatch");
				m_phase = 29;
				m_check->adjust(attotime::from_usec(1));
				return;
			}
			if (data.read_word(0x54) & 1) fatalerror("MU4 DMA completion bit remains set");
			if (m_dma->source_matches(0, AS_DATA, 0x6000)) fatalerror("MU4 completed DMA source inspection remained active");
			if (m_dma->source_read_matches(0, AS_DATA, 0x6000) ||
				m_dma_observed_reads != (m_phase == 25 || m_phase == 29 ? 2U : m_phase == 27 ? 0U : 1U))
				fatalerror("MU4 actual DMA read ownership count mismatch phase=%u reads=%u", m_phase, m_dma_observed_reads);
			if (m_dma->destination_write_matches(0, AS_DATA, 0x6100) ||
				m_dma->destination_write_matches(0, AS_PROGRAM, 0x2ffff) ||
				m_dma_observed_writes != m_dma_observed_reads)
				fatalerror("MU4 actual DMA write ownership count mismatch phase=%u reads=%u writes=%u", m_phase, m_dma_observed_reads, m_dma_observed_writes);
			if (m_phase == 25 && (program.read_word(0x2ffff) != 0x1234 || program.read_word(0x20000) != 0xabcd))
				fatalerror("MU4 DMA program page wrap mismatch");
			if (m_phase == 26 && data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA zero-count single-word mismatch");
			if (m_phase == 27 && data.read_word(0x6100) != 0xffff) fatalerror("MU4 DMA canceled transfer wrote data");
			if (m_phase == 27)
			{
				for (unsigned bank : {0x24U, 0x2aU, 0x2eU, 0x32U, 0x36U, 0x3aU})
				{
					data.write_word(0x55, bank);
					for (unsigned field = 0; field < 4; ++field) data.write_word(0x56, 0x1000 + bank + field);
					data.write_word(0x55, bank);
					for (unsigned field = 0; field < 4; ++field)
						if (data.read_word(0x56) != (field == 3 ? bank + field : 0x1000 + bank + field))
							fatalerror("MU4 DMA reload bank masking mismatch bank=%02x field=%u", bank, field);
				}
				for (unsigned reserved : {0x28U, 0x29U})
				{
					data.write_word(0x55, reserved); data.write_word(0x57, 0xffff);
					if (data.read_word(0x57)) fatalerror("MU4 DMA reserved bank not zero");
				}
				data.write_word(0x54, 1);
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 DMA pending save failed");
				data.write_word(0x55, 0x32); data.write_word(0x57, 0xffff);
				m_phase = 28;
				m_check->adjust(attotime::from_usec(1));
				return;
			}
			if (m_phase == 29)
			{
				if (data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA restored transfer failed");
				m_saved_dma.str(std::string());
				logerror("mu4_dma_conformance: PASS deferred=1 page_wrap=1 zero_count=1 cancel=1 pending_restore=1 reload_banks=6 frame_mask=1 reserved=1\n");
				logerror("mu4_dma_source_inspection: PASS disabled=1 active=1 space=1 address=1 channel=1 index_preserved=1\n");
				logerror("mu4_dma_read_ownership: PASS source_reads=1 ordinary_read_excluded=1 channel=1 space=1 address=1 canceled=1 restored=1\n");
				logerror("mu4_dma_write_ownership: PASS data_program=1 ordinary_write_excluded=1 channel=1 space=1 address=1 source_phase_excluded=1 canceled=1 restored=1\n");
				m_phase = 0;
			}
			else ++m_phase;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 24)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x3aea)
				fatalerror("MU4 original loader failed pc=%04x sp=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
			if (m_cpu->space(AS_DATA).read_word(0x54) & 1) fatalerror("MU4 loader returned before DMA completion");
			auto word = [this](unsigned offset) -> u16
			{
				if (offset + 1 >= m_segment.length()) fatalerror("MU4 loader source bounds");
				return (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
			};
			unsigned segment = 0;
			while (word(segment) != 0xaa22)
				segment += 10 + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
			unsigned cursor = segment + 6, records = 0, checked = 0;
			unsigned const end = cursor + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
			while (word(cursor))
			{
				unsigned const count = word(cursor);
				u32 const destination = (u32(word(cursor + 2)) << 16) | word(cursor + 4);
				cursor += 6;
				if (cursor + count * 2 > end) fatalerror("MU4 loader section bounds");
				bool const program = destination > 0xffff;
				// Independent expected rule from original 2f00..2f0e and 2f60..2f70.
				u32 const address = (destination & 0x8000) ? destination : destination & 0xffff;
				for (unsigned i = 0; i < count; ++i)
				{
					u32 const target = (program ? address & 0x7f0000 : 0) | u16(address + i);
					u16 const actual = m_cpu->space(program ? AS_PROGRAM : AS_DATA).read_word(target);
					if (actual != word(cursor + i * 2))
						fatalerror("MU4 DMA destination mismatch space=%s address=%06x actual=%04x expected=%04x", program ? "program" : "data", target, actual, word(cursor + i * 2));
				}
				cursor += count * 2;
				checked += count;
				++records;
			}
			if (cursor + 2 != end || checked != 51921) fatalerror("MU4 loader coverage mismatch words=%u", checked);
			logerror("mu4_loader: PASS original_loader=1 dma_complete=1 records=%u words=%u data_and_program=1\n", records, checked);
			capture_native_ram("loaded");
			if ((system_bios() == 14 || system_bios() == 15) && !m_native_runtime_observers_installed)
			{
				// Install once across the reset legs; counters are per-leg observations.
				m_native_runtime_observers_installed = true;
				auto const observe = [this](offs_t address, u16 value, bool write)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					static constexpr offs_t candidates[] = {0x374a, 0x374d, 0x374e, 0x3753, 0x3754, 0x3756, 0x3757, 0x3b12, 0x3b13};
					auto const found = std::find(std::begin(candidates), std::end(candidates), address);
					if (found == std::end(candidates)) return;
					unsigned &count = (write ? m_native_runtime_writes : m_native_runtime_reads)[found - std::begin(candidates)];
					if (count++ < 16)
						logerror("mu4_native_runtime_word: leg=%u access=%s address=%04x value=%04x pc=%06x\n",
							m_native_reset_leg, write ? "write" : "read", unsigned(address), value, unsigned(m_cpu->state_int(STATE_GENPC)));
				};
				m_cpu->space(AS_DATA).install_read_tap(0x374a, 0x3b13, "mu4_native_runtime_read",
					[observe](offs_t address, u16 &value, u16) { observe(address, value, false); });
				m_cpu->space(AS_DATA).install_write_tap(0x374a, 0x3b13, "mu4_native_runtime_write",
					[observe](offs_t address, u16 &value, u16) { observe(address, value, true); });
			}
			// Observe unchanged 3538 -> loader -> 2000 -> page-2 common-window entry.
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2000, 0x2000, "mu4_native_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_entry_reads; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d62, 0x6d62, "mu4_native_far_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_far_reads; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x028afc, 0x028afc, "mu4_native_dma_tx_handler",
				[this](offs_t, u16 &, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() &&
						m_cpu->state_int(STATE_GENPC) == 0x028afd) ++m_native_dma_tx_handler;
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x029545, 0x029545, "mu4_native_stream_consumer",
				[this](offs_t, u16 &, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() &&
						m_cpu->state_int(STATE_GENPC) == 0x029546)
					{
						++m_native_stream_consumer_entries;
						if (m_native_consumer_state_traces++ < 8)
						{
							auto const disable = machine().disable_side_effects();
							auto &data = m_cpu->space(AS_DATA);
							logerror("mu4_native_consumer_state: mode_bb80=%04x flag94de=%04x flag0062=%04x phase_bb86=%04x phase_bb91=%04x b66d=%04x b64e=%04x count_b650=%04x,%04x\n",
								data.read_word(0xbb80), data.read_word(0x94de), data.read_word(0x62), data.read_word(0xbb86),
								data.read_word(0xbb91), data.read_word(0xb66d), data.read_word(0xb64e), data.read_word(0xb650), data.read_word(0xb651));
						}
					}
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2954f, 0x2963f, "mu4_native_consumer_paths",
				[this](offs_t address, u16 &, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->state_int(STATE_GENPC) != address + 1) return;
					static constexpr offs_t points[] = {0x2954f, 0x29553, 0x29557, 0x29559, 0x295b2, 0x2963f};
					for (unsigned i = 0; i < std::size(points); ++i)
						if (address == points[i]) ++m_native_consumer_paths[i];
				});
			m_cpu->space(AS_DATA).install_write_tap(0xbb80, 0xbb80, "mu4_native_consumer_mode",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() && m_native_consumer_mode_writes++ < 16)
						logerror("mu4_native_consumer_mode: value=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x39b6d, 0x3acf2, "mu4_native_command_paths",
				[this](offs_t address, u16 &, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->state_int(STATE_GENPC) != address + 1) return;
					static constexpr offs_t points[] = {0x39b6d, 0x39bc2, 0x39dc7, 0x39f08, 0x39f5e, 0x3acf2};
					for (unsigned i = 0; i < std::size(points); ++i)
						if (address == points[i]) ++m_native_command_paths[i];
				});
			m_cpu->space(AS_DATA).install_write_tap(0xb633, 0xb633, "mu4_native_stream_pending_write",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					unsigned &count = value ? m_native_stream_pending_sets : m_native_stream_pending_clears;
					if (count++ < 8) logerror("mu4_native_stream_pending: write=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_read_tap(0xb633, 0xb633, "mu4_native_stream_pending_read",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					if (m_native_stream_pending_reads++ < 8) logerror("mu4_native_stream_pending: read=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_write_tap(0x007e, 0x007e, "mu4_native_dispatch_word",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() && m_native_dispatch_traces++ < 16)
						logerror("mu4_native_dispatch_word: write=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_write_tap(0x806e, 0x807f, "mu4_native_dispatch_descriptor",
				[this](offs_t address, u16 &value, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() && m_native_descriptor_traces++ < 32)
						logerror("mu4_native_dispatch_descriptor: address=%04x write=%04x pc=%06x\n", unsigned(address), value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2000, 0x207f, "mu4_native_dma_vectors",
				[this](offs_t address, u16 &value, u16)
				{
					// fetch() increments PC before reading. Exclude DMA/table/debug reads.
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					u16 const base = m_cpu->state_int(tms320c54x_device::STATE_PMST) & 0xff80;
					if (address != base + 0x68 && address != base + 0x6c) return;
					unsigned &count = address == base + 0x68 ? m_native_dma_rx_vectors : m_native_dma_tx_vectors;
					if (!count++) logerror("mu4_native_dma_vector: address=%04x words=%04x,%04x,%04x,%04x source=%u\n",
						unsigned(address), value, m_cpu->space(AS_PROGRAM).read_word(address + 1),
						m_cpu->space(AS_PROGRAM).read_word(address + 2), m_cpu->space(AS_PROGRAM).read_word(address + 3),
						address == base + 0x68 ? 10 : 11);
				});
			m_cpu->space(AS_DATA).install_write_tap(0xbdb8, 0xbde7, "mu4_native_selection_state",
				[this](offs_t address, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						(address != 0xbdb8 && address != 0xbdb9 && address != 0xbdbc &&
						 address != 0xbde3 && address != 0xbde4 && address != 0xbde7)) return;
					if (m_native_selection_writes++ < 32)
						logerror("mu4_native_selection_write: address=%04x value=%04x pc=%06x\n",
							unsigned(address), value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			auto const selection_observer = [this](offs_t address, u16 &opcode, u16)
				{
					address &= 0xffff;
					if (m_phase != 30 || !m_native_dma_tx_handler || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					static constexpr u16 addresses[] = {
						0x3f37, 0x3f4b, 0x3edc, 0x3ef3, 0x3f3f, 0x3f40, 0x3f5b, 0x3f5f,
						0x3f6b, 0x3da6, 0x3f6d, 0x8b56, 0x8b5a, 0x8b63, 0x8b6c, 0x8b74, 0x4045
					};
					auto const found = std::find(std::begin(addresses), std::end(addresses), address);
					if (found == std::end(addresses)) return;
					unsigned const slot = found - std::begin(addresses);
					if (m_native_selection_traces[slot]++ >= 4) return;
					u16 const st0 = m_cpu->state_int(tms320c54x_device::STATE_ST0), st1 = m_cpu->state_int(tms320c54x_device::STATE_ST1);
					u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
					u16 const base = BIT(st1, 14) ? sp : (st0 & 0x1ff) << 7;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					logerror("mu4_native_selection: pc=%06x opcode=%04x st0=%04x st1=%04x sp=%04x base=%04x word007f=%04x ar2=%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, st0, st1, sp, base, data.read_word(0x7f), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)));
					logerror("mu4_native_selection_acc: a=%010llx\n", static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
					if (base >= 0x80) logerror("mu4_native_selection_ram: base=%04x slot38=%04x slot39=%04x slot63=%04x slot64=%04x slot67=%04x\n",
						base, data.read_word(u16(base + 0x38)), data.read_word(u16(base + 0x39)), data.read_word(u16(base + 0x63)),
						data.read_word(u16(base + 0x64)), data.read_word(u16(base + 0x67)));
					if (base >= 0x80) logerror("mu4_native_selection_control: slot3c=%04x\n", data.read_word(u16(base + 0x3c)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3da6, 0x4045, "mu4_native_selection", selection_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x28afc, 0x28b74, "mu4_native_isr_selection", selection_observer);
			auto const startup_observer = [this](offs_t address, u16 &opcode, u16)
				{
					address &= 0xffff;
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					static constexpr u16 call_sites[] = {
						0x9063, 0x906b, 0x9074, 0x907a, 0x90a5, 0x90a7, 0x90b0,
						0x90b5, 0x90bc, 0x90c4, 0x90c7, 0x90cb, 0x90cd, 0x90d0,
						0x90d2, 0x90d4, 0x90d6, 0x90de, 0x90e0, 0x90e6, 0x90f2,
						0x90f4, 0x90fb, 0x9104, 0x9106, 0x9115
					};
					for (u16 site : call_sites)
						if ((address == site || address == site + 2) && m_native_main_call_traces++ < 64)
							logerror("mu4_native_main_call: site=%04x point=%s pc=%06x opcode=%04x sp=%04x\n",
								site, address == site ? "call" : "return", unsigned(m_cpu->state_int(STATE_GENPC)),
								opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
					unsigned const slot = address == 0x904b ? 0 : address == 0x6dbf ? 1 : address == 0x6d44 ? 2 : address == 0x3f1d ? 3 : 4;
					if (slot == 4 || m_native_startup_fetches[slot]++ >= 4) return;
					logerror("mu4_native_startup_fetch: pc=%06x opcode=%04x sp=%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3f1d, 0x6dbf, "mu4_native_startup_common", startup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2904b, 0x29117, "mu4_native_startup_main", startup_observer);
			auto const startup_return_observer = [this](offs_t address, u16 &opcode, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1 ||
						(address > 0x6d61 && address < 0x6dbf) || m_native_startup_return_traces++ >= 120) return;
					auto const disable = machine().disable_side_effects();
					u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
					logerror("mu4_native_startup_return_step: physical=%04x logical=%06x opcode=%04x sp=%04x stack=%04x,%04x,%04x st0=%04x st1=%04x imr=%04x ifr=%04x\n",
						unsigned(address), unsigned(m_cpu->state_int(STATE_GENPC)), opcode, sp,
						m_cpu->space(AS_DATA).read_word(sp), m_cpu->space(AS_DATA).read_word(u16(sp + 1)),
						m_cpu->space(AS_DATA).read_word(u16(sp + 2)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d44, 0x6dc5, "mu4_native_startup_return", startup_return_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3d73, 0x3da5, "mu4_native_startup_context", startup_return_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x38c3e, 0x3c6cd, "mu4_native_startup_read",
				[this](offs_t address, u16 &opcode, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != u16(address + 1)) return;
					// Restrict the lower-call census to the settings operation, not earlier boot writes.
					if (address == 0x3c6ba && m_cpu->state_int(tms320c54x_device::STATE_SP) >= 0x1200)
						m_native_settings_buffer_active = true;
					else if (address == 0x3c6bc)
						m_native_settings_buffer_active = false;
					if (m_native_settings_buffer_active)
						for (unsigned i = 0; i < std::size(buffer_calls); ++i)
							if (address == buffer_calls[i] || address == buffer_calls[i] + 2)
							{
								bool const returning = address != buffer_calls[i];
								unsigned &count = returning ? m_native_buffer_returns[i] : m_native_buffer_calls[i];
								if (++count > 4) continue;
								auto const disable = machine().disable_side_effects();
								auto &data = m_cpu->space(AS_DATA);
								logerror("mu4_native_buffer_call: site=%06x point=%s count=%u opcode=%04x sp=%04x dirty=%04x erase=%04x retry=%04x a=%010llx\n",
									unsigned(buffer_calls[i]), returning ? "return" : "call", count, opcode,
									unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
									data.read_word(0x3b14), data.read_word(0x3b10), data.read_word(0xbdac),
									static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
							}
					static constexpr offs_t settings_calls[] = {
						0x38c55, 0x38c5a, 0x38c64, 0x38c75, 0x38c80, 0x38c89,
						0x38c93, 0x38ca1, 0x38caf, 0x38cb9, 0x38cc0, 0x38cc6
					};
					for (offs_t site : settings_calls)
						if ((address == site || address == site + 2) && m_native_settings_call_traces++ < 64)
						{
							auto const disable = machine().disable_side_effects();
							logerror("mu4_native_settings_call: site=%06x point=%s opcode=%04x ar2=%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t metadata_calls[] = {0x3c414, 0x3c41f, 0x3c43e, 0x3c441, 0x3c44f};
					for (offs_t site : metadata_calls)
						if ((address == site || address == site + 2) && m_native_metadata_call_traces++ < 40)
						{
							auto const disable = machine().disable_side_effects();
							logerror("mu4_native_metadata_call: site=%06x point=%s opcode=%04x sp=%04x ar2=%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t allocation_calls[] = {0x3c4e9, 0x3c4f0, 0x3c4f7, 0x3c518, 0x3c552};
					for (offs_t site : allocation_calls)
						if ((address == site || address == site + 2) && m_native_allocation_call_traces++ < 40)
						{
							auto const disable = machine().disable_side_effects();
							logerror("mu4_native_allocation_call: site=%06x point=%s opcode=%04x sp=%04x ar1=%04x ar2=%04x ar6=%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t cache_update_calls[] = {
						0x3c5dd, 0x3c5df, 0x3c5ea, 0x3c63c, 0x3c63e, 0x3c64c,
						0x3c67a, 0x3c67c, 0x3c68a, 0x3c6ba, 0x3c6bd
					};
					for (offs_t site : cache_update_calls)
						if ((address == site || address == site + 2) && m_native_cache_update_traces++ < 48)
						{
							auto const disable = machine().disable_side_effects();
							auto &data = m_cpu->space(AS_DATA);
							u16 const ar6 = m_cpu->state_int(tms320c54x_device::STATE_AR6);
							logerror("mu4_native_cache_update: site=%06x point=%s opcode=%04x sp=%04x ar6=%04x mode=%04x cache=%04x,%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), ar6,
								data.read_word(u16(ar6 + 2)), data.read_word(0xb4f8), data.read_word(0xb4f9),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t cache_mode_points[] = {0x3c5f3, 0x3c5f8, 0x3c5fb, 0x3c5ff, 0x3c65c, 0x3c6ad};
					for (unsigned i = 0; i < std::size(cache_mode_points); ++i)
						if (address == cache_mode_points[i] && m_native_cache_mode_traces[i]++ < 4)
						{
							auto const disable = machine().disable_side_effects();
							u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
							logerror("mu4_native_cache_mode: pc=%06x opcode=%04x ar0=%04x ar2=%04x sp=%04x input=%04x st0=%04x a=%010llx\n",
								unsigned(address), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), sp,
								m_cpu->space(AS_DATA).read_word(u16(sp + 7)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					if ((address <= 0x392c2 || (address >= 0x3941d && address <= 0x3957f)) &&
						(opcode & 0xfff8) == 0xf980 && m_native_startup_subcall_traces++ < 64)
					{
						auto const disable = machine().disable_side_effects();
						logerror("mu4_native_startup_subcall: pc=%06x target=%06x\n", unsigned(address),
							unsigned((u32(opcode & 7) << 16) | m_cpu->space(AS_PROGRAM).read_word(address + 1)));
					}
					static constexpr offs_t points[] = {0x39115, 0x39117, 0x3911a, 0x3911c, 0x3bb3d, 0x3bb5f, 0x3bbd6};
					if (std::find(std::begin(points), std::end(points), address) == std::end(points) ||
						m_native_config_call_traces++ >= 24) return;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					logerror("mu4_native_startup_read: pc=%06x opcode=%04x sp=%04x ar1=%04x ar2=%04x buffer1444=%04x,%04x,%04x,%04x,%04x,%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
						data.read_word(0x1444), data.read_word(0x1445), data.read_word(0x1446),
						data.read_word(0x1447), data.read_word(0x1448), data.read_word(0x1449));
				});
			auto const lookup_observer = [this](offs_t address, u16 &opcode, u16)
				{
					// These snapshots are pre-execution; only known opcode boundaries are selected.
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != u16(address + 1)) return;
					if (address >= 0x3035 && address <= 0x305b && address != 0x3035 && address != 0x3050 &&
						address != 0x3052 && address != 0x3053 && address != 0x3057 && address != 0x3058 && address != 0x305a) return;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					if (data.read_word(0x145e) != 6 || m_native_lookup_operand_traces++ >= 40) return;
					u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
					logerror("mu4_native_lookup_operand: pc=%06x opcode=%04x ar0=%04x ar2=%04x ar6=%04x a=%010llx b=%010llx stack=%04x,%04x,%04x,%04x st1=%04x ifr=%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)),
						static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
						static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)) & 0xffffffffffULL,
						data.read_word(sp), data.read_word(u16(sp+1)), data.read_word(u16(sp+2)), data.read_word(u16(sp+3)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3bb7c, 0x3bb7c, "mu4_native_lookup_limit", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3bd13, 0x3bd13, "mu4_native_lookup_address", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x329d, 0x329d, "mu4_native_lookup_byte", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3035, 0x305b, "mu4_native_lookup_nand_address", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3234, 0x327e, "mu4_native_lookup_cache",
				[this](offs_t address, u16 &opcode, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					static constexpr u16 points[] = {0x3234, 0x324c, 0x325d, 0x326e, 0x327b, 0x327e};
					auto const found = std::find(std::begin(points), std::end(points), address);
					if (found == std::end(points)) return;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					if (data.read_word(0x145e) != 6 || m_native_cache_traces[found - std::begin(points)]++ >= 4) return;
					u16 const source = m_cpu->state_int(tms320c54x_device::STATE_AR6);
					logerror("mu4_native_cache: pc=%06x opcode=%04x ar0=%04x ar1=%04x ar2=%04x source=%04x count=%04x row=%04x,%04x a=%010llx\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
						source, data.read_word(u16(source+0x11)), data.read_word(u16(source+0x12)), data.read_word(u16(source+0x13)),
						static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
				});
			if (measurement_profile()) install_measurement_observers();
			m_cpu->space(AS_DATA).install_write_tap(0x48, 0x49, "mu4_native_mcbsp_config",
				[this](offs_t offset, u16 &value, u16)
				{
					if (machine().side_effects_disabled()) return;
					if (offset == 0x48) m_native_mcbsp_index = value;
					if (m_native_mcbsp_trace++ < 64)
						logerror("mu4_native_mcbsp: write address=%04x index=%04x value=%04x pc=%06x\n", unsigned(offset), m_native_mcbsp_index, value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_read_tap(0x49, 0x49, "mu4_native_mcbsp_status",
				[this](offs_t, u16 &value, u16)
				{
					if (machine().side_effects_disabled()) return;
					if (m_native_mcbsp_index == 1)
					{
						++m_native_mcbsp_polls;
						m_native_mcbsp_status = value;
					}
				});
			m_cpu->space(AS_DATA).install_read_tap(0x4a, 0x4a, "mu4_native_adjacent_status",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_adjacent_reads; });
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x3538);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 30;
			if (system_bios() >= 8) m_command->adjust(attotime::from_msec(1));
			m_check->adjust(attotime::from_seconds(8));
			return;
		}
		if (m_phase == 30)
		{
			if (!m_native_entry_reads || !m_native_far_reads)
				fatalerror("MU4 original program transfer missing entry=%u far=%u pc=%06x", m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)));
			if (system_bios() != 13 && m_serial_tx_words != std::vector<u16>({0x0c10, 0x0818, 0x0a01, 0x0e53, 0x1023, 0x1201}))
				fatalerror("MU4 original serial-setup sequence mismatch");
			if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) fatalerror("MU4 original native stream encountered an illegal instruction");
			if (system_bios() >= 2 && (m_stream_words < native_stream_target() || m_dma_completions < native_stream_target() / 128 ||
				m_native_din_words < native_stream_target() || m_rx_dma_completions < native_stream_target() / 128))
				fatalerror("MU4 original streaming block incomplete tx=%u din=%u tx_dma=%u rx_dma=%u pc=%06x illegal=%u",
					m_stream_words, m_native_din_words, m_dma_completions, m_rx_dma_completions,
					unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
			if (system_bios() >= 2) logerror("mu4_native_stream: PASS words=%u dma_completions=%u\n", m_stream_words, m_dma_completions);
			if (system_bios() >= 2 && system_bios() != 13)
			{
				for (unsigned i = 0; i < 64; ++i)
					if (m_cpu->space(AS_DATA).read_word(0x1980 + i) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x19c0 + i) != 0x5678)
						fatalerror("MU4 native converted-ADC buffer mismatch at frame %u", i);
				logerror("mu4_native_receive: PASS converted_fixture=1 frames=64 din_words=%u rx_dma_completions=%u sorted_buffer=1980,19c0\n", m_native_din_words, m_rx_dma_completions);
				if (system_bios() >= 3 && system_bios() != 14)
				{
					if (m_native_dma_tx_vectors < 7 || m_native_dma_tx_handler < 7)
						fatalerror("MU4 original TX interrupt delivery incomplete vectors=%u handler=%u",
							m_native_dma_tx_vectors, m_native_dma_tx_handler);
					for (unsigned i = 0; i < 64; ++i)
						if (m_cpu->space(AS_DATA).read_word(0x1900 + i) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x1940 + i) != 0x5678)
							fatalerror("MU4 native reloaded RX buffer mismatch at frame %u", i);
					logerror("mu4_native_sustained: PASS words=%u din_words=%u tx_blocks=%u rx_blocks=%u reload_buffer=1900,1940\n",
						m_stream_words, m_native_din_words, m_dma_completions, m_rx_dma_completions);
					logerror("mu4_native_interrupts: PASS tx_vectors=%u tx_handler=%u rx_vectors=%u\n",
						m_native_dma_tx_vectors, m_native_dma_tx_handler, m_native_dma_rx_vectors);
				}
			}
			logerror("mu4_native_entry: PASS original_transfer=1 entry_reads=%u far_reads=%u pc=%06x illegal=%u idle=%u pmst=%04x\n",
				m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
			logerror("mu4_native_dma_vector_counts: rx=%u tx=%u imr=%04x ifr=%04x st1=%04x\n",
				m_native_dma_rx_vectors, m_native_dma_tx_vectors,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
			u16 pending;
			{ auto const disable = machine().disable_side_effects(); pending = m_cpu->space(AS_DATA).read_word(0xb633); }
			{
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				logerror("mu4_native_dispatch_snapshot: word007e=%04x descriptor806e=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x writes=%u,%u\n",
					data.read_word(0x7e), data.read_word(0x806e), data.read_word(0x806f), data.read_word(0x8070), data.read_word(0x8071),
					data.read_word(0x8072), data.read_word(0x8073), data.read_word(0x8074), data.read_word(0x8075), m_native_dispatch_traces, m_native_descriptor_traces);
			}
			logerror("mu4_native_stream_pending_counts: sets=%u clears=%u reads=%u final=%04x\n",
				m_native_stream_pending_sets, m_native_stream_pending_clears, m_native_stream_pending_reads, pending);
			logerror("mu4_native_consumer_path_counts: mode_gate=%u flag94de_gate=%u flag0062_gate=%u transfer_entry=%u mode1_test=%u transfer_exit=%u mode_writes=%u\n",
				m_native_consumer_paths[0], m_native_consumer_paths[1], m_native_consumer_paths[2], m_native_consumer_paths[3],
				m_native_consumer_paths[4], m_native_consumer_paths[5], m_native_consumer_mode_writes);
			{
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				logerror("mu4_native_command_paths: enqueue=%u dequeue=%u parser=%u tx_queue=%u command_read=%u dispatch=%u rx_indices=%04x,%04x tx_indices=%04x,%04x queued_words=%04x parser_state=%04x\n",
					m_native_command_paths[0], m_native_command_paths[1], m_native_command_paths[2], m_native_command_paths[3],
					m_native_command_paths[4], m_native_command_paths[5], data.read_word(0x1657), data.read_word(0x1658),
					data.read_word(0x1659), data.read_word(0x165a), data.read_word(0x165e), data.read_word(0x1661));
			}
			if (system_bios() >= 4)
			{
				{
					auto const disable = machine().disable_side_effects();
					if (m_cpu->space(AS_DATA).read_word(0x8074) != 2 || m_cpu->space(AS_DATA).read_word(0x8075) != 0x9545)
						fatalerror("MU4 original stream descriptor has no expected consumer binding");
				}
				if (!m_native_worker_window_started || machine().time() - m_native_worker_window_start < attotime::from_msec(native_worker_tail_ms()))
					fatalerror("MU4 worker observation ended before its %u ms tail", native_worker_tail_ms());
				logerror("mu4_native_worker_window: PASS tail_ms=%u checked_din_prefix=1024 pending=%04x reads=%u\n", native_worker_tail_ms(), pending, m_native_stream_pending_reads);
				logerror("mu4_native_worker_storage: tail_data_reads=%u\n", m_data_reads - m_native_tail_data_reads);
				trace_native_iterator("end");
				logerror("mu4_native_stream_binding: PASS descriptor=806e entry=029545 consumer_entries=%u\n", m_native_stream_consumer_entries);
				logerror("mu4_native_startup_counts: main=%u continuation=%u helper=%u selection_start=%u\n",
					m_native_startup_fetches[0], m_native_startup_fetches[1], m_native_startup_fetches[2], m_native_startup_fetches[3]);
				if (system_bios() >= 7)
				{
					if (!m_native_startup_fetches[1] || !m_native_startup_fetches[2] ||
						!m_native_stream_consumer_entries || !m_native_stream_pending_reads || m_native_stream_pending_clears < 2)
						fatalerror("MU4 longer startup did not activate its original streaming consumer");
					logerror("mu4_native_worker_activation: PASS consumer_entries=%u reads=%u clears=%u full_boot=0\n",
						m_native_stream_consumer_entries, m_native_stream_pending_reads, m_native_stream_pending_clears);
				}
				for (unsigned i = 0; i < std::size(buffer_calls); ++i)
					logerror("mu4_native_buffer_counts: site=%06x calls=%u returns=%u\n",
						unsigned(buffer_calls[i]), m_native_buffer_calls[i], m_native_buffer_returns[i]);
			}
			logerror("mu4_native_mcbsp_boundary: index=%04x status=%04x polls=%u adjacent_reads=%u config_writes=%u tx_words=%u tx_irqs=%u controller_modeled=partial\n",
				m_native_mcbsp_index, m_native_mcbsp_status, m_native_mcbsp_polls, m_native_adjacent_reads, m_native_mcbsp_trace,
				unsigned(m_serial_tx_words.size()), m_serial_tx_irqs);
			if (system_bios() == 8)
			{
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				if (m_native_command_cursor != 8 || m_native_command_paths[0] != 1 || m_native_command_paths[3] != 3 ||
					data.read_word(0x1657) != 8 || data.read_word(0x1658) != 8 || data.read_word(0x1659) != 3 ||
					data.read_word(0x165a) != 1 || data.read_word(0x165e) != 4 || data.read_word(0x33) != 0x7f || data.read_word(0xbb80))
					fatalerror("MU4 native framed request did not reach its original queued-command/acknowledgement boundary");
				logerror("mu4_native_framed_request: PASS rx_words=8 queued_command=1 ack_words=3 first_tx_register=007f tx_wire=0 processing=0\n");
			}
			if (system_bios() >= 9)
			{
				logerror("mu4_native_command_wire: words=%u tx_irqs=%u\n", unsigned(m_command_tx_words.size()), m_command_tx_irqs);
				for (unsigned i = 0; i < std::min<unsigned>(m_command_tx_words.size(), 64); ++i)
					logerror("mu4_native_command_wire_word: index=%u value=%04x\n", i, m_command_tx_words[i]);
				if (system_bios() == 9)
				{
					std::vector<u16> expected = {0x7f, 1, 0x55, 0x1e, 5, 0xaa, 1, 0x71, 1, 0, 0, 0x80, 0x40, 0x55};
					std::vector<u16> const response(expected.begin() + 3, expected.end());
					expected.insert(expected.end(), response.begin(), response.end());
					expected.insert(expected.end(), response.begin(), response.end());
					if (m_command_tx_words != expected || m_command_wire_bit_count || m_command_ack_cursor)
						fatalerror("MU4 unacknowledged status response did not reproduce three complete wire copies");
					logerror("mu4_native_status_noack: PASS tx_words=36 pin_decode=1 response_copies=3 peer_ack=0 processing=0\n");
				}
				if (system_bios() >= 10 && system_bios() != 13 && system_bios() != 14 &&
					(system_bios() != 15 || m_native_command_paths[5]))
				{
					verify_native_status();
				}
			}
			if (measurement_profile()) verify_native_measurement();
			else if (finish_native_replay()) return;
			if (system_bios() == 14)
			{
				if (m_native_reset_leg != 2 || m_native_reset_settings_mask != 15)
					fatalerror("MU4 native reset fixture missed its reset or firmware-created settings file");
				logerror("mu4_native_reset_storage: PASS mid_rx_byte=1 controller_cleared=1 retained_payloads=6 settings_mask=f original_reload=1\n");
				if (m_native_command_cursor || m_command_rx_irqs || !m_command_tx_words.empty() || m_native_command_paths[5])
					fatalerror("MU4 reset frontier changed; re-evaluate the native control-loop restart contract");
				logerror("mu4_native_reset_frontier: control_restart=0 dispatcher=0 rx_words=0 tx_words=0 tail_ms=%u stream_equivalence=0 board_reset=0 music_decode=0\n", native_worker_tail_ms());
			}
			capture_native_ram("settled");
			if (system_bios() == 15)
			{
				if (m_files_checked != 6 || m_native_reset_settings_mask != 15)
					fatalerror("MU4 fresh-process retained medium lacks original uploads or settings");
				if (!m_native_command_paths[5] && (m_native_command_cursor || m_command_rx_irqs || !m_command_tx_words.empty()))
					fatalerror("MU4 retained control frontier has partial unexplained traffic");
				logerror("mu4_native_retained_storage: PASS fresh_process=1 original_payloads=6 settings_mask=f erase=0 runtime_snapshot=0\n");
				logerror("mu4_native_retained_control: active=%u rx_words=%u tx_words=%u tail_ms=%u board_boot=0 music_decode=0\n",
					m_native_command_paths[5] ? 1 : 0, m_command_rx_irqs, unsigned(m_command_tx_words.size()), native_worker_tail_ms());
			}
			machine().schedule_exit();
			return;
		}
		if (m_phase == 20)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
				fatalerror("MU4 segment illegal opcode pc=%04x cursor=%u", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), m_segment_cursor);
			// XF is the original firmware's external busy indication, not a RAM-state override.
			if (m_serial_ready || BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 13))
			{
				if (++m_segment_waits > 10000) fatalerror("MU4 segment serial/busy timeout cursor=%u pc=%04x", m_segment_cursor, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			}
			else if (m_segment_cursor < m_segment.length())
			{
				deliver_serial(m_segment[m_segment_cursor++]);
				m_segment_waits = 0;
			}
			else if (!data.read_word(0x1f6) && data.read_word(0x4bbf) == data.read_word(0x4bc0))
			{
				if (m_serial_reads != m_segment.length() + 2 || data.read_word(0x1a2) != 0x40 || data.read_word(0x1f9) != 0x40 || data.read_word(0x584) != 7)
					fatalerror("MU4 original segment checksum/receive mismatch reads=%u checksum=%04x calculated=%04x", m_serial_reads, data.read_word(0x1a2), data.read_word(0x1f9));
				logerror("mu4_initdisk_segment: PASS segments=7 original_bytes=%u final_checksum=0040 ring_drained=1\n", m_segment_cursor);
				m_phase = 19;
				machine().schedule_soft_reset();
				return;
			}
			else if (++m_segment_waits > 10000)
				fatalerror("MU4 segment completion timeout pc=%04x state=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), data.read_word(0x1f6));
			// Bounded word-ingress cadence, not recovered physical McBSP timing.
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 18)
		{
			auto &data = m_cpu->space(AS_DATA);
			unsigned const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
			if (!((pc >= 0x31f0 && pc <= 0x3203) || (pc >= 0x3499 && pc <= 0x34a4)) ||
				m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) || data.read_word(0x1f6) || data.read_word(0x1f5) ||
				data.read_word(0x4bbf) != 0xff || data.read_word(0x4bc0) != 0xff ||
				data.read_word(0x10d) != 0xaa55 || data.read_word(0x19d) != 0xbbcc)
				fatalerror("MU4 original initialization did not reach receive boundary pc=%04x", pc);
			logerror("mu4_initdisk_initialize: PASS original_init=1 erased_input=1 receiver_wait=1 no_received_words=1 writes=%u reads=%u\n",
				unsigned(m_writes.size()), m_data_reads);
			m_phase = 20;
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 14 || m_phase == 15 || m_phase == 16)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 serial ISR did not return to idle phase=%u pc=%04x sp=%04x illegal=%u", m_phase,
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
			if (m_phase == 14)
			{
				if (data.read_word(0x4bbf) != 0xff || data.read_word(0x4bc0) != 0xff)
					fatalerror("MU4 original serial ring initialization mismatch");
				deliver_serial(0x12);
			}
			else
			{
				unsigned const index = m_phase - 15;
				if (m_serial_ready || m_serial_reads != index + 1 || data.read_word(0x4bbf) != index ||
					data.read_word(0x4abf + index) != (index ? 0x34 : 0x12))
					fatalerror("MU4 original serial ISR ring delivery mismatch");
				if (m_phase == 16)
				{
					m_phase = 17;
					machine().schedule_soft_reset();
					return;
				}
				deliver_serial(0x34);
			}
			++m_phase;
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 8)
		{
			auto &data = m_cpu->space(AS_DATA);
			for (unsigned i = 0; i < 512; ++i)
			{
				u16 const word = data.read_word(0x1746 + i / 2);
				u8 const expected = (i & 1) ? u8(word) : u8(word >> 8);
				if (m_nand->data_r() != expected) fatalerror("MU4 recovery template mismatch byte=%u", i);
			}
			logerror("mu4_storage_template: PASS original_template=512 physical_row=32\n");
			m_phase = 9;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 1 || m_phase == 3 || m_phase == 11)
		{
			if (m_nand->is_busy()) fatalerror("MU4 storage test pattern program did not complete");
			++m_phase;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 5)
		{
			if (m_nand->is_busy()) fatalerror("MU4 flush media read still busy");
			for (unsigned i = 0; i < 256; ++i)
			{
				u16 const low = m_nand->data_r();
				u16 const value = low | (u16(m_nand->data_r()) << 8);
				if (value != pattern(m_page * 256 + i))
					fatalerror("MU4 flush media mismatch page=%u word=%u actual=%04x", m_page, i, value);
			}
			for (unsigned i = 0; i < 16; ++i)
				if (m_nand->data_r() != 0xff) fatalerror("MU4 flush spare fill mismatch");
			if (++m_page < 32) { start_media_read(); return; }
			logerror("mu4_storage_flush: PASS pages=32 words=8192 spare=512 erase=1 program=32 readback=32\n");
			m_phase = 6;
			machine().schedule_soft_reset();
			return;
		}
		u16 const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
				!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
			fatalerror("MU4 original storage routine did not finish: pc=%04x illegal=%u writes=%u reads=%u",
				pc, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_writes.size()), m_data_reads);
		if (m_phase == 9)
		{
			unsigned mutations = 0;
			for (u16 value : m_writes) mutations += value == 0x0160 || value == 0x0180;
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 1 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_data_reads || mutations)
				fatalerror("MU4 format prerequisite contract mismatch");
			logerror("mu4_storage_format: PASS missing_boot_record rejected=1 media_mutations=0\n");
			m_phase = 10;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 10 || m_phase == 12)
		{
			unsigned spare_reads = 0, status_reads = 0;
			for (u16 value : m_writes) { spare_reads += value == 0x0150; status_reads += value == 0x0170; }
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != (m_phase == 10 ? 0xff : 0) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				spare_reads != (m_phase == 10 ? 2 : 1) || status_reads != spare_reads)
				fatalerror("MU4 InitDisk marker contract mismatch a=%llx sp=%04x spare=%u status=%u reads=%u busy=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), spare_reads, status_reads, m_data_reads, m_busy_reads);
			if (m_phase == 12)
			{
				logerror("mu4_initdisk_marker: PASS erased=ff bad_marker=00 original_spare_reader=1\n");
				m_phase = 13;
				machine().schedule_soft_reset();
				return;
			}
			// External bad-block fixture: program only spare byte 5 of physical page zero.
			m_nand->command_w(0x50); m_nand->command_w(0x80);
			m_nand->address_w(5); m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(0);
			m_nand->data_w(0); m_nand->command_w(0x10);
			m_phase = 11;
			m_check->adjust(attotime::from_usec(201));
			return;
		}
		if (m_phase == 13)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || data.read_word(0x2000))
				fatalerror("MU4 InitDisk scan return/reserved bitmap mismatch");
			for (unsigned i = 0; i < 256; ++i)
				// 256d byte-swaps its source bitmap in place before NAND writeback.
				if (data.read_word(0x2100 + i) != (i == 0 ? 0x0100 : 0))
					fatalerror("MU4 InitDisk bitmap mismatch group=%u value=%04x", i, data.read_word(0x2100 + i));
			logerror("mu4_initdisk_scan: PASS blocks=4096 bad_block=0 reserved_bad=0 reads=%u busy_reads=%u\n", m_data_reads, m_busy_reads);
			m_phase = 14;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 17)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0x1234 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				m_cpu->space(AS_DATA).read_word(0x4bc0) != 1)
				fatalerror("MU4 original serial word consumer mismatch");
			logerror("mu4_initdisk_serial: PASS vector=00d8 register=0031 words=2 value=1234 firmware_ring=1\n");
			m_phase = 18;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 6)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 1 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_data_reads)
				fatalerror("MU4 erased mount contract mismatch a=%llx sp=%04x reads=%u busy=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_data_reads, m_busy_reads);
			logerror("mu4_storage_mount: PASS erased_boot_sector rejected=1 reads=%u busy_reads=%u\n", m_data_reads, m_busy_reads);
			m_phase = 7;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 19)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_data_reads)
				fatalerror("MU4 InitData mount of original InitDisk medium failed a=%llx sp=%04x",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
			logerror("mu4_initdisk_mount: PASS original_initdata_mount=1 original_initdisk_media=1\n");
			m_phase = 21;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 21 || m_phase == 23)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 directory reader did not return with balanced stack");
			logerror("mu4_directory: a=%llx sp=%04x name=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x ext=%04x,%04x,%04x length=%04x,%04x\n",
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				data.read_word(0x36b0), data.read_word(0x36b1), data.read_word(0x36b2), data.read_word(0x36b3),
				data.read_word(0x36b4), data.read_word(0x36b5), data.read_word(0x36b6), data.read_word(0x36b7),
				data.read_word(0x36b9), data.read_word(0x36ba), data.read_word(0x36bb), data.read_word(0x36c2), data.read_word(0x36c3));
			if (m_phase == 23 && m_cpu->state_int(tms320c54x_device::STATE_A) == 1)
			{
				if (m_files_checked != 6) fatalerror("MU4 file count mismatch files=%u", m_files_checked);
				logerror("mu4_file_readback: PASS files=6 original_readers=1 complete_payloads=1\n");
				m_phase = 24;
				machine().schedule_soft_reset();
				return;
			}
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_A) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 original directory reader failed pc=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			m_file_size = (unsigned(data.read_word(0x36c2)) << 16) | data.read_word(0x36c3);
			if (((system_bios() == 14 && m_native_reset_leg == 2) || system_bios() == 15) && (m_file_size == 1800 || m_file_size == 56))
			{
				// Original startup creates these settings files. Preserve them and enumerate
				// onward through the unchanged next-entry ABI, not a fabricated list.
				static constexpr char const *settings[] = { "TRACKLST", "SETTING1", "SETTING2", "SETTING3" };
				unsigned match = std::size(settings);
				for (unsigned entry = 0; entry < std::size(settings); ++entry)
				{
					bool equal = true;
					for (unsigned i = 0; i < 9; ++i) equal &= data.read_word(0x36b0 + i) == u8(settings[entry][i]);
					if (equal) match = entry;
				}
				if (match == std::size(settings) || m_file_size != (match ? 56 : 1800))
					fatalerror("MU4 reset encountered an unknown non-upload file");
				if (data.read_word(0x36b9) != 'B' || data.read_word(0x36ba) != 'I' || data.read_word(0x36bb) != 'N' ||
					BIT(m_native_reset_settings_mask, match)) fatalerror("MU4 reset settings entry is invalid or duplicated");
				m_native_reset_settings_mask |= 1U << match;
				logerror("mu4_native_reset_settings: name=%s.BIN bytes=%u retained=1\n", settings[match], m_file_size);
				m_phase = 23;
				machine().schedule_soft_reset();
				return;
			}
			m_file_source = 0;
			for (unsigned offset = 0; offset < m_segment.length(); )
			{
				unsigned const size = (unsigned(m_segment[offset + 2]) << 24) | (unsigned(m_segment[offset + 3]) << 16) |
					(unsigned(m_segment[offset + 4]) << 8) | m_segment[offset + 5];
				if (size == m_file_size)
				{
					if (m_file_source) fatalerror("MU4 ambiguous source size");
					m_file_source = offset + 6;
					m_file_marker = (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
				}
				offset += size + 10;
			}
			if (!m_file_source || (m_file_size & 1)) fatalerror("MU4 file size has no original segment size=%u", m_file_size);
			char const *name = nullptr;
			switch (m_file_marker)
			{
			case 0xaa22: name = "MCUSI16 "; break;
			case 0xaa44: name = "MP3SI16 "; break;
			case 0xaa88: name = "RERSI16 "; break;
			case 0xaabb: name = "AACSI16 "; break;
			case 0xaadd: name = "USBSI16 "; break;
			case 0xaa99: name = "RELSI16 "; break;
			default: fatalerror("MU4 unexpected file marker %04x", m_file_marker);
			}
			for (unsigned i = 0; i < 9; ++i)
				if (data.read_word(0x36b0 + i) != u8(name[i])) fatalerror("MU4 directory name mismatch marker=%04x", m_file_marker);
			if (data.read_word(0x36b9) != 'B' || data.read_word(0x36ba) != 'I' || data.read_word(0x36bb) != 'N')
				fatalerror("MU4 directory extension mismatch");
			m_file_cursor = 0;
			m_phase = 22;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 22)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_A) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 original file reader failed pc=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			for (unsigned i = 0; i < m_read_words; ++i)
			{
				unsigned const source = m_file_source + m_file_cursor + i * 2;
				u16 const expected = (u16(m_segment[source]) << 8) | m_segment[source + 1];
				if (data.read_word(0x6000 + i) != expected)
					fatalerror("MU4 file mismatch marker=%04x byte=%u actual=%04x expected=%04x", m_file_marker, m_file_cursor + i * 2, data.read_word(0x6000 + i), expected);
			}
			m_file_cursor += m_read_words * 2;
			if (m_file_cursor == m_file_size)
			{
				if (std::find(m_verified_files.begin(), m_verified_files.end(), m_file_marker) != m_verified_files.end())
					fatalerror("MU4 directory repeats segment %04x", m_file_marker);
				m_verified_files.push_back(m_file_marker);
				++m_files_checked;
				logerror("mu4_file_payload: PASS marker=%04x bytes=%u\n", m_file_marker, m_file_size);
				m_phase = 23;
			}
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 7)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_busy_reads)
				fatalerror("MU4 original template writer failed");
			m_nand->command_w(0); m_nand->address_w(0); m_nand->address_w(32);
			m_nand->address_w(0); m_nand->address_w(0);
			m_phase = 8;
			m_check->adjust(attotime::from_usec(11));
			return;
		}
		if (m_phase == 4)
		{
			unsigned erase = 0, program = 0;
			for (u16 value : m_writes) { erase += value == 0x0160; program += value == 0x0180; }
			if (erase != 1 || program != 32 || !m_busy_reads ||
				m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x4000) ||
				m_cpu->space(AS_DATA).read_word(0x3b14) != 0)
				fatalerror("MU4 flush failed a=%llx sp=%04x erase=%u program=%u busy_reads=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), erase, program, m_busy_reads);
			m_phase = 5;
			start_media_read();
			return;
		}
		std::vector<u16> const expected = m_phase ?
			std::vector<u16>{0x0100, 0x0200, 0x0200, 0x0200, 0x0201} :
			std::vector<u16>{0x0100, 0x0200, 0x0201, 0x0200, 0x0200};
		// A slower reset clock may reach the BIO poll after NAND is ready.
		// Require the device's minimum data latency, not a CPU-speed-dependent
		// count of polls while it was busy.
		if (m_writes != expected || m_data_reads != 2 ||
				m_cpu->state_int(tms320c54x_device::STATE_A) != (m_phase ? 0x3412 : 0xffff) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				m_first_data - m_read_started < attotime::from_usec(10))
		{
			for (u16 word : m_writes) logerror("mu4_storage_write=%04x\n", word);
			fatalerror("MU4 storage contract mismatch: pc=%04x a=%010llx sp=%04x busy_reads=%u data_reads=%u",
				pc, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_busy_reads, m_data_reads);
		}
		if (!m_phase)
		{
			logerror("mu4_storage_erased: PASS row=1 value=ffff bio_reads=%u busy_reads=%u\n", m_bio_reads, m_busy_reads);
			// External device-conformance pattern, not a provisioned filesystem or firmware state.
			m_nand->command_w(0); m_nand->command_w(0x80);
			m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(1);
			m_nand->data_w(0x12); m_nand->data_w(0x34); m_nand->command_w(0x10);
			m_phase = 1;
			m_check->adjust(attotime::from_usec(201));
			return;
		}
		logerror("mu4_storage_original: PASS rows=1,65536 values=ffff,3412 writes=5 reads=2 bio_reads=%u busy_reads=%u\n",
			m_bio_reads, m_busy_reads);
		// Non-erased marker makes the original block erase necessary before programming.
		m_nand->command_w(0); m_nand->command_w(0x80);
		m_nand->address_w(0); m_nand->address_w(64); m_nand->address_w(0); m_nand->address_w(0);
		m_nand->data_w(0); m_nand->command_w(0x10);
		m_phase = 3;
		m_check->adjust(attotime::from_usec(201));
	}
};
static INPUT_PORTS_START(mu4_storage_test) INPUT_PORTS_END
INPUT_CHANGED_MEMBER(mu4_storage_test_state::mcu_dsp_interrupt)
{
	// Logical assertion of the active-low board net, not an inferred MCU action.
	m_cpu->set_input_line(2, newval ? ASSERT_LINE : CLEAR_LINE);
	logerror("mu4_mcu_int2_input: asserted=%u net=UC_DSP_INT cpu_irq=2 t=%.9f source=bench_pin firmware_state_forcing=0\n",
		unsigned(bool(newval)), machine().time().as_double());
}
INPUT_CHANGED_MEMBER(mu4_storage_test_state::mcu_status_request)
{
	if (!newval || m_manual_status_requested) return;
	if (!playback_stop_profile() || m_phase != 30 || m_native_command_cursor != 29)
		fatalerror("MU4 manual status query requires completed playback-control ingress");
	// Explicit serial-peer command, not an inferred MSP430 policy or guest poke.
	m_manual_status_requested = true;
	m_command->adjust(attotime::from_msec(1));
	logerror("mu4_manual_status_input: request=1 token=04 t=%.9f source=bench_serial_command firmware_state_forcing=0\n",
		machine().time().as_double());
}
static INPUT_PORTS_START(mu4_native_control)
	PORT_INCLUDE(mu4_storage_test)
	PORT_START("MCU_CONTROL")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_OTHER) PORT_NAME("MCU DSP interrupt asserted")
		PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mu4_storage_test_state::mcu_dsp_interrupt), 0)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_OTHER) PORT_NAME("MCU status request")
		PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mu4_storage_test_state::mcu_status_request), 0)
INPUT_PORTS_END
// Isolated codec ABI probe. This does not alter a running handset or recorder.
class mu4_aac_test_state : public driver_device
{
public:
	mu4_aac_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_cpu(*this, "cpu"),
		  m_container(*this, "container"), m_input(*this, "input") { }
	void test(machine_config &config)
	{
		TMS320C54X(config, m_cpu, 13'000'000);
		m_cpu->set_extended_program(true);
		m_cpu->set_c5416_bus_profile(true);
		m_cpu->set_addrmap(AS_PROGRAM, &mu4_aac_test_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &mu4_aac_test_state::data_map);
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) {
			if (address == 0x2442f && ((m_expected_calls == 174 && m_captured == 172) ||
					(m_expected_calls == 343 && m_captured == 342)))
			{
				auto const disable = machine().disable_side_effects();
				logerror("mu4_aac_tail_reserve: opcode=%04x candidate_index=%04x reserve_limit=%04x extent=%04x remaining=%04x identity=Execute_entry observer_writes=0\n",
					opcode,u16(m_cpu->state_int(tms320c54x_device::STATE_AR2)),u16(m_cpu->state_int(tms320c54x_device::STATE_AR0)),m_cpu->space(AS_DATA).read_word(0xb330),m_cpu->space(AS_DATA).read_word(0xa854));
			}
			if (address == 0x23de7)
			{
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
				if (opcode != 0x800f || ++m_sources_seen > m_expected_calls) fatalerror("MU4 AAC ABI source boundary changed");
				m_sources = {data.read_word(sp+6),data.read_word(sp+5)};
				logerror("mu4_aac_abi_sources: left=%04x right=%04x sp=%04x result=%04x identity=Execute_entry observer_writes=0\n",
					m_sources[0],m_sources[1],sp,u16(m_cpu->state_int(tms320c54x_device::STATE_A)));
			}
			if (address == 0x242cb)
			{
				if (++m_headers > m_expected_calls) fatalerror("MU4 AAC header count exceeded caller bound");
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				logerror("mu4_aac_abi_header: ordinal=%u sync=%04x id=%04x layer=%04x protection=%04x profile=%04x frequency=%04x channels=%04x frame_length=%04x blocks=%04x identity=Execute_entry observer_writes=0\n",
					m_headers,data.read_word(0xa8b8),data.read_word(0xa8b9),data.read_word(0xa8ba),data.read_word(0xa8bb),
					data.read_word(0xa8bc),data.read_word(0xa8bd),data.read_word(0xa8bf),data.read_word(0xa8c4),data.read_word(0xa8c6));
			}
			if (address == m_completion)
			{
				if (opcode != 0xf4e1) fatalerror("MU4 AAC ABI completion opcode changed");
				m_done = true;
			}
			if (m_expected_calls > 1 && m_captured < m_expected_calls && address == m_capture_sites[m_expected_calls > 8 ? 0 : m_captured])
			{
				if (opcode != 0xf495) fatalerror("MU4 AAC session return opcode changed");
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				if (m_sources_seen != m_captured+1)
				{
					logerror("mu4_aac_wrapper_early_return: completed_calls=%u headers=%u sources=%u result=%04x cursor=%04x end=%04x base=%04x index=%04x remaining=%04x input_source=%u window_bytes=%u illegal=%u observer_writes=0\n",
						m_captured,m_headers,m_sources_seen,data.read_word(0x1402),data.read_word(0x1400),data.read_word(0x1401),data.read_word(0xb332),data.read_word(0xb331),data.read_word(0xa854),m_input_offset,m_window_bytes,unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
					fatalerror("MU4 AAC wrapper returned without a new decoded block");
				}
				if (data.read_word(0x1402)) fatalerror("MU4 AAC session decode failed");
				capture_output(m_captured != 0);
				logerror("mu4_aac_session_return: call=%u descriptor=%04x base=%04x index=%04x remaining=%04x result=%04x observer_writes=0\n",
					++m_captured,data.read_word(0x1400),data.read_word(0xb332),data.read_word(0xb331),data.read_word(0xa854),data.read_word(0x1402));
				if (m_expected_calls >= 8 && m_captured % 4 == 0 && m_captured < m_expected_calls)
				{
					// Explicit ABI caller input, not a handset hook: retain the decoder's reservoir.
					unsigned const cursor = data.read_word(0x1400);
					if (cursor < 0x162e || cursor > 0x1a2e) fatalerror("MU4 AAC refill cursor bounds");
					m_input_offset += 2*(cursor-0x162e);
					load_input_window();
					logerror("mu4_aac_session_refill: after_call=%u source_byte=%u old_cursor=%04x cursor=162e end=%04x bytes=%u remaining=%04x reservoir_high=%04x reservoir_low=%04x fixture_input=1 decoder_state_writes=0\n",
						m_captured,m_input_offset,cursor,0x162e + m_window_bytes/2,m_window_bytes,data.read_word(0xa854),data.read_word(0xa858),data.read_word(0xa859));
				}
			}
		});
	}
private:
	// Original playback workspace; 8000 aliases live coefficient state at 80de.
	static constexpr u16 OUTPUT_BASE = 0x7000;
	required_device<tms320c54x_device> m_cpu;
	required_region_ptr<u8> m_container, m_input;
	emu_timer *m_check = nullptr;
	unsigned m_completion = 0;
	unsigned m_headers = 0;
	unsigned m_sources_seen = 0;
	unsigned m_expected_calls = 1, m_captured = 0;
	std::array<unsigned,8> m_capture_sites{};
	unsigned m_input_offset = 0;
	unsigned m_source_bytes = 0;
	unsigned m_window_bytes = 2048;
	std::array<u16,2> m_sources{};
	unsigned m_input_reads = 0;
	std::array<bool,1024> m_input_seen{};
	bool m_done = false;
	void program_map(address_map &map)
	{
		map(0, 0x7fffff).ram();
		map(0x2000, 0x7fff).rw(FUNC(mu4_aac_test_state::shared_r), FUNC(mu4_aac_test_state::shared_w));
	}
	void data_map(address_map &map) { map(0, 0xffff).ram(); }
	u16 shared_r(offs_t offset) { return m_cpu->space(AS_DATA).read_word(0x2000 + offset); }
	void shared_w(offs_t offset, u16 value) { m_cpu->space(AS_DATA).write_word(0x2000 + offset, value); }
	u16 source_word(unsigned offset)
	{
		if (offset + 1 >= m_container.length()) fatalerror("MU4 AAC source bounds");
		return (u16(m_container[offset]) << 8) | m_container[offset+1];
	}
	virtual void machine_start() override
	{
		m_check = timer_alloc(FUNC(mu4_aac_test_state::check), this);
		m_cpu->space(AS_DATA).install_read_tap(0x162e,0x1a2d,"mu4_aac_abi_input",
			[this](offs_t address, u16 &value, u16 mask) {
				if (machine().side_effects_disabled()) return;
				unsigned const index = address - 0x162e;
				if (2*index+2 > m_window_bytes)
				{
					logerror("mu4_aac_input_exhausted: completed_calls=%u source_byte=%u file_bytes=%u window_source=%u window_bytes=%u address=%04x context_pc=%06x padding_supplied=0 decoder_state_writes=0\n",
						m_captured,m_input_offset+2*index,m_source_bytes,m_input_offset,m_window_bytes,unsigned(address),unsigned(m_cpu->pc()));
					fatalerror("MU4 AAC input exhausted at a word not wholly inside the source");
				}
				u16 const expected = (u16(m_input[m_input_offset+2*index]) << 8) | m_input[m_input_offset+2*index+1];
				if (++m_input_reads > std::max(4096U,m_expected_calls*1024) || mask != 0xffff || value != expected)
					fatalerror("MU4 AAC ABI changed, partial or capped input read");
				m_input_seen[index] = true;
				if (m_expected_calls >= 8)
					logerror("mu4_aac_session_input_read: ordinal=%u index=%u source_byte=%u address=%04x value=%04x mask=%04x context_pc=%06x observer_writes=0\n",
						m_input_reads,index,m_input_offset+2*index,unsigned(address),value,mask,unsigned(m_cpu->pc()));
				else
					logerror("mu4_aac_abi_input_read: ordinal=%u index=%u address=%04x value=%04x mask=%04x context_pc=%06x observer_writes=0\n",
						m_input_reads,index,unsigned(address),value,mask,unsigned(m_cpu->pc()));
			});
	}
	void load_input_window()
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_input_offset >= m_source_bytes) fatalerror("MU4 AAC input window exceeds source");
		m_window_bytes = std::min(2048U,m_source_bytes-m_input_offset) & ~1U;
		for (unsigned i = 0; i < m_window_bytes/2; ++i)
			data.write_word(0x162e + i, (u16(m_input[m_input_offset+2*i]) << 8) | m_input[m_input_offset+2*i+1]);
		data.write_word(0x1400, 0x162e);
		data.write_word(0x1401, 0x162e + m_window_bytes/2);
	}
	virtual void machine_reset() override
	{
		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);
		unsigned segment = 0;
		while (source_word(segment) != 0xaa99)
			segment += 10 + (u32(source_word(segment+2)) << 16) + source_word(segment+4);
		unsigned cursor = segment + 6, records = 0, total = 0;
		unsigned const end = cursor + (u32(source_word(segment+2)) << 16) + source_word(segment+4);
		while (source_word(cursor))
		{
			unsigned const count = source_word(cursor);
			u32 const destination = (u32(source_word(cursor+2)) << 16) | source_word(cursor+4);
			cursor += 6;
			if (cursor + 2*count > end) fatalerror("MU4 AAC upload extent");
			bool const code = destination > 0xffff;
			u32 const base = (destination & 0x8000) ? destination : destination & 0xffff;
			for (unsigned i = 0; i < count; ++i)
			{
				u32 const address = (code ? base & 0x7f0000 : 0) | u16(base+i);
				if (!code && address < 0x80) continue;
				(code ? program : data).write_word(address, source_word(cursor+2*i));
			}
			cursor += 2*count;
			total += count;
			++records;
		}
		if (cursor+2 != end || records != 3127 || total != 49843)
			fatalerror("MU4 AAC original upload coverage changed");
		// Apply only original C-initialization records, never captured live state.
		cursor = 0x27000;
		unsigned initializers = 0;
		while (unsigned const count = program.read_word((cursor++) & 0xffff))
		{
			unsigned const destination = program.read_word((cursor++) & 0xffff);
			if (cursor+count > 0x27692 || destination < 0x60 || destination+count > 0x10000)
				fatalerror("MU4 AAC initialization record bounds cursor=%06x destination=%04x count=%u",cursor,destination,count);
			for (unsigned i = 0; i < count; ++i) data.write_word(destination+i, program.read_word((cursor++) & 0xffff));
			++initializers;
		}
		if (cursor != 0x27692 || initializers != 228) fatalerror("MU4 AAC initialization coverage changed cursor=%06x records=%u",cursor,initializers);
		// Original input descriptor shape, with the first 2048 source bytes.
		m_input_offset = 0;
		// ROM-region allocation includes other BIOS extents, never treat it as source padding.
		m_source_bytes = system_bios() >= 8 ? 64048 : system_bios() == 7 ? 65785 : system_bios() == 1 ? 61912 : 65339;
		if (m_source_bytes > m_input.length()) fatalerror("MU4 AAC source exceeds ROM region");
		load_input_window();
		std::vector<u16> wrapper = {
			0x7718,0x9000,0xf7be,0x7600,0xd43d,0x7601,0,0x7602,0xd43b,
			0xf020,0xaaac,0xf982,0x3d27
		};
		m_expected_calls = system_bios() == 9 ? 343 : system_bios() == 8 ? 1 : system_bios() >= 6 ? 174 : system_bios() == 5 ? 32 : system_bios() == 4 ? 8 : system_bios() == 3 ? 4 : 1;
		m_captured = 0;
		data.write_word(0x1403,m_expected_calls);
		for (unsigned call = 0; call < (m_expected_calls > 8 ? 1 : m_expected_calls); ++call)
		{
			wrapper.insert(wrapper.end(), {0x7600,0x1400,0x7601,OUTPUT_BASE,0xf020,0xaaac,0xf982,0x3d5d,0x80f8,0x1402});
			if (m_expected_calls > 1)
			{
				// Observe only after the result store has retired; the original wrapper owns input advancement.
				wrapper.insert(wrapper.end(), {0xf495,0xf495,0xf495});
				m_capture_sites[call] = 0x1000 + wrapper.size()-1;
			}
		}
		if (m_expected_calls > 8)
		{
			// Ordinary DSP instructions own the loop count; observers never redirect execution.
			wrapper.insert(wrapper.end(), {0x10f8,0x1403,0xf010,1,0x80f8,0x1403,0xf844,0x100d});
		}
		wrapper.push_back(0xf4e1);
		for (unsigned i = 0; i < std::size(wrapper); ++i) program.write_word(0x1000+i, wrapper[i]);
		m_completion = 0x1000 + std::size(wrapper)-1;
		m_done = false;
		m_headers = 0;
		m_sources_seen = 0; m_sources = {};
		m_input_reads = 0; m_input_seen = {};
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x2028);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1800);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x610f);
		m_cpu->set_state_int(STATE_GENPC, 0x1000);
		m_cpu->set_pipeline_frontend(true);
		m_check->adjust(attotime::from_msec(m_expected_calls > 32 ? 45000 : m_expected_calls > 8 ? 8000 : m_expected_calls > 1 ? 2000 : 250));
		logerror("mu4_aac_abi_setup: original_upload_words=%u original_initializers=%u input_bytes=2048 output_base=%04x output_words=2048 constructor=023d27 process=023d5d isolated_abi=1 handset_boot=0 conformance=0\n",total,initializers,OUTPUT_BASE);
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		auto &data = m_cpu->space(AS_DATA);
		logerror("mu4_aac_abi_result: returned=%u result=%04x base=%04x index=%04x remaining=%04x illegal=%u pc=%06x isolated_abi=1 conformance=0\n",
			m_done,data.read_word(0x1402),data.read_word(0xb332),data.read_word(0xb331),data.read_word(0xa854),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)),unsigned(m_cpu->pc()));
		if (!m_done || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) || data.read_word(0x1402))
			fatalerror("MU4 AAC ABI probe did not decode a successful block");
		unsigned unique = 0, highest = 0;
		for (unsigned i = 0; i < m_input_seen.size(); ++i)
			if (m_input_seen[i]) { ++unique; highest = i; }
		logerror("mu4_aac_abi_input_end: reads=%u unique_words=%u highest_index=%u complete=1 observer_writes=0\n",m_input_reads,unique,highest);
		if (m_sources_seen != m_expected_calls || (m_expected_calls > 1 && m_captured != m_expected_calls))
			fatalerror("MU4 AAC session incomplete");
		if (m_expected_calls == 1) capture_output(false);
		machine().schedule_exit();
	}
	void capture_output(bool append)
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_sources[0] < 0x80 || m_sources[1] < 0x80 ||
			m_sources[0] > 0xf800 || m_sources[1] > 0xf800)
			fatalerror("MU4 AAC ABI output source bounds");
		auto const mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
		std::ofstream raw("mu4_aac_abi_raw.s32le",mode);
		std::ofstream converted("mu4_aac_abi_converted.s16le",mode);
		for (unsigned sample = 0; sample < 1024; ++sample)
			for (unsigned channel = 0; channel < 2; ++channel)
			{
				u16 const source = m_sources[channel] + 2*sample;
				u32 const value = (u32(data.read_word(source)) << 16) | data.read_word(source ^ 1);
				for (unsigned byte = 0; byte < 4; ++byte) raw.put(char(value >> (8*byte)));
				u16 const word = data.read_word(OUTPUT_BASE + channel*1024 + sample);
				converted.put(char(word)); converted.put(char(word >> 8));
			}
		raw.close(); converted.close();
		if (!raw || !converted) fatalerror("MU4 AAC ABI output capture failed");
		logerror("mu4_aac_abi_capture: frames=1024 channels=2 raw_bytes=8192 converted_bytes=4096 source=decoder_return_pointers memory=post_wrapper_return conformance=0\n");
	}
};

ROM_START(mu4aac)
	ROM_SYSTEM_BIOS(0, "mpeg4", "Official-derived MPEG-4 transport negative control")
	ROM_SYSTEM_BIOS(1, "mpeg2", "Generated MPEG-2 AAC-LC ABI control")
	ROM_SYSTEM_BIOS(2, "mpeg2four", "Generated MPEG-2 AAC-LC four-call continuity probe")
	ROM_SYSTEM_BIOS(3, "mpeg2refill", "Generated MPEG-2 AAC-LC eight-call input refill probe")
	ROM_SYSTEM_BIOS(4, "mpeg2stream", "Generated MPEG-2 AAC-LC 32-call repeated refill probe")
	ROM_SYSTEM_BIOS(5, "mpeg2eof", "Generated MPEG-2 AAC-LC strict source exhaustion probe")
	ROM_SYSTEM_BIOS(6, "mpeg2lookahead", "Generated MPEG-2 AAC-LC 174-packet prefix with declared packet lookahead")
	ROM_SYSTEM_BIOS(7, "isoal09", "Original ISO MPEG-2 AAC-LC AL09 44.1 kHz one-call ABI probe")
	ROM_SYSTEM_BIOS(8, "isoal09stream", "Original ISO MPEG-2 AAC-LC AL09 strict source exhaustion probe")
	ROM_REGION(741916, "container", 0)
	ROM_LOAD("mu4_initdata_container.bin", 0, 741916, CRC(e0c05bf2) SHA1(5ff0b99c8d93b6ef2cda0bcd002810a4ab7a0e8f))
	ROM_REGION(65785, "input", 0)
	ROMX_LOAD("al00_44-remux.adts", 0, 61912, CRC(ca68dd78) SHA1(372ec00b9683403e2db6ffe2dc9119b8c07a48b7), ROM_BIOS(0))
	ROMX_LOAD("generated-mpeg2.aac", 0, 65339, CRC(874fe22e) SHA1(6047afd4a8246d838726b7ddfdb9c6185f7d6f16), ROM_BIOS(1))
	ROMX_LOAD("generated-mpeg2.aac", 0, 65339, CRC(874fe22e) SHA1(6047afd4a8246d838726b7ddfdb9c6185f7d6f16), ROM_BIOS(2))
	ROMX_LOAD("generated-mpeg2.aac", 0, 65339, CRC(874fe22e) SHA1(6047afd4a8246d838726b7ddfdb9c6185f7d6f16), ROM_BIOS(3))
	ROMX_LOAD("generated-mpeg2.aac", 0, 65339, CRC(874fe22e) SHA1(6047afd4a8246d838726b7ddfdb9c6185f7d6f16), ROM_BIOS(4))
	ROMX_LOAD("generated-mpeg2.aac", 0, 65339, CRC(874fe22e) SHA1(6047afd4a8246d838726b7ddfdb9c6185f7d6f16), ROM_BIOS(5))
	ROMX_LOAD("generated-mpeg2-lookahead.aac", 0, 65785, CRC(6941fc04) SHA1(b17123beca2a658f69c57056c53197d1b5461bbd), ROM_BIOS(6))
	ROMX_LOAD("al09_44.adts", 0, 64048, CRC(abcba74b) SHA1(99cfa37ec7432f5b5d4ed060c38c741eda4fccb6), ROM_BIOS(7))
	ROMX_LOAD("al09_44.adts", 0, 64048, CRC(abcba74b) SHA1(99cfa37ec7432f5b5d4ed060c38c741eda4fccb6), ROM_BIOS(8))
ROM_END

ROM_START(mu4nand)
	ROM_SYSTEM_BIOS(0, "setup", "Original storage, loader and serial setup")
	ROM_SYSTEM_BIOS(1, "stream", "Original streaming frontier (incomplete)")
	ROM_SYSTEM_BIOS(2, "sustain", "Original eight-block digital streaming fixture")
	ROM_SYSTEM_BIOS(3, "worker", "Original streaming worker observation (100 ms tail)")
	ROM_SYSTEM_BIOS(4, "settle", "Original startup observation (1 second tail)")
	ROM_SYSTEM_BIOS(5, "scan", "Original startup observation (5 second tail)")
	ROM_SYSTEM_BIOS(6, "startup", "Original bounded storage startup observation (20 second tail)")
	ROM_SYSTEM_BIOS(7, "command", "Original framed serial status request (20 second tail)")
	ROM_SYSTEM_BIOS(8, "wire", "Original status request with externally clocked serial TX")
	ROM_SYSTEM_BIOS(9, "wireack", "Original serial status transaction with peer acknowledgement")
	ROM_SYSTEM_BIOS(10, "pins", "Original status transaction with McBSP2 RX and TX pins")
	ROM_SYSTEM_BIOS(11, "replay", "Original pin-level status transaction with mid-byte replay")
	ROM_SYSTEM_BIOS(12, "measure", "Original pin-level sample measurement observation")
	ROM_SYSTEM_BIOS(13, "reset", "Original mid-byte bench reset observation (incomplete)")
	ROM_SYSTEM_BIOS(14, "retained", "Original fresh-process retained-media observation")
	ROM_SYSTEM_BIOS(15, "bootstrap", "Original uploaded startup observation (isolated bench)")
	ROM_SYSTEM_BIOS(16, "bootstatus", "Original uploaded startup with pin-level status transaction")
	ROM_SYSTEM_BIOS(17, "bootreset", "Original uploaded startup with mid-byte reset and status restart")
	ROM_SYSTEM_BIOS(18, "bootreplay", "Original uploaded startup with mid-byte save-state replay")
	ROM_SYSTEM_BIOS(19, "bootmeasure", "Original uploaded startup with pin-level sample measurement")
	ROM_SYSTEM_BIOS(20, "bootrecord", "Original uploaded startup with pin-level recorder command probe")
	ROM_SYSTEM_BIOS(21, "bootrecordram", "Original recorder with provisional shared data/program RAM")
	ROM_SYSTEM_BIOS(22, "bootreccontrol", "Original recorder subsequent control receive (provisional RAM)")
	ROM_SYSTEM_BIOS(23, "bootrecpoll", "Original recorder status-poll/control sequence (provisional RAM)")
	ROM_SYSTEM_BIOS(24, "bootrecsave", "Original recorder metadata transaction (provisional RAM)")
	ROM_SYSTEM_BIOS(25, "bootrecstage", "Staged original recorder coverage diagnostic (fail-closed)")
	ROM_SYSTEM_BIOS(26, "bootrecmeta", "Staged recorder with nonempty ID3 title and artist input")
	ROM_SYSTEM_BIOS(27, "bootopenstage", "Staged original cold saved-track catalogue and status")
	ROM_SYSTEM_BIOS(28, "bootplaystage", "Staged original saved-track playback coverage diagnostic")
	ROM_SYSTEM_BIOS(29, "bootrecac", "Staged original recorder with explicit varying converted-ADC fixture")
	ROM_SYSTEM_BIOS(30, "bootplaystop", "Staged original playback with explicit serial status/control diagnostic")
	ROM_REGION(741916, "segment", 0)
	ROM_LOAD("mu4_initdata_container.bin", 0, 741916, CRC(e0c05bf2) SHA1(5ff0b99c8d93b6ef2cda0bcd002810a4ab7a0e8f))
	ROM_REGION16_LE(240, "disk_vectors", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_vectors.bin", 0, 240, CRC(2f1b5de4) SHA1(145048a918768dbee2d5fce5213a6e26ece50edf))
	ROM_REGION16_LE(44, "disk_helpers", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_helpers.bin", 0, 44, CRC(6e84f71a) SHA1(fa38ceaa537490d6d57c84a6406990aad5b869a6))
	ROM_REGION16_LE(19064, "initdisk", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_library.bin", 0, 19064, CRC(e9ecd12c) SHA1(748672ed6cac0e864fe0e70a9026b8cafaf8a7bc))
	ROM_REGION16_LE(2522, "disk_cinit", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_cinit.bin", 0, 2522, CRC(535a37c1) SHA1(78d90095d93a4cbed326dff901671ece0004462e))
	ROM_REGION16_LE(546, "cinit", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_cinit.bin", 0, 546, CRC(9d1fb99f) SHA1(d08dd94e471519ed69869b3685a8a6c9cd393f58))
	ROM_REGION16_LE(92, "trampolines", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_trampolines.bin", 0, 92, CRC(0de68a60) SHA1(3b2b1ac46f7c0cef5c5c1288a7cb0bcf1bb817de))
	ROM_REGION16_LE(6758, "entry", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_entry.bin", 0, 6758, CRC(aa68038b) SHA1(c4d0c3b8211569ee4ec23d2945a9855b5423ec89))
	ROM_REGION16_LE(11160, "library", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_library.bin", 0, 11160, CRC(f3b1ddd5) SHA1(e6d082993f823ebbff46f33bc36402a8e8c0c5d8))
ROM_END
} // anonymous namespace
CONS(2026, mu4nand, 0, 0, test, mu4_native_control, mu4_storage_test_state, empty_init,
		"Test", "MU4 original NAND routine boundary", MACHINE_IMPERFECT_SOUND)
CONS(2026, mu4aac, 0, 0, test, mu4_storage_test, mu4_aac_test_state, empty_init,
		"Test", "MU4 original AAC isolated ABI probe", MACHINE_NO_SOUND_HW)
