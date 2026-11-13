// license:BSD-3-Clause
// copyright-holders:Gaz
// Executable backend compare-boundary contract, not SCU silicon timing proof.
#include "emu.h"
#include "nokia_ccont.h"
#include "nokia_cobba.h"
#include "nokia_dsp_c54x.h"
#include "nokia_dsp_compare_clock.h"
#include "nokia_dspif.h"
#include "nokia_hagar.h"
#include "nokia_mfi_rx.h"
#include "tms320c54x_test_snapshot.h"
#include <sstream>

namespace {
class c54x_serial_test_state : public driver_device
{
public:
	c54x_serial_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config,type,tag), m_backend(*this,"backend"),m_cpu(*this,"backend:cpu"),m_hagar(*this,"rf"),m_mfi(*this,"mfi") { }
	void test(machine_config &config)
	{
		NOKIA_DSPIF(config,"transport",0);
		NOKIA_COBBA(config,"codec",0);
		NOKIA_CCONT(config,"power",0);
		NOKIA_HAGAR(config,m_hagar,0);
		NOKIA_MFI_RX(config,m_mfi,13'000'000);
		m_mfi->set_staging_limit(96);
		m_mfi->set_latched_block_requests(true);
		m_mfi->adc_cb().set(FUNC(c54x_serial_test_state::adc_sample));
		m_mfi->data_available_cb().set(FUNC(c54x_serial_test_state::adc_ready));
		NOKIA_DSP_C54X(config,m_backend,13'000'000);
		m_backend->set_transport_tag("^transport"); m_backend->set_cobba_tag("^codec");
		m_backend->set_program_rom_tag("^program");
		m_backend->set_data_rom_tag("^data");
		m_backend->transfer_compare_cb().set(FUNC(c54x_serial_test_state::compare));
		auto &external=NOKIA_DSP_C54X(config,"external",13'000'000);
		external.set_transport_tag("^transport"); external.set_cobba_tag("^codec");
		external.set_program_rom_tag("^program");
		external.set_data_rom_tag("^data"); external.set_external_rf_receive(true);
	}
protected:
	void machine_start() override
	{
		m_timer=timer_alloc(FUNC(c54x_serial_test_state::step),this);
		save_item(NAME(m_events)); save_item(NAME(m_tuple)); save_item(NAME(m_time));
		save_item(NAME(m_adc_source)); save_item(NAME(m_adc_consumed)); save_item(NAME(m_adc_blocks));
		save_item(NAME(m_adc_time)); save_item(NAME(m_adc_last_tick));
		save_item(NAME(m_adc_first_time)); save_item(NAME(m_adc_first_tick)); save_item(NAME(m_adc_first_source));
		save_item(NAME(m_defer_adc)); save_item(NAME(m_deferred_requests));
	}
	void machine_reset() override
	{
		m_stage=0; m_events=0; m_tuple=0; m_timer->adjust(attotime::from_usec(100));
	}
private:
	void expect(bool condition,char const *message)
	{
		if (!condition) fatalerror("Compare boundary FAIL: %s",message);
	}
	void compare(u64 tuple)
	{
		m_hagar->compare_output_w(tuple);
		++m_events; m_tuple=tuple; m_time=machine().time();
		expect(m_cpu->space(AS_IO).read_word(0x0d)==u16(tuple),"event is on its own counter tick");
	}
	u16 adc_sample(offs_t component)
	{
		expect(component==(m_adc_source&1),"ADC component order");
		return 0x1000 + m_adc_source++;
	}
	void adc_ready(int state)
	{
		if (!state) return;
		if (m_defer_adc) { ++m_deferred_requests; return; }
		++m_adc_blocks; m_adc_time=machine().time();
		for (unsigned word=0; word<32; ++word)
		{
			u64 const tick=m_mfi->front_source_tick();
			expect(!m_adc_last_tick || tick==m_adc_last_tick+6,"MFI producer provenance is at exact reference ticks");
			m_adc_last_tick=tick;
			expect(m_mfi->read()==0x1000+m_adc_consumed++,"MFI contains actual clocked words in order");
		}
		if (m_adc_blocks==1)
		{
			m_adc_first_time=m_adc_time; m_adc_first_tick=m_adc_last_tick; m_adc_first_source=m_adc_source;
		}
	}
	attotime reference_offset(u64 ticks) const
	{
		using namespace nokia_dsp_compare_clock;
		return attotime(ticks/reference_hz,fractional_attoseconds(ticks));
	}
	void next_counter_phase(u64 ticks)
	{
		++m_stage;
		m_timer->adjust(m_counter_anchor+reference_offset(ticks)-machine().time());
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		auto &io=m_cpu->space(AS_IO);
		switch(m_stage)
		{
		case 0:
			m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_backend->reset_line_w(1);
			{
				auto &external=*subdevice<nokia_dsp_c54x_device>("external");
				auto &cpu=*subdevice<tms320c54x_device>("external:cpu");
				cpu.suspend(SUSPEND_REASON_DISABLE,true); external.reset_line_w(1);
				cpu.space(AS_IO).write_word(0x0e,999); cpu.space(AS_IO).write_word(0x0d,0);
				auto &power=*subdevice<nokia_ccont_device>("power");
				// RTC control must not gate the independent RF-reference timer.
				power.select_w(1); power.serial_w(6<<3); power.serial_w(0x54); power.select_w(0);
			}
			io.write_word(0x0e,999); io.write_word(0x0d,0);
			io.write_word(0x30,0x100); io.write_word(0x31,0xaaaa); io.write_word(0x32,0xbbbb);
			io.write_word(4,100); break;
		case 1:
			expect(!m_events,"no early event before target100");
			io.write_word(0x31,0xcccc); io.write_word(4,200); break;
		case 2:
			expect(!m_events && !(io.read_word(1)&0x1000),"replacement cancels the old deadline");
			m_hagar->compare_output_w(0x0100082a41000000ULL);
			m_hagar->compare_output_w(0x010002084a800000ULL);
			m_hagar->compare_output_w(0x010048c844000000ULL);
			{
				u16 key=0; u64 frequency=0;
				expect(m_hagar->rx_candidate(key,frequency) && key==838 && frequency==1'870'400'000,
					"independent observed-word candidate before snapshot");
			}
			expect(c54x_test::capture_snapshot(machine(),m_saved)==STATERR_NONE,"save armed compare");
			break;
		case 3:
			expect(m_events==1 && m_tuple==0x0100ccccbbbb00c8ULL,"one raw match tuple, independent of firmware service");
			expect(io.read_word(1)&0x1000,"match sets the independent pending latch");
			m_reference_time=m_time;
			io.write_word(0x31,0xdddd); io.write_word(4,15000);
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved)==STATERR_NONE,"restore armed compare and registers");
			expect(m_events==0 && io.read_word(0x31)==0xcccc,"restored sink and source state");
			{
				u16 key=0; u64 frequency=0;
				expect(m_hagar->transactions()==3 && m_hagar->rx_candidate(key,frequency) && key==838,
					"HAGAR command fields and receipt count restore");
			}
			break;
		case 4:
			expect(m_events==1 && m_tuple==0x0100ccccbbbb00c8ULL && m_time==m_reference_time,"exact restored tuple/time");
			expect(m_hagar->transactions()==4 && m_hagar->last_word()==0xbbbbcccc,
				"compare receipt repeats exactly after restore");
			io.write_word(1,0x1000); expect(!(io.read_word(1)&0x1000),"acknowledge only clears pending");
			io.write_word(0x30,0); break;
		case 5:
			{
				auto &external=*subdevice<nokia_dsp_c54x_device>("external");
				auto &cpu=*subdevice<tms320c54x_device>("external:cpu");
				expect(m_cpu->state_int(tms320c54x_device::STATE_IFR)&1,"default frame INT0 survives CCONT register6 bit1 clear");
				expect(!(cpu.state_int(tms320c54x_device::STATE_IFR)&1),"external owner has no idle frame INT0");
				external.rf_data_available_w(1);
			}
			expect(m_events==2 && m_tuple==0x0000ccccbbbb00c8ULL,"next frame still observes raw zero control");
			io.write_word(4,15000); expect(io.read_word(1)&0x1000,"cancel does not acknowledge pending");
			// P02 is the in-service mask (ROM4 35f9..3605 masks with ffff and
			// restores); only the P01 strobe acknowledges.
			io.write_word(2,0x1000); expect(io.read_word(1)&0x1000,"P02 masks without acknowledging pending");
			io.write_word(1,0x1000); expect(!(io.read_word(1)&0x1000),"P01 acknowledges pending");
			break;
		case 6:
			expect(subdevice<tms320c54x_device>("external:cpu")->state_int(tms320c54x_device::STATE_IFR)&1,
				"external data-available asserts INT0 without P03 forcing");
			expect(m_events==2,"sentinel cancels later matches");
			io.write_word(4,100); m_backend->reset_line_w(0); break;
		case 7:
			expect(m_events==2 && !(io.read_word(1)&0x1000),"held reset cancels an armed compare");
			expect(m_hagar->transactions()==5,"held DSP reset does not invent an off-chip reset");
			m_hagar->reset();
			expect(!m_hagar->transactions() && !m_hagar->last_word(),"explicit RF-device reset clears command state");
			m_mfi->enable_w(1); break;
		case 8:
			expect(m_mfi->produced()>0 && m_mfi->produced()<32 && !m_adc_blocks,"partial clocked MFI block before save");
			expect(c54x_test::capture_snapshot(machine(),m_saved)==STATERR_NONE,"save partial MFI block and word clock");
			break;
		case 9:
			expect(m_adc_blocks==1 && m_adc_consumed==32 && !m_mfi->overruns() && !m_mfi->underruns(),"one complete reference MFI block");
			m_reference_adc_time=m_adc_first_time; m_reference_adc_source=m_adc_first_source; m_reference_adc_tick=m_adc_first_tick;
			m_mfi->enable_w(0);
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved)==STATERR_NONE,"restore MFI clock and queued provenance");
			expect(!m_adc_blocks && !m_adc_consumed,"restore partial MFI consumer state");
			break;
		case 10:
			// An in-callback stream load does not rewind this callback's
			// scheduler-local relative deadline. Compare the producer's first
			// restored boundary, not the later test-timer observation time.
			expect(m_adc_blocks>=1 && m_adc_first_time==m_reference_adc_time
				&& m_adc_first_source==m_reference_adc_source && m_adc_first_tick==m_reference_adc_tick,
				"MFI exact first restored time, source words and provenance");
			m_mfi->enable_w(0); m_disabled_adc_source=m_adc_source; m_disabled_adc_blocks=m_adc_blocks; break;
		case 11:
			expect(m_adc_source==m_disabled_adc_source && m_adc_blocks==m_disabled_adc_blocks,"disabled MFI does not clock or notify");
			m_mfi->reset(); expect(!m_mfi->produced() && !m_mfi->consumed(),"MFI reset clears state");
			m_backend->reset_line_w(1);
			io.write_word(0x0e,999); io.write_word(0x0d,0); io.write_word(4,2);
			m_counter_anchor=machine().time(); m_counter_events=m_events;
			next_counter_phase(6); return;
		case 12:
			// Rewrite half a quarter after the load: the divider is not reset.
			io.write_word(0x0e,999); next_counter_phase(13); return;
		case 13:
			expect(io.read_word(0x0d)==1,"unchanged reload preserves fractional reference phase");
			expect(m_events==m_counter_events,"reload does not deliver compare early");
			next_counter_phase(25); return;
		case 14:
			expect(m_events==m_counter_events+1 && m_time==m_counter_anchor+reference_offset(24),
				"unchanged reload retains the exact armed compare deadline");
			io.write_word(0x0e,1999); next_counter_phase(36); return;
		case 15:
			expect(io.read_word(0x0d)==3,"changed reload also preserves the running divider phase");
			m_defer_adc=true; m_deferred_requests=0; m_adc_source=0;
			m_mfi->reset(); m_mfi->enable_w(1);
			++m_stage; m_timer->adjust(attotime::from_usec(31)); return;
		case 16:
			expect(m_mfi->produced()>=64 && m_mfi->produced()<96 && !m_mfi->consumed()
				&& m_deferred_requests==1,"two ready blocks retain one outstanding request");
			expect(c54x_test::capture_snapshot(machine(),m_saved)==STATERR_NONE,"save outstanding MFI block request");
			for (unsigned word=0;word<64;++word)
				expect(m_mfi->read()==0x1000+word,"deferred MFI data remains ordered");
			expect(m_deferred_requests==2,"completed read re-announces exactly one queued block");
			m_mfi->reset();
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved)==STATERR_NONE,"restore outstanding MFI block request");
			expect(!m_mfi->consumed() && m_deferred_requests==1,"restore unread data and request sink");
			for (unsigned word=0;word<64;++word)
				expect(m_mfi->read()==0x1000+word,"restored block data remains ordered");
			expect(m_deferred_requests==2 && !m_mfi->overruns() && !m_mfi->underruns(),
				"restored request rearms once without empty-buffer interrupt");
			m_mfi->enable_w(0);
			osd_printf_info("C54x compare boundary: PASS replacement=1 armed_restore=1 exact_time=1 ack_ports=1 mask_retains=1 periodic=1 raw_zero=1 cancellation=1 reset_abort=1 hagar_restore=1 mfi_clock_restore=1 mfi_irq_owner=1 reload_phase=1 mfi_request_restore=1 physical_latch_claim=0\n");
			fatalerror("C54x compare boundary tests complete");
		}
		++m_stage;
		u32 const delays[]={50,60,100,100,1000,1100,1100,7,13,13,30};
		m_timer->adjust(attotime::from_usec(delays[m_stage-1]));
	}
	required_device<nokia_dsp_c54x_device> m_backend;
	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_hagar_device> m_hagar;
	required_device<nokia_mfi_rx_device> m_mfi;
	emu_timer *m_timer=nullptr;
	unsigned m_stage=0,m_events=0;
	u64 m_tuple=0;
	attotime m_time,m_reference_time;
	attotime m_counter_anchor;
	unsigned m_counter_events=0;
	bool m_defer_adc=false;
	u64 m_deferred_requests=0;
	u64 m_adc_source=0,m_adc_consumed=0,m_adc_blocks=0,m_adc_last_tick=0;
	u64 m_reference_adc_source=0,m_reference_adc_tick=0;
	attotime m_adc_time,m_reference_adc_time;
	attotime m_adc_first_time;
	u64 m_adc_first_tick=0,m_adc_first_source=0,m_disabled_adc_source=0,m_disabled_adc_blocks=0;
	std::stringstream m_saved;
};
ROM_START(tms54scu)
	ROM_REGION16_LE(0x20000,"program",ROMREGION_ERASE00)
	ROM_REGION16_LE(0x20000,"data",ROMREGION_ERASE00)
ROM_END
}
SYST(2026,tms54scu,0,0,test,0,c54x_serial_test_state,empty_init,"MAME",
	"C54x raw compare boundary fixture",MACHINE_NO_SOUND_HW|MACHINE_NOT_WORKING)
