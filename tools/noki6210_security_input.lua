-- Physical PIN entry against a PIN-enabled laboratory SIM, no phone NV edits.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('6210_pin_prompt.png')
    for _, step in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'},
                          {4, 'Keypad 3'}, {2, 'Keypad 4'},
                          {1, 'Left Softkey / Menu'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. step[1]].fields[step[2]])
        machine:logerror('6210_security_physical: action=' ..
            (step[1] == 1 and 'confirm' or step[2]) .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
    end
    if not emu.wait(20) then return end
    local menu = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
    machine:logerror('6210_security_physical: action=menu\n')
    menu:set_value(1)
    if not emu.wait(0.15) then menu:set_value(0); return end
    menu:set_value(0)
    if not emu.wait(2) then return end
    machine.screens[':screen']:snapshot('6210_security_then_menu.png')
end)
_G.noki6210_security_input = input
assert(coroutine.resume(input))
