// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
inline std::vector<clocked_register_case> repeated_parallel_cases(bool arithmetic)
{
	std::vector<clocked_register_case> cases;
	unsigned const forms=arithmetic ? 8 : 6, layouts=arithmetic ? 5 : 4, flags_count=arithmetic ? 8 : 4;
	for (unsigned bank=0;bank<2;++bank) for (unsigned form=0;form<forms;++form) for (unsigned layout=0;layout<layouts;++layout)
	for (unsigned choice=0;choice<(bank ? 3U : 4U*flags_count);++choice) {
		unsigned const flags=bank ? (form+layout+choice)%flags_count : choice/4;
		int const shift=std::array<int,4>{0,-1,7,-16}[bank ? (form+layout+choice)%4 : choice%4];
		unsigned const count=bank ? std::array<unsigned,3>{1,2,256}[choice] : 8;
		u16 const st1=0x800|(flags&1 ? 0x100 : 0)|(flags&4 ? 0x200 : 0)|(shift&31), pmst=flags&2 ? 1 : 0;
		u32 const page=(cases.size()%2)*0x20000;
		auto address=[&](unsigned i,bool y)->u16 { return y && layout!=3 ? 0x1c00+(layout ? i : 0) : 0x1800+(layout==4 ? 3*int(i) : layout==2 ? -int(i) : layout ? int(i) : 0); };
		auto sample=[](unsigned i)->u16 { return u16(0x8001+i*0x1111); };
		auto signed40=[](u64 v)->s64 { return v&0x8000000000ULL ? s64(v)-0x10000000000LL : s64(v); };
		u16 const opcode=(arithmetic ? 0xc000|((form/4)<<10)|((form&1)<<8)|(((form/2)&1)<<9) : form<4 ? 0xc800|((form&1)<<8)|((form/2)<<9) : 0xe400|((form-4)<<9))|std::array<u16,5>{1,0xb9,0x49,0x84,0xc9}[layout];
		u16 const status=arithmetic ? 0xa030|(form&1 ? 0x800 : 0) : 0xa630;
		auto initial=initial_case("repeated-parallel-fresh-X-old-accumulator-Y",{u16(0xec00|(count-1)),opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=page;
		initial.opcode=opcode;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},
			{r::STATE_A,0x0180014321ULL},{r::STATE_B,0xff12348765ULL},{r::STATE_T,0xbeef},{r::STATE_AR0,3},{r::STATE_BK,0}});
		for (unsigned ar=2;ar<6;++ar) initial.initial.push_back({r::STATE_AR0+int(ar),0x4800+ar});
		initial.initial.push_back({r::STATE_AR0+(layout==1 ? 5 : 2),0x1800});
		if (layout!=3) initial.initial.push_back({r::STATE_AR3,0x1c00});
		for (unsigned i=0;i<(layout ? count : 1);++i) { initial.data.push_back({address(i,true),0xdead}); initial.data.push_back({address(i,false),sample(i)}); }
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{5,6+count/2,6+count}[cases.size()%3],count+16};
		c.single_fetch=page+0x601; c.clear_pipeline_before_restore=true;
		c.actions={{5,{},false,{{page+0x601,0xdead}}}};
		for (unsigned i=0;i<count;++i) c.actions.push_back({6+i,{},false,{}, {},{{address(i,false),layout ? u16(~sample(i)) : sample(i+1)}}});
		std::vector<std::array<u64,3>> states{{0x0180014321ULL,0xff12348765ULL,status}};
		std::vector<u16> stores;
		for (unsigned i=0;i<count;++i) {
			auto next=states.back();
			u64 old=next[arithmetic ? (form/2)&1 : form>=4 ? form-4 : form/2];
			u64 shifted=shift<0 ? u64(st1&0x100 ? signed40(old)>>-shift : s64(old>>-shift))&0xffffffffffULL : (old<<shift)&0xffffffffffULL;
			if (pmst) shifted=st1&0x100 ? u64(std::clamp(signed40(shifted),s64(-2147483648LL),s64(2147483647LL))) : std::min(shifted,u64(0xffffffff));
			stores.push_back(u16(shifted>>16));
			s64 const memory=s64(st1&0x100 ? s16(sample(i)) : s32(sample(i)))*65536;
			if (arithmetic) {
				unsigned const destination=form&1; u64 const other=next[!destination], operand=u64(memory)&0xffffffffffULL;
				s64 result=form>=4 ? memory-signed40(other) : signed40(other)+memory;
				bool const carry=form>=4 ? u32(operand)>=u32(other) : u64(u32(operand))+u32(other)>0xffffffffULL;
				next[2]=(next[2]&~u64(0x800))|(carry ? 0x800 : 0);
				if (result<-2147483648LL || result>2147483647LL) { next[2]|=destination ? 0x200 : 0x400; if (st1&0x200) result=std::clamp(result,s64(-2147483648LL),s64(2147483647LL)); }
				next[destination]=u64(result)&0xffffffffffULL;
			} else if (form<4) next[form&1]=u64(memory)&0xffffffffffULL;
			states.push_back(next);
		}
		for (unsigned tick=6;tick<=count+6;++tick) {
			unsigned const retired=std::min(tick-6,count), sampled=std::min(tick-5,count);
			auto const &state=states[retired];
			auto seen=observation("per-clock-accumulator-feedback-and-T-publication",{{r::STATE_A,state[0]},{r::STATE_B,state[1]},
				{r::STATE_T,!arithmetic && form>=4 && sampled ? sample(sampled-1) : 0xbeefU},{r::STATE_ST0,arithmetic ? state[2] : status}});
			c.states.push_back({tick,std::move(seen)});
		}
		auto final=observation("all-stores-drained-once-and-pointers-updated",{{STATE_GENPC,page+0x603},{r::STATE_ST1,st1}});
		for (unsigned ar=2;ar<6;++ar) final.expected.push_back({int(r::STATE_AR0)+int(ar),
			ar==(layout==1 ? 5U : 2U) ? address(count,false) : layout!=3 && ar==3 ? address(count,true) : 0x4800+ar});
		for (unsigned i=0;i<(layout ? count : 1);++i) final.expected_data.push_back({address(i,true),stores[layout ? i : count-1]});
		if (layout!=3) for (unsigned i=0;i<(layout ? count : 1);++i) final.expected_data.push_back({address(i,false),layout ? u16(~sample(i)) : sample(count)});
		final.expected.insert(final.expected.end(),c.states.back().observation.expected.begin(),c.states.back().observation.expected.end());
		c.states.push_back({count+16,std::move(final)});
		c.data_events=std::vector<clocked_data_event>{{false,address(0,false),sample(0),6}};
		for (unsigned i=0;i<count;++i) {
			if (i+1<count) c.data_events->push_back({false,address(i+1,false),sample(i+1),7+i});
			c.data_events->push_back({true,address(i,true),stores[i],7+i});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
