-- NSB-6 physical matrix input; no firmware memory writes.
local source = debug.getinfo(1, "S").source:sub(2)
dofile(assert(source:match("^(.*[/])")) .. "noki8890_bootstrap_observe.lua")
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
cpu.debug:bpset(0x2ff03c, nil,
    'logerror "8890_keypad_decoded: key=%02x\\n",r0;g')
if os.getenv('NOKIA_DCT3_8890_PIN_ENTRY') == '1' then
    cpu.debug:bpset(0x2db270, nil,
        'logerror "8890_band_rx: object=%08x\\n",r0;g')
    cpu.debug:bpset(0x21d768, nil,
        'logerror "8890_band_complete: object=%08x\\n",r1;g')
    cpu.debug:bpset(0x2809fc, nil,
        'logerror "8890_band_parse: object=%08x arfcn=%04x rssi=%02x\\n",r1,w@(r1+6),b@(r1+9);g')
end
local sequence = {
    {2, "Keypad 1"}, {3, "Keypad 2"}, {4, "Keypad 3"},
    {2, "Keypad 4"}, {3, "Keypad 5"}, {1, "Menu"},
}
local input = coroutine.create(function()
    if os.getenv('NOKIA_DCT3_8890_PIN_ENTRY') == '1' then
        if not emu.wait(8) then return end
        machine.screens[':screen']:snapshot('8890_pin_prompt.png')
        for _, item in ipairs({sequence[1], sequence[2], sequence[3], sequence[4], sequence[6]}) do
            local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
            machine:logerror('8890_pin_physical: key=' .. item[2] .. '\n')
            key:set_value(1)
            if not emu.wait(0.15) then key:set_value(0); return end
            key:set_value(0)
            if not emu.wait(0.85) then return end
        end
        if not emu.wait(3) then return end
    elseif not emu.wait(12) then return end
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
