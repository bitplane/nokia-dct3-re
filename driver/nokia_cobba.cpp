// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_cobba.h"

DEFINE_DEVICE_TYPE(NOKIA_COBBA, nokia_cobba_device,
		"nokia_cobba", "Nokia COBBA audio codec")

nokia_cobba_device::nokia_cobba_device(
		const machine_config &mconfig, const char *tag,
		device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_COBBA, tag, owner, clock),
	device_sound_interface(mconfig, *this),
	m_rf_receive_cb(*this, 0)
{
}

void nokia_cobba_device::device_start()
{
	m_trace_enabled = machine().options().verbose();
	// The three differential microphone inputs and two audio outputs are
	// separate physical COBBA pins. PCM conversion and route selection remain
	// inside COBBA.
	m_stream = stream_alloc(microphone_inputs, audio_outputs, pcm_rate);
	m_hle_microphone_gain =
			std::pow(10.0F, m_hle_microphone_gain_db / 20.0F);
	m_hle_output_gain = std::pow(10.0F, m_hle_output_gain_db / 20.0F);
	save_item(NAME(m_earpiece_queue));
	save_item(NAME(m_microphone_queue));
	save_item(NAME(m_earpiece_head));
	save_item(NAME(m_earpiece_count));
	save_item(NAME(m_microphone_head));
	save_item(NAME(m_microphone_count));
	save_item(NAME(m_earpiece_blocks));
	save_item(NAME(m_microphone_blocks));
	save_item(NAME(m_earpiece_overruns));
	save_item(NAME(m_microphone_overruns));
	save_item(NAME(m_microphone_underruns));
	save_item(NAME(m_pcm_sample_bits));
	save_item(NAME(m_hle_microphone));
	save_item(NAME(m_hle_output));
	save_item(NAME(m_control_registers));
	save_item(NAME(m_control_data_latch));
	save_item(NAME(m_control_address));
	save_item(NAME(m_control_read));
	save_item(NAME(m_control_transactions));
	save_item(NAME(m_control_reads));
	save_item(NAME(m_control_writes));
	save_item(NAME(m_parallel_registers));
	save_item(NAME(m_parallel_writes));
	save_item(NAME(m_codec_serial_receive_latch));
	save_item(NAME(m_codec_serial_receive_ready));
	save_item(NAME(m_codec_serial_loopbacks));
	save_item(NAME(m_rf_receive_samples));
}

void nokia_cobba_device::device_reset()
{
	m_stream->update();
	m_earpiece_head = 0;
	m_earpiece_count = 0;
	m_microphone_head = 0;
	m_microphone_count = 0;
	m_earpiece_blocks = 0;
	m_microphone_blocks = 0;
	m_earpiece_overruns = 0;
	m_microphone_overruns = 0;
	m_microphone_underruns = 0;
	std::fill(m_control_registers.begin(), m_control_registers.end(), 0);
	// ROM4's self-test reads COBBA analog measurement registers 5 and 6.
	// These are input measurements supplied by the codec, not synthesized test
	// results; the nominal no-signal values keep both readings in their accepted
	// hardware ranges.
	m_control_registers[0x05] = 0x0160;
	m_control_registers[0x06] = 0x0010;
	// Recovered ROM4 multi-register transactions wait for status register D
	// bits 1:0 clear and bits 3:2 set between transfers.
	m_control_registers[0x0d] = 0x000c;
	m_control_data_latch = 0;
	m_control_address = 0;
	m_control_read = false;
	m_control_transactions = 0;
	m_control_reads = 0;
	m_control_writes = 0;
	std::fill(m_parallel_registers.begin(), m_parallel_registers.end(), 0);
	m_parallel_writes = 0;
	m_codec_serial_receive_latch = 0;
	m_codec_serial_receive_ready = false;
	m_codec_serial_loopbacks = 0;
	m_rf_receive_samples = 0;
}

u16 nokia_cobba_device::rf_receive_sample()
{
	// ROM4 consumes interleaved signed I/Q components, one per port read.
	// An unattached RF front end is deterministic silence; all filtering and
	// measurement decisions remain inside the emulated DSP.
	++m_rf_receive_samples;
	return m_rf_receive_cb();
}

void nokia_cobba_device::parallel_control_w(u16 frame)
{
	const u8 address = frame >> 12;
	const u16 data = frame & 0x0fff;
	m_parallel_registers[address] = data;
	++m_parallel_writes;
	if (m_trace_enabled || m_parallel_writes <= 16)
		machine().logerror("cobba: parallel sequence=%llu address=%x data=%03x t=%.6f\n",
				m_parallel_writes, address, data,
				machine().time().as_double());
}

void nokia_cobba_device::control_data_w(u16 data)
{
	m_control_data_latch = data & 0x0fff;
}

void nokia_cobba_device::control_select_w(u16 select)
{
	m_control_address = select & 0x0f;
	m_control_read = BIT(select, 4);
	++m_control_transactions;
	if (!m_control_read)
	{
		m_control_registers[m_control_address] = m_control_data_latch;
		++m_control_writes;
	}
	else
	{
		++m_control_reads;
	}
	if (m_trace_enabled)
		machine().logerror(
				"cobba: control sequence=%llu direction=%s address=%x data=%03x t=%.6f\n",
				m_control_transactions, m_control_read ? "read" : "write",
				m_control_address,
				m_control_read ? m_control_registers[m_control_address] :
						m_control_data_latch,
				machine().time().as_double());
}

u16 nokia_cobba_device::control_data_r() const
{
	// Bit 12 is the observed busy flag. This untimed model completes the
	// transfer immediately, so it remains clear on readback.
	return m_control_registers[m_control_address] & 0x0fff;
}

bool nokia_cobba_device::codec_serial_loopback() const
{
	// ROM4 self-test reads register 8, sets 0x0610, then enables the C54x
	// buffered serial-port receiver/transmitter and expects each transmitted
	// word at the receive pin. It clears the same three bits afterwards.
	// Individual field meanings are not inferred from this one transaction.
	return (m_control_registers[0x08] & 0x0610) == 0x0610;
}

void nokia_cobba_device::codec_serial_transmit(u16 data)
{
	if (!codec_serial_loopback())
		return;

	m_codec_serial_receive_latch = data;
	m_codec_serial_receive_ready = true;
	++m_codec_serial_loopbacks;
	if (m_trace_enabled)
		machine().logerror(
				"cobba: codec serial loopback data=%04x count=%llu t=%.6f\n",
				data, m_codec_serial_loopbacks, machine().time().as_double());
}

u8 nokia_cobba_device::run_control_conformance_checks()
{
	const auto saved_registers = m_control_registers;
	const u16 saved_latch = m_control_data_latch;
	const u8 saved_address = m_control_address;
	const bool saved_read = m_control_read;
	const u64 saved_transactions = m_control_transactions;
	const u64 saved_reads = m_control_reads;
	const u64 saved_writes = m_control_writes;
	const u16 saved_codec_receive = m_codec_serial_receive_latch;
	const bool saved_codec_ready = m_codec_serial_receive_ready;
	const u64 saved_codec_loopbacks = m_codec_serial_loopbacks;
	u8 result = 0;

	// Reset exposes the nominal analog inputs and recovered idle handshake.
	bool reset_state = m_control_registers[0x05] == 0x0160 &&
			m_control_registers[0x06] == 0x0010 &&
			m_control_registers[0x0d] == 0x000c;
	for (u8 address = 0; address != m_control_registers.size(); ++address)
		if (address != 0x05 && address != 0x06 && address != 0x0d &&
				m_control_registers[address] != 0)
			reset_state = false;
	if (reset_state)
		result |= 0x01;

	// A write-select commits the previously latched, 12-bit payload.
	control_data_w(0xface);
	control_select_w(0x03);
	if (m_control_registers[0x03] == 0x0ace &&
			m_control_address == 0x03 && !m_control_read)
		result |= 0x02;

	// A read-select changes the addressed readback without committing the
	// current data latch. A later write-select commits that retained latch.
	control_data_w(0x0123);
	control_select_w(0x13);
	const bool read_non_destructive =
			control_data_r() == 0x0ace &&
			m_control_registers[0x03] == 0x0ace && m_control_read;
	control_select_w(0x04);
	if (read_non_destructive && m_control_registers[0x04] == 0x0123)
		result |= 0x04;

	// Only select bit 4 and the low address nibble are part of this protocol.
	control_data_w(0x1fed);
	control_select_w(0xa5);
	const bool masked_write =
			m_control_registers[0x05] == 0x0fed &&
			m_control_address == 0x05 && !m_control_read;
	control_select_w(0xb5);
	if (masked_write && m_control_read && control_data_r() == 0x0fed)
		result |= 0x08;

	// The recovered ROM4 sequence brackets its codec serial echo with these
	// exact register-8 bits. A disabled transfer must not manufacture input.
	control_data_w(0x0610);
	control_select_w(0x08);
	codec_serial_transmit(0x0aaa);
	const bool loopback_enabled = codec_serial_receive_ready() &&
			codec_serial_receive() == 0x0aaa && m_codec_serial_loopbacks == 1;
	codec_serial_receive_ack();
	control_data_w(control_register(0x08) & 0x09ef);
	control_select_w(0x08);
	codec_serial_transmit(0x0555);
	if (loopback_enabled && !codec_serial_loopback() &&
			!codec_serial_receive_ready() && m_codec_serial_loopbacks == 1)
		result |= 0x10;

	m_control_registers = saved_registers;
	m_control_data_latch = saved_latch;
	m_control_address = saved_address;
	m_control_read = saved_read;
	m_control_transactions = saved_transactions;
	m_control_reads = saved_reads;
	m_control_writes = saved_writes;
	m_codec_serial_receive_latch = saved_codec_receive;
	m_codec_serial_receive_ready = saved_codec_ready;
	m_codec_serial_loopbacks = saved_codec_loopbacks;
	return result;
}

bool nokia_cobba_device::write_earpiece_pcm(const pcm_block &block)
{
	m_stream->update();
	if (m_earpiece_count > queue_samples - pcm_block_samples)
	{
		++m_earpiece_overruns;
		return false;
	}

	for (s16 sample : block)
	{
		m_earpiece_queue[(m_earpiece_head + m_earpiece_count) % queue_samples] = sample;
		++m_earpiece_count;
	}
	++m_earpiece_blocks;
	return true;
}

void nokia_cobba_device::read_microphone_pcm(pcm_block &block)
{
	m_stream->update();
	for (s16 &sample : block)
	{
		if (m_microphone_count)
		{
			sample = m_microphone_queue[m_microphone_head];
			m_microphone_head = (m_microphone_head + 1) % queue_samples;
			--m_microphone_count;
		}
		else
		{
			sample = 0;
			++m_microphone_underruns;
		}
	}
	++m_microphone_blocks;
}

void nokia_cobba_device::sound_stream_update(sound_stream &stream)
{
	const s32 converter_scale =
			(m_pcm_sample_bits >= 2 && m_pcm_sample_bits <= 16)
					? s32(1) << (m_pcm_sample_bits - 1)
					: 32768;
	for (int index = 0; index < stream.samples(); ++index)
	{
		if (m_microphone_count == queue_samples)
		{
			m_microphone_head = (m_microphone_head + 1) % queue_samples;
			--m_microphone_count;
			++m_microphone_overruns;
		}
		const float analogue_input =
				m_hle_microphone < microphone_inputs
						? float(stream.get(m_hle_microphone, index))
						: 0.0F;
		const float microphone = std::clamp(
				analogue_input * m_hle_microphone_gain,
				-1.0F, float(converter_scale - 1) / converter_scale);
		m_microphone_queue[
				(m_microphone_head + m_microphone_count) % queue_samples] =
				s16(microphone * converter_scale);
		++m_microphone_count;

		s16 sample = 0;
		if (m_earpiece_count)
		{
			sample = m_earpiece_queue[m_earpiece_head];
			m_earpiece_head = (m_earpiece_head + 1) % queue_samples;
			--m_earpiece_count;
		}
		for (u8 output = 0; output != audio_outputs; ++output)
			stream.put(output, index, 0.0F);
		if (m_hle_output < audio_outputs)
			stream.put(m_hle_output, index,
					float(sample) / converter_scale * m_hle_output_gain);
	}
}
