-- Physical NPE-3 address-book entry; only host key fields are changed.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local sequence = {
    {1, 'Right Softkey / C', 'names'}, {1, 'Scroll Down', 'add_entry'},
    {1, 'Left Softkey / Menu', 'name_editor'}, {3, 'Keypad 2', 'name_a'},
    {1, 'Left Softkey / Menu', 'number_editor'}, {2, 'Keypad 1', 'number_1'},
    {3, 'Keypad 2', 'number_12'}, {4, 'Keypad 3', 'number_123'},
    {1, 'Left Softkey / Menu', 'save'},
}
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    for _, step in ipairs(sequence) do
        local key = assert(machine.ioport.ports[':COL.' .. step[1]].fields[step[2]])
        machine:logerror('6210_phonebook_physical: action=' .. step[3] .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[':screen']:snapshot('6210_phonebook_' .. step[3] .. '.png')
    end
end)
_G.noki6210_phonebook_input = input
assert(coroutine.resume(input))
