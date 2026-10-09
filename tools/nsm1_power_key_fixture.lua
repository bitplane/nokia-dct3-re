-- Physical power-key experiment; own static wiring is known, UI acceptance is not.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'nsm1_native_observe.lua')
local machine = manager.machine
local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
power:set_value(1)
machine:logerror(string.format('nsm1_power_key: pressed=1 t=%.9f\n',
    machine.time:as_double()))
local released = false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now >= 1 and not released then
        released = true
        power:set_value(0)
        machine:logerror(string.format('nsm1_power_key: pressed=0 t=%.9f\n', now))
    end
end, 'frame')
