// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "http.h"
#include "main.h"
#include "nokia_lab_call_host.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <mutex>
#include <set>
#include <string>

namespace {

// The same endpoint and limits as nokia_gsm_call_adapter; a machine has one.
constexpr char ENDPOINT[] = "/nokia/dct3/calls";
constexpr unsigned MAXIMUM_HOST_MESSAGE = 512;
constexpr unsigned MAXIMUM_QUEUED_EVENTS = 16;

int hex_nibble(char value)
{
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

}

struct nokia_lab_call_host::state
{
	std::mutex mutex;
	std::set<http_manager::websocket_connection_ptr, std::owner_less<http_manager::websocket_connection_ptr>> connections;
	events pending;

	unsigned queued() const { return pending.calls.size() + pending.terminations.size() + pending.media.size(); }
};

nokia_lab_call_host::nokia_lab_call_host() : m_state(std::make_unique<state>()) { }
nokia_lab_call_host::~nokia_lab_call_host() = default;

bool nokia_lab_call_host::start(running_machine &machine)
{
	http_manager *const server = machine.manager().http();
	if (!server->is_active())
		return false;
	// Every DCT3 machine carries the HLE call adapter, which registers this
	// path at device start even where it stays disabled, as on the native lab
	// machines. Take the route over: re-adding rebinds its callbacks here.
	server->remove_endpoint(ENDPOINT);
	server->add_endpoint(
			ENDPOINT,
			[this](http_manager::websocket_connection_ptr connection)
			{
				std::lock_guard<std::mutex> lock(m_state->mutex);
				m_state->connections.insert(connection);
				m_state->pending.ready_pending = true;
			},
			[this](http_manager::websocket_connection_ptr, std::string const &payload, int opcode)
			{
				if (opcode != 1 || payload.empty() || payload.size() > MAXIMUM_HOST_MESSAGE)
					return;
				rapidjson::Document message;
				message.Parse(payload.data(), payload.size());
				if (!message.IsObject() || !message.HasMember("type") || !message["type"].IsString() ||
						!message.HasMember("epoch") || !message["epoch"].IsUint() ||
						!message.HasMember("request_id") || !message["request_id"].IsUint())
					return;
				std::string_view const type(message["type"].GetString(), message["type"].GetStringLength());
				u32 const epoch = message["epoch"].GetUint(), request_id = message["request_id"].GetUint();
				std::lock_guard<std::mutex> lock(m_state->mutex);
				if (m_state->queued() >= MAXIMUM_QUEUED_EVENTS)
				{
					++m_state->pending.dropped;
					return;
				}
				if (type == "incoming_call")
				{
					if (!request_id || !message.HasMember("caller") || !message["caller"].IsString())
						return;
					incoming item{ epoch, request_id };
					std::string_view const caller(message["caller"].GetString(), message["caller"].GetStringLength());
					if (caller.empty() || caller.size() > item.digits.size())
						return;
					for (char digit : caller)
					{
						if (digit < '0' || digit > '9')
							return;
						item.digits[item.length++] = u8(digit - '0');
					}
					m_state->pending.calls.push_back(item);
				}
				else if (type == "incoming_call_terminate")
				{
					if (message.HasMember("cause") && (!message["cause"].IsUint() ||
							!message["cause"].GetUint() || message["cause"].GetUint() > 0x7f))
						return;
					u8 const cause = message.HasMember("cause") ? u8(message["cause"].GetUint()) : u8(16);
					m_state->pending.terminations.push_back({ epoch, request_id, cause });
				}
				else if (type == "incoming_call_media_downlink")
				{
					downlink media{ epoch, request_id };
					if (!message.HasMember("sequence") || !message["sequence"].IsUint() ||
							!message.HasMember("frame") || !message["frame"].IsString() ||
							message["frame"].GetStringLength() != 2 * media.frame.size())
						return;
					media.sequence = message["sequence"].GetUint();
					char const *const encoded = message["frame"].GetString();
					for (unsigned index = 0; index < media.frame.size(); ++index)
					{
						int const high = hex_nibble(encoded[2 * index]), low = hex_nibble(encoded[2 * index + 1]);
						if (high < 0 || low < 0)
							return;
						media.frame[index] = u8((high << 4) | low);
					}
					m_state->pending.media.push_back(media);
				}
			},
			[this](http_manager::websocket_connection_ptr connection, int, std::string const &)
			{
				std::lock_guard<std::mutex> lock(m_state->mutex);
				m_state->connections.erase(connection);
			},
			[this](http_manager::websocket_connection_ptr connection, std::error_code const &)
			{
				std::lock_guard<std::mutex> lock(m_state->mutex);
				m_state->connections.erase(connection);
			});
	m_active = true;
	return true;
}

void nokia_lab_call_host::stop(running_machine &machine)
{
	if (m_active)
		machine.manager().http()->remove_endpoint(ENDPOINT);
	m_active = false;
}

nokia_lab_call_host::events nokia_lab_call_host::take()
{
	events result;
	std::lock_guard<std::mutex> lock(m_state->mutex);
	std::swap(result, m_state->pending);
	return result;
}

void nokia_lab_call_host::new_epoch()
{
	m_epoch.fetch_add(1);
	std::lock_guard<std::mutex> lock(m_state->mutex);
	bool const connected = !m_state->connections.empty();
	m_state->pending = events{};
	// Connections outlive resets and loads; announce the new epoch to them.
	m_state->pending.ready_pending = connected;
}

void nokia_lab_call_host::send(std::string const &message)
{
	std::lock_guard<std::mutex> lock(m_state->mutex);
	for (auto const &connection : m_state->connections)
		connection->send_message(message, 1);
}

void nokia_lab_call_host::publish_ready(bool calls_idle)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject();
	writer.Key("type"); writer.String("call_adapter_ready");
	writer.Key("protocol_version"); writer.Uint(1);
	writer.Key("calls_idle"); writer.Bool(calls_idle);
	writer.Key("epoch"); writer.Uint(epoch());
	writer.Key("capabilities");
	writer.StartArray();
	writer.String("network_state"); writer.String("calls"); writer.String("gsm_fr_media");
	writer.EndArray();
	writer.EndObject();
	send(buffer.GetString());
}

void nokia_lab_call_host::publish_network(bool registered, u16 arfcn, u8 bsic)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject();
	writer.Key("type"); writer.String("network_state");
	writer.Key("epoch"); writer.Uint(epoch());
	writer.Key("registered"); writer.Bool(registered);
	if (registered)
	{
		writer.Key("arfcn"); writer.Uint(arfcn);
		writer.Key("bsic"); writer.Uint(bsic);
	}
	writer.EndObject();
	send(buffer.GetString());
}

void nokia_lab_call_host::publish_incoming_state(u32 request_id, char const *phase, u32 uplink_sequence, u32 downlink_sequence)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject();
	writer.Key("type"); writer.String("incoming_call_state");
	writer.Key("request_id"); writer.Uint(request_id);
	writer.Key("epoch"); writer.Uint(epoch());
	writer.Key("phase"); writer.String(phase);
	if (!strcmp(phase, "connected"))
	{
		writer.Key("media_uplink_sequence"); writer.Uint(uplink_sequence);
		writer.Key("media_downlink_sequence"); writer.Uint(downlink_sequence);
	}
	writer.EndObject();
	send(buffer.GetString());
}

void nokia_lab_call_host::publish_uplink(u32 request_id, u32 sequence, u64 time_us, bool good, speech_frame const &frame)
{
	static constexpr char HEX[] = "0123456789abcdef";
	std::string encoded;
	for (u8 byte : frame)
	{
		encoded.push_back(HEX[byte >> 4]);
		encoded.push_back(HEX[byte & 15]);
	}
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject();
	writer.Key("type"); writer.String("incoming_call_media_uplink");
	writer.Key("request_id"); writer.Uint(request_id);
	writer.Key("epoch"); writer.Uint(epoch());
	writer.Key("sequence"); writer.Uint(sequence);
	writer.Key("emulation_time_us"); writer.Uint64(time_us);
	writer.Key("good"); writer.Bool(good);
	writer.Key("frame"); writer.String(encoded.data(), encoded.size());
	writer.EndObject();
	send(buffer.GetString());
}
