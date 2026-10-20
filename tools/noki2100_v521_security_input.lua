-- Physical contacts only; this fixture does not assert security-code acceptance.
local machine = manager.machine
assert(machine.system.name == 'noki2100' or machine.system.name == 'nam2c3')
local start = _G.nam2_v521_key_start or 14
local step = _G.nam2_v521_key_step or 1
assert(start >= 0 and step >= 0.2)
local actions = {
    {2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
    {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Left Softkey / Menu'},
}
local input = coroutine.create(function()
    if not emu.wait(start) then return end
    machine.screens[':screen']:snapshot('nam2_v521_security_before.png')
    for index, action in ipairs(actions) do
        local key = assert(machine.ioport.ports[':COL.' .. action[1]].fields[action[2]])
        machine:logerror(string.format('nam2_v521_security_physical: index=%d key=%s t=%.6f\n',
            index, action[2], machine.time:as_double()))
        key:set_value(1)
        if not emu.wait(0.2) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(step - 0.2) then return end
        machine.screens[':screen']:snapshot('nam2_v521_security_' .. index .. '.png')
    end
    if not emu.wait(4) then return end
    machine.screens[':screen']:snapshot('nam2_v521_security_after.png')
end)
_G.nam2_v521_security_input = input
assert(coroutine.resume(input))
