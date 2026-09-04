// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_CPU_TMS320C54X_TMS320C54X_H
#define MAME_CPU_TMS320C54X_TMS320C54X_H

#pragma once

class tms320c54x_device : public cpu_device
{
public:
	tms320c54x_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock);

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
		STATE_ILLEGAL
	};

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u32 execute_min_cycles() const noexcept override { return 1; }
	virtual u32 execute_max_cycles() const noexcept override { return 5; }
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	virtual space_config_vector memory_space_config() const override;
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;

private:
	static constexpr u64 ACC_MASK = (u64(1) << 40) - 1;

	u16 fetch();
	u16 data_read(u16 address);
	void data_write(u16 address, u16 value);
	u16 indirect_read(u8 mode);
	void indirect_write(u8 mode, u16 value);
	void indirect_modify(u8 mode);
	void circular_modify(unsigned ar, s16 step);
	void execute_one(u16 op);
	bool service_interrupt();
	void finish_repeats();
	void push(u16 value);
	u16 pop();
	u64 data_operand(u16 value) const;
	u64 arithmetic_shift_right(u64 value, unsigned shift) const;
	u64 &accumulator(bool b) { return b ? m_b : m_a; }

	address_space_config m_program_config;
	address_space_config m_data_config;
	address_space_config m_io_config;

	memory_access<16, 1, -1, ENDIANNESS_LITTLE>::cache m_cache;
	memory_access<16, 1, -1, ENDIANNESS_LITTLE>::specific m_program;
	memory_access<16, 1, -1, ENDIANNESS_LITTLE>::specific m_data;
	memory_access<16, 1, -1, ENDIANNESS_LITTLE>::specific m_io;

	u16 m_pc = 0;
	u16 m_op = 0;
	u64 m_a = 0;
	u64 m_b = 0;
	u16 m_t = 0;
	u16 m_sp = 0;
	u16 m_st0 = 0;
	u16 m_st1 = 0;
	u16 m_pmst = 0;
	u16 m_ar[8] = {};
	u16 m_brc = 0;
	u16 m_bk = 0;
	u16 m_rsa = 0;
	u16 m_rea = 0;
	u16 m_rptc = 0;
	u16 m_rpt_address = 0;
	u16 m_rpt_end = 0xffff;
	bool m_rpt_armed = false;
	u16 m_delayed_target = 0;
	u8 m_delayed_words = 0;
	u16 m_ifr = 0;
	u16 m_imr = 0;
	bool m_block_repeat_active = false;
	bool m_idle = false;
	bool m_illegal = false;
	int m_icount = 0;
};

DECLARE_DEVICE_TYPE(TMS320C54X, tms320c54x_device)

#endif // MAME_CPU_TMS320C54X_TMS320C54X_H
