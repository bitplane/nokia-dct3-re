-- PIN entry through the product's physical matrix, not firmware state.
local machine = manager.machine
local function key(name)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields[name] then return port.fields[name] end
    end
    error('missing physical key ' .. name)
end
local input = coroutine.create(function()
    local function press(name)
        machine:logerror('5210_pin_physical: key=' .. name .. '\n')
        local field = key(name)
        field:set_value(1)
        assert(emu.wait(_G.noki5210_pin_duration or 0.15))
        field:set_value(0)
        assert(emu.wait(_G.noki5210_pin_gap or 0.2))
    end
    local function snapshot(phase)
        machine.screens[':screen']:snapshot('5210_pin_' .. phase .. '.png')
    end
    -- SIM initialization finishes near 6.4 s on this own-ROM cold profile.
    assert(emu.wait(_G.noki5210_pin_start_delay or 7))
    for _, name in ipairs({'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4'}) do press(name) end
    snapshot('entered')
    press('Left Softkey / Menu')
    if _G.noki5210_pin_after_accept then
        _G.noki5210_pin_after_accept(press, snapshot)
        return
    end
    assert(emu.wait(10))
    snapshot('idle')
    press('Left Softkey / Menu')
    assert(emu.wait(2))
    snapshot('menu')
    press('End')
    assert(emu.wait(2))
    snapshot('recovered')
end)
_G.noki5210_pin_input = input
_G.noki5210_pin_stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('5210_pin_fixture: stopped_at=%.9f\n', machine.time:as_double()))
end)
assert(coroutine.resume(input))
