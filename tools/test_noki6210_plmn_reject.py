import unittest

from tools.run_noki6210_acceptance import check_plmn_reject, check_forbidden_selection


class PlmnRejectTest(unittest.TestCase):
    def fixture(self, home=False):
        storage = bytearray(3562)
        storage[1610] = 3
        storage[3548:3560] = b'\xff' * 12
        text = ('RX enqueue type=80 data=05040b\n'
                'LAPDm Location Updating Reject acknowledged nr=1\n'
                'LAPDm Channel Release acknowledged nr=2\n')
        if not home:
            storage[3548:3551] = bytes.fromhex('00f120')
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
        for changed in (storage[:-1], storage + b'\x00', bytes(3562)):
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

    def cold_fixture(self, forbidden):
        _, previous = self.fixture()
        storage = bytearray(previous)
        text = ('read-binary fid=6f7b offset=0 length=12\n'
                'RX enqueue type=80 data=061b00f120\n')
        if not forbidden:
            storage[3548:3560] = b'\xff' * 12
            storage[1604:1609] = bytes.fromhex('00f1200001')
            storage[1610] = 0
            text += ('TX packet type=0c payload=8\n'
                     'LAPDm Location Updating Accept acknowledged nr=1\n'
                     'LAPDm Channel Release acknowledged nr=2\n')
        return text, storage, previous

    def test_retained_and_cleared_cold_controls(self):
        for forbidden in (True, False):
            text, storage, previous = self.cold_fixture(forbidden)
            check_forbidden_selection(text, storage, forbidden=forbidden, previous=previous)

    def test_forbidden_access_is_failure(self):
        text, storage, previous = self.cold_fixture(True)
        for event in ('TX packet type=0c ', 'Location Updating Accept acknowledged'):
            with self.subTest(event=event), self.assertRaises(ValueError):
                check_forbidden_selection(text + event, storage, forbidden=True, previous=previous)

    def test_cold_missing_evidence_is_failure(self):
        for forbidden in (True, False):
            text, storage, previous = self.cold_fixture(forbidden)
            for line in text.splitlines():
                with self.subTest(forbidden=forbidden, line=line), self.assertRaises(ValueError):
                    check_forbidden_selection(text.replace(line, ''), storage,
                                              forbidden=forbidden, previous=previous)

    def test_forbidden_storage_must_be_exact(self):
        text, storage, previous = self.cold_fixture(True)
        storage[0] ^= 1
        with self.assertRaises(ValueError):
            check_forbidden_selection(text, storage, forbidden=True, previous=previous)

    def test_cleared_control_requires_valid_location(self):
        text, storage, previous = self.cold_fixture(False)
        for offset in (1604, 1610, 3548):
            changed = bytearray(storage)
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                check_forbidden_selection(text, changed, forbidden=False, previous=previous)


if __name__ == '__main__':
    unittest.main()
