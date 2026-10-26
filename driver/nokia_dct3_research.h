// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_DCT3_RESEARCH_H
#define MAME_NOKIA_NOKIA_DCT3_RESEARCH_H

#include "nokia_dct3_state.h"

namespace nokia_dct3 {

// Explicit CPU execution probe. The ordinary MCU uploads through DSPIF;
// The staged frontend and standard SWWSR model are explicit research choices,
// not a claim that the fitted Lead external-bus dialect has been identified.
class nse1_pipeline_fixture_state : public nokia_dct3_state
{
public:
	using nokia_dct3_state::nokia_dct3_state;
	void nse1pipe(machine_config &config);
	void nse1plg(machine_config &config);
	void nse1astd(machine_config &config);
};

// Explicit laboratory attachment, not recovered COBBA clock-enable wiring.
// Separate machines leave ordinary product execution unchanged.
class dct3_pcm_fixture_state : public nokia_dct3_state
{
public:
	using nokia_dct3_state::nokia_dct3_state;
	void nsm1pcm(machine_config &config);
	void nse1pcm(machine_config &config);
	void nse1bit(machine_config &config);
	void nse1pin(machine_config &config);
	void nse1aclk(machine_config &config);
	void nse1pclk(machine_config &config);
protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;
	virtual void device_stop() override;
private:
	u16 m_wire_word = 0;
	u8 m_wire_bits = 0;
	bool m_clocked_loopback = false;
	bool m_execution_demand_observer = false;
	// Diagnostic sequence, intentionally not rewound by architectural save/load.
	unsigned m_demand_entries = 0;
	u64 m_clocked_echo_words = 0;
	void instruction_w(offs_t address, u16 word);
	void bit_w(int state);
	void frame_w(int state);
};

INPUT_PORTS_EXTERN(nmp5stage);
INPUT_PORTS_EXTERN(nsb1r4t);
INPUT_PORTS_EXTERN(nsb3r4t);
INPUT_PORTS_EXTERN(nse2r4v);
INPUT_PORTS_EXTERN(nsk1r4t);
INPUT_PORTS_EXTERN(nsk3r4t);
INPUT_PORTS_EXTERN(nsm1r4t);

} // namespace nokia_dct3

#endif // MAME_NOKIA_NOKIA_DCT3_RESEARCH_H
