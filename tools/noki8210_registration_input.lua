-- Own physical unlock followed by passive registered-idle capture.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_security_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(24) then return end
    machine.screens[':screen']:snapshot('8210_registered_idle.png')
    if not emu.wait(20) then return end
    machine.screens[':screen']:snapshot('8210_registered_idle_late.png')
end)
_G.noki8210_registration_input = input
assert(coroutine.resume(input))
