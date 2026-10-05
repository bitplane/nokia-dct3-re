-- Read a network-originated SMS through physical softkeys.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8850_security_only = true
dofile(directory .. 'noki8850_radio_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    machine.screens[':screen']:snapshot('8850_sms_received.png')
    for index = 1, 4 do
        local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
        machine:logerror('8850_sms_physical: action=read_' .. index .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(1.85) then return end
        machine.screens[':screen']:snapshot('8850_sms_read_' .. index .. '.png')
    end
end)
_G.noki8850_incoming_sms_input = input
assert(coroutine.resume(input))
