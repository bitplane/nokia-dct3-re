// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_far_save_case(bool returning)
{
	m_far_save_return = returning;
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	const unsigned origin = returning ? 0x030600 : 0x010600;
	unsigned next = origin;
	program.write_word(next++, returning ? 0xf6e4 : 0xfb82);
	if (!returning)
		program.write_word(next++, 0x0700);
	program.write_word(next++, 0x0082); // First delay word schedules a zero-time checkpoint.
	program.write_word(next++, 0xe905);
	program.write_word(next, 0xf4e1);
	program.write_word(0x020700, 0xe807);
	program.write_word(0x020701, 0xf4e4);
	program.write_word(0x000700, 0xe80f); // Wrong-page sentinels.
	program.write_word(0x000701, 0xf4e4);
	program.write_word(0x010700, 0xe80e);
	program.write_word(0x010701, 0xf4e4);
	if (returning)
	{
		program.write_word(0x010604, 0xe807);
		program.write_word(0x010605, 0xf4e1);
	}
	data.write_word(0x03fe, 1);
	data.write_word(0x03ff, 0x0604);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_SP, returning ? 0x03fe : 0x0400}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0},
		{cpu_regs::STATE_AR2, 0x0060}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_ST1, 0x0800}, {STATE_GENPC, origin},
		{cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 784;
	m_check_timer->adjust(attotime::from_usec(100));
}

bool tms320c54x_test_state::check_far_save_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 786 || m_phase == 787)
	{
		const unsigned continuation = m_far_save_return ? 0x010606 : 0x010605;
		expect(m_cpu->state_int(STATE_GENPC) == continuation &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 7 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 5 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"uninterrupted and restored pending far transfer complete identically");
		if (m_phase == 786)
		{
			set_registers(*m_cpu, {
				{STATE_GENPC, 0x7f0900}, {cpu_regs::STATE_SP, 0x0500}, {cpu_regs::STATE_A, 99}, {cpu_regs::STATE_B, 88},
			});
			data.write_word(0x03fe, 0x7f);
			data.write_word(0x03ff, 0xbad);
			m_saved_repeat.clear();
			m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE,
				"restore pending far-transfer save state");
			expect(m_cpu->state_int(STATE_GENPC) == (m_far_save_return ? 0x030602 : 0x010603) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
				data.read_word(0x03fe) == 1 && data.read_word(0x03ff) == 0x0604,
				"restore exact extended checkpoint registers and stack RAM");
			m_phase = 787;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (!m_far_save_return)
		{
			start_far_save_case(true);
			return true;
		}
		osd_printf_info("TMS320C54x extended save replay: PASS pending_call pending_return xpc stack delay\n");
		program.write_word(0x010800, 0xec02); // RPT #2
		program.write_word(0x010801, 0x7f90); // WRITA *AR0+
		program.write_word(0x010802, 0xec02);
		program.write_word(0x010803, 0x7e90); // READA *AR0+
		program.write_word(0x010804, 0xf4e1);
		data.write_word(0x0500, 0x1234);
		data.write_word(0x0501, 0x5678);
		data.write_word(0x0502, 0x9abc);
		for (unsigned address = 0x0503; address != 0x0506; ++address)
			data.write_word(address, 0);
		program.write_word(0x00fffe, 0xaaaa);
		program.write_word(0x00ffff, 0xbbbb);
		program.write_word(0x000000, 0xcccc);
		program.write_word(0x020000, 0xdddd);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0500}, {cpu_regs::STATE_A, 0x02fffe}, {STATE_GENPC, 0x010800},
			{cpu_regs::STATE_IDLE, 0},
		});
		m_phase = 788;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 788)
	{
		expect_opcode(0x7f90,
			program.read_word(0x02fffe) == 0x1234 &&
			program.read_word(0x02ffff) == 0x5678 &&
			program.read_word(0x030000) == 0x9abc &&
			data.read_word(0x0503) == 0x1234 &&
			data.read_word(0x0504) == 0x5678 &&
			data.read_word(0x0505) == 0x9abc,
			"repeated extended WRITA/READA cross a page boundary without truncating PAR");
		expect(program.read_word(0x00fffe) == 0xaaaa &&
			program.read_word(0x00ffff) == 0xbbbb &&
			program.read_word(0x000000) == 0xcccc &&
			program.read_word(0x020000) == 0xdddd &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x02fffe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0506 &&
			m_cpu->state_int(STATE_GENPC) == 0x010805 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"extended repeated program transfer retains A, executing XPC and wrong-page sentinels");
		osd_printf_info("TMS320C54x extended program conformance: PASS\n");
		program.write_word(0x0600, 0xf495); // NOP in the common window.
		program.write_word(0x0601, 0x7f90); // WRITA *AR0+
		program.write_word(0x0602, 0x7e90); // READA *AR0+
		program.write_word(0x0603, 0xf882); // FB 02:8000 (unique upper window).
		program.write_word(0x0604, 0x8000);
		program.write_word(0x028000, 0xe909);
		program.write_word(0x028001, 0xf4e1);
		program.write_word(0x020600, 0xf4e1); // Wrong-page fetch sentinel.
		program.write_word(0x020007, 0xaaaa);
		data.write_word(0x0500, 0x1234);
		data.write_word(0x0501, 0);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0500}, {cpu_regs::STATE_PMST, 0x0020}, {cpu_regs::STATE_A, 0x020007},
			{STATE_GENPC, 0x020600}, {cpu_regs::STATE_IDLE, 0},
		});
		m_phase = 60000;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 60000)
	{
		expect(program.read_word(7) == 0x1234 && program.read_word(0x020007) == 0xaaaa &&
			data.read_word(0x0501) == 0x1234 && m_cpu->state_int(tms320c54x_device::STATE_B) == 9 &&
			m_cpu->state_int(STATE_GENPC) == 0x028002 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"OVLY folds CPU fetch and accumulator transfers while preserving logical XPC and upper pages");
		osd_printf_info("TMS320C54x program overlay conformance: PASS\n");
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
		program.write_word(0x0600, 0xf4a0);
		program.write_word(0x0601, 0xf4e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1abc);
		m_cpu->set_state_int(STATE_GENPC, 0x0600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 60001;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase >= 60001 && m_phase <= 60008)
	{
		unsigned const arp = m_phase - 60001;
		expect_opcode(0xf4a0 | arp,
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x1abc | (arp << 13)) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"LD immediate ARP selects all eight pointers and preserves unrelated ST0 fields");
		if (arp != 7)
		{
			program.write_word(0x0600, 0xf4a0 | (arp + 1));
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1abc);
			m_cpu->set_state_int(STATE_GENPC, 0x0600);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x immediate ARP conformance: PASS variants=8\n");
		program.write_word(0x0600, 0x46f8);
		program.write_word(0x0601, 0x0500);
		program.write_word(0x0602, 0xf4e1);
		data.write_word(0x0500, 0xffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xfe00);
		m_cpu->set_state_int(STATE_GENPC, 0x0600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 60010;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase >= 60010 && m_phase <= 60012)
	{
		u16 const values[] = {0xffff, 0x0200, 0x0155};
		unsigned const index = m_phase - 60010;
		expect_opcode(0x46f8,
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xfe00 | (values[index] & 0x01ff)) &&
			m_cpu->state_int(STATE_GENPC) == 0x0603 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"LD absolute Smem,DP consumes its address extension and preserves unrelated ST0 fields");
		if (index != 2)
		{
			data.write_word(0x0500, values[index + 1]);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xfe00);
			m_cpu->set_state_int(STATE_GENPC, 0x0600);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x memory DP conformance: PASS variants=3\n");
		program.write_word(0x0600, 0x75d6);
		program.write_word(0x0601, 0x0124);
		program.write_word(0x0602, 0xfa47);
		program.write_word(0x0603, 0x060c);
		program.write_word(0x0604, 0xe801); // Changes A after the branch predicate is captured.
		write_words(program, {
			{0x0605, 0xe902}, {0x0606, 0xe903}, {0x0607, 0x75d6}, {0x0608, 0x0124}, {0x0609, 0xf4e1}, {0x060c, 0x75d6},
			{0x060d, 0x0124}, {0x060e, 0xf4e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x8000000000ULL);
		m_cpu->set_state_int(STATE_GENPC, 0x0600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 60020;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	return false;
}

} // namespace c54x_test
