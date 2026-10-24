-- Extend physical DISPLAY TEXT dismissal with card-driven key/text entry.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb3_toolkit_input.lua')
local machine, fields = manager.machine, {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local function press(when, name, action)
    assert(emu.wait(when - machine.time:as_double()))
    local field = assert(fields[name])
    field:set_value(1)
    machine:logerror('nsb3_toolkit_input: action=' .. action .. ' down=1\n')
    assert(emu.wait(.15))
    field:set_value(0)
    machine:logerror('nsb3_toolkit_input: action=' .. action .. ' down=0\n')
end
local actions = coroutine.create(function()
    assert(emu.wait(70.5))
    machine.screens[':screen']:snapshot('nsb3_toolkit_inkey.png')
    press(71, 'Keypad 5', 'inkey_5')
    press(72, 'Left Softkey / Menu', 'inkey_confirm')
    assert(emu.wait(73.5 - machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb3_toolkit_input.png')
    press(74, 'Keypad 4', 'input_4')
    press(75, 'Keypad 2', 'input_2')
    assert(emu.wait(76 - machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb3_toolkit_input_filled.png')
    press(77, 'Left Softkey / Menu', 'input_confirm')
    assert(emu.wait(79 - machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb3_toolkit_interactive_final.png')
end)
_G.nsb3_toolkit_interactive = actions
assert(coroutine.resume(actions))
