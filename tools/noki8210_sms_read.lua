-- Physical Read navigation, usable after cold startup or emulator load.
local machine = manager.machine
local input = coroutine.create(function()
    local delay = 22 - machine.time:as_double()
    if delay > 0 and not emu.wait(delay) then return end
    machine.screens[':screen']:snapshot('8210_sms_received.png')
    for index = 1, 2 do
        local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
        machine:logerror('8210_sms_physical: action=read_' .. index .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(1.85) then return end
        machine.screens[':screen']:snapshot('8210_sms_read_' .. index .. '.png')
    end
end)
_G.noki8210_sms_read = input
assert(coroutine.resume(input))
