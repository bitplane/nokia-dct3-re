// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_SIM_CARD_H
#define MAME_NOKIA_NOKIA_SIM_CARD_H

#include "gsm_a3a8.h"
#include "gsm_subscriber.h"

class nokia_sim_card_device : public device_t, public device_nvram_interface
{
public:
	enum class authentication_profile : u8 { none, gsm_aes_example };
	enum class toolkit_profile : u8
	{
		none,
		display_text,
		display_text_get_inkey,
		display_text_get_inkey_get_input,
		display_text_get_inkey_get_input_setup_menu,
		interactive_menu_send_sms,
		interactive_menu_setup_call,
		interactive_menu_select_item,
		interactive_menu_event_list,
		interactive_menu_refresh,
		interactive_menu_polling,
		interactive_menu_play_tone,
		interactive_menu_call_dtmf,
		interactive_menu_launch_browser,
		interactive_menu_timer_management,
		interactive_menu_malformed_tlv
	};

	nokia_sim_card_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto response_cb() { return m_response_cb.bind(); }

	void set_cphs_aoc(bool enabled) { m_cphs_aoc = enabled; }
	void set_cached_location(bool enabled) { m_cached_location = enabled; }
	void set_toolkit_profile(toolkit_profile profile) { m_toolkit_profile = profile; }
	void set_forbidden_test_plmn(bool enabled)
	{
		m_forbidden_test_plmn = enabled;
	}
	void set_subscriber_profile(const gsm::subscriber::profile &profile)
	{
		m_subscriber = profile;
		m_authentication_profile =
				profile.authentication ==
						gsm::subscriber::authentication_algorithm::aes_example ?
					authentication_profile::gsm_aes_example :
					authentication_profile::none;
		m_ki = profile.ki;
	}
	void set_atr(const u8 *data, unsigned length);
	void activate();
	void deactivate();
	void activate_w(int state) { if (state) activate(); else deactivate(); }
	void rx_w(u8 data);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void nvram_default() override;
	virtual bool nvram_read(util::read_stream &file) override;
	virtual bool nvram_write(util::write_stream &file) override;

private:
	TIMER_CALLBACK_MEMBER(toolkit_command_ready);
	enum class file_structure : u8 { directory, transparent, linear_fixed, cyclic };
	enum class access_condition : u8 { always = 0, chv1 = 1, chv2 = 2, adm = 4, never = 15 };
	struct file_descriptor
	{
		u16 fid;
		u16 parent;
		u16 size;
		u8 record_length;
		file_structure structure;
		access_condition update_access;
		access_condition read_access = access_condition::always;
		access_condition read_condition() const
		{
			return read_access;
		}
		access_condition update_condition() const
		{
			return update_access;
		}
	};

	// The structure a command requires of the EF it is addressed to. Named
	// rather than tested inline so the commands that begin with this
	// lookup cannot disagree about the status a mismatch reports.
	enum class access_shape : u8 { transparent, record, cyclic };

	// Each of these queues the failure status itself and reports refusal, so a
	// caller only has to return. Keep them in the order the original commands
	// tested in: which status a malformed command receives depends on it.
	const file_descriptor *selected_file_for(access_shape shape);
	// GSM 09.91 5.1.1 option 1: one backing application, while
	// m_selected_df retains the directory identifier presented to the ME.
	static u16 canonical_df(u16 fid) { return fid == 0x7f21 ? 0x7f20 : fid; }
	bool write_permitted(const file_descriptor &file);
	bool read_permitted(const file_descriptor &file);
	u8 *writable_body(const file_descriptor &file);
	bool resolve_record(const file_descriptor &file, unsigned &record);
	bool chv_ready(unsigned index);
	// Clear the per-session command and selection state. Reached both by reset
	// and by card activation, so a field added here cannot be missed at one
	// site. Reset additionally clears the command header, which activation
	// deliberately leaves alone.
	void reset_session_state();
	bool nvram_read_record(util::read_stream &file);

	void emit_response(const u8 *data, unsigned length);
	void finish_header();
	void finish_body();
	void queue_status(u8 sw1, u8 sw2);
	void append_completion_status(u8 *response, unsigned &length);
	void queue_fcp(u16 fid, unsigned requested);
	void queue_pending_response(unsigned requested);
	void queue_read_binary(unsigned requested);
	void queue_read_record(unsigned requested);
	void update_binary();
	void update_record();
	void seek_record();
	void increase_record();
	void change_file_validity();
	bool file_valid(u16 fid) const;
	void run_gsm_algorithm();
	void accept_terminal_profile();
	void queue_proactive_command(unsigned requested);
	void accept_terminal_response();
	void accept_envelope();
	u8 proactive_command_length() const;
	void process_chv();
	void initialize_chv();
	bool chv_matches(unsigned index, const u8 *value) const;
	void chv_failure(unsigned index);
	void unblock_failure(unsigned index);
	static const file_descriptor *find_file(u16 fid);
	static bool is_directory(u16 fid);
	bool is_known_file(u16 fid) const;
	u8 ef_byte(u16 fid, unsigned offset) const;
	u8 *mutable_file_data(u16 fid);
	const u8 *mutable_file_data(u16 fid) const;
	bool access_allowed(const file_descriptor &file) const;
	bool chv1_authorized() const;
	bool condition_satisfied(access_condition condition) const;
	void initialize_sms_files();
	void initialize_fdn_files();
	void initialize_plmn_files();

	static constexpr unsigned sms_record_length = 176;
	static constexpr unsigned sms_record_count = 10;
	static constexpr unsigned smsp_record_length = 44;
	static constexpr unsigned smsp_record_count = 2;

	devcb_write8 m_response_cb;
	bool m_cphs_aoc = false;
	bool m_cached_location = false;
	toolkit_profile m_toolkit_profile = toolkit_profile::none;
	bool m_forbidden_test_plmn = false;
	authentication_profile m_authentication_profile =
			authentication_profile::none;
	gsm::a3a8::block m_ki{};
	gsm::subscriber::profile m_subscriber = gsm::subscriber::laboratory;
	bool m_trace = false;
	u8 m_atr[40] = { 0x3b, 0x10, 0x05 };
	u8 m_atr_len = 3;
	u8 m_tx[260] = { 0 };
	u16 m_tx_len = 0;
	u16 m_tx_expected = 0;
	bool m_pts_pending = true;
	u8 m_ins = 0;
	u8 m_p1 = 0;
	u8 m_p2 = 0;
	u8 m_p3 = 0;
	u16 m_selected_file = 0x3f00;
	u16 m_selected_df = 0x3f00;
	u16 m_record_pointer = 0;
	bool m_receiving_body = false;
	u8 m_pending_response[256] = { 0 };
	u16 m_pending_response_len = 0;
	bool m_fcp_pending = false;
	bool m_terminal_profile_received = false;
	bool m_proactive_pending = false;
	bool m_proactive_fetched = false;
	u8 m_proactive_command = 0;
	u8 m_proactive_command_number = 0;
	u8 m_menu_selection = 0;
	emu_timer *m_toolkit_timer = nullptr;
	u8 m_adn[50 * 32] = { 0 };
	static constexpr unsigned fdn_record_count = 10;
	u8 m_fdn[fdn_record_count * 32] = { 0 };
	bool m_fdn_present = false;
	bool m_adn_valid = true;
	bool m_adn_access_when_invalid = false;
	bool m_imsi_valid = true;
	bool m_loci_valid = true;
	bool m_fdn_session_invalidated = false;
	bool m_extended_storage = false;
	u8 m_loci[11] = { 0 };
	u8 m_kc[9] = { 0 };
	u8 m_bcch[16] = { 0 };
	u8 m_sms[sms_record_count * sms_record_length] = { 0 };
	u8 m_smsp[smsp_record_count * smsp_record_length] = { 0 };
	u8 m_acm[3] = { 0 };
	u8 m_acm_max[3] = { 0 };
	u8 m_puct[5] = { 0 };
	u8 m_preferred_plmn[24] = { 0 };
	u8 m_forbidden_plmn[12] = { 0 };
	u8 m_smss[2] = { 0xff, 0xff };
	u8 m_chv[2][8] = { { 0 }, { 0 } };
	u8 m_unblock_chv[2][8] = { { 0 }, { 0 } };
	u8 m_chv_attempts[2] = { 3, 3 };
	u8 m_unblock_attempts[2] = { 10, 10 };
	bool m_chv1_enabled = false;
	bool m_chv_verified[2] = { false, false };
};

DECLARE_DEVICE_TYPE(NOKIA_SIM_CARD, nokia_sim_card_device)

#endif // MAME_NOKIA_NOKIA_SIM_CARD_H
