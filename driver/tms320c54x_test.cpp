// license:BSD-3-Clause
// copyright-holders:Gaz

/* Development-only execution tests for the clean-room TMS320C54x core. */

#include "emu.h"
#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_dspif.h"
#include <sstream>

namespace {

class tms320c54x_test_state : public driver_device
{
public:
	tms320c54x_test_state(const machine_config &mconfig, device_type type,
			const char *tag) :
		driver_device(mconfig, type, tag),
		m_cpu(*this, "maincpu"),
		m_transport(*this, "dspif")
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
		map(0x0060, 0x0061).r(FUNC(tms320c54x_test_state::repeat_irq_r));
	}
	void io_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).ram();
		map(0x0123, 0x0123).r(FUNC(tms320c54x_test_state::test_port_r));
		map(0x0124, 0x0124).w(FUNC(tms320c54x_test_state::test_port_w));
	}
	u16 test_port_r()
	{
		const u64 cycle = m_cpu->total_cycles();
		if (m_port_reads++ == 0)
			m_first_port_cycle = cycle;
		else
			m_last_port_cycle = cycle;
		return 0xabcd;
	}
	void test_port_w(u16 value)
	{
		const u64 cycle = m_cpu->total_cycles();
		if (m_port_writes++ == 0)
		{
			m_first_port_cycle = cycle;
			m_first_port_value = value;
		}
		else
		{
			if (m_port_writes == 2)
			{
				m_middle_port_value = value;
				m_middle_port_cycle = cycle;
			}
			m_last_port_cycle = cycle;
			m_last_port_value = value;
		}
	}
	u16 repeat_irq_r(offs_t offset)
	{
		if (offset)
		{
			m_irq_accumulator = m_cpu->state_int(tms320c54x_device::STATE_A);
			return 0;
		}
		m_last_operand_cycle = m_cpu->total_cycles();
		if (++m_repeat_reads == 1)
		{
			m_first_operand_cycle = m_last_operand_cycle;
		}
		if (m_repeat_reads == m_irq_trigger_read)
			m_cpu->set_input_line(2, ASSERT_LINE);
		return 1;
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

	void expect_opcode(u16 opcode, bool condition, const char *message)
	{
		expect(condition, message);
		logerror("[opassert] op=%04x\n", opcode);
	}

	TIMER_CALLBACK_MEMBER(check_results)
	{
		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);
		static constexpr u8 rom4_saved_mmr[] = {
			0x0e, 0x10, 0x11, 0x13, 0x14, 0x15,
			0x16, 0x17, 0x19, 0x1a, 0x1b, 0x1c
		};
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
			expect_opcode(0xf273, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
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
			expect_opcode(0xf4eb, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
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
			expect_opcode(0xe801, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
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
			expect_opcode(0xf0b0, m_cpu->state_int(tms320c54x_device::STATE_A) ==
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
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // Standard addressing: MAR names physical AR0.
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
			expect_opcode(0xa43a, m_cpu->state_int(tms320c54x_device::STATE_A) ==
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
			expect_opcode(0xb336, m_cpu->state_int(tms320c54x_device::STATE_B) == 8 &&
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
			expect_opcode(0xe210, m_cpu->state_int(tms320c54x_device::STATE_A) ==
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
			expect_opcode(0xfa44, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
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
			expect_opcode(0xf484, m_cpu->state_int(tms320c54x_device::STATE_A) ==
					((u64(1) << 40) - 5) &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
					"accumulator negate result and carry");
			program.write_word(0x0448, 0xf4e3); // CALA A
			program.write_word(0x0449, 0xf5e1);
			program.write_word(0x0450, 0x76f8);
			program.write_word(0x0451, 0x0944);
			program.write_word(0x0452, 0xbeef);
			program.write_word(0x0453, 0xfc00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0448);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0450);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 42;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 42)
		{
			expect(data.read_word(0x0944) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x044a &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"CALA A target and return address");
			program.write_word(0x0460, 0xf5e3); // CALA B
			program.write_word(0x0461, 0xf5e1);
			program.write_word(0x0468, 0x76f8);
			program.write_word(0x0469, 0x0945);
			program.write_word(0x046a, 0xcafe);
			program.write_word(0x046b, 0xfc00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0460);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0468);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 43;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 43)
		{
			expect(data.read_word(0x0945) == 0xcafe &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0462 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"CALA B target and return address");
			program.write_word(0x0470, 0xf6e3); // CALAD A
			program.write_word(0x0471, 0xe801);
			program.write_word(0x0472, 0xe902);
			program.write_word(0x0473, 0xf5e1);
			program.write_word(0x0478, 0x76f8);
			program.write_word(0x0479, 0x0946);
			program.write_word(0x047a, 0x1234);
			program.write_word(0x047b, 0xfc00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0470);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0478);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 44;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 44)
		{
			expect(data.read_word(0x0946) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0474 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
					"CALAD delay slots and return address");
			program.write_word(0x0480, 0xf120); // LD #ffff, B
			program.write_word(0x0481, 0xffff);
			program.write_word(0x0482, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0480);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 45;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 45 && m_phase <= 48)
		{
			static constexpr u64 expected[] = {
				0x000000ffff, 0xffffff8000, 0xff80000000, 0x00ffff0000
			};
			osd_printf_info("TMS320C54x immediate B load: phase=%u pc=%04x a=%010llx b=%010llx expected=%010llx\n",
					m_phase, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					m_cpu->state_int(tms320c54x_device::STATE_A),
					m_cpu->state_int(tms320c54x_device::STATE_B), expected[m_phase - 45]);
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
					m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0483 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == expected[m_phase - 45],
					"long-immediate B load destination, SXM and guard extension");
			if (m_phase != 48)
			{
				++m_phase;
				program.write_word(0x0480, m_phase == 46 ? 0xf120 : 0xf162);
				program.write_word(0x0481, m_phase == 48 ? 0xffff : 0x8000);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0480);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_phase == 48 ? 0 : 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x0480, 0xf02f); // LD #8000, 15, A
			program.write_word(0x0481, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0480);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 49;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 49 || m_phase == 50)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 49 ? 0x0040000000ULL : 0xffc0000000ULL) &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00ffff0000,
					"shifted immediate A load respects SXM and preserves B");
			if (m_phase == 49)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0480);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 50;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x0490, 0xf585); // ABS A, B
			program.write_word(0x0491, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0312345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0490);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 51;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 51 || m_phase == 52)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(m_phase == 51 ? 0x007fffffffULL : 0) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
					(m_phase == 52), "ABS saturation and zero carry");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0200,
					"ABS guard-bit overflow is sticky across a zero result");
			if (m_phase == 51)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0490);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 52;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x04a0, 0xf000); // ADD #1, A
			program.write_word(0x04a1, 1);
			program.write_word(0x04a2, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x007fffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04a0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 53;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 53 || m_phase == 54)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 53 ? 0x007fffffffULL : 0xff80000000ULL),
					"immediate ADD/SUB signed saturation");
			expect((m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) ==
					(m_phase == 53 ? 0x0400 : 0x0c00),
					"immediate ADD/SUB overflow and bit-32 carry");
			if (m_phase == 53)
			{
				program.write_word(0x04a0, 0xf010); // SUB #1, A
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04a0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 54;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x04b0, 0xf47f); // SFTA A, -1
			program.write_word(0x04b1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04b0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 55;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 55 || m_phase == 56)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 55 ? 0x7fffffffffULL : 0xffffffffffULL) &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
					"SFTA right shift respects SXM and copies outgoing bit to carry");
			if (m_phase == 55)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04b0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 56;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x04c0, 0x00f8); // ADD *abs, A
			program.write_word(0x04c1, 0x0a80);
			program.write_word(0x04c2, 0xf5e1);
			data.write_word(0x0a80, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04c0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 57;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 57 || m_phase == 58)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 57 ? 0 : 0xffffffffffULL) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
					(m_phase == 57), "memory ADD carry and SUB borrow at bit 32");
			if (m_phase == 57)
			{
				program.write_word(0x04c0, 0x08f8); // SUB *abs, A
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04c0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 58;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200);
			program.write_word(0x04d0, 0xf02f);
			program.write_word(0x04d1, 0xffff);
			program.write_word(0x04d2, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04d0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 59;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 59)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x007fff8000ULL &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
					"shifted unsigned immediate load preserves carry");
			program.write_word(0x04d0, 0xf482); // LD A, ASM, A
			program.write_word(0x04d1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0040000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0202);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04d0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 60;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 60)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x007fffffffULL &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00,
					"ASM shifted load saturates and preserves carry");
			program.write_word(0x04e0, 0xec03); // RPT #3
			program.write_word(0x04e1, 0x0082); // ADD *AR2, A
			program.write_word(0x04e2, 0xf5e1);
			program.write_word(0x0048, 0x0083); // Observe A on ISR entry.
			program.write_word(0x0049, 0xf49b);
			m_repeat_reads = 0;
			m_irq_accumulator = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0060);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0061);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 61;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 61)
		{
			osd_printf_info("repeat IRQ: reads=%u isr_a=%llx a=%llx pc=%04x idle=%u\n",
					m_repeat_reads, (unsigned long long)m_irq_accumulator,
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)));
			expect(m_repeat_reads == 4 && m_irq_accumulator == 4 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
					m_cpu->state_int(tms320c54x_device::STATE_IDLE),
					"pending interrupt defers until the complete single-repeat body retires");
			expect(m_last_operand_cycle - m_first_operand_cycle == 3,
					"repeated ADD consumes one cycle per body iteration");
			m_cpu->set_input_line(2, CLEAR_LINE);
			program.write_word(0x04f0, 0x0082); // Cycle marker before BD.
			program.write_word(0x04f1, 0xf273);
			program.write_word(0x04f2, 0x04f6);
			program.write_word(0x04f3, 0x0082); // IRQ raised in first delay slot.
			program.write_word(0x04f4, 0x0082);
			program.write_word(0x04f5, 0xffff); // Must not execute.
			program.write_word(0x04f6, 0xf5e1);
			m_repeat_reads = 0;
			m_irq_trigger_read = 2;
			m_irq_accumulator = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04f0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 62;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 62)
		{
			expect(m_repeat_reads == 3 && m_irq_accumulator == 3 &&
					m_last_operand_cycle - m_first_operand_cycle == 4 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x04f7 &&
					!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
					"BD takes two cycles and defers IRQ through both delay slots");
			m_cpu->set_input_line(2, CLEAR_LINE);
			program.write_word(0x0500, 0x7726); // STM #TSS, TCR
			program.write_word(0x0501, 0x0010);
			program.write_word(0x0502, 0xf070); // RPT #65535
			program.write_word(0x0503, 0xffff);
			program.write_word(0x0504, 0x0082);
			program.write_word(0x0505, 0xf5e1);
			m_repeat_reads = 0;
			m_irq_trigger_read = 1;
			m_irq_accumulator = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0500);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 63;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 63)
		{
			expect(m_repeat_reads > 0 && m_repeat_reads < 65536 &&
					(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4),
					"save checkpoint has active repeat and pending IRQ");
			m_saved_repeat_reads = m_repeat_reads;
			static constexpr u8 payload[] = { 0x12, 0x34, 0x56, 0x78 };
			m_transport->peer_shared_w(0x1c8 / 2, 0x100 / 2);
			m_transport->peer_shared_w(0x1ca / 2, 0x100 / 2);
			expect(m_transport->enqueue_rx_packet(0x83, payload, sizeof(payload)) &&
					m_transport->enqueue_rx_packet(0x89, payload, sizeof(payload)),
					"queue transport packets before save");
			m_transport->dspif_w(0, 0x5a);
			for (unsigned i = 0; i != m_saved_transport.size(); ++i)
				m_saved_transport[i] = m_transport->shared_word(i);
			m_saved_repeat.str(std::string());
			expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE,
					"write active-repeat save state");
			m_phase = 64;
			m_check_timer->adjust(attotime::from_msec(6));
			return;
		}
		if (m_phase == 64 || m_phase == 65)
		{
			osd_printf_info("repeat state phase=%u reads=%u isr=%llu a=%llu pc=%04x\n", m_phase,
					m_repeat_reads, (unsigned long long)m_irq_accumulator,
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			expect(m_repeat_reads == 65536 && m_irq_accumulator == 65536 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 65536 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0506,
					"active-repeat save replay completes identically and services pending IRQ");
			if (m_phase == 64)
			{
				for (unsigned i = 0; i != m_saved_transport.size(); ++i)
					m_transport->peer_shared_w(i, 0);
				m_transport->dspif_w(0, 0);
				m_saved_repeat.clear();
				m_saved_repeat.seekg(0);
				expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE,
						"restore active-repeat save state");
				for (unsigned i = 0; i != m_saved_transport.size(); ++i)
					expect(m_transport->shared_word(i) == m_saved_transport[i],
							"restore queued DSPIF payloads and ring cursors");
				expect(m_transport->dspif_r(0) == 0x5a,
						"restore DSPIF interface registers");
				m_repeat_reads = m_saved_repeat_reads;
				m_irq_accumulator = 0;
				m_phase = 65;
				m_check_timer->adjust(attotime::from_msec(6));
				return;
			}
			m_cpu->set_input_line(2, CLEAR_LINE);
			program.write_word(0x0520, 0x0082);
			program.write_word(0x0521, 0xf274); // CALLD 0540
			program.write_word(0x0522, 0x0540);
			program.write_word(0x0523, 0xf495);
			program.write_word(0x0524, 0xf495);
			program.write_word(0x0525, 0x0082);
			program.write_word(0x0526, 0xf5e1);
			program.write_word(0x0540, 0x0082);
			program.write_word(0x0541, 0xfe00); // RETD
			program.write_word(0x0542, 0xf495);
			program.write_word(0x0543, 0xf495);
			m_repeat_reads = 0;
			m_irq_trigger_read = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0520);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 66;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 66)
		{
			expect(m_repeat_reads == 3 && m_last_operand_cycle - m_first_operand_cycle == 11 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0527 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"CALLD two-cycle and RETD three-cycle timing with balanced delayed return");
			program.write_word(0x0560, 0xf484);
			program.write_word(0x0561, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x8000000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0560);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 67;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 67 || m_phase == 68)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 67 ? 0x8000000000ULL : 0x007fffffffULL) &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400,
					"NEG 40-bit minimum sets overflow and obeys OVM");
			if (m_phase == 67)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0560);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 68;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x0580, 0x0082);
			program.write_word(0x0581, 0xf020); // LD #1234, A
			program.write_word(0x0582, 0x1234);
			program.write_word(0x0583, 0x0082);
			program.write_word(0x0584, 0xf5e1);
			m_repeat_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0580);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 69;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 69)
		{
			expect(m_repeat_reads == 2 && m_last_operand_cycle - m_first_operand_cycle == 3 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1235,
					"long-immediate LD consumes two cycles");
			program.write_word(0x05a0, 0xf0ff); // SFTL A, -1
			program.write_word(0x05a1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05a0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 70;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 70 && m_phase <= 72)
		{
			const u64 expected = m_phase == 70 ? 0x007fffffffULL :
					m_phase == 71 ? 0x00fffffffeULL : 0x00ffffffffULL;
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == (m_phase != 72),
					"SFTL uses low 32 bits clears guards and publishes carry");
			if (m_phase != 72)
			{
				program.write_word(0x05a0, m_phase == 70 ? 0xf0e1 : 0xf0e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05a0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05c0, 0xf765); // SFTA B, +5, B
			program.write_word(0x05c1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x80aa001234ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05c0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 73;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 73 || m_phase == 74)
		{
			osd_printf_info("SFTA left phase=%u b=%010llx st0=%04x\n", m_phase,
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)));
			expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(m_phase == 73 ? 0x1540024680ULL : 0xff80000000ULL) &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0200,
					"SFTA guard-bit left shift carry overflow and negative saturation");
			if (m_phase == 73)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x80aa001234ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05c0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 74;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e0, 0xf491); // ROL A
			program.write_word(0x05e1, 0xf5e1); // IDLE
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0012345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 75;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 75 || m_phase == 76)
		{
			const bool first = m_phase == 75;
			expect_opcode(first ? 0xf491 : 0xf591, m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(first ? 0 : 0x0012345678ULL) &&
					m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(first ? 0x0012345678ULL : 0x002468acf1ULL) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == first,
					"ROL rotates through carry, clears guard bits and preserves the other accumulator");
			if (first)
			{
				program.write_word(0x05e0, 0xf591); // ROL B
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0012345678ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0012345678ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 76;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e0, 0xf050); // XOR #lk, 0, A, A
			program.write_word(0x05e1, 0x00ff);
			program.write_word(0x05e2, 0xf5e1); // IDLE
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0012345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 77;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
	if (m_phase == 77)
	{
			expect_opcode(0xf050, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345687ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0012345678ULL &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e3,
					"XOR long immediate consumes extension and preserves carry, guards and B");
			program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
			program.write_word(0x05e1, 0x0123);
			program.write_word(0x05e2, 0x74d6);
			program.write_word(0x05e3, 0x0123);
			program.write_word(0x05e4, 0xf5e1); // IDLE
			data.write_word(0x0a03, 0);
			data.write_word(0x0a00, 0);
			m_port_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 78;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 78)
		{
			expect_opcode(0x74d6, m_port_reads == 2 && data.read_word(0x0a03) == 0xabcd &&
					data.read_word(0x0a00) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0a01 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e5 &&
					m_last_port_cycle - m_first_port_cycle == 2,
					"PORTR port reads take two cycles and circularly update AR6");
			program.write_word(0x05e0, 0xb03a); // MAC *AR5, *AR4+, A, A
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0b00, 0xfffe);
			data.write_word(0x0c00, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0b00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 79;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 79)
		{
			expect_opcode(0xb03a, m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 20 &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c01 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b00,
					"ROM4 b03a signed MAC updates T and one pointer");
			program.write_word(0x05e0, 0xb3be); // MAC *AR5+, *AR4+%, B, B
			data.write_word(0x0b00, 0xfffd);
			data.write_word(0x0c03, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c03);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0b00);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 20);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 80;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 80)
		{
			expect_opcode(0xb3be, m_cpu->state_int(tms320c54x_device::STATE_B) == 8 &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b01,
					"ROM4 b3be signed MAC updates both pointer modes");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x75d6);
			program.write_word(0x05e3, 0x0124);
			program.write_word(0x05e4, 0xf5e1); // IDLE
			data.write_word(0x0a03, 0x1234);
			data.write_word(0x0a00, 0x5678);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 81;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 81)
		{
			expect(m_port_writes == 2 && m_first_port_value == 0x1234 &&
					m_last_port_value == 0x5678 &&
					m_last_port_cycle - m_first_port_cycle == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0a01,
					"PORTW memory source takes two cycles and circularly updates AR6");
			program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
			program.write_word(0x05e1, 0x0123);
			program.write_word(0x05e2, 0xf844); // BC 05e4, ANEQ
			program.write_word(0x05e3, 0x05e4);
			program.write_word(0x05e4, 0x74d6);
			program.write_word(0x05e5, 0x0123);
			program.write_word(0x05e6, 0xf5e1); // IDLE
			m_port_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 82;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 82 || m_phase == 83)
		{
			expect_opcode(0xf844, m_port_reads == 2 && m_last_port_cycle - m_first_port_cycle ==
					(m_phase == 82 ? 7 : 5),
					"BC ANEQ costs five cycles taken and three cycles not taken");
			if (m_phase == 82)
			{
				m_port_reads = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 83;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x10f8); // LD *(lk), A
			program.write_word(0x05e3, 0x0d00);
			data.write_word(0x0d00, 0xfffe);
			m_port_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 84;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 84)
		{
			expect_opcode(0x10f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffffeULL &&
					m_port_reads == 2 &&
					m_last_port_cycle - m_first_port_cycle == 4,
					"LD absolute Smem sign-extends and costs an extra cycle");
			program.write_word(0x05e0, 0xb0be); // MAC *AR5+, *AR4+%, A, A
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0b00, 0xfffd);
			data.write_word(0x0c03, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c03);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0b00);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 20);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 9);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 85;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 85)
		{
			expect_opcode(0xb0be, m_cpu->state_int(tms320c54x_device::STATE_A) == 8 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 9 &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b01,
					"ROM4 b0be signed MAC preserves B and updates both pointers");
			program.write_word(0x05e0, 0xe2e4); // SQDST *AR4+%, *AR2-
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0c03, 5);
			data.write_word(0x0d00, 7);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0000030000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c03);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 86;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 86)
		{
			expect_opcode(0xe2e4, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffe0000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 19 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0cff,
					"ROM4 e2e4 SQDST squares old A and loads signed vector difference");
			program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
			program.write_word(0x05e1, 0x0123);
			program.write_word(0x05e2, 0x4f81); // DST B, *AR1
			program.write_word(0x05e3, 0x74d6);
			program.write_word(0x05e4, 0x0123);
			program.write_word(0x05e5, 0xf5e1); // IDLE
			m_port_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 87;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 87)
		{
			expect_opcode(0x4f81, data.read_word(0x0d00) == 0x1234 &&
					data.read_word(0x0d01) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0d00 &&
					m_port_reads == 2 &&
					m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 4f81 DST writes both halves and costs two cycles");
			program.write_word(0x05e0, 0x4f13); // DST B, *AR3+
			program.write_word(0x05e1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 88;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 88)
		{
			expect(data.read_word(0x0e00) == 0x1234 &&
					data.read_word(0x0e01) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e02,
					"DST long operand post-increments AR by two");
			program.write_word(0x05e0, 0x561b); // DLD *+AR3, A
			data.write_word(0x0e02, 0xabcd);
			data.write_word(0x0e03, 0xef01);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 89;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 89)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0xabcdef01ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e02,
					"DLD long operand pre-increments AR before the read");
			program.write_word(0x05e0, 0x4f53); // DST B, *AR3+%
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x76543210);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0e02);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 90;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 90)
		{
			expect(data.read_word(0x0e02) == 0x7654 &&
					data.read_word(0x0e03) == 0x3210 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e00,
					"DST long circular operand advances by two and wraps");
			program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
			program.write_word(0x05e1, 0x0123);
			program.write_word(0x05e2, 0x6ded); // MAR *+AR5(-7)
			program.write_word(0x05e3, 0xfff9);
			program.write_word(0x05e4, 0x74d6);
			program.write_word(0x05e5, 0x0123);
			program.write_word(0x05e6, 0xf5e1); // IDLE
			m_port_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 91;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 91)
		{
			expect_opcode(0x6ded, m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0ff9 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
					m_port_reads == 2 &&
					m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 6ded consumes signed MAR offset and costs two cycles");
			program.write_word(0x05e0, 0x6ddc); // MAR *AR4+0%
			program.write_word(0x05e1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 92;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 92)
		{
			expect_opcode(0x6ddc, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e2,
					"ROM4 6ddc circular MAR uses AR0 and consumes no extension");
			program.write_word(0x05e0, 0x4092); // SUB *AR2+, 16, A
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0d00, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x00050000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 93;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 93)
		{
			expect_opcode(0x4092, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00020000 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d01 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e2,
					"ROM4 4092 subtracts the shifted Smem and advances AR2");
			program.write_word(0x05e0, 0x5781); // DLD *AR1, B
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0e00, 0x8001);
			data.write_word(0x0e01, 0x2345);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 94;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 94)
		{
			expect_opcode(0x5781, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80012345ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0e00,
					"ROM4 5781 DLD sign-extends B without modifying AR1");
			program.write_word(0x05e0, 0xa5be); // MPY *AR5+, *AR4+0%, B
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0b00, 0xfffe);
			data.write_word(0x0c03, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c03);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0b00);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 95;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 95)
		{
			expect_opcode(0xa5be, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffffffaULL &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b01,
					"ROM4 a5be signed dual MPY loads T and updates both pointers");
			program.write_word(0x05e0, 0xb736); // MACR *AR5, *AR4-, B, B
			data.write_word(0x0b00, 3);
			data.write_word(0x0c03, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0b00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c03);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 96;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 96)
		{
			expect_opcode(0xb736, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c02 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b00,
					"ROM4 b736 MACR rounds and updates Y pointer");
			program.write_word(0x05e0, 0xd6e1); // ST B,*AR3 || MACR *AR4+0%,A
			data.write_word(0x0c03, 0xfffe);
			data.write_word(0x0d00, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x8008);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0012345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c03);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 97;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 97)
		{
			expect_opcode(0xd6e1, data.read_word(0x0d00) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0012345678ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 4 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d00,
					"ROM4 d6e1 stores old B and rounds MAC into A");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x6c8a); // BANZ 05e4, *AR2-
			program.write_word(0x05e3, 0x05e4);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0a03, 0x1234);
			data.write_word(0x0a00, 0x5678);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 98;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 98 || m_phase == 99)
		{
			expect_opcode(0x6c8a, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
					(m_phase == 98 ? 6 : 4) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
					(m_phase == 98 ? 0 : 0xffff),
					"ROM4 6c8a BANZ costs four cycles taken, two not taken, and decrements AR2");
			if (m_phase == 98)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 99;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x4593); // LD *AR3+,16,B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0d00, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 100;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 100)
		{
			expect_opcode(0x4593, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d01 &&
					m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 4593 sign-extends into B, post-increments AR3, and costs one cycle");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf830); // BC 05e4, TC
			program.write_word(0x05e3, 0x05e4);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // TC
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 101;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 101 || m_phase == 102)
		{
			expect_opcode(0xf830, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
					(m_phase == 101 ? 7 : 5),
					"ROM4 f830 BC TC costs five cycles taken and three not taken");
			if (m_phase == 101)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 102;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf030); // AND #lk,A
			program.write_word(0x05e3, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 103;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 103)
		{
			expect_opcode(0xf030, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x30 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 f030 AND uses zero-extended immediate and costs two cycles");
			program.write_word(0x05e2, 0xf073); // B 05e4
			program.write_word(0x05e3, 0x05e4);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 104;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 104)
		{
			expect_opcode(0xf073, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 f073 B consumes its target word and costs four cycles");
			program.write_word(0x05e2, 0x75f8); // PORTW *(0d00), 0124
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0d00, 0x9abc);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 105;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 105)
		{
			expect_opcode(0x75f8, m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5 &&
					m_first_port_value == 0x1234 && m_middle_port_value == 0x9abc,
					"ROM4 75f8 consumes absolute source and port words in three cycles");
			program.write_word(0x05e2, 0x74f8); // PORTR 0123, *(0d00)
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0x0123);
			data.write_word(0x0d00, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 106;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 106)
		{
			expect_opcode(0x74f8, data.read_word(0x0d00) == 0xabcd && m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 74f8 consumes absolute destination and port words in three cycles");
			program.write_word(0x05e0, 0x2883); // MAC *AR3,A
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0d00, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // Preserve C.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 107;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 107 || m_phase == 108)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 107 ? 0x0080000000ULL : 0x007fffffffULL) &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00,
					"MAC sets sticky OVA, preserves C, and saturates only with OVM");
			if (m_phase == 107)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 108;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			data.write_word(0x0d00, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0240); // OVM, FRCT
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 109;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 109 || m_phase == 110)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 109 ? 0x7fffffff : 0x7ffffffe) &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) == 0,
					"PMST.SMUL saturates fractional product before accumulation");
			if (m_phase == 109)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0002); // SMUL
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 110;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e0, 0xb03a); // MAC *AR5+,*AR4-,A,A
			data.write_word(0x0b00, 1);
			data.write_word(0x0c00, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0b00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c00);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 111;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 111)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400),
					"dual-memory MAC sets OVA and clamps A with OVM");
			program.write_word(0x05e0, 0xd6e1); // ST B,*AR3 || MACR *AR4+0%,A
			data.write_word(0x0c00, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0c00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12340000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 112;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 112)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
					data.read_word(0x0d00) == 0x1234,
					"parallel ST/MACR stores old B and sets OVA on saturated A");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf040); // OR #lk,A
			program.write_word(0x05e3, 0x00f0);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 113;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 113)
		{
			expect_opcode(0xf040, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff000000f0ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 f040 OR zero-extends its immediate and costs two cycles");
			program.write_word(0x05e2, 0x6082); // CMPM *AR2,#lk
			program.write_word(0x05e3, 0x1234);
			data.write_word(0x0e00, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 114;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 114 || m_phase == 115)
		{
			expect_opcode(0x6082, bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) ==
					(m_phase == 114) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0e00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 6082 CMPM updates TC, preserves AR2, and costs two cycles");
			if (m_phase == 114)
			{
				data.write_word(0x0e00, 0x5678);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 115;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x8093); // STL A,*AR3+
			program.write_word(0x05e3, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e2);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 116;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 116)
		{
			expect_opcode(0x8093, data.read_word(0x0d00) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d01,
					"ROM4 8093 stores the low accumulator word and post-increments AR3");
			program.write_word(0x05e0, 0xf7bb); // SSBX INTM
			program.write_word(0x05e1, 0x4a08); // PSHM AL
			program.write_word(0x05e2, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 117;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 117)
		{
			expect_opcode(0xf7bb, m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800,
					"ROM4 f7bb masks interrupts");
			expect_opcode(0x4a08, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
					data.read_word(0x02ff) == 0x5678,
					"ROM4 4a08 pushes AL to TOS");
			program.write_word(0x05e0, 0x8a08); // POPM AL
			program.write_word(0x05e1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12340000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 118;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 118)
		{
			expect_opcode(0x8a08, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"ROM4 8a08 restores AL from TOS and advances SP");
			program.write_word(0x05e0, 0xf6bb); // RSBX INTM
			program.write_word(0x05e1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 119;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 119)
		{
			expect_opcode(0xf6bb, !(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
					"ROM4 f6bb clears INTM for interrupt return");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x7212); // MVDM 0e00,AR2
			program.write_word(0x05e3, 0x0e00);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0e00, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 120;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 120)
		{
			expect_opcode(0x7212, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 7212 MVDM moves data to AR2 in two cycles");
			program.write_word(0x05e2, 0x7312); // MVMD AR2,0e01
			program.write_word(0x05e3, 0x0e01);
			data.write_word(0x0e01, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 121;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 121)
		{
			expect_opcode(0x7312, data.read_word(0x0e01) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"MVMD moves AR2 to data memory in two cycles");
			program.write_word(0x05e2, 0xec02); // RPT #2
			program.write_word(0x05e3, 0x7212); // MVDM 0e00,AR2
			program.write_word(0x05e4, 0x0e00);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0e00, 0x1111);
			data.write_word(0x0e01, 0x2222);
			data.write_word(0x0e02, 0x3333);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 122;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 122)
		{
			expect_opcode(0x7212, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x3333 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"repeated MVDM advances data source and pipelines after first move");
			program.write_word(0x05e3, 0x7312); // MVMD AR2,0e10
			program.write_word(0x05e4, 0x0e10);
			data.write_word(0x0e10, 0);
			data.write_word(0x0e11, 0);
			data.write_word(0x0e12, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x4455);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 123;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 123)
		{
			expect_opcode(0x7312, data.read_word(0x0e10) == 0x4455 &&
					data.read_word(0x0e11) == 0x4455 &&
					data.read_word(0x0e12) == 0x4455 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"repeated MVMD advances data destination and pipelines after first move");
			program.write_word(0x05e0, 0x4a09); // PSHM AH
			program.write_word(0x05e1, 0x4a0a); // PSHM AG
			program.write_word(0x05e2, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 124;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 124)
		{
			expect_opcode(0x4a09, data.read_word(0x02ff) == 0x3456,
					"ROM4 4a09 pushes AH before decrementing SP again");
			expect_opcode(0x4a0a, data.read_word(0x02fe) == 0x0012 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02fe,
					"ROM4 4a0a pushes the eight-bit guard and decrements SP");
			program.write_word(0x05e0, 0x8a0a); // POPM AG
			program.write_word(0x05e1, 0x8a09); // POPM AH
			program.write_word(0x05e2, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcd987654ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 125;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 125)
		{
			expect_opcode(0x8a0a, (m_cpu->state_int(tms320c54x_device::STATE_A) >> 32) == 0x12,
					"ROM4 8a0a restores AG without sign extension");
			expect_opcode(0x8a09, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234567654ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"ROM4 8a09 restores AH, preserves AL, and advances SP");
			program.write_word(0x05e0, 0x4a0b); // PSHM BL
			program.write_word(0x05e1, 0x4a0c); // PSHM BH
			program.write_word(0x05e2, 0x4a0d); // PSHM BG
			program.write_word(0x05e3, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x7f1234abcdULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 126;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 126)
		{
			expect_opcode(0x4a0b, data.read_word(0x02ff) == 0xabcd,
					"ROM4 4a0b pushes BL");
			expect_opcode(0x4a0c, data.read_word(0x02fe) == 0x1234,
					"ROM4 4a0c pushes BH");
			expect_opcode(0x4a0d, data.read_word(0x02fd) == 0x007f &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02fd,
					"ROM4 4a0d pushes the eight-bit B guard");
			program.write_word(0x05e0, 0x8a0d); // POPM BG
			program.write_word(0x05e1, 0x8a0c); // POPM BH
			program.write_word(0x05e2, 0x8a0b); // POPM BL
			program.write_word(0x05e3, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5511223344ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 127;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 127)
		{
			expect_opcode(0x8a0d, (m_cpu->state_int(tms320c54x_device::STATE_B) >> 32) == 0x7f,
					"ROM4 8a0d restores BG without sign extension");
			expect_opcode(0x8a0c, (m_cpu->state_int(tms320c54x_device::STATE_B) >> 16 & 0xffff) == 0x1234,
					"ROM4 8a0c restores BH");
			expect_opcode(0x8a0b, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7f1234abcdULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234567654ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
					"ROM4 8a0b restores BL, preserves A, and advances SP");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf065); // XOR #lk,16,A
			program.write_word(0x05e3, 0x00ff);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 128;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 128)
		{
			expect_opcode(0xf065, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff00ff0000ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 f065 XORs the shifted immediate and costs two cycles");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf071); // RPTZ A,#lk
			program.write_word(0x05e3, 0x0000);
			program.write_word(0x05e4, 0xf495); // NOP
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 129;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 129)
		{
			expect_opcode(0xf071, m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 f071 clears A, repeats one NOP, and costs two cycles");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf272); // RPTBD 05e6h
			program.write_word(0x05e3, 0x05e6);
			program.write_word(0x05e4, 0xf495); // delay slot 1
			program.write_word(0x05e5, 0xf495); // delay slot 2
			program.write_word(0x05e6, 0xf495); // one-word repeat body
			program.write_word(0x05e7, 0x75d6);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 130;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 130)
		{
			expect_opcode(0xf272, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 7 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ea &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x4000),
					"ROM4 f272 costs two cycles and retires after its one-word block");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf072); // RPTB 05e4h
			program.write_word(0x05e3, 0x05e4);
			program.write_word(0x05e4, 0xf495); // one-word repeat body
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 131;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 131)
		{
			expect_opcode(0xf072, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 7 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x4000),
					"ROM4 f072 costs four cycles and retires after its one-word block");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x34f8); // BITT *(absolute)
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d00, 0x8000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 132;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 132 || m_phase == 133)
		{
			const bool high_bit = m_phase == 132;
			expect_opcode(0x34f8, bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) == high_bit &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 34f8 tests bit 15 minus T and costs two cycles when absolute");
			if (high_bit)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_T, 15);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 133;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x70f8); // MVKD dmad, *(absolute)
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0x0e00);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0d00, 0);
			data.write_word(0x0e00, 0x4567);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 134;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 134)
		{
			expect_opcode(0x70f8, data.read_word(0x0d00) == 0x4567 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 70f8 copies absolute data and costs three cycles");
			program.write_word(0x05e2, 0x61f8); // BITF *(absolute),#lk
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0x0040);
			data.write_word(0x0d00, 0x0140);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 135;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 135)
		{
			expect_opcode(0x61f8, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 61f8 tests the masked absolute word in three cycles");
			program.write_word(0x05e2, 0x6182); // BITF *AR2,#lk
			program.write_word(0x05e3, 0x0040);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d00, 0x0100);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 136;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 136)
		{
			expect_opcode(0x6182, !(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 6182 tests indirect Smem in two cycles without pointer movement");
			program.write_word(0x05e2, 0x68f8); // ANDM #lk,*(absolute)
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0x00f0);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0d00, 0x0ff0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 137;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 137 && m_phase <= 139)
		{
			const u16 opcode = m_phase == 137 ? 0x68f8 : m_phase == 138 ? 0x69f8 : 0x6bf8;
			const u16 expected = m_phase == 137 ? 0x00f0 : m_phase == 138 ? 0x0ff0 : 0x00f3;
			expect_opcode(opcode, data.read_word(0x0d00) == expected &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 absolute immediate memory operator costs three cycles");
			if (m_phase != 139)
			{
				program.write_word(0x05e2, m_phase == 137 ? 0x69f8 : 0x6bf8);
				program.write_word(0x05e4, m_phase == 137 ? 0x0f00 : 0x0003);
				data.write_word(0x0d00, 0x00f0);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xff0c); // XC 2,C
			program.write_word(0x05e3, 0xf495); // NOP
			program.write_word(0x05e4, 0xf793); // CMPL B,B
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 140;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 140 || m_phase == 141)
		{
			const bool carry_set = m_phase == 141;
			expect_opcode(0xff0c, m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(carry_set ? 0xffffffedcbULL : 0x1234ULL) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
					(carry_set ? 5 : 3),
					"ROM4 ff0c executes or skips two words according to carry in one cycle");
			if (!carry_set)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 141;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			expect_opcode(0xf793, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffedcbULL,
					"ROM4 f793 complements the full 40-bit B accumulator");
			program.write_word(0x05e2, 0x47f8); // RPT *(absolute)
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0xf495); // NOP, repeated three times
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0d00, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 142;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 142)
		{
			expect_opcode(0x47f8, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 9,
					"ROM4 absolute RPT Smem costs four cycles and repeats the NOP three times");
			program.write_word(0x05e2, 0x4782); // RPT *AR2
			program.write_word(0x05e3, 0xf495);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 143;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 143)
		{
			expect_opcode(0x4782, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 8 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00,
					"indirect RPT Smem costs three cycles without moving AR2");
			program.write_word(0x05e2, 0x7d92); // MVDP *AR2+,pmad
			program.write_word(0x05e3, 0x0b00);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d00, 0x2468);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 144;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 144)
		{
			expect_opcode(0x7d92, program.read_word(0x0b00) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d01 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 MVDP moves *AR2+ to program memory in four cycles");
			program.write_word(0x05e2, 0x7c92); // MVPD pmad,*AR2+
			program.write_word(0x05e3, 0x0b00);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d10);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 145;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 145)
		{
			expect_opcode(0x7c92, data.read_word(0x0d10) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d11 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 MVPD moves program memory to *AR2+ in three cycles");
			program.write_word(0x05e2, 0x7df8); // MVDP *(absolute),pmad
			program.write_word(0x05e3, 0x0d00);
			program.write_word(0x05e4, 0x0b01);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 146;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 146)
		{
			expect_opcode(0x7df8, program.read_word(0x0b01) == 0x2468 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"absolute MVDP reads address before pmad and costs five cycles");
			program.write_word(0x05e2, 0x7cf8); // MVPD pmad,*(absolute)
			program.write_word(0x05e3, 0x0d11);
			program.write_word(0x05e4, 0x0b01);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 147;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 147)
		{
			expect_opcode(0x7cf8, data.read_word(0x0d11) == 0x2468 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"absolute MVPD reads address before pmad and costs four cycles");
			program.write_word(0x05e2, 0xec02); // RPT #2
			program.write_word(0x05e3, 0x7c92); // MVPD pmad,*AR2+
			program.write_word(0x05e4, 0x0b10);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			program.write_word(0x0b10, 0x1357);
			program.write_word(0x0b11, 0x2468);
			program.write_word(0x0b12, 0x369a);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d20);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 148;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 148)
		{
			expect_opcode(0x7c92, data.read_word(0x0d20) == 0x1357 &&
					data.read_word(0x0d21) == 0x2468 &&
					data.read_word(0x0d22) == 0x369a &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d23 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"repeated MVPD advances program and data addresses at one cycle after setup");
			program.write_word(0x05e2, 0x7f92); // WRITA *AR2+
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0d30, 0xabcd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0b20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d30);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 149;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 149)
		{
			expect_opcode(0x7f92, program.read_word(0x0b20) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0b20 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d31 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 WRITA writes A-addressed program memory in five cycles");
			program.write_word(0x05e2, 0x7ef8); // READA *(absolute)
			program.write_word(0x05e3, 0x0d31);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 150;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 150)
		{
			expect_opcode(0x7ef8, data.read_word(0x0d31) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0b20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"absolute READA reads A-addressed program memory in six cycles");
			program.write_word(0x05e2, 0xec02); // RPT #2
			program.write_word(0x05e3, 0x7f92); // WRITA *AR2+
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d40, 0x1357);
			data.write_word(0x0d41, 0x2468);
			data.write_word(0x0d42, 0x369a);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0b30);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d40);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 151;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 151)
		{
			expect_opcode(0x7f92, program.read_word(0x0b30) == 0x1357 &&
					program.read_word(0x0b31) == 0x2468 &&
					program.read_word(0x0b32) == 0x369a &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0b30 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d43 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
					"repeated WRITA advances program and data addresses at one cycle after setup");
			program.write_word(0x05e2, 0x771a); // STM #lk,BRC
			program.write_word(0x05e3, 0x0042);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 152;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 152)
		{
			expect_opcode(0x771a, m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0x0042 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STM #lk,BRC writes the MMR in two cycles");
			program.write_word(0x05e2, 0x76f8); // ST #lk,*(absolute)
			program.write_word(0x05e3, 0x0d50);
			program.write_word(0x05e4, 0x5678);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 153;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 153)
		{
			expect_opcode(0x76f8, data.read_word(0x0d50) == 0x5678 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"absolute ST #lk,Smem fetches address first and costs three cycles");
			program.write_word(0x05e2, 0x7682); // ST #lk,*AR2
			program.write_word(0x05e3, 0x9abc);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d51);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 154;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 154)
		{
			expect_opcode(0x7682, data.read_word(0x0d51) == 0x9abc &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d51 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"indirect ST #lk,Smem preserves AR2 and costs two cycles");
			program.write_word(0x05e2, 0x4bf8); // PSHD *(absolute)
			program.write_word(0x05e3, 0x0d60);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d60, 0xabcd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 155;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 155)
		{
			expect_opcode(0x4bf8, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0dff &&
					data.read_word(0x0dff) == 0xabcd && m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute PSHD reads source and pushes in two cycles");
			program.write_word(0x05e2, 0x8bf8); // POPD *(absolute)
			program.write_word(0x05e3, 0x0d61);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 156;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 156)
		{
			expect_opcode(0x8bf8, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0e00 &&
					data.read_word(0x0d61) == 0xabcd && m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute POPD restores stack pointer and stores in two cycles");
			program.write_word(0x05e2, 0x4a06); // PSHM ST0
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0005);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 157;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 157)
		{
			expect_opcode(0x4a06, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0dff &&
					data.read_word(0x0dff) == 0x0005 && m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 PSHM ST0 pushes the MMR in one cycle");
			program.write_word(0x05e2, 0x8a06); // POPM ST0
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 158;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 158)
		{
			expect_opcode(0x8a06, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0e00 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0005 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 POPM ST0 restores the MMR in one cycle");
			program.write_word(0x05e2, 0x07f8); // ADDC *(absolute),B
			program.write_word(0x05e3, 0x0d70);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d70, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 159;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 159)
		{
			expect_opcode(0x07f8, m_cpu->state_int(tms320c54x_device::STATE_B) == 8 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute ADDC consumes carry and costs two cycles");
			program.write_word(0x05e2, 0x1af8); // OR *(absolute),A
			program.write_word(0x05e3, 0x0d70);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d70, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 160;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 160)
		{
			expect_opcode(0x1af8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0ff0 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute OR combines the operand in two cycles");
			program.write_word(0x05e2, 0x08f8); // SUB *(absolute),A
			program.write_word(0x05e3, 0x0d70);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d70, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 161;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 161)
		{
			expect_opcode(0x08f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 7 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute SUB subtracts the signed operand in two cycles");
			program.write_word(0x05e2, 0x2494); // MPYU *AR4+,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0d80, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xfffe);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0d80);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 162;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 162)
		{
			expect_opcode(0x2494, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x2fffa &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d81 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 MPYU multiplies unsigned operands and postincrements in one cycle");
			program.write_word(0x05e2, 0x24f8); // MPYU *(absolute),A
			program.write_word(0x05e3, 0x0d80);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 163;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 163)
		{
			expect_opcode(0x24f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x2fffa &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"absolute MPYU keeps the unsigned product and costs two cycles");
			program.write_word(0x05e2, 0x7192); // MVDK *AR2+,dmad
			program.write_word(0x05e3, 0x0da0);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d90, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 164;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 164)
		{
			expect_opcode(0x7192, data.read_word(0x0da0) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d91 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVDK moves *AR2+ to immediate data address in two cycles");
			program.write_word(0x05e2, 0xec02); // RPT #2
			program.write_word(0x05e3, 0x7093); // MVKD dmad,*AR3+
			program.write_word(0x05e4, 0x0d90);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0d90, 0x1111);
			data.write_word(0x0d91, 0x2222);
			data.write_word(0x0d92, 0x3333);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0db0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 165;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 165)
		{
			expect_opcode(0x7093, data.read_word(0x0db0) == 0x1111 &&
					data.read_word(0x0db1) == 0x2222 &&
					data.read_word(0x0db2) == 0x3333 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0db3 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"repeated MVKD advances source and destination at one cycle after setup");
			program.write_word(0x05e2, 0x44f8); // LD *(absolute),16,A
			program.write_word(0x05e3, 0x0dc0);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0dc0, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 166;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 166)
		{
			expect_opcode(0x44f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12340000 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute LD Smem,16,A shifts the word in two cycles");
			program.write_word(0x05e2, 0x80f8); // STL A,*(absolute)
			program.write_word(0x05e3, 0x0dc1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 167;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 167)
		{
			expect_opcode(0x80f8, data.read_word(0x0dc1) == 0x5678 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute STL stores the low accumulator word in two cycles");
			program.write_word(0x05e2, 0x82f8); // STH A,*(absolute)
			program.write_word(0x05e3, 0x0dc2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 168;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 168)
		{
			expect_opcode(0x82f8, data.read_word(0x0dc2) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute STH stores the high accumulator word in two cycles");
			program.write_word(0x05e2, 0x8cf8); // ST T,*(absolute)
			program.write_word(0x05e3, 0x0dc3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4321);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 169;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 169)
		{
			expect_opcode(0x8cf8, data.read_word(0x0dc3) == 0x4321 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute ST T stores the register in two cycles");
			program.write_word(0x05e2, 0x71f8); // MVDK *(absolute),dmad
			program.write_word(0x05e3, 0x0dd0);
			program.write_word(0x05e4, 0x0de0);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0dd0, 0x4444);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 170;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 170)
		{
			expect_opcode(0x71f8, data.read_word(0x0de0) == 0x4444 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 absolute MVDK reads source address before destination in three cycles");
			program.write_word(0x05e2, 0xec02); // RPT #2
			program.write_word(0x05e3, 0x7192); // MVDK *AR2+,dmad
			program.write_word(0x05e4, 0x0de1);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0dd0, 0x1111);
			data.write_word(0x0dd1, 0x2222);
			data.write_word(0x0dd2, 0x3333);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0dd0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 171;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 171)
		{
			expect_opcode(0x7192, data.read_word(0x0de1) == 0x1111 &&
					data.read_word(0x0de2) == 0x2222 &&
					data.read_word(0x0de3) == 0x3333 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0dd3 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"repeated MVDK advances both data addresses at one cycle after setup");
			program.write_word(0x05e2, 0x6f8a); // LD *AR2-,0,A
			program.write_word(0x05e3, 0x0c40);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0df0, 0x2345);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0df0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 172;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 172)
		{
			expect_opcode(0x6f8a, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x2345 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0def &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 extended indirect LD consumes its extension in two cycles");
			program.write_word(0x05e2, 0x6ff8); // LD *(absolute),0,A
			program.write_word(0x05e3, 0x0df0);
			program.write_word(0x05e4, 0x0c40);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 173;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 173)
		{
			expect_opcode(0x6ff8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x2345 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 extended absolute LD consumes address then extension in three cycles");
			program.write_word(0x05e2, 0x6f82); // ADD *AR2,1,A
			program.write_word(0x05e3, 0x0c01);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0df0, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0df0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 174;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 174)
		{
			expect_opcode(0x6f82, m_cpu->state_int(tms320c54x_device::STATE_A) == 14 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0df0 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 extended ADD shifts Smem and costs two cycles");
			program.write_word(0x05e2, 0x6f83); // SUB *AR3,1,A
			program.write_word(0x05e3, 0x0c21);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0df0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 175;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 175)
		{
			expect_opcode(0x6f83, m_cpu->state_int(tms320c54x_device::STATE_A) == 6 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0df0 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 extended SUB shifts Smem and costs two cycles");
			program.write_word(0x05e2, 0x6f92); // STL A,0,*AR2+
			program.write_word(0x05e3, 0x0c80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0df1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 176;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 176)
		{
			expect_opcode(0x6f92, data.read_word(0x0df1) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0df2 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 extended STL writes low accumulator word and postincrements");
			program.write_word(0x05e2, 0x6f93); // STH A,0,*AR3+
			program.write_word(0x05e3, 0x0c60);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0df2);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 177;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 177)
		{
			expect_opcode(0x6f93, data.read_word(0x0df2) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0df3 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 extended STH writes high accumulator word and postincrements");
			program.write_word(0x05e2, 0x7712); // STM #lk, AR2
			program.write_word(0x05e3, 0x4567);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 178;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 178)
		{
			expect_opcode(0x7712, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x4567 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STM #lk, AR2 stores the immediate in two cycles");
			program.write_word(0x05e2, 0x4a12); // PSHM AR2
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 179;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 179)
		{
			expect_opcode(0x4a12, data.read_word(0x02ff) == 0x4567 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 PSHM AR2 pushes the register in one cycle");
			program.write_word(0x05e2, 0x8a12); // POPM AR2
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 180;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 180)
		{
			expect_opcode(0x8a12, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x4567 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 POPM AR2 restores the register in one cycle");
			program.write_word(0x05e2, 0x8807); // STLM A, ST1
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x8123);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 181;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 181)
		{
			expect_opcode(0x8807, m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x8123 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 STLM A, ST1 writes the status MMR in one cycle");
			program.write_word(0x05e2, 0x4807); // LDM ST1, A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 182;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 182)
		{
			expect_opcode(0x4807, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8123 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LDM ST1, A reads the status MMR in one cycle");
			program.write_word(0x05e2, 0x4a07); // PSHM ST1
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 183;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 183)
		{
			expect_opcode(0x4a07, data.read_word(0x02ff) == 0x8123 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 PSHM ST1 pushes the status register in one cycle");
			program.write_word(0x05e2, 0x8a07); // POPM ST1
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 184;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 184)
		{
			expect_opcode(0x8a07, m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x8123 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 POPM ST1 restores status in one cycle");
			program.write_word(0x05e2, 0x4907); // LDM ST1, B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 185;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 185)
		{
			expect_opcode(0x4907, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8123 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"LDM ST1, B ignores SXM and zero-extends in one cycle");
			u16 pc = 0x0600;
			for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
			{
				program.write_word(pc++, 0x7700 | rom4_saved_mmr[i]); // STM #lk, MMR
				program.write_word(pc++, 0x4000 + i);
			}
			for (u8 mmr : rom4_saved_mmr)
				program.write_word(pc++, 0x4a00 | mmr); // PSHM MMR
			program.write_word(pc, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0600);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 186;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 186)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02f4,
					"ROM4 MMR burst pushes twelve values");
			for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
			{
				const bool correct_value = data.read_word(0x02ff - i) == 0x4000 + i;
				expect_opcode(0x7700 | rom4_saved_mmr[i], correct_value,
						"ROM4 STM initializes the selected register");
				expect_opcode(0x4a00 | rom4_saved_mmr[i], correct_value,
						"ROM4 MMR burst pushes the selected register");
			}
			u16 pc = 0x0640;
			for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
			{
				program.write_word(pc++, 0x7700 | rom4_saved_mmr[i]);
				program.write_word(pc++, 0x5000 + i);
			}
			for (unsigned i = std::size(rom4_saved_mmr); i != 0; --i)
				program.write_word(pc++, 0x8a00 | rom4_saved_mmr[i - 1]); // POPM MMR
			for (u8 mmr : rom4_saved_mmr)
				program.write_word(pc++, 0x4a00 | mmr);
			program.write_word(pc, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0640);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 187;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 187)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02f4,
					"ROM4 MMR burst pops then pushes twelve values");
			for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
				expect_opcode(0x8a00 | rom4_saved_mmr[i],
						data.read_word(0x02ff - i) == 0x4000 + i,
						"ROM4 MMR burst restores the selected register");
			program.write_word(0x05e2, 0x0883); // SUB *AR3,A
			program.write_word(0x05e3, 0x1d83); // XOR *AR3,B
			program.write_word(0x05e4, 0x1c83); // XOR *AR3,A
			program.write_word(0x05e5, 0x7713); // STM #0f01,AR3
			program.write_word(0x05e6, 0x0f01);
			program.write_word(0x05e7, 0x1c93); // XOR *AR3+,A
			program.write_word(0x05e8, 0x75d6);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0f00, 0xffff);
			data.write_word(0x0f01, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123400);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 188;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 188)
		{
			expect_opcode(0x0883, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff09,
					"ROM4 signed SUB and distinct XOR operands produce the expected A result");
			expect_opcode(0x1d83, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12cbff,
					"ROM4 XOR Smem,B uses the unextended word");
			expect_opcode(0x1c83, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff09,
					"ROM4 XOR Smem,A uses the unextended first word");
			expect_opcode(0x7713, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f02,
					"ROM4 STM switches the indirect source address");
			expect_opcode(0x1c93, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff09 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"ROM4 XOR *AR3+ uses the second word and advances in one cycle");
			program.write_word(0x05e2, 0xf4e1); // IDLE 1
			program.write_word(0x05e3, 0xffff); // Must not execute before wake.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e2);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 189;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 189)
		{
			expect_opcode(0xf4e1, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e3 &&
					!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
					"ROM4 IDLE 1 retains the next PC and waits for an interrupt");
			program.write_word(0x05e2, 0xed00); // LD #0,ASM
			program.write_word(0x05e3, 0xf482); // LD A,ASM,A
			program.write_word(0x05e4, 0x8083); // STL A,*AR3
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f10, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x011f); // SXM, ASM=-1
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 190;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 190)
		{
			expect_opcode(0xed00, (m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x1f) == 0,
					"ROM4 LD #0,ASM clears the prior negative shift");
			expect_opcode(0xf482, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234,
					"ASM-based load keeps A unchanged after ASM reset");
			expect_opcode(0x8083, data.read_word(0x0f10) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f10 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 STL A,*AR3 stores the low word without pointer change in one cycle");
			program.write_word(0x05e2, 0xf490); // ROR A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xab80000002ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 191;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 191)
		{
			expect_opcode(0xf490, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xc0000001 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 ROR A rotates old C into bit 31, clears guard and updates C in one cycle");
			program.write_word(0x05e2, 0xf590); // ROR B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff00000001ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 192;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 192)
		{
			expect_opcode(0xf590, m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xc0000001 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROR B clears guard, rotates old zero C and captures outgoing bit in one cycle");
			program.write_word(0x05e2, 0xf845); // BC 05f0, AEQ
			program.write_word(0x05e3, 0x05f0);
			program.write_word(0x05e4, 0xffff);
			program.write_word(0x05f0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 193;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 193 || m_phase == 194)
		{
			expect_opcode(0xf845, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 193 ? 7 : 5) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 193 ? 0x05f3 : 0x05e7),
					"ROM4 BC AEQ uses the full 40-bit A and costs five cycles taken, three not taken");
			if (m_phase == 193)
			{
				program.write_word(0x05e4, 0x75d6);
				program.write_word(0x05e5, 0x0124);
				program.write_word(0x05e6, 0xf5e1);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0100000000ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 194;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf3b0); // OR B >> 16, B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff80000001ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 195;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 195)
		{
			expect_opcode(0xf3b0, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80ff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR B >> 16 zero-fills, preserves flags and A, and costs one cycle");
			program.write_word(0x05e2, 0xf3e8); // SFTL B, 8
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff80000081ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 196;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 196 || m_phase == 197)
		{
			expect_opcode(m_phase == 196 ? 0xf3e8 : 0xf3f8,
					m_cpu->state_int(tms320c54x_device::STATE_B) == (m_phase == 196 ? 0x8100 : 0x800000) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == (m_phase == 197) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SFTL B shifts low 32 bits, clears guard, updates C, and costs one cycle");
			if (m_phase == 196)
			{
				program.write_word(0x05e2, 0xf3f8); // SFTL B, -8
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff80000081ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 197;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf330); // AND #lk, B
			program.write_word(0x05e3, 0x0ff0);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffab12cd34ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 198;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 198 || m_phase == 199)
		{
			expect_opcode(m_phase == 198 ? 0xf330 : 0xf130,
					m_cpu->state_int(tms320c54x_device::STATE_B) == (m_phase == 198 ? 0x0d30 : 0x1030) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
							(m_phase == 198 ? 0x1234 : 0xffabcd1234ULL) &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 AND #lk zeroes upper bits, preserves source and C, and costs two cycles");
			if (m_phase == 198)
			{
				program.write_word(0x05e2, 0xf130); // AND #lk, A, B
				program.write_word(0x05e3, 0xf0f0);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffabcd1234ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 199;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf010); // SUB #lk, A
			program.write_word(0x05e3, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 200;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 200 || m_phase == 201)
		{
			expect_opcode(0xf010, m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 200 ? 0x80 : 0x10080) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SUB #lk respects SXM and costs two cycles");
			if (m_phase == 200)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM set
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 201;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf000); // ADD #lk, A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 202;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 202 || m_phase == 203)
		{
			expect_opcode(0xf000, m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 202 ? 0x1ff80 : 0xff80) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ADD #lk respects SXM and costs two cycles");
			if (m_phase == 202)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM set
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 203;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf820); // BC 05f0, NTC
			program.write_word(0x05e3, 0x05f0);
			program.write_word(0x05e4, 0x75d6); // fall-through marker
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			program.write_word(0x05f0, 0x75d6); // branch marker
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // NTC true
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 204;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 204 && m_phase <= 207)
		{
			const bool taken = (m_phase & 1) == 0;
			expect_opcode(m_phase <= 205 ? 0xf820 : 0xf84c,
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05f3 : 0x05e7),
					"ROM4 BC NTC/BNEQ takes five cycles true and three cycles false");
			if (m_phase < 207)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				if (m_phase == 204)
					m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // NTC false
				else if (m_phase == 205)
				{
					program.write_word(0x05e2, 0xf84c); // BC 05f0, BNEQ
					m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0100000000ULL);
				}
				else
					m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf495); // NOP
			program.write_word(0x05e3, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x34);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 208;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 208)
		{
			expect_opcode(0xf495, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x34 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e6,
					"ROM4 NOP preserves state and costs one cycle");
			program.write_word(0x05e2, 0xf074); // CALL 05f0
			program.write_word(0x05e3, 0x05f0);
			program.write_word(0x05e4, 0xf5e1);
			program.write_word(0x05f0, 0x75d6); // subroutine port marker
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xfc00); // RET
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 209;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 209)
		{
			expect_opcode(0xf074, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6 &&
					data.read_word(0x02ff) == 0x05e4 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e5,
					"ROM4 CALL pushes continuation, branches, and costs four cycles");
			program.write_word(0x05e2, 0xfc00); // RET
			program.write_word(0x05f0, 0x75d6); // return port marker
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xf5e1);
			data.write_word(0x0300, 0x05f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 210;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 210)
		{
			expect_opcode(0xfc00, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f3,
					"ROM4 RET pops continuation and costs five cycles");
			program.write_word(0x05e2, 0xf493); // CMPL A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 211;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 211)
		{
			expect_opcode(0xf493, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00edcba987ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 CMPL complements all 40 bits, preserves C and B, and costs one cycle");
			program.write_word(0x05e2, 0xf0e8); // SFTL A, 8
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff81000081ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 212;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 212)
		{
			expect_opcode(0xf0e8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8100 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SFTL A clears guard, shifts low 32 bits, updates C, and costs one cycle");
			program.write_word(0x05e0, 0x6d90); // MAR *AR0+ (ARP-selected in compatibility mode)
			program.write_word(0x05e1, 0xf5e1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0010);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 4 << 13);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0020); // CMPT
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 213;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 213)
		{
			expect_opcode(0x6d90, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0e01 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0010 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) >> 13) == 4,
					"ROM4 MAR AR0 aliases ARP in compatibility mode");
			program.write_word(0x05e0, 0x1090); // LD *AR0+, A
			program.write_word(0x05e1, 0xf5e1);
			data.write_word(0x0e01, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 214;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 214)
		{
			expect_opcode(0x1090, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0e02 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0010,
					"indirect AR0 read and postincrement use ARP in compatibility mode");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x1c8b); // XOR *AR3-, A
			program.write_word(0x05e3, 0x1d93); // XOR *AR3+, B
			program.write_word(0x05e4, 0x1c82); // XOR *AR2, A
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f00, 0x00f0);
			data.write_word(0x0eff, 0xff80);
			data.write_word(0x0f10, 0x0f00);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // Preserve C.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must not sign-extend XOR.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 215;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 215)
		{
			const bool xor_results = m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1dc4 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xa9f8 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f10 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f00 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5;
			expect_opcode(0x1c8b, xor_results, "ROM4 XOR *AR3- selects A and decrements AR3");
			expect_opcode(0x1d93, xor_results, "ROM4 XOR *AR3+ selects B without SXM extension");
			expect_opcode(0x1c82, xor_results, "ROM4 XOR *AR2 selects A; three XORs cost three cycles");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x768a); // STM #lk, *AR2-
			program.write_word(0x05e3, 0xabcd);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0x8092); // STL A, *AR2+
			program.write_word(0x05e7, 0x75d6);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d80);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 216;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 216)
		{
			expect_opcode(0x768a, data.read_word(0x0d80) == 0xabcd &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 4,
					"ROM4 STM #lk,*AR2- writes before decrement and costs two cycles");
			expect_opcode(0x8092, data.read_word(0x0d7f) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d80 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 STL A,*AR2+ writes the low word and restores AR2 in one cycle");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xe5a8); // MVDD *AR4+, *AR2+
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xe5ca); // MVDD *AR2+0%, *AR4+
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0e00, 0x1357);
			data.write_word(0x0d03, 0x2468);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d02);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0e00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 217;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 217)
		{
			expect_opcode(0xe5a8, data.read_word(0x0d02) == 0x1357 &&
					m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 MVDD *AR4+,*AR2+ copies before both increments in one cycle");
			expect_opcode(0xe5ca, data.read_word(0x0e01) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0e02 &&
					m_port_writes == 3 && m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 MVDD *AR2+0%,*AR4+ wraps the X pointer in one cycle");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x12f8); // LDU *(lk), A
			program.write_word(0x05e3, 0x0d40);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0x138b); // LDU *AR3-, B
			program.write_word(0x05e7, 0x75d6);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0d40, 0x8001);
			data.write_word(0x0d50, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d50);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM does not sign-extend LDU.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 218;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 218)
		{
			expect_opcode(0x12f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute LDU zero-extends despite SXM and costs two cycles");
			expect_opcode(0x138b, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d4f &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 indirect LDU reads before decrement and costs one cycle");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf3c8); // XOR B << 8, B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x8080000001ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 219;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 219)
		{
			expect_opcode(0xf3c8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0080000101ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 XOR B<<8,B uses the original 40-bit B, truncates the shift, and costs one cycle");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x6b8a); // ADDM #lk, *AR2-
			program.write_word(0x05e3, 0xfff8);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0d20, 0x8007);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // OVM and SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 220;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 220)
		{
			expect_opcode(0x6b8a, data.read_word(0x0d20) == 0x8000 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d1f &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ADDM saturates signed underflow, sets C/OVA, and costs two cycles");
			program.write_word(0x05e0, 0x6b80); // ADDM #lk, *AR0 (ARP-selected in CMPT)
			program.write_word(0x05e1, 0x123b);
			program.write_word(0x05e2, 0xf5e1);
			data.write_word(0x0d30, 4);
			data.write_word(0x0d40, 0x7777);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0d40);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0d30);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 4 << 13);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0020); // CMPT
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 221;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 221)
		{
			expect_opcode(0x6b80, data.read_word(0x0d30) == 0x123f &&
					data.read_word(0x0d40) == 0x7777 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0d40 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d30 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xec00) == 0x8000,
					"ADDM with compatibility AR0 selects ARP and clears carry without overflow");
			program.write_word(0x05e0, 0x6b8a); // ADDM #1, *AR2-
			program.write_word(0x05e1, 1);
			program.write_word(0x05e2, 0xf5e1);
			data.write_word(0x0d60, 0x7fff);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d60);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM; OVM clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 222;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 222)
		{
			expect(data.read_word(0x0d60) == 0x8000 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400,
					"ADDM wraps positive overflow with OVM clear and sets OVA without carry");
			data.write_word(0x0d60, 0xffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d60);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 223;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 223)
		{
			expect(data.read_word(0x0d60) == 0 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0800,
					"ADDM propagates unsigned carry without signed overflow");
			program.write_word(0x05e0, 0x6880); // ANDM #lk, *AR0
			program.write_word(0x05e1, 0x0f0f);
			program.write_word(0x05e2, 0x6980); // ORM #lk, *AR0
			program.write_word(0x05e3, 0x0033);
			program.write_word(0x05e4, 0xf5e1);
			data.write_word(0x0d30, 0x00f0);
			data.write_word(0x0d40, 0x7777);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0d40);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0d30);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 4 << 13);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0020); // CMPT
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 224;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 224)
		{
			expect_opcode(0x6880, data.read_word(0x0d30) == 0x0033 &&
					data.read_word(0x0d40) == 0x7777,
					"ANDM compatibility AR0 uses ARP before ORM");
			expect_opcode(0x6980, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0d40 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d30 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe000) == 0x8000,
					"ORM compatibility AR0 uses ARP without modifying either pointer");
			program.write_word(0x05e0, 0x6b8a); // ADDM #8000h, *AR2-
			program.write_word(0x05e1, 0x8000);
			program.write_word(0x05e2, 0xf5e1);
			data.write_word(0x0d60, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d60);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM and OVM clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 225;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 225)
		{
			expect(data.read_word(0x0d60) == 0 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400,
					"ADDM SXM-clear 16-bit wrap does not carry out of the 32-bit ALU");
			program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x7182); // MVDK *AR2, dmad
			program.write_word(0x05e3, 0x0d71);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0x8183); // STL B, *AR3
			program.write_word(0x05e7, 0x75d6);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0d70, 0xaaa5);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234abcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d70);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d72);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 226;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 226)
		{
			expect_opcode(0x7182, data.read_word(0x0d71) == 0xaaa5 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d70 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVDK *AR2,dmad copies through a fixed pointer in two cycles");
			expect_opcode(0x8183, data.read_word(0x0d72) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d72 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 STL B,*AR3 stores BL without changing AR3 in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x4492); // LD *AR2+,16,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x458b); // LD *AR3-,16,B
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0d00, 0x8001);
			data.write_word(0x0d10, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 227;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 227)
		{
			expect_opcode(0x4492, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80010000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d01 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD *AR2+,16,A sign-extends, increments, and costs one cycle");
			expect_opcode(0x458b, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d0f &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 LD *AR3-,16,B sign-extends, decrements, and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x4493); // LD *AR3+,16,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x458a); // LD *AR2-,16,B
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0d11, 0x7fff);
			data.write_word(0x0d01, 0x0080);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d01);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d11);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 228;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 228)
		{
			expect_opcode(0x4493, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fff0000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d12 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD *AR3+,16,A loads positive data and increments in one cycle");
			expect_opcode(0x458a, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00800000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 LD *AR2-,16,B loads positive data and decrements in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x1a82); // OR *AR2,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x1b83); // OR *AR3,B
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0d20, 0x00f0);
			data.write_word(0x0d30, 0x0f0f);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12340000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xaaaa0000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d30);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 229;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 229)
		{
			expect_opcode(0x1a82, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123400f0 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d20 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR *AR2,A preserves AR2 and costs one cycle");
			expect_opcode(0x1b83, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xaaaa0f0f &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d30 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 OR *AR3,B preserves AR3 and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x1a83); // OR *AR3,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x1b82); // OR *AR2,B
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0d20, 0x3333);
			data.write_word(0x0d30, 0x5555);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xaaaa0000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xcccc0000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d30);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 230;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 230)
		{
			expect_opcode(0x1a83, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xaaaa5555 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d30 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR *AR3,A loads AR3 data in one cycle");
			expect_opcode(0x1b82, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xcccc3333 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d20 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 OR *AR2,B loads AR2 data in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x6d8c); // MAR *AR4-
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x1c84); // XOR *AR4,A
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0d40, 0x0f0f);
			data.write_word(0x0d41, 0xf0f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12340000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0d41);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 231;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 231)
		{
			expect_opcode(0x6d8c, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d40 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 MAR *AR4- decrements AR4 in one cycle");
			expect_opcode(0x1c84, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12340f0f &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 XOR *AR4,A reads the decremented address in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x108a); // LD *AR2-,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x8084); // STL A,*AR4
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0d51, 0x8001);
			data.write_word(0x0d60, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d51);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0d60);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 232;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 232)
		{
			expect_opcode(0x108a, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d50 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD *AR2-,A sign-extends, decrements, and costs one cycle");
			expect_opcode(0x8084, data.read_word(0x0d60) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d60 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 STL A,*AR4 stores AL without changing AR4 in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x1a8b); // OR *AR3-,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xe598); // MVDD *AR3+,*AR2+
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0d6f, 0x5555);
			data.write_word(0x0d70, 0x00f0);
			data.write_word(0x0d80, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x100000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d80);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d70);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 233;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 233)
		{
			expect_opcode(0x1a8b, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1000f0 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR *AR3-,A uses the old address in one cycle");
			expect_opcode(0xe598, data.read_word(0x0d80) == 0x5555 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d70 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d81 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 MVDD copies from decremented AR3 and advances both pointers in one cycle");
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
					"fast-interrupt return continuation");
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
					"RETF restores the fast return and interrupt-mask state");
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
		program.write_word(0x004a, 0xf49b); // RETF
		program.write_word(0x0400, 0xf5e1);
		program.write_word(0x0401, 0xf5e1);
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
	optional_device<nokia_dspif_device> m_transport;
	emu_timer *m_check_timer = nullptr;
	unsigned m_phase = 0;
	unsigned m_rom4_checks = 0;
	bool m_irq_raised = false;
	unsigned m_repeat_reads = 0;
	unsigned m_irq_trigger_read = 1;
	u64 m_irq_accumulator = 0;
	u64 m_first_operand_cycle = 0;
	u64 m_last_operand_cycle = 0;
	u64 m_first_port_cycle = 0;
	u64 m_middle_port_cycle = 0;
	u64 m_last_port_cycle = 0;
	unsigned m_port_reads = 0;
	unsigned m_port_writes = 0;
	u16 m_first_port_value = 0;
	u16 m_middle_port_value = 0;
	u16 m_last_port_value = 0;
	unsigned m_saved_repeat_reads = 0;
	std::stringstream m_saved_repeat;
	std::array<u16, 0x800> m_saved_transport = {};
};

void tms320c54x_test_state::test(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	NOKIA_DSPIF(config, m_transport, 0);
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::data_map);
	m_cpu->set_addrmap(AS_IO, &tms320c54x_test_state::io_map);
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
