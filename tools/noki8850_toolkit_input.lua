-- Own-product security entry followed by card-owned DISPLAY TEXT dismissal.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8850_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8850_security_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(33))
    machine.screens[':screen']:snapshot('8850_toolkit_display.png')
    assert(emu.wait(1))
    local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    machine:logerror('8850_toolkit_physical: action=dismiss\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    assert(emu.wait(1.85))
    machine.screens[':screen']:snapshot('8850_toolkit_after_dismiss.png')
end)
_G.noki8850_toolkit_input = input
assert(coroutine.resume(input))
