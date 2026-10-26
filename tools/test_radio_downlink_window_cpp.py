"""Execute the radio tick's asynchronous Layer-3 promotion block.

The link/session stand-ins expose window state; handset integration gates cover
the real BLOCK_REQUEST, acknowledgement and release exchange.
"""

import unittest

from tools import test_device_read_contracts_cpp as contracts


class RadioDownlinkWindowTests(unittest.TestCase):
    execute = contracts.DeviceReadContractsTests.execute

    def test_pending_frame_retains_uplink_opportunity_until_acknowledged(self):
        tick = contracts.method("nokia_radio_peer.cpp", "void nokia_radio_peer_device::tick()")
        start = tick.index("\tconst bool queued_disconnect =")
        end = tick.index("\n\tif (current_phase() == phase::random_access", start)
        promotion = tick[start:end]
        self.execute(r"""
#include <array>
struct nokia_gsm_session_device {
    enum class downlink_kind { none, call_disconnect, other };
    downlink_kind kind = downlink_kind::none;
    struct message { u8 sapi = 0; } pending;
    downlink_kind pending_downlink_kind() const { return kind; }
    const message &pending_downlink() const { return pending; }
};
struct link {
    std::array<bool, 4> ack{}, establishment{};
    bool downlink_acknowledgement_pending(u8 sapi) const { return ack.at(sapi); }
    bool awaiting_establishment(u8 sapi) const { return establishment.at(sapi); }
};
struct peer {
    enum class phase { service_uplink_request, service_uplink_wait, service_downlink, other };
    phase current = phase::other;
    bool m_traffic_channel_active = false;
    bool m_report_deferred = false;
    unsigned m_reports_remaining = 0, m_wait_ticks = 17;
    nokia_gsm_session_device session;
    link lapdm;
    nokia_gsm_session_device *m_gsm_session = &session;
    link *m_lapdm_link = &lapdm;
    phase current_phase() const { return current; }
    void set_phase(phase value) { current = value; }
    void promote() {
""" + promotion + r"""
    }
};
int main() {
    using kind = nokia_gsm_session_device::downlink_kind;
    using phase = peer::phase;
    // Check both channels, both uplink phases and unrelated outstanding SAPIs.
    for (u8 sapi : {0, 3})
    for (bool traffic : {false, true})
    for (auto current : {phase::service_uplink_request, phase::service_uplink_wait, phase::other})
    for (auto message : {kind::none, kind::call_disconnect, kind::other})
    for (bool ack : {false, true})
    for (bool establishment : {false, true}) {
        peer p;
        p.current = current;
        p.session.kind = message;
        p.session.pending.sapi = sapi;
        p.m_traffic_channel_active = traffic;
        p.lapdm.ack[sapi] = ack;
        p.lapdm.establishment[sapi] = establishment;
        p.lapdm.ack[sapi == 0 ? 3 : 0] = true;
        p.m_reports_remaining = 1;
        const bool phase_allows = current != phase::other &&
            (message == kind::call_disconnect ||
             (traffic ? current == phase::service_uplink_request : current == phase::service_uplink_wait));
        const bool allowed = message != kind::none && phase_allows && !ack && !establishment;
        p.promote();
        assert(p.current == (allowed ? phase::service_downlink : current));
        assert(p.m_reports_remaining == 1);
        assert(p.m_wait_ticks == (allowed ? 0U : 17U));
        assert(p.m_report_deferred == allowed);
    }
    // Reproduce asynchronous busy: send once, then preserve the scheduled
    // uplink request on every tick while Layer 3 still retains Disconnect.
    peer p;
    p.current = phase::service_uplink_wait;
    p.session.kind = kind::call_disconnect;
    p.promote();
    assert(p.current == phase::service_downlink);
    p.current = phase::service_uplink_request;
    p.lapdm.ack[0] = true;
    p.m_report_deferred = false;
    for (int tick = 0; tick < 1000; tick++) {
        p.promote();
        assert(p.current == phase::service_uplink_request);
        assert(!p.m_report_deferred && p.m_reports_remaining == 1);
    }
    // The handset's ACK retires the old frame; the next queued message can
    // then use the ordinary non-traffic downlink opportunity.
    p.lapdm.ack[0] = false;
    p.session.kind = kind::other;
    p.current = phase::service_uplink_wait;
    p.promote();
    assert(p.current == phase::service_downlink);
}
""")


if __name__ == "__main__":
    unittest.main()
