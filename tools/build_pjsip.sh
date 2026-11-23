#!/bin/sh
# Build the reviewed PJSIP release and its pjsua2 Python binding in
# $SIP_PROJECT_DIR (see docs/external_call_bridge.md). Run by `make sip-build`.
set -eu
archive="$SIP_PROJECT_DIR.tar.gz"
mkdir -p "$SIP_PROJECT_DIR"
[ -f "$archive" ] || curl -L --fail -o "$archive" \
	https://codeload.github.com/pjsip/pjproject/tar.gz/refs/tags/2.16
echo "$SIP_ARCHIVE_SHA256  $archive" | sha256sum -c -
tar -xzf "$archive" -C "$SIP_PROJECT_DIR" --strip-components=1
"$VENV/bin/python" -m pip install swig setuptools
cd "$SIP_PROJECT_DIR"
./configure --disable-video --disable-sound --disable-ssl CFLAGS=-fPIC CXXFLAGS=-fPIC
make -j1 dep
make -j1
cd pjsip-apps/src/swig/python
PATH="$VENV/bin:$PATH" make -j1 PYTHON_EXE="$VENV/bin/python"
