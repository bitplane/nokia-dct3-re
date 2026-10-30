// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<std::array<u16,2>,8> pairs{{{0,2},{0x7fff,1},{0x8007,0xfff8},{0xffff,1},
		{0x8000,0x8000},{0x7fff,0x7fff},{0xffff,0xffff},{0x1234,0xffff}}};
	auto add = [&](unsigned form, unsigned offset_index, bool cmpt, unsigned operation, unsigned pair, unsigned flags,
		int window = -1, bool follow = false) {
		unsigned const mode = form == 24 ? 3 : form/8, reg = form == 24 ? 0 : form%8, selected = cmpt && !reg ? 3 : reg;
		u16 const literal = window < 0 ? mode == 3 ? u16(0x1500+std::array<s16,4>{0,1,-1,s16(0xf495)}[offset_index]) :
			u16(std::array<s16,4>{0,1,-1,s16(0xf495)}[offset_index]) : std::array<u16,3>{0x3b,0x39,0x49}[offset_index];
		u16 const pointer = window < 0 ? mode == 2 ? 0x4007 : 0x1007 : window ? 0x1400 : 0;
		u16 const address = window >= 0 ? pointer+literal : mode == 3 ? literal : mode == 2 ?
			0x4000 | ((7+s16(literal)) & 0x1fff) : u16(pointer+s16(literal));
		u16 const next = mode == 1 || mode == 2 ? address : pointer, bk = mode == 2 ? 8192 : 16;
		u16 const st0 = ((cmpt ? 0x6608 : 0x608) & ~0x400U) | (flags & 4 ? 0x400 : 0) | 0x800;
		u16 const modified = cmpt && reg && mode != 3 ? (st0 & 0x1fff) | (reg << 13) : st0;
		u16 const st1 = 0x800 | (cmpt ? 0x20 : 0) | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0);
		u16 const input = pairs[pair][0], mask = pairs[pair][1];
		u16 result = operation == 1 ? input & mask : operation == 2 ? input | mask : input ^ mask, status = modified;
		if (!operation) {
			s64 const left = (flags & 1) && input >= 0x8000 ? s64(input)-0x10000 : input;
			s64 const right = (flags & 1) && mask >= 0x8000 ? s64(mask)-0x10000 : mask;
			s64 const sum = left+right;
			bool const overflow = sum < -32768 || sum > 32767;
			result = (flags & 2) && overflow ? u16(sum < 0 ? 0x8000 : 0x7fff) : u16(sum);
			status = (modified & ~0x800U) | (u64(u32(left))+u32(right) >= 0x100000000ULL ? 0x800 : 0) | (overflow ? 0x400 : 0);
		}
		u16 const opcode = std::array<u16,4>{0x6b00,0x6800,0x6900,0x6a00}[operation] | (mode == 3 ? 0xf8 : 0xe0 | (mode << 3) | reg);
		u32 const base = cases.size()%2 ? 0x20000 : 0;
		auto initial = initial_case("long-RMW-captured-address-word-and-mask",{opcode,literal,mask,0xf4e1});
		if (follow) { initial.program[3].value = 0x1080 | selected; initial.program[4].value = 0xf4e1; }
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_BK,bk},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		for (unsigned ar = 0; ar != 8; ++ar) initial.initial.push_back({r::STATE_AR0+ar,ar == selected ? pointer : window < 0 ? 0x1007U : 0x5555U});
		initial.data = {{address,input}};
		unsigned const read = address >= 0x20 && address < 0x60 ? 8 : 7;
		auto at_address = observation("only-selected-pointer-updated",{});
		auto final = observation("RMW-survives-pointer-memory-and-literal-poison",{{r::STATE_ST0,status},{r::STATE_ST1,st1},
			{r::STATE_A,follow ? u64((flags & 1) && result >= 0x8000 ? s64(result)-0x10000 : result) & 0xffffffffffULL : 0x123456789aULL},
			{r::STATE_B,0x2233445566ULL},{r::STATE_BK,bk}},{{address,result}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			at_address.expected.push_back({int(r::STATE_AR0+ar),ar == selected ? next : window < 0 ? 0x1007U : 0x5555U});
			final.expected.push_back({int(r::STATE_AR0+ar),ar == selected ? follow ? address : 0x2222U : window < 0 ? 0x1007U : 0x5555U});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("long-RMW-running",{})},{6,std::move(at_address)},
			{7,observation("address-already-retained",{})},{read+1,observation("Write-and-flags-publish-together",{{r::STATE_ST0,status}},{{address,result}})},
			{20,std::move(final)}},std::array<unsigned,5>{4,6,7,8,9}[cases.size()%(window >= 0 ? 5 : 4)],20};
		c.actions = {{5,{},false,{{base+0x601,0xdead},{base+0x602,0xdead}}},
			{6,{{r::STATE_AR0+selected,follow ? address : 0x2222U}}},{read,{},false,{}, {},{{address,0x4444}}}};
		c.retained_program_reads = {{base+0x601,literal},{base+0x602,mask}};
		c.data_events = std::vector<clocked_data_event>{{false,address,input,read},{true,address,result,read+1}};
		c.data_events->back().registers = {{r::STATE_ST0,status}};
		cases.push_back(std::move(c));
	};
	for (unsigned form = 0; form != 25; ++form) for (unsigned offset = 0; offset != 4; ++offset)
		for (unsigned cmpt = 0; cmpt != 2; ++cmpt) for (unsigned op = 0; op != 3; ++op)
			add(form,offset,cmpt,op,(form+offset+cmpt)%8,(form+offset+op)%8);
	for (unsigned op = 0; op != 3; ++op) for (unsigned pair = 0; pair != 8; ++pair) for (unsigned flags = 0; flags != 8; ++flags)
		add(24,0,false,op,pair,flags);
	for (unsigned reg = 0; reg != 8; ++reg) for (unsigned mode = 0; mode != 2; ++mode) for (unsigned offset = 0; offset != 3; ++offset)
		for (unsigned window = 0; window != 2; ++window) for (unsigned cmpt = 0; cmpt != 2; ++cmpt) for (unsigned op = 0; op != 3; ++op)
			add(mode*8+reg,offset,cmpt,op,(reg+mode+offset)%8,(reg+offset+op)%8,window);
	for (unsigned window = 0; window != 2; ++window) for (unsigned op = 0; op != 3; ++op)
		for (unsigned pair = 0; pair != 8; ++pair) for (unsigned flags = 0; flags != 8; ++flags) add(1,0,false,op,pair,flags,window);
	for (unsigned offset = 0; offset != 4; ++offset) for (unsigned pair = 0; pair != 8; ++pair)
		for (unsigned flags = 0; flags != 8; ++flags) for (unsigned cmpt = 0; cmpt != 2; ++cmpt) add(24,offset,cmpt,3,pair,flags,-1,true);
	return cases;
}
}
