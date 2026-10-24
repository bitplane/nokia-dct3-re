-- Physical security-settings navigation after independent SIM/phone unlock.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. (_G.nsb3_security_base or 'nsb3_pin_correct.lua'))
local machine = manager.machine
local fields = {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local actions = coroutine.create(function()
    local steps = _G.nsb3_security_steps or {
        {40, 'Scroll Down'}, {43, 'Scroll Down'}, {46, 'Scroll Down'},
        {49, 'Left Softkey / Menu'}, {52, 'Scroll Down'}, {55, 'Left Softkey / Menu'},
        {58, 'Left Softkey / Menu'}, {61, 'Keypad 1'}, {63, 'Keypad 2'},
        {65, 'Keypad 3'}, {67, 'Keypad 4'}, {69, 'Left Softkey / Menu'},
        {72, _G.nsb3_security_enable and 'Scroll Up' or 'Scroll Down'}, {75, 'Left Softkey / Menu'}}
    for index, step in ipairs(steps) do
        assert(emu.wait(step[1] - machine.time:as_double()))
        local field = assert(fields[step[2]], step[2])
        field:set_value(1)
        machine:logerror(string.format('nsb3_security_key: key=%s down=1 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(0.15))
        field:set_value(0)
        machine:logerror(string.format('nsb3_security_key: key=%s down=0 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(_G.nsb3_security_settle or 1))
        machine.screens[':screen']:snapshot(string.format('nsb3_security_%02d.png', index))
    end
end)
assert(coroutine.resume(actions))
