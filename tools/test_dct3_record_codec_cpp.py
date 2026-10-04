import random
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path
import unittest

from tools.nse5_transform_trace_check import inverse_transform_words


ROOT = Path(__file__).resolve().parents[1]


class RecordCodecCppTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("C++ compiler unavailable")
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        source = Path(cls.directory.name) / "codec.cpp"
        source.write_text('''#include "nokia_record_codec.h"
#include <iostream>
#include <iomanip>
#include <string>
int main() {
    std::string a,b,c;
    while (std::cin >> a >> b >> c) {
        nokia_dct3_record_codec::record x{},t{},s{};
        for (unsigned i=0;i<12;++i) {
            x[i]=std::stoul(a.substr(i*2,2),nullptr,16);
            t[i]=std::stoul(b.substr(i*2,2),nullptr,16);
            s[i]=std::stoul(c.substr(i*2,2),nullptr,16);
        }
        for (auto v:nokia_dct3_record_codec::inverse(x,t,s))
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(v);
        std::cout << "\\n";
    }
}
''')
        cls.binary = Path(cls.directory.name) / "codec"
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I", str(ROOT / "driver"), str(source), "-o", str(cls.binary)],
                       check=True, capture_output=True, text=True)

    def test_compiled_inverse_matches_independent_word_model(self):
        rng = random.Random(8250)
        vectors = [(bytes.fromhex("e7b24d8900000000acadab4c"),
                    bytes.fromhex("50f36525d2b1c1b609aeff4c"),
                    bytes.fromhex("d0162c58b071e2d55a67ce8d"))]
        vectors += [tuple(bytes(rng.randrange(256) for _ in range(12))
                          for _ in range(3)) for _ in range(100)]
        text = "\n".join(" ".join(v.hex() for v in vector) for vector in vectors)
        result = subprocess.run([str(self.binary)], input=text, capture_output=True,
                                text=True, check=True)
        expected = []
        for plain, table, schedule in vectors:
            words = inverse_transform_words(struct.unpack(">6H", plain),
                                            struct.unpack(">6H", table),
                                            tuple(v * 0x101 for v in schedule))
            expected.append(struct.pack(">6H", *words).hex())
        self.assertEqual(result.stdout.splitlines(), expected)
        self.assertEqual(expected[0], "473137e5156e62c245946a70")


if __name__ == "__main__":
    unittest.main()
