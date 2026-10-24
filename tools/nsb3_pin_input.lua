-- Physical laboratory SIM PIN; never substitutes a firmware result.
local source = debug.getinfo(1, 'S').source:sub(2)
local correct = _G.nsb3_pin_correct or false
if correct then
    _G.nsb3_phonecode_start = 20
    dofile(source:match('^(.*[/\\])') .. 'nsb3_phonecode_input.lua')
else
    dofile(source:match('^(.*[/\\])') .. 'nsb3_sim_observe.lua')
end
local machine = manager.machine
local fields = {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local actions = coroutine.create(function()
    assert(emu.wait(10))
    machine.screens[':screen']:snapshot('nsb3_pin_prompt.png')
    assert(emu.wait(2))
    local digits = _G.nsb3_pin_digits or (correct and '1234' or '4321')
    local keys = {}
    for digit in digits:gmatch('.') do keys[#keys + 1] = 'Keypad ' .. digit end
    keys[#keys + 1] = 'Left Softkey / Menu'
    for _, name in ipairs(keys) do
        local field = assert(fields[name])
        field:set_value(1)
        machine:logerror(string.format('nsb3_pin_key: correct=%d key=%s down=1 t=%.9f physical_only=1\n', correct and 1 or 0, name, machine.time:as_double()))
        assert(emu.wait(0.15))
        field:set_value(0)
        machine:logerror(string.format('nsb3_pin_key: correct=%d key=%s down=0 t=%.9f physical_only=1\n', correct and 1 or 0, name, machine.time:as_double()))
        assert(emu.wait(0.25))
    end
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('nsb3_pin_result.png')
end)
assert(coroutine.resume(actions))
