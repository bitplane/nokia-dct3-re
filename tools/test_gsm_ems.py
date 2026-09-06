import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class GsmEmsTest(unittest.TestCase):
    def test_formatted_text_round_trip(self):
        source = r'''
#include "gsm_ems.h"
int main() {
    const auto data = gsm::ems::formatted_ucs2("hello", 0x10);
    const auto format = gsm::ems::parse_text_formatting(data.data.data(), data.length);
    return data.length == 16 && format.valid && format.start == 0 &&
        format.length == 5 && format.mode == 0x10 ? 0 : 1;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            test = directory / "test.cpp"
            binary = directory / "test"
            test.write_text(source)
            subprocess.run([
                "c++", "-std=c++17", "-Idriver", str(test),
                "driver/gsm_ems.cpp", "-o", str(binary)
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)

    def test_malformed_header_is_rejected(self):
        source = r'''
#include "gsm_ems.h"
int main() {
    const unsigned char bad[] = { 5, 0x0a, 4, 0, 5, 0x10 };
    return gsm::ems::parse_text_formatting(bad, sizeof(bad)).valid ? 1 : 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            test = directory / "test.cpp"
            binary = directory / "test"
            test.write_text(source)
            subprocess.run([
                "c++", "-std=c++17", "-Idriver", str(test),
                "driver/gsm_ems.cpp", "-o", str(binary)
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
