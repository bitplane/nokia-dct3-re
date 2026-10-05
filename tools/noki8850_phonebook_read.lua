-- Cold preserved-storage phonebook lookup through physical inputs only.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki8850_security_input.lua")
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(29) then return end
    local key = assert(machine.ioport.ports[":COL.1"].fields["Menu"])
    for _, stage in ipairs({"search_editor", "search_results", "contact"}) do
        machine:logerror("8850_phonebook_read_physical: action=" .. stage .. "\n")
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[":screen"]:snapshot("8850_phonebook_" .. stage .. ".png")
    end
end)
_G.noki8850_phonebook_read = input
assert(coroutine.resume(input))
