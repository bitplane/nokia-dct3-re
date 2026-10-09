-- Independent cold startup after the phone's own 54321 code save.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_bootstrap_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
cpu.debug:bpset(0x2fa48c, nil,
    'logerror "8890_changed_code: event=input bytes=%02x%02x%02x%02x%02x%02x\\n",b@r0,b@(r0+1),b@(r0+2),b@(r0+3),b@(r0+4),b@(r0+5);g')
cpu.debug:bpset(0x2fa4c0, nil,
    'logerror "8890_changed_code: event=compare result=%08x stored=%08x input=%08x\\n",r0,d@1377e4,d@r13;g')
local input = coroutine.create(function()
    assert(emu.wait(12))
    for _, item in ipairs({{3, 'Keypad 5'}, {2, 'Keypad 4'}, {4, 'Keypad 3'},
                          {3, 'Keypad 2'}, {2, 'Keypad 1'}, {1, 'Menu'}}) do
        machine:logerror('8890_changed_code_physical: key=' .. item[2] .. '\n')
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(0.35))
    end
    assert(emu.wait(5))
    machine.screens[':screen']:snapshot('8890_changed_code_cold.png')
end)
_G.noki8890_changed_code_input = input
assert(coroutine.resume(input))
