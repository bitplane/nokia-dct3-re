import unittest
import tempfile
from pathlib import Path
import xml.etree.ElementTree as ET
from tools.run_noki6250_fdn import phase_plan,configure_dial_peer


class FdnPhasePlanTest(unittest.TestCase):
    def test_dial_controls_branch_from_saved_on_and_physically_disabled_cards(self):
        phases=phase_plan(dial=True)
        self.assertEqual(phases[:6],phase_plan(records=True))
        self.assertEqual([(p.name,p.source) for p in phases[6:]],
                         [('listed','read'),('denied','read'),('off-record','read'),('off-call','off-record')])
        self.assertTrue(phases[8].saved_record)
        self.assertEqual(phases[8].validity,2)
        self.assertEqual([p.dial_mode for p in phases[6:]],['listed','denied',None,'off'])
        for phase in phases:
            self.assertEqual(sum(value is not None for value in (phase.validity,phase.record_mode,phase.dial_mode)),1)

    def test_lab_peer_config_accepts_default_omission_without_changing_other_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'nhm3hle.cfg'
            for host in ('','<port tag=":CALLHOST" type="CONFIG" mask="1" defvalue="0" value="1"/>'):
                path.write_text('<mameconfig version="10"><system name="nhm3hle"><input>'+host+
                    '<port tag=":NEIGHBORCFG" type="CONFIG" mask="512" value="512"/></input></system></mameconfig>')
                configure_dial_peer(path)
                configure_dial_peer(path)
                tree=ET.parse(path)
                self.assertEqual(len(tree.findall("./system/input/port[@tag=':CALLHOST']")),1)
                self.assertEqual(tree.find("./system/input/port[@tag=':CALLHOST']").get('value'),'0')
                self.assertEqual(tree.find("./system/input/port[@tag=':NEIGHBORCFG']").get('value'),'512')
            path.write_text('<mameconfig version="10"><system name="nhm3hle"/></mameconfig>')
            configure_dial_peer(path)
            self.assertEqual(ET.parse(path).find("./system/input/port[@tag=':CALLHOST']").get('value'),'0')
            wrong='<mameconfig version="10"><system name="npe3hle"/></mameconfig>'
            path.write_text(wrong)
            with self.assertRaises(ValueError):configure_dial_peer(path)
            self.assertEqual(path.read_text(),wrong)

    def test_existing_four_and_six_process_paths_keep_their_own_sources_and_timings(self):
        ordinary=phase_plan()
        self.assertEqual([(p.name,p.source,p.seconds,p.validity) for p in ordinary],
                         [('on',None,68,0),('cold-on','on',58,1),('off','cold-on',68,2),('cold-off','off',58,3)])
        records=phase_plan(records=True)
        self.assertEqual(records[:4],ordinary)
        self.assertEqual([(p.name,p.source,p.seconds,p.record_mode) for p in records[4:]],
                         [('write','on',88,'write'),('read','write',70,'read')])

    def test_complete_lifecycle_is_a_unique_topological_cold_storage_graph(self):
        phases=phase_plan(lifecycle=True)
        self.assertEqual(len(phases),11)
        seen=set()
        for phase in phases:
            self.assertNotIn(phase.name,seen)
            if phase.source is not None:self.assertIn(phase.source,seen)
            self.assertNotEqual(phase.validity is None,phase.record_mode is None)
            seen.add(phase.name)
        self.assertEqual([(p.name,p.source) for p in phases[6:]],
                         [('edit','read'),('edited-read','edit'),('cancel','edited-read'),('erase','cancel'),('empty','erase')])
        self.assertEqual(phases,phase_plan(records=True,lifecycle=True))

    def test_rejections_recover_only_from_their_own_retry_loss(self):
        phases=phase_plan(rejections=True)
        self.assertEqual(len(phases),8)
        by_name={p.name:p for p in phases}
        self.assertTrue(by_name['reject-on'].rejected)
        self.assertIsNone(by_name['reject-on'].source)
        self.assertEqual((by_name['on'].source,by_name['on'].pin2_attempts),('reject-on',2))
        self.assertEqual((by_name['wrong'].source,by_name['wrong'].record_mode),('on','wrong'))
        self.assertEqual((by_name['write'].source,by_name['write'].pin2_attempts),('wrong',2))
        self.assertEqual(by_name['read'].source,'write')
        self.assertEqual(len(phase_plan(rejections=True,lifecycle=True)),13)


if __name__=='__main__':unittest.main()
