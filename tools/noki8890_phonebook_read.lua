-- Preserved-storage cold boot; read the contact through physical inputs.
local source = debug.getinfo(1, "S").source:sub(2)
_G.noki8890_security_only = true
dofile(assert(source:match("^(.*[/])")) .. "noki8890_security_input.lua")
local machine = manager.machine
local sequence = {
    {1, 'Menu', 'clock_notice'}, {0, 'End', 'idle'},
    {1, 'Names / C', 'names'},
    {1, 'Menu', 'search_editor'},
    {1, 'Menu', 'search_results'}, {1, 'Menu', 'contact'},
}
local input = coroutine.create(function()
    if not emu.wait(21) then return end
    for _, item in ipairs(sequence) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror(string.format('8890_phonebook_read_physical: action=%s key=%s\n', item[3], item[2]))
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[':screen']:snapshot('8890_phonebook_' .. item[3] .. '.png')
    end
end)
_G.noki8890_phonebook_read = input
assert(coroutine.resume(input))
