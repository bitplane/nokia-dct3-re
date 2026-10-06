-- Network-originated call; physical Answer/End, no firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
machine.devices[':maincpu'].debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
local input = coroutine.create(function()
    if not emu.wait(24) then return end
    machine.screens[':screen']:snapshot('6210_incoming_ringing.png')
    for index, name in ipairs({'Send', 'End'}) do
        local key = assert(machine.ioport.ports[':COL.0'].fields[name])
        machine:logerror('6210_incoming_physical: action=' .. name .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(7.85) then return end
        machine.screens[':screen']:snapshot(index == 1 and
            '6210_incoming_connected.png' or '6210_after_incoming_call.png')
    end
end)
_G.noki6210_incoming_call_input = input
assert(coroutine.resume(input))
