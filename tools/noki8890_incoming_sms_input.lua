-- Network-originated SMS, physical UI input only.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_security_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(21) then return end
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
