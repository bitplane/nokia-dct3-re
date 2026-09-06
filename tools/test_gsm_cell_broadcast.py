import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class GsmCellBroadcastTest(unittest.TestCase):
    def test_page_round_trip_and_geometry(self):
        source = r'''
#include "gsm_cell_broadcast.h"
int main() {
    std::array<unsigned char, gsm::cell_broadcast::content_octets> content{};
    content[0] = 0x41;
    const auto page = gsm::cell_broadcast::make_page(0x1234, 0x1000, 0, 1, 1, content);
    const auto decoded = gsm::cell_broadcast::decode_page(page);
    return page.size() == 88 && decoded.valid &&
        decoded.serial_number == 0x1234 && decoded.message_identifier == 0x1000 &&
        decoded.page_index == 1 && decoded.page_count == 1 &&
        decoded.content[0] == 0x41 ? 0 : 1;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            test = directory / "test.cpp"
            binary = directory / "test"
            test.write_text(source)
            subprocess.run([
                "c++", "-std=c++17", "-Idriver", str(test),
                "driver/gsm_cell_broadcast.cpp", "-o", str(binary)
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)

    def test_invalid_page_number_is_rejected(self):
        source = r'''
#include "gsm_cell_broadcast.h"
int main() {
    gsm::cell_broadcast::page page{};
    page[5] = 0x21;
    return gsm::cell_broadcast::decode_page(page).valid ? 1 : 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            test = directory / "test.cpp"
            binary = directory / "test"
            test.write_text(source)
            subprocess.run([
                "c++", "-std=c++17", "-Idriver", str(test),
                "driver/gsm_cell_broadcast.cpp", "-o", str(binary)
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)

    def test_radio_blocks_round_trip_and_reject_wrong_sequence(self):
        source = r'''
#include "gsm_cell_broadcast.h"
int main() {
    gsm::cell_broadcast::page page{};
    for (unsigned n = 0; n < page.size(); ++n) page[n] = n;
    const auto blocks = gsm::cell_broadcast::segment_page(page);
    gsm::cell_broadcast::page decoded{};
    if (blocks[0][0] != 0x40 || blocks[1][0] != 0x41 ||
        blocks[2][0] != 0x42 || blocks[3][0] != 0x53 ||
        !gsm::cell_broadcast::reassemble_page(blocks, decoded) || decoded != page)
        return 1;
    auto malformed = blocks;
    malformed[2][0] = 0x21;
    return gsm::cell_broadcast::reassemble_page(malformed, decoded) ? 1 : 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            test = directory / "test.cpp"
            binary = directory / "test"
            test.write_text(source)
            subprocess.run([
                "c++", "-std=c++17", "-Idriver", str(test),
                "driver/gsm_cell_broadcast.cpp", "-o", str(binary)
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
