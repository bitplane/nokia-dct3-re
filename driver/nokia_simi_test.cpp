// license:BSD-3-Clause
// copyright-holders:Gaz

// Development-only controller/card restoration, not a handset boot fixture.
#include "emu.h"
#include "nokia_simi.h"
#include "nokia_sim_card.h"
#include <sstream>
#include <cmath>

namespace {
class nokia_simi_test_state : public driver_device
{
public:
	nokia_simi_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_simi(*this, "simi"), m_card(*this, "card") { }
	void test(machine_config &config)
	{
		NOKIA_SIMI(config, m_simi);
		NOKIA_SIM_CARD(config, m_card);
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
			transmit({0xa0, 0xa4, 0, 0, 2});
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
			machine().schedule_exit(); return;
		}
		fatalerror("unexpected SIMI fixture phase");
	}
};
static INPUT_PORTS_START(simi_test) INPUT_PORTS_END
ROM_START(simitest) ROM_END
}
CONS(2026, simitest, 0, 0, test, simi_test, nokia_simi_test_state, empty_init,
		"Test", "Nokia SIMI controller/card restoration", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
