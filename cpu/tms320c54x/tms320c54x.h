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

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u32 execute_min_cycles() const noexcept override { return 1; }
	virtual u32 execute_max_cycles() const noexcept override { return 1; }
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	virtual space_config_vector memory_space_config() const override;
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;

private:
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
		STATE_AR7
	};

	static constexpr u64 ACC_MASK = (u64(1) << 40) - 1;

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
	u16 m_rsa = 0;
	u16 m_rea = 0;
	u16 m_ifr = 0;
	u16 m_imr = 0;
	bool m_block_repeat_active = false;
	bool m_illegal = false;
	int m_icount = 0;
};

DECLARE_DEVICE_TYPE(TMS320C54X, tms320c54x_device)

#endif // MAME_CPU_TMS320C54X_TMS320C54X_H
