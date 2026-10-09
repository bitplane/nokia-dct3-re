-- Cold retained alarm: observe, then physical Stop; no clock/alarm entry.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki6250_runtime_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(35))
    machine.screens[':screen']:snapshot('6250_alarm_cold_armed.png')
    machine:logerror('6250_alarm_cold: event=armed_observed\n')
    assert(emu.wait(30))
    machine.screens[':screen']:snapshot('6250_alarm_cold_elapsed.png')
    machine:logerror('6250_alarm_cold: action=stop\n')
    local stop
    for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
        if field.mask == (1 << 1) then stop = field end
    end
    assert(stop, 'missing own left softkey')
    stop:set_value(1)
    assert(emu.wait(0.15))
    stop:set_value(0)
    assert(emu.wait(3.85))
    machine.screens[':screen']:snapshot('6250_alarm_cold_stopped.png')
    machine:logerror('6250_alarm_cold: event=stopped_observed\n')
end)
_G.noki6250_alarm_cold_input = input
assert(coroutine.resume(input))
