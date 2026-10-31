// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {















std::array<u64, 2> tms320c54x_test_state::repeat_subc_result(memory_repeat_fixture const &f, unsigned iterations)
{
	u64 accumulator = f.dividend; u16 flags = 0x1804;
	for (unsigned i = 0; i < iterations; ++i)
	{
		s64 const dividend = BIT(accumulator, 39) ? s64(accumulator) - (s64(1) << 40) : s64(accumulator);
		s64 const divisor = f.flags & 1 ? s16(f.divisor) : s64(f.divisor);
		s64 const difference = dividend - divisor * 32768;
		bool const subtract = difference >= 0;
		flags = (flags & ~0x800) | (subtract ? 0x800 : 0);
		if (difference < -2147483648LL || difference > 2147483647LL) flags |= f.destination ? 0x200 : 0x400;
		accumulator = u64(subtract ? difference * 2 + 1 : dividend * 2) & 0xffffffffffULL;
	}
	return {accumulator, flags};
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_multiply_case(unsigned variant, bool mac, bool rounded)
{
	unsigned const group = mac || rounded ? variant / 18432 : 0;
	variant %= 18432;
	unsigned const kind = variant % 6, form = (variant / 6) % 4, destination = (variant / 24) % 2, pair = (variant / 48) % 16, flags = (variant / 768) % 4;
	u16 const memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair % 4], t = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair / 4];
	u32 const caller = (variant / 3072) % 2 ? 0x20000 : 0, target = kind == 4 ? caller ^ 0x20000 : caller;
	s64 const product = s64(s16(memory)) * s16(t) * (flags & 1 ? 2 : 1);
	u64 const result = u64(product > 0x7fffffffLL && (flags & 2) ? 0x7fffffffLL : product) & 0xffffffffffULL;
	unsigned const read = delay_multiply_fixture::operand_read(kind, form);
	delay_multiply_fixture f{kind, form, destination, flags, std::array<unsigned, 3>{3, read - 1, read}[variant / 6144], memory, t,
		std::array<u16, 4>{0x5000, 0x5000, u16(mac ? 0x15 : 0x17), 0x5003}[form], caller, target,
		kind == 1 ? 0 : kind == 2 ? 1 : 0x123456789aULL, 0xfedcba9876ULL, result};
	if (mac)
	{
		f.accumulate = true; f.operation = group % 4; f.flags |= group / 4 ? 4 : 0;
		s64 const value = f.arithmetic();
		f.result = u64(f.overflow() && (f.flags & 2) ? std::clamp(value, s64(-2147483648LL), s64(2147483647LL)) : value) & 0xffffffffffULL;
	}
	if (rounded)
	{
		f.rounded = true; f.flags |= group ? 4 : 0;
		s64 const value = f.arithmetic();
		f.result = u64(f.flags & 2 ? std::clamp(value, s64(-2147483648LL), s64(2147483647LL)) : value) & 0xffffffffffULL;
	}
	return f;
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_accumulator_multiply_case(unsigned variant)
{
	unsigned v = variant;
	delay_multiply_fixture f{};
	f.kind = v % 6; v /= 6; f.form = v % 3; v /= 3;
	f.operation = v % 4; v /= 4; unsigned const pattern = v % 8; v /= 8;
	f.flags = v % 16; v /= 16; f.caller = v % 2 ? 0x20000 : 0; v /= 2;
	f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
	f.accumulator_source = true; f.destination = 1; f.t = 0x5aa5;
	f.source = f.form ? 0x5000 : 0x1062;
	s32 const high = std::array<s32, 8>{0, 1, -1, 32767, -32768, 65535, -65536, 0x1234}[pattern];
	f.a = f.kind == 1 ? 0 : f.kind == 2 ? 1 : (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	f.b = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[pattern];
	f.memory = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8000, 0xffff, 0x5678}[pattern];
	f.checkpoint = std::array<unsigned, 3>{3, f.read() - 1, f.read()}[v];
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::bd_long_accumulator_case(unsigned v)
{
	delay_multiply_fixture f{};
	f.extended_accumulator = f.accumulator_source = true; f.destination = 1;
	f.form = v % 3; v /= 3; f.address_ar = v % 8; v /= 8;
	f.operation = v % 4; v /= 4; unsigned const pattern = v % 8; v /= 8;
	f.flags = v % 16; v /= 16; f.caller = v % 2 ? 0x20000 : 0; v /= 2;
	f.target = f.caller; f.t = 0x5aa5;
	f.source = f.form == 2 ? 0x5000 : f.form ? 0x3b08 : 0x5003;
	s32 const high = std::array<s32,8>{0,1,-1,32767,-32768,65535,-65536,0x1234}[pattern];
	f.a = (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	f.b = std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x1234567833ULL,0x80aa001234ULL}[pattern];
	f.memory = std::array<u16,8>{0,1,0xffff,0x7fff,0x8000,0x8000,0xffff,0x5678}[pattern];
	f.checkpoint = std::array<unsigned,4>{3,5,8,9}[v % 4];
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

void tms320c54x_test_state::start_delay_multiply(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const mac = profile_is("mac-delay");
	bool const square = profile_is("square-delay");
	bool const memory_square = profile_is("squr-delay");
	bool const long_acc = profile_is("bd-maca-long");
	bool const acc = square || memory_square || long_acc || profile_is("maca-delay");
	bool const rounded = profile_is("mpyr-delay");
	auto const f = long_acc ? bd_long_accumulator_case(variant) : memory_square ? delay_memory_square_case(variant) : square ? delay_square_case(variant) : acc ? delay_accumulator_multiply_case(variant) : delay_multiply_case(variant, mac, rounded); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.caller + 0x600 + i, 0xf495);
	program.write_word(f.caller + 0x600, f.kind == 4 ? 0xfb80 | (f.target >> 16) : std::array<u16, 6>{0xf273, 0xfa45, 0xfa45, 0xf274, 0, 0xfe00}[f.kind]);
	if (f.kind != 5) program.write_word(f.caller + 0x601, 0x700);
	program.write_word(f.caller + 0x600 + f.delay(), memory_square ? 0x2600 | (f.destination << 8) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92) : square ? (0x3800 | (f.operation << 9) | (f.destination << 8) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92)) : acc ? (0x3100 + f.operation * 0x200) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92) : (mac ? 0x2800 + f.operation * 0x200 : rounded ? 0x2200 : 0x2000) | (f.destination << 8) | (f.form == 0 ? rounded ? 0x92 : 0x82 : f.form == 3 ? 0xe2 : 0xf8));
	if (long_acc) program.write_word(f.caller + 0x602,(0x3100 + f.operation * 0x200) | (f.form == 2 ? 0xf8 : 0xe0 | f.address_ar));
	if (f.form && !acc) program.write_word(f.caller + 0x601 + f.delay(), f.form == 3 ? 3 : f.source);
	if (long_acc) program.write_word(f.caller + 0x603,f.extension());
	for (u32 at : {f.caller + 0x604, f.target + 0x700}) { program.write_word(at, 0x880f | (f.destination << 8)); program.write_word(at + 1, 0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.flags & 4 ? 2 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, acc ? 0xa020 : 0xa008); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (acc ? 0x800 | (f.flags & 8 ? 0x100 : 0) : 0x900) | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2468);
	if (long_acc) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b ^ 0xffffffffffULL); }
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); data.write_word(0x3000, 0x700); data.write_word(f.source, f.memory);
	if (long_acc) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address_ar,0x5000);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_multiply_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x3000) { expect(f.kind == 5 && word == 0x700 && !m_fast_stack_reads++, "RETD owns its separate one-word return frame"); return; } if (at != f.source || word != f.memory || m_pipeline_operand_reads || m_cpu->pipeline_cycles() != f.read()) osd_printf_info("delay_multiply_read: case=%u address=%04x word=%04x cycle=%llu expected=%u\n", m_pipeline_peripheral_case, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles(), f.read()); expect(at == f.source && word == f.memory && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == f.read(), "Delay MPY reads one retained RAM or AR operand"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_multiply_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff || at == 0x2ffe) { unsigned const i = m_pipeline_register_writes++; expect((f.kind == 3 || f.kind == 4) && i < (f.kind == 4 ? 2 : 1) && at == 0x2fff - i && word == (i ? f.caller >> 16 : 0x604), "Delayed call owns its continuation and optional caller-page frame"); return; } expect(at == 0x0f && word == u16(f.result) && !m_pipeline_store_writes++ && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() >= f.read() + 2, "The selected callee observes the completed multiply once"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.caller + 0x600, f.caller + 0x604, "delay_multiply_literal", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || (f.accumulator_source && !f.extended_accumulator) || (!f.form && !f.extended_accumulator) || at != f.caller + 0x601 + f.delay()) return; expect(word == f.extension() && !m_pipeline_extensions_reads++ && m_cpu->pipeline_cycles() == f.literal_clock(), "Delay multiply fetches its retained address literal once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254540;
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


























tms320c54x_test_state::register_product_fixture tms320c54x_test_state::register_product_case(unsigned variant)
{
	if (variant >= 176640)
	{
		// Separate domains: POLY excludes SMUL; immediate/absolute products
		// include extension ownership; the final bank isolates EXP producers.
		unsigned v = variant - 176640;
		bool const poly = v < 2304;
		bool const immediate = !poly && v < 29952;
		bool const extended = v >= 29952 && v < 94464;
		bool const exponent = v >= 94464;
		v -= poly ? 0 : immediate ? 2304 : extended ? 29952 : 94464;
		unsigned const operation = v % (poly ? 1 : immediate ? 6 : extended ? 14 : 46); v /= poly ? 1 : immediate ? 6 : extended ? 14 : 46;
		unsigned const pattern = v % 8; v /= 8;
		unsigned flags = v % (poly ? 8 : 16); v /= poly ? 8 : 16;
		if (poly) flags = (flags & 3) | ((flags & 4) << 1); // POLY SMUL-set arithmetic is not validated.
		unsigned const mode = exponent ? 5 : v % 6; if (!exponent) v /= 6;
		unsigned const page = v % 2; v /= 2;
		auto f = register_product_case((exponent && operation < 18 ? operation : 0) + 18 * (pattern + 8 * (flags + 16 * ((mode == 5 ? 0 : mode) + 5 * (page + 2 * v)))));
		f.mode = mode; f.poly = poly; f.immediate = immediate;
		f.length = immediate || extended ? 2 : 1;
		f.checkpoint = std::array<unsigned,3>{4,6+f.length,7+f.length}[v];
		f.smem = extended || (exponent && operation >= 18);
		f.mapped_t = exponent && operation >= 32;
		if (poly) f.opcode = 0x3681;
		else if (immediate) f.opcode = std::array<u16,6>{0xf066,0xf166,0xf067,0xf167,0xf267,0xf367}[operation];
		else if (f.smem) f.opcode = std::array<u16,14>{0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x2d00,0x2e00,0x2f00}[extended ? operation : (operation-18)%14] | (extended ? 0xf8 : f.mapped_t ? 0x0e : 0x81);
		if (f.mapped_t) f.st0 &= ~0x1ff;
		return f;
	}
	if (variant >= 69120)
	{
		unsigned v = variant - 69120, operation = v % 14; v /= 14;
		unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
		unsigned const mode = v % 5; v /= 5; unsigned const page = v % 2; v /= 2;
		unsigned const snapshot = v % 3; v /= 3;
		auto f = register_product_case(18 * (pattern + 8 * (flags + 16 * (mode + 5 * (page + 2 * snapshot)))));
		f.smem = true; f.mapped_t = v;
		f.opcode = std::array<u16,14>{0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x2d00,0x2e00,0x2f00}[operation] | (f.mapped_t ? 0x0e : 0x81);
		if (f.mapped_t) f.st0 &= ~0x1ff;
		return f;
	}
	unsigned v = variant, operation = v % 18; v /= 18;
	unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
	unsigned const mode = v % 5; v /= 5; u32 const base = v % 2 ? 0x20000 : 0; v /= 2;
	u16 const opcode = operation < 2 ? 0xf48c | (operation << 8) : 0xf488 | ((operation-2)&3) | (((operation-2)&4)<<6) | (((operation-2)&8)<<6);
	return {opcode, std::array<u16,8>{0,1,0x4567,0x8000,0xffff,0x7fff,0x8001,0x1234}[pattern],
		u16(0x1804 | (pattern&1 ? 0x600 : 0)), u16(0x800 | (flags&1 ? 0x40 : 0) | (flags&2 ? 0x200 : 0) | (flags&8 ? 0x100 : 0)), u16(flags&4 ? 2 : 0),
		std::array<u64,8>{0,0x12340000,0xff87651111,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern],
		std::array<u64,8>{0x1234,0x7fff8000,0x3200000,0xff80000000,0x7fffffff,0xffffffffff,0x80aa001234,0x7fffffffff}[pattern],base,mode,std::array<unsigned,3>{4,7,8}[v]};
}

u16 tms320c54x_test_state::product_input_t(register_product_fixture const &f)
{
	if (f.mode == 1 || f.mode == 2) return u16(f.b);
	if (f.mode == 5) return exponent_fixture{f.a,false,false,0,f.st0,f.st1,f.base}.exponent();
	return f.t;
}

std::array<u64,3> tms320c54x_test_state::register_product_reference(register_product_fixture const &f)
{
	u64 const a = f.mode == 3 ? 1 : f.a;
	u16 const t = product_input_t(f);
	s64 high = (a >> 16) & 0x1ffff; if (high & 0x10000) high -= 0x20000;
	unsigned const family = (f.opcode >> 8) & 0x0e;
	u16 const memory = f.mapped_t ? t : 0x8001;
	s64 product = (f.immediate ? s64(s16(t)) * s16(memory) : f.smem ? family == 4 ? s64(t) * memory : s64(s16(t)) * s16(memory) : high * s16(t)) * (BIT(f.st1,6) ? 2 : 1);
	bool const multiply = f.poly ? false : f.immediate ? (f.opcode & 0xfeff) == 0xf066 : f.smem ? family <= 4 : (f.opcode & 0xfeff) == 0xf48c;
	if (!multiply && (f.pmst&2) && (f.st1&0x240)==0x240) product = std::clamp(product,s64(-0x80000000LL),s64(0x7fffffffLL));
	u64 const source = f.poly || BIT(f.opcode,f.smem ? 8 : 9) ? f.b : a;
	s64 signed_source = source; if (source & 0x8000000000ULL) signed_source -= 0x10000000000LL;
	s64 result = multiply ? product : signed_source + ((!f.poly && !f.immediate && (f.smem ? family >= 12 : BIT(f.opcode,1))) ? -product : product);
	if (f.poly || (!f.immediate && (f.smem ? family == 2 || family == 10 || family == 14 : !multiply && BIT(f.opcode,0)))) result = ((result+0x8000)>>16)*0x10000;
	u16 st0 = f.st0;
	if (result < -0x80000000LL || result > 0x7fffffffLL)
	{
		st0 |= BIT(f.opcode,8) ? 0x200 : 0x400;
		if (BIT(f.st1,9)) result = result < 0 ? -0x80000000LL : 0x7fffffffLL;
	}
	u64 const stored = u64(result) & 0xffffffffffULL;
	if (f.poly) return {stored,u64((BIT(f.st1,8) ? s64(s16(memory)) : s64(memory))*0x10000)&0xffffffffffULL,st0};
	return {BIT(f.opcode,8) ? a : stored, BIT(f.opcode,8) ? stored : f.b, st0};
}

void tms320c54x_test_state::start_register_product(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto const f = register_product_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i=0;i<16;++i) program.write_word(f.base+0x600+i,0xf495);
	if (f.mode == 1 || f.mode == 2) program.write_word(f.base+0x600+(f.mode==1),0x890e);
	if (f.mode == 3) program.write_word(f.base+0x601,0xe801);
	if (f.mode == 5) program.write_word(f.base+0x601,0xf48e);
	program.write_word(f.base+0x602,f.opcode);
	if (f.length == 2) program.write_word(f.base+0x603,f.immediate ? 0x8001 : 0x1800);
	program.write_word(f.base+0x602+f.length,0xf4e1);
	if (f.mode == 4) { program.write_word(f.base+0x602+f.length,0x770e); program.write_word(f.base+0x603+f.length,0xbeef); program.write_word(f.base+0x604+f.length,0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T,f.t);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1,0x1800); data.write_word(0x1800,0x8001);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_input_line(0,CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI,CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"product_source",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		u16 const t = product_input_t(f);
		expect((f.smem || f.poly) && !m_pipeline_operand_reads && at==(f.mapped_t ? 0x0e : 0x1800) && word==(f.mapped_t ? t : 0x8001) && m_cpu->pipeline_cycles()==6+f.length,"Product reads its RAM or zero-latency mapped T source once in Read"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"register_product_t_producer",[this,f](offs_t at,u16 &word,u16)
	{ if (machine().side_effects_disabled()) return; expect((f.mode==1 || f.mode==2 || f.mode==4) && !m_pipeline_store_writes && at==0x0e && word==(f.mode==4 ? 0xbeef : u16(f.b)),"Only the fixture's STLM or following STM may write T"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base+0x602,f.base+0x601+f.length,"product_fetch",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if (at==f.base+0x602) { expect(!m_pipeline_extensions_reads && word==f.opcode,"Product owns one opcode fetch"); ++m_pipeline_extensions_reads; }
		else { expect(!m_pipeline_register_writes && word==(f.immediate ? 0x8001 : 0x1800),"Product owns one retained immediate or source-address extension"); ++m_pipeline_register_writes; }
	});
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=400000;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}









































bool tms320c54x_test_state::check_register_product_results()
{
	if (m_phase == 400000)
	{
		unsigned const variant=m_pipeline_peripheral_case, tick=++m_idle_fixture_tick; auto const f=register_product_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"T-input products are admitted by the staged frontend");
		if (tick==6+f.length)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_T,0xdead);
			auto const disable=machine().disable_side_effects();
			if ((f.smem || f.poly) && !f.mapped_t) m_cpu->space(AS_DATA).write_word(0x1800,0xdead);
			if (f.length==2) m_cpu->space(AS_PROGRAM).write_word(f.base+0x603,0xdead);
		}
		if (tick==f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads,m_pipeline_register_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save T-input products before/after operand capture and Execute");
		}
		if (tick==24)
		{
			auto const result=register_product_reference(f);
			if (m_cpu->state_int(tms320c54x_device::STATE_A)!=result[0] || m_cpu->state_int(tms320c54x_device::STATE_B)!=result[1] || m_cpu->state_int(tms320c54x_device::STATE_ST0)!=result[2])
				osd_printf_info("product_dependency_failure: case=%u opcode=%04x layout=%u replay=%u a=%010llx b=%010llx expected_a=%010llx expected_b=%010llx\n",variant,f.opcode,f.mode,unsigned(m_pipeline_replay),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),(unsigned long long)result[0],(unsigned long long)result[1]);
			expect_opcode(f.opcode,m_cpu->state_int(tms320c54x_device::STATE_A)==result[0] && m_cpu->state_int(tms320c54x_device::STATE_B)==result[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0)==result[2],"T-input products use retained operands and Execute accumulator sources with correct arithmetic and flags");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T)==(f.mode==4 ? 0xbeef : 0xdead) && m_cpu->state_int(tms320c54x_device::STATE_ST1)==f.st1,"T-input products preserve later T and ST1");
			expect(m_pipeline_operand_reads==unsigned(f.smem || f.poly) && m_pipeline_store_writes==unsigned(f.mode==1 || f.mode==2 || f.mode==4) && m_pipeline_extensions_reads==1 && m_pipeline_register_writes==f.length-1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),"Products own only their source, extension and producer transactions and reach the following IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay=true; m_idle_fixture_tick=f.checkpoint; m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_store_writes=m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads=m_recurring_irq_snapshot[2]; m_pipeline_register_writes=m_recurring_irq_snapshot[3];
				return restore_pipeline_at(400010, "Replay T-input product operand ownership");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant<69119) { start_register_product(variant+1); return true; }
			if (variant>=176640)
			{
				if (variant<306431) { start_register_product(variant+1); return true; }
				osd_printf_info("TMS320C54x staged remaining T dependencies: PASS cases=129792 restore=129792 POLY=2304 immediate=27648 absolute_Smem=64512 EXP_products=35328 patterns=8 layouts=6 pages=2 snapshots=3 poly_smul=excluded source_reads=0,1 literal_fetches=0,1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
				throw emu_fatalerror(0,"TMS320C54x isolated MAC/MAS complete");
			}
			if (variant>=69120)
			{
				if (variant<176639) { start_register_product(variant+1); return true; }
				osd_printf_info("TMS320C54x staged Smem T dependencies: PASS cases=107520 restore=107520 opcodes=14 patterns=8 flags=16 layouts=5 sources=RAM,mapped_T pages=2 snapshots=3 source_reads=1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
				start_register_product(176640); return true;
			}
			osd_printf_info("TMS320C54x staged accumulator T products: PASS cases=69120 restore=69120 opcodes=18 patterns=8 flags=16 layouts=none,STLM_zero_gap,STLM_one_gap,A_producer,T_successor pages=2 snapshots=3 t_sample=Read accumulator_sample=Execute overflow_initial=clear,sticky data_reads=0 producer_writes=0,1 opcode_fetches=1 repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			start_register_product(69120); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase==400010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=400000; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}




bool tms320c54x_test_state::check_bd_long_accumulator_results()
{
	if (m_phase == 254540)
	{
		bool const mac = profile_is("mac-delay");
		bool const square = profile_is("square-delay");
		bool const memory_square = profile_is("squr-delay");
		bool const long_acc = profile_is("bd-maca-long");
		bool const acc = square || memory_square || long_acc || profile_is("maca-delay");
		bool const rounded = profile_is("mpyr-delay");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = long_acc ? bd_long_accumulator_case(variant) : memory_square ? delay_memory_square_case(variant) : square ? delay_square_case(variant) : acc ? delay_accumulator_multiply_case(variant) : delay_multiply_case(variant, mac, rounded);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "A complete memory MPY fits its control-delay words");
		if (acc)
		{
			u64 const mask = long_acc && tick <= f.read() ? 0xffffffffffULL : 0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (!f.destination && tick > f.read() ? f.result : f.a ^ mask) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination && tick > f.read() ? f.result : f.b ^ mask), "Delayed multiply publishes only its selected accumulator in Execute");
			if (tick == f.read() - 1) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.address_ar) == (!long_acc && f.form == 2 ? 0x5001 : 0x5000), "Delayed accumulator multiply owns one pointer update before poisoning");
		}
		if (((f.form && !acc) || long_acc) && tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.caller + 0x601 + f.delay(), 0x5010); }
		if (tick == f.read() - 1) { if (rounded) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.form == 0 ? 0x5001 : 0x5000), "Delayed MPYR owns its postincrement before pointer poisoning"); m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address_ar, 0x6000); }
		if (tick == f.read()) { expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (acc ? f.memory : f.t), "Multiply owns its T publication or retained T source at Read"); auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		if (long_acc && tick == f.read()) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save delayed multiply Decode, Access and retained Read operands"); }
		if (tick == 24)
		{
			u32 const continuation = f.kind == 2 ? f.caller + 0x606 : f.target + 0x702;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == continuation && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == (f.kind == 4 ? 2U : unsigned(f.kind == 3)) && m_fast_stack_reads == unsigned(f.kind == 5) && m_pipeline_extensions_reads == unsigned(long_acc || (f.form != 0 && !acc)) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination ? f.a : f.result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((acc ? 0xa020 : 0xa008) | (f.overflow() ? f.destination ? 0x200 : 0x400 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((acc ? 0x800 | (f.flags & 8 ? 0x100 : 0) : 0x900) | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.kind == 4 ? 0x2ffe : f.kind == 3 ? 0x2fff : f.kind == 5 ? 0x3001 : 0x3000), "Delayed MPY preserves independent signed arithmetic, status, frames and target page through replay");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4]; if (memory_square || long_acc) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(254550, "Replay delayed multiply without repeated source reads or callee publications"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (long_acc ? 98303 : memory_square ? 27647 : acc ? 55295 : rounded ? 36863 : mac ? 147455 : 18431)) { start_delay_multiply(variant + 1); return true; }
			if (long_acc) { osd_printf_info("TMS320C54x BD long accumulator multiply: PASS cases=98304 restore=98304 operations=MPYA,MASA,MACA,MACAR forms=positive_offset,negative_offset,absolute address_registers=8 patterns=8 flags=16 pages=2 snapshots=4 extension_reads=1 source_reads=1 accumulators=Execute t_publish=Read target_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD long accumulator multiply complete"); }
			if (memory_square) { osd_printf_info("TMS320C54x delayed memory SQUR: PASS cases=27648 restore=27648 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD destinations=2 forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read newer_t_preserved=1 smul_product_clamp=0 target_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed memory SQUR complete"); }
			if (square) { osd_printf_info("TMS320C54x delayed square accumulation: PASS cases=55296 restore=55296 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=SQURA,SQURS destinations=2 forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read smul_product_clamp=0 target_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed square accumulation complete"); }
			if (rounded) { osd_printf_info("TMS320C54x delayed MPYR: PASS cases=36864 restore=36864 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=postincrement,absolute_RAM,absolute_AR7,offset destinations=2 operand_pairs=16 frct_ovm_smul=8 pages=2 snapshots=3 source_reads=1 rounding=biased_high_word smul_product_clamp=0 t_retained=1 callee_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed MPYR complete"); }
			if (acc) { osd_printf_info("TMS320C54x delayed accumulator multiply: PASS cases=55296 restore=55296 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=MPYA,MASA,MACA,MACAR forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read target_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed accumulator multiply complete"); }
			if (mac) { osd_printf_info("TMS320C54x staged delay MAC/MAS: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=MAC,MACR,MAS,MASR forms=short,absolute_RAM,absolute_AR5,offset destinations=2 operand_pairs=16 frct_ovm_smul=8 pages=2 snapshots=3 source_reads=1 t_retained=1 callee_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delay MAC/MAS complete"); }
			osd_printf_info("TMS320C54x staged delay MPY: PASS cases=18432 restore=18432 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=short,absolute_RAM,absolute_AR,offset destinations=2 operand_pairs=16 frct_ovm=4 pages=2 snapshots=3 source_reads=1 callee_observes_result=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delay MPY complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254550) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254540; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}












} // namespace c54x_test
