// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_IDLE_DMA
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index=0;index<10;++index) {
		static constexpr u16 modes[]{0xf4e1,0xf6e1,0xf5e1};
		bool const enabled=index&1;
		auto initial=initial_case("DMA-clock-domain-and-idle-wake",{modes[std::min(index/2,2U)],0x76f8,0x501,1,0xe800,0x8800,0xf4e1});
		for (auto &word : initial.program) word.address+=0x10360;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10960},{r::STATE_IMR,enabled ? 0x40U : 0U}});
		initial.data={{0x501,0}};
		for (unsigned word=0;word<4;++word) {
			initial.data.push_back({0x520+word,u16(0x1111*(word+1))});
			initial.data.push_back({0x530+word,0});
		}
		auto asleep=observation("CPU-idle-before-DMA-enable",{{r::STATE_IDLE,1},{STATE_GENPC,0x10961},{r::STATE_IFR,0}},{{0x501,0},{0x530,0}});
		auto final=observation("completed-block-wakes-only-enabled-source",{{r::STATE_IDLE,1},
			{STATE_GENPC,enabled ? 0x10967U : 0x10961U},{r::STATE_IFR,0x40},
			{r::STATE_IMR,0},{r::STATE_ST1,0x800}},{{0x501,u16(enabled)}});
		for (unsigned word=0;word<4;++word) final.data.push_back({0x530+word,u16(0x1111*(word+1))});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(asleep)},
			{17,observation("DMA-enabled",{})},{160,std::move(final)}},index>=6 ? 24U : 18U,160};
		c.pipeline_frontend = true; c.initial_input_lines={{6,CLEAR_LINE}};
		c.data_events=std::vector<clocked_data_event>{};
		for (unsigned word=0;word<4;++word) {
			c.data_events->push_back({false,0x520+word,u16(0x1111*(word+1)),0,false});
			c.data_events->push_back({true,0x530+word,u16(0x1111*(word+1)),0,false});
		}
		if (enabled) {
			c.data_events->push_back({true,0x501,1,0,false});
			c.data_events->back().registers={{r::STATE_IMR,0x40}};
			c.data_events->push_back({true,0,0,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
