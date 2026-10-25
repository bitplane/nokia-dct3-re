"""Keep the processor/handset documentation split navigable; no runtime claims."""
from pathlib import Path
import re
import unittest
from urllib.parse import unquote, urlsplit


class CpuDocumentLinkTests(unittest.TestCase):
    def test_local_section_links_resolve_after_ownership_moves(self):
        docs = Path(__file__).resolve().parents[1] / 'docs'
        names = ('5510_bringup.md', 'rom4_dsp_loader.md', 'c54x_execution_contracts.md')
        for name in names:
            source = docs / name
            text = source.read_text()
            for target in re.findall(r'\]\(([^()]+)\)', text):
                link = urlsplit(target)
                if link.scheme or not link.fragment:
                    continue
                destination = (source.parent / unquote(link.path)) if link.path else source
                if destination.name not in names:
                    continue
                with self.subTest(source=name, target=target):
                    self.assertTrue(destination.is_file())
                    headings = re.findall(r'^#{1,6} (.+)$', destination.read_text(), re.M)
                    anchors = {re.sub(r'[^\w -]', '', h.lower()).replace(' ', '-') for h in headings}
                    self.assertIn(unquote(link.fragment), anchors)


if __name__ == '__main__':
    unittest.main()
