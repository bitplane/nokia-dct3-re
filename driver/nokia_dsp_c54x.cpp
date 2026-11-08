// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_dsp_c54x.h"
#include "nokia_dsp_compare_clock.h"

DEFINE_DEVICE_TYPE(NOKIA_DSP_C54X, nokia_dsp_c54x_device, "nokia_dsp_c54x",
		"Nokia DCT3 C54x DSP backend")

nokia_dsp_c54x_device::nokia_dsp_c54x_device(const machine_config &mconfig,
		const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_DSP_C54X, tag, owner, clock),
	nokia_dsp_backend_interface(mconfig, *this),
	m_cpu(*this, "cpu"),
	m_transport(*this, finder_base::DUMMY_TAG),
	m_cobba(*this, finder_base::DUMMY_TAG),
	m_program_rom(*this, finder_base::DUMMY_TAG),
	m_data_rom(*this, finder_base::DUMMY_TAG),
	m_transfer_compare_cb(*this),
	m_rf_receive_cb(*this, 0),
	m_rf_control_cb(*this),
	m_rf_reset_cb(*this)
{
}

void nokia_dsp_c54x_device::device_add_mconfig(machine_config &config)
{
	TMS320C54X(config, m_cpu, clock());
	m_cpu->reset_execution_pipeline();
	m_cpu->set_standard_swwsr(true);
	// ROM4 ORs its operational masks into retained IMR state and assigns its
	// CTSI receiver to INT0. The MAD2/Lead cold-entry contract supplies bit 0;
	// subsequent reset pulses preserve firmware-written IMR in the CPU core.
	m_cpu->set_power_on_imr(0x0001);
	m_cpu->set_addrmap(AS_PROGRAM, &nokia_dsp_c54x_device::program_map);
	m_cpu->set_addrmap(AS_DATA, &nokia_dsp_c54x_device::data_map);
	m_cpu->set_addrmap(AS_IO, &nokia_dsp_c54x_device::io_map);
}

void nokia_dsp_c54x_device::device_start()
{
	if (m_program_rom.bytes() < 0x20000 || m_data_rom.bytes() < 0x20000)
		fatalerror("DCT3 C54x backend requires complete 64K-word program and data regions");
	std::copy_n(&m_program_rom[0], m_program.size(), m_program.begin());
	std::copy_n(&m_data_rom[0], m_data.size(), m_data.begin());
	m_slot_timer = timer_alloc(FUNC(nokia_dsp_c54x_device::slot_timer_expired), this);
	m_transfer_timer = timer_alloc(FUNC(nokia_dsp_c54x_device::transfer_timer_expired), this);
	m_frame_timer = timer_alloc(FUNC(nokia_dsp_c54x_device::frame_timer_expired), this);
	m_port36_timer = timer_alloc(FUNC(nokia_dsp_c54x_device::port36_complete), this);
	save_item(NAME(m_port36.control));
	save_item(NAME(m_port36.pending));
	save_item(NAME(m_port36.busy));
	save_item(NAME(m_port36.done));
	save_item(NAME(m_port36.strobe));
	save_item(NAME(m_fire.input));
	save_item(NAME(m_fire.command));
	save_item(NAME(m_fire.status));
	save_item(NAME(m_fire.count));
	save_item(NAME(m_fire.phase));
	save_item(NAME(m_fire.output));
	save_item(NAME(m_fire.output_cursor));
	save_item(NAME(m_fire.output_valid));
	save_item(NAME(m_program));
	save_item(NAME(m_data));
	save_item(NAME(m_io));
	save_item(NAME(m_bsp.control));
	save_item(NAME(m_bsp.receive));
	save_item(NAME(m_bsp.transmit));
	save_item(NAME(m_bsp.receive_ready));
	save_item(NAME(m_bsp.transmit_ready));
	save_item(NAME(m_bsp.transmit_pending));
	save_item(NAME(m_bsp.transmit_loaded));
	save_item(NAME(m_bsp.transmit_no_underflow));
	save_item(NAME(m_bsp.receive_overrun));
	save_item(NAME(m_bsp.receive_shift));
	save_item(NAME(m_bsp.receive_bits_left));
	save_item(NAME(m_bsp.receive_width));
	save_item(NAME(m_bsp.receive_clock_level));
	save_item(NAME(m_bsp.receive_sync_active));
	save_item(NAME(m_bsp.receive_continuous));
	save_item(NAME(m_bsp.receive_sync_released));
	save_item(NAME(m_bsp.transmit_shift));
	save_item(NAME(m_bsp.transmit_bits_left));
	save_item(NAME(m_bsp.transmit_clock_level));
	save_item(NAME(m_bsp.transmit_sync_active));
	save_item(NAME(m_bsp.transmit_suppressed));
	save_item(NAME(m_bsp.transmit_continuous));
	save_item(NAME(m_bsp.transmit_sync_released));
	save_item(NAME(m_host_command_line));
	save_item(NAME(m_reset_released));
	save_item(NAME(m_host_command_vector));
	save_item(NAME(m_slot_frame_length));
	save_item(NAME(m_slot_delay));
	save_item(NAME(m_transfer_delay));
	save_item(NAME(m_frame_counter_base));
	save_item(NAME(m_frame_counter_epoch));
	save_item(NAME(m_slot_compare_written));
	save_item(NAME(m_slot_request_pending));
	save_item(NAME(m_transfer_compare_written));
	save_item(NAME(m_transfer_request_pending));
	save_item(NAME(m_slot_timer_expiries));
	save_item(NAME(m_transfer_timer_expiries));
	save_item(NAME(m_frame_timer_expiries));
	save_item(NAME(m_io_trace_count));
	save_item(NAME(m_rf_trace_count));
	save_item(NAME(m_rf_port38_reads));
	save_item(NAME(m_rf_port39_reads));
	save_item(NAME(m_rf_port31));
	save_item(NAME(m_rf_port32));
	save_item(NAME(m_rf_port32_writes));
	save_item(NAME(m_completion_strobes));
	save_item(NAME(m_boot_mailbox_writes));
}

void nokia_dsp_c54x_device::device_reset()
{
	m_fire.reset();
	std::fill(m_io.begin(), m_io.end(), 0);
	m_bsp.reset();
	m_port36.reset();
	m_port36_timer->adjust(attotime::never);
	m_host_command_line = false;
	m_reset_released = false;
	m_host_command_vector = 0;
	m_slot_frame_length = 0;
	m_slot_delay = 0;
	m_transfer_delay = 0;
	m_frame_counter_base = 0;
	m_frame_counter_epoch = machine().time();
	m_slot_compare_written = false;
	m_slot_request_pending = false;
	m_transfer_compare_written = false;
	m_transfer_request_pending = false;
	m_slot_timer_expiries = 0;
	m_transfer_timer_expiries = 0;
	m_frame_timer_expiries = 0;
	m_io_trace_count = 0;
	m_rf_trace_count = 0;
	m_rf_port38_reads = 0;
	m_rf_port39_reads = 0;
	m_rf_port31 = 0;
	m_rf_port32 = 0;
	m_rf_port32_writes = 0;
	m_completion_strobes = 0;
	m_boot_mailbox_writes = 0;
	m_slot_timer->adjust(attotime::never);
	m_transfer_timer->adjust(attotime::never);
	m_frame_timer->adjust(attotime::never);
	m_cpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
}

void nokia_dsp_c54x_device::device_stop()
{
	machine().logerror("native_dsp_terminal_timing: released=%u clock=%u cycles=%llu frame_enabled=%u frame_remaining=%.9f slot_enabled=%u transfer_enabled=%u\n",
			m_reset_released, m_cpu->clock(), (unsigned long long)m_cpu->total_cycles(),
			m_frame_timer->enabled(), m_frame_timer->remaining().as_double(),
			m_slot_timer->enabled(), m_transfer_timer->enabled());
	machine().logerror("rom4_interface_summary: completion_strobes=%u mailbox_writes=%u "
			"slot_expiries=%llu frame_expiries=%llu rf_reads=%u rf_port32_writes=%u "
			"rf_port38_reads=%u rf_port39_reads=%u "
			"pc=%04x pmst=%04x ifr=%04x imr=%04x mode_aa=%04x mode_ac=%04x transfer_expiries=%llu\n",
			m_completion_strobes, m_boot_mailbox_writes,
			m_slot_timer_expiries, m_frame_timer_expiries,
			m_rf_trace_count, m_rf_port32_writes,
			m_rf_port38_reads, m_rf_port39_reads,
			u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
			m_data[0x00aa], m_data[0x00ac], m_transfer_timer_expiries);
}

u32 nokia_dsp_c54x_device::frame_period() const
{
	return u32(m_slot_frame_length ? m_slot_frame_length : 4999) + 1;
}

u16 nokia_dsp_c54x_device::frame_counter() const
{
	const u64 ticks = m_reset_released
			? (machine().time() - m_frame_counter_epoch).as_ticks(13'000'000) / 12 : 0;
	return (m_frame_counter_base + ticks) % frame_period();
}

void nokia_dsp_c54x_device::arm_frame_timer()
{
	// ROM4 CTSI counts quarter-symbols from the 13 MHz reference divided by
	// 12. Hardware starts with a 5000-quarter-symbol GSM frame; a later port
	// 0x0e write replaces the default reload value.
	u16 unused_delay;
	arm_compare_timer(m_frame_timer, 0, unused_delay);
}

TIMER_CALLBACK_MEMBER(nokia_dsp_c54x_device::frame_timer_expired)
{
	if (!m_reset_released)
		return;
	// Keep the original reference epoch: re-anchoring every wrap accumulates
	// rounding and can put an already-armed compare before its counter tick.

	++m_frame_timer_expiries;
	// CTSI uses the RF reference, not CCONT's independent RTC control.
	// Register 6 bit 1 is toggled by the MCU's RTC lifecycle; using it here
	// suppresses ADC service during an otherwise running native DSP boot.
	const bool enabled = !m_external_rf_receive;
	if (m_frame_timer_expiries <= 64)
		machine().logerror("rom4_frame_timer: expiry=%llu enabled=%u length=%u pc=%04x pmst=%04x ifr=%04x imr=%04x t=%.6f\n",
				m_frame_timer_expiries, enabled, m_slot_frame_length ? m_slot_frame_length : 4999,
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
				machine().time().as_double());
	if (enabled)
	{
		m_cpu->set_input_line(0, HOLD_LINE); // CTSI/GSM frame interrupt
	}
	arm_frame_timer();
}

void nokia_dsp_c54x_device::arm_slot_timer(u16 quarter_symbols)
{
	m_slot_compare_written = true;
	arm_compare_timer(m_slot_timer, quarter_symbols, m_slot_delay);
}

void nokia_dsp_c54x_device::arm_transfer_timer(u16 quarter_symbols)
{
	m_transfer_compare_written = true;
	arm_compare_timer(m_transfer_timer, quarter_symbols, m_transfer_delay);
}

void nokia_dsp_c54x_device::arm_compare_timer(emu_timer *timer, u16 quarter_symbols, u16 &delay)
{
	// ROM4 writes absolute event positions, not relative delays. Its 15000
	// sentinel cannot match a counter with the ordinary 4999 reload.
	if (!m_reset_released || quarter_symbols >= frame_period())
	{
		timer->adjust(attotime::never);
		return;
	}
	// Equality on a newly written compare is scheduled at the next wrap;
	// sub-quarter-symbol write/match ordering remains unvalidated.
	using namespace nokia_dsp_compare_clock;
	auto const edge = nokia_dsp_compare_clock::next((machine().time() - m_frame_counter_epoch).as_ticks(reference_hz),
		m_frame_counter_base, frame_period(), quarter_symbols);
	delay = edge.quarters;
	attotime const absolute(edge.reference_ticks / reference_hz,
		fractional_attoseconds(edge.reference_ticks));
	timer->adjust(m_frame_counter_epoch + absolute - machine().time());
}

attotime nokia_dsp_c54x_device::lab_counter_deadline(u16 target) const
{
	if (!m_reset_released || target>=frame_period()) return attotime::never;
	using namespace nokia_dsp_compare_clock;
	auto const edge=nokia_dsp_compare_clock::next((machine().time()-m_frame_counter_epoch).as_ticks(reference_hz),
		m_frame_counter_base,frame_period(),target);
	return m_frame_counter_epoch+attotime(edge.reference_ticks/reference_hz,fractional_attoseconds(edge.reference_ticks));
}

TIMER_CALLBACK_MEMBER(nokia_dsp_c54x_device::transfer_timer_expired)
{
	if (!m_reset_released) return;
	u16 unused_delay;
	arm_compare_timer(m_transfer_timer, m_io[4], unused_delay);
	++m_transfer_timer_expiries;
	u64 const tuple = (u64(m_io[0x30]) << 48) | (u64(m_io[0x31]) << 32)
		| (u64(m_io[0x32]) << 16) | m_io[4];
	bool const was_pending = m_transfer_request_pending;
	m_transfer_compare_cb(tuple);
	if (machine().options().verbose() && m_transfer_timer_expiries <= 65536)
		machine().logerror("dsp_compare_output: sequence=%llu control=%04x low=%04x high=%04x target=%04x counter=%04x pending_before=%u t=%.9f\n",
			m_transfer_timer_expiries, m_io[0x30], m_io[0x31], m_io[0x32], m_io[4],
			frame_counter(), was_pending, machine().time().as_double());
	else if (machine().options().verbose() && m_transfer_timer_expiries == 65537)
		machine().logerror("dsp_compare_output: overflow=1\n");
	if (m_transfer_timer_expiries <= 256)
		machine().logerror("rom4_transfer_timer: expiry=%llu target=%04x counter=%04x delay=%u t=%.9f\n",
			m_transfer_timer_expiries, m_io[4], frame_counter(), m_transfer_delay, machine().time().as_double());
	m_transfer_request_pending = true;
	update_host_command_line();
}

TIMER_CALLBACK_MEMBER(nokia_dsp_c54x_device::slot_timer_expired)
{
	if (!m_reset_released)
		return;
	u16 unused_delay;
	arm_compare_timer(m_slot_timer, m_io[0x0f], unused_delay);
	++m_slot_timer_expiries;
	if (m_slot_timer_expiries <= 16)
		machine().logerror("rom4_slot_timer: expiry=%llu delay=%u pc=%04x ifr=%04x imr=%04x t=%.6f\n",
				m_slot_timer_expiries, m_slot_delay,
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
				machine().time().as_double());
	// Keep the existing modeled slot request separate from the MCU's packed
	// parameter payload. This is peripheral pending state, not a DARAM store.
	m_slot_request_pending = true;
	update_host_command_line();
}

void nokia_dsp_c54x_device::reset_line_w(int released)
{
	if (bool(released) != m_reset_released)
		machine().logerror("native_dsp_reset_line: released=%u t=%.9f context=%s\n",
				bool(released), machine().time().as_double(), machine().describe_context());
	// MAD2 holds the DSP core in reset while leaving its shared/on-chip DARAM
	// intact. Releasing the line restarts at the mask-ROM reset vector.
	m_frame_counter_base = frame_counter();
	m_frame_counter_epoch = machine().time();
	m_reset_released = released;
	m_rf_reset_cb(released);
	if (!released) m_slot_request_pending = m_transfer_request_pending = false;
	m_host_command_line = false;
	m_host_command_vector = 0;
	m_cpu->set_input_line(INPUT_LINE_RESET,
			released ? CLEAR_LINE : ASSERT_LINE);
	if (released)
	{
		arm_frame_timer();
		if (m_slot_compare_written)
			arm_slot_timer(m_io[0x0f]);
		if (m_transfer_compare_written)
			arm_transfer_timer(m_io[4]);
		update_host_command_line();
	}
	else
	{
		if (m_external_rf_receive) m_rf_control_cb(0x21, 0);
		m_bsp.reset();
		m_port36.reset();
		m_port36_timer->adjust(attotime::never);
		m_cobba->codec_serial_receive_ack();
		m_frame_timer->adjust(attotime::never);
		m_slot_timer->adjust(attotime::never);
		m_transfer_timer->adjust(attotime::never);
	}
}

void nokia_dsp_c54x_device::rf_data_available_w(int state)
{
	if (state && m_external_rf_receive && m_reset_released)
		m_cpu->set_input_line(0, HOLD_LINE);
}

TIMER_CALLBACK_MEMBER(nokia_dsp_c54x_device::port36_complete)
{
	if (m_reset_released && m_port36_handshake)
		m_port36.complete();
}

bool nokia_dsp_c54x_device::overlay_address(u16 address) const
{
	// The overlay extent belongs to the configured DSP memory geometry.
	return BIT(m_cpu->state_int(tms320c54x_device::STATE_PMST), 5) &&
			address >= 0x0080 && address < m_overlay_end;
}

u16 nokia_dsp_c54x_device::program_r(offs_t offset)
{
	const u16 address = offset;
	return overlay_address(address) ? data_r(address) : m_program[address];
}

void nokia_dsp_c54x_device::program_w(offs_t offset, u16 data)
{
	const u16 address = offset;
	if (overlay_address(address))
		data_w(address, data);
}

u16 nokia_dsp_c54x_device::data_r(offs_t offset)
{
	const u16 address = offset;
	if (address == 0x0020)
		return machine().side_effects_disabled() ? m_bsp.receive : m_bsp.receive_r();
	if (address == 0x0021)
		return m_bsp.transmit;
	if (address == 0x0022)
		return m_bsp.control_r();
	if (address >= nokia_dspif_device::hpi_daram_base &&
			address < nokia_dspif_device::hpi_daram_base +
				nokia_dspif_device::hpi_daram_words)
		return m_transport->dsp_data_r(address);
	return m_data[address];
}

bool nokia_dsp_c54x_device::codec_transmit_frame(u16 &word)
{
	bool ready_edge;
	const bool transferred = m_bsp.frame_transmit(word, &ready_edge, m_data[0x0023]);
	// Underrun repeats DXR without another XRDY transition (SPRU131G 9.2.4).
	if (ready_edge)
		m_cpu->set_input_line(5, HOLD_LINE); // BXINT0, vector 21.
	return transferred;
}

bool nokia_dsp_c54x_device::codec_receive_frame(u16 word)
{
	const bool accepted = m_bsp.frame_receive(word, m_data[0x0023]);
	if (accepted)
		m_cpu->set_input_line(4, HOLD_LINE); // BRINT0, vector 20.
	return accepted;
}

tms320c54x_bsp_state::transmit_clock_result nokia_dsp_c54x_device::codec_transmit_clock_w(bool clock, bool sync)
{
	const auto result = m_bsp.transmit_clock(clock, sync, m_data[0x0023]);
	if (result.transmit_ready_edge)
		m_cpu->set_input_line(5, HOLD_LINE);
	return result;
}

bool nokia_dsp_c54x_device::codec_receive_clock_w(bool clock, bool sync, bool data)
{
	const auto result = m_bsp.receive_clock(clock, sync, data, m_data[0x0023]);
	if (result.receive_ready_edge)
		m_cpu->set_input_line(4, HOLD_LINE);
	return result.supported;
}

bool nokia_dsp_c54x_device::codec_duplex_frame(u16 input, u16 &output)
{
	const auto result = m_bsp.duplex_frame(input, output, m_data[0x0023]);
	if (result.transmit_ready_edge)
		m_cpu->set_input_line(5, HOLD_LINE);
	if (result.receive_ready_edge)
		m_cpu->set_input_line(4, HOLD_LINE);
	return result.transmitted;
}

void nokia_dsp_c54x_device::data_w(offs_t offset, u16 data)
{
	const u16 address = offset;
	if (address == 0x0022)
	{
		// Resident reset/release and BDXR/BDRR traffic corroborate BSPC0,
		// not a COBBA parallel register-C write. Status bits belong to the
		// completed-word state, not the firmware's writable control value.
		const bool was_ready = m_bsp.transmit_ready;
		m_bsp.control_w(data);
		// XRST=0 while XRDY=0 creates XINT (SPRU131G table 9-5).
		if (!was_ready && m_bsp.transmit_ready)
			m_cpu->set_input_line(5, HOLD_LINE);
		return;
	}
	if (address == 0x0032)
	{
		// Resident stores use serial-control-shaped C008/C0C8 values.
		// The MAD2 peripheral identity is unresolved; do not reinterpret
		// these as COBBA MFI register-C payloads.
		m_data[address] = data;
		return;
	}
	if (address >= 0xb000 && address <= 0xefff)
	{
		// This address range is the C54x data mask ROM. Writes are bus-visible
		// no-ops; allowing them to alter the backing array corrupts resident
		// dispatch tables and constants with transient response data.
		return;
	}
	if (address == 0x0021)
	{
		m_bsp.transmit_w(data);
		// Preserve the existing untimed external boot echo only. Ordinary
		// pending samples need a real external frame attachment, not this path.
		u16 word;
		if (!m_external_codec_clock && m_cobba->codec_serial_loopback() && codec_transmit_frame(word))
		{
			m_cobba->codec_serial_transmit(word);
			if (m_cobba->codec_serial_receive_ready())
			{
				codec_receive_frame(m_cobba->codec_serial_receive());
				m_cobba->codec_serial_receive_ack();
			}
		}
		return;
	}
	const bool shared = address >= nokia_dspif_device::hpi_daram_base &&
			address < nokia_dspif_device::hpi_daram_base +
				nokia_dspif_device::hpi_daram_words;
	const u16 old_data = shared ? m_transport->dsp_data_r(address) : m_data[address];
	if (address == 0x0866 && data != old_data)
		machine().logerror("rom4_work_flags: pc=%04x old=%04x new=%04x t=%.6f\n",
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), old_data, data,
				machine().time().as_double());
	if (shared)
		m_transport->dsp_data_w(address, data);
	else
		m_data[address] = data;
	if (address == 0x0029 && BIT(old_data ^ data, 3))
		m_transport->service_irq_w(BIT(data, 3));
	// The DSP publishes MCU-bound packets by writing the receive-ring producer
	// last. This edge is the physical FIQ0 notification; the shared words alone
	// are not polled by the MCU.
	if (shared && address == nokia_dspif_device::hpi_daram_base + (0x1c8 / 2) &&
			data != old_data)
		m_transport->notify_rx();
	if (address >= 0x0852 && address <= 0x0855)
		update_host_command_line();
}

void nokia_dsp_c54x_device::mcu_shared_write(u16 byte_offset)
{
	if (byte_offset == 0x00fe || byte_offset == 0x0100)
		++m_boot_mailbox_writes;
	if (byte_offset >= 0x0a4 && byte_offset <= 0x0aa)
		update_host_command_line();
}

void nokia_dsp_c54x_device::tx_commit_w(int state)
{
	// MDISND has no separate doorbell. The producer write changes the live
	// ring-not-empty comparator presented on INT2; mcu_shared_write observes the
	// same write and updates that level after the transport has stored it.
	// HPINT belongs only to doorbell_w: asserting it here also enters the
	// parameter handler without a parameter transaction or its return context.
	if (state && m_reset_released)
		update_host_command_line();
}

u16 nokia_dsp_c54x_device::host_request() const
{
	const u16 tx_producer = m_transport->shared_word(0x0a4 / 2);
	const u16 tx_consumer = m_transport->shared_word(0x0a6 / 2);
	return (m_slot_request_pending ? 0x0008 : 0) | (m_transfer_request_pending ? 0x1000 : 0) |
			(tx_producer != tx_consumer ? 0x0002 : 0x0000);
}

void nokia_dsp_c54x_device::acknowledge_host_command(u16 mask)
{
	if (BIT(mask, 3)) m_slot_request_pending = false;
	if (BIT(mask, 12)) m_transfer_request_pending = false;
	// MCU parameter delivery uses its explicit HPINT doorbell. Peripheral
	// acceptance must not reinterpret or clear packed DARAM parameter bits.
}

void nokia_dsp_c54x_device::update_host_command_line()
{
	if (!m_reset_released)
	{
		m_host_command_line = false;
		m_host_command_vector = 0;
		return;
	}
	// Bit 1 is a live ring-not-empty comparator, not an acknowledgeable latch.
	// Firmware may leave bit 1 set in the in-service mask; it must not suppress
	// a later empty-to-nonempty MDISND transition.
	const u16 acknowledgement = m_transport->shared_word(0x0aa / 2) & ~u16(0x0002);
	// Only modeled peripheral sources drive INT2: independent compare latches
	// and the live MDISND comparator. Other ROM dispatcher bits need independently
	// evidenced producers; packed MCU parameter data is not a request bitmap.
	const u16 vector = host_request() & ~acknowledgement;
	const bool level = vector != 0;
	// Re-arm the CPU latch when the effective request changes. Command work and
	// an MDISND commit can overlap without producing an intervening low edge,
	// but are distinct requests to the resident dispatcher.
	if (level && vector != m_host_command_vector)
		m_cpu->set_input_line(2, ASSERT_LINE);
	else if (!level)
		m_cpu->set_input_line(2, CLEAR_LINE);
	m_host_command_line = level;
	m_host_command_vector = vector;
}

u16 nokia_dsp_c54x_device::io_r(offs_t offset)
{
	if (m_port36_handshake && (offset & 0xff) == 0x36)
		return m_port36.read();
	switch (offset & 0xff)
	{
	case 0x01:
		return host_request();
	case 0x02:
		return m_transport->shared_word(0x0aa / 2);
	case 0x0d:
		return frame_counter();
	case 0x27:
		if (machine().side_effects_disabled())
			return m_external_rf_receive ? m_rf_receive_cb() : m_cobba->rf_receive_sample();
		if (m_rf_trace_count++ < 16)
			machine().logerror("rom4_rf_read: sample=%u pc=%04x frame=%llu t=%.6f\n",
					m_rf_trace_count,
					u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					m_frame_timer_expiries,
					machine().time().as_double());
		return m_external_rf_receive ? m_rf_receive_cb() : m_cobba->rf_receive_sample();
	case 0x38:
		++m_rf_port38_reads;
		return m_fire.status_r();
	case 0x39:
		++m_rf_port39_reads;
		return m_fire.data_r(m_io[0x39],!machine().side_effects_disabled());
	case 0x2d:
		return m_cobba->control_data_r();
	default:
		// Resident ROM4 accesses I/O 21 as a mask/write-back word. Keep its
		// opaque saved state separate from data-space BDXR/BDRR self-test echo;
		// operational field meanings and hardware status transitions are open.
		return m_io[offset & 0xff];
	}
}

void nokia_dsp_c54x_device::io_w(offs_t offset, u16 data)
{
	const u8 port = offset & 0xff;
	if (port == 0x03 || port == 0x04 || port == 0x0e || port == 0x0f)
		machine().logerror("rom4_timing_port: port=%02x data=%04x pc=%04x t=%.6f\n",
				port, data, u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				machine().time().as_double());
	if (m_io_trace_count++ < 256)
		machine().logerror("rom4_port_write: port=%02x data=%04x pc=%04x t=%.6f\n",
				port, data, u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				machine().time().as_double());
	if (m_port36_handshake && port == 0x36)
	{
		if (m_port36.write(data)) m_port36_timer->adjust(attotime::never);
		return; // Selected control state owns this word, not the opaque bank.
	}
	m_io[port] = data;
	if (port == 0x38) m_fire.command_w(data);
	else if (port == 0x39) m_fire.data_w(data);
	else if (port == 0x3c) m_fire.control_w(data);
	if (m_external_rf_receive && (port == 3 || (port >= 5 && port <= 8) || port == 0x21))
		m_rf_control_cb(port, data);
	if (m_port36_handshake && port == 0 && m_port36.strobe_w(BIT(data, 4)))
		// Deferred completion, not a read-triggered answer. One backend clock
		// is an unmeasured latency policy, not an identified peripheral rate.
		m_port36_timer->adjust(attotime::from_ticks(1, clock()));
	if (port == 0x02)
	{
		// Acceptance clears the independent slot latch, not parameter data.
		// The ring comparator retires only when its consumer advances. The
		// in-service mask's shared-0aa projection remains a partial model.
		m_transport->peer_shared_w(0x0aa / 2, data);
		acknowledge_host_command(data);
		update_host_command_line();
	}
	else if (port == 0x01)
	{
		// Port 1 writes are DSP status/completion strobes, not writes back to
		// the request latch. Retain the RX doorbell while higher-bit routing
		// is unresolved; the MCU checks whether its receive ring contains work.
		acknowledge_host_command(data);
		++m_completion_strobes;
		update_host_command_line();
		m_transport->notify_rx();
		// Resident TX completion publishes bit 1 after ring consumption;
		// MAD2 FIQ1 wakes the MCU's pending-transmit task independently of RX.
		if (BIT(data, 1))
			m_transport->notify_tx();
	}
	else if (port == 4)
	{
		// Research hypothesis: port 4 schedules the queued-transfer compare
		// serviced by request bit 1000. Fine match phase remains unmeasured.
		arm_transfer_timer(data);
	}
	else if (port == 0x27)
	{
		m_cobba->rf_transmit_sample(data);
	}
	else if (port == 0x0e)
	{
		m_frame_counter_base = frame_counter();
		// Reload changes the modulus, not the running 13MHz/12 divider.
		// Keep the unconsumed fraction; re-anchoring at this MMIO write
		// otherwise delays an armed compare even when data is unchanged.
		using namespace nokia_dsp_compare_clock;
		if (m_reset_released)
		{
			u64 const completed = ((machine().time()-m_frame_counter_epoch).as_ticks(reference_hz)/12)*12;
			m_frame_counter_epoch += attotime(completed/reference_hz,fractional_attoseconds(completed));
		}
		else
			m_frame_counter_epoch = machine().time();
		m_slot_frame_length = data;
		m_frame_counter_base %= frame_period();
		if (m_reset_released)
			arm_frame_timer();
		if (m_slot_compare_written)
			arm_slot_timer(m_io[0x0f]);
		if (m_transfer_compare_written)
			arm_transfer_timer(m_io[4]);
	}
	else if (port == 0x0d)
	{
		m_frame_counter_base = (data & 0x3fff) % frame_period();
		m_frame_counter_epoch = machine().time();
		if (m_reset_released)
			arm_frame_timer();
		if (m_slot_compare_written)
			arm_slot_timer(m_io[0x0f]);
		if (m_transfer_compare_written)
			arm_transfer_timer(m_io[4]);
	}
	else if (port == 0x0f)
	{
		arm_slot_timer(data);
	}
	else if (port == 0x2c)
	{
		m_cobba->control_select_w(data);
	}
	else if (port == 0x2d)
	{
		m_cobba->control_data_w(data);
	}
	else if (port == 0x31)
	{
		m_rf_port31 = data;
	}
	else if (port == 0x32)
	{
		m_rf_port32 = data;
		++m_rf_port32_writes;
		if (m_rf_port32_writes <= 32)
			machine().logerror(
					"rom4_rf_port32: sequence=%u port31=%04x port32=%04x pc=%04x owner=%06x t=%.6f\n",
					m_rf_port32_writes, m_rf_port31, m_rf_port32,
					u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_EXECPC)),
					machine().time().as_double());
	}
	if (m_external_rf_receive && (port==9 || port==0x0d || port==0x0e)) m_rf_control_cb(port,data);
}

void nokia_dsp_c54x_device::doorbell_w(int state)
{
	if (state)
		m_cpu->set_input_line(9, HOLD_LINE); // C54x HPINT, vector 25
}

void nokia_dsp_c54x_device::program_map(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(nokia_dsp_c54x_device::program_r),
			FUNC(nokia_dsp_c54x_device::program_w));
}

void nokia_dsp_c54x_device::data_map(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(nokia_dsp_c54x_device::data_r),
			FUNC(nokia_dsp_c54x_device::data_w));
}

void nokia_dsp_c54x_device::io_map(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(nokia_dsp_c54x_device::io_r),
			FUNC(nokia_dsp_c54x_device::io_w));
}
