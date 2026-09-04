import struct
import tempfile
import unittest
from pathlib import Path

from tools.c54x_rom4_snapshot_check import ENTRY_RESPONSE, check, parse_registers


REGS = """pc=0x4B73 sp=0x1EC3 st0=0x201F st1=0x2103 pmst=0xFFAC imr=0x53FF ifr=0x0000
ar0=0x0001 ar1=0xB0BC ar2=0x0825 ar3=0x06E3 ar4=0x001A ar5=0x12CA ar6=0x06E3 ar7=0x0000
a=0x0000004B73 b=0xFFFFFFFFFE
"""


class C54xRom4SnapshotCheckTest(unittest.TestCase):
    def test_register_parser(self):
        self.assertEqual(parse_registers(REGS)["pmst"], 0xFFAC)

    def test_accepts_semantic_snapshot_without_digest_gate(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "entry"
            (Path(f"{prefix}.prog")).write_bytes(bytes(2))
            api = bytearray(0x2000)
            struct.pack_into(f">{len(ENTRY_RESPONSE)}H", api, 0x1400, *ENTRY_RESPONSE)
            (Path(f"{prefix}.api")).write_bytes(api)
            (Path(f"{prefix}.data")).write_bytes(bytes(2))
            (Path(f"{prefix}.regs")).write_text(REGS)
            self.assertEqual(check(prefix, require_hashes=False)["response_words"], 11)


if __name__ == "__main__":
    unittest.main()
