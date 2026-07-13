#include "emu.h"
#include "nokia_sim_card.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(NOKIA_SIM_CARD, nokia_sim_card_device, "nokia_sim_card", "Nokia DCT3 SIM card")

nokia_sim_card_device::nokia_sim_card_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_SIM_CARD, tag, owner, clock),
	m_irq_cb(*this)
{
}

void nokia_sim_card_device::device_start()
{
	m_rx_timer = timer_alloc(FUNC(nokia_sim_card_device::rx_ready), this);
	save_item(NAME(m_enabled));
	save_item(NAME(m_control));
	save_item(NAME(m_atr));
	save_item(NAME(m_atr_len));
	save_item(NAME(m_rx_fifo));
	save_item(NAME(m_rx_head));
	save_item(NAME(m_rx_tail));
	save_item(NAME(m_rx_count));
	save_item(NAME(m_rx_ready));
	save_item(NAME(m_iir));
	save_item(NAME(m_tx_ready_pending));
	save_item(NAME(m_tx));
	save_item(NAME(m_tx_len));
	save_item(NAME(m_tx_expected));
	save_item(NAME(m_ins));
	save_item(NAME(m_selected_file));
	save_item(NAME(m_selected_df));
	save_item(NAME(m_receiving_body));
}

void nokia_sim_card_device::device_reset()
{
	m_rx_timer->adjust(attotime::never);
	m_control = 0;
	m_rx_head = m_rx_tail = m_rx_count = 0;
	m_rx_ready = false;
	m_iir = 0;
	m_tx_ready_pending = false;
	m_tx_len = m_tx_expected = 0;
	m_ins = 0;
	m_selected_file = 0x3f00;
	m_selected_df = 0x3f00;
	m_receiving_body = false;
}

void nokia_sim_card_device::set_atr(const u8 *data, unsigned length)
{
	m_atr_len = std::min<unsigned>(length, std::size(m_atr));
	std::copy_n(data, m_atr_len, m_atr);
}

u8 nokia_sim_card_device::control_r() const
{
	return m_control | ((m_enabled && BIT(m_control, 7)) ? 0x40 : 0x00);
}

void nokia_sim_card_device::control_w(u8 data)
{
	const bool activate = m_enabled && BIT(data, 7) && !BIT(m_control, 7);
	m_control = data;
	if (activate)
	{
		m_rx_head = m_rx_tail = m_rx_count = 0;
		m_rx_ready = false;
		m_iir = 0;
		m_tx_ready_pending = false;
		m_tx_len = m_tx_expected = 0;
		m_selected_file = 0x3f00;
		m_selected_df = 0x3f00;
		m_receiving_body = false;
		// Deliver ATR outside the control-register write so its FIQ cannot
		// re-enter the firmware's activation routine.
		queue_rx(m_atr, m_atr_len, false, attotime::from_usec(10));
	}
}

void nokia_sim_card_device::queue_rx(
		const u8 *data, unsigned length, bool tx_complete, attotime delay)
{
	for (unsigned i = 0; i < length && m_rx_count < std::size(m_rx_fifo); i++)
	{
		m_rx_fifo[m_rx_tail] = data[i];
		m_rx_tail = (m_rx_tail + 1) % std::size(m_rx_fifo);
		m_rx_count++;
	}
	// A card cannot answer while the CPU is still writing the final transmit
	// byte.  Deferring receive-ready also prevents the firmware's FIQ handler
	// from observing the previous transaction descriptor during that write.
	if (length != 0)
	{
		m_rx_ready = false;
		m_tx_ready_pending = tx_complete;
		m_rx_timer->adjust(delay);
	}
}

TIMER_CALLBACK_MEMBER(nokia_sim_card_device::rx_ready)
{
	if (m_tx_ready_pending)
	{
		m_tx_ready_pending = false;
		m_iir |= 0x10;
		m_irq_cb(1);
		m_irq_cb(0);
		m_rx_timer->adjust(attotime::from_usec(100));
		return;
	}
	if (m_rx_count != 0)
	{
		m_rx_ready = true;
		m_iir |= 0x40;
		m_irq_cb(1);
		m_irq_cb(0);
	}
}

u8 nokia_sim_card_device::rxd_r()
{
	if (!m_rx_ready || m_rx_count == 0)
		return 0;
	const u8 data = m_rx_fifo[m_rx_head];
	m_rx_head = (m_rx_head + 1) % std::size(m_rx_fifo);
	m_rx_count--;
	// SIMI services one received character per FIQ.  Present subsequent
	// characters at serial cadence instead of treating a response as one
	// edge with a pre-filled FIFO.
	if (m_rx_count != 0)
		m_rx_timer->adjust(attotime::from_usec(100));
	else
	{
		m_rx_ready = false;
		m_rx_timer->adjust(attotime::never);
	}
	return data;
}

u8 nokia_sim_card_device::iir_r() const
{
	return m_iir;
}

void nokia_sim_card_device::iir_w(u8 data)
{
	m_iir &= ~data;
}

u8 nokia_sim_card_device::rx_count_r() const
{
	return m_rx_ready ? std::min<u16>(m_rx_count, 0xff) : 0;
}

void nokia_sim_card_device::rx_ack_w(u8 data)
{
}

void nokia_sim_card_device::txd_w(u8 data)
{
	if (!m_enabled || !BIT(m_control, 7))
		return;

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
		queue_rx(m_tx, m_tx_len);
		m_tx_len = m_tx_expected = 0;
		return;
	}

	m_ins = m_tx[1];
	const u8 p3 = m_tx[4];
	if (std::getenv("NOKI3210_TRACE_SIM_RX") != nullptr)
		logerror("sim_device: header cla=%02x ins=%02x p1=%02x p2=%02x p3=%02x selected=%04x t=%.8f\n",
				m_tx[0], m_ins, m_tx[2], m_tx[3], p3, m_selected_file,
				machine().time().as_double());
	if (m_ins == 0xa4 || m_ins == 0x24)
	{
		const u8 procedure = m_ins;
		m_tx_len = 0;
		m_tx_expected = p3;
		m_receiving_body = true;
		queue_rx(&procedure, 1);
		return;
	}

	if (m_ins == 0xc0 || m_ins == 0xf2)
		queue_fcp(m_selected_file, p3);
	else if (m_ins == 0xb0 || m_ins == 0xb2)
		queue_read(m_selected_file, p3);
	else
		queue_status(0x90, 0x00);
	m_tx_len = m_tx_expected = 0;
}

void nokia_sim_card_device::finish_body()
{
	const u16 requested_file = m_tx_len >= 2 ? u16(m_tx[0] << 8) | m_tx[1] : 0;
	const bool select_ok = m_ins != 0xa4 || is_known_file(requested_file);
	if (m_ins == 0xa4 && select_ok)
	{
		m_selected_file = requested_file;
		if (is_directory(requested_file))
			m_selected_df = requested_file;
	}
	if (std::getenv("NOKI3210_TRACE_SIM_RX") != nullptr)
		logerror("sim_device: body ins=%02x length=%u requested=%04x selected=%04x result=%s t=%.8f\n",
				m_ins, m_tx_len, requested_file, m_selected_file, select_ok ? "ok" : "not-found",
				machine().time().as_double());
	m_tx_len = m_tx_expected = 0;
	m_receiving_body = false;
	if (m_ins == 0xa4)
	{
		if (!select_ok)
			queue_status(0x94, 0x04); // GSM 11.11: file ID not found
		else
		{
			const bool df = is_directory(m_selected_file);
			queue_status(0x9f, df ? 22 : 15);
		}
	}
	else
		queue_status(0x90, 0x00);
}

void nokia_sim_card_device::queue_status(u8 sw1, u8 sw2)
{
	const u8 response[] = { sw1, sw2 };
	queue_rx(response, std::size(response));
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
			fcp[13] = 0x81; // clock stop allowed; CHV1 disabled
			fcp[14] = response_fid == 0x3f00 ? 0x02 : 0x00;
			fcp[15] = response_fid == 0x3f00 ? 0x02 : 0x16;
			fcp[16] = 0x03;
			fcp[18] = 0x83; fcp[19] = 0x8a;
			fcp[20] = 0x83; fcp[21] = 0x8a;
			const unsigned count = std::min<unsigned>(requested, std::size(fcp));
			std::copy_n(fcp, count, response + n); n += count;
		}
		else
		{
			const unsigned size = ef_size(fid);
			u8 fcp[15] = { 0, 0, u8(size >> 8), u8(size), u8(fid >> 8), u8(fid),
				0x04, 0, 0, 0, 0, 0x01, 0x02, 0x00, 0x00 };
			const unsigned count = std::min<unsigned>(requested, std::size(fcp));
			std::copy_n(fcp, count, response + n); n += count;
		}
	}
	response[n++] = 0x90; response[n++] = 0x00;
	queue_rx(response, n);
}

void nokia_sim_card_device::queue_read(u16 fid, unsigned requested)
{
	u8 response[260];
	unsigned n = 0;
	response[n++] = m_ins;
	const unsigned count = requested ? requested : ef_size(fid);
	for (unsigned offset = 0; offset < count && n < std::size(response) - 2; offset++)
		response[n++] = ef_byte(fid, offset);
	response[n++] = 0x90; response[n++] = 0x00;
	queue_rx(response, n);
}

bool nokia_sim_card_device::is_directory(u16 fid)
{
	return fid == 0x3f00 || fid == 0x7f10 || fid == 0x7f20 || fid == 0x7f21 || fid == 0x7f40;
}

bool nokia_sim_card_device::is_known_file(u16 fid)
{
	if (is_directory(fid))
		return true;
	return ef_size(fid) != 0;
}

unsigned nokia_sim_card_device::ef_size(u16 fid)
{
	static constexpr struct { u16 fid; u8 size; } files[] = {
		{ 0x2fe2, 0x0a }, { 0x6f05, 0x04 }, { 0x6fb7, 0x0f }, { 0x6fad, 0x03 }, { 0x6f07, 0x09 },
		{ 0x6f38, 0x0f }, { 0x6f74, 0x10 }, { 0x6f78, 0x02 }, { 0x6f7e, 0x0b }, { 0x6f20, 0x09 },
		{ 0x6f7b, 0x0c }, { 0x6fae, 0x01 }, { 0x6f31, 0x01 }, { 0x6f37, 0x03 },
		{ 0x6f41, 0x05 }, { 0x6f43, 0x02 }, { 0x6f46, 0x11 }, { 0x6f13, 0x01 },
		{ 0x6f98, 0x16 }, { 0x6f9b, 0x25 }, { 0x6f91, 0x01 }, { 0x6f93, 0x01 },
		{ 0x6f95, 0x1d }, { 0x6f96, 0x1d }, { 0x6f9f, 0x01 }, { 0x6f92, 0x01 },
		{ 0xea00, 0x12 }, { 0xea03, 0x0b }, { 0x2fe6, 0x04 }
	};
	for (const auto &file : files)
		if (file.fid == fid)
			return file.size;
	return 0;
}

u8 nokia_sim_card_device::ef_byte(u16 fid, unsigned offset)
{
	static constexpr u8 iccid[] = { 0x98, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0 };
	static constexpr u8 ecc[] = { 0x11, 0xf2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	static constexpr u8 language_preference[] = { 0x01, 0xff, 0xff, 0xff };
	// A standards-valid minimal service table: no optional SIM services are
	// allocated or activated. Mandatory card identification remains available.
	static constexpr u8 service_table[15] = { 0 };
	static constexpr u8 imsi[] = { 0x08, 0x09, 0x10, 0x10, 0x32, 0x54, 0x76, 0x98, 0x10 };
	if (fid == 0x2fe2 && offset < std::size(iccid)) return iccid[offset];
	if (fid == 0x6f05 && offset < std::size(language_preference)) return language_preference[offset];
	if (fid == 0x6f38 && offset < std::size(service_table)) return service_table[offset];
	if (fid == 0x6fb7 && offset < std::size(ecc)) return ecc[offset];
	if (fid == 0x6f07 && offset < std::size(imsi)) return imsi[offset];
	if (fid == 0x6f7e) return offset == 10 ? 0x01 : (offset < 4 ? 0xff : 0x00);
	if (fid == 0x6fae) return 0x02;
	return 0xff;
}
