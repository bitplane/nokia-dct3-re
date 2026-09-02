from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class GsmCallAdapterSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = (ROOT / "driver/nokia_gsm_call_adapter.h").read_text()
        cls.source = (ROOT / "driver/nokia_gsm_call_adapter.cpp").read_text()
        cls.driver = (ROOT / "driver/nokia_dct3.cpp").read_text()
        cls.generic_sources = "\n".join(
            (ROOT / path).read_text()
            for path in (
                "driver/nokia_gsm_call_adapter.cpp",
                "driver/nokia_gsm_session.cpp",
                "driver/nokia_gsm_voice_peer.cpp",
                "driver/nokia_radio_peer.cpp",
            )
        )

    def test_host_thread_only_queues_bounded_decisions(self):
        callback = self.source.split(
            "server->add_endpoint(", 1
        )[1].split(
            "void nokia_gsm_call_adapter_device::device_reset", 1
        )[0]
        self.assertIn("MAXIMUM_QUEUED_EVENTS", callback)
        self.assertIn("++m_host->dropped_events", callback)
        self.assertIn("std::lock_guard<std::mutex>", callback)
        self.assertIn("m_host->decisions.push_back", callback)
        self.assertIn("m_host->terminations.push_back", callback)
        self.assertIn("m_host->media.push_back", callback)
        self.assertIn("outgoing_call_terminate", callback)
        self.assertIn("outgoing_call_media_downlink", callback)
        self.assertNotIn("submit_outgoing_decision", callback)
        self.assertNotIn("submit_outgoing_termination", callback)

    def test_emulation_timer_owns_session_submission(self):
        poll = self.source.split(
            "TIMER_CALLBACK_MEMBER(nokia_gsm_call_adapter_device::poll_host)", 1
        )[1].split(
            "void nokia_gsm_call_adapter_device::publish_request", 1
        )[0]
        self.assertIn("m_session->submit_outgoing_decision", poll)
        self.assertIn("m_session->submit_outgoing_termination", poll)
        self.assertIn("m_voice_peer->submit_host_downlink", poll)
        self.assertIn("m_voice_peer->take_host_uplink", poll)
        self.assertIn('"emulation_time_us"', self.source)
        self.assertIn('"source_time_us"', self.source)
        self.assertIn("IsUint64()", self.source)
        self.assertIn("queue overflow dropped=", poll)
        self.assertIn('"media_uplink_sequence"', self.source)
        self.assertIn('"media_downlink_sequence"', self.source)
        self.assertIn("host_next_uplink_sequence()", self.source)
        self.assertIn("host_next_downlink_sequence()", self.source)
        self.assertIn("timer_alloc", self.source)

    def test_disabled_adapter_has_no_periodic_emulation_timer(self):
        start = self.source.split(
            "void nokia_gsm_call_adapter_device::device_start()", 1
        )[1].split(
            "void nokia_gsm_call_adapter_device::set_enabled", 1
        )[0]
        enabled = self.source.split(
            "void nokia_gsm_call_adapter_device::set_enabled", 1
        )[1].split(
            "void nokia_gsm_call_adapter_device::device_reset", 1
        )[0]
        self.assertIn("m_poll_timer->adjust(attotime::never)", start)
        self.assertIn(
            "enabled ? attotime::from_msec(10) : attotime::never",
            enabled,
        )

    def test_protocol_is_bounded_and_excludes_mm_admission(self):
        self.assertIn("MAXIMUM_HOST_MESSAGE = 256", self.source)
        self.assertIn('message.HasMember("epoch")', self.source)
        self.assertIn("m_transport_epoch", self.header)
        self.assertNotIn("save_item(NAME(m_transport_epoch))", self.source)
        for decision in ('decision == "connect"', 'decision == "busy"',
                         'decision == "no_answer"'):
            self.assertIn(decision, self.source)
        self.assertNotIn('decision == "service_reject"', self.source)

    def test_restore_discards_transport_input_and_forces_resynchronization(self):
        postload = self.source.split(
            "void nokia_gsm_call_adapter_device::postload()", 1
        )[1].split(
            "void nokia_gsm_call_adapter_device::device_stop", 1
        )[0]
        self.assertIn("m_transport_epoch.fetch_add(1)", postload)
        for queue in ("decisions", "terminations", "media"):
            self.assertIn(f"m_host->{queue}.clear()", postload)
        self.assertIn("m_host->republish = true", postload)
        self.assertIn('writer.Key("epoch")', self.source)

    def test_completed_firmware_lifecycle_is_published_to_host(self):
        poll = self.source.split(
            "TIMER_CALLBACK_MEMBER(nokia_gsm_call_adapter_device::poll_host)", 1
        )[1].split(
            "void nokia_gsm_call_adapter_device::publish_state", 1
        )[0]
        self.assertIn('publish_state("ended")', poll)
        self.assertIn("!m_session->outgoing_request_pending()", poll)
        self.assertIn("m_last_published_request_id = 0", poll)

    def test_host_mode_explicitly_disables_fallback(self):
        self.assertIn(
            "m_gsm_session->set_outgoing_fallback_enabled(!host_call_adapter)",
            self.driver,
        )
        self.assertIn(
            "m_gsm_call_adapter->set_enabled(host_call_adapter)",
            self.driver,
        )
        self.assertIn(
            "m_radio_peer->set_host_voice_peer(host_call_adapter)",
            self.driver,
        )

    def test_generic_call_components_have_no_product_dispatch(self):
        for product in (
            "noki3210",
            "noki3310",
            "noki3330",
            "noki3410",
            "NSE8",
            "NHM5",
            "NHM6",
            "NHM2",
        ):
            self.assertNotIn(product, self.generic_sources)


if __name__ == "__main__":
    unittest.main()
