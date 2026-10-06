-- Laboratory GSM delivery; physical Read keys only.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
machine.devices[':maincpu'].debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
local input = coroutine.create(function()
    if not emu.wait(22) then return end
    machine.screens[':screen']:snapshot('6210_sms_received.png')
    for index = 1, 2 do
        local key = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
        machine:logerror('6210_sms_physical: action=read_' .. index .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(1.85) then return end
        machine.screens[':screen']:snapshot('6210_sms_read_' .. index .. '.png')
    end
end)
_G.noki6210_incoming_sms_input = input
assert(coroutine.resume(input))
