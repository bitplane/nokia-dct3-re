-- Offline SMS UI exploration through physical contacts only; no network peer.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonecode_input.lua')
local machine = manager.machine
local fields = {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local actions = coroutine.create(function()
    local steps = _G.nsb3_sms_steps or {
        {34, 'Left Softkey / Menu'}, {37, 'Left Softkey / Menu'},
        {40, 'Scroll Down'}, {43, 'Scroll Down'},
        {46, 'Left Softkey / Menu'}, {49, 'Keypad 2'},
        {52, 'Left Softkey / Menu'}, {55, 'Scroll Down'},
        {58, 'Scroll Down'}, {61, 'Left Softkey / Menu'}}
    for index, step in ipairs(steps) do
        assert(emu.wait(step[1] - machine.time:as_double()))
        local field = assert(fields[step[2]], step[2])
        field:set_value(1)
        machine:logerror(string.format('nsb3_sms_key: key=%s down=1 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(0.15))
        field:set_value(0)
        machine:logerror(string.format('nsb3_sms_key: key=%s down=0 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(1))
        machine.screens[':screen']:snapshot(string.format('nsb3_sms_%02d.png', index))
    end
end)
assert(coroutine.resume(actions))
