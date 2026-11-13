// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_RF_LAB_TCH_H
#define MAME_NOKIA_NOKIA_RF_LAB_TCH_H
#include "gsm_normal_p27.h"
#include <functional>
#include <vector>

namespace nokia_rf_lab {
// One laboratory TCH/F + SACCH/TF as the external BTS sees it: TDMA frame
// numbers from its own clock, downlink burst bits out and uplink burst bits
// in. TS 45.002 table 1 schedules the slot; TS 45.003 codes the blocks.
class traffic_channel
{
public:
	using bits = std::array<unsigned,148>;
	using block = gsm::tch_f::packed_control_block;
	enum class uplink_kind { facch, sacch, speech, bad_speech, bad_control };
	struct uplink_event
	{
		uplink_kind kind;
		std::uint64_t frame; // first burst of the decoded block
		block octets{};
		gsm::tch_f::packed_speech_frame speech{};
	};

	// Traffic blocks start on bursts B0, B4 or B8 of each 12-frame half, so
	// both directions begin at the next block boundary at or after frame.
	void start(std::uint64_t frame,unsigned timeslot,unsigned tsc,std::array<block,2> const &sacch)
	{
		auto source=std::move(m_speech_source);
		*this=traffic_channel{};
		m_speech_source=std::move(source);
		m_timeslot=timeslot;m_tsc=tsc;m_active=true;
		for (unsigned kind=0;kind<2;++kind)
			m_sacch[kind]=gsm::tch_f::interleave_sacch(gsm::tch_f::encode_control(information(sacch[kind])));
		while (gsm::tch_f::full_rate_slot(frame,timeslot)!=gsm::tch_f::tdma_slot_kind::traffic || traffic_index(frame)%4) ++frame;
		m_first=m_downlink_next=m_uplink_next=frame;
	}
	void stop() { m_active=false; }
	bool active() const { return m_active; }
	std::uint64_t first_frame() const { return m_first; }
	void set_speech_source(std::function<gsm::tch_f::packed_speech_frame()> source) { m_speech_source=std::move(source); }

	// FACCH/F steals the next speech block that has not started transmission.
	void send_facch(block const &octets)
	{
		m_transmitter.substitute_facch(gsm::tch_f::encode_control(information(octets)));
	}

	// Bits for this frame's burst, or nothing for the idle frame and for
	// frames before the first block. Frames may be skipped but not revisited.
	std::optional<bits> downlink(std::uint64_t frame)
	{
		if (!m_active || frame<m_first) return std::nullopt;
		if (frame+1==m_downlink_next) return m_downlink_last;
		for (;m_downlink_next<=frame;++m_downlink_next)
		{
			std::uint64_t const at=m_downlink_next;
			std::optional<gsm::tch_f::burst_payload> payload;
			switch (gsm::tch_f::full_rate_slot(at,m_timeslot))
			{
			case gsm::tch_f::tdma_slot_kind::traffic:
				if (!m_transmitter.snapshot().phase && !m_transmitter.snapshot().count && m_speech_source)
					m_transmitter.enqueue({gsm::tch_f::encode_speech(m_speech_source()),gsm::tch_f::traffic_block_kind::speech});
				payload=m_transmitter.next_burst();
				break;
			case gsm::tch_f::tdma_slot_kind::sacch:
				payload=m_sacch[sacch_block(at)&1][gsm::tch_f::sacch_burst_index(at,m_timeslot)];
				break;
			case gsm::tch_f::tdma_slot_kind::idle:
				break;
			}
			m_downlink_last.reset();
			if (payload)
			{
				auto const packed=gsm::tch_f::pack_normal_burst(*payload,gsm::tch_f::training_sequence(m_tsc));
				m_downlink_last.emplace();
				std::copy(packed.bits.begin(),packed.bits.end(),m_downlink_last->begin());
			}
		}
		return m_downlink_last;
	}

	// One uplink burst, or nothing when the handset did not transmit. Frames
	// skipped since the last call carry no energy into the deinterleavers.
	std::vector<uplink_event> uplink(std::uint64_t frame,std::optional<gsm::tch_f::burst_payload> const &burst)
	{
		std::vector<uplink_event> events;
		if (!m_active || frame<m_uplink_next) return events;
		for (;m_uplink_next<=frame;++m_uplink_next)
		{
			std::uint64_t const at=m_uplink_next;
			auto const payload=at==frame && burst ? *burst : gsm::tch_f::burst_payload{};
			switch (gsm::tch_f::full_rate_slot(at,m_timeslot))
			{
			case gsm::tch_f::tdma_slot_kind::traffic:
				if (auto const received=m_receiver.receive(payload))
				{
					std::uint64_t const start=m_traffic_frames[(m_traffic_count+1)%8];
					if (received->kind==gsm::tch_f::traffic_block_kind::facch)
					{
						bool const good=received->control.good && !received->control.corrected_bits;
						events.push_back({good ? uplink_kind::facch : uplink_kind::bad_control,start,
							gsm::normal::control_octets(received->control.bits),{}});
					}
					else
						events.push_back({received->speech.good ? uplink_kind::speech : uplink_kind::bad_speech,start,{},received->speech.frame});
				}
				m_traffic_frames[m_traffic_count++%8]=at;
				break;
			case gsm::tch_f::tdma_slot_kind::sacch:
			{
				unsigned const index=gsm::tch_f::sacch_burst_index(at,m_timeslot);
				if (!index) { m_sacch_mask=0;m_sacch_start=at; }
				m_sacch_bursts[index]=payload;
				m_sacch_mask|=1U<<index;
				if (index==3 && m_sacch_mask==15)
				{
					auto const decoded=gsm::normal::decode_control(m_sacch_bursts);
					events.push_back({decoded ? uplink_kind::sacch : uplink_kind::bad_control,m_sacch_start,decoded.value_or(block{}),{}});
				}
				break;
			}
			case gsm::tch_f::tdma_slot_kind::idle:
				break;
			}
		}
		return events;
	}

private:
	static gsm::tch_f::control_bits information(block const &octets)
	{
		gsm::tch_f::control_bits result{};
		for (unsigned bit=0;bit<result.size();++bit) result[bit]=(octets[bit/8]>>(bit%8))&1;
		return result;
	}
	static unsigned traffic_index(std::uint64_t frame)
	{
		unsigned const position=frame%26;
		return position<12 ? position : position-1;
	}
	std::uint64_t sacch_block(std::uint64_t frame) const
	{
		return (frame/26-gsm::tch_f::sacch_burst_index(frame,m_timeslot))/4;
	}

	bool m_active=false;
	unsigned m_timeslot=1,m_tsc=0;
	std::uint64_t m_first=0,m_downlink_next=0,m_uplink_next=0;
	std::array<std::array<gsm::tch_f::burst_payload,4>,2> m_sacch{};
	gsm::tch_f::diagonal_transmitter m_transmitter;
	gsm::tch_f::diagonal_receiver m_receiver;
	std::function<gsm::tch_f::packed_speech_frame()> m_speech_source;
	std::optional<bits> m_downlink_last;
	std::array<std::uint64_t,8> m_traffic_frames{};
	unsigned m_traffic_count=0;
	std::array<gsm::tch_f::burst_payload,4> m_sacch_bursts{};
	unsigned m_sacch_mask=0;
	std::uint64_t m_sacch_start=0;
};
}
#endif // MAME_NOKIA_NOKIA_RF_LAB_TCH_H
