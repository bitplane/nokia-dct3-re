// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "nokia_mfi_rx.h"
#include "nokia_dsp_compare_clock.h"

DEFINE_DEVICE_TYPE(NOKIA_MFI_RX, nokia_mfi_rx_device, "nokia_mfi_rx", "Nokia MFI receive transport prototype")

nokia_mfi_rx_device::nokia_mfi_rx_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: device_t(mconfig, NOKIA_MFI_RX, tag, owner, clock), m_adc_cb(*this, 0), m_data_available_cb(*this), m_fault_observer_cb(*this)
{
}

void nokia_mfi_rx_device::device_start()
{
	if (clock() != 13'000'000 || !m_divider) fatalerror("MFI prototype requires the declared 13MHz reference and a nonzero divider");
	m_timer = timer_alloc(FUNC(nokia_mfi_rx_device::word_tick), this);
	save_item(NAME(m_state.words)); save_item(NAME(m_state.source_ticks));
	save_item(NAME(m_state.head)); save_item(NAME(m_state.count)); save_item(NAME(m_state.last));
	save_item(NAME(m_state.produced)); save_item(NAME(m_state.consumed));
	save_item(NAME(m_state.overruns)); save_item(NAME(m_state.underruns));
	save_item(NAME(m_state.enabled)); save_item(NAME(m_state.fault));
	save_item(NAME(m_next_tick));
}

void nokia_mfi_rx_device::device_reset()
{
	m_state.reset(); m_next_tick = 0;
	m_timer->adjust(attotime::never);
}

void nokia_mfi_rx_device::enable_w(int state)
{
	if (bool(state) == m_state.enabled) return;
	machine().logerror("mfi_rx_enable: state=%u produced=%llu consumed=%llu overrun=%llu underrun=%llu t=%.9f\n",
		bool(state), produced(), consumed(), overruns(), underruns(), machine().time().as_double());
	m_state.enable(state);
	if (!state) { m_timer->adjust(attotime::never); return; }
	m_next_tick = (machine().time().as_ticks(clock()) / m_divider + 1) * m_divider;
	arm();
}

void nokia_mfi_rx_device::arm()
{
	attotime const absolute(m_next_tick / clock(), nokia_dsp_compare_clock::fractional_attoseconds(m_next_tick));
	m_timer->adjust(absolute - machine().time());
}

TIMER_CALLBACK_MEMBER(nokia_mfi_rx_device::word_tick)
{
	if (!m_state.enabled || m_state.fault) return;
	u16 const sample = m_adc_cb(m_state.produced & 1);
	bool const ready = m_state.clock(sample, m_next_tick);
	if (m_state.fault)
	{
		machine().logerror("mfi_rx_fault: overrun=1 produced=%llu consumed=%llu t=%.9f\n",produced(),consumed(),machine().time().as_double());
		m_fault_observer_cb(1);
	}
	if (ready)
	{
		if (produced() <= 1024 && m_block_trace_count < 128)
		{
			++m_block_trace_count;
			machine().logerror("mfi_rx_block: block=%llu source_tick=%llu count=%u t=%.9f\n",produced()/32,m_next_tick,m_state.count,machine().time().as_double());
		}
		m_data_available_cb(1); m_data_available_cb(0);
	}
	m_next_tick += m_divider;
	if (!m_state.fault) arm();
}

u16 nokia_mfi_rx_device::read()
{
	bool const side_effects = !machine().side_effects_disabled();
	if (side_effects && m_state.count && consumed() < 1024 && m_word_trace_count < 4096)
	{
		++m_word_trace_count;
		machine().logerror("mfi_rx_word: sequence=%llu value=%04x source_tick=%llu t=%.9f\n",consumed()+1,
			m_state.words[m_state.head],m_state.source_ticks[m_state.head],machine().time().as_double());
	}
	u64 const before = underruns();
	u16 const result = m_state.read(side_effects);
	if (underruns() != before && underruns() <= 16)
	{
		machine().logerror("mfi_rx_fault: underrun=%llu retained=%04x t=%.9f\n",underruns(),result,machine().time().as_double());
		m_fault_observer_cb(1);
	}
	return result;
}

void nokia_mfi_rx_device::device_stop()
{
	machine().logerror("mfi_rx_trace_complete: words=%u blocks=%u word_cap=4096 block_cap=128\n",m_word_trace_count,m_block_trace_count);
	machine().logerror("mfi_rx_summary: produced=%llu consumed=%llu queued=%u overrun=%llu underrun=%llu fault=%u divider=%u physical_clock_claim=0\n",
		produced(),consumed(),m_state.count,overruns(),underruns(),m_state.fault,m_divider);
}
