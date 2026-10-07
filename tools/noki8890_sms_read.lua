-- Resume the product's physical SMS reading schedule on the saved timeline.
local machine = manager.machine
local input = coroutine.create(function()
    local delay = 21 - machine.time:as_double()
    if delay > 0 and not emu.wait(delay) then return end
    for _, item in ipairs({{1, 'Menu'}, {0, 'End'}, {1, 'Names / C'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
    end
    machine.screens[':screen']:snapshot('8890_sms_received.png')
    for index = 1, 2 do
        local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
        machine:logerror('8890_sms_physical: action=read_' .. index .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(1.85) then return end
        machine.screens[':screen']:snapshot('8890_sms_read_' .. index .. '.png')
    end
end)
_G.noki8890_incoming_sms_input = input
assert(coroutine.resume(input))
