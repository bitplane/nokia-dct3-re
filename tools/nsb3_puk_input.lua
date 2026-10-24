-- Physical PUK1/new-PIN/repeat sequence, followed by the separate phone code.
_G.nsb3_phonecode_start = 40
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. (_G.nsb3_puk_wrong and 'nsb3_sim_observe.lua' or 'nsb3_phonecode_input.lua'))
local machine = manager.machine
local fields = {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local actions = coroutine.create(function()
    assert(emu.wait(10))
    machine.screens[':screen']:snapshot('nsb3_puk_prompt.png')
    assert(emu.wait(2))
    for index, stage in ipairs({{'puk', _G.nsb3_puk_wrong and '12345679' or '12345678'}, {'new', '2468'}, {'repeat', '2468'}}) do
        local keys = {}
        for digit in stage[2]:gmatch('.') do keys[#keys + 1] = 'Keypad ' .. digit end
        keys[#keys + 1] = 'Left Softkey / Menu'
        for _, name in ipairs(keys) do
            local field = assert(fields[name])
            field:set_value(1)
            machine:logerror(string.format('nsb3_puk_key: stage=%s key=%s down=1 t=%.9f physical_only=1\n', stage[1], name, machine.time:as_double()))
            assert(emu.wait(0.15))
            field:set_value(0)
            machine:logerror(string.format('nsb3_puk_key: stage=%s key=%s down=0 t=%.9f physical_only=1\n', stage[1], name, machine.time:as_double()))
            assert(emu.wait(0.25))
        end
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot(string.format('nsb3_puk_stage_%d.png', index))
    end
end)
assert(coroutine.resume(actions))
