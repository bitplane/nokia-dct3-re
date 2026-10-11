import unittest

from tools.run_noki6210_acceptance import check_plmn_reject


class PlmnRejectTest(unittest.TestCase):
    def fixture(self, home=False):
        storage = bytearray(3560)
        storage[1610] = 3
        storage[3548:] = b'\xff' * 12
        text = ('RX enqueue type=80 data=05040b\n'
                'LAPDm Location Updating Reject acknowledged nr=1\n'
                'LAPDm Channel Release acknowledged nr=2\n')
        if not home:
            storage[3548:3551] = bytes.fromhex('130062')
            text += 'update-binary fid=6f7b offset=0 length=12\n'
        return text, storage

    def test_visited_rejection(self):
        check_plmn_reject(*self.fixture())

    def test_home_rejection_does_not_forbid_home(self):
        check_plmn_reject(*self.fixture(home=True), home=True)

    def test_missing_or_wrong_order_events(self):
        text, storage = self.fixture()
        for line in text.splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_plmn_reject(text.replace(line, ''), storage)
        update = text.splitlines()[-1]
        with self.assertRaises(ValueError):
            check_plmn_reject(update + '\n' + text.replace(update, ''), storage)

    def test_acceptance_invalidates_rejection_proof(self):
        text, storage = self.fixture()
        with self.assertRaises(ValueError):
            check_plmn_reject(text + 'LAPDm Location Updating Accept acknowledged nr=1', storage)

    def test_wrong_or_partial_storage(self):
        text, storage = self.fixture()
        for changed in (storage[:-1], storage + b'\x00', bytes(3560)):
            with self.subTest(size=len(changed)), self.assertRaises(ValueError):
                check_plmn_reject(text, changed)

    def test_home_must_not_be_written(self):
        with self.assertRaises(ValueError):
            check_plmn_reject(*self.fixture(), home=True)

    def test_location_status_must_be_rejected(self):
        text, storage = self.fixture()
        storage[1610] = 0
        with self.assertRaises(ValueError):
            check_plmn_reject(text, storage)


if __name__ == '__main__':
    unittest.main()
