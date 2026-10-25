-- Own NSB-1 REFRESH route; filesystem re-reading needs a larger wire cap.
_G.nsb1_toolkit_wire_limit=16384
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsb1_toolkit_menu.lua')
local machine=manager.machine
local actions=coroutine.create(function()
    assert(emu.wait(88))
    machine.screens[':screen']:snapshot('nsb1_toolkit_refresh_88.png')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('nsb1_toolkit_refresh_91.png')
end)
assert(coroutine.resume(actions))
