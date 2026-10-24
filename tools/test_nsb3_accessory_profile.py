from pathlib import Path
import tempfile
import unittest

from tools.nsb3_accessory_profile import (PROFILES, check_trace, configuration,
                                          metadata)
from tools.test_nsb3_pin_check import trace as authentication_trace


def trace(profile):
    raw, external, state, _ = PROFILES[profile]
    rows = [authentication_trace(True), f'ccont_adc0_config: raw={raw:03x} external_input=1',
            f'mad2_external_config: raw={external:x} external_input=1']
    writes = [('00', '0020010e'), ('0f', '002ad1aa')]
    if state != 15:
        writes.append((f'{state:02x}', '00273d96' if state == 16 else '00273df6'))
    rows += [f'nsb3_accessory_state: value={value} pc={pc} t=1.0 observer_writes=0' for value, pc in writes]
    rows += [f'nsb3_accessory_sample: second={time} state={state:02x} sample={raw:04x} external={external:02x} t={time}.0 observer_writes=0'
             for time in (5, 10, 24, 36, 40)]
    return '\n'.join(rows)


class Nsb3AccessoryTest(unittest.TestCase):
    def test_three_independent_inputs_and_firmware_owned_transitions(self):
        for profile in PROFILES:
            with self.subTest(profile=profile):
                check_trace(trace(profile), profile)
                self.assertFalse(metadata(profile)['handset_storage_changed'])
                self.assertFalse(metadata(profile)['physical_open_voltage_or_pin_proved'])

    def test_wrong_branch_pin_physical_input_missing_sample_and_reset_fail(self):
        good = trace('both-high')
        for bad in (good.replace('external=02', 'external=00'),
                    good.replace('sample=03ff', 'sample=0000'),
                    good.replace('state=0f', 'state=20'),
                    good.replace('pc=002ad1aa', 'pc=00273df6'),
                    good.replace('second=40', 'second=41'),
                    good.replace('correct=1 key=Keypad 1', 'correct=1 key=Keypad 5'),
                    good + '\nnsb3_board_reset_request: data=04'):
            with self.assertRaises(ValueError):
                check_trace(bad, 'both-high')
        with self.assertRaises(ValueError):
            check_trace(trace('analog-high'), 'both-high')

    def test_configuration_preserves_other_inputs_and_allows_omitted_zero_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            source, run = (Path(directory) / name for name in ('source.cfg', 'run.cfg'))
            prefix = '<mameconfig><system name="nsb3r4t"><input><keyboard enabled="1"/>'
            suffix = '</input></system></mameconfig>'
            source.write_text(prefix + suffix)
            run.write_text(prefix + suffix)
            configuration(run, source, 'low')
            analog = '<port tag=":RESEARCH_ADC0" type="CONFIG" mask="1023" defvalue="0" value="1023"/>'
            status = '<port tag=":RESEARCH_EXT_STATUS" type="CONFIG" mask="7" defvalue="0" value="2"/>'
            run.write_text(prefix + analog + status + suffix)
            configuration(run, source, 'both-high')
            for ports in (analog, analog + status.replace('mask="7"', 'mask="3"'),
                          analog + analog + status,
                          analog + status + '<port tag=":SATCFG" value="1"/>'):
                run.write_text(prefix + ports + suffix)
                with self.assertRaises(ValueError):
                    configuration(run, source, 'both-high')


if __name__ == '__main__':
    unittest.main()
