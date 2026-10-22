-- Let the card request REFRESH and observe the firmware-owned re-profile.
_G.nsb7_toolkit_menu=true
_G.nsb7_toolkit_refresh=true
_G.nsb7_toolkit_network_wait=60
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_interactive.lua')
local capture=coroutine.create(function()
    assert(emu.wait(129))
    manager.machine.screens[':screen']:snapshot('nsb7_toolkit_refresh_display.png')
end)
_G.nsb7_toolkit_refresh_capture=capture
assert(coroutine.resume(capture))
