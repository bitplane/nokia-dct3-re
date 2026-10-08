-- Physical NSM-3 supplementary-service request; no firmware-state writes.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_security_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(21))
    for _, item in ipairs({{2, 'Keypad *'}, {2, 'Keypad 1'},
                          {3, 'Keypad 2'}, {4, 'Keypad 3'},
                          {4, 'Keypad #'}, {0, 'Call / Send'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror('8210_ussd_physical: key=' .. item[2] .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        assert(emu.wait(0.85))
    end
    assert(emu.wait(8))
    machine.screens[':screen']:snapshot('8210_ussd_result.png')
    local back = assert(machine.ioport.ports[':COL.1'].fields['Names / C'])
    machine:logerror('8210_ussd_physical: key=Back\n')
    back:set_value(1)
    if not emu.wait(0.15) then back:set_value(0); return end
    back:set_value(0)
    assert(emu.wait(1.85))
    machine.screens[':screen']:snapshot('8210_ussd_after_back.png')
end)
_G.noki8210_ussd_input = input
assert(coroutine.resume(input))
