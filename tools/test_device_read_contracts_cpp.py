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
using u8 = uint8_t;
using u16 = uint16_t;
using offs_t = unsigned;
struct test_time { double as_double() const { return 0; } };
struct test_machine {
    bool suppressed = false;
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
