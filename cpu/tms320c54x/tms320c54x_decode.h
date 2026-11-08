// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_CPU_TMS320C54X_DECODE_H
#define MAME_CPU_TMS320C54X_DECODE_H
#include "emucore.h"
#include <array>

namespace c54x_decode {

constexpr bool staged_cmps(u16 word)
{
	return (word & 0xfe00) == 0x8e00 && (u8(word) < 0x98 || (u8(word) >= 0xc0 && u8(word) < 0xe0));
}

constexpr bool staged_xmem_bit(u16 word)
{
	return (word & 0xff00) == 0x9600;
}

constexpr bool staged_xmem_load(u16 word)
{
	return (word & 0xfe00) == 0x9400;
}

constexpr bool staged_xmem_arithmetic(u16 word)
{
	return (word & 0xfc00) == 0x9000;
}

constexpr bool staged_xmem_store(u16 word)
{
	return (word & 0xfc00) == 0x9800;
}

constexpr bool staged_norm(u16 word)
{
	return (word & 0xfcff) == 0xf48f;
}

constexpr bool staged_minmax(u16 word)
{
	return (word & 0xfefe) == 0xf486;
}

constexpr bool staged_dual_multiply(u16 word)
{
	return (word & 0xfc00) == 0xa400 ||
		(word & 0xf000) == 0xb000; // MPY, MACSU, MAC[R]/MAS[R].
}

constexpr bool staged_lms(u16 word)
{
	return (word & 0xff00) == 0xe100;
}

constexpr bool staged_firs(u16 word)
{
	return (word & 0xff00) == 0xe000;
}

constexpr bool staged_coefficient_mac(u16 word)
{
	return (word & 0xfc00) == 0x7800 && u8(word) <= 0xf8 && (word & 0xf8) != 0x98;
}

constexpr bool staged_program_coefficient(u16 word)
{
	return staged_firs(word) || staged_coefficient_mac(word);
}

constexpr bool staged_load_mac(u16 word)
{
	return (word & 0xf800) == 0xa800; // LD Xmem || MAC[R]/MAS[R] Ymem.
}

constexpr bool staged_parallel_mac(u16 word)
{
	return (word & 0xf000) == 0xd000;
}

constexpr bool staged_parallel_mpy(u16 word)
{
	return (word & 0xfc00) == 0xcc00;
}

constexpr bool decode_staged_parallel_product(u16 word)
{
	return staged_parallel_mac(word) || staged_parallel_mpy(word);
}

constexpr bool decode_staged_parallel_load(u16 word)
{
	return (word & 0xfc00) == 0xc800 || (word & 0xfd00) == 0xe400;
}

constexpr bool staged_parallel_alu(u16 word)
{
	return (word & 0xf800) == 0xc000;
}

constexpr bool staged_conditional_store(u16 word)
{
	return (word & 0xfc00) == 0x9c00 && (word & 7) >= 2;
}

constexpr bool staged_dual_arithmetic(u16 word)
{
	return (word & 0xfc00) == 0xa000;
}

constexpr bool staged_distance(u16 word)
{
	return (word & 0xfe00) == 0xe200; // SQDST/ABDST Xmem, Ymem.
}

constexpr bool staged_mvdd(u16 word)
{
	return (word & 0xff00) == 0xe500;
}

constexpr bool staged_poly(u16 word)
{
	return (word & 0xff00) == 0x3600 && u8(word) < 0x98;
}

constexpr bool square_accumulate(u16 word)
{
	return (word & 0xfc00) == 0x3800;
}

constexpr bool register_product_form(u16 word)
{
	return (word & 0xfeff) == 0xf48c || (word & 0xfcfc) == 0xf488;
}

constexpr bool staged_repeat_accumulator_store(u16 word)
{
	// Circular indirect stores use the same per-iteration DAGEN/Write path
	// as ordinary indirect stores, including the mapped-destination guard.
	return (word & 0xfc00) == 0x8000 &&
		((u8(word) >= 0x80 && u8(word) < 0xa0) ||
		 (u8(word) >= 0xc0 && u8(word) < 0xe0));
}

constexpr bool staged_repeat_data_copy(u16 word)
{
	return (word & 0xfff8) == 0x7210 || ((word & 0xff00) == 0x7000 && u8(word) < 0xa0);
}

constexpr bool staged_repeat_destination_copy(u16 word)
{
	// MVMD AL/BL/ARx repeats a fixed MMR source into incrementing EAR
	// destinations (SPRU172C 4-115), unlike MVDK's Smem source DAGEN.
	return word == 0x7308 || word == 0x730b || (word & 0xfff8) == 0x7310 ||
		((word & 0xff00) == 0x7100 && (u8(word) < 0x98 || (u8(word) >= 0xa8 && u8(word) < 0xb8) || (u8(word) >= 0xc0 && u8(word) <= 0xdf)));
}

constexpr bool staged_mvmd_cleared_source(u16 move, u16 producer)
{
	// RPTZ's clear precedes its repeated body's first source sample
	// (SPRU172C 4-150). Do not apply this exception to other ALU producers.
	return (move == 0x7308 && producer == 0xf071) || (move == 0x730b && producer == 0xf171);
}

constexpr bool staged_accumulator_stm(u16 word)
{
	return word >= 0x7708 && word <= 0x770d;
}

constexpr bool staged_indirect_ram_stm(u16 word)
{
	return (word & 0xfff8) == 0x7780;
}

constexpr bool staged_direct_ram_stm(u16 word)
{
	// 5f is the unowned boundary word of the peripheral window: its board
	// mapping supplies storage/side effects, not a fabricated CPU register.
	return (word & 0xffe0) == 0x7760 || word == 0x775f;
}

constexpr bool staged_ram_stm(u16 word)
{
	return staged_direct_ram_stm(word) || staged_indirect_ram_stm(word);
}

constexpr bool staged_immediate_loop_register(u16 word)
{
	return word >= 0x771a && word <= 0x771c; // BRC, RSA, REA.
}

constexpr bool decode_staged_pipelined_repeat(u16 word)
{
	bool const short_mar = (word & 0xff00) == 0x6d00 && u8(word) >= 0x80 && u8(word) < 0xe0 && (word & 0xf8) != 0x98;
	bool const short_add = (word & 0xfe00) == 0 &&
		(u8(word) < 0x98 || (u8(word) >= 0xc0 && u8(word) < 0xe0));
	bool const shifted_circular_add = (word & 0xfc00) == 0x3c00 && u8(word) >= 0xc0 && u8(word) < 0xe0;
	bool const short_logic = ((word & 0xfe00) == 0x1800 || (word & 0xfe00) == 0x1a00 || (word & 0xfe00) == 0x1c00) &&
		(u8(word) < 0x98 || (u8(word) >= 0xc0 && u8(word) < 0xe0));
	bool const coefficient_repeat = staged_firs(word) || (staged_coefficient_mac(word) && !BIT(word, 9) && u8(word) < 0xe0);
	// SFTL consumes the live accumulator in Execute, once per retained issue.
	bool const logical_shift = (word & 0xfce0) == 0xf0e0;
	bool const accumulator_add_sub = (word & 0xfcff) == 0xf400 || (word & 0xfcff) == 0xf420;
	bool const short_double_add = (word & 0xfc00) == 0x5000 && u8(word) < 0x98;
	return accumulator_add_sub || short_double_add || logical_shift || coefficient_repeat || short_add || shifted_circular_add || short_logic || short_mar || staged_repeat_data_copy(word) || staged_repeat_destination_copy(word) || staged_repeat_accumulator_store(word) || staged_poly(word) || decode_staged_parallel_load(word) || staged_parallel_alu(word) || ((square_accumulate(word) || (word & 0xf800) == 0x2800) && u8(word) < 0x98) || (word & 0xf000) == 0xb000 || staged_mvdd(word) || (((word & 0xff00) == 0x4b00 || (word & 0xff00) == 0x8b00) && u8(word) < 0x98);
}

constexpr bool staged_repeat_accumulator_load(u16 word)
{
	return (word & 0xfce0) == 0xf440 || (word & 0xfcff) == 0xf482 || (word & 0xfce0) == 0xf460;
}

constexpr bool staged_repeat_port(u16 word)
{
	return (word & 0xfe00) == 0x7400 && u8(word) < 0x98;
}

constexpr bool decode_staged_immediate_alu(u16 word)
{
	u16 const form = word & 0xfcf0;
	u16 const high = word & 0xfcff;
	return form == 0xf000 || form == 0xf010 || form == 0xf030 || form == 0xf040 || form == 0xf050 ||
		(word & 0xfcfe) == 0xf060 || (high >= 0xf063 && high <= 0xf065);
}

constexpr bool decode_staged_tc_branch(u16 word)
{
	return word == 0xf820 || word == 0xf830 || word == 0xfa20 || word == 0xfa30 || word == 0xf920 || word == 0xf930 || word == 0xfb20 || word == 0xfb30;
}

constexpr bool decode_staged_conditional_call(u16 word)
{
	// CC/CCD admit TC/NTC and six signed predicates for either accumulator.
	u8 const condition = word;
	return word == 0xf920 || word == 0xf930 || word == 0xfb20 || word == 0xfb30 || ((word & 0xfd00) == 0xf900 &&
		(condition == 2 || condition == 3 || condition == 0x76 || (condition >= 0x42 && condition <= 0x47) || (condition >= 0x4a && condition <= 0x4f)));
}

constexpr bool staged_conditional_return(u16 word)
{
	u8 const condition = word;
	return word == 0xfc20 || word == 0xfc30 || ((word & 0xff00) == 0xfc00 && (condition == 2 || condition == 3 || (condition >= 0x42 && condition <= 0x47) || (condition >= 0x4a && condition <= 0x4f)));
}

constexpr bool staged_delayed_conditional_return(u16 word)
{
	return (word & 0xff00) == 0xfe00 && staged_conditional_return(word & 0xfdff);
}

constexpr bool staged_accumulator_branch(u16 word)
{
	u8 const condition = word;
	return (word & 0xfd00) == 0xf800 && ((condition >= 0x42 && condition <= 0x47) || (condition >= 0x4a && condition <= 0x4f));
}

constexpr bool staged_accumulator_condition(u16 word, u64 value)
{
	value &= 0xffffffffffULL;
	bool const negative = BIT(value, 39), zero = !value;
	switch (word & 7)
	{
	case 2: return !negative;
	case 3: return negative;
	case 4: return !zero;
	case 5: return zero;
	case 6: return !negative && !zero;
	case 7: return negative || zero;
	default: return false;
	}
}

constexpr bool decode_staged_conditional_branch(u16 word)
{
	return staged_accumulator_branch(word) || (word & 0xfde7) == 0xf860 || (word & 0xfdfe) == 0xf802 || (word & 0xfdfb) == 0xf808 || decode_staged_tc_branch(word) || decode_staged_conditional_call(word);
}

constexpr bool staged_bio_branch(u16 word)
{
	return (word & 0xfcfe) == 0xf802; // BC/BCD/CC/CCD BIO/NBIO.
}

constexpr bool staged_call(u16 word)
{
	return word == 0xf074 || word == 0xf274;
}

constexpr bool staged_software_intr(u16 word)
{
	return (word & 0xffe0) == 0xf7c0 || (word & 0xffe0) == 0xf4c0;
}

constexpr bool staged_far_branch(u16 word)
{
	return (word & 0xfd80) == 0xf880; // FB/FBD.
}

constexpr bool staged_far_call(u16 word)
{
	return (word & 0xfd80) == 0xf980;
}

constexpr bool staged_accumulator_far_call(u16 word)
{
	return (word & 0xfcff) == 0xf4e7; // FCALA/FCALAD A/B.
}

constexpr bool staged_accumulator_near_call(u16 word)
{
	return (word & 0xfcff) == 0xf4e3; // CALA/CALAD A/B.
}

constexpr bool staged_accumulator_call(u16 word)
{
	return staged_accumulator_far_call(word) || staged_accumulator_near_call(word);
}

constexpr bool staged_accumulator_near_branch(u16 word)
{
	return (word & 0xfcff) == 0xf4e2; // BACC/BACCD A/B.
}

constexpr bool staged_accumulator_far_branch(u16 word)
{
	return (word & 0xfcff) == 0xf4e6; // FBACC/FBACCD A/B.
}

constexpr bool staged_accumulator_transfer(u16 word)
{
	return staged_accumulator_call(word) || staged_accumulator_near_branch(word) || staged_accumulator_far_branch(word);
}

constexpr bool decode_staged_extended_move(u16 word)
{
	return (word & 0xff00) == 0x7100 && u8(word) >= 0xe0 && u8(word) <= 0xf8;
}

constexpr bool staged_circular_move(u16 word)
{
	return (word & 0xff00) == 0x7100 && u8(word) >= 0xc0 && u8(word) <= 0xdf;
}

constexpr bool staged_indexed_move(u16 word)
{
	return (word & 0xff00) == 0x7100 && u8(word) >= 0xa8 && u8(word) < 0xb8;
}

constexpr bool staged_direct_mvmd(u16 word)
{
	return (word & 0xfff8) == 0x7310 || (word >= 0x7308 && word <= 0x730f) || word == 0x7319 || word == 0x731a;
}

constexpr bool decode_staged_move(u16 word)
{
	return staged_direct_mvmd(word) || ((word & 0xff00) == 0x7100 && u8(word) < 0x98) || decode_staged_extended_move(word) || staged_circular_move(word) || staged_indexed_move(word);
}

constexpr bool staged_direct_mvdm(u16 word)
{
	return (word & 0xfff8) == 0x7210 || (word >= 0x7208 && word <= 0x720f);
}

constexpr bool staged_direct_push(u16 word)
{
	return (word & 0xffe0) == 0x4a00 || (word & 0xffe0) == 0x4a60 ||
		((word & 0xff00) == 0x4a00 && ((u8(word) >= 0x80 && u8(word) < 0x98) || (u8(word) >= 0xc0 && u8(word) < 0xe0)));
}

constexpr bool staged_mmr_pop(u16 word)
{
	return (word & 0xff00) == 0x8a00 && (u8(word) <= 1 || (u8(word) >= 6 && u8(word) <= 0x1d) || (u8(word) >= 0x60 && u8(word) < 0xe0));
}

constexpr bool decode_staged_data_copy(u16 word)
{
	return staged_direct_mvdm(word) || ((word & 0xff00) == 0x7000 && (u8(word) < 0xa0 || (u8(word) >= 0xc0 && u8(word) <= 0xf8)));
}

constexpr bool staged_delay_copy(u16 word)
{
	return (word & 0xfe00) == 0x4c00 && (u8(word) < 0x98 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
}

constexpr bool staged_extended_program_move(u16 word)
{
	return (word & 0xff00) == 0x7c00 && u8(word) >= 0xe0 && u8(word) <= 0xf8;
}

constexpr bool decode_staged_program_move(u16 word)
{
	return ((word & 0xff00) == 0x7c00 && u8(word) < 0xa0) || staged_extended_program_move(word);
}

constexpr bool staged_data_to_program(u16 word)
{
	return (word & 0xff00) == 0x7d00 && (u8(word) < 0x98 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
}

constexpr bool decode_staged_immediate_arp(u16 word)
{
	return (word & 0xfff8) == 0xf4a0;
}

constexpr bool decode_staged_immediate_asm(u16 word)
{
	return (word & 0xffe0) == 0xed00;
}

constexpr bool staged_read_a(u16 word)
{
	return (word & 0xff00) == 0x7e00 && (u8(word) < 0x98 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
}

constexpr bool staged_write_a(u16 word)
{
	return (word & 0xff00) == 0x7f00 && (u8(word) < 0x98 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
}

constexpr bool staged_memory_dp(u16 word)
{
	return (word & 0xff80) == 0x4600 || word == 0x46f8;
}

constexpr bool staged_extended_banz(u16 word)
{
	return (word & 0xfd00) == 0x6c00 && u8(word) >= 0xe0 && u8(word) < 0xf8;
}

constexpr bool staged_banz(u16 word)
{
	return ((word & 0xfd00) == 0x6c00 && u8(word) >= 0x80 && u8(word) < 0x98) || staged_extended_banz(word);
}

constexpr unsigned staged_repeat_body_words(u16 word)
{
	return word == 0xf495 || staged_repeat_accumulator_load(word) || ((word & 0xfe00) == 0x1e00 && u8(word) < 0x98) || (staged_write_a(word) && u8(word) < 0xe0) || (staged_read_a(word) && u8(word) != 0xf8) ? 1 : 2;
}

constexpr bool staged_short_repeat(u16 word)
{
	return (word & 0xff00) == 0xec00;
}

constexpr bool decode_staged_immediate_repeat(u16 word)
{
	return word == 0xf070 || (word & 0xfeff) == 0xf071;
}

constexpr bool invalid_mmr_long_form(u16 word)
{
	if (u8(word) < 0xe0)
		return false;
	// SPRU131G 5.6 excludes the long-offset and absolute Smem forms
	// from MMR addressing. The remaining low bytes f9..ff are reserved.
	switch (word & 0xff00)
	{
	case 0x4800: case 0x4900: case 0x4a00:
	case 0x7200: case 0x7300: case 0x7700:
	case 0x8800: case 0x8900: case 0x8a00:
		return true;
	default:
		return false;
	}
}

constexpr bool memory_source_operand(u16 word)
{
	// SPRU172C single-address Smem/Lmem read and RMW families. An implicit
	// destination (LTD, DELAY, MACD) does not turn the encoded source into a
	// write operand. MMR and shifted-extension grammars are handled separately.
	unsigned const family = word >> 8;
	return family <= 0x47 || (family >= 0x4b && family <= 0x4d) ||
			(family >= 0x50 && family <= 0x6b) || family == 0x71 ||
			family == 0x75 || (family >= 0x78 && family <= 0x7b) ||
			family == 0x7d || family == 0x7f;
}

constexpr bool extended_repeat_transfer(u16 word)
{
	if (u8(word) < 0xe0 || u8(word) > 0xf8)
		return false;
	// SPRU131G 5.2.4/5.5.3.2 exclude absolute and long-offset Smem
	// operands from a single-instruction repeat, in either transfer direction.
	switch (word & 0xff00)
	{
	case 0x7000: case 0x7100: case 0x7c00:
	case 0x7d00: case 0x7e00: case 0x7f00:
		return true;
	default:
		return false;
	}
}

constexpr bool decode_staged_memory_repeat(u16 word)
{
	// SPRU131G table 5-4: short MOD=3 preincrement is write-only.
	return (word & 0xff00) == 0x4700 && (u8(word) < 0x98 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
}

constexpr bool staged_extended_push(u16 word)
{
	return (word & 0xff00) == 0x4b00 && u8(word) >= 0xe0 && u8(word) <= 0xf8;
}

constexpr bool staged_extended_pop(u16 word)
{
	return (word & 0xff00) == 0x8b00 && u8(word) >= 0xe0 && u8(word) <= 0xf8;
}

constexpr bool staged_absolute_add(u16 word)
{
	return (word & 0xfeff) == 0x00f8;
}

constexpr bool staged_unsigned_memory_arithmetic(u16 word)
{
	return (word & 0xfe00) == 0x0200 || (word & 0xfe00) == 0x0a00;
}

constexpr bool staged_ts_arithmetic(u16 word)
{
	return (word & 0xfe00) == 0x0400 || (word & 0xfe00) == 0x0c00;
}

constexpr bool decode_staged_smem_arithmetic(u16 word)
{
	return !staged_absolute_add(word) && (word & 0xf8) != 0x98 && ((word & 0xfe00) == 0x0000 || (word & 0xfe00) == 0x0600 || (word & 0xfe00) == 0x0800 || (word & 0xfe00) == 0x0e00 || staged_unsigned_memory_arithmetic(word) || staged_ts_arithmetic(word) || (word & 0xfe00) == 0x1e00 || (word & 0xfc00) == 0x3c00 || (word & 0xfc00) == 0x4000) && u8(word) <= 0xf8;
}

constexpr bool staged_bitt(u16 word)
{
	return (word & 0xff00) == 0x3400 && u8(word) <= 0xf8 && (word & 0xf8) != 0x98;
}

constexpr bool decode_staged_smem_special(u16 word)
{
	return (staged_bitt(word) || square_accumulate(word) || staged_poly(word) || (word & 0xf900) == 0x3100 || (word & 0xf000) == 0x2000 || (word & 0xff00) == 0x3000 || (word & 0xff00) == 0x3200) && u8(word) <= 0xf8 && (word & 0xf8) != 0x98;
}

constexpr bool decode_staged_memory_immediate_multiply(u16 word)
{
	return ((word & 0xfe00) == 0x6200 || (word & 0xfc00) == 0x6400) &&
		(u8(word) < 0x98 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
}

constexpr bool decode_staged_immediate_multiply(u16 word)
{
	return (word & 0xfeff) == 0xf066 || (word & 0xfcff) == 0xf067 || decode_staged_memory_immediate_multiply(word);
}

constexpr bool staged_idle(u16 word)
{
	return (word & 0xfcff) == 0xf4e1 && word != 0xf7e1;
}

constexpr bool decode_staged_bitfield(u16 word)
{
	// CMPM and BITF share the retained Smem address and immediate-word path.
	// CMPM's mapped/peripheral-source exclusion is checked after DAGEN.
	return ((word & 0xff00) == 0x6100 || (word & 0xff00) == 0x6000) && u8(word) <= 0xf8 && (word & 0xf8) != 0x98;
}

constexpr bool decode_staged_port_write(u16 word)
{
	return (word & 0xff00) == 0x7500 && (u8(word) < 0x98 || (u8(word) >= 0xc0 && u8(word) <= 0xf8));
}

constexpr bool decode_staged_port_read(u16 word)
{
	return (word & 0xff00) == 0x7400 && (u8(word) < 0x98 || (u8(word) >= 0xc0 && u8(word) <= 0xf8));
}

constexpr bool decode_staged_mar(u16 word)
{
	return (word & 0xff00) == 0x6d00 &&
		((u8(word) >= 0x80 && u8(word) < 0x98) || (u8(word) >= 0xa0 && u8(word) < 0xe0) || (word & 0xf8) == 0xe8);
}

constexpr bool staged_long_mar(u16 word)
{
	return (word & 0xfff8) == 0x6de8;
}

constexpr bool decode_staged_block_repeat(u16 word)
{
	return word == 0xf072 || word == 0xf272;
}

constexpr bool decode_staged_memory_logic(u16 word)
{
	return ((word & 0xfe00) == 0x1800 || (word & 0xfe00) == 0x1a00 || (word & 0xfe00) == 0x1c00) &&
		(u8(word) < 0x98 || (u8(word) >= 0xc0 && u8(word) <= 0xf8));
}

constexpr bool staged_accumulator_logic(u16 word)
{
	return (word & 0xfce0) == 0xf080 || (word & 0xfce0) == 0xf0a0 || (word & 0xfce0) == 0xf0c0 || (word & 0xfcff) == 0xf493;
}

constexpr bool staged_accumulator_shift(u16 word)
{
	return (word & 0xfce0) == 0xf460 || (word & 0xfce0) == 0xf0e0 || (word & 0xfce0) == 0xf440 || (word & 0xfcff) == 0xf482;
}

constexpr bool staged_accumulator_arithmetic(u16 word)
{
	return (word & 0xfce0) == 0xf400 || (word & 0xfce0) == 0xf420 || (word & 0xfcfe) == 0xf480 || (word & 0xfcfe) == 0xf484;
}

constexpr bool decode_staged_memory_load(u16 word)
{
	return ((word & 0xfc00) == 0x1000 ||
		(word & 0xfc00) == 0x1400 || (word & 0xfe00) == 0x4400) && u8(word) <= 0xf8 && (word & 0xf8) != 0x98;
}

constexpr bool decode_staged_shifted_smem(u16 word)
{
	return (word & 0xff00) == 0x6f00 && u8(word) <= 0xf8;
}

constexpr bool staged_shifted_memory_store(u16 word, u16 extension)
{
	return decode_staged_shifted_smem(word) && ((extension & 0xfee0) == 0x0c60 || (extension & 0xfee0) == 0x0c80);
}

constexpr bool staged_shifted_memory_load(u16 word, u16 extension)
{
	return decode_staged_shifted_smem(word) && (extension & 0xfee0) == 0x0c40;
}

constexpr bool staged_shifted_memory_arithmetic(u16 extension)
{
	return (extension & 0xfce0) == 0x0c00 || (extension & 0xfce0) == 0x0c20;
}

constexpr bool decode_staged_xc(u16 word)
{
	u16 const form = word & ~u16(0x0200);
	return form == 0xfd00 || form == 0xfd02 || form == 0xfd03 || form == 0xfd08 || form == 0xfd0c || form == 0xfd20 || form == 0xfd30 ||
		(form >= 0xfd42 && form <= 0xfd47) || (form >= 0xfd4a && form <= 0xfd4f);
}

constexpr bool decode_staged_rmw(u16 word)
{
	return (word & 0xfc00) == 0x6800;
}

constexpr bool staged_rmw_addressing(u16 word)
{
	u8 const mode = u8(word);
	return mode < 0x98 || (mode >= 0xe0 && mode <= 0xf8) ||
		((word & 0xff00) == 0x6a00 && mode >= 0xc0 && mode < 0xe0);
}

constexpr bool staged_extended_rmw(u16 word)
{
	return decode_staged_rmw(word) && u8(word) >= 0xe0 && u8(word) < 0xf8;
}

constexpr bool decode_staged_extended_store(u16 word)
{
	return ((word & 0xfc00) == 0x8000 || (word & 0xfe00) == 0x8c00) && u8(word) >= 0xe0 && u8(word) <= 0xf8;
}

constexpr bool staged_asm_store(u16 word)
{
	// Each bit covers eight Smem encodings: 00..97, a8..b7, c0..df.
	constexpr u32 modes = ((1U << 19) - 1) | (3U << 21) | (15U << 24);
	return (word & 0xfc00) == 0x8400 && BIT(modes, (word >> 3) & 31);
}

constexpr bool decode_staged_store(u16 word)
{
	return ((word & 0xfc00) == 0x8000 && u8(word) < 0xe0) ||
		((word & 0xfe00) == 0x8c00 && u8(word) < 0x98) ||
		((word & 0xff00) == 0x8d00 && u8(word) >= 0xc0 && u8(word) < 0xe0) ||
		decode_staged_extended_store(word) || staged_asm_store(word);
}

constexpr bool staged_accumulator_mmr_store(u16 word)
{
	return (word & 0xfe00) == 0x8800 && (u8(word) <= 0x01 || u8(word) == 0x07 || (u8(word) >= 0x08 && u8(word) <= 0x17) || u8(word) == 0x19 || u8(word) == 0x1a || u8(word) == 0x1d || u8(word) == 0x21 || (u8(word) >= 0x60 && u8(word) < 0x98));
}

constexpr bool staged_mmr_load(u16 word)
{
	return (word & 0xfe00) == 0x4800 && u8(word) < 0x98;
}

constexpr bool staged_extended_dst(u16 word)
{
	return (word & 0xfe00) == 0x4e00 && u8(word) >= 0xe0 && u8(word) <= 0xf8;
}

constexpr bool staged_dst(u16 word)
{
	return (word & 0xfe00) == 0x4e00 && u8(word) <= 0xf8;
}

constexpr bool staged_dld(u16 word)
{
	return (word & 0xfe00) == 0x5600 && u8(word) <= 0xf8 && (word & 0xf8) != 0x98;
}

constexpr bool staged_extended_dld(u16 word)
{
	return staged_dld(word) && u8(word) >= 0xe0;
}

constexpr bool decode_staged_long_arithmetic(u16 word)
{
	return (word & 0xf000) == 0x5000 && (word & 0xfe00) != 0x5600 && u8(word) <= 0xf8 && (word & 0xf8) != 0x98;
}

constexpr bool staged_extended_long_arithmetic(u16 word)
{
	return decode_staged_long_arithmetic(word) && u8(word) >= 0xe0;
}

constexpr bool staged_indexed_immediate_store(u16 word)
{
	return (word & 0xff00) == 0x7600 && ((word & 0xf8) == 0xa8 || (word & 0xf8) == 0xb0);
}

constexpr bool staged_extended_immediate_store(u16 word)
{
	return (word & 0xff00) == 0x7600 && u8(word) >= 0xe0 && u8(word) < 0xf8;
}

constexpr bool staged_mmr_move(u16 word)
{
	return (word & 0xff00) == 0xe700 && ((word >> 4) & 15) <= 8 && (word & 15) <= 8;
}

// These predicates depend only on the opcode, not its address, extension word,
// or CPU state. Share their decode across stages without caching program memory
// (which may be modified between fetches). No save-state invalidation is needed.
inline const std::array<u32, 65536> staged_opcode_flags = []
{
	std::array<u32, 65536> result{};
	for (unsigned word = 0; word < result.size(); ++word)
	{
		result[word] =
			(decode_staged_memory_load(word) << 0) |
			(decode_staged_memory_logic(word) << 1) |
			(decode_staged_smem_arithmetic(word) << 2) |
			(decode_staged_data_copy(word) << 3) |
			(decode_staged_conditional_branch(word) << 4) |
			(decode_staged_smem_special(word) << 5) |
			(decode_staged_move(word) << 6) |
			(decode_staged_immediate_alu(word) << 7) |
			(decode_staged_immediate_multiply(word) << 8) |
			(decode_staged_conditional_call(word) << 9) |
			(decode_staged_store(word) << 10) |
			(decode_staged_tc_branch(word) << 11) |
			(decode_staged_pipelined_repeat(word) << 12) |
			(decode_staged_xc(word) << 13) |
			(decode_staged_program_move(word) << 14) |
			(decode_staged_bitfield(word) << 15) |
			(u32(decode_staged_long_arithmetic(word)) << 16) |
			(u32(decode_staged_immediate_repeat(word)) << 17) |
			(u32(decode_staged_extended_move(word)) << 18) |
			(u32(decode_staged_memory_immediate_multiply(word)) << 19) |
			(u32(decode_staged_port_write(word)) << 20) |
			(u32(decode_staged_block_repeat(word)) << 21) |
			(u32(decode_staged_extended_store(word)) << 22) |
			(u32(decode_staged_parallel_product(word)) << 23) |
			(u32(decode_staged_port_read(word)) << 24) |
			(u32(decode_staged_rmw(word)) << 25) |
			(u32(decode_staged_memory_repeat(word)) << 26) |
			(u32(decode_staged_immediate_arp(word)) << 27) |
			(u32(decode_staged_immediate_asm(word)) << 28) |
			(u32(decode_staged_parallel_load(word)) << 29) |
			(u32(decode_staged_shifted_smem(word)) << 30) |
			(u32(decode_staged_mar(word)) << 31);
	}
	return result;
}();

inline bool staged_memory_load(u16 word) { return BIT(staged_opcode_flags[word], 0); }
inline bool staged_memory_logic(u16 word) { return BIT(staged_opcode_flags[word], 1); }
inline bool staged_smem_arithmetic(u16 word) { return BIT(staged_opcode_flags[word], 2); }
inline bool staged_data_copy(u16 word) { return BIT(staged_opcode_flags[word], 3); }
inline bool staged_conditional_branch(u16 word) { return BIT(staged_opcode_flags[word], 4); }
inline bool staged_smem_special(u16 word) { return BIT(staged_opcode_flags[word], 5); }
inline bool staged_move(u16 word) { return BIT(staged_opcode_flags[word], 6); }
inline bool staged_immediate_alu(u16 word) { return BIT(staged_opcode_flags[word], 7); }
inline bool staged_immediate_multiply(u16 word) { return BIT(staged_opcode_flags[word], 8); }
inline bool staged_conditional_call(u16 word) { return BIT(staged_opcode_flags[word], 9); }
inline bool staged_store(u16 word) { return BIT(staged_opcode_flags[word], 10); }
inline bool staged_tc_branch(u16 word) { return BIT(staged_opcode_flags[word], 11); }
inline bool staged_pipelined_repeat(u16 word) { return BIT(staged_opcode_flags[word], 12); }
inline bool staged_xc(u16 word) { return BIT(staged_opcode_flags[word], 13); }
inline bool staged_program_move(u16 word) { return BIT(staged_opcode_flags[word], 14); }
inline bool staged_bitfield(u16 word) { return BIT(staged_opcode_flags[word], 15); }
inline bool staged_long_arithmetic(u16 word) { return BIT(staged_opcode_flags[word], 16); }
inline bool staged_immediate_repeat(u16 word) { return BIT(staged_opcode_flags[word], 17); }
inline bool staged_extended_move(u16 word) { return BIT(staged_opcode_flags[word], 18); }
inline bool staged_memory_immediate_multiply(u16 word) { return BIT(staged_opcode_flags[word], 19); }
inline bool staged_port_write(u16 word) { return BIT(staged_opcode_flags[word], 20); }
inline bool staged_block_repeat(u16 word) { return BIT(staged_opcode_flags[word], 21); }
inline bool staged_extended_store(u16 word) { return BIT(staged_opcode_flags[word], 22); }
inline bool staged_parallel_product(u16 word) { return BIT(staged_opcode_flags[word], 23); }
inline bool staged_port_read(u16 word) { return BIT(staged_opcode_flags[word], 24); }
inline bool staged_rmw(u16 word) { return BIT(staged_opcode_flags[word], 25); }
inline bool staged_memory_repeat(u16 word) { return BIT(staged_opcode_flags[word], 26); }
inline bool staged_immediate_arp(u16 word) { return BIT(staged_opcode_flags[word], 27); }
inline bool staged_immediate_asm(u16 word) { return BIT(staged_opcode_flags[word], 28); }
inline bool staged_parallel_load(u16 word) { return BIT(staged_opcode_flags[word], 29); }
inline bool staged_shifted_smem(u16 word) { return BIT(staged_opcode_flags[word], 30); }
inline bool staged_mar(u16 word) { return BIT(staged_opcode_flags[word], 31); }

inline bool staged_accumulator_destination_move(u16 word, u16 destination)
{
	return ((word & 0xff00) == 0x7100 || staged_direct_mvmd(word)) && staged_move(word) && destination >= 8 && destination <= 0x0d;
}

inline bool staged_clock_destination_move(u16 word, u16 destination)
{
	return (word & 0xff00) == 0x7100 && staged_move(word) && destination == 0x58;
}

inline bool staged_t_register_move(u16 word, u16 destination)
{
	return staged_move(word) && (destination == 0x0e || destination == 0x0f) && word != 0x7308 && word != 0x730b;
}

inline bool staged_extended_data_copy(u16 word)
{
	return staged_data_copy(word) && u8(word) >= 0xe0;
}

inline bool staged_two_word_data_copy(u16 word)
{
	return staged_data_copy(word) && !staged_extended_data_copy(word);
}

inline bool staged_extended_smem_arithmetic(u16 word)
{
	return staged_smem_arithmetic(word) && u8(word) >= 0xe0;
}

inline bool staged_extended_smem_special(u16 word)
{
	return staged_smem_special(word) && u8(word) >= 0xe0;
}

inline bool staged_extended_bitfield(u16 word)
{
	return staged_bitfield(word) && u8(word) >= 0xe0;
}

inline bool staged_extended_logic(u16 word)
{
	return staged_memory_logic(word) && u8(word) >= 0xe0;
}

inline bool staged_absolute_logic(u16 word)
{
	return staged_memory_logic(word) && u8(word) == 0xf8;
}

inline bool staged_absolute_load(u16 word)
{
	return staged_memory_load(word) && u8(word) == 0xf8;
}

inline bool staged_offset_load(u16 word)
{
	return staged_memory_load(word) && u8(word) >= 0xe0 && u8(word) < 0xf8;
}

// Retain the profile-dependent bus-register case in the live decoder. All
// other extension counts are a pure function of the fetched opcode.
inline u8 decode_staged_extensions(u16 word)
{
	if ((staged_delay_copy(word) && u8(word) >= 0xe0) || staged_data_to_program(word) || staged_extended_push(word) || (staged_write_a(word) && u8(word) >= 0xe0) ||
		staged_shifted_smem(word) || staged_extended_dld(word) || staged_offset_load(word) || staged_bitfield(word) ||
		staged_extended_smem_arithmetic(word) || staged_extended_smem_special(word) || staged_port_read(word) ||
		staged_port_write(word) || staged_absolute_add(word) || (staged_memory_repeat(word) && u8(word) >= 0xe0) || staged_banz(word) || (staged_read_a(word) && u8(word) >= 0xe0) ||
		staged_program_move(word) || staged_far_call(word) || (staged_absolute_load(word) || staged_extended_logic(word)) ||
		staged_far_branch(word) || staged_long_mar(word) || staged_move(word) || staged_call(word) ||
		staged_conditional_branch(word) || staged_immediate_alu(word) || (word & 0xfef0) == 0xf020 ||
		(word & 0xfeff) == 0xf062 || staged_extended_pop(word) || staged_immediate_multiply(word) ||
		staged_extended_store(word) || staged_extended_dst(word) || staged_extended_long_arithmetic(word) ||
		staged_data_copy(word) || staged_program_coefficient(word) || staged_immediate_repeat(word) || staged_block_repeat(word) || word == 0xf073 ||
		word == 0xf273 || word == 0x46f8 || staged_ram_stm(word) || staged_accumulator_stm(word) || word == 0x7707 ||
		word == 0x771d || word == 0x771e || staged_immediate_loop_register(word) || word == 0x7700 ||
		((word >= 0x7724 && word <= 0x7726) || (word == 0x7721 || word == 0x7722) || (word == 0x7758)) || word == 0x770e ||
		((word & 0xff00) == 0x7700 && u8(word) >= 0x10 && u8(word) <= 0x19) ||
		((word & 0xff00) == 0x7600 && (u8(word) == 0xf8 || u8(word) < 0xa0 || staged_indexed_immediate_store(word) || staged_extended_immediate_store(word))) ||
		(staged_rmw(word) && staged_rmw_addressing(word)))
	{
		return (staged_extended_program_move(word) || (staged_coefficient_mac(word) && u8(word) >= 0xe0) || (staged_data_to_program(word) && u8(word) >= 0xe0) ||
		((staged_shifted_smem(word) || staged_memory_immediate_multiply(word)) && u8(word) >= 0xe0) ||
		staged_extended_banz(word) || staged_extended_bitfield(word) ||
		(staged_rmw(word) && (staged_extended_rmw(word) || u8(word) == 0xf8)) || staged_extended_move(word) ||
		staged_extended_immediate_store(word) || staged_extended_data_copy(word) ||
		((staged_port_read(word) || staged_port_write(word)) && u8(word) >= 0xe0) ||
		word == 0x76f8) ? 2 : 1;
	}
	return 0;
}

// Retirement lengths are independent of extensions and live CPU state. Keep
// their existing admitted-form rules distinct from the decode extension count.
inline u8 decode_staged_retired_words(u16 word)
{
	if (word == 0x46f8 || (staged_memory_repeat(word) && u8(word) >= 0xe0)) return 2;
	if (((staged_shifted_smem(word) || staged_memory_immediate_multiply(word)) && u8(word) >= 0xe0) ||
		staged_extended_banz(word) || staged_extended_bitfield(word) || staged_extended_rmw(word) ||
		staged_extended_move(word) || staged_extended_data_copy(word)) return 3;
	if (staged_port_read(word) || staged_port_write(word)) return u8(word) >= 0xe0 ? 3 : 2;
	if (staged_extended_immediate_store(word)) return 3;
	if (staged_data_copy(word)) return 2;
	if ((staged_delay_copy(word) && u8(word) >= 0xe0) || staged_banz(word) || staged_extended_dld(word) || staged_offset_load(word) || staged_bitfield(word) ||
		staged_absolute_add(word) || staged_extended_smem_arithmetic(word) || staged_extended_smem_special(word) ||
		staged_shifted_smem(word) || staged_extended_store(word) || staged_extended_dst(word) || (staged_read_a(word) && u8(word) >= 0xe0) ||
		staged_extended_long_arithmetic(word)) return 2;
	if ((staged_rmw(word) && u8(word) == 0xf8) || word == 0x76f8 || (staged_coefficient_mac(word) && u8(word) >= 0xe0)) return 3;
	return (staged_program_coefficient(word) || staged_far_call(word) || staged_absolute_load(word) || staged_extended_logic(word) || staged_far_branch(word) ||
		staged_long_mar(word) || staged_move(word) || staged_call(word) || staged_conditional_branch(word) ||
		staged_immediate_alu(word) || (word & 0xfef0) == 0xf020 || (word & 0xfeff) == 0xf062 || staged_immediate_multiply(word) ||
		staged_rmw(word) || (word & 0xff00) == 0x7700 || (word & 0xff00) == 0x7600 || staged_block_repeat(word) ||
		word == 0xf073 || word == 0xf273) ? 2 : 1;
}

// Opcode admission only: extension, delay, repeat and operand checks remain
// live. Bus-register admission is profile-dependent and stays with the caller.
inline bool decode_staged_supported(u16 word, bool extended_program)
{
	bool const software_intr = staged_software_intr(word);
	bool const short_repeat = staged_short_repeat(word);
	bool const short_load = staged_memory_load(word);
	bool const direct_store = staged_store(word) || staged_accumulator_mmr_store(word);
	bool const direct_long_load = staged_dld(word);
	bool const direct_long_store = staged_dst(word);
	bool const long_arithmetic = staged_long_arithmetic(word);
	// Register forms fetch no data operands and redirect no control flow.
	// Accumulators are Execute-owned; T/TS sampling has dedicated Read handlers.
	bool const register_rotate = (word & 0xfeff) == 0xf490 || (word & 0xfeff) == 0xf491 || (word & 0xfeff) == 0xf492;
	bool const register_alu = register_rotate || (word & 0xfeff) == 0xf483 || register_product_form(word) ||
		staged_accumulator_logic(word) || staged_accumulator_shift(word) || staged_accumulator_arithmetic(word) || staged_norm(word) ||
		staged_minmax(word) || (word & 0xfcff) == 0xf49f || (word & 0xfeff) == 0xf48d || (word & 0xfeff) == 0xf48e;
	bool const direct_mmr_load = staged_mmr_load(word);
	bool const mmr_move = staged_mmr_move(word);
	bool const memory_dp = staged_memory_dp(word);
	bool const direct_move = staged_move(word);
	bool const data_copy = staged_data_copy(word) || staged_delay_copy(word);
	bool const program_move = staged_program_move(word);
	bool const read_a = staged_read_a(word) || staged_write_a(word) || staged_data_to_program(word);
	bool const immediate_store = (word & 0xff00) == 0x7600 && (u8(word) == 0xf8 || u8(word) < 0xa0 ||
		staged_indexed_immediate_store(word) || staged_extended_immediate_store(word));
	bool const long_load = (word & 0xfef0) == 0xf020 || (word & 0xfeff) == 0xf062;
	bool const rmw = staged_rmw(word) && staged_rmw_addressing(word);
	bool const fast_return = word == 0xf49b || word == 0xf69b;
	bool const plain_return = word == 0xfc00 || word == 0xf4eb || staged_conditional_return(word);
	bool const delayed_return = word == 0xfe00 || staged_delayed_conditional_return(word);
	bool const far_return = (word & 0xfdfe) == 0xf4e4;
	bool const status_bit = (word & 0xfcf0) == 0xf4b0;
	bool const immediate_dp = (word & 0xfe00) == 0xea00;
	bool const immediate_alu = staged_immediate_alu(word) || staged_immediate_multiply(word);
	bool const smem_arithmetic = staged_smem_arithmetic(word) || staged_smem_special(word) || staged_shifted_smem(word);
	bool const direct_push = staged_direct_push(word);
	bool const data_push = (word & 0xff00) == 0x4b00 && (u8(word) < 0x98 || staged_extended_push(word));
	bool const data_pop = (word & 0xff00) == 0x8b00 && (u8(word) < 0x98 || staged_extended_pop(word));
	bool const mmr_pop = staged_mmr_pop(word);
	bool const frame = (word & 0xff00) == 0xee00;
	bool const branch = staged_banz(word) || staged_block_repeat(word) || word == 0xf073 || word == 0xf273 ||
		staged_conditional_branch(word) || staged_call(word) || staged_accumulator_near_call(word) ||
		staged_accumulator_near_branch(word) || (extended_program && (staged_far_branch(word) || staged_far_call(word) ||
		staged_accumulator_far_call(word) || staged_accumulator_far_branch(word)));
	bool const dagen_immediate = (word & 0xff00) == 0x7700 && u8(word) >= 0x10 && u8(word) <= 0x19;
	bool const immediate_loop = staged_immediate_loop_register(word);
	bool const immediate_t = word == 0x770e;
	bool const immediate_control = staged_ram_stm(word) || staged_accumulator_stm(word) || word == 0x7707 || word == 0x771d || word == 0x771e ||
		word == 0x7700 || ((word >= 0x7724 && word <= 0x7726) || (word == 0x7721 || word == 0x7722) || (word == 0x7758));
	return staged_cmps(word) || software_intr || staged_port_write(word) || staged_absolute_add(word) || staged_memory_repeat(word) ||
		staged_immediate_arp(word) || staged_immediate_asm(word) || staged_immediate_repeat(word) || word == 0xf495 ||
		staged_idle(word) || (word & 0xfcf8) == 0xf4a8 || staged_memory_logic(word) || staged_mar(word) || staged_xc(word) ||
		fast_return || plain_return || delayed_return || far_return || status_bit || immediate_dp || immediate_alu || branch ||
		dagen_immediate || immediate_loop || immediate_t || immediate_control || (word & 0xff00) == 0xe800 || (word & 0xff00) == 0xe900 ||
		short_repeat || frame || direct_push || data_push || data_pop || mmr_pop || direct_long_load || direct_long_store ||
		long_arithmetic || register_alu || direct_mmr_load || mmr_move || memory_dp || direct_move || data_copy || program_move ||
		read_a || short_load || direct_store || immediate_store || long_load || rmw || staged_xmem_bit(word) || staged_xmem_load(word) ||
		staged_xmem_arithmetic(word) || staged_xmem_store(word) || staged_conditional_store(word) || staged_dual_multiply(word) ||
		staged_lms(word) || staged_program_coefficient(word) || staged_load_mac(word) || staged_parallel_alu(word) || staged_parallel_product(word) || staged_parallel_load(word) ||
		staged_distance(word) || staged_dual_arithmetic(word) || staged_mvdd(word) || staged_port_read(word) || smem_arithmetic ||
		staged_bitfield(word);
}

inline bool decode_staged_access_sp(u16 word)
{
	return staged_cmps(word) || staged_coefficient_mac(word) || staged_data_copy(word) || staged_delay_copy(word) || staged_long_arithmetic(word) ||
		staged_memory_repeat(word) || staged_memory_load(word) || staged_memory_immediate_multiply(word) ||
		(word & 0xff00) == 0x7600 || staged_bitfield(word) || staged_smem_arithmetic(word) ||
		staged_smem_special(word) || staged_shifted_smem(word) || staged_port_read(word) || staged_port_write(word) ||
		staged_rmw(word) || staged_memory_logic(word) || staged_store(word) || (word & 0xfe80) == 0x5600 ||
		(word & 0xfe80) == 0x4e00 || (word & 0xff80) == 0x4600 || staged_move(word) ||
		staged_program_move(word) || staged_read_a(word) || (word & 0xff00) == 0x8b00;
}

inline bool decode_staged_read_memory(u16 word)
{
	// Shifted Smem forms also depend on the extension and remain classified live.
	return staged_memory_immediate_multiply(word) || staged_port_write(word) || staged_absolute_add(word) ||
		staged_memory_load(word) || staged_memory_logic(word) || staged_rmw(word) || staged_bitfield(word) ||
		staged_smem_arithmetic(word) || staged_smem_special(word) || staged_xmem_bit(word) ||
		staged_xmem_load(word) || staged_xmem_arithmetic(word) || staged_mmr_load(word);
}

// Bits 0..1: extensions; 2..3: retirement span; 4/5: base/extended admission;
// 6: Access SP snapshot; 7: opcode-only generic Read operand.
inline const std::array<u8, 65536> staged_opcode_descriptor = []
{
	std::array<u8, 65536> result{};
	for (unsigned word = 0; word < result.size(); ++word)
		result[word] = decode_staged_extensions(word) | (decode_staged_retired_words(word) << 2) |
			(decode_staged_supported(word, false) << 4) | (decode_staged_supported(word, true) << 5) |
			(decode_staged_access_sp(word) << 6) | (decode_staged_read_memory(word) << 7);
	return result;
}();

} // namespace c54x_decode

#endif
