// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned producer = 0; producer < 3; ++producer)
	for (unsigned form = 0; form < (producer ? 3U : 1U); ++form)
	for (unsigned consumer = 0; consumer < (producer ? 3U : 1U); ++consumer)
	for (unsigned pattern = 0; pattern < 4; ++pattern) for (unsigned flags = 0; flags < 4; ++flags)
	for (unsigned compiler = 0; compiler < 2; ++compiler) for (unsigned gap = 0; gap < 2; ++gap)
	for (unsigned different = 0; different < 2; ++different)
	{
		bool const copy = producer == 2, sxm = flags & 1, ovm = flags & 2;
		unsigned const extra = form == 2, read = 7 + extra + gap, retired = read + 1;
		u16 const literal = std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		u16 const before = producer ? 0x2468 : std::array<u16,4>{0,0x7fff,0x8000,0xffff}[pattern];
		s32 const input = sxm ? s16(before) : s32(before), sum = input + 1;
		u16 const result = producer ? literal : ovm ? u16(std::clamp(sum,s32(-32768),s32(32767))) : u16(sum);
		u16 const loaded = different ? 0x1357 : result;
		u64 const extended = sxm && (loaded & 0x8000) ? 0xffffff0000ULL | loaded : loaded;
		u64 const output = consumer == 1 ? (0x1234 + extended) & 0xffffffffffULL : extended;
		u16 const status = producer ? 8 | (consumer == 1 && u64(0x1234) + u32(extended) > 0xffffffffULL ? 0x800 : 0) :
			8 | (sum < -32768 || sum > 32767 ? 0x400 : 0) | (u64(u32(input)) + 1 > 0xffffffffULL ? 0x800 : 0);
		u16 const address = compiler ? 0x1002 : 0x402;
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (compiler ? 0x4000 : 0);
		u16 const opcode = copy ? form == 2 ? 0x70f8 : form == 1 ? 0x7092 : 0x7002 :
			producer ? form == 2 ? 0x76f8 : form == 1 ? 0x7692 : 0x7602 : 0x6b02;
		auto initial = initial_case("ram-forwarding", {opcode,u16(extra ? address : copy ? 0x11 : producer ? literal : 1)});
		if (extra) initial.program[2].value = copy ? 0x11 : literal;
		initial.program[2 + extra + gap].value = (consumer == 1 ? 0 : consumer == 2 ? 0x1100 : 0x1000) | (different ? 3 : 2);
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,8},{r::STATE_ST1,st1},{r::STATE_SP,0x1000},
			{r::STATE_A,0x1234},{r::STATE_B,0x5678},{r::STATE_AR1,literal},{r::STATE_AR2,address}});
		initial.data = {{address,before},{unsigned(address + 1),0x1357}};
		unsigned const point = 5 + extra + (producer + form + consumer + pattern + flags + compiler + gap + different) % 3;
		clocked_register_case c{{{0,std::move(initial)}},point,12 + extra};
		c.require_idle = false;
		for (unsigned tick = 1; tick <= retired; ++tick)
		{
			bool const done = tick >= retired;
			auto state = observation("producer-consumer-order", {{r::STATE_A,done && consumer != 2 ? output : 0x1234},
				{r::STATE_B,done && consumer == 2 ? output : 0x5678},
				{r::STATE_ST0,(producer ? done : tick >= 7) ? status : 8U},{r::STATE_ST1,st1},
				{r::STATE_AR2,address + unsigned(form == 1 && tick >= 5)}});
			if (copy && tick > 6 + extra) state.expected.push_back({r::STATE_AR1,unsigned(literal ^ 0x5a5a)});
			c.states.push_back({tick,std::move(state)});
		}
		if (producer) c.actions = {{5 + extra,{},false,{{0x601 + extra,u16((copy ? 0x11 : literal) ^ 0x5a5a)}}}};
		if (copy) c.actions.push_back({6 + extra,{{r::STATE_AR1,literal ^ 0x5a5a}}});
		c.data_events = std::vector<clocked_data_event>{};
		if (!producer) c.data_events->push_back({false,address,before,6});
		if (copy) c.data_events->push_back({false,0x11,literal,6 + extra});
		clocked_data_event const write{true,address,result,7 + extra};
		if (gap) c.data_events->push_back(write);
		if (gap || different) c.data_events->push_back({false,address + different,loaded,read});
		if (!gap) c.data_events->push_back(write);
		cases.push_back(std::move(c));
	}
	return cases;
}
}
