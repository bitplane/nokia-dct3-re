// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_sim_card.h"

#define LOG_SIM (1U << 0)
#define VERBOSE (LOG_SIM)
#include "logmacro.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(NOKIA_SIM_CARD, nokia_sim_card_device, "nokia_sim_card", "Nokia DCT3 SIM card")

nokia_sim_card_device::nokia_sim_card_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_SIM_CARD, tag, owner, clock),
	device_nvram_interface(mconfig, *this),
	m_response_cb(*this)
{
}

void nokia_sim_card_device::device_start()
{
	m_trace = machine().options().verbose();
	save_item(NAME(m_cphs_aoc));
	save_item(NAME(m_cached_location));
	save_item(NAME(m_atr));
	save_item(NAME(m_atr_len));
	save_item(NAME(m_tx));
	save_item(NAME(m_tx_len));
	save_item(NAME(m_tx_expected));
	save_item(NAME(m_ins));
	save_item(NAME(m_p1));
	save_item(NAME(m_p2));
	save_item(NAME(m_p3));
	save_item(NAME(m_selected_file));
	save_item(NAME(m_selected_df));
	save_item(NAME(m_record_pointer));
	save_item(NAME(m_receiving_body));
	save_item(NAME(m_pending_response));
	save_item(NAME(m_pending_response_len));
	save_item(NAME(m_fcp_pending));
	save_item(NAME(m_adn));
	save_item(NAME(m_loci));
	save_item(NAME(m_kc));
	save_item(NAME(m_bcch));
	save_item(NAME(m_sms));
	save_item(NAME(m_smsp));
	save_item(NAME(m_acm));
	save_item(NAME(m_chv));
	save_item(NAME(m_unblock_chv));
	save_item(NAME(m_chv_attempts));
	save_item(NAME(m_unblock_attempts));
	save_item(NAME(m_chv1_enabled));
	save_item(NAME(m_chv_verified));
}

void nokia_sim_card_device::reset_session_state()
{
	m_tx_len = m_tx_expected = 0;
	m_selected_file = 0x3f00;
	m_selected_df = 0x3f00;
	m_record_pointer = 0;
	m_receiving_body = false;
	m_pending_response_len = 0;
	m_fcp_pending = false;
	m_chv_verified[0] = m_chv_verified[1] = false;
}

void nokia_sim_card_device::device_reset()
{
	reset_session_state();
	// Reset additionally discards the command header. Activation does not: a
	// card reactivated mid-command leaves the header for the transport to
	// finish reading, so this stays outside the shared helper.
	m_ins = 0;
	m_p1 = m_p2 = m_p3 = 0;
}

void nokia_sim_card_device::nvram_default()
{
	std::fill(std::begin(m_adn), std::end(m_adn), 0xff);
	std::fill(std::begin(m_kc), std::end(m_kc), 0xff);
	m_kc[8] = 0x07; // no valid ciphering key sequence
	std::fill(std::begin(m_loci), std::end(m_loci), 0xff);
	m_loci[10] = 0x01; // location not updated
	std::fill(std::begin(m_bcch), std::end(m_bcch), 0xff);
	initialize_sms_files();
	initialize_chv();
	std::fill(std::begin(m_acm), std::end(m_acm), 0x00);
	if (m_cached_location)
	{
		const u8 loci[] = { 0xff, 0xff, 0xff, 0xff, 0x00, 0xf1, 0x10, 0x00, 0x01, 0x00, 0x01 };
		std::copy(std::begin(loci), std::end(loci), std::begin(m_loci));
		std::fill(std::begin(m_bcch), std::end(m_bcch), 0x00);
		m_bcch[15] = 0x01;
	}
}

bool nokia_sim_card_device::nvram_read(util::read_stream &file)
{
	auto const [err, actual] = util::read(file, m_adn, sizeof(m_adn));
	if (err || actual != sizeof(m_adn))
		return false;
	auto const [loci_err, loci_actual] = util::read(file, m_loci, sizeof(m_loci));
	auto const [kc_err, kc_actual] = util::read(file, m_kc, sizeof(m_kc));
	auto const [bcch_err, bcch_actual] = util::read(file, m_bcch, sizeof(m_bcch));
	if (loci_err || loci_actual != sizeof(m_loci) || kc_err || kc_actual != sizeof(m_kc) ||
			bcch_err || bcch_actual != sizeof(m_bcch))
		return false;
	auto const [sms_err, sms_actual] = util::read(file, m_sms, sizeof(m_sms));
	if (!sms_err && sms_actual == 0)
	{
		// Preserve compatibility with card NVRAM created before SMS storage was
		// admitted; initialize only the newly appended files.
		initialize_sms_files();
		initialize_chv();
		std::fill(std::begin(m_acm), std::end(m_acm), 0x00);
		return true;
	}
	if (sms_err || sms_actual != sizeof(m_sms))
		return false;
	auto const [smsp_err, smsp_actual] = util::read(
			file, m_smsp, sizeof(m_smsp));
	if (smsp_err || smsp_actual != sizeof(m_smsp))
		return false;
	auto const [chv_err, chv_actual] = util::read(file, m_chv, sizeof(m_chv));
	if (!chv_err && chv_actual == 0)
	{
		// Preserve cards created before persistent CHV state was admitted.
		initialize_chv();
		std::fill(std::begin(m_acm), std::end(m_acm), 0x00);
		return true;
	}
	if (chv_err || chv_actual != sizeof(m_chv))
		return false;
	auto const [unblock_err, unblock_actual] = util::read(
			file, m_unblock_chv, sizeof(m_unblock_chv));
	auto const [attempts_err, attempts_actual] = util::read(
			file, m_chv_attempts, sizeof(m_chv_attempts));
	auto const [unblock_attempts_err, unblock_attempts_actual] = util::read(
			file, m_unblock_attempts, sizeof(m_unblock_attempts));
	u8 enabled;
	auto const [enabled_err, enabled_actual] = util::read(file, &enabled, 1);
	if (unblock_err || unblock_actual != sizeof(m_unblock_chv) ||
			attempts_err || attempts_actual != sizeof(m_chv_attempts) ||
			unblock_attempts_err || unblock_attempts_actual != sizeof(m_unblock_attempts) ||
			enabled_err || enabled_actual != 1)
		return false;
	m_chv1_enabled = enabled != 0;
	auto const [acm_err, acm_actual] = util::read(file, m_acm, sizeof(m_acm));
	if (!acm_err && acm_actual == 0)
	{
		// Preserve cards created before persistent advice-of-charge state was
		// admitted; initialize only the append-only EF_ACM payload.
		std::fill(std::begin(m_acm), std::end(m_acm), 0x00);
		return true;
	}
	if (acm_err || acm_actual != sizeof(m_acm))
		return false;
	u8 trailing;
	auto const [trailing_err, trailing_actual] = util::read(file, &trailing, 1);
	return !trailing_err && trailing_actual == 0;
}

bool nokia_sim_card_device::nvram_write(util::write_stream &file)
{
	auto const [err, actual] = util::write(file, m_adn, sizeof(m_adn));
	if (err || actual != sizeof(m_adn))
		return false;
	auto const [loci_err, loci_actual] = util::write(file, m_loci, sizeof(m_loci));
	auto const [kc_err, kc_actual] = util::write(file, m_kc, sizeof(m_kc));
	auto const [bcch_err, bcch_actual] = util::write(file, m_bcch, sizeof(m_bcch));
	auto const [sms_err, sms_actual] = util::write(file, m_sms, sizeof(m_sms));
	auto const [smsp_err, smsp_actual] = util::write(
			file, m_smsp, sizeof(m_smsp));
	auto const [chv_err, chv_actual] = util::write(file, m_chv, sizeof(m_chv));
	auto const [unblock_err, unblock_actual] = util::write(
			file, m_unblock_chv, sizeof(m_unblock_chv));
	auto const [attempts_err, attempts_actual] = util::write(
			file, m_chv_attempts, sizeof(m_chv_attempts));
	auto const [unblock_attempts_err, unblock_attempts_actual] = util::write(
			file, m_unblock_attempts, sizeof(m_unblock_attempts));
	const u8 enabled = m_chv1_enabled ? 1 : 0;
	auto const [enabled_err, enabled_actual] = util::write(file, &enabled, 1);
	auto const [acm_err, acm_actual] = util::write(file, m_acm, sizeof(m_acm));
	return !loci_err && loci_actual == sizeof(m_loci) && !kc_err && kc_actual == sizeof(m_kc) &&
			!bcch_err && bcch_actual == sizeof(m_bcch) &&
			!sms_err && sms_actual == sizeof(m_sms) &&
			!smsp_err && smsp_actual == sizeof(m_smsp) &&
			!chv_err && chv_actual == sizeof(m_chv) &&
			!unblock_err && unblock_actual == sizeof(m_unblock_chv) &&
			!attempts_err && attempts_actual == sizeof(m_chv_attempts) &&
			!unblock_attempts_err && unblock_attempts_actual == sizeof(m_unblock_attempts) &&
			!enabled_err && enabled_actual == 1 &&
			!acm_err && acm_actual == sizeof(m_acm);
}

void nokia_sim_card_device::initialize_sms_files()
{
	std::fill(std::begin(m_sms), std::end(m_sms), 0xff);
	for (unsigned record = 0; record < sms_record_count; ++record)
		m_sms[record * sms_record_length] = 0x00;

	std::fill(std::begin(m_smsp), std::end(m_smsp), 0xff);
	// GSM 11.11 EF_SMSP: only the service-centre address is present in record
	// one. Parameter indicators mark destination/PID/DCS/validity absent.
	m_smsp[16] = 0xfd;
	const u8 service_centre[] = {
		0x06, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09
	};
	std::copy(std::begin(service_centre), std::end(service_centre),
			std::begin(m_smsp) + 29);
}

void nokia_sim_card_device::initialize_chv()
{
	static constexpr u8 chv1[8] = { '1', '2', '3', '4', 0xff, 0xff, 0xff, 0xff };
	static constexpr u8 chv2[8] = { '5', '6', '7', '8', 0xff, 0xff, 0xff, 0xff };
	static constexpr u8 unblock1[8] = { '1', '2', '3', '4', '5', '6', '7', '8' };
	static constexpr u8 unblock2[8] = { '8', '7', '6', '5', '4', '3', '2', '1' };
	std::copy(std::begin(chv1), std::end(chv1), std::begin(m_chv[0]));
	std::copy(std::begin(chv2), std::end(chv2), std::begin(m_chv[1]));
	std::copy(std::begin(unblock1), std::end(unblock1), std::begin(m_unblock_chv[0]));
	std::copy(std::begin(unblock2), std::end(unblock2), std::begin(m_unblock_chv[1]));
	m_chv_attempts[0] = m_chv_attempts[1] = 3;
	m_unblock_attempts[0] = m_unblock_attempts[1] = 10;
	m_chv1_enabled = false;
	m_chv_verified[0] = m_chv_verified[1] = false;
}

void nokia_sim_card_device::set_atr(const u8 *data, unsigned length)
{
	m_atr_len = std::min<unsigned>(length, std::size(m_atr));
	std::copy_n(data, m_atr_len, m_atr);
}

void nokia_sim_card_device::activate()
{
	reset_session_state();
	emit_response(m_atr, m_atr_len);
}

const nokia_sim_card_device::file_descriptor *
nokia_sim_card_device::selected_file_for(access_shape shape)
{
	const file_descriptor *const file = find_file(m_selected_file);
	const bool matches = file && (
			shape == access_shape::transparent ?
				file->structure == file_structure::transparent :
			shape == access_shape::cyclic ?
				file->structure == file_structure::cyclic :
				(file->structure == file_structure::linear_fixed ||
					file->structure == file_structure::cyclic));
	if (!matches)
	{
		queue_status(0x94, 0x08);
		return nullptr;
	}
	return file;
}

bool nokia_sim_card_device::write_permitted(const file_descriptor &file)
{
	if (access_allowed(file))
		return true;
	queue_status(0x98, 0x04);
	return false;
}

u8 *nokia_sim_card_device::writable_body(const file_descriptor &file)
{
	u8 *const data = mutable_file_data(file.fid);
	if (!data)
		queue_status(0x98, 0x04);
	return data;
}

bool nokia_sim_card_device::resolve_record(
		const file_descriptor &file, unsigned &record)
{
	// GSM 11.11 record addressing: absolute, next, previous, or the current
	// pointer. Next and previous wrap, which is what makes cyclic files
	// traversable without the handset tracking the record count itself.
	const unsigned records = file.size / file.record_length;
	const u8 mode = m_p2 & 0x07;
	record = m_record_pointer;
	if (mode == 0x04)
		record = m_p1;
	else if (mode == 0x02)
		record = record == records ? 1 : record + 1;
	else if (mode == 0x03)
		record = record <= 1 ? records : record - 1;
	else if (mode != 0x00)
	{
		queue_status(0x6b, 0x00);
		return false;
	}
	if (record == 0 || record > records)
	{
		queue_status(0x94, 0x02);
		return false;
	}
	// The caller advances m_record_pointer itself: a read commits it at once,
	// while a write commits only once the body has proved writable.
	return true;
}

bool nokia_sim_card_device::chv_ready(unsigned index)
{
	// A disabled CHV1 is reported as a condition-of-use failure rather than as
	// a mismatch, so it must be tested before the comparison and must not
	// consume a retry.
	if (index == 0 && !m_chv1_enabled)
	{
		queue_status(0x98, 0x08);
		return false;
	}
	if (!chv_matches(index, m_tx))
	{
		chv_failure(index);
		return false;
	}
	return true;
}

void nokia_sim_card_device::emit_response(const u8 *data, unsigned length)
{
	for (unsigned i = 0; i < length; i++)
		m_response_cb(data[i]);
}

void nokia_sim_card_device::rx_w(u8 data)
{

	if (m_tx_len < std::size(m_tx))
		m_tx[m_tx_len++] = data;

	if (m_receiving_body)
	{
		if (m_tx_len >= m_tx_expected)
			finish_body();
		return;
	}

	if (m_tx_len == 1)
		m_tx_expected = data == 0xff ? 3 : 5;
	if (m_tx_expected && m_tx_len >= m_tx_expected)
		finish_header();
}

void nokia_sim_card_device::finish_header()
{
	if (m_tx[0] == 0xff)
	{
		emit_response(m_tx, m_tx_len);
		m_tx_len = m_tx_expected = 0;
		return;
	}

	m_ins = m_tx[1];
	m_p1 = m_tx[2];
	m_p2 = m_tx[3];
	m_p3 = m_tx[4];
	if (m_ins != 0xc0)
	{
		m_pending_response_len = 0;
		m_fcp_pending = false;
	}
	if (m_trace)
		LOGMASKED(LOG_SIM, "sim_device: header cla=%02x ins=%02x p1=%02x p2=%02x p3=%02x selected=%04x t=%.8f\n",
				m_tx[0], m_ins, m_p1, m_p2, m_p3, m_selected_file,
				machine().time().as_double());
	if (m_ins == 0x20 || m_ins == 0x24 || m_ins == 0x26 ||
			m_ins == 0x28 || m_ins == 0x2c || m_ins == 0x88 || m_ins == 0xa4 ||
			m_ins == 0x32 || m_ins == 0xd6 || m_ins == 0xdc)
	{
		const u8 procedure = m_ins;
		m_tx_len = 0;
		m_tx_expected = m_p3 ? m_p3 : 256;
		m_receiving_body = true;
		emit_response(&procedure, 1);
		return;
	}

	if (m_ins == 0xc0 && m_pending_response_len != 0)
		queue_pending_response(m_p3 ? m_p3 : 256);
	else if (m_ins == 0xc0 && m_fcp_pending)
	{
		queue_fcp(m_selected_file, m_p3 ? m_p3 : 256);
		m_fcp_pending = false;
	}
	else if (m_ins == 0xf2)
		queue_fcp(m_selected_file, m_p3 ? m_p3 : 256);
	else if (m_ins == 0xc0)
		queue_status(0x94, 0x00);
	else if (m_ins == 0xb0)
		queue_read_binary(m_p3 ? m_p3 : 256);
	else if (m_ins == 0xb2)
		queue_read_record(m_p3 ? m_p3 : 256);
	else
		queue_status(0x6d, 0x00);
	m_tx_len = m_tx_expected = 0;
}

void nokia_sim_card_device::finish_body()
{
	const u16 requested_file = m_tx_len >= 2 ? u16(m_tx[0] << 8) | m_tx[1] : 0;
	const file_descriptor *requested = find_file(requested_file);
	const bool select_ok = m_ins != 0xa4 || (m_tx_len == 2 && is_known_file(requested_file) &&
		(requested_file == 0x3f00 || requested->parent == m_selected_df ||
		 is_directory(requested_file)));
	if (m_ins == 0xa4 && select_ok)
	{
		m_selected_file = requested_file;
		if (is_directory(requested_file))
			m_selected_df = requested_file;
		m_record_pointer = 0;
	}
	if (m_trace)
	{
		if (m_ins == 0xa4)
			LOGMASKED(LOG_SIM, "sim_device: body ins=%02x length=%u requested=%04x selected=%04x result=%s t=%.8f\n",
					m_ins, m_tx_len, requested_file, m_selected_file, select_ok ? "ok" : "not-found",
					machine().time().as_double());
		else
			LOGMASKED(LOG_SIM, "sim_device: body ins=%02x length=%u selected=%04x t=%.8f\n",
					m_ins, m_tx_len, m_selected_file, machine().time().as_double());
	}
	if (m_ins == 0xdc)
	{
		update_record();
		m_tx_len = m_tx_expected = 0;
		m_receiving_body = false;
		return;
	}
	if (m_ins == 0x32)
	{
		increase_record();
		m_tx_len = m_tx_expected = 0;
		m_receiving_body = false;
		return;
	}
	if (m_ins == 0x88)
	{
		run_gsm_algorithm();
		m_tx_len = m_tx_expected = 0;
		m_receiving_body = false;
		return;
	}
	if (m_ins == 0x20 || m_ins == 0x24 || m_ins == 0x26 ||
			m_ins == 0x28 || m_ins == 0x2c)
	{
		process_chv();
		m_tx_len = m_tx_expected = 0;
		m_receiving_body = false;
		return;
	}
	if (m_ins == 0xd6)
	{
		update_binary();
		m_tx_len = m_tx_expected = 0;
		m_receiving_body = false;
		return;
	}
	m_tx_len = m_tx_expected = 0;
	m_receiving_body = false;
	if (m_ins == 0xa4)
	{
		if (!select_ok)
			queue_status(0x94, 0x04); // GSM 11.11: file ID not found
		else
		{
			const bool df = is_directory(m_selected_file);
			m_fcp_pending = true;
			queue_status(0x9f, df ? 22 : 15);
		}
	}
	else
		queue_status(0x90, 0x00);
}

void nokia_sim_card_device::run_gsm_algorithm()
{
	// GSM 11.11 9.2.11: P1/P2 are zero and the command body is the 16-byte
	// RAND.  A successful card returns SRES || Kc through GET RESPONSE.
	if (m_p1 != 0 || m_p2 != 0 || m_tx_len != 16 ||
			m_authentication_profile == authentication_profile::none)
	{
		queue_status(0x6d, 0x00);
		return;
	}

	gsm::a3a8::block rand;
	std::copy_n(m_tx, rand.size(), rand.begin());
	const gsm::a3a8::result authentication =
			gsm::a3a8::aes_example(m_ki, rand);
	std::copy(authentication.sres.begin(), authentication.sres.end(),
			m_pending_response);
	std::copy(authentication.kc.begin(), authentication.kc.end(),
			m_pending_response + authentication.sres.size());
	m_pending_response_len =
			authentication.sres.size() + authentication.kc.size();
	queue_status(0x9f, m_pending_response_len);
}

bool nokia_sim_card_device::chv_matches(unsigned index, const u8 *value) const
{
	return std::equal(std::begin(m_chv[index]), std::end(m_chv[index]), value);
}

void nokia_sim_card_device::chv_failure(unsigned index)
{
	m_chv_verified[index] = false;
	if (m_chv_attempts[index] != 0)
		--m_chv_attempts[index];
	queue_status(0x98, m_chv_attempts[index] != 0 ? 0x04 : 0x40);
}

void nokia_sim_card_device::unblock_failure(unsigned index)
{
	if (m_unblock_attempts[index] != 0)
		--m_unblock_attempts[index];
	queue_status(0x98, m_unblock_attempts[index] != 0 ? 0x04 : 0x40);
}

void nokia_sim_card_device::process_chv()
{
	if (m_p1 != 0)
	{
		queue_status(0x6b, 0x00);
		return;
	}

	const bool unblock = m_ins == 0x2c;
	unsigned index;
	if (unblock)
	{
		if (m_p2 != 0x00 && m_p2 != 0x02)
		{
			queue_status(0x6b, 0x00);
			return;
		}
		index = m_p2 == 0x00 ? 0 : 1;
	}
	else
	{
		if (m_p2 != 0x01 && m_p2 != 0x02)
		{
			queue_status(0x6b, 0x00);
			return;
		}
		index = m_p2 - 1;
	}

	const unsigned expected = (m_ins == 0x24 || unblock) ? 16 : 8;
	if (m_p3 != expected || m_tx_len != expected)
	{
		queue_status(0x67, 0x00);
		return;
	}

	if (unblock)
	{
		if (m_unblock_attempts[index] == 0)
		{
			queue_status(0x98, 0x40);
			return;
		}
		if (!std::equal(std::begin(m_unblock_chv[index]),
				std::end(m_unblock_chv[index]), m_tx))
		{
			unblock_failure(index);
			return;
		}
		std::copy_n(m_tx + 8, 8, std::begin(m_chv[index]));
		m_unblock_attempts[index] = 10;
		m_chv_attempts[index] = 3;
		m_chv_verified[index] = true;
		if (index == 0)
			m_chv1_enabled = true;
		queue_status(0x90, 0x00);
		return;
	}

	if (m_chv_attempts[index] == 0)
	{
		queue_status(0x98, 0x40);
		return;
	}

	if (m_ins == 0x20)
	{
		if (!chv_ready(index))
			return;
		m_chv_attempts[index] = 3;
		m_chv_verified[index] = true;
		queue_status(0x90, 0x00);
		return;
	}

	if (m_ins == 0x24)
	{
		if (!chv_ready(index))
			return;
		std::copy_n(m_tx + 8, 8, std::begin(m_chv[index]));
		m_chv_attempts[index] = 3;
		m_chv_verified[index] = true;
		queue_status(0x90, 0x00);
		return;
	}

	// ENABLE/DISABLE apply only to CHV1.
	if (index != 0)
	{
		queue_status(0x6b, 0x00);
		return;
	}
	const bool enabling = m_ins == 0x28;
	if (enabling == m_chv1_enabled)
	{
		queue_status(0x98, 0x08);
		return;
	}
	if (!chv_matches(0, m_tx))
	{
		chv_failure(0);
		return;
	}
	m_chv_attempts[0] = 3;
	m_chv1_enabled = enabling;
	m_chv_verified[0] = !enabling;
	queue_status(0x90, 0x00);
}

void nokia_sim_card_device::queue_status(u8 sw1, u8 sw2)
{
	const u8 response[] = { sw1, sw2 };
	emit_response(response, std::size(response));
}

void nokia_sim_card_device::queue_fcp(u16 fid, unsigned requested)
{
	u8 response[26] = { 0 };
	unsigned n = 0;
	response[n++] = m_ins;
	const bool df = is_directory(fid);
	if (requested != 0)
	{
		if (df || m_ins == 0xf2)
		{
			const u16 response_fid = m_ins == 0xf2 ? m_selected_df : fid;
			u8 fcp[22] = { 0 };
			fcp[4] = response_fid >> 8; fcp[5] = response_fid;
			fcp[6] = response_fid == 0x3f00 ? 0x01 : 0x02;
			// GSM 11.11 directory status, bytes 13 onward: length of the
			// GSM-specific data, file characteristics, child counts, code
			// count, RFU, then CHV1/PUK1/CHV2/PUK2 status. The firmware
			// requests the 22-byte prefix and parses these positions directly.
			fcp[12] = 0x15;
			fcp[13] = 0x01 | (m_chv1_enabled ? 0x00 : 0x80);
			fcp[14] = response_fid == 0x3f00 ? 0x02 : 0x00;
			fcp[15] = response_fid == 0x3f00 ? 0x02 : 0x16;
			fcp[16] = 0x03;
			fcp[18] = 0x80 | m_chv_attempts[0];
			fcp[19] = 0x80 | m_unblock_attempts[0];
			fcp[20] = 0x80 | m_chv_attempts[1];
			fcp[21] = 0x80 | m_unblock_attempts[1];
			const unsigned count = std::min<unsigned>(requested, std::size(fcp));
			std::copy_n(fcp, count, response + n); n += count;
		}
		else
		{
			const file_descriptor *file = find_file(fid);
			if (!file)
			{
				queue_status(0x94, 0x04);
				return;
			}
			const unsigned size = file->size;
			u8 fcp[15] = { 0, 0, u8(size >> 8), u8(size), u8(fid >> 8), u8(fid),
				0x04, 0, 0, 0, 0, 0x01, 0x02, 0x00, 0x00 };
			if (file->mutable_data)
				fcp[8] = 0x01; // READ is always; UPDATE requires disabled CHV1.
			if (file->fid == 0x6f39)
			{
				// EF_ACM is READ/UPDATE/INCREASE under CHV1. Access-condition
				// nibbles occupy READ|UPDATE in byte 8 and INCREASE|RFU in 9.
				fcp[8] = 0x11;
				fcp[9] = 0x10;
			}
			fcp[13] = file->structure == file_structure::linear_fixed ? 0x01 :
				file->structure == file_structure::cyclic ? 0x03 : 0x00;
			fcp[14] = file->record_length;
			const unsigned count = std::min<unsigned>(requested, std::size(fcp));
			std::copy_n(fcp, count, response + n); n += count;
		}
	}
	response[n++] = 0x90; response[n++] = 0x00;
	emit_response(response, n);
}

void nokia_sim_card_device::queue_pending_response(unsigned requested)
{
	if (m_p1 != 0 || m_p2 != 0 || requested != m_pending_response_len)
	{
		m_pending_response_len = 0;
		queue_status(0x67, 0x00);
		return;
	}

	u8 response[260];
	unsigned n = 0;
	response[n++] = m_ins;
	std::copy_n(m_pending_response, m_pending_response_len, response + n);
	n += m_pending_response_len;
	response[n++] = 0x90;
	response[n++] = 0x00;
	if (m_trace)
		LOGMASKED(LOG_SIM,
				"sim_device: pending response ins=%02x length=%u status=9000 t=%.8f\n",
				m_ins, m_pending_response_len, machine().time().as_double());
	m_pending_response_len = 0;
	emit_response(response, n);
}

void nokia_sim_card_device::queue_read_binary(unsigned requested)
{
	const file_descriptor *const file =
			selected_file_for(access_shape::transparent);
	if (!file)
		return;
	const unsigned start = (unsigned(m_p1 & 0x7f) << 8) | m_p2;
	if (start + requested > file->size)
	{
		queue_status(0x94, 0x02);
		return;
	}
	u8 response[260];
	unsigned n = 0;
	response[n++] = m_ins;
	for (unsigned offset = 0; offset < requested && n < std::size(response) - 2; offset++)
		response[n++] = ef_byte(m_selected_file, start + offset);
	response[n++] = 0x90; response[n++] = 0x00;
	emit_response(response, n);
}

void nokia_sim_card_device::queue_read_record(unsigned requested)
{
	const file_descriptor *const file =
			selected_file_for(access_shape::record);
	if (!file)
		return;
	if (file->fid == 0x6f39 && m_chv1_enabled && !m_chv_verified[0])
	{
		queue_status(0x98, 0x04);
		return;
	}
	if (requested != file->record_length)
	{
		queue_status(0x67, 0x00);
		return;
	}
	unsigned record;
	if (!resolve_record(*file, record))
		return;
	m_record_pointer = record;
	u8 response[260];
	unsigned n = 0;
	response[n++] = m_ins;
	const unsigned start = (record - 1) * file->record_length;
	for (unsigned offset = 0; offset < file->record_length; offset++)
		response[n++] = ef_byte(m_selected_file, start + offset);
	response[n++] = 0x90; response[n++] = 0x00;
	emit_response(response, n);
}

void nokia_sim_card_device::update_binary()
{
	const file_descriptor *const file =
			selected_file_for(access_shape::transparent);
	if (!file)
		return;
	if (!write_permitted(*file))
		return;
	const unsigned start = (unsigned(m_p1 & 0x7f) << 8) | m_p2;
	if (start + m_tx_len > file->size)
	{
		queue_status(0x94, 0x02);
		return;
	}
	u8 *const data = writable_body(*file);
	if (!data)
		return;
	std::copy_n(m_tx, m_tx_len, data + start);
	if (m_trace)
		LOGMASKED(LOG_SIM, "sim_device: update-binary fid=%04x offset=%u length=%u t=%.8f\n",
				file->fid, start, m_tx_len, machine().time().as_double());
	queue_status(0x90, 0x00);
}

void nokia_sim_card_device::update_record()
{
	const file_descriptor *const file =
			selected_file_for(access_shape::record);
	if (!file)
		return;
	if (!write_permitted(*file))
		return;
	if (m_tx_len != file->record_length)
	{
		queue_status(0x67, 0x00);
		return;
	}
	unsigned record;
	if (!resolve_record(*file, record))
		return;
	u8 *const data = writable_body(*file);
	if (!data)
		return;
	std::copy_n(m_tx, file->record_length, data + (record - 1) * file->record_length);
	m_record_pointer = record;
	if (m_trace)
		LOGMASKED(LOG_SIM, "sim_device: update fid=%04x record=%u length=%u t=%.8f\n",
				file->fid, record, file->record_length, machine().time().as_double());
	queue_status(0x90, 0x00);
}

void nokia_sim_card_device::increase_record()
{
	const file_descriptor *const file =
			selected_file_for(access_shape::cyclic);
	if (!file)
		return;
	if (m_p1 != 0 || m_p2 != 0)
	{
		queue_status(0x6b, 0x00);
		return;
	}
	if (m_tx_len != 3 || file->record_length != 3)
	{
		queue_status(0x67, 0x00);
		return;
	}
	if (!write_permitted(*file))
		return;
	u8 *const data = writable_body(*file);
	if (!data)
		return;

	const u32 current = (u32(data[0]) << 16) | (u32(data[1]) << 8) | data[2];
	const u32 addend = (u32(m_tx[0]) << 16) | (u32(m_tx[1]) << 8) | m_tx[2];
	const u32 increased = current + addend;
	if (increased > 0x00ffffff)
	{
		queue_status(0x98, 0x50);
		return;
	}

	data[0] = increased >> 16;
	data[1] = increased >> 8;
	data[2] = increased;
	m_record_pointer = 1;
	std::copy_n(data, 3, m_pending_response);
	std::copy_n(m_tx, 3, m_pending_response + 3);
	m_pending_response_len = 6;
	if (m_trace)
		LOGMASKED(LOG_SIM, "sim_device: increase fid=%04x value=%06x result=%06x t=%.8f\n",
				file->fid, addend, increased, machine().time().as_double());
	queue_status(0x9f, m_pending_response_len);
}

const nokia_sim_card_device::file_descriptor *nokia_sim_card_device::find_file(u16 fid)
{
	static constexpr file_descriptor files[] = {
		{ 0x3f00, 0x0000, 0, 0, file_structure::directory, false },
		{ 0x7f10, 0x3f00, 0, 0, file_structure::directory, false },
		{ 0x7f20, 0x3f00, 0, 0, file_structure::directory, false },
		{ 0x7f21, 0x3f00, 0, 0, file_structure::directory, false },
		{ 0x7f40, 0x3f00, 0, 0, file_structure::directory, false },
		{ 0x2fe2, 0x3f00, 10, 0, file_structure::transparent, false },
		{ 0x2fe6, 0x3f00, 4, 0, file_structure::transparent, false },
		{ 0x6f3a, 0x7f10, 50 * 32, 32, file_structure::linear_fixed, true },
		{ 0x6f3c, 0x7f10, sms_record_count * sms_record_length,
				sms_record_length, file_structure::linear_fixed, true },
		{ 0x6f42, 0x7f10, smsp_record_count * smsp_record_length,
				smsp_record_length, file_structure::linear_fixed, true },
		{ 0x6f05, 0x7f20, 4, 0, file_structure::transparent, false },
		{ 0x6fb7, 0x7f20, 15, 0, file_structure::transparent, false },
		{ 0x6fad, 0x7f20, 4, 0, file_structure::transparent, false },
		{ 0x6f07, 0x7f20, 9, 0, file_structure::transparent, false },
		{ 0x6f38, 0x7f20, 15, 0, file_structure::transparent, false },
		{ 0x6f74, 0x7f20, 16, 0, file_structure::transparent, true },
		{ 0x6f78, 0x7f20, 2, 0, file_structure::transparent, false },
		{ 0x6f7e, 0x7f20, 11, 0, file_structure::transparent, true },
		{ 0x6f20, 0x7f20, 9, 0, file_structure::transparent, true },
		{ 0x6f30, 0x7f20, 24, 0, file_structure::transparent, false },
		{ 0x6f7b, 0x7f20, 12, 0, file_structure::transparent, false },
		{ 0x6fae, 0x7f20, 1, 0, file_structure::transparent, false },
		{ 0x6f31, 0x7f20, 1, 0, file_structure::transparent, false },
		{ 0x6f37, 0x7f20, 3, 0, file_structure::transparent, false },
		{ 0x6f39, 0x7f20, 3, 3, file_structure::cyclic, true },
		{ 0x6f41, 0x7f20, 5, 0, file_structure::transparent, false },
		{ 0x6f43, 0x7f20, 2, 0, file_structure::transparent, false },
		{ 0x6f46, 0x7f20, 17, 0, file_structure::transparent, false },
		{ 0x6f13, 0x7f20, 1, 0, file_structure::transparent, false },
		{ 0x6f98, 0x7f40, 22, 0, file_structure::transparent, false },
		{ 0x6f9b, 0x7f40, 37, 0, file_structure::transparent, false },
		{ 0x6f91, 0x7f40, 1, 0, file_structure::transparent, false },
		{ 0x6f93, 0x7f40, 1, 0, file_structure::transparent, false },
		{ 0x6f95, 0x7f40, 29, 0, file_structure::transparent, false },
		{ 0x6f96, 0x7f40, 29, 0, file_structure::transparent, false },
		{ 0x6f9f, 0x7f40, 1, 0, file_structure::transparent, false },
		{ 0x6f92, 0x7f40, 1, 0, file_structure::transparent, false },
		{ 0x6f16, 0x7f20, 3, 0, file_structure::transparent, false },
		{ 0x6f15, 0x7f20, 18, 0, file_structure::transparent, false },
		{ 0xea00, 0x7f20, 18, 0, file_structure::transparent, false },
		{ 0xea03, 0x7f20, 11, 0, file_structure::transparent, false }
	};
	for (const auto &file : files)
		if (file.fid == fid)
			return &file;
	return nullptr;
}

bool nokia_sim_card_device::is_directory(u16 fid)
{
	const file_descriptor *file = find_file(fid);
	return file && file->structure == file_structure::directory;
}

bool nokia_sim_card_device::is_known_file(u16 fid) const
{
	if (is_directory(fid))
		return true;
	const file_descriptor *file = find_file(fid);
	if (!file)
		return false;
	if ((fid == 0x6f15 || fid == 0x6f16) && !m_cphs_aoc)
		return false;
	return true;
}

u8 nokia_sim_card_device::ef_byte(u16 fid, unsigned offset) const
{
	static constexpr u8 iccid[] = { 0x98, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0 };
	static constexpr u8 ecc[] = { 0x11, 0xf2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	static constexpr u8 language_preference[] = { 0x01, 0xff, 0xff, 0xff };
	// GSM 11.11 EF_ACC: allocate one ordinary subscriber class (class 0).
	// Erased bytes would allocate every class and set reserved byte-1 bit 3.
	static constexpr u8 access_control_class[] = { 0x00, 0x01 };
	// The base card advertises no optional SIM services.  The opt-in CPHS AoC
	// profile also advertises GSM 11.11 service 5, keeping EF_CSP and EF_SST
	// consistent and causing the ME to read ACM, ACMmax and PUCT normally.
	static constexpr u8 service_table[15] = {
		0xcc, // Services 2 (ADN) and 4 (SMS) allocated and activated.
		0x30, // Service 7: PLMN selector allocated and activated.
		0xc0  // Service 12: SMS parameters allocated and activated.
	};
	// GSM 11.11 defines zero as "ACMmax not valid": this profile enables AoC
	// without imposing a subscriber call-charge limit.
	static constexpr u8 acm_max[] = { 0x00, 0x00, 0x00 };
	static constexpr u8 puct[] = { 'G', 'B', 'P', 0x00, 0x00 };
	// GSM 11.11 EF_AD: normal operation, with no additional administrative
	// information in this phase-2 profile.  Erased 0xff bytes select no valid
	// operation mode and make the subscriber profile internally inconsistent.
	static constexpr u8 administrative_data[] = { 0x00, 0xff, 0xff, 0x02 };
	// IMSI 001010123456789 belongs to the reserved laboratory PLMN advertised
	// by the serving cell. The remaining digits are deterministic test data.
	static constexpr u8 imsi[] = { 0x08, 0x09, 0x10, 0x10, 0x10, 0x32, 0x54, 0x76, 0x98 };
	// Prefer the subscriber's home PLMN 001-01, also advertised by the
	// laboratory serving cell.  Keeping EF_PLMNsel on a different PLMN makes
	// the firmware legitimately leave the serving cell and resume selection.
	static constexpr u8 plmn_selector[] = { 0x00, 0xf1, 0x10 };
	// GSM 11.11 EF_SPN: display the service-provider name on the registered
	// home PLMN. Byte zero is the display-condition octet; the remaining
	// sixteen bytes are the name padded with erased bytes.
	static constexpr u8 service_provider_name[] = {
		0x00, 'D', 'C', 'T', '3', ' ', 'L', 'A', 'B',
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};
	// CPHS phase 2 with only the Customer Service Profile allocated and
	// activated.  The CSP contains its mandatory nine group entries and makes
	// only Advice of Charge (group 03, bit 6) customer-accessible.
	static constexpr u8 cphs_info[] = { 0x02, 0x03, 0x00 };
	static constexpr u8 cphs_aoc[] = {
		0x01, 0x00, 0x02, 0x00, 0x03, 0x20,
		0x04, 0x00, 0x05, 0x00, 0x06, 0x00,
		0x07, 0x00, 0x08, 0x00, 0x09, 0x00
	};
	if (fid == 0x2fe2 && offset < std::size(iccid)) return iccid[offset];
	if (fid == 0x6f3a && offset < sizeof(m_adn)) return m_adn[offset];
	if (fid == 0x6f3c && offset < sizeof(m_sms)) return m_sms[offset];
	if (fid == 0x6f42 && offset < sizeof(m_smsp)) return m_smsp[offset];
	if (fid == 0x6f05 && offset < std::size(language_preference)) return language_preference[offset];
	if (fid == 0x6f38 && offset < std::size(service_table))
	{
		if (m_cphs_aoc && offset == 1)
			return 0x03;
		// Service 17 (SPN) occupies bits 0-1 of byte five.
		if (offset == 4)
			return 0x03;
		return service_table[offset];
	}
	if (fid == 0x6fb7 && offset < std::size(ecc)) return ecc[offset];
	if (fid == 0x6fad && offset < std::size(administrative_data)) return administrative_data[offset];
	if (fid == 0x6f07 && offset < std::size(imsi)) return imsi[offset];
	if (fid == 0x6f20 && offset < std::size(m_kc)) return m_kc[offset];
	if (fid == 0x6f30 && offset < std::size(plmn_selector)) return plmn_selector[offset];
	if (fid == 0x6f46 && offset < std::size(service_provider_name)) return service_provider_name[offset];
	if (fid == 0x6f78 && offset < std::size(access_control_class)) return access_control_class[offset];
	if (fid == 0x6f7b && m_forbidden_test_plmn)
	{
		static constexpr u8 forbidden_plmn[] = { 0x13, 0x00, 0x62 };
		return offset < std::size(forbidden_plmn) ?
				forbidden_plmn[offset] : 0xff;
	}
	if (m_cphs_aoc && fid == 0x6f16 && offset < std::size(cphs_info)) return cphs_info[offset];
	if (m_cphs_aoc && fid == 0x6f15 && offset < std::size(cphs_aoc)) return cphs_aoc[offset];
	if (m_cphs_aoc && fid == 0x6f37 && offset < std::size(acm_max)) return acm_max[offset];
	if (fid == 0x6f39 && offset < std::size(m_acm)) return m_acm[offset];
	if (m_cphs_aoc && fid == 0x6f41 && offset < std::size(puct)) return puct[offset];
	if (fid == 0x6f7e)
		return m_loci[offset];
	if (fid == 0x6f74)
		return m_bcch[offset];
	// GSM 11.11 EF_HPLMN is measured in six-minute units.  Five is an ordinary
	// finite search period; erased 0xff is reserved and not a neutral profile.
	if (fid == 0x6f31) return 0x05;
	if (fid == 0x6fae) return 0x02;
	return 0xff;
}

u8 *nokia_sim_card_device::mutable_file_data(u16 fid)
{
	switch (fid)
	{
	case 0x6f3a: return m_adn;
	case 0x6f3c: return m_sms;
	case 0x6f42: return m_smsp;
	case 0x6f7e: return m_loci;
	case 0x6f20: return m_kc;
	case 0x6f74: return m_bcch;
	case 0x6f39: return m_acm;
	default: return nullptr;
	}
}

const u8 *nokia_sim_card_device::mutable_file_data(u16 fid) const
{
	return const_cast<nokia_sim_card_device *>(this)->mutable_file_data(fid);
}

bool nokia_sim_card_device::access_allowed(const file_descriptor &file) const
{
	// Mutable laboratory files use the CHV1 update condition.  GSM 11.11
	// treats it as ALWAYS while CHV1 is disabled, otherwise verification is
	// scoped to the current card session.
	return file.mutable_data && (!m_chv1_enabled || m_chv_verified[0]);
}
