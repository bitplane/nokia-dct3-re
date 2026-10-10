// license:BSD-3-Clause
// copyright-holders:Gaz
#include "tms320c54x_bsp_state.h"
#include <cassert>

int main()
{
	tms320c54x_bsp_state port;
	std::uint16_t duplex_output = 0xaaaa;
	port.control_w(0x00c2);
	// Released DLB without loaded TX/frame cannot manufacture RX readiness.
	auto frame = port.duplex_frame(0x5555, duplex_output);
	assert(!frame.transmitted && !frame.transmit_ready_edge && !frame.receive_ready_edge);
	assert(!port.receive_ready);
	port.transmit_w(0x1234);
	assert(!port.receive_ready); // Writing DXR does not advance either clock.
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(frame.transmitted && frame.transmit_ready_edge && frame.receive_ready_edge);
	assert(port.receive == 0x1234 && duplex_output == 0x1234);
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(frame.transmitted && !frame.transmit_ready_edge && !frame.receive_ready_edge);
	assert(port.receive_overrun && port.receive_r() == 0x1234);
	port.control_w(0x00c0);
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(frame.receive_ready_edge && port.receive_r() == 0x5555);
	port.control_w(0x0042); // DLB with receiver held in reset.
	port.transmit_w(0xf234);
	frame = port.duplex_frame(0, duplex_output);
	assert(frame.transmitted && frame.transmit_ready_edge && !frame.receive_ready_edge);
	assert(!port.receive_ready);
	port.control_w(0x0082); // Transmitter reset cannot drive a looped frame.
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(!frame.transmitted && !frame.transmit_ready_edge && !frame.receive_ready_edge);
	port.control_w(0x00c2);
	port.transmit_w(0x6789);
	auto duplex_saved = port;
	const auto original_frame = port.duplex_frame(0x5555, duplex_output);
	std::uint16_t restored_output = 0;
	const auto restored_frame = duplex_saved.duplex_frame(0x5555, restored_output);
	assert(original_frame.transmitted == restored_frame.transmitted);
	assert(original_frame.transmit_ready_edge == restored_frame.transmit_ready_edge);
	assert(original_frame.receive_ready_edge == restored_frame.receive_ready_edge);
	assert(duplex_output == restored_output && port.control_r() == duplex_saved.control_r());
	assert(port.receive_r() == duplex_saved.receive_r());
	port.reset();
	assert(port.control_r() == 0x0800);
	for (unsigned value = 0; value <= 0xffff; ++value)
	{
		port.reset();
		port.control_w(value);
		assert(port.control_r() == ((value & 0xc0ff) | 0x0800));
	}
	port.reset();
	assert(!port.frame_receive(1));
	port.control_w(0x00c0);
	assert(port.frame_receive(0x1234));
	assert(port.control_r() == 0x0cc0);
	assert(!port.frame_receive(0x5678));
	assert(port.control_r() == 0x2cc0);
	assert(!port.frame_receive(0x9999));
	assert(port.receive_r() == 0x1234);
	assert(port.control_r() == 0x08c0);
	assert(port.receive_r() == 0x1234);
	assert(port.frame_receive(0x5678));
	port.control_w(0x0040);
	assert(!port.receive_ready && !port.receive_overrun);
	assert(port.receive_r() == 0x5678);
	port.transmit_w(0xabcd);
	assert(port.transmit_pending && !port.transmit_ready);
	std::uint16_t output = 0;
	bool ready_edge = false;
	auto saved = port;
	assert(port.frame_transmit(output, &ready_edge) && output == 0xabcd);
	assert(ready_edge);
	assert(port.transmit_ready && !port.transmit_pending);
	assert(port.frame_transmit(output, &ready_edge) && output == 0xabcd);
	assert(!ready_edge);
	assert(port.transmit_ready && !port.transmit_pending);
	assert(saved.frame_transmit(output) && output == 0xabcd);
	assert(saved.control_r() == port.control_r());
	port.transmit_w(2);
	port.transmit_w(3);
	assert(port.frame_transmit(output) && output == 3);
	assert(port.frame_transmit(output) && output == 3);
	assert(port.frame_transmit(output) && output == 3);
	port.transmit_w(4);
	port.control_w(0);
	assert(port.transmit_ready && !port.transmit_pending);
	ready_edge = true;
	assert(!port.frame_transmit(output, &ready_edge));
	assert(!ready_edge);
	port.control_w(0x0040);
	assert(!port.frame_transmit(output));
	port.reset();
	assert(port.control_r() == 0x0800 && port.receive == 0 && port.transmit == 0);
	for (unsigned value = 0; value <= 0xffff; ++value)
	{
		for (unsigned format = 0; format != 4; ++format)
		{
			const unsigned bits[] = {16, 8, 10, 12};
			const unsigned width = bits[format];
			const std::uint16_t extension = format & 2 ? 0x0080 : 0;
			const unsigned mask = (1U << width) - 1;
			port.reset();
			port.control_w(0x00c0 | (format & 1 ? 0x0004 : 0));
			assert(port.word_bits(extension) == width);
			port.transmit_w(value);
			assert(port.frame_transmit(output, &ready_edge, extension));
			assert(output == (value & mask) && ready_edge);
			assert(port.frame_transmit(output, &ready_edge, extension) && !ready_edge);
			assert(port.frame_receive(value, extension));
			const unsigned payload = value & mask;
			const auto expected = std::uint16_t(payload & (1U << (width - 1))
					? payload | ~mask : payload);
			assert(port.receive_r() == expected);
			port.control_w(port.control | 2);
			port.transmit_w(value);
			const auto looped = port.duplex_frame(~value, output, extension);
			assert(looped.transmitted && looped.transmit_ready_edge && looped.receive_ready_edge);
			assert(output == (value & mask) && port.receive_r() == expected);
		}
		port.reset();
		port.control_w(0x00c4); // 8-bit BSP format.
		port.transmit_w(value);
		assert(port.transmit == value);
		assert(port.frame_transmit(output) && output == (value & 0xff));
		assert(port.frame_receive(value));
		const unsigned expected = value & 0x80 ? (value | 0xff00) : (value & 0xff);
		assert(port.receive_r() == expected);
		port.control_w(0x00c0);
		port.transmit_w(value);
		assert(port.frame_transmit(output) && output == value);
		assert(port.frame_receive(value));
		assert(port.receive_r() == value);
	}
}
