"""Reset/restore correlation tests using the production adapter methods."""
from tools.dct3_products import composition_source

from pathlib import Path
import unittest

from tools import test_device_read_contracts_cpp as contracts


ROOT = Path(__file__).resolve().parents[1]


class HostResetContractTests(unittest.TestCase):
    execute = contracts.DeviceReadContractsTests.execute

    def test_reset_and_restore_never_reuse_live_connection_epoch(self):
        header = (ROOT / "driver/nokia_gsm_call_adapter.h").read_text()
        members = header[header.index("\tu32 m_last_published_request_id"):
                         header.index("\n};")]
        methods = "\n".join(contracts.method(
            "nokia_gsm_call_adapter.cpp",
            f"void nokia_gsm_call_adapter_device::{signature}",
        ) for signature in ("device_reset()", "session_reset_w(int state)", "postload()"))
        self.execute(r"""
#include <array>
#include <atomic>
#include <mutex>
#include <vector>
using u32 = uint32_t;
struct host_state {
    std::mutex mutex;
    std::vector<int> connections{1};
    std::vector<int> decisions, sms_decisions, ussd_responses, terminations;
    std::vector<int> media, incoming, incoming_sms, incoming_ussd;
    bool republish = false, ready_pending = false;
    unsigned dropped_events = 0;
};
struct nokia_gsm_call_adapter_device {
    host_state host;
    host_state *m_host = &host;
    void device_reset();
    void session_reset_w(int state);
    void postload();
""" + members + "\n};\n" + methods + r"""
int main() {
    nokia_gsm_call_adapter_device d;
    running_machine.current_phase = machine_phase::RESET;
    d.session_reset_w(1);
    assert(!d.m_transport_initialized);
    d.device_reset();
    assert(d.m_transport_epoch.load() == 1);
    // A later global reset advances once, independent of device reset order.
    d.session_reset_w(1);
    d.device_reset();
    d.session_reset_w(1);
    assert(d.m_transport_epoch.load() == 2);
    running_machine.current_phase = machine_phase::RUNNING;
    for (int lifecycle = 0; lifecycle < 6; lifecycle++) {
        const u32 delayed_reply_epoch = d.m_transport_epoch.load();
        d.m_last_published_request_id = 1;
        d.m_last_published_sms_request_id = 1;
        d.m_last_published_ussd_request_id = 1;
        d.host.decisions.push_back(1);
        d.host.sms_decisions.push_back(1);
        d.host.ussd_responses.push_back(1);
        d.host.terminations.push_back(1);
        d.host.media.push_back(1);
        d.host.incoming.push_back(1);
        d.host.incoming_sms.push_back(1);
        d.host.incoming_ussd.push_back(1);
        if (lifecycle % 3 == 0) d.device_reset();
        if (lifecycle % 3 == 1) {
            d.session_reset_w(0);
            assert(d.m_transport_epoch.load() == delayed_reply_epoch);
            d.session_reset_w(1);
        }
        if (lifecycle % 3 == 2) d.postload();
        // Even if the session reuses request_id=1, its previous epoch is invalid.
        assert(d.m_transport_epoch.load() > delayed_reply_epoch);
        assert(d.m_last_published_request_id == 0);
        assert(d.m_last_published_sms_request_id == 0);
        assert(d.m_last_published_ussd_request_id == 0);
        assert(d.host.decisions.empty() && d.host.sms_decisions.empty());
        assert(d.host.ussd_responses.empty() && d.host.terminations.empty());
        assert(d.host.media.empty() && d.host.incoming.empty());
        assert(d.host.incoming_sms.empty() && d.host.incoming_ussd.empty());
        assert(d.host.connections.size() == 1);
        assert(d.host.republish && d.host.ready_pending);
    }
}
""")

    def test_all_session_reset_paths_notify_adapter(self):
        # Wiring checks complement the executable adapter lifecycle above.
        reset = contracts.method("nokia_gsm_session.cpp",
                                 "void nokia_gsm_session_device::device_reset()")
        self.assertIn("m_reset_cb(1);", reset)
        self.assertIn("m_reset_cb(0);", reset)
        failure = contracts.method("nokia_gsm_session.cpp",
                                   "void nokia_gsm_session_device::radio_link_failed()")
        self.assertIn("device_reset();", failure)
        board = contracts.method("nokia_dct3.cpp",
                                 "void nokia_dct3_state::reset_digital_baseband(bool retain_sram)")
        self.assertIn("m_gsm_session->reset();", board)
        source = composition_source()
        self.assertIn("m_gsm_session->reset_cb().set(m_gsm_call_adapter,\n"
                      "\t\t\tFUNC(nokia_gsm_call_adapter_device::session_reset_w));", source)


if __name__ == "__main__":
    unittest.main()
