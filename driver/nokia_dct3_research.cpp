// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "machine/clock.h"
#include "machine/intelfsh.h"
#include "speaker.h"
#include "nokia_dct3_research.h"
#include "nokia_dsp_staged.h"
#include "nokia_hagar.h"
#include "nokia_mfi_rx.h"
#include "nokia_rf_lab_signal.h"
#include "nokia_rf_lab_bcch.h"
#include "gsm_rach.h"
#include "gsm_normal_p27.h"
#include <cmath>
#include <cstring>
#include <type_traits>
#include "nokia_radio_timing.h"

namespace nokia_dct3 {

void dct3_rf_adc_fixture_state::nsm3adc(machine_config &config)
{
	nsm3r6(config);
	configure_adc_lab(config);
}

void dct3_rf_adc_fixture_state::configure_adc_lab(machine_config &config)
{
	auto &mfi = NOKIA_MFI_RX(config, "mfi_rx", 13'000'000);
	mfi.set_word_divider(12); // Candidate matched to native post-FIR quarter-rate mixing, not measured silicon.
	mfi.set_staging_limit(96); // One block-rounded >=69-word service-gap trial; not physical MFI capacity.
	mfi.adc_cb().set("cobba", FUNC(nokia_cobba_device::rf_receive_sample));
	mfi.data_available_cb().set("dsp_c54x", FUNC(nokia_dsp_c54x_device::rf_data_available_w));
	mfi.fault_observer_cb().set(FUNC(dct3_rf_adc_fixture_state::fault_observe_w));
	auto &backend = *subdevice<nokia_dsp_c54x_device>("dsp_c54x");
	backend.set_external_rf_receive(true);
	backend.rf_receive_cb().set(mfi, FUNC(nokia_mfi_rx_device::read));
	backend.rf_control_cb().set(FUNC(dct3_rf_adc_fixture_state::control_w));
	backend.rf_reset_cb().set(FUNC(dct3_rf_adc_fixture_state::rf_reset_w));
	subdevice<nokia_cobba_device>("cobba")->rf_receive_cb().set(FUNC(dct3_rf_adc_fixture_state::sample_r));
	subdevice<nokia_cobba_device>("cobba")->rf_transmit_cb().set(FUNC(dct3_rf_adc_fixture_state::transmit_word_w));
}

void dct3_rf_adc_fixture_state::nsm3ad0(machine_config &config)
{
	nsm3adc(config);
	m_tone = false;
}

void dct3_rf_adc_fixture_state::nsm3afb(machine_config &config)
{
	nsm3adc(config);
	m_burst = true;
}

void dct3_rf_adc_fixture_state::nsm3afs(machine_config &config)
{
	nsm3afb(config);
	m_sch = true;
}

void dct3_rf_adc_fixture_state::npe3afs(machine_config &config)
{
	npe3ref(config);
	configure_adc_lab(config);
	m_burst = true;
	m_sch = true;
}

// NSE-1 ROM4 writes the same port-3 bit-0 and port-0x21 bit-10/12 receive
// controls as ROM6; this attaches the ROM6 lab transport to test that grammar.
void dct3_rf_adc_fixture_state::nse1ad0(machine_config &config)
{
	nse1adc(config);
	m_tone = false;
}

void dct3_rf_adc_fixture_state::nse1adc(machine_config &config)
{
	noki5110(config);
	configure_adc_lab(config);
}

void dct3_rf_adc_fixture_state::nse1afs(machine_config &config)
{
	nse1adc(config);
	m_burst = true;
	m_sch = true;
}

void dct3_rf_adc_fixture_state::nse1cp9(machine_config &config)
{
	nse1afs(config);
	m_compare_start = true;
}

void dct3_rf_adc_fixture_state::nse1sp9(machine_config &config)
{
	nse1cp9(config);
	m_bcch = true;
	m_full_broadcast = true;
}

void dct3_rf_adc_fixture_state::nse1tp5(machine_config &config)
{
	nse1sp9(config);
	m_p05_transmit = true;
}

void dct3_rf_adc_fixture_state::nse1ap5(machine_config &config)
{
	nse1tp5(config);
	m_raw_agch = true;
	m_tuned_air = true;
}

// Products running NSE-1 ROM4 as an unproved mask input share its receive
// and transmit grammar; attach the complete nse1ap5 laboratory chain. The lab
// carrier sits in the product's own band.
void dct3_rf_adc_fixture_state::configure_nse1_lab_bts(machine_config &config, lab_band band)
{
	m_band = band;
	m_lab_arfcn = band == lab_band::gsm900 ? 35 : 600;
	configure_adc_lab(config);
	m_burst = m_sch = m_compare_start = true;
	m_bcch = m_full_broadcast = true;
	m_p05_transmit = m_raw_agch = true;
	m_tuned_air = true;
}

void dct3_rf_adc_fixture_state::nse2ap5(machine_config &config)
{
	nse2r4v(config);
	configure_nse1_lab_bts(config);
}

void dct3_rf_adc_fixture_state::nsk1ap5(machine_config &config)
{
	nsk1r4t(config);
	configure_nse1_lab_bts(config, lab_band::dcs1800);
}

void dct3_rf_adc_fixture_state::nsk3ap5(machine_config &config)
{
	nsk3r4t(config);
	configure_nse1_lab_bts(config, lab_band::dcs1800);
}

void dct3_rf_adc_fixture_state::nsb3ap5(machine_config &config)
{
	nsb3r4t(config);
	configure_nse1_lab_bts(config, lab_band::pcs1900);
}

void dct3_rf_adc_fixture_state::npe3ad0(machine_config &config)
{
	npe3afs(config);
	m_tone = false;
}

void dct3_rf_adc_fixture_state::npe3cp9(machine_config &config)
{
	npe3afs(config);
	m_compare_start=true;
	subdevice<nokia_mfi_rx_device>("mfi_rx")->set_latched_block_requests(true);
}

void dct3_rf_adc_fixture_state::npe3zp9(machine_config &config)
{
	npe3cp9(config);
	m_tone=false;
}

void dct3_rf_adc_fixture_state::npe3bp9(machine_config &config)
{
	npe3cp9(config);
	m_bcch=true;
}

void dct3_rf_adc_fixture_state::npe3sp9(machine_config &config)
{
	npe3bp9(config);
	m_full_broadcast=true;
}

void dct3_rf_adc_fixture_state::npe3tp5(machine_config &config)
{
	npe3sp9(config);
	m_p05_transmit=true;
}

void dct3_rf_adc_fixture_state::npe3ap5(machine_config &config)
{
	npe3tp5(config);
	m_raw_agch=true;
	// Passive comparison with the independently recovered NSM-3 word
	// grammar. Do not use its candidate as RF readiness or sample gating.
	auto &hagar=NOKIA_HAGAR(config,"hagar",0);
	hagar.set_trace(true);
	subdevice<nokia_dsp_c54x_device>("dsp_c54x")->set_transfer_trace(true);
	subdevice<nokia_dsp_c54x_device>("dsp_c54x")->transfer_compare_cb().set(hagar,FUNC(nokia_hagar_device::compare_output_w));
}

void dct3_rf_adc_fixture_state::machine_start()
{
	nokia_dct3_state::machine_start();
	m_candidate_timer=timer_alloc(FUNC(dct3_rf_adc_fixture_state::candidate_receive_start),this);
	m_transmit_timer=timer_alloc(FUNC(dct3_rf_adc_fixture_state::candidate_transmit_start),this);
	m_codec_timer=timer_alloc(FUNC(dct3_rf_adc_fixture_state::codec_frame),this);
	// Size the serialized traffic-channel block once; its layout is fixed.
	visit_tch_state([this](auto &field) { m_tch_state.resize(m_tch_state.size()+sizeof(field)); });
	save_item(NAME(m_tch_state));
	machine().save().register_presave(save_prepost_delegate(FUNC(dct3_rf_adc_fixture_state::save_tch_state),this));
	machine().save().register_postload(save_prepost_delegate(FUNC(dct3_rf_adc_fixture_state::load_tch_state),this));
	save_item(NAME(m_transmit_compare_written));
	save_item(NAME(m_agch_start_frame));
	save_item(NAME(m_agch_octets));
	save_item(NAME(m_agch_samples));
	save_item(NAME(m_normal_start_frame));save_item(NAME(m_ua_start_frame));
	save_item(NAME(m_normal_count));save_item(NAME(m_normal_bits));
	save_item(NAME(m_contention_sabm));save_item(NAME(m_contention_active));
	save_item(NAME(m_page_identity));save_item(NAME(m_page_identity_valid));
	save_item(NAME(m_page_pending));save_item(NAME(m_page_phase));save_item(NAME(m_page_offset));
	save_item(NAME(m_page_not_before));save_item(NAME(m_page_last_block));
	save_item(NAME(m_addressed_page_samples));
	save_item(NAME(m_frame_page_block));save_item(NAME(m_frame_paging_samples));
	save_item(NAME(m_ua_samples));
	save_item(NAME(m_lu_start_frame));save_item(NAME(m_lu_samples));
	save_item(NAME(m_setup_start_frame));save_item(NAME(m_setup_samples));
	save_item(NAME(m_service_accept_start_frame));save_item(NAME(m_service_accept_samples));
	save_item(NAME(m_call_proceeding_start_frame));
	save_item(NAME(m_assignment_start_frame));save_item(NAME(m_assignment_samples));
	save_item(NAME(m_call_rr_start_frame));save_item(NAME(m_call_rr_samples));
	save_item(NAME(m_call_rr_sequence));
	save_item(NAME(m_call_release_start_frame));
	save_item(NAME(m_call_channel_release_sent));
	save_item(NAME(m_sacch_samples));
	save_item(NAME(m_lu_acknowledged));save_item(NAME(m_release_start_frame));
	save_item(NAME(m_release_samples));
	save_item(NAME(m_lab_tx_compares));
	save_item(NAME(m_lab_tx.words)); save_item(NAME(m_lab_tx.count));
	save_item(NAME(m_lab_tx.loading)); save_item(NAME(m_lab_tx.ready)); save_item(NAME(m_lab_tx.overflow));
	if (m_p05_transmit) machine().logerror("npe3_tx_candidate: p05_start=1 port_buffer_only=1 network_reply=0 physical_claim=0\n");
	if (m_raw_agch) machine().logerror("npe3_bts_layout: combined_ccch=1 sdcch4=1 timeslot=0 agch_endings=9,15,19 physical_claim=0\n");
	save_item(NAME(m_candidate_active));
	if (m_tuned_air) m_dummy_samples=nokia_rf_lab::gmsk_samples(nokia_rf_lab::dummy_bits());
	save_item(NAME(m_synth_low));save_item(NAME(m_rx_arfcn));save_item(NAME(m_capture_arfcn));
	save_item(NAME(m_candidate_target));
	if (m_compare_start) machine().logerror("npe3_capture_candidate: p09_start=1 all_captures=1 block_request_latch=1 physical_claim=0 source_epoch_shift=0\n");
	if (m_full_broadcast)
	{
		machine().logerror("rf_adc_bcch: frames=2,3,4,5 tsc=2 schedule=0,1,2,3,1,1,2,3 repeated_si3_only=0 arfcn=%u epoch=0 physical_claim=0\n",m_lab_arfcn);
	}
	else if (m_bcch)
	{
		gsm::tch_f::packed_control_block const si3{0x49,0x06,0x1b,0x00,0x01,0x00,0xf1,0x10,
			0x00,0x01,0x40,0,0,0,0,0,0,0,0,0x2b,0x2b,0x2b,0x2b};
		auto const bursts=nokia_rf_lab::bcch_bits(si3,2);
		for (unsigned burst=0; burst<4; ++burst) m_bcch_samples[burst]=nokia_rf_lab::gmsk_samples(bursts[burst]);
		machine().logerror("rf_adc_bcch: frames=2,3,4,5 tsc=2 octets=49061b000100f11000014000000000000000002b2b2b2b repeated_si3_only=1 epoch=0 physical_claim=0\n");
	}
	save_item(NAME(m_q_latch));
	save_item(NAME(m_adc_receive_control)); save_item(NAME(m_adc_mfi_control));
	machine().logerror("rf_adc_lab: direct_adc=1 tuned_air_source=%u tone=%u reference=13000000 source_phase_divider=12 word_divider=%u amplitude=1024 physical_clock_claim=0\n",
		m_tuned_air,m_tone,subdevice<nokia_mfi_rx_device>("mfi_rx")->word_divider());
	if (m_sch)
		machine().logerror("rf_adc_sync: frame_ticks=60000 multiframe=51 fcch_frames=0,10,20,30,40 sch_frames=1,11,21,31,41 active_ticks=7104 epoch=0 envelope=rectangular bt=0.3 bsic=42 waveform_samples_per_symbol=4 other_slots=%s tuned_air_source=0\n",m_bcch ? "bcch" : "zero");
	else if (m_burst)
		machine().logerror("rf_adc_burst: frame_ticks=60000 multiframe=51 frames=0,10,20,30,40 active_ticks=7104 epoch=0 envelope=rectangular other_slots=zero tuned_air_source=0\n");
}

void dct3_rf_adc_fixture_state::machine_reset()
{
	m_lab_tx_compares.fill(0);
	m_agch_start_frame=~u64(0);
	m_normal_start_frame=m_ua_start_frame=~u64(0);
	m_lu_start_frame=~u64(0);
	m_setup_start_frame=~u64(0);
	m_service_accept_start_frame=~u64(0);
	m_call_proceeding_start_frame=~u64(0);
	m_assignment_start_frame=~u64(0);
	m_call_rr_start_frame=~u64(0);
	m_call_rr_sequence=0xff;
	m_call_release_start_frame=~u64(0);m_call_channel_release_sent=false;
	m_lu_acknowledged=false;m_release_start_frame=~u64(0);
	m_tch.stop();m_codec_timer->adjust(attotime::never);m_tch_stop_frame=m_tch_samples_frame=~u64(0);
	m_normal_count=0;m_contention_active=false;
	m_page_identity_valid=m_page_pending=false;
	m_page_identity.fill(0);m_page_phase=m_page_offset=0;
	m_page_not_before=m_page_last_block=~u64(0);
	m_frame_page_block=~u64(0);
	m_lab_tx.reset();
	m_observed_tx_windows=m_transmit_releases=0;
	m_transmit_compare_written=false;
	m_transmit_timer->adjust(attotime::never);
	nokia_dct3_state::machine_reset();
	if (!m_full_broadcast) return;
	// Topology is applied by the board reset, not machine_start. Cache only
	// standards data after that setup; no network packet/verdict is delivered.
	auto &network=*subdevice<nokia_gsm_network_device>("gsm_network");
	// Explicit laboratory BTS composition, not a detected/fitted carrier.
	auto cell=*network.cell_at(0);
	cell.arfcn=m_lab_arfcn;
	network.set_pcs1900_band(m_band==lab_band::pcs1900);
	cell.bsic=42;
	if (m_raw_agch)
	{
		cell.ms_txpwr_max_cch=(ioport("LAB_BTS")->read()&2) ? 5 : 0;
		machine().logerror("npe3_bts_initial_ms_power: level=%u gsm900_dbm=%u external_cell_policy=1 firmware_writes=0\n",
			unsigned(cell.ms_txpwr_max_cch),cell.ms_txpwr_max_cch==5 ? 33U : 39U);
	}
	network.use_single_cell();
	network.set_cell(0,cell);
	if (m_raw_agch)
	{
		// GSM 04.04 7.1: two L1 octets precede the 21-byte SACCH L2 frame.
		// SI5 reuses the neighbour list; SI6 reuses the cell's CI/LAI/options.
		auto const si2=network.system_information(1,m_lab_arfcn),si3=network.system_information(2,m_lab_arfcn);
		for (unsigned kind=0;kind<2;++kind)
		{
			gsm::tch_f::packed_control_block block{};
			block.fill(0x2b);block[0]=cell.ms_txpwr_max_cch;block[1]=0;
			block[2]=3;block[3]=3;block[5]=6;
			if (!kind)
			{
				block[4]=0x49;block[6]=0x1d;
				std::copy_n(si2.begin()+3,16,block.begin()+7);
			}
			else
			{
				block[4]=0x2d;block[6]=0x1e;
				std::copy_n(si3.begin()+3,7,block.begin()+7);
				block[14]=si3[13];block[15]=0xff;
			}
			m_sacch_blocks[kind]=block;
			// SACCH/4 on the SDCCH is optional; the TS1 SACCH/TF always carries them.
			if (!(ioport("LAB_BTS")->read()&64)) continue;
			auto const encoded=nokia_rf_lab::bcch_bits(block,0);
			for (unsigned at=0;at<4;++at) m_sacch_samples[kind][at]=nokia_rf_lab::gmsk_samples(encoded[at]);
			std::string packed;for (auto byte:block) packed+=util::string_format("%02x",byte);
			machine().logerror("npe3_bts_sacch: index=%u starts=42 period=102 octets=%s tsc=0 raw_adc_only=1 physical_claim=0\n",kind,packed);
		}
	}
	if (m_raw_agch)
	{
		auto const fill=network.paging_fill();
		gsm::tch_f::packed_control_block octets{};
		std::copy_n(fill.begin(),octets.size(),octets.begin());
		auto const bursts=nokia_rf_lab::bcch_bits(octets,2);
		for (unsigned at=0;at<4;++at) m_paging_samples[at]=nokia_rf_lab::gmsk_samples(bursts[at]);
		std::string packed;for (auto byte:octets) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_bts_pch: starts=6,12,16 octets=%s tsc=2 no_identity=1 raw_adc_only=1 physical_claim=0\n",packed);
	}
	for (unsigned kind=0; kind<4; ++kind)
	{
		auto const information=network.system_information(kind,m_lab_arfcn);
		gsm::tch_f::packed_control_block octets{};
		std::copy_n(information.begin(),octets.size(),octets.begin());
		// TS44.018 10.5.2.11: combined CCCH on C0/TS0, matching
		// this laboratory BTS's SDCCH/4 assignment. Other sources stay intact.
		if (m_raw_agch && kind==2) octets[10]=(octets[10]&0xf8)|1;
		auto const bursts=nokia_rf_lab::bcch_bits(octets,2);
		for (unsigned burst=0; burst<4; ++burst)
			m_broadcast_samples[kind][burst]=nokia_rf_lab::gmsk_samples(bursts[burst]);
		std::string hex;
		for (auto byte:octets) hex+=util::string_format("%02x",byte);
		machine().logerror("rf_adc_broadcast_si: index=%u arfcn=%u octets=%s physical_claim=0\n",kind,m_lab_arfcn,hex);
	}
}

INPUT_CHANGED_MEMBER(dct3_rf_adc_fixture_state::bts_page_w)
{
	if (!newval || !m_raw_agch) return;
	u64 const frame=machine().time().as_ticks(13'000'000)/60'000;
	if (!m_page_identity_valid || frame<m_page_not_before)
	{
		machine().logerror("npe3_bts_page_ignored: t=%.9f no_released_air_identity=1 physical_claim=0\n",machine().time().as_double());
		return;
	}
	unsigned const suffix=(m_page_identity[6]>>4)*100+(m_page_identity[7]&15)*10+(m_page_identity[7]>>4);
	// The raw BTS advertises combined CCCH,BS_AG_BLKS_RES=0 and
	// BS_PA_MFRMS=2. Do not reuse the HLE cell's nine-block DRX schedule.
	auto const slot=nokia::radio::subscriber_paging_slot(suffix,true,0,2);
	if (!slot) return;
	m_page_phase=slot->multiframe_phase;m_page_offset=slot->frame_offset;
	auto const page=subdevice<nokia_gsm_network_device>("gsm_network")->paging_request(m_page_identity.data(),m_page_identity.size());
	gsm::tch_f::packed_control_block octets{};
	std::copy_n(page.begin(),octets.size(),octets.begin());
	auto const bursts=nokia_rf_lab::bcch_bits(octets,2);
	for (unsigned at=0;at<4;++at) m_addressed_page_samples[at]=nokia_rf_lab::gmsk_samples(bursts[at]);
	m_page_pending=true;m_page_not_before=frame+1;m_page_last_block=~u64(0);
	std::string identity,packed;
	for (auto byte:m_page_identity) identity+=util::string_format("%02x",byte);
	for (auto byte:octets) packed+=util::string_format("%02x",byte);
	machine().logerror("npe3_bts_page_request: t=%.9f identity=%s phase=%u offset=%u multiframes=2 after_frame=%u octets=%s raw_adc_only=1 port_bits_identity=1 physical_claim=0\n",
		machine().time().as_double(),identity,m_page_phase,m_page_offset,unsigned(m_page_not_before),packed);
}

void dct3_rf_adc_fixture_state::control_w(offs_t port, u16 data)
{
	if (port==0x31 || port==0x32)
	{
		// Port 31/32 carry one accumulator's halves (rom4_rf_control.md).
		if (port==0x31) { m_synth_low=data; return; }
		u32 const word=(u32(data)<<16)|m_synth_low;
		if (!m_tuned_air || ((word>>20)&7)!=3) return;
		// NSE-1 manual: 64/65 UHF prescaler, 200 kHz steps, LO 71 MHz above
		// the 935 MHz GSM900 receive band, hence ARFCN = M - 5030. The DCS
		// 1800 and PCS 1900 siblings' power-up scans sweep M over 6591..6964
		// and 7216..7514: exactly 374 and 299 values, their bands' channel
		// counts from ARFCN 512. Their LO/IF plans are not documented here.
		struct band_plan { s32 offset, first, last; };
		static constexpr band_plan plans[]={{5030,1,124},{6079,512,885},{6704,512,810}};
		auto const &plan=plans[unsigned(m_band)];
		s32 const arfcn=s32(((word>>7)&0x1fff)*64+(word&0x7f))-plan.offset;
		m_rx_arfcn=arfcn>=plan.first && arfcn<=plan.last ? arfcn : -1;
		return;
	}
	if (port>=5 && port<=8)
	{
		m_lab_tx_compares[port-5]=data;
		if (m_p05_transmit && port==5)
		{
			m_transmit_compare_written=true;
			arm_transmit_candidate();
		}
		return; // Observation must not re-arm the separate receive candidate.
	}
	if (m_full_broadcast && port==0x21)
	{
		if (m_lab_tx.control(data))
		{
			// A sustained 24-second SDCCH includes SACCH and idle uplink blocks.
			if (m_observed_tx_windows++<(m_raw_agch ? 512U : 16U))
			{
				auto &backend=*subdevice<nokia_dsp_c54x_device>("dsp_c54x");
				std::array<double,4> deadlines{};
				for (unsigned i=0;i<4;++i)
				{
					auto const due=backend.lab_counter_deadline(m_lab_tx_compares[i]);
					deadlines[i]=due==attotime::never ? -1 : due.as_double();
				}
				machine().logerror("npe3_tx_window: sequence=%u t=%.9f words=%u counter=%04x controls=%04x,%04x,%04x,%04x deadlines=%.9f,%.9f,%.9f,%.9f observer_writes=0 physical_claim=0\n",
					m_observed_tx_windows,machine().time().as_double(),m_lab_tx.count+unsigned(m_lab_tx.overflow),backend.lab_counter_value(),
					m_lab_tx_compares[0],m_lab_tx_compares[1],m_lab_tx_compares[2],m_lab_tx_compares[3],
					deadlines[0],deadlines[1],deadlines[2],deadlines[3]);
			}
		}
	}
	if (m_p05_transmit && (port==0x0d || port==0x0e) && m_transmit_compare_written)
		arm_transmit_candidate();
	// Testable lab wiring, not a fitted MFI truth table. The always-on bit10
	// candidate overran before RF work; keep the observed port3 RX window.
	if (port == 3) m_adc_receive_control = data;
	if (port == 0x21) m_adc_mfi_control = data;
	if (port == 9) m_candidate_target = data;
	bool const enabled=BIT(m_adc_receive_control,0) && BIT(m_adc_mfi_control,10) && !BIT(m_adc_mfi_control,12);
	if (!m_compare_start) subdevice<nokia_mfi_rx_device>("mfi_rx")->enable_w(enabled);
	else if (!enabled)
	{
		m_candidate_timer->adjust(attotime::never);
		m_candidate_active=false;
		subdevice<nokia_mfi_rx_device>("mfi_rx")->enable_w(0);
	}
	else if (!m_candidate_active)
	{
		auto &backend=*subdevice<nokia_dsp_c54x_device>("dsp_c54x");
		u16 const target=m_candidate_target;
		auto const deadline=backend.lab_counter_deadline(target);
		if (deadline==attotime::never) m_candidate_timer->adjust(attotime::never);
		else m_candidate_timer->adjust(deadline-machine().time());
		machine().logerror("npe3_capture_arm: target=%04x t=%.9f deadline=%.9f physical_claim=0\n",target,machine().time().as_double(),deadline.as_double());
	}
}

void dct3_rf_adc_fixture_state::transmit_word_w(u16 data)
{
	if (m_full_broadcast) m_lab_tx.write(data);
}

void dct3_rf_adc_fixture_state::rf_reset_w(int released)
{
	if (released) return;
	m_lab_tx.reset();
	m_lab_tx_compares.fill(0);
	m_transmit_compare_written=false;
	if (m_transmit_timer) m_transmit_timer->adjust(attotime::never);
}

void dct3_rf_adc_fixture_state::arm_transmit_candidate()
{
	auto const due=subdevice<nokia_dsp_c54x_device>("dsp_c54x")->lab_counter_deadline(m_lab_tx_compares[0]);
	if (due==attotime::never)
	{
		m_transmit_timer->adjust(attotime::never);
		m_lab_tx.reset();
	}
	else m_transmit_timer->adjust(due-machine().time());
}

TIMER_CALLBACK_MEMBER(dct3_rf_adc_fixture_state::candidate_transmit_start)
{
	if (!m_p05_transmit) return;
	auto &backend=*subdevice<nokia_dsp_c54x_device>("dsp_c54x");
	if (backend.lab_counter_deadline(m_lab_tx_compares[0])==attotime::never)
	{
		arm_transmit_candidate();
		return;
	}
	if (backend.lab_counter_value()!=m_lab_tx_compares[0])
	{
		machine().logerror("npe3_tx_release: missed_counter=1 physical_claim=0\n");
		m_lab_tx.reset();
		arm_transmit_candidate();
		return;
	}
	std::array<u16,15> words{};
	if (m_lab_tx.release(words))
	{
		std::string packed;
		for (auto word:words) packed+=util::string_format("%s%04x",packed.empty() ? "" : ",",word);
		machine().logerror("npe3_tx_release: sequence=%u t=%.9f target=%04x counter=%04x words=%s network_reply=0 physical_claim=0\n",
			++m_transmit_releases,machine().time().as_double(),m_lab_tx_compares[0],
			subdevice<nokia_dsp_c54x_device>("dsp_c54x")->lab_counter_value(),packed);
		u64 const release_tick=machine().time().as_ticks(13'000'000);
		// The uplink trails the downlink by three timeslots: TS1 releases in the fifth.
		if (m_raw_agch && m_tch.active() && release_tick%60'000/7'500==4)
			receive_traffic_uplink(words,release_tick/60'000);
		else if (m_raw_agch)
		{
			// The external BTS sees only serialized bits and their release time.
			// RA_INFO, private DSP memory and MCU request state are not inputs.
			auto const access=gsm::rach::decode_p27(words,42);
			if (access)
			{
				if (m_page_pending)
					machine().logerror("npe3_bts_page_access: t=%.9f ra=%02x last_page_frame=%u port_bits_only=1 physical_claim=0\n",
						machine().time().as_double(),*access,unsigned(m_page_last_block));
				m_page_pending=false;m_page_not_before=~u64(0);
				// A new externally observed access starts a new single-MS lab
				// connection. No handset registration state is changed here.
				m_contention_active=m_lu_acknowledged=false;
				m_normal_count=0;
				m_ua_start_frame=m_lu_start_frame=m_release_start_frame=m_setup_start_frame=~u64(0);
				m_assignment_start_frame=~u64(0);
				m_service_accept_start_frame=~u64(0);
				m_call_proceeding_start_frame=~u64(0);
				m_call_rr_start_frame=~u64(0);
				m_call_rr_sequence=0xff;
				m_call_release_start_frame=~u64(0);m_call_channel_release_sent=false;
				u64 const frame=machine().time().as_ticks(13'000'000)/60'000;
				auto const assignment=subdevice<nokia_gsm_network_device>("gsm_network")->immediate_assignment(
					*access,frame%nokia_rf_lab::hyperframe,m_lab_arfcn);
				std::copy_n(assignment.begin(),m_agch_octets.size(),m_agch_octets.begin());
				auto const bursts=nokia_rf_lab::bcch_bits(m_agch_octets,2);
				for (unsigned burst=0; burst<4; ++burst)
					m_agch_samples[burst]=nokia_rf_lab::gmsk_samples(bursts[burst]);
				m_agch_start_frame=nokia::radio::common_control_block_end(frame+4,true)-3;
				std::string octets;
				for (auto byte:m_agch_octets) octets+=util::string_format("%02x",byte);
				machine().logerror("npe3_raw_agch: ra=%02x access_frame=%u start_frame=%u octets=%s raw_adc_only=1 tuned_air_source=0 physical_claim=0\n",
					*access,unsigned(frame),unsigned(m_agch_start_frame),octets);
			}
			else
			{
				machine().logerror("npe3_raw_agch_skip: not_access=1 response=0\n");
				receive_normal_uplink(words,machine().time().as_ticks(13'000'000)/60'000);
			}
		}
	}
	arm_transmit_candidate();
}

void dct3_rf_adc_fixture_state::receive_normal_uplink(std::array<u16,15> const &words,u64 frame)
{
	// TS45.002 table3: assigned SDCCH/4 subchannel0 uplink37..40,
	// downlink22..25. SACCH and other subchannels cannot trigger this reply.
	unsigned const position=frame%51;
	if (position<37 || position>40) return;
	auto const burst=gsm::normal::decode_p27(words,0);
	if (position==37) { m_normal_start_frame=frame;m_normal_count=0; }
	if (!burst || frame!=m_normal_start_frame+m_normal_count || m_normal_count>=4)
	{
		m_normal_count=0;m_normal_start_frame=~u64(0);
		return;
	}
	m_normal_bits[m_normal_count++]=burst->data;
	if (m_normal_count!=4) return;
	std::array<gsm::tch_f::burst_payload,4> bursts{};
	for (unsigned at=0;at<4;++at)
	{
		bursts[at].data=m_normal_bits[at];bursts[at].hl=bursts[at].hu=1;
	}
	auto const block=gsm::normal::decode_control(bursts);
	if (!block) return;
	std::string packed;
	for (auto byte:*block) packed+=util::string_format("%02x",byte);
	machine().logerror("npe3_bts_uplink: start_frame=%u octets=%s exact_code=1 port_bits_only=1 physical_claim=0\n",
		unsigned(m_normal_start_frame),packed);
	bool const late_assignment=bool(ioport("LAB_BTS")->read()&128);
	if ((ioport("LAB_BTS")->read()&0x200) && m_service_accept_start_frame!=~u64(0) &&
			m_call_proceeding_start_frame==~u64(0) && m_normal_start_frame>m_service_accept_start_frame+3 &&
			(*block)[0]==1 && (*block)[1]==0x20 && ((*block)[2]&3)==1 &&
			((*block)[2]>>2)>=2 && ((*block)[2]>>2)<=20 && (*block)[3]==3 && ((*block)[4]&63)==5)
	{
		auto const proceeding=subdevice<nokia_gsm_network_device>("gsm_network")->call_proceeding((*block)[3]);
		gsm::tch_f::packed_control_block lapdm{};
		lapdm.fill(0x2b);lapdm[0]=3;lapdm[1]=0x22;lapdm[2]=(proceeding.size()<<2)|1;
		std::copy(proceeding.begin(),proceeding.end(),lapdm.begin()+3);
		// Network N(S)=1 follows CM SERVICE ACCEPT; N(R)=1 acknowledges SETUP.
		m_call_proceeding_start_frame=m_call_rr_start_frame=(frame/51+1)*51+22;
		auto const encoded=nokia_rf_lab::bcch_bits(lapdm,0);
		for (unsigned at=0;at<4;++at) m_call_rr_samples[at]=nokia_rf_lab::gmsk_samples(encoded[at]);
		packed.clear();for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_raw_call_proceeding: setup_start_frame=%u start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
			unsigned(m_normal_start_frame),unsigned(m_call_proceeding_start_frame),packed);
		return;
	}
	if ((ioport("LAB_BTS")->read()&0x208)==0x208 && m_call_proceeding_start_frame!=~u64(0) &&
			m_assignment_start_frame==~u64(0) && m_normal_start_frame>m_call_proceeding_start_frame+3 &&
			(*block)[0]==3 && (*block)[1]==0x41 && (*block)[2]==1)
	{
		auto const assignment=subdevice<nokia_gsm_network_device>("gsm_network")->traffic_assignment(m_lab_arfcn);
		gsm::tch_f::packed_control_block lapdm{};
		// Network N(S)=2, N(R)=1: no further mobile I-frame was received.
		lapdm.fill(0x2b);lapdm[0]=3;lapdm[1]=0x24;lapdm[2]=(assignment.size()<<2)|1;
		std::copy(assignment.begin(),assignment.end(),lapdm.begin()+3);
		m_assignment_start_frame=(frame/51+1)*51+22;
		auto const encoded=nokia_rf_lab::bcch_bits(lapdm,0);
		for (unsigned at=0;at<4;++at) m_assignment_samples[at]=nokia_rf_lab::gmsk_samples(encoded[at]);
		packed.clear();for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_raw_mo_assignment: ack_start_frame=%u start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
			unsigned(m_normal_start_frame),unsigned(m_assignment_start_frame),packed);
		start_traffic_channel(m_assignment_start_frame+4);
		return;
	}
	if ((ioport("LAB_BTS")->read()&16) &&
			(!(ioport("LAB_BTS")->read()&8) || (late_assignment &&
			 m_assignment_start_frame==~u64(0) && ((*block)[4]&0x3f)!=7)) &&
			m_setup_start_frame!=~u64(0) &&
			m_normal_start_frame>m_setup_start_frame+3 &&
			(*block)[0]==1 && !((*block)[1]&1) && ((*block)[2]&3)==1 &&
			((*block)[2]>>2)>=2 && ((*block)[2]>>2)<=20 &&
			(*block)[3]==0x83 && (((*block)[1]>>1)&7)!=m_call_rr_sequence)
	{
		// GSM 04.06 3.8.5: RR N(R) acknowledges the observed N(S).
		// This finite no-loss window is independent of optional L3 clearing;
		// neither path assigns a traffic channel.
		m_call_rr_sequence=((*block)[1]>>1)&7;
		gsm::tch_f::packed_control_block rr{};
		rr.fill(0x2b);rr[0]=3;rr[1]=(((m_call_rr_sequence+1)&7)<<5)|1;rr[2]=1;
		m_call_rr_start_frame=(frame/51+1)*51+22;
		char const *reply_kind="call_rr",*source_kind="uplink";
		if ((ioport("LAB_BTS")->read()&32) && ((*block)[4]&0x3f)==0x25 &&
				((*block)[2]>>2)>=5 && (*block)[5]>=2 && (*block)[5]<=((*block)[2]>>2)-3 &&
				m_call_release_start_frame==~u64(0))
		{
			// GSM 04.08 5.4.3.2: RELEASE follows the actual mobile DISCONNECT.
			// Network N(S)=1 follows SETUP; N(R) comes from this uplink.
			auto const release=subdevice<nokia_gsm_network_device>("gsm_network")->call_release((*block)[3]);
			rr[1]=(rr[1]&0xe0)|2;rr[2]=(release.size()<<2)|1;
			std::copy(release.begin(),release.end(),rr.begin()+3);
			m_call_release_start_frame=m_call_rr_start_frame;
			reply_kind="cc_release";source_kind="disconnect";
		}
		if ((ioport("LAB_BTS")->read()&32) && ((*block)[4]&0x3f)==0x2a &&
				m_call_release_start_frame!=~u64(0) && m_normal_start_frame>m_call_release_start_frame+3 &&
				!m_call_channel_release_sent)
		{
			auto const release=subdevice<nokia_gsm_network_device>("gsm_network")->channel_release();
			rr[1]=(rr[1]&0xe0)|4;rr[2]=(release.size()<<2)|1;
			std::copy(release.begin(),release.end(),rr.begin()+3);
			m_release_start_frame=m_call_rr_start_frame;m_call_channel_release_sent=true;
			reply_kind="call_channel_release";source_kind="complete";
		}
		// I-frame clearing replies carry the same N(R) as the standalone RR.
		auto const encoded=nokia_rf_lab::bcch_bits(rr,0);
		for (unsigned at=0;at<4;++at) m_call_rr_samples[at]=nokia_rf_lab::gmsk_samples(encoded[at]);
		if (m_call_channel_release_sent) m_release_samples=m_call_rr_samples;
		packed.clear();for (auto byte:rr) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_raw_%s: %s_start_frame=%u start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
			reply_kind,source_kind,unsigned(m_normal_start_frame),unsigned(m_call_rr_start_frame),packed);
		return;
	}
	if ((ioport("LAB_BTS")->read()&8) && m_setup_start_frame!=~u64(0) && m_assignment_start_frame==~u64(0) &&
			m_normal_start_frame>m_setup_start_frame+3 &&
			(*block)[0]==1 && (*block)[1]==(late_assignment ? 0x24 : 0x20) && ((*block)[2]&3)==1 &&
			((*block)[2]>>2)>=2 && ((*block)[2]>>2)<=20 &&
			(*block)[3]==0x83 && ((*block)[4]&0x3f)==(late_assignment ? 7 : 8))
	{
		// Second network I-frame,N(S)=1; N(R) acknowledges the actual
		// Call Confirmed or CONNECT. Earlier RR replies do not consume N(S).
		auto const assignment=subdevice<nokia_gsm_network_device>("gsm_network")->traffic_assignment(m_lab_arfcn);
		gsm::tch_f::packed_control_block lapdm{};
		lapdm.fill(0x2b);lapdm[0]=3;lapdm[1]=(((((*block)[1]>>1)+1)&7)<<5)|2;lapdm[2]=(assignment.size()<<2)|1;
		std::copy(assignment.begin(),assignment.end(),lapdm.begin()+3);
		m_assignment_start_frame=(frame/51+1)*51+22;
		auto const encoded=nokia_rf_lab::bcch_bits(lapdm,0);
		for (unsigned at=0;at<4;++at) m_assignment_samples[at]=nokia_rf_lab::gmsk_samples(encoded[at]);
		packed.clear();for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_raw_%s: %s_start_frame=%u start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
			late_assignment ? "late_assignment" : "assignment",late_assignment ? "connect" : "confirmed",
			unsigned(m_normal_start_frame),unsigned(m_assignment_start_frame),packed);
		start_traffic_channel(m_assignment_start_frame+4);
		return;
	}
	if (!m_lu_acknowledged && m_lu_start_frame!=~u64(0) &&
			m_normal_start_frame>m_lu_start_frame+3 &&
			(*block)[0]==3 && (*block)[1]==0x21 && (*block)[2]==1)
	{
		m_lu_acknowledged=true;
		auto const release=subdevice<nokia_gsm_network_device>("gsm_network")->channel_release();
		gsm::tch_f::packed_control_block lapdm{};
		lapdm.fill(0x2b);
		// The second network I-frame: N(S)=1,N(R)=0,P=0.
		lapdm[0]=3;lapdm[1]=2;lapdm[2]=(release.size()<<2)|1;
		std::copy(release.begin(),release.end(),lapdm.begin()+3);
		m_release_start_frame=(frame/51+1)*51+22;
		auto const bursts=nokia_rf_lab::bcch_bits(lapdm,0);
		for (unsigned at=0;at<4;++at) m_release_samples[at]=nokia_rf_lab::gmsk_samples(bursts[at]);
		packed.clear();for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_raw_release: ack_start_frame=%u start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
			unsigned(m_normal_start_frame),unsigned(m_release_start_frame),packed);
		return;
	}
	if (m_release_start_frame!=~u64(0) && m_normal_start_frame>m_release_start_frame+3)
	{
		auto const ua=gsm::normal::disconnect_ua(*block);
		if (ua)
		{
			// DISC ends this LAPDm contention context; a subsequent access may
			// establish a new link with different initial information.
			m_contention_active=false;
			m_ua_start_frame=(frame/51+1)*51+22;
			m_page_not_before=m_ua_start_frame+4;
			auto const bursts=nokia_rf_lab::bcch_bits(*ua,0);
			for (unsigned at=0;at<4;++at) m_ua_samples[at]=nokia_rf_lab::gmsk_samples(bursts[at]);
			packed.clear();for (auto byte:*ua) packed+=util::string_format("%02x",byte);
			machine().logerror("npe3_raw_disc_ua: disc_start_frame=%u start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
				unsigned(m_normal_start_frame),unsigned(m_ua_start_frame),packed);
			return;
		}
	}
	auto const ua=gsm::normal::contention_ua(*block);
	if (!ua) return;
	if (m_contention_active &&
			(m_contention_sabm[2]!=(*block)[2] ||
			 !std::equal(block->begin()+3,block->begin()+3+((*block)[2]>>2),m_contention_sabm.begin()+3))) return;
	m_contention_sabm=*block;m_contention_active=true;
	m_ua_start_frame=(frame/51+1)*51+22;
	auto const reply_bursts=nokia_rf_lab::bcch_bits(*ua,0);
	for (unsigned at=0;at<4;++at) m_ua_samples[at]=nokia_rf_lab::gmsk_samples(reply_bursts[at]);
	packed.clear();
	for (auto byte:*ua) packed+=util::string_format("%02x",byte);
	machine().logerror("npe3_raw_ua: uplink_start_frame=%u start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
		unsigned(m_normal_start_frame),unsigned(m_ua_start_frame),packed);
	// A declared plain laboratory registration policy,not an authentication
	// verdict supplied to the handset. Derive identity only from air L3.
	unsigned const length=(*block)[2]>>2;
	auto const *request=block->data()+3;
	if ((ioport("LAB_BTS")->read()&0x100) && m_page_identity_valid &&
			m_service_accept_start_frame==~u64(0) && length==16 &&
			request[0]==5 && request[1]==0x24 && (request[2]&15)==1 &&
			request[3]==3 && request[7]==8 &&
			std::equal(m_page_identity.begin(),m_page_identity.end(),request+8))
	{
		// Plain test-cell admission of the actual air-originated call request.
		auto const accept=subdevice<nokia_gsm_network_device>("gsm_network")->cm_service_accept();
		gsm::tch_f::packed_control_block lapdm{};
		lapdm.fill(0x2b);lapdm[0]=3;lapdm[1]=0;lapdm[2]=(accept.size()<<2)|1;
		std::copy(accept.begin(),accept.end(),lapdm.begin()+3);
		m_service_accept_start_frame=m_ua_start_frame+51;
		auto const encoded=nokia_rf_lab::bcch_bits(lapdm,0);
		for (unsigned at=0;at<4;++at) m_service_accept_samples[at]=nokia_rf_lab::gmsk_samples(encoded[at]);
		packed.clear();for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_raw_cm_service_accept: uplink_start_frame=%u start_frame=%u octets=%s policy=plain_test_cell raw_adc_only=1 physical_claim=0\n",
			unsigned(m_normal_start_frame),unsigned(m_service_accept_start_frame),packed);
	}
	// Accept the response only from the established air identity. No handset
	// state participates in the external network's call-admission decision.
	if (m_page_identity_valid && length==16 && request[0]==6 && request[1]==0x27 &&
			request[2]<=7 && request[3]==3 && request[7]==8 &&
			std::equal(m_page_identity.begin(),m_page_identity.end(),request+8))
	{
		m_page_pending=false;
		if ((ioport("LAB_BTS")->read()&4) && m_setup_start_frame==~u64(0))
		{
			static constexpr u8 digits[]={5,5,5,1,2,3,4};
			auto const setup=subdevice<nokia_gsm_network_device>("gsm_network")->incoming_call_setup(digits,std::size(digits));
			gsm::tch_f::packed_control_block lapdm{};
			lapdm.fill(0x2b);
			lapdm[0]=3;lapdm[1]=0;lapdm[2]=(setup.length<<2)|1;
			std::copy_n(setup.data.begin(),setup.length,lapdm.begin()+3);
			m_setup_start_frame=m_ua_start_frame+51;
			auto const bursts=nokia_rf_lab::bcch_bits(lapdm,0);
			for (unsigned at=0;at<4;++at) m_setup_samples[at]=nokia_rf_lab::gmsk_samples(bursts[at]);
			packed.clear();for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
			machine().logerror("npe3_raw_call_setup: uplink_start_frame=%u start_frame=%u octets=%s policy=plain_test_cell raw_adc_only=1 physical_claim=0\n",
				unsigned(m_normal_start_frame),unsigned(m_setup_start_frame),packed);
		}
	}
	if (length==18 && request[0]==5 && request[1]==8 && request[9]==8 && (request[10]&7)==1)
	{
		bool valid=bool(request[10]&8);
		for (unsigned at=0;at<8;++at)
			valid=valid && (request[10+at]>>4)<=9 && (at==0 || (request[10+at]&15)<=9);
		if (valid)
		{
			std::copy_n(request+10,m_page_identity.size(),m_page_identity.begin());
			m_page_identity_valid=true;
		}
		auto const accept=subdevice<nokia_gsm_network_device>("gsm_network")->location_update_accept(request,length,m_lab_arfcn);
		gsm::tch_f::packed_control_block lapdm{};
		lapdm.fill(0x2b);
		lapdm[0]=3;lapdm[1]=0;lapdm[2]=(accept.size()<<2)|1;
		std::copy(accept.begin(),accept.end(),lapdm.begin()+3);
		m_lu_start_frame=m_ua_start_frame+51;
		auto const accept_bursts=nokia_rf_lab::bcch_bits(lapdm,0);
		for (unsigned at=0;at<4;++at) m_lu_samples[at]=nokia_rf_lab::gmsk_samples(accept_bursts[at]);
		packed.clear();
		for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_raw_lu_accept: uplink_start_frame=%u start_frame=%u octets=%s policy=plain_test_cell raw_adc_only=1 physical_claim=0\n",
			unsigned(m_normal_start_frame),unsigned(m_lu_start_frame),packed);
	}
}

template <typename F> void dct3_rf_adc_fixture_state::visit_tch_state(F &&field)
{
	m_tch.visit_state(field);
	field(m_facch_send);field(m_facch_receive);field(m_facch_t200);
	field(m_facch_window);field(m_facch_window_count);
	field(m_tch_stop_frame);field(m_tch_samples_frame);field(m_tch_samples);
	field(m_tch_codec_state);field(m_tch_speech_enabled);
	field(m_tch_tone_phase);field(m_tch_speech_frames);
	field(m_tch_speech);field(m_tch_bad_speech);field(m_tch_unreadable);
	field(m_earpiece);field(m_earpiece_at);field(m_microphone_phase);
	field(m_earpiece_meter);field(m_uplink_meter);
}

void dct3_rf_adc_fixture_state::save_tch_state()
{
	m_facch_window_count=std::min<size_t>(m_facch_outstanding.size(),m_facch_window.size());
	std::copy_n(m_facch_outstanding.begin(),m_facch_window_count,m_facch_window.begin());
	m_tch_codec_state={m_tch_codec.snapshot(),m_tch_uplink_codec.snapshot()};
	size_t at=0;
	visit_tch_state([&](auto &field) {
		static_assert(std::is_trivially_copyable_v<std::remove_reference_t<decltype(field)>>);
		std::memcpy(m_tch_state.data()+at,&field,sizeof(field));at+=sizeof(field);
	});
}

void dct3_rf_adc_fixture_state::load_tch_state()
{
	size_t at=0;
	visit_tch_state([&](auto &field) { std::memcpy(&field,m_tch_state.data()+at,sizeof(field));at+=sizeof(field); });
	m_facch_outstanding.assign(m_facch_window.begin(),m_facch_window.begin()+m_facch_window_count);
	if (!m_tch_codec.restore(m_tch_codec_state[0]) || !m_tch_uplink_codec.restore(m_tch_codec_state[1]))
		fatalerror("laboratory TCH: invalid codec state in save image\n");
	set_tch_speech_source();
}

void dct3_rf_adc_fixture_state::set_tch_speech_source()
{
	// Optionally fill every traffic block not stolen for FACCH with speech,
	// as a BTS does; otherwise the blocks stay empty and decode as BFI. One
	// tone period is eight samples at 8 kHz, about -12 dBFS.
	if (!m_tch_speech_enabled) { m_tch.set_speech_source(nullptr); return; }
	m_tch.set_speech_source([this] {
		static constexpr s16 tone[8]={0,5793,8192,5793,0,-5793,-8192,-5793};
		// A steady tone is fully predicted by the long-term predictor, so its
		// frames carry no excitation (xmaxc 0, bc 3) and a decoder that joins
		// mid-stream keeps its empty history. Restarting the encoder each
		// second puts the whole signal back in one frame's excitation, which
		// resynchronises the handset's decoder; speech does this naturally.
		if (!(m_tch_speech_frames%50)) m_tch_codec.reset();
		nokia_gsm_fr_codec::pcm_block pcm{};
		for (auto &sample:pcm) sample=tone[m_tch_tone_phase++&7];
		gsm::tch_f::packed_speech_frame frame{};
		m_tch_codec.encode(pcm,frame);
		++m_tch_speech_frames;
		return frame;
	});
}

void dct3_rf_adc_fixture_state::start_traffic_channel(u64 frame)
{
	m_tch_codec.reset();m_tch_uplink_codec.reset();
	m_tch_tone_phase=m_tch_speech_frames=0;
	m_tch_speech_enabled=ioport("LAB_BTS")->read()&0x800;
	set_tch_speech_source();
	// The Assignment Command's Channel Description: TCH/F, TN1, TSC 2.
	m_tch.start(frame,1,2,m_sacch_blocks);
	m_facch_send=m_facch_receive=0;
	m_facch_outstanding.clear();m_facch_t200=~u64(0);
	m_tch_stop_frame=m_tch_samples_frame=~u64(0);
	m_tch_speech=m_tch_bad_speech=m_tch_unreadable=0;
	m_earpiece_at=m_microphone_phase=0;
	m_earpiece_meter=m_uplink_meter=tone_meter{};
	m_codec_timer->adjust(attotime::from_hz(8'000),0,attotime::from_hz(8'000));
	machine().logerror("npe3_tch_start: first_frame=%u timeslot=1 tsc=2 sacch=si5,si6 speech=%s codec=%s raw_adc_only=1 physical_claim=0\n",
		unsigned(m_tch.first_frame()),m_tch_speech_enabled ? "tone_1000hz" : "empty",m_tch_codec.available() ? "gsm_06_10" : "unavailable");
}

void dct3_rf_adc_fixture_state::stop_traffic_channel()
{
	m_tch.stop();
	m_codec_timer->adjust(attotime::never);
	machine().logerror("npe3_tch_audio: earpiece_hz=1000 earpiece_blocks=%u earpiece_tone_blocks=%u earpiece_tone_amplitude=%d microphone_hz=%u uplink_blocks=%u uplink_tone_blocks=%u uplink_tone_amplitude=%d physical_claim=0\n",
		m_earpiece_meter.blocks,m_earpiece_meter.tone_blocks,m_earpiece_meter.peak,m_tch_speech_enabled ? 500 : 0,
		m_uplink_meter.blocks,m_uplink_meter.tone_blocks,m_uplink_meter.peak);
}

void dct3_rf_adc_fixture_state::measure_tone(tone_meter &meter,nokia_gsm_fr_codec::pcm_block const &block,double hz)
{
	// A 160-sample block resolves 50 Hz bins. The tone counts when it stands
	// well above the bins a quarter of its frequency either side, which the
	// other tone, its sidetone and their harmonics do not reach.
	auto const amplitude=[&block](double frequency) {
		double in_phase=0,quadrature=0;
		for (unsigned at=0;at<block.size();++at)
		{
			double const phase=2*M_PI*frequency*at/8'000;
			in_phase+=block[at]*std::cos(phase);quadrature+=block[at]*std::sin(phase);
		}
		return 2*std::hypot(in_phase,quadrature)/block.size();
	};
	if (std::all_of(block.begin(),block.end(),[](s16 sample) { return !sample; })) return;
	++meter.blocks;
	double const tone=amplitude(hz);
	meter.peak=std::max(meter.peak,s32(std::lround(tone)));
	if (tone>=1 && tone>4*std::max(amplitude(hz*0.75),amplitude(hz*1.25))) ++meter.tone_blocks;
}

TIMER_CALLBACK_MEMBER(dct3_rf_adc_fixture_state::codec_frame)
{
	// The lab stands in for COBBA's 8 kHz PCM frame strobe on the DSP serial
	// port; the existing untimed boot echo keeps its loopback frames.
	auto &codec=*subdevice<nokia_cobba_device>("cobba");
	if (codec.codec_serial_loopback()) return;
	auto &backend=*subdevice<nokia_dsp_c54x_device>("dsp_c54x");
	u16 output=0;
	backend.codec_transmit_frame(output);
	static constexpr s16 microphone[16]={0,3135,5793,7568,8192,7568,5793,3135,0,-3135,-5793,-7568,-8192,-7568,-5793,-3135};
	backend.codec_receive_frame(m_tch_speech_enabled ? u16(microphone[m_microphone_phase++&15]) : 0);
	m_earpiece[m_earpiece_at++]=s16(output);
	if (m_earpiece_at<m_earpiece.size()) return;
	m_earpiece_at=0;
	codec.write_earpiece_pcm(m_earpiece);
	measure_tone(m_earpiece_meter,m_earpiece,1'000);
}

void dct3_rf_adc_fixture_state::receive_traffic_uplink(std::array<u16,15> const &words,u64 frame)
{
	auto const burst=gsm::normal::decode_p27_traffic(words,2);
	if (!burst) ++m_tch_unreadable;
	using kind=nokia_rf_lab::traffic_channel::uplink_kind;
	for (auto const &event:m_tch.uplink(frame,burst))
	{
		if (event.kind==kind::speech)
		{
			++m_tch_speech;
			nokia_gsm_fr_codec::pcm_block pcm{};
			if (m_tch_uplink_codec.decode(event.speech,pcm)) measure_tone(m_uplink_meter,pcm,500);
			continue;
		}
		if (event.kind==kind::bad_speech) { ++m_tch_bad_speech;continue; }
		bool const sacch=gsm::tch_f::full_rate_slot(event.frame,1)==gsm::tch_f::tdma_slot_kind::sacch;
		std::string packed;
		for (auto byte:event.octets) packed+=util::string_format("%02x",byte);
		machine().logerror("npe3_tch_uplink: channel=%s start_frame=%u octets=%s exact_code=%u speech_blocks=%u bad_speech_blocks=%u unreadable_bursts=%u downlink_speech_blocks=%u port_bits_only=1 physical_claim=0\n",
			sacch ? "sacch" : "facch",unsigned(event.frame),packed,event.kind!=kind::bad_control,
			m_tch_speech,m_tch_bad_speech,m_tch_unreadable,m_tch_speech_frames);
		if (event.kind==kind::facch) receive_facch(event.octets,event.frame);
	}
}

void dct3_rf_adc_fixture_state::receive_facch(gsm::tch_f::packed_control_block const &block,u64 frame)
{
	// GSM 04.06 on the new channel's SAPI0 link, network side.
	if ((block[0]&0xfd)!=1) return;
	gsm::tch_f::packed_control_block reply{};
	reply.fill(0x2b);
	if ((block[1]==0x3f || block[1]==0x53) && block[2]==1)
	{
		// SABM resets both sequence variables; DISC ends the link.
		bool const disc=block[1]==0x53;
		if (!disc) m_facch_send=m_facch_receive=0;
		m_facch_outstanding.clear();m_facch_t200=~u64(0);
		reply[0]=1;reply[1]=0x73;reply[2]=1;
		send_facch(disc ? "disc_ua" : "ua",reply,frame);
		if (disc)
		{
			// Two traffic blocks carry the UA; the handset then leaves TS1.
			m_tch_stop_frame=frame+60;
			m_contention_active=false;
		}
		return;
	}
	// Every I and S frame carries N(R), acknowledging network I-frames.
	acknowledge_facch(block[1]>>5);
	if (block[1]&1)
	{
		// REJ asks for every unacknowledged I-frame again, from N(R).
		if ((block[1]&0x0f)==0x09) resend_facch("rej",frame);
		return;
	}
	unsigned const length=block[2]>>2;
	if ((block[2]&3)!=1 || length<2 || length>20) return;
	// RR is a network response (C/R=0); F echoes the I-frame's P bit.
	u8 const final=block[1]&0x10;
	if (((block[1]>>1)&7)!=m_facch_receive)
	{
		reply[0]=1;reply[1]=(m_facch_receive<<5)|final|1;reply[2]=1;
		send_facch("duplicate_rr",reply,frame);
		return;
	}
	m_facch_receive=(m_facch_receive+1)&7;
	u8 const *const l3=block.data()+3;
	auto &network=*subdevice<nokia_gsm_network_device>("gsm_network");
	std::vector<u8> message;
	char const *kind="rr";
	u8 kind_index=0;
	if ((l3[0]&15)==3 && (l3[1]&0x3f)==0x07)
	{
		auto const ack=network.connect_acknowledge(l3[0]);
		message.assign(ack.begin(),ack.end());kind_index=0;kind=facch_kinds[kind_index];
	}
	else if ((l3[0]&15)==3 && (l3[1]&0x3f)==0x25 && (ioport("LAB_BTS")->read()&32))
	{
		auto const release=network.call_release(l3[0]);
		message.assign(release.begin(),release.end());kind_index=1;kind=facch_kinds[kind_index];
	}
	else if ((l3[0]&15)==3 && (l3[1]&0x3f)==0x2a && (ioport("LAB_BTS")->read()&32))
	{
		auto const release=network.channel_release();
		message.assign(release.begin(),release.end());kind_index=2;kind=facch_kinds[kind_index];
	}
	if (message.empty())
	{
		reply[0]=1;reply[1]=(m_facch_receive<<5)|final|1;reply[2]=1;
		send_facch(kind,reply,frame);
		return;
	}
	facch_information information{m_facch_send,kind_index,u8(message.size()),{}};
	std::copy(message.begin(),message.end(),information.layer3.begin());
	m_facch_outstanding.push_back(information);
	m_facch_send=(m_facch_send+1)&7;
	send_facch_information(m_facch_outstanding.back(),frame);
}

void dct3_rf_adc_fixture_state::send_facch_information(facch_information const &information,u64 frame)
{
	// An I-frame command (C/R=1) also acknowledges through its N(R).
	gsm::tch_f::packed_control_block lapdm{};
	lapdm.fill(0x2b);
	lapdm[0]=3;lapdm[1]=(m_facch_receive<<5)|(information.sequence<<1);
	lapdm[2]=(information.length<<2)|1;
	std::copy_n(information.layer3.begin(),information.length,lapdm.begin()+3);
	send_facch(facch_kinds[information.kind],lapdm,frame);
	// TS 44.006 T200 for FACCH/F is 155 ms; allow for both interleaving delays.
	m_facch_t200=frame+40;
}

void dct3_rf_adc_fixture_state::acknowledge_facch(u8 received)
{
	// Frames up to N(R)-1 are acknowledged; the window holds at most seven.
	while (!m_facch_outstanding.empty() && ((received-m_facch_outstanding.front().sequence)&7) &&
			((received-m_facch_outstanding.front().sequence)&7)<=m_facch_outstanding.size())
		m_facch_outstanding.erase(m_facch_outstanding.begin());
	if (m_facch_outstanding.empty()) m_facch_t200=~u64(0);
}

void dct3_rf_adc_fixture_state::resend_facch(char const *cause,u64 frame)
{
	if (m_facch_outstanding.empty()) return;
	machine().logerror("npe3_tch_facch_recovery: cause=%s frame=%u outstanding=%u first_sequence=%u physical_claim=0\n",
		cause,unsigned(frame),unsigned(m_facch_outstanding.size()),unsigned(m_facch_outstanding.front().sequence));
	for (auto const &information:m_facch_outstanding) send_facch_information(information,frame);
}

void dct3_rf_adc_fixture_state::send_facch(char const *kind,gsm::tch_f::packed_control_block const &lapdm,u64 frame)
{
	m_tch.send_facch(lapdm);
	std::string packed;
	for (auto byte:lapdm) packed+=util::string_format("%02x",byte);
	machine().logerror("npe3_tch_facch: kind=%s uplink_start_frame=%u octets=%s raw_adc_only=1 physical_claim=0\n",
		kind,unsigned(frame),packed);
}

TIMER_CALLBACK_MEMBER(dct3_rf_adc_fixture_state::candidate_receive_start)
{
	m_candidate_active=true;
	m_capture_arfcn=m_rx_arfcn;
	u16 const counter=subdevice<nokia_dsp_c54x_device>("dsp_c54x")->lab_counter_value();
	machine().logerror("npe3_capture_due: target=%04x counter=%04x t=%.9f physical_claim=0\n",m_candidate_target,counter,machine().time().as_double());
	subdevice<nokia_mfi_rx_device>("mfi_rx")->enable_w(1);
}

void dct3_rf_adc_fixture_state::fault_observe_w(int state)
{
	if (!state) return;
	auto &cpu = *subdevice<tms320c54x_device>("dsp_c54x:cpu");
	machine().logerror("rf_adc_lab_fault_context: cpu_pc=%06x st1=%04x imr=%04x ifr=%04x observer_writes=0 t=%.9f\n",
		u32(cpu.state_int(tms320c54x_device::STATE_PC)),u16(cpu.state_int(tms320c54x_device::STATE_ST1)),
		u16(cpu.state_int(tms320c54x_device::STATE_IMR)),u16(cpu.state_int(tms320c54x_device::STATE_IFR)),machine().time().as_double());
}

u16 dct3_rf_adc_fixture_state::sample_r(offs_t component)
{
	// Independently reproducible periodic laboratory oscillator, not a finite
	// capture wrapped on exhaustion. I/Q conversion is simultaneous; Q stays
	// latched across the following MFI scalar transfer.
	static constexpr s16 i[16] = {1024,946,724,392,0,-392,-724,-946,-1024,-946,-724,-392,0,392,724,946};
	static constexpr s16 q[16] = {0,392,724,946,1024,946,724,392,0,-392,-724,-946,-1024,-946,-724,-392};
	if (!m_tone) return 0;
	if (component) return u16(m_q_latch);
	if (m_tuned_air && m_capture_arfcn!=s32(m_lab_arfcn))
	{
		m_q_latch=0;
		return 0;
	}
	u64 const tick = machine().time().as_ticks(13'000'000);
	u64 const frame=tick/60'000;
	if (m_raw_agch && frame>=m_service_accept_start_frame && frame-m_service_accept_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_service_accept_samples[frame-m_service_accept_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && (ioport("LAB_BTS")->read()&64) && m_contention_active &&
			frame%102>=42 && frame%102<46 && tick%60'000<148*48)
	{
		auto const &sample=m_sacch_samples[(frame/102)&1][frame%102-42][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && frame>=m_call_rr_start_frame && frame-m_call_rr_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_call_rr_samples[frame-m_call_rr_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && frame>=m_assignment_start_frame && frame-m_assignment_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_assignment_samples[frame-m_assignment_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && frame>=m_setup_start_frame && frame-m_setup_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_setup_samples[frame-m_setup_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && frame>=m_release_start_frame && frame-m_release_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_release_samples[frame-m_release_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && frame>=m_lu_start_frame && frame-m_lu_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_lu_samples[frame-m_lu_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && frame>=m_ua_start_frame && frame-m_ua_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_ua_samples[frame-m_ua_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_raw_agch && frame>=m_agch_start_frame && frame-m_agch_start_frame<4 && tick%60'000<148*48)
	{
		auto const &sample=m_agch_samples[frame-m_agch_start_frame][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	auto const pch=nokia::radio::combined_ccch_burst(frame);
	if (m_raw_agch && pch && tick%60'000<148*48)
	{
		bool const addressed=m_page_pending && frame>=m_page_not_before &&
			(frame/51)%2==m_page_phase && frame%51>=m_page_offset && frame%51<m_page_offset+4;
		if (addressed && frame-*pch!=m_page_last_block)
		{
			m_page_last_block=frame-*pch;
			machine().logerror("npe3_bts_page_air: start_frame=%u t=%.9f raw_adc_only=1 physical_claim=0\n",
				unsigned(m_page_last_block),machine().time().as_double());
		}
		bool const frame_page=ioport("LAB_BTS")->read()&0x400;
		if (!addressed && frame_page && frame-*pch!=m_frame_page_block)
		{
			m_frame_page_block=frame-*pch;
			auto const octets=nokia_rf_lab::frame_paging(m_frame_page_block%nokia_rf_lab::hyperframe);
			auto const bursts=nokia_rf_lab::bcch_bits(octets,2);
			for (unsigned at=0;at<4;++at) m_frame_paging_samples[at]=nokia_rf_lab::gmsk_samples(bursts[at]);
			std::string packed;for (auto byte:octets) packed+=util::string_format("%02x",byte);
			machine().logerror("npe3_bts_frame_page: start_frame=%u octets=%s unmatched_tmsi=1 raw_adc_only=1 physical_claim=0\n",
				unsigned(m_frame_page_block),packed);
		}
		auto const &samples=addressed ? m_addressed_page_samples : frame_page ? m_frame_paging_samples : m_paging_samples;
		auto const &sample=samples[*pch][(tick%60'000)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_bcch && nokia_rf_lab::bcch_active(tick))
	{
		unsigned const burst=(tick/60'000)%51-2, position=(tick%60'000)/12;
		auto const &sample=m_full_broadcast ?
			m_broadcast_samples[gsm::mobility::bcch_system_information_index(tick/60'000)][burst][position] :
			m_bcch_samples[burst][position];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	if (m_sch && nokia_rf_lab::sch_active(tick))
	{
		u32 const frame = (tick / 60'000) % nokia_rf_lab::hyperframe;
		if (frame != m_sch_cache_frame)
		{
			m_sch_cache = nokia_rf_lab::sch_samples(frame, 42);
			m_sch_cache_frame = frame;
		}
		auto const &sample = m_sch_cache[(tick % 60'000) / 12];
		m_q_latch = sample[1];
		return u16(sample[0]);
	}
	if (m_tch.active() && frame>=m_tch_stop_frame) stop_traffic_channel();
	if (m_tch.active() && tick%60'000>=7'500 && tick%60'000<7'500+148*48)
	{
		// TS1 carries the dummy burst in its idle frame, as on any BCCH carrier.
		if (frame!=m_tch_samples_frame)
		{
			m_tch_samples_frame=frame;
			if (frame>=m_facch_t200) resend_facch("t200",frame);
			auto const bits=m_tch.downlink(frame);
			m_tch_samples=bits ? nokia_rf_lab::gmsk_samples(*bits) : m_dummy_samples;
		}
		auto const &sample=m_tch_samples[(tick%60'000-7'500)/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	unsigned const offset=tick%60'000%7'500;
	if (m_tuned_air && tick%60'000>=7'500 && offset<148*48)
	{
		auto const &sample=m_dummy_samples[offset/12];
		m_q_latch=sample[1];
		return u16(sample[0]);
	}
	unsigned const index = (tick / 12) % 16;
	bool const active = !m_burst || nokia_rf_lab::fcch_active(tick);
	m_q_latch = active ? q[index] : 0;
	return active ? u16(i[index]) : 0;
}

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

INPUT_PORTS_START( nse1hs )
	PORT_INCLUDE(noki5110)
	// The native PCM fixtures were derived while selector 0 and CTSI+0e
	// bit 1 were both low, which 25ab2c..25ab72 classifies as 10/Headset.
	// They keep that accessory path explicitly; it is not the board default.
	PORT_START("RESEARCH_ADC0")
	PORT_CONFNAME(0x03ff, 0, "Research CCONT ADC0 source")
	PORT_CONFSETTING(0, "Headset control (0)")
	PORT_CONFSETTING(0x03ff, "Unattached input (1023)")
	PORT_START("RESEARCH_EXT_STATUS")
	PORT_CONFNAME(0x07, 0, "Research MAD2 external status")
	PORT_CONFSETTING(0, "Headset control (0)")
	PORT_CONFSETTING(2, "Unattached input (2)")
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

void nokia_dct3_state::nsm3r6(machine_config &config)
{
	noki8210(config);
	// Reuse the recovered NSM-3 board contracts, not the runtime HLE peers.
	// ADC_5210 is the declared BLB-2 nominal-input model, not measured units.
	nokia_product_config native = m_product;
	native.ccont_board = ADC_5210;
	native.keypad_wiring.row_pin_shift = 1; // Own scanner 305998/table 33ee78.
	native.simi_controller = true;
	native.synthetic_sim_card = true;
	apply_product_config(native);
	// Acquired 8210 ROM6 program and separate data view, not a fitted-mask
	// promotion. Keep the ordinary product's own uploads and storage inputs.
	config.device_remove("dsp_hle");
	// NSM-3 System Module issue 1, 12/1999, p.25 specifies 13 MHz x 3.5.
	NOKIA_DSP_C54X(config, m_dsp_c54x, 45'500'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	m_dsp_c54x->set_overlay_end(0x2000);
	// Partial request/strobe/DONE/ack contract recovered from resident I/O.
	// This supplies no RF/sample data or MCU/DSP service verdict.
	m_dsp_c54x->set_port36_handshake(true);
	// Observed NSM-3 serial command state only; no RF lock or samples.
	auto &hagar = NOKIA_HAGAR(config, "hagar", 0);
	m_dsp_c54x->transfer_compare_cb().set(hagar, FUNC(nokia_hagar_device::compare_output_w));
	// Own uploaded code uses FB at 07e6. Enable the existing extended-control
	// grammar; only the acquired page-zero address map is supplied here.
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_extended_program(true);
	// Do not inherit ROM4's power-on INT0 enable. Zero is the generic CPU
	// reset-mask policy; fitted MAD2 reset state remains unmeasured.
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_power_on_imr(0);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
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
	// Own task-0 sleep arm 2a8ace stops the clock with control 0a and loops
	// to 2a8a76 without writing 05; only the normal-idle arm restores it.
	research.mad2_sleep_wake_enable = true;
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
	// Own sleep arm 2a02fe -> 2ac704(1) has no control-05 write (as NSB-3).
	research.mad2_sleep_wake_enable = true;
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
	// Own sleep arm 28940a -> 2951dc(1) has no control-05 write (as NSB-3).
	research.mad2_sleep_wake_enable = true;
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

void nokia_dct3_state::npe3r6(machine_config &config)
{
	noki6210(config);
	nokia_product_config native = m_product;
	// Own 4dc050..4dc058 ORs bit 1 after the bootstrap handshake. Retain
	// the independent release/execution controls across that read/modify/write.
	native.dsp_reset_wiring.readback_mask = 0x05;
	// Own 3d71f8 battery conversion: nominal input, not measured ADC units.
	native.ccont_board.channel_defaults[2] = 0x230;
	native.simi_controller = true;
	native.synthetic_sim_card = true;
	apply_product_config(native);
	config.device_remove("dsp_hle");
	// Compatibility trial at the staged fixture's 13 MHz policy. NPE-3's
	// physical DSP multiplier is not established by its 13 MHz RFIclk table.
	NOKIA_DSP_C54X(config, m_dsp_c54x, 13'000'000);
	configure_native_dsp_attachments(*m_dsp_c54x);
	m_dsp_c54x->set_overlay_end(0x2000);
	m_dsp_c54x->set_port36_handshake(true);
	auto &cpu = *subdevice<tms320c54x_device>("dsp_c54x:cpu");
	cpu.set_extended_program(true);
	cpu.set_power_on_imr(0);
	m_dsp_c54x->tone_update_cb().set(FUNC(nokia_dct3_state::dsp_tone_update_w));
}

void nokia_dct3_state::npe3ref(machine_config &config)
{
	npe3r6(config);
	// Controlled compatibility experiment with the acquired reference DSP's
	// documented rate. This does not identify the fitted NPE-3 DSP clock.
	m_dsp_c54x->set_clock(45'500'000);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_clock(45'500'000);
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
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->reset_execution_pipeline();
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_standard_swwsr(true);
}

void nse1_pipeline_fixture_state::nse1plg(machine_config &config)
{
	nse1pipe(config);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->set_standard_swwsr(false);
}

void dct3_pcm_fixture_state::nsm1pcm(machine_config &config)
{
	nsm1r4t(config);
	CLOCK(config, "external_pcm_frame", 8'000).signal_handler().set(FUNC(dct3_pcm_fixture_state::frame_w));
}

void dct3_pcm_fixture_state::nse1obs(machine_config &config)
{
	noki5110(config);
	subdevice<tms320c54x_device>("dsp_c54x:cpu")->instruction_observer_cb().set(FUNC(dct3_pcm_fixture_state::instruction_w));
	m_execution_demand_observer = true;
}

void dct3_pcm_fixture_state::nse1pcm(machine_config &config)
{
	nse1obs(config);
	CLOCK(config, "external_pcm_frame", 8'000).signal_handler().set(FUNC(dct3_pcm_fixture_state::frame_w));
}

void dct3_pcm_fixture_state::nse1bit(machine_config &config)
{
	nse1obs(config);
	CLOCK(config, "external_pcm_bit", 1'000'000).signal_handler().set(FUNC(dct3_pcm_fixture_state::bit_w));
}

void dct3_pcm_fixture_state::nse1pin(machine_config &config)
{
	nse1bit(config);
	subdevice<nokia_dsp_c54x_device>("dsp_c54x")->set_external_codec_clock(true);
	m_clocked_loopback = true;
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
