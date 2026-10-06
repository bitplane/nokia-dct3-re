-- Physical NSM-3 dial/Send/End against the isolated laboratory network.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_security_input.lua')
local machine = manager.machine
local function press(column, name, action)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror('8210_call_physical: action=' .. action .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.85)
end
local input = coroutine.create(function()
    if not emu.wait(21) then return end
    for _, digit in ipairs({1, 2, 3, 4, 5, 6, 7}) do
        local column = ({[1]=2, [2]=3, [3]=4, [4]=2, [5]=3, [6]=4, [7]=2})[digit]
        if not press(column, 'Keypad ' .. digit, 'digit_' .. digit) then return end
    end
    machine.screens[':screen']:snapshot('8210_dialed_number.png')
    if not press(0, 'Call / Send', 'send') then return end
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('8210_outgoing_call.png')
    if not press(0, 'End', 'end') then return end
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('8210_after_outgoing_call.png')
end)
_G.noki8210_outgoing_call_input = input
assert(coroutine.resume(input))
