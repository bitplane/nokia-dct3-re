-- Clock/date settlement and physical Answer/End; fresh idle gates host INVITE.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_incoming_input.lua')
local ready = coroutine.create(function()
    assert(emu.wait(45))
    manager.machine.screens[':screen']:snapshot('8890_sip_registered_idle.png')
end)
_G.noki8890_sip_incoming_ready = ready
assert(coroutine.resume(ready))
