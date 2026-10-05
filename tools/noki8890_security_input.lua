-- NSB-6 physical matrix input; no firmware memory writes.
local source = debug.getinfo(1, "S").source:sub(2)
dofile(assert(source:match("^(.*[/])")) .. "noki8890_bootstrap_observe.lua")
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
cpu.debug:bpset(0x2ff03c, nil,
    'logerror "8890_keypad_decoded: key=%02x\\n",r0;g')
local sequence = {
    {2, "Keypad 1"}, {3, "Keypad 2"}, {4, "Keypad 3"},
    {2, "Keypad 4"}, {3, "Keypad 5"}, {1, "Menu"},
}
local input = coroutine.create(function()
    if not emu.wait(12) then return end
    for _, item in ipairs(sequence) do
        local key = assert(machine.ioport.ports[":COL." .. item[1]].fields[item[2]])
        machine:logerror(string.format("8890_security_physical: key=%s t=%.6f\n",
            item[2], machine.time:as_double()))
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.35) then return end
    end
    if not emu.wait(5) then return end
    machine.screens[":screen"]:snapshot("8890_after_security.png")
    if _G.noki8890_security_only then return end
    local left = assert(machine.ioport.ports[":COL.1"].fields["Menu"])
    for index = 1, 3 do
        machine:logerror(string.format("8890_navigation_physical: press=%d\n", index))
        left:set_value(1)
        if not emu.wait(0.15) then left:set_value(0); return end
        left:set_value(0)
        if not emu.wait(2) then return end
        machine.screens[":screen"]:snapshot(string.format("8890_navigation_%d.png", index))
    end
end)
_G.noki8890_security_input = input
assert(coroutine.resume(input))
