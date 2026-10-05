-- Compose and send through the firmware UI, using physical matrix inputs.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8850_security_only = true
dofile(directory .. 'noki8850_radio_observe.lua')
local machine = manager.machine
local function press(column, name, label)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror('8850_sms_send_physical: action=' .. label .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.85)
end
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    if not press(1, 'Menu', 'messages') then return end
    if not press(1, 'Menu', 'message_list') then return end
    for index = 1, 2 do
        if not press(1, 'Scroll Down', 'write_select_' .. index) then return end
    end
    machine.screens[':screen']:snapshot('8850_sms_write_menu.png')
    if not press(1, 'Menu', 'write') then return end
    machine.screens[':screen']:snapshot('8850_sms_editor.png')
    if not press(3, 'Keypad 2', 'text_A') then return end
    machine.screens[':screen']:snapshot('8850_sms_text.png')
    if not press(1, 'Menu', 'options') then return end
    machine.screens[':screen']:snapshot('8850_sms_options.png')
    if not press(1, 'Menu', 'send') then return end
    machine.screens[':screen']:snapshot('8850_sms_recipient.png')
    for _, digit in ipairs({5, 5, 5, 1, 2, 3, 4}) do
        local column = ({[1]=2, [2]=3, [3]=4, [4]=2, [5]=3})[digit]
        if not press(column, 'Keypad ' .. digit, 'recipient_' .. digit) then return end
    end
    machine.screens[':screen']:snapshot('8850_sms_destination.png')
    if not press(1, 'Menu', 'confirm_send') then return end
    if not emu.wait(5) then return end
    machine.screens[':screen']:snapshot('8850_sms_sent.png')
end)
_G.noki8850_outgoing_sms_input = input
assert(coroutine.resume(input))
