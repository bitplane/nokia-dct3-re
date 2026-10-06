-- Physical NPE-3 composer; no firmware or SIM storage writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
machine.devices[':maincpu'].debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
local function press(column, name, action)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror('6210_sms_send_physical: action=' .. action .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    if not emu.wait(0.85) then return false end
    machine.screens[':screen']:snapshot('6210_sms_send_' .. action .. '.png')
    return true
end
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    for _, step in ipairs({
        {1, 'Left Softkey / Menu', 'messages'},
        {1, 'Left Softkey / Menu', 'message_list'},
        {1, 'Left Softkey / Menu', 'write'}, {3, 'Keypad 2', 'text_A'},
        {1, 'Left Softkey / Menu', 'options'}, {1, 'Left Softkey / Menu', 'send'},
    }) do
        if not press(step[1], step[2], step[3]) then return end
    end
    for _, digit in ipairs({5, 5, 5, 1, 2, 3, 4}) do
        if not press(2 + (digit - 1) % 3, 'Keypad ' .. digit, 'recipient_' .. digit) then return end
    end
    if not press(1, 'Left Softkey / Menu', 'confirm_send') then return end
    if not emu.wait(1) then return end
    machine.screens[':screen']:snapshot('6210_sms_sent.png')
    if not emu.wait(4) then return end
    machine.screens[':screen']:snapshot('6210_sms_composer_after_send.png')
end)
_G.noki6210_outgoing_sms_input = input
assert(coroutine.resume(input))
