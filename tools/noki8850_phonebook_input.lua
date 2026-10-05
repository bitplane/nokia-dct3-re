-- Product-local physical navigation; no firmware or card-storage writes.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki8850_security_input.lua")
local machine = manager.machine
local sequence = {
    {1, "Scroll Down", "add_entry"}, {1, "Menu", "name_editor"},
    {3, "Keypad 2", "name_a"}, {1, "Menu", "number_editor"},
    {2, "Keypad 1", "number_1"}, {3, "Keypad 2", "number_12"},
    {4, "Keypad 3", "number_123"}, {1, "Menu", "save"},
}
local input = coroutine.create(function()
    if not emu.wait(29) then return end
    for _, item in ipairs(sequence) do
        local key = assert(machine.ioport.ports[":COL." .. item[1]].fields[item[2]])
        machine:logerror(string.format("8850_phonebook_physical: action=%s key=%s t=%.6f\n",
            item[3], item[2], machine.time:as_double()))
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[":screen"]:snapshot("8850_phonebook_" .. item[3] .. ".png")
    end
end)
_G.noki8850_phonebook_input = input
assert(coroutine.resume(input))
