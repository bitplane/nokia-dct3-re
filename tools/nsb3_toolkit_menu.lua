-- Physical navigation of the menu installed by the external SIM application.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb3_toolkit_interactive.lua')
local machine, fields = manager.machine, {}
for _, port in pairs(machine.ioport.ports) do
    for _, field in pairs(port.fields) do fields[field.name] = field end
end
local actions = coroutine.create(function()
    local steps = {{81, 'Scroll Up', 'fieldtest'},
                   {82, 'Scroll Up', 'last'},
                   {84, 'Left Softkey / Menu', 'open'}}
    if _G.nsb3_toolkit_menu_exit then steps[#steps + 1] = {86, 'Scroll Down', 'next'} end
    steps[#steps + 1] = {_G.nsb3_toolkit_menu_exit and 88 or 86, 'Left Softkey / Menu', 'select'}
    if _G.nsb3_toolkit_menu_return then
        assert(not _G.nsb3_toolkit_menu_exit, 'return control uses Continue timing')
        steps[#steps + 1] = {88, 'End', 'end'}
        steps[#steps + 1] = {90, 'Left Softkey / Menu', 'return'}
    end
    for _, step in ipairs(steps) do
        assert(emu.wait(step[1] - machine.time:as_double()))
        local field = assert(fields[step[2]])
        field:set_value(1)
        machine:logerror('nsb3_toolkit_menu: action=' .. step[3] .. ' down=1\n')
        assert(emu.wait(.15))
        field:set_value(0)
        machine:logerror('nsb3_toolkit_menu: action=' .. step[3] .. ' down=0\n')
        assert(emu.wait(.85))
        machine.screens[':screen']:snapshot('nsb3_toolkit_menu_' .. step[3] .. '.png')
    end
end)
_G.nsb3_toolkit_menu = actions
assert(coroutine.resume(actions))
