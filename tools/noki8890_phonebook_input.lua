-- NSB-6 phonebook through physical keys only; SIM writes are firmware-owned.
local source = debug.getinfo(1, "S").source:sub(2)
_G.noki8890_security_only = true
dofile(assert(source:match("^(.*[/])")) .. "noki8890_security_input.lua")
local machine = manager.machine
local sequence = {
    {1, 'Menu', 'clock_notice'}, {0, 'End', 'idle'},
    {1, 'Names / C', 'dismiss_notice'}, {1, 'Names / C', 'names'},
    {1, 'Scroll Down', 'add_entry'},
    {1, 'Menu', 'name_editor'}, {3, 'Keypad 2', 'name_a'},
    {1, 'Menu', 'number_editor'}, {2, 'Keypad 1', 'number_1'},
    {3, 'Keypad 2', 'number_12'}, {4, 'Keypad 3', 'number_123'},
    {1, 'Menu', 'save'},
}
local input = coroutine.create(function()
    if not emu.wait(21) then return end
    for _, item in ipairs(sequence) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror(string.format('8890_phonebook_physical: action=%s key=%s t=%.6f\n',
            item[3], item[2], machine.time:as_double()))
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[':screen']:snapshot('8890_phonebook_' .. item[3] .. '.png')
    end
end)
_G.noki8890_phonebook_input = input
assert(coroutine.resume(input))
