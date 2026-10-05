-- NSB-6 physical dialing against the laboratory network; no firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_security_input.lua')
local machine = manager.machine
local function press(column, name, action)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror('8890_call_physical: action=' .. action .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.85)
end
local input = coroutine.create(function()
    if not emu.wait(21) then return end
    if not press(0, 'End', 'clock_cancel') then return end
    if not press(1, 'Names / C', 'idle') then return end
    machine.screens[':screen']:snapshot('8890_registered_idle.png')
    for _, digit in ipairs({1, 2, 3, 4, 5, 6, 7}) do
        local column = ({[1]=2, [2]=3, [3]=4, [4]=2, [5]=3, [6]=4, [7]=2})[digit]
        if not press(column, 'Keypad ' .. digit, 'digit_' .. digit) then return end
        machine.screens[':screen']:snapshot('8890_dial_digit_' .. digit .. '.png')
    end
    machine.screens[':screen']:snapshot('8890_dialed_number.png')
    if not press(0, 'Call / Send', 'send') then return end
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('8890_outgoing_call.png')
    if not press(0, 'End', 'end') then return end
    if not emu.wait(5) then return end
    machine.screens[':screen']:snapshot('8890_after_outgoing_call.png')
end)
_G.noki8890_outgoing_call_input = input
assert(coroutine.resume(input))
