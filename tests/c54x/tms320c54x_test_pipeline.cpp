// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {















tms320c54x_test_state::repeated_writa_fixture tms320c54x_test_state::repeated_writa_case(unsigned variant, bool standard_width)
{
	repeated_writa_fixture f{};
	f.standard_width = standard_width;
	if (standard_width) variant = variant % 4800 + (variant / 4800) * 9600;
	f.form = variant % 5; f.setup = (variant / 5) % 5; f.reg = (variant / 25) % 8;
	f.count = std::array<unsigned, 4>{0, 1, 7, 255}[(variant / 200) % 4];
	f.target = (standard_width ? std::array<u32, 3>{0x3200, 0x7ffe, 0xfffe} : std::array<u32, 3>{0x033200, 0x037ffe, 0x7ffffe})[(variant / 800) % 3];
	f.mapped = (variant / 2400) % 2; f.page = (variant / 4800) % 2 ? 0x20000 : 0;
	f.words = f.setup == 0 || f.setup == 2 ? 1 : 2;
	f.read = 5 + f.words + (f.setup >= 2 ? 2 : 0); f.write = f.read + 5;
	f.checkpoint = std::array<unsigned, 4>{f.read, f.read + 1, f.write, f.write + 3}[variant / 9600];
	f.source = f.form < 2 ? 0x1201 : 0x3000; f.direction = f.form == 3 ? -1 : f.form == 4 ? 1 : 0;
	f.count_address = f.setup == 2 ? 0x1202 : f.setup == 3 ? 0x1400 : 0x10 + ((f.reg + 1) % 8);
	return f;
}

u16 tms320c54x_test_state::repeated_writa_word(unsigned reg, unsigned index)
{ return 0x2468 ^ (reg * 0x1111) ^ (index * 0x0101); }

u32 tms320c54x_test_state::repeated_writa_destination(repeated_writa_fixture const &f, unsigned index)
{
	if (f.standard_width) return (f.target + index) & 0xffff;
	u32 const logical = (f.target + index) & 0x7fffff;
	return f.mapped && !(logical & 0x8000) ? logical & 0xffff : logical;
}














tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_dld_case(unsigned v)
{
	auto f = delay_load_case(v);
	bool const odd = (v / 192) % 2 == 0;
	f.address = odd ? 0x5007 : 0x5004;
	f.literal = f.form == 3 ? f.address : odd ? 2 : 0xffff;
	if (f.form == 1 || f.form == 2) f.final[f.selected] = f.address;
	f.st1 = (f.st1 & ~u16(0x200)) | (f.flags & 2 ? 0x80 : 0);
	f.memory_low = std::array<u16, 4>{0x1234, 0xffff, 0, 0x8001}[(v / 768) % 4];
	f.result = (u64(f.memory) << 16) | f.memory_low;
	if ((f.flags & 1) && (f.memory & 0x8000)) f.result |= 0xff00000000ULL;
	return f;
}

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

tms320c54x_test_state::block_back_fixture tms320c54x_test_state::block_back_case(unsigned v)
{
	block_back_fixture f{};
	f.outer = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; f.inner = std::array<unsigned, 4>{1, 2, 4, 8}[(v / 4) % 4];
	f.kind = (v / 16) % 2; f.b = (v / 32) % 2; f.base = (v / 64) % 2 ? 0x20000 : 0; f.interrupt = (v / 128) % 3;
	f.checkpoint = std::array<unsigned, 4>{9, 12, 15, 19}[v / 384]; f.end = f.kind ? 0x60d : 0x60a; f.st1 = f.interrupt ? 0x100 : 0x900;
	return f;
}











} // namespace c54x_test
