-- Laboratory call through physical handset inputs; no firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki8850_radio_observe.lua')
local machine = manager.machine
local function press(column, name, label)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror('8850_call_physical: action=' .. label .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.35)
end
local input = coroutine.create(function()
    -- PIN entry shifts the physical phone-code/menu sequence by four seconds.
    if not emu.wait(os.getenv('NOKIA_DCT3_8850_PIN_ENTRY') == '1' and 33 or 29) then return end
    if not press(1, 'Names / C', 'idle') then return end
    machine.screens[':screen']:snapshot('8850_registered_idle.png')
    if _G.noki8850_call_idle_only then return end
    for _, digit in ipairs({5, 5, 5, 1, 2, 3, 4}) do
        local column = ({[1]=2, [2]=3, [3]=4, [4]=2, [5]=3})[digit]
        if not press(column, 'Keypad ' .. digit, 'digit_' .. digit) then return end
    end
    machine.screens[':screen']:snapshot('8850_dialed_number.png')
    if not press(0, 'Call / Send', 'send') then return end
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('8850_outgoing_call.png')
    if not press(0, 'End', 'end') then return end
    if not emu.wait(5) then return end
    machine.screens[':screen']:snapshot('8850_after_outgoing_call.png')
end)
_G.noki8850_outgoing_call_input = input
assert(coroutine.resume(input))
