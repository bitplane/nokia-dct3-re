// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

// Curated firmware-address contract diagnostics for nokia_dct3_state.
//
// This file is included by nokia_dct3.cpp after the state class definition.
// Every retained probe is consumed by a focused regression gate. It may
// observe firmware execution, but must not alter fetched instructions,
// synthesize messages, or force firmware-owned state.

namespace {
constexpr offs_t FW_SCHED_RUNNING_TASK_ID = 0x100022;
}

uint16_t nokia_dct3_state::fw_word(offs_t address) const
{
	if (address < NOKIA_RAM_BASE || address >= NOKIA_RAM_END)
		return 0xffff;

	return m_ram[(address - NOKIA_RAM_BASE) >> 1];
}

void nokia_dct3_state::trace_dsp_audio_shadow_write(
		offs_t address, uint16_t old_data, uint16_t data)
{
	// The compiler tables relocate between the two NSE-8 ROMs. Keep these
	// firmware observations here; the shared DSP wire words are invariant.
	const bool v600_table = address >= 0x0011206c && address <= 0x00112080;
	const bool v501_table = address >= 0x00111e6c && address <= 0x00111e80;
	if (!v600_table && !v501_table)
		return;
	LOGMASKED(LOG_DSP_SHARED,
			"dsp_audio_shadow_write: address=%08x old=%04x data=%04x pc=%08x task=%02x t=%.6f\n",
			address, old_data, data, m_maincpu->pc(),
			fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
}

void nokia_dct3_state::trace_radio_pending_primitive_write(
		offs_t address, uint16_t old_data, uint16_t data)
{
	if (address != 0x0010d12c && address != 0x0010d168 &&
			address != 0x0010ab90 && address != 0x0010abcc)
		return;

	LOGMASKED(LOG_DSP_BOUNDARY,
			"radio_pending_primitive: address=%08x old=%04x data=%04x "
			"pc=%08x task=%02x t=%.6f\n",
			address, old_data, data, m_maincpu->pc(),
			fw_byte(FW_SCHED_RUNNING_TASK_ID),
			machine().time().as_double());
}

uint8_t nokia_dct3_state::fw_byte(offs_t address) const
{
	const uint16_t word = fw_word(address);
	return BIT(address, 0) ? uint8_t(word & 0x00ff) : uint8_t(word >> 8);
}

uint32_t nokia_dct3_state::fw_dword(offs_t address) const
{
	return (uint32_t(fw_word(address)) << 16) | uint32_t(fw_word(address + 2));
}

void nokia_dct3_state::flash_firmware_traces(u32 pc, u32 addr)
{
	if (!m_trace_enabled)
		return;

	// NSE-8 v6.00 and NHM-5 v6.39 share the same independently typed
	// authentication completion: RUN GSM ALGORITHM returns 2, GET RESPONSE
	// returns status 0x0066, and only then is SRES published to the radio task.
	// Observe the final branch, where speculative flash prefetch can no longer
	// make an intermediate register value look like a completed transaction.
	if (pc == addr && (addr == 0x00206914 || addr == 0x0021bcfc))
		LOGMASKED(LOG_DSP_BOUNDARY,
				"sim_authentication_consumer: pc=%08x get_status=%04x "
				"accepted=%02x task=%02x t=%.6f\n",
				addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4) & 0xffff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R5) & 0xff,
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());

	// Serving-cell acceptance checkpoints retained by verify-radio-camp.
	if (pc == addr && addr == 0x00284f74 && m_trace_enabled)
		LOGMASKED(LOG_DSP_BOUNDARY, "radio_channel_change_state: pc=%08x accept=%02x controller=%02x flags=%08x "
				"r0=%08x caller=%08x task=%02x t=%.6f\\n", addr,
				fw_byte(0x0010dbd7), fw_byte(0x0010dc93), fw_dword(0x0010dca0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x00284e50 && m_trace_enabled)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		LOGMASKED(LOG_DSP_BOUNDARY, "radio_no_psw_left_decode: object=%08x accept=%02x lifecycle=%02x "
				"phase=%02x task=%02x t=%.6f\\n", object, fw_byte(0x0010dbd7),
				fw_byte(0x0010c84c), fw_byte(0x0010dbdd),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x00213c04 && m_trace_enabled)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R8);
		LOGMASKED(LOG_DSP_BOUNDARY, "radio_acquisition_transition: pc=%08x object=%08x status=%04x action=%02x "
				"arg=%02x sequence=%02x lifecycle=%02x controller=%02x task=%02x t=%.6f\\n",
				addr, object, fw_word(object), fw_byte(object + 2), fw_byte(object + 3),
				fw_byte(0x00111e29), fw_byte(0x0010c84c), fw_byte(0x0010dbd7),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x0028d404 && m_trace_enabled)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		const bool readable = object >= NOKIA_RAM_BASE && object + 5 < NOKIA_RAM_END;
		LOGMASKED(LOG_DSP_BOUNDARY, "radio_candidate_table: object=%08x status=%04x usable=%02x "
				"arfcn=%04x rssi_id=%02x acquisition_mode=%02x task=%02x t=%.6f\\n", object,
				readable ? fw_word(object) : 0xffff, readable ? fw_byte(object + 2) : 0xff,
				readable ? fw_word(object + 4) : 0xffff, fw_byte(0x0010c84b),
				fw_byte(0x0010c83a), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x00273aac && m_trace_enabled)
	{
		const u32 block = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		LOGMASKED(LOG_DSP_BOUNDARY, "radio_bcch_parse: channel=%02x block=%08x data=",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xff, block);
		for (unsigned index = 0; block >= NOKIA_RAM_BASE && block + index < NOKIA_RAM_END && index < 24; ++index)
			LOGMASKED(LOG_DSP_BOUNDARY, "%02x", fw_byte(block + index));
		LOGMASKED(LOG_DSP_BOUNDARY, " task=%02x t=%.6f\\n", fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	if (pc == addr && addr == 0x00273e58 && m_trace_enabled)
		LOGMASKED(LOG_DSP_BOUNDARY, "radio_si3_path: pc=%08x result=%08x update1=%08x update2=%08x state=%02x active=%02x flags=%02x/%02x "
				"task=%02x t=%.6f\\n", addr,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R6),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R5), fw_byte(0x0010dbd7),
				fw_byte(0x0010edb4), fw_byte(0x0010edb6), fw_byte(0x0010edc1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	// NHM-5 v6.39 reaches this boundary after copying the 23-octet BCCH L3
	// block. The following instructions validate RR protocol discriminator 6
	// and dispatch System Information message types 0x19 through 0x1c.
	if (pc == addr && addr == 0x002934cc && m_trace_enabled)
	{
		const u32 block = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		LOGMASKED(LOG_DSP_BOUNDARY, "nhm5_bcch_parse: channel=%02x block=%08x data=",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R8) & 0xff, block);
		for (unsigned index = 0;
				block >= NOKIA_RAM_BASE && block + index < NOKIA_RAM_END && index < 23;
				++index)
			LOGMASKED(LOG_DSP_BOUNDARY, "%02x", fw_byte(block + index));
		LOGMASKED(LOG_DSP_BOUNDARY, " task=%02x t=%.6f\n",
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x002937e6 && m_trace_enabled)
	{
		const u32 state = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		const u32 block = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		const u32 result = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		LOGMASKED(LOG_DSP_BOUNDARY,
				"nhm5_si_result: message=%02x changed=%02x result=%08x channel=%02x "
				"flags=%02x/%02x/%02x status=%04x ready=%02x gate=%08x "
				"selector_si=%08x/%08x/%08x selector_event=%08x/%04x/%08x "
				"task=%02x t=%.6f\n",
				fw_byte(block + 2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R7) & 0xff,
				fw_dword(result + 4), fw_byte(state + 4), fw_byte(state + 0x0e),
				fw_byte(state + 0x12), fw_byte(state + 0x19),
				fw_word(state + 0x1a), fw_byte(state + 0x11),
				fw_dword(0x0010dc80),
				fw_dword(0x0011195c), fw_dword(0x00111960),
				fw_dword(0x00111964), fw_dword(0x00111ab8),
				fw_word(0x00111abc), fw_dword(0x00111ac0),
				fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}
	// NHM-6 v4.50E relocates and extends the NHM-5 BCCH consumer. These
	// observational probes are anchored by an independently reproduced
	// invariant-instruction comparison; retain opaque state bytes until the
	// divergent cell-suitability transition is understood.
	if (pc == addr && addr == 0x002ba578 && m_trace_enabled)
	{
		const u32 block = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		LOGMASKED(LOG_DSP_BOUNDARY, "nhm6_bcch_parse: channel=%02x block=%08x data=",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R8) & 0xff, block);
		for (unsigned index = 0;
				block >= NOKIA_RAM_BASE && block + index < NOKIA_RAM_END && index < 23;
				++index)
			LOGMASKED(LOG_DSP_BOUNDARY, "%02x", fw_byte(block + index));
		LOGMASKED(LOG_DSP_BOUNDARY, " task=%02x t=%.6f\n",
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && addr == 0x002ba8e2 && m_trace_enabled)
	{
		const u32 state = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		const u32 block = m_maincpu->state_int(arm7_cpu_device::ARM7_R5);
		const u32 result = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		LOGMASKED(LOG_DSP_BOUNDARY,
				"nhm6_si_terminal: message=%02x changed=%02x state=%08x state_data=",
				fw_byte(block + 2),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R7) & 0xff, state);
		for (unsigned index = 0;
				state >= NOKIA_RAM_BASE && state + index < NOKIA_RAM_END && index < 0x20;
				++index)
			LOGMASKED(LOG_DSP_BOUNDARY, "%02x", fw_byte(state + index));
		LOGMASKED(LOG_DSP_BOUNDARY, " result=%08x result_data=", result);
		for (unsigned index = 0;
				result >= NOKIA_RAM_BASE && result + index < NOKIA_RAM_END && index < 0x10;
				++index)
			LOGMASKED(LOG_DSP_BOUNDARY, "%02x", fw_byte(result + index));
		LOGMASKED(LOG_DSP_BOUNDARY,
				" selector_si=%08x/%08x/%08x selector_event=%08x/%04x/%08x "
				"task=%02x t=%.6f\n",
				fw_dword(0x00125cb0), fw_dword(0x00125cb4),
				fw_dword(0x00125cb8),
				fw_dword(0x00126050), fw_word(0x00126054),
				fw_dword(0x00126058),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && (addr == 0x00294000 || addr == 0x002c5e88) &&
			m_trace_enabled)
	{
		const bool nhm6 = addr == 0x002c5e88;
		const u32 caller = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		if (caller == (nhm6 ? 0x002c64c2 : 0x002946ae))
		{
			const u32 state = nhm6 ? 0x00125ca8 : 0x00111954;
			const u32 selected_si = fw_dword(state + 8);
			LOGMASKED(LOG_DSP_BOUNDARY,
					"radio_si_after_advance: firmware=%s selected_si=%08x "
					"selected_type=%04x alternate_si=%08x caller=%08x "
					"task=%02x t=%.6f\n",
					nhm6 ? "nhm6" : "nhm5", selected_si,
					selected_si >= NOKIA_RAM_BASE && selected_si + 1 < NOKIA_RAM_END ?
							fw_word(selected_si) : 0xffff,
					fw_dword(state + 0x0c), caller,
					fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
		}
	}
	if (pc == addr && (addr == 0x00295068 || addr == 0x002c6e7a) &&
			m_trace_enabled)
	{
		const bool nhm6 = addr == 0x002c6e7a;
		const u32 input = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 event = m_maincpu->state_int(arm7_cpu_device::ARM7_R4);
		const u32 state = nhm6 ? 0x00125ca8 : 0x00111954;
		LOGMASKED(LOG_DSP_BOUNDARY,
				"radio_si_selector_insert: firmware=%s input=%08x type=%04x "
				"caller=%08x event=%08x event_data=",
				nhm6 ? "nhm6" : "nhm5", input,
				input >= NOKIA_RAM_BASE && input + 1 < NOKIA_RAM_END ?
						fw_word(input) : 0xffff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				event);
		for (unsigned index = 0;
				event >= NOKIA_RAM_BASE && event + index < NOKIA_RAM_END && index < 0x20;
				++index)
			LOGMASKED(LOG_DSP_BOUNDARY, "%02x", fw_byte(event + index));
		LOGMASKED(LOG_DSP_BOUNDARY,
				" current=%08x pending=%08x task=%02x t=%.6f\n",
				fw_dword(state + 8), fw_dword(state + 0x0c),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	}
	if (pc == addr && (addr == 0x0020a026 || addr == 0x0020a02c) && m_trace_enabled)
	{
		const u32 object = m_maincpu->state_int(arm7_cpu_device::ARM7_R6);
		const u32 payload = object >= NOKIA_RAM_BASE && object + 0x0c < NOKIA_RAM_END ?
				fw_dword(object + 8) : 0;
		LOGMASKED(LOG_DSP_BOUNDARY, "radio_mm_parse: phase=%s object=%08x payload=%08x result=%08x "
				"mm=%02x/%02x/%02x/%02x task=%02x t=%.6f\n",
				addr == 0x0020a026 ? "entry" : "return", object, payload,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0),
				fw_byte(0x0010fe48), fw_byte(0x0010fe49), fw_byte(0x0010fe4a),
				fw_byte(0x0010fe4f), fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}

	// The answered-call codec-boundary gate inventories the firmware's real
	// shared-control commands before they are folded into DSP RAM words.
	if (pc == addr &&
			(addr == 0x00290cf4 || addr == 0x002906a8) &&
			m_trace_enabled)
		LOGMASKED(LOG_DSP_BOUNDARY,
				"dsp_shared_control: command=%02x value=%04x commit=%u caller=%08x task=%02x t=%.6f desired=%08x applied=%08x firmware=%s\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R1) & 0xffff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R2) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double(),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R4),
				m_maincpu->state_int(arm7_cpu_device::ARM7_R5),
				addr == 0x00290cf4 ? "v600" : "v501");

	// The alarm regression needs the firmware's final packed-event mapping.
	if (pc == addr && addr == 0x002af64a && m_trace_enabled && fw_word(0x00110f1a) == 0x06bc)
		LOGMASKED(LOG_CCONT_RTC, "rtc_alarm_route: map-output=%04x base=%08x offset=%08x deadline=%08x task=%02x t=%.9f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff,
				fw_dword(0x001121d8), fw_dword(0x001121e0), fw_dword(0x001121e4),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());

	// The watchdog gate checks the firmware helper's combined MAD2/CCONT service.
	if (pc == addr && addr == 0x002b4dda && m_trace_enabled)
		LOGMASKED(LOG_CCONT_WATCHDOG, "watchdog_terminal: reason=%02x caller=%08x task=%02x t=%.6f\n",
				m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xff,
				m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1),
				fw_byte(FW_SCHED_RUNNING_TASK_ID), machine().time().as_double());
	if (pc == addr && addr == 0x002b4dc0 && m_trace_enabled)
	{
		const u32 mask = m_maincpu->state_int(arm7_cpu_device::ARM7_R0);
		const u32 lr = m_maincpu->state_int(arm7_cpu_device::ARM7_R14) & ~u32(1);
		LOGMASKED(LOG_CCONT_WATCHDOG, "ccont_watchdog_service: mask=%02x caller=%08x task=%02x t=%.6f\n",
				mask & 0xff, lr, fw_byte(FW_SCHED_RUNNING_TASK_ID),
				machine().time().as_double());
	}

	// NV descriptor 0x0749 supplies three 12-byte display-profile records.
	if (pc == addr && m_trace_enabled &&
			(addr == 0x0029a996 || addr == 0x0029ae68 || addr == 0x0029a768 ||
			 addr == 0x002b1e80 || addr == 0x002af2ac))
	{
		LOGMASKED(LOG_DISPLAY_PROFILE, "display_profile: pc=%08x "
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

	// Registration presents the selected operator through resource 0x5031.
	if (pc == addr && addr == 0x002b257e && m_trace_enabled)
	{
		const u32 id = m_maincpu->state_int(arm7_cpu_device::ARM7_R0) & 0xffff;
		const u32 request = m_maincpu->state_int(arm7_cpu_device::ARM7_R1);
		if (id == 0x5031 && request >= NOKIA_RAM_BASE && request + 31 < NOKIA_RAM_END)
		{
			LOGMASKED(LOG_DISPLAY, "display_frontier: operator-resource data=");
			for (unsigned index = 0; index < 32; ++index)
				LOGMASKED(LOG_DISPLAY, "%02x", fw_byte(request + index));
			LOGMASKED(LOG_DISPLAY, "\n");
		}
	}
}
