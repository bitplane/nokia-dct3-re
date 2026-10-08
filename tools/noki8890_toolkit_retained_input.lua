-- Own-product retained-clock input; card and firmware own the protocol.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_read.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(70))
    machine.screens[':screen']:snapshot('8890_toolkit_display.png')
    assert(emu.wait(1))
    local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    machine:logerror('8890_toolkit_physical: action=dismiss\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    assert(emu.wait(3.85))
    machine.screens[':screen']:snapshot('8890_toolkit_after_dismiss.png')
end)
_G.noki8890_toolkit_retained = input
assert(coroutine.resume(input))
