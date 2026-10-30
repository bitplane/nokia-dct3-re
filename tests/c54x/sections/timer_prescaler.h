// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned divider=1;divider<16;++divider) {
		auto initial=initial_case("TDDR-change-keeps-current-readonly-PSC",{0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_PRD,2},{r::STATE_TCR,0x30|divider},
			{r::STATE_TCR,0x10},{r::STATE_TCR,0},{r::STATE_IDLE,1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("timer-running-CPU-idle",{})},
			{2,observation("old-divider-still-counting",{{r::STATE_TIM,2},{r::STATE_TCR,(divider-1)<<6},{r::STATE_IFR,0}})},
			{3,observation("between-prescaler-observations",{})},
			{2*divider+3,observation("first-borrow-loads-new-TDDR",{{r::STATE_TIM,1},{r::STATE_TCR,0},{r::STATE_IFR,0}})},
			{2*divider+4,observation("between-counter-observations",{})},
			{2*divider+5,observation("counter-reaches-zero",{{r::STATE_TIM,0},{r::STATE_TCR,0},{r::STATE_IFR,0}})},
			{2*divider+6,observation("before-expiry-observation",{})},
			{2*divider+7,observation("expiry-reloads-period-and-latches-TINT",{{r::STATE_TIM,2},{r::STATE_PRD,2},{r::STATE_TCR,0},{r::STATE_IFR,8}})}},2,2*divider+7};
		c.observation_subticks=2;
		// Overwrite the reload configuration after its final observation so
		// replay must restore both phase and the original PRD, not just TIM.
		c.actions={{c.finish,{{r::STATE_TCR,0x10},{r::STATE_PRD,9},{r::STATE_TCR,0x20}}}};
		c.forbid_data_access=true;
		cases.push_back(std::move(c));
	}
	for (unsigned divider=2;divider<16;++divider) {
		auto initial=initial_case("TIM-write-preserves-live-PSC-without-prior-read",{0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_PRD,2},{r::STATE_TCR,0x20|divider},{r::STATE_IDLE,1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("no-TIM-TCR-peek-before-write",{})},
			{4,observation("counter-replaced-without-prescaler-restart",{{r::STATE_TIM,9},
				{r::STATE_TCR,divider|((divider-2)<<6)},{r::STATE_IFR,0}})}},divider%2 ? 1U : 3U,4};
		c.observation_subticks=2;
		c.actions={{2,{{r::STATE_TIM,9}}}};
		c.forbid_data_access=true;
		cases.push_back(std::move(c));
	}
	for (unsigned control=0;control<4;++control) for (unsigned reserved=0;reserved<16;++reserved) {
		auto initial=initial_case("TCR-reserved-Free-Soft-storage-and-readonly-PSC",{0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_PRD,9},{r::STATE_TCR,0x33},
			{r::STATE_TCR,(reserved<<12)|(control<<10)|0x13},{r::STATE_IDLE,1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("reserved-bits-zero-with-stopped-PSC-preserved",{
			{r::STATE_TCR,(control<<10)|0xd3},{r::STATE_TIM,9}})}},1,3};
		c.forbid_data_access=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
