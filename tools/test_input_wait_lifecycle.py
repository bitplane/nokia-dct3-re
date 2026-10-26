"""Execute the harness's wait policy with Lua, independently of MAME timers."""
import ctypes
import ctypes.util
from pathlib import Path
import unittest


LUA = ctypes.util.find_library('lua5.4')


@unittest.skipUnless(LUA, 'system Lua 5.4 library unavailable; native gates remain required')
class InputWaitLifecycleTest(unittest.TestCase):
    def test_every_harness_wait_checks_the_shared_stop_policy(self):
        harness = (Path(__file__).resolve().parents[1] /
                   'mame_nokia_dct3_input_exerciser.lua').read_text()
        self.assertEqual(harness.count('emu.wait('), 1)
        sites = [line for line in harness.splitlines() if 'wait_for(' in line
                 and not line.lstrip().startswith(('local function', '--'))]
        self.assertGreater(len(sites), 40)
        for line in sites:
            self.assertIn('if not wait_for(', line)

    def execute(self, setup, assertions):
        harness = (Path(__file__).resolve().parents[1] /
                   'mame_nokia_dct3_input_exerciser.lua').read_text()
        helper = 'local function wait_for' + harness.split(
            'local function wait_for', 1)[1].split('\nend\n', 1)[0] + '\nend\n'
        lua = ctypes.CDLL(LUA)
        lua.luaL_newstate.restype = ctypes.c_void_p
        lua.luaL_openlibs.argtypes = [ctypes.c_void_p]
        lua.luaL_loadstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        lua.lua_pcallk.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_ssize_t, ctypes.c_void_p]
        lua.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
        lua.lua_tolstring.restype = ctypes.c_char_p
        lua.lua_close.argtypes = [ctypes.c_void_p]
        state = lua.luaL_newstate()
        self.assertTrue(state)
        try:
            lua.luaL_openlibs(state)
            status = lua.luaL_loadstring(state, (setup + helper + assertions).encode())
            if not status:
                status = lua.lua_pcallk(state, 0, 0, 0, 0, None)
            self.assertEqual(status, 0, lua.lua_tolstring(state, -1, None))
        finally:
            lua.lua_close(state)

    def test_expired_wait_continues(self):
        self.execute('machine={phase="running"}; emu={wait=function(t) assert(t==0.01); return true end}\n',
                     'assert(wait_for(0.01))')

    def test_save_load_cancellation_preserves_continuation(self):
        self.execute('machine={phase="running"}; emu={wait=function(t) return false end}\n',
                     'assert(wait_for(0.01))')

    def test_stop_cancellation_never_reenters_the_timer(self):
        self.execute('machine={phase="exit"}; calls=0; touched=0; '
                     'emu={wait=function(t) calls=calls+1; assert(calls==1); return false end}\n',
                     '(function() repeat if not wait_for(0.01) then return end; '
                     'touched=touched+1 until false end)(); assert(calls==1 and touched==0)')


if __name__ == '__main__':
    unittest.main()
