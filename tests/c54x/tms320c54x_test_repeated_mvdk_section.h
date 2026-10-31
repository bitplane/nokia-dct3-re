// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> repeated_mvdk_cases(bool indexed)
{
	std::vector<clocked_register_case> cases;
	for(unsigned encoding=0;encoding<(indexed ? 16U : 184U);++encoding)for(unsigned count : {0U,1U,5U,15U})
	for(unsigned placement=0;placement<6;++placement)for(bool compatibility : {false,true})for(bool cpl : {false,true}){
		unsigned const mode=indexed ? 0xa8+encoding : encoding<152 ? encoding : encoding+40,encoded_ar=mode&7,selected=compatibility && !encoded_ar ? 4 : encoded_ar;
		unsigned const seed=encoding+count+placement+compatibility+cpl;u32 const base=seed&1 ? 0x20000 : 0;
		u16 const st0=compatibility && encoded_ar && (mode&0x80) ? (0x8620&0x1fff)|(encoded_ar<<13) : 0x8620,st1=0x800|(compatibility ? 0x20 : 0)|(cpl ? 0x4000 : 0);
		u16 const bk=mode>=0xc0 ? (!selected ? 0x2000 : 82) : 0;
		std::array<u16,8> initial_ar;initial_ar.fill(0x1040);initial_ar[0]=1;if(mode&0x80)initial_ar[selected]=mode>=0xc0 ? 0x1051 : 0x1040;
		u16 const first=mode&0x80 ? initial_ar[selected] : 0x1000+mode,destination=placement ? first+int(placement)-3 : 0x1800;
		auto ar=initial_ar;std::vector<memory_word> sources,outputs;std::map<u16,u16> committed;
		auto word=[](u16 address)->u16{return (unsigned(address)*0x3917)^0xa55a;};bool excluded=false;
		for(unsigned i=0;i<=count;++i){
			u16 const source=mode&0x80 ? ar[selected] : first;excluded|=source<0x60;
			u16 const value=committed.count(source) ? committed[source] : word(source);sources.push_back({source,value});outputs.push_back({u16(destination+i),value});
			// Access(n) samples before Read(n-1)'s RAM publication in the same clock.
			if(i)committed[u16(destination+i-1)]=outputs[i-1].value;
			if(mode&0x80){unsigned const operation=(mode>>3)&15;int const step=operation&1 ? s16(ar[0]) : 1;
				int const delta=indexed ? (operation==5 ? -int(s16(ar[0])) : int(s16(ar[0]))) : operation==1 ? -1 : operation==2 ? 1 : operation>=8 ? (operation<10 ? -step : step) : 0;
				if(operation>=8 && bk){unsigned mask=1;while(mask<=bk)mask*=2;--mask;int const remainder=(int(source&mask)+delta)%bk;ar[selected]=(source&~mask)|u16(remainder<0 ? remainder+bk : remainder);}
				else ar[selected]=source+delta;
			}
		}
		if(excluded)continue; // No fabricated mapped-source behavior for self-indexed AR0.
		auto initial=initial_case("repeated-MVDK-alias-sampling-before-previous-publication",{u16(0xec00|count),u16(0x7100|mode),destination,0xf4e1});
		for(auto &entry : initial.program)entry.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x8620},{r::STATE_ST1,st1},{r::STATE_SP,0x1000},{r::STATE_BK,bk},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0xbeef}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,initial_ar[i]});
		std::map<u16,u16> ram;for(auto const &source : sources)ram[source.address]=word(source.address);for(auto const &output : outputs)ram[output.address]=word(output.address);
		for(auto const &[address,value] : ram)initial.data.push_back({address,value});
		auto final=observation("exact-copies-pointer-updates-and-unselected-state",{{STATE_GENPC,base+0x604},{r::STATE_RC,0},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_SP,0x1000},{r::STATE_BK,bk},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0xbeef}});
		for(unsigned i=0;i<8;++i)final.expected.push_back({int(r::STATE_AR0+i),ar[i]});
		for(auto const &output : outputs)final.expected_data.push_back(output);
		clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},5+seed%3,32};c.clear_pipeline_before_restore=indexed;
		c.program_events={{false,base+0x601,u16(0x7100|mode),0,false},{false,base+0x602,destination,0,false}};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<=count;++i){c.data_events->push_back({false,sources[i].address,sources[i].value,6+i,true});if(i)c.data_events->push_back({true,outputs[i-1].address,outputs[i-1].value,6+i,true});}
		c.data_events->push_back({true,outputs.back().address,outputs.back().value,7+count,true});cases.push_back(std::move(c));
	}
	return cases;
}
}
