import unittest

from tools.c54x_rom4_rf_delivery_check import check


TRACE = """rom4_rf_native_publication: t=1.0 type=008a bytes=2 words=006d
rom4_rf_native_publish_exit: address=3807 t=1.1 type=008a bytes=2 producer=00ab consumer=00a9
rom4_rf_mcu_ring_write: t=1.2 address=000101c8 value=000000aa mask=0000ffff
rom4_rf_mcu_ring_write: t=1.3 address=000101c8 value=000000ab mask=0000ffff
rom4_rf_status_header_write: t=1.4 pc=0026be48 address=00101438 value=13900000 mask=ffff0000
rom4_rf_post_fetch: t=1.5 address=00254c84 pc=00254c84 r0=0000000a r1=00101438 lr=0026be5d
rom4_rf_task10_lifecycle: t=1.6 pc=00211a9a r0=00101438 status=1390
"""


class DeliveryTests(unittest.TestCase):
    def test_complete_chain(self):
        self.assertEqual(check(TRACE), ("00101438", "006d"))

    def test_every_link_required(self):
        lines = TRACE.splitlines()
        for index in range(len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check("\n".join(lines[:index] + lines[index + 1:]))

    def test_identity_and_order(self):
        for old, new in (("r1=00101438", "r1=00101480"),
                         ("status=1390", "status=138f"),
                         ("t=1.6", "t=0.5"),
                         ("producer=00ab", "producer=00ac"),
                         ("mask=0000ffff", "mask=ffff0000")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(TRACE.replace(old, new))

    def test_wrap(self):
        wrapped = TRACE.replace("consumer=00a9", "consumer=00e3")
        wrapped = wrapped.replace("producer=00ab", "producer=0081")
        wrapped = wrapped.replace("value=000000aa", "value=00000080")
        wrapped = wrapped.replace("value=000000ab", "value=00000081")
        self.assertEqual(check(wrapped)[1], "006d")


if __name__ == "__main__":
    unittest.main()
