-- Own retained-clock handset input; no firmware memory writes.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki8890_clock_read.lua')
local machine = manager.machine
local function key(name)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields[name] then return port.fields[name] end
    end
    error('missing physical key ' .. name)
end
local input = coroutine.create(function()
    local function press(name, action)
        machine:logerror('8890_toolkit_interactive: action=' .. action .. '\n')
        local field = key(name)
        field:set_value(1)
        assert(emu.wait(0.15))
        field:set_value(0)
        assert(emu.wait(1.85))
    end
    local function snapshot(phase)
        machine.screens[':screen']:snapshot('8890_toolkit_interactive_' .. phase .. '.png')
    end
    assert(emu.wait(70))
    snapshot('display')
    press('Menu', 'dismiss')
    assert(emu.wait(3))
    snapshot('inkey')
    press('Keypad 5', 'inkey_5')
    -- NSB-6 presents an OK editor; the digit alone does not reply to the card.
    press('Menu', 'inkey_confirm')
    assert(emu.wait(3))
    snapshot('input')
    press('Keypad 4', 'input_4')
    press('Keypad 2', 'input_2')
    snapshot('entered')
    press('Menu', 'confirm')
    assert(emu.wait(3))
    snapshot('idle')
    if _G.noki8890_toolkit_menu then
        press('Menu', 'menu')
        press('Scroll Up', 'menu_last')
        machine.screens[':screen']:snapshot('8890_toolkit_menu_entry.png')
        press('Menu', 'menu_open')
        machine.screens[':screen']:snapshot('8890_toolkit_menu_items.png')
        press('Menu', 'menu_select')
        assert(emu.wait(3))
        machine.screens[':screen']:snapshot('8890_toolkit_menu_result.png')
        if _G.noki8890_toolkit_network_wait then
            assert(emu.wait(_G.noki8890_toolkit_network_wait))
            machine.screens[':screen']:snapshot('8890_toolkit_network_result.png')
        end
        press('End', 'menu_exit')
        machine.screens[':screen']:snapshot('8890_toolkit_menu_idle.png')
    end
end)
_G.noki8890_toolkit_interactive = input
assert(coroutine.resume(input))
