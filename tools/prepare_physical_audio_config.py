"""Copy fixture settings without host-specific mixer routes into a run directory."""

import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


def prepare(source, destination):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if source == destination:
        raise ValueError("audio configuration must not overwrite its fixture")
    paths = sorted(source.glob("*.cfg"))
    if not paths:
        raise ValueError(f"no configuration fixtures in {source}")
    destination.mkdir(parents=True, exist_ok=True)
    for path in paths:
        tree = ET.parse(path)
        for system in tree.getroot().findall("system"):
            for mixer in system.findall("mixer"):
                system.remove(mixer)
        tree.write(destination / path.name, encoding="utf-8", xml_declaration=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.source, args.destination)
