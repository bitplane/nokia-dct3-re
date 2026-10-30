// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
enum class repeat_product { dual, square, tmac, poly };
inline std::vector<clocked_register_case> repeat_product_cases(repeat_product kind)
{
	bool const dual=kind==repeat_product::dual, poly=kind==repeat_product::poly, tmac=kind==repeat_product::tmac;
	unsigned const forms=dual ? 4 : 3, operations=dual ? 4 : tmac ? 8 : poly ? 1 : 4, selectors=dual ? 4 : 1, patterns=dual ? 4 : 8, flag_count=poly ? 8 : 16;
	static constexpr u16 seeds[]{0,1,0x5678,0x8000,0xffff,0x7fff,0x8000,0x1234};
	static constexpr u64 av[]{0,0x12340000,0x12340000,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff};
	static constexpr u64 bv[]{0,0x8000,0x10000,0x7fffffff,0xff80000000,0x7fff8000,0xffffffffff,0x7fffffffff};
	auto signed40=[](u64 value)->s64 { return value&0x8000000000ULL ? s64(value)-0x10000000000LL : s64(value); };
	auto rounded=[](s64 value)->s64 { value+=32768; return (value>=0 ? value/65536 : -((-value+65535)/65536))*65536; };
	std::vector<clocked_register_case> cases;
	for (unsigned bank=0;bank<(poly ? 1U : 2U);++bank) for (unsigned form=0;form<forms;++form)
	for (unsigned operation=0;operation<operations;++operation) for (unsigned selector=0;selector<selectors;++selector)
	for (unsigned choice=0;choice<(bank ? 3U : patterns*flag_count*(poly ? 3U : 1U));++choice) {
		unsigned const pattern=bank ? (form+operation+selector+choice)%patterns : choice%patterns;
		unsigned const flags=bank ? (form+operation+selector+choice)%flag_count : (choice/patterns)%flag_count;
		unsigned const repeat=poly ? choice/(patterns*flag_count) : 1;
		unsigned const count=poly ? repeat==2 ? 3 : 1 : bank ? std::array<unsigned,3>{1,2,256}[choice] : 8;
		u32 const page=(cases.size()%2)*0x20000;
		u16 const st0=poly ? 0x1a30|(pattern&1 ? 0x400 : 0) : dual ? 0x1030|(pattern&1 ? 0x400 : 0)|(pattern&2 ? 0xa00 : 0) : 0x1030|(pattern&1 ? 0x400 : 0)|(pattern&2 ? 0x200 : 0)|(pattern&4 ? 0x800 : 0);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&(dual ? 8 : 2) ? 0x100 : 0)|(flags&(dual ? 2 : 4) ? 0x200 : 0)|(!dual && !poly && (flags&8) ? 0x80 : 0);
		u16 const pmst=(dual ? flags&4 : tmac ? flags&8 : 0) ? 2 : 0;
		u16 const initial_t=dual ? 0x5555 : seeds[pattern];
		u64 const a=dual ? std::array<u64,4>{0,0x7fffffff,0xff80000000,0x8000000000}[pattern] : av[pattern];
		u64 const b=dual ? std::array<u64,4>{0xffffffffff,0x0180000000,0,0x123456789a}[pattern] : bv[pattern];
		u16 const xseed=dual ? std::array<u16,4>{0x8000,0x7fff,0xffff,0x1234}[pattern] : seeds[pattern];
		u16 const yseed=std::array<u16,4>{0x8000,1,0xffff,0x7fff}[pattern%4];
		auto address=[&](unsigned i,bool y)->u16 {
			if (!dual) return 0x1800+(form==2 ? i : 0);
			if (y && form!=3) return 0x1c00+(form==1 || form==2 ? i : 0);
			return 0x1800+(form==2 ? -int(i) : form ? int(i) : 0);
		};
		auto sample=[&](unsigned i,bool y)->u16 {
			if (poly) return std::array<u16,3>{0x8000,0xffff,0x1234}[form==2 ? i : 0];
			if (form==3) y=false;
			return u16((y ? yseed : xseed)+i*(y ? 0x2222 : 0x1111));
		};
		u16 const opcode=dual ? 0xb000|(operation<<10)|(selector<<8)|std::array<u16,4>{1,0x9a,0x49,0x84}[form] : (poly ? 0x3600 : tmac ? 0x2800 : 0x3800)|(operation<<8)|std::array<u16,3>{0,0x82,0x92}[form];
		auto initial=initial_case("repeated-products-retained-operands-live-feedback",{poly && !repeat ? u16(0xf495) : u16(0xec00|(count-1)),opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=page;
		initial.opcode=opcode;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,initial_t},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_AR0,3},{r::STATE_BK,0}});
		for (unsigned ar=2;ar<6;++ar) initial.initial.push_back({r::STATE_AR0+ar,0x4800+ar});
		initial.initial.push_back({r::STATE_AR0+(dual && form==1 ? 3 : 2),0x1800});
		if (dual && form!=3) initial.initial.push_back({r::STATE_AR0+(form==1 ? 4 : 3),0x1c00});
		for (unsigned i=0;i<((dual ? form!=0 : form==2) ? count : 1);++i) for (unsigned side=0;side<(dual ? 2U : 1U);++side) initial.data.push_back({address(i,side),sample(i,side)});
		unsigned const checkpoint=std::array<unsigned,4>{5,6,7+count/2,6+count}[cases.size()%4];
		clocked_register_case c{{{0,std::move(initial)}},checkpoint,poly ? 48U : count+24};
		c.single_fetch=page+0x601; c.clear_pipeline_before_restore=tmac;
		if (!poly) c.actions={{4,{},false,{{page+0x601,0xe8ff}}}};
		std::array<u64,3> state{a,b,st0};
		c.data_events=std::vector<clocked_data_event>{};
		for (unsigned i=0;i<count;++i) {
			for (unsigned side=0;side<(dual ? 2U : 1U);++side) c.data_events->push_back({false,address(i,side),sample(i,side),6+i,!poly});
			if (!poly) {
				auto seen=observation("Read-publishes-T-before-live-accumulator-Execute",{{r::STATE_A,state[0]},{r::STATE_B,state[1]},{r::STATE_ST0,state[2]},{r::STATE_T,tmac ? initial_t : sample(i,false)}});
				c.states.push_back({6+i,std::move(seen)});
				clocked_register_action poison{6+i,{}};
				for (unsigned side=0;side<(dual ? 2U : 1U);++side) poison.data.push_back({address(i,side),(dual ? form!=0 : form==2) ? u16(~sample(i,side)) : sample(i+1,side)});
				if (i+1==count) poison.values={{r::STATE_T,0xdead}};
				c.actions.push_back(std::move(poison));
			}
			unsigned const destination=dual ? selector&1 : operation&1;
			s64 result;
			if (poly) {
				s64 high=(state[0]>>16)&0x1ffff; if (high&0x10000) high-=0x20000;
				result=rounded(high*s16(initial_t)*(st1&0x40 ? 2 : 1)+signed40(state[1]));
				state[1]=u64(s64(st1&0x100 ? s16(sample(i,false)) : s32(sample(i,false)))*65536)&0xffffffffffULL;
			} else {
				s64 product=s64(s16(sample(i,false)))*(dual ? s16(sample(i,true)) : tmac ? s16(initial_t) : s16(sample(i,false)))*(st1&0x40 ? 2 : 1);
				if ((pmst&2) && (st1&0x240)==0x240) product=std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
				result=signed40(state[dual ? selector>>1 : destination])+((operation&(tmac ? 4 : 2)) ? -product : product);
				if ((dual && (operation&1)) || (tmac && (operation&2))) result=rounded(result);
			}
			if (result<-2147483648LL || result>2147483647LL) { state[2]|=poly || !destination ? 0x400 : 0x200; if (st1&0x200) result=std::clamp(result,s64(-2147483648LL),s64(2147483647LL)); }
			state[poly ? 0 : destination]=u64(result)&0xffffffffffULL;
		}
		auto final=observation("complete-feedback-carry-overflow-and-no-duplicate-reads",{{r::STATE_A,state[0]},{r::STATE_B,state[1]},{r::STATE_ST0,state[2]},
				{r::STATE_T,poly ? initial_t : 0xdeadU},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{STATE_GENPC,page+0x603}});
		if (!poly) {
			auto retired=final;
			retired.expected.erase(std::remove_if(retired.expected.begin(),retired.expected.end(),[](auto const &reg) { return reg.index==STATE_GENPC; }),retired.expected.end());
			c.states.push_back({6+count,std::move(retired)});
		}
		for (unsigned ar=2;ar<6;++ar) final.expected.push_back({int(r::STATE_AR0)+int(ar),
			ar==(dual && form==1 ? 3U : 2U) ? address(count,false) : dual && form!=3 && ar==(form==1 ? 4U : 3U) ? address(count,true) : 0x4800+ar});
		c.states.push_back({c.finish,std::move(final)});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
