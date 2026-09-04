// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x.h"

namespace {

class tms320c54x_disassembler : public util::disasm_interface
{
public:
	virtual u32 opcode_alignment() const override { return 1; }

	virtual offs_t disassemble(std::ostream &stream, offs_t pc,
			const data_buffer &opcodes, const data_buffer &params) override
	{
		util::stream_format(stream, ".word   $%04X", opcodes.r16(pc));
		return 1 | SUPPORTED;
	}
};

} // anonymous namespace

DEFINE_DEVICE_TYPE(TMS320C54X, tms320c54x_device, "tms320c54x",
		"Texas Instruments TMS320C54x")

tms320c54x_device::tms320c54x_device(const machine_config &mconfig,
		const char *tag, device_t *owner, u32 clock) :
	cpu_device(mconfig, TMS320C54X, tag, owner, clock),
	m_program_config("program", ENDIANNESS_LITTLE, 16, 16, -1),
	m_data_config("data", ENDIANNESS_LITTLE, 16, 16, -1),
	m_io_config("io", ENDIANNESS_LITTLE, 16, 16, -1)
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
	return std::make_unique<tms320c54x_disassembler>();
}

void tms320c54x_device::device_start()
{
	space(AS_PROGRAM).cache(m_cache);
	space(AS_PROGRAM).specific(m_program);
	space(AS_DATA).specific(m_data);
	space(AS_IO).specific(m_io);

	set_icountptr(m_icount);

	state_add(STATE_PC, "PC", m_pc).formatstr("%04X");
	state_add(STATE_A, "A", m_a).mask(ACC_MASK).formatstr("%010X");
	state_add(STATE_B, "B", m_b).mask(ACC_MASK).formatstr("%010X");
	state_add(STATE_T, "T", m_t).formatstr("%04X");
	state_add(STATE_SP, "SP", m_sp).formatstr("%04X");
	state_add(STATE_ST0, "ST0", m_st0).formatstr("%04X");
	state_add(STATE_ST1, "ST1", m_st1).formatstr("%04X");
	state_add(STATE_PMST, "PMST", m_pmst).formatstr("%04X");
	for (unsigned i = 0; i != std::size(m_ar); ++i)
		state_add(STATE_AR0 + i, string_format("AR%u", i).c_str(), m_ar[i]).formatstr("%04X");
	state_add(STATE_GENPC, "GENPC", m_pc).noshow();
	state_add(STATE_GENPCBASE, "CURPC", m_pc).noshow();

	save_item(NAME(m_pc));
	save_item(NAME(m_op));
	save_item(NAME(m_a));
	save_item(NAME(m_b));
	save_item(NAME(m_t));
	save_item(NAME(m_sp));
	save_item(NAME(m_st0));
	save_item(NAME(m_st1));
	save_item(NAME(m_pmst));
	save_item(NAME(m_ar));
	save_item(NAME(m_brc));
	save_item(NAME(m_rsa));
	save_item(NAME(m_rea));
	save_item(NAME(m_ifr));
	save_item(NAME(m_imr));
	save_item(NAME(m_block_repeat_active));
	save_item(NAME(m_illegal));
}

void tms320c54x_device::device_reset()
{
	m_pc = 0xff80;
	m_op = 0;
	m_a = 0;
	m_b = 0;
	m_t = 0;
	m_sp = 0;
	m_st0 = 0;
	m_st1 = 0x2900;
	m_pmst = 0xffa0;
	std::fill(std::begin(m_ar), std::end(m_ar), 0);
	m_brc = 0;
	m_rsa = 0;
	m_rea = 0;
	m_ifr = 0;
	m_imr = 0;
	m_block_repeat_active = false;
	m_illegal = false;
}

void tms320c54x_device::execute_run()
{
	while (m_icount > 0)
	{
		debugger_instruction_hook(m_pc);
		if (m_illegal)
		{
			m_icount = 0;
			break;
		}

		m_op = m_cache.read_word(m_pc++);
		logerror("%s: unimplemented C54x opcode %04x at %04x\n",
				machine().describe_context(), m_op, u16(m_pc - 1));
		m_illegal = true;
		--m_icount;
	}
}

void tms320c54x_device::execute_set_input(int inputnum, int state)
{
	if (inputnum >= 0 && inputnum < 16)
	{
		if (state == ASSERT_LINE)
			m_ifr |= u16(1U << inputnum);
		else if (state == CLEAR_LINE)
			m_ifr &= u16(~(1U << inputnum));
	}
}
