import unittest
from tools.nsk3_nominal_input import config
from tools.nsk3_nominal_check import check_trace


class NominalInputTest(unittest.TestCase):
    def test_configuration_changes_only_external_adc2(self):
        root=config().getroot()
        self.assertEqual(root.find('system').attrib,{'name':'nsk3r4t'})
        ports=root.findall('.//port')
        self.assertEqual(len(ports),1)
        self.assertEqual(ports[0].attrib,{'tag':':RESEARCH_ADC2','type':'CONFIG',
                                        'mask':'1023','defvalue':'1023','value':'554'})

    def test_card_insertion_is_an_explicit_separate_input(self):
        ports=config(True).getroot().findall('.//port')
        self.assertEqual(len(ports),2)
        self.assertEqual(ports[1].attrib,{'tag':':SIM_REMOVED','type':'OTHER',
                                        'mask':'1','defvalue':'1','value':'0'})

    def test_readiness_and_contact_requirements(self):
        trace='\n'.join([
            'ccont_adc2_config: raw=22a external_input=1',
            'nsk3_ui_analog_sample: accepted=01 first=0dfd second=0dfd gain=3f800000 offset=00000000',
            'nsk3_ui_startup_report: value=0014 context=0010c4a0',
            'nsk3_ui_second_group: bits=0f cycles=0',
            'nsk3_ui_contact: down=1 column=1 mask=04 firmware_writes=0',
            'nsk3_ui_contact: down=0 column=1 mask=04 firmware_writes=0',
            *[f'nsk3_ui_sample: t={t} lcd=3692 scan={6 if t<9 else 42} rows={13 if t<9 else 79} pc=002a02fc columnmask=20' for t in range(1,17)]])
        check_trace(trace)
        for bad in (trace.replace('raw=22a','raw=3ff'),trace.replace('bits=0f','bits=0e'),
                    trace.replace('accepted=01','accepted=00'),trace.replace('scan=42','scan=6'),
                    trace.replace('rows=79','rows=13'),trace.replace('down=0','down=1'),
                    trace.replace('gain=3f800000','gain=00000000')):
            with self.assertRaises(ValueError):check_trace(bad)


if __name__=='__main__':unittest.main()
