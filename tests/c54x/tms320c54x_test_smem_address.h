// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_SMEM_ADDRESS_H
#define C54X_TEST_SMEM_ADDRESS_H

namespace c54x_test {
struct smem_address_example
{
	unsigned selected, read;
	u16 mode, bk, address, next, literal;
};

// Fixed AR=0x5007, DP=4, SP=0x1000 and CMPT ARP=3 fixtures.
// Signed modulo is independent of the core's mask/iterative wrap implementation.
inline smem_address_example smem_address(unsigned form, unsigned reg, bool cmpt, unsigned pattern, bool sxm)
{
	unsigned const selected = cmpt && !reg ? 3 : reg;
	u16 const mode = form < 2 ? reg * 16 + pattern + (sxm ? 8 : 0) :
		form == 17 ? 0xf8 : 0x80 | ((form - 2) << 3) | reg;
	s16 const offset = std::array<s16,8>{0,1,-1,17,s16(0xf495),0x7fff,s16(0x8000),2}[pattern];
	u16 const bk = form == 11 || form == 13 || form == 16 ? 0x8000 : 16;
	auto circular = [&](int delta) {
		int const alignment = bk == 16 ? 32 : 65536, residue = (0x5007 % alignment + delta) % int(bk);
		return u16(0x5007 / alignment * alignment + (residue < 0 ? residue + bk : residue));
	};
	u16 const address = form == 0 ? 0x200 + mode : form == 1 ? 0x1000 + mode :
		form == 16 ? circular(offset) : form >= 14 && form < 17 ? u16(0x5007 + offset) :
		form == 17 ? u16(0x1500 + offset) : 0x5007;
	u16 const next = form < 2 || form == 17 || form == 14 ? 0x5007 : form >= 15 ? address :
		form == 11 || form == 13 ? circular(form == 11 ? -0x5007 : 0x5007) :
		std::array<u16,12>{0x5007,0x5006,0x5008,0x5008,0,0,0xa00e,0x2803,0x5006,0x5000,0x5008,0x500e}[form - 2];
	return {selected,form >= 14 ? 6U : 5U,mode,bk,address,next,form == 17 ? address : u16(offset)};
}
}
#endif
