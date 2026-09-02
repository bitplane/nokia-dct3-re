// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "http.h"
#include "main.h"
#include "nokia_gsm_call_adapter.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <deque>
#include <mutex>
#include <set>
#include <string>

#define LOG_CALL_ADAPTER (1U << 0)
#define VERBOSE (LOG_CALL_ADAPTER)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_CALL_ADAPTER, nokia_gsm_call_adapter_device,
		"nokia_gsm_call_adapter", "Nokia DCT3 GSM host call adapter")

namespace {

constexpr char ENDPOINT[] = "/nokia/dct3/calls";
constexpr unsigned MAXIMUM_HOST_MESSAGE = 256;
constexpr unsigned MAXIMUM_QUEUED_EVENTS = 16;

int hex_nibble(char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	if (value >= 'A' && value <= 'F')
		return value - 'A' + 10;
	return -1;
}

}

struct nokia_gsm_call_adapter_device::host_state
{
	struct queued_decision
	{
		u32 epoch = 0;
		u32 request_id = 0;
		nokia_gsm_network_device::outgoing_call_outcome outcome =
				nokia_gsm_network_device::outgoing_call_outcome::connect;
	};
	struct queued_termination
	{
		u32 epoch = 0;
		u32 request_id = 0;
		u8 cause = 0x10;
	};
	struct queued_media
	{
		u32 epoch = 0;
		u32 request_id = 0;
		u32 sequence = 0;
		u64 source_time_us = 0;
		nokia_gsm_voice_peer_device::speech_frame frame{};
	};

	std::mutex mutex;
	std::set<
			http_manager::websocket_connection_ptr,
			std::owner_less<http_manager::websocket_connection_ptr>>
			connections;
	std::deque<queued_decision> decisions;
	std::deque<queued_termination> terminations;
	std::deque<queued_media> media;
	bool republish = false;
	unsigned dropped_events = 0;
};

nokia_gsm_call_adapter_device::~nokia_gsm_call_adapter_device() = default;

nokia_gsm_call_adapter_device::nokia_gsm_call_adapter_device(
		const machine_config &mconfig, const char *tag,
		device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_CALL_ADAPTER, tag, owner, clock),
	m_session(*this, "^gsm_session"),
	m_voice_peer(*this, "^gsm_voice_peer")
{
}

void nokia_gsm_call_adapter_device::device_start()
{
	m_host = std::make_unique<host_state>();
	m_poll_timer = timer_alloc(
			FUNC(nokia_gsm_call_adapter_device::poll_host), this);
	m_poll_timer->adjust(attotime::never);
	save_item(NAME(m_enabled));
	save_item(NAME(m_last_published_request_id));
	save_item(NAME(m_last_published_connected));
	save_item(NAME(m_last_published_alerting));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_gsm_call_adapter_device::postload), this));

	http_manager *const server = machine().manager().http();
	if (!server->is_active())
		return;
	using namespace std::placeholders;
	server->add_endpoint(
			ENDPOINT,
			[this](http_manager::websocket_connection_ptr connection)
			{
				std::lock_guard<std::mutex> lock(m_host->mutex);
				m_host->connections.insert(connection);
				m_host->republish = true;
			},
			[this](http_manager::websocket_connection_ptr,
					const std::string &payload, int opcode)
			{
				if (opcode != 1 || payload.empty() ||
						payload.size() > MAXIMUM_HOST_MESSAGE)
					return;
				rapidjson::Document message;
				message.Parse(payload.data(), payload.size());
				if (!message.IsObject() ||
						!message.HasMember("type") ||
						!message["type"].IsString() ||
						!message.HasMember("epoch") ||
						!message["epoch"].IsUint() ||
						!message.HasMember("request_id") ||
						!message["request_id"].IsUint())
					return;
				const std::string_view type(
						message["type"].GetString(),
						message["type"].GetStringLength());
				if (type == "outgoing_call_terminate")
				{
					if (message.HasMember("cause") &&
							(!message["cause"].IsUint() ||
								message["cause"].GetUint() == 0 ||
								message["cause"].GetUint() > 0x7f))
						return;
					const u8 cause = message.HasMember("cause") ?
							u8(message["cause"].GetUint()) : u8(0x10);
					std::lock_guard<std::mutex> lock(m_host->mutex);
					if (m_host->terminations.size() +
							m_host->decisions.size() + m_host->media.size() <
									MAXIMUM_QUEUED_EVENTS)
						m_host->terminations.push_back(
								{ message["epoch"].GetUint(),
									message["request_id"].GetUint(), cause });
					else
						++m_host->dropped_events;
					return;
				}
				if (type == "outgoing_call_media_downlink")
				{
					if (!message.HasMember("sequence") ||
							!message["sequence"].IsUint() ||
							!message.HasMember("source_time_us") ||
							!message["source_time_us"].IsUint64() ||
							!message.HasMember("frame") ||
							!message["frame"].IsString() ||
							message["frame"].GetStringLength() !=
									2 * nokia_gsm_voice_peer_device::
											speech_frame{}.size())
						return;
					host_state::queued_media media;
					media.epoch = message["epoch"].GetUint();
					media.request_id = message["request_id"].GetUint();
					media.sequence = message["sequence"].GetUint();
					media.source_time_us =
							message["source_time_us"].GetUint64();
					const char *const encoded = message["frame"].GetString();
					for (unsigned index = 0; index < media.frame.size(); ++index)
					{
						const int high = hex_nibble(encoded[2 * index]);
						const int low = hex_nibble(encoded[2 * index + 1]);
						if (high < 0 || low < 0)
							return;
						media.frame[index] = u8((high << 4) | low);
					}
					std::lock_guard<std::mutex> lock(m_host->mutex);
					if (m_host->media.size() + m_host->terminations.size() +
							m_host->decisions.size() < MAXIMUM_QUEUED_EVENTS)
						m_host->media.push_back(media);
					else
						++m_host->dropped_events;
					return;
				}
				if (type != "outgoing_call_decision" ||
						!message.HasMember("decision") ||
						!message["decision"].IsString())
					return;
				const std::string_view decision(
						message["decision"].GetString(),
						message["decision"].GetStringLength());
				nokia_gsm_network_device::outgoing_call_outcome outcome;
				if (decision == "connect")
					outcome =
							nokia_gsm_network_device::outgoing_call_outcome::
									connect;
				else if (decision == "busy")
					outcome =
							nokia_gsm_network_device::outgoing_call_outcome::
									busy;
				else if (decision == "no_answer")
					outcome =
							nokia_gsm_network_device::outgoing_call_outcome::
									no_answer;
				else
					return;
				std::lock_guard<std::mutex> lock(m_host->mutex);
				if (m_host->decisions.size() +
						m_host->terminations.size() + m_host->media.size() <
								MAXIMUM_QUEUED_EVENTS)
					m_host->decisions.push_back(
							{ message["epoch"].GetUint(),
								message["request_id"].GetUint(), outcome });
				else
					++m_host->dropped_events;
			},
			[this](http_manager::websocket_connection_ptr connection,
					int, const std::string &)
			{
				std::lock_guard<std::mutex> lock(m_host->mutex);
				m_host->connections.erase(connection);
			},
			[this](http_manager::websocket_connection_ptr connection,
					const std::error_code &)
			{
				std::lock_guard<std::mutex> lock(m_host->mutex);
				m_host->connections.erase(connection);
			});
}

void nokia_gsm_call_adapter_device::set_enabled(bool enabled)
{
	m_enabled = enabled;
	m_poll_timer->adjust(
			enabled ? attotime::from_msec(10) : attotime::never,
			0,
			enabled ? attotime::from_msec(10) : attotime::never);
}

void nokia_gsm_call_adapter_device::device_reset()
{
	m_last_published_request_id = 0;
	m_last_published_connected = false;
	m_last_published_alerting = false;
	m_transport_epoch.store(1);
	std::lock_guard<std::mutex> lock(m_host->mutex);
	m_host->decisions.clear();
	m_host->terminations.clear();
	m_host->media.clear();
	m_host->republish = false;
	m_host->dropped_events = 0;
}

void nokia_gsm_call_adapter_device::postload()
{
	m_transport_epoch.fetch_add(1);
	m_last_published_request_id = 0;
	m_last_published_connected = false;
	m_last_published_alerting = false;
	std::lock_guard<std::mutex> lock(m_host->mutex);
	m_host->decisions.clear();
	m_host->terminations.clear();
	m_host->media.clear();
	m_host->republish = true;
	m_host->dropped_events = 0;
}

void nokia_gsm_call_adapter_device::device_stop()
{
	http_manager *const server = machine().manager().http();
	if (server->is_active())
		server->remove_endpoint(ENDPOINT);
	m_host.reset();
}

TIMER_CALLBACK_MEMBER(nokia_gsm_call_adapter_device::poll_host)
{
	if (!m_enabled)
		return;
	std::deque<host_state::queued_decision> decisions;
	std::deque<host_state::queued_termination> terminations;
	std::deque<host_state::queued_media> media;
	bool republish;
	unsigned dropped_events;
	{
		std::lock_guard<std::mutex> lock(m_host->mutex);
		decisions.swap(m_host->decisions);
		terminations.swap(m_host->terminations);
		media.swap(m_host->media);
		republish = m_host->republish;
		m_host->republish = false;
		dropped_events = m_host->dropped_events;
		m_host->dropped_events = 0;
	}
	if (dropped_events)
		LOGMASKED(LOG_CALL_ADAPTER,
				"gsm_call_adapter: queue overflow dropped=%u t=%.6f\n",
				dropped_events, machine().time().as_double());
	for (const auto &decision : decisions)
	{
		if (decision.epoch != m_transport_epoch.load())
		{
			LOGMASKED(LOG_CALL_ADAPTER,
					"gsm_call_adapter: stale epoch=%u current=%u type=decision "
					"id=%u result=rejected t=%.6f\n",
					decision.epoch, m_transport_epoch.load(),
					decision.request_id, machine().time().as_double());
			continue;
		}
		const bool accepted = m_session->submit_outgoing_decision(
				decision.request_id, decision.outcome);
		LOGMASKED(LOG_CALL_ADAPTER,
				"gsm_call_adapter: decision id=%u outcome=%u result=%s t=%.6f\n",
				decision.request_id, unsigned(decision.outcome),
				accepted ? "accepted" : "rejected",
				machine().time().as_double());
	}
	const bool connected = m_session->outgoing_call_connected();
	const bool alerting = m_session->outgoing_call_alerting();
	if (connected)
		m_voice_peer->begin_host_media(m_session->outgoing_request_id());
	else
		m_voice_peer->end_host_media();
	for (const auto &item : media)
	{
		if (item.epoch != m_transport_epoch.load())
		{
			LOGMASKED(LOG_CALL_ADAPTER,
					"gsm_call_adapter: stale epoch=%u current=%u type=media "
					"id=%u result=rejected t=%.6f\n",
					item.epoch, m_transport_epoch.load(), item.request_id,
					machine().time().as_double());
			continue;
		}
		const bool accepted = m_voice_peer->submit_host_downlink(
				item.request_id, item.sequence, item.frame);
		LOGMASKED(LOG_CALL_ADAPTER,
				"gsm_call_adapter: media direction=downlink id=%u sequence=%u "
				"result=%s source_time_us=%llu t=%.6f\n",
				item.request_id, item.sequence,
				accepted ? "accepted" : "rejected",
				item.source_time_us,
				machine().time().as_double());
	}
	for (const auto &termination : terminations)
	{
		if (termination.epoch != m_transport_epoch.load())
		{
			LOGMASKED(LOG_CALL_ADAPTER,
					"gsm_call_adapter: stale epoch=%u current=%u "
					"type=termination id=%u result=rejected t=%.6f\n",
					termination.epoch, m_transport_epoch.load(),
					termination.request_id, machine().time().as_double());
			continue;
		}
		const bool accepted = m_session->submit_outgoing_termination(
				termination.request_id, termination.cause);
		LOGMASKED(LOG_CALL_ADAPTER,
				"gsm_call_adapter: termination id=%u cause=%u result=%s t=%.6f\n",
				termination.request_id, termination.cause,
				accepted ? "accepted" : "rejected",
				machine().time().as_double());
	}
	if (m_session->outgoing_request_pending() &&
			(republish ||
				m_session->outgoing_request_id() !=
						m_last_published_request_id))
		publish_request();
	if (connected && (!m_last_published_connected || republish))
		publish_state("connected");
	else if (alerting && (!m_last_published_alerting || republish))
		publish_state("alerting");
	else if (m_last_published_request_id &&
			!m_session->outgoing_request_pending() &&
			!connected && !alerting)
	{
		// A host must not infer physical End or remote release from a gap in
		// the 20 ms media stream.  Publish closure after the firmware-owned
		// CC/RR lifecycle has actually returned the session to idle.
		publish_state("ended");
		m_last_published_request_id = 0;
	}
	m_last_published_connected = connected;
	m_last_published_alerting = alerting;
	u32 media_request_id;
	u32 media_sequence;
	u64 media_time_us;
	bool media_good;
	nokia_gsm_voice_peer_device::speech_frame media_frame{};
	while (m_voice_peer->take_host_uplink(
			media_request_id, media_sequence, media_time_us,
			media_good, media_frame))
		publish_uplink_media(
				media_request_id, media_sequence, media_time_us,
				media_good, media_frame);
}

void nokia_gsm_call_adapter_device::publish_state(const char *phase)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject();
	writer.Key("type");
	writer.String("outgoing_call_state");
	writer.Key("request_id");
	writer.Uint(m_session->outgoing_request_id());
	writer.Key("epoch");
	writer.Uint(m_transport_epoch.load());
	writer.Key("phase");
	writer.String(phase);
	if (!strcmp(phase, "connected"))
	{
		writer.Key("media_uplink_sequence");
		writer.Uint(m_voice_peer->host_next_uplink_sequence());
		writer.Key("media_downlink_sequence");
		writer.Uint(m_voice_peer->host_next_downlink_sequence());
	}
	writer.EndObject();
	std::lock_guard<std::mutex> lock(m_host->mutex);
	for (const auto &connection : m_host->connections)
		connection->send_message(buffer.GetString(), 1);
	LOGMASKED(LOG_CALL_ADAPTER,
			"gsm_call_adapter: state id=%u epoch=%u phase=%s "
			"media_uplink_sequence=%u media_downlink_sequence=%u t=%.6f\n",
			m_session->outgoing_request_id(), m_transport_epoch.load(), phase,
			m_voice_peer->host_next_uplink_sequence(),
			m_voice_peer->host_next_downlink_sequence(),
			machine().time().as_double());
}

void nokia_gsm_call_adapter_device::publish_uplink_media(
		u32 request_id, u32 sequence, u64 time_us, bool good,
		const nokia_gsm_voice_peer_device::speech_frame &frame)
{
	static constexpr char HEX[] = "0123456789abcdef";
	std::array<char, 2 * nokia_gsm_voice_peer_device::speech_frame{}.size() + 1>
			encoded{};
	for (unsigned index = 0; index < frame.size(); ++index)
	{
		encoded[2 * index] = HEX[frame[index] >> 4];
		encoded[2 * index + 1] = HEX[frame[index] & 0x0f];
	}
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject();
	writer.Key("type");
	writer.String("outgoing_call_media_uplink");
	writer.Key("request_id");
	writer.Uint(request_id);
	writer.Key("epoch");
	writer.Uint(m_transport_epoch.load());
	writer.Key("sequence");
	writer.Uint(sequence);
	writer.Key("emulation_time_us");
	writer.Uint64(time_us);
	writer.Key("good");
	writer.Bool(good);
	writer.Key("frame");
	writer.String(encoded.data(), encoded.size() - 1);
	writer.EndObject();
	std::lock_guard<std::mutex> lock(m_host->mutex);
	for (const auto &connection : m_host->connections)
		connection->send_message(buffer.GetString(), 1);
	if (sequence < 12 || (sequence % 50) == 0)
		LOGMASKED(LOG_CALL_ADAPTER,
				"gsm_call_adapter: media direction=uplink id=%u sequence=%u "
				"good=%u t=%.6f\n",
				request_id, sequence, good, machine().time().as_double());
}

void nokia_gsm_call_adapter_device::publish_request()
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject();
	writer.Key("type");
	writer.String("outgoing_call");
	writer.Key("request_id");
	writer.Uint(m_session->outgoing_request_id());
	writer.Key("epoch");
	writer.Uint(m_transport_epoch.load());
	writer.Key("digits");
	std::string digits;
	for (unsigned index = 0;
			index < m_session->outgoing_called_digits_length(); ++index)
		digits += char('0' + m_session->outgoing_called_digits()[index]);
	writer.String(digits.c_str(), digits.size());
	writer.EndObject();

	unsigned clients = 0;
	{
		std::lock_guard<std::mutex> lock(m_host->mutex);
		for (const auto &connection : m_host->connections)
		{
			connection->send_message(buffer.GetString(), 1);
			++clients;
		}
	}
	m_last_published_request_id = m_session->outgoing_request_id();
	LOGMASKED(LOG_CALL_ADAPTER,
			"gsm_call_adapter: request id=%u epoch=%u digits=%s clients=%u t=%.6f\n",
			m_last_published_request_id, m_transport_epoch.load(),
			digits.c_str(), clients,
			machine().time().as_double());
}
