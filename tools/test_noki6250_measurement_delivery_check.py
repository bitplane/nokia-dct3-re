import unittest

from tools.noki6250_measurement_delivery_check import verify


PUBLICATION = 'RX enqueue type=8b payload=166 producer=0a4 data=00 t=10.364597\n'
DELIVERY = (
    'FIQ0 notify producer=0a4 consumer=0b4 t=10.365000\n'
    'RAM W off=1ca data=00a4 t=10.365100\n'
    '6250_pin_rssi_route: enable=01 message=00104000 t=10.366000\n'
    '6250_pin_radio_post: target=0e class=8b caller=004649c7 '
    'message=00104000 t=10.366100\n'
)


class MeasurementDeliveryTest(unittest.TestCase):
    def test_complete_chain(self):
        self.assertEqual(verify(PUBLICATION + DELIVERY)['producer'], 0xa4)

    def test_enqueue_is_not_delivery(self):
        with self.assertRaisesRegex(ValueError, 'notification'):
            verify(PUBLICATION)

    def test_earlier_notification_does_not_count(self):
        with self.assertRaisesRegex(ValueError, 'notification'):
            verify(DELIVERY + PUBLICATION)

    def test_unconsumed_packet(self):
        with self.assertRaisesRegex(ValueError, 'consumer advance'):
            verify(PUBLICATION + DELIVERY.replace('data=00a4', 'data=00a3'))

    def test_wrong_envelope_does_not_count(self):
        with self.assertRaisesRegex(ValueError, 'forward'):
            verify(PUBLICATION + DELIVERY.replace('message=00104000 t=10.366100',
                                                  'message=00104010 t=10.366100'))

    def test_disabled_route(self):
        with self.assertRaisesRegex(ValueError, 'handler entry'):
            verify(PUBLICATION + DELIVERY.replace('enable=01', 'enable=00'))

    def test_later_measurement_cannot_supply_evidence(self):
        with self.assertRaisesRegex(ValueError, 'notification'):
            verify(PUBLICATION + PUBLICATION + DELIVERY)


if __name__ == '__main__':
    unittest.main()
