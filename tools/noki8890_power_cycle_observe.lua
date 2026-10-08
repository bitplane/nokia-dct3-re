-- Physical power inputs and screenshots; no firmware writes or charger wake.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8890_clock_settlement_only = true
dofile(directory .. 'noki8890_clock_input.lua')
local machine = manager.machine
local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
local function at(when)
    return emu.wait(when - machine.time:as_double())
end
local input = coroutine.create(function()
    if not at(45) then return end
    machine.screens[':screen']:snapshot('8890_power_idle.png')
    machine:logerror('8890_power_physical: action=shutdown_press\n')
    power:set_value(1)
    if not emu.wait(4) then power:set_value(0); return end
    power:set_value(0)
    machine:logerror('8890_power_physical: action=shutdown_release\n')
    if not at(51.8) then return end
    machine.screens[':screen']:snapshot('8890_power_off.png')
    if not at(58) then return end
    -- Re-arm only debugger log caps, so both boots have full passive evidence.
    machine.debugger:command('do temp6=0;do temp7=0;do temp8=0;do temp9=0')
    machine:logerror('8890_power_physical: action=restart_press\n')
    power:set_value(1)
    if not emu.wait(2) then power:set_value(0); return end
    power:set_value(0)
    machine:logerror('8890_power_physical: action=restart_release\n')
    if not at(65) then return end
    machine.screens[':screen']:snapshot('8890_power_after_key.png')
    if not at(71) then return end
    machine.screens[':screen']:snapshot('8890_power_restart_security.png')
    if not at(76) then return end
    for _, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
            {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Menu'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror('8890_power_security: key=' .. item[2] .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.35) then return end
    end
    for _, when in ipairs({81, 86}) do
        if not at(when) then return end
        machine.screens[':screen']:snapshot('8890_power_restart_' .. when .. '.png')
    end
end)
_G.noki8890_power_cycle_observe = input
assert(coroutine.resume(input))
