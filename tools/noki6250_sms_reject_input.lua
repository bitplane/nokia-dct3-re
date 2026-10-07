-- Failed host reply followed by physical End/Menu; no firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_sms_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(44))
    for index = 1, 6 do
        machine.screens[':screen']:snapshot('6250_sms_reject_' .. index .. '.png')
        assert(emu.wait(0.5))
    end
    assert(emu.wait(3))
    for _, item in ipairs({{':COL.0', 'End'}, {':COL.0', 'End'},
                           {':COL.1', 'Left Softkey / Menu'}}) do
        local key = assert(machine.ioport.ports[item[1]].fields[item[2]])
        machine:logerror('6250_sms_recovery_physical: key=' .. item[2] .. '\n')
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(0.85))
    end
    machine.screens[':screen']:snapshot('6250_sms_recovery_menu.png')
end)
_G.noki6250_sms_reject_input = input
assert(coroutine.resume(input))
