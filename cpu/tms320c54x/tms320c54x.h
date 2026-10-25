// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_CPU_TMS320C54X_TMS320C54X_H
#define MAME_CPU_TMS320C54X_TMS320C54X_H

#pragma once

#include <map>

class tms320c54x_device : public cpu_device
{
public:
	tms320c54x_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock);
	void set_power_on_imr(u16 value) { m_power_on_imr = value; }
	void set_extended_program(bool enabled) { m_extended_program = enabled; }
	// Input clock for TI-family PLLSTATUS timing and optional rate selection.
	void set_pll_reference_clock(u32 hz) { m_pll_reference_clock = hz; }
	void set_follow_clkmd(bool enabled) { m_follow_clkmd = enabled; }
	void set_reset_clkmd(u16 value, bool enabled = true) { m_reset_clkmd = value; m_reset_clkmd_enabled = enabled; }
	// Opt-in compatibility; retained I/O has bounded RAM-operand acceptance.
	void set_c5416_bus_profile(bool enabled) { m_c5416_bus_profile = enabled; }
	// SPRU131G table 10-2 only; no C5416 BSCR/SWCR or external RAM map implied.
	void set_standard_swwsr(bool enabled) { m_standard_swwsr = enabled; }
	unsigned external_io_wait_states() const { return software_wait_enabled() ? ((m_swwsr >> 12) & 7) * (m_c5416_bus_profile ? 1 + (m_swcr & 1) : 1) : 0; }
	unsigned clkout_divisor() const { return m_c5416_bus_profile ? 1 + ((m_bscr >> 13) & 3) : 1; }
	bool iack_output_enabled() const { return m_c5416_bus_profile && !(m_bscr & 0x1000); }
	auto clkout_divisor_cb() { return m_clkout_divisor_cb.bind(); }
	auto iack_enable_cb() { return m_iack_enable_cb.bind(); }
	auto ready_in_cb() { return m_ready_in_cb.bind(); }
	// Experimental stage-owned frontend; unsupported instruction forms stop,
	// rather than falling back to instruction-boundary interrupt recognition.
	void set_pipeline_frontend(bool enabled);
	u64 pipeline_cycles() const { return m_pipeline_cycles; }
	auto bio_in_cb() { return m_bio_in_cb.bind(); }
	auto xf_out_cb() { return m_xf_out_cb.bind(); }
	auto peripheral_clock_stop_cb() { return m_peripheral_clock_stop_cb.bind(); }
	auto pll_clock_stop_cb() { return m_pll_clock_stop_cb.bind(); }
	auto system_clock_cb() { return m_system_clock_cb.bind(); }
	// Diagnostic Execute-entry observation, not a pin or retirement signal.
	// Offset is the logical instruction address; data is the captured opcode.
	// Observers must not modify guest state; other registers are live, not a snapshot.
	auto instruction_observer_cb() { return m_instruction_observer_cb.bind(); }
	// Valid only for the matching staged Execute entry; no bus reads or retirement claim.
	bool observe_execute_memory_multiply(offs_t address, u16 opcode, u16 &source, u16 &operand, u16 &immediate) const;
	// Matching staged accumulator-store/direct-PSHM Execute slot; no memory access or write.
	bool observe_execute_memory_store(offs_t address, u16 opcode, u16 &destination, u16 &value) const;
	// Valid only inside an actual staged operand bus read, not forwarded values,
	// debugger peeks, DMA reads or Execute/retirement. Never accesses guest memory.
	bool observe_staged_data_read(u16 address, u32 &instruction, u16 &opcode) const;
	// Actual MVDD destination bus write; source is the retained operand address.
	bool observe_staged_mvdd_write(u16 address, u32 &instruction, u16 &opcode, u16 &source) const;

	enum : unsigned
	{
		STATE_PC = 1,
		STATE_A,
		STATE_B,
		STATE_T,
		STATE_SP,
		STATE_ST0,
		STATE_ST1,
		STATE_PMST,
		STATE_AR0,
		STATE_AR1,
		STATE_AR2,
		STATE_AR3,
		STATE_AR4,
		STATE_AR5,
		STATE_AR6,
		STATE_AR7,
		STATE_BRC,
		STATE_BK,
		STATE_IFR,
		STATE_IMR,
		STATE_IDLE,
		STATE_ILLEGAL,
		STATE_XPC,
		STATE_TIM,
		STATE_PRD,
		STATE_TCR,
		STATE_TRN,
		STATE_RTN,
		STATE_RC,
		STATE_SWWSR,
		STATE_BSCR,
		STATE_SWCR,
		STATE_RSA,
		STATE_REA,
		STATE_READPC,
		STATE_EXECPC
	};

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;
	virtual void device_post_load() override;
	virtual void device_clock_changed() override;

	virtual u32 execute_min_cycles() const noexcept override { return 1; }
	virtual u32 execute_max_cycles() const noexcept override { return 5; }
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	virtual space_config_vector memory_space_config() const override;
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;
	virtual void state_import(const device_state_entry &entry) override;
	virtual void state_export(const device_state_entry &entry) override;

private:
	static constexpr u64 ACC_MASK = (u64(1) << 40) - 1;
	static constexpr u16 TIMER_TDDR_MASK = 0x000f;
	static constexpr u16 TIMER_TSS = 0x0010;
	static constexpr u16 TIMER_TRB = 0x0020;
	static constexpr u16 TIMER_PSC_MASK = 0x03c0;
	static constexpr u16 TIMER_WRITABLE_MASK = 0x0c1f; // Free, Soft, TSS and TDDR; reserved bits read zero.

	TIMER_CALLBACK_MEMBER(timer_expired);
	TIMER_CALLBACK_MEMBER(pll_transition_complete);
	TIMER_CALLBACK_MEMBER(selected_clock_complete);
	void write_clkmd(u16 value);
	void publish_selected_clock();
	void update_timer_counter();
	void arm_timer();
	u64 timer_remaining_ticks() const;
	void leave_idle();
	void update_xf(bool force = false);
	u16 fetch();
	u32 logical_program_address(u16 address) const { return (u32(m_extended_program ? m_xpc : 0) << 16) | address; }
	u32 physical_program_address(u32 address) const
	{
		address &= m_extended_program ? 0x7fffff : 0xffff;
		// SPRU131G 3.2.5: OVLY shares the lower 32K across extended pages.
		// DMA accesses the address space directly and does not use this CPU mapping.
		return m_extended_program && BIT(m_pmst, 5) && !(address & 0x8000) ? address & 0xffff : address;
	}
	u32 program_address(u16 address) const { return physical_program_address(logical_program_address(address)); }
	void far_transfer(u32 address, bool delayed);
	u16 data_read(u16 address);
	void data_write(u16 address, u16 value, unsigned owner_stage = 5);
	void internal_data_map(address_map &map);
	void publish_bus_controls();
	u16 register_read(u16 address);
	void register_write(u16 address, u16 value);
	u16 direct_address(u8 mode) const;
	u16 short_smem_address(u8 mode, unsigned stage = 6) const;
	u16 prepare_smem_address(u8 mode);
	void finish_smem_access(u8 mode);
	u16 indirect_read(u8 mode);
	u16 immediate_smem_read(u8 mode, u16 &immediate);
	u16 mmr_address(u8 mode);
	void indirect_write(u8 mode, u16 value);
	u16 long_offset_address(u8 mode);
	u16 long_offset_address(u8 mode, unsigned ar, s16 offset, unsigned stage = 6);
	bool indirect_compatibility(unsigned stage) const;
	unsigned indirect_ar(u8 mode, unsigned stage = 6) const;
	void indirect_modify(u8 mode, bool long_operand = false, unsigned stage = 6);
	void dual_modify(u8 operand, unsigned stage = 6);
	void circular_modify(unsigned ar, s16 step, unsigned stage = 6);
	void execute_one(u16 op);
	bool software_wait_enabled() const { return m_standard_swwsr || m_c5416_bus_profile; }
	bool staged_bus_control(u16 op) const { return (software_wait_enabled() && op == 0x7728) || (m_c5416_bus_profile && (op == 0x7729 || op == 0x772b)); }
	bool staged_timer_control(u16 op) const { return op >= 0x7724 && op <= 0x7726; }
	bool staged_clock_control(u16 op) const { return op == 0x7758; }
	// BDXR retains its immediate until Execute, like BSPC. It must not use
	// the address-generator STM early-write path. Peripheral phase is partial.
	bool staged_serial_control(u16 op) const { return op == 0x7721 || op == 0x7722; }
	void retire_atomic_instruction(u16 instruction_pc, u16 previous_sp,
		bool repeat_was_armed, bool delayed, bool xc_guarded);
	bool service_interrupt();
	bool accept_interrupt();
	void interrupt_entry_step();
	void reset_pipeline();
	void pipeline_step();
	void pipeline_accept_interrupt();
	void pipeline_delay_compiler_sp(u16 previous);
	void pipeline_begin_repeat_stream();
	void pipeline_resume_after_repeat();
	void pipeline_restore_block_on_return(u32 address);
	void enter_idle(u8 mode);
	void finish_repeats();
	void push(u16 value);
	u16 pop();
	u64 data_operand(u16 value) const;
	u64 shifted_by_t(u16 memory, u16 t) const;
	u16 add_memory_word(u16 value, u16 immediate, bool publish_status = true);
	u16 pipeline_read_data(u16 address);
	u16 pipeline_observed_read(u16 address, unsigned stage);
	u16 pipeline_store_word() const;
	bool pipeline_mapped_accumulator_write(u16 address) const;
	u64 shifted_load(u64 source, int shift, bool destination_b);
	u64 add_sub(u64 source, u64 operand, bool subtract, bool destination_b, bool partial_carry = false);
	u64 conditional_subtract(u64 source, u16 memory, bool destination_b);
	void long_word_arithmetic(u16 op, u16 high, u16 low_word);
	void accumulator_memory_multiply(u16 op, u16 memory);
	void polynomial_step(u16 memory, u16 t);
	void square_accumulate_result(u16 word, u16 memory);
	void register_product_result(u16 word, u16 t);
	u64 memory_t_multiply(u16 op, u16 memory, u16 t, u64 source);
	s64 multiply_product(s64 product) const;
	u64 multiply_result(s64 result, bool destination_b);
	u32 saturated_store(u64 value) const;
	void parallel_store(u16 op, u16 address);
	u64 arithmetic_shift_right(u64 value, unsigned shift) const;
	u64 &accumulator(bool b) { return b ? m_b : m_a; }

	address_space_config m_program_config;
	address_space_config m_data_config;
	address_space_config m_io_config;
	devcb_read_line m_bio_in_cb;
	devcb_read_line m_ready_in_cb;
	devcb_write_line m_xf_out_cb;
	bool m_xf_level = true;
	devcb_write_line m_peripheral_clock_stop_cb;
	devcb_write_line m_pll_clock_stop_cb;

	memory_access<23, 1, -1, ENDIANNESS_LITTLE>::cache m_cache;
	memory_access<23, 1, -1, ENDIANNESS_LITTLE>::specific m_program;
	memory_access<16, 1, -1, ENDIANNESS_LITTLE>::specific m_data;
	memory_access<16, 1, -1, ENDIANNESS_LITTLE>::specific m_io;

	u16 m_pc = 0;
	u8 m_xpc = 0;
	u32 m_debug_pc = 0;
	u32 m_debug_pipeline_pc = 0xffffffff;
	u16 m_debug_timer = 0;
	u16 m_op = 0;
	u64 m_a = 0;
	u64 m_b = 0;
	u16 m_t = 0;
	u16 m_trn = 0;
	u16 m_sp = 0;
	u16 m_sp_address_base = 0;
	bool m_sp_address_pending = false;
	bool m_sp_address_late = false;
	u16 m_st0 = 0;
	u16 m_st1 = 0;
	u16 m_pmst = 0;
	u16 m_ar[8] = {};
	u16 m_brc = 0;
	u16 m_bk = 0;
	u16 m_rsa = 0;
	u16 m_rea = 0;
	u16 m_rtn = 0;
	u16 m_rptc = 0;
	u16 m_rpt_address = 0;
	static constexpr u32 REPEAT_END_UNSET = 0x10000;
	u32 m_rpt_end = REPEAT_END_UNSET; // Every 16-bit PC is a valid repeat endpoint.
	u16 m_rpt_iteration = 0;
	bool m_rpt_armed = false;
	u16 m_delayed_target = 0;
	u8 m_delayed_xpc = 0;
	bool m_delayed_far = false;
	u8 m_delayed_words = 0;
	u8 m_xc_guard = 0;
	std::array<u64, 2> m_atomic_xc_a{}, m_atomic_xc_b{};
	std::array<u16, 2> m_atomic_xc_st0{};
	u8 m_atomic_xc_valid = 0;
	u8 m_atomic_condition_words = 0;
	bool m_atomic_condition_redirect = false;
	u64 m_atomic_condition_before_a = 0, m_atomic_condition_before_b = 0;
	u16 m_atomic_condition_before_st0 = 0;
	bool m_pipeline_xc_active = false;
	u32 m_pipeline_xc_end = 0;
	u8 m_pipeline_xc_skip_words = 0;
	u8 m_intm_guard = 0;
	u16 m_ifr = 0;
	u16 m_imr = 0;
	bool m_nmi_pending = false;
	bool m_nmi_asserted = false;
	u16 m_vector_prefetch_word = 0;
	bool m_vector_prefetch_valid = false;
	u8 m_interrupt_entry_stage = 0;
	u16 m_interrupt_vector_pc = 0;
	u16 m_interrupt_stack_address = 0;
	int m_interrupt_ack_source = 0;
	u16 m_power_on_imr = 0;
	bool m_extended_program = false;
	u16 m_clkmd = 0;
	bool m_c5416_bus_profile = false;
	bool m_standard_swwsr = false;
	u16 m_swwsr = 0;
	u16 m_bscr = 0;
	u16 m_swcr = 0;
	devcb_write8 m_clkout_divisor_cb;
	devcb_write_line m_iack_enable_cb;
	u32 m_pll_reference_clock = 0;
	bool m_follow_clkmd = false;
	u16 m_reset_clkmd = 0;
	bool m_reset_clkmd_enabled = false;
	u32 m_reset_clock = 0;
	u32 m_timer_clock = 0;
	u32 m_pending_clock = 0;
	emu_timer *m_clock_timer = nullptr;
	devcb_write32 m_system_clock_cb;
	devcb_write16 m_instruction_observer_cb;
	bool m_observed_data_read = false;
	bool m_observed_mvdd_write = false;
	u16 m_observed_data_address = 0, m_observed_data_opcode = 0;
	u32 m_observed_data_instruction = 0;
	bool m_pll_locked = false;
	emu_timer *m_pll_timer = nullptr;
	u16 m_tim = 0xffff;
	u16 m_prd = 0xffff;
	u16 m_tcr = 0;
	u16 m_timer_head_tim = 0xffff;
	emu_timer *m_timer = nullptr;
	bool m_idle = false;
	u8 m_idle_mode = 0;
	u64 m_idle_timer_ticks = 0;
	bool m_illegal = false;
	// Synchronous peripheral-MMR accounting, reset before each instruction.
	bool m_peripheral_mmr_access = false;
	// External I/O owns its saved continuation until the bus transfer retires.
	bool m_atomic_io_pending = false;
	bool m_atomic_io_write = false;
	bool m_atomic_io_ready_required = false;
	u8 m_atomic_io_wait_remaining = 0;
	u16 m_atomic_io_address = 0;
	u16 m_atomic_io_port = 0;
	u16 m_atomic_io_value = 0;
	u8 m_atomic_io_mode = 0;
	u16 m_atomic_io_instruction_pc = 0;
	u16 m_atomic_io_previous_sp = 0;
	bool m_atomic_io_repeat_armed = false;
	bool m_atomic_io_delayed = false;
	bool m_atomic_io_xc_guarded = false;
	bool m_pipeline_frontend = false;
	// Prefetch, Fetch, Decode, Access, Read, Execute. Addresses and words are
	// owned by their slots; no speculative instruction executes at acceptance.
	std::array<u32, 6> m_pipeline_address = {};
	std::array<u32, 6> m_pipeline_physical_address = {};
	std::array<u16, 6> m_pipeline_word = {};
	std::array<bool, 6> m_pipeline_annulled = {};
	std::array<u16, 6> m_pipeline_operand_address = {};
	std::array<u16, 6> m_pipeline_operand = {};
	std::array<u16, 6> m_pipeline_immediate = {};
	// Original first extension for three-word diagnostic tuples; DAGEN may
	// subsequently replace operand_address with the resolved address.
	std::array<u16, 6> m_pipeline_extension_first = {};
	std::array<bool, 6> m_pipeline_valid = {};
	std::array<bool, 6> m_pipeline_block_first_final = {};
	std::array<bool, 6> m_pipeline_block_end = {};
	std::array<bool, 6> m_pipeline_block_body = {};
	bool m_pipeline_decode_block_body = false;
	bool m_pipeline_block_active = false;
	bool m_pipeline_block_fetch_enabled = false;
	bool m_pipeline_decode_block_first_final = false;
	u32 m_pipeline_block_start = 0;
	u32 m_pipeline_block_end_address = 0;
	u32 m_pipeline_block_fetch_remaining = 0;
	std::array<bool, 6> m_pipeline_early_write_deferred = {};
	std::array<bool, 6> m_pipeline_peripheral_wait_paid = {};
	std::array<bool, 6> m_pipeline_io_wait_paid = {};
	u8 m_pipeline_io_wait_remaining = 0;
	bool m_pipeline_io_ready_required = false;
	std::array<bool, 6> m_pipeline_intm_protected = {};
	std::array<u16, 6> m_pipeline_direct_status = {};
	std::array<u16, 6> m_pipeline_alu_mode = {};
	std::array<bool, 6> m_pipeline_cmpt = {};
	std::array<u16, 6> m_pipeline_bk = {};
	std::array<u16, 6> m_pipeline_direct_sp = {};
	std::array<u8, 6> m_pipeline_far_call_page = {};
	std::array<u16, 6> m_pipeline_far_call_stack = {};
	u16 m_pipeline_compiler_sp_previous = 0;
	u8 m_pipeline_compiler_sp_wait = 0;
	bool m_pipeline_idle_barrier = false;
	bool m_pipeline_idle_resume_pending = false;
	u32 m_pipeline_idle_resume_address = 0;
	bool m_pipeline_intm_follow = false;
	u8 m_pipeline_decode_remaining = 0;
	u16 m_pipeline_decode_extension = 0;
	u16 m_pipeline_decode_word = 0;
	u32 m_pipeline_decode_address = 0;
	u32 m_pipeline_fetch_address = 0;
	u16 m_pipeline_return_pc = 0;
	bool m_pipeline_flush_front = false;
	bool m_pipeline_vector_pending = false;
	u8 m_pipeline_decode_flush = 0;
	u8 m_pipeline_control_words = 0;
	u8 m_pipeline_delay_decode_words = 0;
	u32 m_pipeline_control_address = 0;
	u16 m_pipeline_control_word = 0;
	bool m_pipeline_bc_held = false;
	u16 m_pipeline_bc_word = 0;
	u32 m_pipeline_bc_address = 0;
	bool m_pipeline_bc_block_body = false;
	bool m_pipeline_bc_block_end = false;
	bool m_pipeline_bc_block_first_final = false;
	u8 m_pipeline_retd_stage = 0;
	std::array<u16, 2> m_pipeline_retd_words = {};
	std::array<u32, 2> m_pipeline_retd_addresses = {};
	u32 m_pipeline_retd_target = 0;
	bool m_pipeline_far_return_pending = false;
	bool m_pipeline_long_store_pending = false;
	bool m_pipeline_program_move_pending = false;
	bool m_pipeline_repeat_active = false;
	bool m_pipeline_repeat_body_latched = false;
	bool m_pipeline_repeat_stream = false;
	bool m_pipeline_repeat_store_pending = false;
	u16 m_pipeline_repeat_store_address = 0;
	u16 m_pipeline_repeat_issue_remaining = 0;
	u16 m_pipeline_repeat_word = 0;
	u16 m_pipeline_repeat_source = 0;
	u16 m_pipeline_repeat_status = 0;
	u16 m_pipeline_repeat_sp = 0;
	u32 m_pipeline_repeat_address = 0;
	bool m_pipeline_repeat_redirect_pending = false;
	u32 m_pipeline_repeat_redirect_target = 0;
	u8 m_pipeline_read_a_wait = 0;
	u8 m_pipeline_write_a_wait = 0;
	u32 m_pipeline_write_a_target = 0;
	u32 m_pipeline_write_a_logical = 0;
	u8 m_pipeline_mvdp_wait = 0;
	bool m_pipeline_acc_transfer_pending = false;
	u32 m_pipeline_acc_transfer_target = 0;
	bool m_pipeline_cc_pending = false;
	u32 m_pipeline_read_a_source = 0;
	u8 m_pipeline_memory_repeat_wait = 0;
	u32 m_pipeline_repeat_program_source = 0;
	u8 m_pipeline_dp_wait = 0;
	u64 m_pipeline_cycles = 0;
	std::array<u16, 0x10000> m_opcode_first_pc;
	std::array<u32, 0x10000> m_opcode_count = {};
	// Verbose-only observations, not architectural state or prefetched operands.
	struct extension_observation { u32 first_pc; u32 count = 0; };
	std::map<std::array<u16, 4>, extension_observation> m_extension_observations;
	std::array<u16, 3> m_instruction_words = {};
	u16 m_instruction_word_count = 0;
	bool m_capture_extensions = false;
	int m_icount = 0;
};

DECLARE_DEVICE_TYPE(TMS320C54X, tms320c54x_device)

#endif // MAME_CPU_TMS320C54X_TMS320C54X_H
