-- Own NSM-2 Toolkit menu; only physical keypad input is changed.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki8850_toolkit_interactive_input.lua')
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
        assert(emu.wait(1.85))
    end
    assert(emu.wait(50))
    press('Menu', 'menu')
    press('Scroll Up', 'menu_last')
    machine.screens[':screen']:snapshot('8850_toolkit_menu_entry.png')
    press('Menu', 'menu_open')
    machine.screens[':screen']:snapshot('8850_toolkit_menu_items.png')
    if _G.noki8850_toolkit_menu_hold then return end
    press('Menu', 'menu_select')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('8850_toolkit_menu_result.png')
    if _G.noki8850_toolkit_call then
        press(_G.noki8850_toolkit_decline and 'Names / C' or 'Menu',
              _G.noki8850_toolkit_decline and 'call_decline' or 'call_accept')
    end
    if _G.noki8850_toolkit_network_wait then
        assert(emu.wait(_G.noki8850_toolkit_network_wait))
        machine.screens[':screen']:snapshot('8850_toolkit_menu_network_result.png')
    end
    press('End', 'menu_exit')
    machine.screens[':screen']:snapshot('8850_toolkit_menu_idle.png')
end)
_G.noki8850_toolkit_menu_input = input
assert(coroutine.resume(input))
