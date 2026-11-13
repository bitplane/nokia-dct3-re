// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "tms320c54x.h"
#include "tms320c54x_dasm.h"
#include "tms320c54x_decode.h"

#include <sstream>

namespace {

using namespace c54x_decode;

constexpr u16 ST1_BRAF = 0x8000;
constexpr u16 ST1_INTM = 0x0800;
constexpr u16 ST1_RESERVED = 0x0400;

constexpr int normalization_shift(u16 t)
{
	// Negative NORM counts alias by sixteen outside the documented range.
	// ROM4 Pow2 uses this together with a conditional SFTA -16.
	unsigned const code = t & 63;
	return code < 32 ? int(code) : int(code & 15) - 16;
}

u16 reverse_carry_index(u16 address, u16 index, bool subtract)
{
	// SPRU131G 5.5.3.5: carry/borrow propagates from the MSB to the LSB.
	const u16 left = bitswap<16>(address, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	const u16 right = bitswap<16>(index, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	return bitswap<16>(u16(subtract ? left - right : left + right), 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}

constexpr bool popd_own_pointer(u16 word, u16 destination)
{
	// SPRU131G tables 7-7/7-11: a POPD into ARx or SP is unprotected for the
	// next pointer use. Within a repeat the DAGEN updates of SP and of the
	// popping pointer survive; the popped word is not written over them.
	u8 const mode = word;
	return (word & 0xff00) == 0x8b00 && (destination == 0x18 ||
		(BIT(mode, 7) && (mode & 0x78) && mode < 0xe0 && destination == 0x10 + (mode & 7)));
}

s32 accumulator_high17(u64 value)
{
	const u32 high = (value >> 16) & 0x1ffff;
	return (high & 0x10000) ? s32(high) - 0x20000 : s32(high);
}

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
	m_ready_in_cb(*this, 1),
	m_xf_out_cb(*this),
	m_peripheral_clock_stop_cb(*this),
	m_pll_clock_stop_cb(*this),
	m_clkout_divisor_cb(*this),
	m_iack_enable_cb(*this),
	m_system_clock_cb(*this),
	m_instruction_observer_cb(*this)
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

bool tms320c54x_device::observe_execute_memory_multiply(offs_t address, u16 opcode, u16 &source, u16 &operand, u16 &immediate) const
{
	if (!m_pipeline_valid[5] || m_pipeline_annulled[5] ||
		m_pipeline_address[5] != address || m_pipeline_word[5] != opcode || !staged_memory_immediate_multiply(opcode))
		return false;
	source = m_pipeline_operand_address[5];
	operand = m_pipeline_operand[5];
	immediate = m_pipeline_immediate[5];
	return true;
}

bool tms320c54x_device::observe_execute_memory_store(offs_t address, u16 opcode, u16 &destination, u16 &value) const
{
	if (!m_pipeline_valid[5] || m_pipeline_annulled[5] || m_pipeline_address[5] != address ||
		m_pipeline_word[5] != opcode || (!staged_store(opcode) && !staged_direct_push(opcode))) return false;
	destination = m_pipeline_operand_address[5];
	value = staged_direct_push(opcode) ? m_pipeline_operand[5] : pipeline_store_word();
	return true;
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
	if (machine().options().verbose()) m_coverage = std::make_unique<opcode_coverage>();
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
	state_add(STATE_RC, "RC", m_rptc).formatstr("%04X");
	state_add(STATE_SP, "SP", m_sp).formatstr("%04X");
	state_add(STATE_ST0, "ST0", m_st0).formatstr("%04X");
	state_add(STATE_ST1, "ST1", m_st1).callimport().formatstr("%04X");
	state_add(STATE_PMST, "PMST", m_pmst).formatstr("%04X");
	for (unsigned i = 0; i != std::size(m_ar); ++i)
		state_add(STATE_AR0 + i, string_format("AR%u", i).c_str(), m_ar[i]).formatstr("%04X");
	state_add(STATE_BRC, "BRC", m_brc).formatstr("%04X");
	state_add(STATE_RSA, "RSA", m_rsa).formatstr("%04X");
	state_add(STATE_REA, "REA", m_rea).formatstr("%04X");
	state_add(STATE_BK, "BK", m_bk).formatstr("%04X");
	state_add(STATE_IFR, "IFR", m_ifr).formatstr("%04X");
	state_add(STATE_IMR, "IMR", m_imr).formatstr("%04X");
	state_add(STATE_SWWSR, "SWWSR", m_swwsr).formatstr("%04X");
	state_add(STATE_BSCR, "BSCR", m_bscr).mask(0xf006).callimport().formatstr("%04X");
	state_add(STATE_SWCR, "SWCR", m_swcr).mask(1).formatstr("%04X");
	state_add(STATE_TIM, "TIM", m_debug_timer).callimport().callexport().formatstr("%04X");
	state_add(STATE_PRD, "PRD", m_debug_timer).callimport().callexport().formatstr("%04X");
	state_add(STATE_TCR, "TCR", m_debug_timer).callimport().callexport().formatstr("%04X");
	state_add(STATE_IDLE, "IDLE", m_idle).callimport().formatstr("%1u");
	state_add(STATE_ILLEGAL, "ILLEGAL", m_illegal).formatstr("%1u");
	state_add(STATE_GENPC, "GENPC", m_debug_pc).mask(0x7fffff).callimport().callexport().noshow();
	state_add(STATE_GENPCBASE, "CURPC", m_debug_pc).mask(0x7fffff).callimport().callexport().noshow();
	state_add(STATE_READPC, "READPC", m_debug_pipeline_pc).readonly().callexport().formatstr("%08X");
	state_add(STATE_EXECPC, "EXECPC", m_debug_pipeline_pc).readonly().callexport().formatstr("%08X");

	save_item(NAME(m_pc));
	save_item(NAME(m_xpc));
	save_item(NAME(m_op));
	save_item(NAME(m_a));
	save_item(NAME(m_b));
	save_item(NAME(m_t));
	save_item(NAME(m_trn));
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
	save_item(NAME(m_rpt_iteration));
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
	save_item(NAME(m_pipeline_physical_address));
	save_item(NAME(m_pipeline_word));
	save_item(NAME(m_pipeline_annulled));
	save_item(NAME(m_pipeline_operand_address));
	save_item(NAME(m_pipeline_operand));
	save_item(NAME(m_pipeline_immediate));
	save_item(NAME(m_pipeline_extension_first));
	save_item(NAME(m_pipeline_valid));
	save_item(NAME(m_pipeline_early_write_deferred));
	save_item(NAME(m_pipeline_peripheral_wait_paid));
	save_item(NAME(m_pipeline_peripheral_read_pending));
	save_item(NAME(m_pipeline_io_wait_paid));
	save_item(NAME(m_pipeline_io_wait_remaining));
	save_item(NAME(m_pipeline_io_ready_required));
	save_item(NAME(m_pipeline_intm_protected));
	save_item(NAME(m_pipeline_direct_status));
	save_item(NAME(m_pipeline_alu_mode));
	save_item(NAME(m_pipeline_t));
	save_item(NAME(m_pipeline_cmpt));
	save_item(NAME(m_pipeline_bk));
	save_item(NAME(m_pipeline_direct_sp));
	save_item(NAME(m_pipeline_far_call_page));
	save_item(NAME(m_pipeline_far_call_stack));
	save_item(NAME(m_pipeline_acc_transfer_pending));
	save_item(NAME(m_pipeline_acc_transfer_target));
	save_item(NAME(m_pipeline_cc_pending));
	save_item(NAME(m_pipeline_block_first_final));
	save_item(NAME(m_pipeline_block_end));
	save_item(NAME(m_pipeline_block_body));
	save_item(NAME(m_pipeline_decode_block_body));
	save_item(NAME(m_pipeline_block_active));
	save_item(NAME(m_pipeline_block_fetch_enabled));
	save_item(NAME(m_pipeline_decode_block_first_final));
	save_item(NAME(m_pipeline_block_start));
	save_item(NAME(m_pipeline_block_end_address));
	save_item(NAME(m_pipeline_block_fetch_remaining));
	save_item(NAME(m_pipeline_compiler_sp_previous));
	save_item(NAME(m_pipeline_compiler_sp_wait));
	save_item(NAME(m_pipeline_idle_barrier));
	save_item(NAME(m_pipeline_idle_resume_pending));
	save_item(NAME(m_pipeline_idle_resume_address));
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
	save_item(NAME(m_pipeline_control_word));
	save_item(NAME(m_pipeline_bc_held));
	save_item(NAME(m_pipeline_bc_word));
	save_item(NAME(m_pipeline_bc_address));
	save_item(NAME(m_pipeline_bc_block_body));
	save_item(NAME(m_pipeline_bc_block_end));
	save_item(NAME(m_pipeline_bc_block_first_final));
	save_item(NAME(m_pipeline_retd_stage));
	save_item(NAME(m_pipeline_retd_words));
	save_item(NAME(m_pipeline_retd_addresses));
	save_item(NAME(m_pipeline_retd_target));
	save_item(NAME(m_pipeline_far_return_pending));
	save_item(NAME(m_pipeline_long_store_pending));
	save_item(NAME(m_pipeline_program_move_pending));
	save_item(NAME(m_pipeline_coefficient_pending));
	save_item(NAME(m_pipeline_coefficient_source));
	save_item(NAME(m_pipeline_repeat_coefficient_started));
	save_item(NAME(m_pipeline_repeat_active));
	save_item(NAME(m_pipeline_repeat_body_latched));
	save_item(NAME(m_pipeline_repeat_stream));
	save_item(NAME(m_pipeline_repeat_store_pending));
	save_item(NAME(m_pipeline_repeat_io_started));
	save_item(NAME(m_pipeline_repeat_io_value));
	save_item(NAME(m_pipeline_repeat_store_address));
	save_item(NAME(m_pipeline_repeat_issue_remaining));
	save_item(NAME(m_pipeline_repeat_word));
	save_item(NAME(m_pipeline_repeat_source));
	save_item(NAME(m_pipeline_repeat_status));
	save_item(NAME(m_pipeline_repeat_sp));
	save_item(NAME(m_pipeline_repeat_address));
	save_item(NAME(m_pipeline_repeat_redirect_pending));
	save_item(NAME(m_pipeline_repeat_redirect_target));
	save_item(NAME(m_pipeline_read_a_wait));
	save_item(NAME(m_pipeline_write_a_wait));
	save_item(NAME(m_pipeline_write_a_target));
	save_item(NAME(m_pipeline_write_a_logical));
	save_item(NAME(m_pipeline_mvdp_wait));
	save_item(NAME(m_pipeline_read_a_source));
	save_item(NAME(m_pipeline_memory_repeat_wait));
	save_item(NAME(m_pipeline_repeat_program_source));
	save_item(NAME(m_pipeline_dp_wait));
	save_item(NAME(m_pipeline_cycles));
	save_item(NAME(m_clkmd));
	save_item(NAME(m_swwsr));
	save_item(NAME(m_bscr));
	save_item(NAME(m_swcr));
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
	save_item(NAME(m_illegal));
}

void tms320c54x_device::device_stop()
{
	if (!m_coverage)
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
	for (unsigned opcode = 0; opcode != m_coverage->count.size(); ++opcode)
		if (m_coverage->count[opcode])
		{
			machine().logerror("[opcov] op=%04x first_pc=%06x count=%llu\n",
					opcode, m_coverage->first_pc[opcode], (unsigned long long)m_coverage->count[opcode]);
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
	if (m_coverage->dropped_tuples)
		machine().logerror("[opwords_limit] dropped=%llu\n", (unsigned long long)m_coverage->dropped_tuples);
	for (auto const &[words, observation] : m_coverage->extensions)
	{
		machine().logerror("[opwords] op=%04x words=%u ext1=%04x ext2=%04x first_pc=%06x count=%llu\n",
			words[1], words[0], words[2], words[3], observation.first_pc, (unsigned long long)observation.count);
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
	m_rpt_iteration = 0;
	m_ifr = 0;
	m_nmi_pending = false;
	m_nmi_asserted = false;
	// Hardware reset sets INTM and clears IFR, but does not initialise IMR.
	// Preserve the mask written by the ROM loader across MAD2 DSP reset pulses.
	m_clkmd = m_reset_clkmd_enabled ? m_reset_clkmd : 0;
	m_swwsr = software_wait_enabled() ? 0x7fff : 0;
	m_bscr = m_c5416_bus_profile ? 0xf000 : 0;
	m_swcr = 0;
	publish_bus_controls();
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
	{
		m_st1 &= ~ST1_RESERVED;
		update_xf();
	}
	else if (entry.index() == STATE_BSCR)
		publish_bus_controls();
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
	m_st1 &= ~ST1_RESERVED;
	// The output cache is derived; re-drive the restored architectural level.
	update_xf(true);
	m_peripheral_clock_stop_cb(m_idle_mode >= 2);
	m_pll_clock_stop_cb(m_idle_mode == 3);
	m_system_clock_cb(clock());
	publish_bus_controls();
}

void tms320c54x_device::publish_bus_controls()
{
	if (!m_c5416_bus_profile) return;
	// These are configuration outputs, not a synthesized CLKOUT waveform
	// or physical IACK pulse. Logical IRQ acknowledgement is independent.
	m_clkout_divisor_cb(clkout_divisor());
	m_iack_enable_cb(iack_output_enabled());
}

void tms320c54x_device::state_export(const device_state_entry &entry)
{
	if (entry.index() == STATE_READPC || entry.index() == STATE_EXECPC)
	{
		unsigned const stage = entry.index() == STATE_READPC ? 4 : 5;
		// Derived observation only: PC can already point beyond a bus owner.
		m_debug_pipeline_pc = m_pipeline_valid[stage] && !m_pipeline_annulled[stage]
				? m_pipeline_address[stage] : 0xffffffff;
	}
	else if (entry.index() == STATE_GENPC || entry.index() == STATE_GENPCBASE)
		m_debug_pc = logical_program_address(m_pc);
	else if (entry.index() >= STATE_TIM && entry.index() <= STATE_TCR)
		m_debug_timer = data_read(0x24 + entry.index() - STATE_TIM);
}

u16 tms320c54x_device::data_read(u16 address)
{
	return m_data.read_word(address);
}

void tms320c54x_device::data_write(u16 address, u16 value, unsigned owner_stage)
{
	bool const early_braf_clear = address == 7 && pipeline_early_braf_clear(owner_stage, value);
	if (m_pipeline_valid[owner_stage] && m_pipeline_block_body[owner_stage] &&
		((address >= 0x1a && address <= 0x1c) || (address == 7 && ((value ^ m_st1) & ST1_BRAF) && !early_braf_clear)))
	{
		logerror("%s: staged in-block loop-register write is unsupported (instruction=%06x word=%04x destination=%04x value=%04x block=%06x..%06x brc=%04x rsa=%04x rea=%04x)\n",
			machine().describe_context(), m_pipeline_address[owner_stage], m_pipeline_word[owner_stage], address, value,
			m_pipeline_block_start, m_pipeline_block_end_address, m_brc, m_rsa, m_rea);
		m_illegal = true; return;
	}
	bool const braf_before = bool(m_st1 & ST1_BRAF);
	m_data.write_word(address, value);
	if (address == 7 && m_pipeline_valid[owner_stage] &&
		bool(m_st1 & ST1_BRAF) != braf_before)
		pipeline_update_braf(m_pipeline_address[owner_stage]);
}

void tms320c54x_device::internal_data_map(address_map &map)
{
	// Only owned registers shadow the board map; reserved gaps stay external.
	for (u16 address = 0; address <= 0x58; ++address)
		if (address <= 1 || (address >= 6 && address <= 0x1d) ||
			(address == 0x1e && m_extended_program) || (address >= 0x24 && address <= 0x26) || address == 0x58 ||
			(software_wait_enabled() && address == 0x28) || (m_c5416_bus_profile && (address == 0x29 || address == 0x2b)))
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
		return m_st1 & ~ST1_RESERVED;
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
	if (software_wait_enabled() && address == 0x28) return m_swwsr;
	if (m_c5416_bus_profile && address == 0x29) return m_bscr;
	if (m_c5416_bus_profile && address == 0x2b) return m_swcr;
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
	{
		// SPRU131G table 4-2 forbids setting INTM through memory writes.
		// Zero-write clearing retains the legacy model; that passage does
		// not independently establish its semantics.
		m_st1 = (value & ~(ST1_INTM | ST1_RESERVED)) | (m_st1 & value & ST1_INTM);
		update_xf();
	}
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
	{
		// SPRU131G table 6-3 excludes ordinary data writes from XPC loads.
		// Debugger XPC assignment remains a separate architectural redirect.
	}
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
	else if (software_wait_enabled() && address == 0x28)
		m_swwsr = m_c5416_bus_profile ? value : value & 0x7fff;
	else if (m_c5416_bus_profile && address == 0x29)
	{
		m_bscr = value & 0xf006;
		publish_bus_controls();
	}
	else if (m_c5416_bus_profile && address == 0x2b)
		m_swcr = value & 1;
	else if (address == 0x58)
	{
		write_clkmd(value);
	}
	else
		fatalerror("Unmapped C54x register write %04x", address);
}

u64 tms320c54x_device::data_operand(u16 value) const
{
	if (BIT(m_st1, 8) && BIT(value, 15))
		return u64(s64(s16(value))) & ACC_MASK;
	return value;
}

u64 tms320c54x_device::shifted_by_t(u16 memory, u16 t) const
{
	const int shift = s8(u8(t << 2)) >> 2;
	const u64 source = data_operand(memory);
	return shift < 0 ?
		(BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) :
		(source << shift) & ACC_MASK;
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

u64 tms320c54x_device::conditional_subtract(u64 source, u16 memory, bool destination_b)
{
	const u64 divisor = data_operand(memory);
	const s64 difference = (s64(source << 24) >> 24) - (s64(divisor << 24) >> 24) * 0x8000;
	const bool subtract = difference >= 0;
	// SUBC's ALU result supplies overflow; OVM must not saturate its quotient step.
	if (difference < -2147483648LL || difference > 2147483647LL)
		m_st0 |= destination_b ? 0x0200 : 0x0400;
	m_st0 = (m_st0 & ~u16(0x0800)) | (subtract ? 0x0800 : 0);
	return (subtract ? (u64(difference) << 1) | 1 : source << 1) & ACC_MASK;
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

void tms320c54x_device::long_word_arithmetic(u16 op, u16 high, u16 low_word, u16 t)
{
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
	if (BIT(m_st1, 8) && BIT(high, 15)) memory |= u64(0xff) << 32;
	if (!BIT(m_st1, 7))
	{
		u64 t_pair = (u64(t) << 16) | t;
		if (BIT(m_st1, 8) && BIT(t, 15)) t_pair |= u64(0xff) << 32;
		const u64 left = memory_first ? memory : source;
		const u64 right = t_variant ? t_pair : memory_first ? source : memory;
		destination = add_sub(left, right, subtract_high, destination_b);
	}
	else // Independent 16-bit lanes; the high lane extends into guard bits.
	{
		const u16 left_high_word = memory_first ? high : u16(source >> 16);
		const u16 right_high_word = t_variant ? t : memory_first ? u16(source >> 16) : high;
		const u16 left_low_word = memory_first ? low_word : u16(source);
		const u16 right_low_word = t_variant ? t : memory_first ? u16(source) : low_word;
		const s32 left_high = BIT(m_st1, 8) ? s32(s16(left_high_word)) : s32(left_high_word);
		const s32 right_high = BIT(m_st1, 8) ? s32(s16(right_high_word)) : s32(right_high_word);
		const s32 result_high = subtract_high ? left_high - right_high : left_high + right_high;
		const u16 result_low = subtract_low ? left_low_word - right_low_word : left_low_word + right_low_word;
		const bool carry = subtract_high ? left_high_word >= right_high_word : u32(left_high_word) + right_high_word > 0xffff;
		m_st0 = (m_st0 & ~u16(0x0800)) | (carry ? 0x0800 : 0);
		destination = (u64(u32(result_high) & 0xffffff) << 16) | result_low;
	}
}

void tms320c54x_device::register_product_result(u16 word, u16 t)
{
	s64 product = s64(accumulator_high17(m_a)) * s16(t);
	if (BIT(m_st1,6)) product *= 2;
	s64 result = product;
	if ((word & 0xfeff) != 0xf48c)
	{
		product = multiply_product(product);
		u64 const source = accumulator(BIT(word,9));
		result = (s64(source << 24) >> 24) + (BIT(word,1) ? -product : product);
		if (BIT(word,0)) result = (result + 0x8000) & ~s64(0xffff);
	}
	accumulator(BIT(word,8)) = multiply_result(result, BIT(word,8));
}

void tms320c54x_device::square_accumulate_result(u16 word, u16 memory)
{
	// SPRU172C 4-163/4-164: signed square, no rounding or SMUL product clamp.
	s64 const product = s64(s16(memory)) * s16(memory) * (BIT(m_st1,6) ? 2 : 1);
	u64 &destination = accumulator(BIT(word,8));
	s64 const current = s64(destination << 24) >> 24;
	destination = multiply_result(BIT(word,9) ? current - product : current + product, BIT(word,8));
}

void tms320c54x_device::polynomial_step(u16 memory, u16 t)
{
	// SPRU172C 4-126: both results use old A/B; T is unchanged.
	s64 product = s64(accumulator_high17(m_a)) * s64(s16(t));
	if (BIT(m_st1, 6)) product *= 2;
	m_a = multiply_result((multiply_product(product) + (s64(m_b << 24) >> 24) + 0x8000) & ~s64(0xffff), false);
	s64 const coefficient = BIT(m_st1, 8) ? s64(s16(memory)) : s64(memory);
	m_b = u64(coefficient * 0x10000) & ACC_MASK;
}

void tms320c54x_device::accumulator_memory_multiply(u16 op, u16 memory)
{
	unsigned const family = (op >> 8) & 0x0f;
	s64 product = s64(accumulator_high17(m_a)) * s64(s16(memory));
	if (BIT(m_st1, 6)) product *= 2;
	if (family != 1) product = multiply_product(product);
	s64 result = family == 1 ? product : family == 3 ? (s64(m_b << 24) >> 24) - product : (s64(m_b << 24) >> 24) + product;
	if (family == 7) result = (result + 0x8000) & ~s64(0xffff);
	m_b = multiply_result(result, true);
}

u64 tms320c54x_device::memory_t_multiply(u16 op, u16 memory, u16 t, u64 source)
{
	unsigned const family = (op >> 8) & 0x0e;
	s64 product = family == 4 ? s64(t) * memory : family == 6
		? s64(s16(memory)) * s16(memory) : s64(s16(t)) * s16(memory);
	if (BIT(m_st1, 6)) product *= 2;
	if (family >= 8) product = multiply_product(product);
	s64 const accumulator_value = s64(source << 24) >> 24;
	s64 result = family <= 6 ? product : family >= 12 ? accumulator_value - product : accumulator_value + product;
	if (family == 2 || family == 10 || family == 14) result = (result + 0x8000) & ~s64(0xffff);
	return multiply_result(result, BIT(op, 8));
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

u64 tms320c54x_device::arithmetic_shift_right(u64 value, unsigned shift) const
{
	const s64 signed_value = s64(value << 24) >> 24;
	return u64(signed_value >> shift) & ACC_MASK;
}

bool tms320c54x_device::indirect_compatibility(unsigned stage) const
{
	return stage < 6 ? m_pipeline_cmpt[stage] : BIT(m_st1, 5);
}

unsigned tms320c54x_device::indirect_ar(u8 mode, unsigned stage) const
{
	const unsigned ar = mode & 7;
	return indirect_compatibility(stage) && !ar ? (m_st0 >> 13) & 7 : ar;
}

void tms320c54x_device::indirect_modify(u8 mode, bool long_operand, unsigned stage)
{
	const unsigned ar = indirect_ar(mode, stage);
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
	case 0x40: circular_modify(ar, -word_step, stage); break;
	case 0x48: circular_modify(ar, -s16(m_ar[0]), stage); break;
	case 0x50: circular_modify(ar, word_step, stage); break;
	case 0x58: circular_modify(ar, s16(m_ar[0]), stage); break;
	default: break;
	}
	// In compatibility mode, a nonzero ARF both selects the auxiliary register
	// and publishes it into ST0.ARP.  Standard mode addresses ARF directly and
	// must leave ARP cleared/unchanged (TMS320C54x CPU Reference Guide 5.5.1).
	if (indirect_compatibility(stage) && (mode & 7))
		m_st0 = (m_st0 & ~u16(0xe000)) | u16((mode & 7) << 13);
}

void tms320c54x_device::dual_modify(u8 operand, unsigned stage)
{
	const unsigned ar = 2 + (operand & 3);
	switch ((operand >> 2) & 3)
	{
	case 0: break;
	case 1: --m_ar[ar]; break;
	case 2: ++m_ar[ar]; break;
	case 3: circular_modify(ar, s16(m_ar[0]), stage); break;
	}
}

void tms320c54x_device::circular_modify(unsigned ar, s16 step, unsigned stage)
{
	u16 const bk = stage < 6 ? m_pipeline_bk[stage] : m_bk;
	if (!bk)
	{
		// Retained ROM4 codec behavior: BK=0 remains linear. SPRU131G
		// describes suppression instead; the fitted-silicon rule is unresolved.
		m_ar[ar] += step;
		return;
	}
	unsigned address_bits = 0;
	while ((1U << address_bits) <= bk && address_bits != 16)
		++address_bits;
	const u32 mask = address_bits == 16 ? 0xffff : (1U << address_bits) - 1;
	const u16 base = m_ar[ar] & ~mask;
	s32 index = (m_ar[ar] & mask) + step;
	while (index >= bk)
		index -= bk;
	while (index < 0)
		index += bk;
	m_ar[ar] = base | u16(index);
}

u16 tms320c54x_device::mmr_address(u8 mode, unsigned stage)
{
	if (!BIT(mode, 7))
		return mode & 0x7f;
	const unsigned ar = indirect_ar(mode, stage);
	const bool preincrement = (mode & 0x78) == 0x18;
	if (preincrement)
		indirect_modify(mode, false, stage);
	const u16 address = m_ar[ar] & 0x7f;
	if (!preincrement)
		indirect_modify(mode, false, stage);
	// MMR addressing clears the selected AR's upper nine bits after the access.
	m_ar[ar] &= 0x7f;
	return address;
}

u16 tms320c54x_device::direct_address(u8 mode) const
{
	// Stage-owned callers retain compiler-mode SP where required by
	// SPRU131G 7.5.5.1; this semantic helper uses architectural state.
	u16 const sp = m_sp;
	return BIT(m_st1, 14) ? u16(sp + (mode & 0x7f)) :
			u16(((m_st0 & 0x01ff) << 7) | (mode & 0x7f));
}

u16 tms320c54x_device::short_smem_address(u8 mode, unsigned stage) const
{
	return BIT(mode, 7) ? m_ar[indirect_ar(mode, stage)] : direct_address(mode);
}

u16 tms320c54x_device::long_offset_address(u8 mode, unsigned ar, s16 offset, unsigned stage)
{
	u16 address;
	if ((mode & 0x78) == 0x70)
	{
		circular_modify(ar, offset, stage);
		address = m_ar[ar];
	}
	else
	{
		address = m_ar[ar] + offset;
		if ((mode & 0x78) == 0x68)
			m_ar[ar] = address;
	}
	if (indirect_compatibility(stage) && (mode & 7))
		m_st0 = (m_st0 & ~u16(0xe000)) | u16((mode & 7) << 13);
	return address;
}

void tms320c54x_device::interrupt_entry_step()
{
	// SPRU131G example 7-18: entry drains the older pipeline slots.
	switch (m_interrupt_entry_stage++)
	{
	case 1: m_rtn = m_pc; break; // INTR Decode, before stack Access.
	case 2:
		// Staged Access shares a clock with older foreground Read operations;
		// pipeline_step reserves its slot after those older SP updates.
		break;
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

bool tms320c54x_device::accept_interrupt()
{
	if (m_vector_prefetch_valid) return false; // Finish the accepted entry before recognizing another source.
	// Maskable hardware sources occupy vectors 16..31 and map directly to
	// IMR/IFR bits 0..15. PMST.IPTR selects their 128-word vector page.
	const u16 pending = m_ifr & m_imr;
	// The C54x does not recognize an interrupt between a delayed control
	// transfer and its two delay words, nor between RPT/RPTZ and the repeated
	// instruction. IFR remains set and is reconsidered at the next legal
	// instruction boundary. pipeline_accept_interrupt owns these guards;
	// this helper selects a source only after that boundary is qualified.
	if (!m_nmi_pending && (BIT(m_st1, 11) || !pending))
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
	m_pipeline_block_first_final.fill(false);
	m_pipeline_block_end.fill(false);
	m_pipeline_block_body.fill(false);
	m_pipeline_decode_block_body = false;
	m_pipeline_block_active = m_pipeline_block_fetch_enabled = m_pipeline_decode_block_first_final = false;
	m_pipeline_block_start = m_pipeline_block_end_address = m_pipeline_block_fetch_remaining = 0;
	m_pipeline_early_write_deferred.fill(false);
	m_pipeline_peripheral_wait_paid.fill(false);
	m_pipeline_peripheral_read_pending = false;
	m_pipeline_io_wait_paid.fill(false);
	m_pipeline_io_wait_remaining = 0;
	m_pipeline_io_ready_required = false;
	m_pipeline_intm_protected.fill(false);
	m_pipeline_direct_status.fill(0);
	m_pipeline_alu_mode.fill(0);
	m_pipeline_t.fill(0);
	m_pipeline_cmpt.fill(false);
	m_pipeline_bk.fill(0);
	m_pipeline_direct_sp.fill(0);
	m_pipeline_far_call_page.fill(0);
	m_pipeline_far_call_stack.fill(0);
	m_pipeline_acc_transfer_pending = false;
	m_pipeline_acc_transfer_target = 0;
	m_pipeline_cc_pending = false;
	m_pipeline_compiler_sp_previous = 0;
	m_pipeline_compiler_sp_wait = 0;
	m_pipeline_idle_barrier = m_pipeline_idle_resume_pending = false;
	m_pipeline_idle_resume_address = 0;
	m_pipeline_intm_follow = false;
	m_pipeline_valid.fill(false);
	m_pipeline_annulled.fill(false);
	m_pipeline_physical_address.fill(0);
	m_pipeline_extension_first.fill(0);
	m_pipeline_decode_remaining = 0;
	m_pipeline_decode_extension = 0;
	m_pipeline_decode_word = 0;
	m_pipeline_decode_address = 0;
	m_pipeline_fetch_address = logical_program_address(m_pc);
	m_pipeline_return_pc = 0;
	m_pipeline_flush_front = m_pipeline_vector_pending = false;
	m_pipeline_decode_flush = m_pipeline_control_words = m_pipeline_delay_decode_words = 0;
	m_pipeline_control_address = 0;
	m_pipeline_control_word = 0;
	m_pipeline_bc_held = false;
	m_pipeline_bc_word = 0;
	m_pipeline_bc_address = 0;
	m_pipeline_bc_block_body = m_pipeline_bc_block_end = m_pipeline_bc_block_first_final = false;
	m_pipeline_retd_stage = 0;
	m_pipeline_retd_words.fill(0);
	m_pipeline_retd_addresses.fill(0);
	m_pipeline_retd_target = 0;
	m_pipeline_xc_active = false;
	m_pipeline_xc_end = 0;
	m_pipeline_xc_skip_words = 0;
	m_pipeline_far_return_pending = false;
	m_pipeline_long_store_pending = false;
	m_pipeline_program_move_pending = false;
	m_pipeline_coefficient_pending = false;
	m_pipeline_coefficient_source = 0;
	m_pipeline_repeat_coefficient_started = false;
	m_pipeline_repeat_active = m_pipeline_repeat_body_latched = m_pipeline_repeat_stream = false;
	m_pipeline_repeat_store_pending = m_pipeline_repeat_io_started = false;
	m_pipeline_repeat_io_value = 0;
	m_pipeline_repeat_store_address = 0;
	m_pipeline_repeat_issue_remaining = 0;
	m_pipeline_repeat_word = m_pipeline_repeat_source = m_pipeline_repeat_status = m_pipeline_repeat_sp = 0;
	m_pipeline_repeat_address = 0;
	m_pipeline_repeat_redirect_pending = false;
	m_pipeline_repeat_redirect_target = 0;
	m_pipeline_read_a_wait = 0;
	m_pipeline_write_a_wait = 0;
	m_pipeline_mvdp_wait = 0;
	m_pipeline_write_a_target = 0;
	m_pipeline_write_a_logical = 0;
	m_pipeline_read_a_source = 0;
	m_pipeline_memory_repeat_wait = 0;
	m_pipeline_repeat_program_source = 0;
	m_pipeline_dp_wait = 0;
	m_pipeline_cycles = 0;
}

void tms320c54x_device::reset_execution_pipeline()
{
	m_vector_prefetch_valid = false;
	m_interrupt_entry_stage = 0;
	reset_pipeline();
}

tms320c54x_device::pipeline_cycle_result tms320c54x_device::pipeline_service_bus_wait()
{
	if (m_pipeline_peripheral_read_pending)
	{
		// The older peripheral write used the preceding bus cycle. Retain
		// this Read slot until its own access completes, without re-executing
		// the older instruction or sampling the register twice.
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		m_pipeline_peripheral_read_pending = false;
		return pipeline_cycle_result::consumed;
	}
	if (software_wait_enabled())
	{
		bool const read = m_pipeline_valid[3] && staged_port_read(m_pipeline_word[3]);
		bool const write = m_pipeline_valid[4] && staged_port_write(m_pipeline_word[4]);
		if (read && write)
		{
			logerror("%s: overlapping staged external I/O arbitration is unsupported\n", machine().describe_context());
			m_illegal = true; return pipeline_cycle_result::rejected;
		}
		if (read || write)
		{
			unsigned const stage = read ? 3 : 4;
			if (!m_pipeline_io_wait_paid[stage])
			{
				m_pipeline_io_wait_paid[stage] = true;
				m_pipeline_io_wait_remaining = external_io_wait_states();
				m_pipeline_io_ready_required = m_pipeline_io_wait_remaining >= 2;
			}
			// Hold the retained slot before the actual I/O side effect. Clock
			// consumers continue; no new interrupt is accepted mid-transfer.
			if (m_pipeline_io_wait_remaining) { --m_pipeline_io_wait_remaining; return pipeline_cycle_result::consumed; }
			if (m_pipeline_io_ready_required && !m_ready_in_cb()) return pipeline_cycle_result::consumed;
			m_pipeline_io_ready_required = false;
		}
	}
	return pipeline_cycle_result::proceed;
}

tms320c54x_device::pipeline_cycle_result tms320c54x_device::pipeline_service_pending_transfer()
{
	if (m_pipeline_compiler_sp_wait) --m_pipeline_compiler_sp_wait;
	if (m_pipeline_dp_wait)
	{
		--m_pipeline_dp_wait;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_long_store_pending)
	{
		m_data.write_word(m_pipeline_operand_address[5] ^ 1, m_pipeline_immediate[5]);
		m_pipeline_long_store_pending = false;
		pipeline_accept_interrupt();
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_far_return_pending)
	{
		// Retain the second stack operand while the return holds Read.
		// This bus placement is a partial model of class 34, not measured
		// silicon phases. The first sample follows older Execute writes.
		m_pipeline_operand_address[4] = data_read(m_sp++);
		m_pipeline_far_return_pending = false;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_acc_transfer_pending)
	{
		// Classes 30A/B hold this transfer boundary; only FCALA owns
		// a second stack write. BACC/FBACC own no stack transaction.
		// Bus placement is a partial model, not measured silicon phases.
		if (staged_accumulator_far_call(m_pipeline_word[4])) data_write(m_pipeline_far_call_stack[4], m_pipeline_far_call_page[4], 4);
		m_pipeline_acc_transfer_pending = false;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_cc_pending)
	{
		// The retained nondelayed Class 31B model holds one clock on both
		// paths. Only a taken predicate owns a stack write; retain that
		// decision through the held clock.
		if (m_pipeline_immediate[4]) data_write(m_pipeline_operand_address[4], u16(m_pipeline_address[4] + 2), 4);
		m_pipeline_cc_pending = false;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_memory_repeat_wait)
	{
		--m_pipeline_memory_repeat_wait;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_mvdp_wait)
	{
		--m_pipeline_mvdp_wait;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_write_a_wait)
	{
		// Class 26's program-bus hold is partial stage placement. A direct
		// accumulator producer must publish before PAR is retained (7.5.10).
		if (m_pipeline_write_a_wait == 4)
		{
			m_pipeline_write_a_logical = u32(m_a) & (m_extended_program ? 0x7fffff : 0xffff);
			m_pipeline_write_a_target = physical_program_address(m_pipeline_write_a_logical);
		}
		--m_pipeline_write_a_wait;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_read_a_wait)
	{
		// READA samples A after older Execute publications, then holds the
		// program bus for its four extra clocks. Placement is partial.
		if (m_pipeline_read_a_wait == 4) m_pipeline_read_a_source = u32(m_a);
		if (!--m_pipeline_read_a_wait)
			m_pipeline_operand[4] = m_program.read_word(physical_program_address(m_pipeline_read_a_source));
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_program_move_pending)
	{
		// MVPD's extra program-bus clock holds Read and younger slots.
		// Retain the sampled word; exact bus phase is a partial model.
		u32 const source = (m_pipeline_address[4] & 0x7f0000) | m_pipeline_immediate[4];
		m_pipeline_operand[4] = m_program.read_word(physical_program_address(source));
		m_pipeline_program_move_pending = false;
		return pipeline_cycle_result::consumed;
	}
	if (m_pipeline_coefficient_pending)
	{
		// Class 8 owns an extra program-bus cycle. Hold the sampled X/Y
		// operands and captured physical coefficient address across it.
		m_pipeline_immediate[4] = m_program.read_word(m_pipeline_coefficient_source);
		m_pipeline_coefficient_pending = false;
		return pipeline_cycle_result::consumed;
	}
	return pipeline_cycle_result::proceed;
}

tms320c54x_device::pipeline_arbitration_result tms320c54x_device::pipeline_arbitrate_peripheral()
{
	bool const read_peripheral = m_pipeline_valid[3] &&
		((m_pipeline_operand_address[3] >= 0x20 && m_pipeline_operand_address[3] <= 0x5f &&
		(staged_memory_load(m_pipeline_word[3]) || staged_shifted_memory_load(m_pipeline_word[3], m_pipeline_immediate[3]) || staged_bitfield(m_pipeline_word[3]) || staged_port_write(m_pipeline_word[3]) || staged_memory_logic(m_pipeline_word[3]) || staged_rmw(m_pipeline_word[3]) || staged_mmr_load(m_pipeline_word[3]) || staged_memory_dp(m_pipeline_word[3]))) ||
		(m_pipeline_immediate[3] >= 0x20 && m_pipeline_immediate[3] <= 0x5f &&
		(staged_mvdd(m_pipeline_word[3]) || staged_parallel_load(m_pipeline_word[3]))));
	bool const write_peripheral = m_pipeline_valid[4] &&
		((m_pipeline_operand_address[4] >= 0x20 && m_pipeline_operand_address[4] <= 0x5f &&
		(staged_port_read(m_pipeline_word[4]) || staged_ram_stm(m_pipeline_word[4]) || staged_store(m_pipeline_word[4]) || staged_accumulator_mmr_store(m_pipeline_word[4]) || staged_rmw(m_pipeline_word[4]) || staged_data_copy(m_pipeline_word[4]) || staged_mvdd(m_pipeline_word[4]) || (m_pipeline_word[4] & 0xff00) == 0x7600)) || staged_bus_control(m_pipeline_word[4]) || (staged_timer_control(m_pipeline_word[4]) || staged_serial_control(m_pipeline_word[4]) || staged_clock_control(m_pipeline_word[4])));
	bool const move_peripheral = m_pipeline_valid[3] && staged_clock_destination_move(m_pipeline_word[3], m_pipeline_immediate[3]);
	// Class 19A/B owns its retained write one stage earlier than STM. Pay
	// the peripheral clock before advancing Access to that write stage.
	if (move_peripheral && (read_peripheral || write_peripheral))
	{
		logerror("%s: simultaneous staged CLKMD move/peripheral arbitration is unsupported\n", machine().describe_context());
		m_illegal = true; return {pipeline_cycle_result::rejected, false};
	}
	// The boot echo loop can read the previous BDRR while BDXR accepts
	// another retry. These independent serial endpoints retain one access
	// wait apiece; neither access synthesizes a serial-ready edge. Other
	// peripheral conflicts still require their own ordering qualification.
	bool const serial_retry_pair = read_peripheral && write_peripheral &&
		m_pipeline_word[4] == 0x7721 && (m_pipeline_word[3] & 0xfeff) == 0x4820;
	bool const clock_read_after_write = read_peripheral && write_peripheral &&
		m_pipeline_word[4] == 0x7758 && (m_pipeline_word[3] & 0xfeff) == 0x4858;
	if (read_peripheral && write_peripheral && !serial_retry_pair && !clock_read_after_write)
	{
		logerror("%s: simultaneous staged peripheral read/write arbitration is unsupported read_pc=%06x read_op=%04x read_address=%04x write_pc=%06x write_op=%04x write_address=%04x read_wait_paid=%u write_wait_paid=%u\n",
			machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_operand_address[3],
			unsigned(m_pipeline_address[4]), m_pipeline_word[4],
			staged_serial_control(m_pipeline_word[4]) ? u8(m_pipeline_word[4]) : m_pipeline_operand_address[4],
			unsigned(m_pipeline_peripheral_wait_paid[3]), unsigned(m_pipeline_peripheral_wait_paid[4]));
		m_illegal = true;
		return {pipeline_cycle_result::rejected, false};
	}
	unsigned const access_stage = (serial_retry_pair || clock_read_after_write) && !m_pipeline_peripheral_wait_paid[4] ? 4 : read_peripheral || move_peripheral ? 3 : 4;
	if ((read_peripheral || write_peripheral || move_peripheral) && !m_pipeline_peripheral_wait_paid[access_stage])
	{
		// SPRU172C establishes one extra peripheral-access clock. Holding
		// pipeline advancement at the bus access is this prototype's declared
		// latency model, not independently recovered silicon stage placement.
		m_pipeline_peripheral_wait_paid[access_stage] = true;
		return {pipeline_cycle_result::consumed, false};
	}
	return {pipeline_cycle_result::proceed, clock_read_after_write};
}

void tms320c54x_device::pipeline_step()
{
	++m_pipeline_cycles;
	if (pipeline_service_bus_wait() != pipeline_cycle_result::proceed) return;
	if (m_pipeline_repeat_stream)
	{
		if (staged_repeat_port(m_pipeline_repeat_word))
		{
			pipeline_repeat_port();
			return;
		}
		// ST #lk is not in SPRU131G's single-cycle repeat acceleration table:
		// retain a separate address and write clock for each later iteration.
		bool const immediate_store = (m_pipeline_repeat_word & 0xff00) == 0x7600;
		if (immediate_store && !m_pipeline_repeat_store_pending)
		{
			u8 const mode = m_pipeline_repeat_word;
			m_pipeline_repeat_store_address = BIT(mode, 7) ? short_smem_address(mode) : BIT(m_pipeline_repeat_status, 14)
				? u16(m_pipeline_repeat_sp + mode) : u16((m_pipeline_repeat_status & 0x1ff) << 7) | mode;
			if (m_pipeline_repeat_store_address < 0x60) { logerror("%s: repeated immediate store register/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			if (BIT(mode, 7)) indirect_modify(mode);
			m_pipeline_repeat_store_pending = true;
			return;
		}
		// Accelerated iterations execute outside the ordinary pipeline slot.
		if (!m_instruction_observer_cb.isunset()) m_instruction_observer_cb(m_pipeline_repeat_address, m_pipeline_repeat_word);
		++m_rpt_iteration;
		if (immediate_store)
		{
			data_write(m_pipeline_repeat_store_address, m_pipeline_repeat_source);
			m_pipeline_repeat_store_pending = false;
		}
		else if ((m_pipeline_repeat_word & 0xfe00) == 0x1e00)
		{
			u8 const mode = m_pipeline_repeat_word;
			bool const preincrement = BIT(mode, 7) && (mode & 0x78) == 0x18;
			if (preincrement) indirect_modify(mode);
			u16 const address = BIT(mode, 7) ? short_smem_address(mode) : BIT(m_pipeline_repeat_status, 14)
				? u16(m_pipeline_repeat_sp + mode) : u16((m_pipeline_repeat_status & 0x1ff) << 7) | mode;
			// SUBC never writes T/TRN, so those mapped sources are plain per-issue reads.
			if (address < 0x60 && address != 0x0e && address != 0x0f) { logerror("%s: repeated SUBC register/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			u16 const value = m_data.read_word(address);
			u64 &source = accumulator(BIT(m_pipeline_repeat_word, 8));
			source = conditional_subtract(source, value, BIT(m_pipeline_repeat_word, 8));
			if (BIT(mode, 7) && !preincrement) indirect_modify(mode);
		}
		else if (staged_repeat_accumulator_load(m_pipeline_repeat_word))
		{
			if ((m_pipeline_repeat_word & 0xfce0) == 0xf460)
				execute_register_effects(m_pipeline_repeat_word);
			else
			{
				// LD src,SHIFT/ASM,dst remains Execute-live on every iteration;
				// only the fetched instruction is retained, not its source value.
				unsigned const code = (m_pipeline_repeat_word & 0xfcff) == 0xf482 ? m_st1 & 31 : m_pipeline_repeat_word & 31;
				accumulator(BIT(m_pipeline_repeat_word, 8)) = shifted_load(accumulator(BIT(m_pipeline_repeat_word, 9)), code >= 16 ? int(code) - 32 : int(code), BIT(m_pipeline_repeat_word, 8));
			}
		}
		else if (staged_data_to_program(m_pipeline_repeat_word))
		{
			u8 const mode = m_pipeline_repeat_word;
			u16 const source = BIT(mode, 7) ? short_smem_address(mode) : BIT(m_pipeline_repeat_status, 14)
				? u16(m_pipeline_repeat_sp + mode) : u16((m_pipeline_repeat_status & 0x1ff) << 7) | mode;
			if (source < 0x60) { logerror("%s: repeated MVDP register/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			u16 const value = data_read(source);
			if (BIT(mode, 7)) indirect_modify(mode);
			u32 const destination = (m_pipeline_repeat_address & 0x7f0000) | u16(m_pipeline_repeat_source + m_rpt_iteration);
			m_program.write_word(physical_program_address(destination), value);
		}
		else if (staged_write_a(m_pipeline_repeat_word))
		{
			u8 const mode = m_pipeline_repeat_word;
			u16 const source = BIT(mode, 7) ? short_smem_address(mode) : BIT(m_pipeline_repeat_status, 14)
				? u16(m_pipeline_repeat_sp + mode) : u16((m_pipeline_repeat_status & 0x1ff) << 7) | mode;
			if (source < 0x60) { logerror("%s: repeated WRITA register/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			u16 const value = data_read(source);
			if (BIT(mode, 7)) indirect_modify(mode);
			m_program.write_word(physical_program_address(m_pipeline_repeat_program_source + m_rpt_iteration), value);
		}
		else if (m_pipeline_repeat_word != 0xf495)
		{
			u8 const mode = m_pipeline_repeat_word;
			bool const preincrement = BIT(mode, 7) && (mode & 0x78) == 0x18;
			if (preincrement) indirect_modify(mode);
			u16 const destination = BIT(mode, 7) ? short_smem_address(mode) : BIT(m_pipeline_repeat_status, 14)
				? u16(m_pipeline_repeat_sp + mode) : u16((m_pipeline_repeat_status & 0x1ff) << 7) | mode;
			if (destination < 0x60) { logerror("%s: repeated MVPD register/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			u32 const source = staged_read_a(m_pipeline_repeat_word) ? m_pipeline_repeat_program_source + m_rpt_iteration :
				(m_pipeline_repeat_address & 0x7f0000) | u16(m_pipeline_repeat_source + m_rpt_iteration);
			u16 const value = m_program.read_word(physical_program_address(source));
			data_write(destination, value);
			if (BIT(mode, 7) && !preincrement) indirect_modify(mode);
		}
		pipeline_finish_repeat_iteration();
		return;
	}
	if (pipeline_service_pending_transfer() != pipeline_cycle_result::proceed) return;
	auto const peripheral = pipeline_arbitrate_peripheral();
	if (peripheral.cycle != pipeline_cycle_result::proceed) return;
	u8 const entry_stage = m_interrupt_entry_stage;
	bool prefetch_bubble = false;
	bool late_popm_read = false;
	u16 late_popm_address = 0;
	bool late_pushm_read = false;
	u16 late_pushm_address = 0;
	// Fixed indices let the compiler combine field moves without per-array
	// memmove calls. Preserve all slots, including invalid ones, and their order.
	auto const advance_stage = [this](unsigned stage)
	{
		m_pipeline_valid[stage] = m_pipeline_valid[stage - 1];
		m_pipeline_block_first_final[stage] = m_pipeline_block_first_final[stage - 1];
		m_pipeline_block_end[stage] = m_pipeline_block_end[stage - 1];
		m_pipeline_block_body[stage] = m_pipeline_block_body[stage - 1];
		m_pipeline_address[stage] = m_pipeline_address[stage - 1];
		m_pipeline_physical_address[stage] = m_pipeline_physical_address[stage - 1];
		m_pipeline_word[stage] = m_pipeline_word[stage - 1];
		m_pipeline_annulled[stage] = m_pipeline_annulled[stage - 1];
		m_pipeline_operand_address[stage] = m_pipeline_operand_address[stage - 1];
		m_pipeline_operand[stage] = m_pipeline_operand[stage - 1];
		m_pipeline_immediate[stage] = m_pipeline_immediate[stage - 1];
		m_pipeline_extension_first[stage] = m_pipeline_extension_first[stage - 1];
		m_pipeline_early_write_deferred[stage] = m_pipeline_early_write_deferred[stage - 1];
		m_pipeline_peripheral_wait_paid[stage] = m_pipeline_peripheral_wait_paid[stage - 1];
		m_pipeline_io_wait_paid[stage] = m_pipeline_io_wait_paid[stage - 1];
		m_pipeline_intm_protected[stage] = m_pipeline_intm_protected[stage - 1];
		m_pipeline_direct_status[stage] = m_pipeline_direct_status[stage - 1];
		m_pipeline_alu_mode[stage] = m_pipeline_alu_mode[stage - 1];
		m_pipeline_t[stage] = m_pipeline_t[stage - 1];
		m_pipeline_cmpt[stage] = m_pipeline_cmpt[stage - 1];
		m_pipeline_bk[stage] = m_pipeline_bk[stage - 1];
		m_pipeline_direct_sp[stage] = m_pipeline_direct_sp[stage - 1];
		m_pipeline_far_call_page[stage] = m_pipeline_far_call_page[stage - 1];
		m_pipeline_far_call_stack[stage] = m_pipeline_far_call_stack[stage - 1];
	};
	advance_stage(5);
	advance_stage(4);
	advance_stage(3);
	advance_stage(2);
	advance_stage(1);
	m_pipeline_valid[0] = false;
	m_pipeline_annulled[0] = false;
	m_pipeline_early_write_deferred[0] = false;
	m_pipeline_peripheral_wait_paid[0] = false;
	m_pipeline_io_wait_paid[0] = false;
	m_pipeline_intm_protected[0] = false;
	if (m_pipeline_flush_front) m_pipeline_valid[2] = false;
	if (m_pipeline_decode_flush)
	{
		m_pipeline_valid[2] = false;
		--m_pipeline_decode_flush;
	}
	bool repeated_literal = false;
	if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched &&
		staged_pipelined_repeat(m_pipeline_repeat_word) && m_pipeline_repeat_issue_remaining)
	{
		// Each copy owns real pipeline slots; Read of iteration n precedes
		// Execute's data write for iteration n-1 in the same clock.
		m_pipeline_valid[2] = true;
		m_pipeline_word[2] = m_pipeline_repeat_word;
		repeated_literal = staged_repeat_advancing_literal(m_pipeline_repeat_word) || staged_repeat_extension_shift(m_pipeline_repeat_word);
		if (staged_repeat_advancing_literal(m_pipeline_repeat_word)) m_pipeline_immediate[2] = m_pipeline_operand[2] = m_pipeline_repeat_source++;
		else if (repeated_literal) m_pipeline_immediate[2] = m_pipeline_repeat_source;
		m_pipeline_address[2] = m_pipeline_repeat_address;
		m_pipeline_block_body[2] = m_pipeline_block_end[2] = m_pipeline_block_first_final[2] = false;
		m_pipeline_peripheral_wait_paid[2] = m_pipeline_io_wait_paid[2] = false;
		--m_pipeline_repeat_issue_remaining;
	}

	// SPRU131G example 7-18: INTR occupies Decode/Access/Read/Execute
	// while older decoded foreground instructions continue to Execute.
	if (entry_stage)
	{
		if (entry_stage == 1) m_pc = m_pipeline_return_pc;
		interrupt_entry_step();
	}
	// INTR's vector Prefetch precedes this clock's older Execute writes.
	// This preserves the IPTR setup latency in SPRU131G table 7-26.
	bool const software_intr_prefetch = m_pipeline_valid[3] && staged_software_intr(m_pipeline_word[3]);
	u32 const software_intr_target = software_intr_prefetch ? (m_pipeline_address[3] & 0x7f0000) |
		(m_pmst & 0xff80) | ((m_pipeline_word[3] & 31) << 2) : 0;
	u32 const software_intr_physical = software_intr_prefetch ? physical_program_address(software_intr_target) : 0;
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
			m_pipeline_word[1] = m_cache.read_word(m_pipeline_physical_address[1]);
		}
	}
	if (m_pipeline_valid[3] && staged_banz(m_pipeline_word[3]))
	{
		// Class 29A short forms test AR before modification. Long forms
		// test the resolved offset address, with MOD 13/14 premodification.
		// Undelayed redirects discard fall-through words; delayed redirects
		// preserve both delay words. Placement remains a partial timing model.
		u8 const mode = m_pipeline_word[3];
		bool const extended = staged_extended_banz(m_pipeline_word[3]);
		bool const taken = (extended ? long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_operand_address[3]), 3) : m_ar[indirect_ar(mode, 3)]) != 0;
		m_pipeline_immediate[3] = taken;
		if (!extended) indirect_modify(mode, false, 3);
		if (taken)
		{
			if (!BIT(m_pipeline_word[3], 9))
			{
				m_pipeline_valid[2] = false;
				m_pipeline_decode_flush = 1;
			}
			m_pipeline_fetch_address = (m_pipeline_address[3] & 0x7f0000) | m_pipeline_operand[3];
			if (m_pipeline_block_body[3]) m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
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
		m_pipeline_bc_block_body = m_pipeline_block_body[2];
		m_pipeline_bc_block_end = m_pipeline_block_end[2];
		m_pipeline_bc_block_first_final = m_pipeline_block_first_final[2];
		m_pipeline_valid[2] = false;
	}
	if (m_pipeline_valid[4] && staged_conditional_branch(m_pipeline_word[4]) && !BIT(m_pipeline_word[4], 9))
	{
		bool const overflow = (m_pipeline_word[4] & 0xfde7) == 0xf860;
		u16 const overflow_mask = 1U << (10 - BIT(m_pipeline_word[4], 3));
		bool const taken = staged_bio_branch(m_pipeline_word[4])
			? (BIT(m_pipeline_word[4], 0) ? !m_bio_in_cb() : bool(m_bio_in_cb())) : staged_tc_branch(m_pipeline_word[4])
			? BIT(m_st0, 12) == BIT(m_pipeline_word[4], 4) : (m_pipeline_word[4] & 0xfdfb) == 0xf808
			? BIT(m_st0, 11) == BIT(m_pipeline_word[4], 2) : overflow ? bool(m_st0 & overflow_mask) == BIT(m_pipeline_word[4], 4) : staged_accumulator_condition(m_pipeline_word[4], accumulator(BIT(m_pipeline_word[4], 3)));
		if (overflow) m_st0 &= ~overflow_mask;
		m_pipeline_immediate[4] = taken;
		if (taken)
		{
			m_pipeline_valid[2] = false;
			m_pipeline_decode_flush = 1; // Discard the already fetched i5 next clock.
			m_pipeline_fetch_address = (m_pipeline_address[4] & 0x7f0000) | m_pipeline_operand[4];
			// Discard endpoint predictions belonging to the annulled path.
			// Decode still owns BRC; the redirected path starts in this iteration.
			if (m_pipeline_block_body[4]) m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
		}
		else if (m_pipeline_bc_held)
		{
			m_pipeline_valid[2] = true;
			m_pipeline_word[2] = m_pipeline_bc_word;
			m_pipeline_address[2] = m_pipeline_bc_address;
			m_pipeline_block_body[2] = m_pipeline_bc_block_body;
			m_pipeline_block_end[2] = m_pipeline_bc_block_end;
			m_pipeline_block_first_final[2] = m_pipeline_bc_block_first_final;
		}
		m_pipeline_bc_held = false;
	}
	if (m_pipeline_valid[4] && staged_bio_branch(m_pipeline_word[4]) && BIT(m_pipeline_word[4], 9))
		// BIO is a physical line, sampled once in Read for BC and BCD
		// (SPRU131G 8.3.1), not at later delay-word retirement.
		m_pipeline_immediate[4] = BIT(m_pipeline_word[4], 0) ? !m_bio_in_cb() : bool(m_bio_in_cb());
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xfcf8) == 0xf4a8)
	{
		// Category-I CMPR reads ARx/AR0 in Access, before older Read or
		// Execute publications (SPRU131G table 7-7).
		u16 const word = m_pipeline_word[3];
		u16 const left = m_ar[word & 7], right = m_ar[0];
		bool const conditions[] = {left == right, left < right, left > right, left != right};
		m_pipeline_operand[3] = conditions[(word >> 8) & 3];
	}
	// Example 7-13 samples XC in Access, before the next word's Decode
	// and before older Execute can change TC in this clock.
	if (m_pipeline_valid[3] && staged_xc(m_pipeline_word[3]))
	{
		u16 const word = m_pipeline_word[3];
		bool execute = (word & 0xfe) == 2 ? (bool(m_bio_in_cb()) != BIT(word, 0)) : BIT(word, 6) ? (word & 7) == 0 || staged_accumulator_condition(word, accumulator(BIT(word, 3))) :
			BIT(word, 5) ? BIT(m_st0, 12) == BIT(word, 4) : BIT(word, 3) ? BIT(m_st0, 11) == BIT(word, 2) : true;
		if (BIT(word, 6) && BIT(word, 5))
		{
			// As for BC and CC, testing OVA/OVB consumes it even when false.
			u16 const overflow = 1U << (10 - BIT(word, 3));
			execute = execute && bool(m_st0 & overflow) == BIT(word, 4);
			m_st0 &= ~overflow;
		}
		m_pipeline_operand[3] = execute;
		if (!execute) m_pipeline_xc_skip_words = BIT(m_pipeline_word[3], 9) ? 2 : 1;
	}
	if (m_pipeline_valid[2] && m_pipeline_xc_skip_words)
	{
		m_pipeline_word[2] = 0xf495; // Rejected words are NOPs, not decoded operands.
		m_pipeline_annulled[2] = true;
		--m_pipeline_xc_skip_words;
	}
	bool const decoding_control_delay = m_pipeline_valid[2] && m_pipeline_delay_decode_words;
	bool const completing_shift_delay = m_pipeline_valid[2] && m_pipeline_delay_decode_words &&
		m_pipeline_decode_remaining == 1 && staged_shifted_smem(m_pipeline_decode_word);
	bool const completing_move_delay = m_pipeline_valid[2] && m_pipeline_delay_decode_words &&
		m_pipeline_decode_remaining == 1 && staged_move(m_pipeline_decode_word);
	bool const completing_t_store_delay = m_pipeline_valid[2] && m_pipeline_delay_decode_words &&
		m_pipeline_decode_remaining == 1 && staged_extended_store(m_pipeline_decode_word) && (m_pipeline_decode_word & 0xfe00) == 0x8c00;
	if (m_pipeline_valid[2])
	{
		if (!m_pipeline_decode_remaining && (invalid_mmr_long_form(m_pipeline_word[2]) ||
			(m_pipeline_repeat_active && extended_repeat_transfer(m_pipeline_word[2]))))
		{
			logerror("%s: unsupported staged MMR or repeated transfer addressing\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (m_pipeline_delay_decode_words)
		{
			u16 const word = m_pipeline_word[2];
			if (!m_pipeline_decode_remaining && (staged_extended_push(word) || staged_extended_pop(word)))
			{
				logerror("%s: staged delay extended stack transfer is unsupported\n", machine().describe_context()); m_illegal = true; return;
			}
			if (!m_pipeline_decode_remaining && (word & 0xfeff) == 0x881d)
			{
				// PMST changes in a delayed span need their own mapping-latency
				// composition; the ordinary store is covered independently.
				logerror("%s: staged delay PMST store is unsupported\n", machine().describe_context()); m_illegal = true; return;
			}
			bool const delay_store = staged_extended_store(word);
			bool const delay_port = staged_repeat_port(word) && m_pipeline_delay_decode_words == 2 &&
				staged_conditional_branch(m_pipeline_control_word) && !staged_conditional_call(m_pipeline_control_word);
			bool const delay_load = staged_offset_load(word) && (word & 0xfe00) == 0x1000;
			bool const delay_dld = staged_extended_dld(word);
			// These two-word delay spans can hold one absolute DST. Its retained
			// address/source and two write clocks use the ordinary DST path.
			bool const delay_dst = staged_extended_dst(word) && u8(word) == 0xf8 &&
				(m_pipeline_control_word == 0xfe00 || m_pipeline_control_word == 0xf272 || m_pipeline_control_word == 0xf273 || m_pipeline_control_word == 0xf274 ||
				 (staged_banz(m_pipeline_control_word) && !staged_extended_banz(m_pipeline_control_word) && BIT(m_pipeline_control_word,9)));
			bool const delay_asm = (word & 0xff00) == 0x3200 &&
				(u8(word) < 0xa0 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
			bool const delay_shift = staged_shifted_smem(word) && u8(word) < 0x98;
			bool const delay_multiply = ((word & 0xfc00) == 0x2000 || (word & 0xf800) == 0x2800) &&
				(u8(word) < 0xa0 || (u8(word) >= 0xe0 && u8(word) <= 0xf8));
			// MPY/MAC Smem,#lk fill a two-word delay; T and the literal are retained in Read.
			bool const delay_immediate_multiply = ((word & 0xfe00) == 0x6200 || (word & 0xfc00) == 0x6400) && u8(word) < 0x98 && m_pipeline_delay_decode_words == 2;
			// MPYA/MASA/MACA/MACAR publish Smem to T in Read;
			// their independently owned B result retires before redirect.
			bool const delay_accumulator_multiply = ((word & 0xf9ff) == (0x3100 | u8(word))) &&
				(u8(word) < 0x98 || (m_pipeline_control_word == 0xf273 && ((word & 0xf8) == 0xe0 || u8(word) == 0xf8)));
			bool delay_compare = false;
			bool delay_bitt = staged_bitt(word) && u8(word) < 0x98;
			bool delay_arithmetic = false;
			bool const delay_square = (square_accumulate(word) || (word & 0xfe00) == 0x2600) && u8(word) < 0x98;
			bool const delay_t_load = (word & 0xff00) == 0x3000 && u8(word) < 0x98;
			bool delay_repeat = false;
			u32 delay_repeat_target = 0;
			bool delayed_block_owner = false;
			for (unsigned stage = 3; stage < 6; ++stage)
				if (m_pipeline_valid[stage] && m_pipeline_address[stage] == m_pipeline_control_address)
				{
					delayed_block_owner = m_pipeline_word[stage] == 0xf272;
					bool const conditional_repeat_owner = staged_accumulator_branch(m_pipeline_word[stage]) && BIT(m_pipeline_word[stage], 9);
					delay_repeat = staged_short_repeat(word) && (m_pipeline_word[stage] == 0xf273 || m_pipeline_word[stage] == 0xf274 || m_pipeline_word[stage] == 0xfe00 || delayed_block_owner || conditional_repeat_owner) &&
						m_pipeline_delay_decode_words == 2 && !m_pipeline_repeat_active &&
						(!m_pipeline_block_active || delayed_block_owner);
					if (delay_repeat) delay_repeat_target = (m_pipeline_control_address & 0x7f0000) | (conditional_repeat_owner ? u16(m_pipeline_control_address + 4) : m_pipeline_operand[stage]);
					delay_bitt |= staged_bitt(word) && u8(word) >= 0xe0 && m_pipeline_word[stage] == 0xfe00;
					bool const basic_arithmetic = (word & 0xfe00) == 0x0000 || (word & 0xfe00) == 0x0800 || staged_unsigned_memory_arithmetic(word);
					delay_arithmetic = basic_arithmetic && u8(word) >= 0xe0 && u8(word) <= 0xf8 && (m_pipeline_word[stage] & 0xfff7) == 0xfa45;
				}
			// SPRU131G 7.5.8.1 forbids BRC modification in RPTBD delay slots.
			// Reject these direct writers before Access; indirect aliases remain
			// outside this bounded contract, not claimed silicon traps.
			if (!m_pipeline_decode_remaining && delayed_block_owner && (word == 0x771a || (word & 0xfeff) == 0x881a || word == 0x8a1a))
			{
				logerror("%s: staged RPTBD delay BRC write is unsupported\n", machine().describe_context()); m_illegal = true; return;
			}
			// Recover the retained near-branch owner; calls/returns remain
			// excluded, without a program reread or additional saved state.
			if (staged_bitfield(word) && !staged_extended_bitfield(word))
				for (unsigned stage = 3; stage < 6; ++stage)
					if (m_pipeline_valid[stage] && m_pipeline_address[stage] == m_pipeline_control_address)
						delay_compare = m_pipeline_word[stage] == 0xf273 ||
							((m_pipeline_word[stage] & 0xfe00) == 0xfa00 && staged_conditional_branch(m_pipeline_word[stage]) && !staged_conditional_call(m_pipeline_word[stage]));
			if (!m_pipeline_decode_remaining && staged_software_intr(word)) { logerror("%s: staged software INTR in a control delay is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			if (!m_pipeline_decode_remaining && (staged_conditional_return(word) || staged_delayed_conditional_return(word))) { logerror("%s: staged RC inside a delay slot is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			if (!m_pipeline_decode_remaining && staged_data_to_program(word)) { logerror("%s: staged MVDP inside a delay slot is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			if (!m_pipeline_decode_remaining && staged_write_a(word)) { logerror("%s: staged WRITA inside a delay slot is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			if (!m_pipeline_decode_remaining && staged_accumulator_transfer(word)) { logerror("%s: staged accumulator transfer inside a delay slot is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			if (!m_pipeline_decode_remaining && !delay_repeat && !delay_t_load && !delay_store && !delay_load && !delay_dld && !delay_dst && !delay_asm && !delay_shift && !delay_multiply && !delay_immediate_multiply && !delay_accumulator_multiply && !delay_compare && !delay_bitt && !delay_arithmetic && !delay_square && ((word & 0xfe00) == 0x1e00 || staged_immediate_multiply(word) || staged_shifted_smem(word) || staged_smem_special(word) || staged_extended_dld(word) || staged_offset_load(word) || (staged_extended_logic(word) && !staged_absolute_logic(word)) || staged_bitfield(word) || staged_extended_smem_arithmetic(word) || staged_extended_rmw(word) || staged_extended_move(word) || staged_extended_dst(word) || (staged_data_copy(word) && !staged_two_word_data_copy(word)) || staged_long_arithmetic(word) || staged_extended_immediate_store(word) || staged_short_repeat(word)))
			{
				logerror("%s: staged long-memory operation in a control delay is unsupported: word=%04x address=%06x decode_words=%u control=%06x control_words=%u cycle=%llu\n",
					machine().describe_context(), word, m_pipeline_address[2], m_pipeline_delay_decode_words,
					m_pipeline_control_address, m_pipeline_control_words, (unsigned long long)m_pipeline_cycles);
				for (unsigned stage = 0; stage < 6; ++stage)
					logerror("staged_control_delay_slot: stage=%u valid=%u address=%06x word=%04x\n", stage, unsigned(m_pipeline_valid[stage]), m_pipeline_address[stage], m_pipeline_word[stage]);
				m_illegal = true; return;
			}
			unsigned const words = staged_absolute_add(word) || delay_store || delay_port || delay_load || delay_dld || delay_dst || delay_shift || delay_compare || (delay_bitt && u8(word) >= 0xe0) || delay_arithmetic || ((delay_multiply || delay_asm || delay_accumulator_multiply) && u8(word) >= 0xe0) || word == 0x46f8 || staged_two_word_data_copy(word) ? 2 : ((staged_rmw(word) && u8(word) == 0xf8) || word == 0x76f8) ? 3 :
				(staged_far_call(word) || (staged_absolute_load(word) || staged_extended_logic(word)) || staged_far_branch(word) || staged_long_mar(word) || staged_move(word) || staged_call(word) || staged_conditional_branch(word) || staged_immediate_alu(word) || (word & 0xfef0) == 0xf020 || (word & 0xfeff) == 0xf062 || staged_rmw(word) || (word & 0xff00) == 0x7700 || (word & 0xff00) == 0x7600 || word == 0xf073 || word == 0xf273) ? 2 : 1;
			bool const delay_read_a = staged_read_a(word) && u8(word) < 0x98 && m_pipeline_control_word == 0xf273;
			if (!m_pipeline_decode_remaining && (words > m_pipeline_delay_decode_words || word == 0x7707 || staged_block_repeat(word) || ((staged_port_read(word) || staged_port_write(word)) && !delay_port) || staged_memory_repeat(word) || staged_banz(word) || (staged_read_a(word) && !delay_read_a) || staged_immediate_repeat(word) || staged_program_move(word) || staged_idle(word) || staged_accumulator_transfer(word) || staged_far_call(word) || staged_far_branch(word) || staged_call(word) || staged_conditional_branch(word) || word == 0xfc00 || word == 0xf4eb || word == 0xfe00 || word == 0xf49b || word == 0xf69b || (word & 0xfdfe) == 0xf4e4 || word == 0xf073 || word == 0xf273))
			{
				logerror("%s: unsupported staged control delay form %04x instruction=%06x control=%06x owner=%04x remaining=%u\n", machine().describe_context(), word, unsigned(m_pipeline_address[2]), unsigned(m_pipeline_control_address), m_pipeline_control_word, m_pipeline_delay_decode_words);
				m_illegal = true;
				return;
			}
			if (delay_repeat && !delayed_block_owner)
			{
				m_pipeline_repeat_redirect_pending = true;
				m_pipeline_repeat_redirect_target = delay_repeat_target;
			}
			--m_pipeline_delay_decode_words;
		}
		if (repeated_literal)
		{
			// The injected iteration already owns its incremented literal;
			// it must not decode or fetch an extension a second time.
		}
		else if (m_pipeline_decode_remaining)
		{
			// Extension words belong to the retained opcode, not independently
			// executable instructions. Absolute RMW/ST use address, then literal.
			if (--m_pipeline_decode_remaining)
			{
				if (m_pipeline_block_end[2]) { logerror("%s: staged block endpoint inside extension words\n", machine().describe_context()); m_illegal = true; return; }
				m_pipeline_decode_extension = m_pipeline_word[2];
				m_pipeline_valid[2] = false;
			}
			else
			{
				m_pipeline_operand[2] = m_pipeline_immediate[2] = m_pipeline_word[2];
				m_pipeline_block_first_final[2] = m_pipeline_decode_block_first_final;
				m_pipeline_block_body[2] = m_pipeline_decode_block_body;
				m_pipeline_operand_address[2] = m_pipeline_decode_extension;
				m_pipeline_extension_first[2] = m_pipeline_decode_extension;
				m_pipeline_word[2] = m_pipeline_decode_word;
				m_pipeline_address[2] = m_pipeline_decode_address;
			}
		}
		else if ((staged_opcode_descriptor[m_pipeline_word[2]] & 3) || staged_bus_control(m_pipeline_word[2]))
		{
			m_pipeline_decode_remaining = (staged_opcode_descriptor[m_pipeline_word[2]] & 3) ? (staged_opcode_descriptor[m_pipeline_word[2]] & 3) : 1;
			m_pipeline_decode_extension = 0;
			m_pipeline_decode_word = m_pipeline_word[2];
			m_pipeline_decode_address = m_pipeline_address[2];
			m_pipeline_decode_block_first_final = m_pipeline_block_first_final[2];
			m_pipeline_decode_block_body = m_pipeline_block_body[2];
			if (m_pipeline_block_end[2]) { logerror("%s: staged block endpoint inside multiword instruction\n", machine().describe_context()); m_illegal = true; return; }
			if (staged_immediate_repeat(m_pipeline_word[2]) || m_pipeline_word[2] == 0x47f8)
			{
				if (m_pipeline_repeat_active) { logerror("%s: nested staged repeat is unsupported\n", machine().describe_context()); m_illegal = true; return; }
				m_pipeline_repeat_active = true;
			}
			m_pipeline_valid[2] = false;
		}
		u16 const word = m_pipeline_word[2];
		if (m_pipeline_valid[2] && staged_program_coefficient(word) &&
			(m_pipeline_xc_active || m_pipeline_block_body[2] || m_pipeline_control_words ||
			 (m_pipeline_repeat_active && !staged_pipelined_repeat(word))))
		{
			logerror("%s: staged program-coefficient repeat/control composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (m_pipeline_valid[2] && staged_extended_store(word) && (word & 0xfe00) == 0x8c00 &&
			(m_pipeline_repeat_active || m_pipeline_xc_active || m_pipeline_block_body[2] || completing_t_store_delay))
		{
			logerror("%s: staged extended T/TRN store repeat/control composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		// Admit the complete shifted store only for the tested RPTBD and BD
		// owners; other delayed transfers retain their independent exclusions.
		bool delay_store = false;
		if (completing_shift_delay && staged_shifted_memory_store(word, m_pipeline_immediate[2]))
		{
			delay_store = m_pipeline_control_words && m_pipeline_control_word == 0xf273;
			for (unsigned stage = 3; stage < 6; ++stage)
				if (m_pipeline_valid[stage] && m_pipeline_address[stage] == m_pipeline_control_address && m_pipeline_word[stage] == 0xf272)
					delay_store = true;
		}
		if (completing_shift_delay && m_pipeline_valid[2] && (m_pipeline_immediate[2] & 0xfee0) != 0x0c40 && !staged_shifted_memory_arithmetic(m_pipeline_immediate[2]) && !delay_store)
		{
			logerror("%s: staged control-delay Smem extension is not an admitted shifted load instruction=%06x word=%04x operation=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[2]), word, m_pipeline_immediate[2]); m_illegal = true; return;
		}
		bool const software_intr = staged_software_intr(word);
		if (m_pipeline_valid[2] && staged_shifted_smem(word) && (word & 0xf8) == 0x98 && ((m_pipeline_immediate[2] & 0xfee0) == 0x0c40 || staged_shifted_memory_arithmetic(m_pipeline_immediate[2])))
		{
			logerror("%s: short MOD=3 is write-only, not a shifted memory source\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const short_repeat = staged_short_repeat(word);
		if (m_pipeline_valid[2] && (short_repeat || (staged_memory_repeat(word) && word != 0x47f8)))
		{
			if (m_pipeline_repeat_active) { logerror("%s: nested staged repeat is unsupported\n", machine().describe_context()); m_illegal = true; return; }
			m_pipeline_repeat_active = true;
			if (short_repeat) m_pipeline_immediate[2] = u8(word);
		}
		// Register/immediate loads, short loads, DP stores and bounded RMW forms.
		// Other memory/control forms need Access/Read/redirect ownership first.
		bool const short_load = staged_memory_load(word);
		bool const direct_store = staged_store(word) || staged_accumulator_mmr_store(word);
		bool const early_braf_clear = word == 0x7707 && pipeline_early_braf_clear(2, m_pipeline_immediate[2]);
		if (m_pipeline_valid[2] && ((word & 0xfeff) == 0x8807 || word == 0x7707) &&
			(m_pipeline_delay_decode_words || m_pipeline_repeat_active || m_pipeline_xc_active || (m_pipeline_block_body[2] && !early_braf_clear)))
		{
			logerror("%s: staged STLM ST1 control/repeat composition is unsupported instruction=%06x word=%04x delay=%u repeat=%u xc=%u xc_end=%06x block=%u body=%u range=%06x..%06x\n", machine().describe_context(), unsigned(m_pipeline_address[2]), word, m_pipeline_delay_decode_words, unsigned(m_pipeline_repeat_active), unsigned(m_pipeline_xc_active), unsigned(m_pipeline_xc_end), unsigned(m_pipeline_block_active), unsigned(m_pipeline_block_body[2]), unsigned(m_pipeline_block_start), unsigned(m_pipeline_block_end_address)); m_illegal = true; return;
		}
		if (m_pipeline_valid[2] && staged_block_repeat(word))
			for (unsigned stage = 3; stage < 6; ++stage)
				if (m_pipeline_valid[stage] && (m_pipeline_word[stage] & 0xfeff) == 0x8807 && m_pipeline_address[2] == ((m_pipeline_address[stage] & 0x7f0000) | u16(m_pipeline_address[stage] + 1)))
				{
					// SPRU131G 7.5.7.1: STLM can overwrite the younger BRAF update.
					logerror("%s: staged adjacent STLM ST1/RPTB hazard is unsupported\n", machine().describe_context()); m_illegal = true; return;
				}
		if (m_pipeline_valid[2] && (word & 0xfeff) == 0x8821 &&
			(m_pipeline_delay_decode_words || m_pipeline_repeat_active || m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged BSP STLM control/repeat composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const direct_long_load = staged_dld(word);
		bool const direct_long_store = staged_dst(word);
		bool const long_arithmetic = staged_long_arithmetic(word);
		bool const memory_dp = staged_memory_dp(word);
		bool const direct_move = staged_move(word);
		bool const data_copy = staged_data_copy(word) || staged_delay_copy(word);
		if (m_pipeline_valid[2] && staged_delay_copy(word) && (m_pipeline_repeat_active || m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged LTD/DELAY repeat/block/XC composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const program_move = staged_program_move(word);
		if (m_pipeline_valid[2] && staged_extended_program_move(word) && (m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged extended MVPD block/XC composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const read_a = staged_read_a(word) || staged_write_a(word) || staged_data_to_program(word);
		if (m_pipeline_valid[2] && staged_read_a(word) && u8(word) >= 0xe0 && u8(word) != 0xf8 &&
			(m_pipeline_delay_decode_words || m_pipeline_repeat_active || m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged offset READA control/repeat composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (m_pipeline_valid[2] && staged_data_to_program(word) && (m_pipeline_delay_decode_words || m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged MVDP repeat/control-delay composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (m_pipeline_valid[2] && staged_write_a(word) && (m_pipeline_delay_decode_words || m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged WRITA repeat/control-delay composition is unsupported instruction=%06x word=%04x repeat=%u delay=%u xc=%u block=%u\n",
				machine().describe_context(), unsigned(m_pipeline_address[2]), word, unsigned(m_pipeline_repeat_active),
				m_pipeline_delay_decode_words, unsigned(m_pipeline_xc_active), unsigned(m_pipeline_block_body[2])); m_illegal = true; return;
		}
		bool const accumulator_status_move = (word == 0x7308 || word == 0x730b) && m_pipeline_immediate[2] == 7;
		bool const brc_move = direct_move && m_pipeline_immediate[2] == 0x1a;
		if (m_pipeline_valid[2] && word == 0x721a &&
			(m_pipeline_block_active || BIT(m_st1,15) || m_pipeline_repeat_active ||
			 m_pipeline_delay_decode_words || completing_move_delay || m_pipeline_xc_active))
		{
			logerror("%s: staged MVDM BRC active-loop or control composition is unsupported\n",machine().describe_context());
			m_illegal=true;return;
		}
		bool const clock_move = staged_clock_destination_move(word, m_pipeline_immediate[2]);
		if (m_pipeline_valid[2] && clock_move && (completing_move_delay || m_pipeline_delay_decode_words || m_pipeline_xc_active || m_pipeline_block_body[2] || m_pipeline_repeat_active))
		{
			logerror("%s: staged CLKMD move repeat/control composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const ordinary_delay_brc_move = completing_move_delay && m_pipeline_control_word != 0xf272;
		if (m_pipeline_valid[2] && brc_move && ((!ordinary_delay_brc_move && (completing_move_delay || m_pipeline_delay_decode_words)) || m_pipeline_block_body[2]))
		{
			logerror("%s: staged memory move BRC in repeat body or control delay is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (m_pipeline_valid[2] && direct_move && !brc_move && !clock_move && !accumulator_status_move && !staged_accumulator_destination_move(word, m_pipeline_immediate[2]) && !staged_t_register_move(word, m_pipeline_immediate[2]) && m_pipeline_immediate[2] < 0x60 &&
			(m_pipeline_immediate[2] < 0x10 || m_pipeline_immediate[2] > 0x19))
		{
			logerror("%s: staged memory move control/peripheral destination is unsupported instruction=%06x word=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[2]), word, m_pipeline_immediate[2]);
			m_illegal = true;
			return;
		}
		bool const immediate_store = (word & 0xff00) == 0x7600 && (u8(word) == 0xf8 || u8(word) < 0xa0 || staged_indexed_immediate_store(word) || staged_extended_immediate_store(word));
		bool const rmw = staged_rmw(word) && staged_rmw_addressing(word);
		bool const fast_return = word == 0xf49b || word == 0xf69b;
		bool const addm_brc = word == 0x6bf8 && m_pipeline_operand_address[2] == 0x1a;
		if (m_pipeline_valid[2] && addm_brc && (m_pipeline_block_active || BIT(m_st1, 15) || m_pipeline_repeat_active || m_pipeline_delay_decode_words))
		{
			logerror("%s: staged ADDM BRC active-loop or delay composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (m_pipeline_valid[2] && word == 0x6bf8 && m_pipeline_operand_address[2] < 0x60 &&
			(m_pipeline_operand_address[2] < 0x10 || m_pipeline_operand_address[2] > 0x18) && m_pipeline_operand_address[2] != 0x0e && !addm_brc)
		{
			logerror("%s: staged ADDM register/peripheral destination is unsupported instruction=%06x word=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[2]), word, m_pipeline_operand_address[2]);
			m_illegal = true; return;
		}
		bool const plain_return = word == 0xfc00 || word == 0xf4eb || staged_conditional_return(word);
		bool const delayed_return = word == 0xfe00 || staged_delayed_conditional_return(word);
		bool const far_return = (word & 0xfdfe) == 0xf4e4;
		bool const intm_bit = word == 0xf6bb || word == 0xf7bb;
		bool const smem_arithmetic = staged_smem_arithmetic(word) || staged_smem_special(word) || staged_shifted_smem(word);
		if (m_pipeline_valid[2] && staged_shifted_smem(word) && (m_pipeline_immediate[2] & 0xfee0) != 0x0c40 && !staged_shifted_memory_store(word, m_pipeline_immediate[2]) && !staged_shifted_memory_arithmetic(m_pipeline_immediate[2]))
		{
			logerror("%s: unsupported staged Smem extension %04x/%04x instruction=%06x last_fetch_physical=%06x\n", machine().describe_context(), word, m_pipeline_immediate[2], unsigned(m_pipeline_address[2]), unsigned(m_pipeline_physical_address[2]));
			m_illegal = true; return;
		}
		bool const data_push = (word & 0xff00) == 0x4b00 && (u8(word) < 0x98 || staged_extended_push(word));
		if (m_pipeline_valid[2] && staged_direct_push(word) && u8(word) >= 0x80 &&
			(m_pipeline_delay_decode_words || m_pipeline_repeat_active || m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged indirect PSHM control/repeat composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (m_pipeline_valid[2] && (staged_extended_push(word) || staged_extended_pop(word)) && (m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged extended stack conditional/block composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const data_pop = (word & 0xff00) == 0x8b00 && (u8(word) < 0x98 || staged_extended_pop(word));
		if (m_pipeline_valid[2] && (word == 0x8a00 || word == 0x8a01) &&
			(decoding_control_delay || m_pipeline_repeat_active || m_pipeline_xc_active || m_pipeline_block_body[2]))
		{
			logerror("%s: staged interrupt-register POPM repeat/control composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (word >= 0x8a1a && word <= 0x8a1c && m_pipeline_block_active && m_pipeline_block_body[2])
		{
			logerror("%s: staged POPM active repeat-control write is unsupported instruction=%06x word=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[2]), word);
			m_illegal = true; return;
		}
		bool const branch = staged_banz(word) || staged_block_repeat(word) || word == 0xf073 || word == 0xf273 || staged_conditional_branch(word) || staged_call(word) || staged_accumulator_near_call(word) || staged_accumulator_near_branch(word) || (m_extended_program && (staged_far_branch(word) || staged_far_call(word) || staged_accumulator_far_call(word) || staged_accumulator_far_branch(word)));
		bool const block_setup = staged_block_repeat(word);
		bool const block_word = m_pipeline_block_body[2];
		u32 const block_branch_target = (m_pipeline_address[2] & 0x7f0000) | m_pipeline_operand[2];
		bool const internal_block_branch = (word == 0xf073 || (staged_banz(word) && !BIT(word, 9)) || (staged_conditional_branch(word) && !staged_conditional_call(word) && !BIT(word, 9))) &&
			m_pipeline_address[2] + (staged_extended_banz(word) ? 2 : 1) < m_pipeline_block_end_address && block_branch_target >= m_pipeline_block_start && block_branch_target <= m_pipeline_block_end_address;
		bool const forward_block_exit = (word == 0xf073 || (staged_conditional_branch(word) && !staged_conditional_call(word) && !BIT(word, 9))) &&
			(m_pipeline_address[2] + 1 < m_pipeline_block_end_address ||
			 (staged_conditional_branch(word) && m_pipeline_address[2] + 1 == m_pipeline_block_end_address)) &&
			block_branch_target > m_pipeline_block_end_address;
		// A conditional BCD may also end its delay words at REA: the endpoint keeps
		// its Decode-owned BRC decrement and the retained branch redirect wins.
		bool const delayed_forward_block_exit = ((word == 0xf273 || (staged_accumulator_branch(word) && !staged_conditional_call(word) && BIT(word, 9))) &&
			m_pipeline_address[2] + 3 < m_pipeline_block_end_address && block_branch_target > m_pipeline_block_end_address) ||
			((staged_accumulator_branch(word) || staged_tc_branch(word)) && !staged_conditional_call(word) && BIT(word, 9) &&
			 m_pipeline_address[2] + 3 == m_pipeline_block_end_address && block_branch_target > m_pipeline_block_end_address);
		bool const internal_delayed_block_branch = (word == 0xf273 || ((staged_accumulator_branch(word) || staged_tc_branch(word) || (word & 0xfdfb) == 0xf808 || (word & 0xfde7) == 0xf860) && !staged_conditional_call(word) && BIT(word, 9))) &&
			m_pipeline_address[2] + 3 < m_pipeline_block_end_address && block_branch_target >= m_pipeline_block_start && block_branch_target <= m_pipeline_block_end_address;
		u32 const far_call_target = (u32(word & 0x7f) << 16) | m_pipeline_operand[2];
		bool const block_far_call = staged_far_call(word) &&
			(BIT(word, 9) ? m_pipeline_address[2] + 3 <= m_pipeline_block_end_address : m_pipeline_address[2] + 1 < m_pipeline_block_end_address) &&
			(far_call_target < m_pipeline_block_start || far_call_target > m_pipeline_block_end_address);
		bool const block_conditional_call = staged_conditional_call(word) && !staged_tc_branch(word) && BIT(word, 9) &&
			m_pipeline_address[2] + 3 <= m_pipeline_block_end_address &&
			(block_branch_target < m_pipeline_block_start || block_branch_target > m_pipeline_block_end_address);
		bool const block_near_call = staged_call(word) &&
			m_pipeline_address[2] + (BIT(word, 9) ? 3 : 1) <= m_pipeline_block_end_address &&
			(block_branch_target < m_pipeline_block_start || block_branch_target > m_pipeline_block_end_address);
		bool const internal_block_xc = staged_xc(word) && m_pipeline_address[2] + (BIT(word, 9) ? 2 : 1) <= m_pipeline_block_end_address;
		bool const internal_memory_repeat = staged_memory_repeat(word) &&
			m_pipeline_address[2] + (word == 0x47f8 ? 2 : 1) <= m_pipeline_block_end_address;
		bool const internal_immediate_repeat = staged_immediate_repeat(word) && m_pipeline_address[2] + 2 <= m_pipeline_block_end_address;
		bool const internal_conditional_return = staged_conditional_return(word) && m_pipeline_address[2] < m_pipeline_block_end_address;
		// Direct STLM 60..7f writes ordinary RAM, not loop-control state.
		if (m_pipeline_valid[2] && block_word && ((branch && !internal_block_branch && !forward_block_exit && !delayed_forward_block_exit && !internal_delayed_block_branch && !block_far_call && !block_conditional_call && !block_near_call) || software_intr || block_setup || (plain_return && !internal_conditional_return) || delayed_return || far_return || fast_return ||
			(staged_immediate_repeat(word) && !internal_immediate_repeat) || (staged_memory_repeat(word) && !internal_memory_repeat) || staged_immediate_loop_register(word) || (short_repeat && m_pipeline_address[2] == m_pipeline_block_end_address) || (staged_xc(word) && !internal_block_xc) || staged_idle(word) ||
			((word & 0xfe00) == 0x8800 && u8(word) >= 0x1a && (u8(word) < 0x60 || u8(word) >= 0x80))))
		{
			logerror("%s: staged block control/count modification is unsupported instruction=%06x word=%04x block=%06x..%06x\n",
				machine().describe_context(), unsigned(m_pipeline_address[2]), word, unsigned(m_pipeline_block_start), unsigned(m_pipeline_block_end_address)); m_illegal = true; return;
		}
		bool const dagen_immediate = (word & 0xff00) == 0x7700 && u8(word) >= 0x10 && u8(word) <= 0x19;
		bool const immediate_loop = staged_immediate_loop_register(word);
		bool const immediate_t = word == 0x770e;
		if (m_pipeline_valid[2] && !BIT(staged_opcode_descriptor[word], m_extended_program ? 5 : 4) && !staged_bus_control(word))
		{
			logerror("%s: unsupported staged C54x form %04x at %06x\n",
				machine().describe_context(), word, m_pipeline_address[2]);
			m_illegal = true;
			return;
		}
		if (m_pipeline_valid[2] && m_pipeline_repeat_active && !staged_immediate_repeat(word) && !staged_memory_repeat(word) && !short_repeat && !m_pipeline_repeat_body_latched)
		{
			if (!program_move && !read_a && !staged_repeat_port(word) && !staged_pipelined_repeat(word) && !staged_repeat_accumulator_load(word) && word != 0xf495 && !((word & 0xff00) == 0x7600 && u8(word) < 0x98) && !((word & 0xfe00) == 0x1e00 && u8(word) < 0x98)) { logerror("%s: staged repeat body %04x at %06x is unsupported\n", machine().describe_context(), word, m_pipeline_address[2]); m_illegal = true; return; }
			if (staged_repeat_port(word) && (m_pipeline_block_body[2] || m_pipeline_xc_active || m_pipeline_repeat_redirect_pending))
			{
				logerror("%s: staged repeated port nested control composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
			}
			// Every body replaces the previous repeat's opcode, including
			// non-pipelined bodies whose count arrives in the older Read slot.
			m_pipeline_repeat_word = word;
			m_pipeline_repeat_coefficient_started = false;
			if (staged_pipelined_repeat(word))
			{
				auto const count_header = [this](unsigned stage) { return m_pipeline_valid[stage] && (staged_short_repeat(m_pipeline_word[stage]) || staged_immediate_repeat(m_pipeline_word[stage]) || staged_memory_repeat(m_pipeline_word[stage])); };
				unsigned owner = 3;
				// BCD's empty prefetch slot moves the short count header to
				// Read before its one-word delay body reaches Decode.
				bool const conditional_delay_repeat = m_pipeline_repeat_redirect_pending &&
					staged_accumulator_branch(m_pipeline_control_word) && BIT(m_pipeline_control_word, 9) &&
					m_pipeline_address[2] == m_pipeline_control_address + 3 &&
					m_pipeline_address[4] == m_pipeline_control_address + 2 && staged_short_repeat(m_pipeline_word[4]);
				if ((staged_repeat_advancing_literal(word) || staged_repeat_extension_shift(word) || conditional_delay_repeat) && !count_header(owner)) owner = 4;
				if (!count_header(owner))
				{
					logerror("%s: staged pipeline repeat count owner is unsupported instruction=%06x word=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[2]), word); m_illegal = true; return;
				}
				// Smem's count is not its address literal. Read supplies it once,
				// before the held pipeline can issue any later body iteration.
				m_pipeline_repeat_issue_remaining = staged_memory_repeat(m_pipeline_word[owner]) ? 0 : m_pipeline_immediate[owner];
				if (staged_repeat_advancing_literal(word)) m_pipeline_repeat_source = m_pipeline_immediate[2] + 1;
				else if (staged_repeat_extension_shift(word)) m_pipeline_repeat_source = m_pipeline_immediate[2];
			}
			m_pipeline_repeat_body_latched = true;
			m_pipeline_repeat_address = m_pipeline_address[2];
			m_pipeline_valid[0] = m_pipeline_valid[1] = false;
		}
		if (m_pipeline_valid[2] && staged_xc(word))
		{
			// XC 1 also protects two following words (SPRU172C 4-199).
			m_pipeline_xc_active = true;
			m_pipeline_xc_end = (m_pipeline_address[2] & 0x7f0000) | u16(m_pipeline_address[2] + 2);
			if (block_word && !BIT(word, 9) && m_pipeline_address[2] + 1 == m_pipeline_block_end_address && m_brc)
				// XC 1 selects the endpoint only, but protects the next executed
				// word too: the repeat's first word, not abandoned fall-through.
				m_pipeline_xc_end = (m_pipeline_address[2] & 0x7f0000) | u16(m_pipeline_block_start);
			if (retd_delay_decode)
				// XC protection counts resumed instruction words, not the
				// abandoned sequential address beyond RETD's delay window.
				m_pipeline_xc_end = (m_pipeline_retd_target & 0x7f0000) | u16(m_pipeline_retd_target + retd_delay_index);
		}
		if (m_pipeline_valid[2] && staged_idle(word))
		{
			// IDLE is a decode barrier: older slots drain, younger words
			// are discarded and refetched only after the wake boundary.
			m_pipeline_idle_barrier = true;
			m_pipeline_idle_resume_address = (m_pipeline_address[2] & 0x7f0000) | u16(m_pipeline_address[2] + 1);
			m_pipeline_control_address = m_pipeline_address[2];
			m_pipeline_control_word = word;
			m_pipeline_control_words = 1;
			m_pipeline_valid[0] = m_pipeline_valid[1] = false;
		}
		if (m_pipeline_valid[2] && (fast_return || plain_return || delayed_return || far_return || branch || block_setup))
		{
			bool const delayed = delayed_return || word == 0xf69b || word == 0xf272 || word == 0xf273 || word == 0xf274 || (staged_accumulator_transfer(word) && BIT(word, 9)) || ((staged_far_call(word) || staged_far_branch(word)) && BIT(word, 9)) || (staged_banz(word) && BIT(word, 9)) || (staged_conditional_branch(word) && BIT(word, 9)) || (far_return && BIT(word, 9));
			// Example 7-17 has one empty prefetch slot, unlike unconditional BD.
			prefetch_bubble = staged_conditional_branch(word);
			m_pipeline_control_address = m_pipeline_address[2];
			m_pipeline_control_word = word;
			m_pipeline_control_words = (staged_accumulator_transfer(word) ? 1 : staged_extended_banz(word) ? 3 : branch || block_setup ? 2 : 1) + (delayed ? 2 : 0);
			m_pipeline_delay_decode_words = delayed ? 2 : 0;
			m_pipeline_decode_flush = delayed || staged_conditional_branch(word) || staged_banz(word) ? 0 : 2;
			if (plain_return || (far_return && !delayed) || (staged_accumulator_transfer(word) && !delayed)) m_pipeline_flush_front = true;
			if (delayed_return) m_pipeline_retd_stage = 1;
		}
		if (m_pipeline_valid[2] && (dagen_immediate || immediate_loop || immediate_t || direct_move)) m_pipeline_early_write_deferred[2] = false;
		if (m_pipeline_valid[2])
		{
			// SPRU131G table 7-16: retain CMPT before the older full-status
			// move publishes. Address selection and AR updates share this sample.
			m_pipeline_cmpt[2] = BIT(m_st1, 5);
			// Table 7-3: STLM BK has a three-word circular-address latency.
			m_pipeline_bk[2] = m_bk;
			// Basic direct operands sample CPL/DP in Decode, before a preceding
			// RSBX/SSBX retires (SPRU131G tables 7-18 and 7-19).
			if (staged_cmps(word) || staged_memory_repeat(word) || staged_bitfield(word) || staged_memory_immediate_multiply(word) || smem_arithmetic || immediate_store || staged_port_read(word) || staged_port_write(word) || rmw || staged_memory_logic(word) || short_load || direct_store || direct_long_load || direct_long_store || long_arithmetic || memory_dp || direct_move || data_copy || program_move || read_a || data_push || data_pop)
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
	// Retain the older mapped load or ordinary ADD/SUB Read before a younger
	// same-clock Access update of that AR (SPRU131G figure 7-1).
	// Other read families and accumulator/control sources keep their
	// independently qualified forwarding and latency rules.
	bool const retained_ar_operand_read = m_pipeline_valid[4] &&
		m_pipeline_operand_address[4] >= 0x10 && m_pipeline_operand_address[4] <= 0x17 &&
		(staged_mmr_load(m_pipeline_word[4]) ||
		 (staged_memory_load(m_pipeline_word[4]) && ((m_pipeline_word[4] & 0xfc00) == 0x1000 ||
		  (m_pipeline_word[4] & 0xfe00) == 0x4400)) ||
		 staged_absolute_add(m_pipeline_word[4]) ||
		 (staged_smem_arithmetic(m_pipeline_word[4]) && ((m_pipeline_word[4] & 0xfe00) == 0 ||
		  (m_pipeline_word[4] & 0xfe00) == 0x0800)));
	if (retained_ar_operand_read)
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x7600 &&
		(m_pipeline_word[3] == 0x76f8 || ((m_pipeline_word[3] & 0xfff8) == 0x7680 && !indirect_compatibility(3)) ||
		 (u8(m_pipeline_word[3]) < 0x80 && !BIT(m_pipeline_direct_status[3], 14))))
	{
		// SPRU131G table 7-4 includes constant ST with the early DAGEN
		// writers. Nonmodifying indirect AR aliases retain their address before
		// publication: a self-alias may change its own pointer at this clock.
		bool const indirect_alias = (m_pipeline_word[3] & 0xfff8) == 0x7680;
		u16 const destination = indirect_alias ? short_smem_address(u8(m_pipeline_word[3]), 3) :
			m_pipeline_word[3] == 0x76f8 ? m_pipeline_operand_address[3] : u16((m_pipeline_direct_status[3] & 0x1ff) << 7) | u8(m_pipeline_word[3]);
		if (indirect_alias) m_pipeline_operand_address[3] = destination;
		if (destination >= 0x10 && destination <= (indirect_alias ? 0x17 : 0x19))
		{
			if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[3] == m_pipeline_repeat_address)
			{
				logerror("%s: repeated immediate store register/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
			}
			m_pipeline_operand_address[3] = destination;
			bool const conflict = m_pipeline_valid[5] &&
				(staged_store(m_pipeline_word[5]) || staged_accumulator_mmr_store(m_pipeline_word[5]) || staged_rmw(m_pipeline_word[5]) || staged_shifted_memory_store(m_pipeline_word[5], m_pipeline_immediate[5])) &&
				m_pipeline_operand_address[5] >= 0x10 && m_pipeline_operand_address[5] <= 0x19;
			m_pipeline_early_write_deferred[3] = conflict;
			if (destination == 0x18) m_pipeline_direct_sp[3] = m_sp;
			if (!conflict)
			{
				data_write(destination, m_pipeline_immediate[3], 3);
				if (destination == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[3]);
			}
		}
	}
	if (m_pipeline_valid[3] && staged_indirect_ram_stm(m_pipeline_word[3]))
	{
		unsigned const ar = indirect_ar(u8(m_pipeline_word[3]), 3);
		u16 const destination = m_ar[ar] & 0x7f;
		if (destination < 0x60)
		{
			logerror("%s: staged indirect STM register/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		// STM's MMR address ignores DP and clears the selected AR's upper
		// nine bits (SPRU172C 4-176). Ordinary RAM publication stays Execute.
		m_pipeline_operand_address[3] = destination;
		m_ar[ar] = destination;
	}
	if (m_pipeline_valid[3] && staged_direct_ram_stm(m_pipeline_word[3]))
		m_pipeline_operand_address[3] = u8(m_pipeline_word[3]);
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x7700 && !staged_ram_stm(m_pipeline_word[3]) && !staged_execute_mmr_stm(m_pipeline_word[3]) && m_pipeline_word[3] != 0x7707 && m_pipeline_word[3] != 0x771d && m_pipeline_word[3] != 0x7700 && !staged_bus_control(m_pipeline_word[3]) && !staged_timer_control(m_pipeline_word[3]) && !staged_serial_control(m_pipeline_word[3]) && !staged_clock_control(m_pipeline_word[3]))
	{
		// SPRU131G 7.5.2: STM writes DAGEN in the opcode word's Read,
		// coincident with Access of our retained two-word operation. An older
		// store using that register bank defers the write one clock (7-22).
		// STM T uses this early-write path as a partial model of example
		// 7-39's zero T latency; T is not subject to the DAGEN-bank deferral.
		// BRC likewise has zero latency before RPTB (table 7-24), without
		// inheriting DAGEN-bank arbitration. In-loop BRC writes stay excluded.
		bool const older_store = m_pipeline_word[3] != 0x770e && !staged_immediate_loop_register(m_pipeline_word[3]) && m_pipeline_valid[5] &&
			(staged_store(m_pipeline_word[5]) || staged_accumulator_mmr_store(m_pipeline_word[5]) || staged_rmw(m_pipeline_word[5]) || staged_shifted_memory_store(m_pipeline_word[5], m_pipeline_immediate[5])) &&
			m_pipeline_operand_address[5] >= 0x10 && m_pipeline_operand_address[5] <= 0x19;
		m_pipeline_early_write_deferred[3] = older_store;
		if (u8(m_pipeline_word[3]) == 0x18) m_pipeline_direct_sp[3] = m_sp;
		if (!older_store)
		{
			data_write(u8(m_pipeline_word[3]), m_pipeline_immediate[3], 3);
			if (u8(m_pipeline_word[3]) == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[3]);
		}
	}
	if (m_pipeline_valid[4] && m_pipeline_early_write_deferred[4])
	{
		bool const move = staged_move(m_pipeline_word[4]);
		bool const constant_store = (m_pipeline_word[4] & 0xff00) == 0x7600;
		u16 const destination = constant_store ? m_pipeline_operand_address[4] : move ? m_pipeline_immediate[4] : u8(m_pipeline_word[4]);
		data_write(destination, move ? m_pipeline_operand[4] : m_pipeline_immediate[4], 4);
		if (destination == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[4]);
		m_pipeline_early_write_deferred[4] = false;
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] == 0xf49b || m_pipeline_word[3] == 0xf69b))
	{
		// SPRU131G examples 7-11/12: Access increments SP and reads RTN,
		// allowing target Prefetch now; no stack-memory read occurs.
		++m_sp;
		m_pipeline_fetch_address = (m_pipeline_address[3] & 0x7f0000) | m_rtn;
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] == 0xf073 || m_pipeline_word[3] == 0xf273 || staged_call(m_pipeline_word[3]) || staged_far_branch(m_pipeline_word[3]) || staged_far_call(m_pipeline_word[3])))
	{
		// SPRU131G examples 7-2/3: the target word's Access starts target
		// Prefetch; the following two sequential words are delay or flush slots.
		m_pipeline_fetch_address = (staged_far_branch(m_pipeline_word[3]) || staged_far_call(m_pipeline_word[3]) ? u32(m_pipeline_word[3] & 0x7f) << 16 : m_pipeline_address[3] & 0x7f0000) | m_pipeline_operand[3];
		if (m_pipeline_block_body[3]) m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
		if (staged_call(m_pipeline_word[3])) m_rtn = m_pipeline_immediate[3];
		if (staged_far_call(m_pipeline_word[3])) m_pipeline_far_call_stack[3] = --m_sp;
	}
	auto const basic_address = [this](unsigned stage, u8 mode) -> u16 {
		if (BIT(mode, 7)) return short_smem_address(mode, stage);
		u16 const status = m_pipeline_direct_status[stage];
		return BIT(status, 14) ? u16(m_pipeline_direct_sp[stage] + mode) : u16((status & 0x1ff) << 7) | mode;
	};
	// Resolve an older store's implicit DAGEN update before a younger
	// memory Access consumes the same AR. The data write remains in Execute.
	// This is address ordering, not forwarding an explicit MMR store.
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xfff8) == 0x7688)
	{
		m_pipeline_operand_address[4] = basic_address(4, u8(m_pipeline_word[4]));
		indirect_modify(u8(m_pipeline_word[4]), false, 4);
	}
	if (m_pipeline_valid[4] && staged_read_a(m_pipeline_word[4]))
	{
		// READA also writes EAB in Read. Resolve its destination before a
		// younger Access updates the same AR; retain the existing PAR hold.
		u8 const mode = m_pipeline_word[4];
		bool const extended = mode >= 0xe0;
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode, false, 4);
		m_pipeline_operand_address[4] = mode == 0xf8 ? m_pipeline_immediate[4] : extended ?
			long_offset_address(mode, indirect_ar(mode, 4), s16(m_pipeline_immediate[4]), 4) : basic_address(4, mode);
		u16 const destination = m_pipeline_operand_address[4];
		bool const repeat_body = m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[4] == m_pipeline_repeat_address;
		bool const bd_delay = m_pipeline_control_words && m_pipeline_control_word == 0xf273 &&
			(m_pipeline_address[4] == m_pipeline_control_address+2 || m_pipeline_address[4] == m_pipeline_control_address+3);
		bool const accumulator_word = destination >= 0x08 && destination <= 0x0d;
		bool const brc_word = destination == 0x1a && !m_pipeline_block_active;
		if (destination < 0x60 && (repeat_body || bd_delay || (!accumulator_word && !brc_word && (destination < 0x10 || destination > 0x17))))
		{
			logerror("%s: staged READA control/peripheral destination is unsupported instruction=%06x word=%04x destination=%04x repeat=%u\n",
				machine().describe_context(), unsigned(m_pipeline_address[4]), m_pipeline_word[4], destination, unsigned(repeat_body)); m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 4);
		m_pipeline_read_a_wait = 4;
	}
	if (m_pipeline_valid[4] && staged_xmem_store(m_pipeline_word[4]))
	{
		// SPRU131G 7.1: a store presents EAB in Read and drives EB in Execute.
		u8 const xmem = m_pipeline_word[4] >> 4;
		m_pipeline_operand_address[4] = m_ar[2 + (xmem & 3)];
		bool const mapped_ar = !(xmem & 12) && m_pipeline_operand_address[4] >= 0x10 && m_pipeline_operand_address[4] <= 0x17;
		if (m_pipeline_operand_address[4] < 0x60 && !mapped_ar)
		{
			logerror("%s: staged Xmem store register/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		dual_modify(xmem, 4);
	}
	if (m_pipeline_valid[4] && staged_store(m_pipeline_word[4]))
	{
		u8 const mode = m_pipeline_word[4];
		bool const extended = staged_extended_store(m_pipeline_word[4]);
		bool const t_store = (m_pipeline_word[4] & 0xfe00) == 0x8c00;
		unsigned const ar = indirect_ar(mode, 4);
		u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		// An older store can reach here after a younger RPT decodes; only the
		// latched body is a repeated store.
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[4] == m_pipeline_repeat_address &&
			staged_repeat_accumulator_store(m_pipeline_word[4]) && u16(basic_address(4, mode) + unsigned(preincrement)) < 0x60)
		{
			// Exclude mapped destinations before any DAGEN or bus effect.
			logerror("%s: repeated accumulator store mapped destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (preincrement) indirect_modify(mode, false, 4);
		if (staged_extended_store(m_pipeline_word[4]))
		{
			m_pipeline_operand_address[4] = mode == 0xf8 ? m_pipeline_immediate[4] :
				long_offset_address(mode, indirect_ar(mode, 4), s16(m_pipeline_immediate[4]), 4);
			// Table 7-3 groups STL/STH writes with STLM: publish in Execute.
			// AR writes are not forwarded to the next two address words.
			// T stores require separation for short shift/bit-test consumers
			// (section 7.5.6), not for long-address consumers or younger LD T.
			if (!t_store && m_pipeline_operand_address[4] < 0x60 &&
				m_pipeline_operand_address[4] != 0x0e &&
				(m_pipeline_operand_address[4] < 0x10 || m_pipeline_operand_address[4] > 0x17))
			{
				logerror("%s: staged extended accumulator store register/peripheral destination is unsupported instruction=%06x last_fetch_physical=%06x word=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[4]), unsigned(m_pipeline_physical_address[4]), m_pipeline_word[4], m_pipeline_operand_address[4]);
				m_illegal = true; return;
			}
		}
		else m_pipeline_operand_address[4] = basic_address(4, mode);
		if (staged_asm_store(m_pipeline_word[4]))
		{
			if (m_pipeline_operand_address[4] < 0x60)
			{
				logerror("%s: staged ASM store register/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
			}
			m_pipeline_immediate[4] = m_st1 & 0x1f;
		}
		if ((m_pipeline_word[4] & 0xfc00) == 0x8000 && mode >= 0xa0 && mode < 0xe0 && m_pipeline_operand_address[4] < 0x60)
		{
			logerror("%s: staged AR0-indexed accumulator store low-page destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if ((m_pipeline_word[4] & 0xfe00) == 0x8c00 && m_pipeline_operand_address[4] < 0x20)
		{
			if (extended) { m_ar[ar] = previous_ar; m_st0 = previous_st0; }
			logerror("%s: staged T/TRN store register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (BIT(mode, 7) && !extended && !preincrement) indirect_modify(mode, false, 4);
	}
	if (m_pipeline_valid[4] && staged_shifted_memory_store(m_pipeline_word[4], m_pipeline_immediate[4]))
	{
		// As for ordinary stores, older Read-owned DAGEN precedes younger
		// Access. Resolving this after Access can both retarget the store
		// and make an adjacent DLD consume an odd/reversed word pair.
		u8 const mode = m_pipeline_word[4];
		bool const extended = mode >= 0xe0;
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode, false, 4);
		u16 const literal = m_pipeline_operand_address[4];
		m_pipeline_operand_address[4] = extended ? (mode == 0xf8 ? literal : long_offset_address(mode, indirect_ar(mode, 4), s16(literal), 4)) : basic_address(4, mode);
		// SPRU131G tables 7-5/7-7: explicit ARx data publication stays in
		// Execute; this ordering concerns only the implicit address update.
		if (m_pipeline_operand_address[4] < 0x60 && (m_pipeline_operand_address[4] < 0x10 || m_pipeline_operand_address[4] > 0x17))
		{
			logerror("%s: staged shifted store register/peripheral destination is unsupported instruction=%06x word=%04x operation=%04x destination=%04x\n",
				machine().describe_context(), unsigned(m_pipeline_address[4]), m_pipeline_word[4], m_pipeline_immediate[4], m_pipeline_operand_address[4]);
			m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 4);
	}
	if (m_pipeline_valid[3] && staged_data_to_program(m_pipeline_word[3]))
	{
		m_pipeline_direct_sp[3] = m_pipeline_compiler_sp_wait ? m_pipeline_compiler_sp_previous :
			m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x4b00 ? m_pipeline_immediate[4] : m_sp;
		u8 const mode = m_pipeline_word[3];
		bool const extended = mode >= 0xe0;
		u16 const literal = m_pipeline_operand_address[3];
		m_pipeline_operand_address[3] = mode == 0xf8 ? literal : extended ?
			long_offset_address(mode, indirect_ar(mode, 3), s16(literal), 3) : basic_address(3, mode);
		if (m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged MVDP register/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7)) indirect_modify(mode, false, 3);
		// Retain the explicit pmad and instruction page, not accumulator A.
		// Both offset and absolute forms retain pmad in the final extension.
	}
	if (m_pipeline_valid[3] && staged_write_a(m_pipeline_word[3]))
	{
		m_pipeline_direct_sp[3] = m_pipeline_compiler_sp_wait ? m_pipeline_compiler_sp_previous :
			m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x4b00 ? m_pipeline_immediate[4] : m_sp;
		u8 const mode = m_pipeline_word[3];
		bool const extended = mode >= 0xe0, absolute = mode == 0xf8;
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode, false, 3);
		m_pipeline_operand_address[3] = absolute ? m_pipeline_immediate[3] : extended ?
			long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_immediate[3]), 3) : basic_address(3, mode);
		if (m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged WRITA register/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[3] && staged_accumulator_mmr_store(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		u16 const destination = BIT(mode, 7) ? m_ar[indirect_ar(mode, 3)] & 0x7f : mode;
		if (BIT(mode, 7) && (destination < 0x10 || destination > 0x17))
		{
			logerror("%s: staged indirect STLM non-AR destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		m_pipeline_operand_address[3] = mmr_address(mode, 3);
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x8a00)
	{
		u8 const mode = m_pipeline_word[3];
		unsigned const ar = indirect_ar(mode, 3);
		u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		u16 const destination = mmr_address(mode, 3);
		if (BIT(mode, 7) && (destination < 0x10 || destination > 0x18))
		{
			m_ar[ar] = previous_ar; m_st0 = previous_st0;
			logerror("%s: staged indirect POPM non-AR/SP destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		m_pipeline_operand_address[3] = destination;
	}
	if (m_pipeline_valid[3] && BIT(staged_opcode_descriptor[m_pipeline_word[3]], 6))
		// Table 7-9: a basic direct consumer samples SP before an older
		// PSHM allocates its stack word in this clock's Read stage.
		m_pipeline_direct_sp[3] = m_pipeline_compiler_sp_wait ? m_pipeline_compiler_sp_previous : m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x4b00
			? m_pipeline_immediate[4] : m_sp; // Table 7-9: one-clock Category-I latency after PSHD.
	if (m_pipeline_valid[3] && staged_delay_copy(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		bool const extended = mode >= 0xe0;
		u16 const source = mode == 0xf8 ? m_pipeline_immediate[3] : extended ?
			long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_immediate[3]), 3) : basic_address(3, mode);
		if (source < 0x60 || source == 0xffff)
		{
			logerror("%s: staged LTD/DELAY mapped or wrapping operand is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		m_pipeline_operand_address[3] = source;
		if (!extended && BIT(mode, 7)) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[4] && staged_delay_copy(m_pipeline_word[4]))
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
	if (m_pipeline_valid[3] && staged_data_copy(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		u16 const source = m_pipeline_immediate[3];
		// SP here is an explicit data source sampled in Read, not the
		// earlier compiler-mode address base captured above (SPRU131G 7.1).
		bool const brc_to_ar = source == 0x1a && staged_direct_mvdm(m_pipeline_word[3]) && mode >= 0x10 && mode <= 0x17;
		if (source < 0x60 && (source < 0x10 || source > 0x19) && !brc_to_ar)
		{
			logerror("%s: staged MVKD control/peripheral source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], source); m_illegal = true; return;
		}
		bool const extended = staged_extended_data_copy(m_pipeline_word[3]);
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		bool const fixed_mmr = staged_direct_mvdm(m_pipeline_word[3]);
		u16 const preview = fixed_mmr ? mode : u16(basic_address(3, mode) + unsigned(preincrement));
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[3] == m_pipeline_repeat_address &&
			(source < 0x60 || (fixed_mmr ? mode < 0x10 || mode > 0x17 : preview < 0x60)))
		{
			logerror("%s: repeated MVKD mapped source/destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (preincrement) indirect_modify(mode, false, 3);
		if (extended && mode != 0xf8)
			m_pipeline_operand_address[3] = long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_operand_address[3]), 3);
		else if (!extended) m_pipeline_operand_address[3] = staged_direct_mvdm(m_pipeline_word[3]) ? mode : basic_address(3, mode);
		u16 const destination = m_pipeline_operand_address[3];
		bool const accumulator_destination = staged_direct_mvdm(m_pipeline_word[3]) && destination >= 8 && destination <= 0x0d;
		bool const t_destination = staged_direct_mvdm(m_pipeline_word[3]) && (destination == 0x0e || destination == 0x0f);
		bool const brc_destination = m_pipeline_word[3] == 0x721a && destination == 0x1a;
		if (destination < 0x20 && !accumulator_destination && !t_destination && !brc_destination && (destination < 0x10 || destination > 0x17))
		{
			logerror("%s: staged MVKD control/peripheral destination is unsupported instruction=%06x word=%04x source=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], source, destination); m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[4] && staged_data_copy(m_pipeline_word[4]))
	{
		// Class 19A/B: retain the source before Execute's destination write.
		// Bus placement is partial; same-address moves must not read back a write.
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_immediate[4]);
		if (m_pipeline_operand_address[4] < 0x20)
			// The opcode's Execute coincides with the complete two-word slot's
			// Read. This satisfies table 7-7's one-gap Category-I AR latency;
			// unlike one-word STLM, no DAGEN write remains in the next clock.
			data_write(m_pipeline_operand_address[4], m_pipeline_operand[4], 4);
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x4b00)
	{
		u8 const mode = m_pipeline_word[3];
		bool const extended = staged_extended_push(m_pipeline_word[3]);
		unsigned const ar = indirect_ar(mode, 3); u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		m_pipeline_operand[3] = !extended ? basic_address(3, mode) : mode == 0xf8 ? m_pipeline_immediate[3] : long_offset_address(mode, ar, s16(m_pipeline_immediate[3]), 3);
		u16 const source = m_pipeline_operand[3];
		bool const modifying = extended ? mode != 0xf8 && (mode & 0x78) != 0x60 : BIT(mode, 7) && (mode & 0x78);
		// Inside a repeat, SP and the modified pointer AR read as their live
		// Read-stage values; the matching repeated POPD keeps its own DAGEN
		// updates (ROM4's RPT PSHD *AR0+ context save, rpt_context_save).
		bool const self_ar = modifying && source == 0x10 + ar;
		bool const repeat_body = m_pipeline_repeat_active && m_pipeline_address[3] == m_pipeline_repeat_address;
		if (((extended || !repeat_body) && (source == 0x18 || self_ar)) || (source >= 0x20 && source < 0x60))
		{
			// A prototype exclusion is not a hardware trap: discard address
			// preview side effects before returning without stack allocation.
			if (extended) { m_ar[ar] = previous_ar; m_st0 = previous_st0; }
			logerror("%s: staged PSHD SP/self-AR/peripheral source is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7)) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0x8b00)
	{
		u8 const mode = m_pipeline_word[3];
		bool const extended = staged_extended_pop(m_pipeline_word[3]);
		unsigned const ar = indirect_ar(mode, 3); u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		m_pipeline_operand_address[3] = !extended ? basic_address(3, mode) : mode == 0xf8 ? m_pipeline_immediate[3] : long_offset_address(mode, ar, s16(m_pipeline_immediate[3]), 3);
		u16 const destination = m_pipeline_operand_address[3];
		bool const repeat_body = m_pipeline_repeat_active && m_pipeline_address[3] == m_pipeline_repeat_address;
		bool const mapped = (destination >= 8 && destination <= 0x1d && destination != 0x18) || (destination == 0x18 && repeat_body);
		if ((extended && destination < 0x60) || (destination < 0x60 && !mapped) || (destination >= 0x1a && destination <= 0x1d && m_pipeline_block_active && m_pipeline_block_body[3]))
		{
			// Extended mapped destinations need their own DAGEN/write arbitration tests.
			if (extended) { m_ar[ar] = previous_ar; m_st0 = previous_st0; }
			logerror("%s: staged POPD status/SP/peripheral or active repeat-control destination is unsupported (word=%04x destination=%04x block_active=%u)\n", machine().describe_context(), m_pipeline_word[3], destination, unsigned(m_pipeline_block_active));
			m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7)) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[3] && staged_mar(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		if (staged_long_mar(m_pipeline_word[3]))
		{
			// SPRU131G table5-4: long preoffset is linear for MOD13,
			// circular for MOD14; both publish the updated auxiliary register.
			long_offset_address(mode,indirect_ar(mode,3),s16(m_pipeline_immediate[3]),3);
		}
		else indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[3] && staged_move(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		bool const extended = staged_extended_move(m_pipeline_word[3]);
		u16 const destination = m_pipeline_immediate[3];
		// Decode may arm a younger RPT while this older move is in Access.
		bool const repeat_body = m_pipeline_repeat_active && m_pipeline_repeat_body_latched &&
			m_pipeline_address[3] == m_pipeline_repeat_address;
		bool const repeated_ar_destination = (m_pipeline_word[3]&0xff00)==0x7100 &&
			mode>=0x80 && mode<0x98 && destination>=0x10 && destination<=0x17;
		if (repeat_body && staged_repeat_destination_copy(m_pipeline_word[3]) && destination < 0x60 && !repeated_ar_destination)
		{
			logerror("%s: repeated MVDK mapped destination is unsupported (word=%04x destination=%04x repeat_word=%04x repeat_source=%04x)\n",
					machine().describe_context(), m_pipeline_word[3], destination,
					m_pipeline_repeat_word, m_pipeline_repeat_source);
			m_illegal = true; return;
		}
		bool const status = (m_pipeline_word[3] == 0x7308 || m_pipeline_word[3] == 0x730b) && destination == 7;
		bool const brc = destination == 0x1a;
		if ((m_pipeline_word[3] == 0x7308 || m_pipeline_word[3] == 0x730b) && !status && !brc && !staged_accumulator_destination_move(m_pipeline_word[3], destination) && destination < 0x60)
		{
			logerror("%s: staged MVMD accumulator-to-register destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (staged_direct_mvmd(m_pipeline_word[3]) && !status && !brc && !staged_accumulator_destination_move(m_pipeline_word[3], destination) && !staged_t_register_move(m_pipeline_word[3], destination) && destination < 0x60 && (destination < 0x10 || destination > 0x19))
		{
			logerror("%s: staged MVMD control/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if ((extended || staged_circular_move(m_pipeline_word[3]) || staged_indexed_move(m_pipeline_word[3])) && !brc && !staged_clock_destination_move(m_pipeline_word[3], destination) && !staged_accumulator_destination_move(m_pipeline_word[3], destination) && !staged_t_register_move(m_pipeline_word[3], destination) && destination < 0x60 && (destination < 0x10 || destination > 0x19))
		{
			logerror("%s: staged extended/circular MVDK control/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		u16 const source = staged_direct_mvmd(m_pipeline_word[3]) ? mode : extended ? (mode == 0xf8 ? m_pipeline_operand_address[3] :
			long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_operand_address[3]), 3)) : basic_address(3, mode);
		if (repeat_body && staged_repeat_destination_copy(m_pipeline_word[3]) && source < 0x60 && !staged_direct_mvmd(m_pipeline_word[3]))
		{
			logerror("%s: repeated MVDK mapped source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (source < 0x60 && (source < 8 || source > 0x19) && !staged_direct_mvmd(m_pipeline_word[3]))
		{
			logerror("%s: staged MVDK register/peripheral source is unsupported\n", machine().describe_context());
			m_illegal = true;
			return;
		}
		// Retain the resolved mapped address as well as the word. A preceding
		// mapped accumulator write may publish later in this same clock;
		// direct ALU producers deliberately retain their documented latency.
		m_pipeline_operand_address[3] = source;
		bool const mapped_write = pipeline_mapped_accumulator_write(source) ||
			(m_pipeline_valid[5] && staged_mvmd_cleared_source(m_pipeline_word[3], m_pipeline_word[5]));
		if (!mapped_write) m_pipeline_operand[3] = pipeline_read_data(source, 3);
		// Retain the old-address sample, update the source AR, then publish
		// the destination; source/destination AR aliasing must not re-read RAM.
		if (BIT(mode, 7) && !extended) indirect_modify(mode, false, 3);
		// SPRU131G table 7-24: MVDK/MVMD BRC has zero latency before
		// RPTB[D]. Publish the retained Access sample before younger Decode
		// initializes its loop; do not apply DAGEN-bank write deferral.
		if (brc && !mapped_write) data_write(destination, m_pipeline_operand[3], 3);
		if (destination >= 0x10 && destination <= 0x19)
		{
			bool const conflict = m_pipeline_valid[5] &&
				(staged_store(m_pipeline_word[5]) || staged_accumulator_mmr_store(m_pipeline_word[5]) || staged_rmw(m_pipeline_word[5]) || staged_shifted_memory_store(m_pipeline_word[5], m_pipeline_immediate[5])) &&
				m_pipeline_operand_address[5] >= 0x10 && m_pipeline_operand_address[5] <= 0x19;
			m_pipeline_early_write_deferred[3] = conflict;
			if (destination == 0x18) m_pipeline_direct_sp[3] = m_sp;
			if (!conflict && !mapped_write)
			{
				data_write(destination, m_pipeline_operand[3], 3);
				if (destination == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[3]);
			}
		}
	}
	if (m_pipeline_valid[3] && staged_memory_dp(m_pipeline_word[3]))
		m_pipeline_operand_address[3] = m_pipeline_word[3] == 0x46f8 ? m_pipeline_immediate[3] : basic_address(3, u8(m_pipeline_word[3]));
	if (m_pipeline_valid[3] && staged_mmr_load(m_pipeline_word[3]))
		m_pipeline_operand_address[3] = mmr_address(u8(m_pipeline_word[3]), 3);
	if (m_pipeline_valid[3] && (staged_dld(m_pipeline_word[3]) || staged_dst(m_pipeline_word[3]) || staged_long_arithmetic(m_pipeline_word[3])))
	{
		if (staged_extended_dld(m_pipeline_word[3]) || staged_extended_dst(m_pipeline_word[3]) || staged_extended_long_arithmetic(m_pipeline_word[3]))
		{
			// Table 5-4: MOD 12 retains AR, 13/14 premodify it, and
			// MOD 15 supplies an absolute address. Do not fetch again here.
			u8 const mode = m_pipeline_word[3];
			if (mode == 0xf8) m_pipeline_operand_address[3] = m_pipeline_immediate[3];
			else
				m_pipeline_operand_address[3] = long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_immediate[3]), 3);
		}
		else
		{
			u8 const mode = m_pipeline_word[3];
			bool const modifying = BIT(mode, 7);
			bool const preincrement = modifying && (mode & 0x78) == 0x18;
			if (preincrement) indirect_modify(mode, true, 3);
			m_pipeline_operand_address[3] = basic_address(3, mode);
			if (modifying && !preincrement) indirect_modify(mode, true, 3);
		}
		if (m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged long-word register/peripheral pair is unsupported\n", machine().describe_context());
			m_illegal = true;
			return;
		}
	}
	if (m_pipeline_valid[3] && staged_cmps(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		m_pipeline_operand_address[3] = basic_address(3, mode);
		if (m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged CMPS mapped/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (BIT(mode, 7)) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[4] && staged_cmps(m_pipeline_word[4]))
	{
		// SPRU172C 4-35 explicitly places comparison in Read, not Execute.
		u64 const source = accumulator(BIT(m_pipeline_word[4], 8));
		s16 const high = u16(source >> 16), low = u16(source);
		m_pipeline_immediate[4] = high <= low;
		m_pipeline_operand[4] = high > low ? u16(high) : u16(low);
	}
	// Load DAGEN and short AR modifications belong to Access (SPRU131G 7.1),
	// not retirement. Retain that address even if the AR changes before Read.
	if (m_pipeline_valid[3] && (staged_memory_load(m_pipeline_word[3]) || staged_memory_logic(m_pipeline_word[3]) ||
		(staged_rmw(m_pipeline_word[3]) && u8(m_pipeline_word[3]) != 0xf8)))
	{
		u8 const mode = m_pipeline_word[3];
		bool const offset_load = staged_offset_load(m_pipeline_word[3]);
		bool const offset_logic = staged_extended_logic(m_pipeline_word[3]) && mode != 0xf8;
		bool const offset_memory = offset_load || offset_logic;
		bool const preincrement = !offset_memory && BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode, false, 3);
		bool const absolute = staged_absolute_load(m_pipeline_word[3]) || staged_absolute_logic(m_pipeline_word[3]);
		bool const extended_rmw = staged_extended_rmw(m_pipeline_word[3]);
		m_pipeline_operand_address[3] = offset_memory ? long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_immediate[3]), 3) :
			extended_rmw ? long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_operand_address[3]), 3) :
			absolute ? m_pipeline_immediate[3] : basic_address(3, mode);
		bool const shifted_load = (m_pipeline_word[3] & 0xfe00) == 0x4400 || (m_pipeline_word[3] & 0xfe00) == 0x1600;
		if ((m_pipeline_word[3] & 0xfe00) == 0x1000 && mode >= 0xa0 && mode < 0xe0 && m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged indexed/circular LD mapped/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (staged_memory_logic(m_pipeline_word[3]) && mode >= 0xc0 && mode < 0xe0 && m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged circular logical mapped/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if ((m_pipeline_word[3] & 0xfe00) == 0x1400 && m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: staged TS load mapped/peripheral source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_operand_address[3]);
			m_illegal = true; return;
		}
		bool const unsigned_control = (m_pipeline_word[3] & 0xfe00) == 0x1200 &&
			(m_pipeline_operand_address[3] == 0 || m_pipeline_operand_address[3] == 7);
		bool const plain_peripheral = ((m_pipeline_word[3] & 0xfe00) == 0x1200 || (m_pipeline_word[3] & 0xfe00) == 0x1600) &&
			m_pipeline_operand_address[3] >= 0x20 && m_pipeline_operand_address[3] <= 0x5f && !offset_load;
		if (staged_memory_load(m_pipeline_word[3]) && (BIT(m_pipeline_word[3], 9) || shifted_load) && m_pipeline_operand_address[3] < 0x60 &&
			!unsigned_control && !plain_peripheral && (m_pipeline_operand_address[3] < (shifted_load ? 0x08 : 0x10) || m_pipeline_operand_address[3] > 0x17))
		{
			logerror("%s: staged memory load control/peripheral source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_operand_address[3]);
			m_illegal = true; return;
		}
		if (offset_load && m_pipeline_operand_address[3] < 0x60 &&
			(m_pipeline_operand_address[3] < (shifted_load ? 0x08 : 0x10) || m_pipeline_operand_address[3] > 0x17))
		{
			logerror("%s: staged long-offset LD control/peripheral source is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (extended_rmw && m_pipeline_operand_address[3] < 0x20)
		{
			logerror("%s: staged long-offset RMW register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (staged_memory_logic(m_pipeline_word[3]) && m_pipeline_repeat_active && m_pipeline_repeat_body_latched &&
			m_pipeline_address[3] == m_pipeline_repeat_address && m_pipeline_operand_address[3] < 0x60)
		{
			logerror("%s: repeated logical register/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (staged_memory_logic(m_pipeline_word[3]) && m_pipeline_operand_address[3] < (offset_logic ? 0x60 : 0x20) &&
			(offset_logic || m_pipeline_operand_address[3] < 8 || m_pipeline_operand_address[3] > 0x17))
		{
			logerror("%s: staged logical register/peripheral operand is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_operand_address[3]);
			m_illegal = true; return;
		}
		if ((m_pipeline_word[3] & 0xff00) == 0x6b00 && m_pipeline_operand_address[3] < (extended_rmw ? 0x20 : 0x60) &&
			m_pipeline_operand_address[3] != 0x0e &&
			!(!extended_rmw && mode < 0x88 && m_pipeline_operand_address[3] >= 0x10 && m_pipeline_operand_address[3] <= 0x17))
		{
			logerror("%s: staged ADDM register/peripheral destination is unsupported instruction=%06x word=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_operand_address[3]);
			m_illegal = true; return;
		}
		if (!absolute && !offset_memory && !extended_rmw && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 3);
	}
	// Only the ADD/SUB/LD extensions of the shifted family are qualified repeat bodies.
	if (m_pipeline_valid[3] && m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[3] == m_pipeline_repeat_address &&
		(m_pipeline_word[3] & 0xff00) == 0x6f00 && staged_shifted_memory_store(m_pipeline_word[3], m_pipeline_immediate[3]))
	{
		logerror("%s: repeated shifted store is unsupported\n", machine().describe_context()); m_illegal = true; return;
	}
	if (m_pipeline_valid[3] && (staged_smem_arithmetic(m_pipeline_word[3]) || staged_bitfield(m_pipeline_word[3]) || staged_smem_special(m_pipeline_word[3]) || (staged_shifted_smem(m_pipeline_word[3]) && !staged_shifted_memory_store(m_pipeline_word[3], m_pipeline_immediate[3]))))
	{
		u8 const mode = m_pipeline_word[3];
		bool const bitfield = staged_bitfield(m_pipeline_word[3]);
		bool const shifted = staged_shifted_smem(m_pipeline_word[3]);
		bool const special = staged_smem_special(m_pipeline_word[3]);
		bool const add_sub = staged_smem_arithmetic(m_pipeline_word[3]) && (m_pipeline_word[3] & 0xfe00) != 0x1e00;
		bool const extended = (shifted && mode >= 0xe0) || staged_extended_smem_arithmetic(m_pipeline_word[3]) || staged_extended_bitfield(m_pipeline_word[3]) || staged_extended_smem_special(m_pipeline_word[3]);
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode, false, 3);
		u16 const literal = bitfield || shifted ? m_pipeline_operand_address[3] : m_pipeline_immediate[3];
		m_pipeline_operand_address[3] = extended ? (mode == 0xf8 ? literal :
			long_offset_address(mode, indirect_ar(mode, 3), s16(literal), 3)) : basic_address(3, mode);
		u16 const source = m_pipeline_operand_address[3];
		if (m_pipeline_repeat_active && (m_pipeline_word[3] & 0xfe00) == 0 && source < 0x60)
		{
			logerror("%s: repeated ADD mapped/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if ((m_pipeline_repeat_active || m_pipeline_control_words) && square_accumulate(m_pipeline_word[3]) && source < 0x60)
		{
			logerror("%s: repeated square mapped/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (m_pipeline_repeat_active && (m_pipeline_word[3] & 0xf800) == 0x2800 && source < 0x60)
		{
			logerror("%s: repeated T-based MAC mapped/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (staged_poly(m_pipeline_word[3]) && source < 0x60)
		{
			logerror("%s: staged POLY mapped/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (staged_ts_arithmetic(m_pipeline_word[3]) && source < 0x60)
		{
			logerror("%s: staged TS arithmetic mapped/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if ((m_pipeline_word[3] & 0xff00) == 0x3200 && source < 0x60 && (source < 8 || source > 0x17))
		{
			logerror("%s: staged LD ASM control/peripheral source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), m_pipeline_address[3], m_pipeline_word[3], source); m_illegal = true; return;
		}
		if (((m_pipeline_word[3] & 0xfc00) == 0x3c00 || (m_pipeline_word[3] & 0xfc00) == 0x4000) && source < 0x60 && (source < 8 || source > 0x17))
		{
			logerror("%s: staged shift-16 memory arithmetic mapped source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], source); m_illegal = true; return;
		}
		if (staged_unsigned_memory_arithmetic(m_pipeline_word[3]) && source < 0x60 && (source < 8 || source > 0x17))
		{
			logerror("%s: staged unsigned memory arithmetic mapped-register source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), m_pipeline_address[3], m_pipeline_word[3], source); m_illegal = true; return;
		}
		bool const absolute_brc_compare = m_pipeline_word[3] == 0x60f8 && source == 0x1a;
		if ((m_pipeline_word[3] & 0xff00) == 0x6000 && source < 0x60 && (source < 8 || source > 0x17) && !absolute_brc_compare)
		{
			logerror("%s: staged CMPM mapped-register source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], source);
			m_illegal = true; return;
		}
		// SUBC never writes T/TRN, so those mapped sources are plain Read samples.
		bool const subc_t = (m_pipeline_word[3] & 0xfe00) == 0x1e00 && (source == 0x0e || source == 0x0f);
		if ((m_pipeline_word[3] & 0xfe00) == 0x1e00 && m_pipeline_repeat_active && source < 0x60 && !subc_t)
		{
			logerror("%s: repeated SUBC register/peripheral source is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		bool const shifted_bk = source == 0x19 && shifted && mode == 0xf8 && staged_shifted_memory_arithmetic(m_pipeline_immediate[3]);
		bool const bk_arithmetic = source == 0x19 && ((m_pipeline_word[3] & 0xfe00) == 0x0000 || (m_pipeline_word[3] & 0xfe00) == 0x0800 || (m_pipeline_word[3] & 0xf800) == 0x2800 || shifted_bk);
		if (shifted_bk && (m_pipeline_repeat_active || m_pipeline_block_body[3] || m_pipeline_xc_active || m_pipeline_control_words))
		{
			logerror("%s: staged shifted BK arithmetic repeat/control composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const shifted_peripheral = staged_shifted_memory_load(m_pipeline_word[3], m_pipeline_immediate[3]) && source >= 0x20 && source <= 0x5f;
		// These gaps are not owned by internal_data_map: retain the ordinary
		// board-bus read instead of inventing a CPU register or return value.
		bool const shifted_unowned = staged_shifted_memory_load(m_pipeline_word[3], m_pipeline_immediate[3]) && source >= 2 && source <= 5;
		if (shifted_peripheral && (m_pipeline_repeat_active || m_pipeline_block_body[3] || m_pipeline_xc_active || m_pipeline_control_words))
		{
			logerror("%s: staged shifted peripheral load repeat/control composition is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (source < 0x60 && (source < (special || shifted || add_sub || bitfield ? 0x08 : 0x10) || source > 0x17) && !subc_t && !absolute_brc_compare && !bk_arithmetic && !shifted_peripheral && !shifted_unowned && !(bitfield && (source == 6 || source == 7 || source >= 0x20)))
		{
			logerror("%s: staged %s control/peripheral source is unsupported instruction=%06x last_fetch_physical=%06x word=%04x source=%04x pmst=%04x\n", machine().describe_context(), shifted ? "shifted load" : special ? "Smem special" : bitfield ? "BITF" : "memory ADD/SUB", unsigned(m_pipeline_address[3]), unsigned(m_pipeline_physical_address[3]), m_pipeline_word[3], source, m_pmst);
			m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[3] && staged_port_write(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		bool const extended = mode >= 0xe0;
		unsigned const ar = indirect_ar(mode, 3);
		u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode, false, 3);
		if (mode != 0xf8) m_pipeline_operand_address[3] = extended ?
			long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_operand_address[3]), 3) : basic_address(3, mode);
		bool const repeat_operand = m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[3] == m_pipeline_repeat_address;
		if (m_pipeline_operand_address[3] < 6 || (repeat_operand && m_pipeline_operand_address[3] < 0x60))
		{
			m_ar[ar] = previous_ar; m_st0 = previous_st0;
			logerror("%s: staged PORTW low control/reserved source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[3] && staged_port_read(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		bool const extended = mode >= 0xe0;
		unsigned const ar = indirect_ar(mode, 3);
		u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		if (mode != 0xf8) m_pipeline_operand_address[3] = extended ?
			long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_operand_address[3]), 3) : basic_address(3, mode);
		u16 const destination = m_pipeline_operand_address[3];
		bool const repeat_operand = m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[3] == m_pipeline_repeat_address;
		if (destination < 0x60 && (repeat_operand || ((destination < 8 || destination > 0x17) && destination != 0x30)))
		{
			m_ar[ar] = previous_ar; m_st0 = previous_st0;
			logerror("%s: staged PORTR control/DAGEN/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7)) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[3] && staged_firs(m_pipeline_word[3]))
		m_pipeline_extension_first[3] = m_pipeline_immediate[3];
	if (m_pipeline_valid[3] && staged_coefficient_mac(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		unsigned const ar = indirect_ar(mode, 3);
		u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		m_pipeline_extension_first[3] = m_pipeline_immediate[3];
		u16 const source = mode == 0xf8 ? m_pipeline_operand_address[3] : mode >= 0xe0 ?
			long_offset_address(mode, ar, s16(m_pipeline_operand_address[3]), 3) : basic_address(3, mode);
		if (source < 0x60 || (BIT(m_pipeline_word[3], 9) && source == 0xffff))
		{
			m_ar[ar] = previous_ar; m_st0 = previous_st0;
			logerror("%s: staged coefficient MAC mapped/wrapping operand is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		m_pipeline_operand_address[3] = source;
		if (mode >= 0x80 && mode < 0xe0) indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[4] && staged_coefficient_mac(m_pipeline_word[4]))
	{
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		pipeline_request_coefficient(m_pipeline_extension_first[4]);
	}
	if (m_pipeline_valid[3] && (staged_dual_multiply(m_pipeline_word[3]) || staged_lms(m_pipeline_word[3]) || staged_firs(m_pipeline_word[3]) || staged_load_mac(m_pipeline_word[3]) || staged_dual_arithmetic(m_pipeline_word[3]) || staged_distance(m_pipeline_word[3])))
	{
		u8 const x = m_pipeline_word[3] >> 4, y = m_pipeline_word[3];
		unsigned const xar = 2 + (x & 3), yar = 2 + (y & 3);
		m_pipeline_operand_address[3] = m_ar[xar];
		m_pipeline_immediate[3] = m_ar[yar];
		bool const mapped_alu = staged_dual_arithmetic(m_pipeline_word[3]);
		bool const interrupt_multiply = (m_pipeline_word[3] & 0xfc00) == 0xa400;
		// Like mapped ALU sources, a dual multiply samples AR0..AR7 in Read after
		// Access. It never touches the other accumulator in Execute, so those
		// mapped words are plain Read samples too; its own accumulators are not.
		u16 const word = m_pipeline_word[3];
		bool const multiply = staged_dual_multiply(word), signed_mac = (word & 0xf000) == 0xb000;
		auto const mapped_multiply = [word, multiply, signed_mac](u16 address) {
			bool const b = address >= 0x0b;
			return multiply && ((address >= 0x10 && address <= 0x17) ||
				(address >= 8 && address <= 0x0d && b != BIT(word, 8) && (!signed_mac || b != BIT(word, 9))));
		};
		auto const unowned = [mapped_alu, interrupt_multiply, &mapped_multiply](u16 address) {
			return address < 0x60 && !(interrupt_multiply && address < 2) && !mapped_multiply(address) &&
				(!mapped_alu || (address > 5 && (address < 8 || address > 0x17)));
		};
		if (unowned(m_pipeline_operand_address[3]) || unowned(m_pipeline_immediate[3]))
		{
			logerror("%s: staged dual arithmetic/multiply low-page register/peripheral operand is unsupported instruction=%06x word=%04x x=%04x y=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_operand_address[3], m_pipeline_immediate[3]); m_illegal = true; return;
		}
		// Both reads use the original address; Xmod wins when ARs alias
		// even if Ymod differs (SPRU131G 5.5.4, page 5-19).
		dual_modify(x, 3);
		if (xar != yar) dual_modify(y, 3);
	}
	if (m_pipeline_valid[4] && (staged_dual_multiply(m_pipeline_word[4]) || staged_lms(m_pipeline_word[4]) || staged_firs(m_pipeline_word[4]) || staged_load_mac(m_pipeline_word[4]) || staged_dual_arithmetic(m_pipeline_word[4]) || staged_distance(m_pipeline_word[4])))
	{
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		m_pipeline_immediate[4] = pipeline_read_data(m_pipeline_immediate[4]);
		if (staged_firs(m_pipeline_word[4]))
		{
			m_pipeline_t[4] = m_pipeline_immediate[4]; // Retain Y while the program bus supplies the coefficient.
			pipeline_request_coefficient(m_pipeline_extension_first[4]);
		}
	}
	if (m_pipeline_valid[3] && staged_conditional_store(m_pipeline_word[3]))
	{
		u8 const x = m_pipeline_word[3] >> 4;
		m_pipeline_operand_address[3] = m_ar[2 + (x & 3)];
		if (m_pipeline_operand_address[3] < 0x60 && (m_pipeline_operand_address[3] < 0x10 || m_pipeline_operand_address[3] > 0x17))
		{
			logerror("%s: staged conditional store mapped destination is unsupported instruction=%06x word=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_operand_address[3]); m_illegal = true; return;
		}
		dual_modify(x, 3);
	}
	if (m_pipeline_valid[4] && staged_conditional_store(m_pipeline_word[4]))
	{
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		m_pipeline_immediate[4] = m_st1 & 31;
	}
	if (m_pipeline_valid[3] && (staged_parallel_alu(m_pipeline_word[3]) || staged_parallel_product(m_pipeline_word[3]) || staged_parallel_load(m_pipeline_word[3])))
	{
		u8 const x = m_pipeline_word[3] >> 4, y = m_pipeline_word[3];
		unsigned const xar = 2 + (x & 3), yar = 2 + (y & 3);
		m_pipeline_immediate[3] = m_ar[xar];
		m_pipeline_operand_address[3] = m_ar[yar];
		bool const mapped_load_source = staged_parallel_load(m_pipeline_word[3]) && m_pipeline_immediate[3] < 0x60 && !m_pipeline_repeat_active;
		// No parallel family reads TRN, so storing to it is an ordinary Execute write.
		bool const trn_destination = m_pipeline_operand_address[3] == 0x0f && !m_pipeline_repeat_active;
		if ((!mapped_load_source && m_pipeline_immediate[3] < 0x60) || (m_pipeline_operand_address[3] < 0x60 && !trn_destination))
		{
			logerror("%s: staged parallel store mapped operand is unsupported instruction=%06x word=%04x source=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_immediate[3], m_pipeline_operand_address[3]); m_illegal = true; return;
		}
		dual_modify(x, 3);
		if (xar != yar) dual_modify(y, 3);
	}
	if (m_pipeline_valid[4] && (staged_parallel_alu(m_pipeline_word[4]) || staged_parallel_product(m_pipeline_word[4]) || staged_parallel_load(m_pipeline_word[4])))
	{
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_immediate[4]);
		m_pipeline_direct_status[4] = m_st1 & 31;
	}
	if (m_pipeline_valid[3] && staged_mvdd(m_pipeline_word[3]))
	{
		u8 const x = m_pipeline_word[3] >> 4, y = m_pipeline_word[3];
		unsigned const xar = 2 + (x & 3), yar = 2 + (y & 3);
		m_pipeline_immediate[3] = m_ar[xar];
		m_pipeline_operand_address[3] = m_ar[yar];
		bool const peripheral_destination = m_pipeline_operand_address[3] >= 0x20 && m_pipeline_operand_address[3] < 0x60;
		bool const sp_destination = m_pipeline_word[3] == 0xe51a && m_pipeline_operand_address[3] == 0x18 &&
			m_pipeline_immediate[3] >= 0x60 && !m_pipeline_repeat_active;
		bool const accumulator_destination = m_pipeline_operand_address[3] >= 8 && m_pipeline_operand_address[3] <= 0x0d &&
			m_pipeline_immediate[3] >= 0x60 && !m_pipeline_repeat_active;
		if ((m_pipeline_operand_address[3] < 0x20 && !sp_destination && !accumulator_destination) ||
			(peripheral_destination && (m_pipeline_repeat_active || m_pipeline_immediate[3] < 0x60)))
		{
			logerror("%s: staged MVDD register/peripheral operand is unsupported instruction=%06x word=%04x source=%04x destination=%04x repeated=%u\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], m_pipeline_immediate[3], m_pipeline_operand_address[3], unsigned(m_pipeline_repeat_active)); m_illegal = true; return;
		}
		dual_modify(x, 3);
		if (xar != yar) dual_modify(y, 3);
	}
	if (m_pipeline_valid[4] && staged_mvdd(m_pipeline_word[4]))
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_immediate[4]);
	if (m_pipeline_valid[3] && (staged_xmem_bit(m_pipeline_word[3]) || staged_xmem_load(m_pipeline_word[3]) || staged_xmem_arithmetic(m_pipeline_word[3])))
	{
		u8 const xmem = m_pipeline_word[3] >> 4;
		m_pipeline_operand_address[3] = m_ar[2 + (xmem & 3)];
		bool const accumulator_bit = staged_xmem_bit(m_pipeline_word[3]) && !(xmem & 12) &&
			m_pipeline_operand_address[3] >= 8 && m_pipeline_operand_address[3] <= 13;
		if (m_pipeline_operand_address[3] < 0x60 && !accumulator_bit)
		{
			logerror("%s: staged Xmem register/peripheral source is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		dual_modify(xmem, 3);
	}
	if (m_pipeline_valid[3] && staged_absolute_add(m_pipeline_word[3]))
	{
		m_pipeline_operand_address[3] = m_pipeline_immediate[3];
		u16 const source = m_pipeline_operand_address[3];
		// Absolute SP is a Read-owned data operand, not a compiler-mode
		// address base (SPRU131G 7.1/7.5.5). Retain it before younger
		// stack updates; do not apply the direct-address SP latency latch.
		if (source < 0x60 && (source < 0x08 || source > 0x19))
		{
			logerror("%s: staged absolute ADD control/peripheral source is unsupported instruction=%06x word=%04x source=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[3]), m_pipeline_word[3], source); m_illegal = true; return;
		}
	}
	if (m_pipeline_valid[3] && staged_memory_immediate_multiply(m_pipeline_word[3]))
	{
		u8 const mode = m_pipeline_word[3];
		bool const extended = mode >= 0xe0, preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		if (preincrement) indirect_modify(mode, false, 3);
		m_pipeline_operand_address[3] = extended ? mode == 0xf8 ? m_pipeline_operand_address[3] :
			long_offset_address(mode, indirect_ar(mode, 3), s16(m_pipeline_operand_address[3]), 3) : basic_address(3, mode);
		u16 const source = m_pipeline_operand_address[3];
		// Only accumulator fields, T/TRN and ARs have mapped-source coverage.
		if (source < 0x60 && (source < 8 || source > 0x17))
		{
			logerror("%s: staged memory/immediate MPY control/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 3);
	}
	bool const read_memory_operand = m_pipeline_valid[4] && (BIT(staged_opcode_descriptor[m_pipeline_word[4]], 7) ||
		(staged_shifted_smem(m_pipeline_word[4]) && !staged_shifted_memory_store(m_pipeline_word[4], m_pipeline_immediate[4])));
	// SPRU131G table 7-28: two-word immediate ALU/MPY instructions have zero mapped-accumulator
	// consumer latency. Finish this retained Read after its older Execute;
	// do not change the one-word producer restriction or other bus reads.
	bool const late_mapped_accumulator_read = read_memory_operand && m_pipeline_valid[5] &&
		(staged_immediate_multiply(m_pipeline_word[5]) || staged_immediate_alu(m_pipeline_word[5])) &&
		m_pipeline_operand_address[4] >= 0x08 && m_pipeline_operand_address[4] <= 0x0d &&
		(m_pipeline_operand_address[4] >= 0x0b) == bool(BIT(m_pipeline_word[5], 8));
	bool const smem_t_product_read = m_pipeline_valid[4] && staged_smem_special(m_pipeline_word[4]) &&
		(m_pipeline_word[4] & 0xf000) == 0x2000;
	// Multiplication has zero T-producer latency (SPRU131G 7.5.6).
	// A mapped T operand must see the same older publication as dedicated T,
	// while retaining exactly one bus read and excluding Category-I consumers.
	bool const late_mapped_t_read = read_memory_operand && smem_t_product_read && m_pipeline_operand_address[4] == 0x0e;
	if (read_memory_operand && !late_mapped_accumulator_read && !late_mapped_t_read && !retained_ar_operand_read)
	{
		if (peripheral.defer_clock_read) m_pipeline_peripheral_read_pending = true;
		else m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		if (staged_memory_immediate_multiply(m_pipeline_word[4])) m_t = m_pipeline_operand[4];
	}
	// TRAP shares vector/stack entry, but preserves INTM and IFR (SPRU172C 4-193).
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xffe0) == 0xf7c0)
	{
		unsigned const vector = m_pipeline_word[4] & 31;
		m_st1 |= 0x0800;
		if (vector >= 16) m_ifr &= ~u16(1U << (vector - 16));
	}
	if (m_pipeline_valid[4] && staged_bitt(m_pipeline_word[4]))
		// Like TS consumers, retain T in Read rather than observing a
		// younger publication during Execute (SPRU131G 7.5.6).
		m_pipeline_immediate[4] = m_t;
	if (m_pipeline_valid[4] && staged_norm(m_pipeline_word[4]))
	{
		// SPRU172C 4-122 samples TS in Read. EXP needs one separating
		// word (SPRU131G table 7-13); do not sample after older Execute.
		m_pipeline_immediate[4] = m_t;
	}
	if (m_pipeline_valid[4] && ((m_pipeline_word[4] & 0xfe00) == 0x1400 || staged_ts_arithmetic(m_pipeline_word[4])))
	{
		// TS belongs to Read. Table 7-13's Category-I restriction applies
		// to short forms; extension clocks already separate long consumers.
		m_pipeline_immediate[4] = m_t;
		unsigned const code = m_t & 63;
		if (staged_ts_arithmetic(m_pipeline_word[4]) && code >= 32 && code < 48)
		{
			logerror("%s: staged TS arithmetic shift outside documented -16..31 range\n", machine().describe_context()); m_illegal = true; return;
		}
	}
	if (m_pipeline_valid[4] && ((m_pipeline_word[4] & 0xfeff) == 0xf066 || (m_pipeline_word[4] & 0xfcff) == 0xf067))
		m_pipeline_operand[4] = m_t;
	if (m_pipeline_valid[4] && staged_port_read(m_pipeline_word[4]))
		m_pipeline_operand[4] = m_io.read_word(m_pipeline_immediate[4]);
	if (m_pipeline_valid[4] && (staged_dld(m_pipeline_word[4]) || staged_long_arithmetic(m_pipeline_word[4])))
	{
		m_pipeline_t[4] = m_t;
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		m_pipeline_immediate[4] = pipeline_read_data(m_pipeline_operand_address[4] ^ 1);
	}
	if (m_pipeline_valid[4] && staged_direct_push(m_pipeline_word[4]))
	{
		// Class 16A: retain the source read and stack destination separately.
		// Read-owned SP allocation is a declared partial stage model;
		// Indirect mapped sources are qualified separately from stack aliases.
		u8 const mode = m_pipeline_word[4];
		unsigned const ar = indirect_ar(mode, 4);
		u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		late_pushm_address = mode < 0x80 ? mode : mmr_address(mode, 4);
		if (mode >= 0x80 && late_pushm_address != 0x0f)
		{
			m_ar[ar] = previous_ar; m_st0 = previous_st0;
			logerror("%s: staged indirect PSHM source is unqualified\n", machine().describe_context()); m_illegal = true; return;
		}
		bool const stack_alias = late_pushm_address >= 0x60 && m_pipeline_valid[5] &&
			staged_direct_push(m_pipeline_word[5]) && m_pipeline_operand_address[5] == late_pushm_address;
		bool const t_publication = late_pushm_address == 0x0e && m_pipeline_valid[5] &&
			(m_pipeline_word[5] == 0x8a0e || (m_pipeline_word[5] & 0xfeff) == 0x880e);
		late_pushm_read = stack_alias || t_publication;
		if (!late_pushm_read) m_pipeline_operand[4] = data_read(late_pushm_address);
		m_pipeline_operand_address[4] = --m_sp;
	}
	if (m_pipeline_valid[4] && ((m_pipeline_word[4] & 0xff00) == 0x8a00 || (m_pipeline_word[4] & 0xff00) == 0x8b00))
	{
		// Class 17A: retain the stack word before destination publication.
		// Read-owned SP increment is a partial placement model. In-body
		// repeat-control writes remain excluded; separated outside-body
		// restoration is covered. POPD still excludes SP, ST0/ST1 and
		// peripheral destinations; active PMST changes are checked below.
		if ((m_pipeline_word[4] & 0xff00) == 0x8b00 && m_sp < 0x60)
		{
			logerror("%s: staged POPD register/peripheral stack source is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if ((m_pipeline_word[4] & 0xff00) == 0x8a00 && m_pipeline_operand_address[4] == 0x18)
		{
			m_pipeline_direct_sp[4] = m_sp;
			pipeline_delay_compiler_sp(m_sp);
		}
		late_popm_address = m_sp++;
		late_popm_read = (m_pipeline_word[4] & 0xff00) == 0x8a00 && late_popm_address >= 0x60 && m_pipeline_valid[5] &&
			staged_direct_push(m_pipeline_word[5]) && m_pipeline_operand_address[5] == late_popm_address;
		if (!late_popm_read) m_pipeline_operand[4] = data_read(late_popm_address);
		u16 const destination = m_pipeline_operand_address[4];
		if ((m_pipeline_word[4] & 0xff00) == 0x8b00 && destination == 0x1d && m_pipeline_block_active && m_pipeline_operand[4] != m_pmst)
		{
			// The active-context contract covers restoration without remapping.
			// Changing PMST while retaining a loop needs separate Prefetch tests.
			logerror("%s: staged active-context POPD PMST mapping change is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (destination >= 0x10 && destination <= 0x19)
		{
			// Table 7-4 gives POPM/POPD DAGEN an early Read write; an older
			// store using that register bank defers publication one clock.
			bool const conflict = m_pipeline_valid[5] && (staged_store(m_pipeline_word[5]) || staged_accumulator_mmr_store(m_pipeline_word[5]) || staged_rmw(m_pipeline_word[5]) || staged_shifted_memory_store(m_pipeline_word[5], m_pipeline_immediate[5])) &&
				m_pipeline_operand_address[5] >= 0x10 && m_pipeline_operand_address[5] <= 0x19;
			m_pipeline_early_write_deferred[4] = conflict;
			bool const own_pointer = popd_own_pointer(m_pipeline_word[4], destination) && m_pipeline_repeat_active && m_pipeline_address[4] == m_pipeline_repeat_address;
			if (!conflict && !late_popm_read && !own_pointer)
			{
				data_write(destination, m_pipeline_operand[4], 4);
				if (destination == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[4]);
			}
		}
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xff00) == 0xee00)
	{
		// FRAME has zero stack-operation latency (SPRU172C 4-60), but
		// must not retarget an older PSHM/POPM's Read-owned stack access.
		// Access placement remains partial; order shared SP updates by age.
		u16 const previous = m_sp;
		m_sp = u16(m_sp + s8(m_pipeline_word[3]));
		pipeline_delay_compiler_sp(previous);
	}
	if (m_pipeline_valid[3] && staged_immediate_arp(m_pipeline_word[3]))
		// Tables 7-3/7-15: no latency to the following indirect consumer.
		// Publish once after older stack reads; Access phase is partial.
		m_st0 = (m_st0 & 0x1fff) | ((m_pipeline_word[3] & 7) << 13);
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x4b00)
		// Same-address pending RMW data uses the internal bypass, not stale RAM.
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand[4]);
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
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xfffe) == 0xf6e4)
	{
		// As for near returns, an older POPM/POPD must finish its Read-owned
		// SP update before this younger Access consumes the next stack word.
		// The delayed class-34 bus placement remains a partial timing model.
		m_pipeline_operand[3] = data_read(m_sp++);
	}
	if (m_pipeline_valid[3] && staged_conditional_return(m_pipeline_word[3]))
	{
		// Retain sequential Prefetch while the condition awaits the older
		// producer. Decode stays flushed until the Read decision is known.
		m_pipeline_fetch_address = (m_pipeline_address[3] & 0x7f0000) | u16(m_pipeline_address[3] + 1);
		m_pipeline_flush_front = false;
	}
	if (m_pipeline_valid[4] && staged_memory_repeat(m_pipeline_word[4]))
	{
		u8 const mode = m_pipeline_word[4];
		// Class 5A/5B samples RC once in Read, not once per body iteration.
		bool const extended = mode >= 0xe0;
		u16 const source = mode == 0xf8 ? m_pipeline_immediate[4] : extended ?
			long_offset_address(mode, indirect_ar(mode, 4), s16(m_pipeline_immediate[4]), 4) : basic_address(4, mode);
		if (source < 0x60 && (source < 8 || source > 0x19))
		{
			logerror("%s: staged RPT control/peripheral source is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		m_pipeline_operand[4] = pipeline_read_data(source);
		if (m_pipeline_repeat_body_latched && staged_pipelined_repeat(m_pipeline_repeat_word) &&
			m_pipeline_repeat_address == ((m_pipeline_address[4] & 0x7f0000) | u16(m_pipeline_address[4] + (extended ? 2 : 1))))
			m_pipeline_repeat_issue_remaining = m_pipeline_operand[4];
		if (!extended && BIT(mode, 7)) indirect_modify(mode, false, 4);
		m_pipeline_memory_repeat_wait = 2; // Class 5A/5B's extra clocks; placement is partial.
	}
	if (m_pipeline_valid[4] && staged_data_to_program(m_pipeline_word[4]))
	{
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		// Source addressing is complete. Reuse this slot's retained fields for
		// the physical destination so adjacent transfers cannot share a latch.
		u32 const target = physical_program_address((m_pipeline_address[4] & 0x7f0000) | m_pipeline_immediate[4]);
		m_pipeline_operand_address[4] = u16(target);
		m_pipeline_immediate[4] = u16(target >> 16);
		// Four/five-clock standalone totals; exact silicon bus phase is partial.
		m_pipeline_mvdp_wait = 2;
	}
	if (m_pipeline_valid[4] && staged_write_a(m_pipeline_word[4]))
	{
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		m_pipeline_write_a_wait = 4;
	}
	if (m_pipeline_valid[4] && staged_program_move(m_pipeline_word[4]))
	{
		u8 const mode = m_pipeline_word[4];
		bool const extended = staged_extended_program_move(m_pipeline_word[4]);
		bool const preincrement = !extended && BIT(mode, 7) && (mode & 0x78) == 0x18;
		unsigned const ar = indirect_ar(mode, 4);
		u16 const previous_ar = m_ar[ar], previous_st0 = m_st0;
		u16 const first_extension = m_pipeline_operand_address[4];
		if (preincrement) indirect_modify(mode, false, 4);
		m_pipeline_operand_address[4] = mode == 0xf8 ? first_extension : extended ?
			long_offset_address(mode, ar, s16(first_extension), 4) : basic_address(4, mode);
		bool const accumulator_destination = m_pipeline_operand_address[4] >= 8 && m_pipeline_operand_address[4] <= 0x0d &&
			!m_pipeline_repeat_active;
		if (m_pipeline_operand_address[4] < 0x60 && !accumulator_destination)
		{
			// An unsupported endpoint is not a hardware trap: undo a long
			// address preview before rejecting it without a program read.
			m_ar[ar] = previous_ar; m_st0 = previous_st0;
			logerror("%s: staged MVPD register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
		if (!extended && BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 4);
		m_pipeline_program_move_pending = true;
	}
	if (m_pipeline_valid[3] && (m_pipeline_word[3] & 0xfff8) == 0x7690)
	{
		// SPRU131G figure 7-1 separates Access's AR update from Read's
		// write-address presentation. MVMM samples its source in Decode
		// (SPRU172C 4-116), so an adjacent copy must see this increment.
		u8 const mode = m_pipeline_word[3];
		m_pipeline_operand_address[3] = basic_address(3, mode);
		indirect_modify(mode, false, 3);
	}
	if (m_pipeline_valid[4] && (m_pipeline_word[4] & 0xff00) == 0x7600)
	{
		u8 const mode = m_pipeline_word[4];
		bool const retained_ar_alias = (m_pipeline_word[4] & 0xfff8) == 0x7680 && !indirect_compatibility(4) &&
			m_pipeline_operand_address[4] >= 0x10 && m_pipeline_operand_address[4] <= 0x17;
		if (staged_extended_immediate_store(m_pipeline_word[4]))
		{
			// Three-word Decode retains the offset separately from its value.
			// Share DST/DSUB address arithmetic; never fetch here.
			m_pipeline_operand_address[4] = long_offset_address(mode, indirect_ar(mode, 4), s16(m_pipeline_operand_address[4]), 4);
			if (m_pipeline_operand_address[4] < 0x20)
			{
				logerror("%s: staged long-offset ST register/peripheral destination is unsupported instruction=%06x word=%04x destination=%04x\n", machine().describe_context(), unsigned(m_pipeline_address[4]), m_pipeline_word[4], m_pipeline_operand_address[4]);
				m_illegal = true; return;
			}
		}
		else if (!retained_ar_alias && mode != 0xf8 && (m_pipeline_word[4] & 0xfff8) != 0x7690 &&
			(m_pipeline_word[4] & 0xfff8) != 0x7688)
		{
			bool const preincrement = BIT(mode, 7) && (mode & 0x78) == 0x18;
			if (preincrement) indirect_modify(mode, false, 4);
			m_pipeline_operand_address[4] = basic_address(4, mode);
			if (staged_indexed_immediate_store(m_pipeline_word[4]) && m_pipeline_operand_address[4] < 0x60)
			{
				logerror("%s: staged indexed immediate ST register/peripheral destination is unsupported\n", machine().describe_context()); m_illegal = true; return;
			}
			if (BIT(mode, 7) && !preincrement) indirect_modify(mode, false, 4);
		}
		if (!retained_ar_alias && m_pipeline_operand_address[4] >= 0x10 && m_pipeline_operand_address[4] <= 0x19 && mode != 0xf8 && (mode >= 0x80 || BIT(m_pipeline_direct_status[4], 14)))
		{
			logerror("%s: staged immediate ST to DAGEN requires early-write ownership instruction=%06x word=%04x destination=%04x mode=%02x ar4=%04x sp=%04x\n",
				machine().describe_context(), m_pipeline_address[4], m_pipeline_word[4], m_pipeline_operand_address[4], mode, m_ar[4], m_sp);
			m_illegal = true;
			return;
		}
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[4] == m_pipeline_repeat_address && m_pipeline_operand_address[4] < 0x60)
		{
			logerror("%s: repeated immediate store register/peripheral destination is unsupported\n", machine().describe_context());
			m_illegal = true; return;
		}
	}
	// STM status is not a DAGEN early write. Read publication gives the
	// two-instruction CPL/CMPT latency in SPRU131G tables 7-16/7-19.
	if (m_pipeline_valid[4] && m_pipeline_word[4] == 0x7707)
		data_write(7, m_pipeline_immediate[4], 4);
	if (m_pipeline_valid[4] && (m_pipeline_word[4] == 0xfc00 || m_pipeline_word[4] == 0xf4eb || m_pipeline_word[4] == 0xfe00))
	{
		// Table 7-2: a DARAM stack Read precedes any same-clock EB write.
		// A just-pushed word requires a separating instruction, not a bypass.
		m_pipeline_operand[4] = data_read(m_pipeline_operand_address[4]);
		if (m_pipeline_word[4] == 0xfe00)
			m_pipeline_retd_target = (m_pipeline_address[4] & 0x7f0000) | m_pipeline_operand[4];
	}
	if (m_pipeline_valid[4])
		// Tables 7-20/7-22: short SXM/ASM consumers sample before an
		// older store retires. Long-offset Decode adds its documented gap.
		m_pipeline_alu_mode[4] = m_st1 & 0x011f;
	// Older Execute follows operand capture, but precedes Read-owned register
	// publications below. Preserve this order when separating further phases.
	if (!pipeline_execute()) return;

	if (m_pipeline_valid[4] && m_pipeline_valid[5] &&
		(staged_immediate_asm(m_pipeline_word[5]) || (m_pipeline_word[5] & 0xff00) == 0x3200))
		m_pipeline_alu_mode[4] = (m_pipeline_alu_mode[4] & ~u16(0x001f)) | (m_st1 & 0x001f);
	if (m_pipeline_valid[4] && staged_asm_store(m_pipeline_word[4]) && m_pipeline_valid[5] &&
		(staged_immediate_asm(m_pipeline_word[5]) || (m_pipeline_word[5] & 0xff00) == 0x3200))
		// SPRU131G example 7-52: dedicated ASM loads forward with no gap;
		// ordinary ST1 writes retain the pre-Execute Read sample (table 7-22).
		m_pipeline_immediate[4] = m_st1 & 0x1f;
	if (late_pushm_read)
		// SPRU131G 7.3.2 resolves the RAM alias; table 7-13 gives PSHM
		// zero T latency. Consume the older publication exactly once.
		m_pipeline_operand[4] = data_read(late_pushm_address);
	if (late_popm_read)
	{
		// SPRU131G 7.3.2: a same-location on-chip read can consume a
		// pending write through an internal bus. Represent PSHM->POPM by
		// sampling after the older Execute in this same clock; the stack
		// address and SP increment still belong to the pop's Read stage.
		m_pipeline_operand[4] = data_read(late_popm_address);
		u16 const destination = u8(m_pipeline_word[4]);
		if (destination >= 0x10 && destination <= 0x19 && !m_pipeline_early_write_deferred[4])
		{
			data_write(destination, m_pipeline_operand[4], 4);
			if (destination == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[4]);
		}
	}
	if (late_mapped_accumulator_read || late_mapped_t_read)
	{
		m_pipeline_operand[4] = pipeline_read_data(m_pipeline_operand_address[4]);
		if (staged_memory_immediate_multiply(m_pipeline_word[4])) m_t = m_pipeline_operand[4];
	}
	if (m_pipeline_valid[4] && staged_move(m_pipeline_word[4]) &&
		(m_pipeline_immediate[4] >= 0x60 || staged_clock_destination_move(m_pipeline_word[4], m_pipeline_immediate[4]) || staged_t_register_move(m_pipeline_word[4], m_pipeline_immediate[4]) || staged_accumulator_destination_move(m_pipeline_word[4], m_pipeline_immediate[4])))
		// Retained MVDK data writes use the same clock for RAM and accumulator
		// fields; register_write preserves siblings and masks AG/BG to 8 bits.
		// Table 7-3: non-accumulator T moves are visible to the next consumer.
		// CLKMD uses this retained write after its separately paid access clock.
		m_data.write_word(m_pipeline_immediate[4], m_pipeline_operand[4]);
	if (m_pipeline_valid[4] && (m_pipeline_word[4] == 0x7308 || m_pipeline_word[4] == 0x730b) && m_pipeline_immediate[4] == 7)
		// Publish at the move's Read clock, not its later Execute clock:
		// SPRU131G table 7-14 permits an immediately following RPTB.
		data_write(7, m_pipeline_operand[4], 4);
	if (m_pipeline_valid[3] && staged_move(m_pipeline_word[3]) &&
		(pipeline_mapped_accumulator_write(m_pipeline_operand_address[3]) ||
		 (m_pipeline_valid[5] && staged_mvmd_cleared_source(m_pipeline_word[3], m_pipeline_word[5]))))
	{
		// SPRU131G 7.5.10/example 7-67: a mapped write followed by a
		// mapped read has no accumulator conflict. Sample after older mapped
		// Execute, without changing direct-ALU producer latency. RPTZ's
		// sequence-opening clear likewise precedes the first MVMD sample.
		m_pipeline_operand[3] = m_data.read_word(m_pipeline_operand_address[3]);
		u16 const destination = m_pipeline_immediate[3];
		if (((destination >= 0x10 && destination <= 0x19) || destination == 0x1a) && !m_pipeline_early_write_deferred[3])
		{
			data_write(destination, m_pipeline_operand[3], 3);
			if (destination == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[3]);
		}
	}
	if (m_pipeline_valid[2] && staged_accumulator_far_call(m_pipeline_word[2]))
		m_pipeline_far_call_page[2] = (m_pipeline_address[2] >> 16) & 0x7f;
	if (m_pipeline_valid[3] && staged_accumulator_call(m_pipeline_word[3]))
	{
		m_pipeline_operand_address[3] = --m_sp;
		m_pipeline_immediate[3] = u16(m_pipeline_address[3] + (BIT(m_pipeline_word[3], 9) ? 3 : 1));
	}
	if (m_pipeline_valid[4] && staged_accumulator_transfer(m_pipeline_word[4]))
	{
		bool const far = staged_accumulator_far_call(m_pipeline_word[4]) || staged_accumulator_far_branch(m_pipeline_word[4]);
		u64 const source = accumulator(BIT(m_pipeline_word[4], 8));
		m_pipeline_acc_transfer_target = far ? source & 0x7fffff : (m_pipeline_address[4] & 0x7f0000) | u16(source);
		if (staged_accumulator_call(m_pipeline_word[4]))
		{
			if (far) m_pipeline_far_call_stack[4] = --m_sp;
			else m_rtn = m_pipeline_immediate[4];
			data_write(m_pipeline_operand_address[4], m_pipeline_immediate[4], 4);
		}
		if (BIT(m_pipeline_word[4], 9))
		{
			m_pipeline_fetch_address = m_pipeline_acc_transfer_target;
			m_pipeline_decode_flush = 1; // Discard sequential Fetch after the two delay words.
		}
		else m_pipeline_acc_transfer_pending = true;
	}
	if (m_pipeline_valid[2] && (staged_call(m_pipeline_word[2]) || staged_far_call(m_pipeline_word[2])))
	{
		// Example 7-4: opcode Access coincides with complete target Decode.
		// Honor older stack updates before allocating this younger call word.
		m_pipeline_operand_address[2] = --m_sp;
		m_pipeline_immediate[2] = u16(m_pipeline_address[2] + (BIT(m_pipeline_word[2], 9) ? 4 : 2));
		if (staged_far_call(m_pipeline_word[2])) m_pipeline_far_call_page[2] = (m_pipeline_address[2] >> 16) & 0x7f;
	}
	if (m_pipeline_valid[4] && (staged_call(m_pipeline_word[4]) || staged_far_call(m_pipeline_word[4])))
		// The opcode's Execute precedes the complete two-word slot's Execute.
		m_data.write_word(m_pipeline_operand_address[4], m_pipeline_immediate[4]);
	if (m_pipeline_valid[4] && staged_conditional_branch(m_pipeline_word[4]) && !staged_bio_branch(m_pipeline_word[4]) && BIT(m_pipeline_word[4], 9))
	{
		// Our complete two-word slot reaches Read at the opcode's Evaluate
		// clock. Retain the predicate after older Execute writes (example 7-17).
		bool const overflow = (m_pipeline_word[4] & 0xfde7) == 0xf860;
		u16 const overflow_mask = 1U << (10 - BIT(m_pipeline_word[4], 3));
		m_pipeline_immediate[4] = staged_tc_branch(m_pipeline_word[4]) ? BIT(m_st0, 12) == BIT(m_pipeline_word[4], 4) :
			(m_pipeline_word[4] & 0xfdfb) == 0xf808 ? BIT(m_st0, 11) == BIT(m_pipeline_word[4], 2) :
			overflow ? bool(m_st0 & overflow_mask) == BIT(m_pipeline_word[4], 4) : staged_accumulator_condition(m_pipeline_word[4], accumulator(BIT(m_pipeline_word[4], 3)));
		if (overflow) m_st0 &= ~overflow_mask;
		if (m_pipeline_repeat_redirect_pending && m_pipeline_control_address == m_pipeline_address[4] && staged_accumulator_branch(m_pipeline_word[4]))
			// Delay repeat Decode precedes Evaluate: retain the selected path
			// only after the predicate has sampled older Execute publications.
			m_pipeline_repeat_redirect_target = (m_pipeline_address[4] & 0x7f0000) |
				(m_pipeline_immediate[4] ? m_pipeline_operand[4] : u16(m_pipeline_address[4] + 4));
	}
	if (m_pipeline_valid[4] && staged_conditional_call(m_pipeline_word[4]))
	{
		if (u8(m_pipeline_word[4]) == 0x76)
		{
			// CC[D] permits AGT+AOV together and consumes OVA even on a false
			// combined predicate (SPRU172C 4-29..31).
			m_pipeline_immediate[4] &= BIT(m_st0, 10);
			m_st0 &= ~u16(0x0400);
		}
		// CCD must replace its target literal with the retained predicate
		// before deciding whether the call owns any stack reservation.
		if (m_pipeline_immediate[4]) { m_pipeline_operand_address[4] = --m_sp; m_rtn = u16(m_pipeline_address[4] + (BIT(m_pipeline_word[4], 9) ? 4 : 2)); }
		if (!BIT(m_pipeline_word[4], 9)) m_pipeline_cc_pending = true;
	}
	if (m_pipeline_valid[4] && (staged_conditional_return(m_pipeline_word[4]) || staged_delayed_conditional_return(m_pipeline_word[4])))
	{
		// Consume older accumulator/TC publications before retaining the
		// decision. Execute must not retest a subsequently changed source.
		u16 const word = m_pipeline_word[4] & 0xfdff;
		bool const taken = (word & 0xfe) == 2 ? (bool(m_bio_in_cb()) != BIT(word, 0)) : (word & 0xffef) == 0xfc20 ? BIT(m_st0, 12) == BIT(word, 4) :
			staged_accumulator_condition(word, accumulator(BIT(word, 3)));
		m_pipeline_immediate[4] = taken;
		if (staged_delayed_conditional_return(m_pipeline_word[4]))
		{
			// Both paths execute the retained two-word delay span. A false
			// condition owns no stack bus access (SPRU172C 4-133/4-134).
			m_pipeline_operand[4] = taken ? data_read(m_sp++) : u16(m_pipeline_address[4] + 3);
			m_pipeline_retd_target = (m_pipeline_address[4] & 0x7f0000) | m_pipeline_operand[4];
		}
		else if (taken)
		{
			m_pipeline_flush_front = true;
			m_pipeline_operand_address[4] = m_sp++;
			m_pipeline_operand[4] = data_read(m_pipeline_operand_address[4]);
		}
		else
		{
			m_pipeline_flush_front = false; m_pipeline_decode_flush = 0;
		}
	}
	if (m_pipeline_valid[4] && staged_memory_dp(m_pipeline_word[4]))
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
				(!staged_memory_load(word) && (word & 0xfc80) != 0x8000 &&
				 (word & 0xfe80) != 0x5600 && (word & 0xfe80) != 0x4e00 && (word & 0xff80) != 0x4600)) continue;
			m_pipeline_direct_status[stage] = (m_pipeline_direct_status[stage] & 0x4000) | (m_pipeline_operand[4] & 0x1ff);
			if (stage == 3) m_pipeline_operand_address[stage] = basic_address(stage, u8(word));
		}
	}
	pipeline_publish_read_results(smem_t_product_read);
	if (entry_stage == 2)
	{
		m_interrupt_stack_address = --m_sp;
		// SPRU131G 7.5.9 / example 7-18: IPTR is evaluated by vector
		// Prefetch after decoded foreground publications, not source acceptance.
		m_interrupt_vector_pc = (m_pmst & 0xff80) | (m_interrupt_vector_pc & 0x7f);
		m_pipeline_fetch_address = logical_program_address(m_interrupt_vector_pc);
		m_pipeline_flush_front = false;
	}
	if (software_intr_prefetch)
	{
		// Allocate after older Read-owned SP updates, as for hardware entry.
		m_pipeline_operand_address[3] = --m_sp;
		m_interrupt_vector_pc = u16(software_intr_target);
		m_pipeline_fetch_address = software_intr_target;
	}
	if (m_pipeline_valid[2] && staged_software_intr(m_pipeline_word[2]))
	{
		if (m_interrupt_entry_stage || m_pipeline_vector_pending) { logerror("%s: overlapping staged software interrupt entry is unsupported\n", machine().describe_context()); m_illegal = true; return; }
		m_pipeline_immediate[2] = u16(m_pipeline_address[2] + 1);
		m_rtn = m_pipeline_immediate[2];
		m_pipeline_control_address = m_pipeline_address[2];
		m_pipeline_control_word = m_pipeline_word[2];
		m_pipeline_control_words = 1;
		m_pipeline_decode_flush = 2;
		m_pipeline_vector_pending = true;
	}
	// SPRU172C 4-116: MVMM reads in Decode and writes in Access.
	// Order both after older publications; forward an Access write to a
	// younger Decode read without resampling the older move's operand.
	if (m_pipeline_valid[3] && staged_mmr_move(m_pipeline_word[3]))
	{
		u16 const destination = 0x10 + (m_pipeline_word[3] & 15);
		u16 const previous_sp = m_sp;
		data_write(destination, m_pipeline_operand[3], 3);
		if (destination == 0x18) pipeline_delay_compiler_sp(previous_sp);
	}
	if (m_pipeline_valid[2] && staged_mmr_move(m_pipeline_word[2]))
	{
		unsigned const source = (m_pipeline_word[2] >> 4) & 15;
		// An adjacent short accumulator ST *ARx+ has a pending DAGEN
		// increment. MVMM consumes that address result, not the stale AR
		// cell; the store still owns its original Read address and EB write.
		bool const postincrement = m_pipeline_valid[3] &&
			(m_pipeline_word[3] & 0xfcf8) == 0x8090 &&
			indirect_ar(u8(m_pipeline_word[3]), 3) == source && m_ar[source] >= 0x60;
		m_pipeline_operand[2] = postincrement ? u16(m_ar[source] + 1) : data_read(0x10 + source);
		// A preceding PSHM still owns its Read allocation. MVMM's Decode
		// view includes that older decrement without allocating the stack twice.
		if (source == 8 && (m_pipeline_word[2] & 15) < 8 &&
			m_pipeline_valid[3] && staged_direct_push(m_pipeline_word[3]))
			--m_pipeline_operand[2];
	}
	if (m_pipeline_valid[2] && staged_block_repeat(m_pipeline_word[2]))
	{
		// There is one architectural loop controller, not a hidden loop stack.
		// An outside-body callback may replace it after software saves its
		// registers; returning to restored bounds rebuilds the fetch prediction.
		if (m_pipeline_block_active && m_pipeline_block_body[2]) { logerror("%s: nested staged block repeat inside active body is unsupported\n", machine().describe_context()); m_illegal = true; return; }
		// The two RPTBD delay words execute once, outside the repeated block.
		m_rsa = u16(m_pipeline_address[2] + (BIT(m_pipeline_word[2], 9) ? 4 : 2));
		m_rea = m_pipeline_immediate[2];
		m_st1 |= ST1_BRAF;
		m_pipeline_block_start = (m_pipeline_address[2] & 0x7f0000) | m_rsa;
		m_pipeline_block_end_address = (m_pipeline_address[2] & 0x7f0000) | m_rea;
		m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
		m_pipeline_block_active = true;
		m_pipeline_block_fetch_enabled = false;
	}
	if (m_pipeline_valid[3] && staged_block_repeat(m_pipeline_word[3]))
	{
		m_pipeline_fetch_address = m_pipeline_block_start;
		m_pipeline_block_fetch_enabled = true;
	}
	if (m_pipeline_valid[2] && m_pipeline_block_active)
	{
		if (m_pipeline_block_first_final[2]) m_st1 &= ~ST1_BRAF;
		if (m_pipeline_block_end[2])
		{
			if (m_brc) --m_brc;
			else m_pipeline_block_active = false;
		}
	}
	pipeline_prefetch(prefetch_bubble, software_intr_prefetch, software_intr_physical);
	pipeline_accept_interrupt();
}

bool tms320c54x_device::pipeline_execute()
{
	if (!m_pipeline_valid[5]) return true;
	u32 const address = m_pipeline_address[5];
	m_pc = u16(address);
	m_xpc = (address >> 16) & 0x7f;
	m_op = m_pipeline_word[5];
	bool const braf_before = bool(m_st1 & ST1_BRAF);
	debugger_instruction_hook(address);
	// XC's replacement NOP consumes a slot, not an original instruction entry.
	if (!m_pipeline_annulled[5] && !m_instruction_observer_cb.isunset()) m_instruction_observer_cb(address, m_op);
	++m_pc;
	if (m_coverage && !m_coverage->count[m_op]++) m_coverage->first_pc[m_op] = address;
	if (m_coverage && staged_shifted_smem(m_op))
	{
		bool const extended = u8(m_op) >= 0xe0;
		std::array<u16, 4> const words = {u16(extended ? 3 : 2), m_op,
			extended ? m_pipeline_extension_first[5] : m_pipeline_immediate[5],
			extended ? m_pipeline_immediate[5] : u16(0)};
		auto entry = m_coverage->extensions.find(words);
		if (entry == m_coverage->extensions.end() && m_coverage->extensions.size() < opcode_coverage::MAX_TUPLES)
			entry = m_coverage->extensions.emplace(words, opcode_coverage::extension_observation{address}).first;
		if (entry != m_coverage->extensions.end()) ++entry->second.count;
		else ++m_coverage->dropped_tuples;
	}
	if (staged_immediate_repeat(m_op) || staged_memory_repeat(m_op) || staged_short_repeat(m_op))
	{
		if ((m_op & 0xfeff) == 0xf071) accumulator(BIT(m_op, 8)) = 0;
		m_rptc = staged_memory_repeat(m_op) ? m_pipeline_operand[5] : m_pipeline_immediate[5];
		m_rpt_iteration = 0;
		if (staged_immediate_repeat(m_op) || (staged_memory_repeat(m_op) && u8(m_op) >= 0xe0)) ++m_pc;
	}
	else if (staged_block_repeat(m_op))
		++m_pc; // Setup belongs to complete Decode, not an atomic operand fetch.
	else if (staged_idle(m_op))
	{
		enter_idle(BIT(m_op, 9) ? 2 : BIT(m_op, 8) ? 3 : 1);
		m_pipeline_idle_barrier = false;
		m_pipeline_idle_resume_pending = true;
		m_pipeline_fetch_address = m_pipeline_idle_resume_address;
		m_pipeline_valid.fill(false);
		if (m_nmi_pending || (m_ifr & m_imr)) leave_idle();
	}
	else if (staged_software_intr(m_op))
		m_data.write_word(m_pipeline_operand_address[5], m_pipeline_immediate[5]);
	else if (m_op == 0xf49b || m_op == 0xf69b)
		m_st1 &= ~u16(0x0800);
	else if (staged_conditional_return(m_op) || staged_delayed_conditional_return(m_op))
	{
		if (m_pipeline_immediate[5] || staged_delayed_conditional_return(m_op))
		{
			m_pc = m_pipeline_operand[5];
			m_pipeline_fetch_address = (address & 0x7f0000) | m_pc;
			if (m_pipeline_immediate[5] && staged_delayed_conditional_return(m_op)) pipeline_restore_block_on_return(address);
			m_pipeline_flush_front = false; m_pipeline_decode_flush = 0;
		}
	}
	else if (m_op == 0xfc00 || m_op == 0xf4eb || m_op == 0xfe00)
	{
		m_pc = m_pipeline_operand[5];
		m_pipeline_fetch_address = logical_program_address(m_pc);
		if (m_op == 0xfc00 && m_pipeline_xc_active && m_pipeline_xc_end == ((address & 0x7f0000) | u16(address + 1)))
			// The return discards the sequential second protected word.
			// Retain protection for the first resumed instruction word.
			m_pipeline_xc_end = m_pipeline_fetch_address;
		pipeline_restore_block_on_return(address);
		m_pipeline_flush_front = false;
		if (m_op == 0xf4eb) m_st1 &= ~u16(0x0800);
	}
	else if ((m_op & 0xfcf0) == 0xf4b0)
	{
		if (m_op == 0xf6bf && m_pipeline_block_active && !m_pipeline_block_body[5])
		{
			// A forward exit annuls its body predictions, but does not clear
			// architectural BRAF. Outside-body RSBX may retire that predictor
			// only with no younger body in flight. In-loop six-word latency
			// (SPRU131G 7.5.8.2) remains outside this bounded composition.
			for (unsigned stage = 0; stage < 5; ++stage)
				if (m_pipeline_valid[stage] && m_pipeline_block_body[stage])
				{ logerror("%s: staged outside BRAF clear with pending block words is unsupported\n", machine().describe_context()); m_illegal = true; return false; }
			m_pipeline_block_active = m_pipeline_block_fetch_enabled = false;
			m_pipeline_block_fetch_remaining = 0;
		}
		u16 &status = BIT(m_op, 9) ? m_st1 : m_st0;
		u16 const mask = u16(1) << (m_op & 15);
		status = BIT(m_op, 8) ? status | mask : status & ~mask;
		if (BIT(m_op, 9)) status &= ~ST1_RESERVED;
	}
	else if ((m_op & 0xfdfe) == 0xf4e4)
	{
		m_xpc = m_extended_program ? m_pipeline_operand[5] & 0x7f : 0;
		m_pc = m_pipeline_operand_address[5];
		if (!BIT(m_op, 9))
		{
			m_pipeline_fetch_address = logical_program_address(m_pc);
			pipeline_restore_block_on_return(address);
		}
		m_pipeline_flush_front = false;
		if (BIT(m_op, 0)) m_st1 &= ~u16(0x0800);
	}
	else if (staged_banz(m_op))
		m_pc += staged_extended_banz(m_op) ? 2 : 1; // Predicate, AR modification and target ownership belong to Access.
	else if (staged_conditional_branch(m_op))
	{
		++m_pc;
		// CCD retains its Read decision and one-word frame. Do not add
		// the nondelayed CC hold to the complete two-word delay span.
		if (staged_conditional_call(m_op) && BIT(m_op, 9) && m_pipeline_immediate[5])
			data_write(m_pipeline_operand_address[5], u16(address + 4));
		if (m_pipeline_immediate[5] && BIT(m_op, 9))
		{
			m_pipeline_fetch_address = (address & 0x7f0000) | m_pipeline_operand[5];
			m_pipeline_decode_flush = 1;
			if (m_pipeline_block_body[5] && !staged_conditional_call(m_op))
				// Annul fall-through endpoint predictions, not Decode's count.
				m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
		}
	}
	else if (staged_accumulator_transfer(m_op))
	{
		m_pipeline_flush_front = false;
		m_pipeline_decode_flush = 0;
		if (BIT(m_op, 9))
		{
			// Delayed accumulator transfers already redirected Prefetch in Read;
			// only FCALAD owns a second frame word.
			if (staged_accumulator_far_call(m_op)) data_write(m_pipeline_far_call_stack[5], m_pipeline_far_call_page[5], 5);
			m_pc = u16(address + 1);
		}
		else
		{
			m_pc = u16(m_pipeline_acc_transfer_target);
			m_xpc = (m_pipeline_acc_transfer_target >> 16) & 0x7f;
			m_pipeline_fetch_address = m_pipeline_acc_transfer_target;
		}
	}
	else if (staged_far_branch(m_op) || staged_far_call(m_op))
	{
		if (staged_far_call(m_op)) data_write(m_pipeline_far_call_stack[5], m_pipeline_far_call_page[5]);
		if (BIT(m_op, 9))
			m_pc = u16(address + 2); // Delay words retain the caller's architectural page.
		else
		{
			m_xpc = m_op & 0x7f;
			m_pc = m_pipeline_operand[5];
		}
	}
	else if (m_op == 0xf073 || m_op == 0xf273 || staged_call(m_op))
		++m_pc; // Target fetch was already redirected in Access.
	else if ((m_op & 0xff00) == 0xee00)
		; // FRAME's SP update already belongs to its Access stage.
	else if (staged_mmr_move(m_op))
		; // MVMM's retained Decode read and Access write are already complete.
	else if ((m_op & 0xff00) == 0x7700)
	{
		if (staged_ram_stm(m_op)) data_write(m_pipeline_operand_address[5], m_pipeline_immediate[5]);
		// Non-DAGEN PMST/IMR/bus controls retain their literal until Execute; do not
		// inherit address-generator early publication. Phase is partial.
		if (staged_execute_mmr_stm(m_op) || m_op == 0x771d || m_op == 0x7700 || staged_bus_control(m_op) || (staged_timer_control(m_op) || staged_serial_control(m_op) || staged_clock_control(m_op))) data_write(u8(m_op), m_pipeline_immediate[5]);
		++m_pc;
	}
	else if (staged_write_a(m_op))
	{
		m_program.write_word(m_pipeline_write_a_target, m_pipeline_operand[5]);
		if (u8(m_op) >= 0xe0) ++m_pc;
	}
	else if (staged_read_a(m_op))
	{
		data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		if (u8(m_op) >= 0xe0) ++m_pc;
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && address == m_pipeline_repeat_address) pipeline_begin_repeat_stream();
	}
	else if (staged_data_to_program(m_op))
	{
		m_program.write_word((u32(m_pipeline_immediate[5]) << 16) | m_pipeline_operand_address[5], m_pipeline_operand[5]);
		m_pc += u8(m_op) >= 0xe0 ? 2 : 1;
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && address == m_pipeline_repeat_address)
		{
			pipeline_begin_repeat_stream();
			// Read reused the immediate field for the physical target's page;
			// the low word still holds pmad, the repeat's incrementing base.
			m_pipeline_repeat_source = m_pipeline_operand_address[5];
		}
	}
	else if (staged_program_move(m_op))
	{
		data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		m_pc += staged_extended_program_move(m_op) ? 2 : 1;
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && m_pipeline_address[5] == m_pipeline_repeat_address)
			pipeline_begin_repeat_stream();
	}
	else if (staged_delay_copy(m_op))
	{
		data_write(u16(m_pipeline_operand_address[5] + 1), m_pipeline_operand[5]);
		m_pc += u8(m_op) >= 0xe0;
	}
	else if (staged_data_copy(m_op))
	{
		if (m_pipeline_operand_address[5] >= 0x20) data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		m_pc += staged_extended_data_copy(m_op) ? 2 : 1;
	}
	else if (staged_move(m_op))
		m_pc += staged_extended_move(m_op) ? 2 : 1; // Retained move already committed at its owned write clock.
	else if (staged_memory_load(m_op))
	{
		u16 const status = m_st1;
		m_st1 = (m_st1 & ~u16(0x011f)) | m_pipeline_alu_mode[5];
		u64 const value = (m_op & 0xfe00) == 0x1200 ? m_pipeline_operand[5] : data_operand(m_pipeline_operand[5]);
		accumulator(BIT(m_op, 8)) = (m_op & 0xfe00) == 0x1400 ? shifted_by_t(m_pipeline_operand[5], m_pipeline_immediate[5]) :
			(m_op & 0xfe00) == 0x1600 ? ((value << 16) | 0x8000) & ACC_MASK :
			(m_op & 0xfe00) == 0x4400 ? (value << 16) & ACC_MASK : value;
		m_st1 = status;
		if (staged_offset_load(m_op) || staged_absolute_load(m_op)) ++m_pc;
	}
	else if (staged_mmr_load(m_op))
		accumulator(BIT(m_op, 8)) = m_pipeline_operand[5];
	else if (staged_memory_dp(m_op))
	{
		m_st0 = (m_st0 & ~u16(0x1ff)) | (m_pipeline_operand[5] & 0x1ff);
		if (m_op == 0x46f8) ++m_pc;
	}
	else if (register_product_form(m_op))
		register_product_result(m_op, m_pipeline_operand[5]);
	else if ((m_op & 0xfeff) == 0xf48d)
	{
		// SPRU172C 4-161: square A(32-16), not the signed 16-bit AH.
		// The accumulator form does not publish T.
		s64 const high = accumulator_high17(m_a);
		accumulator(BIT(m_op, 8)) = multiply_result(high * high * (BIT(m_st1, 6) ? 2 : 1), BIT(m_op, 8));
	}
	else if (staged_minmax(m_op))
	{
		s64 const a = s64(m_a << 24) >> 24, b = s64(m_b << 24) >> 24;
		bool const choose_a = BIT(m_op, 0) ? a < b : a > b;
		accumulator(BIT(m_op, 8)) = choose_a ? m_a : m_b;
		m_st0 = (m_st0 & ~u16(0x0800)) | (choose_a ? 0 : 0x0800);
	}
	else if (staged_norm(m_op))
	{
		accumulator(BIT(m_op, 8)) = shifted_load(accumulator(BIT(m_op, 9)), normalization_shift(m_pipeline_immediate[5]), BIT(m_op, 8));
	}
	else if (staged_long_arithmetic(m_op))
	{
		long_word_arithmetic(m_op, m_pipeline_operand[5], m_pipeline_immediate[5], m_pipeline_t[5]);
		if (staged_extended_long_arithmetic(m_op)) ++m_pc;
	}
	else if (staged_dld(m_op))
	{
		u64 value = (u64(m_pipeline_operand[5]) << 16) | m_pipeline_immediate[5];
		if (BIT(m_st1, 8) && BIT(m_pipeline_operand[5], 15)) value |= u64(0xff) << 32;
		accumulator(BIT(m_op, 8)) = value;
		if (staged_extended_dld(m_op)) ++m_pc;
	}
	else if (staged_dst(m_op))
	{
		m_data.write_word(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		m_pipeline_long_store_pending = true;
		if (staged_extended_dst(m_op)) ++m_pc;
	}
	else if (staged_direct_push(m_op) || (m_op & 0xff00) == 0x4b00)
	{
		data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		if (staged_extended_push(m_op)) ++m_pc;
	}
	else if ((m_op & 0xff00) == 0x8a00)
	{
		u16 const destination = m_pipeline_operand_address[5];
		if (destination < 0x10 || destination > 0x19 || m_pipeline_early_write_deferred[5])
		{
			data_write(destination, m_pipeline_operand[5]);
			if (destination == 0x18) pipeline_delay_compiler_sp(m_pipeline_direct_sp[5]);
		}
	}
	else if ((m_op & 0xff00) == 0x8b00)
	{
		u16 const destination = m_pipeline_operand_address[5];
		bool const own_pointer = popd_own_pointer(m_op, destination) && m_pipeline_repeat_active && m_pipeline_address[5] == m_pipeline_repeat_address;
		if ((destination < 0x10 || destination > 0x19 || m_pipeline_early_write_deferred[5]) && !own_pointer)
			data_write(destination, m_pipeline_operand[5]);
		if (staged_extended_pop(m_op)) ++m_pc;
	}
	else if (staged_accumulator_mmr_store(m_op))
	{
		// SPRU131G table 7-5: store-type DAGEN writes belong to Execute.
		// Direct MMR addressing ignores DP/CPL (SPRU172C 4-175).
		// PMST also publishes here; later Prefetch owns mapping (7.5.9).
		u16 const value = u16(saturated_store(accumulator(BIT(m_op, 8))));
		if (u8(m_op) == 7 && !(value & ST1_BRAF) && m_pipeline_block_active)
		{
			// Software may suspend an interrupted loop outside its body.
			// Retire only the predictor; architectural BRC/RSA/REA remain
			// available for the handler's own save/restore sequence.
			for (unsigned stage = 0; stage < 5; ++stage)
				if (m_pipeline_valid[stage] && m_pipeline_block_body[stage])
				{ logerror("%s: staged outside STLM BRAF clear with pending block words is unsupported\n", machine().describe_context()); m_illegal = true; return false; }
			m_pipeline_block_active = m_pipeline_block_fetch_enabled = false;
			m_pipeline_block_fetch_remaining = 0;
		}
		data_write(m_pipeline_operand_address[5], value);
	}
	else if (staged_store(m_op))
	{
		data_write(m_pipeline_operand_address[5], pipeline_store_word());
		if (staged_extended_store(m_op)) ++m_pc;
	}
	else if (staged_rmw(m_op))
	{
		m_pc += staged_extended_rmw(m_op) || u8(m_op) == 0xf8 ? 2 : 1;
		data_write(m_pipeline_operand_address[5], (m_op & 0xff00) == 0x6b00
			? add_memory_word(m_pipeline_operand[5], m_pipeline_immediate[5]) : (m_op & 0xff00) == 0x6a00
			? m_pipeline_operand[5] ^ m_pipeline_immediate[5] : BIT(m_op, 8)
			? m_pipeline_operand[5] | m_pipeline_immediate[5]
			: m_pipeline_operand[5] & m_pipeline_immediate[5]);
	}
	else if ((m_op & 0xff00) == 0x7600)
	{
		m_pc += u8(m_op) == 0xf8 || staged_extended_immediate_store(m_op) ? 2 : 1;
		if (m_pipeline_operand_address[5] < 0x10 || m_pipeline_operand_address[5] > 0x19)
			data_write(m_pipeline_operand_address[5], pipeline_store_word());
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && address == m_pipeline_repeat_address) pipeline_begin_repeat_stream();
	}
	else if (staged_port_write(m_op))
	{
		m_io.write_word(m_pipeline_immediate[5], m_pipeline_operand[5]);
		m_pc += u8(m_op) >= 0xe0 ? 2 : 1;
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && address == m_pipeline_repeat_address) pipeline_begin_repeat_stream();
	}
	else if (staged_port_read(m_op))
	{
		data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		m_pc += u8(m_op) >= 0xe0 ? 2 : 1;
		if (m_pipeline_repeat_active && m_pipeline_repeat_body_latched && address == m_pipeline_repeat_address) pipeline_begin_repeat_stream();
	}
	else if (staged_conditional_store(m_op))
		data_write(m_pipeline_operand_address[5], pipeline_store_word());
	else if (staged_parallel_alu(m_op))
	{
		u64 const other = accumulator(!BIT(m_op, 8)) & ACC_MASK;
		s64 const sample = m_pipeline_alu_mode[5] & 0x100 ? s16(m_pipeline_operand[5]) : s32(m_pipeline_operand[5]);
		u64 const operand = u64(sample * 65536) & ACC_MASK;
		data_write(m_pipeline_operand_address[5], pipeline_store_word());
		// SUB reverses the usual accumulator-minus-memory order
		// (SPRU172C 4-185); the old accumulator is stored first.
		accumulator(BIT(m_op, 8)) = BIT(m_op, 10) ? add_sub(operand, other, true, BIT(m_op, 8)) : add_sub(other, operand, false, BIT(m_op, 8));
	}
	else if (staged_parallel_load(m_op))
	{
		// Store the old accumulator before replacing an aliased destination
		// (SPRU172C 4-179). Retain the unresolved C policy.
		data_write(m_pipeline_operand_address[5], pipeline_store_word());
		if ((m_op & 0xfd00) != 0xe400)
		{
			s64 const value = m_pipeline_alu_mode[5] & 0x100 ? s16(m_pipeline_operand[5]) : s32(m_pipeline_operand[5]);
			accumulator(BIT(m_op, 8)) = u64(value * 65536) & ACC_MASK;
		}
	}
	else if (staged_parallel_mpy(m_op))
	{
		// The store consumes the old accumulator; MPY uses Read-owned X/T.
		u16 const stored = pipeline_store_word();
		s64 product = s64(s16(m_pipeline_operand[5])) * s16(m_pipeline_immediate[5]);
		if (BIT(m_st1, 6)) product *= 2;
		data_write(m_pipeline_operand_address[5], stored);
		accumulator(BIT(m_op, 8)) = multiply_result(product, BIT(m_op, 8));
	}
	else if (staged_parallel_mac(m_op))
	{
		// Store before replacing an aliased accumulator (SPRU172C 4-180).
		// C remains an implementation hypothesis, not qualified silicon behaviour.
		u16 const stored = pipeline_store_word();
		s64 product = s64(s16(m_pipeline_operand[5])) * s64(s16(m_pipeline_immediate[5]));
		if (BIT(m_st1, 6)) product *= 2;
		product = multiply_product(product);
		s64 result = (s64(accumulator(BIT(m_op, 8)) << 24) >> 24) + (BIT(m_op, 11) ? -product : product);
		if (BIT(m_op, 10)) result = (result + 0x8000) & ~s64(0xffff);
		data_write(m_pipeline_operand_address[5], stored);
		accumulator(BIT(m_op, 8)) = multiply_result(result, BIT(m_op, 8));
	}
	else if (staged_mvdd(m_op))
	{
		m_observed_mvdd_write = true;
		data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
		m_observed_mvdd_write = false;
	}
	else if (staged_coefficient_mac(m_op))
	{
		bool const destination_b = BIT(m_op, 8);
		s64 product = s64(s16(m_pipeline_operand[5])) * s16(m_pipeline_immediate[5]);
		if (BIT(m_st1, 6)) product *= 2;
		accumulator(destination_b) = multiply_result((s64(accumulator(destination_b) << 24) >> 24) + multiply_product(product), destination_b);
		if (BIT(m_op, 9)) data_write(u16(m_pipeline_operand_address[5] + 1), m_pipeline_operand[5]);
		m_pc += u8(m_op) >= 0xe0 ? 2 : 1;
	}
	else if (staged_firs(m_op))
	{
		s64 product = accumulator_high17(m_a) * s64(s16(m_pipeline_immediate[5]));
		if (BIT(m_st1, 6)) product *= 2;
		m_b = multiply_result((s64(m_b << 24) >> 24) + multiply_product(product), true);
		m_a = add_sub((data_operand(m_pipeline_operand[5]) << 16) & ACC_MASK,
			(data_operand(m_pipeline_t[5]) << 16) & ACC_MASK, false, false);
		++m_pc;
	}
	else if (staged_distance(m_op))
	{
		// Both forms consume old A before the vector subtraction replaces it.
		s64 const high = accumulator_high17(m_a);
		s64 distance = BIT(m_op, 8) ? (high < 0 ? -high : high) : high * high;
		if (BIT(m_st1, 6)) distance *= 2;
		m_b = BIT(m_op, 8) ? add_sub(m_b, u64(distance), false, true) :
			multiply_result((s64(m_b << 24) >> 24) + multiply_product(distance), true);
		s64 const x = BIT(m_st1, 8) ? s64(s16(m_pipeline_operand[5])) : s64(m_pipeline_operand[5]);
		s64 const y = BIT(m_st1, 8) ? s64(s16(m_pipeline_immediate[5])) : s64(m_pipeline_immediate[5]);
		m_a = add_sub(u64(x * 65536) & ACC_MASK, u64(y * 65536) & ACC_MASK, true, false);
	}
	else if (staged_dual_arithmetic(m_op))
	{
		s64 const x = BIT(m_st1, 8) ? s64(s16(m_pipeline_operand[5])) : s64(m_pipeline_operand[5]);
		s64 const y = BIT(m_st1, 8) ? s64(s16(m_pipeline_immediate[5])) : s64(m_pipeline_immediate[5]);
		accumulator(BIT(m_op, 8)) = add_sub(u64(x * 0x10000) & ACC_MASK,
			u64(y * 0x10000) & ACC_MASK, BIT(m_op, 9), BIT(m_op, 8));
	}
	else if (staged_lms(m_op))
	{
		s64 const x = BIT(m_st1, 8) ? s64(s16(m_pipeline_operand[5])) : s64(m_pipeline_operand[5]);
		m_a = add_sub(m_a, u64(x * 65536 + 32768) & ACC_MASK, false, false);
		s64 product = s64(s16(m_pipeline_operand[5])) * s16(m_pipeline_immediate[5]);
		if (BIT(m_st1, 6)) product *= 2;
		m_b = multiply_result((s64(m_b << 24) >> 24) + multiply_product(product), true);
	}
	else if (staged_load_mac(m_op))
	{
		bool const destination_b = BIT(m_op, 8);
		s64 product = s64(s16(m_pipeline_t[5])) * s16(m_pipeline_immediate[5]);
		if (BIT(m_st1, 6)) product *= 2;
		product = multiply_product(product);
		s64 result = (s64(accumulator(!destination_b) << 24) >> 24) + (BIT(m_op, 10) ? -product : product);
		if (BIT(m_op, 9)) result = (result + 0x8000) & ~s64(0xffff);
		accumulator(!destination_b) = multiply_result(result, !destination_b);
		accumulator(destination_b) = (data_operand(m_pipeline_operand[5]) << 16) & ACC_MASK;
	}
	else if (staged_dual_multiply(m_op))
	{
		bool const signed_mac = (m_op & 0xf000) == 0xb000;
		bool const accumulate = signed_mac || BIT(m_op, 9), destination_b = BIT(m_op, 8);
		s64 product = (accumulate && !signed_mac ? s64(m_pipeline_operand[5]) : s64(s16(m_pipeline_operand[5]))) * s64(s16(m_pipeline_immediate[5]));
		if (BIT(m_st1, 6)) product *= 2;
		// X/Y and T belong to Read; source selection and feedback belong to
		// Execute. SMUL applies before accumulation, rounding afterwards.
		if (accumulate)
		{
			product = multiply_product(product);
			if (signed_mac && BIT(m_op, 11)) product = -product;
			product += s64(accumulator(signed_mac ? BIT(m_op, 9) : destination_b) << 24) >> 24;
		}
		if (signed_mac && BIT(m_op, 10)) product = (product + 0x8000) & ~s64(0xffff);
		accumulator(destination_b) = multiply_result(product, destination_b);
	}
	else if (staged_cmps(m_op))
	{
		m_trn = u16((m_trn << 1) | m_pipeline_immediate[5]);
		m_st0 = (m_st0 & ~u16(0x1000)) | (m_pipeline_immediate[5] ? 0x1000 : 0);
		data_write(m_pipeline_operand_address[5], m_pipeline_operand[5]);
	}
	else if (staged_xmem_store(m_op))
	{
		data_write(m_pipeline_operand_address[5], pipeline_store_word());
	}
	else if (staged_xmem_arithmetic(m_op))
	{
		u64 const operand = (data_operand(m_pipeline_operand[5]) << (m_op & 15)) & ACC_MASK;
		u64 &destination = accumulator(BIT(m_op, 8));
		destination = add_sub(destination, operand, BIT(m_op, 9), BIT(m_op, 8));
	}
	else if (staged_xmem_load(m_op))
		accumulator(BIT(m_op, 8)) = shifted_load(data_operand(m_pipeline_operand[5]), m_op & 15, BIT(m_op, 8));
	else if (staged_xmem_bit(m_op))
		m_st0 = (m_st0 & ~u16(0x1000)) | (BIT(m_pipeline_operand[5], 15 - (m_op & 15)) ? 0x1000 : 0);
	else if (staged_bitfield(m_op))
	{
		bool const tc = BIT(m_op, 8) ? bool(m_pipeline_operand[5] & m_pipeline_immediate[5]) : m_pipeline_operand[5] == m_pipeline_immediate[5];
		m_st0 = (m_st0 & ~u16(0x1000)) | (tc ? 0x1000 : 0);
		m_pc += staged_extended_bitfield(m_op) ? 2 : 1;
	}
	else if (staged_shifted_smem(m_op))
	{
		u16 const extension = m_pipeline_immediate[5];
		int const shift = s8((extension & 0x1f) << 3) >> 3;
		if (staged_shifted_memory_store(m_op, extension))
		{
			data_write(m_pipeline_operand_address[5], pipeline_store_word());
		}
		else if (staged_shifted_memory_arithmetic(extension))
		{
			u64 const operand = data_operand(m_pipeline_operand[5]);
			u64 const shifted = shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(operand, -shift) : operand >> -shift) : (operand << shift) & ACC_MASK;
			accumulator(BIT(extension, 8)) = add_sub(accumulator(BIT(extension, 9)), shifted, BIT(extension, 5), BIT(extension, 8));
		}
		else accumulator(BIT(extension, 8)) = shifted_load(data_operand(m_pipeline_operand[5]), shift, BIT(extension, 8));
		m_pc += u8(m_op) >= 0xe0 ? 2 : 1;
	}
	else if (staged_smem_special(m_op))
	{
		if ((m_op & 0xff00) == 0x3000)
		{
			// T was published in Read; retirement must not overwrite a newer T.
		}
		else if (staged_bitt(m_op))
			m_st0 = (m_st0 & ~u16(0x1000)) | (BIT(m_pipeline_operand[5], 15 - (m_pipeline_immediate[5] & 15)) ? 0x1000 : 0);
		else if ((m_op & 0xff00) == 0x3200)
			m_st1 = (m_st1 & ~u16(0x001f)) | (m_pipeline_operand[5] & 0x001f);
		else if ((m_op & 0xf000) == 0x2000)
		{
			u64 &destination = accumulator(BIT(m_op, 8));
			destination = memory_t_multiply(m_op, m_pipeline_operand[5], m_pipeline_immediate[5], destination);
		}
		else if (staged_poly(m_op)) polynomial_step(m_pipeline_operand[5], m_pipeline_immediate[5]);
		else if (square_accumulate(m_op)) square_accumulate_result(m_op, m_pipeline_operand[5]);
		else accumulator_memory_multiply(m_op, m_pipeline_operand[5]);
		if (staged_extended_smem_special(m_op)) ++m_pc;
	}
	else if (staged_smem_arithmetic(m_op))
	{
		u64 &destination = accumulator(BIT(m_op, 8));
		bool const with_carry = (m_op & 0xfe00) == 0x0600;
		bool const with_borrow = (m_op & 0xfe00) == 0x0e00;
		bool const shift16 = (m_op & 0xfc00) == 0x3c00 || (m_op & 0xfc00) == 0x4000;
		u64 const memory = with_carry || with_borrow || staged_unsigned_memory_arithmetic(m_op) ? m_pipeline_operand[5] : data_operand(m_pipeline_operand[5]);
		// ADDC/SUBB retain unsigned Smem in Read, but consume the accumulator
		// and carry in Execute, including an adjacent ALU's carry output.
		u64 const operand = with_carry ? memory + unsigned(BIT(m_st0, 11)) : with_borrow ? memory + unsigned(!BIT(m_st0, 11)) : staged_ts_arithmetic(m_op) ? shifted_by_t(m_pipeline_operand[5], m_pipeline_immediate[5]) : shift16 ? (memory << 16) & ACC_MASK : memory;
		destination = (m_op & 0xfe00) == 0x1e00 ? conditional_subtract(destination, m_pipeline_operand[5], BIT(m_op, 8)) :
			add_sub(shift16 ? accumulator(BIT(m_op, 9)) : destination, operand, shift16 ? (m_op & 0xfc00) == 0x4000 : BIT(m_op, 11), BIT(m_op, 8), shift16);
		if (staged_extended_smem_arithmetic(m_op)) ++m_pc;
	}
	else if (staged_absolute_add(m_op))
	{
		u64 &destination = accumulator(BIT(m_op, 8));
		destination = add_sub(destination, data_operand(m_pipeline_operand[5]), false, BIT(m_op, 8));
		++m_pc;
	}
	else if (staged_immediate_multiply(m_op))
	{
		s64 product = s64(s16(m_pipeline_operand[5])) * s16(m_pipeline_immediate[5]);
		if (BIT(m_st1, 6)) product *= 2;
		if ((m_op & 0xfcff) == 0xf067 || (m_op & 0xfc00) == 0x6400)
		{
			product = multiply_product(product);
			u64 const source = accumulator(BIT(m_op, 9));
			product += s64(source << 24) >> 24;
		}
		accumulator(BIT(m_op, 8)) = multiply_result(product, BIT(m_op, 8));
		m_pc += staged_memory_immediate_multiply(m_op) && u8(m_op) >= 0xe0 ? 2 : 1;
	}
	else if (staged_immediate_alu(m_op))
	{
		++m_pc;
		bool const high_immediate = (m_op & 0xfcf8) == 0xf060;
		unsigned const operation = high_immediate ? m_op & 7 : (m_op >> 4) & 15;
		u64 const source = accumulator(BIT(m_op, 9));
		u64 &destination = accumulator(BIT(m_op, 8));
		if (operation <= 1)
		{
			u64 const operand = (data_operand(m_pipeline_immediate[5]) << (high_immediate ? 16 : m_op & 15)) & ACC_MASK;
			destination = add_sub(source, operand, operation == 1, BIT(m_op, 8));
		}
		else
		{
			u64 const operand = u64(m_pipeline_immediate[5]) << (high_immediate ? 16 : m_op & 15);
			destination = operation == 3 ? source & operand : operation == 4 ? source | operand : source ^ operand;
		}
	}
	else if ((m_op & 0xfef0) == 0xf020 || (m_op & 0xfeff) == 0xf062)
	{
		++m_pc;
		unsigned const shift = (m_op & 0xff) == 0x62 ? 16 : m_op & 15;
		accumulator(BIT(m_op, 8)) = (data_operand(m_pipeline_operand[5]) << shift) & ACC_MASK;
	}
	else if (staged_memory_logic(m_op))
	{
		if (staged_extended_logic(m_op)) ++m_pc;
		u64 &destination = accumulator(BIT(m_op, 8));
		u64 const operand = m_pipeline_operand[5]; // Logical Smem is zero-extended, independent of SXM.
		unsigned const operation = (m_op >> 9) & 3;
		destination = (operation == 0 ? destination & operand : operation == 1 ? destination | operand : destination ^ operand) & ACC_MASK;
	}
	else if (staged_immediate_arp(m_op))
		; // Access publication must not overwrite a younger indirect ARP update.
	else if (staged_immediate_asm(m_op))
		// SPRU131G example 7-52: the following ASM consumer has no gap.
		m_st1 = (m_st1 & ~u16(0x001f)) | (m_op & 0x001f);
	else if ((m_op & 0xfcf8) == 0xf4a8)
		m_st0 = (m_st0 & ~u16(0x1000)) | (m_pipeline_operand[5] ? 0x1000 : 0);
	else if (staged_mar(m_op))
	{
		if (staged_long_mar(m_op)) ++m_pc;
	}
	else if ((m_op & 0xfcff) == 0xf480 || (m_op & 0xfcff) == 0xf481 || (m_op & 0xfcff) == 0xf482)
	{
		u16 const status = m_st1;
		m_st1 = (m_st1 & ~u16(0x011f)) | m_pipeline_alu_mode[5];
		execute_register_effects(m_op);
		m_st1 = status;
	}
	else if (!staged_xc(m_op))
		execute_register_effects(m_op);
	if ((m_op & 0xfeff) == 0xf6bf && bool(m_st1 & ST1_BRAF) != braf_before) pipeline_update_braf(address);
	if (staged_pipelined_repeat(m_op) && m_pipeline_repeat_active && m_pipeline_repeat_body_latched && address == m_pipeline_repeat_address)
	{
		if (m_rptc) { --m_rptc; ++m_rpt_iteration; }
		else
		{
			m_pipeline_repeat_active = m_pipeline_repeat_body_latched = false;
			m_rpt_iteration = 0;
			pipeline_resume_after_repeat();
		}
	}
	if ((m_op == 0xf495 || staged_write_a(m_op) || staged_repeat_accumulator_load(m_op) || (m_op & 0xfe00) == 0x1e00) && m_pipeline_repeat_active && m_pipeline_repeat_body_latched && address == m_pipeline_repeat_address)
		pipeline_begin_repeat_stream();
	// A younger decoded XC extends protection. In a short repeat loop,
	// an older instance can retire at the same PC as the newer span's end;
	// that older retirement must not clear the younger owner's protection.
	if (m_pipeline_xc_active && (address & 0x7f0000) == (m_pipeline_xc_end & 0x7f0000) &&
		u16(m_pipeline_xc_end - address) < ((staged_opcode_descriptor[m_op] >> 2) & 3))
	{
		bool younger_xc = false;
		for (unsigned stage = 2; stage < 5; ++stage)
			younger_xc |= m_pipeline_valid[stage] && staged_xc(m_pipeline_word[stage]);
		if (!younger_xc) m_pipeline_xc_active = false;
	}
	// A backward branch can retire alongside its target IDLE's Decode.
	// That older branch must not consume the younger one-word guard.
	if (m_pipeline_control_words && (address & 0x7f0000) == (m_pipeline_control_address & 0x7f0000) &&
		(address == m_pipeline_control_address || (!staged_idle(m_pipeline_control_word) && u16(address - m_pipeline_control_address) <= 3)))
	{
		unsigned const words = (staged_opcode_descriptor[m_op] >> 2) & 3;
		m_pipeline_control_words = words >= m_pipeline_control_words ? 0 : m_pipeline_control_words - words;
	}
	if (m_pipeline_vector_pending && u16(address) == m_interrupt_vector_pc)
		m_pipeline_vector_pending = false;
	m_pipeline_intm_protected[5] = false;
	update_xf();
	return true;
}

void tms320c54x_device::pipeline_publish_read_results(bool smem_t_product_read)
{
	if (m_pipeline_valid[4] && register_product_form(m_pipeline_word[4]))
		// SPRU131G 7.5.6/table 7-13: multiply is not a Category-I TS consumer.
		// Retain T after older Execute but before younger Read publications;
		// sampling live T in Execute could instead consume a following STM.
		m_pipeline_operand[4] = m_t;
	if (m_pipeline_valid[4] && staged_load_mac(m_pipeline_word[4]))
		// Retain T after older Execute publications; X loads the accumulator, not T.
		m_pipeline_t[4] = m_t;
	if (smem_t_product_read || (m_pipeline_valid[4] && (staged_poly(m_pipeline_word[4]) || staged_parallel_product(m_pipeline_word[4]))))
		m_pipeline_immediate[4] = m_t;
	if (m_pipeline_valid[4] && (staged_dual_multiply(m_pipeline_word[4]) || staged_coefficient_mac(m_pipeline_word[4]) || (m_pipeline_word[4] & 0xff00) == 0x4c00))
		// MPY/MAC/MACSU/LTD load T in Read (SPRU172C 4-101/4-82/4-91/4-81). An older
		// Execute write must not overwrite this younger publication.
		m_t = m_pipeline_operand[4];
	if (m_pipeline_valid[4] && staged_parallel_load(m_pipeline_word[4]) && (m_pipeline_word[4] & 0xfd00) == 0xe400)
		// The load side owns T in Read; do not republish it in Execute or
		// let an older Execute write erase this younger publication.
		m_t = m_pipeline_operand[4];
	if (m_pipeline_valid[4] && staged_smem_special(m_pipeline_word[4]))
	{
		// LD T, memory SQUR and MPYA/MACA/MASA publish T in Read after older Execute.
		// T-based Smem MPY/MAC/MAS consume retained T without publishing it.
		if ((m_pipeline_word[4] & 0xfe00) == 0x2600 ||
			((m_pipeline_word[4] & 0xf000) != 0x2000 && (m_pipeline_word[4] & 0xff00) != 0x3200 && !staged_poly(m_pipeline_word[4]) && !staged_bitt(m_pipeline_word[4])))
			m_t = m_pipeline_operand[4];
	}
	if (m_pipeline_valid[4] && staged_dst(m_pipeline_word[4]))
	{
		// Retain the source after older Execute writes, then occupy both
		// write clocks (SPRU172C classes 13A/13B). Placement is a partial model.
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
		pipeline_restore_block_on_return(m_pipeline_address[4]);
		// Both delay words have reached Decode; discard the next sequential word.
		m_pipeline_decode_flush = 1;
	}
}

void tms320c54x_device::pipeline_prefetch(bool prefetch_bubble, bool software_intr_prefetch, u32 software_intr_physical)
{
	if (!m_pipeline_flush_front && !prefetch_bubble && !m_pipeline_idle_barrier && !m_pipeline_idle_resume_pending && !m_pipeline_repeat_body_latched && !m_idle)
	{
		m_pipeline_address[0] = m_pipeline_fetch_address;
		// SPRU131G 7.5.9: program mapping belongs to Prefetch, not Fetch.
		m_pipeline_physical_address[0] = software_intr_prefetch ? software_intr_physical : physical_program_address(m_pipeline_fetch_address);
		m_pipeline_valid[0] = true;
		m_pipeline_block_first_final[0] = m_pipeline_block_end[0] = false;
		m_pipeline_block_body[0] = false;
		m_pipeline_fetch_address = (m_pipeline_fetch_address & 0x7f0000) | u16(m_pipeline_fetch_address + 1);
		if (m_pipeline_block_active && m_pipeline_block_fetch_enabled)
		{
			// SPRA599A section 2: REA does not compare XPC. A foreign
			// page can consume this loop; RSA reload changes PC, not XPC.
			m_pipeline_block_body[0] = u16(m_pipeline_address[0] - m_pipeline_block_start) <= u16(m_pipeline_block_end_address - m_pipeline_block_start);
			m_pipeline_block_first_final[0] = u16(m_pipeline_address[0]) == m_rsa && m_pipeline_block_fetch_remaining == 1;
			m_pipeline_block_end[0] = u16(m_pipeline_address[0]) == m_rea;
			if (m_pipeline_block_end[0] && m_pipeline_block_fetch_remaining)
				if (--m_pipeline_block_fetch_remaining) m_pipeline_fetch_address = (m_pipeline_address[0] & 0x7f0000) | m_rsa;
		}
	}
}

void tms320c54x_device::pipeline_begin_repeat_stream()
{
	m_pipeline_repeat_word = m_op;
	m_pipeline_repeat_store_pending = false;
	m_pipeline_repeat_io_started = false;
	m_pipeline_repeat_source = m_pipeline_immediate[5];
	m_pipeline_repeat_program_source = staged_write_a(m_op) ? m_pipeline_write_a_logical : m_pipeline_read_a_source;
	m_pipeline_repeat_address = m_pipeline_address[5];
	m_pipeline_repeat_status = m_pipeline_direct_status[5];
	m_pipeline_repeat_sp = m_pipeline_direct_sp[5];
	m_pipeline_valid.fill(false);
	m_pipeline_repeat_stream = m_rptc != 0;
	if (!m_rptc)
	{
		m_pipeline_repeat_active = m_pipeline_repeat_body_latched = false;
		pipeline_resume_after_repeat();
	}
}

void tms320c54x_device::pipeline_restore_block_on_return(u32 address)
{
	if (!(m_st1 & ST1_BRAF) || (address >= m_pipeline_block_start && address <= m_pipeline_block_end_address)) return;
	u32 const restored_start = (m_pipeline_fetch_address & 0x7f0000) | m_rsa;
	u32 const restored_end = (m_pipeline_fetch_address & 0x7f0000) | m_rea;
	u32 const after_end = (m_pipeline_fetch_address & 0x7f0000) | u16(m_rea + 1);
	if ((m_pipeline_fetch_address < restored_start || m_pipeline_fetch_address > restored_end) && m_pipeline_fetch_address != after_end) return;
	// The ISR/callback may have used the one loop controller itself. Software
	// restored its architectural registers; rebuild only the fetch prediction,
	// before the return target is prefetched (Read for delayed far returns).
	m_pipeline_block_start = restored_start;
	m_pipeline_block_end_address = restored_end;
	m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
	m_pipeline_block_active = m_pipeline_block_fetch_enabled = true;
	if (m_pipeline_fetch_address == after_end) m_pipeline_fetch_address = restored_start;
}

bool tms320c54x_device::pipeline_early_braf_clear(unsigned stage, u16 value) const
{
	return m_pipeline_valid[stage] && m_pipeline_block_body[stage] &&
		(m_pipeline_word[stage] == 0x7707 || m_pipeline_word[stage] == 0x8a07) &&
		!(value & ST1_BRAF) && (value & ~ST1_BRAF) == (m_st1 & ~ST1_BRAF) &&
		u16(m_pipeline_block_end_address - m_pipeline_address[stage]) >= 6;
}

void tms320c54x_device::pipeline_update_braf(u32 address)
{
	if (!(m_st1 & ST1_BRAF))
	{
		// Software disables future loop prediction, not words already fetched.
		m_pipeline_block_active = m_pipeline_block_fetch_enabled = false;
		m_pipeline_block_fetch_remaining = 0;
		return;
	}
	if (m_pipeline_block_active) return;
	u32 const start = (address & 0x7f0000) | m_rsa;
	u32 const end = (address & 0x7f0000) | m_rea;
	if ((m_pipeline_fetch_address & 0x7f0000) != (address & 0x7f0000) ||
		m_pipeline_fetch_address > start || start > end)
		// Context restoration can enable BRAF outside its body; the return
		// boundary separately reconstructs that predictor from RSA/REA/BRC.
		return;
	m_pipeline_block_start = start;
	m_pipeline_block_end_address = end;
	m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
	m_pipeline_block_active = m_pipeline_block_fetch_enabled = true;
}

void tms320c54x_device::pipeline_request_coefficient(u16 address)
{
	u32 const source = physical_program_address((m_pipeline_address[4] & 0x7f0000) | address);
	bool const repeated = m_pipeline_repeat_active && m_pipeline_repeat_body_latched &&
		m_pipeline_address[4] == m_pipeline_repeat_address;
	if (repeated && m_pipeline_repeat_coefficient_started)
		// A running repeat uses PB for coefficients, not opcode refetches.
		m_pipeline_immediate[4] = m_program.read_word(source);
	else
	{
		m_pipeline_coefficient_source = source;
		m_pipeline_coefficient_pending = true;
		if (repeated) m_pipeline_repeat_coefficient_started = true;
	}
}

void tms320c54x_device::pipeline_finish_repeat_iteration()
{
	if (m_coverage && !m_coverage->count[m_pipeline_repeat_word]++)
		m_coverage->first_pc[m_pipeline_repeat_word] = m_pipeline_repeat_address;
	if (!--m_rptc)
	{
		m_pipeline_repeat_active = m_pipeline_repeat_body_latched = m_pipeline_repeat_stream = false;
		m_rpt_iteration = 0;
		pipeline_resume_after_repeat();
		pipeline_accept_interrupt();
	}
}

void tms320c54x_device::pipeline_repeat_port()
{
	bool const write = BIT(m_pipeline_repeat_word, 8);
	if (!m_pipeline_repeat_io_started)
	{
		u8 const mode = m_pipeline_repeat_word;
		u16 const address = BIT(mode, 7) ? short_smem_address(mode) : BIT(m_pipeline_repeat_status, 14)
			? u16(m_pipeline_repeat_sp + mode) : u16((m_pipeline_repeat_status & 0x1ff) << 7) | mode;
		if (address < 0x60)
		{
			logerror("%s: staged repeated port mapped operand is unsupported\n", machine().describe_context()); m_illegal = true; return;
		}
		m_pipeline_repeat_store_address = address;
		if (write) m_pipeline_repeat_io_value = data_read(address);
		if (BIT(mode, 7)) indirect_modify(mode);
		m_pipeline_io_wait_remaining = software_wait_enabled() ? external_io_wait_states() : 0;
		m_pipeline_io_ready_required = m_pipeline_io_wait_remaining >= 2;
		m_pipeline_repeat_io_started = true;
		if (write) return; // Retain the RAM Read before the port Write.
	}
	if (m_pipeline_io_wait_remaining) { --m_pipeline_io_wait_remaining; return; }
	if (m_pipeline_io_ready_required && !m_ready_in_cb()) return;
	m_pipeline_io_ready_required = false;
	if (write) m_io.write_word(m_pipeline_repeat_source, m_pipeline_repeat_io_value);
	else if (!m_pipeline_repeat_store_pending)
	{
		m_pipeline_repeat_io_value = m_io.read_word(m_pipeline_repeat_source);
		m_pipeline_repeat_store_pending = true;
		return;
	}
	else data_write(m_pipeline_repeat_store_address, m_pipeline_repeat_io_value);
	m_pipeline_repeat_io_started = m_pipeline_repeat_store_pending = false;
	if (!m_instruction_observer_cb.isunset()) m_instruction_observer_cb(m_pipeline_repeat_address, m_pipeline_repeat_word);
	++m_rpt_iteration;
	pipeline_finish_repeat_iteration();
}

void tms320c54x_device::pipeline_resume_after_repeat()
{
	// RPT in a qualified control's first delay word discards target prefetch. The
	// repeated second word must drain before that retained target resumes.
	if (m_pipeline_repeat_redirect_pending)
	{
		m_pipeline_fetch_address = m_pipeline_repeat_redirect_target;
		m_pipeline_repeat_redirect_pending = false;
		return;
	}
	u32 const next = (m_pipeline_repeat_address & 0x7f0000) | u16(m_pipeline_repeat_address +
		(staged_repeat_advancing_literal(m_pipeline_repeat_word) || staged_repeat_extension_shift(m_pipeline_repeat_word) ? 2 : staged_pipelined_repeat(m_pipeline_repeat_word) ? 1 : staged_repeat_body_words(m_pipeline_repeat_word)));
	// Inner RPT discards undecoded prefetch, including any predicted block
	// wrap. Rebuild that prediction from Decode-owned BRC, not RPTC. The
	// outer endpoint was decoded once, never once per repeated body copy.
	if (m_pipeline_block_active && m_pipeline_block_fetch_enabled &&
		m_pipeline_repeat_address == ((m_pipeline_block_start & 0x7f0000) | u16(m_pipeline_block_start - 1)))
		m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
	if (m_pipeline_block_active && m_pipeline_block_fetch_enabled &&
		m_pipeline_repeat_address >= m_pipeline_block_start && m_pipeline_repeat_address <= m_pipeline_block_end_address)
	{
		m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
		u32 const after_end = (m_pipeline_block_end_address & 0x7f0000) | u16(m_pipeline_block_end_address + 1);
		m_pipeline_fetch_address = next == after_end ? m_pipeline_block_start : next;
	}
	else m_pipeline_fetch_address = next;
}

void tms320c54x_device::enter_idle(u8 mode)
{
	// IDLE2/3 stop peripheral clocks independently of TSS; retain PSC phase.
	if (mode >= 2 && !(m_tcr & TIMER_TSS))
	{
		m_idle_timer_ticks = timer_remaining_ticks();
		update_timer_counter();
		m_timer->adjust(attotime::never);
	}
	m_idle_mode = mode;
	// IDLE3 drops PLL lock; software must acquire it again after wake.
	if (mode == 3) m_pll_locked = false;
	m_peripheral_clock_stop_cb(mode >= 2);
	m_pll_clock_stop_cb(mode == 3);
	m_idle = true;
}

void tms320c54x_device::pipeline_delay_compiler_sp(u16 previous)
{
	// Table 7-9 Category-I consumers must not use a newly published SP
	// before its compiler-addressing path is ready. Stack operations use
	// architectural SP directly and are not stalled by this window.
	m_pipeline_compiler_sp_previous = previous;
	m_pipeline_compiler_sp_wait = 2;
}

void tms320c54x_device::pipeline_accept_interrupt()
{
	// No pipeline hazard can make an absent or masked source acceptable.
	// Check after this clock's publications, just as accept_interrupt does.
	if (!m_nmi_pending && ((m_st1 & ST1_INTM) || !(m_ifr & m_imr)))
		return;
	// Acceptance occurs after this cycle's Decode. Retain decoded slots,
	// replace the next Decode with INTR, and drain the prefetched bus word.
	bool intm_protected = m_pipeline_intm_follow;
	for (unsigned stage = 0; stage < 5; ++stage)
		intm_protected |= m_pipeline_valid[stage] && m_pipeline_intm_protected[stage];
	// Qualified near-branch / two-word LD composition: the branch has
	// retired and the load has reached Access. It drains ahead of INTR
	// (SPRU131G 7.2); waiting for its Execute can starve a short polling loop.
	// Calls, returns and flag-writing delay instructions retain their guards.
	bool near_branch_retired = !m_pipeline_decode_flush &&
		(m_pipeline_control_word == 0xf273 || (staged_conditional_branch(m_pipeline_control_word) && !staged_conditional_call(m_pipeline_control_word) && BIT(m_pipeline_control_word, 9)));
	for (unsigned stage = 2; stage < 5; ++stage)
		if (m_pipeline_valid[stage] && m_pipeline_address[stage] == m_pipeline_control_address) near_branch_retired = false;
	bool const retained_load_delay = !m_pipeline_delay_decode_words && m_pipeline_valid[3] &&
		m_pipeline_address[3] == m_pipeline_control_address + 2 && staged_absolute_load(m_pipeline_word[3]);
	bool const control_protected = m_pipeline_control_words && !(near_branch_retired && retained_load_delay);
	if ((!intm_protected || m_nmi_pending) && !m_interrupt_entry_stage && !m_pipeline_vector_pending && !m_pipeline_decode_remaining &&
		!m_pipeline_peripheral_read_pending && !m_pipeline_mvdp_wait && !m_pipeline_read_a_wait && !m_pipeline_write_a_wait && !m_pipeline_acc_transfer_pending && !m_pipeline_cc_pending && !m_pipeline_repeat_active && !m_pipeline_xc_active && !control_protected && !m_pipeline_long_store_pending && !m_pipeline_program_move_pending && !m_pipeline_coefficient_pending && !m_pipeline_dp_wait && accept_interrupt())
	{
		m_pipeline_return_pc = u16(m_pipeline_valid[1] ? m_pipeline_address[1] :
			m_pipeline_valid[0] ? m_pipeline_address[0] : m_pipeline_fetch_address);
		// Undecoded prefetch predictions are discarded at INTR acceptance.
		// Decode already owns the architectural loop count at the return PC.
		if (m_pipeline_block_active && m_pipeline_block_fetch_enabled)
			m_pipeline_block_fetch_remaining = u32(m_brc) + 1;
		m_pipeline_flush_front = m_pipeline_vector_pending = true;
	}
}

bool tms320c54x_device::pipeline_mapped_accumulator_write(u16 address) const
{
	return address >= 8 && address <= 0x0d && m_pipeline_valid[5] &&
		(m_pipeline_word[5] == (0x8a00 | address) || m_pipeline_word[5] == (0x7700 | address) ||
		((staged_store(m_pipeline_word[5]) || staged_mvdd(m_pipeline_word[5]) || staged_program_move(m_pipeline_word[5])) && m_pipeline_operand_address[5] == address));
}

u16 tms320c54x_device::pipeline_store_word() const
{
	// Younger Read bypass and the real Execute write must publish the same
	// word, including the store's retained shift rather than live ST1.ASM.
	u16 const word = m_pipeline_word[5];
	if (staged_conditional_store(word))
	{
		if (!staged_accumulator_condition(word, BIT(word, 3) ? m_b : m_a))
			return m_pipeline_operand[5]; // A false condition still writes back the Read sample.
		if (!BIT(word, 9)) return BIT(word, 8) ? m_brc : m_t;
		int const shift = s8((m_pipeline_immediate[5] & 31) << 3) >> 3;
		u64 const source = (BIT(word, 8) ? m_b : m_a) & ACC_MASK;
		u64 const value = shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) : (source << shift) & ACC_MASK;
		// SACCD is not an SST-saturating store (SPRU131G table 4-3).
		return u16(value >> 16);
	}
	if (staged_parallel_alu(word) || staged_parallel_product(word) || staged_parallel_load(word))
	{
		int const shift = s8((m_pipeline_direct_status[5] & 31) << 3) >> 3;
		u64 const source = BIT(word, 9) ? m_b : m_a;
		u64 const value = shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) : (source << shift) & ACC_MASK;
		return u16(saturated_store(value) >> 16);
	}
	if (staged_shifted_memory_store(word, m_pipeline_immediate[5]))
	{
		u16 const extension = m_pipeline_immediate[5];
		int const shift = s8((extension & 31) << 3) >> 3;
		u64 const source = BIT(extension, 8) ? m_b : m_a;
		u64 const value = shift < 0 ? (BIT(m_st1, 8) ? arithmetic_shift_right(source, -shift) : source >> -shift) : (source << shift) & ACC_MASK;
		return u16(saturated_store(value) >> ((extension & 0xfee0) == 0x0c60 ? 16 : 0));
	}
	if ((word & 0xff00) == 0x7600 || staged_ram_stm(word))
		return m_pipeline_immediate[5];
	if ((word & 0xfe00) == 0x8c00)
		return BIT(word, 8) ? m_trn : m_t;
	u64 value = BIT(word, 8) ? m_b : m_a;
	if (staged_asm_store(word))
	{
		int const shift = s8((m_pipeline_immediate[5] & 31) << 3) >> 3;
		value = shift < 0 ? BIT(m_st1, 8) ? arithmetic_shift_right(value, -shift) : value >> -shift : (value << shift) & ACC_MASK;
	}
	else if (staged_xmem_store(word))
		value = (value << (word & 15)) & ACC_MASK;
	return u16(saturated_store(value) >> (BIT(word, 9) ? 16 : 0));
}

bool tms320c54x_device::observe_staged_data_read(u16 address, u32 &instruction, u16 &opcode) const
{
	if (!m_observed_data_read || address != m_observed_data_address || machine().side_effects_disabled()) return false;
	instruction = m_observed_data_instruction; opcode = m_observed_data_opcode;
	return true;
}

bool tms320c54x_device::observe_staged_mvdd_write(u16 address, u32 &instruction, u16 &opcode, u16 &source) const
{
	if (!m_observed_mvdd_write || address != m_pipeline_operand_address[5] || machine().side_effects_disabled()) return false;
	instruction = m_pipeline_address[5]; opcode = m_pipeline_word[5]; source = m_pipeline_immediate[5];
	return true;
}

u16 tms320c54x_device::pipeline_read_data(u16 address, unsigned stage)
{
	// SPRU131G 7.5.10/example 7-67: a mapped accumulator write followed
	// by a mapped read has no conflict. Program/data copies retained their
	// source word; forward it without moving the real EB write
	// or relaxing the separate direct-ALU-to-mapped-read latency.
	if (address >= 8 && address <= 0x0d && m_pipeline_valid[5] &&
		(staged_read_a(m_pipeline_word[5]) || staged_mvdd(m_pipeline_word[5]) || staged_program_move(m_pipeline_word[5])) && address == m_pipeline_operand_address[5])
		return address == 0x0a || address == 0x0d ? u8(m_pipeline_operand[5]) : m_pipeline_operand[5];
	// SPRU131G 7.3.2: a same-location on-chip read can bypass a pending
	// write through an internal bus. Preserve the real EB write and flags;
	// This partial model covers retained copies, RMW, ordinary and double stores,
	// not bank contention or stack instructions' separate read ordering.
	if (address >= 0x60 && m_pipeline_valid[5] && staged_dst(m_pipeline_word[5]))
	{
		// Both DST words were retained in Read. The real EB writes still
		// occupy Execute and its following hold clock; do not reread live A/B.
		if (address == m_pipeline_operand_address[5]) return m_pipeline_operand[5];
		if (address == (m_pipeline_operand_address[5] ^ 1)) return m_pipeline_immediate[5];
	}
	if (address >= 0x60 && m_pipeline_valid[5] && address == m_pipeline_operand_address[5] &&
		(staged_data_copy(m_pipeline_word[5]) || staged_port_read(m_pipeline_word[5]) || staged_read_a(m_pipeline_word[5]) ||
		(staged_mvdd(m_pipeline_word[5]) && !staged_mvdd(m_pipeline_word[stage]))))
		// Copies, PORTR and READA already retained their Read sample. An adjacent
		// consumer must not see the old RAM word while EB is still pending.
		// Forward the sample without another I/O access; preserve the EB write.
		// Pipelined MVDD iterations retain their separately qualified
		// Read-before-older-Write ordering rather than using this bypass.
		return m_pipeline_operand[5];
	if (address >= 0x60 && m_pipeline_valid[5] && address == m_pipeline_operand_address[5] &&
		(staged_ram_stm(m_pipeline_word[5]) || staged_store(m_pipeline_word[5]) || staged_xmem_store(m_pipeline_word[5]) || staged_conditional_store(m_pipeline_word[5]) || staged_parallel_alu(m_pipeline_word[5]) || staged_parallel_product(m_pipeline_word[5]) || staged_parallel_load(m_pipeline_word[5]) || staged_shifted_memory_store(m_pipeline_word[5], m_pipeline_immediate[5]) || (m_pipeline_word[5] & 0xff00) == 0x7600))
		return pipeline_store_word();
	if (address >= 0x60 && m_pipeline_valid[5] && address == m_pipeline_operand_address[5] && staged_rmw(m_pipeline_word[5]))
	{
		u16 const value = m_pipeline_operand[5], immediate = m_pipeline_immediate[5];
		return (m_pipeline_word[5] & 0xff00) == 0x6b00 ? add_memory_word(value, immediate, false) :
			(m_pipeline_word[5] & 0xff00) == 0x6a00 ? value ^ immediate : BIT(m_pipeline_word[5], 8) ? value | immediate : value & immediate;
	}
	return pipeline_observed_read(address, stage);
}

u16 tms320c54x_device::pipeline_observed_read(u16 address, unsigned stage)
{
	// MVDK retains its operand in Address; ordinary operands arrive in Read.
	// This transient identity surrounds only the actual bus access, not forwarding.
	m_observed_data_read = m_pipeline_valid[stage];
	m_observed_data_address = address;
	m_observed_data_instruction = m_pipeline_address[stage];
	m_observed_data_opcode = m_pipeline_word[stage];
	u16 const value = m_data.read_word(address);
	m_observed_data_read = false;
	return value;
}

u16 tms320c54x_device::add_memory_word(u16 value, u16 immediate, bool publish_status)
{
	const s32 left = BIT(m_st1, 8) ? s16(value) : s32(value);
	const s32 right = BIT(m_st1, 8) ? s16(immediate) : s32(immediate);
	const s32 result = left + right;
	const bool overflow = result < -0x8000 || result > 0x7fff;
	// ADDM stores 16 bits, but the ALU carry is taken at bit 32.
	if (publish_status)
	{
		m_st0 = (m_st0 & ~u16(0x0800)) |
			(u64(u32(left)) + u32(right) > 0xffffffffULL ? 0x0800 : 0);
		if (overflow) m_st0 |= 0x0400;
	}
	return overflow && BIT(m_st1, 9)
		? u16(std::clamp(result, s32(-0x8000), s32(0x7fff))) : u16(result);
}

void tms320c54x_device::execute_register_effects(u16 const op)
{
	// Register-only effects left after ordered pipeline interception. Caller
	// opcode sets are checked by c54x_dispatch_audit; this owns no bus timing.
	const u8 low = op;
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
	if ((op & 0xfce0) == 0xf460) // SFTA src, SHIFT, dst
	{
		const int shift = s8(u8(op << 3)) >> 3;
		const u64 source = accumulator(BIT(op, 9)) & ACC_MASK;
		// SPRU172C 4-155's left-carry formula disagrees with 4-156 example 2.
		// Preserve the formula/zero-shift policy pending independent evidence.
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
	switch (op & 0xff00)
	{
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
	default:
		break;
	}

	switch (op)
	{
	case 0xf493: // CMPL source accumulator, destination accumulator
	case 0xf593:
	case 0xf693:
	case 0xf793:
		accumulator(BIT(op, 8)) = ~accumulator(BIT(op, 9)) & ACC_MASK;
		return;
	case 0xf495: // NOP
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
		if (m_illegal || m_idle) { m_icount = 0; break; }
		if (m_pipeline_idle_resume_pending)
		{
			m_pipeline_idle_resume_pending = false;
			if (!m_interrupt_entry_stage && !m_pipeline_vector_pending)
				m_pipeline_fetch_address = m_pipeline_idle_resume_address;
			pipeline_accept_interrupt();
		}
		pipeline_step();
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
