// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 5; ++form)
	for (unsigned reg = 0; reg != (form < 2 ? 1U : 8U); ++reg)
	for (unsigned count : {0U,1U,2U,7U,255U})
	for (unsigned prefix : {1U,2U})
	for (u16 destination : {0x2e00,0xfffc})
	for (bool mapped : {false,true}) {
		u32 const base = (form + reg + count + prefix + mapped) % 2 ? 0x20000 : 0;
		auto physical = [base,mapped](u16 address) { return mapped && address < 0x8000 ? u32(address) : base | address; };
		u16 const opcode = 0x7d00 | (form < 2 ? 3 : (form == 2 ? 0x80 : form == 3 ? 0x90 : 0x88) | reg);
		u16 const st1 = form == 1 ? 0x4800 : 0x800;
		auto initial = initial_case("repeated-MVDP-page-wrap-and-overlay", {0xf495});
		initial.program = {{physical(0x600),u16(prefix == 2 ? 0xf070 : 0xec00 | count)},
			{physical(0x600 + prefix),opcode},{physical(0x601 + prefix),destination},{physical(0x602 + prefix),0xf4e1}};
		if (prefix == 2) initial.program.push_back({physical(0x601),u16(count)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa624},
			{r::STATE_ST1,st1},{r::STATE_PMST,mapped ? 0x20U : 0U},{r::STATE_SP,0x1200},{r::STATE_BK,16}});
		unsigned const finish = count + 64;
		auto final = observation("count-plus-one-transfers-and-continuation", {{STATE_GENPC,base + 0x603 + prefix},
			{r::STATE_ST0,0xa624},{r::STATE_ST1,st1},{r::STATE_PMST,mapped ? 0x20U : 0U},
			{r::STATE_SP,0x1200},{r::STATE_BK,16},{r::STATE_RC,0}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),ar == reg ? 0x1230U : 0x3000U + ar});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar != reg ? 0x3000U + ar :
				u16(form == 3 ? 0x1230 + count + 1 : form == 4 ? 0x1230 - count - 1 : 0x1230)});
		}
		std::vector<clocked_data_event> reads, writes;
		for (unsigned i = 0; i <= count; ++i) {
			u16 const source = form < 2 ? 0x1203 : form == 3 ? 0x1230 + i : form == 4 ? 0x1230 - i : 0x1230;
			u16 const word = 0x2468 + (form >= 3 ? i : 0);
			if (!i || form >= 3) initial.data.push_back({source,word});
			u32 const target = physical(u16(destination + i));
			initial.program.push_back({target,0xbeef});
			final.expected_program.push_back({target,word});
			reads.push_back({false,source,word,0,false});
			writes.push_back({true,target,word,9 + prefix + i});
		}
		unsigned const checkpoint = std::array<unsigned,3>{5,8,12}[(form + reg + count + prefix + mapped) % 3];
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},checkpoint,finish};
		c.data_events = std::move(reads); c.program_events = std::move(writes);
		cases.push_back(std::move(c));
	}
	return cases;
}
}
