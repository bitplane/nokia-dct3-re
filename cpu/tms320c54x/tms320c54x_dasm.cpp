// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_dasm.h"
#include "tms320c54x_decode.h"

offs_t tms320c54x_disassembler::disassemble(std::ostream &stream, offs_t pc,
			const data_buffer &opcodes, const data_buffer &params)
{
	u16 const op = opcodes.r16(pc);
	auto const next_word = [&](unsigned index = 1) { return opcodes.r16((pc & 0x7f0000) | u16(pc + index)); };
	auto const target = [&]() { return (pc & 0x7f0000) | next_word(); };
	u8 const mode = op;
	u16 const family = op & 0xff00;
	// SPRU172C 4-34: CC occupies bits 9..8 and ARx occupies bits 2..0.
	if ((op & 0xfcf8) == 0xf4a8)
	{
		util::stream_format(stream, "CMPR    %u, AR%u", (op >> 8) & 3, op & 7);
		return 1 | SUPPORTED;
	}
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
		// Short MOD=3 is write-only (SPRU131G table 5-4); f9..ff
		// are reserved. Long MOD=13 remains a valid source form.
		bool const operand_valid = mode <= 0xf8 &&
			(!((mode & 0xf8) == 0x98) || high || low);
		if (operand_valid && (arithmetic || load || high || low))
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
	if ((op & 0xfeff) == 0xf066)
	{
		util::stream_format(stream, "MPY     #$%04X, %c", next_word(), BIT(op, 8) ? 'B' : 'A');
		return 2 | SUPPORTED;
	}
	if ((family == 0x6200 || family == 0x6300) && mode <= 0xf8 && (mode & 0xf8) != 0x98)
	{
		// MPY's address extension precedes its immediate (SPRU172C syntax 3).
		bool const extended = mode >= 0xe0;
		stream << "MPY     "; smem(extended ? next_word() : 0);
		util::stream_format(stream, ", #$%04X, %c", next_word(extended ? 2 : 1), BIT(op, 8) ? 'B' : 'A');
		return (2 + extended) | SUPPORTED;
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
	if ((op & 0xfc00) == 0x9c00 && (op & 7) >= 2)
	{
		bool const accumulator_store = BIT(op, 9);
		util::stream_format(stream, "%-8s", accumulator_store ? "SACCD" : BIT(op, 8) ? "SRCCD" : "STRCD");
		if (accumulator_store) util::stream_format(stream, "%c, ", BIT(op, 8) ? 'B' : 'A');
		dual(op >> 4);
		util::stream_format(stream, ", %s", predicate_name(0x40 | (op & 15)));
		return 1 | SUPPORTED;
	}
	if ((op & 0xff00) == 0x9600)
	{
		stream << "BIT     "; dual(op >> 4);
		util::stream_format(stream, ", #%u", op & 15);
		return 1 | SUPPORTED;
	}
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
		util::stream_format(stream, "MVPD    $%04X, ", extended ? second : first);
		smem(extended ? first : 0);
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
	if ((family >= 0x2000 && family <= 0x2700) || family == 0x3400)
	{
		bool const extended = mode >= 0xe0;
		char const *name = family == 0x3400 ? "BITT" : family >= 0x2600 ? "SQUR" : family >= 0x2400 ? "MPYU" : family >= 0x2200 ? "MPYR" : "MPY";
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
	if (c54x_decode::square_accumulate(op))
	{
		bool const extended = mode >= 0xe0;
		stream << (BIT(op,9) ? "SQURS   " : "SQURA   "); smem(extended ? next_word() : 0);
		util::stream_format(stream, ", %c", BIT(op,8) ? 'B' : 'A');
		return (1 + extended) | SUPPORTED;
	}
	if (family == 0x3600)
	{
		bool const extended = mode >= 0xe0;
		stream << "POLY    "; smem(extended ? next_word() : 0);
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
		u16 const offset = extended ? next_word() : 0;
		if (family == 0x7e00)
		{
			stream << "READA   "; smem(offset);
			return (1 + extended) | SUPPORTED;
		}
		u16 const operand = next_word(extended ? 2 : 1);
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
	if ((family == 0x6800 || family == 0x6900 || family == 0x6a00 || family == 0x6b00 || family == 0x7600 || (family == 0x7700 && mode < 0xe0)) &&
		((family & 0xfc00) != 0x6800 || (mode & 0xf8) != 0x98))
	{
		bool const extended = family != 0x7700 && mode >= 0xe0;
		u16 const immediate = next_word(extended ? 2 : 1);
		u16 const offset = extended ? next_word(1) : 0;
		char const *const name = family == 0x6800 ? "ANDM" : family == 0x6900 ? "ORM" : family == 0x6a00 ? "XORM" : family == 0x6b00 ? "ADDM" : family == 0x7600 ? "ST" : "STM";
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
