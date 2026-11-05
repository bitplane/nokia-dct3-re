// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_CPU_TMS320C54X_DASM_H
#define MAME_CPU_TMS320C54X_DASM_H

#include "util/disasmintf.h"

class tms320c54x_disassembler : public util::disasm_interface
{
public:
	explicit tms320c54x_disassembler(bool extended) : m_extended(extended) { }
	virtual u32 opcode_alignment() const override { return 1; }
	virtual u32 interface_flags() const override { return PAGED; }
	virtual u32 page_address_bits() const override { return 16; }
	virtual offs_t disassemble(std::ostream &stream, offs_t pc,
		const data_buffer &opcodes, const data_buffer &params) override;
private:
	bool const m_extended;
};

#endif
