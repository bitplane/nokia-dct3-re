// license:BSD-3-Clause
// copyright-holders:Gaz
#include "tms320c54x_bsp_state.h"
#include <cassert>

int main()
{
	tms320c54x_bsp_state port;
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
	auto saved = port;
	assert(port.frame_transmit(output) && output == 0xabcd);
	assert(port.transmit_ready && !port.transmit_pending);
	assert(port.frame_transmit(output) && output == 0xabcd);
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
	assert(!port.frame_transmit(output));
	port.control_w(0x0040);
	assert(!port.frame_transmit(output));
	port.reset();
	assert(port.control_r() == 0x0800 && port.receive == 0 && port.transmit == 0);
	for (unsigned value = 0; value <= 0xffff; ++value)
	{
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
