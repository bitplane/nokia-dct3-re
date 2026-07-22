// Firmware-address-specific research diagnostics for nokia_3310_state.
//
// This file is included by nokia_3310.cpp after the state class definition.
// It may observe firmware execution and RAM writes, but must not alter fetched
// instructions, synthesize messages, or force firmware-owned state.

// ============================================================================
// Firmware-research RAM-write observations. The real backing-store write stays
// in noki3310_state::ram_w so removing this file cannot change emulation.
// ============================================================================
void noki3310_state::ram_w_firmware_traces(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	const offs_t address = 0x100000 + (offset << 1);
	const u32 pc = m_maincpu->pc();
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010fe7a)
		logerror("radio_location_update_latch_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010fe4a)
		logerror("radio_location_update_source_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && address == FW_SERVICE_SESSION_STATUS)
		logerror("csstatus_write: pc=%08x data=%04x mask=%04x old=%04x task=%02x "
				"svc_ready=%02x dsp=%04x/%04x/%04x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], fw_byte(FW_SCHED_RUNNING_TASK_ID),
				fw_byte(FW_STARTUP_SERVICE_READY), m_dspif->shared_r(0x0da >> 1),
				m_dspif->shared_r(0x0e2 >> 1), m_dspif->shared_r(0x0e4 >> 1),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 &&
			(address == 0x0011fedc || address == 0x0011fede))
		logerror("csaddress_write: pc=%08x address=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), u32(address), data, mem_mask,
				m_ram[offset], fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && address == 0x0010dcb6)
		logerror("sim_contract: notify-latch write pc=%08x data=%04x mask=%04x old=%04x task=%02x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], fw_byte(0x00100022),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010dbd6)
		logerror("radio_controller_state_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010dc98)
		logerror("radio_identity_scalar_write: pc=%08x data=%04x mask=%04x old=%04x "
				"value=%02x task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask,
				m_ram[offset], (mem_mask & 0xff00) ? (data >> 8) & 0xff : fw_byte(0x0010dc99),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010dc94)
		logerror("radio_identity_owner_state_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(address == 0x0010d94c || address == 0x0010d94e))
		logerror("radio_identity_tuple_write: pc=%08x address=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), u32(address), data, mem_mask,
				m_ram[offset], fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			address == 0x0010c5dc)
	{
		if ((pc & ~u32(1)) != 0x00213490)
		{
			logerror("radio_record_class_write: pc=%08x data=%04x mask=%04x old=%04x "
					"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
					fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
		}
		else
		{
		const u32 source = m_maincpu->state_int(arm7_cpu_device::ARM7_R3);
		const u32 cursor = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		const u32 selected = m_maincpu->state_int(arm7_cpu_device::ARM7_R9);
		logerror("radio_record_class_write: pc=%08x data=%04x mask=%04x old=%04x "
				"source=%08x/%04x/%04x cursor=%08x/%02x selected=%08x/%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				source, fw_word(source), fw_word(source + 6), cursor, fw_byte(cursor),
				selected, fw_word(selected + 6), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010edc4)
		logerror("radio_si_publish_latch_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010dbda)
		logerror("radio_completion_selector_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010dbd0)
		logerror("radio_channel_identity_write: pc=%08x data=%04x mask=%04x old=%04x "
				"bytes=%02x/%02x task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask,
				m_ram[offset], fw_byte(0x0010dbd0), fw_byte(0x0010dbd1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 &&
			(address == 0x0010fd00 || address == 0x0010fd02 ||
			 address == 0x0010fd14 || address == 0x0010fd16))
		logerror("registration_plmn_write: pc=%08x address=%08x data=%04x mask=%04x old=%04x "
				"desired=%02x%02x%02x source=%02x%02x%02x task=%02x t=%.6f\n",
				pc & ~u32(1), u32(address), data, mem_mask, m_ram[offset],
				fw_byte(0x0010fd00), fw_byte(0x0010fd01), fw_byte(0x0010fd02),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010fda0)
		logerror("registration_plmn_list_write: pc=%08x data=%04x mask=%04x old=%04x "
				"bytes=%02x%02x%02x%02x task=%02x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(0x0010fda0), fw_byte(0x0010fda1), fw_byte(0x0010fda2), fw_byte(0x0010fda3),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010fe80)
		logerror("registration_cell_result_write: pc=%08x data=%04x mask=%04x old=%04x "
				"result=%02x task=%02x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], fw_byte(0x0010fe81),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010fe6c)
		logerror("radio_task15_aux_state_write: pc=%08x data=%04x mask=%04x old=%04x "
				"value=%02x task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask,
				m_ram[offset], fw_byte(0x0010fe6c), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010fe64)
	{
		const u32 source = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		logerror("radio_task15_availability_write: pc=%08x data=%04x mask=%04x old=%04x "
				"source=%08x status=%04x bytes=%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], source, fw_word(source),
				fw_byte(source + 2), fw_byte(source + 3), fw_byte(source + 4), fw_byte(source + 5),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010fcc0)
		logerror("registration_selection_mode_write: pc=%08x data=%04x mask=%04x old=%04x "
				"mode=%02x task=%02x t=%.6f\n",
				pc & ~u32(1), data, mem_mask, m_ram[offset], fw_byte(0x0010fcc0),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010fcba)
		logerror("radio_source_update_window_write: pc=%08x data=%04x mask=%04x old=%04x "
				"window=%02x task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask,
				m_ram[offset], fw_byte(0x0010fcba), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && address == 0x0010fcca)
		logerror("radio_result_list_count_write: pc=%08x data=%04x mask=%04x old=%04x "
				"count=%04x task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask,
				m_ram[offset], fw_word(0x0010fcca), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010fd10)
		logerror("registration_context_state_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x00112354)
		logerror("registration_phase_owner_write: pc=%08x data=%04x mask=%04x old=%04x "
				"task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && address == 0x0010c274)
		logerror("radio_task11_mode_write: pc=%08x data=%04x mask=%04x old=%04x "
				"mode=%02x task=%02x t=%.6f\n", pc & ~u32(1), data, mem_mask, m_ram[offset],
				fw_byte(0x0010c275), fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
}
// ============================================================================
// Firmware-research traces for flash fetches. This is diagnostic observation,
// not hardware behaviour, and never changes the fetched instruction.
// ============================================================================
void noki3310_state::flash_firmware_traces(u32 pc, u32 addr)
{
	if (pc == addr && addr == 0x00221d74 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_task17_selection_send: selector=%08x arg=%08x caller=%08x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00208ee0 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_mm_outbound_wrapper: event=%04x selector=%02x caller=%08x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr &&
			(addr == 0x00250b5a || addr == 0x00251552 || addr == 0x002515f0) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		logerror("radio_rr_location_update_trigger: pc=%08x object=%08x status=%04x "
				"bytes=%02x/%02x/%02x/%02x r4=%08x r7=%08x caller=%08x "
				"task=%02x t=%.6f\n", addr, object,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_word(object) : 0xffff,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_byte(object + 2) : 0xff,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_byte(object + 3) : 0xff,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_byte(object + 4) : 0xff,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_byte(object + 5) : 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R7),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && (addr == 0x0020e9a8 || addr == 0x0020e9ac) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
		const u32 state = m_maincpu->state_int(arm7_cpu_device::ARM7_R7);
		const u32 location = fw_dword(sp + 0x1c);
		const u32 candidate = fw_dword(sp + 0x20);
		logerror("radio_location_update_gate: pc=%08x helper=%08x state=%08x "
				"bytes=%02x/%02x/%02x/%02x location=%08x/%02x candidate=%08x/%02x "
				"task=%02x t=%.6f\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0), state,
				fw_byte(state), fw_byte(state + 2), fw_byte(state + 0x11), fw_byte(state + 0x13),
				location, location >= NOKIA_RAM_BASE && location < NOKIA_RAM_END ? fw_byte(location) : 0xff,
				candidate, candidate >= NOKIA_RAM_BASE && candidate < NOKIA_RAM_END ? fw_byte(candidate) : 0xff,
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x002ab6dc && nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0)
		logerror("radio_selection_controller_handler: arg=%08x state=%02x/%02x/%02x/%02x/%04x/%04x "
				"flag=%02x caller=%08x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0), fw_byte(0x00112355),
				fw_byte(0x00112356), fw_byte(0x00112358), fw_byte(0x00112359),
				fw_word(0x0011235e), fw_word(0x00112360), fw_byte(0x00112358),
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002ab750 && nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0)
		logerror("radio_selection_completion_handler: arg=%08x flag=%02x caller=%08x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0), fw_byte(0x00112358),
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(addr == 0x00218770 || addr == 0x0021bbee || addr == 0x0021bc38 ||
			 addr == 0x0021bea8 || addr == 0x00215e44 || addr == 0x002148c0 ||
			 addr == 0x00214b26 || addr == 0x0028cc8c || addr == 0x002192a0 ||
			 addr == 0x00218930))
		logerror("radio_plmn_measurement: pc=%08x r0=%08x r1=%08x task11=%02x/%02x/%02x "
				"controller=%02x/%02x/%02x task=%02x t=%.6f\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				fw_byte(0x0010c274), fw_byte(0x0010c275), fw_byte(0x0010c27c),
				fw_byte(0x0010dbd7), fw_byte(0x0010dbd2), fw_byte(0x0010dbd4),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(addr == 0x00215bcc || addr == 0x00215bf6 || addr == 0x00215bfe ||
			 addr == 0x00215c20 || addr == 0x00215c3e))
	{
		const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
		const u32 object = fw_dword(sp + 0x14);
		logerror("radio_rssi_reconcile: pc=%08x object=%08x status=%04x bytes="
				"%02x/%02x/%02x/%02x task11=%02x/%02x/%02x/%02x policy=%02x/%02x "
				"owner=%02x/%02x/%02x search=%02x/%02x/%02x "
				"r4=%08x r6=%08x r8=%08x task=%02x t=%.6f\n",
				addr, object,
				object >= NOKIA_RAM_BASE && object + 7 < NOKIA_RAM_END ? fw_word(object) : 0xffff,
				object >= NOKIA_RAM_BASE && object + 7 < NOKIA_RAM_END ? fw_byte(object + 2) : 0xff,
				object >= NOKIA_RAM_BASE && object + 7 < NOKIA_RAM_END ? fw_byte(object + 3) : 0xff,
				object >= NOKIA_RAM_BASE && object + 7 < NOKIA_RAM_END ? fw_byte(object + 4) : 0xff,
				object >= NOKIA_RAM_BASE && object + 7 < NOKIA_RAM_END ? fw_byte(object + 5) : 0xff,
				fw_byte(0x0010c274), fw_byte(0x0010c275), fw_byte(0x0010c27c),
				fw_byte(0x0010c281), fw_byte(0x0010c84c), fw_byte(0x0010c83a),
				fw_byte(0x0010dbd7), fw_byte(0x0010dbdd), fw_byte(0x0010dbd2),
				fw_byte(0x0010c851), fw_byte(0x0010c838 + 0x19), fw_byte(0x0010c274 + 8),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R8),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && addr >= 0x002d7570 && addr < 0x002d7578)
		logerror("radio_nmeas_catalog_read: pc=%08x addr=%08x task=%02x t=%.6f\n",
				pc & ~u32(1), addr, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002b4628 &&
			nokia_env_u32("NOKI3210_TRACE_DISPLAY", 0) != 0)
		logerror("keypad_decoded: code=%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	// Serving-cell acceptance checkpoints retained by verify-radio-camp.
	if (pc == addr && addr == 0x00284f74 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_channel_change_state: pc=%08x accept=%02x controller=%02x flags=%08x "
				"r0=%08x caller=%08x task=%02x t=%.6f\\n", addr,
				fw_byte(0x0010dbd7), fw_byte(0x0010dc93), fw_dword(0x0010dca0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00284e50 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		logerror("radio_no_psw_left_decode: object=%08x accept=%02x lifecycle=%02x "
				"phase=%02x task=%02x t=%.6f\\n", object, fw_byte(0x0010dbd7),
				fw_byte(0x0010c84c), fw_byte(0x0010dbdd),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00213c04 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		// The dispatcher retains its received object in r8; r0 is scratch here.
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R8);
		logerror("radio_acquisition_transition: pc=%08x object=%08x status=%04x action=%02x "
				"arg=%02x sequence=%02x lifecycle=%02x controller=%02x task=%02x t=%.6f\\n",
				addr, object, fw_word(object), fw_byte(object + 2), fw_byte(object + 3),
				fw_byte(0x00111e29), fw_byte(0x0010c84c), fw_byte(0x0010dbd7),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x0021b468 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		logerror("radio_task10_receive: object=%08x status=%04x bytes=%02x/%02x/%02x/%02x "
				"task=%02x t=%.6f\n", object, fw_word(object), fw_byte(object + 2),
				fw_byte(object + 3), fw_byte(object + 4), fw_byte(object + 5),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00213d32 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 records = 0x0010c5ac;
		logerror("radio_task11_selected_list: count=%02x first=%04x/%04x/%04x/%04x "
				"marker=%02x state=%02x/%02x/%02x task=%02x t=%.6f\n",
				fw_byte(records), fw_word(records + 4 + 0x10),
				fw_word(records + 4 + 0x12), fw_word(records + 4 + 0x28),
				fw_word(records + 4 + 0x2c), fw_byte(records + 4 + 0x2c),
				fw_byte(0x0010c274 + 0x18), fw_byte(0x0010c838 + 2),
				fw_byte(0x0010c838 + 0x14), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x00210ce0 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 record = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		logerror("radio_si_record_classify: record=%08x marker=%02x eligibility=%02x "
				"arfcn=%04x task=%02x t=%.6f\n", record, fw_byte(record + 0x2c),
				fw_byte(record + 0x1b), fw_word(record + 0x28),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00212b20 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 order = 0x0010c36c;
		const u32 records = 0x0010c5ac;
		logerror("radio_task11_select_begin: order_cursor=%04x order_count=%02x "
				"order_first=%04x records=%02x first_marker=%02x first_arfcn=%04x "
				"active=%04x task=%02x t=%.6f\n",
				fw_word(order), fw_byte(order + 2), fw_word(order + 4),
				fw_byte(records), fw_byte(records + 4 + 0x2c),
				fw_word(records + 4 + 0x28), fw_word(0x0010dbce),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00212d54 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 order = 0x0010c36c;
		const u32 records = 0x0010c5ac;
		logerror("radio_task11_select_none: order_cursor=%04x order_count=%02x "
				"records=%02x first_marker=%02x first_arfcn=%04x active=%04x "
				"r4=%08x r5=%08x r6=%08x r7=%08x task=%02x t=%.6f\n",
				fw_word(order), fw_byte(order + 2), fw_byte(records),
				fw_byte(records + 4 + 0x2c), fw_word(records + 4 + 0x28),
				fw_word(0x0010dbce), m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R5),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R7),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00213710 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 selected = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 record = 0x0010c5b0 + selected * 0x48;
		logerror("radio_task11_result_build: selected=%02x marker=%02x arfcn=%04x "
				"active=%04x task=%02x t=%.6f\n", selected,
				selected == 0xff ? 0xff : fw_byte(record + 0x2c),
				selected == 0xff ? 0xffff : fw_word(record + 0x28),
				fw_word(0x0010dbce), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x002229e8 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_selected_plmn_commit: caller=%08x r0=%08x r1=%08x "
				"visible=%02x/%02x/%02x desired=%02x/%02x/%02x candidate=%02x/%02x/%02x "
				"source=%02x/%02x/%02x flags=%02x/%02x/%02x/%02x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				fw_byte(0x0010d94c), fw_byte(0x0010d94d), fw_byte(0x0010d94e),
				fw_byte(0x0010fd00), fw_byte(0x0010fd01), fw_byte(0x0010fd02),
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(0x0010fccf), fw_byte(0x0010fcd9), fw_byte(0x0010fd04),
				fw_byte(0x0010fd05),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00221fac &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_plmn_tuple_compare: source=%02x/%02x/%02x "
				"candidate=%02x/%02x/%02x task=%02x t=%.6f\n",
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x0022236c &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_source_candidate_gate: phase=entry state=%02x mode=%02x "
				"candidate=%02x/%02x/%02x source=%02x/%02x/%02x policy=%02x/%02x/%02x/%02x "
				"task=%02x t=%.6f\n", fw_byte(0x0010fcc5), fw_byte(0x0010fcc0),
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(0x0010fca8), fw_byte(0x0010fcb9), fw_byte(0x0010fcbd),
				fw_byte(0x0010fd04), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (pc == addr && addr == 0x002222cc &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1)) == 0x002223b0 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_source_candidate_gate: phase=accepted candidate=%02x/%02x/%02x "
				"source=%02x/%02x/%02x task=%02x t=%.6f\n",
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002239b8 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_equal_plmn_continuation: operation=%02x request=%02x "
				"continuation=%02x source=%02x/%02x/%02x candidate=%02x/%02x/%02x "
				"task=%02x t=%.6f\n",
				fw_byte(0x0010fd10), fw_byte(0x0010fd11), fw_byte(0x0010fcc5),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00223530 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_selected_network_admissibility: phase=entry "
				"desired=%02x/%02x/%02x candidate=%02x/%02x/%02x source=%02x/%02x/%02x "
				"operation=%02x request=%02x task=%02x t=%.6f\n",
				fw_byte(0x0010fd00), fw_byte(0x0010fd01), fw_byte(0x0010fd02),
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(0x0010fd10), fw_byte(0x0010fd11),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00225662 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_selected_network_admissibility: phase=result accepted=%02x "
				"policy=%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
				fw_byte(0x0010fca8), fw_byte(0x0010fcb9), fw_byte(0x0010fcc5),
				fw_byte(0x0010fcca), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (pc == addr &&
			(addr == 0x0022354c || addr == 0x00223558 || addr == 0x0022356c ||
			 addr == 0x00223576 || addr == 0x00223580 || addr == 0x002235a2 ||
			 addr == 0x002235b2 || addr == 0x002235c8 || addr == 0x002235d2 ||
			 addr == 0x002235e6 || addr == 0x002235f0 || addr == 0x002235fa ||
			 addr == 0x00223604 || addr == 0x0022360e || addr == 0x0022361e ||
			 addr == 0x00223628 || addr == 0x0022362e) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_selected_network_admissibility: phase=predicate site=%08x "
				"r0=%08x r1=%08x desired=%02x/%02x/%02x source=%02x/%02x/%02x "
				"candidate=%02x/%02x/%02x task=%02x t=%.6f\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				fw_byte(0x0010fd00), fw_byte(0x0010fd01), fw_byte(0x0010fd02),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002255c4 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_periodic_policy_result: timer=%04x mode=%02x/%02x/%02x/%02x "
				"desired=%02x/%02x/%02x source=%02x/%02x/%02x task=%02x t=%.6f\n",
				fw_word(0x0010fcca), fw_byte(0x0010fca8), fw_byte(0x0010fcb9),
				fw_byte(0x0010fcc5), fw_byte(0x0010fcca), fw_byte(0x0010fd00),
				fw_byte(0x0010fd01), fw_byte(0x0010fd02), fw_byte(0x0010fd14),
				fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00279466 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_sim_registration_notice: selector=%02x caller=%08x "
				"sim_state=%02x/%02x/%02x/%02x task17=%02x/%02x/%02x/%02x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x00111c64), fw_byte(0x00111c79), fw_byte(0x00111c96),
				fw_byte(0x00111c97), fw_byte(0x0010fccf), fw_byte(0x0010fcd9),
				fw_byte(0x0010fd04), fw_byte(0x0010fd05),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && (addr == 0x00225eea || addr == 0x00226008) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		logerror("radio_task17_sim_notice: site=%08x object=%08x bytes=%02x/%02x/%02x/%02x "
				"ctx=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x "
				"desired=%02x/%02x/%02x source=%02x/%02x/%02x task=%02x t=%.6f\n",
				addr, object, fw_byte(object), fw_byte(object + 1), fw_byte(object + 2),
				fw_byte(object + 3), fw_byte(0x0010fca8), fw_byte(0x0010fcb3),
				fw_byte(0x0010fcc0), fw_byte(0x0010fcc4), fw_byte(0x0010fcc5),
				fw_byte(0x0010fcc6), fw_byte(0x0010fcbe), fw_byte(0x0010fcc7),
				fw_byte(0x0010fcc8), fw_byte(0x0010fccf), fw_byte(0x0010fd00),
				fw_byte(0x0010fd01), fw_byte(0x0010fd02), fw_byte(0x0010fd14),
				fw_byte(0x0010fd15), fw_byte(0x0010fd16), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x0028d404 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		logerror("radio_candidate_table: object=%08x status=%04x usable=%02x "
				"arfcn=%04x rssi_id=%02x acquisition_mode=%02x task=%02x t=%.6f\\n", object,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_word(object) : 0xffff,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_byte(object + 2) : 0xff,
				object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END ? fw_word(object + 4) : 0xffff,
				fw_byte(0x0010c84b), fw_byte(0x0010c83a),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00211f60 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 records = fw_dword(0x0010c2b8);
		const u32 samples = fw_dword(0x0010c2c0);
		logerror("radio_measurement_table_scan: records=%08x first=%04x/%02x/%02x/%02x/%02x "
				"samples=%08x first_sample=%04x/%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				records, fw_word(records), fw_byte(records + 2), fw_byte(records + 3),
				fw_byte(records + 4), fw_byte(records + 5), samples, fw_word(samples),
				fw_byte(samples + 2), fw_byte(samples + 3), fw_byte(samples + 4),
				fw_byte(samples + 5), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	// Complete SI status 0x1396 shares this acceptance arm with 0x1397.  These
	// are the four firmware-owned identity fields compared before state 4 and
	// the home-PLMN publication can be committed.
	if (pc == addr && addr == 0x0021b790 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R8);
		logerror("radio_identity_acceptance: status=%04x selector=%02x controller=%02x "
				"sim=%02x/%02x/%02x/%02x candidate=%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				fw_word(object), fw_byte(object + 2), fw_byte(0x0010dc94),
				(fw_byte(0x0010ef34) >> 3) & 0x07, fw_byte(0x0010ef34) & 0x01,
				(fw_byte(0x0010ef35) & 0x07) + 2, ((fw_byte(0x0010ef34) >> 1) & 0x03) + 1,
				fw_byte(0x0010d94c), fw_byte(0x0010d94d), fw_byte(0x0010d94e),
				fw_byte(0x0010d94f), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x00273fde &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_identity_status1396_post: channel=%02x bitmap=%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R8) & 0xff,
				fw_byte(0x0010edb6), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (pc == addr && (addr == 0x00273fb0 || addr == 0x00273fc8) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 accumulator = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		const u32 channel_context = m_maincpu->state_int(arm7_cpu_device::ARM7_R10);
		logerror("radio_identity_status1396_gate: pc=%08x channel=%02x fp=%04x context=%08x/%04x "
				"selector=%08x r0=%08x r1=%08x flags=%02x posted=%02x "
				"sim=%02x/%02x/%02x/%02x candidate=%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				addr, m_maincpu->state_int(arm7_cpu_device::ARM7_R8) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R11) & 0xffff,
				channel_context, fw_word(channel_context + 2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R9),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				fw_byte(accumulator + 2), fw_byte(accumulator + 0x11),
				(fw_byte(0x0010ef34) >> 3) & 0x07, fw_byte(0x0010ef34) & 0x01,
				(fw_byte(0x0010ef35) & 0x07) + 2, ((fw_byte(0x0010ef34) >> 1) & 0x03) + 1,
				fw_byte(0x0010d94c), fw_byte(0x0010d94d), fw_byte(0x0010d94e),
				fw_byte(0x0010d94f),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00273aac &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 block = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		logerror("radio_bcch_parse: channel=%02x block=%08x data=",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff, block);
		for (unsigned index = 0; block >= NOKIA_RAM_BASE && block + index < NOKIA_RAM_END && index < 24; ++index)
			logerror("%02x", fw_byte(block + index));
		logerror(" task=%02x t=%.6f\\n", fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x00273e58 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_si3_path: pc=%08x result=%08x update1=%08x update2=%08x state=%02x active=%02x flags=%02x/%02x "
				"task=%02x t=%.6f\\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R5), fw_byte(0x0010dbd7),
				fw_byte(0x0010edb4), fw_byte(0x0010edb6), fw_byte(0x0010edc1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x0021c36a &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
		const u32 object = fw_dword(sp + 0x1c);
		const u32 state = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		logerror("radio_si_update_dispatch: object=%08x state_object=%08x state=%02x "
				"controller=%02x accept=%02x flags=%02x/%02x lr=%08x task=%02x t=%.6f\n",
				object, state, fw_byte(state),
				fw_byte(0x0010dbd7), fw_byte(0x0010dc93), fw_byte(0x0010edb6),
				fw_byte(0x0010edc1), m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x00219e30 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 result = fw_dword(0x0010d93c);
		logerror("radio_idle_completion: selector=%02x controller=%02x flags=%02x/%02x "
				"result=%08x data=", fw_byte(0x0010dbdb), fw_byte(0x0010dbd7),
				fw_byte(0x0010edb6), fw_byte(0x0010edc1), result);
		for (unsigned index = 0; result >= NOKIA_RAM_BASE && result + index < NOKIA_RAM_END && index < 24; ++index)
			logerror("%02x", fw_byte(result + index));
		logerror(" task=%02x t=%.6f\n", fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x0021adac &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_home_plmn_publish: caller=%08x sim=%02x/%02x/%02x/%02x "
				"state=%02x controller=%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x0010ef34), fw_byte(0x0010ef35), fw_byte(0x0010ef36),
				fw_byte(0x0010ef37), fw_byte(0x0010d944), fw_byte(0x0010dbd7),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x0021b468 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		logerror("radio_task10_status_entry: object=%08x status=%04x operation=%02x "
				"controller_state=%02x radio_state=%02x task=%02x t=%.6f\n",
				object, fw_word(object), fw_byte(object + 2), fw_byte(0x0010dc94),
				fw_byte(0x0010dc93), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x0021b790 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R8);
		logerror("radio_task10_identity_result: object=%08x dispatch_status=%04x status=%04x operation=%02x "
				"controller_state=%02x radio_state=%02x task=%02x t=%.6f\n",
				object, m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xffff,
				fw_word(object), fw_byte(object + 2), fw_byte(0x0010dc94),
				fw_byte(0x0010dc93), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x0021b7a4 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_task10_identity_classify: operation=%02x result=%08x controller_state=%02x "
				"radio_state=%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0), fw_byte(0x0010dc94),
				fw_byte(0x0010dc93), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (pc == addr && addr == 0x0021b828 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_task10_identity_predicate: context=%02x sim=%02x/%02x "
				"cached=%02x/%02x/%02x/%02x output=%02x task=%02x t=%.6f\n",
				fw_byte(0x0010dc94), fw_byte(0x0010ef34), fw_byte(0x0010ef35),
				fw_byte(0x0010d94c), fw_byte(0x0010d94d), fw_byte(0x0010d94e),
				fw_byte(0x0010d94f), fw_byte(0x0010d960),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x0021bb5c &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_task10_controller_dispatch: controller_state=%02x radio_state=%02x "
				"flags=%02x/%02x task=%02x t=%.6f\n", fw_byte(0x0010dc93),
				fw_byte(0x0010dc94), fw_byte(0x0010dca3), fw_byte(0x0010dca4),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00222174 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_selection_terminal_predicate: caller=%08x continuation=%02x operation=%02x "
				"request=%02x previous=%02x source=%02x%02x%02x candidate=%02x%02x%02x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x0010fcc5), fw_byte(0x0010fd10), fw_byte(0x0010fd11),
				fw_byte(0x0010fd16), fw_byte(0x0010fd14), fw_byte(0x0010fd15),
				fw_byte(0x0010fd16), fw_byte(0x0010fcf8), fw_byte(0x0010fcf9),
				fw_byte(0x0010fcfa), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (pc == addr && addr == 0x00208ee0 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 status = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if ((status >= 0x09ec && status <= 0x09f3) || status == 0x09ff)
			logerror("radio_selection_status_construct: status=%04x arg=%08x caller_r6=%08x caller=%08x "
					"task=%02x t=%.6f\n", status,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x0020eda8 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_task15_receive_reentry: session=%08x caller=%08x "
				"state=%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x0010fe48), fw_byte(0x0010fe49), fw_byte(0x0010fe4a),
				fw_byte(0x0010fe4f), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (pc == addr && (addr == 0x0020a026 || addr == 0x0020a02c) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		const u32 payload = object >= NOKIA_RAM_BASE && object + 0x0c < NOKIA_RAM_END ?
				fw_dword(object + 8) : 0;
		logerror("radio_mm_parse: phase=%s object=%08x payload=%08x result=%08x "
				"mm=%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				addr == 0x0020a026 ? "entry" : "return", object, payload,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				fw_byte(0x0010fe48), fw_byte(0x0010fe49), fw_byte(0x0010fe4a),
				fw_byte(0x0010fe4f), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && (addr == 0x0020be0c || addr == 0x0020f324) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_mm_accept_completion: site=%08x mm=%02x/%02x/%02x/%02x "
				"context=%02x/%02x/%02x/%02x task=%02x t=%.6f\n", addr,
				fw_byte(0x0010fe48), fw_byte(0x0010fe49), fw_byte(0x0010fe4a),
				fw_byte(0x0010fe4f), fw_byte(0x0010fea4), fw_byte(0x0010fea5),
				fw_byte(0x0010fea6), fw_byte(0x0010fea7),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr &&
			(addr == 0x0020aebc || addr == 0x0020b712 || addr == 0x0020c76e ||
			 addr == 0x0020cd3c || addr == 0x0020d370 || addr == 0x0020d734 ||
			 addr == 0x0020d7f8 || addr == 0x0020dde0 || addr == 0x0020e63c ||
			 addr == 0x0020e6b8 || addr == 0x0020e6d8 || addr == 0x0020e74a) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_task15_09ec_dispatch: site=%08x state=%02x/%02x/%02x/%02x "
				"task=%02x t=%.6f\n", addr, fw_byte(0x0010fe48), fw_byte(0x0010fe49),
				fw_byte(0x0010fe4a), fw_byte(0x0010fe4f),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002226e8 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		logerror("radio_selection_status_received: object=%08x header=%02x/%02x/%02x/%02x "
				"bytes=%02x/%02x/%02x/%02x operation=%02x request=%02x continuation=%02x "
				"state=%02x/%02x/%02x/%02x desired=%02x/%02x/%02x "
				"source=%02x/%02x/%02x candidate=%02x/%02x/%02x task=%02x t=%.6f\n", object,
				fw_byte(object - 8), fw_byte(object - 7), fw_byte(object - 6), fw_byte(object - 5),
				fw_byte(object), fw_byte(object + 1), fw_byte(object + 2), fw_byte(object + 3),
				fw_byte(0x0010fd10), fw_byte(0x0010fd11), fw_byte(0x0010fcc5),
				fw_byte(0x0010fca8), fw_byte(0x0010fcb9), fw_byte(0x0010fcc0),
				fw_byte(0x0010fcc4), fw_byte(0x0010fd00), fw_byte(0x0010fd01),
				fw_byte(0x0010fd02), fw_byte(0x0010fd14), fw_byte(0x0010fd15),
				fw_byte(0x0010fd16), fw_byte(0x0010fcf8), fw_byte(0x0010fcf9),
				fw_byte(0x0010fcfa),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr &&
			(addr == 0x0022500a || addr == 0x00225014 ||
			 addr == 0x0022611a || addr == 0x00226126) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_candidate_commit_gate: pc=%08x candidate=%08x result=%08x "
				"policy=%02x mode=%02x source=%02x%02x%02x selected=%02x%02x%02x "
				"task=%02x t=%.6f\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R10),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				fw_byte(0x0010fcb9), fw_byte(0x0010fcc0),
				fw_byte(0x0010fd14), fw_byte(0x0010fd15), fw_byte(0x0010fd16),
				fw_byte(0x0010fcf8), fw_byte(0x0010fcf9), fw_byte(0x0010fcfa),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && (addr == 0x002090bc || addr == 0x00251f44) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		const u32 object = addr == 0x002090bc ?
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4) :
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u16 status = fw_word(object);
		if (status >= 0x09ec && status <= 0x09f3)
			logerror("radio_selection_status_adapter: pc=%08x object=%08x status=%04x "
					"bytes=%02x/%02x/%02x/%02x caller=%08x task=%02x t=%.6f\n",
					addr, object, status, fw_byte(object + 2), fw_byte(object + 3),
					fw_byte(object + 4), fw_byte(object + 5),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00209036 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_selection_result_convergence: input=%04x arg=%08x object=%08x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R5) & 0xffff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00209100 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_selection_search_result: input=%04x arg=%08x object=%08x "
				"result_byte=%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R5) & 0xffff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4), fw_byte(0x0010fe81),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002505ac &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("radio_l1_control_status: status=%04x rr_continuation=%02x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
				fw_byte(0x0010fcc5), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_CCONT_ADC", 0) != 0 &&
			addr == 0x002b08c6)
	{
		logerror("ccont_charger_policy: mode=%04x event=%04x flags=%02x "
				"task=%02x caller=%08x t=%.9f\n",
			fw_word(FW_STARTUP_MODE), fw_word(FW_STARTUP_EVENT),
			fw_byte(0x00112399), fw_byte(FW_SCHED_RUNNING_TASK_ID),
			m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
			machine().time().as_double());
	}
	// Keep the CCONT policy and alarm publisher visible without tracing the
	// surrounding task-1 dispatcher.
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0 &&
			(addr == 0x002b08c6 || addr == 0x002b0942))
	{
		const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		logerror("ccont_policy: pc=%08x caller=%08x r0=%08x task=%02x t=%.9f\n",
			addr, lr, m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
			fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x002b13d4 &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0)
	{
		const u32 event = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
		if (event >= 0x7701 && event <= 0x7708)
			logerror("ccont_policy: pc=%08x event=%04x caller=%08x task=%02x t=%.9f\n",
				addr, event, m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x0020b40a &&
			fw_byte(FW_SCHED_RUNNING_TASK_ID) == 15)
		logerror("registration_state_select: state=%02x protocol=%02x/%02x/%02x/%02x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
				fw_byte(0x0010fe48), fw_byte(0x0010fe49),
				fw_byte(0x0010fe4a), fw_byte(0x0010fe4f),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002b13b0 &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0)
	{
		const u32 event = m_maincpu->state_int(arm7_cpu_device::ARM7_R5) & 0xffff;
		if (event >= 0x7701 && event <= 0x7708)
			logerror("ccont_policy: event=%04x available=%u task=%02x t=%.9f\n",
				event, m_maincpu->state_int(arm7_cpu_device::ARM7_R0) != 0,
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x0029b082 &&
				nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0)
	{
		logerror("rtc_minute_policy: pc=%08x state=%02x flags=%02x/%02x base=%08x offset=%08x deadline=%08x task=%02x t=%.9f\n",
			addr,
			fw_byte(0x001121d2), fw_byte(0x001121d5), fw_byte(0x001121d6),
			fw_dword(0x001121d8), fw_dword(0x001121e0), fw_dword(0x001121e4),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x0029b1ec &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0)
	{
		logerror("rtc_alarm_match: candidate=%08x current=%08x state=%02x flags=%02x/%02x task=%02x caller=%08x t=%.9f\n",
			m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
			m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
			fw_byte(0x001121d3), fw_byte(0x001121d5), fw_byte(0x001121d6),
			fw_byte(FW_SCHED_RUNNING_TASK_ID),
			m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
			machine().time().as_double());
	}
	if (pc == addr && (addr == 0x0029b4d8 || addr == 0x0029b580) &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0)
	{
		logerror("rtc_alarm_state: pc=%08x state=%02x flags=%02x/%02x deadline=%08x previous=%08x task=%02x caller=%08x t=%.9f\n",
			addr, fw_byte(0x001121d3), fw_byte(0x001121d5), fw_byte(0x001121d6),
			fw_dword(0x001121e4), fw_dword(0x001121dc),
			fw_byte(FW_SCHED_RUNNING_TASK_ID),
			m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
			machine().time().as_double());
	}
	// Observe the branch target before the 0x46bc publication wrapper.  The
	// wrapper entry itself is not a stable flash-tap boundary on ARM7 fetches.
	if (pc == addr && addr == 0x0029b0d4 &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0)
	{
		logerror("rtc_minute_policy: event=46bc payload=%08x task=%02x t=%.9f\n",
			m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
			fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0 &&
			addr == 0x002af77c &&
			(m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff) == 0x46bc)
	{
		logerror("rtc_alarm_route: packed=46bc caller=%08x task=%02x t=%.9f\n",
			m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
			fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x002af646 &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0 &&
			fw_word(0x00110f1a) == 0x06bc)
	{
		logerror("rtc_alarm_route: map-input=06bc task=%02x t=%.9f\n",
			fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x002af64a &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_RTC", 0) != 0 &&
			fw_word(0x00110f1a) == 0x06bc)
		logerror("rtc_alarm_route: map-output=%04x base=%08x offset=%08x deadline=%08x task=%02x t=%.9f\n",
			m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
			fw_dword(0x001121d8), fw_dword(0x001121e0), fw_dword(0x001121e4),
			fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());

	// CCONT watchdog-service provenance. 0x2b4dc0 services MAD2 for input bit 0
	// and CCONT for bit 1; descriptor 6 at the generic masked-write entry maps
	// through GENSIO to physical CCONT register 5. These values are lost before
	// the byte reaches the device.
	if (pc == addr && addr == 0x002b4dc0 &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_WATCHDOG", 0) != 0)
	{
		const u32 mask = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		logerror("ccont_watchdog_service: mask=%02x caller=%08x task=%02x t=%.6f\n",
				mask & 0xff, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x002afa74 &&
			nokia_env_u32("NOKI3210_TRACE_CCONT_WATCHDOG", 0) != 0)
	{
		const u32 descriptor = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 value = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		if ((descriptor & 0xff) == 6 || ((descriptor >> 24) & 0xff) == 6)
			logerror("ccont_watchdog_write: descriptor=%08x physical_reg=5 value=%02x "
					"caller=%08x task=%02x t=%.6f\n",
					descriptor, value & 0xff, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID),
					machine().time().as_double());
	}

	// Display-profile provenance: NV descriptor 0x0749 supplies three 12-byte
	// records at 0x112188.  Observe the load/update/copy boundaries without
	// changing either the stored records or the active record at 0x11fc80.
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DISPLAY_PROFILE", 0) != 0 &&
			(addr == 0x0029a996 || addr == 0x0029ae68 ||
			 addr == 0x0029a768 || addr == 0x002b1e80 || addr == 0x002af2ac))
	{
		logerror("display_profile: pc=%08x "
				"source0=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x "
				"active=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x task=%02x t=%.6f\n",
				addr,
				fw_byte(0x00112188), fw_byte(0x00112189), fw_byte(0x0011218a),
				fw_byte(0x0011218b), fw_byte(0x0011218c), fw_byte(0x0011218d),
				fw_byte(0x0011218e), fw_byte(0x0011218f), fw_byte(0x00112190),
				fw_byte(0x00112191), fw_byte(0x00112192), fw_byte(0x00112193),
				fw_byte(0x0011fc80), fw_byte(0x0011fc81), fw_byte(0x0011fc82),
				fw_byte(0x0011fc83), fw_byte(0x0011fc84), fw_byte(0x0011fc85),
				fw_byte(0x0011fc86), fw_byte(0x0011fc87), fw_byte(0x0011fc88),
				fw_byte(0x0011fc89), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}

	// Post-editor display frontier: observe firmware-owned render, resource,
	// window-manager, and DSP-TX boundaries without completing any operation.
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DISPLAY", 0) != 0 &&
			(addr == 0x002aefba || addr == 0x002af77c ||
			 addr == 0x002af64a || addr == 0x002af650 || addr == 0x002af656 ||
			 addr == 0x002af65c || addr == 0x002af662 || addr == 0x002af668 ||
			 addr == 0x002af66e))
	{
		const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		if (addr == 0x002aefba)
			logerror("display_frontier: event-map-enter status=%04x caller=%08x task=%02x t=%.6f\n",
					r0 & 0xffff, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
		else if (addr == 0x002af77c)
			logerror("display_frontier: event-queue status=%04x caller=%08x task=%02x t=%.6f\n",
					r0 & 0xffff, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
		else
			logerror("display_frontier: event-pipeline pc=%08x status=%04x task=%02x t=%.6f\n",
					addr, r0 & 0xffff, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DISPLAY", 0) != 0 &&
			(addr == 0x0028f4e4 || addr == 0x0028f504 || addr == 0x002ac54c ||
			 addr == 0x002af6ea || addr == 0x002b257e ||
			 addr == 0x00292878 ||
			 addr == 0x0029800c || addr == 0x00290840 ||
			 addr == 0x0029817e || addr == 0x002981a8 || addr == 0x002981ba ||
			 addr == 0x002981bc || addr == 0x002982a2 || addr == 0x002982ac ||
			 addr == 0x00298346 || addr == 0x00298348 ||
			 addr == 0x0026a204 || addr == 0x0026a354 ||
			 addr == 0x002b1e80 || addr == 0x002b1f24 || addr == 0x002b1f64))
	{
		static unsigned display_trace_count = 0;
		if (display_trace_count++ < 1024)
		{
			const u32 r0 = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			const u32 r1 = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
			const u32 r2 = m_maincpu->state_int(arm7_cpu_device::ARM7_R2);
			const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
			if (addr == 0x002af6ea)
				logerror("display_frontier: render-post packed=%04x event=%04x arg=%08x caller=%08x task=%02x mode=%04x t=%.6f\n",
						r0 & 0xffff, r0 & 0x1fff, r1, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID),
						fw_word(FW_STARTUP_MODE), machine().time().as_double());
			else if (addr == 0x002b257e)
			{
				logerror("display_frontier: resource-get id=%04x request=%08x caller=%08x task=%02x t=%.6f\n",
						r0 & 0xffff, r1, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
				if ((r0 & 0xffff) == 0x5031 && r1 >= NOKIA_RAM_BASE && r1 + 31 < NOKIA_RAM_END)
				{
					logerror("display_frontier: operator-resource data=");
					for (unsigned index = 0; index < 32; ++index)
						logerror("%02x", fw_byte(r1 + index));
					logerror("\n");
				}
			}
			else if (addr == 0x0029800c)
				logerror("display_frontier: window-recv msg=%08x bytes=%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x "
						"caller=%08x t=%.6f\n", r0, fw_byte(r0), fw_byte(r0 + 1),
						fw_byte(r0 + 2), fw_byte(r0 + 3), fw_byte(r0 + 4), fw_byte(r0 + 5),
						fw_byte(r0 + 6), fw_byte(r0 + 7), lr, machine().time().as_double());
			else if (addr == 0x0026a204 || addr == 0x0026a354)
			{
				const u32 message = r1;
				if ((r0 & 0xff) == 6 && message >= NOKIA_RAM_BASE && message + 7 < NOKIA_RAM_END)
					logerror("display_frontier: window-send via=%08x msg=%08x "
							"bytes=%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x caller=%08x task=%02x t=%.6f\n",
							addr, message, fw_byte(message), fw_byte(message + 1), fw_byte(message + 2),
							fw_byte(message + 3), fw_byte(message + 4), fw_byte(message + 5),
							fw_byte(message + 6), fw_byte(message + 7), lr,
							fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
			}
			else if (addr == 0x002b1e80 || addr == 0x002b1f24 || addr == 0x002b1f64)
				logerror("display_frontier: window-construct pc=%08x r0=%08x r1=%08x r2=%08x "
						"caller=%08x task=%02x t=%.6f\n", addr, r0, r1, r2, lr,
						fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
			else if (addr == 0x00292878)
				logerror("display_frontier: ui-start-callback status=%04x context-gate=%02x "
						"callback-state=%02x task=%02x t=%.6f\n", r0 & 0xffff,
						fw_byte(0x00110f1f), fw_byte(0x0011fc90),
						fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
			else if (addr >= 0x0029817e && addr <= 0x00298348)
			{
				logerror("display_frontier: window-state pc=%08x active=%02x flags=%02x idle=%02x "
						"message=%08x entries=", addr, fw_byte(0x001116f9),
						fw_byte(0x001116fa), fw_byte(0x001116fd), fw_dword(0x00111700));
				for (unsigned index = 0; index < 4; index++)
				{
					const u32 entry = 0x00111724 + index * 0x1c;
					logerror("%s%u:{obj=%08x mask=%02x/%02x type=%02x life=%02x}",
							index ? "," : "", index, fw_dword(entry + 0x14),
							fw_byte(entry + 0x0e), fw_byte(entry + 0x0f),
							fw_byte(entry + 0x13), fw_byte(entry + 0x19));
				}
				logerror(" task=%02x t=%.6f\n", fw_byte(FW_SCHED_RUNNING_TASK_ID),
						machine().time().as_double());
			}
			else if (addr == 0x002ac54c)
				logerror("display_frontier: callback-advance status=%04x cursor=%04x/%04x selector=%02x "
						"task=%02x t=%.6f\n",
						m_maincpu->state_int(arm7_cpu_device::ARM7_R6) & 0xffff,
						fw_word(0x00110f1c), fw_word(0x00110f1e),
						fw_byte(0x0011fcce), fw_byte(FW_SCHED_RUNNING_TASK_ID),
						machine().time().as_double());
			else
				logerror("display_frontier: pc=%08x r0=%08x r1=%08x r2=%08x caller=%08x task=%02x t=%.6f\n",
						addr, r0, r1, r2, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
		}
	}
	if (pc == addr && addr == 0x00290cf4 && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("dsp_boundary: service-command command=%08x arg=%08x payload=%08x caller=%08x "
				"task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x00100022), machine().time().as_double());
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(addr == 0x002a58cc || addr == 0x0023d62c || addr == 0x0023c55c))
	{
		const u32 argument = addr == 0x002a58cc ?
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4) :
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 message = addr == 0x0023c55c ? argument - 9 : argument;
		if (message >= NOKIA_RAM_BASE && message + 17 < NOKIA_RAM_END)
		{
			logerror("dsp_boundary: framed-rx pc=%08x msg=%08x data=", addr, message);
			for (unsigned index = 0; index < 18; index++)
				logerror("%02x", fw_byte(message + index));
			logerror(" task=%02x t=%.6f\n", fw_byte(FW_SCHED_RUNNING_TASK_ID),
					machine().time().as_double());
		}
	}
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(addr == 0x00284316 || addr == 0x0028464c))
	{
		const u32 message = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (message >= NOKIA_RAM_BASE && message + 31 < NOKIA_RAM_END)
		{
			logerror("dsp_boundary: lower-rx pc=%08x msg=%08x data=", addr, message);
			for (unsigned index = 0; index < 32; index++)
				logerror("%02x", fw_byte(message + index));
			logerror(" controller=%02x task=%02x t=%.6f\n", fw_byte(0x0010d0e2),
					fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
		}
	}
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(addr == 0x0023471e || addr == 0x00237c5a || addr == 0x002831a8 ||
			 addr == 0x0028316c || addr == 0x002824e4 ||
			 addr == 0x00282734 || addr == 0x00282a76))
	{
		const u32 message = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		logerror("dsp_boundary: framed-session pc=%08x phase=%02x index=%02x "
				"envelope=%02x/%02x/%02x msg=%08x task=%02x t=%.6f\n",
				addr, fw_byte(0x0011fedb), fw_byte(0x0011fedc),
				fw_byte(0x0011fe6a), fw_byte(0x0011fe6b), fw_byte(0x0011fe6c),
				message, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x002a8ca4 &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
	{
		logerror("dsp_boundary: callback0a status=%04x object=%08x state=%02x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1), fw_byte(0x0011fc8a),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 &&
			(addr == 0x002a2074 || addr == 0x0028d710 || addr == 0x0028d7fe || addr == 0x0028d886))
	{
		static unsigned l1_config_count = 0;
		if (l1_config_count++ < 128)
			logerror("dsp_boundary: l1-config pc=%08x caller=%08x task=%02x "
					"flags=%02x current=%02x active=%02x target=%02x applied=%04x requested=%04x "
					"r0=%04x r2=%04x t=%.6f\n",
					addr, m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					fw_byte(0x00100022), fw_byte(0x00110b3b), fw_byte(0x00112813),
					fw_byte(0x00112816), fw_byte(0x00112817), fw_word(0x0011281d),
					fw_word(0x0011281f),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)) & 0xffff,
					machine().time().as_double());
	}
	if (pc == addr && (addr == 0x002a1c88 || addr == 0x002a1ea6) &&
			nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("dsp_boundary: l1-state-input pc=%08x r0=%08x r1=%08x caller=%08x task=%02x t=%.6f\n",
				addr, m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x00100022), machine().time().as_double());
	if (pc == addr && addr == 0x00291028 && nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0)
		logerror("dsp_boundary: service-update r0=%08x r1=%08x r2=%08x r3=%08x caller=%08x "
				"a8=%04x ac=%04x ae=%04x b6=%04x bc=%04x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R3),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				m_dspif->shared_r(0x0a8 >> 1), m_dspif->shared_r(0x0ac >> 1),
				m_dspif->shared_r(0x0ae >> 1), m_dspif->shared_r(0x0b6 >> 1),
				m_dspif->shared_r(0x0bc >> 1), fw_byte(0x00100022),
				machine().time().as_double());
	// TRACE_TASKS (opt-in): app-task liveness tap. Log the first time each RTOS task reaches
	// the universal recv 0x26a458 (i.e. is scheduled and runs its message loop). Under the deep profile
	// stack this shows which of the app tasks 10-17 actually reach their receive loops.
	// alive. Task id = [0x100022]. docs/mmi_settlement.md app-task sweep.
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x0026a458)
	{
		static u32 seen = 0;
		const u32 tid = debug_ram_byte(0x00100022);
		if (tid < 31 && !(seen & (1u << tid)))
		{
			seen |= (1u << tid);
			logerror("task_alive: task %2u first recv caller=%08x t=%.4f\n", tid,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_DISPLAY", 0) != 0 && pc == addr && addr == 0x002af798)
	{
		const u16 packed = u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff;
		if ((packed & 0x1fff) == 0x0367)
			logerror("display_frontier: event-source status=0367 packed=%04x source=%08x caller=%08x "
					"task=%02x t=%.6f\n",
					packed, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a03b4)
	{
		static unsigned sim_rx_count = 0;
		if (sim_rx_count++ < 64)
			logerror("sim_rx_isr: irq=%04x fiq=%04x byte=%02x state=%02x remaining=%04x received=%04x t=%.8f\n",
					m_mad2->irq_status(), m_mad2->fiq_status(), debug_ram_byte(0x001106c4),
					debug_ram_byte(0x001106d4), debug_ram_word(0x001106d6),
					debug_ram_word(0x001106d8), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002028a4)
	{
		static bool sim_presence_seen = false;
		if (!sim_presence_seen)
		{
			sim_presence_seen = true;
			logerror("sim_presence_monitor: mode=%04x enable=%02x no-sim=%02x selected-df=%04x "
					"ready=%02x task=%02x caller=%08x t=%.8f\n",
					debug_ram_word(FW_STARTUP_MODE), debug_ram_byte(0x00111c79),
					debug_ram_byte(0x00111c64), debug_ram_word(0x00111ca4),
					debug_ram_byte(0x0010dcaf), debug_ram_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					machine().time().as_double());
		}
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x002028f4 || addr == 0x0020290a))
	{
		static unsigned sim_presence_result_count = 0;
		if (sim_presence_result_count++ < 4)
			logerror("sim_presence_result: %s current-df=%04x t=%.8f\n",
					addr == 0x0020290a ? "steady" : "changed",
					debug_ram_word(0x00111ca4), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a04c8)
	{
		static unsigned sim_irq_count = 0;
		if (sim_irq_count++ < 32)
			logerror("sim_irq_dispatch: irq=%04x fiq=%04x fifo=%02x control=%02x t=%.8f\n",
					m_mad2->irq_status(), m_mad2->fiq_status(), m_simi->rx_count_r(),
					debug_ram_byte(0x001106c3), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x0027e240 || addr == 0x002902ac || addr == 0x00293f30 ||
			 addr == 0x00207234 || addr == 0x0020733c || addr == 0x0027e046 ||
			 addr == 0x0027e98c || addr == 0x002a0218 || addr == 0x002a0060 ||
			 addr == 0x002a02e6 || addr == 0x002a0268 || addr == 0x002a0336))
	{
		static unsigned sim_contract_count = 0;
		if (sim_contract_count++ < 256)
			logerror("sim_contract: pc=%08x r0=%08x r1=%08x r2=%08x task=%02x enable=%02x t=%.8f\n",
					addr, u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)),
					debug_ram_byte(0x00100022), debug_ram_byte(0x00111c79),
					machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027ee94)
	{
		static unsigned sim_procedure_count = 0;
		const u32 r9 = m_maincpu->state_int(arm7_cpu_device::ARM7_R9);
		const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
		const u32 command = fw_dword(sp + 0x30);
		const u32 reply = fw_dword(sp + 0x34);
		if (sim_procedure_count++ < 128)
			logerror("sim_procedure: r9=%08x proc=%02x command=%08x ins=%02x command_len=%04x reply=%08x reply_len=%04x t=%.8f\n",
					r9, debug_ram_byte(r9), command, debug_ram_byte(command + 6),
					debug_ram_word(command + 2), reply, debug_ram_word(reply + 2),
					machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027df10)
	{
		static unsigned sim_recv_count = 0;
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		if (sim_recv_count++ < 256 && msg >= NOKIA_RAM_BASE && msg + 8 < NOKIA_RAM_END)
			logerror("sim_recv: msg=%08x code=%02x len=%04x bytes=%02x %02x %02x task=%02x lr=%08x sp=%08x t=%.8f\n",
					msg, debug_ram_byte(msg + 4), debug_ram_word(msg + 2),
					debug_ram_byte(msg + 5), debug_ram_byte(msg + 6), debug_ram_byte(msg + 7),
					debug_ram_byte(0x00100022),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R13)), machine().time().as_double());
		else if (sim_recv_count <= 256)
			logerror("sim_recv_static: msg=%08x task=%02x t=%.8f\n", msg,
					debug_ram_byte(0x00100022), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027defc)
	{
		static unsigned sim_recv_entry_count = 0;
		if (sim_recv_entry_count++ < 256)
			logerror("sim_recv_entry: lr=%08x r0=%08x r1=%08x sp=%08x task=%02x t=%.8f\n",
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R13)),
					debug_ram_byte(0x00100022), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x0027ebc0)
	{
		const u32 manager = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		logerror("sim_atr_result: result=%02x atr_len=%04x state=%02x manager=%08x fields=%02x/%02x/%02x/%02x/%02x t=%.8f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xff,
				debug_ram_word(0x0010dddc), debug_ram_byte(0x001106c0), manager,
				debug_ram_byte(manager + 0xa), debug_ram_byte(manager + 0xb),
				debug_ram_byte(manager + 0xc), debug_ram_byte(manager + 0xd),
				debug_ram_byte(manager + 0x10), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a04b6)
	{
		const u32 msg = (u32(debug_ram_word(0x001106cc)) << 16) |
				debug_ram_word(0x001106ce);
		logerror("sim_rx_complete: msg=%08x code=%02x len=%04x mode=%02x remaining=%04x received=%04x t=%.8f\n",
				msg, debug_ram_byte(msg + 4), debug_ram_word(msg + 2),
				debug_ram_byte(0x001106d4), debug_ram_word(0x001106d6),
				debug_ram_word(0x001106d8), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr && addr == 0x002a04be)
		logerror("sim_rx_post_result: r0=%08x task=%02x t=%.8f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)),
				debug_ram_byte(0x00100022), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && pc == addr && addr == 0x0023f396)
	{
		const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
		const u32 context = fw_dword(sp + 0x10);
		logerror("radio_l2_request: context=%08x state=%04x sapi=%02x flags=%02x/%02x/%02x input=%04x t=%.6f\n",
				context, fw_word(context + 4), fw_byte(context), fw_byte(context + 1),
				fw_byte(context + 6), fw_byte(context + 7), fw_dword(sp + 0x14) & 0xffff,
				machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && pc == addr && addr == 0x0023e306)
	{
		const u32 context = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		logerror("radio_l2_ack: context=%08x state=%04x sapi=%02x flags=%02x/%02x/%02x t=%.6f\n",
				context, fw_word(context + 4), fw_byte(context), fw_byte(context + 1),
				fw_byte(context + 6), fw_byte(context + 7), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && pc == addr &&
			(addr == 0x0023e94c || addr == 0x002403f4))
	{
		const u32 context = addr == 0x0023e94c ?
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R4)) :
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1));
		logerror("radio_l2_release: site=%08x context=%08x state=%04x sapi=%02x flags=%02x/%02x/%02x t=%.6f\n",
				addr, context, fw_word(context + 4), fw_byte(context), fw_byte(context + 1),
				fw_byte(context + 6), fw_byte(context + 7), machine().time().as_double());
	}
	if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && pc == addr &&
			addr == 0x0028cfec && (m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff) == 0x05df)
		logerror("radio_l2_request_build: caller=%08x object=%08x sapi=%02x t=%.6f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R1)),
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R2)) & 0xff,
				machine().time().as_double());
	// TRACE_TASKS post-flow: inter-task messages during the handoff/stall. r1 is a message pointer;
	// decode its status instead of treating the allocation address as a protocol code.
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && (addr == 0x0026a354 || addr == 0x0026a204))
	{
		const u32 from = debug_ram_byte(0x00100022);
		const u32 to = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff;
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		const bool ram_msg = msg >= NOKIA_RAM_BASE && msg + 32 <= NOKIA_RAM_END;
		const bool flash_msg = msg >= 0x00200000 && msg + 32 <= 0x00400000;
		const bool readable_msg = ram_msg || flash_msg;
		auto message_byte = [&](u32 address) {
			return ram_msg ? fw_byte(address) : m_maincpu->space(AS_PROGRAM).read_byte(address);
		};
		const u16 status = ram_msg ? fw_word(msg) :
				(flash_msg ? m_maincpu->space(AS_PROGRAM).read_word(msg) : 0xffff);
		if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && readable_msg &&
				status >= 0x09ec && status <= 0x09f3)
			logerror("radio_selection_post: t%u -> t%u status=%04x msg=%08x caller=%08x "
					"bytes=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x t=%.6f\n",
					from, to, status, msg,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					message_byte(msg), message_byte(msg + 1), message_byte(msg + 2),
					message_byte(msg + 3), message_byte(msg + 4), message_byte(msg + 5),
					message_byte(msg + 6), message_byte(msg + 7), machine().time().as_double());
		if (readable_msg && (status == 0x1391 || status == 0x1396 || status == 0x1397 || status == 0x0433))
			logerror("radio_identity_post: t%u -> t%u status=%04x msg=%08x bytes=%02x/%02x/%02x/%02x "
					"tail=%02x/%02x/%02x/%02x/%02x sim=%02x/%02x/%02x/%02x "
					"candidate=%02x/%02x/%02x/%02x t=%.6f\n",
					from, to, status, msg, message_byte(msg + 2), message_byte(msg + 3),
					message_byte(msg + 4), message_byte(msg + 5),
					message_byte(msg + 0x14), message_byte(msg + 0x15), message_byte(msg + 0x16),
					message_byte(msg + 0x17), message_byte(msg + 0x18),
					(fw_byte(0x0010ef34) >> 3) & 0x07, fw_byte(0x0010ef34) & 0x01,
					(fw_byte(0x0010ef35) & 0x07) + 2, ((fw_byte(0x0010ef34) >> 1) & 0x03) + 1,
					fw_byte(0x0010d94c), fw_byte(0x0010d94d), fw_byte(0x0010d94e),
					fw_byte(0x0010d94f), machine().time().as_double());
		if (nokia_env_u32("NOKI3210_TRACE_DSP_BOUNDARY", 0) != 0 && readable_msg &&
				(from == 13 || to == 13 || from == 16 || to == 16))
			logerror("radio_rr_post: t%u -> t%u status=%04x msg=%08x caller=%08x "
					"bytes=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x t=%.6f\n",
					from, to, status, msg,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					message_byte(msg), message_byte(msg + 1), message_byte(msg + 2), message_byte(msg + 3),
					message_byte(msg + 4), message_byte(msg + 5), message_byte(msg + 6), message_byte(msg + 7),
					machine().time().as_double());
		if (readable_msg && (status == 0x0fa3 || status == 0x09d2))
			logerror("call_boundary_post: t%u -> t%u status=%04x msg=%08x caller=%08x bytes="
					"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x t=%.6f\n",
					from, to, status, msg,
					u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
					message_byte(msg + 0), message_byte(msg + 1), message_byte(msg + 2), message_byte(msg + 3),
					message_byte(msg + 4), message_byte(msg + 5), message_byte(msg + 6), message_byte(msg + 7),
					message_byte(msg + 8), message_byte(msg + 9), message_byte(msg + 10), message_byte(msg + 11),
					message_byte(msg + 12), message_byte(msg + 13), message_byte(msg + 14), message_byte(msg + 15),
					machine().time().as_double());
		if (to == 19 && readable_msg)
		{
			static unsigned task19_posts = 0;
			if (task19_posts++ < 128)
				logerror("task19_post: from=%02x status=%04x via=%08x msg=%08x "
						"bytes=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x "
						"caller=%08x t=%.6f\n",
						from, status, addr, msg,
						message_byte(msg), message_byte(msg + 1), message_byte(msg + 2), message_byte(msg + 3),
						message_byte(msg + 4), message_byte(msg + 5), message_byte(msg + 6), message_byte(msg + 7),
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						machine().time().as_double());
		}
		static u32 seen[64]; static unsigned n = 0; static u32 total = 0;
		total++;
		const u32 kv = (from << 8) | to;   // dedup by communication EDGE (codes are per-message seq ids)
		bool dup = false; for (unsigned i = 0; i < n; i++) if (seen[i] == kv) { dup = true; break; }
		if (!dup && n < 64)
		{
			seen[n++] = kv;
			logerror("msgedge: t%u -> t%u (first status=%04x msg=%08x) t=%.4f\n",
					from, to, status, msg, machine().time().as_double());
		}
		if ((from == 3 || from == 5 || from == 10 || from == 15 || from == 16 || from == 17 || from == 20 || from == 21 ||
				to == 3 || to == 5 || to == 10 || to == 15 || to == 16 || to == 17 || to == 20 || to == 21) && readable_msg)
		{
			static unsigned provider_posts = 0;
			static unsigned task17_posts = 0;
			static unsigned task5_posts = 0;
			const bool task17_edge = from == 17 || to == 17;
			const bool task5_edge = from == 5 || to == 5;
			const bool log_post = task17_edge ? task17_posts++ < 256 :
				(task5_edge ? task5_posts++ < 256 : provider_posts++ < 128);
			if (log_post)
				logerror("sim_owner_post: t%u -> t%u status=%04x msg=%08x caller=%08x bytes="
						"%02x %02x %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x t=%.4f\n",
						from, to, status, msg,
						u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
						message_byte(msg + 0), message_byte(msg + 1), message_byte(msg + 2), message_byte(msg + 3),
						message_byte(msg + 4), message_byte(msg + 5), message_byte(msg + 6), message_byte(msg + 7),
						message_byte(msg + 8), message_byte(msg + 9), message_byte(msg + 10), message_byte(msg + 11),
						message_byte(msg + 12), message_byte(msg + 13), message_byte(msg + 14), message_byte(msg + 15),
						machine().time().as_double());
		}
		if ((total % 2000) == 0)
			logerror("msgrate: %u posts by t=%.4f (protocol still active)\n", total, machine().time().as_double());
	}
	// TRACE_SERVICE_COMMAND: log both sides of the class-0x40 external-service transport.
	// 0x234634 constructs an MCU-to-peer frame and 0x234684 queues it; the dispatcher below
	// consumes peer-to-MCU frames. Keeping direction explicit prevents acknowledgements from
	// being mistaken for organic producers of inbound commands with the same numeric id.
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x00234634)
		{
			static unsigned csconstruct_count = 0;
			if (csconstruct_count++ < 256)
				logerror("csconstruct: command=%02x payload=%02x output=%08x caller=%08x task=%02x t=%.4f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R2) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x00234826)
		{
			const u32 sp = m_maincpu->state_int(arm7_cpu_device::ARM7_R13);
			logerror("cseeprom_fail: computed=%04x stored=%04x guard=%04x sp=%08x task=%02x t=%.6f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R4) & 0xffff,
					fw_word(sp + 4), fw_word(sp + 6), sp, fw_byte(FW_SCHED_RUNNING_TASK_ID),
					machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x00234684)
		{
			static unsigned cssend_count = 0;
			const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			if (cssend_count++ < 256 && msg >= NOKIA_RAM_BASE && msg + 9 < NOKIA_RAM_END)
				logerror("cssend: command=%02x class=%02x destination=%02x source=%02x route=%02x/%02x/%02x "
						"length=%04x msg=%08x caller=%08x task=%02x t=%.4f\n",
						fw_byte(msg + 8), fw_byte(msg + 3), fw_byte(msg), fw_byte(msg + 1),
						fw_byte(msg + 2), fw_byte(msg + 6), fw_byte(msg + 7), fw_word(msg + 4), msg,
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
						debug_ram_byte(0x00100022), machine().time().as_double());
		}
		// The class-0x40 service-command dispatcher 0x237400 reads the
		// command byte [msg+8] into r4 at 0x23741a and binary-searches to a handler. cmd 0x70/0x71 route to
		// the channel-map handler 0x23670c: 0x70 = resource ENABLE (-> 0x2b140a, config blob = msg+9),
		// 0x71 = resource DISABLE. Log every command byte the service dispatcher processes -- confirms whether
		// the resource-enable command 0x70 is ever delivered on our boot, and inventories the command set.
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x0023741a)
		{
			const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
			constexpr u32 task2_tcb = 0x00101484 + 2 * 0x1c;
			static unsigned cc = 0;
			if (cc++ < 80 && msg >= 0x00100000 && msg < 0x00180000)
				logerror("cscmd: command=%02x class=%02x destination=%02x source=%02x route=%02x/%02x/%02x "
						"length=%04x payload0=%02x "
						"msg=%08x svcready=%02x qlink=%08x qa=%08x:%02x/%02x qb=%08x:%02x/%02x "
						"caller=%08x task=%02x t=%.4f\n",
						debug_ram_byte(msg + 8), debug_ram_byte(msg + 3), debug_ram_byte(msg), debug_ram_byte(msg + 1),
						debug_ram_byte(msg + 2), debug_ram_byte(msg + 6), debug_ram_byte(msg + 7),
						debug_ram_word(msg + 4), debug_ram_byte(msg + 9), msg, debug_ram_byte(0x0011fed1),
						fw_dword(task2_tcb + 8), fw_dword(task2_tcb + 0xc),
						fw_byte(task2_tcb + 0x10), fw_byte(task2_tcb + 0x11),
						fw_dword(task2_tcb + 0x14), fw_byte(task2_tcb + 0x18), fw_byte(task2_tcb + 0x19),
						m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
						debug_ram_byte(0x00100022), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x002a5468)
		{
			const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
			static unsigned csroute_count = 0;
			if (csroute_count++ < 256 && msg >= NOKIA_RAM_BASE && msg + 9 < NOKIA_RAM_END)
				logerror("csroute: msg=%08x address=%02x/%02x route=%02x/%02x/%02x class=%02x command=%02x "
						"local=%02x caller=%08x task=%02x t=%.6f\n",
					msg, fw_byte(msg), fw_byte(msg + 1), fw_byte(msg + 2), fw_byte(msg + 6),
					fw_byte(msg + 7), fw_byte(msg + 3), fw_byte(msg + 8), fw_byte(0x00111794),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					fw_byte(0x00100022), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x00237c8e)
		{
			static unsigned csfree_count = 0;
			if (csfree_count++ < 256)
				logerror("csfree: msg=%08x command=%02x caller=%08x task=%02x t=%.6f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
					fw_byte(m_maincpu->state_int(arm7_cpu_device::ARM7_R4) + 8),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), machine().time().as_double());
		}
		// Service-session result handling and its watchdog share 0x236dc4. Record
		// which side actually drives repeated command-0x64 reports, together with
		// the compact state block consumed by both paths.
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x00236dc4)
		{
			static unsigned csresult_count = 0;
			if (csresult_count++ < 256)
				logerror("csresult: result=%02x caller=%08x task=%02x "
						"present=%02x flags=%02x stored=%02x counter=%02x state=%02x ack=%02x enable=%02x t=%.6f\n",
					m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), debug_ram_byte(FW_SERVICE_SESSION_STATUS),
					debug_ram_byte(FW_SERVICE_SESSION_STATUS + 1),
					debug_ram_byte(FW_SERVICE_SESSION_RESULT),
					debug_ram_byte(FW_SERVICE_SESSION_COUNTER),
					debug_ram_byte(FW_SERVICE_SESSION_SUBSTATE),
					debug_ram_byte(FW_SERVICE_SESSION_ACK),
					debug_ram_byte(FW_SERVICE_CHANNEL_ENABLE_FLAGS), machine().time().as_double());
		}
		if (nokia_env_u32("NOKI3210_TRACE_SERVICE_COMMAND", 0) != 0 && pc == addr && addr == 0x00237b80)
		{
			static unsigned cswatch_count = 0;
			if (cswatch_count++ < 256)
				logerror("cswatch: present=%02x flags=%02x stored=%02x counter=%02x state=%02x ack=%02x enable=%02x "
						"caller=%08x task=%02x t=%.6f\n",
					debug_ram_byte(FW_SERVICE_SESSION_STATUS),
					debug_ram_byte(FW_SERVICE_SESSION_STATUS + 1),
					debug_ram_byte(FW_SERVICE_SESSION_RESULT),
					debug_ram_byte(FW_SERVICE_SESSION_COUNTER),
					debug_ram_byte(FW_SERVICE_SESSION_SUBSTATE),
					debug_ram_byte(FW_SERVICE_SESSION_ACK),
					debug_ram_byte(FW_SERVICE_CHANNEL_ENABLE_FLAGS),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					debug_ram_byte(0x00100022), machine().time().as_double());
		}
	// Observe the lower GSM receive path without participating in it.  Task 15 receives event 0x07dd
	// at 0x20a026; its payload pointer at message+8 is parsed by 0x209978 and converted to an
	// internal result before task 15 emits any 0x09e* status toward task 14.
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x0027e3cc || addr == 0x0027e8fe || addr == 0x002085ce ||
			 addr == 0x00203d2c || addr == 0x002938b0 || addr == 0x00293522 ||
			 addr == 0x002726e0))
		logerror("sim_contract: registration-object pc=%08x r0=%08x r1=%08x r2=%08x r4=%08x "
				"notify=%02x task=%02x caller=%08x t=%.6f\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4), fw_byte(0x0010dcb7),
				fw_byte(0x00100022),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_SIM_RX", 0) != 0 && pc == addr &&
			(addr == 0x002038ec || addr == 0x00203906 || addr == 0x0020390c ||
			 addr == 0x00203936 || addr == 0x00203948 || addr == 0x0027def4))
		logerror("sim_contract: toolkit-gate pc=%08x phase=%02x selected=%04x latch=%02x "
				"r0=%08x caller=%08x task=%02x t=%.6f\n", addr,
				fw_byte(0x00111c78), fw_word(0x00111ca6), fw_byte(0x0010dcb7),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(0x00100022), machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_GSM_SERVICE", 0) != 0 && pc == addr && addr == 0x002618e8)
		logerror("gsm_lower: service5 callback status=%04x state=%02x index=%02x queue=%02x/%02x/%02x/%02x flags=%08x caller=%08x r4=%08x r5=%08x r6=%08x r7=%08x t=%.4f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
				fw_byte(0x0011fcc3),
				fw_byte(0x0011fcce),
				fw_byte(0x00111930), fw_byte(0x00111931), fw_byte(0x00111938), fw_byte(0x00111939),
				m_maincpu->space(AS_PROGRAM).read_dword(0x002db864),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4), m_maincpu->state_int(arm7_cpu_device::ARM7_R5),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6), m_maincpu->state_int(arm7_cpu_device::ARM7_R7),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_GSM_SERVICE", 0) != 0 && pc == addr && addr == 0x002af6ea)
	{
		const u32 packed = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
		const u32 event = packed & 0x1fff;
		if (event == 0x0bcc || event == 0x13f8)
			logerror("gsm_service: frontier-packed event=%04x packed=%04x argument=%08x caller=%08x task=%02x t=%.4f\n",
					event, packed, m_maincpu->state_int(arm7_cpu_device::ARM7_R1),
					m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
					fw_byte(0x00100022), machine().time().as_double());
	}
	// Observe generic service-framework registration and dispatch. This is diagnostic only;
	// service 5 is the organic callback path relevant to the argumentless 0x05e8 result.
	if (pc == addr && nokia_env_u32("NOKI3210_TRACE_GSM_SERVICE", 0) != 0)
	{
		static unsigned trace_count = 0;
		auto reg = [&](int n){ return u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0 + n)); };
		auto dump = [&](u32 p, unsigned n, char *out, size_t cap) {
			size_t used = 0;
			if (p < NOKIA_RAM_BASE || p + n > NOKIA_RAM_END)
			{
				std::snprintf(out, cap, "<%08x>", p);
				return;
			}
			for (unsigned i = 0; i < n && used + 4 < cap; i++)
				used += std::snprintf(out + used, cap - used, "%02x%s", fw_byte(p + i), i + 1 == n ? "" : " ");
		};
		if (addr == 0x002438e8 && (reg(0) == 0x21 || reg(2) == 0x1a))
		{
			static unsigned radio_resource_calls = 0;
			if (radio_resource_calls++ < 64)
				logerror("gsm_service: radio-resource channel=%02x slot=%02x resource=%04x arg=%08x "
						"caller=%08x task=%02x t=%.4f\n",
						reg(0) & 0xff, reg(1) & 0xff, reg(2) & 0xffff, reg(3),
						reg(14) & ~u32(1), fw_byte(FW_SCHED_RUNNING_TASK_ID),
						machine().time().as_double());
		}
		if (addr == 0x002438e8 && reg(2) > 0xff)
			logerror("gsm_service: callback-resource id=%04x channel=%02x slot=%02x arg=%08x t=%.4f\n",
					reg(2) & 0xffff, reg(0) & 0xff, reg(1) & 0xff, reg(3),
					machine().time().as_double());
		if (addr == 0x002ae8ba)
			logerror("gsm_service: registered-callback event=%04x source=%08x t=%.4f\n",
					reg(0) & 0xffff, reg(1), machine().time().as_double());
		if (addr == 0x00262bc6 || addr == 0x00262dfe || addr == 0x00262e0c)
			logerror("gsm_service: callback-gate pc=%08x r0=%08x r1=%08x r2=%08x r4=%08x r5=%08x t=%.4f\n",
					addr, reg(0), reg(1), reg(2), reg(4), reg(5), machine().time().as_double());
		if (addr == 0x002aed5c || addr == 0x002aefca)
			logerror("gsm_service: task5-lookup pc=%08x current=%04x packed=%04x family=%02x result=%08x t=%.4f\n",
					addr, fw_word(0x00112086), fw_word(0x00112088),
					fw_byte(0x0011fc80 + (fw_word(0x00112086) & 0xff)), reg(0),
					machine().time().as_double());
		if (addr == 0x002632fc && trace_count++ < 512 &&
				reg(2) >= NOKIA_RAM_BASE && reg(2) + 0x1c <= NOKIA_RAM_END)
		{
			const u8 service = reg(0) & 0xff;
			const u32 descriptor = reg(2);
			const u32 data = (uint32_t(fw_word(descriptor)) << 16) | fw_word(descriptor + 2);
			const u16 callback = fw_word(descriptor + 0x12);
			char bytes[128] = {};
			char data_bytes[160] = {};
			char source_bytes[96] = {};
			const u32 source = (uint32_t(fw_word(descriptor + 0x0c)) << 16) | fw_word(descriptor + 0x0e);
			dump(descriptor, 0x1c, bytes, sizeof(bytes));
			dump(data, 32, data_bytes, sizeof(data_bytes));
			dump(source, 16, source_bytes, sizeof(source_bytes));
			logerror("gsm_service: register service=%02x ordinal=%u descriptor=%08x data=%08x raw=%08x source=%08x event=%04x callback=%04x caller=%08x bytes=[%s] data_bytes=[%s] source_bytes=[%s] t=%.4f\n",
					service, reg(1) & 0xff, descriptor, data, fw_dword(descriptor),
					source, fw_word(descriptor + 0x10), callback, reg(14) & ~u32(1),
					bytes, data_bytes, source_bytes, machine().time().as_double());
		}
		if (addr == 0x00263d30 && trace_count++ < 512)
			logerror("gsm_service: resident descriptor=%08x enable=%08x mask=%08x caller=%08x t=%.4f\n",
					reg(0), reg(1), reg(2), reg(14) & ~u32(1), machine().time().as_double());
		else if (addr == 0x00263154 && ((reg(0) & 0xff) == 0x0a || (reg(0) & 0xff) == 0x0b ||
				(reg(0) & 0xff) == 0x1e))
		{
			const u8 service = reg(0) & 0xff;
			const bool enabling = (reg(1) & 0xff) != 0;
			char registry[160] = {};
			dump(0x0010b2fc, 48, registry, sizeof(registry));
			logerror("gsm_service: service=%02x enabled=%u t=%.4f\n",
					service, enabling,
					machine().time().as_double());
			logerror("gsm_service: registry=[%s]\n", registry);
		}
		else if (addr == 0x002633d0 && ((reg(0) & 0xff) == 0x0a || (reg(0) & 0xff) == 0x0b ||
				(reg(0) & 0xff) == 0x1e))
		{
			logerror("gsm_service: declare service=%02x class=%02x opcode=%02x flags=%02x t=%.4f\n",
					reg(0) & 0xff, reg(1) & 0xff, reg(2) & 0xff, reg(3) & 0xff, machine().time().as_double());
		}
		else if (addr == 0x002629d0 && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: trigger token=%02x mode=%02x t=%.4f\n",
					reg(0) & 0xff, reg(1) & 0xff,
					machine().time().as_double());
		}
		else if (addr == 0x002624b8 && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: callback-dispatch service=%02x t=%.4f\n",
					reg(0) & 0xff, machine().time().as_double());
		}
		else if (addr == 0x0026265c && trace_count < 256)
		{
			trace_count++;
			char registry[160] = {};
			dump(0x0010b2dc, 48, registry, sizeof(registry));
			logerror("gsm_service: pending-scan caller=%08x active-slot=%02x current-service=%02x "
					"registry=[%s] t=%.4f\n",
					reg(14) & ~u32(1), fw_byte(0x00111931), fw_byte(0x00111f0f),
					registry, machine().time().as_double());
		}
		else if (addr == 0x00262544 && trace_count < 160)
		{
			trace_count++;
			char resident[128] = {};
			dump(reg(0), 0x1c, resident, sizeof(resident));
			logerror("gsm_service: resident-match object=%08x selector=%02x bytes=[%s] t=%.4f\n",
					reg(0), reg(1) & 0xff, resident, machine().time().as_double());
		}
		else if ((addr == 0x0026265c || addr == 0x0026309c || addr == 0x002625ac) && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: framework-step pc=%08x r0=%08x r1=%08x t=%.4f\n",
					addr, reg(0), reg(1), machine().time().as_double());
		}
		else if ((addr == 0x002438e8 || addr == 0x0024383c || addr == 0x0024387a ||
				 addr == 0x00243550 || addr == 0x00296f4e) && trace_count < 160)
		{
			trace_count++;
			logerror("gsm_service: resource-step pc=%08x r0=%08x r1=%08x r2=%08x r3=%08x t=%.4f\n",
					addr, reg(0), reg(1), reg(2), reg(3), machine().time().as_double());
		}
	}
}
