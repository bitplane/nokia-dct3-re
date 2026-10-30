// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

// Independently asserted instruction words reached by the ROM4 corpus; no acquired ROM is loaded.
bool tms320c54x_test_state::check_atomic_corpus_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 325)
	{
		program.write_word(0x05e3, 0xfa20); // BCD 05f0h, NTC
		program.write_word(0x05e4, 0x05f0);
		program.write_word(0x05e5, 0xe801); // Delay slot 1
		program.write_word(0x05e6, 0xe902); // Delay slot 2
		program.write_word(0x05e7, 0xe803); // Fall-through only
		write_words(program, {
			{0x05e8, 0x75f8}, {0x05e9, 0x0d00}, {0x05ea, 0x0124}, {0x05eb, 0xf5e1}, {0x05f0, 0x75f8}, {0x05f1, 0x0d00},
			{0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // NTC true
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(326);
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
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(327);
	}
	if (m_phase == 327)
	{
		expect_opcode(0xfa20, m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ec &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"BCD NTC false executes delay words and falls through");
		write_words(program, {
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
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
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234beef}, {cpu_regs::STATE_B, 0x5678cafe}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		++m_phase;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 332)
	{
		expect_opcode(0x8912,
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0xcafe &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"STLM B writes AR2 in one cycle");
		write_words(program, {
			{0x05e3, 0xec0c}, // RPT #12
			{0x05e4, 0x6d10}, // MAR *AR0+
			{0x05e5, 0xe80f}, // LD #15,A
			{0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(333);
	}
	if (m_phase == 333)
	{
		expect_opcode(0xec0c, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 13 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 18,
				"RPT #12 executes MAR thirteen times after one-cycle setup");
		expect_opcode(0xe80f, m_cpu->state_int(tms320c54x_device::STATE_A) == 15,
				"LD #15,A writes the accumulator after the repeated body");
		program.write_word(0x05e3, 0x13f8); // LDU *(0f20h),B
		write_words(program, {
			{0x05e4, 0x0f20}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		return resume_atomic_at(0x05e0, 334);
	}
	if (m_phase == 334)
	{
		expect_opcode(0x13f8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"absolute LDU B zero-extends even with SXM set in two cycles");
		write_words(program, {
			{0x05e3, 0xe800}, // LD #k8,A/B.
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x11111111}, {cpu_regs::STATE_B, 0x22222222}, {cpu_regs::STATE_ST0, 0x1800},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(8000);
	}
	if (m_phase >= 8000 && m_phase <= 9023)
	{
		const unsigned index = m_phase - 8000;
		const u16 opcode = 0xe800 | (index & 0x1ff);
		const bool b = BIT(index, 8);
		const u16 st1 = index >= 512 ? 0x0100 : 0;
		expect_opcode(opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (b ? 0x11111111 : index & 0xff) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (b ? index & 0xff : 0x22222222) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == st1 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"LD #k8 selects A/B, zero-extends all byte immediates with either SXM setting and preserves other accumulator/status in one cycle");
		if (index < 1023)
		{
			program.write_word(0x05e3, 0xe800 | ((index + 1) & 0x1ff));
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x11111111}, {cpu_regs::STATE_B, 0x22222222}, {cpu_regs::STATE_ST0, 0x1800},
				{cpu_regs::STATE_ST1, index + 1 >= 512 ? 0x0100 : 0}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x short immediate load conformance: PASS variants=512 sxm_settings=2\n");
		program.write_word(0x05e3, 0x81f8); // STL B,*(0f21h)
		write_words(program, {
			{0x05e4, 0x0f21}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f21, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234beef);
		return resume_atomic_at(0x05e0, 335);
	}
	if (m_phase == 335)
	{
		expect_opcode(0x81f8, data.read_word(0x0f21) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234beef &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"absolute STL B stores the low word in two cycles");
		write_words(program, {
			{0x05e3, 0x8192}, // STL B,*AR2+.
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0f24, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f24);
		return resume_atomic_at(0x05e0, 6538);
	}
	if (m_phase == 6538)
	{
		expect_opcode(0x8192,
			data.read_word(0x0f24) == 0xbeef &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234beef &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f25 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 STL B stores the low word before one AR2 increment and preserves B in one cycle");
		program.write_word(0x05e3, 0x8190); // STL B,*AR0+.
		data.write_word(0x0f26, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR0, 0x0f26}, {cpu_regs::STATE_AR1, 0x0f28},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6540);
	}
	if ((m_phase >= 6540 && m_phase <= 6542) || m_phase == native_store_phase)
	{
		const unsigned variant = m_phase == native_store_phase ? 3 : m_phase - 6540;
		const u16 opcode = variant == 0 ? 0x8190 : variant == 1 ? 0x8191 : variant == 2 ? 0x8090 : 0x8194;
		const u16 destination = variant == 1 ? 0x0f28 : variant == 3 ? 0x0f2a : 0x0f26;
		expect_opcode(opcode,
			data.read_word(destination) == (variant == 2 ? 0x5678 : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234beef &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == (variant == 1 || variant == 3 ? 0x0f26 : 0x0f27) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == (variant == 1 ? 0x0f29 : 0x0f28) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f25 &&
			(variant != 3 || (m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f2b &&
				data.read_word(0x0f2b) == 0xcafe && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800)) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 STL selects A/B and AR0/AR1/AR4 independently, postincrements only its destination and preserves accumulators/status in one cycle");
		if (variant < 3)
		{
			program.write_word(0x05e3, variant == 0 ? 0x8191 : variant == 1 ? 0x8090 : 0x8194);
			data.write_word(0x0f26, 0);
			data.write_word(0x0f28, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0f26);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f28);
			if (variant == 2)
			{
				m_native_store_context = {u16(m_cpu->state_int(tms320c54x_device::STATE_AR4)),
					u16(m_cpu->state_int(tms320c54x_device::STATE_ST0)), u16(m_cpu->state_int(tms320c54x_device::STATE_ST1))};
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f2a);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
				data.write_word(0x0f2a, 0);
				data.write_word(0x0f2b, 0xcafe);
			}
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(variant == 2 ? native_store_phase : m_phase + 1);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_AR4, m_native_store_context[0]);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, m_native_store_context[1]);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_native_store_context[2]);
		program.write_word(0x05e3, 0x7582); // PORTW *AR2,0124h
		write_words(program, {
			{0x05e4, 0x0124}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 0xface);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
		return resume_atomic_at(0x05e0, 336);
	}
	if (m_phase == 336)
	{
		expect_opcode(0x7582, m_middle_port_value == 0xface &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
				"PORTW *AR2 emits memory word without modifying AR2 in two cycles");
		program.write_word(0x05e3, 0x7583); // PORTW *AR3,0124h.
		data.write_word(0x0f22, 0xbeef);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f22);
		return resume_atomic_at(0x05e0, 6536);
	}
	if (m_phase == 6536)
	{
		expect_opcode(0x7583, m_middle_port_value == 0xbeef &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
			data.read_word(0x0f22) == 0xbeef &&
			m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 PORTW selects AR3 rather than AR2, preserves source/pointers and consumes the port extension in two cycles");
		program.write_word(0x05e3, 0x7595); // PORTW *AR5+,0124h.
		data.write_word(0x0f26, 0xcafe);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f26);
		return resume_atomic_at(0x05e0, 6551);
	}
	if (m_phase == 6551 || m_phase == 6552)
	{
		const bool ar2 = m_phase == 6552;
		expect_opcode(ar2 ? 0x7592 : 0x7595,
			m_middle_port_value == (ar2 ? 0xface : 0xcafe) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (ar2 ? 0x0f29 : 0x0f20) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f27 &&
			data.read_word(ar2 ? 0x0f28 : 0x0f26) == (ar2 ? 0xface : 0xcafe) &&
			m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 PORTW selects AR2/AR5 independently, emits the pre-increment word, preserves source memory and increments once in two cycles");
		if (!ar2)
		{
			program.write_word(0x05e3, 0x7592); // PORTW *AR2+,0124h.
			data.write_word(0x0f28, 0xface);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f28);
			return resume_atomic_at(0x05e0, 6552);
		}
		program.write_word(0x05e3, 0x758a); // PORTW *AR2-,0124h.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f28);
		return resume_atomic_at(0x05e0, 6571);
	}
	if (m_phase == 6571)
	{
		expect_opcode(0x758a,
			m_middle_port_value == 0xface && data.read_word(0x0f28) == 0xface &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f27 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f27 &&
			m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 PORTW emits old AR2 memory before one decrement, preserves source/unrelated pointers and costs two cycles");
	}
	if (m_phase == 340)
	{
		program.write_word(0x05e3, 0xfa4d); // BCD 05f0h, BEQ
		program.write_word(0x05e4, 0x05f0);
		program.write_word(0x05e5, 0xe801); // Delay slot 1
		program.write_word(0x05e6, 0xe902); // Delay slot 2 changes B
		program.write_word(0x05e7, 0xe803); // Fall-through only
		write_words(program, {
			{0x05e8, 0x75f8}, {0x05e9, 0x0d00}, {0x05ea, 0x0124}, {0x05eb, 0xf5e1}, {0x05f0, 0x75f8}, {0x05f1, 0x0d00},
			{0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(341);
	}
	if (m_phase == 341)
	{
		expect_opcode(0xfa4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"BCD BEQ uses pre-slot B and executes both delay words");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(342);
	}
	if (m_phase == 342)
	{
		expect_opcode(0xfa4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ec &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"BCD BEQ false executes slots and falls through");
		write_words(program, {
			{0x05e3, 0xfc4d}, // RC BEQ
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x02ff, 0x05f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(343);
	}
	if (m_phase == 343)
	{
		expect_opcode(0xfc4d, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"RC BEQ pops return address in five cycles when B is zero");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(344);
	}
	if (m_phase == 344)
	{
		expect_opcode(0xfc4d, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"RC BEQ leaves stack untouched in three cycles when B is nonzero");
		write_words(program, {
			{0x05e3, 0x47e2}, // RPT *AR2(5)
			{0x05e4, 5},
			{0x05e5, 0x6d10}, // MAR *AR0+
			{0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		data.write_word(0x0f05, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(345);
	}
	if (m_phase == 345)
	{
		expect_opcode(0x47e2, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
				"long-offset RPT Smem runs MAR three times after four-cycle setup");
	}
	if (m_phase == 354)
	{
		write_words(program, {
			{0x05e3, 0xf84e}, // BC 05f0,BGT
			{0x05e4, 0x05f0},
			{0x05e5, 0x75f8}, // Fallthrough marker.
			{0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
			{0x05f0, 0x75f8}, // Taken marker.
			{0x05f1, 0x0d00}, {0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
		return resume_atomic_at(0x05e0, 355);
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
			return resume_atomic_at(0x05e0, 356);
		}
		write_words(program, {
			{0x05e3, 0xec01}, // RPT #1
			{0x05e4, 0x6d10}, // MAR *AR0+
			{0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0f00);
		return resume_atomic_at(0x05e0, 357);
	}
	if (m_phase == 357)
	{
		expect_opcode(0xec01, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f02 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 RPT #1 executes MAR twice after one-cycle setup");
		write_words(program, {
			{0x05e3, 0xfa30}, // BCD 05f0,TC
			{0x05e4, 0x05f0},
			{0x05e5, 0xe801}, // Delay slot 1
			{0x05e6, 0xe902}, // Delay slot 2
			{0x05e7, 0xe803}, // Fallthrough only
			{0x05e8, 0x75f8}, {0x05e9, 0x0d00}, {0x05ea, 0x0124}, {0x05eb, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // TC true
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(358);
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
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(359);
		}
	}
	if (m_phase == 361)
	{
		program.write_word(0x05e3, 0x75ea); // PORTW *+AR2(5),0124
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 5);
		data.write_word(0x0f05, 0xabcd);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 362);
	}
	if (m_phase == 362)
	{
		expect_opcode(0x75ea, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 3 && m_middle_port_value == 0xabcd &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"long-offset PORTW consumes port before offset and takes three cycles");
	}
	if (m_phase == 363)
	{
		program.write_word(0x05e3, 0x7dea); // MVDP *+AR2(5),0600
		program.write_word(0x05e4, 0x0600);
		program.write_word(0x05e5, 5);
		program.write_word(0x0600, 0);
		data.write_word(0x0f05, 0xbeef);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 364);
	}
	if (m_phase == 364)
	{
		expect_opcode(0x7dea, program.read_word(0x0600) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"long-offset MVDP consumes pmad before offset and takes five cycles");
		program.write_word(0x05e3, 0x7fea); // WRITA *+AR2(5)
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1}, {0x0600, 0},
		});
		data.write_word(0x0f05, 0x1234);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0600}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(365);
	}
	if (m_phase == 365)
	{
		expect_opcode(0x7fea, program.read_word(0x0600) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"long-offset WRITA stores the data word at A in six cycles");
	}
	if (m_phase == 367)
	{
		program.write_word(0x05e3, 0x74ea); // PORTR 0123,*+AR2(5)
		program.write_word(0x05e4, 0x0123);
		program.write_word(0x05e5, 5);
		data.write_word(0x0f05, 0);
		m_port_reads = 0;
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 368);
	}
	if (m_phase == 368)
	{
		expect_opcode(0x74ea, data.read_word(0x0f05) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_reads == 1 && m_port_writes == 2 &&
				m_middle_port_cycle - m_first_port_cycle == 3,
				"long-offset PORTR consumes port before offset and takes three cycles");
	}
	if (m_phase == 373)
	{
		program.write_word(0x05e3, 0x1080); // LD *AR0,A.
		data.write_word(0x0f22, 0x8002);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0f22}, {cpu_regs::STATE_B, 0x87654321}, {cpu_regs::STATE_T, 0x1234},
			{cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6591);
	}
	if (m_phase == 6591 || m_phase == 6592)
	{
		const bool signed_operand = m_phase == 6591;
		expect_opcode(0x1080,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (signed_operand ? 0xffffff8002ULL : 0x8002) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f20 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x87654321 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (signed_operand ? 0x0100 : 0) &&
			data.read_word(0x0f22) == 0x8002 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 stationary AR0 load selects its source, obeys SXM and preserves pointers/B/T/status in one cycle");
		if (signed_operand)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			return resume_atomic_at(0x05e0, 6592);
		}
	}
	if (m_phase == 376)
	{
		write_words(program, {
			{0x05e3, 0xf517}, // ADD A >> 9,B
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 3);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
		return resume_atomic_at(0x05e0, 377);
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
			return resume_atomic_at(0x05e0, 378);
		}
		write_words(program, {
			{0x05e3, 0xf4e2}, // BACC A
			{0x05f0, 0x75f8}, {0x05f1, 0x0d00}, {0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x05f0}, {cpu_regs::STATE_B, 0x05e4}, {cpu_regs::STATE_SP, 0x0300},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(379);
	}
	if (m_phase == 379)
	{
		expect_opcode(0xf4e2, m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"ROM4 BACC A branches to A's low word without a stack push in six cycles");
		write_words(program, {
			{0x05e3, 0xf474}, // SFTA A,-12
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000800ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
		return resume_atomic_at(0x05e0, 380);
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
			return resume_atomic_at(0x05e0, 381);
		}
		program.write_word(0x05e3, 0x7722); // STM #lk, MMR 22h
		write_words(program, {
			{0x05e4, 0xa55a}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0022, 0);
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 382);
	}
	if (m_phase == 382)
	{
		expect_opcode(0x7722, data.read_word(0x0022) == 0xa55a &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 STM #lk,MMR22 includes the peripheral access cycle");
		write_words(program, {
			{0x05e3, 0xf537}, // SUB A >> 9,B
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
		return resume_atomic_at(0x05e0, 383);
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
			return resume_atomic_at(0x05e0, 384);
		}
		program.write_word(0x05e3, 0x6184); // BITF *AR4,#00f0
		write_words(program, {
			{0x05e4, 0x00f0}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 0x00a0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f20}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(385);
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
			return resume_atomic_at(0x05e0, 386);
		}
	}
	if (m_phase == 422)
	{
		write_words(program, {
			{0x05e3, 0xff4d}, // XC 2,BEQ
			{0x05e4, 0xe801}, // LD #1,A
			{0x05e5, 0xe902}, // LD #2,B
			{0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(424);
	}
	if (m_phase == 424)
	{
		expect_opcode(0xff4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 XC 2,BEQ executes two words after a one-cycle condition test");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(425);
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
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0061}, {cpu_regs::STATE_SP, 0x02ff},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 4}, {cpu_regs::STATE_IFR, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(426);
	}
	if (m_phase == 426)
	{
		expect_opcode(0xff4d, m_repeat_reads == 1 && m_irq_accumulator == 1 &&
				m_port_writes_at_irq == 1 && m_port_writes == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 1,
				"XC 2 releases a pending interrupt after one two-word instruction");
		m_cpu->set_input_line(2, CLEAR_LINE);
	}
	return false;
}

} // namespace c54x_test
