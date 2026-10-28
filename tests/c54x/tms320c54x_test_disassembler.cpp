// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::check_control_disassembler()
{
	using dasm = util::disasm_interface;
	class words : public dasm::data_buffer
	{
	public:
		words(u32 pc, u16 op, u16 next, u16 third = 0x6789) : m_pc(pc), m_words{op, next, third} { }
		virtual u16 r16(u32 pc) const override
		{
			for (unsigned i = 0; i < 3; ++i)
				if (pc == ((m_pc & 0x7f0000) | u16(m_pc + i))) { m_seen |= 1 << i; return m_words[i]; }
			throw emu_fatalerror("C54x disassembler read beyond fixture");
		}
		u8 seen() const { return m_seen; }
		virtual u8 r8(u32 pc) const override { throw emu_fatalerror("unexpected byte disassembly read"); }
		virtual u32 r32(u32 pc) const override { throw emu_fatalerror("unexpected long disassembly read"); }
		virtual u64 r64(u32 pc) const override { throw emu_fatalerror("unexpected quad disassembly read"); }
	private:
		u32 const m_pc;
		u16 const m_words[3];
		mutable u8 m_seen = 0;
	};
	struct vector { u16 op; u32 result; const char *text; };
	vector const vectors[] = {
		{0xf495, 1 | dasm::SUPPORTED, "NOP"},
		{0xf4e1, 1 | dasm::SUPPORTED, "IDLE    1"},
		{0xf6e1, 1 | dasm::SUPPORTED, "IDLE    2"},
		{0xf5e1, 1 | dasm::SUPPORTED, "IDLE    3"},
		{0xf7e1, 1, ".word   $F7E1"},
		{0xf073, 2 | dasm::SUPPORTED, "B       $012345"},
		{0xf074, 2 | dasm::SUPPORTED | dasm::STEP_OVER, "CALL    $012345"},
		{0xf273, 2 | dasm::SUPPORTED, "BD      $012345"},
		{0xf274, 2 | dasm::SUPPORTED, "CALLD   $012345"},
		{0xf803, 2 | dasm::SUPPORTED | dasm::STEP_COND, "BC      $012345, BIO"},
		{0xfa02, 2 | dasm::SUPPORTED | dasm::STEP_COND, "BCD     $012345, NBIO"},
		{0xf80c, 2 | dasm::SUPPORTED | dasm::STEP_COND, "BC      $012345, C"},
		{0xf808, 2 | dasm::SUPPORTED | dasm::STEP_COND, "BC      $012345, NC"},
		{0xfc00, 1 | dasm::SUPPORTED | dasm::STEP_OUT, "RET"},
		{0xf4eb, 1 | dasm::SUPPORTED | dasm::STEP_OUT, "RETE"},
		{0xf49b, 1 | dasm::SUPPORTED | dasm::STEP_OUT, "RETF"},
		{0xf69b, 1 | dasm::SUPPORTED, "RETFD"},
		{0xfe00, 1 | dasm::SUPPORTED, "RETD"},
		{0xf4e2, 1 | dasm::SUPPORTED, "BACC    A"},
		{0xf5e2, 1 | dasm::SUPPORTED, "BACC    B"},
		{0xf4e3, 1 | dasm::SUPPORTED | dasm::STEP_OVER, "CALA    A"},
		{0xf7e3, 1 | dasm::SUPPORTED, "CALAD   B"},
		{0xf072, 2 | dasm::SUPPORTED, "RPTB    $012345"},
		{0xf272, 2 | dasm::SUPPORTED, "RPTBD   $012345"},
		{0xf070, 2 | dasm::SUPPORTED, "RPT     #$2345"},
		{0xf071, 2 | dasm::SUPPORTED, "RPTZ    A, #$2345"},
		{0xf171, 2 | dasm::SUPPORTED, "RPTZ    B, #$2345"},
		{0xecff, 1 | dasm::SUPPORTED, "RPT     #255"},
		{0xf6bb, 1 | dasm::SUPPORTED, "RSBX    1, 11"},
		{0xf5b0, 1 | dasm::SUPPORTED, "SSBX    0, 0"},
		{0xf7bb, 1 | dasm::SUPPORTED, "SSBX    1, 11"},
		{0xf7c1, 1 | dasm::SUPPORTED | dasm::STEP_OVER, "INTR    1"},
		{0xf4cf, 1 | dasm::SUPPORTED | dasm::STEP_OVER, "TRAP    15"},
		{0xf884, 2 | dasm::SUPPORTED, "FB      $042345"},
		{0xf984, 2 | dasm::SUPPORTED | dasm::STEP_OVER, "FCALL   $042345"},
		{0xfa84, 2 | dasm::SUPPORTED, "FBD     $042345"},
		{0xfb84, 2 | dasm::SUPPORTED, "FCALLD  $042345"},
		{0xf4e4, 1 | dasm::SUPPORTED | dasm::STEP_OUT, "FRET"},
		{0xf6e5, 1 | dasm::SUPPORTED, "FRETED"},
		{0xf4e6, 1 | dasm::SUPPORTED, "FBACC   A"},
		{0xf5e6, 1 | dasm::SUPPORTED, "FBACC   B"},
		{0xf6e6, 1 | dasm::SUPPORTED, "FBACCD  A"},
		{0xf7e6, 1 | dasm::SUPPORTED, "FBACCD  B"},
		{0xf4e7, 1 | dasm::SUPPORTED | dasm::STEP_OVER, "FCALA   A"},
		{0xf5e7, 1 | dasm::SUPPORTED | dasm::STEP_OVER, "FCALA   B"},
		{0xf6e7, 1 | dasm::SUPPORTED, "FCALAD  A"},
		{0xf7e7, 1 | dasm::SUPPORTED, "FCALAD  B"},
		{0xffff, 1, ".word   $FFFF"},
	};
	auto &decoder = m_cpu->get_disassembler();
	expect(decoder.opcode_alignment() == 1 && decoder.interface_flags() == dasm::PAGED &&
		decoder.page_address_bits() == 16, "C54x debugger uses word-addressed program pages");
	for (u32 pc : {0x010100, 0x01ffff})
		for (auto const &test : vectors)
		{
			words const buffer(pc, test.op, 0x2345);
			std::ostringstream text;
			u32 const result = decoder.disassemble(text, pc, buffer, buffer);
			expect(result == test.result && text.str() == test.text,
				"control disassembly preserves mnemonic, operand, word length, page and bounded stepping flags");
		}
	osd_printf_info("TMS320C54x control disassembler: PASS vectors=%u pages=2\n",
		unsigned(std::size(vectors)));
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x6800, 0x6900, 0x6a00, 0x6b00, 0x7600, 0x7700})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				u32 const result = decoder.disassemble(text, pc, buffer, buffer);
				bool const valid = (family != 0x7700 || mode < 0xe0) &&
					((family & 0xfc00) != 0x6800 || (mode & 0xf8) != 0x98);
				unsigned const length = valid ? 2 + (family != 0x7700 && mode >= 0xe0) : 1;
				expect(result == (length | (valid ? dasm::SUPPORTED : 0)) && buffer.seen() == (1U << length) - 1,
					"Literal disassembly bounds extension reads, wraps within XPC and rejects invalid MMR modes");
			}
	vector const literals[] = {
		{0x7711, 2 | dasm::SUPPORTED, "STM     #$2345, $11"},
		{0x7781, 2 | dasm::SUPPORTED, "STM     #$2345, *AR1"},
		{0x7681, 2 | dasm::SUPPORTED, "ST      #$2345, *AR1"},
		{0x76f8, 3 | dasm::SUPPORTED, "ST      #$6789, *($2345)"},
		{0x76e1, 3 | dasm::SUPPORTED, "ST      #$6789, *AR1($2345)"},
		{0x68f8, 3 | dasm::SUPPORTED, "ANDM    #$6789, *($2345)"},
		{0x68e1, 3 | dasm::SUPPORTED, "ANDM    #$2345, *AR1($6789)"},
		{0x69e9, 3 | dasm::SUPPORTED, "ORM     #$2345, *+AR1($6789)"},
		{0x69f1, 3 | dasm::SUPPORTED, "ORM     #$2345, *+AR1($6789)%"},
		{0x6891, 2 | dasm::SUPPORTED, "ANDM    #$2345, *AR1+"},
		{0x69d9, 2 | dasm::SUPPORTED, "ORM     #$2345, *AR1+0%"},
		{0x6af8, 3 | dasm::SUPPORTED, "XORM    #$6789, *($2345)"},
		{0x6aeb, 3 | dasm::SUPPORTED, "XORM    #$2345, *+AR3($6789)"},
		{0x6a93, 2 | dasm::SUPPORTED, "XORM    #$2345, *AR3+"},
		{0x689a, 1, ".word   $689A"},
		{0x699a, 1, ".word   $699A"},
		{0x6a9a, 1, ".word   $6A9A"},
		{0x6b9a, 1, ".word   $6B9A"},
		{0x77e1, 1, ".word   $77E1"},
	};
	for (auto const &test : literals)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Literal disassembly preserves asymmetric immediate/offset ordering and indirect modifiers");
	}
	osd_printf_info("TMS320C54x literal disassembler: PASS decoded_modes=1472 pages=2 invalid_mmr=32 invalid_rmw=32 bounded_extensions=1 operand_order=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x6000, 0x6100, 0x7000, 0x7100, 0x7400, 0x7500, 0x7d00, 0x7e00})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = (family == 0x7e00 ? 1 : 2) + (mode >= 0xe0);
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) &&
					buffer.seen() == (1U << length) - 1, "Move/test decoding bounds instruction extensions within XPC");
			}
	vector const moves[] = {
		{0x6000, 2 | dasm::SUPPORTED, "CMPM    $00, #$2345"},
		{0x61e2, 3 | dasm::SUPPORTED, "BITF    *AR2($2345), #$6789"},
		{0x70f8, 3 | dasm::SUPPORTED, "MVKD    $6789, *($2345)"},
		{0x7192, 2 | dasm::SUPPORTED, "MVDK    *AR2+, $2345"},
		{0x74e2, 3 | dasm::SUPPORTED, "PORTR   $2345, *AR2($6789)"},
		{0x75f8, 3 | dasm::SUPPORTED, "PORTW   *($2345), $6789"},
		{0x7de2, 3 | dasm::SUPPORTED, "MVDP    *AR2($6789), $2345"},
		{0x7e92, 1 | dasm::SUPPORTED, "READA   *AR2+"},
		{0x7ef8, 2 | dasm::SUPPORTED, "READA   *($2345)"},
	};
	for (auto const &test : moves)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Move/test decoding retains family-specific operand ordering and modifiers");
	}
	osd_printf_info("TMS320C54x move/test disassembler: PASS decoded_modes=2048 pages=2 bounded_extensions=1 operand_order=1 pipeline_claim=0\n");
	static constexpr u8 predicates[] = {0x02,0x03,0x08,0x0c,0x20,0x30,0x42,0x43,0x44,0x45,0x46,0x47,
		0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x60,0x68,0x70,0x78};
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xf800,0xfa00,0xf900,0xfb00,0xfd00,0xff00})
			for (u8 predicate : predicates)
			{
				words const buffer(pc, family | predicate, 0x2345);
				std::ostringstream text;
				bool const xc = family == 0xfd00 || family == 0xff00;
				u32 const flags = dasm::SUPPORTED | dasm::STEP_COND | (family == 0xf900 ? dasm::STEP_OVER : 0);
				expect(decoder.disassemble(text, pc, buffer, buffer) == ((xc ? 1 : 2) | flags) &&
					buffer.seen() == (xc ? 1 : 3), "Conditional decoder preserves length, page and bounded stepping flags");
			}
	vector const conditional[] = {
		{0xff0c, 1 | dasm::SUPPORTED | dasm::STEP_COND, "XC      2, C"},
		{0xfd68, 1 | dasm::SUPPORTED | dasm::STEP_COND, "XC      1, BNOV"},
		{0xfa45, 2 | dasm::SUPPORTED | dasm::STEP_COND, "BCD     $012345, AEQ"},
		{0xf94d, 2 | dasm::SUPPORTED | dasm::STEP_COND | dasm::STEP_OVER, "CC      $012345, BEQ"},
		{0xfb70, 2 | dasm::SUPPORTED | dasm::STEP_COND, "CCD     $012345, AOV"},
		{0xfd80, 1, ".word   $FD80"},
		{0xff10, 1, ".word   $FF10"},
	};
	for (auto const &test : conditional)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Named conditional predicates decode exactly; unreviewed forms stay unsupported");
	}
	osd_printf_info("TMS320C54x conditional disassembler: PASS forms=132 pages=2 named_predicates=22 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xfc00, 0xfe00})
			for (u8 predicate : predicates)
			{
				words const buffer(pc, family | predicate, 0x2345);
				std::ostringstream text;
				u32 const flags = dasm::SUPPORTED | dasm::STEP_COND | (family == 0xfc00 ? dasm::STEP_OUT : 0);
				expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | flags) && buffer.seen() == 1,
					"Conditional return decoder uses one word and conservative delayed stepping");
			}
	vector const conditional_returns[] = {
		{0xfc20, 1 | dasm::SUPPORTED | dasm::STEP_COND | dasm::STEP_OUT, "RC      NTC"},
		{0xfc45, 1 | dasm::SUPPORTED | dasm::STEP_COND | dasm::STEP_OUT, "RC      AEQ"},
		{0xfe44, 1 | dasm::SUPPORTED | dasm::STEP_COND, "RCD     ANEQ"},
		{0xfe78, 1 | dasm::SUPPORTED | dasm::STEP_COND, "RCD     BOV"},
		{0xfc10, 1, ".word   $FC10"},
		{0xfe80, 1, ".word   $FE80"},
	};
	for (auto const &test : conditional_returns)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == 1,
			"Conditional return syntax names reviewed predicates and leaves unknown combinations raw");
	}
	osd_printf_info("TMS320C54x conditional return disassembler: PASS forms=44 pages=2 named_predicates=22 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x1000, 0x1100, 0x1400, 0x1500, 0x4400, 0x4500})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == ((mode >= 0xe0 ? 2 : 1) | dasm::SUPPORTED) &&
					buffer.seen() == (mode >= 0xe0 ? 3 : 1), "Load decoder reads only its address extension within the program page");
			}
	vector const loads[] = {
		{0x103a, 1 | dasm::SUPPORTED, "LD      $3A, A"},
		{0x1182, 1 | dasm::SUPPORTED, "LD      *AR2, B"},
		{0x1095, 1 | dasm::SUPPORTED, "LD      *AR5+, A"},
		{0x11ed, 2 | dasm::SUPPORTED, "LD      *+AR5($2345), B"},
		{0x10f5, 2 | dasm::SUPPORTED, "LD      *+AR5($2345)%, A"},
		{0x10f8, 2 | dasm::SUPPORTED, "LD      *($2345), A"},
		{0x1482, 1 | dasm::SUPPORTED, "LD      *AR2, TS, A"},
		{0x15f8, 2 | dasm::SUPPORTED, "LD      *($2345), TS, B"},
		{0x44ed, 2 | dasm::SUPPORTED, "LD      *+AR5($2345), 16, A"},
		{0x4593, 1 | dasm::SUPPORTED, "LD      *AR3+, 16, B"},
	};
	for (auto const &test : loads)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Load decoder retains address modifiers and accumulator destination");
	}
	osd_printf_info("TMS320C54x load disassembler: PASS modes=1536 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned family = 0x18; family <= 0x1d; ++family)
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, (family << 8) | mode, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == ((mode >= 0xe0 ? 2 : 1) | dasm::SUPPORTED) &&
					buffer.seen() == (mode >= 0xe0 ? 3 : 1), "Memory logical decoder bounds address extensions");
			}
	vector const memory_logical[] = {
		{0x1882, 1 | dasm::SUPPORTED, "AND     *AR2, A"},
		{0x19f8, 2 | dasm::SUPPORTED, "AND     *($2345), B"},
		{0x1aed, 2 | dasm::SUPPORTED, "OR      *+AR5($2345), A"},
		{0x1b3a, 1 | dasm::SUPPORTED, "OR      $3A, B"},
		{0x1c95, 1 | dasm::SUPPORTED, "XOR     *AR5+, A"},
		{0x1df5, 2 | dasm::SUPPORTED, "XOR     *+AR5($2345)%, B"},
	};
	for (auto const &test : memory_logical)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Memory logical syntax preserves family and accumulator destination");
	}
	osd_printf_info("TMS320C54x memory logical disassembler: PASS modes=1536 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned family = 0; family < 16; ++family)
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, (family << 8) | mode, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == ((mode >= 0xe0 ? 2 : 1) | dasm::SUPPORTED) &&
					buffer.seen() == (mode >= 0xe0 ? 3 : 1), "Memory ALU decoder preserves length and bounded page-wrapped extension reads");
			}
	vector const memory_alu[] = {
		{0x0082, 1 | dasm::SUPPORTED, "ADD     *AR2, A"},
		{0x033a, 1 | dasm::SUPPORTED, "ADDS    $3A, B"},
		{0x04f8, 2 | dasm::SUPPORTED, "ADD     *($2345), TS, A"},
		{0x0795, 1 | dasm::SUPPORTED, "ADDC    *AR5+, B"},
		{0x0881, 1 | dasm::SUPPORTED, "SUB     *AR1, A"},
		{0x0aed, 2 | dasm::SUPPORTED, "SUBS    *+AR5($2345), A"},
		{0x0df5, 2 | dasm::SUPPORTED, "SUB     *+AR5($2345)%, TS, B"},
		{0x0ef8, 2 | dasm::SUPPORTED, "SUBB    *($2345), A"},
	};
	for (auto const &test : memory_alu)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Memory ALU decoding distinguishes unsigned, TS and carry/borrow families");
	}
	osd_printf_info("TMS320C54x memory ALU disassembler: PASS modes=4096 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xf460, 0xf440, 0xf0c0})
			for (unsigned accumulators = 0; accumulators < 4; ++accumulators)
				for (unsigned shift = 0; shift < 32; ++shift)
				{
					words const buffer(pc, family | (accumulators << 8) | shift, 0x2345);
					std::ostringstream text;
					expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
						"Accumulator shift decoder has no extension reads");
				}
	vector const accumulator_forms[] = {
		{0xf460, 1 | dasm::SUPPORTED, "SFTA    A, 0, A"},
		{0xf57d, 1 | dasm::SUPPORTED, "SFTA    A, -3, B"},
		{0xf74f, 1 | dasm::SUPPORTED, "LD      B, 15, B"},
		{0xf650, 1 | dasm::SUPPORTED, "LD      B, -16, A"},
		{0xf1df, 1 | dasm::SUPPORTED, "XOR     A, -1, B"},
		{0xf491, 1 | dasm::SUPPORTED, "ROL     A"},
		{0xf591, 1 | dasm::SUPPORTED, "ROL     B"},
		{0xf490, 1 | dasm::SUPPORTED, "ROR     A"},
		{0xf590, 1 | dasm::SUPPORTED, "ROR     B"},
		{0xf492, 1 | dasm::SUPPORTED, "ROLTC   A"},
		{0xf592, 1 | dasm::SUPPORTED, "ROLTC   B"},
	};
	for (u32 pc : {0x010100, 0x01ffff})
		for (auto const &test : accumulator_forms)
		{
			words const buffer(pc, test.op, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == 1,
				"Accumulator decoding retains signed shift and rotate source");
		}
	osd_printf_info("TMS320C54x accumulator disassembler: PASS shift_forms=384 rotate_forms=6 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xf000, 0xf010, 0xf030, 0xf040, 0xf050})
			for (unsigned accumulators = 0; accumulators < 4; ++accumulators)
				for (unsigned shift = 0; shift <= 16; ++shift)
				{
					u16 const high = 0xf060 | ((family >> 4) & 15);
					u16 const op = (shift == 16 ? high : family | shift) | (accumulators << 8);
					words const buffer(pc, op, 0x2345);
					std::ostringstream text;
					expect(decoder.disassemble(text, pc, buffer, buffer) == (2 | dasm::SUPPORTED) && buffer.seen() == 3,
						"Immediate arithmetic decoder reads one page-wrapped literal");
				}
	vector const immediate_alu[] = {
		{0xf000, 2 | dasm::SUPPORTED, "ADD     #$2345, 0, A, A"},
		{0xf10f, 2 | dasm::SUPPORTED, "ADD     #$2345, 15, A, B"},
		{0xf21f, 2 | dasm::SUPPORTED, "SUB     #$2345, 15, B, A"},
		{0xf350, 2 | dasm::SUPPORTED, "XOR     #$2345, 0, B, B"},
		{0xf360, 2 | dasm::SUPPORTED, "ADD     #$2345, 16, B, B"},
		{0xf161, 2 | dasm::SUPPORTED, "SUB     #$2345, 16, A, B"},
		{0xf065, 2 | dasm::SUPPORTED, "XOR     #$2345, 16, A, A"},
		{0xf030, 2 | dasm::SUPPORTED, "AND     #$2345, 0, A, A"},
		{0xf13f, 2 | dasm::SUPPORTED, "AND     #$2345, 15, A, B"},
		{0xf263, 2 | dasm::SUPPORTED, "AND     #$2345, 16, B, A"},
		{0xf040, 2 | dasm::SUPPORTED, "OR      #$2345, 0, A, A"},
		{0xf34f, 2 | dasm::SUPPORTED, "OR      #$2345, 15, B, B"},
		{0xf164, 2 | dasm::SUPPORTED, "OR      #$2345, 16, A, B"},
		{0xf220, 1, ".word   $F220"},
	};
	for (auto const &test : immediate_alu)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Immediate decoder retains shift and independent accumulators without claiming other families");
	}
	osd_printf_info("TMS320C54x immediate ALU disassembler: PASS forms=340 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned word = 0; word < 4608; ++word)
		{
			u16 const op = word < 512 ? 0xa400 + word : 0xb000 + word - 512;
			words const buffer(pc, op, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
				"Dual-memory multiply decoder does not read an extension");
		}
	vector const dual_multiply[] = {
		{0xa43a, 1 | dasm::SUPPORTED, "MPY     *AR5, *AR4+, A"},
		{0xa5be, 1 | dasm::SUPPORTED, "MPY     *AR5+, *AR4+0%, B"},
		{0xb03a, 1 | dasm::SUPPORTED, "MAC     *AR5, *AR4+, A, A"},
		{0xb336, 1 | dasm::SUPPORTED, "MAC     *AR5, *AR4-, B, B"},
		{0xb736, 1 | dasm::SUPPORTED, "MACR    *AR5, *AR4-, B, B"},
		{0xbabe, 1 | dasm::SUPPORTED, "MAS     *AR5+, *AR4+0%, B, A"},
		{0xbd00, 1 | dasm::SUPPORTED, "MASR    *AR2, *AR2, A, B"},
	};
	for (auto const &test : dual_multiply)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == 1,
			"Dual-memory multiply decoding retains modifiers, rounding and accumulator selectors");
	}
	osd_printf_info("TMS320C54x dual multiply disassembler: PASS forms=4608 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned mode = 0; mode < 256; ++mode)
		{
			words const buffer(pc, 0x6d00 | mode, 0x2345);
			std::ostringstream text;
			u32 const result = mode < 0x80 ? 1 : (mode >= 0xe0 ? 2 : 1) | dasm::SUPPORTED;
			expect(decoder.disassemble(text, pc, buffer, buffer) == result && buffer.seen() == (mode >= 0xe0 ? 3 : 1),
				"MAR decoder claims only indirect layouts and bounds their extensions");
		}
	vector const mar_forms[] = {
		{0x6d90, 1 | dasm::SUPPORTED, "MAR     *AR0+"},
		{0x6ddc, 1 | dasm::SUPPORTED, "MAR     *AR4+0%"},
		{0x6ded, 2 | dasm::SUPPORTED, "MAR     *+AR5($2345)"},
		{0x6df5, 2 | dasm::SUPPORTED, "MAR     *+AR5($2345)%"},
		{0x6df8, 2 | dasm::SUPPORTED, "MAR     *($2345)"},
		{0x6d10, 1, ".word   $6D10"},
		{0x6d60, 1, ".word   $6D60"},
	};
	for (auto const &test : mar_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"MAR syntax preserves circular and long-offset modes without treating direct forms as indirect");
	}
	osd_printf_info("TMS320C54x MAR disassembler: PASS indirect=128 direct_unclaimed=128 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned family = 0x3c; family <= 0x43; ++family)
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, (family << 8) | mode, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == ((mode >= 0xe0 ? 2 : 1) | dasm::SUPPORTED) &&
					buffer.seen() == (mode >= 0xe0 ? 3 : 1), "Shift-16 memory ALU decoder bounds address extensions");
			}
	vector const high_alu[] = {
		{0x3c3a, 1 | dasm::SUPPORTED, "ADD     $3A, 16, A, A"},
		{0x3ded, 2 | dasm::SUPPORTED, "ADD     *+AR5($2345), 16, A, B"},
		{0x3ef8, 2 | dasm::SUPPORTED, "ADD     *($2345), 16, B, A"},
		{0x3f81, 1 | dasm::SUPPORTED, "ADD     *AR1, 16, B, B"},
		{0x4092, 1 | dasm::SUPPORTED, "SUB     *AR2+, 16, A, A"},
		{0x41f5, 2 | dasm::SUPPORTED, "SUB     *+AR5($2345)%, 16, A, B"},
		{0x4283, 1 | dasm::SUPPORTED, "SUB     *AR3, 16, B, A"},
		{0x43f8, 2 | dasm::SUPPORTED, "SUB     *($2345), 16, B, B"},
	};
	for (auto const &test : high_alu)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Shift-16 memory ALU syntax preserves independent accumulator selectors");
	}
	osd_printf_info("TMS320C54x high memory ALU disassembler: PASS modes=2048 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x4e00, 0x4f00, 0x5600, 0x5700})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == ((mode >= 0xe0 ? 2 : 1) | dasm::SUPPORTED) &&
					buffer.seen() == (mode >= 0xe0 ? 3 : 1), "Double load/store decoder bounds address extensions");
			}
	vector const double_memory[] = {
		{0x4e3a, 1 | dasm::SUPPORTED, "DST     A, $3A"},
		{0x4f81, 1 | dasm::SUPPORTED, "DST     B, *AR1"},
		{0x4eed, 2 | dasm::SUPPORTED, "DST     A, *+AR5($2345)"},
		{0x4ff8, 2 | dasm::SUPPORTED, "DST     B, *($2345)"},
		{0x5681, 1 | dasm::SUPPORTED, "DLD     *AR1, A"},
		{0x5793, 1 | dasm::SUPPORTED, "DLD     *AR3+, B"},
		{0x56f5, 2 | dasm::SUPPORTED, "DLD     *+AR5($2345)%, A"},
		{0x57f8, 2 | dasm::SUPPORTED, "DLD     *($2345), B"},
	};
	for (auto const &test : double_memory)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Double load/store syntax preserves operand order and address modifiers");
	}
	osd_printf_info("TMS320C54x double memory disassembler: PASS modes=1024 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x6c00, 0x6e00})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				bool const supported = mode >= 0x80 && mode < 0xf8;
				bool const extended = supported && mode >= 0xe0;
				u32 const result = supported ? (2 + extended) | dasm::SUPPORTED | dasm::STEP_COND : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == result && buffer.seen() ==
					(supported ? (extended ? 7 : 3) : 1), "BANZ decoder bounds target and displacement without claiming direct/absolute forms");
			}
	vector const banz_forms[] = {
		{0x6c8a, 2 | dasm::SUPPORTED | dasm::STEP_COND, "BANZ    $012345, *AR2-"},
		{0x6e93, 2 | dasm::SUPPORTED | dasm::STEP_COND, "BANZD   $012345, *AR3+"},
		{0x6ce3, 3 | dasm::SUPPORTED | dasm::STEP_COND, "BANZ    $016789, *AR3($2345)"},
		{0x6eeb, 3 | dasm::SUPPORTED | dasm::STEP_COND, "BANZD   $016789, *+AR3($2345)"},
		{0x6cf3, 3 | dasm::SUPPORTED | dasm::STEP_COND, "BANZ    $016789, *+AR3($2345)%"},
		{0x6c00, 1, ".word   $6C00"},
		{0x6ef8, 1, ".word   $6EF8"},
	};
	for (auto const &test : banz_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"BANZ syntax retains program page and displacement-before-target ordering");
	}
	osd_printf_info("TMS320C54x BANZ disassembler: PASS indirect=240 unclaimed=272 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x4800, 0x4900, 0x4a00, 0x8800, 0x8900, 0x8a00})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (mode < 0xe0 ? 1 | dasm::SUPPORTED : 1) && buffer.seen() == 1,
					"MMR decoder excludes forbidden extended modes and reads no extensions");
			}
	vector const mmr_forms[] = {
		{0x4810, 1 | dasm::SUPPORTED, "LDM     $10, A"},
		{0x4993, 1 | dasm::SUPPORTED, "LDM     *AR3+, B"},
		{0x4a14, 1 | dasm::SUPPORTED, "PSHM    $14"},
		{0x4a8b, 1 | dasm::SUPPORTED, "PSHM    *AR3-"},
		{0x8810, 1 | dasm::SUPPORTED, "STLM    A, $10"},
		{0x89dc, 1 | dasm::SUPPORTED, "STLM    B, *AR4+0%"},
		{0x8a14, 1 | dasm::SUPPORTED, "POPM    $14"},
		{0x8a93, 1 | dasm::SUPPORTED, "POPM    *AR3+"},
		{0x48e0, 1, ".word   $48E0"},
		{0x8af8, 1, ".word   $8AF8"},
	};
	for (auto const &test : mmr_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == 1,
			"MMR syntax retains register/accumulator ordering and rejects extended forms");
	}
	osd_printf_info("TMS320C54x MMR disassembler: PASS modes=1344 rejected=192 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned family = 0x9c; family <= 0x9f; ++family)
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, (family << 8) | mode, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == ((mode & 7) >= 2 ? 1 | dasm::SUPPORTED : 1) && buffer.seen() == 1,
					"Conditional-store decoder admits twelve predicates without reading an extension");
			}
	vector const conditional_stores[] = {
		{0x9d3d, 1 | dasm::SUPPORTED, "SRCCD   *AR5, BEQ"},
		{0x9c46, 1 | dasm::SUPPORTED, "STRCD   *AR2-, AGT"},
		{0x9cf3, 1 | dasm::SUPPORTED, "STRCD   *AR5+0%, ALT"},
		{0x9e25, 1 | dasm::SUPPORTED, "SACCD   A, *AR4, AEQ"},
		{0x9f9c, 1 | dasm::SUPPORTED, "SACCD   B, *AR3+, BNEQ"},
	};
	for (auto const &test : conditional_stores)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Conditional-store syntax names its source, Xmem modifier and predicate");
	}
	osd_printf_info("TMS320C54x conditional-store disassembler: PASS opcodes=1024 supported=768 rejected=256 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned word = 0; word < 4352; ++word)
		{
			u16 const op = word < 4096 ? 0xd000 + word : 0xe200 + word - 4096;
			words const buffer(pc, op, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
				"Parallel multiply and SQDST decoders read no extension words");
		}
	vector const parallel_forms[] = {
		{0xd6e1, 1 | dasm::SUPPORTED, "ST      B, *AR3 || MACR *AR4+0%, A"},
		{0xd03a, 1 | dasm::SUPPORTED, "ST      A, *AR4+ || MAC *AR5, A"},
		{0xdbae, 1 | dasm::SUPPORTED, "ST      B, *AR4+0% || MAS *AR4+, B"},
		{0xdd00, 1 | dasm::SUPPORTED, "ST      A, *AR2 || MASR *AR2, B"},
		{0xe210, 1 | dasm::SUPPORTED, "SQDST   *AR3, *AR2"},
		{0xe2e4, 1 | dasm::SUPPORTED, "SQDST   *AR4+0%, *AR2-"},
		{0xf493, 1 | dasm::SUPPORTED, "CMPL    A, A"},
		{0xf593, 1 | dasm::SUPPORTED, "CMPL    A, B"},
		{0xf693, 1 | dasm::SUPPORTED, "CMPL    B, A"},
		{0xf793, 1 | dasm::SUPPORTED, "CMPL    B, B"},
	};
	for (u32 pc : {0x010100, 0x01ffff})
		for (auto const &test : parallel_forms)
		{
			words const buffer(pc, test.op, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == 1,
				"Parallel instruction syntax separates store and arithmetic selectors");
		}
	osd_printf_info("TMS320C54x parallel disassembler: PASS forms=4352 complement=4 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
	{
		for (unsigned word = 0; word < 2048; ++word)
		{
			words const buffer(pc, 0x8000 + word, 0x2345);
			std::ostringstream text;
			bool const extended = (word & 255) >= 0xe0;
			expect(decoder.disassemble(text, pc, buffer, buffer) == ((1 + extended) | dasm::SUPPORTED) &&
				buffer.seen() == (extended ? 3 : 1), "Accumulator store decoder bounds address extensions");
		}
		for (unsigned word = 0; word < 544; ++word)
		{
			words const buffer(pc, word < 512 ? 0xe800 + word : 0xed00 + word - 512, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
				"Short immediate decoder reads no extension words");
		}
	}
	vector const store_short_forms[] = {
		{0x8094, 1 | dasm::SUPPORTED, "STL     A, *AR4+"},
		{0x81f8, 2 | dasm::SUPPORTED, "STL     B, *($2345)"},
		{0x8283, 1 | dasm::SUPPORTED, "STH     A, *AR3"},
		{0x83ed, 2 | dasm::SUPPORTED, "STH     B, *+AR5($2345)"},
		{0x8410, 1 | dasm::SUPPORTED, "STL     A, ASM, $10"},
		{0x8593, 1 | dasm::SUPPORTED, "STL     B, ASM, *AR3+"},
		{0x86f5, 2 | dasm::SUPPORTED, "STH     A, ASM, *+AR5($2345)%"},
		{0x87f8, 2 | dasm::SUPPORTED, "STH     B, ASM, *($2345)"},
		{0xe801, 1 | dasm::SUPPORTED, "LD      #$01, A"},
		{0xe9ff, 1 | dasm::SUPPORTED, "LD      #$FF, B"},
		{0xed00, 1 | dasm::SUPPORTED, "LD      #0, ASM"},
		{0xed0f, 1 | dasm::SUPPORTED, "LD      #15, ASM"},
		{0xed10, 1 | dasm::SUPPORTED, "LD      #-16, ASM"},
		{0xed1f, 1 | dasm::SUPPORTED, "LD      #-1, ASM"},
		{0xed20, 1, ".word   $ED20"},
	};
	for (auto const &test : store_short_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Store/short-immediate syntax retains ASM selection and signed immediate endpoints");
	}
	osd_printf_info("TMS320C54x store/short disassembler: PASS stores=2048 immediates=544 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x7200, 0x7300, 0x4b00, 0x8b00})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				bool const move = family == 0x7200 || family == 0x7300;
				bool const supported = !move || mode < 0xe0;
				unsigned const length = move ? 2 : mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (supported ? length | dasm::SUPPORTED : 1) &&
					buffer.seen() == (supported && length == 2 ? 3 : 1), "MMR move and memory-stack decoders bound extensions by operand class");
			}
	vector const move_stack[] = {
		{0x7212, 2 | dasm::SUPPORTED, "MVDM    $2345, $12"},
		{0x7293, 2 | dasm::SUPPORTED, "MVDM    $2345, *AR3+"},
		{0x7312, 2 | dasm::SUPPORTED, "MVMD    $12, $2345"},
		{0x738b, 2 | dasm::SUPPORTED, "MVMD    *AR3-, $2345"},
		{0x4b81, 1 | dasm::SUPPORTED, "PSHD    *AR1"},
		{0x4bf8, 2 | dasm::SUPPORTED, "PSHD    *($2345)"},
		{0x8bed, 2 | dasm::SUPPORTED, "POPD    *+AR5($2345)"},
		{0x8bf8, 2 | dasm::SUPPORTED, "POPD    *($2345)"},
		{0x72e0, 1, ".word   $72E0"},
		{0x73f8, 1, ".word   $73F8"},
	};
	for (auto const &test : move_stack)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"MMR move syntax preserves direction without admitting forbidden addressing");
	}
	osd_printf_info("TMS320C54x move/stack disassembler: PASS forms=960 rejected=64 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned mode = 0; mode < 256; ++mode)
		{
			words const buffer(pc, 0x6b00 | mode, 0x2345, 0x6789);
			std::ostringstream text;
			bool const valid = (mode & 0xf8) != 0x98;
			unsigned const length = !valid ? 1 : mode >= 0xe0 ? 3 : 2;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (length | (valid ? dasm::SUPPORTED : 0)) &&
				buffer.seen() == (1U << length) - 1, "ADDM decoder bounds address/literal extensions and rejects short preincrement");
		}
	vector const addm_forms[] = {
		{0x6b12, 2 | dasm::SUPPORTED, "ADDM    #$2345, $12"},
		{0x6b8a, 2 | dasm::SUPPORTED, "ADDM    #$2345, *AR2-"},
		{0x6bea, 3 | dasm::SUPPORTED, "ADDM    #$6789, *+AR2($2345)"},
		{0x6bf8, 3 | dasm::SUPPORTED, "ADDM    #$6789, *($2345)"},
	};
	for (auto const &test : addm_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345, 0x6789);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"ADDM syntax retains displacement-before-immediate layout");
	}
	osd_printf_info("TMS320C54x ADDM disassembler: PASS forms=248 rejected=8 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xf484, 0xf485, 0xf48f})
			for (unsigned accumulators = 0; accumulators < 4; ++accumulators)
			{
				words const buffer(pc, family | (accumulators << 8), 0x2345);
				std::ostringstream text, expected;
				char const *name = family == 0xf484 ? "NEG" : family == 0xf485 ? "ABS" : "NORM";
				util::stream_format(expected, "%-8s%c, %c", name, accumulators & 2 ? 'B' : 'A', accumulators & 1 ? 'B' : 'A');
				expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) &&
					buffer.seen() == 1 && text.str() == expected.str(), "Unary accumulator syntax preserves both selectors without extension reads");
			}
	osd_printf_info("TMS320C54x unary disassembler: PASS forms=12 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	vector const normalization_forms[] = {
		{0xf483, 1 | dasm::SUPPORTED, "SAT     A"},
		{0xf583, 1 | dasm::SUPPORTED, "SAT     B"},
		{0xf486, 1 | dasm::SUPPORTED, "MAX     A"},
		{0xf586, 1 | dasm::SUPPORTED, "MAX     B"},
		{0xf487, 1 | dasm::SUPPORTED, "MIN     A"},
		{0xf587, 1 | dasm::SUPPORTED, "MIN     B"},
		{0xf48e, 1 | dasm::SUPPORTED, "EXP     A"},
		{0xf58e, 1 | dasm::SUPPORTED, "EXP     B"},
		{0xf49f, 1 | dasm::SUPPORTED, "RND     A, A"},
		{0xf59f, 1 | dasm::SUPPORTED, "RND     A, B"},
		{0xf69f, 1 | dasm::SUPPORTED, "RND     B, A"},
		{0xf79f, 1 | dasm::SUPPORTED, "RND     B, B"},
		{0xf683, 1, ".word   $F683"},
		{0xf786, 1, ".word   $F786"},
		{0xf687, 1, ".word   $F687"},
		{0xf78e, 1, ".word   $F78E"},
	};
	for (u32 pc : {0x010100, 0x01ffff})
		for (auto const &test : normalization_forms)
		{
			words const buffer(pc, test.op, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == 1,
				"Normalization/min-max decoder preserves selector width without reading extensions");
		}
	osd_printf_info("TMS320C54x normalization disassembler: PASS forms=12 rejected=4 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xf400, 0xf420, 0xf480, 0xf481, 0xf482})
			for (unsigned accumulators = 0; accumulators < 4; ++accumulators)
				for (unsigned shift = 0; shift < (family < 0xf480 ? 32U : 1U); ++shift)
				{
					words const buffer(pc, family | (accumulators << 8) | shift, 0x2345);
					std::ostringstream text, expected;
					char const *name = family == 0xf400 || family == 0xf480 ? "ADD" : family == 0xf482 ? "LD" : "SUB";
					util::stream_format(expected, "%-8s%c, ", name, accumulators & 2 ? 'B' : 'A');
					if (family >= 0xf480) expected << "ASM";
					else expected << (shift & 16 ? int(shift) - 32 : int(shift));
					util::stream_format(expected, ", %c", accumulators & 1 ? 'B' : 'A');
					expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) &&
						buffer.seen() == 1 && text.str() == expected.str(), "Accumulator ALU decoder preserves signed shift and ASM source/destination selectors");
				}
	osd_printf_info("TMS320C54x accumulator ALU disassembler: PASS shifts=256 asm=12 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned destination = 0; destination < 2; ++destination)
			for (unsigned shift = 0; shift <= 16; ++shift)
			{
				u16 const opcode = (shift == 16 ? 0xf062 : 0xf020 + shift) | (destination << 8);
				words const buffer(pc, opcode, 0x2345);
				std::ostringstream text, expected;
				util::stream_format(expected, "LD      #$2345, %u, %c", shift, destination ? 'B' : 'A');
				expect(decoder.disassemble(text, pc, buffer, buffer) == (2 | dasm::SUPPORTED) &&
					buffer.seen() == 3 && text.str() == expected.str(), "Immediate LD decoder preserves shift and destination with one literal extension");
				words const alias(pc, opcode | 0x0200, 0x2345);
				std::ostringstream alias_text;
				expect(decoder.disassemble(alias_text, pc, alias, alias) == 1 && alias.seen() == 1,
					"Immediate LD decoder does not promote undocumented source-bit aliases");
			}
	osd_printf_info("TMS320C54x immediate load disassembler: PASS forms=34 rejected=34 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x1200, 0x1300})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) &&
					buffer.seen() == (length == 2 ? 3 : 1), "Unsigned load decoder bounds extensions for both destinations");
			}
	vector const unsigned_load_forms[] = {
		{0x1212, 1 | dasm::SUPPORTED, "LDU     $12, A"},
		{0x1383, 1 | dasm::SUPPORTED, "LDU     *AR3, B"},
		{0x1294, 1 | dasm::SUPPORTED, "LDU     *AR4+, A"},
		{0x13ed, 2 | dasm::SUPPORTED, "LDU     *+AR5($2345), B"},
		{0x12f8, 2 | dasm::SUPPORTED, "LDU     *($2345), A"},
		{0x13f8, 2 | dasm::SUPPORTED, "LDU     *($2345), B"},
	};
	for (auto const &test : unsigned_load_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Unsigned load syntax retains memory addressing and destination selectors");
	}
	osd_printf_info("TMS320C54x unsigned load disassembler: PASS forms=512 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned mode = 0; mode < 256; ++mode)
		{
			words const buffer(pc, 0x7f00 | mode, 0x2345);
			std::ostringstream text;
			unsigned const length = mode >= 0xe0 ? 2 : 1;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
				"WRITA decoder bounds its source address extension");
		}
	vector const writa_forms[] = {
		{0x7f12, 1 | dasm::SUPPORTED, "WRITA   $12"},
		{0x7f92, 1 | dasm::SUPPORTED, "WRITA   *AR2+"},
		{0x7fe2, 2 | dasm::SUPPORTED, "WRITA   *AR2($2345)"},
		{0x7ff8, 2 | dasm::SUPPORTED, "WRITA   *($2345)"},
	};
	for (auto const &test : writa_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"WRITA syntax preserves data source addressing");
	}
	osd_printf_info("TMS320C54x WRITA disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xc800, 0xc900, 0xca00, 0xcb00, 0xe400, 0xe600})
			for (unsigned operands = 0; operands < 256; ++operands)
			{
				words const buffer(pc, family | operands, 0x2345);
				std::ostringstream text;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
					"Parallel store/load decoder reads no extensions for dual-memory operands");
			}
	vector const parallel_load_forms[] = {
		{0xc849, 1 | dasm::SUPPORTED, "ST      A, *AR3+ || LD *AR2-, A"},
		{0xcba5, 1 | dasm::SUPPORTED, "ST      B, *AR3- || LD *AR4+, B"},
		{0xe4e9, 1 | dasm::SUPPORTED, "ST      A, *AR3+ || LD *AR4+0%, T"},
		{0xe623, 1 | dasm::SUPPORTED, "ST      B, *AR5 || LD *AR4, T"},
		{0xe79f, 1, ".word   $E79F"},
		{0xe790, 1, ".word   $E790"},
	};
	for (auto const &test : parallel_load_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == 1,
			"Parallel store/load syntax preserves source, destination and dual-memory modifiers");
	}
	osd_printf_info("TMS320C54x parallel load disassembler: PASS forms=1536 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned operand = 0; operand < 256; ++operand)
		{
			words const frame(pc, 0xee00 | operand, 0x2345);
			std::ostringstream frame_text, expected;
			util::stream_format(expected, "FRAME   #%d", operand & 128 ? int(operand) - 256 : int(operand));
			expect(decoder.disassemble(frame_text, pc, frame, frame) == (1 | dasm::SUPPORTED) &&
				frame.seen() == 1 && frame_text.str() == expected.str(), "FRAME decoder retains signed immediate values without extensions");
			words const move(pc, 0xe700 | operand, 0x2345);
			std::ostringstream move_text;
			bool const valid = (operand >> 4) <= 8 && (operand & 15) <= 8;
			expect(decoder.disassemble(move_text, pc, move, move) == (valid ? 1 | dasm::SUPPORTED : 1) && move.seen() == 1,
				"MVMM decoder admits only the nine documented registers");
		}
	vector const frame_move_forms[] = {
		{0xee80, 1 | dasm::SUPPORTED, "FRAME   #-128"},
		{0xee7f, 1 | dasm::SUPPORTED, "FRAME   #127"},
		{0xe781, 1 | dasm::SUPPORTED, "MVMM    SP, AR1"},
		{0xe718, 1 | dasm::SUPPORTED, "MVMM    AR1, SP"},
		{0xe723, 1 | dasm::SUPPORTED, "MVMM    AR2, AR3"},
	};
	for (auto const &test : frame_move_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"FRAME/MVMM syntax preserves signed endpoints and SP identity");
	}
	osd_printf_info("TMS320C54x frame/MMR move disassembler: PASS frames=256 moves=81 rejected=175 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned immediate = 0; immediate < 512; ++immediate)
		{
			words const buffer(pc, 0xea00 | immediate, 0x2345);
			std::ostringstream text, expected;
			util::stream_format(expected, "LD      #%u, DP", immediate);
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1 && text.str() == expected.str(),
				"Immediate DP decoder retains all nine bits without reading extensions");
		}
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned operands = 0; operands < 256; ++operands)
		{
			words const buffer(pc, 0xe500 | operands, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
				"MVDD decoder uses only the packed dual-memory word");
		}
	vector const dual_move_forms[] = {
		{0xe5c9, 1 | dasm::SUPPORTED, "MVDD    *AR2+0%, *AR3+"},
		{0xe598, 1 | dasm::SUPPORTED, "MVDD    *AR3+, *AR2+"},
		{0xe5a8, 1 | dasm::SUPPORTED, "MVDD    *AR4+, *AR2+"},
		{0xe5ca, 1 | dasm::SUPPORTED, "MVDD    *AR2+0%, *AR4+"},
	};
	for (auto const &test : dual_move_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"MVDD syntax preserves X source and Y destination ordering");
	}
	osd_printf_info("TMS320C54x dual move/DP disassembler: PASS moves=256 dp=512 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x3000, 0x3200, 0x4600})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
					"Special-register memory loads bound their address extension");
			}
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned arp = 0; arp < 8; ++arp)
		{
			words const buffer(pc, 0xf4a0 | arp, 0x2345);
			std::ostringstream text, expected;
			util::stream_format(expected, "LD      #%u, ARP", arp);
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1 && text.str() == expected.str(),
				"Immediate ARP decoder preserves all three selector bits");
		}
	vector const special_load_forms[] = {
		{0x3093, 1 | dasm::SUPPORTED, "LD      *AR3+, T"},
		{0x32e2, 2 | dasm::SUPPORTED, "LD      *AR2($2345), ASM"},
		{0x46f8, 2 | dasm::SUPPORTED, "LD      *($2345), DP"},
		{0x4612, 1 | dasm::SUPPORTED, "LD      $12, DP"},
		{0xf4a8, 1, ".word   $F4A8"},
	};
	for (auto const &test : special_load_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Special-register load syntax names destinations and rejects undocumented ARP aliases");
	}
	osd_printf_info("TMS320C54x special load disassembler: PASS memory_forms=768 arp=8 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0xf080, 0xf0a0})
			for (unsigned accumulators = 0; accumulators < 4; ++accumulators)
				for (unsigned shift = 0; shift < 32; ++shift)
				{
					words const buffer(pc, family | (accumulators << 8) | shift, 0x2345);
					std::ostringstream text, expected;
					util::stream_format(expected, "%-8s%c, %d, %c", family == 0xf080 ? "AND" : "OR",
						accumulators & 2 ? 'B' : 'A', shift & 16 ? int(shift) - 32 : int(shift), accumulators & 1 ? 'B' : 'A');
					expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1 && text.str() == expected.str(),
						"Accumulator AND/OR decoder preserves signed shifts and both selectors without extensions");
				}
	osd_printf_info("TMS320C54x accumulator logical disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700, 0x3400})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
					"Memory multiply and BITT decoders bound source address extensions");
			}
	vector const multiply_bit_forms[] = {
		{0x2082, 1 | dasm::SUPPORTED, "MPY     *AR2, A"},
		{0x218a, 1 | dasm::SUPPORTED, "MPY     *AR2-, B"},
		{0x22f8, 2 | dasm::SUPPORTED, "MPYR    *($2345), A"},
		{0x23e2, 2 | dasm::SUPPORTED, "MPYR    *AR2($2345), B"},
		{0x2494, 1 | dasm::SUPPORTED, "MPYU    *AR4+, A"},
		{0x2512, 1 | dasm::SUPPORTED, "MPYU    $12, B"},
		{0x2692, 1 | dasm::SUPPORTED, "SQUR    *AR2+, A"},
		{0x27f8, 2 | dasm::SUPPORTED, "SQUR    *($2345), B"},
		{0x34f8, 2 | dasm::SUPPORTED, "BITT    *($2345)"},
	};
	for (auto const &test : multiply_bit_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Memory multiply syntax retains signed/rounded/unsigned distinction and BITT has no accumulator selector");
	}
	osd_printf_info("TMS320C54x memory multiply/bit disassembler: PASS forms=2304 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned accumulators = 0; accumulators < 4; ++accumulators)
			for (unsigned shift = 0; shift < 32; ++shift)
			{
				words const buffer(pc, 0xf0e0 | (accumulators << 8) | shift, 0x2345);
				std::ostringstream text, expected;
				util::stream_format(expected, "SFTL    %c, %d, %c", accumulators & 2 ? 'B' : 'A',
					shift & 16 ? int(shift) - 32 : int(shift), accumulators & 1 ? 'B' : 'A');
				expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1 && text.str() == expected.str(),
					"SFTL decoder retains signed shift and logical mnemonic without extensions");
			}
	osd_printf_info("TMS320C54x logical shift disassembler: PASS forms=128 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned word = 0; word < 1024; ++word)
		{
			words const buffer(pc, 0xa000 | word, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
				"Dual-memory ADD/SUB decoder consumes only the packed operand word");
		}
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned accumulators = 0; accumulators < 4; ++accumulators)
			for (unsigned operation = 0; operation < 4; ++operation)
			{
				words const buffer(pc, 0xf488 | (accumulators << 8) | operation, 0x2345);
				std::ostringstream text, expected;
				char const *names[] = {"MACA", "MACAR", "MASA", "MASAR"};
				util::stream_format(expected, "%-8sT, %c, %c", names[operation], accumulators & 2 ? 'B' : 'A', accumulators & 1 ? 'B' : 'A');
				expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1 && text.str() == expected.str(),
					"Accumulator multiply-accumulate decoder retains subtraction and rounding fields");
			}
	vector const dual_arithmetic_forms[] = {
		{0xa249, 1 | dasm::SUPPORTED, "SUB     *AR2-, *AR3+, A"},
		{0xa201, 1 | dasm::SUPPORTED, "SUB     *AR2, *AR3, A"},
		{0xa3c9, 1 | dasm::SUPPORTED, "SUB     *AR2+0%, *AR3+, B"},
		{0xa198, 1 | dasm::SUPPORTED, "ADD     *AR3+, *AR2+, B"},
	};
	for (auto const &test : dual_arithmetic_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Dual arithmetic syntax preserves X/Y order and destination");
	}
	osd_printf_info("TMS320C54x dual/accumulator arithmetic disassembler: PASS dual_forms=1024 accumulator_forms=16 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x4700, 0x8c00, 0x8d00})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
					"Memory repeat and register-store decoders bound address extensions");
			}
	vector const repeat_store_forms[] = {
		{0x47f8, 2 | dasm::SUPPORTED, "RPT     *($2345)"},
		{0x4792, 1 | dasm::SUPPORTED, "RPT     *AR2+"},
		{0x8cf8, 2 | dasm::SUPPORTED, "ST      T, *($2345)"},
		{0x8ce2, 2 | dasm::SUPPORTED, "ST      T, *AR2($2345)"},
		{0x8d12, 1 | dasm::SUPPORTED, "ST      TRN, $12"},
		{0x8d94, 1 | dasm::SUPPORTED, "ST      TRN, *AR4+"},
	};
	for (auto const &test : repeat_store_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Memory repeat and register-store syntax distinguishes count source and stored register");
	}
	osd_printf_info("TMS320C54x memory repeat/store disassembler: PASS forms=768 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x5000, 0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5800, 0x5900})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
					"Double arithmetic decoder bounds long-memory address extensions");
			}
	vector const double_arithmetic_forms[] = {
		{0x5082, 1 | dasm::SUPPORTED, "DADD    *AR2, A, A"},
		{0x5385, 1 | dasm::SUPPORTED, "DADD    *AR5, B, B"},
		{0x51f8, 2 | dasm::SUPPORTED, "DADD    *($2345), A, B"},
		{0x54e2, 2 | dasm::SUPPORTED, "DSUB    *AR2($2345), A"},
		{0x5512, 1 | dasm::SUPPORTED, "DSUB    $12, B"},
		{0x5894, 1 | dasm::SUPPORTED, "DRSUB   *AR4+, A"},
		{0x59f8, 2 | dasm::SUPPORTED, "DRSUB   *($2345), B"},
	};
	for (auto const &test : double_arithmetic_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Double arithmetic syntax preserves separate DADD selectors and subtract direction");
	}
	osd_printf_info("TMS320C54x double arithmetic disassembler: PASS forms=2048 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x1e00, 0x1f00})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
					"Conditional subtract decoder bounds operand address extensions");
			}
	vector const subc_forms[] = {
		{0x1e82, 1 | dasm::SUPPORTED, "SUBC    *AR2, A"},
		{0x1e84, 1 | dasm::SUPPORTED, "SUBC    *AR4, A"},
		{0x1f94, 1 | dasm::SUPPORTED, "SUBC    *AR4+, B"},
		{0x1fe2, 2 | dasm::SUPPORTED, "SUBC    *AR2($2345), B"},
		{0x1ef8, 2 | dasm::SUPPORTED, "SUBC    *($2345), A"},
	};
	for (auto const &test : subc_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Conditional subtract syntax preserves both accumulator selections");
	}
	osd_printf_info("TMS320C54x conditional subtract disassembler: PASS forms=512 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x3100, 0x3300, 0x3500, 0x3700})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				unsigned const length = mode >= 0xe0 ? 2 : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
					"Accumulator-A memory multiply decoder bounds its source extensions");
			}
	vector const accumulator_a_forms[] = {
		{0x3185, 1 | dasm::SUPPORTED, "MPYA    *AR5"},
		{0x33f8, 2 | dasm::SUPPORTED, "MASA    *($2345)"},
		{0x35e2, 2 | dasm::SUPPORTED, "MACA    *AR2($2345)"},
		{0x3794, 1 | dasm::SUPPORTED, "MACAR   *AR4+"},
		{0xf48c, 1 | dasm::SUPPORTED, "MPYA    A"},
		{0xf58c, 1 | dasm::SUPPORTED, "MPYA    B"},
		{0xf48d, 1 | dasm::SUPPORTED, "SQUR    A, A"},
		{0xf58d, 1 | dasm::SUPPORTED, "SQUR    A, B"},
	};
	for (u32 pc : {0x010100, 0x01ffff})
		for (auto const &test : accumulator_a_forms)
		{
			words const buffer(pc, test.op, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == test.result && text.str() == test.text,
				"Accumulator-A multiply syntax distinguishes fixed and selected operands");
		}
	osd_printf_info("TMS320C54x accumulator-A multiply disassembler: PASS memory_forms=1024 short_forms=4 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (u16 family : {0x6200, 0x6300})
			for (unsigned mode = 0; mode < 256; ++mode)
			{
				words const buffer(pc, family | mode, 0x2345);
				std::ostringstream text;
				bool const valid = mode <= 0xf8 && (mode & 0xf8) != 0x98;
				unsigned const length = valid ? 2 + (mode >= 0xe0) : 1;
				expect(decoder.disassemble(text, pc, buffer, buffer) == (length | (valid ? dasm::SUPPORTED : 0)) && buffer.seen() == (1U << length) - 1,
					"Immediate memory multiply decoding bounds extensions and rejects write-only or invalid source modes");
			}
	vector const immediate_multiply_forms[] = {
		{0xf066, 2 | dasm::SUPPORTED, "MPY     #$2345, A"},
		{0xf166, 2 | dasm::SUPPORTED, "MPY     #$2345, B"},
		{0x6283, 2 | dasm::SUPPORTED, "MPY     *AR3, #$2345, A"},
		{0x63f8, 3 | dasm::SUPPORTED, "MPY     *($2345), #$6789, B"},
		{0x62e3, 3 | dasm::SUPPORTED, "MPY     *AR3($2345), #$6789, A"},
		{0x63eb, 3 | dasm::SUPPORTED, "MPY     *+AR3($2345), #$6789, B"},
		{0x63f3, 3 | dasm::SUPPORTED, "MPY     *+AR3($2345)%, #$6789, B"},
		{0x629b, 1, ".word   $629B"},
		{0x63f9, 1, ".word   $63F9"},
	};
	for (u32 pc : {0x010100, 0x01ffff})
		for (auto const &test : immediate_multiply_forms)
		{
			words const buffer(pc, test.op, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == test.result && text.str() == test.text && buffer.seen() == (1U << (test.result & dasm::LENGTHMASK)) - 1,
				"Immediate multiply syntax preserves accumulator, address-before-immediate order and page wrapping");
		}
	osd_printf_info("TMS320C54x immediate multiply disassembler: PASS memory_forms=512 immediate_forms=2 pages=2 bounded_extensions=1 operand_order=1 invalid_modes=1 execution_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned word = 0; word < 2048; ++word)
		{
			words const buffer(pc, 0x2800 | word, 0x2345);
			std::ostringstream text;
			unsigned const length = (word & 255) >= 0xe0 ? 2 : 1;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 2 ? 3 : 1),
				"Memory MAC/MAS decoder bounds address extensions across rounding and destination variants");
		}
	vector const memory_mac_forms[] = {
		{0x2882, 1 | dasm::SUPPORTED, "MAC     *AR2, A"},
		{0x29f8, 2 | dasm::SUPPORTED, "MAC     *($2345), B"},
		{0x2ae2, 2 | dasm::SUPPORTED, "MACR    *AR2($2345), A"},
		{0x2b94, 1 | dasm::SUPPORTED, "MACR    *AR4+, B"},
		{0x2c12, 1 | dasm::SUPPORTED, "MAS     $12, A"},
		{0x2d92, 1 | dasm::SUPPORTED, "MAS     *AR2+, B"},
		{0x2ef8, 2 | dasm::SUPPORTED, "MASR    *($2345), A"},
		{0x2f84, 1 | dasm::SUPPORTED, "MASR    *AR4, B"},
	};
	for (auto const &test : memory_mac_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Memory MAC/MAS syntax retains subtraction, rounding and destination bits");
	}
	osd_printf_info("TMS320C54x memory MAC/MAS disassembler: PASS forms=2048 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned word = 0; word < 1024; ++word)
		{
			words const buffer(pc, 0x9800 | word, 0x2345);
			std::ostringstream text;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (1 | dasm::SUPPORTED) && buffer.seen() == 1,
				"Short shifted store has no extension across accumulator, half and address variants");
		}
	vector const shifted_store_forms[] = {
		{0x9800, 1 | dasm::SUPPORTED, "STL     A, 0, *AR2"},
		{0x99ff, 1 | dasm::SUPPORTED, "STL     B, 15, *AR5+0%"},
		{0x9a5f, 1 | dasm::SUPPORTED, "STH     A, 15, *AR3-"},
		{0x9b80, 1 | dasm::SUPPORTED, "STH     B, 0, *AR2+"},
	};
	for (auto const &test : shifted_store_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"Short shifted store syntax retains unsigned shift and dual address modifier");
	}
	osd_printf_info("TMS320C54x short shifted store disassembler: PASS forms=1024 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned mode = 0; mode < 256; ++mode)
		{
			words const buffer(pc, 0x7c00 | mode, 0x2345, 0x6789);
			std::ostringstream text;
			unsigned const length = mode >= 0xe0 ? 3 : 2;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && buffer.seen() == (length == 3 ? 7 : 3),
				"MVPD fetches only its program address and any destination extension across page wrap");
		}
	vector const mvpd_forms[] = {
		{0x7c12, 2 | dasm::SUPPORTED, "MVPD    $2345, $12"},
		{0x7c92, 2 | dasm::SUPPORTED, "MVPD    $2345, *AR2+"},
		{0x7ce2, 3 | dasm::SUPPORTED, "MVPD    $2345, *AR2($6789)"},
		{0x7cea, 3 | dasm::SUPPORTED, "MVPD    $2345, *+AR2($6789)"},
		{0x7cf8, 3 | dasm::SUPPORTED, "MVPD    $6789, *($2345)"},
	};
	for (auto const &test : mvpd_forms)
	{
		words const buffer(0x01ffff, test.op, 0x2345, 0x6789);
		std::ostringstream text;
		expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == test.result && text.str() == test.text,
			"MVPD syntax distinguishes long-offset and absolute extension order");
	}
	osd_printf_info("TMS320C54x MVPD disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0\n");
	unsigned shifted_valid = 0, shifted_invalid = 0;
	for (u32 pc : {0x010100, 0x01ffff})
		for (unsigned mode = 0; mode < 256; ++mode)
			for (unsigned kind = 0; kind < 5; ++kind)
				for (unsigned selection = 0; selection < (kind < 2 ? 4 : 2); ++selection)
					for (unsigned shift = 0; shift < 32; ++shift)
					{
						u16 const extension = 0x0c00 | (kind << 5) | (selection << 8) | shift;
						bool const extended = mode >= 0xe0;
						words const buffer(pc, 0x6f00 | mode, extended ? 0x2345 : extension, extension);
						std::ostringstream text;
						bool const valid = mode <= 0xf8 &&
							((mode & 0xf8) != 0x98 || kind >= 3);
						if (valid) ++shifted_valid; else ++shifted_invalid;
						u32 const expected = valid ? ((2 + extended) | dasm::SUPPORTED) : 1;
						expect(decoder.disassemble(text, pc, buffer, buffer) == expected &&
							buffer.seen() == (extended ? 7 : 3), "Shifted memory decoder distinguishes source/store operand domains across page wrap");
					}
	for (unsigned extension = 0; extension < 65536; ++extension)
	{
		unsigned const kind = (extension >> 5) & 7;
		bool const legal = (extension >> 10) == 3 && kind <= 4 && (kind < 2 || !(extension & 0x0200));
		if (legal) continue;
		for (u16 op : {0x6f82, 0x6ff8})
		{
			bool const extended = op == 0x6ff8;
			words const buffer(0x01ffff, op, extended ? 0x2345 : extension, extension);
			std::ostringstream text;
			expect(decoder.disassemble(text, 0x01ffff, buffer, buffer) == 1 && buffer.seen() == (extended ? 7 : 3),
				"Reserved shifted memory extension remains raw rather than claiming an instruction boundary");
		}
	}
	struct fetched_vector { u16 op, first, second; char const *text; };
	fetched_vector const fetched_forms[] = {
		{0x6f82, 0x0c58, 0, "LD      *AR2, -8, A"},
		{0x6f83, 0x0c1e, 0, "ADD     *AR3, -2, A, A"},
		{0x6f83, 0x0c5f, 0, "LD      *AR3, -1, A"},
		{0x6f8a, 0x0c96, 0, "STL     A, -10, *AR2-"},
		{0x6f8b, 0x0d91, 0, "STL     B, -15, *AR3-"},
		{0x6f92, 0x0d96, 0, "STL     B, -10, *AR2+"},
		{0x6f93, 0x0c91, 0, "STL     A, -15, *AR3+"},
		{0x6f95, 0x0e20, 0, "SUB     *AR5+, 0, B, A"},
		{0x6fd2, 0x0c45, 0, "LD      *AR2+%, 5, A"},
		{0x6ff8, 0x0020, 0x0c44, "LD      *($0020), 4, A"},
		{0x6ff8, 0x00ee, 0x0d20, "SUB     *($00EE), 0, A, B"},
		{0x6ff8, 0x0308, 0x0d20, "SUB     *($0308), 0, A, B"},
		{0x6ff8, 0x06c3, 0x0c48, "LD      *($06C3), 8, A"},
		{0x6ff8, 0x0852, 0x0e20, "SUB     *($0852), 0, B, A"},
		{0x6ff8, 0x08e4, 0x0e20, "SUB     *($08E4), 0, B, A"},
		{0x6ff8, 0x1f0c, 0x0c9c, "STL     A, -4, *($1F0C)"},
		{0x6ff8, 0x1f0d, 0x0c8c, "STL     A, 12, *($1F0D)"},
		{0x6ff8, 0x219b, 0x0d20, "SUB     *($219B), 0, A, B"},
		{0x6f82, 0x0c6f, 0, "STH     A, 15, *AR2"},
		{0x6fea, 0x2345, 0x0d70, "STH     B, -16, *+AR2($2345)"},
	};
	for (u32 pc : {0x010100, 0x01ffff})
		for (auto const &test : fetched_forms)
		{
			words const buffer(pc, test.op, test.first, test.second);
			std::ostringstream text;
			unsigned const length = (test.op & 255) >= 0xe0 ? 3 : 2;
			expect(decoder.disassemble(text, pc, buffer, buffer) == (length | dasm::SUPPORTED) && text.str() == test.text,
				"Captured shifted memory syntax retains operation, signed shift, address and independent accumulators");
		}
	expect(shifted_valid == 217984 && shifted_invalid == 11392,
		"Shifted memory source/store census covers all address/extension tuples on both pages");
	osd_printf_info("TMS320C54x shifted memory disassembler: PASS forms=%u invalid_operands=%u rejected=130176 captured=18 syntax=20 pages=2 bounded_extensions=1 pipeline_claim=0\n",
		shifted_valid / 2, shifted_invalid / 2);
}

} // namespace c54x_test
