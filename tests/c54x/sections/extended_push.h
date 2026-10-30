// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row {
		u16 mode,literal,address,value,bk; unsigned cmpt,pattern,irq,seed;
		bool mapped = false;
	};
	std::vector<clocked_register_case> cases;
	auto append = [&](row v) {
		u32 const base = v.seed % 2 ? 0x20000 : 0;
		unsigned const reg = v.mode & 7, selected = v.mode == 0xf8 ? 8 : v.cmpt && reg == 0 ? 5 : reg;
		u16 const status = v.cmpt && v.mode != 0xf8 && reg ? (0xb608 & 0x1fff) | (reg << 13) : 0xb608;
		u16 const st1 = (!v.mapped && (v.seed & 2) ? 0x4000 : 0) | (v.cmpt ? 0x20 : 0) | (v.irq ? 0 : 0x800) |
			(v.pattern & 1 ? 0x100 : 0) | (v.pattern & 2 ? 0x200 : 0) | 0x13;
		u64 a = 0x123456789aULL, b = 0xfedcba9876ULL;
		u16 t = 0x5aa5,trn = 0x1234;
		std::array<u16,8> pointers;
		for (unsigned ar = 0; ar != 8; ++ar) pointers[ar] = ar == selected ? 0x5007 : 0x7000 + ar;
		if (v.mapped) {
			if (v.address >= 8 && v.address <= 13) {
				unsigned const part = (v.address - 8) % 3, shift = part * 16;
				u64 const mask = (part == 2 ? 0xffULL : 0xffffULL) << shift;
				u64 &accumulator = v.address >= 11 ? b : a;
				accumulator = (accumulator & ~mask) | ((u64(v.value) << shift) & mask);
				if (part == 2) v.value &= 0xff;
			} else if (v.address == 14) t = v.value;
			else if (v.address == 15) trn = v.value;
			else if (v.address >= 16 && v.address <= 23) pointers[v.address - 16] = v.value;
			else if (v.address == 0x19) v.bk = v.value;
			else if (v.address <= 1) v.value = 0;
			else if (v.address == 6) v.value = status;
			else if (v.address == 7) v.value = st1;
			else if (v.address == 0x1a) v.value = 13;
			else if (v.address == 0x1b) v.value = 0x1234;
			else if (v.address == 0x1c) v.value = 0x5678;
			else if (v.address == 0x1d) v.value = v.pattern & 1;
			else if (v.address == 0x1e) v.value = base >> 16;
			v.literal = v.mode == 0xf8 ? v.address : u16(v.address - pointers[selected]);
		}
		auto initial = initial_case("extended-PSHD-retained-source-and-owned-interrupt-frame", {u16(0x4b00 | v.mode),v.literal,0xe955});
		initial.program[15].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		if (v.irq) {
			u32 const vector = base + (v.irq == 2 || v.irq == 4 ? 4 : 0x48);
			initial.program.insert(initial.program.end(),{{vector,0xf495},{vector + 1,0xf4eb}});
		}
		initial.data = {{0x2fff,0}};
		if (!v.mapped) initial.data.push_back({v.address,v.value});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xb608},{r::STATE_ST1,st1},
			{r::STATE_PMST,v.pattern & 1},{r::STATE_SP,0x3000},{r::STATE_BK,v.bk},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,t},{r::STATE_TRN,trn},{r::STATE_BRC,13},{r::STATE_RSA,0x1234},{r::STATE_REA,0x5678},
			{r::STATE_IMR,v.irq == 1 || v.irq == 3 ? 4U : 0U}});
		auto access = observation("Access-allocates-once-and-publishes-owned-ARP", {{r::STATE_ST0,status},{r::STATE_SP,0x2fff}});
		auto retained = observation("Write-retains-source-and-arithmetic-state", {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,status}});
		auto final = observation("one-push-and-balanced-optional-IRQ", {{STATE_GENPC,base + 0x610},{r::STATE_SP,0x2fff},
			{r::STATE_A,a},{r::STATE_B,0x55},{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_PMST,v.pattern & 1},
			{r::STATE_BK,v.bk},{r::STATE_T,t},{r::STATE_TRN,trn},{r::STATE_BRC,13},{r::STATE_RSA,0x1234},{r::STATE_REA,0x5678}},{{0x2fff,v.value}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),pointers[ar]});
			u16 const after = ar == selected && v.mode >= 0xe8 && v.mode < 0xf8 ? v.address : pointers[ar];
			for (auto *state : {&access,&retained,&final}) state->expected.push_back({int(r::STATE_AR0) + int(ar),after});
		}
		clocked_register_case c{{{0,std::move(initial)},{5,std::move(access)},{6,observation("Read-and-optional-frame-allocation", {})},{7,std::move(retained)},
			{8,observation("continuation-or-IRQ-running", {})},{96,std::move(final)}},std::array<unsigned,3>{3,5,7}[v.seed % 3],96};
		c.actions = {{3,{},false,{{base + 0x601,0xdead}}}};
		if (!v.mapped) c.actions.push_back({6,{},false,{}, {},{{v.address,0xdead}}});
		c.program_events = {{false,base + 0x601,v.literal,3}};
		c.data_events = std::vector<clocked_data_event>{{false,v.address,v.value,6},{true,0x2fff,v.value,7}};
		if (v.irq) {
			int const line = v.irq == 2 || v.irq == 4 ? INPUT_LINE_NMI : 2;
			unsigned const arrival = v.irq <= 2 ? 2 : 8;
			u16 const continuation = v.irq <= 2 ? 0x602 : 0x607;
			c.actions.push_back({arrival,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
			std::sort(c.actions.begin(),c.actions.end(),[](auto const &x,auto const &y) { return x.tick < y.tick; });
			// Early selection acknowledges before the older Execute tap; frame publication still follows the push.
			c.irqs = {{line,v.irq <= 2 ? 7U : 12U,v.irq <= 2 ? 7U : 12U,v.irq <= 2 ? 1U : 2U,continuation}};
			c.data_events->insert(c.data_events->end(),{{true,0x2ffe,continuation,0,false},{false,0x2ffe,continuation,0,false}});
		}
		cases.push_back(std::move(c));
	};
	for (unsigned form = 0; form != 25; ++form)
	for (unsigned offset = 0; offset != 4; ++offset)
	for (unsigned pattern = 0; pattern != 4; ++pattern)
	for (unsigned cmpt = 0; cmpt != 2; ++cmpt)
	for (unsigned bk : {0U,9U}) {
		u16 const literal = form == 24 ? std::array<u16,4>{0x5000,0x3000,0x2fff,0xffff}[offset] :
			form >= 16 && bk && offset == 3 ? 8 : std::array<u16,4>{0xffff,0,4,0x7fff}[offset];
		int const position = (7 + s16(literal)) % 9;
		u16 const address = form == 24 ? literal : form >= 16 && bk ? 0x5000 + (position < 0 ? position + 9 : position) : u16(0x5007 + s16(literal));
		append({u16(form == 24 ? 0xf8 : 0xe0 + form),literal,address,std::array<u16,4>{0,1,0x8000,0xffff}[pattern],u16(bk),
			cmpt,pattern,0,form + offset + pattern + cmpt + bk});
	}
	for (unsigned form = 0; form != 25; ++form)
	for (unsigned cmpt = 0; cmpt != 2; ++cmpt)
	for (unsigned irq = 1; irq != 5; ++irq) {
		unsigned const seed = form + cmpt + irq, pattern = seed % 4;
		// IRQ ordering composes with every encoded address form; data patterns/offsets are covered above.
		append({u16(form == 24 ? 0xf8 : 0xe0 + form),u16(form == 24 ? 0x5007 : 0),0x5007,
			std::array<u16,4>{0,1,0x8000,0xffff}[pattern],9,cmpt,pattern,irq,seed});
	}
	constexpr u16 sources[]{0,1,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,25,26,27,28,29,30};
	for (unsigned form = 0; form != 3; ++form)
	for (u16 address : sources) {
		if (form == 1 && address == 0x13) continue;
		for (unsigned pattern = 0; pattern != 4; ++pattern)
		for (unsigned cmpt = 0; cmpt != 2; ++cmpt)
			append({std::array<u16,3>{0xe3,0xeb,0xf8}[form],0,address,std::array<u16,4>{0,1,0x8000,0xffff}[pattern],9,
				cmpt,pattern,0,form + address + pattern + cmpt,true});
	}
	return cases;
}
}
