// Firmware-address-specific research diagnostics for nokia_3310_state.
//
// This file is included by nokia_3310.cpp after the state class definition.
// It may observe firmware execution and RAM writes, but must not alter fetched
// instructions, synthesize messages, or force firmware-owned state.

// ============================================================================
// Firmware-research RAM-write path: execution traces around the real backing
// store (COMBINE_DATA). NOT hardware behaviour; should shrink as investigations
// close.
// ============================================================================
void noki3310_state::ram_w_firmware_traces(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	const offs_t address = 0x100000 + (offset << 1);
	const u32 pc = m_maincpu->pc();
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
	COMBINE_DATA(&m_ram[offset]);
}

// ============================================================================
// Firmware-research traces for flash fetches. This is diagnostic observation,
// not hardware behaviour, and never changes the fetched instruction.
// ============================================================================
void noki3310_state::flash_firmware_traces(u32 pc, u32 addr)
{
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
				logerror("display_frontier: resource-get id=%04x request=%08x caller=%08x task=%02x t=%.6f\n",
						r0 & 0xffff, r1, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
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
	// stack this shows which of the app tasks 10-17 (resumed by mode-0xc's 0x2795e6) actually come
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
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x00221d74)
		logerror("sim_owner_event: task17 build status=%04x caller=%08x state=%02x/%02x/%02x/%02x t=%.4f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1),
				fw_byte(0x0010fca8), fw_byte(0x0010fca9), fw_byte(0x0010fcaa), fw_byte(0x0010fcba),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x00225c9c)
		logerror("sim_owner_phase_rx: status=%04x latch=%02x phase=%02x/%02x t=%.4f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R0)) & 0xffff,
				fw_byte(0x0010fcb4), fw_byte(0x0010fca9), fw_byte(0x0010fcba),
				machine().time().as_double());
	if (nokia_env_u32("NOKI3210_TRACE_TASKS", 0) != 0 && pc == addr && addr == 0x00251c2a)
	{
		const u32 msg = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		logerror("sim_owner_043c: caller=%08x msg=%08x bytes=%02x %02x %02x %02x %02x %02x %02x %02x t=%.4f\n",
				u32(m_maincpu->state_int(arm7_cpu_device::ARM7_R14)) & ~u32(1), msg,
				fw_byte(msg), fw_byte(msg + 1), fw_byte(msg + 2), fw_byte(msg + 3),
				fw_byte(msg + 4), fw_byte(msg + 5), fw_byte(msg + 6), fw_byte(msg + 7),
				machine().time().as_double());
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
