-- Extend the own packet/queue baseline; no injected liveness traffic.
_G.nsm1_native_radio_times={5,20,40,45,65,90,100,120,140,160}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_native_radio_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local requests=0
_G.nsm1_native_long_reset_tap=memory:install_read_tap(0x2c2a00,0x2c2a03,'nsm1_native_long_reset',function()
    if cpu.state['PC'].value~=0x2c2a02 then return end
    requests=requests+1
    if requests<=16 then
        machine:logerror(string.format('nsm1_native_long_restart: index=%d reason=%08x caller=%08x t=%.9f observer_writes=0\n',
            requests,cpu.state['R0'].value,cpu.state['R14'].value,machine.time:as_double()))
    end
end)
