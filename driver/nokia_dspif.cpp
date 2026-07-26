// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "emuopts.h"
#include "nokia_dspif.h"

#define LOG_DSPIF (1U << 0)
#define VERBOSE (LOG_DSPIF)
#include "logmacro.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {

std::string packet_hex(const u8 *payload, unsigned length)
{
	std::ostringstream result;
	result << std::hex << std::setfill('0');
	for (unsigned index = 0; index < length; index++)
		result << std::setw(2) << unsigned(payload[index]);
	return result.str();
}

} // anonymous namespace

DEFINE_DEVICE_TYPE(NOKIA_DSPIF, nokia_dspif_device, "nokia_dspif", "Nokia DCT3 DSP interface")

nokia_dspif_device::nokia_dspif_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_DSPIF, tag, owner, clock),
	m_tx_commit_cb(*this),
	m_service_pending_cb(*this),
	m_doorbell_cb(*this),
	m_shared_002_write_cb(*this),
	m_shared_0fe_read_cb(*this),
	m_shared_0fe_write_cb(*this),
	m_shared_100_read_cb(*this),
	m_shared_100_write_cb(*this),
	m_fiq0_cb(*this),
	m_service_irq_cb(*this)
{
}

void nokia_dspif_device::device_start()
{
	m_trace_enabled = machine().options().verbose();
	save_item(NAME(m_ram));
	save_item(NAME(m_dspif));
}

void nokia_dspif_device::device_reset()
{
	std::fill(std::begin(m_ram), std::end(m_ram), 0);
	std::fill(std::begin(m_dspif), std::end(m_dspif), 0);
}

u16 nokia_dspif_device::shared_r(offs_t offset)
{
	offset &= 0x7ff;
	const unsigned byte_offset = offset << 1;
	const u16 value = m_ram[offset];
	if (offset == (0x0fe / 2))
	{
		m_shared_0fe_read_cb(1);
		m_shared_0fe_read_cb(0);
	}
	if (offset == (0x100 / 2))
	{
		m_shared_100_read_cb(1);
		m_shared_100_read_cb(0);
	}
	if (m_trace_enabled && (byte_offset <= 0x004 || byte_offset == 0x0e0 ||
			byte_offset == 0x0fe || byte_offset == 0x100 ||
			byte_offset == 0x0a4 || byte_offset == 0x0a6 ||
			byte_offset == 0x0a8 || byte_offset == 0x0aa ||
			byte_offset == 0x1c8 || byte_offset == 0x1ca || byte_offset == 0x0e4))
		LOGMASKED(LOG_DSPIF, "dspif_transport: RAM R off=%03x data=%04x t=%.6f\n",
				byte_offset, value, machine().time().as_double());
	return value;
}

void nokia_dspif_device::shared_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset &= 0x7ff;
	COMBINE_DATA(&m_ram[offset]);
	if (m_trace_enabled && ((offset << 1) <= 0x004 || (offset << 1) == 0x0e0 ||
			(offset << 1) == 0x0e2 || (offset << 1) == 0x0e4 ||
			(offset << 1) == 0x0fe || (offset << 1) == 0x100 ||
			(offset << 1) == 0x0a8 || (offset << 1) == 0x0aa ||
			(offset << 1) == 0x0dc ||
			(offset << 1) == 0x0ae || (offset << 1) == 0x0b0 ||
			(offset << 1) == 0x0b6 ||
			offset == TX_PRODUCER || offset == TX_CONSUMER ||
			offset == RX_PRODUCER || offset == RX_CONSUMER || offset == SVC_PENDING))
		LOGMASKED(LOG_DSPIF, "dspif_transport: RAM W off=%03x data=%04x t=%.6f\n",
				offset << 1, m_ram[offset], machine().time().as_double());
	if (offset == TX_PRODUCER)
	{
		if (m_trace_enabled)
			LOGMASKED(LOG_DSPIF, "dspif_transport: TX commit producer=%03x context=%s t=%.6f\n",
					m_ram[offset], machine().describe_context(), machine().time().as_double());
		m_tx_commit_cb(1);
		m_tx_commit_cb(0);
	}
	if (offset == SVC_PENDING && m_ram[offset] != 0)
	{
		m_service_pending_cb(1);
		m_service_pending_cb(0);
	}
	if (offset == (0x002 / 2))
	{
		m_shared_002_write_cb(1);
		m_shared_002_write_cb(0);
	}
	if (offset == (0x0fe / 2))
	{
		m_shared_0fe_write_cb(1);
		m_shared_0fe_write_cb(0);
	}
	if (offset == (0x100 / 2))
	{
		m_shared_100_write_cb(1);
		m_shared_100_write_cb(0);
	}
}

void nokia_dspif_device::peer_shared_w(offs_t offset, u16 data)
{
	offset &= 0x7ff;
	const u16 old_data = m_ram[offset];
	m_ram[offset] = data;
	if (m_trace_enabled)
		LOGMASKED(LOG_DSPIF, "dspif_transport: peer RAM W off=%03x old=%04x data=%04x t=%.6f\n",
				offset << 1, old_data, data, machine().time().as_double());
}

u8 nokia_dspif_device::dspif_r(offs_t offset) const
{
	return m_dspif[offset & 3];
}

void nokia_dspif_device::dspif_w(offs_t offset, u8 data)
{
	offset &= 3;
	m_dspif[offset] = data;
	if (offset != 1)
		return;
	if (m_trace_enabled)
		LOGMASKED(LOG_DSPIF, "dspif_transport: doorbell command=%04x pending=%04x t=%.6f\n",
				(u16(m_dspif[0]) << 8) | m_dspif[1], m_ram[SVC_PENDING],
				machine().time().as_double());
	m_doorbell_cb(1);
	m_doorbell_cb(0);
}

u8 nokia_dspif_device::tx_payload_byte(unsigned cursor, unsigned index) const
{
	const u16 packed = m_ram[(cursor + 1 + (index / 2)) % TX_WORDS];
	return BIT(index, 0) ? packed : packed >> 8;
}

bool nokia_dspif_device::peek_tx_packet(packet &result) const
{
	const unsigned cursor = m_ram[TX_CONSUMER] % TX_WORDS;
	const unsigned producer = m_ram[TX_PRODUCER] % TX_WORDS;
	if (cursor == producer)
		return false;
	const u16 header = m_ram[cursor];
	result.length = header >> 8;
	result.type = header & 0xff;
	result.words = (result.length + 3) / 2;
	const unsigned available = producer >= cursor ? producer - cursor : TX_WORDS - cursor + producer;
	if (result.words == 0 || result.words > available)
		return false;
	for (unsigned i = 0; i < result.length; i++)
		result.payload[i] = tx_payload_byte(cursor, i);
	return true;
}

void nokia_dspif_device::consume_tx_packet(const packet &value)
{
	peer_shared_w(TX_CONSUMER, (m_ram[TX_CONSUMER] + value.words) % TX_WORDS);
	if (m_trace_enabled)
		LOGMASKED(LOG_DSPIF, "dspif_transport: TX consume type=%02x payload=%u words=%u consumer=%02x data=%s t=%.6f\n",
				value.type, value.length, value.words, m_ram[TX_CONSUMER],
				packet_hex(value.payload.data(), value.length).c_str(),
				machine().time().as_double());
}

bool nokia_dspif_device::enqueue_rx_packet(u8 type, const u8 *payload, unsigned payload_length)
{
	if (payload_length > 255)
		return false;
	const unsigned words = (payload_length + 3) / 2;
	unsigned producer = m_ram[RX_PRODUCER];
	const unsigned consumer = m_ram[RX_CONSUMER];
	if (producer < RX_START || producer >= RX_END ||
			consumer < RX_START || consumer >= RX_END)
		return false;
	const unsigned free_words = consumer > producer ? consumer - producer - 1 :
			(RX_END - producer) + (consumer - RX_START) - 1;
	if (words == 0 || words > free_words)
		return false;
	auto put = [&](u16 value) {
		m_ram[producer] = value;
		if (++producer == RX_END)
			producer = RX_START;
	};
	put((payload_length << 8) | type);
	for (unsigned i = 0; i < payload_length; i += 2)
		put((u16(payload[i]) << 8) | (i + 1 < payload_length ? payload[i + 1] : 0));
	peer_shared_w(RX_PRODUCER, producer);
	if (m_trace_enabled)
		LOGMASKED(LOG_DSPIF, "dspif_transport: RX enqueue type=%02x payload=%u producer=%03x data=%s t=%.6f\n",
				type, payload_length, producer, packet_hex(payload, payload_length).c_str(),
				machine().time().as_double());
	return true;
}

void nokia_dspif_device::notify_rx()
{
	if (m_trace_enabled)
		LOGMASKED(LOG_DSPIF, "dspif_transport: FIQ0 notify producer=%03x consumer=%03x t=%.6f\n",
				m_ram[RX_PRODUCER], m_ram[RX_CONSUMER], machine().time().as_double());
	m_fiq0_cb(1);
	m_fiq0_cb(0);
}

u16 nokia_dspif_device::service_pending() const
{
	return m_ram[SVC_PENDING];
}

void nokia_dspif_device::complete_service()
{
	peer_shared_w(SVC_PENDING, 0);
	if (m_trace_enabled)
		LOGMASKED(LOG_DSPIF, "dspif_transport: IRQ4 service-complete request=%04x t=%.6f\n",
				m_ram[0x0e2 / 2], machine().time().as_double());
	m_service_irq_cb(1);
	m_service_irq_cb(0);
}

u8 nokia_dspif_device::run_conformance_checks()
{
	std::array<u16, 0x800> saved;
	std::copy(std::begin(m_ram), std::end(m_ram), saved.begin());
	u8 result = 0;

	// A three-word packet starting at the final ring word must wrap cleanly.
	m_ram[RX_PRODUCER] = RX_END - 1;
	m_ram[RX_CONSUMER] = RX_START + 4;
	const u8 wrap_payload[4] = { 0x12, 0x34, 0x56, 0x78 };
	if (enqueue_rx_packet(0x8e, wrap_payload, std::size(wrap_payload)) &&
			m_ram[RX_PRODUCER] == RX_START + 2 &&
			m_ram[RX_END - 1] == 0x048e && m_ram[RX_START] == 0x1234 &&
			m_ram[RX_START + 1] == 0x5678)
		result |= 0x01;

	// One-word producer/consumer separation leaves no usable slot.
	m_ram[RX_PRODUCER] = RX_START;
	m_ram[RX_CONSUMER] = RX_START + 1;
	if (!enqueue_rx_packet(0x8e, wrap_payload, 2) && m_ram[RX_PRODUCER] == RX_START)
		result |= 0x02;

	// A committed header without all payload words is not consumable.
	m_ram[TX_CONSUMER] = 0;
	m_ram[TX_PRODUCER] = 1;
	m_ram[0] = 0x0470;
	packet partial;
	if (!peek_tx_packet(partial) && m_ram[TX_CONSUMER] == 0)
		result |= 0x04;

	std::copy(saved.begin(), saved.end(), std::begin(m_ram));
	return result;
}
