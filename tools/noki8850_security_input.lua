-- Physical input only, using the product-local matrix. No firmware writes.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki8850_startup_observe.lua")
local machine = manager.machine
local sequence = {
    {2, "Keypad 1"}, {3, "Keypad 2"}, {4, "Keypad 3"},
    {2, "Keypad 4"}, {3, "Keypad 5"}, {1, "Menu"},
}
local input = coroutine.create(function()
    if not emu.wait(12) then return end
    for _, item in ipairs(sequence) do
        local key = assert(machine.ioport.ports[":COL." .. item[1]].fields[item[2]])
        machine:logerror(string.format("8850_security_physical: key=%s t=%.6f\n",
            item[2], machine.time:as_double()))
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.35) then return end
    end
    if not emu.wait(3) then return end
    machine.screens[":screen"]:snapshot("8850_after_security.png")
    if _G.noki8850_security_only then return end
    if not emu.wait(5) then return end
    for _, name in ipairs({"messages_menu", "inbox"}) do
        local key = assert(machine.ioport.ports[":COL.1"].fields["Menu"])
        machine:logerror("8850_navigation_physical: action=" .. name .. "\n")
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[":screen"]:snapshot("8850_" .. name .. ".png")
    end
    local right = assert(machine.ioport.ports[":COL.1"].fields["Names / C"])
    for index = 1, 3 do
        machine:logerror(string.format("8850_navigation_physical: action=%s\n",
            index == 3 and "names" or "back"))
        right:set_value(1)
        if not emu.wait(0.15) then right:set_value(0); return end
        right:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[":screen"]:snapshot(string.format("8850_right_%d.png", index))
    end
    machine.screens[":screen"]:snapshot("8850_names.png")
end)
_G.noki8850_security_input = input
assert(coroutine.resume(input))
