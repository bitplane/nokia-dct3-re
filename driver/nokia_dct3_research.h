// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_DCT3_RESEARCH_H
#define MAME_NOKIA_NOKIA_DCT3_RESEARCH_H

#include "nokia_dct3_state.h"
#include "nokia_rf_lab_tx.h"
#include "nokia_rf_lab_tch.h"
#include "nokia_gsm_fr_codec.h"

namespace nokia_dct3 {

// Explicit CPU execution probe. The ordinary MCU uploads through DSPIF;
// The staged frontend and standard SWWSR model are explicit research choices,
// not a claim that the fitted Lead external-bus dialect has been identified.
class nse1_pipeline_fixture_state : public nokia_dct3_state
{
public:
	using nokia_dct3_state::nokia_dct3_state;
	void nse1pipe(machine_config &config);
	void nse1plg(machine_config &config);
};

// Explicit laboratory attachment, not recovered COBBA clock-enable wiring.
// Separate machines leave ordinary product execution unchanged.
class dct3_pcm_fixture_state : public nokia_dct3_state
{
public:
	using nokia_dct3_state::nokia_dct3_state;
	void nsm1pcm(machine_config &config);
	void nse1pcm(machine_config &config);
	void nse1obs(machine_config &config);
	void nse1bit(machine_config &config);
	void nse1pin(machine_config &config);
protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;
	virtual void device_stop() override;
private:
	u16 m_wire_word = 0;
	u8 m_wire_bits = 0;
	bool m_clocked_loopback = false;
	bool m_execution_demand_observer = false;
	// Diagnostic sequence, intentionally not rewound by architectural save/load.
	unsigned m_demand_entries = 0;
	u64 m_clocked_echo_words = 0;
	void instruction_w(offs_t address, u16 word);
	void bit_w(int state);
	void frame_w(int state);
};

// ADC-pin experiment only: no over-the-air source or camping qualification.
class dct3_rf_adc_fixture_state : public nokia_dct3_state
{
public:
	using nokia_dct3_state::nokia_dct3_state;
	void nsm3adc(machine_config &config);
	void nsm3ad0(machine_config &config);
	void nsm3afb(machine_config &config);
	void nsm3afs(machine_config &config);
	void npe3afs(machine_config &config);
	void npe3ad0(machine_config &config);
	void nse1ad0(machine_config &config);
	void nse1adc(machine_config &config);
	void nse1afs(machine_config &config);
	void nse1cp9(machine_config &config);
	void nse1sp9(machine_config &config);
	void nse1tp5(machine_config &config);
	void nse1ap5(machine_config &config);
	void nse2ap5(machine_config &config);
	void nsk1ap5(machine_config &config);
	void nsk3ap5(machine_config &config);
	void nsb3ap5(machine_config &config);
	void npe3cp9(machine_config &config);
	void npe3zp9(machine_config &config);
	void npe3bp9(machine_config &config);
	void npe3sp9(machine_config &config);
	void npe3tp5(machine_config &config);
	void npe3ap5(machine_config &config);
	DECLARE_INPUT_CHANGED_MEMBER(bts_page_w);
protected:
	void machine_start() override;
	void machine_reset() override;
private:
	void configure_adc_lab(machine_config &config);
	enum class lab_band { gsm900, dcs1800, pcs1900 };
	void configure_nse1_lab_bts(machine_config &config, lab_band band = lab_band::gsm900);
	void control_w(offs_t port, u16 data);
	void transmit_word_w(u16 data);
	void rf_reset_w(int released);
	u16 sample_r(offs_t component);
	void fault_observe_w(int state);
	TIMER_CALLBACK_MEMBER(candidate_receive_start);
	TIMER_CALLBACK_MEMBER(candidate_transmit_start);
	void arm_transmit_candidate();
	void receive_normal_uplink(std::array<u16,15> const &words,u64 frame);
	void start_traffic_channel(u64 frame);
	void receive_traffic_uplink(std::array<u16,15> const &words,u64 frame);
	void receive_facch(gsm::tch_f::packed_control_block const &block,u64 frame);
	void send_facch(char const *kind,gsm::tch_f::packed_control_block const &lapdm,u64 frame);
	struct facch_information { u8 sequence; char const *kind; std::vector<u8> layer3; };
	void send_facch_information(facch_information const &information,u64 frame);
	void acknowledge_facch(u8 received);
	void resend_facch(char const *cause,u64 frame);
	emu_timer *m_candidate_timer=nullptr;
	emu_timer *m_transmit_timer=nullptr;
	bool m_p05_transmit=false, m_transmit_compare_written=false;
	bool m_raw_agch=false;
	u64 m_agch_start_frame=~u64(0);
	std::array<u8,23> m_agch_octets{};
	std::array<std::array<std::array<s16,2>,148*4>,4> m_agch_samples{};
	u64 m_normal_start_frame=~u64(0),m_ua_start_frame=~u64(0);
	unsigned m_normal_count=0;
	std::array<std::array<u8,114>,4> m_normal_bits{};
	std::array<u8,23> m_contention_sabm{};
	bool m_contention_active=false;
	std::array<std::array<std::array<s16,2>,148*4>,4> m_ua_samples{};
	u64 m_lu_start_frame=~u64(0);
	std::array<std::array<std::array<s16,2>,148*4>,4> m_lu_samples{};
	u64 m_setup_start_frame=~u64(0);
	u64 m_service_accept_start_frame=~u64(0);
	u64 m_call_proceeding_start_frame=~u64(0);
	std::array<std::array<std::array<s16,2>,148*4>,4> m_service_accept_samples{};
	std::array<std::array<std::array<s16,2>,148*4>,4> m_setup_samples{};
	u64 m_assignment_start_frame=~u64(0);
	std::array<std::array<std::array<s16,2>,148*4>,4> m_assignment_samples{};
	u64 m_call_rr_start_frame=~u64(0);
	u8 m_call_rr_sequence=0xff;
	std::array<std::array<std::array<s16,2>,148*4>,4> m_call_rr_samples{};
	u64 m_call_release_start_frame=~u64(0);
	bool m_call_channel_release_sent=false;
	std::array<std::array<std::array<std::array<s16,2>,148*4>,4>,2> m_sacch_samples{};
	bool m_lu_acknowledged=false;
	// TCH/F on TS1 after the Assignment Command, with the network side of
	// its FACCH LAPDm link (V(S), V(R)). Not saved: a diagnostic lab channel.
	std::array<gsm::tch_f::packed_control_block,2> m_sacch_blocks{};
	nokia_rf_lab::traffic_channel m_tch;
	u8 m_facch_send=0,m_facch_receive=0;
	std::vector<facch_information> m_facch_outstanding;
	u64 m_facch_t200=~u64(0);
	u64 m_tch_stop_frame=~u64(0),m_tch_samples_frame=~u64(0);
	std::array<std::array<s16,2>,148*4> m_tch_samples{};
	// Downlink speech: a continuous 1 kHz tone, GSM 06.10-encoded per block.
	nokia_gsm_fr_codec m_tch_codec;
	unsigned m_tch_tone_phase=0,m_tch_speech_frames=0;
	unsigned m_tch_speech=0,m_tch_bad_speech=0,m_tch_unreadable=0;
	u64 m_release_start_frame=~u64(0);
	std::array<std::array<std::array<s16,2>,148*4>,4> m_release_samples{};
	// Default fill and separate diagnostic pages for unassigned TMSIs.
	std::array<std::array<std::array<s16,2>,148*4>,4> m_paging_samples{};
	std::array<std::array<std::array<s16,2>,148*4>,4> m_frame_paging_samples{};
	u64 m_frame_page_block=~u64(0);
	// External BTS state, populated only from an actual decoded uplink IMSI.
	std::array<u8,8> m_page_identity{};
	bool m_page_identity_valid=false,m_page_pending=false;
	unsigned m_page_phase=0,m_page_offset=0;
	u64 m_page_not_before=~u64(0),m_page_last_block=~u64(0);
	std::array<std::array<std::array<s16,2>,148*4>,4> m_addressed_page_samples{};
	bool m_compare_start=false, m_candidate_active=false;
	// NSE-1 UHF synthesizer word (tag 3, M = N*64 + A at 200 kHz) selects
	// which channel the lab cell is heard on; -1 is untuned. ROM4 programs
	// the next window's channel during the current capture, so the lab
	// latches the programmed channel when a capture window opens.
	bool m_tuned_air=false;
	lab_band m_band=lab_band::gsm900;
	u16 m_lab_arfcn=35;
	u16 m_synth_low=0;
	s32 m_rx_arfcn=-1, m_capture_arfcn=-1;
	std::array<std::array<s16,2>,148*4> m_dummy_samples{};
	u16 m_candidate_target=0;
	bool m_tone = true;
	bool m_burst = false;
	bool m_sch = false;
	bool m_bcch = false;
	bool m_full_broadcast = false;
	std::array<std::array<std::array<s16, 2>, 148 * 4>, 4> m_bcch_samples{};
	std::array<std::array<std::array<std::array<s16, 2>, 148 * 4>, 4>, 4> m_broadcast_samples{};
	// Pure FN-derived memoization, not independent emulated state. Never
	// restore its key separately from its table; a time rewind misses/rebuilds it.
	u32 m_sch_cache_frame = ~u32(0);
	std::array<std::array<s16, 2>, 148 * 4> m_sch_cache{};
	s16 m_q_latch = 0;
	u16 m_adc_receive_control = 0, m_adc_mfi_control = 0;
	std::array<u16,4> m_lab_tx_compares{};
	nokia_rf_lab_tx m_lab_tx;
	// Diagnostic counters are not rewound; they never select firmware work.
	unsigned m_observed_tx_windows=0, m_transmit_releases=0;
};

INPUT_PORTS_EXTERN(nmp5stage);
INPUT_PORTS_EXTERN(nsb1r4t);
INPUT_PORTS_EXTERN(nsb3r4t);
INPUT_PORTS_EXTERN(nse1hs);
INPUT_PORTS_EXTERN(nse2r4v);
INPUT_PORTS_EXTERN(nsk1r4t);
INPUT_PORTS_EXTERN(nsk3r4t);
INPUT_PORTS_EXTERN(nsm1r4t);

} // namespace nokia_dct3

#endif // MAME_NOKIA_NOKIA_DCT3_RESEARCH_H
