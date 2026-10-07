"""Optional host-only libgsm codec: 33-byte GSM-FR <-> 160 signed PCM samples."""

import ctypes
import ctypes.util
import struct


class GsmFrCodec:
    def __init__(self):
        name = ctypes.util.find_library('gsm')
        if not name:
            raise RuntimeError('libgsm is required for the SIP media bridge')
        self.library = ctypes.CDLL(name)
        self.library.gsm_create.restype = ctypes.c_void_p
        self.library.gsm_destroy.argtypes = [ctypes.c_void_p]
        self.library.gsm_encode.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ctypes.c_short), ctypes.POINTER(ctypes.c_ubyte)]
        self.library.gsm_encode.restype = None
        self.library.gsm_decode.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(ctypes.c_short)]
        self.library.gsm_decode.restype = ctypes.c_int
        self.encoder = self.library.gsm_create()
        self.decoder = self.library.gsm_create()
        if not self.encoder or not self.decoder:
            self.close()
            raise RuntimeError('libgsm state allocation failed')

    def close(self):
        for name in ('encoder', 'decoder'):
            state = getattr(self, name, None)
            if state:
                self.library.gsm_destroy(state)
                setattr(self, name, None)

    def encode(self, pcm: bytes) -> bytes:
        if not self.encoder:
            raise RuntimeError('codec is closed')
        if len(pcm) != 320:
            raise ValueError('GSM-FR requires 160 signed 16-bit PCM samples')
        samples = (ctypes.c_short * 160)(*struct.unpack('=160h', pcm))
        frame = (ctypes.c_ubyte * 33)()
        self.library.gsm_encode(self.encoder, samples, frame)
        return bytes(frame)

    def decode(self, frame: bytes) -> bytes:
        if not self.decoder:
            raise RuntimeError('codec is closed')
        if len(frame) != 33:
            raise ValueError('GSM-FR frame must contain 33 bytes')
        encoded = (ctypes.c_ubyte * 33).from_buffer_copy(frame)
        samples = (ctypes.c_short * 160)()
        if self.library.gsm_decode(self.decoder, encoded, samples):
            raise ValueError('invalid GSM-FR frame')
        return struct.pack('=160h', *samples)

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
