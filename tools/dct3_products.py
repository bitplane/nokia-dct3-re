"""Product identities declared by the MAME driver."""

from functools import lru_cache
from pathlib import Path
import re


DRIVER = Path(__file__).resolve().parents[1] / "driver/nokia_dct3.cpp"
SYSTEM_RE = re.compile(r"^SYST\(\s*\d+\s*,\s*noki(\d+)\s*,", re.MULTILINE)


@lru_cache(maxsize=1)
def product_ids() -> frozenset[str]:
    return frozenset(SYSTEM_RE.findall(DRIVER.read_text()))
