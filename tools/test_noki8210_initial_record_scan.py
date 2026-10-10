import unittest

from tools.noki8210_radio_contract import scan_initial_record_flags


class InitialRecordScanTests(unittest.TestCase):
    def test_every_single_flag_position(self):
        for column in range(9):
            for index in range(7):
                rows = [bytearray(9) for _ in range(7)]
                rows[index][column] = 1
                expected = (1, index) if column == 0 else (column, 7)
                with self.subTest(column=column, index=index):
                    self.assertEqual(scan_initial_record_flags(rows), expected)

    def test_column_priority_precedes_row_priority(self):
        rows = [bytearray(9) for _ in range(7)]
        rows[0][4] = 1
        rows[6][0] = 1
        self.assertEqual(scan_initial_record_flags(rows), (1, 6))
        rows[2][0] = 0xff
        self.assertEqual(scan_initial_record_flags(rows), (1, 2))

    def test_observed_gsm_and_dcs_tables(self):
        rows = [bytearray(9) for _ in range(7)]
        rows[5][4] = 1
        self.assertEqual(scan_initial_record_flags(rows), (4, 7))
        rows[5][4] = 0
        rows[5][8] = 1
        rows[6][0] = rows[6][4] = 1
        self.assertEqual(scan_initial_record_flags(rows), (1, 6))

    def test_empty_and_invalid_dimensions(self):
        self.assertEqual(scan_initial_record_flags([bytes(9)] * 7), (8, 7))
        for rows in ([], [bytes(9)] * 6, [bytes(8)] * 7):
            with self.assertRaises(ValueError):
                scan_initial_record_flags(rows)


if __name__ == '__main__':
    unittest.main()
