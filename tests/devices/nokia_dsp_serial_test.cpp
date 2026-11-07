// license:BSD-3-Clause
// copyright-holders:Gaz
// Executable backend compare-boundary contract, not SCU silicon timing proof.
#include "emu.h"
#include "nokia_ccont.h"
#include "nokia_cobba.h"
#include "nokia_dsp_c54x.h"
#include "nokia_dspif.h"
#include "tms320c54x_test_snapshot.h"
#include <sstream>

namespace {
class c54x_serial_test_state : public driver_device
{
public:
	c54x_serial_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config,type,tag), m_backend(*this,"backend"),m_cpu(*this,"backend:cpu") { }
	void test(machine_config &config)
	{
		NOKIA_DSPIF(config,"transport",0);
		NOKIA_COBBA(config,"codec",0);
		NOKIA_CCONT(config,"power",0);
		NOKIA_DSP_C54X(config,m_backend,13'000'000);
		m_backend->set_transport_tag("^transport"); m_backend->set_cobba_tag("^codec");
		m_backend->set_ccont_tag("^power"); m_backend->set_program_rom_tag("^program");
		m_backend->set_data_rom_tag("^data");
		m_backend->transfer_compare_cb().set(FUNC(c54x_serial_test_state::compare));
	}
protected:
	void machine_start() override
	{
		m_timer=timer_alloc(FUNC(c54x_serial_test_state::step),this);
		save_item(NAME(m_events)); save_item(NAME(m_tuple)); save_item(NAME(m_time));
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
		++m_events; m_tuple=tuple; m_time=machine().time();
		expect(m_cpu->space(AS_IO).read_word(0x0d)==u16(tuple),"event is on its own counter tick");
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		auto &io=m_cpu->space(AS_IO);
		switch(m_stage)
		{
		case 0:
			m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_backend->reset_line_w(1);
			io.write_word(0x0e,999); io.write_word(0x0d,0);
			io.write_word(0x30,0x100); io.write_word(0x31,0xaaaa); io.write_word(0x32,0xbbbb);
			io.write_word(4,100); break;
		case 1:
			expect(!m_events,"no early event before target100");
			io.write_word(0x31,0xcccc); io.write_word(4,200); break;
		case 2:
			expect(!m_events && !(io.read_word(1)&0x1000),"replacement cancels the old deadline");
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
			break;
		case 4:
			expect(m_events==1 && m_tuple==0x0100ccccbbbb00c8ULL && m_time==m_reference_time,"exact restored tuple/time");
			io.write_word(1,0x1000); expect(!(io.read_word(1)&0x1000),"acknowledge only clears pending");
			io.write_word(0x30,0); break;
		case 5:
			expect(m_events==2 && m_tuple==0x0000ccccbbbb00c8ULL,"next frame still observes raw zero control");
			io.write_word(4,15000); expect(io.read_word(1)&0x1000,"cancel does not acknowledge pending");
			io.write_word(2,0x1000); expect(!(io.read_word(1)&0x1000),"P02 also acknowledges pending");
			break;
		case 6:
			expect(m_events==2,"sentinel cancels later matches");
			io.write_word(4,100); m_backend->reset_line_w(0); break;
		case 7:
			expect(m_events==2 && !(io.read_word(1)&0x1000),"held reset cancels an armed compare");
			osd_printf_info("C54x compare boundary: PASS replacement=1 armed_restore=1 exact_time=1 ack_ports=1,2 periodic=1 raw_zero=1 cancellation=1 reset_abort=1 physical_latch_claim=0\n");
			fatalerror("C54x compare boundary tests complete");
		}
		++m_stage;
		u32 const delays[]={50,60,100,100,1000,1100,1100};
		m_timer->adjust(attotime::from_usec(delays[m_stage-1]));
	}
	required_device<nokia_dsp_c54x_device> m_backend;
	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_timer=nullptr;
	unsigned m_stage=0,m_events=0;
	u64 m_tuple=0;
	attotime m_time,m_reference_time;
	std::stringstream m_saved;
};
ROM_START(tms54scu)
	ROM_REGION16_LE(0x20000,"program",ROMREGION_ERASE00)
	ROM_REGION16_LE(0x20000,"data",ROMREGION_ERASE00)
ROM_END
}
SYST(2026,tms54scu,0,0,test,0,c54x_serial_test_state,empty_init,"MAME",
	"C54x raw compare boundary fixture",MACHINE_NO_SOUND_HW|MACHINE_NOT_WORKING)
