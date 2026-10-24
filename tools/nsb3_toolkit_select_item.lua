-- External card follow-up after menu selection; physical item choice only.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb3_toolkit_menu.lua')
local machine, fields = manager.machine, {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local function press(when, name, action)
    assert(emu.wait(when - machine.time:as_double()))
    local field = assert(fields[name])
    field:set_value(1)
    machine:logerror('nsb3_toolkit_select_item: action=' .. action .. ' down=1\n')
    assert(emu.wait(.15))
    field:set_value(0)
    machine:logerror('nsb3_toolkit_select_item: action=' .. action .. ' down=0\n')
end
local actions = coroutine.create(function()
    assert(emu.wait(88))
    machine.screens[':screen']:snapshot('nsb3_toolkit_item_prompt.png')
    press(89, 'Scroll Down', 'next')
    assert(emu.wait(.35))
    machine.screens[':screen']:snapshot('nsb3_toolkit_item_selected.png')
    press(90.5, 'Left Softkey / Menu', 'confirm')
    assert(emu.wait(.5))
    machine.screens[':screen']:snapshot('nsb3_toolkit_item_result.png')
end)
_G.nsb3_toolkit_select_item = actions
assert(coroutine.resume(actions))
