import unittest

from tools import mad2_residual_census


class Mad2ResidualCensusTests(unittest.TestCase):
	def report(self, label="fixture"):
		counts = {}
		for offset in mad2_residual_census.REVIEWED_OFFSETS:
			counts[f"{offset:02x}_read"] = 1
			counts[f"{offset:02x}_write"] = 1
		return {"label": label, "counts": counts}

	def test_markdown_states_evidence_boundary(self):
		text = mad2_residual_census.markdown([self.report()])
		self.assertIn("physical owner is not recoverable", text)
		self.assertIn("not safe targets", text)

	def test_validation_rejects_external_status_write(self):
		reports = []
		for label, _ in mad2_residual_census.DEFAULT_ROMS:
			report = self.report(label)
			report["counts"].update({
				"03_read": 0, "03_write": 3,
				"0d_read": 10, "0d_write": 10,
				"0e_read": 5, "0e_write": 0,
			})
			reports.append(report)
		mad2_residual_census.validate(reports)
		reports[0]["counts"]["0e_write"] = 1
		with self.assertRaisesRegex(ValueError, "external-status"):
			mad2_residual_census.validate(reports)


if __name__ == "__main__":
	unittest.main()
