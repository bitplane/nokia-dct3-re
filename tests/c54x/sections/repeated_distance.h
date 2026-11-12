// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
// RPT-retained SQDST/ABDST: Read captures both operands once per issue, while
// Execute consumes the previous issue's A before replacing it with X-Y.
inline std::vector<clocked_register_case> section_cases()
{
	static constexpr std::array<u64,4> av{0,0x7fffffff,0xff80000000,0x0100000000};
	static constexpr std::array<u64,4> bv{0xffffffffff,0x0180000000,0,0x7fff0000};
	static constexpr std::array<u16,4> xs{0x8000,0x7fff,0xffff,0x1234}, ys{0x8000,1,0xffff,0x7fff};
	auto signed40=[](u64 value)->s64 { return BIT(value,39) ? s64(value)-(s64(1)<<40) : s64(value); };
	auto saturate=[](s64 value) { return std::clamp(value,s64(-2147483648LL),s64(2147483647LL)); };
	auto overflow=[](s64 value) { return value<-2147483648LL || value>2147483647LL; };
	std::vector<clocked_register_case> cases;
	for (unsigned bank=0;bank<2;++bank) for (unsigned absolute=0;absolute<2;++absolute) for (unsigned form=0;form<4;++form)
	for (unsigned choice=0;choice<(bank ? 4U : 128U);++choice) {
		unsigned const pattern=choice%4, flags=bank ? (form+choice+absolute)%16 : (choice/4)%16;
		unsigned const count=bank ? 256 : std::array<unsigned,2>{1,8}[choice/64];
		u32 const page=(cases.size()%2)*0x20000;
		u16 const st0=0x1030|(pattern&1 ? 0x400 : 0)|(pattern&2 ? 0x800 : 0);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&8 ? 0x100 : 0);
		u16 const pmst=flags&4 ? 2 : 0;
		// Form 3 is ROM4's abb3 shape: X=*AR4+0% around a sixteen-word buffer, Y=*AR2.
		bool const reused=form==0 || form==3;
		unsigned const xar=form==3 ? 4 : 3, yar=form==3 ? 2 : 4;
		auto address=[&](unsigned i,bool y)->u16 {
			if (form==3) return y ? 0x1c00 : 0x1800+(3*i)%16;
			if (!form) return y ? 0x1c00 : 0x1800;
			return y ? 0x1c00+i : 0x1800+(form==2 ? -int(i) : int(i));
		};
		auto sample=[&](unsigned i,bool y)->u16 { return u16((y ? ys : xs)[pattern]+i*(y ? 0x2222 : 0x1111)); };
		u16 const opcode=(absolute ? 0xe300 : 0xe200)|std::array<u16,4>{0x12,0x9a,0x5a,0xe0}[form];
		auto initial=initial_case("repeated-distance-retained-operands-live-accumulator",{u16(0xec00|((count-1)&0xff)),opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=page;
		initial.opcode=opcode;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_A,av[pattern]},{r::STATE_B,bv[pattern]},
			{r::STATE_T,0xbeef},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_AR0,3},{r::STATE_BK,form==3 ? 16U : 0U}});
		for (unsigned ar=2;ar<6;++ar) initial.initial.push_back({r::STATE_AR0+ar,ar==xar ? 0x1800U : ar==yar ? 0x1c00U : 0x4800+ar});
		for (unsigned i=0;i<(reused ? 1 : count);++i) for (unsigned side=0;side<2;++side) initial.data.push_back({address(i,side),sample(i,side)});
		unsigned const checkpoint=std::array<unsigned,4>{5,6,7+count/2,6+count}[cases.size()%4];
		clocked_register_case c{{{0,std::move(initial)}},checkpoint,count+24};
		c.single_fetch=page+0x601;
		c.actions={{4,{},false,{{page+0x601,0xe8ff}}}};
		c.data_events=std::vector<clocked_data_event>{};
		std::array<u64,3> state{av[pattern],bv[pattern],st0};
		for (unsigned i=0;i<count;++i) {
			for (unsigned side=0;side<2;++side) c.data_events->push_back({false,address(i,side),sample(i,side),6+i});
			c.states.push_back({6+i,observation("Read-precedes-live-accumulator-Execute",
				{{r::STATE_A,state[0]},{r::STATE_B,state[1]},{r::STATE_ST0,state[2]},{r::STATE_T,0xbeef}})});
			clocked_register_action poison{6+i,{}};
			for (unsigned side=0;side<2;++side) {
				poison.data.push_back({address(i,side),u16(~sample(i,side))});
				if (reused) poison.data.push_back({address(i+1,side),sample(i+1,side)});
			}
			c.actions.push_back(std::move(poison));
			s64 high=s64((state[0]>>16)&0x1ffff); if (high&0x10000) high-=0x20000;
			s64 distance=(absolute ? std::abs(high) : high*high)*(st1&0x40 ? 2 : 1);
			if (!absolute && (pmst&2) && (st1&0x240)==0x240) distance=saturate(distance);
			s64 result_b=signed40(state[1])+distance;
			s64 const x=st1&0x100 ? s16(sample(i,false)) : s32(sample(i,false)), y=st1&0x100 ? s16(sample(i,true)) : s32(sample(i,true));
			s64 result_a=(x-y)*65536;
			state[2]=(state[2]&~0x800)|(sample(i,false)>=sample(i,true) ? 0x800 : 0)|(overflow(result_a) ? 0x400 : 0)|(overflow(result_b) ? 0x200 : 0);
			if (st1&0x200) { result_a=saturate(result_a); result_b=saturate(result_b); }
			state[0]=u64(result_a)&0xffffffffffULL; state[1]=u64(result_b)&0xffffffffffULL;
		}
		auto final=observation("complete-distance-feedback-carry-overflow-and-no-duplicate-reads",{{r::STATE_A,state[0]},{r::STATE_B,state[1]},
			{r::STATE_ST0,state[2]},{r::STATE_T,0xbeef},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{STATE_GENPC,page+0x603}});
		auto retired=final;
		retired.expected.erase(std::remove_if(retired.expected.begin(),retired.expected.end(),[](auto const &reg) { return reg.index==STATE_GENPC; }),retired.expected.end());
		c.states.push_back({6+count,std::move(retired)});
		for (unsigned ar=2;ar<6;++ar) final.expected.push_back({int(r::STATE_AR0)+int(ar),ar==xar ? address(count,false) : ar==yar ? address(count,true) : 0x4800+ar});
		c.states.push_back({c.finish,std::move(final)});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
