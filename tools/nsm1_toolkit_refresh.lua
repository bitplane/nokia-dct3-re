-- REFRESH rereads the SIM filesystem, exceeding the ordinary RX observation cap.
_G.nsm1_toolkit_wire_limit=16384
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_menu.lua')
local machine=manager.machine
local actions=coroutine.create(function()
    assert(emu.wait(88))
    machine.screens[':screen']:snapshot('nsm1_toolkit_refresh_88.png')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('nsm1_toolkit_refresh_91.png')
end)
assert(coroutine.resume(actions))
