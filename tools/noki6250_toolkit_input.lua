-- Card-owned proactive command and physical NHM-3 dismissal.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_runtime_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(33))
    machine.screens[':screen']:snapshot('6250_toolkit_display.png')
    assert(emu.wait(1))
    local key = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
    machine:logerror('6250_toolkit_physical: action=dismiss\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    assert(emu.wait(1.85))
    machine.screens[':screen']:snapshot('6250_toolkit_after_dismiss.png')
end)
_G.noki6250_toolkit_input = input
assert(coroutine.resume(input))
