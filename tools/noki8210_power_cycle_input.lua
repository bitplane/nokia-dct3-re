-- Physical shutdown and PWRONX restart; no firmware or storage writes.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_security_input.lua')
local machine = manager.machine
local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
local function at(when)
    return emu.wait(when - machine.time:as_double())
end
local input = coroutine.create(function()
    assert(at(24))
    machine.screens[':screen']:snapshot('8210_power_idle.png')
    assert(at(25))
    machine:logerror('8210_power_physical: action=shutdown_press\n')
    power:set_value(1)
    assert(emu.wait(4))
    power:set_value(0)
    machine:logerror('8210_power_physical: action=shutdown_release\n')
    assert(at(34))
    machine.screens[':screen']:snapshot('8210_power_off.png')
    assert(at(43))
    -- Reset passive debugger log caps, not handset state.
    machine.debugger:command('do temp1=0;do temp6=0;do temp7=0;do temp8=0;do temp9=0')
    machine:logerror('8210_power_physical: action=restart_press\n')
    power:set_value(1)
    assert(emu.wait(2))
    power:set_value(0)
    machine:logerror('8210_power_physical: action=restart_release\n')
    assert(at(58))
    machine.screens[':screen']:snapshot('8210_power_restart.png')
    assert(at(61))
    for _, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
            {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Menu'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror('8210_power_security: key=' .. item[2] .. '\n')
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(0.35))
    end
    assert(at(70))
    machine.screens[':screen']:snapshot('8210_power_after_security.png')
end)
_G.noki8210_power_cycle_input = input
assert(coroutine.resume(input))
