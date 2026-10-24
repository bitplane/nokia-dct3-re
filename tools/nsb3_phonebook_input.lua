-- Continue the qualified security/menu path with physical phonebook contacts.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonecode_input.lua')
local machine = manager.machine
local fields = {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local actions = coroutine.create(function()
    local steps = _G.nsb3_phonebook_steps or {
        {34, 'Right Softkey / C'}, {37, 'Right Softkey / C'},
        {40, 'Scroll Down'}, {43, 'Left Softkey / Menu'},
        {46, 'Keypad 2'}, {49, 'Left Softkey / Menu'},
        {52, 'Keypad 1'}, {54, 'Keypad 2'}, {56, 'Keypad 3'},
        {58, 'Keypad 4'}, {60, 'Keypad 5'}, {63, 'Left Softkey / Menu'}}
    for index, step in ipairs(steps) do
        assert(emu.wait(step[1] - machine.time:as_double()))
        local field = assert(fields[step[2]], step[2])
        field:set_value(1)
        machine:logerror(string.format('nsb3_phonebook_key: key=%s down=1 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(0.15))
        field:set_value(0)
        machine:logerror(string.format('nsb3_phonebook_key: key=%s down=0 t=%.9f physical_only=1\n', step[2], machine.time:as_double()))
        assert(emu.wait(1))
        machine.screens[':screen']:snapshot(string.format('nsb3_phonebook_%02d.png', index))
    end
end)
assert(coroutine.resume(actions))
local capture = coroutine.create(function()
    assert(emu.wait(30))
    local memory = machine.devices[':maincpu'].spaces.program
    local stream = assert(io.open('nsb3_idle_ram.bin', 'wb'))
    for address = 0x100000, 0x10ffff do stream:write(string.char(memory:read_u8(address))) end
    stream:close()
end)
assert(coroutine.resume(capture))
