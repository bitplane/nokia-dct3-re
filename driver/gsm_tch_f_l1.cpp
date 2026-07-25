// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_tch_f_l1.h"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace gsm::tch_f
{
namespace
{

constexpr std::array<unsigned, 76> PARAMETER_WIDTHS = {
	6, 6, 5, 5, 4, 4, 3, 3,
	7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

unsigned serial_index(unsigned parameter, unsigned value_bit)
{
	unsigned result = 0;
	for (unsigned p = 1; p < parameter; ++p)
		result += PARAMETER_WIDTHS[p - 1];
	return result + PARAMETER_WIDTHS[parameter - 1] - 1 - value_bit;
}

std::array<unsigned, 260> make_importance_map()
{
	std::array<unsigned, 260> map{};
	unsigned out = 0;
	auto one = [&](unsigned parameter, unsigned value_bit)
	{
		map[out++] = serial_index(parameter, value_bit);
	};
	auto each = [&](std::initializer_list<unsigned> parameters, unsigned value_bit)
	{
		for (unsigned parameter : parameters)
			one(parameter, value_bit);
	};
	auto range = [&](unsigned first, unsigned last, unsigned value_bit)
	{
		for (unsigned parameter = first; parameter <= last; ++parameter)
			one(parameter, value_bit);
	};

	// TS 45.003 table 2, expanded in printed order.
	one(1, 5);
	each({12, 29, 46, 63}, 5);
	one(1, 4);
	one(2, 5);
	one(3, 4);
	one(1, 3);
	one(2, 4);
	one(3, 3);
	one(4, 4);
	each({9, 26, 43, 60}, 6);
	each({12, 29, 46, 63}, 4);
	each({2, 5, 6}, 3);
	each({9, 26, 43, 60}, 5);
	each({9, 26, 43, 60}, 4);
	each({9, 26, 43, 60}, 3);
	each({9, 26, 43, 60}, 2);
	each({12, 29, 46, 63}, 3);
	one(1, 2);
	one(4, 3);
	one(7, 2);
	each({9, 26, 43, 60}, 1);
	each({5, 6}, 2);
	each({10, 27, 44, 61}, 1);
	each({9, 26, 43, 60}, 0);
	each({11, 28, 45, 62}, 1);
	one(1, 1);
	each({2, 3, 8, 4}, 2);
	each({5, 7}, 1);
	each({10, 27, 44, 61}, 0);
	each({12, 29, 46, 63}, 2);
	range(13, 25, 2);
	range(30, 42, 2);
	range(47, 59, 2);
	range(64, 76, 2);
	each({11, 28, 45, 62}, 0);
	each({12, 29, 46, 63}, 1);
	range(13, 25, 1);
	range(30, 42, 1);
	range(47, 59, 1);
	range(64, 67, 1);
	range(68, 76, 1);
	one(1, 0);
	each({2, 3, 6}, 1);
	one(7, 0);
	one(8, 1);
	each({8, 3}, 0);
	one(4, 1);
	each({4, 5}, 0);
	each({12, 29, 46, 63}, 0);
	range(13, 25, 0);
	range(30, 42, 0);
	range(47, 59, 0);
	range(64, 76, 0);
	each({2, 6}, 0);

	return map;
}

const std::array<unsigned, 260> IMPORTANCE_MAP = make_importance_map();

unsigned polynomial_remainder(const std::array<bit, 53> &word)
{
	// Long division by D^3 + D + 1, with word[0] as the D^52 term.
	std::array<bit, 53> work = word;
	for (unsigned i = 0; i < 50; ++i)
	{
		if (!work[i])
			continue;
		work[i] ^= 1;
		work[i + 2] ^= 1;
		work[i + 3] ^= 1;
	}
	return (unsigned(work[50]) << 2) |
			(unsigned(work[51]) << 1) | unsigned(work[52]);
}

std::array<bit, 3> parity_for(const speech_bits &d)
{
	std::array<bit, 53> word{};
	std::copy_n(d.begin(), 50, word.begin());
	for (unsigned candidate = 0; candidate < 8; ++candidate)
	{
		word[50] = (candidate >> 2) & 1;
		word[51] = (candidate >> 1) & 1;
		word[52] = candidate & 1;
		if (polynomial_remainder(word) == 7)
			return {word[50], word[51], word[52]};
	}
	return {};
}

std::array<bit, 189> make_uncoded(const speech_bits &d)
{
	std::array<bit, 189> u{};
	for (unsigned k = 0; k <= 90; ++k)
	{
		u[k] = d[2 * k];
		u[184 - k] = d[2 * k + 1];
	}
	const auto parity = parity_for(d);
	std::copy(parity.begin(), parity.end(), u.begin() + 91);
	// u(185)..u(188) remain zero tail bits.
	return u;
}

template <std::size_t N>
std::array<bit, 2 * N> convolutional_encode(const std::array<bit, N> &u)
{
	std::array<bit, 2 * N> result{};
	auto prior = [&](int k) -> bit { return k < 0 ? 0 : u[unsigned(k)]; };
	for (int k = 0; k < int(N); ++k)
	{
		result[2 * k] = prior(k) ^ prior(k - 3) ^ prior(k - 4);
		result[2 * k + 1] =
				prior(k) ^ prior(k - 1) ^ prior(k - 3) ^ prior(k - 4);
	}
	return result;
}

template <std::size_t N>
struct viterbi_result
{
	std::array<bit, N> bits{};
	unsigned distance = 0;
};

template <std::size_t N>
viterbi_result<N> convolutional_decode(const coded_block &coded)
{
	constexpr unsigned states = 16;
	constexpr unsigned infinity = std::numeric_limits<unsigned>::max() / 4;
	std::array<unsigned, states> metric{};
	metric.fill(infinity);
	metric[0] = 0;
	std::array<std::array<std::uint8_t, states>, N> previous{};
	std::array<std::array<bit, states>, N> decision{};

	for (unsigned k = 0; k < N; ++k)
	{
		std::array<unsigned, states> next{};
		next.fill(infinity);
		for (unsigned state = 0; state < states; ++state)
		{
			if (metric[state] == infinity)
				continue;
			for (bit input = 0; input <= 1; ++input)
			{
				const bit u1 = (state >> 0) & 1;
				const bit u3 = (state >> 2) & 1;
				const bit u4 = (state >> 3) & 1;
				const bit c0 = input ^ u3 ^ u4;
				const bit c1 = input ^ u1 ^ u3 ^ u4;
				const unsigned successor = ((state << 1) | input) & 15;
				const unsigned branch =
						(c0 != coded[2 * k]) + (c1 != coded[2 * k + 1]);
				if (metric[state] + branch < next[successor])
				{
					next[successor] = metric[state] + branch;
					previous[k][successor] = std::uint8_t(state);
					decision[k][successor] = input;
				}
			}
		}
		metric = next;
	}

	viterbi_result<N> result{};
	// Four zero tail bits require the all-zero final state.
	result.distance = metric[0];
	unsigned state = 0;
	for (int k = int(N) - 1; k >= 0; --k)
	{
		result.bits[unsigned(k)] = decision[unsigned(k)][state];
		state = previous[unsigned(k)][state];
	}
	return result;
}

std::array<bit, 40> fire_parity(const control_bits &information)
{
	// g(D) = (D^23 + 1)(D^17 + D^3 + 1)
	//      = D^40 + D^26 + D^23 + D^17 + D^3 + 1.
	std::array<bit, 224> work{};
	std::copy(information.begin(), information.end(), work.begin());
	for (unsigned i = 0; i < information.size(); ++i)
	{
		if (!work[i])
			continue;
		work[i] ^= 1;
		work[i + 14] ^= 1;
		work[i + 17] ^= 1;
		work[i + 23] ^= 1;
		work[i + 37] ^= 1;
		work[i + 40] ^= 1;
	}
	std::array<bit, 40> parity{};
	for (unsigned k = 0; k < parity.size(); ++k)
		parity[k] = work[184 + k] ^ 1;
	return parity;
}

bool fire_check(const std::array<bit, 224> &word)
{
	std::array<bit, 224> work = word;
	for (unsigned i = 0; i < 184; ++i)
	{
		if (!work[i])
			continue;
		work[i] ^= 1;
		work[i + 14] ^= 1;
		work[i + 17] ^= 1;
		work[i + 23] ^= 1;
		work[i + 37] ^= 1;
		work[i + 40] ^= 1;
	}
	return std::all_of(
			work.begin() + 184, work.end(),
			[](bit value) { return value == 1; });
}

} // anonymous namespace

bool unpack_speech(const packed_speech_frame &packed, speech_bits &serial)
{
	if ((packed[0] >> 4) != 0x0d)
		return false;
	for (unsigned k = 0; k < serial.size(); ++k)
	{
		const unsigned packed_bit = k + 4;
		serial[k] = (packed[packed_bit / 8] >> (7 - packed_bit % 8)) & 1;
	}
	return true;
}

packed_speech_frame pack_speech(const speech_bits &serial)
{
	packed_speech_frame packed{};
	packed[0] = 0xd0;
	for (unsigned k = 0; k < serial.size(); ++k)
	{
		const unsigned packed_bit = k + 4;
		packed[packed_bit / 8] |= serial[k] << (7 - packed_bit % 8);
	}
	return packed;
}

speech_bits importance_order(const speech_bits &serial)
{
	speech_bits result{};
	for (unsigned k = 0; k < result.size(); ++k)
		result[k] = serial[IMPORTANCE_MAP[k]];
	return result;
}

speech_bits serial_order(const speech_bits &importance)
{
	speech_bits result{};
	for (unsigned k = 0; k < result.size(); ++k)
		result[IMPORTANCE_MAP[k]] = importance[k];
	return result;
}

coded_block encode_speech(const packed_speech_frame &frame)
{
	speech_bits serial{};
	if (!unpack_speech(frame, serial))
		return {};
	const speech_bits d = importance_order(serial);
	const auto convolutional = convolutional_encode(make_uncoded(d));
	coded_block result{};
	std::copy(convolutional.begin(), convolutional.end(), result.begin());
	std::copy(d.begin() + 182, d.end(), result.begin() + 378);
	return result;
}

decoded_speech decode_speech(const coded_block &coded)
{
	const viterbi_result<189> decoded = convolutional_decode<189>(coded);
	speech_bits d{};
	for (unsigned k = 0; k <= 90; ++k)
	{
		d[2 * k] = decoded.bits[k];
		d[2 * k + 1] = decoded.bits[184 - k];
	}
	std::copy(coded.begin() + 378, coded.end(), d.begin() + 182);

	std::array<bit, 53> parity_word{};
	std::copy_n(d.begin(), 50, parity_word.begin());
	std::copy_n(decoded.bits.begin() + 91, 3, parity_word.begin() + 50);
	const bool parity_ok = polynomial_remainder(parity_word) == 7;
	const bool tail_ok = std::all_of(
			decoded.bits.begin() + 185, decoded.bits.end(),
			[](bit value) { return value == 0; });

	decoded_speech result{};
	result.frame = pack_speech(serial_order(d));
	result.good = parity_ok && tail_ok;
	result.corrected_bits = decoded.distance;
	return result;
}

coded_block encode_control(const control_bits &information)
{
	std::array<bit, 228> uncoded{};
	std::copy(information.begin(), information.end(), uncoded.begin());
	const auto parity = fire_parity(information);
	std::copy(parity.begin(), parity.end(), uncoded.begin() + 184);
	// uncoded[224..227] are the four zero tail bits.
	return convolutional_encode(uncoded);
}

decoded_control decode_control(const coded_block &coded)
{
	const viterbi_result<228> decoded = convolutional_decode<228>(coded);
	std::array<bit, 224> protected_word{};
	std::copy_n(decoded.bits.begin(), protected_word.size(), protected_word.begin());
	decoded_control result{};
	std::copy_n(decoded.bits.begin(), result.bits.size(), result.bits.begin());
	result.good = fire_check(protected_word) &&
			std::all_of(
				decoded.bits.begin() + 224, decoded.bits.end(),
				[](bit value) { return value == 0; });
	result.corrected_bits = decoded.distance;
	return result;
}

control_bits unpack_control(const packed_control_block &packed)
{
	control_bits result{};
	for (unsigned k = 0; k < result.size(); ++k)
		result[k] = (packed[k / 8] >> (7 - k % 8)) & 1;
	return result;
}

packed_control_block pack_control(const control_bits &bits)
{
	packed_control_block result{};
	for (unsigned k = 0; k < bits.size(); ++k)
		result[k / 8] |= bits[k] << (7 - k % 8);
	return result;
}

std::array<burst_payload, 4> interleave_sacch(const coded_block &coded)
{
	std::array<burst_payload, 4> result{};
	for (unsigned k = 0; k < coded.size(); ++k)
	{
		const unsigned burst = k % 4;
		const unsigned j = 2 * ((49 * k) % 57) + ((k % 8) / 4);
		result[burst].data[j] = coded[k];
	}
	for (auto &burst : result)
		burst.hl = burst.hu = 1;
	return result;
}

coded_block deinterleave_sacch(
		const std::array<burst_payload, 4> &bursts)
{
	coded_block result{};
	for (unsigned k = 0; k < result.size(); ++k)
	{
		const unsigned burst = k % 4;
		const unsigned j = 2 * ((49 * k) % 57) + ((k % 8) / 4);
		result[k] = bursts[burst].data[j];
	}
	return result;
}

std::array<burst_payload, 8> interleave(const coded_block &coded)
{
	std::array<burst_payload, 8> result{};
	for (unsigned k = 0; k < coded.size(); ++k)
	{
		const unsigned burst = k % 8;
		const unsigned j = 2 * ((49 * k) % 57) + ((k % 8) / 4);
		result[burst].data[j] = coded[k];
	}
	return result;
}

coded_block deinterleave(const std::array<burst_payload, 8> &bursts)
{
	coded_block result{};
	for (unsigned k = 0; k < result.size(); ++k)
	{
		const unsigned burst = k % 8;
		const unsigned j = 2 * ((49 * k) % 57) + ((k % 8) / 4);
		result[k] = bursts[burst].data[j];
	}
	return result;
}

normal_burst pack_normal_burst(
		const burst_payload &payload, const std::array<bit, 26> &training)
{
	normal_burst result{};
	std::copy_n(payload.data.begin(), 57, result.bits.begin() + 3);
	result.bits[60] = payload.hl;
	std::copy(training.begin(), training.end(), result.bits.begin() + 61);
	result.bits[87] = payload.hu;
	std::copy_n(payload.data.begin() + 57, 57, result.bits.begin() + 88);
	return result;
}

burst_payload unpack_normal_burst(const normal_burst &burst)
{
	burst_payload result{};
	std::copy_n(burst.bits.begin() + 3, 57, result.data.begin());
	result.hl = burst.bits[60];
	result.hu = burst.bits[87];
	std::copy_n(burst.bits.begin() + 88, 57, result.data.begin() + 57);
	return result;
}

std::array<bit, 26> training_sequence(unsigned tsc)
{
	// TS 45.002 table 5.2.3a, GMSK TSC set 1.
	static constexpr std::array<std::array<bit, 26>, 8> sequences = {{
		{{0,0,1,0,0,1,0,1,1,1,0,0,0,0,1,0,0,0,1,0,0,1,0,1,1,1}},
		{{0,0,1,0,1,1,0,1,1,1,0,1,1,1,1,0,0,0,1,0,1,1,0,1,1,1}},
		{{0,1,0,0,0,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,0,1,1,1,0}},
		{{0,1,0,0,0,1,1,1,1,0,1,1,0,1,0,0,0,1,0,0,0,1,1,1,1,0}},
		{{0,0,0,1,1,0,1,0,1,1,1,0,0,1,0,0,0,0,0,1,1,0,1,0,1,1}},
		{{0,1,0,0,1,1,1,0,1,0,1,1,0,0,0,0,0,1,0,0,1,1,1,0,1,0}},
		{{1,0,1,0,0,1,1,1,1,1,0,1,1,0,0,0,1,0,1,0,0,1,1,1,1,1}},
		{{1,1,1,0,1,1,1,1,0,0,0,1,0,0,1,0,1,1,1,0,1,1,1,1,0,0}}
	}};
	return sequences[tsc & 7];
}

void invert_data_bits(burst_payload &payload)
{
	for (bit &value : payload.data)
		value ^= 1;
}

burst_payload combine_diagonal(
		const std::array<burst_payload, 8> &old_block,
		const std::array<burst_payload, 8> &new_block, unsigned phase)
{
	burst_payload result{};
	const unsigned p = phase & 3;
	for (unsigned j = 0; j < 114; ++j)
		result.data[j] = old_block[4 + p].data[j] | new_block[p].data[j];
	result.hl = old_block[4 + p].hl;
	result.hu = new_block[p].hu;
	return result;
}

void mark_facch(std::array<burst_payload, 8> &bursts)
{
	for (unsigned k = 0; k < 4; ++k)
		bursts[k].hu = 1;
	for (unsigned k = 4; k < 8; ++k)
		bursts[k].hl = 1;
}

bool indicates_facch(const std::array<burst_payload, 8> &bursts)
{
	return std::all_of(
				bursts.begin(), bursts.begin() + 4,
				[](const burst_payload &burst) { return burst.hu != 0; }) &&
			std::all_of(
				bursts.begin() + 4, bursts.end(),
				[](const burst_payload &burst) { return burst.hl != 0; });
}

bool diagonal_transmitter::enqueue(const traffic_block &block)
{
	if (m_state.count == queue_depth)
		return false;
	m_state.queue[(m_state.head + m_state.count) % queue_depth] = block;
	++m_state.count;
	return true;
}

bool diagonal_transmitter::substitute_facch(const coded_block &coded)
{
	for (unsigned offset = 0; offset < m_state.count; ++offset)
	{
		traffic_block &queued =
				m_state.queue[(m_state.head + offset) % queue_depth];
		if (queued.kind == traffic_block_kind::speech)
		{
			queued.coded = coded;
			queued.kind = traffic_block_kind::facch;
			return true;
		}
	}
	return enqueue({coded, traffic_block_kind::facch});
}

burst_payload diagonal_transmitter::next_burst()
{
	if (m_state.phase == 0)
	{
		m_state.previous = m_state.current;
		m_state.current = {};
		if (m_state.count)
		{
			const traffic_block &block = m_state.queue[m_state.head];
			m_state.current = interleave(block.coded);
			if (block.kind == traffic_block_kind::facch)
				mark_facch(m_state.current);
			m_state.head = (m_state.head + 1) % queue_depth;
			--m_state.count;
		}
	}
	const burst_payload result =
			combine_diagonal(
				m_state.previous, m_state.current, m_state.phase);
	m_state.phase = (m_state.phase + 1) & 3;
	return result;
}

void diagonal_transmitter::reset()
{
	m_state = {};
}

std::optional<received_traffic_block> diagonal_receiver::receive(
		const burst_payload &burst)
{
	m_state.pending[4 + m_state.phase] = burst;
	m_state.incoming[m_state.phase] = burst;
	std::optional<received_traffic_block> result;
	if (m_state.phase == 3)
	{
		if (m_state.pending_valid)
		{
			received_traffic_block decoded{};
			if (indicates_facch(m_state.pending))
			{
				decoded.kind = traffic_block_kind::facch;
				decoded.control =
						decode_control(deinterleave(m_state.pending));
			}
			else
			{
				decoded.kind = traffic_block_kind::speech;
				decoded.speech =
						decode_speech(deinterleave(m_state.pending));
			}
			result = decoded;
		}
		m_state.pending = m_state.incoming;
		m_state.incoming = {};
		m_state.pending_valid = true;
	}
	m_state.phase = (m_state.phase + 1) & 3;
	return result;
}

void diagonal_receiver::reset()
{
	m_state = {};
}

bool sacch_transmitter::enqueue(const control_bits &information)
{
	if (m_state.pending)
		return false;
	m_state.bursts = interleave_sacch(encode_control(information));
	m_state.pending = true;
	m_state.phase = 0;
	return true;
}

std::optional<burst_payload> sacch_transmitter::next_burst(
		unsigned scheduled_phase)
{
	if (!m_state.pending || (scheduled_phase & 3) != m_state.phase)
		return std::nullopt;
	const burst_payload result = m_state.bursts[m_state.phase++];
	if (m_state.phase == 4)
	{
		m_state.phase = 0;
		m_state.pending = false;
	}
	return result;
}

void sacch_transmitter::reset()
{
	m_state = {};
}

std::optional<decoded_control> sacch_receiver::receive(
		const burst_payload &burst)
{
	m_state.bursts[m_state.phase++] = burst;
	if (m_state.phase != 4)
		return std::nullopt;
	m_state.phase = 0;
	return decode_control(deinterleave_sacch(m_state.bursts));
}

void sacch_receiver::reset()
{
	m_state = {};
}

tdma_slot_kind full_rate_slot(std::uint32_t frame_number, unsigned timeslot)
{
	const unsigned position = frame_number % 26;
	// SACCH/TF is displaced by 13 frames for odd timeslots. Table 1 gives
	// timeslot 0 at FN mod 104 = 12,38,64,90 and timeslot 1 at 25,51,77,103.
	const unsigned sacch_position = (timeslot & 1) ? 25 : 12;
	const unsigned idle_position = (sacch_position + 13) % 26;
	if (position == sacch_position)
		return tdma_slot_kind::sacch;
	if (position == idle_position)
		return tdma_slot_kind::idle;
	return tdma_slot_kind::traffic;
}

unsigned sacch_burst_index(std::uint32_t frame_number, unsigned timeslot)
{
	// TS 45.002 table 1 rotates the four SACCH/TF bursts by one multiframe
	// for each pair of timeslots: TS0/1 B(12/25,38/51,64/77,90/103),
	// TS2/3 start at the second entry, and so on.
	const unsigned multiframe = (frame_number / 26) & 3;
	return (multiframe + 4 - ((timeslot & 7) / 2)) & 3;
}

} // namespace gsm::tch_f
