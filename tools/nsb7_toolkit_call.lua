-- Card requests a call; physical acceptance and End own the user lifecycle.
_G.nsb7_toolkit_menu=true
_G.nsb7_toolkit_call=true
_G.nsb7_toolkit_network_wait=20
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_interactive.lua')
local capture=coroutine.create(function()
    assert(emu.wait(100))
    manager.machine.screens[':screen']:snapshot('nsb7_toolkit_call_active.png')
end)
_G.nsb7_toolkit_call_capture=capture
assert(coroutine.resume(capture))
