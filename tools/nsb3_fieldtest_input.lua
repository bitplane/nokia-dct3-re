-- Physical Field test selection only; does not write firmware/NV state.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_correct.lua')
local machine = manager.machine
local fields = {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local actions = coroutine.create(function()
    local steps = {{40, 'Scroll Up'}, {42, 'Left Softkey / Menu'},
                   {46, 'Keypad 0'}, {48, 'Keypad 0'},
                   {50, 'Left Softkey / Menu'}, {53, 'End'}}
    for i, step in ipairs(steps) do
        assert(emu.wait(step[1] - machine.time:as_double()))
        local field = assert(fields[step[2]], step[2])
        field:set_value(1)
        machine:logerror(string.format('nsb3_fieldtest_key: key=%s down=1 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(0.15))
        field:set_value(0)
        machine:logerror(string.format('nsb3_fieldtest_key: key=%s down=0 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(1.5))
        machine.screens[':screen']:snapshot(string.format('nsb3_fieldtest_%02d.png', i))
    end
end)
assert(coroutine.resume(actions))
