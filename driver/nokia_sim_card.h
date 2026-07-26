// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_SIM_CARD_H
#define MAME_NOKIA_NOKIA_SIM_CARD_H

class nokia_sim_card_device : public device_t, public device_nvram_interface
{
public:
	nokia_sim_card_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto response_cb() { return m_response_cb.bind(); }

	void set_cphs_aoc(bool enabled) { m_cphs_aoc = enabled; }
	void set_cached_location(bool enabled) { m_cached_location = enabled; }
	void set_atr(const u8 *data, unsigned length);
	void activate();
	void rx_w(u8 data);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void nvram_default() override;
	virtual bool nvram_read(util::read_stream &file) override;
	virtual bool nvram_write(util::write_stream &file) override;

private:
	enum class file_structure : u8 { directory, transparent, linear_fixed, cyclic };
	struct file_descriptor
	{
		u16 fid;
		u16 parent;
		u16 size;
		u8 record_length;
		file_structure structure;
		bool mutable_data;
	};

	void emit_response(const u8 *data, unsigned length);
	void finish_header();
	void finish_body();
	void queue_status(u8 sw1, u8 sw2);
	void queue_fcp(u16 fid, unsigned requested);
	void queue_pending_response(unsigned requested);
	void queue_read_binary(unsigned requested);
	void queue_read_record(unsigned requested);
	void update_binary();
	void update_record();
	void increase_record();
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
	void initialize_sms_files();

	static constexpr unsigned sms_record_length = 176;
	static constexpr unsigned sms_record_count = 10;
	static constexpr unsigned smsp_record_length = 44;
	static constexpr unsigned smsp_record_count = 2;

	devcb_write8 m_response_cb;
	bool m_cphs_aoc = false;
	bool m_cached_location = false;
	bool m_trace = false;
	u8 m_atr[40] = { 0x3b, 0x10, 0x05 };
	u8 m_atr_len = 3;
	u8 m_tx[260] = { 0 };
	u16 m_tx_len = 0;
	u16 m_tx_expected = 0;
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
	u8 m_adn[50 * 32] = { 0 };
	u8 m_loci[11] = { 0 };
	u8 m_kc[9] = { 0 };
	u8 m_bcch[16] = { 0 };
	u8 m_sms[sms_record_count * sms_record_length] = { 0 };
	u8 m_smsp[smsp_record_count * smsp_record_length] = { 0 };
	u8 m_acm[3] = { 0 };
	u8 m_chv[2][8] = { { 0 }, { 0 } };
	u8 m_unblock_chv[2][8] = { { 0 }, { 0 } };
	u8 m_chv_attempts[2] = { 3, 3 };
	u8 m_unblock_attempts[2] = { 10, 10 };
	bool m_chv1_enabled = false;
	bool m_chv_verified[2] = { false, false };
};

DECLARE_DEVICE_TYPE(NOKIA_SIM_CARD, nokia_sim_card_device)

#endif // MAME_NOKIA_NOKIA_SIM_CARD_H
