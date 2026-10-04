// license:BSD-3-Clause
// copyright-holders:Gaz

/* Development-only execution tests for the clean-room TMS320C54x core. */

#include "emu.h"
#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_dspif.h"
#include "nokia_cobba.h"
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
		m_bleq_case = 0;
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
			m_port_writes_at_irq = m_port_writes;
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
			expect_opcode(0xf070, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3,
					"long-immediate RPT iteration count");
			expect_opcode(0xf062, data.read_word(0x090c) == 0x1234 &&
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
			expect_opcode(0x7214, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xabcd,
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
			expect_opcode(0xf5e2, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
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
			expect_opcode(0xf520, m_cpu->state_int(tms320c54x_device::STATE_B) == 6,
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
			expect_opcode(0x3292, (m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x1f) == 1,
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
			expect_opcode(0xe902, m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
					"BANZD second delay word loads B");
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
			expect_opcode(0xf944, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x5a,
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
			expect_opcode(0xf485, m_cpu->state_int(tms320c54x_device::STATE_A) == 7,
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
			expect_opcode(0xe903, m_cpu->state_int(tms320c54x_device::STATE_B) == 3,
					"conditional branch second delay word loads B");
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
			if (m_phase == 45)
				expect_opcode(0xf120, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff,
						"long-immediate LD B preserves unsigned value with SXM clear");
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
			expect_opcode(0xec03, m_repeat_reads == 4 && m_irq_accumulator == 4 &&
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
			expect_opcode(0xf274, m_repeat_reads == 3 && m_last_operand_cycle - m_first_operand_cycle == 11 &&
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
			program.write_word(0x05e0, 0x4f93); // DST B, *AR3+
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
			program.write_word(0x05e0, 0x569b); // DLD *+AR3, A
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
			program.write_word(0x05e0, 0x4fd3); // DST B, *AR3+%
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
					5,
					"ROM4 ff0c executes or replaces two words with NOPs according to carry");
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
					"ROM4 ST #lk,*AR2- writes before decrement in two cycles");
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
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xe742); // MVMM AR4,AR2
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xe743); // MVMM AR4,AR3
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0d90);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 234;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 234)
		{
			expect_opcode(0xe742, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d90 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"ROM4 MVMM AR4,AR2 copies an auxiliary register in one cycle");
			expect_opcode(0xe743, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d90 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d90 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"ROM4 MVMM AR4,AR3 preserves its source in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xe748); // MVMM AR4,SP
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xe782); // MVMM SP,AR2
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0d90);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 235;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 235)
		{
			expect_opcode(0xe748, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0d90 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
					"MVMM AR4,SP accepts SP as destination in one cycle");
			expect_opcode(0xe782, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d90 &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"MVMM SP,AR2 accepts SP as source in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xe5c9); // MVDD *AR2+0%,*AR3+
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0e04, 0x2468);
			data.write_word(0x0e10, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0e04);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0e10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 236;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 236)
		{
			expect_opcode(0xe5c9, data.read_word(0x0e10) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0e00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e11 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 MVDD wraps source AR2 and increments destination AR3 in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf0f8); // SFTL A,-8
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12348180ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 237;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 237)
		{
			expect_opcode(0xf0f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00123481 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SFTL A,-8 clears guard bits, captures carry, and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x1293); // LDU *AR3+,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0ef0, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffabcdef00ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0ef0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must not affect LDU.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 238;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 238)
		{
			expect_opcode(0x1293, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0ef1 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LDU *AR3+,A ignores SXM, clears upper bits, and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf0c8); // XOR A<<8,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0001020304ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 239;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 239)
		{
			expect_opcode(0xf0c8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0103010704ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 XOR A<<8,A preserves the full 40-bit shifted result in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xe800); // LD #0,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 240;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 240)
		{
			expect_opcode(0xe800, m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD #0,A clears the 40-bit accumulator in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfc30); // RC TC, false
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 241;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 241)
		{
			expect_opcode(0xfc30, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 RC TC false falls through without popping and costs three cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfc30); // RC TC, true
			program.write_word(0x05e3, 0xf5e1); // Must be bypassed by the return.
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0300, 0x05e6);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 242;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 242)
		{
			expect_opcode(0xfc30, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 RC TC true pops its target and costs five cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x4ef8); // DST A, absolute Lmem
			program.write_word(0x05e3, 0x0f00);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f00, 0);
			data.write_word(0x0f01, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffabcd1234ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 243;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 243)
		{
			expect_opcode(0x4ef8, data.read_word(0x0f00) == 0xabcd &&
					data.read_word(0x0f01) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 absolute DST stores high then low word in three cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x4e8b); // DST A,*AR3-
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f10, 0);
			data.write_word(0x0f11, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x00abcd1234ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f11);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 244;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 244)
		{
			expect_opcode(0x4e8b, data.read_word(0x0f11) == 0xabcd &&
					data.read_word(0x0f10) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f0f &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"odd-address DST stores its high word at AR3 and low word before it");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x5783); // DLD *AR3,B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f20, 0x5678);
			data.write_word(0x0f21, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 245;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 245)
		{
			expect_opcode(0x5783, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80015678ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"odd-address DLD loads high word from AR3 and low word before it");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfe00); // RETD
			program.write_word(0x05e3, 0xe801); // LD #1,A, first delay word
			program.write_word(0x05e4, 0xe802); // LD #2,A, second delay word
			program.write_word(0x05e5, 0xf5e1); // Must be bypassed by the return.
			program.write_word(0x05e8, 0x75d6);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0300, 0x05e8);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 246;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 246)
		{
			expect_opcode(0xfe00, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 RETD executes two delay words before return and costs three cycles");
			expect_opcode(0xe802, m_cpu->state_int(tms320c54x_device::STATE_A) == 2,
					"ROM4 LD #2,A executes as the second RETD delay word");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x1092); // LD *AR2+, A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f20, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 247;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 247)
		{
			expect_opcode(0x1092, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD *AR2+,A sign-extends, postincrements, and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfc4b); // RC BLT, false
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 248;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 248)
		{
			expect_opcode(0xfc4b, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 RC BLT false retains SP and costs three cycles");
			program.write_word(0x05e3, 0xf5e1); // Must be bypassed by the return.
			program.write_word(0x05e8, 0x75d6);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0300, 0x05e8);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 249;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 249)
		{
			expect_opcode(0xfc4b, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 RC BLT true tests the 40-bit sign and costs five cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfa45); // BCD pmad, AEQ
			program.write_word(0x05e3, 0x05ec);
			program.write_word(0x05e4, 0xe801); // First delay word
			program.write_word(0x05e5, 0xe802); // Second delay word
			program.write_word(0x05e6, 0x75d6); // Fallthrough marker
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x05ec, 0x75d6); // Taken marker
			program.write_word(0x05ed, 0x0124);
			program.write_word(0x05ee, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 250;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 250)
		{
			expect_opcode(0xfa45, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e9 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 BCD AEQ false executes both delay words and costs three cycles");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 251;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 251)
		{
			expect_opcode(0xfa45, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ef &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 BCD AEQ true executes both delay words before target in three cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf947); // CC pmad, ALEQ
			program.write_word(0x05e3, 0x05ec);
			program.write_word(0x05e4, 0x75d6); // Fallthrough marker
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			program.write_word(0x05ec, 0x75d6); // Called marker
			program.write_word(0x05ed, 0x0124);
			program.write_word(0x05ee, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 252;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 252)
		{
			expect_opcode(0xf947, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 CC ALEQ false does not push and costs three cycles");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 253;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 253)
		{
			expect_opcode(0xf947, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
					data.read_word(0x02ff) == 0x05e4 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ef &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 CC ALEQ tests the 40-bit sign, pushes return PC, and costs five cycles");
			program.write_word(0x05e2, 0x6e8f); // BANZD pmad, *AR7-
			program.write_word(0x05e3, 0x05ec);
			program.write_word(0x05e4, 0xe801); // First delay word
			program.write_word(0x05e5, 0xe802); // Second delay word
			program.write_word(0x05e6, 0x75d6); // Fallthrough marker
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 254;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 254)
		{
			expect_opcode(0x6e8f, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0xffff &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e9 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 BANZD false decrements AR7, executes delay words, and costs two cycles");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 255;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 255)
		{
			expect_opcode(0x6e8f, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ef &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 BANZD branches on pre-decrement AR7 after delay words in two cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf020); // LD #lk, A
			program.write_word(0x05e3, 0xff80);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 256;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 256 || m_phase == 257)
		{
			const bool sign_extend = m_phase == 257;
			expect_opcode(0xf020,
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
							(sign_extend ? 0xffffffff80ULL : 0xff80ULL) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD #lk,A obeys SXM and costs two cycles");
			if (!sign_extend)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 257;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x8094); // STL A,*AR4+
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f20, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 258;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 258)
		{
			expect_opcode(0x8094, data.read_word(0x0f20) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 STL A,*AR4+ stores AL, postincrements, and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0x00f8); // ADD *(lk),A
			program.write_word(0x05e3, 0x0f20);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f20, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 259;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 259 || m_phase == 260)
		{
			const bool sign_extend = m_phase == 260;
			expect_opcode(0x00f8,
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
							(sign_extend ? 0xffffffffffULL : 0) &&
					(sign_extend || (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800)) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute ADD obeys SXM, publishes carry, and costs two cycles");
			if (!sign_extend)
			{
				data.write_word(0x0f20, 0xfffe);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 260;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x09f8); // SUB *(lk),B
			data.write_word(0x0f20, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 261;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 261 || m_phase == 262)
		{
			const bool sign_extend = m_phase == 262;
			expect_opcode(0x09f8,
					m_cpu->state_int(tms320c54x_device::STATE_B) ==
							(sign_extend ? 1 : 0xffffffffffULL) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == sign_extend &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute SUB B obeys SXM, publishes no-borrow carry, and costs two cycles");
			if (!sign_extend)
			{
				data.write_word(0x0f20, 0xfffe);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 262;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf0ff); // SFTL A,-1,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x80000003);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 263;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 263)
		{
			expect_opcode(0xf0ff, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x40000001 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SFTL A,-1,A shifts low 32 bits, publishes outgoing carry, and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xec05); // RPT #5
			program.write_word(0x05e3, 0x0082); // ADD *AR2,A
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f20, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 264;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 264)
		{
			expect_opcode(0xec05, m_cpu->state_int(tms320c54x_device::STATE_A) == 6 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
					"ROM4 RPT #5 runs its one-cycle ADD body six times after one-cycle setup");
			program.write_word(0x05e2, 0x8091); // STL A,*AR1+
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f20, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 265;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 265)
		{
			expect_opcode(0x8091, data.read_word(0x0f20) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 STL A,*AR1+ stores AL and postincrements in one cycle");
			program.write_word(0x05e2, 0x7593); // PORTW *AR3+,PA
			program.write_word(0x05e3, 0x0124);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f20, 0xabcd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 266;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 266)
		{
			expect_opcode(0x7593, m_middle_port_value == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f21 &&
					m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 2 &&
					m_last_port_cycle - m_middle_port_cycle == 2,
					"ROM4 PORTW *AR3+,PA transfers the old source and costs two cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfc20); // RC NTC, false
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 267;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 267)
		{
			expect_opcode(0xfc20, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 RC NTC false retains SP and costs three cycles");
			program.write_word(0x05e3, 0xf5e1); // Must be bypassed by the return.
			program.write_word(0x05e8, 0x75d6);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0300, 0x05e8);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 268;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 268)
		{
			expect_opcode(0xfc20, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 RC NTC true pops its target and costs five cycles");
			program.write_word(0x05e0, 0x75f8); // Absolute marker does not modify AR6.
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0x1086); // LD *AR6,A
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0d00, 0x1234);
			data.write_word(0x0f20, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 269;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 269)
		{
			expect_opcode(0x1086, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffff80ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR6,A sign-extends without pointer update in one cycle");
			program.write_word(0x05e0, 0x75f8);
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0x6f8b); // LD *AR3-,0,A
			program.write_word(0x05e4, 0x0c40);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f21, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 270;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 270)
		{
			expect_opcode(0x6f8b, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffff80ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 extended LD *AR3-,0,A sign-extends and costs two cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfc45); // RC AEQ, false with nonzero guard byte
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0100000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 271;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 271)
		{
			expect_opcode(0xfc45, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 RC AEQ false checks the 40-bit guard and costs three cycles");
			program.write_word(0x05e3, 0xf5e1); // Must be bypassed by the return.
			program.write_word(0x05e8, 0x75d6);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0300, 0x05e8);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 272;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 272)
		{
			expect_opcode(0xfc45, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 RC AEQ true pops its target and costs five cycles");
			program.write_word(0x05e0, 0x75f8);
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0xf310); // SUB #lk,B
			program.write_word(0x05e4, 0xff80);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 273;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 273)
		{
			expect_opcode(0xf310, m_cpu->state_int(tms320c54x_device::STATE_B) == 133 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 SUB #lk,B sign-extends with SXM and costs two cycles");
			program.write_word(0x05e0, 0x75f8);
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0x6d8a); // MAR *AR2-
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // Standard addressing mode.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 274;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 274)
		{
			expect_opcode(0x6d8a, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MAR *AR2- modifies the pointer in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xf846); // BC 05f0,AGT
			program.write_word(0x05e3, 0x05f0);
			program.write_word(0x05e4, 0xf5e1); // Taken branch bypasses fallthrough.
			program.write_word(0x05f0, 0x75d6);
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0100000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 275;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 275)
		{
			expect_opcode(0xf846, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 BC AGT uses the 40-bit sign and costs five cycles taken");
			program.write_word(0x05e2, 0xf84d); // BC 05f0,BEQ
			program.write_word(0x05e4, 0x75d6); // Fallthrough marker.
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0100000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 276;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 276)
		{
			expect_opcode(0xf84d, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 BC BEQ checks B's guard byte and costs three cycles false");
			program.write_word(0x05e0, 0x75f8);
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0x11f8); // LD *(lk),B
			program.write_word(0x05e4, 0x0f21);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f21, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 277;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 277)
		{
			expect_opcode(0x11f8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffff80ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 absolute LD Smem,B sign-extends and costs two cycles");
			program.write_word(0x05e3, 0xf110); // SUB #lk,A,B
			program.write_word(0x05e4, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 278;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 278)
		{
			expect_opcode(0xf110, m_cpu->state_int(tms320c54x_device::STATE_B) == 133 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 5 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 SUB #lk,A,B reads A, writes B, and costs two cycles");
			program.write_word(0x05e3, 0x7482); // PORTR PA,*AR2
			program.write_word(0x05e4, 0x0123);
			program.write_word(0x05e5, 0x7482);
			program.write_word(0x05e6, 0x0123);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f21, 0);
			m_port_reads = 0;
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 279;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 279)
		{
			expect_opcode(0x7482, data.read_word(0x0f21) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
					m_port_reads == 2 && m_port_writes == 1 &&
					m_last_port_cycle - m_first_port_cycle == 2,
					"ROM4 PORTR PA,*AR2 writes the port word in two cycles");
			program.write_word(0x05e0, 0x75f8);
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0x56f8); // DLD *(lk),A
			program.write_word(0x05e4, 0x0f20);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f20, 0xff80);
			data.write_word(0x0f21, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 280;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 280)
		{
			expect_opcode(0x56f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffff801234ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 DLD absolute reads high/low words and costs two cycles");
			program.write_word(0x05e3, 0x7213); // MVDM dmad,AR3
			program.write_word(0x05e4, 0x0f20);
			data.write_word(0x0f20, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 281;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 281)
		{
			expect_opcode(0x7213, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 MVDM copies dmad to AR3 in two cycles");
			program.write_word(0x05e3, 0x708a); // MVKD dmad,*AR2-
			program.write_word(0x05e4, 0x0f20);
			data.write_word(0x0f20, 0x5678);
			data.write_word(0x0f21, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 282;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 282)
		{
			expect_opcode(0x708a, data.read_word(0x0f21) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 MVKD copies source to *AR2- in two cycles");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xec0e); // RPT #14
			program.write_word(0x05e3, 0x0082); // ADD *AR2,A
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f21, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 283;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 283)
		{
			expect_opcode(0xec0e, m_cpu->state_int(tms320c54x_device::STATE_A) == 15 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 18,
					"ROM4 RPT #14 runs ADD fifteen times after one-cycle setup");
			program.write_word(0x05e2, 0xe753); // MVMM AR5,AR3
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 284;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 284)
		{
			expect_opcode(0xe753, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 MVMM copies AR5 to AR3 in one cycle");
			program.write_word(0x05e2, 0x7194); // MVDK *AR4+,dmad
			program.write_word(0x05e3, 0x0f22);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f21, 0xabcd);
			data.write_word(0x0f22, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 285;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 285)
		{
			expect_opcode(0x7194, data.read_word(0x0f22) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f22 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVDK copies *AR4+ to dmad in two cycles");
			program.write_word(0x05e0, 0x75f8);
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0x6c88); // BANZ 05f0,*AR0-
			program.write_word(0x05e4, 0x05f0);
			program.write_word(0x05e5, 0x75f8); // Fallthrough marker.
			program.write_word(0x05e6, 0x0d01);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x05f0, 0x75f8); // Taken marker.
			program.write_word(0x05f1, 0x0d02);
			program.write_word(0x05f2, 0x0124);
			program.write_word(0x05f3, 0xf5e1);
			data.write_word(0x0d00, 0x1111);
			data.write_word(0x0d01, 0x2222);
			data.write_word(0x0d02, 0x3333);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8000); // ARP=4
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0020); // CMPT
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 286;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 286 || m_phase == 287)
		{
			const bool taken = m_phase == 286;
			expect_opcode(0x6c88, m_port_writes == 2 &&
					m_last_port_value == (taken ? 0x3333 : 0x2222) &&
					m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == (taken ? 0 : 1) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == (taken ? 0 : 0xffff),
					"ROM4 BANZ *AR0- tests and updates ARP-selected AR4 in compatibility mode");
			if (taken)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 287;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x6e88); // BANZD 05f0,*AR0-
			program.write_word(0x05e5, 0xf495); // First delay word.
			program.write_word(0x05e6, 0xf495); // Second delay word.
			program.write_word(0x05e7, 0x75f8); // Must be bypassed.
			program.write_word(0x05e8, 0x0d01);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 288;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 288)
		{
			expect_opcode(0x6e88, m_port_writes == 2 && m_last_port_value == 0x3333 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0 &&
					m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 BANZD *AR0- tests ARP-selected AR4 before delayed branch");
			program.write_word(0x05e0, 0x75f8);
			program.write_word(0x05e1, 0x0d00);
			program.write_word(0x05e2, 0x0124);
			program.write_word(0x05e3, 0xf063); // AND #lk,16,A
			program.write_word(0x05e4, 0x0ff0);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // C
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 289;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 289)
		{
			expect_opcode(0xf063, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x02300000 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 AND #lk,16,A masks upper word and costs two cycles");
			program.write_word(0x05e3, 0x8184); // STL B,*AR4
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f21, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f21);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 290;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 290)
		{
			expect_opcode(0x8184, data.read_word(0x0f21) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STL B,*AR4 stores BL without pointer update in one cycle");
			program.write_word(0x05e3, 0xf162); // LD #lk,16,B
			program.write_word(0x05e4, 0xff80);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 291;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 291)
		{
			expect_opcode(0xf162, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 LD #lk,16,B sign-extends and costs two cycles");
			program.write_word(0x05e3, 0x6d96); // MAR *AR6+
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0f42);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 292;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 292)
		{
			expect_opcode(0x6d96, m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0f43 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MAR *AR6+ increments AR6 in one cycle");
			program.write_word(0x05e3, 0xe712); // MVMM AR1,AR2
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1a23);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 293;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 293)
		{
			expect_opcode(0xe712, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1a23 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1a23 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVMM AR1,AR2 copies without modifying source in one cycle");
			program.write_word(0x05e3, 0xe713); // MVMM AR1,AR3
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 294;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 294)
		{
			expect_opcode(0xe713, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1a23 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1a23 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVMM AR1,AR3 copies without modifying source in one cycle");
			program.write_word(0x05e3, 0x6c8b); // BANZ 05eb,*AR3-
			program.write_word(0x05e4, 0x05eb);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x05eb, 0x75f8);
			program.write_word(0x05ec, 0x0d00);
			program.write_word(0x05ed, 0x0124);
			program.write_word(0x05ee, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 295;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 295 || m_phase == 296)
		{
			expect_opcode(0x6c8b, m_cpu->state_int(tms320c54x_device::STATE_AR3) ==
					(m_phase == 295 ? 0 : 0xffff) && m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 295 ? 7 : 5),
					"ROM4 BANZ *AR3- tests before decrement and has taken/false cycle costs");
			if (m_phase == 295)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 296;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0xf847); // BC 05eb,ALEQ
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 297;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 297 || m_phase == 298)
		{
			expect_opcode(0xf847, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 297 ? 8 : 6),
					"ROM4 BC ALEQ uses the 40-bit sign and has taken/false cycle costs");
			if (m_phase == 297)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 298;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x880e); // STLM A,T
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 299;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 299)
		{
			expect_opcode(0x880e, m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345678ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STLM A,T copies AL into T in one cycle");
			program.write_word(0x05e3, 0x0af8); // SUBS *abs,A
			program.write_word(0x05e4, 0x0f64);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f64, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 300;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 300 || m_phase == 301)
		{
			expect_opcode(0x0af8,
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 300 ? 1 : 0xffffff0001ULL) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
					(m_phase == 300) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 absolute SUBS suppresses sign extension, sets carry, and costs two cycles");
			if (m_phase == 300)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 301;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x1e82); // SUBC *AR2,A
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f65, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f65);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 302;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 302 || m_phase == 303)
		{
			expect_opcode(0x1e82,
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 302 ? 1 : 0xfffe) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
					(m_phase == 302) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f65 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SUBC sets quotient and carry only on successful subtraction in one cycle");
			if (m_phase == 302)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fff);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 303;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x6dea); // MAR *+AR2(-7)
			program.write_word(0x05e4, 0xfff9);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 304;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 304)
		{
			expect_opcode(0x6dea, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0ff9 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 MAR long-offset consumes signed extension and costs two cycles");
			program.write_word(0x05e3, 0xf300); // ADD #lk,B
			program.write_word(0x05e4, 0xffff);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 305;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 305 || m_phase == 306)
		{
			expect_opcode(0xf300,
					m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(m_phase == 305 ? 0xffffffffffULL : 0x100000000ULL) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
					(m_phase == 306) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 ADD #lk,B sign-extends under SXM and carries at bit 32 in two cycles");
			if (m_phase == 305)
			{
				program.write_word(0x05e4, 1);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 306;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0xf210); // SUB #lk,B,A
			program.write_word(0x05e4, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 307;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 307 || m_phase == 308)
		{
			expect_opcode(0xf210,
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 307 ? 8 : 0xffffffffffULL) &&
					m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(m_phase == 307 ? 10 : 0) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
					(m_phase == 307) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 SUB #lk,B,A uses B as source, preserves it, and costs two cycles");
			if (m_phase == 307)
			{
				program.write_word(0x05e4, 1);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 308;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0xf150); // XOR #lk,A,B
			program.write_word(0x05e4, 0xff00);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // C
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 309;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 309)
		{
			expect_opcode(0xf150,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345678ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff1234a978ULL &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 XOR #lk,A,B uses unextended operand, preserves A/C, and costs two cycles");
			program.write_word(0x05e3, 0x0282); // ADDS *AR2,A
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f66, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f66);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 310;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 310 || m_phase == 311)
		{
			expect_opcode(0x0282,
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 310 ? 0x10000 : 0x100000000ULL) &&
					bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
					(m_phase == 311) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f66 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ADDS *AR2,A ignores SXM and carries at bit 32 in one cycle");
			if (m_phase == 310)
			{
				data.write_word(0x0f66, 1);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 311;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x06ea); // ADDC *+AR2(5),A
			program.write_word(0x05e4, 5);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f05, 4);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x13);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // C
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 312;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 312)
		{
			expect_opcode(0x06ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x18 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ADDC *+AR2(lk),A preupdates AR and costs two cycles");
			program.write_word(0x05e3, 0x02e2); // ADDS *AR2(5),A
			data.write_word(0x0f05, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 313;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 313)
		{
			expect_opcode(0x02e2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ADDS *AR2(lk),A uses offset address without updating AR");
			program.write_word(0x05e3, 0x02f2); // ADDS *+AR2(2)%,A
			program.write_word(0x05e4, 2);
			data.write_word(0x0f01, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 314;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 314)
		{
			expect_opcode(0x02f2, m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f01 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ADDS *+AR2(lk)%,A wraps circular address before read in two cycles");
			program.write_word(0x05e3, 0x80ea); // STL A,*+AR2(-2)
			program.write_word(0x05e4, 0xfffe);
			data.write_word(0x0f0e, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f10);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 315;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 315)
		{
			expect_opcode(0x80ea, data.read_word(0x0f0e) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f0e &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"STL A,*+AR2(lk) stores after signed preupdate in two cycles");
			program.write_word(0x05e3, 0x6bea); // ADDM #1,*+AR2(5)
			program.write_word(0x05e4, 1);
			program.write_word(0x05e5, 5);
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0f05, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 316;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 316)
		{
			expect_opcode(0x6bea, data.read_word(0x0f05) == 4 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ADDM *+AR2(lk) consumes immediate before offset in three cycles");
			program.write_word(0x05e3, 0x68e2); // ANDM #0f0f,*AR2(5)
			program.write_word(0x05e4, 0x0f0f);
			program.write_word(0x05e5, 5);
			data.write_word(0x0f05, 0xff00);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // C
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 317;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 317)
		{
			expect_opcode(0x68e2, data.read_word(0x0f05) == 0x0f00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ANDM *AR2(lk) uses offset without AR update or status change");
			program.write_word(0x05e3, 0x69f2); // ORM #8000,*+AR2(2)%
			program.write_word(0x05e4, 0x8000);
			program.write_word(0x05e5, 2);
			data.write_word(0x0f01, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f03);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 318;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 318)
		{
			expect_opcode(0x69f2, data.read_word(0x0f01) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f01 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ORM *+AR2(lk)% wraps circular address and preserves carry");
			program.write_word(0x05e3, 0x76ea); // STM #cafe,*+AR2(5)
			program.write_word(0x05e4, 0xcafe);
			program.write_word(0x05e5, 5);
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 319;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 319)
		{
			expect_opcode(0x76ea, data.read_word(0x0f05) == 0xcafe &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"STM *+AR2(lk) consumes immediate before offset in three cycles");
			program.write_word(0x05e3, 0x82ea); // STH A,*+AR2(5)
			program.write_word(0x05e4, 5);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 320;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 320)
		{
			expect_opcode(0x82ea, data.read_word(0x0f05) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"STH A,*+AR2(lk) stores high word after preupdate in two cycles");
			program.write_word(0x05e3, 0x83e2); // STH B,*AR2(5)
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x56789abc);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 321;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 321)
		{
			expect_opcode(0x83e2, data.read_word(0x0f05) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"STH B,*AR2(lk) stores high word without AR update in two cycles");
			program.write_word(0x05e3, 0x8cea); // ST T,*+AR2(5)
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4321);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 322;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 322)
		{
			expect_opcode(0x8cea, data.read_word(0x0f05) == 0x4321 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ST T,*+AR2(lk) stores after preupdate in two cycles");
			program.write_word(0x05e3, 0x10ea); // LD *+AR2(5),A
			data.write_word(0x0f05, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 323;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 323)
		{
			expect_opcode(0x10ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"LD *+AR2(lk),A sign-extends in two cycles");
			program.write_word(0x05e3, 0x12e2); // LDU *AR2(5),A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 324;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 324)
		{
			expect_opcode(0x12e2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"LDU *AR2(lk),A zero-extends without AR update in two cycles");
			program.write_word(0x05e3, 0x45ea); // LD *+AR2(5),16,B
			data.write_word(0x0f05, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 325;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 325)
		{
			expect_opcode(0x45ea, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12340000 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"LD *+AR2(lk),16,B shifts in two cycles");
			program.write_word(0x05e3, 0xfa20); // BCD 05f0h, NTC
			program.write_word(0x05e4, 0x05f0);
			program.write_word(0x05e5, 0xe801); // Delay slot 1
			program.write_word(0x05e6, 0xe902); // Delay slot 2
			program.write_word(0x05e7, 0xe803); // Fall-through only
			program.write_word(0x05e8, 0x75f8);
			program.write_word(0x05e9, 0x0d00);
			program.write_word(0x05ea, 0x0124);
			program.write_word(0x05eb, 0xf5e1);
			program.write_word(0x05f0, 0x75f8);
			program.write_word(0x05f1, 0x0d00);
			program.write_word(0x05f2, 0x0124);
			program.write_word(0x05f3, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // NTC true
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 326;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 326)
		{
			expect_opcode(0xfa20, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"BCD NTC executes two delay words then branches in three cycles");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // NTC false
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 327;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 327)
		{
			expect_opcode(0xfa20, m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ec &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
					"BCD NTC false executes delay words and falls through");
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_phase = 328;
		}
		if (m_phase >= 328 && m_phase <= 331)
		{
			static constexpr u16 opcodes[] = { 0x8814, 0x8813, 0x8811, 0x8912 };
			static constexpr unsigned registers[] = { 4, 3, 1, 2 };
			const unsigned index = m_phase - 328;
			if (index)
			{
				expect_opcode(opcodes[index - 1],
						m_cpu->state_int(tms320c54x_device::STATE_AR0 + registers[index - 1]) ==
							0xbeef &&
						m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
						"STLM writes the selected address register in one cycle");
			}
			program.write_word(0x05e3, opcodes[index]);
			m_port_writes = 0;
			for (unsigned ar = 0; ar != 8; ++ar)
				m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234beef);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678cafe);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 332)
		{
			expect_opcode(0x8912,
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0xcafe &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"STLM B writes AR2 in one cycle");
			program.write_word(0x05e3, 0xec0c); // RPT #12
			program.write_word(0x05e4, 0x6d10); // MAR *AR0+
			program.write_word(0x05e5, 0xe80f); // LD #15,A
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 333;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 333)
		{
			expect_opcode(0xec0c, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 13 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 18,
					"RPT #12 executes MAR thirteen times after one-cycle setup");
			expect_opcode(0xe80f, m_cpu->state_int(tms320c54x_device::STATE_A) == 15,
					"LD #15,A writes the accumulator after the repeated body");
			program.write_word(0x05e3, 0x13f8); // LDU *(0f20h),B
			program.write_word(0x05e4, 0x0f20);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f20, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 334;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 334)
		{
			expect_opcode(0x13f8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"absolute LDU B zero-extends even with SXM set in two cycles");
			program.write_word(0x05e3, 0x81f8); // STL B,*(0f21h)
			program.write_word(0x05e4, 0x0f21);
			data.write_word(0x0f21, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234beef);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 335;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 335)
		{
			expect_opcode(0x81f8, data.read_word(0x0f21) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234beef &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"absolute STL B stores the low word in two cycles");
			program.write_word(0x05e3, 0x7582); // PORTW *AR2,0124h
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f20, 0xface);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 336;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 336)
		{
			expect_opcode(0x7582, m_middle_port_value == 0xface &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
					"PORTW *AR2 emits memory word without modifying AR2 in two cycles");
			program.write_word(0x05e3, 0x61ea); // BITF *+AR2(5),#8000
			program.write_word(0x05e4, 0x8000);
			program.write_word(0x05e5, 5);
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0f05, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 337;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 337)
		{
			expect_opcode(0x61ea, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) == 0x1800 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"long-offset BITF sets TC and preserves carry in three cycles");
			program.write_word(0x05e3, 0x60e2); // CMPM *AR2(5),#1235
			program.write_word(0x05e4, 0x1235);
			program.write_word(0x05e5, 5);
			data.write_word(0x0f05, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 338;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 338)
		{
			expect_opcode(0x60e2, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) == 0x0800 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"long-offset CMPM clears TC without AR update in three cycles");
			program.write_word(0x05e3, 0x6180); // BITF *AR0,#0001
			program.write_word(0x05e4, 1);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f20, 4);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 339;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 339)
		{
			expect_opcode(0x6180, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) == 0x0800 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"BITF *AR0 clears TC when masked bit is absent in two cycles");
			program.write_word(0x05e3, 0xe736); // MVMM AR3,AR6
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1357);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 340;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 340)
		{
			expect_opcode(0xe736, m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1357 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1357 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"MVMM AR3,AR6 transfers the MMR word in one cycle");
			program.write_word(0x05e3, 0xfa4d); // BCD 05f0h, BEQ
			program.write_word(0x05e4, 0x05f0);
			program.write_word(0x05e5, 0xe801); // Delay slot 1
			program.write_word(0x05e6, 0xe902); // Delay slot 2 changes B
			program.write_word(0x05e7, 0xe803); // Fall-through only
			program.write_word(0x05e8, 0x75f8);
			program.write_word(0x05e9, 0x0d00);
			program.write_word(0x05ea, 0x0124);
			program.write_word(0x05eb, 0xf5e1);
			program.write_word(0x05f0, 0x75f8);
			program.write_word(0x05f1, 0x0d00);
			program.write_word(0x05f2, 0x0124);
			program.write_word(0x05f3, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 341;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 341)
		{
			expect_opcode(0xfa4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"BCD BEQ uses pre-slot B and executes both delay words");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 342;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 342)
		{
			expect_opcode(0xfa4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ec &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
					"BCD BEQ false executes slots and falls through");
			program.write_word(0x05e3, 0xfc4d); // RC BEQ
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x02ff, 0x05f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x02ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 343;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 343)
		{
			expect_opcode(0xfc4d, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"RC BEQ pops return address in five cycles when B is zero");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x02ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 344;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 344)
		{
			expect_opcode(0xfc4d, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"RC BEQ leaves stack untouched in three cycles when B is nonzero");
			program.write_word(0x05e3, 0x47e2); // RPT *AR2(5)
			program.write_word(0x05e4, 5);
			program.write_word(0x05e5, 0x6d10); // MAR *AR0+
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0f05, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 345;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 345)
		{
			expect_opcode(0x47e2, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
					"long-offset RPT Smem runs MAR three times after four-cycle setup");
			program.write_word(0x05e3, 0x4bea); // PSHD *+AR2(5)
			program.write_word(0x05e4, 5);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f05, 0x1234);
			data.write_word(0x02ff, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 346;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 346)
		{
			expect_opcode(0x4bea, data.read_word(0x02ff) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"long-offset PSHD preupdates AR and pushes in two cycles");
			program.write_word(0x05e3, 0x8bea); // POPD *+AR2(5)
			data.write_word(0x02ff, 0xbeef);
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 347;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 347)
		{
			expect_opcode(0x8bea, data.read_word(0x0f05) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"long-offset POPD preupdates AR and pops in two cycles");
			program.write_word(0x05e3, 0x4fea); // DST B,*+AR2(6)
			program.write_word(0x05e4, 6);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f06, 0);
			data.write_word(0x0f07, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 348;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 348)
		{
			expect_opcode(0x4fea, data.read_word(0x0f06) == 0x1234 &&
					data.read_word(0x0f07) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f06 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"long-offset DST preupdates AR and stores high/low in three cycles");
			program.write_word(0x05e3, 0x57e2); // DLD *AR2(6),B
			data.write_word(0x0f06, 0x8001);
			data.write_word(0x0f07, 0x2345);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 349;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 349)
		{
			expect_opcode(0x57e2, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80012345ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"long-offset DLD keeps AR and sign-extends in two cycles");
			program.write_word(0x05e3, 0x40ea); // SUB *+AR2(5),16,A
			program.write_word(0x05e4, 5);
			data.write_word(0x0f05, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x30000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 350;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 350)
		{
			expect_opcode(0x40ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x20000 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"long-offset SUB preupdates AR and takes two cycles");
			program.write_word(0x05e3, 0x34e2); // BITT *AR2(5)
			program.write_word(0x05e4, 5);
			data.write_word(0x0f05, 0x8000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 351;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 351)
		{
			expect_opcode(0x34e2, bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"long-offset BITT keeps AR and takes two cycles");
			program.write_word(0x05e3, 0x20ea); // MPY *+AR2(5),A
			program.write_word(0x05e4, 5);
			data.write_word(0x0f05, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 352;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 352)
		{
			expect_opcode(0x20ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 6 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"long-offset MPY preupdates AR and takes two cycles");
			program.write_word(0x05e3, 0x1282); // LDU *AR2,A
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f00, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must not sign-extend LDU.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 353;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 353)
		{
			expect_opcode(0x1282, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LDU *AR2,A zero-extends without changing AR2 in one cycle");
			program.write_word(0x05e3, 0x1082); // LD *AR2,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 354;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 354)
		{
			expect_opcode(0x1082, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR2,A sign-extends without changing AR2 in one cycle");
			program.write_word(0x05e3, 0xf84e); // BC 05f0,BGT
			program.write_word(0x05e4, 0x05f0);
			program.write_word(0x05e5, 0x75f8); // Fallthrough marker.
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x05f0, 0x75f8); // Taken marker.
			program.write_word(0x05f1, 0x0d00);
			program.write_word(0x05f2, 0x0124);
			program.write_word(0x05f3, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 355;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 355 || m_phase == 356)
		{
			expect_opcode(0xf84e, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 355 ? 8 : 6) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 355 ? 0x05f4 : 0x05e9),
					"ROM4 BC BGT tests signed 40-bit B and costs five/three cycles");
			if (m_phase == 355)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff00000000ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 356;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0xec01); // RPT #1
			program.write_word(0x05e4, 0x6d10); // MAR *AR0+
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 357;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 357)
		{
			expect_opcode(0xec01, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f02 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 RPT #1 executes MAR twice after one-cycle setup");
			program.write_word(0x05e3, 0xfa30); // BCD 05f0,TC
			program.write_word(0x05e4, 0x05f0);
			program.write_word(0x05e5, 0xe801); // Delay slot 1
			program.write_word(0x05e6, 0xe902); // Delay slot 2
			program.write_word(0x05e7, 0xe803); // Fallthrough only
			program.write_word(0x05e8, 0x75f8);
			program.write_word(0x05e9, 0x0d00);
			program.write_word(0x05ea, 0x0124);
			program.write_word(0x05eb, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // TC true
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 358;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 358 || m_phase == 359)
		{
			expect_opcode(0xfa30,
					m_cpu->state_int(tms320c54x_device::STATE_A) == (m_phase == 358 ? 1 : 3) &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 358 ? 0x05f4 : 0x05ec) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
						(m_phase == 358 ? 8 : 9),
					"ROM4 BCD TC executes both delay words and selects the correct target");
			if (m_phase == 358)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // TC false
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 359;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x8914); // STLM B,AR4
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678cafe);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 360;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 360)
		{
			expect_opcode(0x8914, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xcafe &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STLM B,AR4 stores BL in one cycle");
			program.write_word(0x05e3, 0x71ea); // MVDK *+AR2(5),0f20
			program.write_word(0x05e4, 0x0f20);
			program.write_word(0x05e5, 5);
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0f05, 0xbeef);
			data.write_word(0x0f20, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 361;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 361)
		{
			expect_opcode(0x71ea, data.read_word(0x0f20) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"long-offset MVDK consumes destination before offset in three cycles");
			program.write_word(0x05e3, 0x75ea); // PORTW *+AR2(5),0124
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 5);
			data.write_word(0x0f05, 0xabcd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 362;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 362)
		{
			expect_opcode(0x75ea, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 3 && m_middle_port_value == 0xabcd &&
					m_last_port_cycle - m_middle_port_cycle == 3,
					"long-offset PORTW consumes port before offset and takes three cycles");
			program.write_word(0x05e3, 0x70ea); // MVKD 0f20,*+AR2(5)
			program.write_word(0x05e4, 0x0f20);
			program.write_word(0x05e5, 5);
			data.write_word(0x0f20, 0x5678);
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 363;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 363)
		{
			expect_opcode(0x70ea, data.read_word(0x0f05) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"long-offset MVKD consumes source before offset and takes three cycles");
			program.write_word(0x05e3, 0x7dea); // MVDP *+AR2(5),0600
			program.write_word(0x05e4, 0x0600);
			program.write_word(0x05e5, 5);
			program.write_word(0x0600, 0);
			data.write_word(0x0f05, 0xbeef);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 364;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 364)
		{
			expect_opcode(0x7dea, program.read_word(0x0600) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"long-offset MVDP consumes pmad before offset and takes five cycles");
			program.write_word(0x05e3, 0x7fea); // WRITA *+AR2(5)
			program.write_word(0x05e4, 5);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x0600, 0);
			data.write_word(0x0f05, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0600);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 365;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 365)
		{
			expect_opcode(0x7fea, program.read_word(0x0600) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
					"long-offset WRITA stores the data word at A in six cycles");
			program.write_word(0x05e3, 0x7eea); // READA *+AR2(5)
			program.write_word(0x0600, 0xabcd);
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 366;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 366)
		{
			expect_opcode(0x7eea, data.read_word(0x0f05) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
					"long-offset READA stores the A-addressed program word in six cycles");
			program.write_word(0x05e3, 0x7cea); // MVPD 0600,*+AR2(5)
			program.write_word(0x05e4, 0x0600);
			program.write_word(0x05e5, 5);
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			program.write_word(0x0600, 0xbeef);
			data.write_word(0x0f05, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 367;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 367)
		{
			expect_opcode(0x7cea, data.read_word(0x0f05) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"long-offset MVPD consumes pmad before offset in four cycles");
			program.write_word(0x05e3, 0x74ea); // PORTR 0123,*+AR2(5)
			program.write_word(0x05e4, 0x0123);
			program.write_word(0x05e5, 5);
			data.write_word(0x0f05, 0);
			m_port_reads = 0;
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 368;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 368)
		{
			expect_opcode(0x74ea, data.read_word(0x0f05) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_reads == 1 && m_port_writes == 2 &&
					m_middle_port_cycle - m_first_port_cycle == 3,
					"long-offset PORTR consumes port before offset and takes three cycles");
			program.write_word(0x05e3, 0x6fea); // LD *+AR2(5),0,A
			program.write_word(0x05e4, 0x0c40);
			program.write_word(0x05e5, 5);
			data.write_word(0x0f05, 0x2345);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 369;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 369)
		{
			expect_opcode(0x6fea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x2345 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"long-offset shifted LD consumes shift before offset in three cycles");
			program.write_word(0x05e3, 0xe58b); // MVDD *AR2+,*AR5+
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0d20, 0x4321);
			data.write_word(0x0e20, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0e20);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 370;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 370)
		{
			expect_opcode(0xe58b, data.read_word(0x0e20) == 0x4321 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d21 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0e21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVDD *AR2+,*AR5+ copies before both increments in one cycle");
			program.write_word(0x05e3, 0x13d2); // LDU *AR2+%,B
			data.write_word(0x0f23, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 371;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 371)
		{
			expect_opcode(0x13d2, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 unsigned LDU wraps AR2 circularly after the read in one cycle");
			program.write_word(0x05e3, 0x1882); // AND *AR2,A
			data.write_word(0x0f20, 0x0f0f);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 372;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 372)
		{
			expect_opcode(0x1882, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0204 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 AND *AR2,A masks without updating the pointer in one cycle");
			program.write_word(0x05e3, 0x1081); // LD *AR1,A
			data.write_word(0x0f20, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 373;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 373)
		{
			expect_opcode(0x1081, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR1,A sign-extends under SXM without updating AR1 in one cycle");
			program.write_word(0x05e3, 0xee01); // FRAME #1
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x03ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 374;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 374)
		{
			expect_opcode(0xee01, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 FRAME #1 advances SP across a page boundary in one cycle");
			program.write_word(0x05e3, 0xf340); // OR #lk,B
			program.write_word(0x05e4, 0x00f0);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xab00123400ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 375;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 375)
		{
			expect_opcode(0xf340, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xab001234f0ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 OR #lk,B preserves upper accumulator bits and takes two cycles");
			program.write_word(0x05e3, 0xf230); // AND #lk,B,A
			program.write_word(0x05e4, 0x0ff0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffabcdf123ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 376;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 376)
		{
			expect_opcode(0xf230, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0120 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffabcdf123ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 AND #lk,B,A masks into A without changing B in two cycles");
			program.write_word(0x05e3, 0xf517); // ADD A >> 9,B
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 377;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 377 || m_phase == 378)
		{
			expect_opcode(0xf517,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(m_phase == 377 ? 0x7fc00003ULL : 0xffffc00003ULL) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ADD A >> 9,B uses SXM-controlled fill in one cycle");
			if (m_phase == 377)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 3);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM on
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 378;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0xf4e2); // BACC A
			program.write_word(0x05f0, 0x75f8);
			program.write_word(0x05f1, 0x0d00);
			program.write_word(0x05f2, 0x0124);
			program.write_word(0x05f3, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x05f0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x05e4);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 379;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 379)
		{
			expect_opcode(0xf4e2, m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
					"ROM4 BACC A branches to A's low word without a stack push in six cycles");
			program.write_word(0x05e3, 0xf474); // SFTA A,-12
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000800ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 380;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 380 || m_phase == 381)
		{
			expect_opcode(0xf474,
					m_cpu->state_int(tms320c54x_device::STATE_A) ==
					(m_phase == 380 ? 0x00ff80000ULL : 0xfffff80000ULL) &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SFTA A,-12 uses SXM fill and exports shifted-out bit 11 to carry");
			if (m_phase == 380)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000800ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM on
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 381;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x7722); // STM #lk, MMR 22h
			program.write_word(0x05e4, 0xa55a);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0022, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 382;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 382)
		{
			expect_opcode(0x7722, data.read_word(0x0022) == 0xa55a &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 STM #lk,MMR22 writes its immediate in two cycles");
			program.write_word(0x05e3, 0xf537); // SUB A >> 9,B
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 383;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 383 || m_phase == 384)
		{
			expect_opcode(0xf537,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(m_phase == 383 ? 0xff80400000ULL : 0x00400000ULL) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"shifted SUB uses SXM-controlled fill without changing A in one cycle");
			if (m_phase == 383)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM on
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 384;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x6184); // BITF *AR4,#00f0
			program.write_word(0x05e4, 0x00f0);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f20, 0x00a0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 385;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 385 || m_phase == 386)
		{
			expect_opcode(0x6184,
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) ==
					(m_phase == 385 ? 0x1800 : 0x0800) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 BITF *AR4 sets and clears TC while preserving carry in two cycles");
			if (m_phase == 385)
			{
				data.write_word(0x0f20, 0x0a00);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 386;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0xe908); // LD #8,B
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 387;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 387)
		{
			expect_opcode(0xe908, m_cpu->state_int(tms320c54x_device::STATE_B) == 8 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD #8,B replaces B without changing A in one cycle");
			program.write_word(0x05e3, 0x4914); // LDM AR4,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x8001);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 388;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 388)
		{
			expect_opcode(0x4914, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LDM AR4,B zero-fills despite SXM in one cycle");
			program.write_word(0x05e3, 0xf3ff); // SFTL B,-1
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff80000003ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must not sign-fill SFTL.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 389;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 389)
		{
			expect_opcode(0xf3ff, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x40000001 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SFTL B,-1 zero-fills guard bits and shifts bit 0 into carry");
			program.write_word(0x05e3, 0x60f8); // CMPM *(lk),#1234
			program.write_word(0x05e4, 0x0f24);
			program.write_word(0x05e5, 0x1234);
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0f24, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 390;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 390)
		{
			expect_opcode(0x60f8, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) == 0x1800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 absolute CMPM consumes address before immediate and takes three cycles");
			program.write_word(0x05e3, 0x0892); // SUB *AR2+,A
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f20, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 391;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 391)
		{
			expect_opcode(0x0892, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SUB *AR2+,A sign-extends and increments after read in one cycle");
			program.write_word(0x05e3, 0x8082); // STL A,*AR2
			data.write_word(0x0f30, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcd1234ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f30);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 392;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 392)
		{
			expect_opcode(0x8082, data.read_word(0x0f30) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xabcd1234ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f30 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STL A,*AR2 writes the low word without pointer update in one cycle");
			program.write_word(0x05e3, 0x10d2); // LD *AR2+%,A
			data.write_word(0x0f23, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 393;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 393)
		{
			expect_opcode(0x10d2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 signed LD sign-extends before circular AR2 post-update in one cycle");
			program.write_word(0x05e3, 0xe50b); // MVDD *AR2,*AR5+
			data.write_word(0x0f20, 0x5a3c);
			data.write_word(0x0f33, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f33);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 394;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 394)
		{
			expect_opcode(0xe50b, data.read_word(0x0f33) == 0x5a3c &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f34 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVDD copies through X/Y pointers and increments only Y in one cycle");
			program.write_word(0x05e3, 0x1083); // LD *AR3,A
			data.write_word(0x0f35, 0x8002);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f35);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 395;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 395)
		{
			expect_opcode(0x1083, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8002ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f35 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR3,A sign-extends without pointer update in one cycle");
			program.write_word(0x05e3, 0x1284); // LDU *AR4,A
			data.write_word(0x0f36, 0x8002);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f36);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 396;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 396)
		{
			expect_opcode(0x1284, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8002 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f36 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LDU *AR4,A zero-extends without pointer update in one cycle");
			program.write_word(0x05e3, 0x890e); // STLM B,T
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 397;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 397)
		{
			expect_opcode(0x890e, m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STLM B,T stores B's low word in one cycle");
			program.write_word(0x05e3, 0xf1f4); // SFTL A,-12,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345e78);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 398;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 398)
		{
			expect_opcode(0xf1f4, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345e78 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SFTL A,-12,B zero-fills B and moves source bit 11 into carry");
			program.write_word(0x05e3, 0xf280); // AND B,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xf0f0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0ff0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 399;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 399)
		{
			expect_opcode(0xf280, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00f0 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0ff0 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 AND B,A updates only A in one cycle");
			program.write_word(0x05e3, 0xf350); // XOR #lk,B
			program.write_word(0x05e4, 0x00ff);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 400;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 400)
		{
			expect_opcode(0xf350, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345687 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xabcd &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 XOR #lk,B changes only B and takes two cycles");
			program.write_word(0x05e3, 0x71d2); // MVDK *AR2+%,dmad
			program.write_word(0x05e4, 0x0f40);
			program.write_word(0x05e5, 0x75f8);
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f23, 0x5a3c);
			data.write_word(0x0f40, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 401;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 401)
		{
			expect_opcode(0x71d2, data.read_word(0x0f40) == 0x5a3c &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 MVDK copies before circular AR2 wrap in two cycles");
			program.write_word(0x05e3, 0xf842); // BC 05f0,AGEQ
			program.write_word(0x05e4, 0x05f0);
			program.write_word(0x05e5, 0x75f8); // Fallthrough marker.
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x05f0, 0x75f8); // Taken marker.
			program.write_word(0x05f1, 0x0d00);
			program.write_word(0x05f2, 0x0124);
			program.write_word(0x05f3, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 402;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 402 || m_phase == 403)
		{
			expect_opcode(0xf842, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 402 ? 8 : 6) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 402 ? 0x05f4 : 0x05e9),
					"ROM4 BC AGEQ takes zero A and rejects negative A at five/three cycles");
			if (m_phase == 402)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 403;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x0083); // ADD *AR3,A
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f52, 0xfffe);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f52);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 404;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 404)
		{
			expect_opcode(0x0083, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffffULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f52 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ADD *AR3,A sign-extends negative Smem without pointer update");
			program.write_word(0x05e3, 0xe726); // MVMM AR2,AR6
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x4567);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 405;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 405)
		{
			expect_opcode(0xe726, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x4567 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x4567 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVMM copies AR2 into AR6 in one cycle");
			program.write_word(0x05e3, 0x7726); // STM #TSS,TCR
			program.write_word(0x05e4, 0x0010);
			program.write_word(0x05e5, 0x4826); // LDM TCR,A
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 406;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 406)
		{
			expect_opcode(0x7726, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0010 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 STM #TSS,TCR survives MMR readback with two-cycle store");
			program.write_word(0x05e3, 0xf945); // CC 05f0,AEQ
			program.write_word(0x05e4, 0x05f0);
			program.write_word(0x05e5, 0x75f8); // Fallthrough marker.
			program.write_word(0x05e6, 0x0d00);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x05f0, 0x75f8); // Taken marker.
			program.write_word(0x05f1, 0x0d00);
			program.write_word(0x05f2, 0x0124);
			program.write_word(0x05f3, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 407;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 407 || m_phase == 408)
		{
			expect_opcode(0xf945, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 407 ? 8 : 6) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 407 ? 0x05f4 : 0x05e9) &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == (m_phase == 407 ? 0x02ff : 0x0300) &&
					data.read_word(0x02ff) == (m_phase == 407 ? 0x05e5 : 0xa55a),
					"ROM4 CC AEQ pushes return only on the five-cycle taken path");
			if (m_phase == 407)
			{
				data.write_word(0x02ff, 0xa55a);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
				m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 408;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0xfc44); // RC ANEQ
			program.write_word(0x05e4, 0x75f8); // Fallthrough marker.
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x02ff, 0x05f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x02ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 409;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 409 || m_phase == 410)
		{
			expect_opcode(0xfc44, m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 409 ? 8 : 6) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 409 ? 0x05f4 : 0x05e8) &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == (m_phase == 409 ? 0x0300 : 0x02ff),
					"ROM4 RC ANEQ pops only on the five-cycle taken path");
			if (m_phase == 409)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x02ff);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 410;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e3, 0x2883); // MAC *AR3,A
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f52, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f52);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xfffe);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0040); // FRCT
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 411;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 411)
		{
			expect_opcode(0x2883, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffff9ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f52 &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"fractional MAC doubles a negative product without changing T or AR3");
			program.write_word(0x05e3, 0xa43a); // MPY *AR5,*AR4+,A
			data.write_word(0x0f60, 0xfffe);
			data.write_word(0x0f70, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f60);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f70);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 412;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 412)
		{
			expect_opcode(0xa43a, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffff4ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f60 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f71 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"fractional dual MPY doubles a negative product and updates only Y");
			program.write_word(0x05e3, 0xe201); // SQDST *AR2,*AR3
			data.write_word(0x0f80, 0xffff);
			data.write_word(0x0f81, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x00020000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f80);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f81);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 413;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 413)
		{
			expect_opcode(0xe201, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffe0000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 11 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f80 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f81 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"SQDST sign-extends negative X-Y while accumulating fractional square");
			program.write_word(0x05e3, 0x2282); // MPYR *AR2,A
			data.write_word(0x0f90, 0x4000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xfffd);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 414;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 414)
		{
			expect_opcode(0x2282, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff0000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"MPYR rounds negative product after adding 0x8000 and preserves carry");
			program.write_word(0x05e3, 0x2a82); // MACR *AR2,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x4000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 415;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 415)
		{
			expect_opcode(0x2a82, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"MACR rounds after accumulation, not before, in one cycle");
			program.write_word(0x05e3, 0x2e82); // MASR *AR2,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 416;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 416)
		{
			expect_opcode(0x2e82, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff0000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"MASR rounds negative accumulator-minus-product in one cycle");
			program.write_word(0x05e3, 0x2482); // MPYU *AR2,A
			data.write_word(0x0f90, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0040); // FRCT
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 417;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 417)
		{
			expect_opcode(0x2482, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1fffc0002ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0xffff &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"MPYU treats both operands as unsigned and doubles product under FRCT");
			program.write_word(0x05e3, 0x4482); // LD *AR2,16,A
			data.write_word(0x0f90, 0x8000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0400); // Sticky OVA
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM, no SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 418;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 418)
		{
			expect_opcode(0x4482, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x80000000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0600) == 0x0400 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"fixed-16 LD zero-extends without SXM and ignores OVM without changing overflow");
			program.write_word(0x05e3, 0x4815); // LDM AR5,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x8001);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0400);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 419;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 419)
		{
			expect_opcode(0x4815, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0400 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LDM AR5,A zero-extends under SXM and costs one cycle");
			program.write_word(0x05e3, 0x4915); // LDM AR5,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 420;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 420)
		{
			expect_opcode(0x4915, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0400 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LDM AR5,B zero-extends under SXM without changing A or status");
			program.write_word(0x05e3, 0x4882); // LDM *AR2,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0xff15);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x8001);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 421;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 421)
		{
			expect_opcode(0x4882, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0015 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"indirect LDM masks AR2 to the MMR page and clears its high bits");
			program.write_word(0x05e3, 0x8892); // STLM A,*AR2+
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x3456);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0xff17);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 422;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 422)
		{
			expect_opcode(0x8892, m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x3456 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0018 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"indirect STLM writes the pointed MMR and postincrements masked AR2");
			program.write_word(0x05e3, 0x489a); // LDM *+AR2,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0xff14);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x8001);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 423;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 423)
		{
			expect_opcode(0x489a, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0015 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"indirect LDM preincrements before selecting the seven-bit MMR address");
			program.write_word(0x05e3, 0xff4d); // XC 2,BEQ
			program.write_word(0x05e4, 0xe801); // LD #1,A
			program.write_word(0x05e5, 0xe902); // LD #2,B
			program.write_word(0x05e6, 0x75f8);
			program.write_word(0x05e7, 0x0d00);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 424;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 424)
		{
			expect_opcode(0xff4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"ROM4 XC 2,BEQ executes two words after a one-cycle condition test");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 425;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 425)
		{
			expect_opcode(0xff4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 1 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"rejected XC 2,BEQ executes two NOP slots at their cycle cost");
			program.write_word(0x0048, 0x0083); // ISR samples A through *AR3.
			program.write_word(0x0049, 0xf4eb); // RETE
			program.write_word(0x05e3, 0xff4d); // XC 2,BEQ
			program.write_word(0x05e4, 0x00f8); // ADD *(absolute),A
			program.write_word(0x05e5, 0x0060); // Read raises INT2.
			m_port_writes = 0;
			m_port_writes_at_irq = 0;
			m_repeat_reads = 0;
			m_irq_trigger_read = 1;
			m_irq_accumulator = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0061);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x02ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 426;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 426)
		{
			expect_opcode(0xff4d, m_repeat_reads == 1 && m_irq_accumulator == 1 &&
					m_port_writes_at_irq == 1 && m_port_writes == 2 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 1,
					"XC 2 releases a pending interrupt after one two-word instruction");
			m_cpu->set_input_line(2, CLEAR_LINE);
			program.write_word(0x05e3, 0x1183); // LD *AR3,B
			program.write_word(0x05e4, 0x75f8);
			program.write_word(0x05e5, 0x0d00);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0fa0, 0xfffe);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0fa0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 427;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 427)
		{
			expect_opcode(0x1183, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffffffeULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0fa0 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR3,B sign-extends under SXM without modifying AR3");
			program.write_word(0x05e3, 0x0093); // ADD *AR3+,A
			data.write_word(0x0fa1, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0fa1);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 428;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 428)
		{
			expect_opcode(0x0093, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0fa2 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ADD *AR3+,A saturates under OVM and sets sticky overflow in one cycle");
			program.write_word(0x05e3, 0xf1a0); // OR A,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0c00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 429;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 429)
		{
			expect_opcode(0xf1a0, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff00000000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0c00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 OR A,B combines all 40 bits without changing source or status");
			program.write_word(0x05e3, 0xf2c0); // XOR B,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff000000f0ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff0000000fULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 430;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 430)
		{
			expect_opcode(0xf2c0, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff0000000fULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0c00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 XOR B,A combines all 40 bits without changing source or status");
			program.write_word(0x05e3, 0xf620); // SUB B,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 431;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 431)
		{
			expect_opcode(0xf620, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffffffULL &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SUB B,A saturates signed-32 overflow without changing B");
			program.write_word(0x05e3, 0x818a); // STL B,*AR2-
			data.write_word(0x0f10, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f10);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 432;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 432)
		{
			expect_opcode(0x818a, data.read_word(0x0f10) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f0f &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STL B,*AR2- stores before decrement without changing carry");
			program.write_word(0x05e3, 0x81d3); // STL B,*AR3+%
			data.write_word(0x0ffb, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xabcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0ffb);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 433;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 433)
		{
			expect_opcode(0x81d3, data.read_word(0x0ffb) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0ff8 &&
					m_cpu->state_int(tms320c54x_device::STATE_BK) == 4 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STL B,*AR3+% wraps a four-word circular buffer after store");
			program.write_word(0x05e3, 0x8282); // STH A,*AR2
			data.write_word(0x0f90, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7ffff0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0001); // SST
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 434;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 434)
		{
			expect_opcode(0x8282, data.read_word(0x0f90) == 0x7fff &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7ffff0000ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"PMST.SST clamps signed STH without changing accumulator or cycle cost");
			program.write_word(0x05e3, 0x4f82); // DST B,*AR2
			data.write_word(0x0f90, 0);
			data.write_word(0x0f91, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x8ffff0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 435;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 435)
		{
			expect_opcode(0x4f82, data.read_word(0x0f90) == 0xffff &&
					data.read_word(0x0f91) == 0xffff &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8ffff0000ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"PMST.SST clamps unsigned DST without changing accumulator or two-cycle cost");
			program.write_word(0x05e3, 0xd6e1); // ST B,*AR3 || MACR *AR4+0%,A
			data.write_word(0x0f90, 0);
			data.write_word(0x0fa0, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x7ffff0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0fa0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, ASM=0
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 436;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 436)
		{
			expect_opcode(0xd6e1, data.read_word(0x0fa0) == 0x7fff &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7ffff0000ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"PMST.SST clamps parallel ST old B while MACR keeps one-cycle cost");
			data.write_word(0x0fa0, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x3ffff0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0fa0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // SXM, ASM=1
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 437;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 437)
		{
			expect_opcode(0xd6e1, data.read_word(0x0fa0) == 0x7fff &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x3ffff0000ULL &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"parallel ST shifts by ASM before SST clamps the stored value");
			program.write_word(0x05e3, 0x1093); // LD *AR3+,A
			data.write_word(0x0fa0, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0fa0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 438;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 438)
		{
			expect_opcode(0x1093, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0fa1 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR3+,A sign-extends and postincrements in one cycle");
			program.write_word(0x05e3, 0x1084); // LD *AR4,A
			data.write_word(0x0f90, 0x8002);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 439;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 439)
		{
			expect_opcode(0x1084, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8002 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f90 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR4,A zero-extends without SXM and leaves pointer unchanged");
			program.write_word(0x05e3, 0x1094); // LD *AR4+,A
			data.write_word(0x0f90, 0xfffd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 440;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 440)
		{
			expect_opcode(0x1094, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffffdULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f91 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD *AR4+,A sign-extends and postincrements in one cycle");
			program.write_word(0x05e3, 0x4912); // LDM AR2,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x9abc);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 441;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 441)
		{
			expect_opcode(0x4912, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x9abc &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x9abc &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LDM AR2,B zero-extends despite SXM in one cycle");
			program.write_word(0x05e3, 0x730b); // MVMD BL,dmad
			program.write_word(0x05e4, 0x0fa0);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0fa0, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 442;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 442)
		{
			expect_opcode(0x730b, data.read_word(0x0fa0) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 MVMD BL,dmad copies without modifying BL in two cycles");
			program.write_word(0x05e3, 0x6882); // ANDM #0f0f,*AR2
			program.write_word(0x05e4, 0x0f0f);
			data.write_word(0x0f90, 0xff00);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 443;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 443)
		{
			expect_opcode(0x6882, data.read_word(0x0f90) == 0x0f00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 ANDM #lk,*AR2 preserves carry and costs two cycles");
			program.write_word(0x05e3, 0x6884); // ANDM #ff0f,*AR4
			program.write_word(0x05e4, 0xff0f);
			data.write_word(0x0f92, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f92);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 444;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 444)
		{
			expect_opcode(0x6884, data.read_word(0x0f92) == 0x1204 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 ANDM #lk,*AR4 selects its own pointer in two cycles");
			program.write_word(0x05e3, 0x6db1); // MAR *AR1+0
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f10);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // CMPT clear
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 445;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 445)
		{
			expect_opcode(0x6db1, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f13 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MAR *AR1+0 adds AR0 without changing ARP in one cycle");
			program.write_word(0x05e3, 0x6dc2); // MAR *AR2-%
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f08);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 446;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 446)
		{
			expect_opcode(0x6dc2, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f0b &&
					m_cpu->state_int(tms320c54x_device::STATE_BK) == 4 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MAR *AR2-% wraps at BK without changing status in one cycle");
			program.write_word(0x05e3, 0xe900); // LD #0,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 447;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 447)
		{
			expect_opcode(0xe900, m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD #0,B clears guard and payload in one cycle");
			program.write_word(0x05e3, 0xe901); // LD #1,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 448;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 448)
		{
			expect_opcode(0xe901, m_cpu->state_int(tms320c54x_device::STATE_B) == 1 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD #1,B clears prior guard bits in one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xec1f); // RPT #31
			program.write_word(0x05e3, 0x7693); // ST #lk,*AR3+
			program.write_word(0x05e4, 0x5a3c);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			for (unsigned i = 0; i != 32; ++i)
				data.write_word(0x0d00 + i, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 449;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 449)
		{
			bool all_stored = true;
			for (unsigned i = 0; i != 32; ++i)
				all_stored &= data.read_word(0x0d00 + i) == 0x5a3c;
			expect_opcode(0xec1f, all_stored &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 67,
					"ROM4 RPT #31 repeats a two-cycle ST body 32 times");
			expect_opcode(0x7693, all_stored &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d20,
					"ROM4 ST #lk,*AR3+ writes consecutive ordinary-memory words");
			program.write_word(0x05e2, 0xf495); // NOP between marker and target
			program.write_word(0x05e3, 0xf032); // AND #lk,2,A
			program.write_word(0x05e4, 0x0f0f);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 450;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 450)
		{
			expect_opcode(0xf032, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1438 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 AND #lk,2,A masks the shifted immediate without changing carry");
			program.write_word(0x05e3, 0xf035); // AND #lk,5,A
			program.write_word(0x05e4, 0x00ff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 451;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 451)
		{
			expect_opcode(0xf035, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1660 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 AND #lk,5,A uses five-bit immediate alignment in two cycles");
			program.write_word(0x05e3, 0xf0fb); // SFTL A,-5,A
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000013ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 452;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 452)
		{
			expect_opcode(0xf0fb, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x04000000 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SFTL A,-5 clears guard and copies outgoing bit four to carry");
			program.write_word(0x05e3, 0xf0fe); // SFTL A,-2,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000003ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 453;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 453)
		{
			expect_opcode(0xf0fe, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x20000000 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 SFTL A,-2 logically shifts bit 31 and publishes bit one");
			program.write_word(0x05e2, 0xff20); // XC 2,NTC
			program.write_word(0x05e3, 0x6d91); // MAR *AR1+
			program.write_word(0x05e4, 0x6d92); // MAR *AR2+
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 454;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 454)
		{
			expect_opcode(0x6d92, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0131,
					"ROM4 MAR *AR2+ advances the second auxiliary register");
			expect_opcode(0xff20, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0121 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0131 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 XC 2,NTC executes two one-word MAR slots when TC is clear");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 455;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 455)
		{
			expect_opcode(0xff20, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0120 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0130 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"rejected ROM4 XC 2,NTC charges two NOP slots without modifying ARs");
			program.write_word(0x05e2, 0xe741); // MVMM AR4,AR1
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0xcdef);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 456;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 456)
		{
			expect_opcode(0xe741, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0xcdef &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xcdef &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 MVMM AR4,AR1 copies only the destination in one cycle");
			program.write_word(0x05e2, 0x1181); // LD *AR1,B
			data.write_word(0x0f20, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 457;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 457)
		{
			expect_opcode(0x1181, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD *AR1,B sign-extends under SXM in one cycle");
			program.write_word(0x05e2, 0x1a8a); // OR *AR2-,A
			data.write_word(0x0f20, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x120000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 458;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 458)
		{
			expect_opcode(0x1a8a, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1200f0 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f1f &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR *AR2-,A uses the old address and decrements in one cycle");
			program.write_word(0x05e2, 0x12d2); // LDU *AR2+%,A
			data.write_word(0x0f23, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must not sign-extend LDU.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 459;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 459)
		{
			expect_opcode(0x12d2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LDU *AR2+%,A zero-extends and wraps after the read in one cycle");
			program.write_word(0x05e2, 0x1cf8); // XOR *(lk),A
			program.write_word(0x05e3, 0x0f20);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f20, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12000f);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 460;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 460)
		{
			expect_opcode(0x1cf8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1200ff &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute XOR uses its extension, preserves status, and costs two cycles");
			program.write_word(0x05e2, 0x6982); // ORM #lk,*AR2
			program.write_word(0x05e3, 0x00a0);
			data.write_word(0x0f20, 0x1001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 461;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 461)
		{
			expect_opcode(0x6982, data.read_word(0x0f20) == 0x10a1 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ORM #lk,*AR2 preserves pointer and status in two cycles");
			program.write_word(0x05e2, 0x6984); // ORM #lk,*AR4
			program.write_word(0x05e3, 0x0005);
			data.write_word(0x0f24, 0x8000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f24);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 462;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 462)
		{
			expect_opcode(0x6984, data.read_word(0x0f24) == 0x8005 &&
					data.read_word(0x0f20) == 0x10a1 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f24 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ORM #lk,*AR4 selects a distinct data address without pointer motion");
			program.write_word(0x05e2, 0x1b8d); // OR *AR5-,B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f25, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x34000f);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f25);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 463;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 463)
		{
			expect_opcode(0x1b8d, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x3400ff &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f24 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR *AR5-,B preserves status and decrements after one cycle");
			program.write_word(0x05e2, 0x4595); // LD *AR5+,16,B
			data.write_word(0x0f30, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f30);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 464;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 464)
		{
			expect_opcode(0x4595, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f31 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD *AR5+,16,B sign-extends and postincrements in one cycle");
			program.write_word(0x05e2, 0x4917); // LDM AR7,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x8001);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 465;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 465)
		{
			expect_opcode(0x4917, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x8001 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LDM AR7,B ignores SXM and preserves AR7 in one cycle");
			program.write_word(0x05e2, 0x6883); // ANDM #lk,*AR3
			program.write_word(0x05e3, 0x0ff0);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f33, 0xff0f);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f33);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 466;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 466)
		{
			expect_opcode(0x6883, data.read_word(0x0f33) == 0x0f00 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f33 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ANDM #lk,*AR3 preserves pointer and status in two cycles");
			program.write_word(0x05e2, 0x6983); // ORM #lk,*AR3
			program.write_word(0x05e3, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 467;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 467)
		{
			expect_opcode(0x6983, data.read_word(0x0f33) == 0x0ff0 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f33 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ORM #lk,*AR3 preserves pointer and status in two cycles");
			program.write_word(0x05e2, 0x6fd2); // LD *AR2+%,5,A
			program.write_word(0x05e3, 0x0c45);
			data.write_word(0x0f23, 0xff80);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 468;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 468)
		{
			expect_opcode(0x6fd2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffff000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 shifted LD sign-extends before shift and wraps AR2 in two cycles");
			program.write_word(0x05e2, 0x7e8b); // READA *AR3-
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			program.write_word(0x0d40, 0xabcd);
			data.write_word(0x0f33, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0d40);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f33);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 469;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 469)
		{
			expect_opcode(0x7e8b, data.read_word(0x0f33) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0d40 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f32 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 READA uses A as program source and decrements AR3 after five cycles");
			program.write_word(0x05e2, 0xf1c0); // XOR A,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff0000000fULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x00abcd0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 470;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 470)
		{
			expect_opcode(0xf1c0, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffabcd000fULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff0000000fULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 XOR A,B preserves the 40-bit guard and status in one cycle");
			program.write_word(0x05e2, 0xf508); // ADD A<<8,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 471;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 471)
		{
			expect_opcode(0xf508, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x133400 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 ADD A<<8,B updates B only in one cycle");
			program.write_word(0x05e2, 0xf400); // ADD A,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 472;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 472)
		{
			expect_opcode(0xf400, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffeULL &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 ADD A,A sets overflow across the 32-bit boundary in one cycle");
			program.write_word(0x05e2, 0xf028); // LD #00aa,8,A
			program.write_word(0x05e3, 0x00aa);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 473;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 473)
		{
			expect_opcode(0xf028, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xaa00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LD #00aa,8,A consumes its immediate in two cycles");
			program.write_word(0x05e2, 0xf1b8); // OR A>>8,B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 474;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 474)
		{
			expect_opcode(0xf1b8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00ff801234ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR A>>8,B uses a logical right shift without changing status");
			program.write_word(0x05e2, 0xf468); // SFTA A,8,A
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x800000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, OVM clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 475;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 475)
		{
			expect_opcode(0xf468, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x80000000 &&
					(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SFTA A,8,A raises OVA on a signed 32-bit overflow in one cycle");
			program.write_word(0x05e2, 0x3182); // MPYA *AR2
			data.write_word(0x0f20, 7);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffd0000ULL); // A(32-16)=-3.
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // FRCT clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 476;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 476)
		{
			expect_opcode(0x3182, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffffebULL &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffd0000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 7 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 MPYA *AR2 writes signed product to B and operand to T in one cycle");
			program.write_word(0x05e2, 0x0885); // SUB *AR5,A
			data.write_word(0x0f25, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f25);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 477;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 477)
		{
			expect_opcode(0x0885, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x17fff &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f25 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SUB *AR5,A sign-extends the operand without pointer motion in one cycle");
			program.write_word(0x05e2, 0x1df8); // XOR *(lk),B
			program.write_word(0x05e3, 0x0f21);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f21, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff0000000fULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 478;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 478)
		{
			expect_opcode(0x1df8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute XOR into B preserves guard and status in two cycles");
			program.write_word(0x05e2, 0x80d3); // STL A,*AR3+%
			program.write_word(0x05e3, 0x818d); // STL B,*AR5-
			program.write_word(0x05e4, 0x81d2); // STL B,*AR2+%
			program.write_word(0x05e5, 0x8395); // STH B,*AR5+
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0f22, 0);
			data.write_word(0x0f23, 0);
			data.write_word(0x0f24, 0);
			data.write_word(0x0f25, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x89ababcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f22);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f25);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); // SST clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 479;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 479)
		{
			expect_opcode(0x80d3, data.read_word(0x0f23) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f20,
					"ROM4 STL A,*AR3+% stores before circular wrap");
			expect_opcode(0x818d, data.read_word(0x0f25) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f25,
					"ROM4 STL B,*AR5- stores at the old address");
			expect_opcode(0x81d2, data.read_word(0x0f22) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f23,
					"ROM4 STL B,*AR2+% stores before circular advance");
			expect_opcode(0x8395, data.read_word(0x0f24) == 0x89ab &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x89ababcd &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
					"four ROM4 accumulator stores preserve status and cost one cycle each");
			program.write_word(0x05e2, 0x8815); // STLM A,AR5
			program.write_word(0x05e3, 0xe552); // MVDD *AR3-,*AR4
			program.write_word(0x05e4, 0xe754); // MVMM AR5,AR4
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f23, 0xcafe);
			data.write_word(0x0f26, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12340f25);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f26);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 480;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 480)
		{
			expect_opcode(0x8815,
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f25 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12340f25,
					"ROM4 STLM A,AR5 writes AL without changing A");
			expect_opcode(0xe552, data.read_word(0x0f26) == 0xcafe &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22,
					"ROM4 MVDD copies the old AR3 word then decrements AR3");
			expect_opcode(0xe754,
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f25 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 MVMM copies AR5 to AR4; three moves cost one cycle each");
			program.write_word(0x05e2, 0xf843); // BC 05f0,ALT
			program.write_word(0x05e3, 0x05f0);
			program.write_word(0x05e4, 0x75d6); // Rejected-branch marker.
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			program.write_word(0x05f0, 0x75d6); // Taken-branch marker.
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 481;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 481 || m_phase == 482)
		{
			expect_opcode(0xf843,
					m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (m_phase == 481 ? 7 : 5) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) ==
						(m_phase == 481 ? 0x05f3 : 0x05e7),
					"ROM4 BC ALT tests A's guard sign and costs five/three cycles");
			if (m_phase == 481)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0100000000ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 482;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xff30); // XC 2,TC
			program.write_word(0x05e3, 0x6d91); // MAR *AR1+
			program.write_word(0x05e4, 0x6d92); // MAR *AR2+
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 483;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 483 || m_phase == 484)
		{
			expect_opcode(0xff30,
					m_cpu->state_int(tms320c54x_device::STATE_AR1) ==
						(m_phase == 483 ? 0x0121 : 0x0120) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
						(m_phase == 483 ? 0x0131 : 0x0130) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 XC 2,TC executes or rejects two slots at five-cycle marker span");
			if (m_phase == 483)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 484;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x1192); // LD *AR2+,B
			program.write_word(0x05e3, 0x6dd2); // MAR *AR2+%
			program.write_word(0x05e4, 0x7683); // ST #lk,*AR3
			program.write_word(0x05e5, 0x5aa5);
			program.write_word(0x05e6, 0x8085); // STL A,*AR5
			program.write_word(0x05e7, 0x75d6);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			data.write_word(0x0f22, 0x8000);
			data.write_word(0x0f30, 0);
			data.write_word(0x0f31, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234abcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f22);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f30);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f31);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 485;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 485)
		{
			expect_opcode(0x1192,
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffff8000ULL,
					"ROM4 LD *AR2+,B sign-extends under SXM");
			expect_opcode(0x6dd2,
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20,
					"ROM4 MAR *AR2+% wraps after the preceding linear load");
			expect_opcode(0x7683, data.read_word(0x0f30) == 0x5aa5 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f30,
					"ROM4 ST immediate writes the direct AR3 destination");
			expect_opcode(0x8085, data.read_word(0x0f31) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f31 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234abcd &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
					"ROM4 load/MAR/immediate-store/accumulator-store cost 1+1+2+1 cycles");
			program.write_word(0x05e2, 0x771d); // STM #lk,PMST
			program.write_word(0x05e3, 0x4567);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f13);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 486;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 486)
		{
			expect_opcode(0x771d,
					m_cpu->state_int(tms320c54x_device::STATE_PMST) == 0x4567 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f13 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 STM writes direct PMST without pointer motion in two cycles");
			program.write_word(0x05e2, 0xff45); // XC 2,AEQ
			program.write_word(0x05e3, 0x6d91); // MAR *AR1+
			program.write_word(0x05e4, 0x6d92); // MAR *AR2+
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 487;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 487 && m_phase <= 490)
		{
			const bool accepted = m_phase == 487 || m_phase == 489;
			expect_opcode(m_phase < 489 ? 0xff45 : 0xff4e,
					m_cpu->state_int(tms320c54x_device::STATE_AR1) ==
						(accepted ? 0x0121 : 0x0120) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
						(accepted ? 0x0131 : 0x0130) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 XC AEQ/BGT executes or rejects two guarded MAR slots");
			if (m_phase == 490)
			{
				program.write_word(0x05e2, 0x4812); // LDM AR2,A
				program.write_word(0x05e3, 0x8095); // STL A,*AR5+
				program.write_word(0x05e4, 0x4813); // LDM AR3,A
				program.write_word(0x05e5, 0x8095);
				program.write_word(0x05e6, 0x4814); // LDM AR4,A
				program.write_word(0x05e7, 0x8095);
				program.write_word(0x05e8, 0x4817); // LDM AR7,A
				program.write_word(0x05e9, 0x8095);
				program.write_word(0x05ea, 0x4910); // LDM AR0,B
				program.write_word(0x05eb, 0x75d6);
				program.write_word(0x05ec, 0x0124);
				program.write_word(0x05ed, 0xf5e1);
				for (u16 address = 0x0f40; address != 0x0f44; ++address)
					data.write_word(address, 0);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x8000);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x8002);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x8003);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x8004);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f40);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x8007);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 491;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			if (m_phase == 488)
				program.write_word(0x05e2, 0xff4e); // XC 2,BGT
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A,
					m_phase == 487 ? 1 : 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B,
					m_phase == 488 ? 0x0100000000ULL : 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 491)
		{
			expect_opcode(0x4812, data.read_word(0x0f40) == 0x8002,
					"ROM4 LDM AR2,A preserves the zero-extended load via STL");
			expect_opcode(0x4813, data.read_word(0x0f41) == 0x8003,
					"ROM4 LDM AR3,A preserves the zero-extended load via STL");
			expect_opcode(0x4814, data.read_word(0x0f42) == 0x8004,
					"ROM4 LDM AR4,A preserves the zero-extended load via STL");
			expect_opcode(0x4817, data.read_word(0x0f43) == 0x8007 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8007,
					"ROM4 LDM AR7,A zero-extends despite SXM");
			expect_opcode(0x4910,
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8000 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f44 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 11,
					"five ROM4 LDM words and four STL stores cost one cycle each");
			program.write_word(0x05e2, 0xf200); // ADD #lk,B,A
			program.write_word(0x05e3, 0xfffe);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 492;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 492)
		{
			expect_opcode(0xf200,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00fe &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0100 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 ADD #lk,B,A sign-extends under SXM and costs two cycles");
			program.write_word(0x05e2, 0xf2a0); // OR B,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xf0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff0000000fULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 493;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 493)
		{
			expect_opcode(0xf2a0,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff000000ffULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff0000000fULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR B,A combines all 40 bits without modifying B or status");
			program.write_word(0x05e2, 0xf540); // LD A,0,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff000000f0ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 494;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 494)
		{
			expect_opcode(0xf540,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff000000f0ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000f0ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0a00 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD A,0,B preserves all 40 bits and sets OVB in one cycle");
			program.write_word(0x05e2, 0x10da); // LD *AR2+0%,A
			program.write_word(0x05e3, 0x6d93); // MAR *AR3+
			program.write_word(0x05e4, 0x7308); // MVMD AL,dmad
			program.write_word(0x05e5, 0x0f50);
			program.write_word(0x05e6, 0x7313); // MVMD AR3,dmad
			program.write_word(0x05e7, 0x0f51);
			program.write_word(0x05e8, 0x7681); // ST #lk,*AR1
			program.write_word(0x05e9, 0x5a5a);
			program.write_word(0x05ea, 0x75d6);
			program.write_word(0x05eb, 0x0124);
			program.write_word(0x05ec, 0xf5e1);
			data.write_word(0x0f22, 0x8001);
			data.write_word(0x0f50, 0);
			data.write_word(0x0f51, 0);
			data.write_word(0x0f52, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f52);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f22);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f30);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 495;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 495)
		{
			expect_opcode(0x10da,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20,
					"ROM4 LD *AR2+0%,A sign-extends before circular advance");
			expect_opcode(0x6d93,
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f31,
					"ROM4 MAR *AR3+ updates the register in one cycle");
			expect_opcode(0x7308, data.read_word(0x0f50) == 0x8001,
					"ROM4 MVMD AL,dmad captures A's low word");
			expect_opcode(0x7313, data.read_word(0x0f51) == 0x0f31,
					"ROM4 MVMD AR3,dmad captures the modified register");
			expect_opcode(0x7681, data.read_word(0x0f52) == 0x5a5a &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f52 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
					"ROM4 load/MAR/two MVMDs/ST cost 1+1+2+2+2 cycles");
			program.write_word(0x05e2, 0x7718); // STM #lk,SP
			program.write_word(0x05e3, 0x0320);
			program.write_word(0x05e4, 0x7728); // STM #lk,028h
			program.write_word(0x05e5, 0x5aa5);
			program.write_word(0x05e6, 0x7758); // STM #lk,CLKMD
			program.write_word(0x05e7, 0x0002);
			program.write_word(0x05e8, 0x4858); // LDM CLKMD,A
			program.write_word(0x05e9, 0x75d6);
			program.write_word(0x05ea, 0x0124);
			program.write_word(0x05eb, 0xf5e1);
			data.write_word(0x0028, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 496;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 496)
		{
			expect_opcode(0x7718,
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0320,
					"ROM4 STM direct SP writes the stack pointer");
			expect_opcode(0x7728, data.read_word(0x0028) == 0x5aa5,
					"ROM4 STM direct page-zero address writes backing data");
			expect_opcode(0x7758,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0003 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
					"ROM4 CLKMD write reads back modeled status; three STMs cost two cycles each");
			program.write_word(0x05e2, 0x8812); // STLM A,AR2
			program.write_word(0x05e3, 0x881a); // STLM A,BRC
			program.write_word(0x05e4, 0x731a); // MVMD BRC,dmad
			program.write_word(0x05e5, 0x0f50);
			program.write_word(0x05e6, 0x8915); // STLM B,AR5
			program.write_word(0x05e7, 0x891a); // STLM B,BRC
			program.write_word(0x05e8, 0x75d6);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0f50, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 497;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 497)
		{
			expect_opcode(0x8812,
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1234,
					"ROM4 STLM A,AR2 writes AL");
			expect_opcode(0x881a, data.read_word(0x0f50) == 0x1234,
					"ROM4 STLM A,BRC is captured before B overwrites BRC");
			expect_opcode(0x8915,
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x5678,
					"ROM4 STLM B,AR5 writes BL");
			expect_opcode(0x891a,
					m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"four ROM4 STLM stores and one MVMD cost 1+1+2+1+1 cycles");
			program.write_word(0x05e2, 0xe589); // MVDD *AR2+,*AR3+
			program.write_word(0x05e3, 0xe734); // MVMM AR3,AR4
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f60, 0xbeef);
			data.write_word(0x0f70, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f60);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f70);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 498;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 498)
		{
			expect_opcode(0xe589,
					data.read_word(0x0f70) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f61 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f71,
					"ROM4 MVDD reads/writes old addresses before both ARs advance");
			expect_opcode(0xe734,
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f71 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 MVMM copies the advanced AR3 in one cycle");
			program.write_word(0x05e2, 0xec06); // RPT #6
			program.write_word(0x05e3, 0x6d91); // MAR *AR1+
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 499;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 499)
		{
			expect_opcode(0xec06,
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0127 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
					"ROM4 RPT #6 executes MAR seven times and costs one setup cycle");
			program.write_word(0x05e2, 0xf84a); // BC 05f0,BGEQ
			program.write_word(0x05e3, 0x05f0);
			program.write_word(0x05e4, 0x75d6); // Rejected-branch marker.
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			program.write_word(0x05f0, 0x75d6); // Taken-branch marker.
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 500;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 500 && m_phase <= 503)
		{
			const bool taken = m_bleq_case ? m_bleq_case < 3 : m_phase == 500 || m_phase == 502;
			const u16 opcode = m_bleq_case ? 0xf84f : m_phase <= 501 ? 0xf84a : 0xf84b;
			expect_opcode(opcode,
					m_port_writes == 2 &&
					m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05f3 : 0x05e7),
					"ROM4 BC tests B's 40-bit sign and costs five/three cycles");
			if (m_phase < 503)
			{
				if (m_phase == 501)
					program.write_word(0x05e2, 0xf84b); // BC 05f0,BLT
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B,
						m_phase == 500 || m_phase == 501 ? 0xff00000000ULL : 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			if (m_bleq_case < 3)
			{
				static constexpr u64 values[] = {0, 0xff00000000ULL, 1};
				program.write_word(0x05e2, 0xf84f); // BC 05f0,BLEQ
				m_cpu->set_state_int(tms320c54x_device::STATE_B, values[m_bleq_case++]);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_port_writes = 0;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xfc47); // RC ALEQ
			program.write_word(0x05e3, 0x75d6); // Rejected-return marker.
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			program.write_word(0x05f0, 0x75d6); // Accepted-return marker.
			program.write_word(0x05f1, 0x0124);
			program.write_word(0x05f2, 0xf5e1);
			data.write_word(0x0300, 0x05f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 504;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 504 || m_phase == 505)
		{
			const bool taken = m_phase == 505;
			expect_opcode(0xfc47,
					m_cpu->state_int(tms320c54x_device::STATE_SP) == (taken ? 0x0301 : 0x0300) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05f3 : 0x05e6) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5),
					"ROM4 RC ALEQ checks 40-bit guard, stack pop, and five/three cycles");
			if (!taken)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
				m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 505;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xff47); // XC 2,ALEQ
			program.write_word(0x05e3, 0x6d91); // MAR *AR1+
			program.write_word(0x05e4, 0x6d92); // MAR *AR2+
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 506;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 506 && m_phase <= 509)
		{
			const bool accepted = m_phase == 506 || m_phase == 508;
			const u16 opcode = m_phase <= 507 ? 0xff47 : 0xff4c;
			expect_opcode(opcode,
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == (accepted ? 0x0121 : 0x0120) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == (accepted ? 0x0131 : 0x0130) &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
					"ROM4 XC ALEQ/BNEQ executes or rejects two guarded MAR slots");
			if (m_phase < 509)
			{
				if (m_phase == 507)
					program.write_word(0x05e2, 0xff4c); // XC 2,BNEQ
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A,
						m_phase == 506 ? 1 : 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_B,
						m_phase == 507 ? 0xff00000000ULL : 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0130);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x0882); // SUB *AR2,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f22, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f22);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 510;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 510)
		{
			expect_opcode(0x0882,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x17fff &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f22 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SUB *AR2,A sign-extends without pointer motion in one cycle");
			program.write_word(0x05e2, 0x1b84); // OR *AR4,B
			data.write_word(0x0f24, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff0000000fULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f24);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 511;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 511)
		{
			expect_opcode(0x1b84,
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f24 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 OR *AR4,B preserves guard, status, and pointer in one cycle");
			program.write_word(0x05e2, 0x1bf8); // OR *(lk),B
			program.write_word(0x05e3, 0x0f24);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff0000000fULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 512;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 512)
		{
			expect_opcode(0x1bf8,
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute OR into B consumes address and costs two cycles");
			program.write_word(0x05e2, 0x45f8); // LD *(lk),16,B
			data.write_word(0x0f24, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 513;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 513)
		{
			expect_opcode(0x45f8,
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80010000ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 absolute LD *,16,B sign-extends before shift in two cycles");
			program.write_word(0x05e0, 0x7586); // PORTW *AR6,port: marker must not mutate AR6.
			program.write_word(0x05e2, 0x4816); // LDM AR6,A
			program.write_word(0x05e3, 0x4913); // LDM AR3,B
			program.write_word(0x05e4, 0x7586);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x8006);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x8003);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 514;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 514)
		{
			expect_opcode(0x4816,
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8006 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x8006,
					"ROM4 LDM AR6,A zero-extends without changing AR6");
			expect_opcode(0x4913,
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8003 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x8003 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 LDM AR3,B zero-extends despite SXM; both loads cost one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e2, 0x7215); // MVDM dmad,AR5
			program.write_word(0x05e3, 0x0f60);
			program.write_word(0x05e4, 0x7314); // MVMD AR4,dmad
			program.write_word(0x05e5, 0x0f62);
			program.write_word(0x05e6, 0x7315); // MVMD AR5,dmad
			program.write_word(0x05e7, 0x0f63);
			program.write_word(0x05e8, 0x75d6);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0f60, 0x4321);
			data.write_word(0x0f62, 0);
			data.write_word(0x0f63, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0xabcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 515;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 515)
		{
			expect_opcode(0x7215,
					m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x4321 &&
					data.read_word(0x0f60) == 0x4321,
					"ROM4 MVDM copies data into AR5 without altering the source");
			expect_opcode(0x7314,
					data.read_word(0x0f62) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xabcd,
					"ROM4 MVMD copies AR4 into data memory");
			expect_opcode(0x7315,
					data.read_word(0x0f63) == 0x4321 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"ROM4 MVMD copies the new AR5; three MMR moves cost two cycles each");
			program.write_word(0x05e0, 0x7586); // Preserve AR6 across both timing markers.
			program.write_word(0x05e2, 0x8816); // STLM A,AR6
			program.write_word(0x05e3, 0x8817); // STLM A,AR7
			program.write_word(0x05e4, 0x8821); // STLM A,MMR 0x21
			program.write_word(0x05e5, 0x8910); // STLM B,AR0
			program.write_word(0x05e6, 0x8911); // STLM B,AR1
			program.write_word(0x05e7, 0x4920); // LDM MMR 0x20,B
			program.write_word(0x05e8, 0x7586);
			program.write_word(0x05e9, 0x0124);
			program.write_word(0x05ea, 0xf5e1);
			data.write_word(0x0020, 0x9abc);
			data.write_word(0x0021, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 516;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 516)
		{
			expect_opcode(0x8816,
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1234,
					"ROM4 STLM A,AR6 publishes A's low word");
			expect_opcode(0x8817,
					m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1234,
					"ROM4 STLM A,AR7 publishes A's low word");
			expect_opcode(0x8821, data.read_word(0x0021) == 0x1234,
					"ROM4 STLM A,MMR 0x21 writes the peripheral-facing data space");
			expect_opcode(0x8910,
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x5678,
					"ROM4 STLM B,AR0 publishes B's low word");
			expect_opcode(0x8911,
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x5678,
					"ROM4 STLM B,AR1 publishes B's low word");
			expect_opcode(0x4920,
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x9abc &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"ROM4 LDM MMR 0x20,B zero-extends; six MMR transfers cost one cycle each");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e2, 0x71da); // MVDK *AR2+0%,dmad
			program.write_word(0x05e3, 0x0f40);
			program.write_word(0x05e4, 0x7692); // ST #lk,*AR2+
			program.write_word(0x05e5, 0xabcd);
			program.write_word(0x05e6, 0x7c8b); // MVPD pmad,*AR3-
			program.write_word(0x05e7, 0x0610);
			program.write_word(0x05e8, 0x7d82); // MVDP *AR2,pmad
			program.write_word(0x05e9, 0x0620);
			program.write_word(0x05ea, 0x8182); // STL B,*AR2
			program.write_word(0x05eb, 0x75d6);
			program.write_word(0x05ec, 0x0124);
			program.write_word(0x05ed, 0xf5e1);
			program.write_word(0x0610, 0xbeef);
			program.write_word(0x0620, 0);
			data.write_word(0x0f21, 0);
			data.write_word(0x0f22, 0x1234);
			data.write_word(0x0f23, 0x5a3c);
			data.write_word(0x0f40, 0);
			data.write_word(0x0f50, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f23);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f50);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xcafe);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 517;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 517)
		{
			expect_opcode(0x71da,
					data.read_word(0x0f40) == 0x5a3c &&
					data.read_word(0x0f23) == 0x5a3c,
					"ROM4 MVDK copies old AR2 word before AR0-step circular wrap");
			expect_opcode(0x7692,
					data.read_word(0x0f21) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f22,
					"ROM4 ST #lk,*AR2+ writes at the wrapped address before advancing");
			expect_opcode(0x7c8b,
					data.read_word(0x0f50) == 0xbeef &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f4f,
					"ROM4 MVPD copies program memory to old AR3 address then decrements");
			expect_opcode(0x7d82,
					program.read_word(0x0620) == 0x1234,
					"ROM4 MVDP copies old AR2 data word into program memory");
			expect_opcode(0x8182,
					data.read_word(0x0f22) == 0xcafe &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f22 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 14,
					"ROM4 STL B,*AR2 overwrites data only; transfers cost 2+2+3+4+1 cycles");
			program.write_word(0x05e0, 0x7586); // Preserve source AR6 across markers.
			program.write_word(0x05e2, 0xe762); // MVMM AR6,AR2
			program.write_word(0x05e3, 0x7586);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0xabcd);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 518;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 518)
		{
			expect_opcode(0xe762,
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0xabcd &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0xabcd &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 MVMM AR6,AR2 preserves the source and costs one cycle");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e2, 0xe80d); // LD #13,A
			program.write_word(0x05e3, 0x75d6);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 519;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 519 && m_phase <= 525)
		{
			static constexpr u16 immediate_words[] = {
				0xe80d, 0xe820, 0xe905, 0xe906, 0xe90c, 0xe90f, 0xe97c
			};
			const unsigned index = m_phase - 519;
			const u16 opcode = immediate_words[index];
			const bool destination_b = (opcode & 0xff00) == 0xe900;
			expect_opcode(opcode,
					m_cpu->state_int(destination_b ? tms320c54x_device::STATE_B :
						tms320c54x_device::STATE_A) == (opcode & 0xff) &&
					m_cpu->state_int(destination_b ? tms320c54x_device::STATE_A :
						tms320c54x_device::STATE_B) == 0 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 LD #K writes only its destination without flags in one cycle");
			if (m_phase < 525)
			{
				program.write_word(0x05e2, immediate_words[index + 1]);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xec09); // RPT #9
			program.write_word(0x05e3, 0x6d91); // MAR *AR1+
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 526;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 526 && m_phase <= 532)
		{
			static constexpr u16 repeat_words[] = {
				0xec09, 0xec0a, 0xec0f, 0xec11, 0xec13, 0xec1e, 0xec9f
			};
			const unsigned index = m_phase - 526;
			const u16 opcode = repeat_words[index];
			const unsigned count = (opcode & 0xff) + 1;
			expect_opcode(opcode,
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0120 + count &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == count + 3,
					"ROM4 RPT #K executes MAR K+1 times after one setup cycle");
			if (m_phase < 532)
			{
				program.write_word(0x05e2, repeat_words[index + 1]);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf6bd); // RSBX ST1 bit 13
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 533;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 533 && m_phase <= 536)
		{
			static constexpr u16 status_words[] = { 0xf6bd, 0xf6bf, 0xf7bd, 0xf7be };
			static constexpr u16 before[] = { 0x2000, 0x8000, 0, 0 };
			static constexpr u16 after[] = { 0, 0, 0x2000, 0x4000 };
			const unsigned index = m_phase - 533;
			expect_opcode(status_words[index],
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == after[index] &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 RSBX/SSBX changes only the selected ST1 bit in one cycle");
			if (m_phase < 536)
			{
				program.write_word(0x05e2, status_words[index + 1]);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, before[index + 1]);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xfd30); // XC 1,TC
			program.write_word(0x05e3, 0x6d91); // MAR *AR1+
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 537;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 537 && m_phase <= 542)
		{
			static constexpr u16 xc_words[] = {
				0xfd30, 0xfd30, 0xfd4b, 0xfd4b, 0xfd4d, 0xfd4d
			};
			static constexpr u16 st0_values[] = { 0x1000, 0, 0, 0, 0, 0 };
			static constexpr u64 b_values[] = {
				0, 0, 0xff00000000ULL, 0, 0, 1
			};
			const unsigned index = m_phase - 537;
			const bool accepted = (index & 1) == 0;
			expect_opcode(xc_words[index],
					m_cpu->state_int(tms320c54x_device::STATE_AR1) ==
						(accepted ? 0x0121 : 0x0120) &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"ROM4 XC 1 tests TC/BLT/BEQ and executes or rejects one MAR slot");
			if (m_phase < 542)
			{
				program.write_word(0x05e2, xc_words[index + 1]);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, st0_values[index + 1]);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, b_values[index + 1]);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xf1fc); // SFTL A,-4,B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 543;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 543 && m_phase <= 547)
		{
			struct shift_case { u16 opcode; u64 a_before; u64 b_before; u64 a_after; u64 b_after; u16 st0_before; u16 st0_after; };
			static constexpr shift_case cases[] = {
				{ 0xf1fc, 0x12345678, 0, 0x12345678, 0x01234567, 0, 0x0800 },
				{ 0xf463, 0x1001, 0, 0x8008, 0, 0x0800, 0 },
				{ 0xf470, 0xff80000000ULL, 0, 0xffffff8000ULL, 0, 0x0800, 0 },
				{ 0xf578, 0x123456ff, 0, 0x123456ff, 0x123456, 0, 0x0800 },
				{ 0xf763, 0, 0x1234, 0, 0x91a0, 0x0800, 0 }
			};
			const unsigned index = m_phase - 543;
			const shift_case &row = cases[index];
			expect_opcode(row.opcode,
					m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == row.st0_after &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"ROM4 SFTL/SFTA checks source, destination, carry, and one-cycle cost");
			if (m_phase < 547)
			{
				const shift_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, next.a_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, next.b_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, next.st0_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x2682); // SQUR *AR2,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f90, 0x8000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0240); // OVM, FRCT
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 548;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 548)
		{
			expect_opcode(0x2682,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SQUR fractional signed square saturates under OVM in one cycle");
			program.write_word(0x05e2, 0x27f8); // SQUR *(absolute),B
			program.write_word(0x05e3, 0x0f90);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f90, 0xfffe);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, no FRCT
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 549;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 549)
		{
			expect_opcode(0x27f8,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"absolute SQUR squares signed input into B in two cycles");
			program.write_word(0x05e2, 0xf48d); // SQUR A,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffe0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 550;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 550)
		{
			expect_opcode(0xf48d,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SQUR A,A squares signed AH without changing T in one cycle");
			program.write_word(0x05e2, 0xf58d); // SQUR A,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0240); // OVM, FRCT
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 551;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 551)
		{
			expect_opcode(0xf58d,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fffffff &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0200) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SQUR A,B uses original signed AH and saturates under FRCT/OVM");
			program.write_word(0x05e2, 0x3882); // SQURA *AR2,A
			data.write_word(0x0f90, 0xfffe);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 552;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 552)
		{
			expect_opcode(0x3882,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 14 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SQURA adds signed square, publishes T, and preserves carry");
			program.write_word(0x05e2, 0x3bf8); // SQURS *(absolute),B
			program.write_word(0x05e3, 0x0f90);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f90, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 5);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 553;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 553)
		{
			expect_opcode(0x3bf8,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffffffcULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"absolute SQURS subtracts signed square and costs two cycles");
			program.write_word(0x05e2, 0x5193); // DADD *AR3+,A,B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f90, 0x1534);
			data.write_word(0x0f91, 0x3456);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x56788933);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 554;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 554 && m_phase <= 562)
		{
			struct long_alu_case { u16 opcode; u64 a_before; u16 st1; u16 high; u16 low; u64 a_after; u64 b_after; u16 ar3_after; u16 st0_after; unsigned cycles; };
			static constexpr long_alu_case cases[] = {
				{ 0x5193, 0x56788933, 0x0100, 0x1534, 0x3456, 0x56788933, 0x6bacbd89, 0x0f92, 0, 3 },
				{ 0x518b, 0x56783933, 0x0180, 0x1534, 0x3456, 0x56783933, 0x6bac6d89, 0x0f8e, 0, 3 },
				{ 0x5493, 0x56788933, 0x0100, 0x1534, 0x3456, 0x414454dd, 0, 0x0f92, 0x0800, 3 },
				{ 0x548b, 0x56783933, 0x0180, 0x1534, 0x3456, 0x414404dd, 0, 0x0f8e, 0x0800, 3 },
				{ 0x50f8, 0x7fffffff, 0x0300, 0, 1, 0x7fffffff, 0, 0x0f90, 0x0400, 4 },
				{ 0x5083, 0x7fffffff, 0x0380, 1, 1, 0x80000000, 0, 0x0f90, 0, 3 },
				{ 0x5483, 0x00010000, 0x0380, 0, 1, 0x0001ffff, 0, 0x0f90, 0x0800, 3 },
				{ 0x5893, 0x56788933, 0x0100, 0x1534, 0x3456, 0xffbebbab23ULL, 0, 0x0f92, 0, 3 },
				{ 0x588b, 0x56783933, 0x0180, 0x1534, 0x3456, 0xffbebcfb23ULL, 0, 0x0f8e, 0, 3 }
			};
			const unsigned index = m_phase - 554;
			const long_alu_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == row.ar3_after &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == row.st0_after &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == row.cycles,
				"DADD/DSUB/DRSUB long-word arithmetic matches TI C16 examples and cycle costs");
			if (m_phase < 562)
			{
				const long_alu_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				program.write_word(0x05e3, next.opcode == 0x50f8 ? 0x0f90 : 0x75d6);
				program.write_word(0x05e4, next.opcode == 0x50f8 ? 0x75d6 : 0x0124);
				program.write_word(0x05e5, next.opcode == 0x50f8 ? 0x0124 : 0xf5e1);
				program.write_word(0x05e6, 0xf5e1);
				data.write_word(0x0f90, next.high);
				data.write_word(0x0f91, next.low);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, next.a_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0,
						next.opcode == 0x588b ? 0x0800 : 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, next.st1);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x5a8b); // DADST *AR3-,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f90, 0x1534);
			data.write_word(0x0f91, 0x3456);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x2345);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0180); // SXM, C16
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 563;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 563 && m_phase <= 569)
		{
			struct t_long_case { u16 opcode; u16 st1; u64 a_after; u64 b_after; u16 ar3_after; int carry; unsigned cycles; };
			static constexpr t_long_case cases[] = {
				{ 0x5a8b, 0x0180, 0x38791111, 0, 0x0f8e, 0, 3 },
				{ 0x5a93, 0x0100, 0x3879579b, 0, 0x0f92, 0, 3 },
				{ 0x5e93, 0x0100, 0xfff1ef1111ULL, 0, 0x0f92, 0, 3 },
				{ 0x5e8b, 0x0180, 0xfff1ef579bULL, 0, 0x0f8e, -1, 3 },
				{ 0x5c93, 0x0100, 0xfff1ef1111ULL, 0, 0x0f92, 0, 3 },
				{ 0x5c8b, 0x0180, 0xfff1ef1111ULL, 0, 0x0f8e, 0, 3 },
				{ 0x5bf8, 0x0180, 0, 0x38791111, 0x0f90, 0, 4 }
			};
			const unsigned index = m_phase - 563;
			const t_long_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x2345 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == row.ar3_after &&
				(row.carry < 0 || bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == bool(row.carry)) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == row.cycles,
				"T-based long-word arithmetic matches TI result and cycle examples");
			if (m_phase < 569)
			{
				const t_long_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				program.write_word(0x05e3, next.opcode == 0x5bf8 ? 0x0f90 : 0x75d6);
				program.write_word(0x05e4, next.opcode == 0x5bf8 ? 0x75d6 : 0x0124);
				program.write_word(0x05e5, next.opcode == 0x5bf8 ? 0x0124 : 0xf5e1);
				program.write_word(0x05e6, 0xf5e1);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x2345);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, next.st1);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x1f83); // SUBC *AR3,B
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f90, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 570;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 570)
		{
			expect_opcode(0x1f83,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10001 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUBC B uses the 1f encoding and leaves A unchanged");
			program.write_word(0x05e2, 0x5aea); // DADST *+AR2(6),A
			program.write_word(0x05e3, 6);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f06, 0x1534);
			data.write_word(0x0f07, 0x3456);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x2345);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, C16 clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 571;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 571)
		{
			expect_opcode(0x5aea,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x3879579b &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x2345 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f06 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"long-offset DADST preupdates AR and costs two cycles");
			program.write_word(0x05e2, 0x0e83); // SUBB *AR3,A
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f90, 6);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 6);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // Borrow input.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 572;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 572)
		{
			expect_opcode(0x0e83,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffffULL &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUBB A consumes inverted carry as borrow in one cycle");
			program.write_word(0x05e2, 0x0f83); // SUBB *AR3,B
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff80000006ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // No borrow input.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 573;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 573)
		{
			expect_opcode(0x0f83,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80000000ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUBB B preserves A and no-borrow carry");
			program.write_word(0x05e2, 0x0183); // ADD *AR3,B
			m_port_writes = 0;
			data.write_word(0x0f90, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 574;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 574)
		{
			expect_opcode(0x0183,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x100000000ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ADD Smem,B carries across bit 32 in one cycle");
			program.write_word(0x05e2, 0x0b83); // SUBS *AR3,B
			data.write_word(0x0f90, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 575;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 575)
		{
			expect_opcode(0x0b83,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 1 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUBS Smem,B suppresses SXM sign extension in one cycle");
			program.write_word(0x05e2, 0x1983); // AND *AR3,B
			data.write_word(0x0f90, 0x00ff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123456);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 576;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 576)
		{
			expect_opcode(0x1983,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x56 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"AND Smem,B zeroes upper bits without changing carry");
			program.write_word(0x05e2, 0x2183); // MPY *AR3,B
			data.write_word(0x0f90, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xfffe);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, FRCT clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 577;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 577)
		{
			expect_opcode(0x2183,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffffffaULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MPY Smem,B signs both factors and leaves carry unchanged");
			program.write_word(0x05e2, 0x2383); // MPYR *AR3,B
			data.write_word(0x0f90, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 578;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 578 && m_phase <= 586)
		{
			struct multiply_case { u16 opcode, t, memory, t_after; u64 a_before, b_before, a_after, b_after; };
			static constexpr multiply_case cases[] = {
				{ 0x2383, 0x4000, 3,      0x4000, 0x1234, 0,       0x1234, 0x10000 },
				{ 0x2583, 0xfffe, 3,      0xfffe, 0x1234, 0,       0x1234, 0x2fffa },
				{ 0x2783, 0x4000, 0xfffe, 0xfffe, 0x1234, 0,       0x1234, 4 },
				{ 0x2983, 0xfffe, 3,      0xfffe, 0x1234, 10,      0x1234, 4 },
				{ 0x2b83, 0x4000, 3,      0x4000, 0x1234, 0x10000, 0x1234, 0x20000 },
				{ 0x2c83, 2,      3,      2,      10,     0x5678,  4,      0x5678 },
				{ 0x2d83, 2,      3,      2,      0x1234, 10,      0x1234, 4 },
				{ 0x2e83, 0x4000, 3,      0x4000, 0x30000, 0x5678, 0x20000, 0x5678 },
				{ 0x2f83, 0x4000, 3,      0x4000, 0x1234, 0x30000, 0x1234, 0x20000 }
			};
			const unsigned index = m_phase - 578;
			const multiply_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"multiply/MAC family result, side effects, and one-cycle DARAM timing");
			if (m_phase < 586)
			{
				const multiply_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				data.write_word(0x0f90, next.memory);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, next.a_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, next.b_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, next.t);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x3383); // MASA *AR3
			data.write_word(0x0f90, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x20000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 10);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 587;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 587 && m_phase <= 590)
		{
			struct acca_case { u16 opcode, memory, t_before, t_after, ar3_after; u64 a_before, b_before, a_after, b_after; };
			static constexpr acca_case cases[] = {
				{ 0x3383, 3,      0,      3,      0x0f90, 0x20000,    10,      0x20000, 4 },
				{ 0x3583, 3,      0,      3,      0x0f90, 0x20000,    10,      0x20000, 16 },
				{ 0x3783, 3,      0,      3,      0x0f90, 0x40000000, 0x10000, 0x40000000, 0x20000 },
				{ 0x3693, 0x2000, 0x5678, 0x5678, 0x0f91, 0x12340000, 0x10000, 0x06270000, 0x20000000 }
			};
			const unsigned index = m_phase - 587;
			const acca_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == row.ar3_after &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ACCA multiply and POLY result, T/AR update, and one-cycle timing");
			if (m_phase < 590)
			{
				const acca_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				data.write_word(0x0f90, next.memory);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, next.a_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, next.b_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, next.t_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, FRCT clear.
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x3083); // LD *AR3,T
			data.write_word(0x0f90, 0xabcd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 591;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 591 && m_phase <= 597)
		{
			struct shifted_case { u16 opcode, memory, t_before, t_after; u64 a_before, b_before, a_after, b_after; };
			static constexpr shifted_case cases[] = {
				{ 0x3083, 0xabcd, 0, 0xabcd, 0x1234, 0x5678, 0x1234, 0x5678 },
				{ 0x3983, 0xfffe, 0, 0xfffe, 0x1234, 10, 0x1234, 14 },
				{ 0x3a83, 3, 0, 3, 10, 0x5678, 1, 0x5678 },
				{ 0x3c83, 0xfffe, 0, 0, 0x30000, 0x5678, 0x10000, 0x5678 },
				{ 0x3d83, 2, 0, 0, 1, 0x5678, 1, 0x20001 },
				{ 0x3e83, 0xfffe, 0, 0, 0x1234, 0x30000, 0x10000, 0x30000 },
				{ 0x3f83, 2, 0, 0, 0x1234, 1, 0x1234, 0x20001 }
			};
			const unsigned index = m_phase - 591;
			const shifted_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"T load, square, and shifted-add routing with one-cycle DARAM timing");
			if (m_phase < 597)
			{
				const shifted_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				data.write_word(0x0f90, next.memory);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, next.a_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, next.b_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, next.t_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, FRCT clear.
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x4083); // SUB *AR3,16,A,A
			data.write_word(0x0f90, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x30000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // No borrow must retain cleared C.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 598;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 598 && m_phase <= 601)
		{
			struct shifted_sub_case { u16 opcode, st0_before, st0_after; u64 a_before, b_before, a_after, b_after; };
			static constexpr shifted_sub_case cases[] = {
				{ 0x4083, 0,      0,      0x30000, 0x5678, 0x10000,       0x5678 },
				{ 0x4183, 0x0800, 0x0800, 0x30000, 0x5678, 0x30000,       0x10000 },
				{ 0x4283, 0x0800, 0,      0x1234,  0x10000, 0xffffff0000, 0x10000 },
				{ 0x4383, 0,      0,      0x1234,  0x30000, 0x1234,        0x10000 }
			};
			const unsigned index = m_phase - 598;
			const shifted_sub_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x9abc &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == row.st0_after &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"shifted SUB routing, borrow-only carry change, and one-cycle DARAM timing");
			if (m_phase < 601)
			{
				const shifted_sub_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				data.write_word(0x0f90, 2);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, next.a_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, next.b_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, next.st0_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x5283); // DADD *AR3,B,A
			data.write_word(0x0f90, 1);
			data.write_word(0x0f91, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x30002);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 602;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 602 && m_phase <= 605)
		{
			struct dadd_case { u16 opcode, st1; u64 a_after, b_after; };
			static constexpr dadd_case cases[] = {
				{ 0x5283, 0x0100, 0x50001, 0x30002 },
				{ 0x5383, 0x0100, 0x1234,  0x50001 },
				{ 0x5283, 0x0180, 0x40001, 0x30002 },
				{ 0x5383, 0x0180, 0x1234,  0x40001 }
			};
			const unsigned index = m_phase - 602;
			const dadd_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"DADD B-source routing and C16 carry isolation with one-cycle DARAM timing");
			if (m_phase < 605)
			{
				const dadd_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x30002);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, next.st1);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0x5583); // DSUB *AR3,B
			data.write_word(0x0f90, 1);
			data.write_word(0x0f91, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x20001);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 606;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 606 && m_phase <= 613)
		{
			struct long_b_case { u16 opcode, high, low, st1; u64 b_before, b_after; };
			static constexpr long_b_case cases[] = {
				{ 0x5583, 1, 0xffff, 0x0100, 0x20001, 2 },
				{ 0x5583, 1, 0xffff, 0x0180, 0x20001, 0x10002 },
				{ 0x5983, 2, 1,      0x0100, 0x1ffff, 2 },
				{ 0x5983, 2, 1,      0x0180, 0x1ffff, 0x10002 },
				{ 0x5d83, 1, 1,      0x0100, 0x5678,  0xfffffeffff },
				{ 0x5d83, 1, 1,      0x0180, 0x5678,  0xffffffffff },
				{ 0x5f83, 1, 1,      0x0100, 0x5678,  0xfffffeffff },
				{ 0x5f83, 1, 1,      0x0180, 0x5678,  0xffffff0003 }
			};
			const unsigned index = m_phase - 606;
			const long_b_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"B long-arithmetic C16 mode and one-cycle DARAM timing");
			if (m_phase < 613)
			{
				const long_b_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				data.write_word(0x0f90, next.high);
				data.write_word(0x0f91, next.low);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, next.b_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, next.st1);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xb03a); // MAC *AR5,*AR4+,A,A
			data.write_word(0x0f90, 2);
			data.write_word(0x0f91, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x18000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x28000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 614;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 614 && m_phase <= 629)
		{
			struct dual_mac_case { u16 opcode; u64 a_after, b_after; };
			static constexpr dual_mac_case cases[] = {
				{ 0xb03a, 0x18006, 0x28000 }, { 0xb13a, 0x18000, 0x18006 },
				{ 0xb23a, 0x28006, 0x28000 }, { 0xb33a, 0x18000, 0x28006 },
				{ 0xb43a, 0x20000, 0x28000 }, { 0xb53a, 0x18000, 0x20000 },
				{ 0xb63a, 0x30000, 0x28000 }, { 0xb73a, 0x18000, 0x30000 },
				{ 0xb83a, 0x17ffa, 0x28000 }, { 0xb93a, 0x18000, 0x17ffa },
				{ 0xba3a, 0x27ffa, 0x28000 }, { 0xbb3a, 0x18000, 0x27ffa },
				{ 0xbc3a, 0x10000, 0x28000 }, { 0xbd3a, 0x18000, 0x10000 },
				{ 0xbe3a, 0x20000, 0x28000 }, { 0xbf3a, 0x18000, 0x20000 }
			};
			const unsigned index = m_phase - 614;
			const dual_mac_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"dual MAC/MAS source, destination, rounding, pointer, and one-cycle timing");
			if (m_phase < 629)
			{
				const dual_mac_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x18000);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x28000);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xd03a); // ST A,*AR4+ || MAC *AR5,A
			data.write_word(0x0f90, 3);
			data.write_word(0x0f91, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x18000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x28000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 630;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 630 && m_phase <= 645)
		{
			struct parallel_mac_case { u16 opcode, store; u64 a_after, b_after; };
			static constexpr parallel_mac_case cases[] = {
				{ 0xd03a, 1, 0x18006, 0x28000 }, { 0xd13a, 1, 0x18000, 0x28006 },
				{ 0xd23a, 2, 0x18006, 0x28000 }, { 0xd33a, 2, 0x18000, 0x28006 },
				{ 0xd43a, 1, 0x20000, 0x28000 }, { 0xd53a, 1, 0x18000, 0x30000 },
				{ 0xd63a, 2, 0x20000, 0x28000 }, { 0xd73a, 2, 0x18000, 0x30000 },
				{ 0xd83a, 1, 0x17ffa, 0x28000 }, { 0xd93a, 1, 0x18000, 0x27ffa },
				{ 0xda3a, 2, 0x17ffa, 0x28000 }, { 0xdb3a, 2, 0x18000, 0x27ffa },
				{ 0xdc3a, 1, 0x10000, 0x28000 }, { 0xdd3a, 1, 0x18000, 0x20000 },
				{ 0xde3a, 2, 0x10000, 0x28000 }, { 0xdf3a, 2, 0x18000, 0x20000 }
			};
			const unsigned index = m_phase - 630;
			const parallel_mac_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
				data.read_word(0x0f91) == row.store &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"parallel store MAC/MAS routing, rounding, and one-cycle timing");
			if (m_phase < 645)
			{
				const parallel_mac_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				data.write_word(0x0f91, 0);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x18000);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x28000);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xd93a); // TI SPRU172C: ST A,*AR4+ || MAS *AR5,B.
			data.write_word(0x0f90, 0x4321);
			data.write_word(0x0f91, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x111111);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1111);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x0400);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0105); // ASM = 5.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 646;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 646 || m_phase == 647)
		{
			const bool rounded = m_phase == 647;
			expect_opcode(rounded ? 0xddba : 0xd93a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x111111 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) ==
					(rounded ? 0xfffef40000ULL : 0xfffef38d11ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0400 &&
				data.read_word(0x0f91) == (rounded ? 0x0022 : 0x0222) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == (rounded ? 0x0f91 : 0x0f90) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI parallel MAS/MASR examples match result, ASM store, pointer, and cycle cost");
			if (!rounded)
			{
				program.write_word(0x05e2, 0xddba); // ST A,*AR4+ || MASR *AR5+,B.
				data.write_word(0x0f91, 0x1234);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x111111);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1111);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x0400);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // ASM = 1.
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				m_phase = 647;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xd9b7); // ST A,*AR5- || MAS *AR5+,B.
			data.write_word(0x0f90, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x18000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x28000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 648;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 648)
		{
			expect_opcode(0xd9b7,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x18000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x27ffa &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
				data.read_word(0x0f90) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f91 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"parallel MAS reads X before aliased Y store and applies Xmod once");
			program.write_word(0x05e2, 0xb83a); // MAS *AR5,*AR4+,A,A with FRCT.
			data.write_word(0x0f90, 2);
			data.write_word(0x0f91, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x18000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x28000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 649;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 649 && m_phase <= 652)
		{
			struct multiply_mode_case {
				u16 opcode, x, y, t_before, t_after, st1, st0_after, store;
				u64 a_before, b_before, a_after, b_after;
			};
			static constexpr multiply_mode_case cases[] = {
				{ 0xb83a, 2,      3, 0, 2,      0x0140, 0,      3,
				  0x18000,    0x28000, 0x17ff4,    0x28000 },
				{ 0xd93a, 3,      0, 2, 2,      0x0140, 0,      1,
				  0x18000,    0x28000, 0x18000,    0x27ff4 },
				{ 0xb83a, 0xffff, 2, 0, 0xffff, 0x0300, 0x0400, 2,
				  0x7fffffff, 0,       0x7fffffff, 0 },
				{ 0xd93a, 0xffff, 0, 2, 2,      0x0300, 0x0200, 1,
				  0x18000,    0x7fffffff, 0x18000, 0x7fffffff }
			};
			const unsigned index = m_phase - 649;
			const multiply_mode_case &row = cases[index];
			expect_opcode(row.opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0600) == row.st0_after &&
				data.read_word(0x0f91) == row.store &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"dual and parallel MAS FRCT doubling, OVM saturation, and one-cycle timing");
			if (m_phase < 652)
			{
				const multiply_mode_case &next = cases[index + 1];
				program.write_word(0x05e2, next.opcode);
				data.write_word(0x0f90, next.x);
				data.write_word(0x0f91, next.y);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, next.a_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, next.b_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_T, next.t_before);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, next.st1);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xe13a); // LMS *AR5,*AR4+.
			data.write_word(0x0f90, 0x0055);
			data.write_word(0x0f91, 0x00aa);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x77778888);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x100);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4444);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 653;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 653)
		{
			expect_opcode(0xe13a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x77cd0888 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x3972 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI LMS example updates both accumulators without overwriting T in one cycle");
			program.write_word(0x05e2, 0xa6ab); // MACSU *AR4+,*AR5+,A.
			data.write_word(0x0f90, 0x8765);
			data.write_word(0x0f91, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2222);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 8);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 654;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 654)
		{
			expect_opcode(0xa6ab,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x09a0aa84 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2222 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8765 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f91 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f92 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI MACSU example uses unsigned X, signed Y, and one-cycle dual addressing");
			program.write_word(0x05e2, 0xa7ab); // MACSU *AR4+,*AR5+,B.
			data.write_word(0x0f90, 0x8765);
			data.write_word(0x0f91, 0xfffe);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 655;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 655)
		{
			expect_opcode(0xa7ab,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffde26cULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8765 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f91 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f92 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MACSU B uses signed Y, unsigned X, FRCT doubling, and one cycle");
			program.write_word(0x05e2, 0x788b); // MACP *AR3-,0700,A.
			program.write_word(0x05e3, 0x0700);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			program.write_word(0x0700, 0x1234);
			data.write_word(0x0f90, 0x0055);
			data.write_word(0x0f91, 0x0066);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 8);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 656;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 656)
		{
			expect_opcode(0x788b,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
				data.read_word(0x0f91) == 0x0066 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"TI MACP example reads program coefficient without delay write in three cycles");
			program.write_word(0x05e2, 0x7a8b); // MACD *AR3-,0700,A.
			data.write_word(0x0f91, 0x0066);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 8);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 657;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 657)
		{
			expect_opcode(0x7a8b,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
				data.read_word(0x0f91) == 0x0055 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"TI MACD example copies Smem to successor and costs three cycles");
			program.write_word(0x05e2, 0xec02); // RPT #2.
			program.write_word(0x05e3, 0x7893); // MACP *AR3+,0700,A.
			program.write_word(0x05e4, 0x0700);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			program.write_word(0x0700, 4);
			program.write_word(0x0701, 5);
			program.write_word(0x0702, 6);
			data.write_word(0x0f90, 1);
			data.write_word(0x0f91, 2);
			data.write_word(0x0f92, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 658;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 658)
		{
			expect_opcode(0x7893,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 32 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f93 &&
				data.read_word(0x0f91) == 2 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"repeated MACP advances program coefficient and pipelines after first multiply");
			program.write_word(0x05e2, 0xe03a); // FIRS *AR5,*AR4+,0700.
			program.write_word(0x05e3, 0x0700);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			program.write_word(0x0700, 0x1234);
			data.write_word(0x0f90, 0x0055);
			data.write_word(0x0f91, 0x00aa);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4444);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 659;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 659)
		{
			expect_opcode(0xe03a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff0000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8762c &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"TI FIRS example accumulates old A high into B and sums X/Y into A");
			program.write_word(0x05e2, 0xec02); // RPT #2.
			program.write_word(0x05e3, 0xe03a); // FIRS *AR5,*AR4+,0700.
			program.write_word(0x05e4, 0x0700);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			program.write_word(0x0700, 1);
			program.write_word(0x0701, 2);
			program.write_word(0x0702, 3);
			data.write_word(0x0f90, 1);
			data.write_word(0x0f91, 2);
			data.write_word(0x0f92, 2);
			data.write_word(0x0f93, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4444);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 660;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 660)
		{
			expect_opcode(0xe03a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x30000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x86 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f94 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"repeated FIRS uses old A, advances coefficients, and pipelines after first pass");
			program.write_word(0x05e2, 0xa03a); // ADD *AR5,*AR4+,A.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f90, 0x0055);
			data.write_word(0x0f91, 0x00aa);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4444);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 661;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase >= 661 && m_phase <= 664)
		{
			const u16 opcode = 0xa03a + ((m_phase - 661) << 8);
			const bool subtract = m_phase >= 663;
			const bool destination_b = BIT(m_phase - 661, 0);
			const u64 result = subtract ? 0xffffab0000ULL : 0xff0000;
			expect_opcode(opcode,
				m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? 0x1234 : result) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? result : 0x5678) &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"dual-memory ADD/SUB selects A/B, shifts both operands, and costs one cycle");
			if (m_phase < 664)
			{
				program.write_word(0x05e2, opcode + 0x100);
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
			program.write_word(0x05e2, 0xe33a); // ABDST *AR5,*AR4+.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffabcd0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 665;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 665)
		{
			expect_opcode(0xe33a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffab0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5433 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI ABDST example adds absolute old A high to B and subtracts dual operands");
			data.write_word(0x0f90, 0xfffe);
			data.write_word(0x0f91, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffabcd0000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 666;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 666)
		{
			expect_opcode(0xe33a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffb0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xa866 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ABDST sign-extends X/Y under SXM and doubles distance under FRCT");
			program.write_word(0x05e2, 0x4c83); // LTD *AR3.
			data.write_word(0x0f90, 0x6cac);
			data.write_word(0x0f91, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 667;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 667)
		{
			expect_opcode(0x4c83,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x6cac &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
				data.read_word(0x0f90) == 0x6cac &&
				data.read_word(0x0f91) == 0x6cac &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI LTD example copies Smem into T and the next data address in one cycle");
			program.write_word(0x05e2, 0x4c93); // LTD *AR3+.
			data.write_word(0x0f90, 0x1234);
			data.write_word(0x0f91, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 668;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 668)
		{
			expect_opcode(0x4c93,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f91 &&
				data.read_word(0x0f91) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"LTD postincrements only after writing the original address successor");
			program.write_word(0x05e2, 0xf43f); // SUB A>>1,A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffffffcULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 669;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 669)
		{
			expect_opcode(0xf43f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffffeULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUB A>>1,A sign-fills the 40-bit source when SXM is set in one cycle");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffffffcULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 670;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 670)
		{
			expect_opcode(0xf43f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7ffffffffeULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUB A>>1,A zero-fills the 40-bit source when SXM is clear in one cycle");
			program.write_word(0x05e2, 0xf165); // XOR #lk,16,A,B.
			program.write_word(0x05e3, 0x00ff);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12340000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 671;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 671)
		{
			expect_opcode(0xf165,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12340000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff12cb0000ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"XOR #lk,16,A,B preserves A and carry, updates B, and takes two cycles");
			program.write_word(0x05e2, 0xb43a); // MACR *AR5,*AR4+,A,A.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f90, 0);
			data.write_word(0x0f91, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffff8000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 672;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 672)
		{
			expect_opcode(0xb43a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MACR rounds negative half-word exactly to zero in one cycle");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffff7fffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f91);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 673;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 673)
		{
			expect_opcode(0xb43a,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MACR rounds just below negative half-word to minus one word in one cycle");
			program.write_word(0x05e2, 0x798b); // MACP *AR3-,0700,B.
			program.write_word(0x05e3, 0x0700);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			program.write_word(0x0700, 0x1234);
			data.write_word(0x0f90, 0x0055);
			data.write_word(0x0f91, 0x0066);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 8);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 674;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 674)
		{
			expect_opcode(0x798b,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
				data.read_word(0x0f91) == 0x0066 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"MACP B preserves A and the successor while multiplying in three cycles");
			program.write_word(0x05e2, 0x7b8b); // MACD *AR3-,0700,B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 8);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 675;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 675)
		{
			expect_opcode(0x7b8b,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
				data.read_word(0x0f91) == 0x0055 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"MACD B preserves A, copies the successor, and costs three cycles");
			program.write_word(0x05e2, 0x79f8); // MACP *(absolute),0700,B.
			program.write_word(0x05e3, 0x0f90);
			program.write_word(0x05e4, 0x0700);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0f90, 0x0055);
			data.write_word(0x0f91, 0x0066);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 676;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 676)
		{
			expect_opcode(0x79f8,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				data.read_word(0x0f91) == 0x0066 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"absolute MACP consumes data address before coefficient in four cycles");
			program.write_word(0x05e2, 0x7bf8); // MACD *(absolute),0700,B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 677;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 677)
		{
			expect_opcode(0x7bf8,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				data.read_word(0x0f91) == 0x0055 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"absolute MACD copies the successor and costs four cycles");
			program.write_word(0x05e2, 0x79ea); // MACP *+AR2(5),0700,B.
			program.write_word(0x05e3, 0x0700);
			program.write_word(0x05e4, 5);
			data.write_word(0x0f05, 0x0055);
			data.write_word(0x0f06, 0x0066);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 678;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 678)
		{
			expect_opcode(0x79ea,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				data.read_word(0x0f06) == 0x0066 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset MACP consumes coefficient before offset in four cycles");
			program.write_word(0x05e2, 0x7bea); // MACD *+AR2(5),0700,B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 679;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 679)
		{
			expect_opcode(0x7bea,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				data.read_word(0x0f06) == 0x0055 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset MACD copies successor after AR2 preupdate in four cycles");
			program.write_word(0x05e2, 0x4cf8); // LTD *(absolute).
			program.write_word(0x05e3, 0x0f90);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f90, 0x6cac);
			data.write_word(0x0f91, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 680;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 680)
		{
			expect_opcode(0x4cf8,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x6cac &&
				data.read_word(0x0f90) == 0x6cac &&
				data.read_word(0x0f91) == 0x6cac &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"absolute LTD copies the selected word to its successor in two cycles");
			program.write_word(0x05e2, 0x4cea); // LTD *+AR2(5).
			program.write_word(0x05e3, 5);
			data.write_word(0x0f05, 0x1234);
			data.write_word(0x0f06, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 681;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 681)
		{
			expect_opcode(0x4cea,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				data.read_word(0x0f05) == 0x1234 &&
				data.read_word(0x0f06) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"long-offset LTD preupdates AR2 and copies to its successor in two cycles");
			program.write_word(0x05e2, 0x1045); // LD 45h,A, direct Smem.
			program.write_word(0x05e3, 0x8046); // STL A,46h, direct Smem.
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0945, 0x1234);
			data.write_word(0x0946, 0);
			data.write_word(0x0f90, 0xabcd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0012); // DP 12h.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // CPL clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 682;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 682)
		{
			expect_opcode(0x1045,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				data.read_word(0x0946) == 0x1234 &&
				data.read_word(0x0f90) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"direct Smem load/store use DP:offset with CPL clear in one cycle each");
			data.write_word(0x0f45, 0x5678);
			data.write_word(0x0f46, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4100); // CPL set.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 683;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 683)
		{
			expect_opcode(0x1045,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x5678 &&
				data.read_word(0x0f46) == 0x5678 &&
				data.read_word(0x0946) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"direct Smem load/store use SP+offset with CPL set in one cycle each");
			program.write_word(0x05e2, 0x6b45); // ADDM #1, 45h.
			program.write_word(0x05e3, 1);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0945, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 684;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 684)
		{
			expect_opcode(0x6b45,
				data.read_word(0x0945) == 0x1235 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"direct ADDM uses DP:offset without modifying an AR");
			program.write_word(0x05e2, 0x4c45); // LTD 45h.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f45, 0x5678);
			data.write_word(0x0f46, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4100);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 685;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 685)
		{
			expect_opcode(0x4c45,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5678 &&
				data.read_word(0x0f46) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"direct LTD uses SP+offset without modifying an AR");
			program.write_word(0x05e2, 0x2045); // MPY 45h, A.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0945, 4);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 686;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 686)
		{
			expect_opcode(0x2045,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 12 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"direct MPY uses DP:offset and costs one cycle");
			program.write_word(0x05e2, 0x2945); // MAC 45h, B.
			data.write_word(0x0f45, 0xfffc);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4100);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 16);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 687;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 687)
		{
			expect_opcode(0x2945,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 12 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"direct MAC uses SP+offset and costs one cycle");
			program.write_word(0x05e2, 0x26ea); // SQUR *+AR2(5), A.
			program.write_word(0x05e3, 5);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f05, 0xfffd);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 688;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 688)
		{
			expect_opcode(0x26ea,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 9 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"long-offset SQUR publishes T, preupdates AR, and costs two cycles");
			program.write_word(0x05e2, 0x271e); // SQUR 30, B (TI SPRU172C example).
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x031e, 0x000f);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0006);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x01f4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 689;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 689)
		{
			expect_opcode(0x271e,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00e1 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x000f &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI direct SQUR example squares 15 into B in one cycle");
			program.write_word(0x05e2, 0x391e); // SQURA 30, B (TI SPRU172C example).
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x03200000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 690;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 690)
		{
			expect_opcode(0x391e,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x032000e1 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x000f &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI direct SQURA example adds 15 squared in one cycle");
			program.write_word(0x05e2, 0x3a09); // SQURS 9, A (TI SPRU172C example).
			data.write_word(0x0309, 0x1234);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x014b5db0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x8765);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 691;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 691)
		{
			expect_opcode(0x3a09,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0320 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI direct SQURS example subtracts 1234h squared in one cycle");
			program.write_word(0x05e2, 0x0605); // ADDC 5, A.
			data.write_word(0x0405, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0808); // C=1, DP=8.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must be ignored.
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 692;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 692)
		{
			expect_opcode(0x0605,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"direct ADDC zero-extends FFFFh and consumes carry despite SXM");
			program.write_word(0x05e2, 0x0e05); // SUBB 5, A (TI SPRU172C example).
			data.write_word(0x0405, 6);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0008); // C=0, DP=8.
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 6);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 693;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 693)
		{
			expect_opcode(0x0e05,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffffULL &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI direct SUBB example subtracts inverted carry as borrow");
			program.write_word(0x05e2, 0x6b94); // ADDM #FFF8h, *AR4+ (TI example).
			program.write_word(0x05e3, 0xfff8);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0100, 0x8007);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 694;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 694)
		{
			expect_opcode(0x6b94,
				data.read_word(0x0100) == 0x8000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0101 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"TI ADDM example saturates negative overflow and costs two cycles");
			program.write_word(0x05e2, 0xcdb9); // ST A,*AR3+ || MPY *AR5+,B.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0200, 0x1111);
			data.write_word(0x0300, 0x4000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff84211234ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x4000);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 695;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 695)
		{
			expect_opcode(0xcdb9,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff84211234ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x20000000 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4000 &&
				data.read_word(0x0200) == 0x8421 &&
				data.read_word(0x0300) == 0x4000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0201 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI ST||MPY stores old A and squares fractional 4000h into B in one cycle");
			program.write_word(0x05e2, 0xcfb9); // ST B,*AR3+ || MPY *AR5+,B.
			data.write_word(0x0200, 0);
			data.write_word(0x0300, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcdef);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 696;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 696)
		{
			expect_opcode(0xcfb9,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xabcdef &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 6 &&
				data.read_word(0x0200) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0201 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ST||MPY stores the old destination before replacing it with the product");
			program.write_word(0x05e2, 0xc131); // ST A,*AR3 || ADD *AR5,B.
			data.write_word(0x0200, 0);
			data.write_word(0x0300, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 697;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 697)
		{
			expect_opcode(0xc131,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12365678 &&
				data.read_word(0x0200) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ST||ADD uses opposite accumulator and stores old source in one cycle");
			program.write_word(0x05e2, 0xc5f5); // ST A,*AR3- || SUB *AR5+0%,B.
			data.write_word(0x01ff, 0x1111);
			data.write_word(0x0300, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff84210000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0010000001ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x01ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); // SST clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // SXM, ASM=1.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 698;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 698)
		{
			expect_opcode(0xc5f5,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff84210000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffbe00000ULL &&
				data.read_word(0x01ff) == 0x0842 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x01fe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0302 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI ST||SUB example subtracts opposite accumulator in one cycle");
			program.write_word(0x05e2, 0xca31); // ST B,*AR3 || LD *AR5,A.
			data.write_word(0x0200, 0);
			data.write_word(0x0300, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 699;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 699)
		{
			expect_opcode(0xca31,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80010000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
				data.read_word(0x0200) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ST||LD sign-extends Xmem into A and stores B in one cycle");
			program.write_word(0x05e2, 0xe411); // ST A,*AR3 || LD *AR3,T.
			data.write_word(0x0200, 0x80ff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 700;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 700)
		{
			expect_opcode(0xe411,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x80ff &&
				data.read_word(0x0200) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ST||LD T reads Xmem before writing the same Ymem cell");
			program.write_word(0x05e2, 0xc131); // ST A,*AR3 || ADD *AR5,B.
			data.write_word(0x0300, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // Clear carry on ADD.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 701;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 701)
		{
			expect_opcode(0xc131,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fffffff &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == 0x0200 &&
				data.read_word(0x0200) == 0x7fff &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ST||ADD saturates B, sets OVB and clears carry without altering the store");
			program.write_word(0x05e2, 0xc531); // ST A,*AR3 || SUB *AR5,B.
			data.write_word(0x0300, 0x8000);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x00010000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 702;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 702)
		{
			expect_opcode(0xc531,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80000000ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == 0x0a00 &&
				data.read_word(0x0200) == 1 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ST||SUB saturates negative B, sets OVB and carry, and stores old A");
			program.write_word(0x05e2, 0x0483); // ADD *AR3,TS,A.
			data.write_word(0x0200, 0x0100);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 4);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 703;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 703)
		{
			expect_opcode(0x0483,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1010 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ADD Smem,TS uses T low six bits and costs one cycle");
			program.write_word(0x05e2, 0x0d83); // SUB *AR3,TS,B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 704;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 704)
		{
			expect_opcode(0x0d83,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUB Smem,TS sets no-borrow carry and costs one cycle");
			program.write_word(0x05e2, 0x1583); // LD *AR3,TS,B.
			data.write_word(0x0200, 0xfedc);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 8);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 705;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 705)
		{
			expect_opcode(0x1583,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffedc00ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI LD Smem,TS example sign-extends fedc and shifts by eight");
			program.write_word(0x05e2, 0x1483); // LD *AR3,TS,A.
			data.write_word(0x0200, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x003f); // TS=-1.
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 706;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 706)
		{
			expect_opcode(0x1483,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffc000ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"LD Smem,TS sign-fills a negative T shift in one cycle");
			program.write_word(0x05e2, 0x9083); // ADD *AR2+,3,A.
			data.write_word(0x0300, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 707;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 707)
		{
			expect_opcode(0x9083,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 17 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ADD Xmem,SHFT shifts Xmem and postincrements its pointer in one cycle");
			program.write_word(0x05e2, 0x9304); // SUB *AR2,4,B.
			data.write_word(0x0300, 0x8001);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 708;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 708)
		{
			expect_opcode(0x9304,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fff0 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUB Xmem,SHFT sign-extends Xmem and records borrow in one cycle");
			program.write_word(0x05e2, 0xf486); // MAX A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffffff6ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffcbULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 709;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 709)
		{
			expect_opcode(0xf486,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffff6ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffffcbULL &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI MAX A chooses the larger signed accumulator and clears carry");
			program.write_word(0x05e2, 0xf586); // MAX B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x55);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 710;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 710)
		{
			expect_opcode(0xf586,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x55 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MAX B preserves B and sets carry when B is larger");
			program.write_word(0x05e2, 0xf487); // MIN A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 711;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 711)
		{
			expect_opcode(0xf487,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI MIN tie chooses B and sets carry");
			program.write_word(0x05e2, 0xf587); // MIN B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 712;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 712)
		{
			expect_opcode(0xf587,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff00000000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff00000000ULL &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MIN B compares the signed guard byte and selects A in one cycle");
			program.write_word(0x05e2, 0xf48e); // EXP A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffcbULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 713;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 713)
		{
			expect_opcode(0xf48e,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x19 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffcbULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI EXP A example computes 25 redundant sign shifts in one cycle");
			program.write_word(0x05e2, 0xf58e); // EXP B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0785432105ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 714;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 714)
		{
			expect_opcode(0xf58e,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffc &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0785432105ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI EXP B example reports negative exponent for guard-byte content");
			program.write_word(0x05e2, 0xf48f); // NORM A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffff001ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x13);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 715;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 715)
		{
			expect_opcode(0xf48f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80080000ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI NORM A example shifts negative A left by 19 in one cycle");
			program.write_word(0x05e2, 0xf68f); // NORM B,A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffff001ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x210a0a0a0aULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xfff9); // TS=-7.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 716;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 716)
		{
			expect_opcode(0xf68f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0042141414ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x210a0a0a0aULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI NORM B,A example shifts guarded B right by seven");
			program.write_word(0x05e2, 0xf48e); // EXP A with zero source.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 717;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 717)
		{
			expect_opcode(0xf48e,
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"EXP zero source publishes zero exponent");
			program.write_word(0x05e2, 0xf48f); // NORM A with OVM.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x40000000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 718;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 718)
		{
			expect_opcode(0xf48f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"NORM left shift saturates on OVM and preserves carry in one cycle");
			program.write_word(0x05e2, 0xf59f); // RND A,B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0a00);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 719;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 719)
		{
			expect_opcode(0xf59f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffffULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fff &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0a00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI RND A,B example adds 8000h without changing status flags");
			program.write_word(0x05e2, 0xf49f); // RND A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 720;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 720)
		{
			expect_opcode(0xf49f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0a00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI RND A example saturates under OVM without changing flags");
			program.write_word(0x05e2, 0xf583); // SAT B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x7123456789ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // OVM clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 721;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 721)
		{
			expect_opcode(0xf583,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fffffff &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0a00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI SAT B example clamps positive guard overflow with OVM clear");
			program.write_word(0x05e2, 0xf483); // SAT A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xf812345678ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 722;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 722)
		{
			expect_opcode(0xf483,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI SAT A example clamps negative guard overflow and sets OVA");
			program.write_word(0x05e2, 0xf583); // SAT B without overflow.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x00123456);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0a00);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 723;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 723)
		{
			expect_opcode(0xf583,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00123456 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"TI SAT B in-range example clears OVB and preserves carry");
			program.write_word(0x05e2, 0x6283); // MPY *AR3,#4,A.
			program.write_word(0x05e3, 4);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0200, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0040); // FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 724;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 724)
		{
			expect_opcode(0x6283,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 24 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MPY Smem,#lk publishes T, doubles under FRCT and costs two cycles");
			program.write_word(0x05e2, 0xf166); // MPY #fffe,B.
			program.write_word(0x05e3, 0xfffe);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 725;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 725)
		{
			expect_opcode(0xf166,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffc000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x2000 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"TI MPY #fffe,B example uses signed immediate and keeps T");
			program.write_word(0x05e2, 0x62f8); // MPY *(0200),#5,A.
			program.write_word(0x05e3, 0x0200);
			program.write_word(0x05e4, 5);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 726;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 726)
		{
			expect_opcode(0x62f8,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 15 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"MPY absolute Smem consumes address before immediate and costs three cycles");
			program.write_word(0x05e2, 0x629b); // MPY *+AR3,#4,A.
			program.write_word(0x05e3, 4);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x01ff, 7);
			data.write_word(0x0200, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x01ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 727;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 727)
		{
			expect_opcode(0x629b,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 12 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MPY preincrements AR3 before reading Smem in two cycles");
			program.write_word(0x05e2, 0xf480); // ADD A,ASM,A.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffff00ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x011f); // SXM, ASM=-1.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 728;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 728)
		{
			expect_opcode(0xf480,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffe80ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ADD A,ASM sign-fills a right shift and records carry in one cycle");
			program.write_word(0x05e2, 0xf681); // SUB B,ASM,A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x40);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // SXM, ASM=1.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 729;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 729)
		{
			expect_opcode(0xf681,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffff90ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x40 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SUB B,ASM,A uses B as source and records borrow in one cycle");
			program.write_word(0x05e2, 0xf58c); // MPYA T,B.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0080000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 2);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 730;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 730)
		{
			expect_opcode(0xf58c,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0080000000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MPYA T uses signed 17-bit A high word in one cycle");
			program.write_word(0x05e2, 0x3183); // MPYA *AR3.
			data.write_word(0x0200, 2);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 731;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 731)
		{
			expect_opcode(0x3183,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MPYA Smem publishes T and uses the same signed 17-bit A high word");
			program.write_word(0x05e2, 0xf488); // MACA T,A,A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0080000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 732;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 732)
		{
			expect_opcode(0xf488,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0080008000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MACA T preserves old A as multiplicand and accumulator source");
			program.write_word(0x05e2, 0xf48b); // MASAR T,A,A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 733;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 733)
		{
			expect_opcode(0xf48b,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x20000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MASAR T subtracts a negative 17-bit product and rounds in one cycle");
			program.write_word(0x05e2, 0xe211); // SQDST *AR3,*AR3.
			data.write_word(0x0200, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0080010000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 734;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 734)
		{
			expect_opcode(0xe211,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x40010001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SQDST squares signed 17-bit A high word before replacing A");
			program.write_word(0x05e2, 0xf48d); // SQUR A,A.
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0080010000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 735;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 735)
		{
			expect_opcode(0xf48d,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x40010001 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"SQUR A uses the full signed 17-bit high word in one cycle");
			program.write_word(0x05e2, 0xf167); // MAC #0345h,A,B.
			program.write_word(0x05e3, 0x0345);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x0400);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0040); // FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 736;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 736)
		{
			expect_opcode(0xf167,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x001a3800 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0400 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MAC #lk uses T, FRCT and a separate source accumulator in two cycles");
			program.write_word(0x05e2, 0x6495); // MAC *AR5+,#1234h,A.
			program.write_word(0x05e3, 0x1234);
			data.write_word(0x0100, 0x5678);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1000);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 737;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 737)
		{
			expect_opcode(0x6495,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x06261060 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0101 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MAC Smem,#lk writes T and postincrements AR5 in two cycles");
			program.write_word(0x05e2, 0x65f8); // MAC *(0200h),#5,A,B.
			program.write_word(0x05e3, 0x0200);
			program.write_word(0x05e4, 5);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0200, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x100);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 738;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 738)
		{
			expect_opcode(0x65f8,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x100 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10f &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"MAC absolute Smem fetches address before immediate with cycle surcharge");
			program.write_word(0x05e2, 0x649b); // MAC *+AR3,#4,A.
			program.write_word(0x05e3, 4);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x01ff, 7);
			data.write_word(0x0200, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x01ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 739;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 739)
		{
			expect_opcode(0x649b,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1c &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MAC preincrements AR3 before reading Smem in two cycles");
			program.write_word(0x05e2, 0xf160); // ADD #ffffh,16,A,B.
			program.write_word(0x05e3, 0xffff);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x00010000);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 740;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 740)
		{
			expect_opcode(0xf160,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00010000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ADD #lk,16 sign-extends with SXM, preserves source and sets carry");
			program.write_word(0x05e2, 0xf361); // SUB #1,16,B,B.
			program.write_word(0x05e3, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x00020000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 741;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 741)
		{
			expect_opcode(0xf361,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00010000 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"SUB #lk,16 subtracts from B and records no borrow in two cycles");
			program.write_word(0x05e2, 0xf060); // ADD #ffffh,16,A,A with SXM clear.
			program.write_word(0x05e3, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 742;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 742)
		{
			expect_opcode(0xf060,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00ffff0000ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ADD #lk,16 zero-extends with SXM clear and records overflow");
			program.write_word(0x05e2, 0x65e3); // MAC *AR3(2),#4,A,B.
			program.write_word(0x05e3, 4);
			program.write_word(0x05e4, 2);
			program.write_word(0x05e5, 0x75d6);
			program.write_word(0x05e6, 0x0124);
			program.write_word(0x05e7, 0xf5e1);
			data.write_word(0x0202, 3);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 743;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 743)
		{
			expect_opcode(0x65e3,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1c &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"MAC long-offset fetches immediate then offset with a cycle surcharge");
			program.write_word(0x05e2, 0xf167); // MAC #8000h,A,B with FRCT and OVM.
			program.write_word(0x05e3, 0x8000);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0240); // OVM, FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 744;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 744)
		{
			expect_opcode(0xf167,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x007fffffffULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0200) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MAC immediate saturates a fractional product and records OVB");
			program.write_word(0x05e2, 0x4d83); // DELAY *AR3.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0200, 0x6cac);
			data.write_word(0x0201, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 745;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 745)
		{
			expect_opcode(0x4d83,
				data.read_word(0x0200) == 0x6cac &&
				data.read_word(0x0201) == 0x6cac &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"DELAY copies Smem to its successor without changing T or AR in one cycle");
			program.write_word(0x05e2, 0x4d9b); // DELAY *+AR3.
			data.write_word(0x01ff, 0x1111);
			data.write_word(0x0200, 0x2222);
			data.write_word(0x0201, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x01ff);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 746;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 746)
		{
			expect_opcode(0x4d9b,
				data.read_word(0x01ff) == 0x1111 &&
				data.read_word(0x0201) == 0x2222 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"DELAY preincrements AR3 before its source read");
			program.write_word(0x05e2, 0x4df8); // DELAY *(0200h).
			program.write_word(0x05e3, 0x0200);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0200, 0x55aa);
			data.write_word(0x0201, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 747;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 747)
		{
			expect_opcode(0x4df8,
				data.read_word(0x0200) == 0x55aa &&
				data.read_word(0x0201) == 0x55aa &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"DELAY absolute consumes its address extension and surcharge cycle");
			program.write_word(0x05e2, 0x4de3); // DELAY *AR3(2).
			program.write_word(0x05e3, 2);
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0202, 0xabcd);
			data.write_word(0x0203, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 748;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 748)
		{
			expect_opcode(0x4de3,
				data.read_word(0x0202) == 0xabcd &&
				data.read_word(0x0203) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"DELAY long-offset preserves AR3 and charges the extension cycle");
			program.write_word(0x05e2, 0x2483); // MPYU *AR3,A with FRCT and OVM.
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0200, 0xffff);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0240); // OVM, FRCT.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 749;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 749)
		{
			expect_opcode(0x2483,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x007fffffffULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xffff &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
					"MPYU doubles unsigned operands under FRCT and saturates with OVA");
			program.write_word(0x05e0, 0x75d6);
			program.write_word(0x05e1, 0x0124);
			program.write_word(0x05e2, 0xfe44); // RCD ANEQ
			program.write_word(0x05e3, 0xe800); // Delay word changes tested A.
			program.write_word(0x05e4, 0xe802);
			program.write_word(0x05e5, 0xe803); // Only the false path reaches this.
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			program.write_word(0x05ec, 0x75d6);
			program.write_word(0x05ed, 0x0124);
			program.write_word(0x05ee, 0xf5e1);
			data.write_word(0x0300, 0x05ec);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 750;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 750)
		{
			expect_opcode(0xfe44,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"RCD ANEQ captures the true condition before its two delay words");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 751;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 751)
		{
			expect_opcode(0xfe44,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"RCD ANEQ false path preserves the stack and falls through");
			// 0x60/0x61 belong to this fixture's interrupting read peripheral.
			program.write_word(0x05e2, 0x4a62); // PSHM MMR 0x62
			program.write_word(0x05e3, 0x4a63); // PSHM MMR 0x63
			program.write_word(0x05e4, 0x8a62); // POPM MMR 0x62
			program.write_word(0x05e5, 0x8a63); // POPM MMR 0x63
			program.write_word(0x05e6, 0x75d6);
			program.write_word(0x05e7, 0x0124);
			program.write_word(0x05e8, 0xf5e1);
			data.write_word(0x0062, 0x1234);
			data.write_word(0x0063, 0x5678);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 752;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 752)
		{
			expect_opcode(0x4a62,
				data.read_word(0x02ff) == 0x1234 && data.read_word(0x02fe) == 0x5678 &&
				data.read_word(0x0062) == 0x5678 && data.read_word(0x0063) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"PSHM/POPM decode all seven MMR address bits with balanced stack and one-cycle costs");
			expect_opcode(0x8a63, data.read_word(0x0063) == 0x1234,
				"POPM preserves the high MMR address bits");
			program.write_word(0x05e2, 0xfa43); // BCD ALT
			program.write_word(0x05e3, 0x05ec);
			program.write_word(0x05e4, 0xe801); // Changes A after the decision.
			program.write_word(0x05e5, 0xe802);
			program.write_word(0x05e6, 0xe803);
			program.write_word(0x05e7, 0x75d6);
			program.write_word(0x05e8, 0x0124);
			program.write_word(0x05e9, 0xf5e1);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 753;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 753)
		{
			expect_opcode(0xfa43,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"BCD ALT captures the signed condition before both delay words");
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 754;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 754)
		{
			expect_opcode(0xfa43,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
					"BCD ALT false path falls through with its three-cycle cost");
			program.write_word(0x05e2, 0xfa4f); // BCD BLEQ (SPRU172C condition 01001111)
			program.write_word(0x05e4, 0xe901); // Changes B after the decision.
			program.write_word(0x05e5, 0xe902);
			program.write_word(0x05e6, 0xe903);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 755;
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}
		if (m_phase == 755 || m_phase == 756 || m_phase == 757)
		{
			const bool taken = m_phase != 757;
			expect_opcode(0xfa4f,
				m_cpu->state_int(tms320c54x_device::STATE_B) == (taken ? 2 : 3) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 8),
				"BCD BLEQ tests signed 40-bit B including zero before delay slots");
			if (m_phase != 757)
			{
				m_port_writes = 0;
				m_cpu->set_state_int(tms320c54x_device::STATE_B, m_phase == 755 ? 0 : 1);
				m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
				++m_phase;
				m_check_timer->adjust(attotime::from_usec(100));
				return;
			}
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
		expect_opcode(0xe59c, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x089c &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x131a,
				"ROM4 MVDD receive-ring copy updates both operands after 26 checked words");

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
	unsigned m_bleq_case = 0;
	unsigned m_rom4_checks = 0;
	bool m_irq_raised = false;
	unsigned m_repeat_reads = 0;
	unsigned m_irq_trigger_read = 1;
	u64 m_irq_accumulator = 0;
	unsigned m_port_writes_at_irq = 0;
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

// Execute the stock flash-staged verifier without supplying a DSP mask ROM.
// This is a protocol investigation fixture, not an NSM-3 compatibility backend.
class nsm3_verifier_state : public driver_device
{
public:
	nsm3_verifier_state(const machine_config &config, device_type type, const char *tag) :
		driver_device(config, type, tag), m_cpu(*this, "maincpu"), m_cobba(*this, "cobba") { }
	void verifier(machine_config &config)
	{
		TMS320C54X(config, m_cpu, 13'000'000);
		NOKIA_COBBA(config, m_cobba, 0);
		m_cpu->set_addrmap(AS_PROGRAM, &nsm3_verifier_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &nsm3_verifier_state::data_map);
		m_cpu->set_addrmap(AS_IO, &nsm3_verifier_state::io_map);
	}
	void nse5_verifier(machine_config &config)
	{
		verifier(config);
		m_cpu->set_addrmap(AS_PROGRAM, &nsm3_verifier_state::nse5_program_map);
	}

private:
	bool npe3() const { return !strcmp(machine().system().name, "npe3verify"); }
	bool nhm3() const { return !strcmp(machine().system().name, "nhm3verify"); }
	bool nse5() const { return !strcmp(machine().system().name, "nse5verify"); }
	unsigned block_count() const { return nse5() ? 228 : (npe3() || nhm3()) ? 232 : 116; }
	void program_map(address_map &map)
	{
		map(0, 0xffff).ram();
		// ROM4's acquired PROM has the immutable version word here. This
		// fixture supplies an explicit version input, not missing ROM code.
		map(0xff87, 0xff87).rw(FUNC(nsm3_verifier_state::version_r), FUNC(nsm3_verifier_state::version_w));
	}
	void nse5_program_map(address_map &map)
	{
		map(0, 0x7fff).ram();
		map(0x8000, 0xffff).rom().region("mask", 0x10000);
	}
	u16 version_r() { return !strcmp(machine().options().bios(), "rom4") ? 4 : 6; }
	void version_w(u16 value)
	{
		logerror("nsm3_verifier: immutable_version_write=%04x\n", value);
	}
	void data_map(address_map &map) { map(0, 0xffff).ram(); }
	void io_map(address_map &map)
	{
		map(0, 0xffff).rw(FUNC(nsm3_verifier_state::io_r), FUNC(nsm3_verifier_state::io_w));
	}
	bool using_cobba() const
	{
		return !strcmp(machine().options().bios(), "cobba") ||
				!strcmp(machine().options().bios(), "cobba_alt") ||
				!strcmp(machine().options().bios(), "rom4");
	}
	u16 io_r(offs_t offset)
	{
		if (using_cobba() && offset == 0x2d)
		{
			u16 const value = m_cobba->control_data_r();
			logerror("nsm3_verifier: port_read=%04x data=%04x blocks=%u\n", u16(offset), value, m_block);
			return value;
		}
		throw emu_fatalerror(1, "NSM3 verifier requires peripheral read: port=%04x pc=%04x blocks=%u",
			u16(offset), u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), m_block);
	}
	void io_w(offs_t offset, u16 data)
	{
		logerror("nsm3_verifier: port_write=%04x data=%04x blocks=%u\n", u16(offset), data, m_block);
		if (using_cobba())
		{
			if (offset == 0x2c)
				m_cobba->control_select_w(data);
			else if (offset == 0x2d)
				m_cobba->control_data_w(data);
		}
	}
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(nsm3_verifier_state::step), this);
	}
	void machine_reset() override
	{
		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);
		for (unsigned i = 0; i != (nse5() ? 0x8000 : 0x10000); ++i)
			if (i != 0xff87)
				program.write_word(i, 0xffff);
		u16 const *const staged = &memregion("verifier")->as_u16();
		for (unsigned i = 0; i != (nse5() ? 210 : 223); ++i)
			program.write_word(0x0f00 + i, staged[i]);
		// Values written by the MCU at 0x2cac80..0x2cacee, not a mask-ROM snapshot.
		data.write_word(0x0800, 0);
		data.write_word(0x0801, 0xffff);
		data.write_word(0x0802, 0xffff);
		data.write_word(0x0803, 0xffff);
		data.write_word(0x087b, 0x0100);
		data.write_word(0x087c, 0x0300);
		data.write_word(0x087d, (npe3() || nhm3() || nse5()) ? 1 : 0);
		data.write_word(0x087e, nse5() ? 0xc800 : (npe3() || nhm3()) ? 0xd000 : 0xe800);
		data.write_word(0x087f, 1);
		data.write_word(0x0880, 1);
		data.write_word(0x0881, 0x0200);
		m_block = 0;
		m_ticks = 0;
		// Metamorphic peripheral input, not a proposed handset identity.
		if (!strcmp(machine().options().bios(), "cobba_alt"))
		{
			m_cobba->control_data_w(0x0016);
			m_cobba->control_select_w(0x000f);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0f00);
		m_timer->adjust(attotime::from_usec(1), 0, attotime::from_usec(1));
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		auto &data = m_cpu->space(AS_DATA);
		unsigned const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		if ((pc < 0x0f00 || pc >= (nse5() ? 0x0fd2 : 0x0fdf)) &&
				!(nse5() && pc >= 0x8000))
			throw emu_fatalerror(1, "NSM3 verifier escaped staged image: pc=%04x block=%u", pc, m_block);
		if (data.read_word(0x0801) != 0xffff)
			throw emu_fatalerror(0, "NSM3 verifier publication: blocks=%u word0=%04x word1=%04x word2=%04x word3=%04x pc=%04x fingerprint=%04x%04x pmst=%04x",
				m_block, data.read_word(0x0800), data.read_word(0x0801), data.read_word(0x0802), data.read_word(0x0803), pc,
				data.read_word(nse5() ? 0x1f0e : 0x04f7), data.read_word(nse5() ? 0x1f0f : 0x04f8), u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
		if (++m_ticks == 2000000)
			throw emu_fatalerror(1, "NSM3 verifier timeout: pc=%04x blocks=%u flags=%04x/%04x",
				pc, m_block, data.read_word(0x087f), data.read_word(0x0880));
		bool const second = BIT(m_block, 0);
		unsigned const flag = second ? 0x0880 : 0x087f;
		bool const polling = second ? (pc == 0x0f33 || pc == 0x0f35) : (pc == 0x0f17 || pc == 0x0f19);
		if (m_block == block_count() || !polling || data.read_word(flag) != 1)
			return;
		u8 const *const flash = memregion("flash")->base();
		unsigned const base = second ? 0x0b00 : 0x0900;
		for (unsigned i = 0; i != 512; ++i)
		{
			unsigned const offset = 0x40 + (m_block * 512 + i) * 0x20;
			u16 const value = (m_block == block_count() - 1 && i >= 510) ? 0xffff :
				(u16(flash[offset]) << 8) | flash[offset + 1];
			data.write_word(base + i, value);
		}
		data.write_word(flag, 0);
		logerror("nsm3_verifier: block=%u flag=%04x pc=%04x\n", m_block, flag, pc);
		++m_block;
	}
	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_cobba_device> m_cobba;
	emu_timer *m_timer = nullptr;
	unsigned m_block = 0;
	unsigned m_ticks = 0;
};

ROM_START(nsm3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "Compare existing COBBA register model (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture (not handset identity)")
	ROM_SYSTEM_BIOS(3, "rom4", "Immutable PROM version sensitivity fixture (not NSM-3 hardware)")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x1d0000, "flash", 0)
	ROM_LOAD("8210_5.31ppm_c.fls", 0, 0x1d0000,
		CRC(927022b1) SHA1(c1a0fe95cedb89a92b19654208cc4855e1a4988e))
ROM_END

// Same staged program and observed geometry, distinct stock 8250 flash input.
// Peripheral variants remain sensitivity fixtures, not fitted NSM-3D identity.
ROM_START(nsm3dverify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x1d0000, "flash", 0)
	ROM_LOAD("8250-502mcuppmk.fls", 0, 0x1d0000,
		CRC(2c58e48b) SHA1(f26c98ffcfffbbd5714889e10cfa41c5f6dd2529))
ROM_END

// NPE-3 additionally supplies a different block count and source extent.
ROM_START(npe3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x3a0000, "flash", 0)
	ROM_LOAD("6210_556c.fls", 0, 0x3a0000,
		CRC(203fb962) SHA1(3d9ea319503e78ec69b60d72cda23e461e118ea9))
ROM_END

// 6250's own staged bytes and sparse flash stream; no handset verdict supplied.
ROM_START(nhm3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x3a0000, "flash", 0)
	ROM_LOAD("6250-503mcuppmc.fls", 0, 0x3a0000,
		CRC(8dffb91b) SHA1(95607ce39c383bda75f1e6aeae67a214b787b0a1))
ROM_END

// Stock NSE-5 upload plus independently recovered ROM4 CRC routines. COBBA
// variants are explicit peripheral fixtures, not a measured 7110 publication.
ROM_START(nse5verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_REGION16_LE(420, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nse5_verifier.bin", 0, 420,
		CRC(e6c77fdc) SHA1(caca7599d9ca1a7dddf2df37f32be4aacd420deb))
	ROM_REGION(0x390000, "flash", 0)
	ROM_LOAD("7110f501_ppmc.fls", 0, 0x390000,
		CRC(919ac753) SHA1(53af8324919f455ba8199d2c05f7a921cfb811d5))
	ROM_REGION16_LE(0x20000, "mask", ROMREGION_ERASEFF)
	ROM_LOAD16_WORD_SWAP("dsp_full.bin", 0, 0x1fffe,
		CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739))
ROM_END

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
SYST(2026, nsm3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSM-3 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nsm3dverify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSM-3D stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, npe3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NPE-3 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nhm3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "6250 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nse5verify, 0, 0, nse5_verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSE-5 stock staged DSP verifier with ROM4 CRC routines",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, tms54rom4, 0, 0, rom4, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x ROM4 private execution fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
