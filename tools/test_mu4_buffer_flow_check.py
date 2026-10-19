import unittest

from tools.mu4_buffer_flow_check import check


def fixture():
    lines = ['mu4_buffer_scope: descriptor_first=0.825 last=10.0 max_nodes=12 max_writes=131072 boundary=changed_data_write identical_full_writes=deduplicated context_pc=not_owner observer_writes=0']
    for base,links in ((0x1622,[0,0x1616]),(0x1616,[0x1622,0x160a]),(0x160a,[0x1616,0])):
        words = [0]*8+links+[0,0]
        lines.append(f'mu4_buffer_node: base={base:04x} words={",".join(f"{word:04x}" for word in words)} t=0.833333333 observer_writes=0')
    lines += ['mu4_buffer_write: ordinal=1 bus_ordinal=1 kind=control base=0000 address=15e0 value=00ba mask=ffff t=0.85 context_pc=02068e observer_writes=0',
              'mu4_buffer_write: ordinal=2 bus_ordinal=4 kind=descriptor base=1622 address=1628 value=1c00 mask=ffff t=0.86 context_pc=020877 observer_writes=0',
              'mu4_buffer_closed: writes=2 bus_writes=5 capped=0 observer_writes=0',
              'mu4_buffer_terminal: closed=1 writes=2 bus_writes=5 capped=0 illegal=0 observer_writes=0']
    return '\n'.join(lines)


class BufferFlowTest(unittest.TestCase):
    def test_changed_writes_are_distinct_from_total_bus_writes(self):
        result = check(fixture())
        self.assertEqual(result['changed_writes'],2)
        self.assertEqual(result['observed_bus_writes'],5)
        self.assertEqual(result['last_fields']['1628']['value'],'1c00')
        self.assertIn('not retained instruction ownership',result['scope'])

    def test_corruption_cap_domains_and_terminal_fail_closed(self):
        text = fixture()
        for changed in (text+'\0',text.replace('capped=0','capped=1'),
                        text.replace('bus_ordinal=4','bus_ordinal=1'),
                        text.replace('address=1628','address=1638'),
                        text.replace('mask=ffff','mask=00ff'),
                        text.replace('illegal=0','illegal=1'),
                        text.replace('writes=2 bus_writes=5','writes=3 bus_writes=5'),
                        text.replace('closed=1','closed=0'),
                        text.replace('t=0.86','t=nan'),text+'\nLUA ERROR',
                        text.replace('mu4_buffer_node: base=160a','missing_node: base=160a')):
            with self.subTest(changed=changed[-100:]), self.assertRaises(ValueError):
                check(changed)


if __name__ == '__main__':
    unittest.main()
