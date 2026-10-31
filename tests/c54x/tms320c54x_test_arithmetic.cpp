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




} // namespace c54x_test
