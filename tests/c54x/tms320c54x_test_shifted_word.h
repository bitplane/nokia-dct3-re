// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_SHIFTED_WORD_H
#define C54X_TEST_SHIFTED_WORD_H
namespace c54x_test {
// Mathematical floor division, not the CPU's repeated arithmetic-shift loop.
inline u64 shifted_word40(u16 word, unsigned code, bool sxm)
{
	code &= 63;
	int const shift = code >= 32 ? int(code) - 64 : int(code);
	s64 const number = sxm ? s64(s16(word)) : s64(word);
	u64 const bits = u64(number) & 0xffffffffffULL;
	if (shift >= 0) return (bits << shift) & 0xffffffffffULL;
	s64 const divisor = s64(1) << -shift;
	return sxm ? u64(number / divisor - (number < 0 && number % divisor != 0)) & 0xffffffffffULL : bits >> -shift;
}
}
#endif
