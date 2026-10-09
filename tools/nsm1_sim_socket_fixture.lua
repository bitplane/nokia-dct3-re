-- Physical socket-edge experiment on the explicitly provisional NSM-1 board.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'nsm1_native_observe.lua')
local machine = manager.machine
local socket = assert(machine.ioport.ports[':SIM_REMOVED'].fields['Remove SIM card'])
local removed = false
local inserted = false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now >= 3 and not removed then
        removed = true
        socket:set_value(1)
        machine:logerror(string.format('nsm1_sim_socket: removed=1 t=%.9f\n', now))
    end
    if now >= 4 and not inserted then
        inserted = true
        socket:set_value(0)
        machine:logerror(string.format('nsm1_sim_socket: removed=0 t=%.9f\n', now))
    end
end, 'frame')
