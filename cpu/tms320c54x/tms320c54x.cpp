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
	state_add(STATE_IFR, "IFR", m_ifr).formatstr("%04X");
	state_add(STATE_IMR, "IMR", m_imr).formatstr("%04X");
	state_add(STATE_IDLE, "IDLE", m_idle).formatstr("%1u");
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
	save_item(NAME(m_delayed_target));
	save_item(NAME(m_delayed_words));
	save_item(NAME(m_ifr));
	save_item(NAME(m_imr));
	save_item(NAME(m_block_repeat_active));
	save_item(NAME(m_idle));
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
	m_delayed_target = 0;
	m_delayed_words = 0;
	m_ifr = 0;
	m_imr = 0;
	m_block_repeat_active = false;
	m_idle = false;
	m_illegal = false;
}

u16 tms320c54x_device::fetch()
{
	return m_cache.read_word(m_pc++);
}

u16 tms320c54x_device::data_read(u16 address)
{
	if (address == 0x00)
		return m_imr;
	if (address == 0x01)
		return m_ifr;
	if (address == 0x06)
		return m_st0;
	if (address == 0x07)
		return m_st1;
	if (address == 0x08)
		return u16(m_a);
	if (address == 0x09)
		return u16(m_a >> 16);
	if (address == 0x0a)
		return u16(m_a >> 32);
	if (address == 0x0b)
		return u16(m_b);
	if (address == 0x0c)
		return u16(m_b >> 16);
	if (address == 0x0d)
		return u16(m_b >> 32);
	if (address == 0x0e)
		return m_t;
	if (address >= 0x10 && address <= 0x17)
		return m_ar[address - 0x10];
	if (address == 0x18)
		return m_sp;
	if (address == 0x19)
		return m_bk;
	if (address == 0x1a)
		return m_brc;
	if (address == 0x1d)
		return m_pmst;
	return m_data.read_word(address);
}

void tms320c54x_device::data_write(u16 address, u16 value)
{
	if (address == 0x00)
		m_imr = value;
	else if (address == 0x01)
		m_ifr &= ~value;
	else if (address == 0x06)
		m_st0 = value;
	else if (address == 0x07)
		m_st1 = value;
	else if (address == 0x08)
		m_a = (m_a & ~u64(0xffff)) | value;
	else if (address == 0x09)
		m_a = (m_a & ~u64(0xffff0000)) | (u64(value) << 16);
	else if (address == 0x0a)
		m_a = (m_a & ~u64(0xff00000000)) | (u64(value & 0xff) << 32);
	else if (address == 0x0b)
		m_b = (m_b & ~u64(0xffff)) | value;
	else if (address == 0x0c)
		m_b = (m_b & ~u64(0xffff0000)) | (u64(value) << 16);
	else if (address == 0x0d)
		m_b = (m_b & ~u64(0xff00000000)) | (u64(value & 0xff) << 32);
	else if (address == 0x0e)
		m_t = value;
	else if (address >= 0x10 && address <= 0x17)
		m_ar[address - 0x10] = value;
	else if (address == 0x18)
		m_sp = value;
	else if (address == 0x19)
		m_bk = value;
	else if (address == 0x1a)
		m_brc = value;
	else if (address == 0x1d)
		m_pmst = value;
	else
		m_data.write_word(address, value);
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

u64 tms320c54x_device::arithmetic_shift_right(u64 value, unsigned shift) const
{
	const s64 signed_value = s64(value << 24) >> 24;
	return u64(signed_value >> shift) & ACC_MASK;
}

void tms320c54x_device::indirect_modify(u8 mode)
{
	const unsigned ar = mode & 7;
	switch (mode & 0x78)
	{
	case 0x08: --m_ar[ar]; break;
	case 0x10: ++m_ar[ar]; break;
	case 0x30: m_ar[ar] += m_ar[0]; break;
	case 0x38: m_ar[ar] -= m_ar[0]; break;
	case 0x48: circular_modify(ar, -1); break;
	case 0x50: circular_modify(ar, 1); break;
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
	if (mode == 0xf8)
		return data_read(fetch());
	const unsigned ar = mode & 7;
	const u16 value = data_read(m_ar[ar]);
	indirect_modify(mode);
	return value;
}

void tms320c54x_device::indirect_write(u8 mode, u16 value)
{
	if (mode == 0xf8)
	{
		data_write(fetch(), value);
		return;
	}
	const unsigned ar = mode & 7;
	data_write(m_ar[ar], value);
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

bool tms320c54x_device::service_interrupt()
{
	// Maskable hardware sources occupy vectors 16..31 and map directly to
	// IMR/IFR bits 0..15. PMST.IPTR selects their 128-word vector page.
	const u16 pending = m_ifr & m_imr;
	if (BIT(m_st1, 11) || !pending)
		return false;

	unsigned source = 0;
	while (!BIT(pending, source))
		++source;
	m_ifr &= ~(u16(1) << source);
	push(m_pc);
	m_st1 |= 0x0800;
	m_pc = (m_pmst & 0xff80) | ((source + 16) << 2);
	m_idle = false;
	m_icount -= 5;
	return true;
}

void tms320c54x_device::execute_one(u16 op)
{
	const u8 low = op;
	if (op == 0x6ff8 && m_cache.read_word(m_pc + 1) == 0x0e20)
	{
		const u16 address = fetch();
		fetch();
		m_a = (m_b - data_operand(data_read(address))) & ACC_MASK;
		return;
	}
	if ((op == 0x6f92 || op == 0x6f8a || op == 0x6f93 || op == 0x6f8b) &&
			(m_cache.read_word(m_pc) == 0x0d96 || m_cache.read_word(m_pc) == 0x0c96 ||
			 m_cache.read_word(m_pc) == 0x0d91 || m_cache.read_word(m_pc) == 0x0c91))
	{
		const u16 extension = fetch();
		const int shift = s8((extension & 0x1f) << 3) >> 3;
		const u64 value = BIT(extension, 8) ? m_b : m_a;
		const u16 result = shift < 0
				? u16(arithmetic_shift_right(value, -shift))
				: u16((value << shift) & ACC_MASK);
		indirect_write(low, result);
		return;
	}
	if (op == 0x70f8) // MVKD dmad, Smem (absolute destination form)
	{
		const u16 destination = fetch();
		const u16 source = fetch();
		data_write(destination, data_read(source));
		return;
	}
	switch (op & 0xff00)
	{
	case 0x1000: // LD Smem, A
		m_a = data_operand(indirect_read(low));
		return;
	case 0x1100: // LD Smem, B
		m_b = data_operand(indirect_read(low));
		return;
	case 0x1200: // LD uns(Smem), A
		m_a = indirect_read(low);
		return;
	case 0x1300: // LD uns(Smem), B
		m_b = indirect_read(low);
		return;
	case 0x0200: // ADD uns(Smem), A
		m_a = (m_a + indirect_read(low)) & ACC_MASK;
		return;
	case 0x0800: // SUB Smem, A
		m_a = (m_a - data_operand(indirect_read(low))) & ACC_MASK;
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
	case 0x6100: // BITF Smem, #lk
	{
		const u16 value = indirect_read(low);
		const u16 mask = fetch();
		m_st0 = (m_st0 & ~0x1000) | ((value & mask) ? 0x1000 : 0);
		return;
	}
	case 0x4800: // LDM MMR, A
		m_a = data_operand(data_read(low & 0x1f));
		return;
	case 0x4900: // LDM MMR, B
		m_b = data_operand(data_read(low & 0x1f));
		return;
	case 0x6800: // AND #lk, Smem
	case 0x6900: // OR #lk, Smem
	{
		const unsigned ar = low & 7;
		const u16 address = low == 0xf8 ? fetch() : m_ar[ar];
		const u16 immediate = fetch();
		const u16 value = data_read(address);
		data_write(address, (op & 0xff00) == 0x6800
				? value & immediate : value | immediate);
		if (low != 0xf8)
			indirect_modify(low);
		return;
	}
	case 0x6b00: // ADD #lk, Smem
	{
		const unsigned ar = low & 7;
		const u16 address = low == 0xf8 ? fetch() : m_ar[ar];
		const u16 immediate = fetch();
		data_write(address, data_read(address) + immediate);
		if (low != 0xf8)
			indirect_modify(low);
		return;
	}
	case 0x6c00: // BANZ pmad, *ARx modification
	{
		const unsigned ar = low & 7;
		const bool branch = m_ar[ar] != 0;
		indirect_modify(low);
		const u16 destination = fetch();
		if (branch)
			m_pc = destination;
		return;
	}
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
	case 0xe800: // LD #k, A
		m_a = data_operand(low);
		return;
	case 0xe900: // LD #k, B
		m_b = data_operand(low);
		return;
	case 0x8800: // STLM A, MMR
		data_write(low & 0x1f, u16(m_a));
		return;
	case 0x8900: // STLM B, MMR
		data_write(low & 0x1f, u16(m_b));
		return;
	case 0x7100: // MVDM Smem, dmad
	{
		const u16 value = indirect_read(low);
		data_write(fetch(), value);
		return;
	}
	case 0x7300: // MVDM MMR, dmad
		data_write(fetch(), data_read(low & 0x1f));
		return;
	case 0x7400: // PORTR port, Smem
		if (low == 0xf8)
		{
			const u16 destination = fetch();
			data_write(destination, m_io.read_word(fetch()));
		}
		else
			indirect_write(low, m_io.read_word(fetch()));
		return;
	case 0x7500: // PORTW Smem, port
	{
		const u16 value = indirect_read(low);
		m_io.write_word(fetch(), value);
		return;
	}
	case 0x7000: // MVKD dmad, Smem
		indirect_write(low, data_read(fetch()));
		return;
	case 0x7600: // STM #lk, Smem
		indirect_write(low, fetch());
		return;
	case 0x7700: // STM #lk, MMR
	{
		const u16 value = fetch();
		const unsigned reg = low & 0x1f;
		data_write(reg, value);
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
	case 0x34f8: // BITT dmad
	{
		const u16 value = data_read(fetch());
		m_st0 = (m_st0 & ~0x1000) |
			(BIT(value, m_t & 0x0f) ? 0x1000 : 0);
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
	case 0xfc45: // RETC AEQ
		if ((m_a & ACC_MASK) == 0)
		{
			m_pc = pop();
			m_icount -= 4;
		}
		return;
	case 0xfc20: // RETC NTC
		if (!(m_st0 & 0x1000))
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
	case 0xf073: // B pmad
		m_pc = fetch();
		m_icount -= 3;
		return;
	case 0xf274: // CALLD pmad
	{
		const u16 destination = fetch();
		push(u16(m_pc + 2));
		m_delayed_target = destination;
		m_delayed_words = 2;
		m_icount -= 3;
		return;
	}
	case 0xfe00: // RETD
		m_delayed_target = pop();
		m_delayed_words = 2;
		m_icount -= 3;
		return;
	case 0xf493: // NOT A
		m_a = ~m_a & ACC_MASK;
		return;
	case 0xf495: // NOP
		return;
	case 0xf5e1: // IDLE 3
		m_idle = true;
		return;
	case 0xf7bb: // SSBX INTM
		m_st1 |= 0x0800;
		return;
	case 0xf6bb: // RSBX INTM
		m_st1 &= ~0x0800;
		return;
	case 0xf7bd: // SSBX XF
		m_st1 |= 0x2000;
		return;
	case 0xf6bd: // RSBX XF
		m_st1 &= ~0x2000;
		return;
	case 0xf1c0: // XOR A, B
		m_b = (m_b ^ m_a) & ACC_MASK;
		return;
	case 0xf2a0: // OR B, A
		m_a = (m_a | m_b) & ACC_MASK;
		return;
	case 0xff45: // XC 2, AEQ
		if ((m_a & ACC_MASK) != 0)
			m_pc += 2;
		return;
	case 0xff20: // XC 2, NTC
		if (m_st0 & 0x1000)
			m_pc += 2;
		return;
	case 0xf846: // BC pmad, AGT
	{
		const u16 destination = fetch();
		if ((s64(m_a << 24) >> 24) > 0)
			m_pc = destination;
		return;
	}
	case 0xf843: // BC pmad, ALT
	{
		const u16 destination = fetch();
		if ((s64(m_a << 24) >> 24) < 0)
			m_pc = destination;
		return;
	}
	case 0xf820: // BC pmad, NTC
	{
		const u16 destination = fetch();
		if (!(m_st0 & 0x1000))
			m_pc = destination;
		return;
	}
	case 0xf830: // BC pmad, TC
	{
		const u16 destination = fetch();
		if (m_st0 & 0x1000)
			m_pc = destination;
		return;
	}
	case 0xf847: // BC pmad, ALEQ
	{
		const u16 destination = fetch();
		if ((s64(m_a << 24) >> 24) <= 0)
			m_pc = destination;
		return;
	}
	case 0xf845: // BC pmad, AEQ
	{
		const u16 destination = fetch();
		if ((m_a & ACC_MASK) == 0)
			m_pc = destination;
		return;
	}
	case 0xf844: // BC pmad, ANEQ
	{
		const u16 destination = fetch();
		if ((m_a & ACC_MASK) != 0)
			m_pc = destination;
		return;
	}
	case 0xf84c: // BC pmad, BNEQ
	{
		const u16 destination = fetch();
		if ((m_b & ACC_MASK) != 0)
			m_pc = destination;
		return;
	}
	case 0xf84d: // BC pmad, BEQ
	{
		const u16 destination = fetch();
		if ((m_b & ACC_MASK) == 0)
			m_pc = destination;
		return;
	}
	case 0xf0c8: // XOR A << 8, A
		m_a = (m_a ^ (m_a << 8)) & ACC_MASK;
		return;
	case 0xf3c8: // XOR B << 8, B
		m_b = (m_b ^ (m_b << 8)) & ACC_MASK;
		return;
	case 0xf030: // AND #lk, A
		m_a &= fetch();
		return;
	case 0xf130: // AND #lk, A, B
		m_b = m_a & fetch();
		return;
	case 0xf040: // OR #lk, A
		m_a = (m_a | fetch()) & ACC_MASK;
		return;
	case 0xf063: // AND #lk << 16, A
		m_a &= u64(fetch()) << 16;
		return;
	case 0xf340: // OR #lk, B
		m_b = (m_b | fetch()) & ACC_MASK;
		return;
	case 0xf020: // LD #lk, A
		m_a = data_operand(fetch());
		return;
	case 0xf330: // AND #lk, B
		m_b &= fetch();
		return;
	case 0xf230: // AND #lk, B, A
		m_a = m_b & fetch();
		return;
	case 0xf0e8: // ROL A, 8
		m_a = ((m_a << 8) | (m_a >> 32)) & ACC_MASK;
		return;
	case 0xf3e8: // ROL B, 8
		m_b = ((m_b << 8) | (m_b >> 32)) & ACC_MASK;
		return;
	case 0xf0f8: // ROR A, 8
		m_a = ((m_a >> 8) | (m_a << 32)) & ACC_MASK;
		return;
	case 0xf3f8: // ROR B, 8
		m_b = ((m_b >> 8) | (m_b << 32)) & ACC_MASK;
		return;
	case 0xf0ff: // ROR A, 1
		m_a = ((m_a >> 1) | (m_a << 39)) & ACC_MASK;
		return;
	case 0xf3ff: // ROR B, 1
		m_b = ((m_b >> 1) | (m_b << 39)) & ACC_MASK;
		return;
	case 0xf0b0: // OR A >> 16, A
		m_a |= arithmetic_shift_right(m_a, 16);
		return;
	case 0xf3b0: // OR B >> 16, B
		m_b |= arithmetic_shift_right(m_b, 16);
		return;
	case 0xf000: // ADD #lk, A
		m_a = (m_a + data_operand(fetch())) & ACC_MASK;
		return;
	case 0xf300: // ADD #lk, B
		m_b = (m_b + data_operand(fetch())) & ACC_MASK;
		return;
	case 0xf620: // SUB B, A
		m_a = (m_a - m_b) & ACC_MASK;
		return;
	case 0xf508: // ADD A << 8, B
		m_b = (m_b + (m_a << 8)) & ACC_MASK;
		return;
	case 0xf517: // ADD A >> 9, B
		m_b = (m_b + arithmetic_shift_right(m_a, 9)) & ACC_MASK;
		return;
	case 0xf010: // SUB #lk, A
		m_a = (m_a - data_operand(fetch())) & ACC_MASK;
		return;
	case 0xf310: // SUB #lk, B
		m_b = (m_b - data_operand(fetch())) & ACC_MASK;
		return;
	case 0xf210: // SUB #lk, B, A
		m_a = (m_b - data_operand(fetch())) & ACC_MASK;
		return;
	case 0xf490: // ROL A through carry
	case 0xf590: // ROL B through carry
	{
		u64 &acc = accumulator(BIT(op, 8));
		const u32 value = u32(acc);
		const u32 carry = BIT(m_st0, 11);
		m_st0 = (m_st0 & ~0x0800) | (BIT(value, 31) ? 0x0800 : 0);
		acc = u32((value << 1) | carry);
		return;
	}
	case 0xf491: // ROR A through carry
	case 0xf591: // ROR B through carry
	{
		u64 &acc = accumulator(BIT(op, 8));
		const u32 value = u32(acc);
		const u32 carry = BIT(m_st0, 11);
		m_st0 = (m_st0 & ~0x0800) | (BIT(value, 0) ? 0x0800 : 0);
		acc = (value >> 1) | (carry << 31);
		return;
	}
	default:
		if ((op & 0xffe0) == 0x4a00) // PSHM MMR
		{
			push(data_read(low & 0x1f));
			return;
		}
		if ((op & 0xffe0) == 0x8a00) // POPM MMR
		{
			data_write(low & 0x1f, pop());
			return;
		}
		if ((op & 0xff00) == 0xec00) // RPT #k
		{
			m_rptc = low;
			m_rpt_address = m_pc;
			return;
		}
		if ((op & 0xff00) == 0x6d00) // MAR indirect auxiliary-register modification
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
		if (service_interrupt())
			continue;
		if (m_idle)
		{
			m_icount = 0;
			break;
		}
		debugger_instruction_hook(m_pc);
		if (m_illegal)
		{
			m_icount = 0;
			break;
		}

		const bool delayed = m_delayed_words != 0;
		const u16 instruction_pc = m_pc;
		m_op = fetch();
		execute_one(m_op);
		if (!m_illegal)
		{
			finish_repeats();
			if (delayed)
			{
				const unsigned words = u16(m_pc - instruction_pc);
				m_delayed_words = words >= m_delayed_words ? 0 : m_delayed_words - words;
				if (!m_delayed_words)
					m_pc = m_delayed_target;
			}
		}
		--m_icount;
	}
}

void tms320c54x_device::execute_set_input(int inputnum, int state)
{
	if (inputnum >= 0 && inputnum < 16)
	{
		if (state != CLEAR_LINE)
		{
			m_ifr |= u16(1U << inputnum);
			m_idle = false;
		}
	}
}
