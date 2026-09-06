import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


HARNESS = r'''
#include "gsm_supplementary.h"

#include <cstdio>

int main()
{
	const std::uint8_t query[] = {
		0x1b, 0x7b, 0x1c, 0x0d, 0xa1, 0x0b, 0x02, 0x01, 0x01,
		0x02, 0x01, 0x0e, 0x30, 0x03, 0x04, 0x01, 0x21, 0x7f, 0x01, 0x00
	};
	auto request = gsm::ss::parse_register(query, sizeof(query));
	if (!request.valid || request.transaction != 0x1b || request.invoke_id != 1 ||
			request.operation_code != gsm::ss::operation::interrogate_ss ||
			request.service_code != 0x21)
		return 1;
	auto response = gsm::ss::interrogate_result(request, false);
	for (unsigned i = 0; i < response.length; ++i)
		std::printf("%02x", response.data[i]);
	std::puts("");

	std::uint8_t malformed[sizeof(query)];
	for (unsigned i = 0; i < sizeof(query); ++i) malformed[i] = query[i];
	malformed[5] = 0x20;
	if (gsm::ss::parse_register(malformed, sizeof(malformed)).valid)
		return 2;
	return 0;
}
'''


class GsmSupplementaryTest(unittest.TestCase):
    def test_interrogate_round_trip_and_malformed_length(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "test.cpp"
            binary = pathlib.Path(directory) / "test"
            source.write_text(HARNESS)
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-Idriver", str(source), "driver/gsm_supplementary.cpp",
                "-o", str(binary),
            ], cwd=ROOT, check=True)
            result = subprocess.run(
                [str(binary)], cwd=ROOT, check=True, text=True,
                stdout=subprocess.PIPE)
        self.assertEqual(
            "9b2a1c0da20b020101300602010e800100\n", result.stdout)


if __name__ == "__main__":
    unittest.main()
