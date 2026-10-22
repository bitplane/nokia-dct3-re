-- Save at observed CP-DATA ingress, not a host-wall-clock-dependent timestamp.
_G.nsb7_security_with_pin=true
_G.nsb7_state_scenario='sms'
_G.nsb7_sms_read_times={26,28,30,32}
_G.nsb7_state_finish_time=35.1
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local armed=false
_G.nsb7_host_sms_ingress=memory:install_read_tap(0x2645ec,0x2645ef,'host_sms_state_ingress',function()
    if armed or cpu.state['R15'].value~=0x2645ec then return end
    local object=cpu.state['R0'].value
    if object<0x100000 or object>=0x17ff00 then return end
    if memory:read_u8(object+11)==0x80 and memory:read_u8(object+17)&15==9
            and memory:read_u8(object+18)==1 then
        armed=true
        machine:logerror(string.format('nsb7_host_sms_save_trigger: pc=002645ec pd=09 message=01 t=%.9f observer_writes=0\n',machine.time:as_double()))
    end
end)
_G.nsb7_state_wait_for_save=function()
    assert(emu.wait(10))
    while not armed do assert(emu.wait(0.0001)) end
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_state_roundtrip.lua')
