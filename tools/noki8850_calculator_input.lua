-- Exercise the firmware application through the product-local key matrix.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki8850_security_input.lua")
local machine = manager.machine
local sequence = {
    {1, "Names / C", "idle"}, {1, "Menu", "messages"},
    {1, "Scroll Down", "menu_2"}, {1, "Scroll Down", "menu_3"},
    {1, "Scroll Down", "menu_4"}, {1, "Scroll Down", "menu_5"},
    {1, "Scroll Down", "menu_6"}, {1, "Scroll Down", "menu_7"},
    {1, "Menu", "application"},
    {2, "Keypad 1", "input_1"}, {3, "Keypad 2", "input_12"},
    {1, "Menu", "operation_options"}, {1, "Scroll Down", "add"},
    {1, "Menu", "plus"}, {4, "Keypad 3", "input_3"},
    {1, "Menu", "options"}, {1, "Menu", "result"},
}
local input = coroutine.create(function()
    if not emu.wait(29) then return end
    for _, item in ipairs(sequence) do
        local key = assert(machine.ioport.ports[":COL." .. item[1]].fields[item[2]])
        machine:logerror("8850_application_physical: action=" .. item[3] .. "\n")
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
        machine.screens[":screen"]:snapshot("8850_calculator_" .. item[3] .. ".png")
    end
end)
_G.noki8850_calculator_input = input
assert(coroutine.resume(input))
