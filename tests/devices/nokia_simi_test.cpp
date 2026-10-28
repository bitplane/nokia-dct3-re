// license:BSD-3-Clause
// copyright-holders:Gaz

// Development-only controller/card restoration, not a handset boot fixture.
#include "emu.h"
#include "nokia_simi.h"
#include "nokia_sim_card.h"
#include <sstream>
#include <cmath>

namespace {
struct write_case {
	u8 ins, p1, p2, length;
	u16 body;
	u8 sw1, sw2;
	bool checkpoint = false, restore = false;
	bool preserve_checkpoint = false;
};

class nokia_simi_test_state : public driver_device
{
public:
	nokia_simi_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_simi(*this, "simi"), m_card(*this, "card") { }
	void test(machine_config &config)
	{
		NOKIA_SIMI(config, m_simi);
		NOKIA_SIM_CARD(config, m_card);
		m_card->set_subscriber_profile(gsm::subscriber::laboratory);
		m_simi->set_enabled(true);
		m_simi->set_card_present(true);
		m_simi->card_activate_cb().set(m_card, FUNC(nokia_sim_card_device::activate_w));
		m_simi->card_tx_cb().set(m_card, FUNC(nokia_sim_card_device::rx_w));
		m_card->response_cb().set(m_simi, FUNC(nokia_simi_device::card_rx_w));
		m_simi->irq_cb().set(FUNC(nokia_simi_test_state::irq_w));
	}
private:
	required_device<nokia_simi_device> m_simi;
	required_device<nokia_sim_card_device> m_card;
	emu_timer *m_timer = nullptr;
	unsigned m_phase = 0; // Observer control is intentionally outside the saved device state.
	unsigned m_activation_checks = 0;
	unsigned m_parameter_case = 0;
	unsigned m_record_case = 0;
	unsigned m_write_case = 0;
	bool m_seek_replayed = false;
	static constexpr write_case WRITE_CASES[] = {
		{0xdc, 0, 4, 32, 0x11, 0x90, 0},
		{0xb2, 50, 4, 32, 0x11, 0x90, 0},
		{0xdc, 1, 4, 32, 0x22, 0x90, 0},
		{0xb2, 0, 4, 32, 0x11, 0x90, 0},
		{0xdc, 0, 2, 32, 0x33, 0x94, 2}, // Last linear record cannot wrap on write.
		{0xb2, 1, 4, 32, 0x22, 0x90, 0},
		{0xb2, 0, 4, 32, 0x11, 0x90, 0},
		{0xdc, 0, 3, 32, 0x44, 0x90, 0},
		{0xb2, 0, 4, 32, 0x44, 0x90, 0}, // Save record 49 and its pointer here.
		{0xdc, 0, 4, 32, 0x99, 0x90, 0}, // Restore after this deliberate overwrite.
		{0xb2, 0, 4, 32, 0x44, 0x90, 0},
		{0xdc, 0, 4, 31, 0x55, 0x67, 0},
		{0xb2, 0, 4, 32, 0x44, 0x90, 0},
		{0xb2, 50, 4, 32, 0x11, 0x90, 0},
		{0xb2, 0, 2, 32, 0x11, 0x90, 0},
		{0xdc, 0, 0, 32, 0x66, 0x6b, 0},
		{0xb2, 0, 4, 32, 0x11, 0x90, 0},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f39, 0x9f, 15},
		{0xb2, 0, 4, 3, 0x00, 0x90, 0},
		{0xdc, 0, 4, 3, 0x77, 0x6b, 0}, // Cyclic writes permit PREVIOUS only.
		{0xb2, 0, 4, 3, 0x00, 0x90, 0},
		{0xdc, 0, 2, 3, 0x88, 0x6b, 0},
		{0xb2, 0, 4, 3, 0x00, 0x90, 0},
		{0xdc, 1, 4, 3, 0x99, 0x6b, 0},
		{0xb2, 0, 4, 3, 0x00, 0x90, 0},
		{0xdc, 0, 3, 3, 0x12, 0x90, 0},
		{0xb2, 0, 4, 3, 0x12, 0x90, 0},
		{0xb2, 0, 2, 3, 0x12, 0x90, 0},
		{0xb2, 0, 3, 3, 0x12, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f7e, 0x9f, 15}, // Transparent LOCI, 11 bytes.
		{0xd6, 0, 0, 1, 0x55, 0x90, 0},
		{0xb0, 0, 0, 1, 0x55, 0x90, 0},
		{0xb0, 0x80, 0, 1, 0, 0x94, 2}, // Full high offset must not alias zero.
		{0xd6, 0x80, 0, 1, 0x77, 0x94, 2},
		{0xb0, 0, 0, 1, 0x55, 0x90, 0},
		{0xb0, 0xff, 0, 1, 0, 0x94, 2},
		{0xd6, 0xff, 0, 1, 0x88, 0x94, 2},
		{0xb0, 0, 0, 1, 0x55, 0x90, 0},
		{0xd6, 0, 10, 1, 0x66, 0x90, 0},
		{0xb0, 0, 10, 1, 0x66, 0x90, 0},
		{0xd6, 0, 11, 1, 0x99, 0x94, 2},
		{0xb0, 0, 10, 1, 0x66, 0x90, 0},
		{0xb0, 0, 11, 1, 0, 0x94, 2},
		{0xb0, 0, 0, 1, 0x55, 0x90, 0},
		{0x88, 1, 0, 16, 0x5a, 0x6b, 0},
		{0x88, 0, 1, 16, 0x5a, 0x6b, 0},
		{0x88, 0, 0, 15, 0x5a, 0x67, 0},
		{0x88, 0, 0, 17, 0x5a, 0x67, 0},
		{0x88, 0, 0, 0, 0, 0x67, 0},
		{0x88, 0, 0, 16, 0x5a, 0x9f, 12},
		{0xc0, 0, 0, 12, 0, 0x90, 0},
		{0xc0, 0, 0, 12, 0, 0x6f, 0}, // A response is consumed once.
		{0xa4, 0, 0, 2, 0x3f00, 0x9f, 22},
		{0xc0, 0, 0, 22, 0x3f00, 0x90, 0},
		{0xc0, 0, 0, 22, 0, 0x6f, 0},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xf2, 0, 0, 22, 0x7f20, 0x90, 0}, // STATUS breaks SELECT/GET RESPONSE adjacency.
		{0xc0, 0, 0, 22, 0, 0x6f, 0},
		{0xa4, 0, 0, 2, 0x6f7e, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f7e, 0x90, 0},
		{0xc0, 0, 0, 15, 0, 0x6f, 0},
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f3a, 0x9f, 15},
		{0xa2, 0, 2, 1, 0x22, 0x90, 0}, // Undefined NEXT starts at record one.
		{0xb2, 0, 4, 32, 0x22, 0x90, 0},
		{0xa2, 0, 0x12, 1, 0x44, 0x9f, 1}, // Pending result is checkpointed here.
		{0xc0, 0, 0, 1, 49, 0x90, 0},
		{0xb2, 0, 4, 32, 0x44, 0x90, 0},
		{0xa2, 0, 2, 1, 0x44, 0x94, 4},
		{0xb2, 0, 4, 32, 0x44, 0x90, 0},
		{0xa2, 0, 0x12, 1, 0x11, 0x9f, 1},
		{0xc0, 0, 0, 1, 50, 0x90, 0},
		{0xa2, 0, 2, 1, 0x22, 0x94, 4}, // No wrap from the last record.
		{0xb2, 0, 4, 32, 0x11, 0x90, 0},
		{0xa2, 0, 3, 1, 0x22, 0x90, 0},
		{0xb2, 0, 4, 32, 0x22, 0x90, 0},
		{0xa2, 0, 3, 1, 0x22, 0x94, 4}, // No wrap before the first record.
		{0xb2, 0, 4, 32, 0x22, 0x90, 0},
		{0xa2, 0, 0x11, 1, 0xff, 0x9f, 1},
		{0xc0, 0, 0, 1, 48, 0x90, 0},
		{0xa2, 0, 0x10, 16, 0xff, 0x9f, 1},
		{0xc0, 0, 0, 1, 2, 0x90, 0},
		{0xa2, 1, 0, 1, 0xff, 0x6b, 0},
		{0xa2, 0, 0x80, 1, 0xff, 0x6b, 0},
		{0xa2, 0, 0x20, 1, 0xff, 0x6b, 0},
		{0xa2, 0, 4, 1, 0xff, 0x6b, 0},
		{0xa2, 0, 0, 0, 0, 0x67, 0},
		{0xa2, 0, 0, 17, 0xff, 0x67, 0},
		{0xb2, 0, 4, 32, 0xff, 0x90, 0},
		{0xa2, 0, 0, 1, 0x44, 0x90, 0},
		{0xc0, 0, 0, 1, 0, 0x6f, 0}, // Type one has no response body.
		{0xa4, 0, 0, 2, 0x6f3a, 0x9f, 15},
		{0xa2, 0, 0x13, 1, 0x11, 0x9f, 1}, // Undefined PREVIOUS starts at last.
		{0xc0, 0, 0, 1, 50, 0x90, 0},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f39, 0x9f, 15},
		{0xa2, 0, 0, 1, 0x12, 0x94, 8},
		{0xa4, 0, 0, 2, 0x6f7e, 0x9f, 15},
		{0xa2, 0, 0, 1, 0x55, 0x94, 8},
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0xa2, 0, 0, 1, 0xff, 0x94, 0},
		{0xb2, 1, 4, 32, 0, 0x94, 0},
		{0xb0, 0, 0, 1, 0, 0x94, 0},
		{0xa4, 0, 0, 2, 0x6f3a, 0x9f, 15},
		{0xdc, 2, 4, 32, 0xaabb, 0x90, 0}, // Distinct prefix and interior bytes.
		{0xb2, 2, 4, 32, 0xaabb, 0x90, 0},
		{0xa2, 0, 0, 1, 0xbb, 0x94, 4}, // Interior-only match must not succeed.
		{0xa2, 0, 0x10, 1, 0xaa, 0x9f, 1},
		{0xc0, 0, 0, 1, 2, 0x90, 0},
		{0x28, 0, 1, 8, 0, 0x90, 0, true}, // ENABLE revokes the implicit disabled-CHV authorization.
		{0xb2, 2, 4, 32, 0, 0x98, 4},
		{0xa2, 0, 0, 1, 0xaa, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f3c, 0x9f, 15},
		{0xb2, 1, 4, 176, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f42, 0x9f, 15},
		{0xb2, 1, 4, 44, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f39, 0x9f, 15},
		{0xb2, 1, 4, 3, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f3a, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f3a, 0x90, 0},
		{0x20, 0, 1, 8, 1, 0x98, 4},
		{0xb2, 2, 4, 32, 0, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb2, 2, 4, 32, 0xaabb, 0x90, 0, false, true},
		{0xb2, 2, 4, 32, 0, 0x98, 4}, // Restored enabled/unverified card must deny again.
		{0xa2, 0, 0, 1, 0xaa, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb2, 2, 4, 32, 0xaabb, 0x90, 0},
		{0xa2, 0, 0x10, 1, 0xaa, 0x9f, 1},
		{0xc0, 0, 0, 1, 2, 0x90, 0},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f39, 0x9f, 15},
		{0xb2, 1, 4, 3, 0x12, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f07, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f07, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb0, 0, 0, 1, 8, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f07, 0x9f, 15},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb0, 0, 0, 1, 8, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f20, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f20, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f30, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f30, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f31, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f31, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f37, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f37, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f74, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f74, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f78, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f78, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f7b, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f7b, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f7e, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f7e, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f38, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f38, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb0, 0, 0, 1, 0xcf, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f20, 0x9f, 15},
		{0xb0, 0, 0, 1, 0xff, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f30, 0x9f, 15},
		{0xb0, 0, 0, 1, 0, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f31, 0x9f, 15},
		{0xb0, 0, 0, 1, 5, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f37, 0x9f, 15},
		{0xb0, 0, 0, 1, 0xff, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f74, 0x9f, 15},
		{0xb0, 0, 0, 1, 0xff, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f78, 0x9f, 15},
		{0xb0, 0, 0, 1, 0, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f7b, 0x9f, 15},
		{0xb0, 0, 0, 1, 0xff, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f7e, 0x9f, 15},
		{0xb0, 0, 0, 1, 0x55, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0x88, 0, 0, 16, 0x5a, 0x98, 4},
		{0xc0, 0, 0, 12, 0, 0x6f, 0},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0x88, 0, 0, 16, 0x5a, 0x9f, 12},
		{0xc0, 0, 0, 12, 0, 0x90, 0},
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0x88, 0, 0, 16, 0x5a, 0x94, 8},
		{0xc0, 0, 0, 12, 0, 0x6f, 0},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0x88, 0, 0, 16, 0x5a, 0x9f, 12},
		{0xc0, 0, 0, 12, 0, 0x90, 0},
		{0x26, 0, 1, 8, 0, 0x90, 0},
		{0x28, 0, 1, 8, 1, 0x98, 4},
		{0x28, 0, 1, 8, 1, 0x98, 4},
		{0x28, 0, 1, 8, 1, 0x98, 0x40},
		{0xa4, 0, 0, 2, 0x6f07, 0x9f, 15},
		{0xb0, 0, 0, 1, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x3f00, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x2fe2, 0x9f, 15},
		{0xb0, 0, 0, 1, 0x98, 0x90, 0}, // ALWAYS access survives CHV blocking.
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0x88, 0, 0, 16, 0x5a, 0x98, 4},
		{0xc0, 0, 0, 12, 0, 0x6f, 0},
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f3a, 0x9f, 15},
		{0xdc, 2, 4, 32, 0xcc, 0x98, 4},
		{0x2c, 0, 0, 16, 0, 0x90, 0},
		{0xb2, 2, 4, 32, 0xaabb, 0x90, 0},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0x88, 0, 0, 16, 0x5a, 0x9f, 12},
		{0xc0, 0, 0, 12, 0, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f41, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f41, 0x90, 0},
		{0xb0, 0, 0, 5, 0, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb0, 0, 0, 5, 0xff, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f41, 0x9f, 15},
		{0xb0, 0, 0, 5, 0, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f43, 0x94, 4}, // SMSS belongs to TELECOM, not GSM.
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f43, 0x9f, 15, true, false, true},
		{0xc0, 0, 0, 15, 0x6f43, 0x90, 0},
		{0xb0, 0, 0, 2, 0, 0x98, 4},
		{0xd6, 0, 0, 2, 0x42fe, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb0, 0, 0, 2, 0xff, 0x90, 0},
		{0xd6, 0, 0, 2, 0x42fe, 0x90, 0},
		{0xb0, 0, 0, 2, 0x42fe, 0x90, 0},
		{0xd6, 0, 2, 1, 0x55, 0x94, 2},
		{0xb0, 0, 0, 2, 0x42fe, 0x90, 0, false, true},
		{0xb0, 0, 0, 2, 0, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb0, 0, 0, 2, 0xff, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f30, 0x9f, 15, true, false, true},
		{0xc0, 0, 0, 15, 0x6f30, 0x90, 0},
		{0xd6, 0, 0, 24, 0x1122, 0x98, 4},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xd6, 0, 0, 24, 0x1122, 0x90, 0},
		{0xb0, 0, 0, 1, 0x11, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f7b, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f7b, 0x90, 0},
		{0xd6, 0, 0, 12, 0x3344, 0x90, 0},
		{0xd6, 0, 12, 1, 0x55, 0x94, 2},
		{0xb0, 0, 0, 1, 0x33, 0x90, 0, false, true},
		{0x20, 0, 1, 8, 0, 0x90, 0},
		{0xb0, 0, 0, 1, 0, 0x90, 0}, // Restored SELECT is PLMNsel, not FPLMN.
		{0xa4, 0, 0, 2, 0x6f7b, 0x9f, 15},
		{0xb0, 0, 0, 1, 0xff, 0x90, 0},
		{0xd6, 0, 0, 12, 0x3344, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f30, 0x9f, 15},
		{0xd6, 0, 0, 24, 0x1122, 0x90, 0},
		{0xb0, 0, 0, 1, 0x11, 0x90, 0},
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f43, 0x9f, 15},
		{0xd6, 0, 0, 2, 0x42fe, 0x90, 0},
		{0xb0, 0, 0, 2, 0x42fe, 0x90, 0},
		// Verified CHV1 does not satisfy an administrative UPDATE condition.
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6f07, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f07, 0x90, 0},
		{0xd6, 0, 0, 1, 0x55, 0x98, 4},
		{0xb0, 0, 0, 1, 8, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f31, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f31, 0x90, 0},
		{0xd6, 0, 0, 1, 0x55, 0x98, 4},
		{0xb0, 0, 0, 1, 5, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f78, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f78, 0x90, 0},
		{0xd6, 0, 0, 1, 0x55, 0x98, 4},
		{0xb0, 0, 0, 1, 0, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f38, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f38, 0x90, 0},
		{0xd6, 0, 0, 1, 0x55, 0x98, 4},
		{0xb0, 0, 0, 1, 0xcf, 0x90, 0},
		// AoC UPDATE is CHV1 in this administered laboratory card profile.
		{0x20, 0, 1, 8, 1, 0x98, 4},
		{0x20, 0, 1, 8, 1, 0x98, 4},
		{0x20, 0, 1, 8, 1, 0x98, 0x40},
		{0xa4, 0, 0, 2, 0x6f37, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f37, 0x90, 0},
		{0xd6, 0, 0, 3, 0x1122, 0x98, 4},
		{0xa4, 0, 0, 2, 0x6f41, 0x9f, 15},
		{0xc0, 0, 0, 15, 0x6f41, 0x90, 0},
		{0xd6, 0, 0, 5, 0x3344, 0x98, 4},
		{0x2c, 0, 0, 16, 0, 0x90, 0},
		{0xb0, 0, 0, 5, 0xff, 0x90, 0, true},
		{0xa4, 0, 0, 2, 0x6f37, 0x9f, 15},
		{0xd6, 0, 0, 3, 0x1122, 0x90, 0},
		{0xb0, 0, 0, 3, 0x1122, 0x90, 0},
		{0xd6, 0, 3, 1, 0x55, 0x94, 2},
		{0xb0, 0, 0, 3, 0x1122, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f41, 0x9f, 15},
		{0xd6, 0, 0, 5, 0x3344, 0x90, 0},
		{0xd6, 0, 5, 1, 0x55, 0x94, 2},
		{0xb0, 0, 0, 5, 0x3344, 0x90, 0, true, false, true},
		{0xd6, 0, 0, 5, 0x55, 0x90, 0},
		{0xb0, 0, 0, 5, 0x55, 0x90, 0, false, true},
		{0xb0, 0, 0, 5, 0x3344, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f37, 0x9f, 15},
		{0xb0, 0, 0, 3, 0x1122, 0x90, 0, false, true},
		{0xa4, 0, 0, 2, 0x6f37, 0x9f, 15},
		{0xb0, 0, 0, 3, 0xff, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f41, 0x9f, 15},
		{0xb0, 0, 0, 5, 0xff, 0x90, 0},
		// GSM 09.91 option 1: DCS1800 selects the same backing application,
		// but GET RESPONSE/STATUS retain the externally selected identifier.
		{0xa4, 0, 0, 2, 0x7f21, 0x9f, 22},
		{0xc0, 0, 0, 22, 0x7f21, 0x90, 0, true},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xf2, 0, 0, 22, 0x7f20, 0x90, 0, false, true},
		{0xf2, 0, 0, 22, 0x7f21, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6fae, 0x9f, 15},
		{0xb0, 0, 0, 1, 2, 0x90, 0},
		{0x88, 0, 0, 16, 0x5a, 0x9f, 12},
		{0xc0, 0, 0, 12, 0, 0x90, 0},
		{0xa4, 0, 0, 2, 0x7f10, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6fae, 0x94, 4},
		{0xa4, 0, 0, 2, 0x7f20, 0x9f, 22},
		{0xa4, 0, 0, 2, 0x6fae, 0x9f, 15},
		{0xb0, 0, 0, 1, 2, 0x90, 0},
		// Only the final activation leaves these serial-written values for
		// the independent cold NVRAM loader gate.
		{0xa4, 0, 0, 2, 0x6f37, 0x9f, 15},
		{0xd6, 0, 0, 3, 0x1122, 0x90, 0},
		{0xa4, 0, 0, 2, 0x6f41, 0x9f, 15},
		{0xd6, 0, 0, 5, 0x3344, 0x90, 0}
	};
	static constexpr u8 RECORD_HEADERS[][6] = {
		// Header followed by expected SW1; successful 32-byte records contain ff.
		{0xa0, 0xb2, 0, 4, 32, 0x94}, // Undefined current pointer.
		{0xa0, 0xb2, 0, 3, 32, 0x90}, // Previous from undefined selects last.
		{0xa0, 0xb2, 0, 2, 32, 0x94}, // Linear last must not wrap.
		{0xa0, 0xb2, 0, 4, 32, 0x90}, // Failed next preserves current last.
		{0xa0, 0xb2, 0, 3, 32, 0x90}, // Previous moves to penultimate.
		{0xa0, 0xb2, 1, 4, 32, 0x90}, // Absolute first leaves pointer unchanged.
		{0xa0, 0xb2, 0, 2, 32, 0x90}, // Next therefore reaches last again.
		{0xa0, 0xb2, 0, 2, 32, 0x94},
		{0xa0, 0xb2, 0, 0, 32, 0x6b}, // Current is P1=0/P2=4, not mode zero.
		{0xa0, 0xb2, 1, 0x84, 32, 0x6b}, // Do not mask reserved mode bits.
		{0xa0, 0xb2, 0, 4, 32, 0x90}
	};
	static constexpr u8 PARAMETER_HEADERS[][5] = {
		{0xa0, 0xa4, 1, 0, 2}, {0xa0, 0xa4, 0, 1, 2},
		{0xa0, 0xf2, 1, 0, 22}, {0xa0, 0xf2, 0, 1, 22},
		{0xa0, 0xc0, 1, 0, 22}, {0xa0, 0xc0, 0, 1, 22},
		{0xa0, 0xa4, 0, 0, 0}, {0xa0, 0xa4, 0, 0, 1},
		{0xa0, 0xa4, 0, 0, 3}
	};
	u8 m_bytes[64] = {};
	u8 m_causes[64] = {};
	double m_times[64] = {};
	unsigned m_count = 0, m_cause_count = 0;
	u8 m_reference[64] = {}, m_reference_causes[64] = {};
	double m_reference_times[64] = {};
	unsigned m_reference_count = 0, m_reference_cause_count = 0;
	unsigned m_snapshot_cause_count = 0;
	double m_saved_time = 0, m_replay_time = 0;
	std::stringstream m_checkpoint;
	// PLMN mutation/restoration nests inside the authorization checkpoint
	// reused by the later reset and reinsertion sequences.
	std::string m_previous_checkpoint;
	std::string m_access_checkpoint;
	void expect(bool value, char const *message)
	{
		if (!value) fatalerror("SIMI restoration: %s", message);
	}
	void irq_w(int state)
	{
		if (!state) return;
		u8 const cause = m_simi->iir_r();
		expect(m_cause_count < std::size(m_causes), "bounded interrupt capture");
		m_times[m_cause_count] = machine().time().as_double();
		m_causes[m_cause_count++] = cause;
		if (cause & 0x40)
		{
			expect(m_simi->rx_count_r() == 1 && m_count < std::size(m_bytes), "one visible RX byte");
			m_bytes[m_count++] = m_simi->rxd_r();
		}
		m_simi->iir_w(cause);
	}
	void clear_capture() { m_count = m_cause_count = 0; }
	void transmit(std::initializer_list<u8> bytes)
	{
		m_simi->tx_fifo_control_w(4);
		for (u8 byte : bytes) m_simi->txd_w(byte);
		m_simi->tx_fifo_control_w(0);
	}
	void transmit_parameter_case()
	{
		auto const &header = PARAMETER_HEADERS[m_parameter_case];
		transmit({header[0], header[1], header[2], header[3], header[4]});
	}
	void transmit_record_case()
	{
		auto const &header = RECORD_HEADERS[m_record_case];
		transmit({header[0], header[1], header[2], header[3], header[4]});
	}
	void begin_write_case()
	{
		// Keep the authorization replay's unverified baseline independent of
		// the appended AoC mutation checkpoints.
		if (m_write_case == 290)
			m_access_checkpoint = m_checkpoint.str();
		auto const &item = WRITE_CASES[m_write_case];
		transmit({0xa0, item.ins, item.p1, item.p2, item.length});
		bool const reading = item.ins == 0xb2 || item.ins == 0xb0 || item.ins == 0xc0 || item.ins == 0xf2 || !item.length;
		m_phase = reading ? 30 : 29;
		m_timer->adjust(attotime::from_msec(reading ? 40 : 5));
	}
	void transmit_write_body()
	{
		auto const &item = WRITE_CASES[m_write_case];
		if (item.ins == 0xa4) { transmit({u8(item.body >> 8), u8(item.body)}); return; }
		if (item.ins == 0x20 || item.ins == 0x28 || item.ins == 0x26)
		{
			transmit({u8(item.body ? '9' : '1'), '2', '3', '4', 0xff, 0xff, 0xff, 0xff});
			return;
		}
		if (item.ins == 0x2c)
		{
			transmit({'1', '2', '3', '4', '5', '6', '7', '8',
				'1', '2', '3', '4', 0xff, 0xff, 0xff, 0xff});
			return;
		}
		for (unsigned offset = 0; offset < item.length; offset += 16)
		{
			m_simi->tx_fifo_control_w(4);
			for (unsigned i = offset; i < std::min<unsigned>(offset + 16, item.length); ++i)
				m_simi->txd_w(i == 0 && item.body > 0xff ? u8(item.body >> 8) : u8(item.body));
			m_simi->tx_fifo_control_w(0);
		}
	}
	void save_checkpoint()
	{
		m_saved_time = machine().time().as_double();
		m_snapshot_cause_count = m_cause_count;
		m_checkpoint.str(std::string()); m_checkpoint.clear();
		expect(machine().save().write_stream(m_checkpoint) == STATERR_NONE, "write checkpoint");
	}
	void restore_checkpoint()
	{
		m_checkpoint.clear(); m_checkpoint.seekg(0);
		expect(machine().save().read_stream(m_checkpoint) == STATERR_NONE, "read checkpoint");
		m_replay_time = machine().time().as_double();
	}
	void remember_result()
	{
		std::copy(std::begin(m_bytes), std::end(m_bytes), std::begin(m_reference));
		std::copy(std::begin(m_causes), std::end(m_causes), std::begin(m_reference_causes));
		std::copy(std::begin(m_times), std::end(m_times), std::begin(m_reference_times));
		m_reference_count = m_count; m_reference_cause_count = m_cause_count;
	}
	void compare_result(bool pending_timer = false)
	{
		expect(m_count == m_reference_count && m_cause_count == m_reference_cause_count &&
			std::equal(m_bytes, m_bytes + m_count, m_reference) &&
			std::equal(m_causes, m_causes + m_cause_count, m_reference_causes),
			"exact response and interrupt order after restoration");
		for (unsigned i = 0; i < m_cause_count; ++i)
		{
			// A restored pending timer retains its saved absolute deadline. Work
			// started in this restore callback uses that callback's current time.
			double const delta = pending_timer || i < m_snapshot_cause_count ? m_times[i] - m_reference_times[i] :
				(m_times[i] - m_replay_time) - (m_reference_times[i] - m_saved_time);
			if (std::abs(delta) >= 1e-12)
				osd_printf_info("SIMI deadline mismatch: phase=%u event=%u actual=%.12f reference=%.12f saved=%.12f restored=%.12f delta=%.12f\n",
					m_phase, i, m_times[i], m_reference_times[i], m_saved_time, m_replay_time, delta);
			expect(std::abs(delta) < 1e-12, "preserved response prefix and relative serial deadlines");
		}
		expect(m_simi->iir_r() == 0 && m_simi->rx_count_r() == 0 && m_simi->tx_count_r() == 0,
			"no residual controller causes or visible bytes");
	}
	virtual void machine_start() override
	{
		m_timer = timer_alloc(FUNC(nokia_simi_test_state::check), this);
		save_item(NAME(m_bytes)); save_item(NAME(m_causes));
		save_item(NAME(m_times));
		save_item(NAME(m_count)); save_item(NAME(m_cause_count));
		m_timer->adjust(attotime::from_usec(1));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		switch (m_phase++)
		{
		case 0:
			m_simi->set_clock_enabled(1); m_simi->control_w(0x80);
			m_timer->adjust(attotime::from_msec(30)); return;
		case 1:
			expect(m_count != 0, "ATR delivered through controller"); clear_capture();
			save_checkpoint();
			transmit({0xa0, 0xc0, 0, 0, 22});
			m_phase = 31;
			m_timer->adjust(attotime::from_msec(40)); return;
		case 40:
			expect(m_count != 0 && m_bytes[0] == 0x3b,
				"reset or reinsertion delivers ATR through SIMI");
			clear_capture();
			m_write_case = 136; // Re-run IMSI SELECT/FCP, denial and verification.
			begin_write_case(); return;
		case 31:
		case 32:
			expect(m_count == 25 && m_bytes[0] == 0xc0 && m_bytes[5] == 0x3f &&
				m_bytes[6] == 0 && m_bytes[7] == 1 && m_bytes[23] == 0x90 && m_bytes[24] == 0,
				"GET RESPONSE immediately after activation returns implicitly selected MF");
			if (m_phase == 32)
			{
				restore_checkpoint(); clear_capture();
				transmit({0xa0, 0xc0, 0, 0, 22});
				m_phase = 32;
				m_timer->adjust(attotime::from_msec(40)); return;
			}
			clear_capture();
			transmit({0xa0, 0xa4, 0, 0, 2});
			m_phase = 2;
			m_timer->adjust(attotime::from_msec(5)); return;
		case 2:
			expect(m_count == 1 && m_bytes[0] == 0xa4, "SELECT procedure byte");
			clear_capture(); transmit({0x3f});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 3:
			expect(m_count == 0, "partial SELECT body produces no status"); clear_capture();
			m_simi->tx_fifo_control_w(4); m_simi->txd_w(0);
			expect(m_simi->tx_count_r() == 1, "unflushed final body byte");
			save_checkpoint(); m_simi->tx_fifo_control_w(0);
			m_timer->adjust(attotime::from_msec(5)); return;
		case 4:
			expect(m_count == 2 && m_bytes[0] == 0x9f && m_bytes[1] == 22, "SELECT MF completion");
			expect(m_cause_count == 3 && m_causes[0] == 0x10 && m_causes[1] == 0x40 &&
				m_causes[2] == 0x40, "SELECT TX-empty before two receive characters");
			remember_result(); restore_checkpoint();
			expect(m_count == 0 && m_simi->tx_count_r() == 1, "restore TX and partial card parser");
			m_simi->tx_fifo_control_w(0);
			m_timer->adjust(attotime::from_msec(5)); return;
		case 5:
			compare_result(); clear_capture(); transmit({0xa0, 0xc0, 0, 0, 22});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 6:
			expect(m_count > 0 && m_count < 25, "checkpoint during serialized response");
			save_checkpoint();
			m_timer->adjust(attotime::from_msec(40)); return;
		case 7:
			expect(m_count == 25 && m_bytes[0] == 0xc0 && m_bytes[5] == 0x3f &&
				m_bytes[6] == 0 && m_bytes[7] == 1 && m_bytes[23] == 0x90 && m_bytes[24] == 0,
				"GET RESPONSE metadata and success status");
			expect(m_cause_count == 26 && m_causes[0] == 0x10 &&
				std::all_of(m_causes + 1, m_causes + m_cause_count, [](u8 cause) { return cause == 0x40; }),
				"GET RESPONSE TX-empty before 25 serialized receive characters");
			remember_result(); restore_checkpoint();
			expect(m_count > 0 && m_count < 25, "restore response prefix");
			m_timer->adjust(attotime::from_msec(40)); return;
		case 8:
			compare_result(true);
			clear_capture(); transmit({0xa0, 0x20, 0, 2, 8});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 9:
			expect(m_count == 1 && m_bytes[0] == 0x20, "VERIFY CHV procedure byte");
			clear_capture(); transmit({0x30, 0x30, 0x30, 0x30, 0xff, 0xff, 0xff, 0xff});
			expect(m_count == 0, "failed VERIFY status is still queued");
			save_checkpoint();
			m_timer->adjust(attotime::from_msec(5)); return;
		case 10:
			expect(m_count == 2 && m_bytes[0] == 0x98 && m_bytes[1] == 4,
				"failed VERIFY consumes one attempt");
			remember_result(); clear_capture(); transmit({0xa0, 0x20, 0, 2, 8});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 11:
			expect(m_count == 1 && m_bytes[0] == 0x20, "second VERIFY procedure byte");
			clear_capture(); transmit({0x30, 0x30, 0x30, 0x30, 0xff, 0xff, 0xff, 0xff});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 12:
			expect(m_count == 2 && m_bytes[0] == 0x98 && m_bytes[1] == 4,
				"second failed VERIFY status");
			clear_capture(); transmit({0xa0, 0xf2, 0, 0, 22});
			m_timer->adjust(attotime::from_msec(40)); return;
		case 13:
			expect(m_count == 25 && m_bytes[19] == 0x83 && m_bytes[21] == 0x81,
				"STATUS proves second failed VERIFY consumed another attempt before restore");
			restore_checkpoint();
			m_timer->adjust(attotime::from_msec(5)); return;
		case 14:
			compare_result(true); clear_capture(); transmit({0xa0, 0xf2, 0, 0, 22});
			m_timer->adjust(attotime::from_msec(40)); return;
		case 15:
			expect(m_count == 25 && m_bytes[0] == 0xf2 && m_bytes[19] == 0x83 && m_bytes[21] == 0x82 &&
				m_bytes[23] == 0x90 && m_bytes[24] == 0,
				"STATUS reports restored PIN2 attempts and unchanged PIN1 attempts, without repeating VERIFY");
			osd_printf_info("SIMI restoration: PASS checkpoints=3 partial_apdu=1 pending_rx=1 security_attempts=1 exact_irq_order=1 exact_deadlines=1 handset_claim=0\n");
			clear_capture(); transmit({0, 0xf2, 0, 0, 22});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 16:
			expect(m_count == 2 && m_bytes[0] == 0x6e && m_bytes[1] == 0,
				"unsupported class cannot read GSM directory status");
			clear_capture(); transmit({0x80, 0x20, 0, 2, 8});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 17:
			expect(m_count == 2 && m_bytes[0] == 0x6e && m_bytes[1] == 0,
				"unsupported class cannot begin VERIFY or request its body");
			clear_capture(); transmit({0xa1, 0xa4, 0, 0, 2});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 18:
			expect(m_count == 2 && m_bytes[0] == 0x6e && m_bytes[1] == 0,
				"unsupported class cannot begin SELECT or change selection");
			clear_capture(); transmit({0xa0, 0xf2, 0, 0, 22});
			m_timer->adjust(attotime::from_msec(40)); return;
		case 19:
			expect(m_count == 25 && m_bytes[5] == 0x3f && m_bytes[6] == 0 &&
				m_bytes[19] == 0x83 && m_bytes[21] == 0x82 && m_bytes[23] == 0x90 && m_bytes[24] == 0,
				"valid status after class errors preserves selection and PIN retries");
			clear_capture(); transmit({0xa0, 0x20, 0, 2, 0});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 20:
			expect(m_count == 2 && m_bytes[0] == 0x67 && m_bytes[1] == 0,
				"zero-length incoming VERIFY rejects length without a procedure byte or 256-byte wait");
			clear_capture(); transmit({0xa0, 0xf2, 0, 0, 22});
			m_timer->adjust(attotime::from_msec(40)); return;
		case 21:
			expect(m_count == 25 && m_bytes[19] == 0x83 && m_bytes[21] == 0x82 &&
				m_bytes[23] == 0x90 && m_bytes[24] == 0,
				"next APDU remains aligned and empty VERIFY consumes no PIN attempt");
			osd_printf_info("SIM card header conformance: PASS unsupported_classes=3 empty_verify=1 retry_preservation=1 following_apdu=1 handset_claim=0\n");
			clear_capture(); transmit_parameter_case();
			m_timer->adjust(attotime::from_msec(5)); return;
		case 22:
			expect(m_count == 2 && m_bytes[0] == (m_parameter_case < 6 ? 0x6b : 0x67) && m_bytes[1] == 0,
				"invalid fixed parameters reject at the header without accepting a body");
			clear_capture();
			if (++m_parameter_case < std::size(PARAMETER_HEADERS))
			{
				--m_phase; transmit_parameter_case();
				m_timer->adjust(attotime::from_msec(5)); return;
			}
			transmit({0xa0, 0xf2, 0, 0, 22});
			m_timer->adjust(attotime::from_msec(40)); return;
		case 23:
			expect(m_count == 25 && m_bytes[5] == 0x3f && m_bytes[6] == 0 &&
				m_bytes[19] == 0x83 && m_bytes[21] == 0x82 && m_bytes[23] == 0x90 && m_bytes[24] == 0,
				"parameter errors preserve selection, retries and the following APDU");
			osd_printf_info("SIM card parameter conformance: PASS reserved_parameters=6 select_lengths=3 state_preservation=1 following_apdu=1 handset_claim=0\n");
			clear_capture(); transmit({0xa0, 0xa4, 0, 0, 2});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 24:
			expect(m_count == 1 && m_bytes[0] == 0xa4, "DF TELECOM select procedure");
			clear_capture(); transmit({0x7f, 0x10});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 25:
			expect(m_count == 2 && m_bytes[0] == 0x9f && m_bytes[1] == 22, "select DF TELECOM");
			clear_capture(); transmit({0xa0, 0xa4, 0, 0, 2});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 26:
			expect(m_count == 1 && m_bytes[0] == 0xa4, "ADN select procedure");
			clear_capture(); transmit({0x6f, 0x3a});
			m_timer->adjust(attotime::from_msec(5)); return;
		case 27:
			expect(m_count == 2 && m_bytes[0] == 0x9f && m_bytes[1] == 15, "select linear ADN");
			clear_capture(); transmit_record_case();
			m_timer->adjust(attotime::from_msec(40)); return;
		case 28:
		{
			u8 const expected = RECORD_HEADERS[m_record_case][5];
			if (expected == 0x90)
			{
				expect(m_count == 35 && m_bytes[0] == 0xb2 && m_bytes[33] == 0x90 && m_bytes[34] == 0,
					"complete record response");
				for (unsigned i = 1; i <= 32; ++i) expect(m_bytes[i] == 0xff, "virgin ADN record contents");
			}
			else
				expect(m_count == 2 && m_bytes[0] == expected && m_bytes[1] == (expected == 0x94 ? 2 : 0),
					"linear record boundary or invalid addressing rejects without data");
			clear_capture();
			if (++m_record_case < std::size(RECORD_HEADERS))
			{
				--m_phase; transmit_record_case();
				m_timer->adjust(attotime::from_msec(40)); return;
			}
			osd_printf_info("SIM record addressing: PASS cases=11 linear_no_wrap=1 absolute_preserves_pointer=1 current_record=1 mode_validation=1 handset_claim=0\n");
			begin_write_case(); return;
		}
		case 29:
			expect(m_count == 1 && m_bytes[0] == WRITE_CASES[m_write_case].ins, "record-write procedure byte");
			clear_capture(); transmit_write_body();
			m_timer->adjust(attotime::from_msec(5)); return;
		case 30:
		{
			auto const &item = WRITE_CASES[m_write_case];
			if (item.ins == 0xc0 && item.length == 12 && item.sw1 == 0x90)
			{
				gsm::a3a8::block rand;
				rand.fill(0x5a);
				auto const result = gsm::a3a8::aes_example(gsm::subscriber::laboratory.ki, rand);
				expect(m_count == 15 && m_bytes[0] == 0xc0 && m_bytes[13] == 0x90 && m_bytes[14] == 0,
					"authentication response framing");
				expect(std::equal(result.sres.begin(), result.sres.end(), m_bytes + 1) &&
					std::equal(result.kc.begin(), result.kc.end(), m_bytes + 5),
					"valid authentication after malformed commands returns laboratory SRES and Kc");
			}
			else if (item.ins == 0xc0 && item.length == 1 && item.sw1 == 0x90)
			{
				expect(m_count == 4 && m_bytes[0] == 0xc0 && m_bytes[1] == item.body &&
					m_bytes[2] == 0x90 && m_bytes[3] == 0,
					"SEEK type two returns the independently expected record number");
			}
			else if ((item.ins == 0xc0 || item.ins == 0xf2) && item.sw1 == 0x90)
			{
				expect(m_count == unsigned(item.length) + 3 && m_bytes[0] == item.ins &&
					m_bytes[5] == u8(item.body >> 8) && m_bytes[6] == u8(item.body) &&
					m_bytes[m_count - 2] == 0x90 && m_bytes[m_count - 1] == 0,
					"immediately preceding SELECT supplies the selected file response");
				if (item.body == 0x6f3a)
					expect(m_bytes[9] == 0x11, "ADN advertises CHV1 for READ and UPDATE");
				if (item.body == 0x6f07 || item.body == 0x6f31 || item.body == 0x6f78 || item.body == 0x6f38)
					expect(m_bytes[9] == 0x14, "administrative files advertise CHV1 READ and ADM UPDATE");
				if (item.body == 0x6f30 || item.body == 0x6f7b)
					expect(m_bytes[9] == 0x11, "PLMN lists advertise CHV1 READ and UPDATE");
				if (item.body == 0x6f43)
					expect(m_bytes[9] == 0x11, "SMSS advertises CHV1 READ and UPDATE");
				if (item.body == 0x6f37 || item.body == 0x6f41)
					expect(m_bytes[9] == 0x11, "AoC files advertise administered CHV1 READ and UPDATE");
				if (m_write_case >= 147 && item.length == 15)
					expect((m_bytes[9] & 0xf0) == 0x10, "GSM file advertises CHV1 READ");
			}
			else if ((item.ins == 0xb2 || item.ins == 0xb0) && item.sw1 == 0x90)
			{
				expect(m_count == unsigned(item.length) + 3 && m_bytes[0] == item.ins &&
					m_bytes[m_count - 2] == 0x90 && m_bytes[m_count - 1] == 0, "record-write readback response");
				for (unsigned i = 1; i <= item.length; ++i)
				{
					u8 const expected = i == 1 && item.body > 0xff ? u8(item.body >> 8) : u8(item.body);
					if (m_bytes[i] != expected)
						osd_printf_info("SIM readback mismatch: case=%u byte=%u actual=%02x expected=%02x\n",
								m_write_case, i, m_bytes[i], expected);
					expect(m_bytes[i] == expected, "record-write readback contents");
				}
			}
			else
			{
				if (m_count != 2 || m_bytes[0] != item.sw1 || m_bytes[1] != item.sw2)
					osd_printf_info("SIM record update mismatch: case=%u ins=%02x count=%u sw=%02x%02x expected=%02x%02x\n",
						m_write_case, item.ins, m_count, m_bytes[0], m_bytes[1], item.sw1, item.sw2);
				expect(m_count == 2 && m_bytes[0] == item.sw1 && m_bytes[1] == item.sw2,
					"record-write completion status");
			}
			if (m_write_case == 8) save_checkpoint();
			if (m_write_case == 9) restore_checkpoint();
			if (m_write_case == 29)
				osd_printf_info("SIM record updates: PASS cases=30 linear_mutation=1 failure_preservation=1 state_restore=1 cyclic_previous_only=1 handset_claim=0\n");
			if (m_write_case == 44)
				osd_printf_info("SIM binary offsets: PASS cases=14 full_high_byte=1 failed_write_preserves_data=1 boundary_readback=1 handset_claim=0\n");
			if (m_write_case == 51)
				osd_printf_info("SIM authentication parameters: PASS invalid_parameters=2 invalid_lengths=3 following_authentication=1 laboratory_algorithm=1 handset_claim=0\n");
			if (m_write_case == 61)
				osd_printf_info("SIM response sequencing: PASS initial_mf=1 activation_replay=1 cases=10 consumed_once=1 interrupted_pair=1 technical_error=1 handset_claim=0\n");
			if (m_write_case == 66) save_checkpoint();
			if (m_write_case == 67 && !m_seek_replayed)
			{
				restore_checkpoint(); clear_capture(); m_seek_replayed = true;
				begin_write_case(); return;
			}
			if (m_write_case == 109)
				osd_printf_info("SIM record search: PASS directions=4 types=2 undefined_starts=2 prefix_only=1 failed_pointer_preservation=1 parameter_cases=6 incompatible_files=2 no_selected_ef=3 pending_result_replay=1 handset_claim=0\n");
			if (item.checkpoint)
			{
				if (item.preserve_checkpoint) m_previous_checkpoint = m_checkpoint.str();
				save_checkpoint();
			}
			if (item.restore)
			{
				restore_checkpoint();
				if (!m_previous_checkpoint.empty())
				{
					m_checkpoint.str(m_previous_checkpoint);
					m_checkpoint.clear();
					m_previous_checkpoint.clear();
				}
			}
			clear_capture();
			++m_write_case;
			if (m_activation_checks < 2 && m_write_case == std::size(WRITE_CASES) - 4)
				m_write_case = std::size(WRITE_CASES);
			if (m_write_case < std::size(WRITE_CASES)) { begin_write_case(); return; }
			m_checkpoint.str(m_access_checkpoint);
			m_checkpoint.clear();
			if (m_activation_checks < 2)
			{
				if (m_activation_checks++ == 1)
				{
					m_simi->set_card_present(false);
					m_simi->set_card_present(true);
				}
				m_simi->control_w(0);
				m_simi->control_w(0x80);
				m_phase = 40;
				m_timer->adjust(attotime::from_msec(30)); return;
			}
			osd_printf_info("SIM record read access: PASS protected_files=4 before_pin=1 wrong_pin=1 after_pin=1 seek_access=1 restored_unverified=1 fcp_matches=1 handset_claim=0\n");
			osd_printf_info("SIM IMSI read access: PASS before_pin=1 after_pin=1 restored_unverified=1 reset_revokes=1 removal_revokes=1 fcp_matches=1 handset_claim=0\n");
			osd_printf_info("SIM GSM binary read access: PASS files=9 before_pin=1 after_pin=1 fcp_matches=1 handset_claim=0\n");
			osd_printf_info("SIM authentication access: PASS before_pin=1 after_pin=1 gsm_directory=1 rejected_result_absent=1 handset_claim=0\n");
			osd_printf_info("SIM disabled CHV blocking: PASS failed_enable=3 protected_read=1 protected_write=1 authentication=1 always_read=1 unblock_recovery=1 failed_write_preservation=1 handset_claim=0\n");
			osd_printf_info("SIM PLMN updates: PASS files=2 pin_access=1 readback=1 bounds_preserve=1 state_restore=1 fcp_matches=1 handset_claim=0\n");
			osd_printf_info("SIM PUCT read access: PASS before_pin=1 after_pin=1 restored_unverified=1 fcp_matches=1 handset_claim=0\n");
			osd_printf_info("SIM AoC binary updates: PASS files=2 blocked_pin_denied=1 readback=1 bounds_preserve=1 state_restore=1 fcp_matches=1 handset_claim=0\n");
			osd_printf_info("SIM SMS status: PASS telecom_parent=1 pin_access=1 readback=1 bounds_preserve=1 state_restore=1 fcp_matches=1 handset_claim=0\n");
			osd_printf_info("SIM administrative updates: PASS files=4 fcp_adm=1 verified_pin_denied=1 data_preserved=1 handset_claim=0\n");
			osd_printf_info("SIM DCS directory alias: PASS visible_fid=7f21 shared_application=1 selection_restore=1 authentication=1 telecom_isolation=1 handset_claim=0\n");
			machine().schedule_exit(); return;
		}
		}
		fatalerror("unexpected SIMI fixture phase");
	}
};
static INPUT_PORTS_START(simi_test) INPUT_PORTS_END
ROM_START(simitest) ROM_END

// Card-only APDU conformance; no handset firmware or controller timing claim.
class nokia_sim_fdn_test_state : public driver_device
{
public:
	nokia_sim_fdn_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_card(*this, "card") { }
	void strict(machine_config &config)
	{
		NOKIA_SIM_CARD(config, m_card);
		m_card->response_cb().set(FUNC(nokia_sim_fdn_test_state::response));
	}
	void readable(machine_config &config) { strict(config); m_readable = true; }
protected:
	virtual void machine_start() override
	{
		m_timer = timer_alloc(FUNC(nokia_sim_fdn_test_state::run), this);
		m_timer->adjust(attotime::from_msec(1));
	}
private:
	required_device<nokia_sim_card_device> m_card;
	emu_timer *m_timer = nullptr;
	bool m_readable = false;
	std::vector<u8> m_response;
	void response(u8 data) { m_response.push_back(data); }
	void expect(bool value, const char *message)
	{
		if (!value) fatalerror("SIM FDN conformance: %s", message);
	}
	void command(std::vector<u8> const &tx, std::vector<u8> const &expected)
	{
		m_response.clear();
		for (u8 value : tx) m_card->rx_w(value);
		expect(m_response == expected, "exact APDU response");
	}
	void select(u16 fid) { command({0xa0,0xa4,0,0,2,u8(fid >> 8),u8(fid)}, {0xa4,0x9f,u8(fid == 0x7f10 || fid == 0x7f20 ? 22 : 15)}); }
	void verify(unsigned index)
	{
		std::vector<u8> tx = {0xa0,0x20,0,u8(index),8};
		for (u8 value : index == 1 ? std::initializer_list<u8>{'1','2','3','4'} : std::initializer_list<u8>{'5','6','7','8'}) tx.push_back(value);
		tx.insert(tx.end(),4,0xff); command(tx,{0x20,0x90,0});
	}
	void write_record(u8 sw1, u8 sw2)
	{
		std::vector<u8> tx = {0xa0,0xdc,1,4,32}; tx.insert(tx.end(),32,0x41);
		command(tx,{0xdc,sw1,sw2});
	}
	void read_record(u8 value)
	{
		std::vector<u8> expected = {0xb2};expected.insert(expected.end(),32,value);
		expected.insert(expected.end(),{0x90,0});command({0xa0,0xb2,1,4,32},expected);
	}
	void metadata(bool valid)
	{
		m_response.clear(); for (u8 value : {0xa0,0xc0,0,0,15}) m_card->rx_w(value);
		expect(m_response.size() == 18 && m_response[9] == 0x11 && m_response[11] == 0x22 &&
			m_response[12] == ((valid ? 1 : 0) | (m_readable ? 4 : 0)), "ADN access and validity metadata");
	}
	TIMER_CALLBACK_MEMBER(run)
	{
		m_card->activate();
		select(0x7f10); select(0x6f3b);
		command({0xa0,0xb2,1,4,32},{0x98,4});
		verify(1); read_record(0xff); write_record(0x98,4);
		verify(2);write_record(0x90,0);read_record(0x41);
		select(0x6f3a);metadata(true);
		command({0xa0,4,1,0,0},{0x6b,0});
		command({0xa0,4,0,0,1},{0x67,0});
		std::stringstream saved;
		expect(machine().save().write_stream(saved) == STATERR_NONE,"save card before invalidation");
		command({0xa0,4,0,0,0},{0x90,0});
		command({0xa0,4,0,0,0},{0x98,0x10});
		select(0x6f3a);metadata(false);
		if (m_readable) read_record(0xff);
		else command({0xa0,0xb2,1,4,32},{0x98,0x10});
		saved.seekg(0);
		expect(machine().save().read_stream(saved) == STATERR_NONE,"restore ADN validity");
		select(0x6f3a);metadata(true);
		command({0xa0,4,0,0,0},{0x90,0});
		select(0x7f20);select(0x6f7e);
		command({0xa0,0xb0,0,0,1},{0x98,0x10});
		command({0xa0,0x44,0,0,0},{0x90,0});
		select(0x6f07);command({0xa0,0xb0,0,0,1},{0x98,0x10});
		command({0xa0,0x44,0,0,0},{0x90,0});
		select(0x6f7e);command({0xa0,0xb0,0,0,1},{0xb0,0xff,0x90,0});
		m_card->deactivate();m_card->activate();
		select(0x7f20);select(0x6f7e);
		command({0xa0,0xb0,0,0,1},{0x98,0x10});
		command({0xa0,0x44,0,0,0},{0x98,4});
		verify(1);command({0xa0,0x44,0,0,0},{0x90,0});
		select(0x6f07);command({0xa0,0x44,0,0,0},{0x90,0});
		select(0x7f10);select(0x6f3a);
		command({0xa0,0x44,0,0,0},{0x98,4});
		verify(1);command({0xa0,0x44,0,0,0},{0x98,4});
		verify(2);command({0xa0,0x44,0,0,0},{0x90,0});
		command({0xa0,0x44,0,0,0},{0x98,0x10});
		select(0x6f3a);metadata(true);read_record(0xff);
		select(0x6f3b);read_record(0x41);
		command({0xa0,4,0,0,0},{0x98,4});
		osd_printf_info("SIM FDN conformance: PASS readable_invalid=%u chv2_update=1 reset_revokes=1 validity_restore=1 adn_preserved=1 identity_session_invalidation=1 handset_claim=0\n",m_readable);
		machine().schedule_exit();
	}
};
// Card-byte conformance only: no claim about SIMI's unmeasured clock wiring.
class nokia_sim_pts_test_state : public driver_device
{
public:
	nokia_sim_pts_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_card(*this, "card") { }
	void test(machine_config &config)
	{
		NOKIA_SIM_CARD(config, m_card);
		m_card->response_cb().set(FUNC(nokia_sim_pts_test_state::response));
	}
protected:
	virtual void machine_start() override
	{
		m_timer = timer_alloc(FUNC(nokia_sim_pts_test_state::run), this);
		m_timer->adjust(attotime::from_msec(1));
	}
private:
	required_device<nokia_sim_card_device> m_card;
	emu_timer *m_timer = nullptr;
	unsigned m_case = 0;
	std::vector<u8> m_response;
	void response(u8 byte) { m_response.push_back(byte); }
	void expect(bool value, char const *message)
	{
		if (!value) fatalerror("SIM PTS conformance: case=%u bytes=%u %s",
				m_case, unsigned(m_response.size()), message);
	}
	void command(std::vector<u8> const &bytes, std::vector<u8> const &expected)
	{
		m_response.clear();
		for (u8 byte : bytes) m_card->rx_w(byte);
		expect(m_response == expected, "exact negotiation response");
	}
	void activate()
	{
		m_response.clear();
		m_card->activate();
		expect(m_response == std::vector<u8>({0x3b, 0x10, 0x05}), "unchanged ATR profile");
	}
	TIMER_CALLBACK_MEMBER(run)
	{
		struct pts_case { std::vector<u8> request, response; };
		std::array<pts_case, 10> const cases = {{
			{{0xff,0x00,0xff},{0xff,0x00,0xff}},
			{{0xff,0x10,0x11,0xfe},{0xff,0x10,0x11,0xfe}},
			{{0xff,0x10,0x94,0x7b},{0xff,0x00,0xff}},
			{{0xff,0x10,0x05,0xea},{0xff,0x00,0xff}},
			{{0xff,0x00,0xfe},{}},
			{{0xff,0x01,0xfe},{}},
			{{0xff,0x80,0x7f},{}},
			{{0xff,0x20,0x00,0xdf},{}},
			{{0xff,0x40,0x00,0xbf},{}},
			{{0xff,0x70,0x11,0x22,0x33,0x8f},{}}
		}};
		for (auto const &item : cases)
		{
			activate();
			command(item.request, item.response);
			m_card->deactivate();
			activate();
			command({0xff,0,0xff},{0xff,0,0xff});
			command({0xa0,0xa4,0,0,2,0x3f,0},{0xa4,0x9f,22});
			++m_case;
		}
		// Both outcomes resume the same partial request, including its open
		// negotiation window. The response observer is intentionally not saved.
		activate();
		command({0xff,0x10},{});
		std::stringstream saved;
		expect(machine().save().write_stream(saved) == STATERR_NONE, "save partial PTS");
		command({0x94,0x7b},{0xff,0,0xff});
		saved.seekg(0);
		expect(machine().save().read_stream(saved) == STATERR_NONE, "restore partial PTS");
		command({0x11,0xfe},{0xff,0x10,0x11,0xfe});
		command({0xa0,0xa4,0,0,2,0x3f,0},{0xa4,0x9f,22});
		command({0xff,0,0xff,0,0},{0x6e,0});
		activate();
		command({0xff},{});
		m_card->deactivate();
		activate();
		command({0xff,0,0xff},{0xff,0,0xff});
		osd_printf_info("SIM PTS conformance: PASS cases=10 default_speed=1 unsupported_speed_fallback=2 checksum=1 reserved_fields=1 partial_restore=1 following_apdu=10 late_pts_class_error=1 deactivation_abort=1 enhanced_clock_claim=0 handset_claim=0\n");
		machine().schedule_exit();
	}
};
ROM_START(simptst) ROM_END
ROM_START(simfdnt) ROM_END
ROM_START(simfdnrt) ROM_END
}
CONS(2026, simitest, 0, 0, test, simi_test, nokia_simi_test_state, empty_init,
		"Test", "Nokia SIMI controller/card restoration", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
CONS(2026, simfdnt, 0, 0, strict, simi_test, nokia_sim_fdn_test_state, empty_init,
		"Test", "SIM fixed dialling strict conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
CONS(2026, simfdnrt, 0, 0, readable, simi_test, nokia_sim_fdn_test_state, empty_init,
		"Test", "SIM fixed dialling readable-invalid conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
CONS(2026, simptst, 0, 0, test, simi_test, nokia_sim_pts_test_state, empty_init,
		"Test", "SIM PTS negotiation conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
