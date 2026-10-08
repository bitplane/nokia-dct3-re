-- Physical supplementary-service input after the own-product security fixture.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
_G.noki8850_security_only = true
dofile(directory .. "noki8850_security_input.lua")
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(25) then return end
    for _, item in ipairs({{2, "Keypad *"}, {2, "Keypad 1"},
            {3, "Keypad 2"}, {4, "Keypad 3"}, {4, "Keypad #"},
            {0, "Call / Send"}}) do
        local key = assert(machine.ioport.ports[":COL." .. item[1]].fields[item[2]])
        machine:logerror("8850_ussd_physical: key=" .. item[2] .. "\n")
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
    end
    if not emu.wait(8) then return end
    machine.screens[":screen"]:snapshot("8850_ussd_result.png")
    local back = assert(machine.ioport.ports[":COL.1"].fields["Names / C"])
    machine:logerror("8850_ussd_physical: key=Back\n")
    back:set_value(1)
    if not emu.wait(0.15) then back:set_value(0); return end
    back:set_value(0)
    if not emu.wait(1.85) then return end
    machine.screens[":screen"]:snapshot("8850_ussd_after_back.png")
end)
_G.noki8850_ussd_input = input
assert(coroutine.resume(input))
