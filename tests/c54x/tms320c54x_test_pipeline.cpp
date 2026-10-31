// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {















tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_add_case(unsigned v)
{
	delay_load_fixture f{};
	f.kind = std::array<unsigned, 4>{0, 3, 4, 5}[v % 4]; v /= 4;
	f.form = v % 6; v /= 6; unsigned const operand = v % 4; v /= 4;
	unsigned const initial = v % 4; v /= 4; f.destination = v % 2; v /= 2; f.flags = v % 4; v /= 4;
	f.caller = v % 2 ? 0x20000 : 0; v /= 2; f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
	f.address = f.literal = std::array<u16, 6>{0x500, 0x13, 8, 9, 0x0b, 0x0c}[f.form];
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[operand];
	u64 const left = std::array<u64, 4>{0, 0x7fffffff, 0xff80000000ULL, 0xffffffffffULL}[initial];
	f.a = f.destination ? 0x123456789aULL : left; f.b = f.destination ? left : 0xfedcba9876ULL;
	if (f.address == 8 || f.address == 9) { unsigned const shift = f.address == 9 ? 16 : 0; f.a = (f.a & ~(0xffffULL << shift)) | (u64(f.memory) << shift); }
	if (f.address == 0x0b || f.address == 0x0c) { unsigned const shift = f.address == 0x0c ? 16 : 0; f.b = (f.b & ~(0xffffULL << shift)) | (u64(f.memory) << shift); }
	f.st0 = 0xa008 | (operand & 1 ? 0x600 : 0) | (initial & 1 ? 0x800 : 0);
	f.st1 = 0x800 | (f.flags & 1 ? 0x100 : 0) | (f.flags & 2 ? 0x200 : 0);
	u64 const source = f.destination ? f.b : f.a;
	s64 const addend = f.flags & 1 ? s16(f.memory) : f.memory;
	s64 const sum = (source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source)) + addend;
	bool const overflow = sum > 0x7fffffffLL || sum < -0x80000000LL;
	bool const carry = u64(u32(source)) + u32(addend) > 0xffffffffULL;
	f.result = overflow && (f.flags & 2) ? (sum < 0 ? 0xff80000000ULL : 0x7fffffffULL) : u64(sum) & 0xffffffffffULL;
	f.final_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x5100 + i;
	if (f.address == 0x13) f.initial[3] = f.memory;
	f.final = f.initial; f.read = delay_multiply_fixture::operand_read(f.kind, 1);
	f.checkpoint = std::array<unsigned, 3>{f.literal_clock(), f.read, f.read + 1}[v];
	return f;
}

} // namespace c54x_test
