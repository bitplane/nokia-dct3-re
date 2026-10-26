"""Execute production accessors with minimal MAME callback/timer stand-ins.

These tests cover local state transitions and bounds, not scheduler integration.
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def method(filename, signature):
    source = (ROOT / "driver" / filename).read_text()
    match = re.search(re.escape(signature) + r"\n\{.*?\n\}", source, re.S)
    if match is None:
        raise AssertionError(f"Missing production method: {signature}")
    return match.group(0)


PREAMBLE = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <iterator>
using u8 = uint8_t;
using u16 = uint16_t;
using offs_t = unsigned;
struct test_time { double as_double() const { return 0; } };
enum class machine_phase { RESET, RUNNING };
struct test_machine {
    bool suppressed = false;
    machine_phase current_phase = machine_phase::RUNNING;
    machine_phase phase() const { return current_phase; }
    bool side_effects_disabled() const { return suppressed; }
    test_time time() const { return {}; }
};
static test_machine running_machine;
static test_machine &machine() { return running_machine; }
template<class... T> void fatalerror(const char *, T...) { throw std::runtime_error("fatal"); }
static std::string log_text;
template<class... T> void log_message(const char *format, T... args) {
    char line[1024];
    std::snprintf(line, sizeof(line), format, args...);
    log_text += line;
}
#define LOGMASKED(mask, ...) log_message(__VA_ARGS__)
"""


class DeviceReadContractsTests(unittest.TestCase):
    def execute(self, source):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "contract.cpp"
            binary = Path(directory) / "contract"
            cpp.write_text(PREAMBLE + source)
            result = subprocess.run(
                [compiler, "-std=c++17", "-fsanitize=undefined",
                 "-fno-sanitize-recover=all", str(cpp), "-o", str(binary)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_dsp_peek_does_not_acknowledge_bootstrap(self):
        self.execute(r"""
struct nokia_dspif_device {
    u16 m_ram[0x800] = {};
    bool m_trace_enabled = true;
    unsigned callbacks = 0;
    void m_shared_0fe_read_cb(int state) { callbacks++; if (state) m_ram[0x7f] = 0; }
    void m_shared_100_read_cb(int state) { callbacks++; if (state) m_ram[0x80] = 0; }
    u16 shared_r(offs_t offset);
};
""" + method("nokia_dspif.cpp", "u16 nokia_dspif_device::shared_r(offs_t offset)") + r"""
int main() {
    nokia_dspif_device d;
    for (unsigned offset : {0x7fU, 0x80U}) {
        d.m_ram[offset] = 0x1234;
        d.callbacks = 0;
        running_machine.suppressed = true;
        assert(d.shared_r(offset) == 0x1234 && d.shared_r(offset) == 0x1234);
        assert(d.m_ram[offset] == 0x1234 && d.callbacks == 0);
        assert(log_text.empty());
        running_machine.suppressed = false;
        assert(d.shared_r(offset) == 0x1234);
        assert(d.m_ram[offset] == 0 && d.callbacks == 2);
        log_text.clear();
    }
}
""")

    def test_serial_peeks_preserve_receive_and_interrupt_state(self):
        self.execute(r"""
constexpr int SIMI_CHARACTER_TIME = 1;
struct attotime { static constexpr int never = -1; };
struct timer { unsigned calls = 0; int deadline = 0;
    void adjust(int value) { calls++; deadline = value; } };
struct nokia_simi_device {
    u8 m_rx_fifo[320] = {0x3b, 0x90};
    u16 m_rx_head = 0, m_rx_count = 2;
    bool m_rx_ready = true;
    timer rx_timer;
    timer *m_rx_timer = &rx_timer;
    u8 rxd_r();
};
constexpr u8 CTRL_RESET = 0x80;
struct nokia_mbus_device {
    u8 m_control = 0, m_data = 0xa5;
    bool m_rx_ready = true;
    unsigned acknowledgements = 0, traces = 0;
    u8 status() const { return m_rx_ready ? 0x20 : 0; }
    void m_fiq2_cb(int) { acknowledgements++; }
    void trace_event(const char *, u8) { traces++; }
    u8 read(offs_t offset);
};
""" + method("nokia_simi.cpp", "u8 nokia_simi_device::rxd_r()")
             + method("nokia_mbus.cpp", "u8 nokia_mbus_device::read(offs_t offset)") + r"""
int main() {
    nokia_simi_device sim;
    nokia_mbus_device mbus;
    running_machine.suppressed = true;
    for (int i = 0; i < 2; i++) {
        assert(sim.rxd_r() == 0x3b && mbus.read(2) == 0xa5);
        assert(sim.m_rx_head == 0 && sim.m_rx_count == 2 && sim.m_rx_ready);
        assert(sim.rx_timer.calls == 0 && mbus.m_rx_ready);
        assert(mbus.acknowledgements == 0 && mbus.traces == 0);
    }
    running_machine.suppressed = false;
    assert(sim.rxd_r() == 0x3b && sim.m_rx_head == 1 && sim.m_rx_count == 1);
    assert(!sim.m_rx_ready && sim.rx_timer.calls == 1);
    assert(sim.rx_timer.deadline == SIMI_CHARACTER_TIME);
    assert(mbus.read(2) == 0xa5 && !mbus.m_rx_ready);
    assert(mbus.acknowledgements == 1 && mbus.traces == 1);
    sim.m_rx_ready = true;
    assert(sim.rxd_r() == 0x90 && sim.m_rx_count == 0);
    assert(sim.rx_timer.deadline == attotime::never);
}
""")

    def test_gensio_peek_preserves_ccont_transaction(self):
        ccont = (ROOT / "driver/nokia_ccont.cpp").read_text()
        constants = "\n".join(re.findall(r"^constexpr [^;]+;", ccont, re.M))
        self.execute(constants + r"""
struct nokia_ccont_device {
    u8 m_cmd = (ADC_MSB << CMD_ADDR_SHIFT) | CMD_READ;
    u8 m_regs[16] = {};
    bool m_data_cycle = true, m_adc_trace = true, m_rtc_trace = true;
    uint8_t serial_r();
};
constexpr u8 STATUS_RX_READY = 4;
struct nokia_gensio_device {
    struct { unsigned ccont_read = 0x6c, status = 0x6d; } m_wiring;
    nokia_ccont_device card;
    u8 m_status = 7;
    u8 m_ccont_read_cb() { return card.serial_r(); }
    u8 peek(offs_t) const { return 0; }
    u8 read(offs_t offset);
};
""" + method("nokia_ccont.cpp", "uint8_t nokia_ccont_device::serial_r()")
             + method("nokia_gensio.cpp", "u8 nokia_gensio_device::read(offs_t offset)") + r"""
int main() {
    nokia_gensio_device d;
    d.card.m_regs[ADC_MSB] = 2;
    running_machine.suppressed = true;
    assert(d.read(0x6c) == 0xb2 && d.read(0x6c) == 0xb2);
    assert(d.card.m_data_cycle && d.m_status == 7);
    assert(log_text.empty());
    running_machine.suppressed = false;
    assert(d.read(0x6c) == 0xb2);
    assert(!d.card.m_data_cycle && d.m_status == 3);
}
""")

    def test_dsp_trace_checks_guest_cursor_before_reading(self):
        header = (ROOT / "driver/nokia_dspif.h").read_text()
        constants = "\n".join(re.findall(r"\s*static constexpr [^;]+;", header))
        self.execute(r"""
struct nokia_dspif_device {
""" + constants + r"""
    u16 m_ram[0x800] = {};
    bool m_trace_enabled = true;
    struct packet { unsigned type = 0, length = 0; };
    bool peek_tx_packet(packet &) { return false; }
    std::string tx_packet_trace(const packet &) { return {}; }
    void peer_shared_w(offs_t offset, u16 data) { m_ram[offset] = data; }
    void dsp_data_w(u16 address, u16 data);
};
""" + method("nokia_dspif.cpp", "void nokia_dspif_device::dsp_data_w(u16 address, u16 data)") + r"""
int main() {
    nokia_dspif_device d;
    for (unsigned cursor : {0U, d.RX_START - 1, d.RX_END, 0x800U, 0xffffU}) {
        d.m_ram[d.RX_PRODUCER] = cursor;
        log_text.clear();
        d.dsp_data_w(d.hpi_daram_base + d.RX_PRODUCER, d.RX_START);
        assert(d.m_ram[d.RX_PRODUCER] == d.RX_START);
        assert(log_text.find("invalid-cursor=") != std::string::npos);
    }
    for (unsigned cursor : {d.RX_START, d.RX_END - 1}) {
        d.m_ram[d.RX_PRODUCER] = cursor;
        d.m_ram[cursor] = 0x0374;
        log_text.clear();
        d.dsp_data_w(d.hpi_daram_base + d.RX_PRODUCER, cursor + 1);
        assert(log_text.find("type=74 payload=3") != std::string::npos);
    }
    d.m_trace_enabled = false;
    d.m_ram[d.RX_PRODUCER] = 0xffff;
    log_text.clear();
    d.dsp_data_w(d.hpi_daram_base + d.RX_PRODUCER, 0x1234);
    assert(d.m_ram[d.RX_PRODUCER] == 0x1234 && log_text.empty());
}
""")


if __name__ == "__main__":
    unittest.main()
