// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_LAB_CALL_HOST_H
#define MAME_NOKIA_NOKIA_LAB_CALL_HOST_H

#include <array>
#include <atomic>
#include <deque>
#include <memory>

// Host side of the laboratory BTS: the subset of the host call protocol
// (protocol_version 1, as nokia_gsm_call_adapter and dct3_sip_bridge.py speak
// it) that the native lab cell serves. Network registration, host-originated
// incoming calls, call phases, GSM-FR media both ways and host clearing.
// WebSocket callbacks run on the HTTP thread; the lab drains them in emulation.
class nokia_lab_call_host
{
public:
	using speech_frame = std::array<u8, 33>;
	struct incoming { u32 epoch = 0, request_id = 0; std::array<u8, 20> digits{}; unsigned length = 0; };
	struct termination { u32 epoch = 0, request_id = 0; u8 cause = 16; };
	struct downlink { u32 epoch = 0, request_id = 0, sequence = 0; speech_frame frame{}; };
	struct events
	{
		bool ready_pending = false;
		std::deque<incoming> calls;
		std::deque<termination> terminations;
		std::deque<downlink> media;
		unsigned dropped = 0;
	};

	nokia_lab_call_host();
	~nokia_lab_call_host();

	// Registers the endpoint only while MAME's HTTP server runs (-http).
	bool start(running_machine &machine);
	void stop(running_machine &machine);
	bool active() const { return m_active; }
	events take();
	// A reset or state load invalidates every host identity.
	void new_epoch();
	u32 epoch() const { return m_epoch.load(); }

	void publish_ready(bool calls_idle);
	void publish_network(bool registered, u16 arfcn, u8 bsic);
	void publish_incoming_state(u32 request_id, char const *phase, u32 uplink_sequence = 0, u32 downlink_sequence = 0);
	void publish_uplink(u32 request_id, u32 sequence, u64 time_us, bool good, speech_frame const &frame);

private:
	struct state;
	void send(std::string const &message);

	std::unique_ptr<state> m_state;
	std::atomic<u32> m_epoch{1};
	bool m_active = false;
};

#endif // MAME_NOKIA_NOKIA_LAB_CALL_HOST_H
