-- Own NSM-2 security entry and physical responses to card-owned commands.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8850_security_only = true
dofile(directory .. 'noki8850_security_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    local function press(name, action)
        local field
        for _, port in pairs(machine.ioport.ports) do
            if port.fields[name] then field = port.fields[name]; break end
        end
        assert(field, 'missing physical key ' .. name)
        machine:logerror('8850_toolkit_interactive: action=' .. action .. '\n')
        field:set_value(1)
        if not emu.wait(0.15) then field:set_value(0); return end
        field:set_value(0)
        assert(emu.wait(0.85))
    end
    assert(emu.wait(33))
    machine.screens[':screen']:snapshot('8850_toolkit_display.png')
    assert(emu.wait(1))
    press('Menu', 'dismiss')
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('8850_toolkit_inkey.png')
    press('Keypad 5', 'inkey_5')
    press('Menu', 'inkey_confirm')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('8850_toolkit_get_input.png')
    press('Keypad 4', 'input_4')
    press('Keypad 2', 'input_2')
    press('Menu', 'confirm')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('8850_toolkit_interactive_idle.png')
end)
_G.noki8850_toolkit_interactive = input
assert(coroutine.resume(input))
