"""Keep project documentation navigable; no emulation or runtime claims."""
from pathlib import Path
import re
import unittest
from urllib.parse import unquote, urlsplit


def heading_anchors(text):
    anchors = set()
    counts = {}
    fence = None
    for line in text.splitlines():
        marker = re.match(r'^\s*(`{3,}|~{3,})', line)
        if marker:
            kind = marker[1][0]
            if fence is None:
                fence = kind
            elif kind == fence:
                fence = None
            continue
        if fence is not None:
            continue
        heading = re.match(r'^#{1,6} (.+)$', line)
        if not heading:
            continue
        slug = re.sub(r'[^\w -]', '', heading[1].lower()).replace(' ', '-')
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        anchors.add(f'{slug}-{count}' if count else slug)
    return anchors


class CpuDocumentLinkTests(unittest.TestCase):
    def test_local_section_links_resolve_after_ownership_moves(self):
        root = Path(__file__).resolve().parents[1]
        docs = root / 'docs'
        sources = [root / 'README.md', root / 'roms' / 'README.md', *docs.glob('*.md')]
        for source in sources:
            text = source.read_text()
            for target in re.findall(r'\]\(([^()]+)\)', text):
                link = urlsplit(target)
                if link.scheme:
                    continue
                destination = (source.parent / unquote(link.path)) if link.path else source
                if destination.suffix != '.md':
                    continue
                with self.subTest(source=source.relative_to(root), target=target):
                    self.assertTrue(destination.is_file())
                    if link.fragment:
                        self.assertIn(unquote(link.fragment), heading_anchors(destination.read_text()))

    def test_documentation_index_covers_every_contract(self):
        docs = Path(__file__).resolve().parents[1] / 'docs'
        index = (docs / 'README.md').read_text()
        for source in docs.glob('*.md'):
            if source.name != 'README.md':
                with self.subTest(document=source.name):
                    self.assertIn(f'`{source.name}`', index)

    def test_heading_anchors_exclude_code_and_number_duplicates(self):
        self.assertEqual(heading_anchors(
            '# Status\n```sh\n# Not a heading\n```\n## Status\n## `IRQ` / FIQ\n'),
            {'status', 'status-1', 'irq--fiq'})


if __name__ == '__main__':
    unittest.main()
