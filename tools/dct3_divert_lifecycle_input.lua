-- Shared physical sequence for independently verified five-column matrices.
local machine = manager.machine
local product = assert(_G.dct3_divert_product)
local prefix = product .. '_divert_lifecycle'
local function press(column, name)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror(prefix .. '_physical: key=' .. name .. '\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.85)
end
local sequences = _G.dct3_divert_sequences or {'*21*5551234#', '*#21#', '#21#', '*#21#'}
local function transactions(first)
    for index = first, #sequences do
        machine:logerror(prefix .. '_physical: transaction=' .. index .. '\n')
        for character in sequences[index]:gmatch('.') do
            local column = character == '*' and 2 or character == '#' and 4
                or 2 + (tonumber(character) - 1) % 3
            if not press(column, 'Keypad ' .. character) then return end
        end
        if not press(0, 'Send') or not emu.wait(1) then return end
        machine.screens[':screen']:snapshot(prefix .. '_' .. index .. '.png')
        if not press(1, 'Right Softkey / C') or not emu.wait(1) then return end
        if index == 1 and _G.dct3_divert_save_window then
            if not emu.wait(_G.dct3_divert_save_window) then return end
        end
    end
    machine.screens[':screen']:snapshot(prefix .. '_idle.png')
end
_G.dct3_resume_divert = function() transactions(2) end
local input = coroutine.create(function()
    if not emu.wait(assert(_G.dct3_divert_start)) then return end
    transactions(1)
end)
_G.dct3_divert_lifecycle_input = input
assert(coroutine.resume(input))
