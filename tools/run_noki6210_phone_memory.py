#!/usr/bin/env python3
"""Compatibility entry point; product qualification lives in the shared runner."""
import sys
from pathlib import Path
if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_dct3_phone_memory import (
    main, run, verify_storage, verify_lifecycle_digests, verify_lifecycle_frames,
    LIFECYCLE_FRAMES, EMPTY_SHA256)
if __name__=='__main__':main()
