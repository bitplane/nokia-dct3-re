-- Own NSM-3 composer UI; no firmware or SIM storage writes.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_security_input.lua')
local machine = manager.machine
local function press(column, name, label)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror('8210_sms_send_physical: action=' .. label .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    if not emu.wait(0.85) then return false end
    machine.screens[':screen']:snapshot('8210_sms_send_' .. label .. '.png')
    return true
end
local input = coroutine.create(function()
    if not emu.wait(21) then return end
    for _, item in ipairs({
        {1, 'Menu', 'messages'}, {1, 'Menu', 'message_list'},
        {1, 'Scroll Down', 'write_select_1'},
        {1, 'Scroll Down', 'write_select_2'},
        {1, 'Menu', 'write'}, {3, 'Keypad 2', 'text_A'},
        {1, 'Menu', 'options'}, {1, 'Menu', 'send'},
    }) do
        if not press(item[1], item[2], item[3]) then return end
    end
    for _, digit in ipairs({5, 5, 5, 1, 2, 3, 4}) do
        local column = ({[1]=2, [2]=3, [3]=4, [4]=2, [5]=3})[digit]
        if not press(column, 'Keypad ' .. digit, 'recipient_' .. digit) then return end
    end
    if not press(1, 'Menu', 'confirm_send') then return end
    if not emu.wait(1) then return end
    machine.screens[':screen']:snapshot('8210_sms_sent.png')
    if not emu.wait(4) then return end
    machine.screens[':screen']:snapshot('8210_sms_composer_after_send.png')
end)
_G.noki8210_outgoing_sms_input = input
assert(coroutine.resume(input))
