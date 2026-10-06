-- Physical NPE-3 dial/Send/End against the isolated laboratory network.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
machine.devices[':maincpu'].debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
local function press(column, name, action)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror('6210_call_physical: action=' .. action .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.85)
end
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    for digit = 1, 7 do
        if not press(2 + (digit - 1) % 3, 'Keypad ' .. digit, 'digit_' .. digit) then return end
    end
    machine.screens[':screen']:snapshot('6210_dialed_number.png')
    if not press(0, 'Send', 'send') then return end
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('6210_outgoing_call.png')
    if not press(0, 'End', 'end') then return end
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('6210_after_outgoing_call.png')
end)
_G.noki6210_outgoing_call_input = input
assert(coroutine.resume(input))
