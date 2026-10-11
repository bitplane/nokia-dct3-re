-- Physical NSM-5 Toolkit replies; the card and firmware own the transactions.
local machine = manager.machine
local function key(name)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields[name] then return port.fields[name] end
    end
    error('missing physical key ' .. name)
end
local input = coroutine.create(function()
    local function press(name, action)
        machine:logerror('5210_toolkit_interactive: action=' .. action .. '\n')
        local field = key(name)
        field:set_value(1)
        assert(emu.wait(0.15))
        field:set_value(0)
        assert(emu.wait(1.85))
    end
    local function snapshot(phase)
        machine.screens[':screen']:snapshot('5210_toolkit_interactive_' .. phase .. '.png')
    end
    assert(emu.wait(34))
    snapshot('display')
    press('Left Softkey / Menu', 'dismiss')
    assert(emu.wait(3))
    snapshot('inkey')
    press('Keypad 5', 'inkey_5')
    -- Own NSM-5 observation: the digit remains pending until physical OK.
    press('Left Softkey / Menu', 'inkey_confirm')
    assert(emu.wait(3))
    snapshot('input')
    press('Keypad 4', 'input_4')
    press('Keypad 2', 'input_2')
    snapshot('entered')
    press('Left Softkey / Menu', 'confirm')
    assert(emu.wait(3))
    snapshot('idle')
end)
_G.noki5210_toolkit_interactive = input
assert(coroutine.resume(input))
