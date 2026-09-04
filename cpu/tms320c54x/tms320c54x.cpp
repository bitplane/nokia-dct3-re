// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x.h"

namespace {

class tms320c54x_disassembler : public util::disasm_interface
{
public:
	virtual u32 opcode_alignment() const override { return 1; }

	virtual offs_t disassemble(std::ostream &stream, offs_t pc,
			const data_buffer &opcodes, const data_buffer &params) override
	{
		util::stream_format(stream, ".word   $%04X", opcodes.r16(pc));
		return 1 | SUPPORTED;
	}
};

} // anonymous namespace

DEFINE_DEVICE_TYPE(TMS320C54X, tms320c54x_device, "tms320c54x",
		"Texas Instruments TMS320C54x")

tms320c54x_device::tms320c54x_device(const machine_config &mconfig,
		const char *tag, device_t *owner, u32 clock) :
	cpu_device(mconfig, TMS320C54X, tag, owner, clock),
	m_program_config("program", ENDIANNESS_LITTLE, 16, 16, -1),
	m_data_config("data", ENDIANNESS_LITTLE, 16, 16, -1),
	m_io_config("io", ENDIANNESS_LITTLE, 16, 16, -1)
{
}

device_memory_interface::space_config_vector tms320c54x_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_PROGRAM, &m_program_config),
		std::make_pair(AS_DATA, &m_data_config),
		std::make_pair(AS_IO, &m_io_config)
	};
}

std::unique_ptr<util::disasm_interface> tms320c54x_device::create_disassembler()
{
	return std::make_unique<tms320c54x_disassembler>();
}

void tms320c54x_device::device_start()
{
	space(AS_PROGRAM).cache(m_cache);
	space(AS_PROGRAM).specific(m_program);
	space(AS_DATA).specific(m_data);
	space(AS_IO).specific(m_io);

	set_icountptr(m_icount);

	state_add(STATE_PC, "PC", m_pc).formatstr("%04X");
	state_add(STATE_A, "A", m_a).mask(ACC_MASK).formatstr("%010X");
	state_add(STATE_B, "B", m_b).mask(ACC_MASK).formatstr("%010X");
	state_add(STATE_T, "T", m_t).formatstr("%04X");
	state_add(STATE_SP, "SP", m_sp).formatstr("%04X");
	state_add(STATE_ST0, "ST0", m_st0).formatstr("%04X");
	state_add(STATE_ST1, "ST1", m_st1).formatstr("%04X");
	state_add(STATE_PMST, "PMST", m_pmst).formatstr("%04X");
	for (unsigned i = 0; i != std::size(m_ar); ++i)
		state_add(STATE_AR0 + i, string_format("AR%u", i).c_str(), m_ar[i]).formatstr("%04X");
	state_add(STATE_BRC, "BRC", m_brc).formatstr("%04X");
	state_add(STATE_BK, "BK", m_bk).formatstr("%04X");
	state_add(STATE_ILLEGAL, "ILLEGAL", m_illegal).formatstr("%1u");
	state_add(STATE_GENPC, "GENPC", m_pc).noshow();
	state_add(STATE_GENPCBASE, "CURPC", m_pc).noshow();

	save_item(NAME(m_pc));
	save_item(NAME(m_op));
	save_item(NAME(m_a));
	save_item(NAME(m_b));
	save_item(NAME(m_t));
	save_item(NAME(m_sp));
	save_item(NAME(m_st0));
	save_item(NAME(m_st1));
	save_item(NAME(m_pmst));
	save_item(NAME(m_ar));
	save_item(NAME(m_brc));
	save_item(NAME(m_bk));
	save_item(NAME(m_rsa));
	save_item(NAME(m_rea));
	save_item(NAME(m_rptc));
	save_item(NAME(m_rpt_address));
	save_item(NAME(m_ifr));
	save_item(NAME(m_imr));
	save_item(NAME(m_block_repeat_active));
	save_item(NAME(m_illegal));
}

void tms320c54x_device::device_reset()
{
	m_pc = 0xff80;
	m_op = 0;
	m_a = 0;
	m_b = 0;
	m_t = 0;
	m_sp = 0;
	m_st0 = 0;
	m_st1 = 0x2900;
	m_pmst = 0xffa0;
	std::fill(std::begin(m_ar), std::end(m_ar), 0);
	m_brc = 0;
	m_bk = 0;
	m_rsa = 0;
	m_rea = 0;
	m_rptc = 0;
	m_rpt_address = 0;
	m_ifr = 0;
	m_imr = 0;
	m_block_repeat_active = false;
	m_illegal = false;
}

u16 tms320c54x_device::fetch()
{
	return m_cache.read_word(m_pc++);
}

void tms320c54x_device::push(u16 value)
{
	// The C54x system stack grows downward; SP always names its top word.
	m_data.write_word(--m_sp, value);
}

u16 tms320c54x_device::pop()
{
	const u16 value = m_data.read_word(m_sp);
	++m_sp;
	return value;
}

u64 tms320c54x_device::data_operand(u16 value) const
{
	if (BIT(m_st1, 8) && BIT(value, 15))
		return u64(s64(s16(value))) & ACC_MASK;
	return value;
}

void tms320c54x_device::indirect_modify(u8 mode)
{
	const unsigned ar = mode & 7;
	switch (mode & 0x78)
	{
	case 0x08: --m_ar[ar]; break;
	case 0x10: ++m_ar[ar]; break;
	default: break;
	}
}

void tms320c54x_device::circular_modify(unsigned ar, s16 step)
{
	if (!m_bk)
		return;
	unsigned address_bits = 0;
	while ((1U << address_bits) <= m_bk && address_bits != 16)
		++address_bits;
	const u32 mask = address_bits == 16 ? 0xffff : (1U << address_bits) - 1;
	const u16 base = m_ar[ar] & ~mask;
	s32 index = (m_ar[ar] & mask) + step;
	while (index >= m_bk)
		index -= m_bk;
	while (index < 0)
		index += m_bk;
	m_ar[ar] = base | u16(index);
}

u16 tms320c54x_device::indirect_read(u8 mode)
{
	const unsigned ar = mode & 7;
	const u16 value = m_data.read_word(m_ar[ar]);
	indirect_modify(mode);
	return value;
}

void tms320c54x_device::indirect_write(u8 mode, u16 value)
{
	const unsigned ar = mode & 7;
	m_data.write_word(m_ar[ar], value);
	indirect_modify(mode);
}

void tms320c54x_device::finish_repeats()
{
	if (m_rptc && m_pc == u16(m_rpt_address + 1))
	{
		--m_rptc;
		m_pc = m_rpt_address;
	}

	// REA names the last program word in the block. This also covers a
	// multiword CALL at the end of the block after its RET restores REA+1.
	if (m_block_repeat_active && m_pc == u16(m_rea + 1))
	{
		if (m_brc)
		{
			--m_brc;
			m_pc = m_rsa;
		}
		else
			m_block_repeat_active = false;
	}
}

void tms320c54x_device::execute_one(u16 op)
{
	const u8 low = op;
	switch (op & 0xff00)
	{
	case 0x1000: // LD Smem, A
		m_a = data_operand(indirect_read(low));
		return;
	case 0x1100: // LD Smem, B
		m_b = data_operand(indirect_read(low));
		return;
	case 0x1a00: // OR Smem, A
		m_a = (m_a | indirect_read(low)) & ACC_MASK;
		return;
	case 0x1b00: // OR Smem, B
		m_b = (m_b | indirect_read(low)) & ACC_MASK;
		return;
	case 0x1c00: // XOR Smem, A
		m_a = (m_a ^ indirect_read(low)) & ACC_MASK;
		return;
	case 0x1d00: // XOR Smem, B
		m_b = (m_b ^ indirect_read(low)) & ACC_MASK;
		return;
	case 0x4400: // LD Smem, 16, A
		m_a = u64(indirect_read(low)) << 16;
		return;
	case 0x4500: // LD Smem, 16, B
		m_b = u64(indirect_read(low)) << 16;
		return;
	case 0x8000: // STL A, Smem
		indirect_write(low, u16(m_a));
		return;
	case 0x8100: // STL B, Smem
		indirect_write(low, u16(m_b));
		return;
	case 0x8200: // STH A, Smem
		indirect_write(low, u16(m_a >> 16));
		return;
	case 0x8300: // STH B, Smem
		indirect_write(low, u16(m_b >> 16));
		return;
	case 0x7600: // STM #lk, Smem
		indirect_write(low, fetch());
		return;
	case 0x7700: // STM #lk, MMR
	{
		const u16 value = fetch();
		const unsigned reg = low & 0x1f;
		if (reg >= 0x10 && reg <= 0x17)
			m_ar[reg - 0x10] = value;
		else if (reg == 0x19)
			m_bk = value;
		else if (reg == 0x1a)
			m_brc = value;
		else
			m_illegal = true;
		return;
	}
	case 0xe700: // MVDK source auxiliary register to destination MMR
	{
		const unsigned source = (low >> 4) & 7;
		const unsigned destination = low & 7;
		m_ar[destination] = m_ar[source];
		return;
	}
	default:
		break;
	}

	if (op == 0xe598) // MVDD *AR3+, *AR2+
	{
		m_data.write_word(m_ar[2]++, m_data.read_word(m_ar[3]++));
		return;
	}
	if (op == 0xe5a8) // MVDD *AR4+, *AR2+
	{
		m_data.write_word(m_ar[2]++, m_data.read_word(m_ar[4]++));
		return;
	}
	if (op == 0xe50b) // MVDD *AR2, *AR5+
	{
		m_data.write_word(m_ar[5]++, m_data.read_word(m_ar[2]));
		return;
	}
	if (op == 0xe5c9) // MVDD *AR2+0%, *AR3+
	{
		m_data.write_word(m_ar[3]++, m_data.read_word(m_ar[2]));
		circular_modify(2, s16(m_ar[0]));
		return;
	}
	if (op == 0xe58b) // MVDD *AR2+, *AR5+
	{
		m_data.write_word(m_ar[5]++, m_data.read_word(m_ar[2]++));
		return;
	}

	switch (op)
	{
	case 0xe800: // LD #0, A
		m_a = 0;
		return;
	case 0x61f8: // BITF dmad, #lk
	{
		const u16 address = fetch();
		const u16 mask = fetch();
		m_st0 = (m_st0 & ~0x1000) |
			((m_data.read_word(address) & mask) ? 0x1000 : 0);
		return;
	}
	case 0x70f8: // MVKD dmad, Smem (absolute destination form)
	{
		const u16 destination = fetch();
		const u16 source = fetch();
		m_data.write_word(destination, m_data.read_word(source));
		return;
	}
	case 0xfc00: // RET
		m_pc = pop();
		m_icount -= 4;
		return;
	case 0xfc30: // RETC TC
		if (m_st0 & 0x1000)
		{
			m_pc = pop();
			m_icount -= 4;
		}
		return;
	case 0xf072: // RPTB pmad
		m_rsa = u16(m_pc + 1);
		m_rea = fetch();
		m_block_repeat_active = true;
		m_icount -= 3;
		return;
	case 0xf074: // CALL pmad
	{
		const u16 destination = fetch();
		push(m_pc);
		m_pc = destination;
		m_icount -= 3;
		return;
	}
	case 0xf493: // NOT A
		m_a = ~m_a & ACC_MASK;
		return;
	case 0xf1c0: // XOR A, B
		m_b = (m_b ^ m_a) & ACC_MASK;
		return;
	default:
		if ((op & 0xfff8) == 0x4a10) // PSHM ARx
		{
			push(m_ar[low & 7]);
			return;
		}
		if ((op & 0xfff8) == 0x8a10) // POPM ARx
		{
			m_ar[low & 7] = pop();
			return;
		}
		if ((op & 0xff00) == 0xec00) // RPT #k
		{
			m_rptc = low;
			m_rpt_address = m_pc;
			return;
		}
		if ((op & 0xfff8) == 0x6d88) // MAR *ARx-
		{
			indirect_modify(low);
			return;
		}
		break;
	}

	logerror("%s: unimplemented C54x opcode %04x at %04x\n",
			machine().describe_context(), op, u16(m_pc - 1));
	m_illegal = true;
}

void tms320c54x_device::execute_run()
{
	while (m_icount > 0)
	{
		debugger_instruction_hook(m_pc);
		if (m_illegal)
		{
			m_icount = 0;
			break;
		}

		m_op = fetch();
		execute_one(m_op);
		if (!m_illegal)
			finish_repeats();
		--m_icount;
	}
}

void tms320c54x_device::execute_set_input(int inputnum, int state)
{
	if (inputnum >= 0 && inputnum < 16)
	{
		if (state == ASSERT_LINE)
			m_ifr |= u16(1U << inputnum);
		else if (state == CLEAR_LINE)
			m_ifr &= u16(~(1U << inputnum));
	}
}
