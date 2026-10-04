// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "tms320c54x.h"

namespace {

constexpr u16 ST1_BRAF = 0x4000;

s32 accumulator_high17(u64 value)
{
	const u32 high = (value >> 16) & 0x1ffff;
	return (high & 0x10000) ? s32(high) - 0x20000 : s32(high);
}

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
	m_imr = m_power_on_imr;
	m_opcode_first_pc.fill(0xffff);
	m_timer = timer_alloc(FUNC(tms320c54x_device::timer_expired), this);
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
	save_item(NAME(m_rtn));
	save_item(NAME(m_rptc));
	save_item(NAME(m_rpt_address));
	save_item(NAME(m_rpt_end));
	save_item(NAME(m_rpt_iteration));
	save_item(NAME(m_rpt_armed));
	save_item(NAME(m_delayed_target));
	save_item(NAME(m_delayed_words));
	save_item(NAME(m_xc_guard));
	save_item(NAME(m_ifr));
	save_item(NAME(m_imr));
	save_item(NAME(m_clkmd));
	save_item(NAME(m_tim));
	save_item(NAME(m_prd));
	save_item(NAME(m_tcr));
	save_item(NAME(m_block_repeat_active));
	save_item(NAME(m_idle));
	save_item(NAME(m_illegal));
}

void tms320c54x_device::device_stop()
{
	if (!machine().options().verbose())
		return;

	for (unsigned opcode = 0; opcode != m_opcode_first_pc.size(); ++opcode)
		if (m_opcode_first_pc[opcode] != 0xffff)
			machine().logerror("[opcov] op=%04x first_pc=%04x count=%u\n",
					opcode, m_opcode_first_pc[opcode], m_opcode_count[opcode]);
}

void tms320c54x_device::device_reset()
{
	m_pc = 0xff80;
	m_op = 0;
	m_a = 0;
	m_b = 0;
	m_t = 0;
	m_sp = 0;
	// C54x silicon resets DP to page 0x1f with ARP/CNF set.  ROM4's
	// resident dispatcher relies on this state surviving loader startup.
	m_st0 = 0x181f;
	m_st1 = 0x2900;
	m_pmst = 0xffa0;
	std::fill(std::begin(m_ar), std::end(m_ar), 0);
	m_brc = 0;
	m_bk = 0;
	m_rsa = 0;
	m_rea = 0;
	m_rtn = 0;
	m_rptc = 0;
	m_rpt_address = 0;
	m_rpt_end = 0xffff;
	m_rpt_iteration = 0;
	m_rpt_armed = false;
	m_delayed_target = 0;
	m_delayed_words = 0;
	m_xc_guard = 0;
	m_ifr = 0;
	// Hardware reset sets INTM and clears IFR, but does not initialise IMR.
	// Preserve the mask written by the ROM loader across MAD2 DSP reset pulses.
	m_clkmd = 0;
	m_tim = 0xffff;
	m_prd = 0xffff;
	m_tcr = 0;
	arm_timer();
	m_block_repeat_active = false;
	m_idle = false;
	m_illegal = false;
}

void tms320c54x_device::update_timer_counter()
{
	if ((m_tcr & TIMER_TSS) || !m_timer->enabled())
		return;
	const u64 remaining = m_timer->remaining().as_ticks(clock());
	const u32 divider = (m_tcr & TIMER_TDDR_MASK) + 1;
	m_tim = remaining ? u16((remaining - 1) / divider) : 0;
}

void tms320c54x_device::arm_timer()
{
	if (m_tcr & TIMER_TSS)
	{
		m_timer->adjust(attotime::never);
		return;
	}
	const u64 cycles = u64(m_tim + 1) * ((m_tcr & TIMER_TDDR_MASK) + 1);
	m_timer->adjust(attotime::from_ticks(cycles, clock()));
}

TIMER_CALLBACK_MEMBER(tms320c54x_device::timer_expired)
{
	m_tim = m_prd;
	m_ifr |= 0x0008; // TINT, vector 19
	m_idle = false;
	arm_timer();
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
	if (address == 0x1b)
		return m_rsa;
	if (address == 0x1c)
		return m_rea;
	if (address == 0x1d)
		return m_pmst;
	if (address == 0x24)
	{
		update_timer_counter();
		return m_tim;
	}
	if (address == 0x25)
		return m_prd;
	if (address == 0x26)
	{
		if (!(m_tcr & TIMER_TSS) && m_timer->enabled())
		{
			const u32 divider = (m_tcr & TIMER_TDDR_MASK) + 1;
			const u64 remaining = m_timer->remaining().as_ticks(clock());
			const u16 psc = remaining ? u16((remaining - 1) % divider) : 0;
			return (m_tcr & ~TIMER_PSC_MASK) | (psc << 6);
		}
		return m_tcr & ~TIMER_PSC_MASK;
	}
	if (address == 0x58)
		return m_clkmd;
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
	else if (address == 0x1b)
		m_rsa = value;
	else if (address == 0x1c)
		m_rea = value;
	else if (address == 0x1d)
		m_pmst = value;
	else if (address == 0x24)
	{
		m_tim = value;
		arm_timer();
	}
	else if (address == 0x25)
		m_prd = value;
	else if (address == 0x26)
	{
		update_timer_counter();
		m_tcr = value & ~(TIMER_TRB | TIMER_PSC_MASK);
		if (value & TIMER_TRB)
			m_tim = m_prd;
		arm_timer();
	}
	else if (address == 0x58)
	{
		// CLKMD bit 0 is read-only PLLSTATUS. Model the mode transition as
		// immediate until the input-clock-derived PLL lock timer is exposed.
		m_clkmd = (value & ~u16(1)) | BIT(value, 1);
	}
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

u64 tms320c54x_device::shifted_load(u64 source, int shift, bool destination_b)
{
	source &= ACC_MASK;
	if (shift < 0)
		return BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift;
	const s64 value = s64(source << 24) >> 24;
	const s64 factor = s64(1) << shift;
	if (value > 0x7fffffffLL / factor || value < -0x80000000LL / factor)
	{
		m_st0 |= destination_b ? 0x0200 : 0x0400;
		if (BIT(m_st1, 9))
			return value < 0 ? 0xff80000000ULL : 0x007fffffffULL;
	}
	return (source << shift) & ACC_MASK;
}

u64 tms320c54x_device::add_sub(u64 source, u64 operand, bool subtract, bool destination_b, bool partial_carry)
{
	const bool carry = subtract ? u32(source) >= u32(operand) :
			u64(u32(source)) + u32(operand) > 0xffffffffULL;
	if (!partial_carry || carry != subtract)
		m_st0 = (m_st0 & ~0x0800) | (carry ? 0x0800 : 0);
	const s64 left = s64(source << 24) >> 24;
	const s64 right = s64(operand << 24) >> 24;
	const s64 result = subtract ? left - right : left + right;
	if (result > 0x7fffffffLL || result < -0x80000000LL)
	{
		m_st0 |= destination_b ? 0x0200 : 0x0400;
		if (BIT(m_st1, 9))
			return result < 0 ? 0xff80000000ULL : 0x007fffffffULL;
	}
	return u64(result) & ACC_MASK;
}

s64 tms320c54x_device::multiply_product(s64 product) const
{
	// PMST.SMUL saturates the product before MAC/MAS accumulation only in
	// fractional overflow mode (SPRU131G section 4.5.3).
	if (BIT(m_pmst, 1) && BIT(m_st1, 9) && BIT(m_st1, 6))
		return std::clamp(product, s64(-0x80000000LL), s64(0x7fffffffLL));
	return product;
}

u64 tms320c54x_device::multiply_result(s64 result, bool destination_b)
{
	if (result > 0x7fffffffLL || result < -0x80000000LL)
	{
		m_st0 |= destination_b ? 0x0200 : 0x0400;
		if (BIT(m_st1, 9))
			return result < 0 ? 0xff80000000ULL : 0x007fffffffULL;
	}
	return u64(result) & ACC_MASK;
}

u32 tms320c54x_device::saturated_store(u64 value) const
{
	value &= ACC_MASK;
	if (!BIT(m_pmst, 0)) // SST
		return u32(value);
	if (!BIT(m_st1, 8)) // SXM clear: unsigned saturation
		return value > 0xffffffffULL ? 0xffffffffU : u32(value);
	return u32(std::clamp(s64(value << 24) >> 24,
			s64(-0x80000000LL), s64(0x7fffffffLL)));
}

void tms320c54x_device::parallel_store(u16 op, u16 address)
{
	const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
	const int asm_shift = s8((m_st1 & 0x001f) << 3) >> 3;
	const u64 shifted_source = asm_shift < 0 ?
			arithmetic_shift_right(source, -asm_shift) :
			(source << asm_shift) & ACC_MASK;
	data_write(address, u16(saturated_store(shifted_source) >> 16));
}

u64 tms320c54x_device::arithmetic_shift_right(u64 value, unsigned shift) const
{
	const s64 signed_value = s64(value << 24) >> 24;
	return u64(signed_value >> shift) & ACC_MASK;
}

unsigned tms320c54x_device::indirect_ar(u8 mode) const
{
	const unsigned ar = mode & 7;
	return BIT(m_st1, 5) && !ar ? (m_st0 >> 13) & 7 : ar;
}

void tms320c54x_device::indirect_modify(u8 mode, bool long_operand)
{
	const unsigned ar = indirect_ar(mode);
	const s16 word_step = long_operand ? 2 : 1;
	switch (mode & 0x78)
	{
	case 0x08: m_ar[ar] -= word_step; break;
	case 0x10: m_ar[ar] += word_step; break;
	case 0x18: m_ar[ar] += word_step; break;
	case 0x20: m_ar[ar] -= m_ar[0]; break;
	case 0x28: m_ar[ar] -= m_ar[0]; break;
	case 0x30: m_ar[ar] += m_ar[0]; break;
	case 0x38: m_ar[ar] += m_ar[0]; break;
	case 0x40: circular_modify(ar, -word_step); break;
	case 0x48: circular_modify(ar, -s16(m_ar[0])); break;
	case 0x50: circular_modify(ar, word_step); break;
	case 0x58: circular_modify(ar, s16(m_ar[0])); break;
	default: break;
	}
	// In compatibility mode, a nonzero ARF both selects the auxiliary register
	// and publishes it into ST0.ARP.  Standard mode addresses ARF directly and
	// must leave ARP cleared/unchanged (TMS320C54x CPU Reference Guide 5.5.1).
	if (BIT(m_st1, 5) && (mode & 7))
		m_st0 = (m_st0 & ~u16(0xe000)) | u16((mode & 7) << 13);
}

void tms320c54x_device::dual_modify(u8 operand)
{
	const unsigned ar = 2 + (operand & 3);
	switch ((operand >> 2) & 3)
	{
	case 0: break;
	case 1: --m_ar[ar]; break;
	case 2: ++m_ar[ar]; break;
	case 3: circular_modify(ar, s16(m_ar[0])); break;
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

u16 tms320c54x_device::mmr_address(u8 mode)
{
	if (!BIT(mode, 7))
		return mode & 0x7f;
	const unsigned ar = indirect_ar(mode);
	const bool preincrement = (mode & 0x78) == 0x18;
	if (preincrement)
		indirect_modify(mode);
	const u16 address = m_ar[ar] & 0x7f;
	if (!preincrement)
		indirect_modify(mode);
	// MMR addressing clears the selected AR's upper nine bits after the access.
	m_ar[ar] &= 0x7f;
	return address;
}

u16 tms320c54x_device::direct_address(u8 mode) const
{
	return BIT(m_st1, 14) ? u16(m_sp + (mode & 0x7f)) :
			u16(((m_st0 & 0x01ff) << 7) | (mode & 0x7f));
}

u16 tms320c54x_device::short_smem_address(u8 mode) const
{
	return BIT(mode, 7) ? m_ar[indirect_ar(mode)] : direct_address(mode);
}

u16 tms320c54x_device::indirect_read(u8 mode)
{
	if (!BIT(mode, 7))
		return data_read(direct_address(mode));
	if (mode >= 0xf8)
		return data_read(fetch());
	if (mode >= 0xe0)
		return data_read(long_offset_address(mode));
	const unsigned ar = indirect_ar(mode);
	const bool preincrement = (mode & 0x78) == 0x18;
	if (preincrement)
		indirect_modify(mode);
	const u16 value = data_read(m_ar[ar]);
	if (!preincrement)
		indirect_modify(mode);
	return value;
}

u16 tms320c54x_device::immediate_smem_read(u8 mode, u16 &immediate)
{
	u16 address;
	const bool preincrement = mode >= 0x80 && mode < 0xe0 && (mode & 0x78) == 0x18;
	if (mode >= 0xe0 && mode < 0xf8)
	{
		immediate = fetch();
		address = long_offset_address(mode);
	}
	else
	{
		if (preincrement)
			indirect_modify(mode);
		address = mode >= 0xf8 ? fetch() : short_smem_address(mode);
		immediate = fetch();
	}
	const u16 value = data_read(address);
	if (mode >= 0x80 && mode < 0xe0 && !preincrement)
		indirect_modify(mode);
	return value;
}

void tms320c54x_device::indirect_write(u8 mode, u16 value)
{
	if (!BIT(mode, 7))
	{
		data_write(direct_address(mode), value);
		return;
	}
	if (mode >= 0xf8)
	{
		data_write(fetch(), value);
		return;
	}
	if (mode >= 0xe0)
	{
		data_write(long_offset_address(mode), value);
		return;
	}
	const unsigned ar = indirect_ar(mode);
	const bool preincrement = (mode & 0x78) == 0x18;
	if (preincrement)
		indirect_modify(mode);
	data_write(m_ar[ar], value);
	if (!preincrement)
		indirect_modify(mode);
}

u16 tms320c54x_device::long_offset_address(u8 mode)
{
	const unsigned ar = indirect_ar(mode);
	const s16 offset = s16(fetch());
	u16 address;
	if ((mode & 0x78) == 0x70)
	{
		circular_modify(ar, offset);
		address = m_ar[ar];
	}
	else
	{
		address = m_ar[ar] + offset;
		if ((mode & 0x78) == 0x68)
			m_ar[ar] = address;
	}
	if (BIT(m_st1, 5) && (mode & 7))
		m_st0 = (m_st0 & ~u16(0xe000)) | u16((mode & 7) << 13);
	return address;
}

void tms320c54x_device::finish_repeats()
{
	// This is the first retirement of the repeated body.  Capture its extent
	// now that its instruction length is known; #n executes the body n+1 times.
	if (m_rpt_armed)
	{
		m_rpt_armed = false;
		m_rpt_end = m_pc;
		if (m_rptc)
		{
			--m_rptc;
			++m_rpt_iteration;
			m_pc = m_rpt_address;
		}
		else
		{
			m_rpt_end = 0xffff;
			m_rpt_iteration = 0;
		}
		return;
	}

	if (m_rptc)
	{
		if (m_rpt_end == 0xffff)
			m_rpt_end = m_pc;
		if (m_pc == m_rpt_end)
		{
			--m_rptc;
			++m_rpt_iteration;
			m_pc = m_rpt_address;
		}
	}
	else if (m_rpt_end != 0xffff && m_pc == m_rpt_end)
	{
		m_rpt_end = 0xffff;
		m_rpt_iteration = 0;
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
		{
			m_block_repeat_active = false;
			m_st1 &= ~ST1_BRAF;
		}
	}
}

bool tms320c54x_device::service_interrupt()
{
	// Maskable hardware sources occupy vectors 16..31 and map directly to
	// IMR/IFR bits 0..15. PMST.IPTR selects their 128-word vector page.
	const u16 pending = m_ifr & m_imr;
	// The C54x does not recognize an interrupt between a delayed control
	// transfer and its two delay words, nor between RPT/RPTZ and the repeated
	// instruction. IFR remains set and is reconsidered at the next legal
	// instruction boundary. Taking it inside either atomic sequence can skip a
	// vector prologue's context-save slots and corrupt the return stack.
	const bool single_repeat_active = m_rpt_armed || m_rptc || m_rpt_end != 0xffff;
	if (BIT(m_st1, 11) || !pending || m_delayed_words || m_xc_guard ||
			single_repeat_active)
		return false;

	unsigned source = 0;
	while (!BIT(pending, source))
		++source;
	m_ifr &= ~(u16(1) << source);
	m_rtn = m_pc;
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
	const auto branch_if = [this](bool condition)
	{
		const u16 destination = fetch();
		if (condition)
			m_pc = destination;
		m_icount -= condition ? 4 : 2;
	};
	if ((op & 0xfcff) == 0xf4e1) // IDLE 1/2/3
	{
		m_idle = true;
		m_icount -= 3; // Four-cycle minimum before the idle interval (SPRU172C).
		return;
	}
	if ((op & 0xfdf0) == 0xf5b0) // SSBX bit, ST0/ST1
	{
		u16 &status = BIT(op, 9) ? m_st1 : m_st0;
		status |= u16(1) << (low & 0x0f);
		return;
	}
	if ((op & 0xfdf0) == 0xf4b0) // RSBX bit, ST0/ST1
	{
		u16 &status = BIT(op, 9) ? m_st1 : m_st0;
		status &= ~(u16(1) << (low & 0x0f));
		return;
	}
	if ((op & 0xfcff) == 0xf485) // ABS src, dst
	{
		const bool source_b = BIT(op, 9);
		const bool destination_b = BIT(op, 8);
		const u64 raw = accumulator(source_b) & ACC_MASK;
		const s64 value = s64(raw << 24) >> 24;
		const bool overflow = value > 0x7fffffffLL || value < -0x7fffffffLL;
		u64 result = value < 0 ? (-value & ACC_MASK) : raw;
		if (BIT(m_st1, 9) && result > 0x007fffffffULL) // OVM clamps to signed 32 bits.
			result = 0x007fffffffULL;
		accumulator(destination_b) = result;
		const u16 overflow_mask = u16(1) << (destination_b ? 9 : 10);
		if (overflow)
			m_st0 |= overflow_mask; // ALU overflow flags remain set until explicitly cleared.
		m_st0 = (m_st0 & ~0x0800) | (result == 0 ? 0x0800 : 0);
		return;
	}
	if ((op & 0xfd00) == 0xfd00) // XC n, condition
	{
		const unsigned words = BIT(op, 9) ? 2 : 1;
		bool execute = false;
		if (BIT(low, 6))
		{
			const bool b = BIT(low, 3);
			const u64 value = accumulator(b);
			const s64 signed_value = s64(value << 24) >> 24;
			execute = true;
			switch (low & 7)
			{
			case 0: break;
			case 2: execute = signed_value >= 0; break;
			case 3: execute = signed_value < 0; break;
			case 4: execute = value != 0; break;
			case 5: execute = value == 0; break;
			case 6: execute = signed_value > 0; break;
			case 7: execute = signed_value <= 0; break;
			default: execute = false; break;
			}
			if (BIT(low, 5))
			{
				const bool overflow = BIT(m_st0, b ? 9 : 10);
				execute = execute && (BIT(low, 4) ? overflow : !overflow);
			}
		}
		else
		{
			execute = true;
			if (low & 0x30)
				execute = execute && ((low & 0x30) == 0x30
						? BIT(m_st0, 12) : !BIT(m_st0, 12));
			if (low & 0x0c)
				execute = execute && ((low & 0x0c) == 0x0c
						? BIT(m_st0, 11) : !BIT(m_st0, 11));
			if (low & 0x03)
			{
				// BIO defaults deasserted until a board supplies the input line.
				execute = execute && ((low & 0x03) == 0x02);
			}
		}
		if (!execute)
		{
			m_pc += words;
			m_icount -= words; // Rejected slots execute as NOPs.
		}
		else
			m_xc_guard = words;
		return;
	}
	if ((op & 0xfcf0) == 0xf050) // XOR #lk, shift, source, destination
	{
		const u64 value = u64(fetch()) << (low & 0x0f);
		accumulator(BIT(op, 8)) = (accumulator(BIT(op, 9)) ^ value) & ACC_MASK;
		--m_icount;
		return;
	}
	if ((op & 0xfce0) == 0xf420) // SUB source accumulator, shift, destination accumulator
	{
		const int shift = s8((op & 0x1f) << 3) >> 3;
		const u64 source = accumulator(BIT(op, 9));
		const u64 value = shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) :
				(source << shift) & ACC_MASK;
		u64 &destination = accumulator(BIT(op, 8));
		destination = add_sub(destination, value, true, BIT(op, 8));
		return;
	}
	if ((op & 0xfce0) == 0xf400) // ADD source accumulator, shift, destination accumulator
	{
		const int shift = s8((op & 0x1f) << 3) >> 3;
		const u64 source = accumulator(BIT(op, 9));
		const u64 value = shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) :
				(source << shift) & ACC_MASK;
		u64 &destination = accumulator(BIT(op, 8));
		destination = add_sub(destination, value, false, BIT(op, 8));
		return;
	}
	if ((op & 0xfce0) == 0xf080) // AND source accumulator, shift, destination accumulator
	{
		const int shift = s8((op & 0x1f) << 3) >> 3;
		const u64 source = accumulator(BIT(op, 9));
		const u64 value = shift < 0 ? source >> -shift :
				(source << shift) & ACC_MASK;
		accumulator(BIT(op, 8)) &= value;
		return;
	}
	if ((op & 0xfce0) == 0xf0a0) // OR source accumulator, shift, destination accumulator
	{
		const int shift = s8((op & 0x1f) << 3) >> 3;
		const u64 source = accumulator(BIT(op, 9));
		const u64 value = shift < 0 ? source >> -shift :
				(source << shift) & ACC_MASK;
		accumulator(BIT(op, 8)) |= value;
		return;
	}
	if ((op & 0xfce0) == 0xf0e0) // SFTL source accumulator, shift, destination accumulator
	{
		const int shift = s8((op & 0x1f) << 3) >> 3;
		const u32 source = u32(accumulator(BIT(op, 9)));
		const bool carry = shift && BIT(source, shift < 0 ? -shift - 1 : 32 - shift);
		m_st0 = (m_st0 & ~0x0800) | (carry ? 0x0800 : 0);
		accumulator(BIT(op, 8)) = shift < 0 ? source >> -shift : u32(source << shift);
		return;
	}
	if ((op & 0xfcff) == 0xf480 || (op & 0xfcff) == 0xf481) // ADD/SUB src, ASM, dst
	{
		const int shift = s8((m_st1 & 0x1f) << 3) >> 3;
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		const u64 operand = shift < 0 ?
			(BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) :
			(source << shift) & ACC_MASK;
		u64 &destination = accumulator(BIT(op, 8));
		destination = add_sub(destination, operand, BIT(op, 0), BIT(op, 8));
		return;
	}
	if ((op & 0xfcff) == 0xf482) // LD source accumulator, ASM, destination accumulator
	{
		const int shift = s8((m_st1 & 0x1f) << 3) >> 3;
		const u64 source = accumulator(BIT(op, 9));
		accumulator(BIT(op, 8)) = shifted_load(source, shift, BIT(op, 8));
		return;
	}
	if ((op & 0xfeff) == 0xf486 || (op & 0xfeff) == 0xf487) // MAX/MIN A,B
	{
		const s64 a = s64(m_a << 24) >> 24;
		const s64 b = s64(m_b << 24) >> 24;
		const bool choose_a = BIT(op, 0) ? a < b : a >= b;
		accumulator(BIT(op, 8)) = choose_a ? m_a : m_b;
		m_st0 = (m_st0 & ~u16(0x0800)) | (choose_a ? 0 : 0x0800);
		return;
	}
	if ((op & 0xfcfc) == 0xf488) // MACA[R]/MASA[R] T, source, destination
	{
		s64 product = s64(accumulator_high17(m_a)) * s64(s16(m_t));
		if (BIT(m_st1, 6))
			product *= 2;
		product = multiply_product(product);
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		s64 result = (s64(source << 24) >> 24) + (BIT(op, 1) ? -product : product);
		if (BIT(op, 0))
			result = (result + 0x8000) & ~s64(0xffff);
		accumulator(BIT(op, 8)) = multiply_result(result, BIT(op, 8));
		return;
	}
	if ((op & 0xfeff) == 0xf48c) // MPYA T, A/B
	{
		s64 product = s64(accumulator_high17(m_a)) * s64(s16(m_t));
		if (BIT(m_st1, 6))
			product *= 2;
		accumulator(BIT(op, 8)) = multiply_result(product, BIT(op, 8));
		return;
	}
	if ((op & 0xfeff) == 0xf48e) // EXP A/B
	{
		const u64 source = accumulator(BIT(op, 8)) & ACC_MASK;
		int exponent = -8;
		if (source)
		{
			const bool sign = BIT(source, 39);
			for (int bit = 38; bit >= 0 && BIT(source, bit) == sign; --bit)
				++exponent;
		}
		m_t = source ? u16(s16(exponent)) : 0;
		return;
	}
	if ((op & 0xfcff) == 0xf48f) // NORM source accumulator, destination
	{
		const int shift = s8(u8(m_t << 2)) >> 2;
		accumulator(BIT(op, 8)) = shifted_load(accumulator(BIT(op, 9)), shift, BIT(op, 8));
		return;
	}
	if ((op & 0xfcff) == 0xf49f) // RND source accumulator, destination
	{
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		s64 result = (s64(source << 24) >> 24) + 0x8000;
		if (BIT(m_st1, 9))
			result = std::clamp(result, s64(-0x80000000LL), s64(0x7fffffffLL));
		accumulator(BIT(op, 8)) = u64(result) & ACC_MASK;
		return;
	}
	if ((op & 0xfeff) == 0xf483) // SAT A/B
	{
		u64 &source = accumulator(BIT(op, 8));
		const s64 value = s64((source & ACC_MASK) << 24) >> 24;
		const bool overflow = value < -0x80000000LL || value > 0x7fffffffLL;
		const u16 mask = BIT(op, 8) ? 0x0200 : 0x0400;
		m_st0 = (m_st0 & ~mask) | (overflow ? mask : 0);
		source = u64(std::clamp(value, s64(-0x80000000LL), s64(0x7fffffffLL))) & ACC_MASK;
		return;
	}
	if ((op & 0xff00) == 0x6f00)
	{
		const bool absolute = low == 0xf8;
		const u16 address = absolute ? fetch() : 0;
		const u16 extension = fetch();
		const int shift = s8((extension & 0x1f) << 3) >> 3;
		auto shifted = [this, shift](u64 value)
		{
			return shift < 0 ? arithmetic_shift_right(value, -shift) :
					(value << shift) & ACC_MASK;
		};
		auto read_operand = [this, absolute, address, low]()
		{
			return absolute ? data_read(address) : indirect_read(low);
		};
		auto write_operand = [this, absolute, address, low](u16 value)
		{
			if (absolute)
				data_write(address, value);
			else
				indirect_write(low, value);
		};
		if ((extension & 0xfee0) == 0x0c40) // LD Smem, shift, dst
			accumulator(BIT(extension, 8)) = shifted_load(data_operand(read_operand()), shift, BIT(extension, 8));
		else if ((extension & 0xfee0) == 0x0c60) // STH src, shift, Smem
			write_operand(u16(saturated_store(shifted(accumulator(BIT(extension, 8)))) >> 16));
		else if ((extension & 0xfee0) == 0x0c80) // STL src, shift, Smem
			write_operand(u16(saturated_store(shifted(accumulator(BIT(extension, 8))))));
		else if ((extension & 0xfce0) == 0x0c00) // ADD Smem, shift, src, dst
			accumulator(BIT(extension, 8)) =
					add_sub(accumulator(BIT(extension, 9)), shifted(data_operand(read_operand())), false, BIT(extension, 8));
		else if ((extension & 0xfce0) == 0x0c20) // SUB Smem, shift, src, dst
			accumulator(BIT(extension, 8)) =
					add_sub(accumulator(BIT(extension, 9)), shifted(data_operand(read_operand())), true, BIT(extension, 8));
		else
		{
			logerror("%s: unimplemented C54x extended opcode %04x/%04x at %04x\n",
					machine().describe_context(), op, extension, u16(m_pc - 3));
			m_illegal = true;
			return;
		}
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	if ((op & 0xff00) == 0xe000) // FIRS Xmem, Ymem, pmad
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		const u16 coefficient_address = fetch();
		const u16 coefficient = m_program.read_word(u16(coefficient_address +
				(repeated ? m_rpt_iteration : 0)));
		s64 product = s64(accumulator_high17(m_a)) * s64(s16(coefficient));
		if (BIT(m_st1, 6))
			product *= 2;
		m_b = multiply_result((s64(m_b << 24) >> 24) +
				multiply_product(product), true);
		const s64 xsum = BIT(m_st1, 8) ? s64(s16(xvalue)) : s64(xvalue);
		const s64 ysum = BIT(m_st1, 8) ? s64(s16(yvalue)) : s64(yvalue);
		m_a = add_sub(0, u64((xsum + ysum) * 0x10000) & ACC_MASK, false, false);
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		if (!repeated || !m_rpt_iteration)
			m_icount -= 2;
		return;
	}
	if ((op & 0xff00) == 0xe100) // LMS Xmem, Ymem
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		const s64 xadd = BIT(m_st1, 8) ? s64(s16(xvalue)) : s64(xvalue);
		m_a = add_sub(m_a, u64(xadd * 0x10000 + 0x8000) & ACC_MASK, false, false);
		s64 product = s64(s16(xvalue)) * s64(s16(yvalue));
		if (BIT(m_st1, 6))
			product *= 2;
		m_b = multiply_result((s64(m_b << 24) >> 24) + multiply_product(product), true);
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xfc00) == 0xf000 && ((op >> 4) & 0x0f) <= 5)
	{
		// Immediate accumulator ALU family.  Bit 9 selects the source,
		// bit 8 the destination, and the low nibble is the shift count.
		const unsigned operation = (op >> 4) & 0x0f;
		const unsigned shift = op & 0x0f;
		const u16 immediate = fetch();
		const u64 source = accumulator(BIT(op, 9));
		u64 &destination = accumulator(BIT(op, 8));
		// Arithmetic and load operands obey SXM; logical immediates are unextended.
		const u64 operand = operation < 3 ?
				((data_operand(immediate) << shift) & ACC_MASK) :
				(u64(immediate) << shift);
		switch (operation)
		{
		case 0: destination = add_sub(source, operand, false, BIT(op, 8)); break;
		case 1: destination = add_sub(source, operand, true, BIT(op, 8)); break;
		case 2: destination = shifted_load(data_operand(immediate), shift, BIT(op, 8)); break;
		case 3: destination = source & operand; break;
		case 4: destination = source | operand; break;
		case 5: destination = source ^ operand; break;
		}
		--m_icount; // Long-immediate ALU instructions take two cycles.
		return;
	}
	if ((op & 0xfcfe) == 0xf060) // ADD/SUB #lk,16,source,destination
	{
		const u64 operand = (data_operand(fetch()) << 16) & ACC_MASK;
		accumulator(BIT(op, 8)) = add_sub(accumulator(BIT(op, 9)), operand,
				BIT(op, 0), BIT(op, 8));
		--m_icount;
		return;
	}
	if ((op & 0xfeff) == 0xf066) // MPY #lk, A/B
	{
		const u16 immediate = fetch();
		s64 product = s64(s16(m_t)) * s64(s16(immediate));
		if (BIT(m_st1, 6))
			product *= 2;
		accumulator(BIT(op, 8)) = multiply_result(product, BIT(op, 8));
		--m_icount;
		return;
	}
	if ((op & 0xfcff) == 0xf067) // MAC #lk, source, destination
	{
		const u16 immediate = fetch();
		s64 product = s64(s16(m_t)) * s64(s16(immediate));
		if (BIT(m_st1, 6))
			product *= 2;
		product = multiply_product(product);
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		accumulator(BIT(op, 8)) = multiply_result((s64(source << 24) >> 24) + product,
				BIT(op, 8));
		--m_icount;
		return;
	}
	if (op == 0x70f8) // MVKD dmad, Smem (absolute destination form)
	{
		const u16 destination = fetch();
		const u16 source = fetch();
		data_write(destination, data_read(source));
		m_icount -= 2;
		return;
	}
	if ((op & 0xfce0) == 0xf460) // SFTA src, SHIFT, dst
	{
		const int shift = s8(u8(op << 3)) >> 3;
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		if (shift)
			m_st0 = (m_st0 & ~0x0800) |
					(BIT(source, shift < 0 ? -shift - 1 : 39 - shift) ? 0x0800 : 0);
		u64 result = shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) :
				(source << shift) & ACC_MASK;
		if (shift > 0)
		{
			const s64 full = (s64(source << 24) >> 24) * (s64(1) << shift);
			if (full > 0x7fffffffLL || full < -0x80000000LL)
			{
				m_st0 |= BIT(op, 8) ? 0x0200 : 0x0400;
				if (BIT(m_st1, 9))
					result = full < 0 ? 0xff80000000ULL : 0x007fffffffULL;
			}
		}
		accumulator(BIT(op, 8)) = result;
		return;
	}
	if ((op & 0xfce0) == 0xf440) // LD src, SHIFT, dst
	{
		const int shift = s8(u8(op << 3)) >> 3;
		accumulator(BIT(op, 8)) = shifted_load(accumulator(BIT(op, 9)), shift, BIT(op, 8));
		return;
	}
	if ((op & 0xfce0) == 0xf0c0) // XOR source accumulator, shift, destination accumulator
	{
		const int shift = s8(u8(op << 3)) >> 3;
		u64 source = accumulator(BIT(op, 9));
		if (shift >= 0)
			source = (source << shift) & ACC_MASK;
		else
			source >>= -shift;
		u64 &destination = accumulator(BIT(op, 8));
		destination ^= source;
		destination &= ACC_MASK;
		return;
	}
	const auto alu_smem = [this, low]()
	{
		m_icount -= low >= 0xe0;
		return indirect_read(low);
	};
	const auto shifted_by_t = [this](u16 memory)
	{
		const int shift = s8(u8(m_t << 2)) >> 2;
		const u64 source = data_operand(memory);
		return shift < 0 ?
			(BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) :
			(source << shift) & ACC_MASK;
	};
	switch (op & 0xff00)
	{
	case 0x1000: // LD Smem, A
		m_a = data_operand(indirect_read(low));
		m_icount -= low >= 0xe0;
		return;
	case 0x1100: // LD Smem, B
		m_b = data_operand(indirect_read(low));
		m_icount -= low >= 0xe0;
		return;
	case 0x4000: case 0x4100: case 0x4200: case 0x4300: // SUB Smem, 16, src, dst
	{
		const u64 operand = (data_operand(indirect_read(low)) << 16) & ACC_MASK;
		accumulator(BIT(op, 8)) = add_sub(accumulator(BIT(op, 9)), operand,
				true, BIT(op, 8), true);
		m_icount -= low >= 0xe0;
		return;
	}
	case 0x1200: // LD uns(Smem), A
		m_a = indirect_read(low);
		m_icount -= low >= 0xe0;
		return;
	case 0x1300: // LD uns(Smem), B
		m_b = indirect_read(low);
		m_icount -= low >= 0xe0;
		return;
	case 0x3000: case 0x3100: case 0x3200: case 0x3300:
	case 0x3400: case 0x3500: case 0x3600: case 0x3700:
	case 0x3800: case 0x3900: case 0x3a00: case 0x3b00:
	case 0x3c00: case 0x3d00: case 0x3e00: case 0x3f00:
	{
		const unsigned family = (op >> 8) & 0x0f;
		const u16 memory = indirect_read(low);
		m_icount -= low >= 0xe0;
		if (family == 0)
		{
			m_t = memory;
			return;
		}
		if (family == 2)
		{
			m_st1 = (m_st1 & ~u16(0x001f)) | (memory & 0x001f);
			return;
		}
		if (family == 4)
		{
			const bool bit = BIT(memory, 15 - (m_t & 0x0f));
			m_st0 = (m_st0 & ~u16(0x1000)) | (bit ? 0x1000 : 0);
			return;
		}
		if (family == 1 || family == 3 || family == 5 || family == 7)
		{
			s64 product = s64(accumulator_high17(m_a)) * s64(s16(memory));
			if (BIT(m_st1, 6))
				product *= 2;
			if (family != 1)
				product = multiply_product(product);
			s64 result = family == 1 ? product :
					family == 3 ? (s64(m_b << 24) >> 24) - product :
					(s64(m_b << 24) >> 24) + product;
			if (family == 7)
				result = (result + 0x8000) & ~s64(0xffff);
			m_b = multiply_result(result, true);
			m_t = memory;
			return;
		}
		if (family == 6)
		{
			s64 product = s64(accumulator_high17(m_a)) * s64(s16(m_t));
			if (BIT(m_st1, 6))
				product *= 2;
			m_a = multiply_result((multiply_product(product) +
					(s64(m_b << 24) >> 24) + 0x8000) & ~s64(0xffff), false);
			const s64 value = BIT(m_st1, 8) ? s64(s16(memory)) : s64(memory);
			m_b = u64(value * 0x10000) & ACC_MASK;
			return;
		}
		if (family >= 8 && family <= 0x0b)
		{
			s64 product = s64(s16(memory)) * s64(s16(memory));
			if (BIT(m_st1, 6))
				product *= 2;
			m_t = memory;
			u64 &destination = accumulator(BIT(family, 0));
			const s64 current = s64(destination << 24) >> 24;
			destination = multiply_result(BIT(family, 1) ? current - product : current + product,
					BIT(family, 0));
			return;
		}
		const s64 value = (BIT(m_st1, 8) ? s64(s16(memory)) : s64(memory)) * 0x10000;
		const u64 source = accumulator(BIT(family, 1));
		accumulator(BIT(family, 0)) = add_sub(source, u64(value) & ACC_MASK, false, BIT(family, 0), true);
		return;
	}
	case 0x0000: // ADD Smem, A
		m_a = add_sub(m_a, data_operand(alu_smem()), false, false);
		return;
	case 0x0100: // ADD Smem, B
		m_b = add_sub(m_b, data_operand(alu_smem()), false, true);
		return;
	case 0x0200: // ADDS Smem, A
		m_a = add_sub(m_a, alu_smem(), false, false);
		return;
	case 0x0400: // ADD Smem, TS, A
	case 0x0500: // ADD Smem, TS, B
	{
		u64 &destination = accumulator(BIT(op, 8));
		destination = add_sub(destination, shifted_by_t(alu_smem()), false, BIT(op, 8));
		return;
	}
	case 0x0600: // ADDC Smem, A
	case 0x0700: // ADDC Smem, B
	{
		// ADDC zero-extends Smem and consumes ST0.C. Carry is defined at
		// the 32-bit accumulator boundary, independently of the guard byte.
		u64 &destination = accumulator(BIT(op, 8));
		const u64 operand = u64(alu_smem()) + (BIT(m_st0, 11) ? 1 : 0);
		destination = add_sub(destination, operand, false, BIT(op, 8));
		return;
	}
	case 0x0800: // SUB Smem, A
		m_a = add_sub(m_a, data_operand(alu_smem()), true, false);
		return;
	case 0x0900: // SUB Smem, B
		m_b = add_sub(m_b, data_operand(alu_smem()), true, true);
		return;
	case 0x0a00: // SUBS Smem, A
		m_a = add_sub(m_a, alu_smem(), true, false);
		return;
	case 0x0b00: // SUBS Smem, B
		m_b = add_sub(m_b, alu_smem(), true, true);
		return;
	case 0x0c00: // SUB Smem, TS, A
	case 0x0d00: // SUB Smem, TS, B
	{
		u64 &destination = accumulator(BIT(op, 8));
		destination = add_sub(destination, shifted_by_t(alu_smem()), true, BIT(op, 8));
		return;
	}
	case 0x0e00: // SUBB Smem, A
	case 0x0f00: // SUBB Smem, B
	{
		u64 &destination = accumulator(BIT(op, 8));
		const u64 operand = u64(alu_smem()) + (BIT(m_st0, 11) ? 0 : 1);
		destination = add_sub(destination, operand, true, BIT(op, 8));
		return;
	}
	case 0x1400: // LD Smem, TS, A
	case 0x1500: // LD Smem, TS, B
		accumulator(BIT(op, 8)) = shifted_by_t(alu_smem());
		return;
	case 0x1800: // AND Smem, A
		m_a &= alu_smem();
		return;
	case 0x1900: // AND Smem, B
		m_b &= alu_smem();
		return;
	case 0x1a00: // OR Smem, A
		m_a = (m_a | alu_smem()) & ACC_MASK;
		return;
	case 0x1b00: // OR Smem, B
		m_b = (m_b | alu_smem()) & ACC_MASK;
		return;
	case 0x1c00: // XOR Smem, A
		m_a = (m_a ^ alu_smem()) & ACC_MASK;
		return;
	case 0x1d00: // XOR Smem, B
		m_b = (m_b ^ alu_smem()) & ACC_MASK;
		return;
	case 0x1e00: // SUBC Smem, A
	case 0x1f00: // SUBC Smem, B
	{
		u64 &source = accumulator(BIT(op, 8));
		const u64 divisor = data_operand(alu_smem());
		const s64 difference = (s64(source << 24) >> 24) -
				(s64(divisor << 24) >> 24) * 0x8000;
		const bool subtract = difference >= 0;
		source = (subtract ? (u64(difference) << 1) | 1 : source << 1) &
				ACC_MASK;
		m_st0 = (m_st0 & ~u16(0x0800)) | (subtract ? 0x0800 : 0);
		return;
	}
	case 0x2000: case 0x2100: // MPY Smem, A/B
	case 0x2200: case 0x2300: // MPYR Smem, A/B
	case 0x2400: case 0x2500: // MPYU Smem, A/B
	case 0x2600: case 0x2700: // SQUR Smem, A/B
	case 0x2800: case 0x2900: // MAC Smem, A/B
	case 0x2a00: case 0x2b00: // MACR Smem, A/B
	case 0x2c00: case 0x2d00: // MAS Smem, A/B
	case 0x2e00: case 0x2f00: // MASR Smem, A/B
	{
		const unsigned family = (op >> 8) & 0x0e;
		const u16 memory = indirect_read(low);
		s64 product;
		if (family == 0x04)
			product = s64(u16(m_t)) * s64(memory);
		else if (family == 0x06)
		{
			product = s64(s16(memory)) * s64(s16(memory));
			m_t = memory;
		}
		else
			product = s64(s16(m_t)) * s64(s16(memory));
		if (BIT(m_st1, 6)) // FRCT
			product *= 2;
		if (family >= 0x08)
			product = multiply_product(product);

		u64 &destination = accumulator(BIT(op, 8));
		s64 result;
		if (family <= 0x06)
			result = product;
		else if (family >= 0x0c)
			result = (s64(destination << 24) >> 24) - product;
		else
			result = (s64(destination << 24) >> 24) + product;
		if (family == 0x02 || family == 0x0a || family == 0x0e)
			result = (result + 0x8000) & ~s64(0xffff);
		destination = multiply_result(result, BIT(op, 8));
		m_icount -= low >= 0xe0;
		return;
	}
	case 0x6200: case 0x6300: // MPY Smem, #lk, A/B
	{
		u16 immediate;
		const u16 memory = immediate_smem_read(low, immediate);
		m_t = memory;
		s64 product = s64(s16(memory)) * s64(s16(immediate));
		if (BIT(m_st1, 6))
			product *= 2;
		accumulator(BIT(op, 8)) = multiply_result(product, BIT(op, 8));
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x6400: case 0x6500: case 0x6600: case 0x6700: // MAC Smem, #lk, src, dst
	{
		u16 immediate;
		const u16 memory = immediate_smem_read(low, immediate);
		m_t = memory;
		s64 product = s64(s16(memory)) * s64(s16(immediate));
		if (BIT(m_st1, 6))
			product *= 2;
		product = multiply_product(product);
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		accumulator(BIT(op, 8)) = multiply_result((s64(source << 24) >> 24) + product,
				BIT(op, 8));
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x4400: // LD Smem, 16, A
		m_a = (data_operand(indirect_read(low)) << 16) & ACC_MASK;
		m_icount -= low >= 0xe0;
		return;
	case 0x4500: // LD Smem, 16, B
		m_b = (data_operand(indirect_read(low)) << 16) & ACC_MASK;
		m_icount -= low >= 0xe0;
		return;
	case 0x4700: // RPT Smem
		m_rptc = indirect_read(low);
		m_rpt_address = m_pc;
		m_rpt_end = 0xffff;
		m_rpt_iteration = 0;
		m_rpt_armed = true;
		m_icount -= low >= 0xe0 ? 3 : 2;
		return;
	case 0x4b00: // PSHD Smem
		push(indirect_read(low));
		m_icount -= low >= 0xe0;
		return;
	case 0x4c00: // LTD Smem
	case 0x4d00: // DELAY Smem
	{
		const bool preincrement = low >= 0x80 && low < 0xe0 && (low & 0x78) == 0x18;
		if (preincrement)
			indirect_modify(low);
		const u16 address = low >= 0xf8 ? fetch() :
				low >= 0xe0 ? long_offset_address(low) : short_smem_address(low);
		const u16 value = data_read(address);
		data_write(u16(address + 1), value);
		if ((op & 0xff00) == 0x4c00)
			m_t = value;
		if (low >= 0x80 && low < 0xe0 && !preincrement)
			indirect_modify(low);
		m_icount -= low >= 0xe0;
		return;
	}
	case 0x8c00: // ST T, Smem
		indirect_write(low, m_t);
		m_icount -= low >= 0xe0;
		return;
	case 0x8b00: // POPD Smem
		indirect_write(low, pop());
		m_icount -= low >= 0xe0;
		return;
	case 0x7f00: // WRITA Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		m_program.write_word(u16(m_a) + (repeated ? m_rpt_iteration : 0),
				indirect_read(low));
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 5 : 4;
		return;
	}
	case 0x6100: // BITF Smem, #lk
	{
		u16 value;
		u16 mask;
		if (low >= 0xe0 && low < 0xf8)
		{
			mask = fetch();
			value = indirect_read(low);
		}
		else
		{
			value = indirect_read(low);
			mask = fetch();
		}
		m_st0 = (m_st0 & ~0x1000) | ((value & mask) ? 0x1000 : 0);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x6000: // CMPM Smem, #lk
	{
		u16 value;
		u16 immediate;
		if (low >= 0xe0 && low < 0xf8)
		{
			immediate = fetch();
			value = indirect_read(low);
		}
		else
		{
			value = indirect_read(low);
			immediate = fetch();
		}
		m_st0 = (m_st0 & ~0x1000) | (value == immediate ? 0x1000 : 0);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x4800: // LDM MMR, A
		m_a = data_read(mmr_address(low));
		return;
	case 0x4900: // LDM MMR, B
		m_b = data_read(mmr_address(low));
		return;
	case 0x4e00: // DST A, Lmem
	case 0x4f00: // DST B, Lmem
	{
		const bool extended = low >= 0xe0;
		const bool preincrement = low >= 0x80 && (low & 0x78) == 0x18;
		if (!extended && preincrement)
			indirect_modify(low, true);
		const u16 address = low >= 0xf8 ? fetch() :
				low >= 0xe0 ? long_offset_address(low) : short_smem_address(low);
		const u32 value = saturated_store(accumulator(BIT(op, 8)));
		data_write(address, u16(value >> 16));
		data_write(address ^ 1, u16(value));
		if (low >= 0x80 && !extended && !preincrement)
			indirect_modify(low, true);
		m_icount -= extended ? 2 : 1;
		return;
	}
	case 0x5000: case 0x5100: case 0x5200: case 0x5300: // DADD Lmem, src, dst
	case 0x5400: case 0x5500: // DSUB Lmem, src
	case 0x5800: case 0x5900: // DRSUB Lmem, src
	case 0x5a00: case 0x5b00: // DADST Lmem, dst
	case 0x5c00: case 0x5d00: // DSUBT Lmem, dst
	case 0x5e00: case 0x5f00: // DSADT Lmem, dst
	{
		const bool extended = low >= 0xe0;
		const bool preincrement = low >= 0x80 && (low & 0x78) == 0x18;
		if (!extended && preincrement)
			indirect_modify(low, true);
		const u16 address = low >= 0xf8 ? fetch() :
				low >= 0xe0 ? long_offset_address(low) : short_smem_address(low);
		const u16 high = data_read(address);
		const u16 low_word = data_read(address ^ 1);
		if (low >= 0x80 && !extended && !preincrement)
			indirect_modify(low, true);
		const unsigned family = (op >> 8) & 0x0f;
		const bool t_variant = family >= 0x0a;
		const bool memory_first = family >= 0x08;
		const bool subtract_high = family == 0x04 || family == 0x05 ||
				family == 0x08 || family == 0x09 || family >= 0x0c;
		const bool subtract_low = (family >= 0x04 && family <= 0x05) ||
				(family >= 0x08 && family <= 0x0d);
		const bool destination_b = BIT(op, 8);
		const u64 source = accumulator(family < 4 ? BIT(op, 9) : destination_b);
		u64 &destination = accumulator(destination_b);
		u64 memory = (u64(high) << 16) | low_word;
		if (BIT(m_st1, 8) && BIT(high, 15))
			memory |= u64(0xff) << 32;
		if (!BIT(m_st1, 7)) // Double-precision mode.
		{
			u64 t_pair = (u64(m_t) << 16) | m_t;
			if (BIT(m_st1, 8) && BIT(m_t, 15))
				t_pair |= u64(0xff) << 32;
			const u64 left = memory_first ? memory : source;
			const u64 right = t_variant ? t_pair : memory_first ? source : memory;
			destination = add_sub(left, right, subtract_high, destination_b);
		}
		else // Dual 16-bit mode: the two lanes do not carry into one another.
		{
			const u16 left_high_word = memory_first ? high : u16(source >> 16);
			const u16 right_high_word = t_variant ? m_t : memory_first ? u16(source >> 16) : high;
			const u16 left_low_word = memory_first ? low_word : u16(source);
			const u16 right_low_word = t_variant ? m_t : memory_first ? u16(source) : low_word;
			const s32 left_high = BIT(m_st1, 8) ? s32(s16(left_high_word)) : s32(left_high_word);
			const s32 right_high = BIT(m_st1, 8) ? s32(s16(right_high_word)) : s32(right_high_word);
			const s32 result_high = subtract_high ? left_high - right_high : left_high + right_high;
			const u16 result_low = subtract_low ? left_low_word - right_low_word : left_low_word + right_low_word;
			const bool carry = subtract_high ? left_high_word >= right_high_word :
					u32(left_high_word) + right_high_word > 0xffff;
			m_st0 = (m_st0 & ~u16(0x0800)) | (carry ? 0x0800 : 0);
			destination = (u64(u32(result_high) & 0xffffff) << 16) | result_low;
		}
		m_icount -= extended;
		return;
	}
	case 0x5600: // DLD Lmem, A
	case 0x5700: // DLD Lmem, B
	{
		const bool extended = low >= 0xe0;
		const bool preincrement = low >= 0x80 && (low & 0x78) == 0x18;
		if (!extended && preincrement)
			indirect_modify(low, true);
		const u16 address = low >= 0xf8 ? fetch() :
				low >= 0xe0 ? long_offset_address(low) : short_smem_address(low);
		const u16 high = data_read(address);
		const u16 low_word = data_read(address ^ 1);
		u64 value = (u64(high) << 16) | low_word;
		// C16 dual mode and ordinary double-precision mode have the same
		// 32-bit payload layout here. SXM extends the high halfword into the
		// accumulator guard byte in either mode.
		if (BIT(m_st1, 8) && BIT(high, 15))
			value |= u64(0xff) << 32;
		accumulator(BIT(op, 8)) = value;
		if (low >= 0x80 && !extended && !preincrement)
			indirect_modify(low, true);
		m_icount -= extended;
		return;
	}
	case 0x6800: // ANDM #lk, Smem
	case 0x6900: // ORM #lk, Smem
	{
		u16 address;
		u16 immediate;
		if (low >= 0xe0 && low < 0xf8)
		{
			immediate = fetch();
			address = long_offset_address(low);
		}
		else
		{
			address = low >= 0xf8 ? fetch() : short_smem_address(low);
			immediate = fetch();
		}
		const u16 value = data_read(address);
		data_write(address, (op & 0xff00) == 0x6800
				? value & immediate : value | immediate);
		if (low >= 0x80 && low < 0xe0)
			indirect_modify(low);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x6b00: // ADDM #lk, Smem
	{
		u16 address;
		u16 immediate;
		if (low >= 0xe0 && low < 0xf8)
		{
			immediate = fetch();
			address = long_offset_address(low);
		}
		else
		{
			address = low >= 0xf8 ? fetch() : short_smem_address(low);
			immediate = fetch();
		}
		const u16 value = data_read(address);
		const s32 left = BIT(m_st1, 8) ? s16(value) : s32(value);
		const s32 right = BIT(m_st1, 8) ? s16(immediate) : s32(immediate);
		const s32 result = left + right;
		const bool overflow = result < -0x8000 || result > 0x7fff;
		// ADDM stores 16 bits, but the ALU carry is taken at bit 32.
		m_st0 = (m_st0 & ~u16(0x0800)) |
				(u64(u32(left)) + u32(right) > 0xffffffffULL ? 0x0800 : 0);
		if (overflow)
			m_st0 |= 0x0400;
		data_write(address, overflow && BIT(m_st1, 9)
				? u16(std::clamp(result, s32(-0x8000), s32(0x7fff))) : u16(result));
		if (low >= 0x80 && low < 0xe0)
			indirect_modify(low);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x6c00: // BANZ pmad, *ARx modification
	{
		const unsigned ar = indirect_ar(low);
		const bool branch = m_ar[ar] != 0;
		indirect_modify(low);
		const u16 destination = fetch();
		if (branch)
			m_pc = destination;
		m_icount -= branch ? 3 : 1;
		return;
	}
	case 0x6e00: // BANZD pmad, *ARx modification
	{
		const unsigned ar = indirect_ar(low);
		const bool branch = m_ar[ar] != 0;
		indirect_modify(low);
		const u16 destination = fetch();
		if (branch)
		{
			m_delayed_target = destination;
			m_delayed_words = 2;
		}
		--m_icount;
		return;
	}
	case 0x8000: // STL A, Smem
		indirect_write(low, u16(saturated_store(m_a)));
		m_icount -= low >= 0xe0;
		return;
	case 0x8100: // STL B, Smem
		indirect_write(low, u16(saturated_store(m_b)));
		m_icount -= low >= 0xe0;
		return;
	case 0x8200: // STH A, Smem
		indirect_write(low, u16(saturated_store(m_a) >> 16));
		m_icount -= low >= 0xe0;
		return;
	case 0x8300: // STH B, Smem
		indirect_write(low, u16(saturated_store(m_b) >> 16));
		m_icount -= low >= 0xe0;
		return;
	case 0xe800: // LD #k, A
		m_a = data_operand(low);
		return;
	case 0xe900: // LD #k, B
		m_b = data_operand(low);
		return;
	case 0xee00: // FRAME #k8
		m_sp = u16(m_sp + s8(low));
		return;
	case 0x8800: // STLM A, MMR
		data_write(mmr_address(low), u16(saturated_store(m_a)));
		return;
	case 0x8900: // STLM B, MMR
		data_write(mmr_address(low), u16(saturated_store(m_b)));
		return;
	case 0x7100: // MVDK Smem, dmad
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		u16 value;
		u16 destination;
		if (low >= 0xe0 && low < 0xf8)
		{
			destination = fetch() + (repeated ? m_rpt_iteration : 0);
			value = indirect_read(low);
		}
		else
		{
			value = indirect_read(low);
			destination = fetch() + (repeated ? m_rpt_iteration : 0);
		}
		data_write(destination, value);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x7300: // MVMD MMR, dmad
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		const u16 destination = fetch() + (repeated ? m_rpt_iteration : 0);
		data_write(destination, data_read(mmr_address(low)));
		m_icount -= !repeated || !m_rpt_iteration;
		return;
	}
	case 0x7400: // PORTR port, Smem
		if (low == 0xf8)
		{
			const u16 destination = fetch();
			data_write(destination, m_io.read_word(fetch()));
		}
		else
			indirect_write(low, m_io.read_word(fetch()));
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	case 0x7500: // PORTW Smem, port
	{
		u16 value;
		u16 port;
		if (low >= 0xe0 && low < 0xf8)
		{
			port = fetch();
			value = indirect_read(low);
		}
		else
		{
			value = indirect_read(low);
			port = fetch();
		}
		m_io.write_word(port, value);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x7000: // MVKD dmad, Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		const u16 source = fetch() + (repeated ? m_rpt_iteration : 0);
		indirect_write(low, data_read(source));
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x7200: // MVDM dmad, MMR
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		const u16 source = fetch() + (repeated ? m_rpt_iteration : 0);
		data_write(mmr_address(low), data_read(source));
		m_icount -= !repeated || !m_rpt_iteration;
		return;
	}
	case 0x7600: // ST #lk, Smem
		if (low >= 0xe0)
		{
			u16 address;
			u16 value;
			if (low < 0xf8)
			{
				value = fetch();
				address = long_offset_address(low);
			}
			else
			{
				address = fetch();
				value = fetch();
			}
			data_write(address, value);
		}
		else
			indirect_write(low, fetch());
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	case 0x7700: // STM #lk, MMR
	{
		const u16 value = fetch();
		const u16 reg = mmr_address(low);
		data_write(reg, value);
		m_icount -= 1;
		return;
	}
	case 0x7800: case 0x7900: // MACP Smem, pmad, A/B
	case 0x7a00: case 0x7b00: // MACD Smem, pmad, A/B
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		u16 coefficient_address;
		u16 address;
		if (low >= 0xe0 && low < 0xf8)
		{
			coefficient_address = fetch();
			address = long_offset_address(low);
		}
		else
		{
			const bool preincrement = low >= 0x80 && low < 0xe0 && (low & 0x78) == 0x18;
			if (preincrement)
				indirect_modify(low);
			address = low >= 0xf8 ? fetch() : short_smem_address(low);
			coefficient_address = fetch();
		}
		const u16 value = data_read(address);
		const u16 coefficient = m_program.read_word(u16(coefficient_address +
				(repeated ? m_rpt_iteration : 0)));
		s64 product = s64(s16(value)) * s64(s16(coefficient));
		if (BIT(m_st1, 6))
			product *= 2;
		const bool b = BIT(op, 8);
		accumulator(b) = multiply_result((s64(accumulator(b) << 24) >> 24) +
				multiply_product(product), b);
		m_t = value;
		if (BIT(op, 9))
			data_write(u16(address + 1), value);
		if (low >= 0x80 && low < 0xe0 && (low & 0x78) != 0x18)
			indirect_modify(low);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 3 : 2;
		return;
	}
	case 0x7d00: // MVDP Smem, pmad
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		u16 value;
		u16 destination;
		if (low >= 0xe0 && low < 0xf8)
		{
			destination = fetch() + (repeated ? m_rpt_iteration : 0);
			value = indirect_read(low);
		}
		else
		{
			value = indirect_read(low);
			destination = fetch() + (repeated ? m_rpt_iteration : 0);
		}
		m_program.write_word(destination, value);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 4 : 3;
		return;
	}
	case 0x7c00: // MVPD pmad, Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		u16 destination;
		u16 source;
		if (low == 0xf8)
		{
			destination = fetch();
			source = fetch();
		}
		else
		{
			source = fetch();
			destination = low >= 0xe0 ? long_offset_address(low) : short_smem_address(low);
		}
		const u16 value = m_program.read_word(source + (repeated ? m_rpt_iteration : 0));
		data_write(destination, value);
		if (low >= 0x80 && low < 0xe0)
			indirect_modify(low);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 3 : 2;
		return;
	}
	case 0x7e00: // READA Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != 0xffff) &&
			u16(m_pc - 1) == m_rpt_address;
		const u16 value = m_program.read_word(u16(m_a) +
				(repeated ? m_rpt_iteration : 0));
		indirect_write(low, value);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 5 : 4;
		return;
	}
	case 0xe700: // MVMM MMRx, MMRy
	{
		const unsigned source = low >> 4;
		const unsigned destination = low & 0x0f;
		if (source > 8 || destination > 8)
			break;
		data_write(0x10 + destination, data_read(0x10 + source));
		return;
	}
	default:
		break;
	}

	if ((op & 0xff00) == 0xe500) // MVDD Xmem, Ymem
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 source_address = m_ar[xar];
		const u16 destination_address = m_ar[yar];
		const u16 value = data_read(source_address);
		data_write(destination_address, value);
		dual_modify(x);
		// When both operands select the same AR with different modifiers, the
		// X operand's modifier owns the address update (SPRU131G section 5.5.4).
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xfe00) == 0xa400) // MPY Xmem, Ymem, A/B
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		s64 product = s64(s16(xvalue)) * s64(s16(yvalue));
		if (BIT(m_st1, 6))
			product *= 2;
		accumulator(BIT(op, 8)) = multiply_result(product, BIT(op, 8));
		m_t = xvalue;
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xfe00) == 0xa600) // MACSU Xmem, Ymem, A/B
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		s64 product = s64(xvalue) * s64(s16(yvalue));
		if (BIT(m_st1, 6))
			product *= 2;
		const bool b = BIT(op, 8);
		accumulator(b) = multiply_result((s64(accumulator(b) << 24) >> 24) +
				multiply_product(product), b);
		m_t = xvalue;
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xf000) == 0xb000) // MAC[R]/MAS[R] Xmem, Ymem, src, dst
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		s64 product = s64(s16(xvalue)) * s64(s16(yvalue));
		if (BIT(m_st1, 6))
			product *= 2;
		product = multiply_product(product);
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		s64 result = (s64(source << 24) >> 24) + (BIT(op, 11) ? -product : product);
		if (BIT(op, 10))
			result = (result + 0x8000) & ~s64(0xffff);
		accumulator(BIT(op, 8)) = multiply_result(result, BIT(op, 8));
		m_t = xvalue;
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xf800) == 0xc000) // ST src, Ymem || ADD/SUB Xmem, dst
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u64 other = accumulator(!BIT(op, 8)) & ACC_MASK;
		const u64 operand = (data_operand(xvalue) << 16) & ACC_MASK;
		parallel_store(op, m_ar[yar]);
		const bool subtract = BIT(op, 10);
		accumulator(BIT(op, 8)) = subtract ?
				add_sub(operand, other, true, BIT(op, 8)) :
				add_sub(other, operand, false, BIT(op, 8));
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xfc00) == 0xc800 || (op & 0xfd00) == 0xe400) // ST src, Ymem || LD Xmem, dst/T
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		parallel_store(op, m_ar[yar]);
		if ((op & 0xfd00) == 0xe400)
			m_t = xvalue;
		else
			accumulator(BIT(op, 8)) = (data_operand(xvalue) << 16) & ACC_MASK;
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xfc00) == 0xcc00) // ST src, Ymem || MPY Xmem, dst
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		parallel_store(op, m_ar[yar]);
		s64 product = s64(s16(m_t)) * s64(s16(xvalue));
		if (BIT(m_st1, 6))
			product *= 2;
		accumulator(BIT(op, 8)) = multiply_result(product, BIT(op, 8));
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xf000) == 0xd000) // ST src, Ymem || MAC[R]/MAS[R] Xmem, dst
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		parallel_store(op, m_ar[yar]);

		s64 product = s64(s16(m_t)) * s64(s16(xvalue));
		if (BIT(m_st1, 6))
			product *= 2;
		product = multiply_product(product);
		const u64 old_destination = accumulator(BIT(op, 8)) & ACC_MASK;
		s64 result = (s64(old_destination << 24) >> 24) + (BIT(op, 11) ? -product : product);
		if (BIT(op, 10))
			result = (result + 0x8000) & ~s64(0xffff);
		accumulator(BIT(op, 8)) = multiply_result(result, BIT(op, 8));
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xff00) == 0xe200) // SQDST Xmem, Ymem
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const s32 xvalue = s16(data_read(m_ar[xar]));
		const s32 yvalue = s16(data_read(m_ar[yar]));
		const s64 old_a_high = accumulator_high17(m_a);
		s64 square = old_a_high * old_a_high;
		if (BIT(m_st1, 6))
			square *= 2;
		const u64 old_b = m_b & ACC_MASK;
		m_b = u64((s64(old_b << 24) >> 24) + square) & ACC_MASK;
		m_a = u64(s64(xvalue - yvalue) * 0x10000) & ACC_MASK;
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xfc00) == 0x9000) // ADD/SUB Xmem, SHFT, A/B
	{
		const u8 x = op >> 4;
		const unsigned xar = 2 + (x & 3);
		const u64 operand = (data_operand(data_read(m_ar[xar])) << (op & 0x0f)) & ACC_MASK;
		u64 &destination = accumulator(BIT(op, 8));
		destination = add_sub(destination, operand, BIT(op, 9), BIT(op, 8));
		dual_modify(x);
		return;
	}
	if ((op & 0xfc00) == 0xa000) // ADD/SUB Xmem, Ymem, A/B
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		const s64 xsigned = BIT(m_st1, 8) ? s64(s16(xvalue)) : s64(xvalue);
		const s64 ysigned = BIT(m_st1, 8) ? s64(s16(yvalue)) : s64(yvalue);
		accumulator(BIT(op, 8)) = add_sub(u64(xsigned * 0x10000) & ACC_MASK,
				u64(ysigned * 0x10000) & ACC_MASK, BIT(op, 9), BIT(op, 8));
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xff00) == 0xe300) // ABDST Xmem, Ymem
	{
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		const s64 a_high = accumulator_high17(m_a);
		s64 distance = a_high < 0 ? -a_high : a_high;
		if (BIT(m_st1, 6))
			distance *= 2;
		m_b = add_sub(m_b, u64(distance), false, true);
		const s64 xsigned = BIT(m_st1, 8) ? s64(s16(xvalue)) : s64(xvalue);
		const s64 ysigned = BIT(m_st1, 8) ? s64(s16(yvalue)) : s64(yvalue);
		m_a = add_sub(u64(xsigned * 0x10000) & ACC_MASK,
				u64(ysigned * 0x10000) & ACC_MASK, true, false);
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xff80) == 0xf900) // CC pmad, condition
	{
		const u8 condition = op;
		const u16 destination = fetch();
		bool take = false;
		if (BIT(condition, 6))
		{
			const bool b = BIT(condition, 3);
			const u64 raw = accumulator(b) & ACC_MASK;
			const s64 value = s64(raw << 24) >> 24;
			switch (condition & 7)
			{
			case 2: take = value >= 0; break;
			case 3: take = value < 0; break;
			case 4: take = raw != 0; break;
			case 5: take = raw == 0; break;
			case 6: take = value > 0; break;
			case 7: take = value <= 0; break;
			default: take = true; break;
			}
			if ((condition & 0x70) == 0x70)
				take = BIT(m_st0, b ? 9 : 10);
			else if ((condition & 0x70) == 0x60)
				take = !BIT(m_st0, b ? 9 : 10);
		}
		else if ((condition & 0x30) == 0x30)
			take = BIT(m_st0, 12);
		else if ((condition & 0x30) == 0x20)
			take = !BIT(m_st0, 12);
		else if ((condition & 0x0c) == 0x0c)
			take = BIT(m_st0, 11);
		else if ((condition & 0x0c) == 0x08)
			take = !BIT(m_st0, 11);
		else
			take = true;
		m_icount -= 2;
		if (take)
		{
			push(m_pc);
			m_pc = destination;
			m_icount -= 2;
		}
		return;
	}

	const auto return_if = [this](bool take)
	{
		if (take)
		{
			m_pc = pop();
			m_icount -= 4;
		}
		else
			m_icount -= 2;
	};
	switch (op)
	{
	case 0xfc00: // RET
		m_pc = pop();
		m_icount -= 4;
		return;
	case 0xf4eb: // RETE
		m_pc = pop();
		m_st1 &= ~0x0800;
		m_icount -= 4;
		return;
	case 0xf49b: // RETF
		m_pc = m_rtn;
		pop();
		m_st1 &= ~0x0800;
		m_icount -= 2;
		return;
	case 0xfc30: // RETC TC
		return_if(m_st0 & 0x1000);
		return;
	case 0xfc45: // RETC AEQ
		return_if((m_a & ACC_MASK) == 0);
		return;
	case 0xfc47: // RETC ALEQ
		return_if((s64(m_a << 24) >> 24) <= 0);
		return;
	case 0xfc20: // RETC NTC
		return_if(!(m_st0 & 0x1000));
		return;
	case 0xfc4b: // RC BLT
		return_if((s64(m_b << 24) >> 24) < 0);
		return;
	case 0xfc4d: // RC BEQ
		return_if((m_b & ACC_MASK) == 0);
		return;
	case 0xfc44: // RC ANEQ
		return_if((m_a & ACC_MASK) != 0);
		return;
	case 0xf072: // RPTB pmad
		m_rsa = u16(m_pc + 1);
		m_rea = fetch();
		m_block_repeat_active = true;
		m_st1 |= ST1_BRAF;
		m_icount -= 3;
		return;
	case 0xf272: // RPTBD pmad
		m_rea = fetch();
		// The two words after the two-word instruction are delay slots.  The
		// repeated block begins immediately after them.
		m_rsa = u16(m_pc + 2);
		m_block_repeat_active = true;
		m_st1 |= ST1_BRAF;
		--m_icount;
		return;
	case 0xf071: // RPTZ A, #lk
	case 0xf171: // RPTZ B, #lk
		accumulator(BIT(op, 8)) = 0;
		m_rptc = fetch();
		m_rpt_address = m_pc;
		m_rpt_end = 0xffff;
		m_rpt_iteration = 0;
		m_rpt_armed = true;
		--m_icount;
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
	case 0xf4e2: // BACC A
	case 0xf5e2: // BACC B
		m_pc = u16(accumulator(BIT(op, 8)));
		m_icount -= 5;
		return;
	case 0xf4e3: // CALA A
	case 0xf5e3: // CALA B
		push(m_pc);
		m_pc = u16(accumulator(BIT(op, 8)));
		m_icount -= 5;
		return;
	case 0xf6e3: // CALAD A
	case 0xf7e3: // CALAD B
		push(u16(m_pc + 2));
		m_delayed_target = u16(accumulator(BIT(op, 8)));
		m_delayed_words = 2;
		m_icount -= 3;
		return;
	case 0xf493: // CMPL source accumulator, destination accumulator
	case 0xf593:
	case 0xf693:
	case 0xf793:
		accumulator(BIT(op, 8)) = ~accumulator(BIT(op, 9)) & ACC_MASK;
		return;
	case 0xf065: // XOR #lk, 16, source accumulator, destination accumulator
	case 0xf165:
	case 0xf265:
	case 0xf365:
		accumulator(BIT(op, 8)) =
				(accumulator(BIT(op, 9)) ^ (u64(fetch()) << 16)) & ACC_MASK;
		--m_icount;
		return;
	case 0xf274: // CALLD pmad
	{
		const u16 destination = fetch();
		push(u16(m_pc + 2));
		m_delayed_target = destination;
		m_delayed_words = 2;
		m_icount -= 1;
		return;
	}
	case 0xf273: // BD pmad
		m_delayed_target = fetch();
		m_delayed_words = 2;
		m_icount -= 1;
		return;
	case 0xfa45: // BCD pmad, AEQ
	case 0xfa44: // BCD pmad, ANEQ
	case 0xfa43: // BCD pmad, ALT
	case 0xfa20: // BCD pmad, NTC
	case 0xfa30: // BCD pmad, TC
	case 0xfa4d: // BCD pmad, BEQ
	case 0xfa4f: // BCD pmad, BLEQ
	{
		const u16 destination = fetch();
		const bool condition = op == 0xfa45 ? (m_a & ACC_MASK) == 0 :
				op == 0xfa44 ? (m_a & ACC_MASK) != 0 :
				op == 0xfa43 ? (s64(m_a << 24) >> 24) < 0 :
				op == 0xfa4d ? (m_b & ACC_MASK) == 0 :
				op == 0xfa4f ? (s64(m_b << 24) >> 24) <= 0 :
				op == 0xfa30 ? bool(m_st0 & 0x1000) : !(m_st0 & 0x1000);
		m_icount -= 2;
		if (condition)
		{
			m_delayed_target = destination;
			m_delayed_words = 2;
		}
		return;
	}
	case 0xfe00: // RETD
		m_delayed_target = pop();
		m_delayed_words = 2;
		m_icount -= 2;
		return;
	case 0xfe44: // RCD ANEQ (SPRU172C, RC[D])
		if ((m_a & ACC_MASK) != 0)
		{
			m_delayed_target = pop();
			m_delayed_words = 2;
		}
		m_icount -= 2;
		return;
	case 0xf495: // NOP
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
		branch_if((s64(m_a << 24) >> 24) > 0);
		return;
	case 0xf843: // BC pmad, ALT
		branch_if((s64(m_a << 24) >> 24) < 0);
		return;
	case 0xf820: // BC pmad, NTC
		branch_if(!(m_st0 & 0x1000));
		return;
	case 0xf830: // BC pmad, TC
		branch_if(m_st0 & 0x1000);
		return;
	case 0xf847: // BC pmad, ALEQ
		branch_if((s64(m_a << 24) >> 24) <= 0);
		return;
	case 0xf845: // BC pmad, AEQ
		branch_if((m_a & ACC_MASK) == 0);
		return;
	case 0xf844: // BC pmad, ANEQ
		branch_if((m_a & ACC_MASK) != 0);
		return;
	case 0xf84c: // BC pmad, BNEQ
		branch_if((m_b & ACC_MASK) != 0);
		return;
	case 0xf84d: // BC pmad, BEQ
		branch_if((m_b & ACC_MASK) == 0);
		return;
	case 0xf84e: // BC pmad, BGT
		branch_if((s64(m_b << 24) >> 24) > 0);
		return;
	case 0xf84a: // BC pmad, BGEQ
		branch_if((s64(m_b << 24) >> 24) >= 0);
		return;
	case 0xf84b: // BC pmad, BLT
		branch_if((s64(m_b << 24) >> 24) < 0);
		return;
	case 0xf84f: // BC pmad, BLEQ
		branch_if((s64(m_b << 24) >> 24) <= 0);
		return;
	case 0xf48d: // SQUR A,A
	case 0xf58d: // SQUR A,B
	{
		s64 product = s64(accumulator_high17(m_a)) * s64(accumulator_high17(m_a));
		if (BIT(m_st1, 6))
			product *= 2;
		accumulator(BIT(op, 8)) = multiply_result(product, BIT(op, 8));
		return;
	}
	case 0xf842: // BC pmad, AGEQ
		branch_if((s64(m_a << 24) >> 24) >= 0);
		return;
	case 0xf063: // AND #lk << 16, A
		m_a &= u64(fetch()) << 16;
		--m_icount;
		return;
	case 0xf062: // LD #lk, 16, A
	case 0xf162: // LD #lk, 16, B
		accumulator(BIT(op, 8)) = (data_operand(fetch()) << 16) & ACC_MASK;
		--m_icount;
		return;
	case 0xf491: // ROL A through carry
	case 0xf591: // ROL B through carry
	{
		u64 &acc = accumulator(BIT(op, 8));
		const u32 value = u32(acc);
		const u32 carry = BIT(m_st0, 11);
		m_st0 = (m_st0 & ~0x0800) | (BIT(value, 31) ? 0x0800 : 0);
		acc = u32((value << 1) | carry);
		return;
	}
	case 0xf490: // ROR A through carry
	case 0xf590: // ROR B through carry
	{
		u64 &acc = accumulator(BIT(op, 8));
		const u32 value = u32(acc);
		const u32 carry = BIT(m_st0, 11);
		m_st0 = (m_st0 & ~0x0800) | (BIT(value, 0) ? 0x0800 : 0);
		acc = (value >> 1) | (carry << 31);
		return;
	}
	default:
		if ((op & 0xfcff) == 0xf484) // NEG source accumulator, destination
		{
			const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
			accumulator(BIT(op, 8)) = add_sub(0, source, true, BIT(op, 8));
			m_st0 = (m_st0 & ~u16(0x0800)) | (source ? 0 : 0x0800);
			return;
		}
		if ((op & 0xffe0) == 0xed00) // LD #k5, ASM
		{
			m_st1 = (m_st1 & ~u16(0x001f)) | (op & 0x001f);
			return;
		}
		if ((op & 0xff80) == 0x4a00) // PSHM MMR (seven-bit address)
		{
			push(data_read(low & 0x7f));
			return;
		}
		if ((op & 0xff80) == 0x8a00) // POPM MMR (seven-bit address)
		{
			data_write(low & 0x7f, pop());
			return;
		}
		if ((op & 0xff00) == 0xec00) // RPT #k
		{
			m_rptc = low;
			m_rpt_address = m_pc;
			m_rpt_end = 0xffff;
			m_rpt_iteration = 0;
			m_rpt_armed = true;
			return;
		}
		if (op == 0xf070) // RPT #lk
		{
			m_rptc = fetch();
			m_rpt_address = m_pc;
			m_rpt_end = 0xffff;
			m_rpt_iteration = 0;
			m_rpt_armed = true;
			return;
		}
		if ((op & 0xff00) == 0x6d00) // MAR indirect auxiliary-register modification
		{
			const unsigned mod = (low >> 3) & 0x0f;
			if (mod >= 12)
			{
				const unsigned ar = indirect_ar(low);
				const u16 offset = fetch();
				if (mod == 13)
					m_ar[ar] += s16(offset);
				else if (mod == 14)
					circular_modify(ar, s16(offset));
				if (BIT(m_st1, 5) && (low & 7))
					m_st0 = (m_st0 & ~u16(0xe000)) | u16((low & 7) << 13);
				--m_icount;
			}
			else
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
		const bool xc_guarded = m_xc_guard != 0;
		const u16 instruction_pc = m_pc;
		const bool repeat_was_armed = m_rpt_armed;
		m_op = fetch();
		if (m_opcode_first_pc[m_op] == 0xffff)
			m_opcode_first_pc[m_op] = instruction_pc;
		++m_opcode_count[m_op];
		execute_one(m_op);
		if (!m_illegal)
		{
			// RPT/RPTZ arms the next instruction; its own retirement must not
			// consume one of the requested body executions.
			if (repeat_was_armed || !m_rpt_armed)
				finish_repeats();
			const unsigned words = u16(m_pc - instruction_pc);
			if (delayed)
			{
				m_delayed_words = words >= m_delayed_words ? 0 : m_delayed_words - words;
				if (!m_delayed_words)
					m_pc = m_delayed_target;
			}
			if (xc_guarded)
				m_xc_guard = words >= m_xc_guard ? 0 : m_xc_guard - words;
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
