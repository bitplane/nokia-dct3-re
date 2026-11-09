// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_RF_LAB_SIGNAL_H
#define MAME_NOKIA_NOKIA_RF_LAB_SIGNAL_H
#include <cstdint>
#include <array>
#include <cmath>
#include <stdexcept>

namespace nokia_rf_lab {
// GSM 05.02 5.2.4 and table 3: 148 zero bits in TS0, frames 0/10/20/30/40.
// Ideal rectangular ADC-pin stimulus only, not a BTS power envelope or air link.
constexpr bool fcch_active(std::uint64_t reference_tick)
{
	unsigned const frame = (reference_tick / 60'000) % 51;
	return frame <= 40 && frame % 10 == 0 && reference_tick % 60'000 < 148 * 48;
}

constexpr std::uint32_t hyperframe = 26 * 51 * 2048;
constexpr bool sch_active(std::uint64_t reference_tick)
{
	unsigned const frame = (reference_tick / 60'000) % 51;
	return frame <= 41 && frame % 10 == 1 && reference_tick % 60'000 < 148 * 48;
}

using sch_iq = std::array<std::array<std::int16_t, 2>, 148 * 4>;

// Ideal BT=0.3 GMSK source, not a receiver or an RF power-envelope model.
// Four waveform samples/symbol cover both possible quarter-bit I phases
// of the two-complex-samples/symbol MFI experiment.
inline std::array<unsigned, 148> sch_bits(std::uint32_t frame, unsigned bsic)
{
	if (frame >= hyperframe || frame % 51 > 41 || frame % 51 % 10 != 1 || bsic >= 64)
		throw std::invalid_argument("SCH frame/BSIC outside the declared laboratory schedule");
	unsigned const t1 = frame / (26 * 51), t2 = frame % 26, t3 = (frame % 51 - 1) / 10;
	std::array<unsigned, 4> const octets{(bsic << 2) | (t1 >> 9), (t1 >> 1) & 255,
		((t1 & 1) << 7) | (t2 << 2) | (t3 >> 1), t3 & 1};
	std::array<unsigned, 39> data{};
	std::uint64_t polynomial = 0;
	for (unsigned bit = 0; bit < 25; ++bit)
	{
		data[bit] = (octets[bit / 8] >> (bit % 8)) & 1;
		polynomial = (polynomial << 1) | data[bit];
	}
	polynomial <<= 10;
	for (int shift = 24; shift >= 0; --shift)
		if (polynomial & (std::uint64_t(1) << (10 + shift))) polynomial ^= std::uint64_t(0x575) << shift;
	unsigned const parity = unsigned(polynomial) ^ 0x3ff;
	for (unsigned bit = 0; bit < 10; ++bit) data[25 + bit] = (parity >> (9 - bit)) & 1;
	std::array<unsigned, 78> coded{};
	for (unsigned bit = 0; bit < 39; ++bit)
	{
		unsigned const third = bit >= 3 ? data[bit - 3] : 0, fourth = bit >= 4 ? data[bit - 4] : 0;
		coded[2 * bit] = data[bit] ^ third ^ fourth;
		coded[2 * bit + 1] = coded[2 * bit] ^ (bit ? data[bit - 1] : 0);
	}
	std::array<unsigned, 148> burst{};
	for (unsigned bit = 0; bit < 39; ++bit) { burst[3 + bit] = coded[bit]; burst[106 + bit] = coded[39 + bit]; }
	for (unsigned bit = 0; bit < 64; ++bit) burst[42 + bit] = (UINT64_C(0xb962040f2d45761b) >> (63 - bit)) & 1;
	return burst;
}

inline sch_iq gmsk_samples(std::array<unsigned, 148> const &burst)
{
	std::array<unsigned, 149> changes{};
	unsigned count = 0, previous = 1;
	for (unsigned bit = 0; bit <= 148; ++bit)
	{
		unsigned const current = bit < 148 ? burst[bit] : 1;
		if (current != previous) changes[count++] = bit;
		previous = current;
	}
	constexpr double pi = 3.14159265358979323846;
	double const sigma = std::sqrt(std::log(2.0)) / (2 * pi * 0.3);
	auto const primitive = [sigma](double x) {
		return x * (1 + std::erf(x / (sigma * std::sqrt(2.0)))) / 2 +
			sigma / std::sqrt(2 * pi) * std::exp(-x * x / (2 * sigma * sigma));
	};
	std::array<double, 1185> pulse{};
	for (int at = -592; at <= 592; ++at) pulse[at + 592] = primitive(at / 4.0 + 0.5) - primitive(at / 4.0 - 0.5);
	double origin = 0;
	for (unsigned bit = 0; bit < count; ++bit) origin += pulse[592 - 4 * changes[bit]];
	sch_iq result{};
	for (unsigned sample = 0; sample < result.size(); ++sample)
	{
		double correction = 0;
		for (unsigned bit = 0; bit < count; ++bit) correction += pulse[592 + sample - 4 * changes[bit]];
		double const phase = pi * sample / 8 - pi * (correction - origin);
		result[sample] = {std::int16_t(std::lround(1024 * std::cos(phase))), std::int16_t(std::lround(1024 * std::sin(phase)))};
	}
	return result;
}

// GSM 05.02 5.2.6 dummy burst, transcribed from OsmoTRX GSM::gDummyBurst.
// A BCCH carrier fills otherwise idle timeslots with it at constant power.
inline std::array<unsigned, 148> dummy_bits()
{
	static constexpr char bits[] = "0001111101101110110000010100100111000001001000100000001111100011100010111000101110001010111010010100011001100111001111010011111000100101111101010000";
	std::array<unsigned, 148> burst{};
	for (unsigned bit = 0; bit < 148; ++bit) burst[bit] = bits[bit] - '0';
	return burst;
}

inline sch_iq sch_samples(std::uint32_t frame, unsigned bsic)
{
	return gmsk_samples(sch_bits(frame, bsic));
}
}
#endif // MAME_NOKIA_NOKIA_RF_LAB_SIGNAL_H
