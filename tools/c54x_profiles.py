"""Conformance BIOS declarations own selection, never instruction results."""
import ast
from functools import lru_cache
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
STRING = r'"(?:\\.|[^"\\])*"'


def harness_sources(root=ROOT):
    """Compiled bench modules and shared declarations, in reproducible order."""
    paths = [path for directory in ('c54x', 'firmware')
             for path in (root / 'tests' / directory).glob('tms320c54x_test*')
             if path.suffix in ('.cpp', '.h')]
    backend = root / 'tests/devices/nokia_dsp_c54x_test.cpp'
    if backend.is_file():
        paths.append(backend)
    return tuple(sorted(paths, key=lambda path: path.name))


def declarations(source):
    source = re.sub(STRING + r'|//[^\n]*|/\*.*?\*/',
                    lambda match: match[0] if match[0].startswith('"') else
                    ''.join('\n' if char == '\n' else ' ' for char in match[0]),
                    source, flags=re.S)
    result = {}
    bios_pattern = re.compile(r'\bROM_SYSTEM_BIOS\s*\(\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*('
                              + STRING + r')\s*,\s*(' + STRING + r')\s*\)')
    for machine, body in re.findall(r'^\s*ROM_START\s*\(\s*(\w+)\s*\)(.*?)\bROM_END\b',
                                    source, re.M | re.S):
        if re.search(r'^\s*#\s*(if|ifdef|ifndef|else|elif)\b', body, re.M):
            raise ValueError('conditional conformance BIOS table: ' + machine)
        entries = [dict(index=int(index, 16 if index.startswith('0x') else 10),
                        name=ast.literal_eval(name), description=ast.literal_eval(description))
                   for index, name, description in bios_pattern.findall(body)]
        if len(entries) != len(re.findall(r'\bROM_SYSTEM_BIOS\s*\(', body)):
            raise ValueError('unsupported conformance BIOS declaration: ' + machine)
        if not entries:
            continue
        if any(len(entry['name']) > 16 for entry in entries):
            raise ValueError('conformance BIOS name exceeds MAME limit of 16 characters')
        if machine in result:
            raise ValueError('duplicate conformance machine: ' + machine)
        if (len({entry['name'] for entry in entries}) != len(entries) or
                len({entry['index'] for entry in entries}) != len(entries)):
            raise ValueError('duplicate conformance BIOS name/index: ' + machine)
        if [entry['index'] for entry in entries] != list(range(len(entries))):
            raise ValueError('non-sequential conformance BIOS indices: ' + machine)
        result[machine] = entries
    return result


@lru_cache(maxsize=1)
def declared_machines(root=ROOT):
    result = {}
    for source in harness_sources(root):
        if source.suffix != '.cpp':
            continue
        found = declarations(source.read_text())
        duplicates = result.keys() & found.keys()
        if duplicates:
            raise ValueError('duplicate conformance machines: ' + ', '.join(sorted(duplicates)))
        result.update(found)
    return result


def machine_for_profile(profile):
    matches = [machine for machine, entries in declared_machines().items()
               if any(entry['name'] == profile for entry in entries)]
    if len(matches) != 1:
        raise ValueError(f'conformance BIOS {profile!r} needs one declared owner; found {matches}')
    return matches[0]
