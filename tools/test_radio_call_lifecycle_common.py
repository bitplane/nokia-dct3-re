import unittest

from tools.radio_call_lifecycle_common import (
    CONNECT,
    DISCONNECT,
    require_count,
    require_ordered,
    laboratory_incoming_setup,
)


class RadioCallLifecycleCommonTest(unittest.TestCase):
    def test_laboratory_calling_number_payload(self):
        self.assertEqual(laboratory_incoming_setup('5551234').hex(),
                         '030504046002008134015c0581551532f4')
        self.assertEqual(laboratory_incoming_setup('447700900123').hex(),
                         '030504046002008134015c0781447700091032')
        for value in ('','+123','\uff11\uff12\uff13','123x','1'*21):
            with self.subTest(value=value),self.assertRaises(ValueError):
                laboratory_incoming_setup(value)
    def test_ordered_checkpoints_return_cursor(self):
        text = "\n".join((
            "GSM service uplink sapi=0 pd=03 message=07 length=2",
            "GSM service uplink sapi=0 pd=03 message=25 length=5",
        ))
        cursor = require_ordered(
            text, (("Answer", CONNECT), ("End", DISCONNECT)), "test")
        self.assertEqual(cursor, len(text))

    def test_ordered_checkpoints_do_not_accept_reversed_lifecycle(self):
        text = "\n".join((
            "GSM service uplink sapi=0 pd=03 message=25 length=5",
            "GSM service uplink sapi=0 pd=03 message=07 length=2",
        ))
        with self.assertRaisesRegex(ValueError, "End"):
            require_ordered(
                text, (("Answer", CONNECT), ("End", DISCONNECT)), "test")

    def test_exact_count_rejects_duplicate_event(self):
        text = "\n".join((
            "GSM service uplink sapi=0 pd=03 message=07 length=2",
            "GSM service uplink sapi=0 pd=03 message=07 length=2",
        ))
        with self.assertRaisesRegex(ValueError, "one Connect"):
            require_count(text, CONNECT, 1, "expected one Connect")


if __name__ == "__main__":
    unittest.main()
