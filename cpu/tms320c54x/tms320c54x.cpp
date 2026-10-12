// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "tms320c54x.h"

#include <sstream>

namespace {

constexpr u16 ST1_BRAF = 0x8000;

constexpr bool staged_immediate_alu(u16 word)
{
	u16 const form = word & 0xfcf0;
	return form == 0xf000 || form == 0xf030 || form == 0xf040 || form == 0xf050;
}

constexpr bool staged_conditional_branch(u16 word)
{
	return word == 0xfa44 || word == 0xfa45 || word == 0xfa4c || word == 0xfa4d ||
		word == 0xf844 || word == 0xf845 || word == 0xf84c || word == 0xf84d;
}

constexpr bool staged_call(u16 word)
{
	return word == 0xf074 || word == 0xf274;
}

constexpr bool staged_move(u16 word)
{
	return (word & 0xff00) == 0x7100 && u8(word) < 0x98;
}

constexpr bool staged_xc(u16 word)
{
	u16 const form = word & ~u16(0x0200);
	return form == 0xfd00 || form == 0xfd20 || form == 0xfd30 ||
		form == 0xfd44 || form == 0xfd45 || form == 0xfd4c || form == 0xfd4d;
}

constexpr bool staged_rmw(u16 word)
{
	return (word & 0xfe00) == 0x6800 || (word & 0xff00) == 0x6b00;
}

constexpr bool staged_store(u16 word)
{
	return ((word & 0xfc00) == 0x8000 || (word & 0xfe00) == 0x8c00) && u8(word) < 0x98;
}

u16 reverse_carry_index(u16 address, u16 index, bool subtract)
{
	// SPRU131G 5.5.3.5: carry/borrow propagates from the MSB to the LSB.
	const u16 left = bitswap<16>(address, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	const u16 right = bitswap<16>(index, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	return bitswap<16>(u16(subtract ? left - right : left + right), 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}

s32 accumulator_high17(u64 value)
{
	const u32 high = (value >> 16) & 0x1ffff;
	return (high & 0x10000) ? s32(high) - 0x20000 : s32(high);
}

class tms320c54x_disassembler : public util::disasm_interface
{
public:
	explicit tms320c54x_disassembler(bool extended) : m_extended(extended) { }
	virtual u32 opcode_alignment() const override { return 1; }
	virtual u32 interface_flags() const override { return PAGED; }
	virtual u32 page_address_bits() const override { return 16; }

	virtual offs_t disassemble(std::ostream &stream, offs_t pc,
			const data_buffer &opcodes, const data_buffer &params) override
	{
		u16 const op = opcodes.r16(pc);
		auto const next_word = [&](unsigned index = 1) { return opcodes.r16((pc & 0x7f0000) | u16(pc + index)); };
		auto const target = [&]() { return (pc & 0x7f0000) | next_word(); };
		u8 const mode = op;
		u16 const family = op & 0xff00;
		auto const predicate_name = [](u8 predicate) -> char const * {
			switch (predicate)
			{
			case 0x02: return "NBIO"; case 0x03: return "BIO";
			case 0x08: return "NC"; case 0x0c: return "C";
			case 0x20: return "NTC"; case 0x30: return "TC";
			case 0x42: return "AGEQ"; case 0x43: return "ALT";
			case 0x44: return "ANEQ"; case 0x45: return "AEQ";
			case 0x46: return "AGT"; case 0x47: return "ALEQ";
			case 0x4a: return "BGEQ"; case 0x4b: return "BLT";
			case 0x4c: return "BNEQ"; case 0x4d: return "BEQ";
			case 0x4e: return "BGT"; case 0x4f: return "BLEQ";
			case 0x60: return "ANOV"; case 0x68: return "BNOV";
			case 0x70: return "AOV"; case 0x78: return "BOV";
			default: return nullptr;
			}
		};
		char const *const predicate = predicate_name(mode);
		if (predicate && (op & 0xfd00) == 0xfc00)
		{
			bool const delayed = BIT(op, 9);
			util::stream_format(stream, "%-8s%s", delayed ? "RCD" : "RC", predicate);
			return 1 | SUPPORTED | STEP_COND | (delayed ? 0 : STEP_OUT);
		}
		if (predicate && ((op & 0xfd80) == 0xf800 || (op & 0xfd80) == 0xf900))
		{
			bool const call = BIT(op, 8), delayed = BIT(op, 9);
			char const *name = call ? (delayed ? "CCD" : "CC") : (delayed ? "BCD" : "BC");
			util::stream_format(stream, "%-8s$%06X, %s", name, target(), predicate);
			return 2 | SUPPORTED | STEP_COND | (call && !delayed ? STEP_OVER : 0);
		}
		if (predicate && (op & 0xfd80) == 0xfd00)
		{
			util::stream_format(stream, "XC      %u, %s", BIT(op, 9) ? 2 : 1, predicate);
			return 1 | SUPPORTED | STEP_COND;
		}
		auto const smem = [&](u16 offset) {
			if (mode < 0x80)
				util::stream_format(stream, "$%02X", mode);
			else if (mode >= 0xf8)
				util::stream_format(stream, "*($%04X)", offset);
			else if (mode >= 0xe0)
				util::stream_format(stream, "%sAR%u($%04X)%s", mode < 0xe8 ? "*" : "*+", mode & 7, offset, mode >= 0xf0 ? "%" : "");
			else
			{
				static constexpr char const *forms[] = {
					"*AR%u", "*AR%u-", "*AR%u+", "*+AR%u", "*AR%u-0B", "*AR%u-0",
					"*AR%u+0", "*AR%u+0B", "*AR%u-%%", "*AR%u-0%%", "*AR%u+%%", "*AR%u+0%%"
				};
				util::stream_format(stream, forms[(mode >> 3) & 15], mode & 7);
			}
		};
		if (family == 0x6f00)
		{
			bool const extended = mode >= 0xe0;
			u16 const offset = extended ? next_word() : 0;
			u16 const extension = next_word(extended ? 2 : 1);
			bool const arithmetic = (extension & 0xfce0) == 0x0c00 || (extension & 0xfce0) == 0x0c20;
			bool const load = (extension & 0xfee0) == 0x0c40;
			bool const high = (extension & 0xfee0) == 0x0c60;
			bool const low = (extension & 0xfee0) == 0x0c80;
			if (arithmetic || load || high || low)
			{
				int const shift = int(extension & 31) - (BIT(extension, 4) ? 32 : 0);
				char const *name = arithmetic ? (BIT(extension, 5) ? "SUB" : "ADD") : load ? "LD" : high ? "STH" : "STL";
				util::stream_format(stream, "%-8s", name);
				if (high || low) util::stream_format(stream, "%c, %d, ", BIT(extension, 8) ? 'B' : 'A', shift);
				smem(offset);
				if (arithmetic) util::stream_format(stream, ", %d, %c, %c", shift,
					BIT(extension, 9) ? 'B' : 'A', BIT(extension, 8) ? 'B' : 'A');
				if (load) util::stream_format(stream, ", %d, %c", shift, BIT(extension, 8) ? 'B' : 'A');
				return (2 + extended) | SUPPORTED;
			}
		}
		if ((family == 0x6c00 || family == 0x6e00) && mode >= 0x80 && mode < 0xf8)
		{
			bool const extended = mode >= 0xe0;
			util::stream_format(stream, "%-8s$%06X, ", family == 0x6e00 ? "BANZD" : "BANZ",
				(pc & 0x7f0000) | next_word(extended ? 2 : 1));
			smem(extended ? next_word() : 0);
			return (2 + extended) | SUPPORTED | STEP_COND;
		}
		if ((family >= 0x5000 && family <= 0x5500) || family == 0x5800 || family == 0x5900)
		{
			bool const extended = mode >= 0xe0;
			char const *name = family < 0x5400 ? "DADD" : family < 0x5800 ? "DSUB" : "DRSUB";
			util::stream_format(stream, "%-8s", name); smem(extended ? next_word() : 0);
			if (family < 0x5400) util::stream_format(stream, ", %c", BIT(op, 9) ? 'B' : 'A');
			util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x4e00 || family == 0x4f00 || family == 0x5600 || family == 0x5700)
		{
			bool const extended = mode >= 0xe0;
			bool const store = family < 0x5000;
			util::stream_format(stream, "%-8s", store ? "DST" : "DLD");
			if (store) util::stream_format(stream, "%c, ", BIT(op, 8) ? 'B' : 'A');
			smem(extended ? next_word() : 0);
			if (!store) util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if ((family >= 0x3c00 && family <= 0x3f00) || (family >= 0x4000 && family <= 0x4300))
		{
			bool const extended = mode >= 0xe0;
			util::stream_format(stream, "%-8s", family < 0x4000 ? "ADD" : "SUB");
			smem(extended ? next_word() : 0);
			util::stream_format(stream, ", 16, %c, %c", BIT(op, 9) ? 'B' : 'A', BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x6d00 && mode >= 0x80)
		{
			bool const extended = mode >= 0xe0;
			stream << "MAR     "; smem(extended ? next_word() : 0);
			return (1 + extended) | SUPPORTED;
		}
		auto const dual = [&](u8 operand) {
			static constexpr char const *forms[] = {"*AR%u", "*AR%u-", "*AR%u+", "*AR%u+0%%"};
			util::stream_format(stream, forms[(operand >> 2) & 3], 2 + (operand & 3));
		};
		if ((op & 0xfc00) == 0x9800)
		{
			util::stream_format(stream, "%-8s%c, %u, ", BIT(op, 9) ? "STH" : "STL", BIT(op, 8) ? 'B' : 'A', op & 15);
			dual(op >> 4);
			return 1 | SUPPORTED;
		}
		if ((op & 0xfc00) == 0xa000)
		{
			util::stream_format(stream, "%-8s", BIT(op, 9) ? "SUB" : "ADD");
			dual(op >> 4); stream << ", "; dual(op);
			util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if (family == 0xe500)
		{
			stream << "MVDD    "; dual(op >> 4); stream << ", "; dual(op);
			return 1 | SUPPORTED;
		}
		if ((op & 0xfc00) == 0xc800 || (op & 0xfd00) == 0xe400)
		{
			bool const to_t = (op & 0xfd00) == 0xe400;
			util::stream_format(stream, "ST      %c, ", BIT(op, 9) ? 'B' : 'A'); dual(op);
			stream << " || LD "; dual(op >> 4);
			util::stream_format(stream, ", %c", to_t ? 'T' : BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if (family == 0xe200 || (op & 0xf000) == 0xd000)
		{
			if (family == 0xe200)
			{
				stream << "SQDST   "; dual(op >> 4); stream << ", "; dual(op);
			}
			else
			{
				util::stream_format(stream, "ST      %c, ", BIT(op, 9) ? 'B' : 'A'); dual(op);
				char const *name = BIT(op, 11) ? (BIT(op, 10) ? "MASR" : "MAS") : (BIT(op, 10) ? "MACR" : "MAC");
				util::stream_format(stream, " || %s ", name); dual(op >> 4);
				util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			}
			return 1 | SUPPORTED;
		}
		if ((op & 0xfe00) == 0xa400 || (op & 0xf000) == 0xb000)
		{
			bool const multiply = (op & 0xfe00) == 0xa400;
			char const *name = multiply ? "MPY" : BIT(op, 11) ? (BIT(op, 10) ? "MASR" : "MAS") : (BIT(op, 10) ? "MACR" : "MAC");
			util::stream_format(stream, "%-8s", name);
			dual(op >> 4); stream << ", "; dual(op);
			if (!multiply) util::stream_format(stream, ", %c", BIT(op, 9) ? 'B' : 'A');
			util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if (family <= 0x0f00)
		{
			static constexpr char const *names[] = {"ADD", "ADDS", "ADD", "ADDC", "SUB", "SUBS", "SUB", "SUBB"};
			bool const extended = mode >= 0xe0;
			util::stream_format(stream, "%-8s", names[family >> 9]);
			smem(extended ? next_word() : 0);
			util::stream_format(stream, ", %s%s", (family & 0x0600) == 0x0400 ? "TS, " : "", BIT(op, 8) ? "B" : "A");
			return (1 + extended) | SUPPORTED;
		}
		if ((family == 0x7200 || family == 0x7300) && mode < 0xe0)
		{
			if (family == 0x7200)
			{
				util::stream_format(stream, "MVDM    $%04X, ", next_word()); smem(0);
			}
			else
			{
				stream << "MVMD    "; smem(0); util::stream_format(stream, ", $%04X", next_word());
			}
			return 2 | SUPPORTED;
		}
		if (family == 0x7c00)
		{
			bool const extended = mode >= 0xe0;
			u16 const first = next_word();
			u16 const second = extended ? next_word(2) : 0;
			util::stream_format(stream, "MVPD    $%04X, ", mode >= 0xf8 ? second : first);
			smem(mode >= 0xf8 ? first : second);
			return (2 + extended) | SUPPORTED;
		}
		if (family == 0x4b00 || family == 0x8b00 || family == 0x7f00)
		{
			bool const extended = mode >= 0xe0;
			util::stream_format(stream, "%-8s", family == 0x4b00 ? "PSHD" : family == 0x8b00 ? "POPD" : "WRITA");
			smem(extended ? next_word() : 0);
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x4700 || family == 0x8c00 || family == 0x8d00)
		{
			bool const extended = mode >= 0xe0;
			if (family == 0x4700) stream << "RPT     ";
			else util::stream_format(stream, "ST      %s, ", family == 0x8c00 ? "T" : "TRN");
			smem(extended ? next_word() : 0);
			return (1 + extended) | SUPPORTED;
		}
		if (family >= 0x8000 && family <= 0x8700)
		{
			bool const extended = mode >= 0xe0;
			util::stream_format(stream, "%-8s%c, %s", BIT(op, 9) ? "STH" : "STL", BIT(op, 8) ? 'B' : 'A',
				BIT(op, 10) ? "ASM, " : "");
			smem(extended ? next_word() : 0);
			return (1 + extended) | SUPPORTED;
		}
		if ((family == 0x4800 || family == 0x4900 || family == 0x4a00 || family == 0x8800 || family == 0x8900 ||
			family == 0x8a00) && mode < 0xe0)
		{
			bool const load = family == 0x4800 || family == 0x4900;
			bool const store = family == 0x8800 || family == 0x8900;
			char const *name = load ? "LDM" : store ? "STLM" : family == 0x4a00 ? "PSHM" : "POPM";
			util::stream_format(stream, "%-8s", name);
			if (store) util::stream_format(stream, "%c, ", BIT(op, 8) ? 'B' : 'A');
			smem(0);
			if (load) util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((family >= 0x2000 && family <= 0x2500) || family == 0x3400)
		{
			bool const extended = mode >= 0xe0;
			char const *name = family == 0x3400 ? "BITT" : family >= 0x2400 ? "MPYU" : family >= 0x2200 ? "MPYR" : "MPY";
			util::stream_format(stream, "%-8s", name); smem(extended ? next_word() : 0);
			if (family != 0x3400) util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family >= 0x2800 && family <= 0x2f00)
		{
			bool const extended = mode >= 0xe0;
			char const *name = BIT(op, 10) ? (BIT(op, 9) ? "MASR" : "MAS") : (BIT(op, 9) ? "MACR" : "MAC");
			util::stream_format(stream, "%-8s", name); smem(extended ? next_word() : 0);
			util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x3100 || family == 0x3300 || family == 0x3500 || family == 0x3700)
		{
			bool const extended = mode >= 0xe0;
			char const *name = family == 0x3100 ? "MPYA" : family == 0x3300 ? "MASA" : family == 0x3500 ? "MACA" : "MACAR";
			util::stream_format(stream, "%-8s", name); smem(extended ? next_word() : 0);
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x3000 || family == 0x3200 || family == 0x4600)
		{
			bool const extended = mode >= 0xe0;
			stream << "LD      "; smem(extended ? next_word() : 0);
			util::stream_format(stream, ", %s", family == 0x3000 ? "T" : family == 0x3200 ? "ASM" : "DP");
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x1200 || family == 0x1300)
		{
			bool const extended = mode >= 0xe0;
			stream << "LDU     "; smem(extended ? next_word() : 0);
			util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x1000 || family == 0x1100 || family == 0x1400 || family == 0x1500 ||
			family == 0x4400 || family == 0x4500)
		{
			bool const extended = mode >= 0xe0;
			stream << "LD      "; smem(extended ? next_word() : 0);
			util::stream_format(stream, ", %s%c", family >= 0x4400 ? "16, " : family >= 0x1400 ? "TS, " : "",
				BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x1e00 || family == 0x1f00)
		{
			bool const extended = mode >= 0xe0;
			stream << "SUBC    "; smem(extended ? next_word() : 0);
			util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family >= 0x1800 && family <= 0x1d00)
		{
			static constexpr char const *names[] = {"AND", "OR", "XOR"};
			bool const extended = mode >= 0xe0;
			util::stream_format(stream, "%-8s", names[(family - 0x1800) >> 9]);
			smem(extended ? next_word() : 0);
			util::stream_format(stream, ", %c", BIT(op, 8) ? 'B' : 'A');
			return (1 + extended) | SUPPORTED;
		}
		if (family == 0x6000 || family == 0x6100 || family == 0x7000 || family == 0x7100 ||
			family == 0x7400 || family == 0x7500 || family == 0x7d00 || family == 0x7e00)
		{
			bool const extended = mode >= 0xe0;
			bool const address_first = !(mode >= 0xe0 && mode < 0xf8 &&
				(family == 0x7400 || family == 0x7500 || family == 0x7d00));
			u16 const offset = extended ? next_word(address_first ? 1 : 2) : 0;
			if (family == 0x7e00)
			{
				stream << "READA   "; smem(offset);
				return (1 + extended) | SUPPORTED;
			}
			u16 const operand = next_word(extended && address_first ? 2 : 1);
			char const *name = family == 0x6000 ? "CMPM" : family == 0x6100 ? "BITF" :
				family == 0x7000 ? "MVKD" : family == 0x7100 ? "MVDK" : family == 0x7400 ? "PORTR" :
				family == 0x7500 ? "PORTW" : "MVDP";
			util::stream_format(stream, "%-8s", name);
			if (family == 0x7000 || family == 0x7400)
			{
				util::stream_format(stream, "$%04X, ", operand); smem(offset);
			}
			else
			{
				smem(offset);
				util::stream_format(stream, ", %s$%04X", family == 0x6000 || family == 0x6100 ? "#" : "", operand);
			}
			return (2 + extended) | SUPPORTED;
		}
		if (family == 0x6800 || family == 0x6900 || family == 0x6b00 || family == 0x7600 || (family == 0x7700 && mode < 0xe0))
		{
			bool const extended = family != 0x7700 && mode >= 0xe0;
			bool const immediate_first = (family == 0x6800 || family == 0x6900) && mode >= 0xe0 && mode < 0xf8;
			u16 const immediate = next_word(extended && !immediate_first ? 2 : 1);
			u16 const offset = extended ? next_word(immediate_first ? 2 : 1) : 0;
			char const *const name = family == 0x6800 ? "ANDM" : family == 0x6900 ? "ORM" : family == 0x6b00 ? "ADDM" : family == 0x7600 ? "ST" : "STM";
			util::stream_format(stream, "%-8s#$%04X, ", name, immediate);
			smem(offset);
			return (2 + extended) | SUPPORTED;
		}
		if ((op & 0xffe0) == 0xf7c0 || (op & 0xffe0) == 0xf4c0)
		{
			util::stream_format(stream, "%s    %u", BIT(op, 8) ? "INTR" : "TRAP", op & 31);
			return 1 | SUPPORTED | STEP_OVER;
		}
		if (m_extended && (op & 0xfc80) == 0xf880)
		{
			bool const call = BIT(op, 8), delayed = BIT(op, 9);
			const char *name = call ? (delayed ? "FCALLD" : "FCALL") : (delayed ? "FBD" : "FB");
			util::stream_format(stream, "%-8s$%06X", name,
				(u32(op & 0x7f) << 16) | next_word());
			return 2 | SUPPORTED | (call && !delayed ? STEP_OVER : 0);
		}
		if (m_extended && (op & 0xfdfe) == 0xf4e4)
		{
			bool const delayed = BIT(op, 9);
			util::stream_format(stream, "%s%s", BIT(op, 0) ? "FRETE" : "FRET", delayed ? "D" : "");
			return 1 | SUPPORTED | (delayed ? 0 : STEP_OUT);
		}
		if (m_extended && (op & 0xfcfe) == 0xf4e6)
		{
			bool const call = BIT(op, 0), delayed = BIT(op, 9);
			const char *name = call ? (delayed ? "FCALAD" : "FCALA") : (delayed ? "FBACCD" : "FBACC");
			util::stream_format(stream, "%-8s%c", name, BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED | (call && !delayed ? STEP_OVER : 0);
		}
		if ((op & 0xfcff) == 0xf4e1 && op != 0xf7e1)
		{
			util::stream_format(stream, "IDLE    %u", BIT(op, 9) ? 2 : BIT(op, 8) ? 3 : 1);
			return 1 | SUPPORTED;
		}
		if ((op & 0xfdf0) == 0xf5b0 || (op & 0xfdf0) == 0xf4b0)
		{
			util::stream_format(stream, "%s    %u, %u", BIT(op, 8) ? "SSBX" : "RSBX", BIT(op, 9), op & 15);
			return 1 | SUPPORTED;
		}
		if ((op & 0xfe00) == 0xe800)
		{
			util::stream_format(stream, "LD      #$%02X, %c", op & 255, BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfff8) == 0xf4a0)
		{
			util::stream_format(stream, "LD      #%u, ARP", op & 7);
			return 1 | SUPPORTED;
		}
		if ((op & 0xfe00) == 0xea00)
		{
			util::stream_format(stream, "LD      #%u, DP", op & 0x1ff);
			return 1 | SUPPORTED;
		}
		if ((op & 0xffe0) == 0xed00)
		{
			int const immediate = (op & 0x10) ? int(op & 31) - 32 : int(op & 31);
			util::stream_format(stream, "LD      #%d, ASM", immediate);
			return 1 | SUPPORTED;
		}
		if (family == 0xee00)
		{
			util::stream_format(stream, "FRAME   #%d", mode & 0x80 ? int(mode) - 256 : int(mode));
			return 1 | SUPPORTED;
		}
		if (family == 0xe700 && (mode >> 4) <= 8 && (mode & 15) <= 8)
		{
			stream << "MVMM    ";
			if ((mode >> 4) == 8) stream << "SP";
			else util::stream_format(stream, "AR%u", mode >> 4);
			stream << ", ";
			if ((mode & 15) == 8) stream << "SP";
			else util::stream_format(stream, "AR%u", mode & 15);
			return 1 | SUPPORTED;
		}
		if ((op & 0xff00) == 0xec00)
		{
			util::stream_format(stream, "RPT     #%u", op & 255);
			return 1 | SUPPORTED;
		}
		u16 const immediate_form = op & 0xfcf0;
		u16 const immediate_high = op & 0xfcff;
		if ((op & 0xfef0) == 0xf020 || (op & 0xfeff) == 0xf062)
		{
			util::stream_format(stream, "LD      #$%04X, %u, %c", next_word(), (op & 255) == 0x62 ? 16 : op & 15,
				BIT(op, 8) ? 'B' : 'A');
			return 2 | SUPPORTED;
		}
		if (immediate_form == 0xf000 || immediate_form == 0xf010 || immediate_form == 0xf030 ||
			immediate_form == 0xf040 || immediate_form == 0xf050 ||
			(immediate_high >= 0xf060 && immediate_high <= 0xf065 && immediate_high != 0xf062))
		{
			static constexpr char const *names[] = {"ADD", "SUB", "LD", "AND", "OR", "XOR"};
			bool const high = immediate_high >= 0xf060;
			char const *name = names[high ? op & 15 : (op >> 4) & 15];
			util::stream_format(stream, "%-8s#$%04X, %u, %c, %c", name, next_word(), high ? 16 : op & 15,
				BIT(op, 9) ? 'B' : 'A', BIT(op, 8) ? 'B' : 'A');
			return 2 | SUPPORTED;
		}
		if ((op & 0xfeff) == 0xf48c || (op & 0xfeff) == 0xf48d)
		{
			if (BIT(op, 0)) util::stream_format(stream, "SQUR    A, %c", BIT(op, 8) ? 'B' : 'A');
			else util::stream_format(stream, "MPYA    %c", BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfcfc) == 0xf488)
		{
			char const *name = BIT(op, 1) ? (BIT(op, 0) ? "MASAR" : "MASA") : (BIT(op, 0) ? "MACAR" : "MACA");
			util::stream_format(stream, "%-8sT, %c, %c", name, BIT(op, 9) ? 'B' : 'A', BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfcff) == 0xf484 || (op & 0xfcff) == 0xf485 || (op & 0xfcff) == 0xf48f || (op & 0xfcff) == 0xf49f)
		{
			char const *name = (op & 255) == 0x84 ? "NEG" : (op & 255) == 0x85 ? "ABS" : (op & 255) == 0x8f ? "NORM" : "RND";
			util::stream_format(stream, "%-8s%c, %c", name, BIT(op, 9) ? 'B' : 'A', BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfeff) == 0xf483 || (op & 0xfeff) == 0xf486 || (op & 0xfeff) == 0xf487 || (op & 0xfeff) == 0xf48e)
		{
			char const *name = (op & 255) == 0x83 ? "SAT" : (op & 255) == 0x86 ? "MAX" : (op & 255) == 0x87 ? "MIN" : "EXP";
			util::stream_format(stream, "%-8s%c", name, BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfcff) == 0xf493)
		{
			util::stream_format(stream, "CMPL    %c, %c", BIT(op, 9) ? 'B' : 'A', BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfcff) == 0xf480 || (op & 0xfcff) == 0xf481 || (op & 0xfcff) == 0xf482)
		{
			char const *name = (op & 255) == 0x80 ? "ADD" : (op & 255) == 0x81 ? "SUB" : "LD";
			util::stream_format(stream, "%-8s%c, ASM, %c", name, BIT(op, 9) ? 'B' : 'A', BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfce0) == 0xf460 || (op & 0xfce0) == 0xf440 || (op & 0xfce0) == 0xf0c0 ||
			(op & 0xfce0) == 0xf400 || (op & 0xfce0) == 0xf420 ||
			(op & 0xfce0) == 0xf080 || (op & 0xfce0) == 0xf0a0 || (op & 0xfce0) == 0xf0e0)
		{
			int const shift = (op & 0x10) ? int(op & 31) - 32 : int(op & 31);
			char const *name = (op & 0xfce0) == 0xf460 ? "SFTA" : (op & 0xfce0) == 0xf440 ? "LD" :
				(op & 0xfce0) == 0xf400 ? "ADD" : (op & 0xfce0) == 0xf420 ? "SUB" :
				(op & 0xfce0) == 0xf080 ? "AND" : (op & 0xfce0) == 0xf0a0 ? "OR" :
				(op & 0xfce0) == 0xf0e0 ? "SFTL" : "XOR";
			util::stream_format(stream, "%-8s%c, %d, %c", name, BIT(op, 9) ? 'B' : 'A', shift, BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		if ((op & 0xfeff) == 0xf490 || (op & 0xfeff) == 0xf491 || (op & 0xfeff) == 0xf492)
		{
			char const *name = (op & 255) == 0x90 ? "ROR" : (op & 255) == 0x91 ? "ROL" : "ROLTC";
			util::stream_format(stream, "%-8s%c", name, BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		}
		switch (op)
		{
		case 0xf495: stream << "NOP"; return 1 | SUPPORTED;
		case 0xfc00: stream << "RET"; return 1 | SUPPORTED | STEP_OUT;
		case 0xf4eb: stream << "RETE"; return 1 | SUPPORTED | STEP_OUT;
		case 0xf49b: stream << "RETF"; return 1 | SUPPORTED | STEP_OUT;
		case 0xf69b: stream << "RETFD"; return 1 | SUPPORTED;
		case 0xfe00: stream << "RETD"; return 1 | SUPPORTED;
		case 0xf4e2: case 0xf5e2:
			util::stream_format(stream, "BACC    %c", BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED;
		case 0xf4e3: case 0xf5e3: case 0xf6e3: case 0xf7e3:
			util::stream_format(stream, "%-8s%c", BIT(op, 9) ? "CALAD" : "CALA", BIT(op, 8) ? 'B' : 'A');
			return 1 | SUPPORTED | (BIT(op, 9) ? 0 : STEP_OVER);
		case 0xf073: case 0xf273: case 0xf074: case 0xf274:
		{
			const char *name = (op & 255) == 0x74 ? (BIT(op, 9) ? "CALLD" : "CALL") : (BIT(op, 9) ? "BD" : "B");
			util::stream_format(stream, "%-8s$%06X", name, target());
			return 2 | SUPPORTED | (op == 0xf074 ? STEP_OVER : 0);
		}
		case 0xf072: case 0xf272:
			util::stream_format(stream, "%-8s$%06X", BIT(op, 9) ? "RPTBD" : "RPTB", target());
			return 2 | SUPPORTED;
		case 0xf070:
			util::stream_format(stream, "RPT     #$%04X", next_word());
			return 2 | SUPPORTED;
		case 0xf071: case 0xf171:
			util::stream_format(stream, "RPTZ    %c, #$%04X", BIT(op, 8) ? 'B' : 'A', next_word());
			return 2 | SUPPORTED;
		}
		// Unknown forms retain their raw word without claiming a decoded length.
		util::stream_format(stream, ".word   $%04X", op);
		return 1;
	}
private:
	bool const m_extended;
};

} // anonymous namespace

DEFINE_DEVICE_TYPE(TMS320C54X, tms320c54x_device, "tms320c54x",
		"Texas Instruments TMS320C54x")

tms320c54x_device::tms320c54x_device(const machine_config &mconfig,
		const char *tag, device_t *owner, u32 clock) :
	cpu_device(mconfig, TMS320C54X, tag, owner, clock),
	m_program_config("program", ENDIANNESS_LITTLE, 16, 23, -1),
	m_data_config("data", ENDIANNESS_LITTLE, 16, 16, -1),
	m_io_config("io", ENDIANNESS_LITTLE, 16, 16, -1),
	m_bio_in_cb(*this, 1),
	m_xf_out_cb(*this),
	m_peripheral_clock_stop_cb(*this),
	m_pll_clock_stop_cb(*this),
	m_system_clock_cb(*this)
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
	return std::make_unique<tms320c54x_disassembler>(m_extended_program);
}

void tms320c54x_device::device_start()
{
	m_imr = m_power_on_imr;
	m_reset_clock = unscaled_clock();
	m_timer_clock = clock();
	if (m_follow_clkmd && !m_pll_reference_clock)
		fatalerror("C54x CLKMD rate selection requires an input clock");
	if (m_reset_clkmd_enabled && BIT(m_reset_clkmd, 0) != BIT(m_reset_clkmd, 1))
		fatalerror("C54x reset CLKMD requires matching requested and actual source selection");
	m_opcode_first_pc.fill(0xffff);
	m_timer = timer_alloc(FUNC(tms320c54x_device::timer_expired), this);
	m_pll_timer = timer_alloc(FUNC(tms320c54x_device::pll_transition_complete), this);
	m_clock_timer = timer_alloc(FUNC(tms320c54x_device::selected_clock_complete), this);
	// CPU-owned MMRs take precedence over broad board RAM windows.
	space(AS_DATA).install_device(0, 0x58, *this, &tms320c54x_device::internal_data_map);
	space(AS_PROGRAM).cache(m_cache);
	space(AS_PROGRAM).specific(m_program);
	space(AS_DATA).specific(m_data);
	space(AS_IO).specific(m_io);

	set_icountptr(m_icount);

	state_add(STATE_PC, "PC", m_pc).callimport().formatstr("%04X");
	state_add(STATE_XPC, "XPC", m_xpc).mask(0x7f).callimport().formatstr("%02X");
	state_add(STATE_A, "A", m_a).mask(ACC_MASK).formatstr("%010X");
	state_add(STATE_B, "B", m_b).mask(ACC_MASK).formatstr("%010X");
	state_add(STATE_T, "T", m_t).formatstr("%04X");
	state_add(STATE_TRN, "TRN", m_trn).formatstr("%04X");
	state_add(STATE_RTN, "RTN", m_rtn).formatstr("%04X");
	state_add(STATE_SP, "SP", m_sp).formatstr("%04X");
	state_add(STATE_ST0, "ST0", m_st0).formatstr("%04X");
	state_add(STATE_ST1, "ST1", m_st1).callimport().formatstr("%04X");
	state_add(STATE_PMST, "PMST", m_pmst).formatstr("%04X");
	for (unsigned i = 0; i != std::size(m_ar); ++i)
		state_add(STATE_AR0 + i, string_format("AR%u", i).c_str(), m_ar[i]).formatstr("%04X");
	state_add(STATE_BRC, "BRC", m_brc).formatstr("%04X");
	state_add(STATE_BK, "BK", m_bk).formatstr("%04X");
	state_add(STATE_IFR, "IFR", m_ifr).formatstr("%04X");
	state_add(STATE_IMR, "IMR", m_imr).formatstr("%04X");
	state_add(STATE_TIM, "TIM", m_debug_timer).callimport().callexport().formatstr("%04X");
	state_add(STATE_PRD, "PRD", m_debug_timer).callimport().callexport().formatstr("%04X");
	state_add(STATE_TCR, "TCR", m_debug_timer).callimport().callexport().formatstr("%04X");
	state_add(STATE_IDLE, "IDLE", m_idle).callimport().formatstr("%1u");
	state_add(STATE_ILLEGAL, "ILLEGAL", m_illegal).formatstr("%1u");
	state_add(STATE_GENPC, "GENPC", m_debug_pc).mask(0x7fffff).callimport().callexport().noshow();
	state_add(STATE_GENPCBASE, "CURPC", m_debug_pc).mask(0x7fffff).callimport().callexport().noshow();

	save_item(NAME(m_pc));
	save_item(NAME(m_xpc));
	save_item(NAME(m_op));
	save_item(NAME(m_a));
	save_item(NAME(m_b));
	save_item(NAME(m_t));
	save_item(NAME(m_trn));
	save_item(NAME(m_sp));
	save_item(NAME(m_sp_address_base));
	save_item(NAME(m_sp_address_pending));
	save_item(NAME(m_sp_address_late));
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
	save_item(NAME(m_delayed_xpc));
	save_item(NAME(m_delayed_far));
	save_item(NAME(m_delayed_words));
	save_item(NAME(m_xc_guard));
	save_item(NAME(m_pipeline_xc_active));
	save_item(NAME(m_pipeline_xc_end));
	save_item(NAME(m_pipeline_xc_skip_words));
	save_item(NAME(m_ifr));
	save_item(NAME(m_imr));
	save_item(NAME(m_nmi_pending));
	save_item(NAME(m_nmi_asserted));
	save_item(NAME(m_vector_prefetch_word));
	save_item(NAME(m_vector_prefetch_valid));
	save_item(NAME(m_interrupt_entry_stage));
	save_item(NAME(m_interrupt_vector_pc));
	save_item(NAME(m_interrupt_stack_address));
	save_item(NAME(m_interrupt_ack_source));
	save_item(NAME(m_pipeline_address));
	save_item(NAME(m_pipeline_word));
	save_item(NAME(m_pipeline_operand_address));
	save_item(NAME(m_pipeline_operand));
	save_item(NAME(m_pipeline_immediate));
	save_item(NAME(m_pipeline_valid));
	save_item(NAME(m_pipeline_early_write_deferred));
	save_item(NAME(m_pipeline_peripheral_wait_paid));
	save_item(NAME(m_pipeline_intm_protected));
	save_item(NAME(m_pipeline_direct_status));
	save_item(NAME(m_pipeline_direct_sp));
	save_item(NAME(m_pipeline_intm_follow));
	save_item(NAME(m_pipeline_decode_remaining));
	save_item(NAME(m_pipeline_decode_extension));
	save_item(NAME(m_pipeline_decode_word));
	save_item(NAME(m_pipeline_decode_address));
	save_item(NAME(m_pipeline_fetch_address));
	save_item(NAME(m_pipeline_return_pc));
	save_item(NAME(m_pipeline_flush_front));
	save_item(NAME(m_pipeline_vector_pending));
	save_item(NAME(m_pipeline_decode_flush));
	save_item(NAME(m_pipeline_control_words));
	save_item(NAME(m_pipeline_delay_decode_words));
	save_item(NAME(m_pipeline_control_address));
	save_item(NAME(m_pipeline_bc_held));
	save_item(NAME(m_pipeline_bc_word));
	save_item(NAME(m_pipeline_bc_address));
	save_item(NAME(m_pipeline_retd_stage));
	save_item(NAME(m_pipeline_retd_words));
	save_item(NAME(m_pipeline_retd_addresses));
	save_item(NAME(m_pipeline_retd_target));
	save_item(NAME(m_pipeline_far_return_pending));
	save_item(NAME(m_pipeline_long_store_pending));
	save_item(NAME(m_pipeline_dp_wait));
	save_item(NAME(m_pipeline_cycles));
	save_item(NAME(m_clkmd));
	save_item(NAME(m_pll_locked));
	save_item(NAME(m_timer_clock));
	save_item(NAME(m_pending_clock));
	save_item(NAME(m_tim));
	save_item(NAME(m_prd));
	save_item(NAME(m_tcr));
	save_item(NAME(m_timer_head_tim));
	save_item(NAME(m_idle));
	save_item(NAME(m_idle_mode));
	save_item(NAME(m_idle_timer_ticks));
	save_item(NAME(m_intm_guard));
	save_item(NAME(m_illegal));
}

void tms320c54x_device::device_stop()
{
	if (!machine().options().verbose())
		return;

	class opcode_buffer : public util::disasm_interface::data_buffer
	{
	public:
		explicit opcode_buffer(u16 opcode) : m_opcode(opcode) { }
		virtual u16 r16(u32 address) const override { return address ? 0 : m_opcode; }
		virtual u8 r8(u32) const override { throw emu_fatalerror("C54x decoder census requires word reads"); }
		virtual u32 r32(u32) const override { throw emu_fatalerror("C54x decoder census requires word reads"); }
		virtual u64 r64(u32) const override { throw emu_fatalerror("C54x decoder census requires word reads"); }
	private:
		u16 const m_opcode;
	};
	for (unsigned opcode = 0; opcode != m_opcode_first_pc.size(); ++opcode)
		if (m_opcode_first_pc[opcode] != 0xffff)
		{
			machine().logerror("[opcov] op=%04x first_pc=%04x count=%u\n",
					opcode, m_opcode_first_pc[opcode], m_opcode_count[opcode]);
			opcode_buffer const buffer(opcode);
			std::ostringstream text;
			u32 const result = get_disassembler().disassemble(text, 0, buffer, buffer);
			bool const supported = result & util::disasm_interface::SUPPORTED;
			// A raw-word fallback is not an instruction-length claim. Zero-valued
			// extensions classify the decoder only; no target memory is read.
			machine().logerror("[opdecode] op=%04x supported=%u words=%u\n", opcode,
				supported, supported ? result & util::disasm_interface::LENGTHMASK : 0);
		}
	class fetched_buffer : public util::disasm_interface::data_buffer
	{
	public:
		explicit fetched_buffer(std::array<u16, 4> const &words) : m_words(words) { }
		virtual u16 r16(u32 address) const override
		{
			if (address >= m_words[0]) throw emu_fatalerror("C54x decoder exceeded captured instruction words");
			return m_words[address + 1];
		}
		virtual u8 r8(u32) const override { throw emu_fatalerror("C54x tuple decoder requires word reads"); }
		virtual u32 r32(u32) const override { throw emu_fatalerror("C54x tuple decoder requires word reads"); }
		virtual u64 r64(u32) const override { throw emu_fatalerror("C54x tuple decoder requires word reads"); }
	private:
		std::array<u16, 4> const &m_words;
	};
	for (auto const &[words, observation] : m_extension_observations)
	{
		machine().logerror("[opwords] op=%04x words=%u ext1=%04x ext2=%04x first_pc=%06x count=%u\n",
			words[1], words[0], words[2], words[3], observation.first_pc, observation.count);
		fetched_buffer const buffer(words);
		std::ostringstream text;
		u32 const result = get_disassembler().disassemble(text, 0, buffer, buffer);
		bool const supported = result & util::disasm_interface::SUPPORTED;
		machine().logerror("[optupledecode] op=%04x words=%u ext1=%04x ext2=%04x supported=%u decoded_words=%u\n",
			words[1], words[0], words[2], words[3], supported,
			supported ? result & util::disasm_interface::LENGTHMASK : 0);
	}
}

void tms320c54x_device::device_reset()
{
	m_pc = 0xff80;
	m_xpc = 0;
	m_op = 0;
	m_vector_prefetch_word = 0;
	m_vector_prefetch_valid = false;
	m_interrupt_entry_stage = 0;
	m_interrupt_vector_pc = m_interrupt_stack_address = 0;
	m_interrupt_ack_source = 0;
	reset_pipeline();
	m_a = 0;
	m_b = 0;
	m_t = 0;
	m_trn = 0;
	m_sp = 0;
	// C54x silicon resets DP to page 0x1f with ARP/CNF set.  ROM4's
	// resident dispatcher relies on this state surviving loader startup.
	m_st0 = 0x181f;
	m_st1 = 0x2900;
	update_xf(true);
	m_pmst = 0xffa0;
	std::fill(std::begin(m_ar), std::end(m_ar), 0);
	m_brc = 0;
	m_bk = 0;
	m_rsa = 0;
	m_rea = 0;
	m_rtn = 0;
	m_rptc = 0;
	m_rpt_address = 0;
	m_rpt_end = REPEAT_END_UNSET;
	m_rpt_iteration = 0;
	m_rpt_armed = false;
	m_delayed_target = 0;
	m_delayed_xpc = 0;
	m_delayed_far = false;
	m_delayed_words = 0;
	m_xc_guard = 0;
	m_intm_guard = 0;
	m_sp_address_base = 0;
	m_sp_address_pending = m_sp_address_late = false;
	m_ifr = 0;
	m_nmi_pending = false;
	m_nmi_asserted = false;
	// Hardware reset sets INTM and clears IFR, but does not initialise IMR.
	// Preserve the mask written by the ROM loader across MAD2 DSP reset pulses.
	m_clkmd = m_reset_clkmd_enabled ? m_reset_clkmd : 0;
	m_pll_locked = m_reset_clkmd_enabled && BIT(m_clkmd, 0);
	m_pll_timer->adjust(attotime::never);
	m_clock_timer->adjust(attotime::never);
	m_pending_clock = 0;
	if (m_follow_clkmd)
	{
		if (m_reset_clkmd_enabled)
			publish_selected_clock();
		else
			set_unscaled_clock(m_reset_clock);
	}
	m_tim = 0xffff;
	m_prd = 0xffff;
	m_tcr = 0;
	m_timer_head_tim = 0xffff;
	m_idle_mode = 0;
	m_idle_timer_ticks = 0;
	m_peripheral_clock_stop_cb(0);
	m_pll_clock_stop_cb(0);
	arm_timer();
	m_idle = false;
	m_illegal = false;
}

u64 tms320c54x_device::timer_remaining_ticks() const
{
	const attotime remaining = m_timer->remaining();
	if (remaining <= attotime::zero)
		return 0;
	u64 ticks = remaining.as_ticks(m_timer_clock);
	// A partially remaining clock must not decrement TIM/PSC early.
	if (attotime::from_ticks(ticks, m_timer_clock) < remaining)
		++ticks;
	return ticks;
}

void tms320c54x_device::update_timer_counter()
{
	if ((m_tcr & TIMER_TSS) || (!m_timer->enabled() && m_idle_mode < 2))
		return;
	const u64 remaining = m_idle_mode >= 2 ? m_idle_timer_ticks : timer_remaining_ticks();
	const u32 divider = (m_tcr & TIMER_TDDR_MASK) + 1;
	const u64 subsequent_ticks = u64(m_timer_head_tim) * divider;
	// A new TDDR can be smaller than the current read-only PSC. The first
	// interval keeps that PSC; only later intervals reload from the new TDDR.
	u16 const psc = remaining > subsequent_ticks
			? u16(remaining - subsequent_ticks - 1)
			: remaining ? u16((remaining - 1) % divider) : 0;
	m_tim = remaining > subsequent_ticks ? m_timer_head_tim
			: remaining ? u16((remaining - 1) / divider) : 0;
	m_tcr = (m_tcr & ~TIMER_PSC_MASK) | (psc << 6);
}

void tms320c54x_device::arm_timer()
{
	if (m_tcr & TIMER_TSS)
	{
		m_timer->adjust(attotime::never);
		return;
	}
	const u64 cycles = u64(m_tim) * ((m_tcr & TIMER_TDDR_MASK) + 1) +
		((m_tcr & TIMER_PSC_MASK) >> 6) + 1;
	m_timer_head_tim = m_tim;
	if (m_idle_mode >= 2)
	{
		m_idle_timer_ticks = cycles;
		m_timer->adjust(attotime::never);
		return;
	}
	m_timer->adjust(attotime::from_ticks(cycles, clock()));
}

void tms320c54x_device::leave_idle()
{
	m_idle = false;
	if (m_idle_mode == 3)
		m_pll_clock_stop_cb(0);
	if (m_idle_mode >= 2)
		m_peripheral_clock_stop_cb(0);
	if (m_idle_mode >= 2 && !(m_tcr & TIMER_TSS))
		m_timer->adjust(attotime::from_ticks(m_idle_timer_ticks, clock()));
	m_idle_mode = 0;
}

TIMER_CALLBACK_MEMBER(tms320c54x_device::timer_expired)
{
	m_tim = m_prd;
	m_tcr = (m_tcr & ~TIMER_PSC_MASK) | ((m_tcr & TIMER_TDDR_MASK) << 6);
	m_ifr |= 0x0008; // TINT, vector 19
	// SPRU131G 6.11.1: IMR enables wake even when INTM blocks ISR entry.
	if (m_imr & 0x0008)
		leave_idle();
	arm_timer();
}

void tms320c54x_device::write_clkmd(u16 value)
{
	// SPRU131G table 8-18: protected fields remain protected until the
	// actual source changes, not merely until PLLNDIV requests DIV mode.
	bool const pll_mode = BIT(m_clkmd, 0);
	if (pll_mode)
		value = (m_clkmd & ~u16(3)) | (value & 2);
	if (!m_pll_reference_clock)
	{
		m_clkmd = (value & ~u16(1)) | BIT(value, 1);
		return;
	}
	u16 const previous = m_clkmd;
	m_clkmd = (value & ~u16(1)) | unsigned(pll_mode);
	publish_selected_clock();
	if (((previous ^ m_clkmd) & 0xf800) || !(m_clkmd & 6))
		m_pll_locked = false;
	if (BIT(m_clkmd, 1) == pll_mode)
	{
		m_pll_timer->adjust(attotime::never);
		return;
	}
	// Repeating the same request must not postpone the pending switch.
	if (m_pll_timer->enabled() && previous == m_clkmd)
		return;
	unsigned const mul = m_clkmd >> 12;
	u32 numerator, denominator;
	if (!BIT(m_clkmd, 11))
	{
		numerator = mul == 15 ? 1 : mul + 1;
		denominator = 1;
	}
	else
	{
		numerator = mul & 1 ? mul : mul + 1;
		denominator = mul & 1 ? 4 : 2;
	}
	u32 input_cycles = !BIT(m_clkmd, 1) && mul == 15 ? 12 : 6;
	if (BIT(m_clkmd, 1) && !m_pll_locked)
		input_cycles += ((m_clkmd >> 3) & 0xff) * 16;
	// Six CLKIN cycles plus 3.5 PLL cycles, with optional lock count.
	// Express both terms in the same rational clock domain before rounding.
	u64 const ticks = u64(input_cycles) * 2 * numerator + 7 * denominator;
	m_pll_timer->adjust(attotime::from_ticks(ticks, m_pll_reference_clock) / (2 * numerator));
}

TIMER_CALLBACK_MEMBER(tms320c54x_device::pll_transition_complete)
{
	m_clkmd = (m_clkmd & ~u16(1)) | BIT(m_clkmd, 1);
	if (BIT(m_clkmd, 0))
		m_pll_locked = true;
	publish_selected_clock();
}

void tms320c54x_device::publish_selected_clock()
{
	if (!m_follow_clkmd)
		return;
	u32 const mul = m_clkmd >> 12;
	u32 numerator = 1, denominator;
	if (!BIT(m_clkmd, 0))
		denominator = mul == 15 ? 4 : 2;
	else if (!BIT(m_clkmd, 11))
	{
		numerator = mul == 15 ? 1 : mul + 1;
		denominator = 1;
	}
	else
	{
		numerator = mul & 1 ? mul : mul + 1;
		denominator = mul & 1 ? 4 : 2;
	}
	u64 const hz = u64(m_pll_reference_clock) * numerator;
	if (hz % denominator || hz / denominator > 0xffffffffU || !hz)
		fatalerror("C54x selected clock cannot be represented as an integer device rate");
	m_pending_clock = hz / denominator;
	if (m_pending_clock == unscaled_clock())
	{
		m_clock_timer->adjust(attotime::never);
		return;
	}
	if (executing())
	{
		// Changing rate inside execute_run would re-price elapsed timeslice
		// cycles at the new rate. End that slice in the old domain first.
		m_clock_timer->adjust(attotime::zero);
	}
	else
	{
		m_clock_timer->adjust(attotime::never);
		set_unscaled_clock(m_pending_clock);
	}
}

TIMER_CALLBACK_MEMBER(tms320c54x_device::selected_clock_complete)
{
	set_unscaled_clock(m_pending_clock);
}

void tms320c54x_device::device_clock_changed()
{
	// MAME has already changed clock(). Retain whole pending source clocks;
	// do not count timer ticks early or restart TIM/PSC at a PLL transition.
	if (m_timer && m_timer_clock && m_timer_clock != clock())
	{
		u64 const ticks = timer_remaining_ticks();
		if (m_timer->enabled() && !(m_tcr & TIMER_TSS) && m_idle_mode < 2)
			m_timer->adjust(attotime::from_ticks(ticks, clock()));
	}
	m_timer_clock = clock();
	m_system_clock_cb(clock());
}

u16 tms320c54x_device::fetch()
{
	u16 value;
	if (m_vector_prefetch_valid)
	{
		m_vector_prefetch_valid = false;
		++m_pc;
		value = m_vector_prefetch_word;
	}
	else
		value = m_cache.read_word(program_address(m_pc++));
	if (m_capture_extensions)
	{
		if (m_instruction_word_count < m_instruction_words.size())
			m_instruction_words[m_instruction_word_count] = value;
		++m_instruction_word_count;
	}
	return value;
}

void tms320c54x_device::state_import(const device_state_entry &entry)
{
	if (entry.index() == STATE_PC || entry.index() == STATE_XPC)
	{
		m_vector_prefetch_valid = false;
		m_interrupt_entry_stage = 0;
		reset_pipeline();
	}
	if (entry.index() == STATE_GENPC || entry.index() == STATE_GENPCBASE)
	{
		m_vector_prefetch_valid = false; // A debugger redirect discards the retained fetch.
		m_interrupt_entry_stage = 0;
		m_pc = u16(m_debug_pc);
		m_xpc = m_extended_program ? (m_debug_pc >> 16) & 0x7f : 0;
		reset_pipeline();
	}
	else if (entry.index() == STATE_ST1)
		update_xf();
	else if (entry.index() >= STATE_TIM && entry.index() <= STATE_TCR)
		data_write(0x24 + entry.index() - STATE_TIM, m_debug_timer);
	else if (entry.index() == STATE_IDLE && !m_idle)
		leave_idle();
}

void tms320c54x_device::update_xf(bool force)
{
	const bool level = BIT(m_st1, 13);
	if (force || level != m_xf_level)
	{
		m_xf_level = level;
		m_xf_out_cb(level);
	}
}

void tms320c54x_device::device_post_load()
{
	// The output cache is derived; re-drive the restored architectural level.
	update_xf(true);
	m_pll_clock_stop_cb(m_idle_mode == 3);
	m_system_clock_cb(clock());
}

void tms320c54x_device::state_export(const device_state_entry &entry)
{
	if (entry.index() == STATE_GENPC || entry.index() == STATE_GENPCBASE)
		m_debug_pc = logical_program_address(m_pc);
	else if (entry.index() >= STATE_TIM && entry.index() <= STATE_TCR)
		m_debug_timer = data_read(0x24 + entry.index() - STATE_TIM);
}

void tms320c54x_device::far_transfer(u32 address, bool delayed)
{
	if (delayed)
	{
		m_delayed_target = u16(address);
		m_delayed_xpc = (address >> 16) & 0x7f;
		m_delayed_far = true;
		m_delayed_words = 2;
	}
	else
	{
		m_pc = u16(address);
		m_xpc = (address >> 16) & 0x7f;
	}
}

u16 tms320c54x_device::data_read(u16 address)
{
	m_peripheral_mmr_access |= address >= 0x20 && address <= 0x5f;
	return m_data.read_word(address);
}

void tms320c54x_device::data_write(u16 address, u16 value)
{
	m_peripheral_mmr_access |= address >= 0x20 && address <= 0x5f;
	m_data.write_word(address, value);
}

void tms320c54x_device::internal_data_map(address_map &map)
{
	// Only owned registers shadow the board map; reserved gaps stay external.
	for (u16 address = 0; address <= 0x58; ++address)
		if (address <= 1 || (address >= 6 && address <= 0x1d) ||
			(address == 0x1e && m_extended_program) || (address >= 0x24 && address <= 0x26) || address == 0x58)
			// Device submaps use byte addresses; the C54x data space is word-addressed.
			map(u32(address) << 1, (u32(address) << 1) | 1).lrw16(
				NAME(([this, address]() { return register_read(address); })),
				NAME(([this, address](offs_t, u16 value, u16 mem_mask) {
					if (address == 1) value &= mem_mask; // IFR writes clear only selected bits.
					else if (mem_mask != 0xffff) value = (register_read(address) & ~mem_mask) | (value & mem_mask);
					register_write(address, value);
				})));
}

u16 tms320c54x_device::register_read(u16 address)
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
	if (address == 0x0f)
		return m_trn;
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
	if (address == 0x1e && m_extended_program)
		return m_xpc;
	if (address == 0x24)
	{
		update_timer_counter();
		return m_tim;
	}
	if (address == 0x25)
		return m_prd;
	if (address == 0x26)
	{
		update_timer_counter();
		return m_tcr;
	}
	if (address == 0x58)
		return m_clkmd;
	fatalerror("Unmapped C54x register read %04x", address);
}

void tms320c54x_device::register_write(u16 address, u16 value)
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
	else if (address == 0x0f)
		m_trn = value;
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
	else if (address == 0x1e && m_extended_program)
		m_xpc = value & 0x7f;
	else if (address == 0x24)
	{
		update_timer_counter();
		m_tim = value;
		arm_timer();
	}
	else if (address == 0x25)
		m_prd = value;
	else if (address == 0x26)
	{
		update_timer_counter();
		// SPRU131G 8.4.2: PSC is readable, but cannot be written directly.
		m_tcr = (value & TIMER_WRITABLE_MASK) | (m_tcr & TIMER_PSC_MASK);
		if (value & TIMER_TRB)
		{
			m_tim = m_prd;
			// SPRU131G table 8-14: TRB loads PSC even when TSS stops counting.
			m_tcr = (m_tcr & ~TIMER_PSC_MASK) | ((m_tcr & TIMER_TDDR_MASK) << 6);
		}
		arm_timer();
	}
	else if (address == 0x58)
	{
		write_clkmd(value);
	}
	else
		fatalerror("Unmapped C54x register write %04x", address);
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
			(BIT(m_st1, 8) ? arithmetic_shift_right(source, -asm_shift) : source >> -asm_shift) :
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
	case 0x20: m_ar[ar] = reverse_carry_index(m_ar[ar], m_ar[0], true); break;
	case 0x28: m_ar[ar] -= m_ar[0]; break;
	case 0x30: m_ar[ar] += m_ar[0]; break;
	case 0x38: m_ar[ar] = reverse_carry_index(m_ar[ar], m_ar[0], false); break;
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
	{
		// BK=0 makes the % modifier linear (SPRU429A section 3.3.6).
		m_ar[ar] += step;
		return;
	}
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
	// SPRU131G 7.5.5.1: compiler-mode DAGEN can precede the
	// previous stack operation's SP update. Extended-shift operands are later.
	u16 const sp = m_sp_address_pending && !m_sp_address_late ? m_sp_address_base : m_sp;
	return BIT(m_st1, 14) ? u16(sp + (mode & 0x7f)) :
			u16(((m_st0 & 0x01ff) << 7) | (mode & 0x7f));
}

u16 tms320c54x_device::short_smem_address(u8 mode) const
{
	return BIT(mode, 7) ? m_ar[indirect_ar(mode)] : direct_address(mode);
}

u16 tms320c54x_device::prepare_smem_address(u8 mode)
{
	if (!BIT(mode, 7))
		return direct_address(mode);
	if (mode >= 0xf8)
		return fetch();
	if (mode >= 0xe0)
		return long_offset_address(mode);
	const unsigned ar = indirect_ar(mode);
	if ((mode & 0x78) == 0x18)
		indirect_modify(mode);
	return m_ar[ar];
}

void tms320c54x_device::finish_smem_access(u8 mode)
{
	if (mode >= 0x80 && mode < 0xe0 && (mode & 0x78) != 0x18)
		indirect_modify(mode);
}

u16 tms320c54x_device::indirect_read(u8 mode)
{
	const u16 address = prepare_smem_address(mode);
	const u16 value = data_read(address);
	finish_smem_access(mode);
	return value;
}

u16 tms320c54x_device::immediate_smem_read(u8 mode, u16 &immediate)
{
	u16 address;
	if (mode >= 0xe0 && mode < 0xf8)
	{
		immediate = fetch();
		address = prepare_smem_address(mode);
	}
	else
	{
		address = prepare_smem_address(mode);
		immediate = fetch();
	}
	const u16 value = data_read(address);
	finish_smem_access(mode);
	return value;
}

void tms320c54x_device::indirect_write(u8 mode, u16 value)
{
	const u16 address = prepare_smem_address(mode);
	data_write(address, value);
	finish_smem_access(mode);
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
			return;
		}
		else
		{
			m_rpt_end = REPEAT_END_UNSET;
			m_rpt_iteration = 0;
		}
		// RPT #0 still retires a body instruction. If it ends an active
		// block repeat, the outer boundary must be processed below too.
	}

	if (m_rptc)
	{
		if (m_rpt_end == REPEAT_END_UNSET)
			m_rpt_end = m_pc;
		if (m_pc == m_rpt_end)
		{
			--m_rptc;
			++m_rpt_iteration;
			m_pc = m_rpt_address;
		}
	}
	else if (m_rpt_end != REPEAT_END_UNSET && m_pc == m_rpt_end)
	{
		m_rpt_end = REPEAT_END_UNSET;
		m_rpt_iteration = 0;
	}

	// REA names the last program word in the block. This also covers a
	// multiword CALL at the end of the block after its RET restores REA+1.
	// SPRU131G 6.8: software-visible BRAF is the loop's activation control.
	if ((m_st1 & ST1_BRAF) && m_pc == u16(m_rea + 1))
	{
		if (m_brc)
		{
			--m_brc;
			m_pc = m_rsa;
		}
		else
		{
			m_st1 &= ~ST1_BRAF;
		}
	}
}

void tms320c54x_device::interrupt_entry_step()
{
	// SPRU131G example 7-18: these entry operations are shared by both
	// execution frontends; only the staged frontend also drains older slots.
	switch (m_interrupt_entry_stage++)
	{
	case 1: m_rtn = m_pc; break; // INTR Decode, before stack Access.
	case 2: m_interrupt_stack_address = --m_sp; break;
	case 3:
		// SPRU131G 6.10.4/6.10.8 tie IFR clearance and maskable INTM
		// setting to vector Fetch/IACK, not the earlier source selection.
		if (m_interrupt_ack_source != INPUT_LINE_NMI)
		{
			m_st1 |= 0x0800;
			m_ifr &= ~(u16(1) << m_interrupt_ack_source);
		}
		standard_irq_callback(m_interrupt_ack_source, logical_program_address(m_rtn));
		m_pc = m_interrupt_vector_pc;
		m_vector_prefetch_word = fetch();
		m_vector_prefetch_valid = true;
		m_pc = m_interrupt_vector_pc;
		break;
	case 4:
		m_data.write_word(m_interrupt_stack_address, m_rtn);
		m_interrupt_entry_stage = 0;
		break;
	}
}

bool tms320c54x_device::service_interrupt()
{
	if (m_interrupt_entry_stage)
	{
		interrupt_entry_step();
		--m_icount;
		return true;
	}
	if (!accept_interrupt()) return false;
	--m_icount;
	return true;
}

bool tms320c54x_device::accept_interrupt()
{
	if (m_vector_prefetch_valid) return false; // Finish the accepted entry before recognizing another source.
	// Maskable hardware sources occupy vectors 16..31 and map directly to
	// IMR/IFR bits 0..15. PMST.IPTR selects their 128-word vector page.
	const u16 pending = m_ifr & m_imr;
	// The C54x does not recognize an interrupt between a delayed control
	// transfer and its two delay words, nor between RPT/RPTZ and the repeated
	// instruction. IFR remains set and is reconsidered at the next legal
	// instruction boundary. Taking it inside either atomic sequence can skip a
	// vector prologue's context-save slots and corrupt the return stack.
	const bool single_repeat_active = m_rpt_armed || m_rptc || m_rpt_end != REPEAT_END_UNSET;
	if ((!m_nmi_pending && (BIT(m_st1, 11) || !pending || m_intm_guard)) ||
			m_delayed_words || m_xc_guard || single_repeat_active)
		return false;

	unsigned vector = 1; // SPRU131G table 6-20: NMI, independent of IFR/IMR.
	int acknowledged_source = INPUT_LINE_NMI;
	if (m_nmi_pending)
		m_nmi_pending = false;
	else
	{
		unsigned source = 0;
		while (!BIT(pending, source))
			++source;
		vector = source + 16;
		acknowledged_source = source;
	}
	if (acknowledged_source == INPUT_LINE_NMI)
		m_st1 |= 0x0800;
	m_interrupt_vector_pc = (m_pmst & 0xff80) | (vector << 2);
	m_interrupt_ack_source = acknowledged_source;
	m_interrupt_entry_stage = 1;
	leave_idle();
	return true;
}

void tms320c54x_device::reset_pipeline()
{
	m_pipeline_early_write_deferred.fill(false);
	m_pipeline_peripheral_wait_paid.fill(false);
	m_pipeline_intm_protected.fill(false);
	m_pipeline_direct_status.fill(0);
	m_pipeline_direct_sp.fill(0);
	m_pipeline_intm_follow = false;
	m_pipeline_valid.fill(false);
	m_pipeline_decode_remaining = 0;
	m_pipeline_decode_extension = 0;
	m_pipeline_decode_word = 0;
	m_pipeline_decode_address = 0;
	m_pipeline_fetch_address = logical_program_address(m_pc);
	m_pipeline_return_pc = 0;
	m_pipeline_flush_front = m_pipeline_vector_pending = false;
	m_pipeline_decode_flush = m_pipeline_control_words = m_pipeline_delay_decode_words = 0;
	m_pipeline_control_address = 0;
	m_pipeline_bc_held = false;
	m_pipeline_bc_word = 0;
	m_pipeline_bc_address = 0;
	m_pipeline_retd_stage = 0;
	m_pipeline_retd_words.fill(0);
	m_pipeline_retd_addresses.fill(0);
	m_pipeline_retd_target = 0;
	m_pipeline_xc_active = false;
	m_pipeline_xc_end = 0;
	m_pipeline_xc_skip_words = 0;
	m_pipeline_far_return_pending = false;
	m_pipeline_long_store_pending = false;
	m_pipeline_dp_wait = 0;
	m_pipeline_cycles = 0;
}

void tms320c54x_device::set_pipeline_frontend(bool enabled)
{
	m_pipeline_frontend = enabled;
	m_vector_prefetch_valid = false;
	m_interrupt_entry_stage = 0;
	reset_pipeline();
}

void tms320c54x_device::pipeline_step()
{
	++m_pipeline_cycles;
	if (m_pipeline_dp_wait)
	{
		--m_pipeline_dp_wait;
		return;
	}
	if (m_pipeline_long_store_pending)
	{
		m_data.write_word(m_pipeline_operand_address[5] ^ 1, m_pipeline_immediate[5]);
		m_pipeline_long_store_pending = false;
		pipeline_accept_interrupt();
		return;
	}
	if (m_pipeline_far_return_pending)
	{
		// Retain the second stack operand while the return holds Read.
		// This bus placement is a partial model of class 34, not measured
		// silicon phases. The first sample follows older Execute writes.
		m_pipeline_operand_address[4] = data_read(m_sp++);
		m_pipeline_far_return_pending = false;
		return;
	}
	bool const read_peripheral = m_pipeline_valid[3] &&
		((m_pipeline_word[3] & 0xfe00) == 0x1000 || staged_rmw(m_pipeline_word[3]) || (m_pipeline_word[3] & 0xfe80) == 0x4800 || (m_pipeline_word[3] & 0xff80) == 0x4600) &&
		m_pipeline_operand_address[3] >= 0x20 && m_pipeline_operand_address[3] <= 0x5f;
	bool const write_peripheral = m_pipeline_valid[4] &&
		(staged_store(m_pipeline_word[4]) || staged_rmw(m_pipeline_word[4]) || (m_pipeline_word[4] & 0xff00) == 0x7600) &&
		m_pipeline_operand_address[4] >= 0x20 && m_pipeline_operand_address[4] <= 0x5f;
	if (read_peripheral && write_peripheral)
	{
		logerror("%s: simultaneous staged peripheral read/write arbitration is unsupported\n", machine().describe_context());
		m_illegal = true;
		return;
	}
	unsigned const access_stage = read_peripheral ? 3 : 4;
	if ((read_peripheral || write_peripheral) && !m_pipeline_peripheral_wait_paid[access_stage])
	{
		// SPRU172C establishes one extra peripheral-access clock. Holding
		// pipeline advancement at the bus access is this prototype's declared
		// latency model, not independently recovered silicon stage placement.
		m_pipeline_peripheral_wait_paid[access_stage] = true;
		return;
	}
	u8 const entry_stage = m_interrupt_entry_stage;
	bool prefetch_bubble = false;
	for (unsigned stage = 5; stage; --stage)
	{
		m_pipeline_valid[stage] = m_pipeline_valid[stage - 1];
		m_pipeline_address[stage] = m_pipeline_address[stage - 1];
		m_pipeline_word[stage] = m_pipeline_word[stage - 1];
		m_pipeline_operand_address[stage] = m_pipeline_operand_address[stage - 1];
		m_pipeline_operand[stage] = m_pipeline_operand[stage - 1];
		m_pipeline_immediate[stage] = m_pipeline_immediate[stage - 1];
		m_pipeline_early_write_deferred[stage] = m_pipeline_early_write_deferred[stage - 1];
		m_pipeline_peripheral_wait_paid[stage] = m_pipeline_peripheral_wait_paid[stage - 1];
		m_pipeline_intm_protected[stage] = m_pipeline_intm_protected[stage - 1];
		m_pipeline_direct_status[stage] = m_pipeline_direct_status[stage - 1];
		m_pipeline_direct_sp[stage] = m_pipeline_direct_sp[stage - 1];
	}
	m_pipeline_valid[0] = false;
	m_pipeline_early_write_deferred[0] = false;
	m_pipeline_peripheral_wait_paid[0] = false;
	m_pipeline_intm_protected[0] = false;
	if (m_pipeline_flush_front) m_pipeline_valid[2] = false;
	if (m_pipeline_decode_flush)
	{
		m_pipeline_valid[2] = false;
		--m_pipeline_decode_flush;
	}

	// SPRU131G example 7-18: INTR occupies Decode/Access/Read/Execute
	// while older decoded foreground instructions continue to Execute.
	if (entry_stage)
	{
		if (entry_stage == 1) m_pc = m_pipeline_return_pc;
		interrupt_entry_step();
	}
	if (m_pipeline_valid[1])
	{
		u32 const address = m_pipeline_address[1];
		if (entry_stage == 3 && u16(address) == m_interrupt_vector_pc)
		{
			m_pipeline_word[1] = m_vector_prefetch_word;
			m_vector_prefetch_valid = false;
		}
		else
		{
			m_pc = u16(address + 1);
			m_xpc = (address >> 16) & 0x7f;
			m_pipeline_word[1] = m_cache.read_word(physical_program_address(address));
		}
	}
	bool retd_delay_decode = false;
	unsigned retd_delay_index = 0;
	if (m_pipeline_retd_stage)
	{
		// Example 7-8 retains both prefetched words until return Read is
		// complete. Decode then releases one word per clock without re-fetch.
		unsigned const index = m_pipeline_retd_stage <= 2 ? m_pipeline_retd_stage - 1 : m_pipeline_retd_stage - 3;
		if (m_pipeline_retd_stage <= 2)
		{
			if (!m_pipeline_valid[2]) { m_illegal = true; return; }
			m_pipeline_retd_words[index] = m_pipeline_word[2];
			m_pipeline_retd_addresses[index] = m_pipeline_address[2];
			m_pipeline_valid[2] = false;
			prefetch_bubble = true;
		}
		else
		{
			retd_delay_decode = true;
			retd_delay_index = index;
			m_pipeline_valid[2] = true;
			m_pipeline_word[2] = m_pipeline_retd_words[index];
			m_pipeline_address[2] = m_pipeline_retd_addresses[index];
		}
		m_pipeline_retd_stage = m_pipeline_retd_stage == 4 ? 0 : m_pipeline_retd_stage + 1;
	}
	if (m_pipeline_valid[3] && staged_conditional_branch(m_pipeline_word[3]) && !BIT(m_pipeline_word[3], 9))
	{
		// Example 7-16 retains i4 across the empty Fetch slot, rather than
		// decoding it before BC's decision or fetching it a second time.
		m_pipeline_bc_held = m_pipeline_valid[2];
		m_pipeline_bc_word = m_pipeline_word[2];
		m_pipeline_bc_address = m_pipeline_address[2];
		m_pipeline_valid[2] = false;
	}
	if (m_pipeline_valid[4] && staged_conditional_branch(m_pipeline_word[4]) && !BIT(m_pipeline_word[4], 9))
	{
		bool const zero = (accumulator(BIT(m_pipeline_word[4], 3)) & ACC_MASK) == 0;
		bool const taken = BIT(m_pipeline_word[4], 0) ? zero : !zero;
		m_pipeline_immediate[4] = taken;
		if (taken)
		{
			m_pipeline_valid[2] = false;
			m_pipeline_decode_flush = 1; // Discard the already fetched i5 next clock.
			m_pipeline_fetch_address = (m_pipeline_address[4] & 0x7f0000) | m_pipeline_operand[4];
		}
		else if (m_pipeline_bc_held)
		{
			m_pipeline_valid[2] = true;
			m_pipeline_word[2] = m_pipeline_bc_word;
			m_pipeline_address[2] = m_pipeline_bc_address;
		}
		m_pipeline_bc_held = false;
	}
	// Example 7-13 samples XC in Access, before the next word's Decode
	// and before older Execute can change TC in this clock.
	if (m_pipeline_valid[3] && staged_xc(m_pipeline_word[3]))
	{
		u16 const word = m_pipeline_word[3];
		bool const zero = (accumulator(BIT(word, 3)) & ACC_MASK) == 0;
		bool const execute = BIT(word, 6) ? (BIT(word, 0) ? zero : !zero) :
			BIT(word, 5) ? BIT(m_st0, 12) == BIT(word, 4) : true;
		m_pipeline_operand[3] = execute;
		if (!execute) m_pipeline_xc_skip_words = BIT(m_pipeline_word[3], 9) ? 2 : 1;
	}
	if (m_pipeline_valid[2] && m_pipeline_xc_skip_words)
	{
		m_pipeline_word[2] = 0xf495; // Rejected words are NOPs, not decoded operands.
		--m_pipeline_xc_skip_words;
	}
	if (m_pipeline_valid[2])
	{
		if (m_pipeline_delay_decode_words)
		{
			u16 const word = m_pipeline_word[2];
			unsigned const words = ((staged_rmw(word) && u8(word) == 0xf8) || word == 0x76f8) ? 3 :
				(staged_move(word) || staged_call(word) || staged_conditional_branch(word) || staged_immediate_alu(word) || (word & 0xfef0) == 0xf020 || (word & 0xfeff) == 0xf062 || staged_rmw(word) || (word & 0xff00) == 0x7700 || (word & 0xff00) == 0x7600 || word == 0xf073 || word == 0xf273) ? 2 : 1;
			if (!m_pipeline_decode_remaining && (words > m_pipeline_delay_decode_words || staged_call(word) || staged_conditional_branch(word) || word == 0xfc00 || word == 0xf4eb || word == 0xfe00 || word == 0xf49b || word == 0xf69b || (word & 0xfdfe) == 0xf4e4 || word == 0xf073 || word == 0xf273))
			{
				logerror("%s: unsupported staged control delay form %04x\n", machine().describe_context(), word);
				m_illegal = true;
				return;
			}
			--m_pipeline_delay_decode_words;
		}
		if (m_pipeline_decode_remaining)
		{
			// Extension words belong to the retained opcode, not independently
			// executable instructions. Absolute RMW/ST use address, then literal.
			if (--m_pipeline_decode_remaining)
			{
				m_pipeline_decode_extension = m_pipeline_word[2];
				m_pipeline_valid[2] = false;
			}
			else
			{
				m_pipeline_operand[2] = m_pipeline_immediate[2] = m_pipeline_word[2];
				m_pipeline_operand_address[2] = m_pipeline_decode_extension;
				m_pipeline_word[2] = m_pipeline_decode_word;
				m_pipeline_address[2] = m_pipeline_decode_address;
			}
		}
		else if (staged_move(m_pipeline_word[2]) || staged_call(m_pipeline_word[2]) || staged_conditional_branch(m_pipeline_word[2]) || staged_immediate_alu(m_pipeline_word[2]) || (m_pipeline_word[2] & 0xfef0) == 0xf020 ||
			(m_pipeline_word[2] & 0xfeff) == 0xf062 ||
			m_pipeline_word[2] == 0xf073 || m_pipeline_word[2] == 0xf273 ||
			m_pipeline_word[2] == 0x770e || ((m_pipeline_word[2] & 0xff00) == 0x7700 && u8(m_pipeline_word[2]) >= 0x10 && u8(m_pipeline_word[2]) <= 0x19) ||
			((m_pipeline_word[2] & 0xff00) == 0x7600 && (u8(m_pipeline_word[2]) == 0xf8 ||
				(u8(m_pipeline_word[2]) < 0x98 && (BIT(m_pipeline_word[2], 7) || !BIT(m_st1, 14))))) ||
			(staged_rmw(m_pipeline_word[2]) &&
				(u8(m_pipeline_word[2]) == 0xf8 || (u8(m_pipeline_word[2]) < 0x98 &&
					(BIT(m_pipeline_word[2], 7) || !BIT(m_st1, 14))))))
		{
			m_pipeline_decode_remaining = ((staged_rmw(m_pipeline_word[2]) && u8(m_pipeline_word[2]) == 0xf8) || m_pipeline_word[2] == 0x76f8) ? 2 : 1;
			m_pipeline_decode_extension = 0;
			m_pipeline_decode_word = m_pipeline_word[2];
			m_pipeline_decode_address = m_pipeline_address[2];
			m_pipeline_valid[2] = false;
		}
		u16 const word = m_pipeline_word[2];
		// Register/immediate loads, short loads, DP stores and bounded RMW forms.
		// Other memory/control forms need Access/Read/redirect ownership first.
		bool const short_load = (word & 0xfe00) == 0x1000 && u8(word) < 0xa0 &&
			(BIT(word, 7) || u8(word) < 0x80);
		bool const direct_store = staged_store(word);
		bool const direct_long_load = (word & 0xfe80) == 0x5600;
		bool const direct_long_store = (word & 0xfe80) == 0x4e00;
		bool const direct_mmr_load = (word & 0xfe80) == 0x4800;
		bool const memory_dp = (word & 0xff80) == 0x4600;
		bool const direct_move = staged_move(word);
		if (m_pipeline_valid[2] && direct_move && m_pipeline_immediate[2] < 0x60 &&
			(m_pipeline_immediate[2] < 0x10 || m_pipeline_immediate[2] > 0x19))
		{
			logerror("%s: staged MVDK control/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true;
			return;
		}
		bool const immediate_store = (word & 0xff00) == 0x7600 && (u8(word) == 0xf8 ||
			(u8(word) < 0x98 && (BIT(word, 7) || !BIT(m_st1, 14))));
		bool const long_load = (word & 0xfef0) == 0xf020 || (word & 0xfeff) == 0xf062;
		bool const rmw = staged_rmw(word) &&
			(u8(word) == 0xf8 || (u8(word) < 0x98 && (BIT(word, 7) || !BIT(m_st1, 14))));
		bool const fast_return = word == 0xf49b || word == 0xf69b;
		if (m_pipeline_valid[2] && word == 0x6bf8 && m_pipeline_operand_address[2] < 0x60)
		{
			logerror("%s: staged ADDM register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		bool const plain_return = word == 0xfc00 || word == 0xf4eb;
		bool const delayed_return = word == 0xfe00;
		bool const far_return = (word & 0xfdfe) == 0xf4e4;
		bool const intm_bit = word == 0xf6bb || word == 0xf7bb;
		bool const status_bit = (word & 0xfcf0) == 0xf4b0;
		bool const immediate_dp = (word & 0xfe00) == 0xea00;
		bool const immediate_alu = staged_immediate_alu(word);
		bool const direct_push = (word & 0xffe0) == 0x4a00;
		bool const data_push = (word & 0xff00) == 0x4b00 && u8(word) < 0x98;
		bool const data_pop = (word & 0xff00) == 0x8b00 && u8(word) < 0x98;
		bool const direct_pop = (word & 0xff00) == 0x8a00 &&
			((u8(word) >= 6 && u8(word) <= 0x17) || u8(word) == 0x19);
		bool const frame = (word & 0xff00) == 0xee00;
		bool const branch = word == 0xf073 || word == 0xf273 || staged_conditional_branch(word) || staged_call(word);
		bool const dagen_immediate = (word & 0xff00) == 0x7700 && u8(word) >= 0x10 && u8(word) <= 0x19;
		bool const immediate_t = word == 0x770e;
		if (m_pipeline_valid[2] && word != 0xf495 && !staged_xc(word) && !fast_return && !plain_return && !delayed_return && !far_return && !status_bit && !immediate_dp && !immediate_alu && !branch && !dagen_immediate && !immediate_t && (word & 0xff00) != 0xe800 &&
			(word & 0xff00) != 0xe900 && !frame && !direct_push && !data_push && !data_pop && !direct_pop && !direct_long_load && !direct_long_store && !direct_mmr_load && !memory_dp && !direct_move && !short_load && !direct_store && !immediate_store && !long_load && !rmw)
		{
			logerror("%s: unsupported staged C54x form %04x at %06x\n",
				machine().describe_context(), word, m_pipeline_address[2]);
			m_illegal = true;
			return;
		}
		if (m_pipeline_valid[2] && staged_xc(word))
		{
			// XC 1 also protects two following words (SPRU172C 4-199).
			m_pipeline_xc_active = true;
			m_pipeline_xc_end = (m_pipeline_address[2] & 0x7f0000) | u16(m_pipeline_address[2] + 2);
			if (retd_delay_decode)
				// XC protection counts resumed instruction words, not the
				// abandoned sequential address beyond RETD's delay window.
				m_pipeline_xc_end = (m_pipeline_retd_target & 0x7f0000) | u16(m_pipeline_retd_target + retd_delay_index);
		}
		if (m_pipeline_valid[2] && (fast_return || plain_return || delayed_return || far_return || branch))
		{
			bool const delayed = delayed_return || word == 0xf69b || word == 0xf273 || word == 0xf274 || (staged_conditional_branch(word) && BIT(word, 9)) || (far_return && BIT(word, 9));
			// Example 7-17 has one empty prefetch slot, unlike unconditional BD.
			prefetch_bubble = staged_conditional_branch(word);
			m_pipeline_control_address = m_pipeline_address[2];
			m_pipeline_control_words = (branch ? 2 : 1) + (delayed ? 2 : 0);
			m_pipeline_delay_decode_words = delayed ? 2 : 0;
			m_pipeline_decode_flush = delayed || staged_conditional_branch(word) ? 0 : 2;
			if (plain_return || (far_return && !delayed)) m_pipeline_flush_front = true;
			if (delayed_return) m_pipeline_retd_stage = 1;
		}
		if (m_pipeline_valid[2] && (dagen_immediate || immediate_t || direct_move)) m_pipeline_early_write_deferred[2] = false;
		if (m_pipeline_valid[2])
		{
			// Basic direct operands sample CPL/DP in Decode, before a preceding
			// RSBX/SSBX retires (SPRU131G tables 7-18 and 7-19).
			if (short_load || direct_store || direct_long_load || direct_long_store || memory_dp || direct_move || data_push || data_pop)
			{
				m_pipeline_direct_status[2] = (m_st0 & 0x1ff) | (m_st1 & 0x4000);
				// Table 7-17 gives LD #k,DP zero latency. Forward the nearest
				// in-flight immediate DP without committing architectural ST0 early.
				for (unsigned stage = 3; stage < 6; ++stage)
					if (m_pipeline_valid[stage] && (m_pipeline_word[stage] & 0xfe00) == 0xea00)
					{
						m_pipeline_direct_status[2] = (m_pipeline_direct_status[2] & 0x4000) | (m_pipeline_word[stage] & 0x1ff);
						break;
					}
			}
			if (data_push) m_pipeline_direct_sp[2] = m_sp; // PSHD samples SP in Decode (SPRU172C 4-131).
			// SPRU131G 6.10.7: protection starts in Decode and covers the
			// next complete instruction, including all its extension words.
			m_pipeline_intm_protected[2] = m_pipeline_intm_follow || intm_bit;
			m_pipeline_intm_follow = intm_bit;
		}
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x7700)
	{
		// SPRU131G 7.5.2: STM writes DAGEN in the opcode word's Read,
		// coincident with Access of our retained two-word operation. An older
		// store using that register bank defers the write one clock (7-22).
		// STM T uses this early-write path as a partial model of example
		// 7-39's zero T latency; T is not subject to the DAGEN-bank deferral.
		bool const older_store = m_pipeline_word[3] != 0x770e && m_pipeline_valid[5] &&
			(staged_store(m_pipeline_word[5]) || staged_rmw(m_pipeline_word[5])) &&
			m_pipeline_operand_address[5] >= 0x10 && m_pipeline_operand_address[5] <= 0x19;
		m_pipeline_early_write_deferred[3] = older_store;
		if (!older_store) data_write(u8(m_pipeline_word[3]), m_pipeline_immediate[3]);
	}
	if (m_pipeline_valid[4] && m_pipeline_early_write_deferred[4])
	{
		bool const move = staged_move(m_pipeline_word[4]);
		data_write(move ? m_pipeline_immediate[4] : u8(m_pipeline_word[4]), move ? m_pipeline_operand[4] : m_pipeline_immediate[4]);
		m_pipeline_early_write_deferred[4] = false;
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] == 0xf49b || m_pipeline_word[3] == 0xf69b))
	{
		// SPRU131G examples 7-11/12: Access increments SP and reads RTN,
		// allowing target Prefetch now; no stack-memory read occurs.
		++m_sp;
		m_pipeline_fetch_address = (m_pipeline_address[3] & 0x7f0000) | m_rtn;
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] == 0xf073 || m_pipeline_word[3] == 0xf273 || staged_call(m_pipeline_word[3])))
	{
		// SPRU131G examples 7-2/3: the target word's Access starts target
		// Prefetch; the following two sequential words are delay or flush slots.
		m_pipeline_fetch_address = (m_pipeline_address[3] & 0x7f0000) | m_pipeline_operand[3];
		if (staged_call(m_pipeline_word[3])) m_rtn = m_pipeline_immediate[3];
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0xee00)
	{
		// SPRU172C 4-60: FRAME has no latency to the following stack
		// operation. Access-owned SP update is a partial stage model.
		m_sp = u16(m_sp + s8(m_pipeline_word[3]));
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xfffe) == 0xf6e4)
	{
		// Delayed class-34 prototype: two stack samples without the plain
		// return's held Read clock. Placement is not measured silicon timing.
		m_pipeline_operand[3] = data_read(m_sp++);
	}
	auto const basic_address = [this](unsigned stage, u8 mode) -> u16 {
		if (BIT(mode, 7)) return short_smem_address(mode);
		u16 const status = m_pipeline_direct_status[stage];
		return BIT(status, 14) ? u16(m_pipeline_direct_sp[stage] + mode) : u16((status & 0x1ff) << 7) | mode;
	};
	if (m_pipeline_valid[3] && ((m_pipeline_word[3] & 0xfe00) == 0x1000 ||
		staged_store(m_pipeline_word[3]) || (m_pipeline_word[3] & 0xfe80) == 0x5600 || (m_pipeline_word[3] & 0xfe80) == 0x4e00 || (m_pipeline_word[3] & 0xff80) == 0x4600 || staged_move(m_pipeline_word[3]) || (m_pipeline_word[3] & 0xff00) == 0x8b00))
		// Table 7-9: a basic direct consumer samples SP before an older
		// PSHM allocates its stack word in this clock's Read stage.
		m_pipeline_direct_sp[3] = m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x4b00
			? m_pipeline_immediate[4] : m_sp; // Table 7-9: one-clock Category-I latency after PSHD.
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x4b00)
	{
		u8 const mode = m_pipeline_word[3];
		m_pipeline_operand[3] = basic_address(3, mode);
		if (m_pipeline_operand[3] < 0x60)
		{
			logerror("%s: staged PSHD register/peripheral source is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (BIT(mode, 7)) indirect_modify(mode);
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x8b00)
	{
		u8 const mode = m_pipeline_word[3];
		m_pipeline_operand_address[3] = basic_address(3, mode);
		if (m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged POPD register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (BIT(mode, 7)) indirect_modify(mode);
	}
	if (m_pipeline_valid[3] && staged_move(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		u16 const source = basic_address(3, mode);
		if (source < 0x60)
		{
			logerror("%s: staged MVDK register/peripheral source is unsupported\n", machine().describe_context());
			m_illegal = true;
			return;
		}
		m_pipeline_operand[3] = m_data.read_word(source);
		// Retain the old-address sample, update the source AR, then publish
		// the destination; source/destination AR aliasing must not re-read RAM.
		if (BIT(mode, 7)) indirect_modify(mode);
		u16 const destination = m_pipeline_immediate[3];
		if (destination >= 0x10 && destination <= 0x19)
		{
			bool const conflict = m_pipeline_valid[5] &&
				(staged_store(m_pipeline_word[5]) || staged_rmw(m_pipeline_word[5])) &&
				m_pipeline_operand_address[5] >= 0x10 && m_pipeline_operand_address[5] <= 0x19;
			m_pipeline_early_write_deferred[3] = conflict;
			if (!conflict) data_write(destination, m_pipeline_operand[3]);
		}
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff80) == 0x4600)
		m_pipeline_operand_address[3] = basic_address(3, u8(m_pipeline_word[3]));
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xfe80) == 0x4800)
		m_pipeline_operand_address[3] = u8(m_pipeline_word[3]);
	if (m_pipeline_valid[3] && ((m_pipeline_word[3] & 0xfe80) == 0x5600 || (m_pipeline_word[3] & 0xfe80) == 0x4e00))
	{
		m_pipeline_operand_address[3] = basic_address(3, u8(m_pipeline_word[3]));
		if (m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged long-word register/peripheral pair is unsupported\n", machine().describe_context());
			m_illegal = true;
			return;
		}
	}
	// Load DAGEN and short AR modifications belong to Access (SPRU131G 7.1),
	// not retirement. Retain that address even if the AR changes before Read.
	if (m_pipeline_valid[3] && ((m_pipeline_word[3] & 0xfe00) == 0x1000 ||
		(staged_rmw(m_pipeline_word[3]) && u8(m_pipeline_word[3]) != 0xf8)))
	{
		u8 const mode = m_pipeline_word[3];
		bool const preincrement = BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode);
		m_pipeline_operand_address[3] = (m_pipeline_word[3] & 0xfe00) == 0x1000
			? basic_address(3, mode) : short_smem_address(mode);
		if ((m_pipeline_word[3] & 0xff00) == 0x6b00 && m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged ADDM register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (BIT(mode, 7) && !preincrement) indirect_modify(mode);
	}
	if (m_pipeline_valid[4] && ((m_pipeline_word[4] & 0xfe00) == 0x1000 ||
		staged_rmw(m_pipeline_word[4]) || (m_pipeline_word[4] & 0xfe80) == 0x4800))
		m_pipeline_operand[4] = m_data.read_word(m_pipeline_operand_address[4]);
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xfe80) == 0x5600)
	{
		m_pipeline_operand[4] = m_data.read_word(m_pipeline_operand_address[4]);
		m_pipeline_immediate[4] = m_data.read_word(m_pipeline_operand_address[4] ^ 1);
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xffe0) == 0x4a00)
	{
		// Class 16A: retain the source read and stack destination separately.
		// Read-owned SP allocation is a declared partial stage model;
		// indirect PSHM and stack/DAGEN conflicts are not enabled here.
		m_pipeline_operand[4] = data_read(u8(m_pipeline_word[4]));
		m_pipeline_operand_address[4] = --m_sp;
	}
	if (m_pipeline_valid[4] && ((m_pipeline_word[4] & 0xff00) == 0x8a00 || (m_pipeline_word[4] & 0xff00) == 0x8b00))
	{
		// Class 17A: retain the stack word before destination publication.
		// Read-owned SP increment is a partial placement model; POPM excludes
		// SP/XPC/control destinations and POPD is bounded to RAM.
		if ((m_pipeline_word[4] & 0xff00) == 0x8b00 && m_sp < 0x60)
		{
			logerror("%s: staged POPD register/peripheral stack source is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		m_pipeline_operand[4] = data_read(m_sp++);
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x4b00)
		// DARAM DB reads precede EB writes in the same clock (table 7-2).
		m_pipeline_operand[4] = m_data.read_word(m_pipeline_operand[4]);
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x4b00)
	{
		// Access-owned allocation follows any older stack Read update in
		// this clock; retain the pre-allocation SP for younger direct users.
		m_pipeline_immediate[3] = m_sp;
		m_pipeline_operand_address[3] = --m_sp;
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] == 0xfc00 || m_pipeline_word[3] == 0xf4eb || m_pipeline_word[3] == 0xfe00))
	{
		// Example 7-7 allocates the return's DAB in Access. Account for an
		// older PSHM/POPM Read update before allocating this younger pop.
		m_pipeline_operand_address[3] = m_sp++;
	}
	// SPRU131G 7.1: a store presents EAB in Read, then writes EB in Execute.
	if (m_pipeline_valid[4] && staged_store(m_pipeline_word[4]))
	{
		u8 const mode = m_pipeline_word[4];
		m_pipeline_operand_address[4] = basic_address(4, mode);
		if ((m_pipeline_word[4] & 0xfe00) == 0x8c00 && m_pipeline_operand_address[4] < 0x60)
		{
			logerror("%s: staged T/TRN store register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (BIT(mode, 7)) indirect_modify(mode);
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x7600)
	{
		u8 const mode = m_pipeline_word[4];
		if (mode != 0xf8)
		{
			bool const preincrement = BIT(mode, 7) && (mode & 0x78) == 0x18;
			if (preincrement) indirect_modify(mode);
			m_pipeline_operand_address[4] = short_smem_address(mode);
			if (BIT(mode, 7) && !preincrement) indirect_modify(mode);
		}
		if (m_pipeline_operand_address[4] >= 0x10 && m_pipeline_operand_address[4] <= 0x19)
		{
			logerror("%s: staged immediate ST to DAGEN requires early-write ownership\n", machine().describe_context());
			m_illegal = true;
			return;
		}
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] == 0xfc00 || m_pipeline_word[4] == 0xf4eb || m_pipeline_word[4] == 0xfe00))
	{
		// Table 7-2: a DARAM stack Read precedes any same-clock EB write.
		// A just-pushed word requires a separating instruction, not a bypass.
		m_pipeline_operand[4] = data_read(m_pipeline_operand_address[4]);
		if (m_pipeline_word[4] == 0xfe00)
			m_pipeline_retd_target = (m_pipeline_address[4] & 0x7f0000) | m_pipeline_operand[4];
	}
	if (m_pipeline_valid[5])
	{
		u32 const address = m_pipeline_address[5];
		m_pc = u16(address);
		m_xpc = (address >> 16) & 0x7f;
		m_op = m_pipeline_word[5];
		debugger_instruction_hook(address);
		++m_pc;
		if (m_opcode_first_pc[m_op] == 0xffff) m_opcode_first_pc[m_op] = u16(address);
		++m_opcode_count[m_op];
		if (m_op == 0xf49b || m_op == 0xf69b)
			m_st1 &= ~u16(0x0800);
		else if (m_op == 0xfc00 || m_op == 0xf4eb || m_op == 0xfe00)
		{
			m_pc = m_pipeline_operand[5];
			m_pipeline_fetch_address = logical_program_address(m_pc);
			m_pipeline_flush_front = false;
			if (m_op == 0xf4eb) m_st1 &= ~u16(0x0800);
		}
		else if ((m_op & 0xfcf0) == 0xf4b0)
		{
			u16 &status = BIT(m_op, 9) ? m_st1 : m_st0;
			u16 const mask = u16(1) << (m_op & 15);
			status = BIT(m_op, 8) ? status | mask : status & ~mask;
		}
		else if ((m_op & 0xfdfe) == 0xf4e4)
		{
			m_xpc = m_extended_program ? m_pipeline_operand[5] & 0x7f : 0;
			m_pc = m_pipeline_operand_address[5];
			if (!BIT(m_op, 9)) m_pipeline_fetch_address = logical_program_address(m_pc);
			m_pipeline_flush_front = false;
			if (BIT(m_op, 0)) m_st1 &= ~u16(0x0800);
		}
		else if (staged_conditional_branch(m_op))
		{
			++m_pc;
			if (m_pipeline_immediate[5] && BIT(m_op, 9))
			{
				m_pipeline_fetch_address = (address & 0x7f0000) | m_pipeline_operand[5];
				m_pipeline_decode_flush = 1;
			}
		}
		else if (m_op == 0xf073 || m_op == 0xf273 || staged_call(m_op))
			++m_pc; // Target fetch was already redirected in Access.
		else if ((m_op & 0xff00) == 0xee00)
			; // FRAME's SP update already belongs to its Access stage.
		else if ((m_op & 0xff00) == 0x7700)
			++m_pc; // Register write already committed in the opcode's Read stage.
		else if (staged_move(m_op))
			++m_pc; // Retained move already committed at its owned write clock.
		else if ((m_op & 0xfe00) == 0x1000)
			accumulator(BIT(m_op, 8)) = data_operand(m_pipeline_operand[5]);
		else if ((m_op & 0xfe80) == 0x4800)
			accumulator(BIT(m_op, 8)) = m_pipeline_operand[5];
		else if ((m_op & 0xff80) == 0x4600)
			m_st0 = (m_st0 & ~u16(0x1ff)) | (m_pipeline_operand[5] & 0x1ff);
		else if ((m_op & 0xfe80) == 0x5600)
		{
			u64 value = (u64(m_pipeline_operand[5]) << 16) | m_pipeline_immediate[5];
			if (BIT(m_st1, 8) && BIT(m_pipeline_operand[5], 15)) value |= u64(0xff) << 32;
			accumulator(BIT(m_op, 8)) = value;
		}
		else if ((m_op & 0xfe80) == 0x4e00)
		{
			m_data.write_word(m_pipeline_operand_address[5], m_pipeline_operand[5]);
			m_pipeline_long_store_pending = true;
		}
		else if ((m_op & 0xffe0) == 0x4a00 || (m_op & 0xff00) == 0x4b00)
			data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		else if ((m_op & 0xff00) == 0x8a00)
			data_write(u8(m_op), m_pipeline_operand[5]);
		else if ((m_op & 0xff00) == 0x8b00)
			data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		else if (staged_store(m_op))
			data_write(m_pipeline_operand_address[5], (m_op & 0xfe00) == 0x8c00
				? (BIT(m_op, 8) ? m_trn : m_t)
				: u16(saturated_store(accumulator(BIT(m_op, 8))) >> (BIT(m_op, 9) ? 16 : 0)));
		else if (staged_rmw(m_op))
		{
			m_pc += u8(m_op) == 0xf8 ? 2 : 1;
			data_write(m_pipeline_operand_address[5], (m_op & 0xff00) == 0x6b00
				? add_memory_word(m_pipeline_operand[5], m_pipeline_immediate[5]) : BIT(m_op, 8)
				? m_pipeline_operand[5] | m_pipeline_immediate[5]
				: m_pipeline_operand[5] & m_pipeline_immediate[5]);
		}
		else if ((m_op & 0xff00) == 0x7600)
		{
			m_pc += u8(m_op) == 0xf8 ? 2 : 1;
			data_write(m_pipeline_operand_address[5], m_pipeline_immediate[5]);
		}
		else if (staged_immediate_alu(m_op))
		{
			++m_pc;
			unsigned const operation = (m_op >> 4) & 15;
			u64 const source = accumulator(BIT(m_op, 9));
			u64 &destination = accumulator(BIT(m_op, 8));
			if (!operation)
			{
				u64 const operand = (data_operand(m_pipeline_immediate[5]) << (m_op & 15)) & ACC_MASK;
				destination = add_sub(source, operand, false, BIT(m_op, 8));
			}
			else
			{
				u64 const operand = u64(m_pipeline_immediate[5]) << (m_op & 15);
				destination = operation == 3 ? source & operand : operation == 4 ? source | operand : source ^ operand;
			}
		}
		else if ((m_op & 0xfef0) == 0xf020 || (m_op & 0xfeff) == 0xf062)
		{
			++m_pc;
			unsigned const shift = (m_op & 0xff) == 0x62 ? 16 : m_op & 15;
			accumulator(BIT(m_op, 8)) = (data_operand(m_pipeline_operand[5]) << shift) & ACC_MASK;
		}
		else if (!staged_xc(m_op))
			execute_one(m_op);
		unsigned const xc_retired_words = ((staged_rmw(m_op) && u8(m_op) == 0xf8) || m_op == 0x76f8) ? 3 :
			(staged_move(m_op) || staged_call(m_op) || staged_conditional_branch(m_op) || staged_immediate_alu(m_op) || (m_op & 0xfef0) == 0xf020 || (m_op & 0xfeff) == 0xf062 || staged_rmw(m_op) || (m_op & 0xff00) == 0x7700 || (m_op & 0xff00) == 0x7600 || m_op == 0xf073 || m_op == 0xf273) ? 2 : 1;
		if (m_pipeline_xc_active && (address & 0x7f0000) == (m_pipeline_xc_end & 0x7f0000) &&
			u16(m_pipeline_xc_end - address) < xc_retired_words)
			m_pipeline_xc_active = false;
		if (m_pipeline_control_words && (address & 0x7f0000) == (m_pipeline_control_address & 0x7f0000) &&
			u16(address - m_pipeline_control_address) <= 3)
		{
			unsigned const words = ((staged_rmw(m_op) && u8(m_op) == 0xf8) || m_op == 0x76f8) ? 3 :
				(staged_move(m_op) || staged_call(m_op) || staged_conditional_branch(m_op) || staged_immediate_alu(m_op) || (m_op & 0xfef0) == 0xf020 || (m_op & 0xfeff) == 0xf062 || staged_rmw(m_op) || (m_op & 0xff00) == 0x7700 || (m_op & 0xff00) == 0x7600 || m_op == 0xf073 || m_op == 0xf273) ? 2 : 1;
			m_pipeline_control_words -= words;
		}
		if (m_pipeline_vector_pending && u16(address) == m_interrupt_vector_pc)
			m_pipeline_vector_pending = false;
		m_pipeline_intm_protected[5] = false;
		update_xf();
	}

	if (m_pipeline_valid[4] && staged_move(m_pipeline_word[4]) && m_pipeline_immediate[4] >= 0x60)
		m_data.write_word(m_pipeline_immediate[4], m_pipeline_operand[4]);
	if (m_pipeline_valid[2] && staged_call(m_pipeline_word[2]))
	{
		// Example 7-4: opcode Access coincides with complete target Decode.
		// Honor older stack updates before allocating this younger call word.
		m_pipeline_operand_address[2] = --m_sp;
		m_pipeline_immediate[2] = u16(m_pipeline_address[2] + (m_pipeline_word[2] == 0xf274 ? 4 : 2));
	}
	if (m_pipeline_valid[4] && staged_call(m_pipeline_word[4]))
		// The opcode's Execute precedes the complete two-word slot's Execute.
		m_data.write_word(m_pipeline_operand_address[4], m_pipeline_immediate[4]);
	if (m_pipeline_valid[4] && staged_conditional_branch(m_pipeline_word[4]) && BIT(m_pipeline_word[4], 9))
	{
		// Our complete two-word slot reaches Read at the opcode's Evaluate
		// clock. Retain the predicate after older Execute writes (example 7-17).
		bool const zero = (accumulator(BIT(m_pipeline_word[4], 3)) & ACC_MASK) == 0;
		m_pipeline_immediate[4] = BIT(m_pipeline_word[4], 0) ? zero : !zero;
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff80) == 0x4600)
	{
		// Class 5A takes three clocks but has zero DP-consumer latency
		// (SPRU172C 4-71, SPRU131G table 7-17). Retain one memory sample
		// and forward it to younger direct operands before the two held clocks.
		m_pipeline_operand[4] = m_data.read_word(m_pipeline_operand_address[4]);
		m_pipeline_dp_wait = 2;
		for (unsigned stage = 2; stage < 4; ++stage)
		{
			u16 const word = m_pipeline_word[stage];
			if (!m_pipeline_valid[stage] || BIT(word, 7) ||
				((word & 0xfe00) != 0x1000 && (word & 0xfc80) != 0x8000 &&
				 (word & 0xfe80) != 0x5600 && (word & 0xfe80) != 0x4e00 && (word & 0xff80) != 0x4600)) continue;
			m_pipeline_direct_status[stage] = (m_pipeline_direct_status[stage] & 0x4000) | (m_pipeline_operand[4] & 0x1ff);
			if (stage == 3) m_pipeline_operand_address[stage] = basic_address(stage, u8(word));
		}
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xfe80) == 0x4e00)
	{
		// Retain the source after older Execute writes, then occupy both
		// write clocks (SPRU172C class 13A). Placement is a partial model.
		u32 const value = saturated_store(accumulator(BIT(m_pipeline_word[4], 8)));
		m_pipeline_operand[4] = u16(value >> 16);
		m_pipeline_immediate[4] = u16(value);
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xfffe) == 0xf4e4)
	{
		// An adjacent PSHM commits its stack word in older Execute first.
		m_pipeline_operand[4] = data_read(m_sp++);
		m_pipeline_far_return_pending = true;
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xfffe) == 0xf6e4)
	{
		m_pipeline_operand_address[4] = data_read(m_sp++);
		m_pipeline_fetch_address = ((m_pipeline_operand[4] & 0x7f) << 16) | m_pipeline_operand_address[4];
		// Both delay words have reached Decode; discard the next sequential word.
		m_pipeline_decode_flush = 1;
	}
	if (entry_stage == 2)
	{
		m_pipeline_fetch_address = logical_program_address(m_interrupt_vector_pc);
		m_pipeline_flush_front = false;
	}
	if (!m_pipeline_flush_front && !prefetch_bubble)
	{
		m_pipeline_address[0] = m_pipeline_fetch_address;
		m_pipeline_valid[0] = true;
		m_pipeline_fetch_address = (m_pipeline_fetch_address & 0x7f0000) | u16(m_pipeline_fetch_address + 1);
	}
	pipeline_accept_interrupt();
}

void tms320c54x_device::pipeline_accept_interrupt()
{
	// Acceptance occurs after this cycle's Decode. Retain decoded slots,
	// replace the next Decode with INTR, and drain the prefetched bus word.
	bool intm_protected = m_pipeline_intm_follow;
	for (unsigned stage = 0; stage < 5; ++stage)
		intm_protected |= m_pipeline_valid[stage] && m_pipeline_intm_protected[stage];
	if ((!intm_protected || m_nmi_pending) && !m_interrupt_entry_stage && !m_pipeline_vector_pending && !m_pipeline_decode_remaining &&
		!m_pipeline_xc_active && !m_pipeline_control_words && !m_pipeline_long_store_pending && !m_pipeline_dp_wait && accept_interrupt())
	{
		m_pipeline_return_pc = u16(m_pipeline_valid[1] ? m_pipeline_address[1] :
			m_pipeline_valid[0] ? m_pipeline_address[0] : m_pipeline_fetch_address);
		m_pipeline_flush_front = m_pipeline_vector_pending = true;
	}
}

u16 tms320c54x_device::add_memory_word(u16 value, u16 immediate)
{
	const s32 left = BIT(m_st1, 8) ? s16(value) : s32(value);
	const s32 right = BIT(m_st1, 8) ? s16(immediate) : s32(immediate);
	const s32 result = left + right;
	const bool overflow = result < -0x8000 || result > 0x7fff;
	// ADDM stores 16 bits, but the ALU carry is taken at bit 32.
	m_st0 = (m_st0 & ~u16(0x0800)) |
		(u64(u32(left)) + u32(right) > 0xffffffffULL ? 0x0800 : 0);
	if (overflow) m_st0 |= 0x0400;
	return overflow && BIT(m_st1, 9)
		? u16(std::clamp(result, s32(-0x8000), s32(0x7fff))) : u16(result);
}

void tms320c54x_device::execute_one(u16 op)
{
	const u8 low = op;
	if ((op & 0xffe0) == 0xf4c0) // TRAP K (SPRU172C 4-195): no status changes.
	{
		push(m_pc);
		m_pc = (m_pmst & 0xff80) | ((op & 31) << 2);
		m_icount -= 2;
		return;
	}
	if ((op & 0xffe0) == 0xf7c0) // INTR K, independent of IMR/INTM (SPRU172C 4-65).
	{
		unsigned const vector = op & 31;
		m_rtn = m_pc; // SPRU131G example 7-6: software INTR also loads RTN.
		push(m_pc);
		if (vector >= 16) m_ifr &= ~u16(1U << (vector - 16));
		m_st1 |= 0x0800;
		m_pc = (m_pmst & 0xff80) | (vector << 2);
		m_icount -= 2;
		return;
	}
	// SPRU172C: far calls push PC then XPC; delayed transfers retain the
	// caller's page while the two delay words execute.
	if (m_extended_program && (op & 0xfc80) == 0xf880) // FB[D] / FCALL[D] extpmad
	{
		const u32 destination = (u32(op & 0x7f) << 16) | fetch();
		const bool delayed = BIT(op, 9);
		if (BIT(op, 8))
		{
			push(u16(m_pc + (delayed ? 2 : 0)));
			push(m_xpc);
		}
		far_transfer(destination, delayed);
		m_icount -= delayed ? 1 : 3;
		return;
	}
	if (m_extended_program && (op & 0xfcfe) == 0xf4e6) // FBACC[D] / FCALA[D] A/B
	{
		const u32 destination = accumulator(BIT(op, 8)) & 0x7fffff;
		const bool delayed = BIT(op, 9);
		if (BIT(op, 0))
		{
			push(u16(m_pc + (delayed ? 2 : 0)));
			push(m_xpc);
		}
		far_transfer(destination, delayed);
		m_icount -= delayed ? 3 : 5;
		return;
	}
	if (m_extended_program && (op & 0xfdfe) == 0xf4e4) // FRET[D] / FRETE[D]
	{
		const u32 page = pop() & 0x7f;
		const u32 destination = (page << 16) | pop();
		if (BIT(op, 0))
			m_st1 &= ~u16(0x0800);
		const bool delayed = BIT(op, 9);
		far_transfer(destination, delayed);
		m_icount -= delayed ? 3 : 5;
		return;
	}
	const auto branch_if = [this](bool condition)
	{
		const u16 destination = fetch();
		if (condition)
			m_pc = destination;
		m_icount -= condition ? 4 : 2;
	};
	if ((op & 0xfdfe) == 0xf802) // BC[D] pmad, BIO/NBIO (SPRU172C).
	{
		const u16 destination = fetch();
		const bool take = BIT(op, 0) ? !m_bio_in_cb() : bool(m_bio_in_cb());
		if (take)
		{
			if (BIT(op, 9))
			{
				m_delayed_target = destination;
				m_delayed_words = 2;
			}
			else
				m_pc = destination;
		}
		m_icount -= BIT(op, 9) ? 2 : take ? 4 : 2;
		return;
	}
	if ((op & 0xfff8) == 0xf4a0) // LD #k3,ARP (SPRU172C syntax 5).
	{
		m_st0 = (m_st0 & 0x1fff) | ((op & 7) << 13);
		return;
	}
	if ((op & 0xfcf8) == 0xf4a8) // CMPR CC, ARx (unsigned comparison with AR0)
	{
		u16 const left = m_ar[op & 7], right = m_ar[0];
		bool const conditions[] = {left == right, left < right, left > right, left != right};
		m_st0 = (m_st0 & ~u16(0x1000)) | (conditions[(op >> 8) & 3] ? 0x1000 : 0);
		return;
	}
	if ((op & 0xfdfb) == 0xf808) // BC[D] pmad, C/NC (SPRU172C condition codes).
	{
		const u16 destination = fetch();
		const bool take = BIT(m_st0, 11) == BIT(op, 2);
		if (take)
		{
			if (BIT(op, 9))
			{
				m_delayed_target = destination;
				m_delayed_words = 2;
			}
			else
				m_pc = destination;
		}
		m_icount -= BIT(op, 9) ? 2 : take ? 4 : 2;
		return;
	}
	if ((op & 0xfcff) == 0xf4e1 && op != 0xf7e1) // IDLE 1/2/3; NN=11 is reserved.
	{
		// SPRU172C encodes K=2 as NN=10 and K=3 as NN=01.
		u8 const mode = BIT(op, 9) ? 2 : BIT(op, 8) ? 3 : 1;
		if (mode >= 2 && !(m_tcr & TIMER_TSS))
		{
			// Peripheral clocks stop independently of TSS; preserve prescaler phase.
			m_idle_timer_ticks = timer_remaining_ticks();
			update_timer_counter();
			m_timer->adjust(attotime::never);
		}
		m_idle_mode = mode;
		// SPRU131G 6.11.3 and 8.5.2: IDLE3 halts the PLL. A later
		// software-managed DIV -> PLL entry must acquire lock again.
		if (mode == 3)
			m_pll_locked = false;
		m_peripheral_clock_stop_cb(mode >= 2);
		m_pll_clock_stop_cb(mode == 3);
		m_idle = true;
		m_icount -= 3; // Four-cycle minimum before the idle interval (SPRU172C).
		return;
	}
	if ((op & 0xfdf0) == 0xf5b0) // SSBX bit, ST0/ST1
	{
		u16 &status = BIT(op, 9) ? m_st1 : m_st0;
		status |= u16(1) << (low & 0x0f);
		// SPRU131G 6.10.7 protects this instruction and the following
		// complete instruction (not merely one program word).
		if (BIT(op, 9) && (low & 0x0f) == 11)
			m_intm_guard = 2;
		return;
	}
	if ((op & 0xfdf0) == 0xf4b0) // RSBX bit, ST0/ST1
	{
		u16 &status = BIT(op, 9) ? m_st1 : m_st0;
		status &= ~(u16(1) << (low & 0x0f));
		if (BIT(op, 9) && (low & 0x0f) == 11)
			m_intm_guard = 2;
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
				execute = execute && ((low & 3) == 3 ? !m_bio_in_cb() : (low & 3) == 2 && m_bio_in_cb());
			}
		}
		if (!execute)
		{
			m_pc += words;
			m_icount -= words; // Rejected slots execute as NOPs.
			m_xc_guard = 2 - words;
		}
		else
			// SPRU172C 4-199 protects two following words even for XC 1.
			m_xc_guard = 2;
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
		const bool choose_a = BIT(op, 0) ? a < b : a > b; // Equality selects B and sets C (SPRU172C 4-99/100).
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
		// Long Smem's address extension precedes the opcode-extension word.
		const bool extended = low >= 0xe0;
		const u16 address = low >= 0xf8 ? fetch() : extended ? long_offset_address(low) : 0;
		const u16 extension = fetch();
		const int shift = s8((extension & 0x1f) << 3) >> 3;
		auto shifted = [this, shift](u64 value)
		{
			return shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(value, -shift) : value >> -shift) :
					(value << shift) & ACC_MASK;
		};
		auto read_operand = [this, extended, address, low]()
		{
			return extended ? data_read(address) : indirect_read(low);
		};
		auto write_operand = [this, extended, address, low](u16 value)
		{
			if (extended)
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
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		const u8 x = op >> 4;
		const u8 y = op;
		const unsigned xar = 2 + (x & 3);
		const unsigned yar = 2 + (y & 3);
		const u16 coefficient_address = fetch();
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		const u16 coefficient = m_program.read_word(program_address(u16(coefficient_address +
				(repeated ? m_rpt_iteration : 0))));
		s64 product = s64(accumulator_high17(m_a)) * s64(s16(coefficient));
		if (BIT(m_st1, 6))
			product *= 2;
		m_b = multiply_result((s64(m_b << 24) >> 24) +
				multiply_product(product), true);
		m_a = add_sub((data_operand(xvalue) << 16) & ACC_MASK,
				(data_operand(yvalue) << 16) & ACC_MASK, false, false);
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
	case 0x0300: // ADDS Smem, B (SPRU172C 4-10)
	{
		u64 &destination = accumulator(BIT(op, 8));
		destination = add_sub(destination, alu_smem(), false, BIT(op, 8));
		return;
	}
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
	case 0x1600: // LDR Smem, A
	case 0x1700: // LDR Smem, B (SPRU172C 4-78)
		accumulator(BIT(op, 8)) = ((data_operand(alu_smem()) << 16) | 0x8000) & ACC_MASK;
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
	case 0x4600: // LD Smem,DP (SPRU172C syntax 2, three cycles).
	{
		const u16 value = indirect_read(low);
		m_st0 = (m_st0 & ~u16(0x01ff)) | (value & 0x01ff);
		m_icount -= 2 + (low >= 0xe0);
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
		m_rpt_end = REPEAT_END_UNSET;
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
	case 0x8d00: // ST TRN, Smem (SPRU172C 4-167)
		indirect_write(low, BIT(op, 8) ? m_trn : m_t);
		m_icount -= low >= 0xe0;
		return;
	case 0x8b00: // POPD Smem
	{
		const u16 destination = prepare_smem_address(low);
		const u16 value = pop();
		data_write(destination, value);
		finish_smem_access(low);
		m_icount -= low >= 0xe0;
		return;
	}
	case 0x7f00: // WRITA Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		m_program.write_word(physical_program_address(m_a + (repeated ? m_rpt_iteration : 0)),
				indirect_read(low));
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 5 : 4;
		return;
	}
	case 0x9400: case 0x9500: // LD Xmem, SHFT, dst (SPRU172C syntax 5).
	{
		const u8 operand = low >> 4;
		const u16 value = data_read(m_ar[2 + (operand & 3)]);
		accumulator(BIT(op, 8)) = shifted_load(data_operand(value), low & 15, BIT(op, 8));
		dual_modify(operand);
		return;
	}
	case 0x9600: // BIT Xmem, BITC; bit numbering is 15-BITC (SPRU172C).
	{
		const u8 operand = low >> 4;
		const u16 value = data_read(m_ar[2 + (operand & 3)]);
		m_st0 = (m_st0 & ~0x1000) | (BIT(value, 15 - (low & 15)) ? 0x1000 : 0);
		dual_modify(operand);
		return;
	}
	case 0x6100: // BITF Smem, #lk
	{
		const u16 address = prepare_smem_address(low);
		const u16 mask = fetch();
		const u16 value = data_read(address);
		finish_smem_access(low);
		m_st0 = (m_st0 & ~0x1000) | ((value & mask) ? 0x1000 : 0);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x6000: // CMPM Smem, #lk
	{
		const u16 address = prepare_smem_address(low);
		const u16 immediate = fetch();
		const u16 value = data_read(address);
		finish_smem_access(low);
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
			address = long_offset_address(low);
			immediate = fetch();
		}
		else
		{
			address = low >= 0xf8 ? fetch() : short_smem_address(low);
			immediate = fetch();
		}
		const u16 value = data_read(address);
		data_write(address, add_memory_word(value, immediate));
		if (low >= 0x80 && low < 0xe0)
			indirect_modify(low);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x6c00: // BANZ pmad, *ARx modification
	{
		const unsigned ar = indirect_ar(low);
		const bool extended = low >= 0xe0 && low < 0xf8;
		const bool branch = (extended ? long_offset_address(low) : m_ar[ar]) != 0;
		if (!extended) indirect_modify(low);
		const u16 destination = fetch();
		if (branch)
			m_pc = destination;
		m_icount -= (branch ? 3 : 1) + unsigned(extended);
		return;
	}
	case 0x6e00: // BANZD pmad, *ARx modification
	{
		const unsigned ar = indirect_ar(low);
		const bool extended = low >= 0xe0 && low < 0xf8;
		const bool branch = (extended ? long_offset_address(low) : m_ar[ar]) != 0;
		if (!extended) indirect_modify(low);
		const u16 destination = fetch();
		if (branch)
		{
			m_delayed_target = destination;
			m_delayed_words = 2;
		}
		m_icount -= 1 + unsigned(extended);
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
	case 0x8400: case 0x8500: // STL src, ASM, Smem
	case 0x8600: case 0x8700: // STH src, ASM, Smem
	{
		const int shift = s8((m_st1 & 0x1f) << 3) >> 3;
		const u64 source = accumulator(BIT(op, 8)) & ACC_MASK;
		const u64 shifted = shift < 0
			? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift)
			: (source << shift) & ACC_MASK;
		indirect_write(low, u16(saturated_store(shifted) >> (BIT(op, 9) ? 16 : 0)));
		m_icount -= low >= 0xe0;
		return;
	}
	case 0xe800: // LD #k, A
		m_a = data_operand(low);
		return;
	case 0xe900: // LD #k, B
		m_b = data_operand(low);
		return;
	case 0xea00: // LD #k9, DP (SPRU172C syntax 3)
	case 0xeb00:
		m_st0 = (m_st0 & ~u16(0x01ff)) | (op & 0x01ff);
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
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		const u16 address = prepare_smem_address(low);
		const u16 destination = fetch() + (repeated ? m_rpt_iteration : 0);
		const u16 value = data_read(address);
		finish_smem_access(low);
		data_write(destination, value);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x7300: // MVMD MMR, dmad
	{
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		const u16 destination = fetch() + (repeated ? m_rpt_iteration : 0);
		data_write(destination, data_read(mmr_address(low)));
		m_icount -= !repeated || !m_rpt_iteration;
		return;
	}
	case 0x7400: // PORTR port, Smem
	{
		u16 address, port;
		if (low >= 0xe0 && low < 0xf8)
		{
			port = fetch();
			address = prepare_smem_address(low);
		}
		else
		{
			address = prepare_smem_address(low);
			port = fetch();
		}
		data_write(address, m_io.read_word(port));
		finish_smem_access(low);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x7500: // PORTW Smem, port
	{
		u16 address, port;
		if (low >= 0xe0 && low < 0xf8)
		{
			port = fetch();
			address = prepare_smem_address(low);
		}
		else
		{
			address = prepare_smem_address(low);
			port = fetch();
		}
		const u16 value = data_read(address);
		finish_smem_access(low);
		m_io.write_word(port, value);
		m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x7000: // MVKD dmad, Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		if (low >= 0xe0 && low < 0xf8)
		{
			const u16 destination = prepare_smem_address(low);
			const u16 source = fetch() + (repeated ? m_rpt_iteration : 0);
			data_write(destination, data_read(source));
			finish_smem_access(low);
			if (!repeated || !m_rpt_iteration) m_icount -= 2;
			return;
		}
		const u16 source = fetch() + (repeated ? m_rpt_iteration : 0);
		const u16 destination = prepare_smem_address(low);
		data_write(destination, data_read(source));
		finish_smem_access(low);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 2 : 1;
		return;
	}
	case 0x7200: // MVDM dmad, MMR
	{
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
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
				address = long_offset_address(low);
				value = fetch();
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
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
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
		const u16 coefficient = m_program.read_word(program_address(u16(coefficient_address +
				(repeated ? m_rpt_iteration : 0))));
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
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		u16 address, destination;
		if (low >= 0xe0 && low < 0xf8)
		{
			destination = fetch() + (repeated ? m_rpt_iteration : 0);
			address = prepare_smem_address(low);
		}
		else
		{
			address = prepare_smem_address(low);
			destination = fetch() + (repeated ? m_rpt_iteration : 0);
		}
		const u16 value = data_read(address);
		finish_smem_access(low);
		m_program.write_word(program_address(destination), value);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 4 : 3;
		return;
	}
	case 0x7c00: // MVPD pmad, Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		u16 destination;
		u16 source;
		if (low >= 0xf8)
		{
			destination = prepare_smem_address(low);
			source = fetch();
		}
		else
		{
			source = fetch();
			destination = prepare_smem_address(low);
		}
		const u16 value = m_program.read_word(program_address(u16(source + (repeated ? m_rpt_iteration : 0))));
		data_write(destination, value);
		finish_smem_access(low);
		if (!repeated || !m_rpt_iteration)
			m_icount -= low >= 0xe0 ? 3 : 2;
		return;
	}
	case 0x7e00: // READA Smem
	{
		const bool repeated = (m_rptc || m_rpt_end != REPEAT_END_UNSET) &&
			u16(m_pc - 1) == m_rpt_address;
		const u16 destination = prepare_smem_address(low);
		const u16 value = m_program.read_word(physical_program_address(m_a +
				(repeated ? m_rpt_iteration : 0)));
		data_write(destination, value);
		finish_smem_access(low);
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
	if ((op & 0xf800) == 0xa800) // LD Xmem,dst || MAC[R]/MAS[R] Ymem,other
	{
		const u8 x = op >> 4, y = op;
		const unsigned xar = 2 + (x & 3), yar = 2 + (y & 3);
		const u16 xvalue = data_read(m_ar[xar]);
		const u16 yvalue = data_read(m_ar[yar]);
		const bool destination_b = BIT(op, 8);
		s64 product = s64(s16(m_t)) * s64(s16(yvalue));
		if (BIT(m_st1, 6)) product *= 2;
		product = multiply_product(product);
		s64 result = (s64(accumulator(!destination_b) << 24) >> 24) +
				(BIT(op, 10) ? -product : product);
		if (BIT(op, 9)) result = (result + 0x8000) & ~s64(0xffff);
		accumulator(!destination_b) = multiply_result(result, !destination_b);
		accumulator(destination_b) = (data_operand(xvalue) << 16) & ACC_MASK;
		dual_modify(x);
		if (xar != yar) dual_modify(y);
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
		const u64 xvalue = (data_operand(data_read(m_ar[xar])) << 16) & ACC_MASK;
		const u64 yvalue = (data_operand(data_read(m_ar[yar])) << 16) & ACC_MASK;
		const s64 old_a_high = accumulator_high17(m_a);
		s64 square = old_a_high * old_a_high;
		if (BIT(m_st1, 6))
			square *= 2;
		const u64 old_b = m_b & ACC_MASK;
		m_b = multiply_result((s64(old_b << 24) >> 24) + multiply_product(square), true);
		m_a = add_sub(xvalue, yvalue, true, false);
		dual_modify(x);
		if (xar != yar)
			dual_modify(y);
		return;
	}
	if ((op & 0xfe00) == 0x9e00 && (op & 7) >= 2) // SACCD (SPRU172C 4-152)
	{
		const u8 x = op >> 4;
		const unsigned xar = 2 + (x & 3);
		const u64 tested = accumulator(BIT(op, 3)) & ACC_MASK;
		const s64 signed_tested = s64(tested << 24) >> 24;
		bool take = false;
		switch (op & 7)
		{
		case 2: take = signed_tested >= 0; break;
		case 3: take = signed_tested < 0; break;
		case 4: take = tested != 0; break;
		case 5: take = tested == 0; break;
		case 6: take = signed_tested > 0; break;
		case 7: take = signed_tested <= 0; break;
		}
		const u16 address = m_ar[xar];
		u16 value = data_read(address);
		if (take)
		{
			const int shift = s8((m_st1 & 0x1f) << 3) >> 3;
			const u64 source = accumulator(BIT(op, 8)) & ACC_MASK;
			// SACCD obeys SXM, but is not an SST-saturating store (SPRU131G table 4-3).
			const u64 shifted = shift < 0
				? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift)
				: (source << shift) & ACC_MASK;
			value = u16(shifted >> 16);
		}
		// A false condition still performs a read/write-back bus transaction.
		data_write(address, value);
		dual_modify(x);
		return;
	}
	if ((op & 0xfc00) == 0x9800) // STL/STH src, SHFT, Xmem (SPRU172C 4-169/172)
	{
		const u8 x = op >> 4;
		const unsigned xar = 2 + (x & 3);
		const u32 value = saturated_store((accumulator(BIT(op, 8)) << (op & 15)) & ACC_MASK);
		data_write(m_ar[xar], u16(value >> (BIT(op, 9) ? 16 : 0)));
		dual_modify(x);
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
	if ((op & 0xfd80) == 0xf900) // CC[D] pmad, condition (SPRU172C 4-29)
	{
		const bool delayed = BIT(op, 9);
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
				take = take && BIT(m_st0, b ? 9 : 10);
			else if ((condition & 0x70) == 0x60)
				take = take && !BIT(m_st0, b ? 9 : 10);
		}
		else
		{
			take = true;
			if (condition & 0x30)
				take = take && ((condition & 0x30) == 0x30 ? BIT(m_st0, 12) : !BIT(m_st0, 12));
			if (condition & 0x0c)
				take = take && ((condition & 0x0c) == 0x0c ? BIT(m_st0, 11) : !BIT(m_st0, 11));
			if (condition & 3)
				take = take && ((condition & 3) == 3 ? !m_bio_in_cb() : (condition & 3) == 2 && m_bio_in_cb());
		}
		m_icount -= 2;
		if (take)
		{
			push(u16(m_pc + (delayed ? 2 : 0)));
			if (delayed)
			{
				m_delayed_target = destination;
				m_delayed_words = 2;
			}
			else
			{
				m_pc = destination;
				m_icount -= 2;
			}
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
	if ((op & 0xfdfe) == 0xfc02) // RC[D] BIO/NBIO.
	{
		const bool take = BIT(op, 0) ? !m_bio_in_cb() : bool(m_bio_in_cb());
		if (!BIT(op, 9))
			return_if(take);
		else
		{
			if (take)
			{
				m_delayed_target = pop();
				m_delayed_words = 2;
			}
			m_icount -= 2;
		}
		return;
	}
	if ((op & 0xfff0) == 0xfe40 && (op & 7) >= 2) // RCD accumulator conditions (SPRU172C 4-133)
	{
		const u64 raw = accumulator(BIT(op, 3)) & ACC_MASK;
		const s64 value = s64(raw << 24) >> 24;
		bool take = false;
		switch (op & 7)
		{
		case 2: take = value >= 0; break;
		case 3: take = value < 0; break;
		case 4: take = raw != 0; break;
		case 5: take = raw == 0; break;
		case 6: take = value > 0; break;
		case 7: take = value <= 0; break;
		}
		if (take)
		{
			m_delayed_target = pop();
			m_delayed_words = 2;
		}
		m_icount -= 2;
		return;
	}
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
		++m_sp; // SPRU172C: RTN supplies PC; no stack-memory read occurs.
		m_st1 &= ~0x0800;
		m_icount -= 2;
		return;
	case 0xf69b: // RETFD
		m_delayed_target = m_rtn;
		m_delayed_words = 2;
		++m_sp;
		m_st1 &= ~0x0800;
		return;
	case 0xfc30: // RETC TC
		return_if(m_st0 & 0x1000);
		return;
	case 0xfc45: // RETC AEQ
		return_if((m_a & ACC_MASK) == 0);
		return;
	case 0xfc43: // RC ALT (signed 40-bit accumulator)
		return_if((s64(m_a << 24) >> 24) < 0);
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
		m_st1 |= ST1_BRAF;
		m_icount -= 3;
		return;
	case 0xf272: // RPTBD pmad
		m_rea = fetch();
		// The two words after the two-word instruction are delay slots.  The
		// repeated block begins immediately after them.
		m_rsa = u16(m_pc + 2);
		m_st1 |= ST1_BRAF;
		--m_icount;
		return;
	case 0xf071: // RPTZ A, #lk
	case 0xf171: // RPTZ B, #lk
		accumulator(BIT(op, 8)) = 0;
		m_rptc = fetch();
		m_rpt_address = m_pc;
		m_rpt_end = REPEAT_END_UNSET;
		m_rpt_iteration = 0;
		m_rpt_armed = true;
		--m_icount;
		return;
	case 0xf074: // CALL pmad
	{
		const u16 destination = fetch();
		m_rtn = m_pc; // SPRU131G 7.1.2: CALL loads RTN before stacking it.
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
		m_rtn = m_pc;
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
	case 0xfa42: // BCD pmad, AGEQ
	case 0xfa46: // BCD pmad, AGT
	case 0xfa47: // BCD pmad, ALEQ
	case 0xfa20: // BCD pmad, NTC
	case 0xfa30: // BCD pmad, TC
	case 0xfa4d: // BCD pmad, BEQ
	case 0xfa4c: // BCD pmad, BNEQ
	case 0xfa4a: // BCD pmad, BGEQ
	case 0xfa4b: // BCD pmad, BLT
	case 0xfa4e: // BCD pmad, BGT
	case 0xfa4f: // BCD pmad, BLEQ
	{
		const u16 destination = fetch();
		const bool condition = op == 0xfa45 ? (m_a & ACC_MASK) == 0 :
				op == 0xfa44 ? (m_a & ACC_MASK) != 0 :
				op == 0xfa43 ? (s64(m_a << 24) >> 24) < 0 :
				op == 0xfa42 ? (s64(m_a << 24) >> 24) >= 0 :
				op == 0xfa46 ? (s64(m_a << 24) >> 24) > 0 :
				op == 0xfa47 ? (s64(m_a << 24) >> 24) <= 0 :
				op == 0xfa4d ? (m_b & ACC_MASK) == 0 :
				op == 0xfa4c ? (m_b & ACC_MASK) != 0 :
				op == 0xfa4a ? (s64(m_b << 24) >> 24) >= 0 :
				op == 0xfa4b ? (s64(m_b << 24) >> 24) < 0 :
				op == 0xfa4e ? (s64(m_b << 24) >> 24) > 0 :
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
	case 0xf492: // ROLTC A (SPRU172C 4-144)
	case 0xf592: // ROLTC B
	{
		u64 &acc = accumulator(BIT(op, 8));
		const u32 value = u32(acc);
		const u32 carry = BIT(m_st0, BIT(op, 1) ? 12 : 11);
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
		if ((op & 0xff00) == 0x4a00) // PSHM MMR, direct or indirect page-zero address.
		{
			push(data_read(mmr_address(low)));
			return;
		}
		if ((op & 0xff00) == 0x8a00 && low < 0xe0) // POPM MMR, direct or short-indirect page-zero address.
		{
			const u16 destination = mmr_address(low);
			data_write(destination, pop());
			return;
		}
		if ((op & 0xff00) == 0xec00) // RPT #k
		{
			m_rptc = low;
			m_rpt_address = m_pc;
			m_rpt_end = REPEAT_END_UNSET;
			m_rpt_iteration = 0;
			m_rpt_armed = true;
			return;
		}
		if (op == 0xf070) // RPT #lk
		{
			m_rptc = fetch();
			m_rpt_address = m_pc;
			m_rpt_end = REPEAT_END_UNSET;
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
		if (m_pipeline_frontend)
		{
			if (m_illegal || m_idle) { m_icount = 0; break; }
			pipeline_step();
			--m_icount;
			continue;
		}
		if (service_interrupt())
			continue;
		if (m_idle)
		{
			m_icount = 0;
			break;
		}
		debugger_instruction_hook(logical_program_address(m_pc));
		if (m_illegal)
		{
			m_icount = 0;
			break;
		}

		const bool delayed = m_delayed_words != 0;
		const bool xc_guarded = m_xc_guard != 0;
		const u16 instruction_pc = m_pc;
		const u32 instruction_address = logical_program_address(instruction_pc);
		const u16 previous_sp = m_sp;
		const bool repeat_was_armed = m_rpt_armed;
		m_op = fetch();
		m_capture_extensions = (m_op & 0xff00) == 0x6f00 && machine().options().verbose();
		if (m_capture_extensions)
		{
			m_instruction_words = {m_op, 0, 0};
			m_instruction_word_count = 1;
		}
		m_sp_address_late = (m_op & 0xff00) == 0x6f00;
		if (m_opcode_first_pc[m_op] == 0xffff)
			m_opcode_first_pc[m_op] = instruction_pc;
		++m_opcode_count[m_op];
		m_peripheral_mmr_access = false;
		execute_one(m_op);
		// SPRU172C memory-operand classes add one cycle for peripheral MMR
		// access. SPRU131G maps peripherals at 20..5f; 60..7f is scratch DARAM.
		// Charge once for an RMW instruction, not separately for read and write.
		if (m_peripheral_mmr_access) --m_icount;
		if (m_capture_extensions)
		{
			std::array<u16, 4> const words = {m_instruction_word_count,
				m_instruction_words[0], m_instruction_words[1], m_instruction_words[2]};
			auto entry = m_extension_observations.try_emplace(words,
				extension_observation{instruction_address}).first;
			++entry->second.count;
			m_capture_extensions = false;
		}
		update_xf();
		// One-cycle stack-operation group from SPRU131G table 7-9.
		// Calls/returns use the architectural SP, not this DAGEN snapshot.
		m_sp_address_pending = (m_op & 0xff00) == 0xee00 ||
			((m_op & 0xff00) == 0x4a00) ||
			((m_op & 0xff80) == 0x8a00 && (m_op & 0x7f) != 0x18);
		if (m_sp_address_pending) m_sp_address_base = previous_sp;
		if (!m_illegal)
		{
			if (m_intm_guard)
				--m_intm_guard;
			// A repeat redirect is not an executed instruction word. Measure
			// guard consumption before finish_repeats changes the continuation.
			const unsigned words = u16(m_pc - instruction_pc);
			// RPT/RPTZ arms the next instruction; its own retirement must not
			// consume one of the requested body executions.
			if (repeat_was_armed || !m_rpt_armed)
				finish_repeats();
			if (delayed)
			{
				m_delayed_words = words >= m_delayed_words ? 0 : m_delayed_words - words;
				if (!m_delayed_words)
				{
					m_pc = m_delayed_target;
					if (m_delayed_far)
						m_xpc = m_delayed_xpc;
					m_delayed_far = false;
				}
			}
			if (xc_guarded)
				m_xc_guard = words >= m_xc_guard ? 0 : m_xc_guard - words;
		}
		--m_icount;
	}
}

void tms320c54x_device::execute_set_input(int inputnum, int state)
{
	if (inputnum == INPUT_LINE_NMI)
	{
		bool const asserted = state != CLEAR_LINE;
		if (asserted && !m_nmi_asserted)
		{
			m_nmi_pending = true;
			leave_idle();
		}
		m_nmi_asserted = asserted;
		return;
	}
	if (inputnum >= 0 && inputnum < 16)
	{
		if (state != CLEAR_LINE)
		{
			m_ifr |= u16(1U << inputnum);
			if (BIT(m_imr, inputnum))
				leave_idle();
		}
	}
}
