// license:BSD-3-Clause
// copyright-holders:Gaz

/* Development-only execution tests for the clean-room TMS320C54x core. */

#include "emu.h"
#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"

namespace {

class tms320c54x_test_state : public driver_device
{
public:
	tms320c54x_test_state(const machine_config &mconfig, device_type type,
			const char *tag) :
		driver_device(mconfig, type, tag),
		m_cpu(*this, "maincpu")
	{
	}

	void test(machine_config &config);
	void rom4(machine_config &config);

private:
	virtual void machine_start() override
	{
		m_check_timer = timer_alloc(FUNC(tms320c54x_test_state::check_results), this);
	}

	virtual void machine_reset() override
	{
		if (!strcmp(machine().system().name, "tms54rom4") &&
				!strcmp(machine().options().bios(), "cold"))
		{
			m_phase = 4;
			m_rom4_checks = 0;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (!strcmp(machine().system().name, "tms54rom4"))
		{
			m_rom4_checks = 0;
			auto &data = m_cpu->space(AS_DATA);
			u16 const *const initial = &memregion("dspdata")->as_u16();
			for (unsigned address = 0; address != 0x10000; ++address)
				data.write_word(address, initial[address]);
			u16 const *const drom = &memregion("dspdrom")->as_u16();
			for (unsigned address = 0xb000; address != 0xf000; ++address)
				data.write_word(address, drom[address]);
			// The sparse entry snapshot predates the firmware-provided challenge.
			// Supply the factory-profile record encoded for COBBA 00160010 while
			// retaining the deterministic peripheral-free entry state.
			static constexpr u16 challenge[] = {
				0xd6fb, 0x4394, 0xe437, 0xda16, 0x9668, 0x964f, 0x5cd4,
				0x32fe, 0x5be2, 0xdba6, 0x9643, 0x82d7, 0x0000, 0x0000
			};
			for (unsigned i = 0; i != std::size(challenge); ++i)
				data.write_word(0x0825 + i, challenge[i]);
			m_phase = 2;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x4b73);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x281f);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0x0052);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x000e);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0000004b73);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfffffffffe);
			static constexpr u16 ar[] = {
				0x0001, 0xb0bc, 0x0825, 0x06e3,
				0x001a, 0x12ca, 0x06e3, 0x0000
			};
			for (unsigned i = 0; i != std::size(ar); ++i)
				m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, ar[i]);
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}

		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);
		// CALL/RET, then a three-word RPT copy.
		program.write_word(0x0100, 0xf074);
		program.write_word(0x0101, 0x0200);
		program.write_word(0x0102, 0xec02);
		program.write_word(0x0103, 0xe598);

		// Reset source/destination pointers and execute a two-word CALL at the
		// end of an RPTB block three times.
		program.write_word(0x0104, 0x7712);
		program.write_word(0x0105, 0x0500);
		program.write_word(0x0106, 0x7714);
		program.write_word(0x0107, 0x0600);
		program.write_word(0x0108, 0x771a);
		program.write_word(0x0109, 0x0002);
		program.write_word(0x010a, 0xf072);
		program.write_word(0x010b, 0x010d);
		program.write_word(0x010c, 0xf074);
		program.write_word(0x010d, 0x0210);

		// Three circular moves starting at the final element must wrap through
		// the first two elements and return to the final element.
		program.write_word(0x010e, 0x7710);
		program.write_word(0x010f, 0x0001);
		program.write_word(0x0110, 0x7712);
		program.write_word(0x0111, 0x0802);
		program.write_word(0x0112, 0x7713);
		program.write_word(0x0113, 0x0900);
		program.write_word(0x0114, 0x7719);
		program.write_word(0x0115, 0x0003);
		program.write_word(0x0116, 0xec02);
		program.write_word(0x0117, 0xe5c9);
		program.write_word(0x0118, 0xe726);
		program.write_word(0x0119, 0xf074);
		program.write_word(0x011a, 0x0220);

		program.write_word(0x011b, 0x70f8);
		program.write_word(0x011c, 0x0904);
		program.write_word(0x011d, 0x0801);
		program.write_word(0x011e, 0x7714);
		program.write_word(0x011f, 0x0a00);
		program.write_word(0x0120, 0x70f8);
		program.write_word(0x0121, 0x0905);
		program.write_word(0x0122, 0x0014);
		program.write_word(0x0123, 0x7712);
		program.write_word(0x0124, 0x0800);
		program.write_word(0x0125, 0x7192);
		program.write_word(0x0126, 0x0014);
		program.write_word(0x0127, 0x7713);
		program.write_word(0x0128, 0x0800);
		program.write_word(0x0129, 0x1293);
		program.write_word(0x012a, 0xf0c8);
		program.write_word(0x012b, 0xf0e8);
		program.write_word(0x012c, 0xf0f8);
		program.write_word(0x012d, 0x7713);
		program.write_word(0x012e, 0x0800);
		program.write_word(0x012f, 0x1393);
		program.write_word(0x0130, 0xf330);
		program.write_word(0x0131, 0x00ff);
		program.write_word(0x0132, 0xf3e8);
		program.write_word(0x0133, 0x7df8);
		program.write_word(0x0134, 0x0800);
		program.write_word(0x0135, 0x0907);
		program.write_word(0x0136, 0x7cf8);
		program.write_word(0x0137, 0x0908);
		program.write_word(0x0138, 0x0907);
		// ROM4 receive enqueue: copy 26 words from *AR3+ into the circular
		// MDIRCV ring at *AR2+0%.  E59C's Y operand is AR2, not AR6.
		program.write_word(0x0139, 0x7710);
		program.write_word(0x013a, 0x0001);
		program.write_word(0x013b, 0x7712);
		program.write_word(0x013c, 0x0882);
		program.write_word(0x013d, 0x7713);
		program.write_word(0x013e, 0x1300);
		program.write_word(0x013f, 0x7719);
		program.write_word(0x0140, 0x0052);
		program.write_word(0x0141, 0xec19);
		program.write_word(0x0142, 0xe59c);
		program.write_word(0x0143, 0xf5e1);

		program.write_word(0x0200, 0x7680);
		program.write_word(0x0201, 0xbeef);
		program.write_word(0x0202, 0xfc00);
		program.write_word(0x0210, 0x1092);
		program.write_word(0x0211, 0x8094);
		program.write_word(0x0212, 0xfc00);
		program.write_word(0x0220, 0x61f8);
		program.write_word(0x0221, 0x0800);
		program.write_word(0x0222, 0x8000);
		program.write_word(0x0223, 0xfc30);
		program.write_word(0x0224, 0x7680);
		program.write_word(0x0225, 0xdead);
		program.write_word(0x0226, 0xfc00);

		// Final ROM4 challenge-transform loop. These are observed operands and
		// generic instruction encodings, not firmware code or a canned result.
		program.write_word(0x0300, 0x7712);
		program.write_word(0x0301, 0x13d9);
		program.write_word(0x0302, 0x7713);
		program.write_word(0x0303, 0x13d7);
		program.write_word(0x0304, 0x7714);
		program.write_word(0x0305, 0x1208);
		program.write_word(0x0306, 0x771a);
		program.write_word(0x0307, 0x0005);
		program.write_word(0x0308, 0xf072);
		program.write_word(0x0309, 0x030b);
		program.write_word(0x030a, 0xf074);
		program.write_word(0x030b, 0x0320);
		program.write_word(0x0320, 0x108a);
		program.write_word(0x0321, 0xf493);
		program.write_word(0x0322, 0x1a8b);
		program.write_word(0x0323, 0x6d8c);
		program.write_word(0x0324, 0x1c84);
		program.write_word(0x0325, 0x8084);
		program.write_word(0x0326, 0xfc00);
		// Long-immediate repeat executes the following two-word instruction
		// exactly lk + 1 times.
		program.write_word(0x0350, 0xf062);
		program.write_word(0x0351, 0x1234);
		program.write_word(0x0352, 0x4ef8);
		program.write_word(0x0353, 0x090c);
		program.write_word(0x0354, 0xe800);
		program.write_word(0x0355, 0xf070);
		program.write_word(0x0356, 0x0002);
		program.write_word(0x0357, 0x6d10);
		program.write_word(0x0358, 0xf5e1);
		program.write_word(0x0360, 0x76f8);
		program.write_word(0x0361, 0x0910);
		program.write_word(0x0362, 0x5678);
		program.write_word(0x0363, 0x7214);
		program.write_word(0x0364, 0x0912);
		program.write_word(0x0365, 0x57f8);
		program.write_word(0x0366, 0x0914);
		program.write_word(0x0367, 0xf793);
		program.write_word(0x0368, 0xff0c);
		program.write_word(0x0369, 0xf495);
		program.write_word(0x036a, 0xf793);
		program.write_word(0x036b, 0xf065);
		program.write_word(0x036c, 0x00ff);
		program.write_word(0x036d, 0xf054);
		program.write_word(0x036e, 0x00f0);
		program.write_word(0x036f, 0xf5e1);
		program.write_word(0x0370, 0xf171);
		program.write_word(0x0371, 0x0001);
		program.write_word(0x0372, 0x6d10);
		program.write_word(0x0373, 0xf5e1);
		program.write_word(0x0374, 0x47f8);
		program.write_word(0x0375, 0x0918);
		program.write_word(0x0376, 0x6bf8);
		program.write_word(0x0377, 0x091a);
		program.write_word(0x0378, 0x0001);
		program.write_word(0x0379, 0xf5e1);
		program.write_word(0x037a, 0xf070);
		program.write_word(0x037b, 0x0001);
		program.write_word(0x037c, 0x7d92);
		program.write_word(0x037d, 0x0924);
		program.write_word(0x037e, 0xf5e1);
		program.write_word(0x0380, 0xf273);
		program.write_word(0x0381, 0x0390);
		program.write_word(0x0382, 0xf495);
		program.write_word(0x0383, 0xf495);
		program.write_word(0x0384, 0xf5e1);
		program.write_word(0x0390, 0xf5e1);
		program.write_word(0x0392, 0xf4eb);
		program.write_word(0x0398, 0xf5e1);
		program.write_word(0x039a, 0xfa45);
		program.write_word(0x039b, 0x03a0);
		program.write_word(0x039c, 0xf495);
		program.write_word(0x039d, 0xf495);
		program.write_word(0x039e, 0xf5e1);
		program.write_word(0x03a0, 0xf5e1);
		program.write_word(0x03a2, 0x6ff8);
		program.write_word(0x03a3, 0x0920);
		program.write_word(0x03a4, 0x0c48);
		program.write_word(0x03a5, 0xf5e1);
		program.write_word(0x03a6, 0xf5e2);
		program.write_word(0x03b0, 0xf5e1);
		program.write_word(0x03b2, 0x09f8);
		program.write_word(0x03b3, 0x0918);
		program.write_word(0x03b4, 0xf5e1);
		program.write_word(0x03b6, 0xfc4b);
		program.write_word(0x03b7, 0xf5e1);
		program.write_word(0x03c0, 0xf5e1);
		program.write_word(0x03c2, 0xf947);
		program.write_word(0x03c3, 0x03d0);
		program.write_word(0x03c4, 0xf5e1);
		program.write_word(0x03d0, 0xf5e1);
		program.write_word(0x03d2, 0xf520);
		program.write_word(0x03d3, 0xf5e1);
		program.write_word(0x03d4, 0xf070);
		program.write_word(0x03d5, 0x0001);
		program.write_word(0x03d6, 0x7f92);
		program.write_word(0x03d7, 0xf5e1);
		program.write_word(0x03d8, 0x3292);
		program.write_word(0x03d9, 0xf5e1);
		program.write_word(0x03da, 0xeeff);
		program.write_word(0x03db, 0xee02);
		program.write_word(0x03dc, 0xf5e1);
		program.write_word(0x03e0, 0xf0ff);
		program.write_word(0x03e1, 0xf5e1);
		data.write_word(0x0918, 1);
		data.write_word(0x091a, 0);
		data.write_word(0x0920, 0xaaaa);
		data.write_word(0x0921, 0xbbbb);
		data.write_word(0x0912, 0xabcd);
		data.write_word(0x0914, 0x1234);
		data.write_word(0x0915, 0x5678);

		data.write_word(0x0500, 0x1111);
		data.write_word(0x0501, 0x2222);
		data.write_word(0x0502, 0x3333);
		data.write_word(0x0800, 0xaaaa);
		data.write_word(0x0801, 0xbbbb);
		data.write_word(0x0802, 0xcccc);
		data.write_word(0x13d2, 0x6d4d);
		data.write_word(0x13d3, 0xc431);
		data.write_word(0x13d4, 0xbfe4);
		data.write_word(0x13d5, 0x5d91);
		data.write_word(0x13d6, 0x71b1);
		data.write_word(0x13d7, 0x9ac9);
		data.write_word(0x13d8, 0x6d4d);
		data.write_word(0x13d9, 0xc431);
		data.write_word(0x1202, 0x71b1);
		data.write_word(0x1203, 0x9ac9);
		data.write_word(0x1204, 0x6d4d);
		data.write_word(0x1205, 0xc431);
		data.write_word(0x1206, 0xbfe4);
		data.write_word(0x1207, 0x5d91);
		for (unsigned i = 0; i != 26; ++i)
			data.write_word(0x1300 + i, 0x6000 + i);
		m_phase = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0100);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0700);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0400);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0500);

		m_check_timer->adjust(attotime::from_usec(100));
	}

	void program_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).ram();
	}

	void data_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).ram();
	}

	void rom4_program_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).rom().region("dspprg", 0);
	}

	void rom4_data_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).ram();
	}

	void expect(bool condition, const char *message)
	{
		if (!condition)
			throw emu_fatalerror("TMS320C54x core conformance: %s", message);
	}

	TIMER_CALLBACK_MEMBER(check_results)
	{
		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);
		if (m_phase == 5)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
					"long-immediate RPT terminal IDLE3");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3,
					"long-immediate RPT iteration count");
			 expect(data.read_word(0x090c) == 0x1234 &&
					data.read_word(0x090d) == 0,
					"long-immediate load and long-memory store");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0360);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_phase = 6;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 6)
		{
			expect(data.read_word(0x0910) == 0x5678,
					"absolute STM extension order");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xabcd,
					"data-memory to MMR move");
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(0x12345678 ^ ((u64(1) << 40) - 1)),
					"absolute double-word load and accumulator complement");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00ff0f00,
					"shifted long-immediate accumulator XOR");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0370);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_phase = 7;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 7)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0,
					"repeat-with-zero clears its accumulator");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0) == 2,
					"repeat-with-zero iteration count");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0374);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_phase = 8;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 8)
		{
			expect(data.read_word(0x091a) == 2,
					"memory-counted multiword repeat iteration count");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x037a);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0920);
			m_phase = 9;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 9)
		{
			osd_printf_info("TMS320C54x repeated MVDP result: %04x,%04x\n",
					m_cpu->space(AS_PROGRAM).read_word(0x0924),
					m_cpu->space(AS_PROGRAM).read_word(0x0925));
			expect(m_cpu->space(AS_PROGRAM).read_word(0x0924) == 0xaaaa &&
					m_cpu->space(AS_PROGRAM).read_word(0x0925) == 0xbbbb,
					"repeated MVDP advances its program destination");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0380);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 10;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 10)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0391,
					"delayed branch executes both delay-slot words");
			data.write_word(0x02ff, 0x0398);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0392);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x02ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 11;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 11)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0399 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
					"interrupt return restores PC/SP and enables interrupts");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x039a);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 12;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03a1,
					"delayed accumulator-equal branch");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03a2);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 13;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 13)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0xaaaa00,
					"extended absolute load with positive shift");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03a6);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x03b0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 14;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 14)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03b1,
					"accumulator-indirect branch");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03b2);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 15;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 15)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 4,
					"data-memory subtract from accumulator B");
			data.write_word(0x02ff, 0x03c0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03b6);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x02ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, (u64(1) << 40) - 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 16;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03c1 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"conditional return on negative accumulator B");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03c2);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 17;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 17)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03d1 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
					data.read_word(0x02ff) == 0x03c4,
					"conditional call on non-positive accumulator A");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03d2);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 9);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 18;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 18)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 6,
					"accumulator subtract with independent destination");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03d4);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0940);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0920);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 19;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 19)
		{
			expect(m_cpu->space(AS_PROGRAM).read_word(0x0940) == 0xaaaa &&
					m_cpu->space(AS_PROGRAM).read_word(0x0941) == 0xbbbb,
					"repeated accumulator-addressed program write");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03d8);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0918);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 20;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 20)
		{
			expect((m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x1f) == 1,
					"data-memory load into ST1.ASM");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03da);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 21;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 21)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301,
					"signed stack-frame adjustment");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x35);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 22;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 22)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1a,
					"arithmetic accumulator shift right");
			program.write_word(0x03e4, 0x07f8); // ADDC *(absolute), B
			program.write_word(0x03e5, 0x0920);
			program.write_word(0x03e6, 0xf5e1);
			data.write_word(0x0920, 0xabcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03e4);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 23;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 23)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0xbe02,
					"ADDC absolute operand and carry input");
			expect(!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
					"ADDC 32-bit carry output");
			program.write_word(0x03e8, 0x6e8f); // BANZD 03efh, *AR7-
			program.write_word(0x03e9, 0x03ef);
			program.write_word(0x03ea, 0xe801);
			program.write_word(0x03eb, 0xe902);
			program.write_word(0x03ec, 0xe803);
			program.write_word(0x03ef, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03e8);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 24;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
					"BANZD executes two delay words");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0,
					"BANZD address-register modification");
			program.write_word(0x03f0, 0x24f8); // MPYU *(absolute), A
			program.write_word(0x03f1, 0x0922);
			program.write_word(0x03f2, 0xf5e1);
			data.write_word(0x0922, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03f0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 25;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 25)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1fffe,
					"MPYU unsigned operands");
			program.write_word(0x03f4, 0x31f8); // MPYA *(absolute)
			program.write_word(0x03f5, 0x0924);
			program.write_word(0x03f6, 0xf5e1);
			data.write_word(0x0924, 0xfffe);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03f4);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 3U << 16);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 26;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 26)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
					((u64(1) << 40) - 6), "MPYA signed product");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe,
					"MPYA loads T");
			program.write_word(0x03f8, 0xf944); // CC 0400h, ANEQ
			program.write_word(0x03f9, 0x0400);
			program.write_word(0x03fa, 0xf5e1);
			program.write_word(0x0400, 0xe85a);
			program.write_word(0x0401, 0xfc00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03f8);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 27;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 27)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x5a,
					"conditional call ANEQ");
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"conditional call return stack");
			program.write_word(0x0404, 0x7ef8); // READA *(absolute)
			program.write_word(0x0405, 0x0926);
			program.write_word(0x0406, 0x7ff8); // WRITA *(absolute)
			program.write_word(0x0407, 0x0927);
			program.write_word(0x0408, 0xf5e1);
			program.write_word(0x0925, 0xcafe);
			data.write_word(0x0927, 0xbeef);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0404);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0925);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 28;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 28)
		{
			expect(data.read_word(0x0926) == 0xcafe,
					"READA accumulator-addressed data read");
			expect(program.read_word(0x0925) == 0xbeef,
					"WRITA accumulator-addressed data write");
			program.write_word(0x040c, 0xf485); // ABS A, A
			program.write_word(0x040d, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x040c);
			m_cpu->set_state_int(tms320c54x_device::STATE_A,
					(u64(1) << 40) - 7);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 29;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 29)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 7,
					"ABS signed accumulator magnitude");
			program.write_word(0x0410, 0x1ef8); // SUBC *(absolute), A
			program.write_word(0x0411, 0x0928);
			program.write_word(0x0412, 0xf5e1);
			data.write_word(0x0928, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0410);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 30;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 30)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10001,
					"SUBC conditional subtract and quotient bit");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800,
					"SUBC successful subtraction carry");
			program.write_word(0x0414, 0x1092); // LD *AR2+, A
			program.write_word(0x0415, 0xf5e1);
			data.write_word(0x0930, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0414);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0930);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0000);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 31;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 31)
		{
			expect((m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe000) == 0xa000,
					"standard-mode indirect operand preserves ST0.ARP");
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0414);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0930);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0020);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 32;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 32)
		{
			expect((m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe000) == 0x4000,
					"compatibility-mode indirect operand updates ST0.ARP");
			program.write_word(0x0418, 0xf0b0); // OR A, -16, A
			program.write_word(0x0419, 0xf5e1);
			constexpr u64 negative = (u64(0xff) << 32) | 0x80000000U;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0418);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, negative);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 33;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 33)
		{
			constexpr u64 negative = (u64(0xff) << 32) | 0x80000000U;
			osd_printf_info("TMS320C54x logical shift: actual=%010llx expected=%010llx\n",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					(unsigned long long)((negative | (negative >> 16)) & ((u64(1) << 40) - 1)));
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					((negative | (negative >> 16)) & ((u64(1) << 40) - 1)),
					"logical accumulator right shift zero-fills guard bits");
			program.write_word(0x041c, 0xed18); // LD #-8, ASM
			program.write_word(0x041d, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x041c);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0xa5a5);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 34;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 34)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0xa5b8,
					"short-immediate load into ST1.ASM");
			program.write_word(0x0420, 0x771a); // STM #1, BRC
			program.write_word(0x0421, 0x0001);
			program.write_word(0x0422, 0xf272); // RPTBD 0428h
			program.write_word(0x0423, 0x0428);
			program.write_word(0x0424, 0xe801); // delay slot 1
			program.write_word(0x0425, 0xe902); // delay slot 2
			program.write_word(0x0426, 0x6d10); // MAR *AR0+
			program.write_word(0x0427, 0xf495);
			program.write_word(0x0428, 0xf495);
			program.write_word(0x0429, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0420);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 35;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 35)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
					"RPTBD executes both delay slots once");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x4000),
					"RPTBD repeats its body and retires BRAF");
			program.write_word(0x042c, 0xa43a); // MPY *AR5, *AR4+, A
			program.write_word(0x042d, 0xf5e1);
			data.write_word(0x0940, 3);
			data.write_word(0x0950, 0xfffe);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x042c);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0940);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0950);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 36;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 36)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					((u64(1) << 40) - 6) &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe,
					"dual-memory multiply result and T load");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0941 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0950,
					"dual-memory multiply address updates");
			program.write_word(0x0430, 0xb336); // MAC *AR5, *AR4-, B, B
			program.write_word(0x0431, 0xf5e1);
			data.write_word(0x0941, 4);
			data.write_word(0x0950, 0xfffd);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0430);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0941);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0950);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 37;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 37)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 8 &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd,
					"dual-memory signed multiply-accumulate");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0940 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0950,
					"dual-memory MAC address updates");
			program.write_word(0x0434, 0xd631); // ST B,*AR3 || MACR *AR5,A
			program.write_word(0x0435, 0xf5e1);
			data.write_word(0x0950, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0434);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0960);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0950);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10001);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 38;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 38)
		{
			expect(data.read_word(0x0960) == 0x1234,
					"parallel store uses the pre-accumulate source");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000,
					"parallel rounded multiply-accumulate");
			program.write_word(0x0438, 0xe210); // SQDST *AR3,*AR2
			program.write_word(0x0439, 0xf5e1);
			data.write_word(0x0960, 5);
			data.write_word(0x0970, 8);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0438);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0970);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0960);
			m_cpu->set_state_int(tms320c54x_device::STATE_A,
					(u64(0xff) << 32) | (u64(0xfffe) << 16));
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 39;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 39)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					((u64(1) << 40) - 0x30000),
					"square-distance signed vector difference");
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 14,
					"square-distance accumulation of old A high half");
			program.write_word(0x043c, 0xfa44); // BCD 0442h, ANEQ
			program.write_word(0x043d, 0x0442);
			program.write_word(0x043e, 0xe802);
			program.write_word(0x043f, 0xe903);
			program.write_word(0x0440, 0xf5e1);
			program.write_word(0x0442, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x043c);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 40;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 40)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0443 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 3,
					"delayed accumulator-not-equal branch and delay slots");
			program.write_word(0x0444, 0xf484); // NEG A
			program.write_word(0x0445, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0444);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 41;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 41)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					((u64(1) << 40) - 5) &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
					"accumulator negate result and carry");
			osd_printf_info("TMS320C54x core conformance: PASS\n");
			throw emu_fatalerror(0, "TMS320C54x core tests complete");
		}
		if (m_phase == 4)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
					!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
			{
				expect(++m_rom4_checks < 10000, "ROM4 cold execution timeout");
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			osd_printf_info("TMS320C54x ROM4 cold frontier: pc=%04x sp=%04x "
					"pmst=%04x idle=%u\n",
					u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
					u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)));
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
					"ROM4 cold loader-upload boundary");
			expect(m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01,
					"ROM4 cold loader entry PC");
			expect(m_cpu->space(AS_PROGRAM).read_word(0x0f00) == 0,
					"ROM4 loader1 must be MCU-uploaded");
			throw emu_fatalerror(0, "TMS320C54x ROM4 cold frontier complete");
		}
		if (m_phase == 3)
		{
			if (!m_irq_raised)
			{
				expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
						"IDLE3 entry");
				expect(m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0401,
						"IDLE3 continuation PC");
				m_cpu->set_input_line(2, ASSERT_LINE);
				m_irq_raised = true;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			expect(data.read_word(0x0a10) == 0xcafe,
					"maskable interrupt vector execution");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
					"interrupt-vector terminal IDLE3");
			m_cpu->set_input_line(2, CLEAR_LINE);
			m_phase = 1;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 2)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
					!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
			{
				expect(++m_rom4_checks < 10000,
						"ROM4 execution frontier timeout");
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			const u16 pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
			static constexpr u16 expected_response[] = {
				0x3532, 0x0000, 0xffff, 0xffff, 0xff0f, 0x0000, 0x0078,
				0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x087c, 0x0000
			};
			osd_printf_info("TMS320C54x ROM4 execution frontier: pc=%04x "
					"header=%04x,%04x ar2=%04x ar3=%04x\n", pc,
					data.read_word(0x1200), data.read_word(0x1201),
					u16(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
					u16(m_cpu->state_int(tms320c54x_device::STATE_AR3)));
			for (unsigned i = 0; i != std::size(expected_response); ++i)
			{
				const u16 observed = data.read_word(0x1200 + i);
				if (observed != expected_response[i])
					osd_printf_info("TMS320C54x ROM4 response mismatch word=%u actual=%04x expected=%04x\n",
							i, observed, expected_response[i]);
				expect(observed == expected_response[i],
						"complete ROM4 challenge response");
			}
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
					"ROM4 DSP sleep boundary");
			// IDLE3 at 0x7ec9 advances PC before waiting for a wake source.
			expect(pc == 0x7eca, "ROM4 DSP sleep PC");
			throw emu_fatalerror(0, "TMS320C54x ROM4 frontier complete");
		}
		if (m_phase)
		{
			static constexpr u16 expected[] = {
				0x1cee, 0x7cb6, 0xd2a3, 0xb986, 0x4c57, 0xe65e
			};
			osd_printf_info("TMS320C54x transform result: %04x,%04x,%04x,%04x,%04x,%04x\n",
					data.read_word(0x1202), data.read_word(0x1203),
					data.read_word(0x1204), data.read_word(0x1205),
					data.read_word(0x1206), data.read_word(0x1207));
			for (unsigned i = 0; i != std::size(expected); ++i)
				expect(data.read_word(0x1202 + i) == expected[i],
						"ROM4 challenge transform terminal loop");
			expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0,
					"ROM4 challenge transform repeat count");
			m_phase = 5;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0350);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}

		osd_printf_info("TMS320C54x test state: pc=%04x sp=%04x brc=%04x "
				"rpt=%04x,%04x,%04x block=%04x,%04x,%04x\n",
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_BRC)),
				data.read_word(0x0400), data.read_word(0x0401), data.read_word(0x0402),
				data.read_word(0x0600), data.read_word(0x0601), data.read_word(0x0602));
		expect(data.read_word(0x0700) == 0xbeef, "CALL/RET continuation");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"balanced system stack");
		expect(data.read_word(0x0400) == 0x1111 &&
				data.read_word(0x0401) == 0x2222 &&
				data.read_word(0x0402) == 0x3333, "RPT memory transfer");
		expect(data.read_word(0x0600) == 0x1111 &&
				data.read_word(0x0601) == 0x2222 &&
				data.read_word(0x0602) == 0x3333, "RPTB multiword CALL");
		expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0,
				"RPTB terminal count");
		expect(!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x4000),
				"RPTB terminal state clears ST1.BRAF");
		expect(data.read_word(0x0900) == 0xcccc &&
				data.read_word(0x0901) == 0xaaaa &&
				data.read_word(0x0902) == 0xbbbb,
				"circular addressing wrap order");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0802,
				"circular addressing final pointer");
		expect(data.read_word(0x0903) == 0x0000,
				"BITF and conditional return");
		expect(data.read_word(0x0904) == 0xbbbb,
				"absolute data-memory copy");
		expect(data.read_word(0x0905) == 0x0a00,
				"memory-mapped auxiliary-register read");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xaaaa,
				"memory-mapped auxiliary-register write");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00aa00aa,
				"unsigned data operand load and accumulator XOR shift");
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0xaa00,
				"immediate accumulator mask and rotate");
		expect(m_cpu->space(AS_PROGRAM).read_word(0x0907) == 0xaaaa,
				"data-to-program memory transfer");
		expect(data.read_word(0x0908) == 0xaaaa,
				"program-to-data memory transfer");
		for (unsigned i = 0; i != 26; ++i)
			expect(data.read_word(0x0882 + i) == 0x6000 + i,
					"MVDD dual-operand circular transfer");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x089c &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x131a,
				"MVDD dual-operand address updates");

		// IDLE3 must retain its continuation PC, then an enabled source must
		// vector through PMST.IPTR and preserve the return address on stack.
		program.write_word(0x0048, 0x7680);
		program.write_word(0x0049, 0xcafe);
		program.write_word(0x004a, 0xf5e1);
		program.write_word(0x0400, 0xf5e1);
		m_phase = 3;
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0400);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0000);
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0000);
		m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x0004);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0a10);
		m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_check_timer->adjust(attotime::from_usec(100));
	}

	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_check_timer = nullptr;
	unsigned m_phase = 0;
	unsigned m_rom4_checks = 0;
	bool m_irq_raised = false;
};

void tms320c54x_test_state::test(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::data_map);
}

void tms320c54x_test_state::rom4(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::rom4_program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::rom4_data_map);
}

ROM_START(tms54test)
ROM_END

ROM_START(tms54rom4)
	ROM_SYSTEM_BIOS(0, "entry", "Transform-entry snapshot")
	ROM_SYSTEM_BIOS(1, "cold", "Cold reset from mask ROM and DROM")
	ROM_REGION16_LE(0x80000, "dspprg", 0)
	ROMX_LOAD("transform_entry_prog.bin", 0, 0x80000,
			CRC(99757118) SHA1(0a1da67d21f4c333acd331271c3d9f08e896008f),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(0))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(1))
	ROM_REGION16_LE(0x20000, "dspdata", 0)
	ROMX_LOAD("transform_entry_data.bin", 0, 0x20000,
			CRC(bef92101) SHA1(1c547eb7fd457d95cdf7956462e80a40dbadb46e),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(0))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(1))
	ROM_REGION16_LE(0x20000, "dspdrom", 0)
	ROM_LOAD16_WORD_SWAP("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d))
ROM_END

} // anonymous namespace

SYST(2026, tms54test, 0, 0, test, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x core conformance tests",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, tms54rom4, 0, 0, rom4, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x ROM4 private execution fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
