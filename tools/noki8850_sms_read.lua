-- Physical read schedule, reusable after emulator restoration.
local machine = manager.machine
local input = coroutine.create(function()
    local start = os.getenv('NOKIA_DCT3_8850_PIN_ENTRY') == '1' and 24 or 20
    local delay = start - machine.time:as_double()
    if delay > 0 and not emu.wait(delay) then return end
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
