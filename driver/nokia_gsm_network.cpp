// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_gsm_network.h"
#include "gsm_mm_authentication.h"

#define LOG_GSM_NETWORK (1U << 0)
#define VERBOSE (LOG_GSM_NETWORK)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device,
		"nokia_gsm_network", "Nokia DCT3 laboratory GSM network")

namespace {

// Minimum broadcast set for a GSM 900 cell. The reserved test PLMN
// 001-01 avoids coupling the synthetic network to handset network-lock data;
// LAC and cell ID are both 1.
constexpr std::array<std::array<u8, 24>, 4> SYSTEM_INFORMATION = {{
	// SI1 Cell Channel Description is completed by system_information() using
	// bitmap-0 for GSM 900 or a variable bitmap for a single non-GSM-900
	// laboratory carrier.
	{{ 0x55, 0x06, 0x19,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0x2b, 0 }},
	{{ 0x59, 0x06, 0x1a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0, 0, 0 }},
	{{ 0x49, 0x06, 0x1b, 0x00, 0x01, 0x00, 0xf1, 0x10,
		0x00, 0x01, 0x40, 0, 0, 0, 0, 0, 0, 0, 0,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b }},
	{{ 0x31, 0x06, 0x1c, 0x00, 0xf1, 0x10, 0x00, 0x01,
		0, 0, 0, 0, 0, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b }}
}};

constexpr std::array<u8, 9> CCCH_BLOCK_OFFSETS = {
	6, 12, 16, 22, 26, 32, 36, 42, 46
};

} // anonymous namespace

nokia_gsm_network_device::nokia_gsm_network_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_NETWORK, tag, owner, clock)
{
}

void nokia_gsm_network_device::device_start()
{
	save_item(NAME(m_stable_camp_seen));
	save_item(NAME(m_neighbour_bcch_seen));
	save_item(NAME(m_primary_cell_lost));
	save_item(NAME(m_all_cells_lost));
	save_item(NAME(m_recovery_cell_available));
	save_item(NAME(m_stale_neighbour_lost));
}

void nokia_gsm_network_device::device_reset()
{
	m_stable_camp_seen = false;
	m_neighbour_bcch_seen = false;
	m_primary_cell_lost = false;
	m_all_cells_lost = false;
	m_recovery_cell_available = false;
	m_stale_neighbour_lost = false;
}

void nokia_gsm_network_device::set_mobility_profile(mobility_profile profile)
{
	m_mobility_profile = profile;
	m_stable_camp_seen = false;
	m_neighbour_bcch_seen = false;
	m_primary_cell_lost = false;
	m_all_cells_lost = false;
	m_recovery_cell_available = false;
	m_stale_neighbour_lost = false;
	if (profile == mobility_profile::single_cell)
	{
		m_cells.use_single_cell();
		return;
	}

	gsm::mobility::cell primary;
	gsm::mobility::cell neighbour = primary;
	neighbour.arfcn = 2;
	neighbour.bsic = 0x22;
	neighbour.identity = 2;
	neighbour.rxlev_dbm = -55;
	if (profile == mobility_profile::two_cell_different_lac)
		neighbour.location.lac = 2;
	else if (profile == mobility_profile::two_cell_loss_recovery ||
			profile == mobility_profile::two_cell_persistent_loss)
		neighbour.rxlev_dbm = -70;
	m_cells.set(0, primary);
	m_cells.set(1, neighbour);
}

void nokia_gsm_network_device::set_cell_carriers(
		u16 primary_arfcn, u16 neighbour_arfcn)
{
	if (auto *primary = m_cells.at(0))
		primary->arfcn = primary_arfcn;
	if (auto *neighbour = m_cells.at(1))
		neighbour->arfcn = neighbour_arfcn;
}

void nokia_gsm_network_device::set_neighbour_bsic(u8 bsic)
{
	if (auto *neighbour = m_cells.at(1))
		neighbour->bsic = bsic & 0x3f;
}

void nokia_gsm_network_device::set_neighbour_fault_profile(
		neighbour_fault_profile profile)
{
	m_neighbour_fault = profile;
	m_stale_neighbour_lost = false;
	auto *cell = m_cells.at(1);
	if (!cell)
		return;
	if (profile == neighbour_fault_profile::forbidden_plmn)
		cell->location.plmn = { 0x13, 0x00, 0x62 };
	if (profile == neighbour_fault_profile::access_class_excluded)
		// Exclude every ordinary subscriber class (0..9). Emergency and
		// privileged classes retain their separate standards semantics.
		cell->access_class_barred = 0x03ff;
}

u8 nokia_gsm_network_device::synchronization_bsic(
		u16 arfcn, u32 frame_number) const
{
	const auto *cell = m_cells.find(arfcn);
	if (!cell)
		return 0;
	if (cell == m_cells.at(1) &&
			m_neighbour_fault == neighbour_fault_profile::unstable_bsic)
		return cell->bsic ^ ((frame_number / 10) & 1);
	return cell->bsic;
}

bool nokia_gsm_network_device::synchronization_stable(u16 arfcn) const
{
	return !(m_neighbour_fault == neighbour_fault_profile::unstable_bsic &&
			m_cells.at(1) && arfcn == m_cells.at(1)->arfcn);
}

bool nokia_gsm_network_device::system_information_decodable(u16 arfcn) const
{
	if (!m_cells.at(1) || arfcn != m_cells.at(1)->arfcn)
		return true;
	return m_neighbour_fault !=
					neighbour_fault_profile::malformed_system_information &&
			m_neighbour_fault != neighbour_fault_profile::unstable_bsic;
}

bool nokia_gsm_network_device::configured_neighbour(
		u16 serving_arfcn, u16 candidate_arfcn) const
{
	return m_cells.find(serving_arfcn) &&
			m_cells.find(candidate_arfcn) &&
			serving_arfcn != candidate_arfcn;
}

void nokia_gsm_network_device::stable_camp_observed()
{
	if (m_mobility_profile == mobility_profile::single_cell)
		return;
	m_stable_camp_seen = true;
}

void nokia_gsm_network_device::neighbour_list_observed()
{
	if (m_mobility_profile == mobility_profile::single_cell)
		return;
	// The deterministic mobility scenario changes RF conditions only after
	// firmware has demonstrated stable camp and published a neighbour set.
	// BSIC/SI validation, suitability and selection remain handset-owned
	// consequences of that standards-level signal degradation.
	if (m_mobility_profile != mobility_profile::two_cell_loss_recovery)
		m_primary_cell_lost = m_stable_camp_seen;
}

void nokia_gsm_network_device::neighbour_bcch_observed(u16 arfcn)
{
	if (m_mobility_profile == mobility_profile::single_cell ||
			!m_cells.at(0) || arfcn == m_cells.at(0)->arfcn)
		return;
	m_neighbour_bcch_seen = true;
	if (m_neighbour_fault == neighbour_fault_profile::stale_measurement)
		m_stale_neighbour_lost = true;
	if ((m_mobility_profile == mobility_profile::two_cell_loss_recovery ||
				m_mobility_profile ==
						mobility_profile::two_cell_persistent_loss) &&
			m_stable_camp_seen)
		m_all_cells_lost = true;
}

void nokia_gsm_network_device::downlink_signalling_failed(u16 serving_arfcn)
{
	if (m_mobility_profile != mobility_profile::two_cell_loss_recovery ||
			!m_all_cells_lost || !m_cells.at(0) ||
			serving_arfcn != m_cells.at(0)->arfcn)
		return;
	// The deterministic RF scenario restores the former serving carrier after
	// the handset has organically exhausted the TS 45.008 downlink-signalling
	// counter. This proves no-service recovery independently from replacement-
	// cell reselection; search, suitability and selection remain firmware-owned.
	m_all_cells_lost = false;
	m_recovery_cell_available = true;
}

bool nokia_gsm_network_device::cell_receivable(u16 arfcn) const
{
	const auto *cell = m_cells.find(arfcn);
	if (!cell)
		return m_cells.size() == 1;
	if (m_all_cells_lost)
		return false;
	if (m_stale_neighbour_lost && cell == m_cells.at(1))
		return false;
	if (m_mobility_profile == mobility_profile::two_cell_loss_recovery &&
			m_recovery_cell_available)
		return cell == m_cells.at(0) && cell->available;
	return cell->available;
}

void nokia_gsm_network_device::set_cell_profile(cell_profile profile)
{
	m_cell_profile = profile;
	auto *cell = m_cells.at(0);
	if (!cell)
		return;
	cell->cell_barred = profile == cell_profile::barred;
	cell->rxlev_access_min =
			profile == cell_profile::unattainable_rxlev ? 63 : 0;
	cell->available = profile != cell_profile::unavailable;
}

void nokia_gsm_network_device::set_neighbour_cell_profile(cell_profile profile)
{
	auto *cell = m_cells.at(1);
	if (!cell)
		return;
	cell->cell_barred = profile == cell_profile::barred;
	cell->rxlev_access_min =
			profile == cell_profile::unattainable_rxlev ? 63 : 0;
	cell->available = profile != cell_profile::unavailable;
}

gsm::mobility::cell nokia_gsm_network_device::resolved_cell(u16 arfcn) const
{
	if (const auto *configured = m_cells.find(arfcn))
		return *configured;

	// Existing product contracts select their evidenced carrier organically.
	// Until a topology explicitly lists that carrier, preserve the historical
	// single-cell behavior while giving its broadcast data a concrete identity.
	gsm::mobility::cell legacy = *m_cells.at(0);
	legacy.arfcn = arfcn;
	return legacy;
}

std::array<u8, 24> nokia_gsm_network_device::system_information(
		unsigned index, u16 serving_arfcn) const
{
	const gsm::mobility::cell cell = resolved_cell(serving_arfcn);
	const unsigned message_index = index % SYSTEM_INFORMATION.size();
	std::array<u8, 24> result =
			SYSTEM_INFORMATION[message_index];
	if (message_index == 0 &&
			serving_arfcn >= 1 && serving_arfcn <= 124)
	{
		// TS 44.018 10.5.2.1b bitmap-0: ARFCN 1 is bit 0 of octet 18,
		// and the remaining GSM-900 carriers run backwards through the
		// sixteen-octet Cell Channel Description.
		const unsigned bit = serving_arfcn - 1;
		result[18 - bit / 8] |= 1U << (bit & 7);
	}
	else if (message_index == 1 &&
			serving_arfcn >= 1 && serving_arfcn <= 124)
	{
		// TS 44.018 SI2 BCCH Frequency List, bitmap-0. Advertise every
		// configured GSM-900 neighbour except the serving carrier. This is
		// topology discovery data only; the handset remains responsible for
		// measuring, ranking and selecting a candidate.
		for (unsigned index = 0; index < m_cells.size(); ++index)
		{
			const auto *neighbour = m_cells.at(index);
			if (!neighbour || !neighbour->available ||
					neighbour->arfcn == serving_arfcn ||
					neighbour->arfcn < 1 || neighbour->arfcn > 124)
				continue;
			const unsigned bit = neighbour->arfcn - 1;
			result[18 - bit / 8] |= 1U << (bit & 7);
		}
	}
	else if (message_index == 0 &&
			serving_arfcn <= 1023)
	{
		// TS 44.018 10.5.2.1b.7 variable bitmap format. The origin itself is
		// always a member of the set, so a one-carrier cell has no relative
		// RF-channel bits set. This represents DCS 1800 (and other non-bitmap-0
		// ARFCNs) without assigning product-specific meaning to the carrier.
		// Its mandatory SI1 Rest Octets byte is already the following 0x2b:
		// no NCH, DCS 1800 band indicator, then the standard padding pattern.
		result[0] = 0x59;
		result[3] = 0x8e | BIT(serving_arfcn, 9);
		result[4] = serving_arfcn >> 1;
		result[5] = serving_arfcn << 7;
		std::fill(result.begin() + 6, result.begin() + 19, 0);
	}
	else if (message_index == 1 && serving_arfcn <= 1023)
	{
		// TS 44.018 10.5.2.13 variable-bitmap Frequency List. The origin
		// occupies ten bits; its following 111 bitmap positions represent
		// origin+1 through origin+111 modulo 1024. This lets DCS cells
		// advertise configured neighbours without any Nokia packet knowledge.
		result[3] = 0x8e | BIT(serving_arfcn, 9);
		result[4] = serving_arfcn >> 1;
		result[5] = serving_arfcn << 7;
		std::fill(result.begin() + 6, result.begin() + 19, 0);
		for (unsigned index = 0; index < m_cells.size(); ++index)
		{
			const auto *neighbour = m_cells.at(index);
			if (!neighbour || !neighbour->available ||
					neighbour->arfcn == serving_arfcn)
				continue;
			const unsigned offset =
					(neighbour->arfcn + 1024 - serving_arfcn) % 1024;
			if (offset == 0 || offset > 111)
				continue;
			const unsigned bitmap_bit = offset - 1;
			if (bitmap_bit < 7)
				result[5] |= 1U << (6 - bitmap_bit);
			else
			{
				const unsigned following_bit = bitmap_bit - 7;
				result[6 + following_bit / 8] |=
						1U << (7 - (following_bit & 7));
			}
		}
	}

	if (message_index == 2)
	{
		result[3] = cell.identity >> 8;
		result[4] = cell.identity;
		std::copy(cell.location.plmn.begin(), cell.location.plmn.end(),
				result.begin() + 5);
		result[8] = cell.location.lac >> 8;
		result[9] = cell.location.lac;
	}
	else if (message_index == 3)
	{
		std::copy(cell.location.plmn.begin(), cell.location.plmn.end(),
				result.begin() + 3);
		result[6] = cell.location.lac >> 8;
		result[7] = cell.location.lac;
	}

	if (m_neighbour_fault ==
				neighbour_fault_profile::malformed_system_information &&
			m_cells.at(1) && serving_arfcn == m_cells.at(1)->arfcn)
	{
		// A zero pseudo-length is not a complete RR System Information
		// message. Keep its RF carrier and SCH valid so rejection belongs to
		// the firmware's decoded-SI lifecycle rather than carrier discovery.
		result[0] = 0;
	}

	if (cell.cell_barred)
	{
		// TS 44.018 10.5.2.29: CELL_BAR_ACCESS is bit 2 of the first
		// RACH Control Parameters octet. SI3 and SI4 must describe the same
		// cell-access policy.
		if (message_index == 2)
			result[16] |= 0x02;
		else if (message_index == 3)
			result[10] |= 0x02;
	}
	if (cell.rxlev_access_min != 0)
	{
		// TS 44.018 10.5.2.34: RXLEV_ACCESS_MIN=63 requires approximately
		// -48 dBm. The laboratory carrier's evidenced serving level is lower,
		// so this remains a valid broadcast cell which is unsuitable here.
		if (message_index == 2)
			result[15] = (result[15] & 0xc0) |
					(cell.rxlev_access_min & 0x3f);
		else if (message_index == 3)
			result[9] = (result[9] & 0xc0) |
					(cell.rxlev_access_min & 0x3f);
	}
	if (cell.access_class_barred != 0)
	{
		// TS 44.018 10.5.2.29: the two Access Control Class octets occupy the
		// end of the RACH Control Parameters in SI3 and SI4.
		if (message_index == 2)
		{
			result[17] |= cell.access_class_barred >> 8;
			result[18] |= cell.access_class_barred & 0xff;
		}
		else if (message_index == 3)
		{
			result[11] |= cell.access_class_barred >> 8;
			result[12] |= cell.access_class_barred & 0xff;
		}
	}
	if (message_index == 2)
		result[12] = m_periodic_update_timer.encoded();
	return result;
}

std::array<u8, 24> nokia_gsm_network_device::paging_fill() const
{
	// GSM 04.08 9.1.22: Paging Request Type 1 with a one-octet "no identity"
	// mobile identity keeps the subscriber's PCH group active when no service
	// is pending. The final byte is the Nokia decoded-block padding octet.
	std::array<u8, 24> block;
	block.fill(0x2b);
	block[0] = 0x15;
	block[1] = 0x06;
	block[2] = 0x21;
	block[3] = 0x00;
	block[4] = 0x01;
	block[5] = 0xf0;
	return block;
}

std::array<u8, 24> nokia_gsm_network_device::paging_request(
		const u8 *mobile_identity, unsigned length) const
{
	std::array<u8, 24> block;
	block.fill(0x2b);
	if (length != 8)
		return block;

	// GSM 04.08 9.1.22 and 10.5.2.5. Request an SDCCH and preserve the IMSI
	// mobile-identity contents received organically during registration.
	block[0] = 0x31;
	block[1] = 0x06;
	block[2] = 0x21;
	block[3] = 0x10;
	block[4] = length;
	std::copy_n(mobile_identity, length, block.begin() + 5);
	if (m_paging_profile == paging_profile::unmatched_identity)
	{
		// Preserve a valid IMSI encoding while addressing a different
		// subscriber. The last BCD digit is changed without relying on a
		// fixture-specific replacement identity.
		u8 &last = block[5 + length - 1];
		const u8 digit = last >> 4;
		last = (last & 0x0f) | (((digit + 1) % 10) << 4);
	}
	else if (m_paging_profile == paging_profile::malformed_request)
	{
		// Paging Request Type 1 carries an LV Mobile Identity. A length larger
		// than the eight encoded IMSI octets is structurally invalid.
		block[4] = length + 1;
	}
	return block;
}

nokia_gsm_network_device::paging_group
nokia_gsm_network_device::subscriber_paging_group(
		const u8 *mobile_identity, unsigned length) const
{
	// The phase-2 IMSI identity has digit one in the high nibble of octet one,
	// followed by low/high semi-octets. TS 05.02 uses IMSI mod 1000 across nine
	// CCCH blocks and BS_PA_MFRMS=2, as advertised by this cell's SI3.
	if (length != 8 || (mobile_identity[0] & 0x07) != 0x01)
		return { 0, 6 };

	auto digit = [mobile_identity](unsigned index) -> u8
	{
		if (index == 0)
			return mobile_identity[0] >> 4;
		const unsigned packed = index - 1;
		const u8 octet = mobile_identity[1 + packed / 2];
		return (packed & 1) ? octet >> 4 : octet & 0x0f;
	};
	const unsigned last_three = digit(12) * 100 + digit(13) * 10 + digit(14);
	const unsigned group = last_three % (CCCH_BLOCK_OFFSETS.size() * 2);
	return {
		u8(group / CCCH_BLOCK_OFFSETS.size()),
		CCCH_BLOCK_OFFSETS[group % CCCH_BLOCK_OFFSETS.size()]
	};
}

nokia_gsm_network_device::paging_group
nokia_gsm_network_device::paging_request_group(
		const u8 *mobile_identity, unsigned length) const
{
	paging_group result = subscriber_paging_group(mobile_identity, length);
	if (m_paging_profile == paging_profile::wrong_group)
	{
		// Transmit on the next standards-defined CCCH paging block, which is
		// deliberately not the registered IMSI's DRX group.
		auto found = std::find(
				CCCH_BLOCK_OFFSETS.begin(), CCCH_BLOCK_OFFSETS.end(),
				result.frame_offset);
		const unsigned index = found == CCCH_BLOCK_OFFSETS.end() ?
				0 : found - CCCH_BLOCK_OFFSETS.begin();
		result.frame_offset =
				CCCH_BLOCK_OFFSETS[(index + 1) % CCCH_BLOCK_OFFSETS.size()];
		if (result.frame_offset == CCCH_BLOCK_OFFSETS[0])
			result.multiframe_phase ^= 1;
	}
	return result;
}

std::array<u8, 24> nokia_gsm_network_device::immediate_assignment(
		u8 random_access, u32 frame_number, u16 serving_arfcn) const
{
	// GSM 04.08 9.1.18 and 10.5.2.30. Assign SDCCH/8 subchannel 0,
	// timeslot 0 on the non-hopping serving carrier, and echo the exact random
	// access octet and reception frame which identify the phone's request.
	const u8 t1_prime = (frame_number / 1326) & 0x1f;
	const u8 t2 = frame_number % 26;
	const u8 t3 = frame_number % 51;
	std::array<u8, 24> block = {
		0x2d, 0x06, 0x3f, 0x00,
		0x20, u8((serving_arfcn >> 8) & 0x03), u8(serving_arfcn),
		random_access, u8((t1_prime << 3) | (t3 >> 3)), u8((t3 << 5) | t2),
		0x00, 0x00,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b
	};
	if (m_assignment_profile == assignment_profile::mismatched_request_reference)
		block[7] ^= 0x01;
	return block;
}

std::array<u8, 17> nokia_gsm_network_device::location_update_accept(
		const u8 *location_update_request, unsigned length,
		u16 serving_arfcn) const
{
	// GSM 04.08 9.2.13. The mandatory body is the MM header and Location
	// Area Identification. With no allocated TMSI, the network includes the
	// IMSI mobile-identity IE so the phone discards any stale TMSI. This accepts
	// the subscriber in the laboratory PLMN and LAC advertised by SI3.
	std::array<u8, 17> message = {
		0x05, 0x02, 0x00, 0xf1, 0x10, 0x00, 0x01,
		0x17, 0x08, 0, 0, 0, 0, 0, 0, 0, 0
	};
	const auto serving = resolved_cell(serving_arfcn);
	std::copy(serving.location.plmn.begin(), serving.location.plmn.end(),
			message.begin() + 2);
	message[5] = serving.location.lac >> 8;
	message[6] = serving.location.lac;
	// GSM 04.08 Location Updating Request places the mobile-identity length
	// at octet 10. Preserve the received identity when it is the eight-octet
	// IMSI form used by the laboratory subscriber.
	if (length >= 18 && location_update_request[9] == 8)
		std::copy_n(location_update_request + 10, 8, message.begin() + 9);
	return message;
}

const gsm::a3a8::block &nokia_gsm_network_device::laboratory_ki()
{
	static constexpr gsm::a3a8::block key = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	return key;
}

std::array<u8, 19> nokia_gsm_network_device::authentication_request() const
{
	// GSM 04.08 9.2.2: MM header, key sequence 0, and a 128-bit RAND.
	static constexpr gsm::a3a8::block rand = {
		0x23, 0x55, 0x3c, 0xbe, 0x96, 0x37, 0xa8, 0x9d,
		0x21, 0x8a, 0xe6, 0x4d, 0xae, 0x47, 0xbf, 0x35
	};
	return gsm::mm::authentication::request(0, rand);
}

std::array<u8, 2> nokia_gsm_network_device::authentication_reject() const
{
	return gsm::mm::authentication::reject();
}

bool nokia_gsm_network_device::authentication_response_valid(
		const u8 *information, unsigned length) const
{
	return gsm::mm::authentication::response_valid(
			authentication_result(), information, length);
}

gsm::a3a8::result nokia_gsm_network_device::authentication_result() const
{
	const auto request = authentication_request();
	gsm::a3a8::block rand;
	std::copy(request.begin() + 3, request.end(), rand.begin());
	return gsm::a3a8::aes_example(laboratory_ki(), rand);
}

std::array<u8, 3> nokia_gsm_network_device::cipher_mode_command() const
{
	// GSM 04.08 9.1.9: SC in bit 0; algorithm identifier in bits 3..1.
	// A5/0 has SC clear.  A5/1 uses identifier zero with SC set.
	const u8 setting = m_cipher_algorithm == gsm::a5::algorithm::a5_0 ?
			0x00 : u8(1 | ((u8(m_cipher_algorithm) - 1) << 1));
	return { 0x06, 0x35, setting };
}

std::array<u8, 2> nokia_gsm_network_device::cm_service_accept() const
{
	// GSM 04.08 9.2.5. The network accepts the MM connection requested by the
	// handset before any mobile-originated call-control transaction begins.
	return { 0x05, 0x21 };
}

std::array<u8, 3> nokia_gsm_network_device::cm_service_reject() const
{
	// GSM 04.08 9.2.6. Cause 0x20 is "service option not supported"; it
	// rejects this MM connection without fabricating a call-control result.
	return { 0x05, 0x22, 0x20 };
}

std::array<u8, 10> nokia_gsm_network_device::mm_information() const
{
	// GSM 04.08 9.2.15. Keep this deterministic: 2026-07-24 12:00:00 UTC.
	// The phone consumes this on the newly established MM connection before
	// entering call control. Octets use GSM's swapped semi-octet BCD form.
	return { 0x05, 0x32, 0x47, 0x62, 0x70, 0x42, 0x21, 0x00, 0x00, 0x00 };
}

nokia_gsm_network_device::layer3_message
nokia_gsm_network_device::incoming_call_setup(
		const u8 *digits, unsigned digit_count) const
{
	// GSM 04.08 9.3.23. Transaction 0 is network-originated. The bearer
	// capability is speech and the SIGNAL IE requests ordinary ringing.
	layer3_message result;
	digit_count = std::min<unsigned>(digit_count, 20);
	const unsigned bcd_octets = (digit_count + 1) / 2;
	const std::array<u8, 10> prefix = {
		0x03, 0x05, 0x04, 0x04, 0x60, 0x02, 0x00, 0x81, 0x34, 0x01
	};
	std::copy(prefix.begin(), prefix.end(), result.data.begin());
	result.data[10] = 0x5c;
	result.data[11] = u8(1 + bcd_octets);
	result.data[12] = 0x81;
	for (unsigned index = 0; index < bcd_octets; ++index)
	{
		const u8 low = digits[2 * index];
		const u8 high = 2 * index + 1 < digit_count ?
				digits[2 * index + 1] : 0x0f;
		result.data[13 + index] = low | (high << 4);
	}
	result.length = 13 + bcd_octets;
	return result;
}

std::array<u8, 2> nokia_gsm_network_device::call_proceeding(
		u8 transaction) const
{
	// GSM 04.08 9.3.3. Preserve the transaction allocated by the mobile while
	// setting the network-to-mobile transaction-identifier flag.
	return { u8(transaction ^ 0x80), 0x02 };
}

std::array<u8, 2> nokia_gsm_network_device::call_alerting(
		u8 transaction) const
{
	// GSM 04.08 9.3.1. The remote laboratory subscriber is now alerting.
	return { u8(transaction ^ 0x80), 0x01 };
}

std::array<u8, 2> nokia_gsm_network_device::call_connect(
		u8 transaction) const
{
	// GSM 04.08 9.3.5. Connect the same mobile-originated CC transaction.
	return { u8(transaction ^ 0x80), 0x07 };
}

std::array<u8, 5> nokia_gsm_network_device::call_disconnect(
		u8 transaction, u8 cause) const
{
	// GSM 04.08 9.3.7 and 10.5.4.11. The cause IE uses the public-network,
	// local-user location and an extension-qualified Q.850 cause value.
	return {
		u8(transaction ^ 0x80), 0x25, 0x02, 0xe0, u8(0x80 | cause)
	};
}

std::array<u8, 8> nokia_gsm_network_device::traffic_assignment() const
{
	// GSM 04.08 9.1.2 and 10.5.2.5. Move the call from its temporary SDCCH
	// onto TCH/F timeslot 1 on non-hopping ARFCN 1. TSC 2 is the BCC carried
	// by the laboratory cell's BSIC 0x12; power level 0 is the mandatory
	// initial Power Command. Channel Mode selects GSM full-rate speech v1.
	return { 0x06, 0x2e, 0x09, 0x40, 0x01, 0x00, 0x63, 0x01 };
}

unsigned nokia_gsm_network_device::incoming_sms_message_count() const
{
	if (m_sms_profile == sms_profile::fill_capacity)
		return 11;
	return m_sms_profile == sms_profile::two_sequential ||
			m_sms_profile == sms_profile::duplicate ? 2 : 1;
}

nokia_gsm_network_device::layer3_message
nokia_gsm_network_device::incoming_sms_cp_data(unsigned message_index) const
{
	// GSM 04.11/03.40 mobile-terminated CP-DATA containing RP-DATA and one
	// SMS-DELIVER. The deterministic fixture is text "hello" from 5551234,
	// via service centre +1234567890, timestamped 2026-07-24 12:00:00 UTC.
	static constexpr std::array FIRST = {
		0x09, 0x01, 0x21,
		0x01, 0x40, 0x06, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09,
		0x00, 0x16,
		0x04, 0x07, 0x81, 0x55, 0x15, 0x32, 0xf4,
		0x00, 0x00,
		0x62, 0x70, 0x42, 0x21, 0x00, 0x00, 0x00,
		0x05, 0xe8, 0x32, 0x9b, 0xfd, 0x06
	};
	static constexpr std::array SECOND = {
		0x09, 0x01, 0x21,
		0x01, 0x41, 0x06, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09,
		0x00, 0x16,
		0x04, 0x07, 0x81, 0x55, 0x95, 0x87, 0xf6,
		0x00, 0x00,
		0x62, 0x70, 0x42, 0x21, 0x10, 0x00, 0x00,
		0x05, 0xf7, 0xb7, 0x9c, 0x4d, 0x06
	};

	layer3_message result;
	if (message_index >= incoming_sms_message_count())
		return result;
	const auto &source =
			message_index == 1 && m_sms_profile != sms_profile::duplicate ?
			SECOND : FIRST;
	std::copy(source.begin(), source.end(), result.data.begin());
	result.length = source.size();
	if (m_sms_profile == sms_profile::fill_capacity)
	{
		result.data[4] = 0x40 + message_index;
		// Give each standards-shaped TPDU a separately observable sender
		// suffix and timestamp while filling the card through ordinary
		// firmware delivery rather than pre-populating SIM records.
		result.data[19] = u8((message_index % 10) << 4) | 0x02;
		result.data[27] =
				u8((message_index % 10) << 4) | u8(message_index / 10);
	}

	switch (m_sms_profile)
	{
	case sms_profile::invalid_originating_address:
		result.data[15] = 0x20;
		break;
	case sms_profile::unsupported_dcs:
		result.data[22] = 0x80;
		break;
	case sms_profile::truncated_user_data:
		result.length -= 2;
		result.data[2] -= 2;
		result.data[13] -= 2;
		break;
	case sms_profile::inconsistent_user_data_length:
		result.data[30] = 0x20;
		break;
	default:
		break;
	}
	return result;
}

bool nokia_gsm_network_device::incoming_sms_admissible(
		unsigned message_index) const
{
	const auto message = incoming_sms_cp_data(message_index);
	return gsm::sms::parse_deliver(
			message.data.data(), message.length).valid;
}

nokia_gsm_network_device::layer3_message
nokia_gsm_network_device::sms_status_report_cp_data(
		u8 message_reference, const u8 *recipient_digits,
		unsigned recipient_digit_count) const
{
	// GSM 04.11 RP-DATA carrying a GSM 03.40 SMS-STATUS-REPORT. The report
	// echoes TP-MR and TP-RA from the accepted SMS-SUBMIT and records successful
	// delivery (TP-ST 00). The fixed timestamps are laboratory network time.
	layer3_message result;
	if (!recipient_digits || recipient_digit_count == 0 ||
			recipient_digit_count > 20)
		return result;
	std::array<u8, 20> tpdu{};
	unsigned n = 0;
	tpdu[n++] = 0x02;
	tpdu[n++] = message_reference;
	tpdu[n++] = recipient_digit_count;
	tpdu[n++] = 0x81;
	for (unsigned digit = 0; digit < recipient_digit_count; digit += 2)
	{
		const u8 high = digit + 1 < recipient_digit_count ?
				recipient_digits[digit + 1] : 0x0f;
		tpdu[n++] = recipient_digits[digit] | (high << 4);
	}
	static constexpr std::array<u8, 7> SERVICE_TIME =
			{ 0x62, 0x90, 0x50, 0x10, 0x00, 0x00, 0x00 };
	static constexpr std::array<u8, 7> DELIVERY_TIME =
			{ 0x62, 0x90, 0x50, 0x10, 0x10, 0x00, 0x00 };
	std::copy(SERVICE_TIME.begin(), SERVICE_TIME.end(), tpdu.begin() + n);
	n += SERVICE_TIME.size();
	std::copy(DELIVERY_TIME.begin(), DELIVERY_TIME.end(), tpdu.begin() + n);
	n += DELIVERY_TIME.size();
	tpdu[n++] = 0x00;

	unsigned out = 0;
	result.data[out++] = 0x09;
	result.data[out++] = 0x01;
	const unsigned cp_length = out++;
	result.data[out++] = 0x01;
	result.data[out++] = 0x42;
	static constexpr std::array<u8, 7> SMSC =
			{ 0x06, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09 };
	std::copy(SMSC.begin(), SMSC.end(), result.data.begin() + out);
	out += SMSC.size();
	result.data[out++] = 0x00;
	result.data[out++] = n;
	std::copy_n(tpdu.begin(), n, result.data.begin() + out);
	out += n;
	result.data[cp_length] = out - 3;
	result.length = out;
	if (machine().options().verbose())
	{
		std::string recipient;
		for (unsigned index = 0; index < recipient_digit_count; ++index)
			recipient += char('0' + recipient_digits[index]);
		LOGMASKED(LOG_GSM_NETWORK,
				"gsm_sms_status_report: mr=%02x recipient=%s status=00 "
				"length=%u t=%.6f\n",
				message_reference, recipient.c_str(), result.length,
				machine().time().as_double());
	}
	return result;
}

unsigned nokia_gsm_network_device::incoming_smart_message_part_count() const
{
	if (m_smart_message_profile ==
			smart_message_profile::missing_second_part)
		return 1;
	if (m_smart_message_profile ==
			smart_message_profile::stale_then_valid)
		return 3;
	return (smart_message_ringtone_payload_length +
			smart_message_multipart_part_capacity - 1) /
			smart_message_multipart_part_capacity;
}

nokia_gsm_network_device::layer3_message
nokia_gsm_network_device::incoming_smart_message_cp_data(
		unsigned part_index) const
{
	// This is a complete Nokia RTPL tone rather than the former four-byte
	// routing marker. RTPL permits more than one command before its zero
	// terminator; composing the same known-valid command twice makes a legal
	// 251-byte long ringtone and exercises Nokia's multipart envelope.
	static constexpr std::array<u8, 125> RINGTONE_COMMAND = {
		0x02, 0x4a, 0x3a, 0x7d, 0x51, 0x95, 0xcd, 0xd0,
		0x81, 0x99, 0xbd, 0xc8, 0x81, 0x11, 0xa1, 0xa5,
		0xc9, 0x85, 0xb4, 0x04, 0x00, 0x9b, 0x28, 0xca,
		0xea, 0x22, 0x82, 0x28, 0x49, 0xa4, 0x1c, 0x41,
		0xa6, 0x1c, 0x41, 0x84, 0x10, 0x42, 0x88, 0xa0,
		0x8a, 0x12, 0x69, 0x07, 0x18, 0x69, 0x84, 0x18,
		0x61, 0x24, 0x10, 0x55, 0x05, 0x50, 0x61, 0x05,
		0x90, 0x55, 0x85, 0x90, 0xa2, 0x2c, 0x49, 0x16,
		0x13, 0x61, 0x54, 0x15, 0x61, 0x56, 0x10, 0x61,
		0x56, 0x18, 0x41, 0x02, 0x28, 0x8a, 0x08, 0xa1,
		0x26, 0x90, 0x71, 0x06, 0x98, 0x71, 0x06, 0x10,
		0x41, 0x0a, 0x22, 0x82, 0x2d, 0x49, 0x08, 0x1a,
		0x41, 0xc6, 0x1a, 0x41, 0x84, 0x90, 0x41, 0x54,
		0x15, 0x41, 0x84, 0x16, 0x41, 0x56, 0x16, 0x42,
		0x88, 0xb1, 0x24, 0x58, 0x4d, 0x85, 0x50, 0x55,
		0x84, 0xd0, 0x4d, 0x84, 0x50
	};
	static constexpr unsigned PAYLOAD_LENGTH =
			RINGTONE_COMMAND.size() * 2 + 1;
	static_assert(PAYLOAD_LENGTH == smart_message_ringtone_payload_length);
	const unsigned delivered_part_count = incoming_smart_message_part_count();
	const unsigned declared_part_count =
			(smart_message_ringtone_payload_length +
					smart_message_multipart_part_capacity - 1) /
					smart_message_multipart_part_capacity;
	layer3_message result;
	if (part_index >= delivered_part_count ||
			declared_part_count > smart_message_maximum_parts)
		return result;

	std::array<u8, PAYLOAD_LENGTH> ringtone{};
	std::copy(RINGTONE_COMMAND.begin(), RINGTONE_COMMAND.end(),
			ringtone.begin());
	std::copy(RINGTONE_COMMAND.begin(), RINGTONE_COMMAND.end(),
			ringtone.begin() + RINGTONE_COMMAND.size());
	ringtone.back() = 0x00;
	if (m_smart_message_profile ==
			smart_message_profile::invalid_rtpl_command)
	{
		// Retain the evidenced terminal zero but remove every valid top-level
		// command, so parser rejection cannot be masked by the second command.
		ringtone.fill(0xff);
		ringtone.back() = 0x00;
	}
	if (m_smart_message_profile ==
			smart_message_profile::missing_rtpl_terminator)
		ringtone.back() = 0xff;

	unsigned payload_part_index = part_index;
	if (m_smart_message_profile ==
			smart_message_profile::stale_then_valid)
		payload_part_index = part_index == 0 ? 0 : part_index - 1;
	if (m_smart_message_profile ==
			smart_message_profile::duplicate_first_part)
		payload_part_index = 0;
	else if (m_smart_message_profile ==
			smart_message_profile::second_part_first)
		payload_part_index = declared_part_count - 1 - part_index;
	const unsigned payload_offset =
			payload_part_index * smart_message_multipart_part_capacity;
	const unsigned payload_length = std::min<unsigned>(
			smart_message_multipart_part_capacity,
			ringtone.size() - payload_offset);
	const unsigned user_data_length = 12 + payload_length;
	const unsigned tpdu_length = 17 + user_data_length;
	const unsigned rpdu_length = 11 + tpdu_length;

	auto append = [&result](u8 value) {
		result.data[result.length++] = value;
	};
	append(0x09);
	append(0x01);
	append(rpdu_length);
	append(0x01);
	append(0x40 + part_index);
	append(0x06);
	append(0x91);
	append(0x21);
	append(0x43);
	append(0x65);
	append(0x87);
	append(0x09);
	append(0x00);
	append(tpdu_length);
	const unsigned transaction_part_index =
			m_smart_message_profile ==
					smart_message_profile::stale_then_valid &&
					part_index != 0 ?
			part_index - 1 : part_index;
	append(transaction_part_index + 1 < declared_part_count ? 0x40 : 0x44);
	append(0x07);
	append(0x81);
	append(0x55);
	append(0x15);
	append(0x32);
	append(0xf4);
	append(0x00);
	append(0xf5);
	append(0x62);
	append(0x70);
	append(0x42);
	append(0x21);
	append(0x00);
	append(0x00);
	append(0x00);
	append(user_data_length);
	const bool truncated_udh =
			m_smart_message_profile ==
					smart_message_profile::truncated_udh;
	append(truncated_udh ? 0x06 : 0x0b);
	append(0x05);
	append(0x04);
	append(0x15);
	append(m_smart_message_profile ==
					smart_message_profile::wrong_destination_port ?
			0x82 : 0x81);
	if (truncated_udh)
	{
		std::copy_n(ringtone.begin() + payload_offset, payload_length,
				result.data.begin() + result.length);
		result.length += payload_length;
		return result;
	}
	append(0x00);
	append(0x00);
	append(0x00);
	append(0x03);
	append(m_smart_message_profile ==
					smart_message_profile::mismatched_reference &&
			part_index == 1 ? 0x7b : 0x7a);
	if (m_smart_message_profile ==
			smart_message_profile::stale_then_valid)
		result.data[result.length - 1] = part_index == 0 ? 0x7a : 0x7b;
	append(m_smart_message_profile ==
					smart_message_profile::incorrect_total &&
			part_index == 1 ? declared_part_count + 1 :
			declared_part_count);
	append(m_smart_message_profile ==
					smart_message_profile::duplicate_first_part ?
			1 :
			m_smart_message_profile ==
					smart_message_profile::second_part_first ?
				declared_part_count - part_index :
			m_smart_message_profile ==
					smart_message_profile::stale_then_valid ?
				(part_index == 0 ? 1 : part_index) :
				part_index + 1);
	std::copy_n(ringtone.begin() + payload_offset, payload_length,
			result.data.begin() + result.length);
	result.length += payload_length;
	return result;
}

std::array<u8, 2> nokia_gsm_network_device::sms_cp_ack(u8 transaction) const
{
	return { u8(transaction ^ 0x80), 0x04 };
}

std::array<u8, 5> nokia_gsm_network_device::sms_rp_ack(
		u8 mobile_transaction, u8 rp_reference) const
{
	// GSM 04.11: acknowledge the mobile's CP transaction with network-to-
	// mobile CP-DATA carrying RP-ACK for the submitted RP message reference.
	return { u8(mobile_transaction ^ 0x80), 0x01, 0x02, 0x03, rp_reference };
}

std::array<u8, 7> nokia_gsm_network_device::sms_rp_error(
		u8 mobile_transaction, u8 rp_reference, u8 cause) const
{
	// GSM 04.11 sections 7.3.4 and 8.2.5.4: network-to-mobile
	// RP-ERROR (MTI 5) with the mandatory one-octet LV cause.
	return { u8(mobile_transaction ^ 0x80), 0x01, 0x04,
			0x05, rp_reference, 0x01, u8(cause & 0x7f) };
}

std::array<u8, 2> nokia_gsm_network_device::connect_acknowledge(
		u8 transaction) const
{
	return { u8(transaction ^ 0x80), 0x0f };
}

std::array<u8, 2> nokia_gsm_network_device::call_release(u8 transaction) const
{
	return { u8(transaction ^ 0x80), 0x2d };
}

std::array<u8, 2> nokia_gsm_network_device::call_release_complete(
		u8 transaction) const
{
	return { u8(transaction ^ 0x80), 0x2a };
}

std::array<u8, 3> nokia_gsm_network_device::channel_release() const
{
	// GSM 04.08 9.1.7. Cause 0 is "normal event". Once Location Updating has
	// completed, the network releases the temporary SDCCH and the mobile returns
	// to the already-selected serving cell.
	return { 0x06, 0x0d, 0x00 };
}

s8 nokia_gsm_network_device::serving_rssi(unsigned sample) const
{
	return cell_rssi(m_cells.at(0)->arfcn, sample);
}

s8 nokia_gsm_network_device::cell_rssi(u16 arfcn, unsigned sample) const
{
	// A real receiver never returns an identical RSSI forever. The ROM retains
	// the previous sample and requires a strict improvement before promoting a
	// background measurement to a usable candidate. Keep each laboratory cell
	// deterministic while representing that measured-signal variation.
	const auto *configured = m_cells.find(arfcn);
	if (!configured && m_cells.size() > 1)
		return -127;
	const gsm::mobility::cell cell =
			configured ? *configured : resolved_cell(arfcn);
	if (!cell_receivable(arfcn))
		return -127;
	const s8 baseline =
			m_primary_cell_lost && configured == m_cells.at(0) ?
					-120 :
			m_mobility_profile == mobility_profile::two_cell_loss_recovery &&
					m_recovery_cell_available &&
					configured == m_cells.at(1) ?
					-55 : cell.rxlev_dbm;
	static constexpr std::array<s8, 4> RSSI_VARIATION = { 0, -1, 1, 0 };
	return std::clamp<int>(
			int(baseline) + RSSI_VARIATION[sample % RSSI_VARIATION.size()],
			-127, 127);
}
