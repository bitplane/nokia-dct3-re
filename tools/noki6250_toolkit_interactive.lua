-- Own NHM-3 physical replies to card-owned DISPLAY TEXT/INKEY/INPUT.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki6250_runtime_observe.lua')
local machine = manager.machine
local function key(name)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields[name] then return port.fields[name] end
    end
    error('missing physical key ' .. name)
end
local input = coroutine.create(function()
    local function press(name, action)
        local field = key(name)
        machine:logerror('6250_toolkit_interactive: action=' .. action .. '\n')
        field:set_value(1)
        assert(emu.wait(0.15))
        field:set_value(0)
        assert(emu.wait(1.85))
    end
    assert(emu.wait(33))
    machine.screens[':screen']:snapshot('6250_toolkit_interactive_display.png')
    press('Left Softkey / Menu', 'dismiss')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('6250_toolkit_interactive_inkey.png')
    press('Keypad 5', 'inkey_5')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('6250_toolkit_interactive_input.png')
    press('Keypad 4', 'input_4')
    press('Keypad 2', 'input_2')
    machine.screens[':screen']:snapshot('6250_toolkit_interactive_entered.png')
    press('Left Softkey / Menu', 'confirm')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('6250_toolkit_interactive_idle.png')
end)
_G.noki6250_toolkit_interactive = input
assert(coroutine.resume(input))
