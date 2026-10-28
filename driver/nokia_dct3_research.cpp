// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "machine/clock.h"
#include "machine/intelfsh.h"
#include "speaker.h"
#include "nokia_dct3_research.h"
#include "nokia_dsp_staged.h"

namespace nokia_dct3 {

void nokia_dct3_state::dct3_nsm1_research_map(address_map &map)
{
	map.global_mask(0x00ffffff);
	// Capacity-compatible decode; fitted aliases remain unproved.
	map(0x000000, 0x00ffff).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));
	map(0x010000, 0x010fff).rw(FUNC(nokia_dct3_state::dsp_ram_r), FUNC(nokia_dct3_state::dsp_ram_w));
	map(0x020000, 0x0200ff).rw(FUNC(nokia_dct3_state::mad2_io_r), FUNC(nokia_dct3_state::mad2_io_w));
	map(0x030000, 0x030003).rw(FUNC(nokia_dct3_state::mad2_dspif_r), FUNC(nokia_dct3_state::mad2_dspif_w));
	map(0x040000, 0x040003).rw(FUNC(nokia_dct3_state::mad2_mcuif_r), FUNC(nokia_dct3_state::mad2_mcuif_w));
	map(0x100000, 0x11ffff).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));
	map(0x200000, 0x3fffff).rw(FUNC(nokia_dct3_state::flash_r), FUNC(nokia_dct3_state::flash_w));
}

void nokia_dct3_state::dct3_nse6_research_map(address_map &map)
{
	map.global_mask(0x00ffffff);
	// NSE-6 service chapter: 256 KiB SRAM, 2 MiB flash. Aliases unproved.
	map(0x000000, 0x00ffff).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));
	map(0x010000, 0x010fff).rw(FUNC(nokia_dct3_state::dsp_ram_r), FUNC(nokia_dct3_state::dsp_ram_w));
	map(0x020000, 0x0200ff).rw(FUNC(nokia_dct3_state::mad2_io_r), FUNC(nokia_dct3_state::mad2_io_w));
	map(0x030000, 0x030003).rw(FUNC(nokia_dct3_state::mad2_dspif_r), FUNC(nokia_dct3_state::mad2_dspif_w));
	map(0x040000, 0x040003).rw(FUNC(nokia_dct3_state::mad2_mcuif_r), FUNC(nokia_dct3_state::mad2_mcuif_w));
	map(0x100000, 0x13ffff).rw(FUNC(nokia_dct3_state::ram_r), FUNC(nokia_dct3_state::ram_w));
	map(0x200000, 0x3fffff).rw(FUNC(nokia_dct3_state::flash_r), FUNC(nokia_dct3_state::flash_w));
}

INPUT_PORTS_START( nmp5stage )
	PORT_INCLUDE(dct3_network_config)
	// MU4 key transport is unrecovered; expose no borrowed logical keys.
	PORT_START("COL.0")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START("COL.1")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START("COL.2")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START("COL.3")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START("COL.4")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START("PWR")
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START("CHARGER")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

static INPUT_PORTS_START( dct3_research_adc2 )
	PORT_START("RESEARCH_ADC2")
	// Explicit analog-input comparison, not measured handset calibration.
	// 554 is the nominal 3.6-V / typical CCONT 6.5-mV-per-bit hypothesis.
	PORT_CONFNAME(0x03ff, 0x03ff, "Research CCONT ADC2 source")
	PORT_CONFSETTING(0x03ff, "Saturated control (1023)")
	PORT_CONFSETTING(554, "Nominal input hypothesis (554)")
INPUT_PORTS_END

INPUT_PORTS_START( nsm1r4t )
	PORT_INCLUDE(noki5110)
	PORT_INCLUDE(dct3_research_adc2)
	PORT_START("RESEARCH_ADC0")
	// Own 2bb822 reads selector 0 for the 280b48 state-machine branch.
	// Raw-input comparison, not a measured unattached-board voltage.
	PORT_CONFNAME(0x03ff, 0, "Research CCONT ADC0 source")
	PORT_CONFSETTING(0, "Low control (0)")
	PORT_CONFSETTING(0x03ff, "High input comparison (1023)")
	PORT_START("RESEARCH_EXT_STATUS")
	// Own 280bf4..280c20 tests CTSI+0e bit 1 before emitting 0720.
	// Raw-input contrast only; physical pin ownership remains unproved.
	PORT_CONFNAME(0x07, 0, "Research MAD2 external status")
	PORT_CONFSETTING(0, "Low control (0)")
	PORT_CONFSETTING(2, "Bit 1 high comparison (2)")
	// Own normal table 0x2e0b18: digits retain the NSE-1 cells, while
	// navigation and side keys occupy different cells. Row zero is unused.
	PORT_MODIFY("COL.0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Volume Down") PORT_CODE(KEYCODE_PGDN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Send") PORT_CODE(KEYCODE_S) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("End / Mode") PORT_CODE(KEYCODE_E) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Volume Up") PORT_CODE(KEYCODE_PGUP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_MODIFY("COL.1")
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Left Softkey") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYPAD ) PORT_NAME("Right Softkey") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
INPUT_PORTS_END

INPUT_PORTS_START( nsb3r4t )
	PORT_INCLUDE(noki5210)
	PORT_INCLUDE(dct3_research_adc2)
	PORT_START("RESEARCH_ADC0")
	// Own 2ad13a reads selector 0 for accessory classification. These are
	// external raw-input contrasts, not measured NSB-3 open-pin voltages.
	PORT_CONFNAME(0x03ff, 0, "Research CCONT ADC0 source")
	PORT_CONFSETTING(0, "Low control (0)")
	PORT_CONFSETTING(0x03ff, "High input comparison (1023)")
	PORT_START("RESEARCH_EXT_STATUS")
	// Own 273dc4..273dcc reads CTSI+0e bit 1 before accessory selection.
	// Physical signal ownership/polarity remains a board-research question.
	PORT_CONFNAME(0x07, 0, "Research MAD2 external status")
	PORT_CONFSETTING(0, "Low control (0)")
	PORT_CONFSETTING(2, "Bit 1 high comparison (2)")
	PORT_MODIFY("SIM_REMOVED")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Insert synthetic SIM card") PORT_TOGGLE
	PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::sim_removed_changed), 0)
INPUT_PORTS_END

INPUT_PORTS_START( nse2r4v )
	PORT_INCLUDE(noki5110)
	PORT_MODIFY("SIM_REMOVED")
	// A card is available, but the research machine defaults to an empty socket.
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Insert synthetic SIM card") PORT_TOGGLE
	PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::sim_removed_changed), 0)
INPUT_PORTS_END

INPUT_PORTS_START( nsb1r4t )
	PORT_INCLUDE(noki5110)
	PORT_MODIFY("SIM_REMOVED")
	// Default empty socket; active insertion drives the existing removed line low.
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Insert synthetic SIM card") PORT_TOGGLE
	PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::sim_removed_changed), 0)
INPUT_PORTS_END

INPUT_PORTS_START( nsk1r4t )
	PORT_INCLUDE(noki5110)
	PORT_INCLUDE(dct3_research_adc2)
	PORT_MODIFY("SIM_REMOVED")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Insert synthetic SIM card") PORT_TOGGLE
	PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::sim_removed_changed), 0)
INPUT_PORTS_END

INPUT_PORTS_START( nsk3r4t )
	PORT_INCLUDE(noki5110)
	PORT_INCLUDE(dct3_research_adc2)
	// Own key table2c7a30, not the NSE-1 single-Navi physical labels.
	PORT_MODIFY("COL.0")
	PORT_BIT(0x03, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Send") PORT_CODE(KEYCODE_S) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("End") PORT_CODE(KEYCODE_E) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_MODIFY("COL.1")
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Left Softkey") PORT_CODE(KEYCODE_ENTER) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Scroll Up") PORT_CODE(KEYCODE_UP) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Scroll Down") PORT_CODE(KEYCODE_DOWN) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Right Softkey") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)
	PORT_MODIFY("SIM_REMOVED")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Insert synthetic SIM card") PORT_TOGGLE
	PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::sim_removed_changed), 0)
INPUT_PORTS_END

void nokia_dct3_state::nam2c3(machine_config &config)
{
	// Own-ROM explicit 0089/88c2 support; fitted board part remains unproved.
	noki2100(config);
	INTEL_28F160C3T(config.replace(), "flash");
}

void nokia_dct3_state::nhm9stage(machine_config &config)
{
	noki3350(config);
	// Own 247-word upload and MCU-supplied source-end argument. The staged
	// execution rate is a research cadence, not a measured DSP clock contract.
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x1f0c54);
	staged.set_verifier_program_words(247);
	staged.set_verifier_source_end(0x8000);
	staged.set_loader2_source(0x1f0d80, 629);
}

void nokia_dct3_state::nsm4stage(machine_config &config)
{
	noki8855(config);
	// NSM-4 selector 15 fragment and selector 27 verifier, not a sibling mask.
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x2007fc);
	staged.set_verifier_program_words(246);
	staged.set_verifier_source_end(0x0000);
}

void nokia_dct3_state::nsb7stage(machine_config &config)
{
	noki8290(config);
	// NSB-7 System Module p27 and UI p8: own COBBA PCM and MIC2/EAR.
	m_cobba->add_route(nokia_cobba_device::ear, "mono", 1.0);
	MICROPHONE(config, "microphone", 1).front_center()
			.add_route(0, m_cobba, 1.0, nokia_cobba_device::mic2);
	nokia_product_config research = PRODUCT_8290_UPLOAD;
	research.cobba_pcm.data_clock = 512'000;
	research.cobba_pcm.frame_clock = 8'000;
	research.cobba_pcm.sample_bits = 13;
	research.cobba_pcm.sync_clocks = 1;
	research.cobba_pcm.word_clocks = 16;
	research.cobba_pcm.msb_first = true;
	research.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	apply_product_config(research);
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x11dcac);
	staged.set_loader2_source(0x11ddd8, 623);
	staged.set_loader_control_address(0x0880);
	staged.set_cycle_guard_for_loader(true);
}

void nokia_dct3_state::nsb7hle(machine_config &config)
{
	nsb7stage(config);
	nokia_product_config runtime = m_product;
	// Nominal VBATT transfer hypothesis: 3.6 V / 6.5 mV per CCONT bit.
	// Other analog channels remain provisional; no charger is asserted.
	runtime.ccont_board.channel_defaults[2] = 554;
	// Own 2eef68 initializes IIR/control 38/39, FIFOs 3d/3e and
	// clock bit 5; 2eeee6 services the FIQ6 cause register.
	runtime.simi_controller = true;
	runtime.synthetic_sim_card = true;
	// Own 24b840 class-74 route calls 248b00. Command 0d at 248b74
	// cancels timer 19 and consumes reply +9 bits 0/1 as fault results.
	runtime.dsp_service = true;
	runtime.external_service_transport = true;
	runtime.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	// Own 55/03050000 and task-13 8b consumer agree on forty RSSI records.
	// Own 56 list and 287574/28734c accept the shared SCH encoding;
	// 2e50ce commits the cached receiver configuration on type 89;
	// task-17 parser 29d42c consumes the shared BCCH block layout.
	runtime.radio.acquisition = nokia_radio_peer_device::acquisition_strategy::candidate_window;
	runtime.radio.coverage = nokia_radio_peer_device::acquisition_coverage::candidate_paging;
	// Own 2b6a1e emits the same selector grammar for late type-57 scans.
	runtime.radio.background_band_measurements = true;
	// Own post-call CHANNEL_CONFIGURE requests carry deconfiguration 14.
	runtime.radio.traffic_release_parameter = 0x14;
	runtime.radio.assigned_channel_confirmation = 1;
	runtime.radio.band_scan_accepts_candidate_window = true;
	// Own selector17/shadow12df02 and writer2cb93e; physical Send/End
	// publish 860b/840a. This enables HLE voice, not resident DSP speech.
	runtime.dsp_speech_control = {
		0x08, nokia_dsp_hle_device::speech_request_predicate { 0x0200, 0x0200 }
	};
	runtime.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	runtime.cobba_hle_voice.output = nokia_cobba_device::ear;
	apply_product_config(runtime);
	auto *const staged = subdevice<nokia_dsp_staged_device>("dsp_staged");
	staged->set_observe_after_missing_code(true);
	staged->set_runtime_hle_after_loader(true);
}

void nokia_dct3_state::nsm2stage(machine_config &config)
{
	noki8850(config);
	// NSM-2's own upload releases reset through CTSI+2 bit 0. Execute
	// flash-contained code only, leaving absent mask instructions unknown.
	m_mad2->set_dsp_reset_wiring_contract({ 0x10, 0x01 });
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x11ad54);
	staged.set_loader2_source(0x11ae80);
	staged.set_loader_control_address(0x0880);
	staged.set_cycle_guard_for_loader(true);
	staged.set_observe_after_missing_code(true);
}

void nokia_dct3_state::nsm2hle(machine_config &config)
{
	nsm2stage(config);
	nokia_product_config runtime = PRODUCT_8850;
	runtime.dsp_reset_wiring = { 0x10, 0x01 };
	// Native uploads publish their own verdict. Runtime acknowledges the
	// shared service handshake; LCD output does not require a service map.
	runtime.dsp_service = true;
	runtime.external_service_transport = true;
	// Research card input: firmware still owns reset, detection and APDUs.
	runtime.simi_controller = true;
	runtime.synthetic_sim_card = true;
	runtime.radio = RADIO_NSM2;
	// Own dispatcher 243a42 routes class 74 to 240dbc; command 0d at
	// 240e2c cancels the armed wait and reads two fault bits at body +9.
	runtime.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	// NSM-2 System Module issue 1 12/1999, p24: independent PCM timing.
	runtime.cobba_pcm.data_clock = 1'000'000;
	runtime.cobba_pcm.frame_clock = 8'000;
	runtime.cobba_pcm.sample_bits = 13;
	runtime.cobba_pcm.sync_clocks = 1;
	runtime.cobba_pcm.word_clocks = 16;
	runtime.cobba_pcm.msb_first = true;
	runtime.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	// Own selector 17 updates 0200; selector 8 publishes at 2cb3ca.
	// Physical Send/End independently verifies 860b -> 840a.
	runtime.dsp_speech_control = {
		0x08, nokia_dsp_hle_device::speech_request_predicate { 0x0200, 0x0200 }
	};
	// NSM-2 UI Module identifies internal MIC2 and EAR, not runtime gains.
	runtime.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	runtime.cobba_hle_voice.output = nokia_cobba_device::ear;
	m_cobba->add_route(nokia_cobba_device::ear, "mono", 1.0);
	MICROPHONE(config, "microphone", 1).front_center()
			.add_route(0, m_cobba, 1.0, nokia_cobba_device::mic2);
	apply_product_config(runtime);
	subdevice<nokia_dsp_staged_device>("dsp_staged")->set_runtime_hle_after_loader(true);
}

void nokia_dct3_state::nsm3dr6(machine_config &config)
{
	noki8xxx(config);
	// Product flash contains a ROM6 bootstrap fragment and two uploaded
	// loaders. Execute those bytes only; the complete fitted mask is absent.
	m_mad2->set_dsp_reset_wiring_contract({ 0x10, 0x01 });
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x11770c);
	staged.set_loader2_source(0x117838);
	// Diagnostic-only composition: stop before missing native code, retain
	// DSP ownership, and observe subsequent MCU requests without replies.
	staged.set_observe_after_missing_code(true);
}

void nokia_dct3_state::nsb6stage(machine_config &config)
{
	noki8xxx(config);
	// NSB-6 UI Description, issue 1 (06/2000), p7: internal MIC2 and EAR.
	// Board connectivity; codec gains remain neutral pending measured evidence.
	m_cobba->add_route(nokia_cobba_device::ear, "mono", 1.0);
	MICROPHONE(config, "microphone", 1).front_center()
			.add_route(0, m_cobba, 1.0, nokia_cobba_device::mic2);
	nokia_product_config research = PRODUCT_8XXX;
	// Own acquired PMM occupies 3d0000..3fffff. Reuse the passive
	// persistent-flash bus census; it never changes read/write results.
	research.flash_persistent_start = 0x003d0000;
	research.dsp_reset_wiring = { 0x10, 0x01 };
	// NSB-6 System Module issue 1 06/2000 p25 (acquired PDF p39):
	// COBBA generates 512 kHz/8 kHz, one active-high sync clock and a
	// 16-bit MSB-first word sign-extended from bit 12. Data changes on
	// rising edges; the receiver samples falling edges, 64 clocks/frame.
	research.cobba_pcm.data_clock = 512'000;
	research.cobba_pcm.frame_clock = 8'000;
	research.cobba_pcm.sample_bits = 13;
	research.cobba_pcm.sync_clocks = 1;
	research.cobba_pcm.word_clocks = 16;
	research.cobba_pcm.msb_first = true;
	research.cobba_pcm.data_edge = nokia_mad2_pcm_device::clock_edge::falling;
	// Own consumer 2c2dbc requires matching non-sentinel identity words.
	// Six is the acquired fragment's ROM input, not measured NSB-6 silicon.
	research.dsp_bootstrap = {
		nokia_dsp_hle_device::bootstrap_exchange_strategy::ping_pong,
		0, {}, 0, std::nullopt,
		nokia_dsp_hle_device::bootstrap_parked_contract { 0x004, 0xffff, 6 }, 0
	};
	apply_product_config(research);
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x11508c);
	staged.set_loader2_source(0x1151b8);
	staged.set_loader_control_address(0x0880);
	staged.set_cycle_guard_for_loader(true);
	staged.set_observe_after_missing_code(true);
}

void nokia_dct3_state::nsb6hle(machine_config &config)
{
	nsb6stage(config);
	// Explicit runtime transport comparison, not missing resident execution.
	// Identity and security-record replies remain unimplemented here;
	// the request-correlated compact self-test contract is selected below.
	nokia_product_config runtime = m_product;
	// NSB-6's Nokia user manual specifies BLB-2. These shared nominal
	// board samples are calibrated inputs, not measured electrical units.
	runtime.ccont_board = ADC_5210;
	// Own scanner 2fb850 drives row pins 1..4. IRQ handler 2fb9a4
	// reads pending columns at 2b before acknowledging IRQ0.
	runtime.keypad_wiring.row_pin_shift = 1;
	runtime.keypad_wiring.column_irq_status = 0x2b;
	// Compose the physical SIM interface and removable laboratory card;
	// the NSB-6 firmware still owns activation and every APDU.
	runtime.simi_controller = true;
	runtime.synthetic_sim_card = true;
	runtime.radio = RADIO_NSB6;
	// Own task-12 parser 2809fc consumes forty ARFCN/signed-RSSI records.
	runtime.radio.background_band_measurements = true;
	runtime.radio.background_measurement_request = {1, 0x14, 0, 0};
	runtime.dsp_service = true;
	runtime.external_service_transport = true;
	// Own class-74 dispatcher 24373a calls 240938; command 0d at
	// 2409ac cancels the armed wait and interprets fault bits at +9.
	runtime.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	// Own selector 17 updates bit 0200; selector 8 publishes at 2c338e.
	// Physical Answer/Send and End independently produce 860b -> 840a.
	runtime.dsp_speech_control = {
		0x08, nokia_dsp_hle_device::speech_request_predicate { 0x0200, 0x0200 }
	};
	runtime.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	runtime.cobba_hle_voice.output = nokia_cobba_device::ear;
	apply_product_config(runtime);
	m_dsp_hle->set_opaque_parameter_acceptance(true);
	subdevice<nokia_dsp_staged_device>("dsp_staged")->set_runtime_hle_after_loader(true);
}

void nokia_dct3_state::nhm3stage(machine_config &config)
{
	noki6250(config);
	// Execute this product's flash-contained bytes, not a fitted DSP mask.
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x1d390);
	staged.set_loader2_source(0x1d4bc);
	staged.set_verifier_source_end(0xd000);
	staged.set_loader_control_address(0x0880);
	staged.set_cycle_guard_for_loader(true);
	staged.set_observe_after_missing_code(true);
}

void nokia_dct3_state::nhm3hle(machine_config &config)
{
	nhm3stage(config);
	// Native uploads stop before the absent mask routine; the existing
	// request-derived transport peer owns runtime afterward, not mask code.
	nokia_product_config runtime = PRODUCT_6250;
	// Own SIM initialization 0x491fd8..0x492034 and reset 0x491bb4
	// use the existing 0x37/0x38/0x39 SIMI grammar. Keep this boundary
	// comparison in the research composition; physical phonebook and SMS
	// acceptance exercise the card exchange without promoting default boot.
	runtime.simi_controller = true;
	runtime.synthetic_sim_card = true;
	runtime.external_service_transport = true;
	// Own TX 0x56/160 publishes eighty big-endian candidate channels,
	// beginning with 19 and padding with 0xffff. Only acquisition is
	// selected here. Assigned-channel and call-release contracts are
	// configured below; handover remains unproved.
	runtime.radio.acquisition = nokia_radio_peer_device::acquisition_strategy::candidate_window;
	// Own type-57 (3,0) -> task-14 1802/8b -> 3cbe14 is exercised by
	// physical slow-PIN registration. This remains a runtime DSP HLE contract.
	runtime.radio.background_band_measurements = true;
	// Own consumer 0x464756 compares RX body bit 0 with pending context
	// 0x0402 byte 2, observed as 1 for assigned SDCCH.
	runtime.radio.assigned_channel_confirmation = 1;
	// Own physical-End release publishes type 02 with channel 0x60 and
	// parameter 0x14; acknowledge that transaction through the peer.
	runtime.radio.traffic_release_parameter = 0x14;
	// NHM-3 compiler 0x3fbc54/0x3fbc68 clears field 0x0200 with ROM
	// keep-mask 0xfdff and adds 0x0200 from its selector table. Physical
	// Answer/End publish 0x8626/0x8426 through own command-8 helper.
	runtime.dsp_speech_control = {
		0x08, nokia_dsp_hle_device::speech_request_predicate { 0x0200, 0x0200 }
	};
	// 6250 consumer 304494 dispatches 0d, clears its armed timer and reads
	// two fault bits from the following octet. This is a declared peer model.
	runtime.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	apply_product_config(runtime);
	subdevice<nokia_dsp_staged_device>("dsp_staged")->set_runtime_hle_after_loader(true);
}

void nokia_dct3_state::nsm3dhle(machine_config &config)
{
	nsm3dr6(config);
	nokia_product_config runtime = PRODUCT_8XXX;
	runtime.dsp_reset_wiring = { 0x10, 0x01 };
	// Request-derived D0 discovery only; no donor application/channel map.
	runtime.external_service_transport = true;
	apply_product_config(runtime);
	subdevice<nokia_dsp_staged_device>("dsp_staged")->set_runtime_hle_after_loader(true);
	m_dsp_hle->set_opaque_parameter_acceptance(true);
	// Declared candidate codec only: no record/self-test verdict, no PMM edits.
	// Revision 0 is an unmeasured HLE input, not a donor chip identity.
	m_dsp_hle->set_record_codec83(true, 0);
}

void nokia_dct3_state::nsm3stage(machine_config &config)
{
	noki8210(config);
	// ROM6 fragment and selector 14 are acquired NSM-3 uploads. The
	// alternative ROM5 catalogue is not selected by this composition.
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x11ab14);
	staged.set_loader2_source(0x11ac40, 623);
	staged.set_loader_control_address(0x0880);
	staged.set_cycle_guard_for_loader(true);
	staged.set_observe_after_missing_code(true);
}

void nokia_dct3_state::nsm3hle(machine_config &config)
{
	nsm3stage(config);
	// Explicit missing-resident boundary comparison. No service verdict or
	// donor provisioning is selected. Own TX type 05 carries the ordinary
	// D0 discovery transaction after native upload completion.
	nokia_product_config runtime = m_product;
	runtime.external_service_transport = true;
	// Own class-74 call 243a3e selects handler 240db0. Command 0d at
	// 240e20 requires the armed flag and consumes two fault bits at +9.
	runtime.dsp_service = true;
	runtime.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	// NSM-3's Nokia user guide specifies BLB-2. Nominal board samples
	// are calibrated external inputs, not measured 8210 electrical units.
	runtime.ccont_board = ADC_5210;
	// Own scanner 305998 drives pins 0..4 and returns row*5+column;
	// matrix 33ee78 leaves row 0 empty and maps physical keys on 1..4.
	runtime.keypad_wiring.row_pin_shift = 1;
	// Compose the physical controller and removable laboratory card.
	// NSM-3 firmware owns reset, activation and APDU sequencing.
	runtime.simi_controller = true;
	runtime.synthetic_sim_card = true;
	runtime.radio = RADIO_NSM3;
	// Own class-88 error consumer plus independent HLE comparison; this is
	// a lab failure completion, not recovered ROM6 measurement behavior.
	runtime.radio.unlisted_mode4_timing_failure = true;
	// Own parameter publisher 2c6cf4 adds field 0200 from table 337db0;
	// command 8 changes 840a -> 860b -> 840a on physical Send/End.
	// This selects the HLE request only, not an unevidenced analogue route.
	runtime.dsp_speech_control = {
		0x08, nokia_dsp_hle_device::speech_request_predicate { 0x0200, 0x0200 }
	};
	// NSM-3 UI Module issue 1 12/1999, pages 4/15 establishes EAR/MIC2.
	// Declared HLE endpoints only: its 32 dB electrical test condition does
	// not establish a runtime gain or a COBBA control-register sequence.
	runtime.cobba_hle_voice.microphone = nokia_cobba_device::mic2;
	runtime.cobba_hle_voice.output = nokia_cobba_device::ear;
	m_cobba->add_route(nokia_cobba_device::ear, "mono", 1.0);
	MICROPHONE(config, "microphone", 1).front_center()
			.add_route(0, m_cobba, 1.0, nokia_cobba_device::mic2);
	// Own task-12 8b completion 21ef4c -> 2a2250 -> 28755e
	// consumes forty ARFCN/RSSI records; runtime delivery is gated separately.
	runtime.radio.background_band_measurements = true;
	apply_product_config(runtime);
	subdevice<nokia_dsp_staged_device>("dsp_staged")->set_runtime_hle_after_loader(true);
}

void nokia_dct3_state::nsb3r4t(machine_config &config)
{
	noki6190(config);
	nokia_product_config research = PRODUCT_6190;
	research.synthetic_sim_card = true; // Available lab card; tray defaults empty.
	apply_product_config(research);
	// Explicit acquired NSE-1 mask input, not a fitted NSB-3 identity claim.
	// Keep own firmware, erased storage, ADC configuration and empty SIM tray.
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	// Own catalogue-4 payload at 2bd548 installs through program word 2b2a.
	// The observed 811 writes above 27ff must not disappear into mask ROM.
	m_dsp_c54x->set_overlay_end(0x3000);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nse2r4t(machine_config &config)
{
	noki5110i(config);
	// Explicit NSE-1 resident-mask compatibility experiment, not fitted NSE-2
	// identity. Preserve own firmware, ADC inputs and erased external EEPROM.
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nse2r4v(machine_config &config)
{
	noki5110i(config);
	// Own source-7 map selects ADC2 and converts it with 1500/232 before
	// validating 1800..5500. Test the independently derived nominal input:
	// NSE-1's 3.6 V pack / CCONT's typical 6.5 mV per bit = 554.
	// This is an explicit cross-product battery hypothesis, not NSE-2 calibration.
	nokia_product_config research = PRODUCT_5110I;
	research.ccont_board.channel_defaults[2] = 554;
	research.synthetic_sim_card = true; // Physical removed input defaults high.
	apply_product_config(research);
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nsb1r4t(machine_config &config)
{
	noki5190(config);
	// Explicit nominal-input experiment, not measured NSB-1 board calibration:
	// own source-7 acquisition uses ADC2; the 5190 manual specifies 3.6 V,
	// and CCONT's typical VBAT transfer is 6.5 mV/bit, hence raw 554.
	// Keep the ordinary silent machine and all other channels unchanged.
	nokia_product_config research = PRODUCT_5190;
	research.ccont_board.channel_defaults[2] = 554;
	research.synthetic_sim_card = true;
	apply_product_config(research);
	// Compatibility instrument only: NSE-1 resident ROM4 is acquired,
	// but fitted NSB-1 mask identity and DSP cadence remain unproved.
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	// Own executable upload reaches 2b38, including its 29e1 hook.
	m_dsp_c54x->set_overlay_end(0x3000);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nsk3r4t(machine_config &config)
{
	noki6130(config);
	nokia_product_config research = m_product;
	research.synthetic_sim_card = true;
	apply_product_config(research);
	// Own boot4 payload calls resident 8023/802b. NSE-1 ROM4 and its
	// 52-MHz clock are explicit compatibility inputs, not a fitted NSK-3 dump.
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	// Own boot4 loader uploads 811 words above 2800, through 2b2a.
	m_dsp_c54x->set_overlay_end(0x3000);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nsk1r4t(machine_config &config)
{
	dct3_base(config);
	// Minimum 64-KiB research decode covering own stack 10f370. The NSE-3
	// map supplies that bounded extent, not a claim about fitted NSK-1 parts.
	m_maincpu->set_addrmap(AS_PROGRAM, &nokia_dct3_state::dct3_nse3_map);
	INTEL_28F800B3T(config.replace(), "flash");
	// Own descriptor 92 declares one-byte addressing and 2 KiB. Fitted
	// page geometry and timing remain unproved; start with erased storage.
	I2C_24C16(config, m_eeprom);
	m_eeprom->set_write_cycle_time(attotime::from_msec(5));
	m_pup->eeprom_sda_read_cb().set(m_eeprom, FUNC(i2cmem_device::read_sda));
	m_pup->eeprom_sda_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_sda));
	m_pup->eeprom_scl_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_scl));
	nokia_product_config research;
	// Own 290886 samples SDA bit 0; START/STOP drive SCL bit 2.
	research.pup_eeprom_scl_bit = 2;
	research.keypad_wiring = KEYPAD_NSK1;
	// Own 280ff8 uses IIR/control 38/39, FIFO controls 3d/3e,
	// clock bit 5 and FIQ6/7. Default socket remains physically empty.
	research.simi_controller = true;
	research.synthetic_sim_card = true;
	// Own 291830 selects 22 at 28, commands at 2a, polls 29 bit 2,
	// receives at 2d; LCD command/data use 2c/2b at 292182/2921d8.
	research.gensio_wiring = GENSIO_NSE1;
	apply_product_config(research);
	config.device_remove("dsp_hle");
	// Explicit NSE-1 resident mask/clock compatibility input. No NSK-1
	// fitted-mask, physical-clock, keypad or SIM acceptance is implied.
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	// Own loader writes executable words through 2b2a; its 26d0 hook
	// branches to uploaded 29d3. The default 2800 extent drops this code.
	m_dsp_c54x->set_overlay_end(0x3000);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nsm1r4t(machine_config &config)
{
	dct3_base(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &nokia_dct3_state::dct3_nsm1_research_map);
	// The base 16-Mbit flash is an instrument, not a fitted-part claim.
	I2C_24C128(config, m_eeprom);
	m_eeprom->set_write_cycle_time(attotime::from_msec(5));
	m_pup->eeprom_sda_read_cb().set(m_eeprom, FUNC(i2cmem_device::read_sda));
	m_pup->eeprom_sda_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_sda));
	m_pup->eeprom_scl_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_scl));
	nokia_product_config research;
	// Own receiver 0x2bdece proves SCL bit 2. No HLE service/radio peer.
	research.pup_eeprom_scl_bit = 2;
	// Own 0x2bf082 read selects 0x28=0x22, writes command at 0x2a,
	// polls status 0x29 bit 2 and reads the reply at 0x2d.
	research.gensio_wiring = GENSIO_NSE1;
	// Own scanner confirms this non-overlapping GPIO layout. Runtime UI
	// input acceptance remains distinct from the static wiring contract.
	research.keypad_wiring = KEYPAD_NSM1;
	// Own 0x2af080 resets SIMI at 0x38/0x39; 0x2af0f6 drains RX at
	// 0x37 while polling its count at 0x3c. Attach the removable lab card,
	// not a handset identity fixture or an internal readiness publication.
	research.simi_controller = true;
	research.synthetic_sim_card = true;
	apply_product_config(research);
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	// Own loader writes executable words through 2a5c. Preserve the
	// uploaded bank instead of exposing the resident image at 2800.
	m_dsp_c54x->set_overlay_end(0x3000);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nse6stage(machine_config &config)
{
	dct3_base(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &nokia_dct3_state::dct3_nse6_research_map);
	// Capacity/address-compatible instrument, not an identified fitted part.
	// Firmware descriptor 0xf6 splits writes at 32 bytes; this model has
	// 64-byte pages. Do not claim page-wrap fidelity or measured latency.
	I2C_24C256(config, m_eeprom);
	m_eeprom->set_write_cycle_time(attotime::from_msec(5));
	m_pup->eeprom_sda_read_cb().set(m_eeprom, FUNC(i2cmem_device::read_sda));
	m_pup->eeprom_sda_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_sda));
	m_pup->eeprom_scl_write_cb().set(m_eeprom, FUNC(i2cmem_device::write_scl));
	nokia_product_config research;
	research.pup_eeprom_scl_bit = 2;
	research.ccont_board = ADC_NSE6;
	// Independently recovered own six-register tuple and keypad tables.
	research.gensio_wiring = GENSIO_NSE1;
	research.keypad_wiring = KEYPAD_NSE6;
	research.simi_controller = true;
	research.synthetic_sim_card = true;
	apply_product_config(research);
	// Keep the required backend with its default disabled exchange/service
	// contracts. No fitted mask or fabricated bootstrap completion is selected.
}

void nokia_dct3_state::nse6r4t(machine_config &config)
{
	nse6stage(config);
	// Compatibility instrument only: the fitted NSE-6 DSP mask is unknown.
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	// Own NSE-6 upload writes executable words at 0x2946; the patched
	// continuation enters that bank. The fitted mask identity remains unknown.
	m_dsp_c54x->set_overlay_end(0x3000);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nse5r4t(machine_config &config)
{
	// Compatibility instrument, not the fitted NSE-5 mask identity. Execute
	// the acquired NSE-1 ROM4 against stock NSE-5 flash and local PMM.
	noki7110(config);
	config.device_remove("dsp_hle");
	NOKIA_DSP_C54X(config, m_dsp_c54x, 52'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	// Research geometry: NSE-5 uploads executable code above 0x2800.
	// This tests the missing overlay bank, not the fitted mask identity.
	m_dsp_c54x->set_overlay_end(0x3000);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::nmp5stage(machine_config &config)
{
	dct3_32mbit_flash_base(config);
	nokia_product_config research;
	// Own 3acc98 polls 6d, selects 25 at 2d, transfers through 2c
	// and receives at 6c. This selects the recovered GENSIO register contract.
	research.gensio_wiring = GENSIO_NSM3;
	research.ccont_board = ADC_NPM5;
	// Own 3a7424 configures mixed port 2a through 6a; 3a73d8 reads
	// pending inputs at 2b. This does not attach an unverified MU4 peer.
	research.keypad_wiring.column_direction = 0x6a;
	research.keypad_wiring.column_irq_status = 0x2b;
	// NPM-5 0x383fa0 arms MBUSTIM while its startup serial queue drains.
	research.mbus_timer_enabled = true;
	// Bootstrap instrument: bit 0 is the own verifier execution release.
	// Keep the written 0c setup bits in readback; no silicon-ready bit is
	// asserted. This is not a general NPM-5 clock-status contract.
	research.dsp_reset_wiring = { 0x0d, 0x01 };
	apply_product_config(research);
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x1e924c);
	staged.set_loader2_source(0x1e9378, 629);
	staged.set_verifier_source_end(0xa800);
	staged.set_cycle_guard_for_loader(true);
	staged.set_observe_after_missing_code(true);
	// Own 638-word block spans shared D:0d00..0f7d: 512 data words
	// followed by the 126-word 0f00 loader. Missing mask calls remain silent.
}

void nokia_dct3_state::nmp5hle(machine_config &config)
{
	nmp5stage(config);
	// Own first post-loader TX is type 05, 1eff00d000030101e000:
	// enable common request-derived discovery, not sibling application events.
	nokia_product_config runtime = m_product;
	runtime.external_service_transport = true;
	// Own 24c9d2..24c9e6 selects command 0d -> 24ca36, gated by
	// 13fe5d bit 2. Bytes 0/1 of the compact reply become command/faults;
	// fault bits 0/1 are consumed at 24ca6a..24ca96.
	runtime.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	apply_product_config(runtime);
	subdevice<nokia_dsp_staged_device>("dsp_staged")->set_runtime_hle_after_loader(true);
}

void nokia_dct3_state::npe3stage(machine_config &config)
{
	noki6210(config);
	// Own NPE-3 uploads: descriptor 224a44 contains 104 words;
	// descriptor 224b70 contains 629 second-loader words, not NSM-3's 623.
	auto &staged = NOKIA_DSP_STAGED(config, "dsp_staged", 13'000'000);
	configure_staged_dsp_attachments(staged);
	staged.set_program_fragment(0x24a50);
	staged.set_loader2_source(0x24b7c, 629);
	staged.set_verifier_source_end(0xd000);
	staged.set_loader_control_address(0x0880);
	staged.set_cycle_guard_for_loader(true);
	staged.set_observe_after_missing_code(true);
}

void nokia_dct3_state::npe3hle(machine_config &config)
{
	npe3stage(config);
	nokia_product_config runtime = PRODUCT_6210;
	// Own 3d71f8 converts source 7 (selector 2) by calibration * 1500/232.
	// Full scale yields 19f0, outside its 0708..157c accepted window.
	// Nominal 0230 models about 3.6 V; this is not a measured ADC curve.
	runtime.ccont_board.channel_defaults[2] = 0x230;
	// Own SIMI path (48b7ac cause write / 48b7be control write) uses
	// the physical controller; the firmware owns activation and APDUs.
	runtime.simi_controller = true;
	runtime.synthetic_sim_card = true;
	runtime.radio = RADIO_NPE3;
	// Own 8b handler 45835c and completion 3c9750/38b6ae recover the
	// forty-record background measurement contract; late-PIN run verifies it.
	runtime.radio.background_band_measurements = true;
	// Own foreign-selection continuation 2d107c -> 3f2684 publishes this
	// follow-up; state one permits the ordinary 8b measurement consumer.
	// Keep the selector product-owned rather than accepting arbitrary modes.
	runtime.radio.background_measurement_followup_request = {{3, 3, 0, 0}};
	// Own compiler 426eb4 selector 11 updates field 0200 with keep-mask
	// fdff. Physical Send/End publish command-8 870b/850a at 42727c.
	runtime.dsp_speech_control = {
		0x08, nokia_dsp_hle_device::speech_request_predicate { 0x0200, 0x0200 }
	};
	// Explicit research handoff after own uploads, before absent mask code.
	// Own 3029fe..302a02 selects command 0d -> 302a52, requires
	// flag 17fd99 bit 2 and consumes fault bits 0/1 from message byte 9.
	// This is a declared compact peer; no identity/record replies are selected.
	runtime.external_service_transport = true;
	runtime.dsp_service = true;
	runtime.dsp_service_control = DSP_SERVICE_CONTROL_COMPACT;
	apply_product_config(runtime);
	subdevice<nokia_dsp_staged_device>("dsp_staged")->set_runtime_hle_after_loader(true);
}

void nokia_dct3_state::npe3mbus(machine_config &config)
{
	npe3hle(config);
	nokia_product_config research = m_product;
	research.mbus_timer_enabled = true;
	apply_product_config(research);
}

void nse1_pipeline_fixture_state::nse1pipe(machine_config &config)
{
	noki5110(config);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_pipeline_frontend(true);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_standard_swwsr(true);
}

void nse1_pipeline_fixture_state::nse1plg(machine_config &config)
{
	nse1pipe(config);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_standard_swwsr(false);
}

void nse1_pipeline_fixture_state::nse1astd(machine_config &config)
{
	noki5110(config);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_standard_swwsr(true);
}

void dct3_pcm_fixture_state::nsm1pcm(machine_config &config)
{
	nsm1r4t(config);
	CLOCK(config, "external_pcm_frame", 8'000).signal_handler().set(FUNC(dct3_pcm_fixture_state::frame_w));
}

void dct3_pcm_fixture_state::nse1pcm(machine_config &config)
{
	noki5110(config);
	CLOCK(config, "external_pcm_frame", 8'000).signal_handler().set(FUNC(dct3_pcm_fixture_state::frame_w));
}

void dct3_pcm_fixture_state::nse1bit(machine_config &config)
{
	noki5110(config);
	CLOCK(config, "external_pcm_bit", 1'000'000).signal_handler().set(FUNC(dct3_pcm_fixture_state::bit_w));
}

void dct3_pcm_fixture_state::nse1pin(machine_config &config)
{
	nse1bit(config);
	subdevice<nokia_dsp_c54x_device>("dsp_c54x")->set_external_codec_clock(true);
	m_clocked_loopback = true;
}

void dct3_pcm_fixture_state::nse1aclk(machine_config &config)
{
	nse1pin(config);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_standard_swwsr(true);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->instruction_observer_cb().set(FUNC(dct3_pcm_fixture_state::instruction_w));
	m_execution_demand_observer = true;
}

void dct3_pcm_fixture_state::nse1pclk(machine_config &config)
{
	nse1aclk(config);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_pipeline_frontend(true);
}

void dct3_pcm_fixture_state::machine_start()
{
	nokia_dct3_state::machine_start();
	save_item(NAME(m_wire_word));
	save_item(NAME(m_wire_bits));
	save_item(NAME(m_clocked_echo_words));
}

void dct3_pcm_fixture_state::machine_reset()
{
	nokia_dct3_state::machine_reset();
	m_wire_word = 0;
	m_wire_bits = 0;
	m_clocked_echo_words = 0;
	m_demand_entries = 0;
}

void dct3_pcm_fixture_state::device_stop()
{
	if (m_clocked_loopback)
		logerror("native_pcm_fixture_echo_summary: words=%llu untimed=%llu handset_clock_claim=0\n",
			m_clocked_echo_words, subdevice<nokia_cobba_device>("cobba")->codec_serial_loopbacks());
	if (m_execution_demand_observer)
		logerror("rom4_codec_demand_summary: entries=%u t=%.9f\n", m_demand_entries, machine().time().as_double());
}

void dct3_pcm_fixture_state::instruction_w(offs_t address, u16 word)
{
	if (address != 0xa484 && address != 0xa4cb)
		return;
	++m_demand_entries;
	if (m_demand_entries > 128)
		return;
	auto &cpu = *subdevice<tms320c54x_device>("dsp_c54x:cpu");
	// This laboratory observer owns firmware symbols. CPU and transport
	// devices remain symbol-free; reading this bitmap is ordinary RAM.
	logerror("rom4_codec_demand_entry: hit=%u operation=%s mask=%04x previous=%04x t=%.9f\n",
		m_demand_entries, address == 0xa484 ? "acquire" : "release",
		u16(cpu.state_int(tms320c54x_device::STATE_B)), cpu.space(AS_DATA).read_word(0x00ef),
		machine().time().as_double());
}

void dct3_pcm_fixture_state::bit_w(int state)
{
	auto &backend = *subdevice<nokia_dsp_c54x_device>("dsp_c54x");
	auto &codec = *subdevice<nokia_cobba_device>("cobba");
	// Explicit lab fan-out and one-clock active-high sync assumption.
	// Ordinary machines do not attach this source. Only nse1pin owns
	// boot loopback clocks; the older fixture leaves untimed echo exclusive.
	const bool loopback = codec.codec_serial_loopback();
	if (loopback && !m_clocked_loopback)
	{
		m_wire_bits = 0;
		return;
	}
	const bool sync = !(machine().time().as_ticks(1'000'000) % 125);
	const auto output = backend.codec_transmit_clock_w(bool(state), sync);
	backend.codec_receive_clock_w(bool(state), sync, m_clocked_loopback && loopback && output.driven && output.data);
	if (state)
	{
		if (sync)
		{
			m_wire_word = 0;
			m_wire_bits = 0;
		}
		return;
	}
	if (!output.supported || !output.driven || m_wire_bits >= 16) return;
	m_wire_word = (m_wire_word << 1) | output.data;
	if (++m_wire_bits == 16)
	{
		if (m_clocked_loopback && loopback)
		{
			++m_clocked_echo_words;
			logerror("native_pcm_fixture_echo_word: value=%04x t=%.9f\n", m_wire_word, machine().time().as_double());
		}
		else
			logerror("native_pcm_fixture_word: value=%04x t=%.9f\n", m_wire_word, machine().time().as_double());
	}
}

void dct3_pcm_fixture_state::frame_w(int state)
{
	if (!state) return;
	auto &backend = *subdevice<nokia_dsp_c54x_device>("dsp_c54x");
	auto &codec = *subdevice<nokia_cobba_device>("cobba");
	// The established untimed boot echo already owns this test-mode frame.
	if (codec.codec_serial_loopback()) return;
	u16 output = 0;
	const bool sent = backend.codec_transmit_frame(output);
	const bool received = backend.codec_receive_frame(0);
	if (sent)
	{
		codec.codec_serial_transmit(output);
		logerror("native_pcm_fixture_word: value=%04x t=%.9f\n", output, machine().time().as_double());
	}
	const u64 tick = machine().time().as_ticks(8'000);
	if (!(tick % 800))
		logerror("native_pcm_fixture_frame: tick=%llu sent=%u received=%u handset_clock_claim=0 t=%.9f\n",
			tick, sent, received, machine().time().as_double());
}

} // namespace nokia_dct3
