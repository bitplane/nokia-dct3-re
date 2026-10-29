// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "nokia_dspif.h"
#include <array>
#include <sstream>

namespace {

class dspif_test_state : public driver_device
{
public:
	dspif_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_transport(*this, "dspif") { }
	void test(machine_config &config) { NOKIA_DSPIF(config, m_transport, 0); }
protected:
	void machine_start() override { m_timer = timer_alloc(FUNC(dspif_test_state::check), this); }
	void machine_reset() override { m_timer->adjust(attotime::from_usec(1)); }
private:
	void require(bool condition, const char *message)
	{
		if (!condition) throw emu_fatalerror("DSPIF state fixture: %s", message);
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		// Relocated from the CPU repeat fixture: no CPU or signal connection
		// participated in this transport-state preservation contract.
		constexpr u8 payload[] = {0x12, 0x34, 0x56, 0x78};
		m_transport->peer_shared_w(0x1c8 / 2, 0x100 / 2);
		m_transport->peer_shared_w(0x1ca / 2, 0x100 / 2);
		require(m_transport->enqueue_rx_packet(0x83, payload, sizeof(payload)) &&
			m_transport->enqueue_rx_packet(0x89, payload, sizeof(payload)), "queue both RX packets");
		m_transport->dspif_w(0, 0x5a);
		std::array<u16, 0x800> before;
		for (unsigned i = 0; i < before.size(); ++i) before[i] = m_transport->shared_word(i);
		std::stringstream saved;
		require(machine().save().write_stream(saved) == STATERR_NONE, "capture queued transport");
		for (unsigned i = 0; i < before.size(); ++i) m_transport->peer_shared_w(i, 0);
		m_transport->dspif_w(0, 0);
		saved.seekg(0);
		require(machine().save().read_stream(saved) == STATERR_NONE, "restore queued transport");
		unsigned checked = 0;
		for (unsigned i = 0; i < before.size(); ++i)
		{
			require(m_transport->shared_word(i) == before[i], "restore RAM, queued payloads and ring cursors");
			++checked;
		}
		require(m_transport->dspif_r(0) == 0x5a, "restore interface register");
		osd_printf_info("Nokia DSPIF state: PASS ram_words=%u queued_packets=2 registers=1 save_replay=1 cpu=0 firmware=0\n", checked);
		throw emu_fatalerror(3, "Nokia DSPIF state fixture complete");
	}
	required_device<nokia_dspif_device> m_transport;
	emu_timer *m_timer = nullptr;
};

ROM_START(dspiftst)
ROM_END

} // anonymous namespace

SYST(2026, dspiftst, 0, 0, test, 0, dspif_test_state, empty_init,
	"Test", "Nokia DSPIF state preservation", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
